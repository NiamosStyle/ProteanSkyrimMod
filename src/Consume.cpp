#include "Consume.h"

#include "Evolve.h"
#include "Face.h"
#include "Registry.h"
#include "Shift.h"
#include "State.h"

namespace protean::Consume
{
	namespace
	{
		constexpr const char* kSound = "NPCWerewolfFeeding";  // the body's sound of Survivor's meal (user's pick, 2026-10-24)
		constexpr float       kFront = 0.5f;   // animal form: cos of the widest angle off the facing a body is looked for (60 deg)
		constexpr float       kGone = 0.5f;    // seconds from the tentacles' end to the body's
		constexpr float       kCalmRange = 4000.0f;  // Assassin: how far the alarm is called off, game units (~57 m)
		// BloodSprayImpactSetRed, Skyrim.esm: the blood Survivor's meal splashed (user's pick, live 2026-10-24); here it
		// bursts from the body as the tentacles leave it and again as it goes (user, 2026-10-28). "A lot more" (the user,
		// the same day, after seeing 4-6 splashes): it sprays from every body part every kBloodEvery seconds while the
		// tentacles are on, and the body goes in Devastator's blood burst
		constexpr RE::FormID  kBlood = 0x01F82A;
		constexpr float       kBloodEvery = 0.25f;
		constexpr RE::FormID  kBloodBurst = 0x015373;  // DLC1VampChangeExplosion (Dawnguard.esm), as Devastator's burst

		struct Meal
		{
			bool                 on{ false };
			RE::ObjectRefHandle  body;
			float                t{ 0.0f };
			float                seconds{ 0.0f };   // the tentacles' time
			bool                 animal{ false };   // the player is in animal form
			bool                 silent{ false };   // Assassin: nobody saw the start, nobody notices
			bool                 stopped{ false };  // the tentacles were taken off
			float                blood{ 0.0f };     // when the next spray of blood is due
			std::set<RE::FormID> seen;              // people who saw it
			bool                 stealth{ false };  // Stealth Consume took it
		};
		Meal g_meal;

		RE::PlayerCharacter* Player() { return RE::PlayerCharacter::GetSingleton(); }

		float Dial(std::string_view a_name, float a_fallback) { return Registry::Get().Dial(a_name, a_fallback); }

		bool IsPerson(RE::Actor* a_body)
		{
			auto* race = a_body->GetRace();
			return race && race->data.flags.all(RE::RACE_DATA::Flag::kFaceGenHead) && !Registry::Get().ByRace(race);
		}

		// what a quest holds the body for: a quest object (the flag that keeps quest items from being dropped) is
		// refused; any other alias is only logged, so a quest that breaks can be traced
		bool QuestObject(RE::Actor* a_body)
		{
			auto* ex = a_body->extraList.GetByType<RE::ExtraAliasInstanceArray>();
			if (!ex) {
				return false;
			}
			RE::BSReadLockGuard l(ex->lock);
			bool                held = false;
			for (const auto* d : ex->aliases) {
				if (!d || !d->quest || !d->alias) {
					continue;
				}
				const bool object = d->alias->flags.all(RE::BGSBaseAlias::FLAGS::kQuestObject);
				logger::info("consume: '{}' {:08X} is alias '{}' of quest '{}'{}", a_body->GetName(), a_body->GetFormID(),
					d->alias->aliasName.c_str(), d->quest->GetFormEditorID(), object ? " (a quest object: refused)" : "");
				held = held || object;
			}
			return held;
		}

		// a body that can be consumed (the quest check is the caller's, as it tells the player)
		bool Edible(RE::Actor* a_body)
		{
			auto* race = a_body ? a_body->GetRace() : nullptr;
			if (!race || !a_body->IsDead() || a_body->IsPlayerRef() || a_body->IsDisabled() || race->IsChildRace()) {
				return false;
			}
			// bones, dust and machines are no biomass; a vampire is
			const bool undead = a_body->HasKeywordString("ActorTypeUndead") && !a_body->HasKeywordString("Vampire");
			return !undead && !a_body->HasKeywordString("ActorTypeDwarven") && !a_body->HasKeywordString("ActorTypeGhost");
		}

		// people who see the player now (only people count, as for a stolen face and a change of form)
		void Watchers(RE::PlayerCharacter* a_p, RE::Actor* a_body, std::set<RE::FormID>& a_out)
		{
			auto* pl = RE::ProcessLists::GetSingleton();
			if (!pl) {
				return;
			}
			const float range = Dial("Protean_ConsumeWitnessRange", 3000.0f);
			const auto  here = a_p->GetPosition();
			for (auto& h : pl->highActorHandles) {
				auto a = h.get();
				if (!a || a.get() == a_p || a.get() == a_body || a->IsDead() || a->IsPlayerTeammate() || !IsPerson(a.get())) {
					continue;
				}
				if (here.GetDistance(a->GetPosition()) <= range && a->RequestDetectionLevel(a_p) > 0) {
					a_out.insert(a->GetFormID());
				}
			}
		}

		// Assassin: the people the death alarmed who cannot see the player stand down
		void Calm(RE::PlayerCharacter* a_p)
		{
			auto* pl = RE::ProcessLists::GetSingleton();
			if (!pl) {
				return;
			}
			int calmed = 0;
			for (auto& h : pl->highActorHandles) {
				auto a = h.get();
				if (!a || a.get() == a_p || a->IsDead() || a->IsPlayerTeammate() || !a->IsInCombat() || !IsPerson(a.get()) ||
					a_p->GetPosition().GetDistance(a->GetPosition()) > kCalmRange || a->RequestDetectionLevel(a_p) > 0) {
					continue;
				}
				a->StopCombat();
				a->StopAlarmOnActor();
				++calmed;
				logger::info("consume: '{}' {:08X} stands down (Assassin)", a->GetName(), a->GetFormID());
			}
			// every alarm against the player ends too, as when a bounty is paid: Calcelmo, called off, went for the player
			// again until the stolen face came off (2026-10-28)
			pl->StopCombatAndAlarmOnActor(a_p, false);
			logger::info("consume: {} alarmed actor(s) called off", calmed);
		}

		// each witness's crime faction is owed the bounty, once
		void Crime(const std::set<RE::FormID>& a_seen)
		{
			const auto               bounty = static_cast<std::int32_t>(Dial("Protean_ConsumeBounty", 1000.0f));
			std::set<RE::TESFaction*> owed;
			for (const auto id : a_seen) {
				auto* a = RE::TESForm::LookupByID<RE::Actor>(id);
				auto* f = a ? a->GetCrimeFaction() : nullptr;
				if (bounty > 0 && f && f->TracksCrimes() && owed.insert(f).second) {
					f->ModCrimeGold(bounty, true);
					logger::info("consume: seen by '{}' {:08X}: {} bounty in '{}'", a->GetName(), id, bounty, f->GetName());
				}
			}
		}

		// everything the body carries, or only its quest items
		void Loot(RE::PlayerCharacter* a_p, RE::Actor* a_body, bool a_all)
		{
			struct Take
			{
				RE::TESBoundObject* obj;
				std::int32_t        count;
				RE::ExtraDataList*  list;
			};
			std::vector<Take> takes;
			auto              inv = a_body->GetInventory([](RE::TESBoundObject& a_o) { return a_o.GetPlayable(); });
			for (auto& [obj, data] : inv) {
				auto& [count, entry] = data;
				if (count <= 0 || !entry || (!a_all && !entry->IsQuestObject())) {
					continue;
				}
				std::int32_t listed = 0;
				if (entry->extraLists) {
					for (auto* x : *entry->extraLists) {
						if (x) {
							const auto n = std::max(x->GetCount(), 1);
							takes.push_back({ obj, n, x });
							listed += n;
						}
					}
				}
				if (count > listed) {
					takes.push_back({ obj, count - listed, nullptr });
				}
			}
			for (const auto& t : takes) {
				a_body->RemoveItem(t.obj, t.count, RE::ITEM_REMOVE_REASON::kStoreInContainer, t.list, a_p);
			}
			logger::info("consume: {} stack(s) taken ({})", takes.size(), a_all ? "all the loot" : "quest items only");
		}

		// evolution points: the body's bulk (person and middling animal 2)
		std::uint32_t Biomass(RE::Actor* a_body, const Creature* a_c, bool a_person)
		{
			if (a_person) {
				return 2;
			}
			if (a_c) {
				return a_c->size == "small" ? 1 : a_c->size == "medium" ? 2 : a_c->size == "outdoor_only" ? 5 : 3;
			}
			// a creature with no form (a dragon, a spriggan...): by its height
			const float h = a_body->GetHeight();
			return h < 70.0f ? 1 : h < 180.0f ? 2 : h < 320.0f ? 3 : 5;
		}

		void Sound(RE::TESObjectREFR* a_at, const char* a_name)
		{
			RE::BSSoundHandle h;
			if (auto* audio = RE::BSAudioManager::GetSingleton()) {
				audio->GetSoundHandleByName(h, a_name, 0x1A);
			}
			if (h.IsValid()) {
				h.SetPosition(a_at->GetPosition());
				h.Play();
			}
		}

		// from every part of the body the race names (with no node the game plays nothing), down onto the body and the
		// ground: the game plays a splash where the direction first meets something, and sprays up and sideways landed
		// on walls (user, 2026-10-28)
		void Blood(RE::Actor* a_body, bool a_log = false)
		{
			auto* set = RE::TESForm::LookupByID<RE::BGSImpactDataSet>(kBlood);
			auto* im = RE::BGSImpactManager::GetSingleton();
			auto* race = a_body->GetRace();
			auto* parts = race ? race->bodyPartData : nullptr;
			if (!set || !im || !parts) {
				return;
			}
			const RE::NiPoint3 ways[] = { { 0.0f, 0.0f, -1.0f }, { 0.3f, 0.0f, -0.95f }, { -0.3f, 0.0f, -0.95f },
				{ 0.0f, 0.3f, -0.95f }, { 0.0f, -0.3f, -0.95f } };
			int                played = 0;
			for (const auto* part : parts->parts) {
				if (!part || part->targetName.empty()) {
					continue;
				}
				for (auto way : ways) {
					played += im->PlayImpactEffect(a_body, set, part->targetName.c_str(), way, 200.0f, false, false) ? 1 : 0;
				}
			}
			if (a_log) {
				logger::info("consume: {} blood splashes", played);
			}
		}

		// the Devastator's way: the shader effect ended directly (EffectShader.Stop left it on for seconds)
		void StopShader(RE::TESObjectREFR* a_body)
		{
			auto* fx = Registry::Get().DevastatorShader();
			auto* lists = RE::ProcessLists::GetSingleton();
			if (!fx || !lists) {
				return;
			}
			lists->ForEachShaderEffect([&](RE::ShaderReferenceEffect* a_e) {
				if (a_e && a_e->effectData == fx && a_e->target.get().get() == a_body) {
					a_e->lifetime = a_e->age;
					a_e->finished = true;
				}
				return RE::BSContainer::ForEachResult::kContinue;
			});
		}

		void Start(RE::PlayerCharacter* a_p, RE::Actor* a_body, bool a_stealth = false)
		{
			const bool animal = Shift::ActiveCreature() != nullptr;
			const bool person = IsPerson(a_body);
			Meal       m;
			Watchers(a_p, a_body, m.seen);
			m.on = true;
			m.body = a_body->GetHandle();
			m.animal = animal;
			m.silent = person && !animal && m.seen.empty() && Evolve::Has("assassin");
			m.stealth = a_stealth;
			m.seconds = m.silent || a_stealth ? Dial("Protean_AssassinConsumeSeconds", 1.0f) : Dial("Protean_ConsumeSeconds", 2.0f);
			g_meal = std::move(m);
			if (auto* fx = Registry::Get().DevastatorShader()) {
				a_body->ApplyEffectShader(fx, g_meal.seconds + kGone);
				a_p->ApplyEffectShader(fx, g_meal.seconds + kGone);  // on the player too, as Devastator's wind-up (user, 2026-10-28)
			}
			Sound(a_body, kSound);
			if (g_meal.silent) {
				Calm(a_p);
			}
			logger::info("consume: '{}' {:08X} starts ({}, {}, seen by {}{})", a_body->GetName(), a_body->GetFormID(),
				person ? "a person" : "a creature", animal ? "animal form" : "own form", g_meal.seen.size(),
				g_meal.silent ? ", unnoticed (Assassin)" : "");
		}

		void Finish(RE::PlayerCharacter* a_p)
		{
			auto ref = g_meal.body.get();
			auto m = std::move(g_meal);
			g_meal = {};
			auto* body = ref ? ref->As<RE::Actor>() : nullptr;
			if (!body || body->IsDisabled()) {
				logger::info("consume: the body is gone");
				return;
			}
			auto&       s = State::Get();
			auto&       reg = Registry::Get();
			const bool  person = IsPerson(body);
			const auto* c = person ? nullptr : reg.ByActor(body);
			if (!m.silent) {
				Watchers(a_p, body, m.seen);  // anyone who came by meanwhile
			}
			Loot(a_p, body, m.stealth && s.stealthLoot ? true : s.consumeLoot);
			if (person && !m.seen.empty()) {
				Crime(m.seen);  // before a stolen face goes on, so taking it off cannot forgive it
				if (m.animal) {
					Shift::SeenFeeding(body);
				}
			}
			if (m.silent) {
				// Assassin: what the worn face did (the sneak attack) is not held against the player, as when the face comes
				// off unseen; then the alarms are ended again, in case any rose during the consumption
				Shift::ForgiveFaceCrime(true);
				Calm(a_p);
			}
			if (person && Evolve::Has("borrowed_visage")) {
				Face::FromBody(body, !m.animal, m.silent || m.stealth, !m.seen.empty());
			}
			if (person) {
				Evolve::OnRenowned(body);
			}
			auto*       av = a_p->AsActorValueOwner();
			const float max = av->GetPermanentActorValue(RE::ActorValue::kHealth) +
			                  a_p->GetActorValueModifier(RE::ACTOR_VALUE_MODIFIER::kTemporary, RE::ActorValue::kHealth);
			const float heal = std::round(max * (Evolve::Has("survivor_2") ? Dial("Protean_ConsumeHealSurvivor", 0.5f) :
			                                                                 Dial("Protean_ConsumeHeal", 0.2f)));
			av->RestoreActorValue(RE::ActorValue::kHealth, heal);
			if (c) {
				Shift::Learn(*c);
			}
			Evolve::AddBoost();
			const std::string label = person ? std::string(body->GetDisplayFullName()) : c ? c->name : std::string(body->GetName());
			Evolve::OnConsume(person ? "people" : c ? c->id : "other", label, Biomass(body, c, person));
			logger::info("consume: '{}' {:08X} consumed, healed {:.0f} of {:.0f}", body->GetName(), body->GetFormID(), heal, max);
			Blood(body, true);
			if (auto* dh = RE::TESDataHandler::GetSingleton()) {
				if (auto* burst = dh->LookupForm<RE::BGSExplosion>(kBloodBurst, "Dawnguard.esm")) {
					body->PlaceObjectAtMe(burst, false);
				}
			}
			body->Disable();
		}

		RE::FormID g_loot{ 0 };  // a body the player chose to loot: its activation comes back through the hook

		// the box before consuming: in the player's own form, Loot / Consume / Cancel as Stolen Face's box was (user,
		// 2026-10-28; the Protean page can turn it off), and the first time ever, the warning that the loot goes too
		class Box : public RE::IMessageBoxCallback
		{
		public:
			Box(RE::FormID a_id, bool a_loot) :
				_id(a_id), _loot(a_loot) {}

			void Run(std::uint8_t a_button) override
			{
				const auto id = _id;
				const int  pick = _loot ? a_button : a_button + 1;  // 0 loot, 1 consume, 2 cancel
				SKSE::GetTaskInterface()->AddTask([id, pick]() {
					auto* p = Player();
					auto* a = RE::TESForm::LookupByID<RE::Actor>(id);
					logger::info("consume: the box answered: {}", pick == 0 ? "loot" : pick == 1 ? "consume" : "cancel");
					if (!p || !a || g_meal.on) {
						return;
					}
					if (pick == 0) {
						g_loot = id;
						a->ActivateRef(p, 0, nullptr, 1, false);
					} else if (pick == 1 && Edible(a)) {
						State::Get().consumeWarned = true;
						Start(p, a);
					}
				});
			}

		private:
			RE::FormID _id;
			bool       _loot;
		};

		void Ask(RE::PlayerCharacter* a_p, RE::Actor* a_body, bool a_own)
		{
			const bool choose = a_own && State::Get().consumeAsk;
			const bool warn = !State::Get().consumeWarned;
			if (!choose && !warn) {
				Start(a_p, a_body);
				return;
			}
			auto* box = RE::UIMessageDataFactory::Create<RE::MessageBoxData>();
			if (!box) {
				Start(a_p, a_body);
				return;
			}
			std::string text = std::format("Body of {}", a_body->GetDisplayFullName());
			if (warn) {
				text += "\n\nConsuming destroys the body and everything it carries (quest items are kept). To keep the loot, "
				        "turn on \"Take the loot when consuming\" on the Protean page of the SKSE Menu Framework (F1).\n\n"
				        "This warning will not show again.";
			}
			box->bodyText = text.c_str();
			if (choose) {
				box->buttonText.push_back(RE::BSString("Loot"));
			}
			box->buttonText.push_back(RE::BSString("Consume"));
			box->buttonText.push_back(RE::BSString("Cancel"));
			box->cancelButtonIndex = choose ? 2 : 1;
			box->isCancellable = true;
			box->callback = RE::BSTSmartPointer<RE::IMessageBoxCallback>(new Box(a_body->GetFormID(), choose));
			RE::MessageBoxMenu::QueueMessage(box);
			logger::info("consume: the box shown ({}{})", choose ? "loot, consume, cancel" : "consume, cancel",
				warn ? ", with the first-time warning" : "");
		}

		// ---- Stealth Consume

		struct Pending
		{
			RE::ActorHandle target;
			float           left;  // seconds it may still take to die
		};
		std::vector<Pending> g_pending;  // people the player sneak-attacked in melee while it was on (main thread)
		bool                 g_stealthWas{ false };

		bool StealthOn(RE::PlayerCharacter* a_p)
		{
			auto* e = Registry::Get().StealthEffect();
			auto* mt = a_p ? a_p->AsMagicTarget() : nullptr;
			return e && mt && mt->HasMagicEffect(e);
		}

		bool Allowed(RE::PlayerCharacter* a_p, RE::Actor* a_body);

		// a sneak attack's target that dies within a moment is consumed (the killing blow and the death are not always
		// reported in the same frame)
		void Stealth(RE::PlayerCharacter* a_p, float a_delta)
		{
			const bool on = StealthOn(a_p);
			if (g_stealthWas && !on) {
				Shift::Notify("Stealth Consume has worn off.");
				logger::info("stealth consume: worn off");
			}
			g_stealthWas = on;
			for (auto it = g_pending.begin(); it != g_pending.end();) {
				auto a = it->target.get();
				it->left -= a_delta;
				if (a && a->IsDead() && !g_meal.on) {
					logger::info("stealth consume: '{}' {:08X} killed by a sneak attack", a->GetName(), a->GetFormID());
					if (Allowed(a_p, a.get())) {
						Start(a_p, a.get(), true);
					}
					it = g_pending.erase(it);
				} else if (!a || it->left <= 0.0f) {
					it = g_pending.erase(it);
				} else {
					++it;
				}
			}
		}

		class HitSink : public RE::BSTEventSink<RE::TESHitEvent>
		{
		public:
			RE::BSEventNotifyControl ProcessEvent(const RE::TESHitEvent* a_event, RE::BSTEventSource<RE::TESHitEvent>*) override
			{
				if (!a_event || !a_event->cause || !a_event->target || !a_event->cause->IsPlayerRef() ||
					!a_event->flags.any(RE::TESHitEvent::Flag::kSneakAttack)) {
					return RE::BSEventNotifyControl::kContinue;
				}
				// melee only: claws, fists, a weapon in hand; not a bow, a crossbow or a spell
				auto* source = RE::TESForm::LookupByID(a_event->source);
				if (source) {
					auto* w = source->As<RE::TESObjectWEAP>();
					if (!w || w->IsBow() || w->IsCrossbow()) {
						return RE::BSEventNotifyControl::kContinue;
					}
				}
				auto* target = a_event->target->As<RE::Actor>();
				if (!target) {
					return RE::BSEventNotifyControl::kContinue;
				}
				const auto h = target->GetHandle();
				SKSE::GetTaskInterface()->AddTask([h] {
					auto* p = Player();
					auto  a = h.get();
					if (p && a && StealthOn(p) && !Shift::ActiveCreature() && IsPerson(a.get())) {
						g_pending.push_back({ h, 1.5f });
						logger::info("stealth consume: sneak attack on '{}' {:08X}", a->GetName(), a->GetFormID());
					}
				});
				return RE::BSEventNotifyControl::kContinue;
			}
		};

		// the checks both ways in share; true = go ahead
		bool Allowed(RE::PlayerCharacter* a_p, RE::Actor* a_body)
		{
			if (!Edible(a_body) || !a_p->HasSpell(Registry::Get().Starter())) {
				return false;
			}
			if (QuestObject(a_body)) {
				Shift::Notify("A quest still needs this body.");
				return false;
			}
			return true;
		}
	}

	bool OnActivate(RE::Actor* a_body)
	{
		auto* p = Player();
		if (!p || !a_body) {
			return false;
		}
		if (g_loot == a_body->GetFormID()) {
			g_loot = 0;
			return false;  // Loot picked in the box: the game's loot window
		}
		if (g_meal.on) {
			return g_meal.body.get().get() == a_body;  // the body going: no loot window
		}
		if (Shift::ActiveCreature() || !p->IsSneaking() || !Allowed(p, a_body)) {
			return false;
		}
		Ask(p, a_body, true);
		return true;
	}

	bool OnAnimalActivate()
	{
		auto*       p = Player();
		const auto* c = Shift::ActiveCreature();
		if (!p || !c) {
			return false;
		}
		if (g_meal.on) {
			return true;
		}
		if (p->IsInMidair()) {
			return false;
		}
		// the nearest body in reach and in front of the animal
		const auto         here = p->GetPosition();
		const float        yaw = p->GetAngleZ();
		const RE::NiPoint3 facing{ std::sin(yaw), std::cos(yaw), 0.0f };
		RE::Actor*         best = nullptr;
		float              bestDist = Dial("Protean_FeedReach", 250.0f) + c->reach * 0.5f;
		RE::TES::GetSingleton()->ForEachReferenceInRange(p, bestDist, [&](RE::TESObjectREFR* a_ref) {
			auto* a = a_ref ? a_ref->As<RE::Actor>() : nullptr;
			if (!a || !Edible(a)) {
				return RE::BSContainer::ForEachResult::kContinue;
			}
			auto to = a->GetPosition() - here;
			to.z = 0.0f;
			const float dist = to.Length();
			if (dist < bestDist && (dist < 60.0f || (to.x * facing.x + to.y * facing.y) / dist >= kFront)) {
				best = a;
				bestDist = dist;
			}
			return RE::BSContainer::ForEachResult::kContinue;
		});
		if (!best || !Allowed(p, best)) {
			return false;
		}
		const auto to = best->GetPosition() - here;
		p->SetAngle(RE::NiPoint3{ p->GetAngleX(), p->GetAngleY(), std::atan2(to.x, to.y) });
		Ask(p, best, false);
		return true;
	}

	void OnFrame(RE::PlayerCharacter* a_p, float a_delta)
	{
		Stealth(a_p, a_delta);
		if (!g_meal.on) {
			return;
		}
		auto ref = g_meal.body.get();
		if (!ref || a_p->IsDead()) {
			logger::info("consume: stopped ({})", ref ? "the player died" : "the body is gone");
			StopShader(a_p);
			g_meal = {};
			return;
		}
		g_meal.t += a_delta;
		if (!g_meal.stopped && g_meal.t >= g_meal.seconds) {
			g_meal.stopped = true;
			StopShader(ref.get());
			StopShader(a_p);
			if (auto* body = ref->As<RE::Actor>()) {
				Blood(body, true);
			}
		}
		if (!g_meal.stopped && g_meal.t >= g_meal.blood) {
			g_meal.blood = g_meal.t + kBloodEvery;
			if (auto* body = ref->As<RE::Actor>()) {
				Blood(body);
			}
		}
		if (g_meal.t >= g_meal.seconds + kGone) {
			Finish(a_p);
		}
	}

	void Cancel()
	{
		g_meal = {};
		g_pending.clear();
		g_stealthWas = false;
	}

	void Install()
	{
		static HitSink sink;
		RE::ScriptEventSourceHolder::GetSingleton()->AddEventSink<RE::TESHitEvent>(&sink);
	}

	void OnStealthCast()
	{
		auto* p = Player();
		auto* spell = Registry::Get().StealthSpell();
		if (!p || !spell) {
			return;
		}
		if (Shift::ActiveCreature()) {
			Shift::Notify("Stealth Consume works only in your own form.");
			return;
		}
		const float cost = Dial("Protean_StealthConsumeCost", 25.0f);
		auto*       av = p->AsActorValueOwner();
		if (cost > 0.0f && av->GetActorValue(RE::ActorValue::kStamina) < cost) {
			Shift::Notify("You are too tired for Stealth Consume.");
			logger::info("stealth consume: refused, stamina {:.0f} < {:.0f}", av->GetActorValue(RE::ActorValue::kStamina), cost);
			return;
		}
		if (cost > 0.0f) {
			av->DamageActorValue(RE::ActorValue::kStamina, cost);
		}
		if (auto* caster = p->GetMagicCaster(RE::MagicSystem::CastingSource::kInstant)) {
			caster->CastSpellImmediate(spell, false, p, 1.0f, false, 0.0f, p);
		}
		Shift::Notify("Stealth Consume: your sneak kills will be consumed.");
		logger::info("stealth consume: cast ({:.0f} Stamina)", cost);
	}

	bool Busy() { return g_meal.on; }

	bool Nearest()
	{
		auto* p = Player();
		if (!p || g_meal.on) {
			return false;
		}
		RE::Actor* best = nullptr;
		float      bestDist = 600.0f;
		RE::TES::GetSingleton()->ForEachReferenceInRange(p, bestDist, [&](RE::TESObjectREFR* a_ref) {
			auto* a = a_ref ? a_ref->As<RE::Actor>() : nullptr;
			if (a && Edible(a) && a->GetPosition().GetDistance(p->GetPosition()) < bestDist) {
				best = a;
				bestDist = a->GetPosition().GetDistance(p->GetPosition());
			}
			return RE::BSContainer::ForEachResult::kContinue;
		});
		if (!best || !Allowed(p, best)) {
			logger::info("dev: no body to consume near");
			return false;
		}
		Start(p, best);
		return true;
	}
}
