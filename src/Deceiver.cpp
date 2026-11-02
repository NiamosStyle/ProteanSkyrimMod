#include "Deceiver.h"

#include "Evolve.h"
#include "Face.h"
#include "Registry.h"
#include "Shift.h"
#include "State.h"

namespace protean::Deceiver
{
	namespace
	{
		constexpr float kCheckEvery = 0.5f;     // seconds
		constexpr float kFightRange = 4096.0f;  // the infected join fights against the player this close (about 58 m)
		constexpr float kMarkRange = 4000.0f;   // game units: well beyond any sneak attack's reach
		constexpr float kBrave = 3.0f;          // Confidence: the infected do not run from a fight

		float Dial(std::string_view a_name, float a_fallback) { return Registry::Get().Dial(a_name, a_fallback); }

		// a person: a race with a face, not one of Protean's animals
		bool Person(RE::Actor* a_a)
		{
			auto* r = a_a->GetRace();
			return r && r->data.flags.all(RE::RACE_DATA::Flag::kFaceGenHead) && !Registry::Get().ByRace(r);
		}

		template <class F>
		void ForNear(RE::PlayerCharacter* a_p, float a_range, F&& a_f)
		{
			auto* pl = RE::ProcessLists::GetSingleton();
			if (!pl) {
				return;
			}
			const auto here = a_p->GetPosition();
			for (auto& h : pl->highActorHandles) {
				auto a = h.get();
				if (a && a.get() != a_p && here.GetDistance(a->GetPosition()) <= a_range) {
					a_f(a.get());
				}
			}
		}

		// ---- Inconspicuous

		void Inconspicuous(RE::PlayerCharacter* a_p)
		{
			auto& s = State::Get();
			float want = 0.0f;
			int   people = 0;
			if (Evolve::Has("inconspicuous") && !Shift::ActiveCreature() && !a_p->IsDead()) {
				ForNear(a_p, Dial("Protean_InconspicuousRange", 1050.0f), [&](RE::Actor* a) {
					if (!a->IsDead() && !a->IsPlayerTeammate() && Person(a) && !a->IsHostileToActor(a_p)) {
						++people;
					}
				});
				want = std::min(people * Dial("Protean_InconspicuousPerPerson", 5.0f), Dial("Protean_InconspicuousMax", 30.0f));
			}
			if (want != s.inconspicuousApplied) {
				a_p->AsActorValueOwner()->ModActorValue(RE::ACTOR_VALUE_MODIFIER::kTemporary, RE::ActorValue::kSneak,
					want - s.inconspicuousApplied);
				logger::info("inconspicuous: Sneak +{:.0f} ({} friendly people near)", want, people);
				s.inconspicuousApplied = want;
			}
		}

		// ---- Assassin

		std::set<RE::FormID> g_marked;

		void Assassin(RE::PlayerCharacter* a_p)
		{
			auto&      reg = Registry::Get();
			auto*      mark = reg.AssassinMark();
			auto*      perk = reg.AssassinPerk();
			if (!mark || !perk) {
				return;
			}
			// the user, 2026-10-28: no stolen face needed for now ("to see if it feels like Prototype")
			const bool on = Evolve::Has("assassin") && !Shift::ActiveCreature();
			if (a_p->HasPerk(perk) != on) {
				on ? a_p->AddPerk(perk) : a_p->RemovePerk(perk);
				logger::info("assassin: {}", on ? "sneak attacks on anyone not hostile multiplied" : "off");
			}
			// the user, 2026-10-28: anyone who would not attack the player on sight (no longer only the worn face's factions)
			auto trusts = [&](RE::Actor* a) { return on && !a->IsDead() && !AttacksOnSight(a, a_p).has_value(); };
			std::set<RE::FormID> now;
			ForNear(a_p, kMarkRange, [&](RE::Actor* a) {
				const bool should = trusts(a);
				if (should) {
					now.insert(a->GetFormID());
				}
				if (should != a->IsInFaction(mark)) {  // also clears a mark left from an earlier session
					should ? a->AddToFaction(mark, 0) : a->RemoveFromFaction(mark);
					logger::info("assassin: '{}' {:08X} {}", a->GetName(), a->GetFormID(), should ? "unsuspecting (x10)" : "no longer marked");
				}
			});
			for (auto id : g_marked) {  // out of range now
				if (!now.contains(id)) {
					if (auto* a = RE::TESForm::LookupByID<RE::Actor>(id); a && a->IsInFaction(mark)) {
						a->RemoveFromFaction(mark);
					}
				}
			}
			g_marked = std::move(now);
		}

		// ---- Infected

		std::int32_t Count(RE::Actor* a_a, RE::TESBoundObject* a_item)
		{
			const auto counts = a_a->GetInventoryCounts([a_item](RE::TESBoundObject& a_obj) { return &a_obj == a_item; });
			const auto it = counts.find(a_item);
			return it != counts.end() ? it->second : 0;
		}

		float Now() { return RE::Calendar::GetSingleton()->GetDaysPassed(); }

		// the user, 2026-11-02: the infected ignore the player's crimes; no crime faction = outside the law (the game's own
		// Actor.SetCrimeFaction, kept per actor in the save)
		void SetCrime(RE::Actor* a_a, RE::TESFaction* a_faction)
		{
			auto* vm = RE::BSScript::Internal::VirtualMachine::GetSingleton();
			if (!vm) {
				return;
			}
			const auto handle = vm->GetObjectHandlePolicy()->GetHandleForObject(a_a->GetFormType(), a_a);
			RE::BSTSmartPointer<RE::BSScript::IStackCallbackFunctor> cb;
			vm->DispatchMethodCall2(handle, "Actor", "SetCrimeFaction", RE::MakeFunctionArguments(std::move(a_faction)), cb);
		}

		void Cure(std::string_view a_why)
		{
			auto& s = State::Get();
			if (!s.infected) {
				return;
			}
			if (auto* a = RE::TESForm::LookupByID<RE::Actor>(s.infected)) {
				auto* av = a->AsActorValueOwner();
				av->SetBaseActorValue(RE::ActorValue::kAssistance, s.infectedAssistance);
				av->SetBaseActorValue(RE::ActorValue::kConfidence, s.infectedConfidence);
				if (auto* f = Registry::Get().InfectedFaction()) {
					a->RemoveFromFaction(f);
				}
				if (auto* base = a->GetActorBase(); base && base->crimeFaction) {
					SetCrime(a, base->crimeFaction);  // their own law again
				}
				logger::info("infected: '{}' {:08X} cured ({})", a->GetName(), s.infected, a_why);
				if (a_why == "the day is over" && !a->IsDead()) {
					Shift::Notify(std::format("The infection in {} has run its course.", a->GetName()));
				}
			}
			s.infected = 0;
		}

		void Infect(RE::Actor* a_a)
		{
			auto& s = State::Get();
			auto& reg = Registry::Get();
			auto* p = RE::PlayerCharacter::GetSingleton();
			if (!p || !a_a || a_a->IsDead() || !reg.InfectedFaction()) {
				return;
			}
			if (auto* item = reg.Infection(); item && Count(a_a, item) > 0) {
				a_a->RemoveItem(item, Count(a_a, item), RE::ITEM_REMOVE_REASON::kRemove, nullptr, nullptr);
			}
			if (s.infected && s.infected != a_a->GetFormID()) {
				Cure("a new one infected");
			}
			auto* av = a_a->AsActorValueOwner();
			if (s.infected != a_a->GetFormID()) {
				s.infectedAssistance = av->GetBaseActorValue(RE::ActorValue::kAssistance);
				s.infectedConfidence = av->GetBaseActorValue(RE::ActorValue::kConfidence);
			}
			s.infected = a_a->GetFormID();
			s.infectedUntil = Now() + Dial("Protean_InfectedHours", 24.0f) / 24.0f;
			a_a->AddToFaction(reg.InfectedFaction(), 0);
			av->SetBaseActorValue(RE::ActorValue::kAssistance, std::max(s.infectedAssistance, 1.0f));  // helps allies
			av->SetBaseActorValue(RE::ActorValue::kConfidence, std::max(s.infectedConfidence, kBrave));
			if (a_a->IsInCombat()) {
				if (auto t = a_a->GetActorRuntimeData().currentCombatTarget.get(); t && t.get() == p) {
					a_a->StopCombat();
				}
			}
			Shift::Notify(std::format("{} is infected and will fight at your side for a day.", a_a->GetName()));
			logger::info("infected: '{}' {:08X} (assistance was {:.0f}, confidence {:.0f}), until day {:.2f}", a_a->GetName(),
				s.infected, s.infectedAssistance, s.infectedConfidence, s.infectedUntil);
		}

		// the user, 2026-11-02: the infected never turn on the player, crimes included (a guard sided with his city when
		// the player attacked Thongvor); they fight whoever fights the player, the nearest first
		void Loyal(RE::Actor* a_a)
		{
			auto* p = RE::PlayerCharacter::GetSingleton();
			if (!p || !a_a->Is3DLoaded()) {
				return;
			}
			if (a_a->GetCrimeFaction()) {
				SetCrime(a_a, nullptr);
				logger::info("infected: '{}' {:08X} no longer answers to the law", a_a->GetName(), a_a->GetFormID());
			}
			auto target = a_a->GetActorRuntimeData().currentCombatTarget.get();
			if (a_a->IsInCombat() && target && target.get() == p) {
				a_a->StopCombat();
				a_a->StopAlarmOnActor();
				logger::info("infected: '{}' {:08X} called off the player", a_a->GetName(), a_a->GetFormID());
				target.reset();
			}
			auto* pl = RE::ProcessLists::GetSingleton();
			if (!pl) {
				return;
			}
			RE::Actor* foe = nullptr;
			float      best = kFightRange;
			for (auto& h : pl->highActorHandles) {
				auto e = h.get();
				if (!e || e.get() == a_a || e.get() == p || e->IsDead() || !e->IsInCombat()) {
					continue;
				}
				auto t = e->GetActorRuntimeData().currentCombatTarget.get();
				if (!t || t.get() != p) {
					continue;
				}
				if (const float d = a_a->GetPosition().GetDistance(e->GetPosition()); d < best) {
					best = d;
					foe = e.get();
				}
			}
			if (foe && (!target || target.get() != foe) && !(target && target.get() != p && a_a->IsInCombat())) {
				a_a->StartCombat(foe);
				logger::info("infected: '{}' {:08X} attacks '{}' {:08X}", a_a->GetName(), a_a->GetFormID(), foe->GetName(), foe->GetFormID());
			}
		}

		void Infected()
		{
			auto& s = State::Get();
			if (!s.infected) {
				return;
			}
			auto* a = RE::TESForm::LookupByID<RE::Actor>(s.infected);
			if (!a) {
				s.infected = 0;
			} else if (a->IsDead()) {
				Cure("dead");
			} else if (Now() >= s.infectedUntil) {
				Cure("the day is over");
			} else {
				if (auto* f = Registry::Get().InfectedFaction(); f && !a->IsInFaction(f)) {
					a->AddToFaction(f, 0);  // kept, whatever took it away
				}
				Loyal(a);
			}
		}

		// the Infection leaving the player's pockets for someone else's: a reverse pickpocket that worked (one caught
		// fails before the item moves)
		class Sink : public RE::BSTEventSink<RE::TESContainerChangedEvent>
		{
		public:
			RE::BSEventNotifyControl ProcessEvent(const RE::TESContainerChangedEvent* a_event, RE::BSTEventSource<RE::TESContainerChangedEvent>*) override
			{
				auto* item = Registry::Get().Infection();
				if (a_event && item && a_event->baseObj == item->GetFormID() && a_event->oldContainer == 0x14 &&
					a_event->newContainer != 0 && a_event->newContainer != 0x14) {
					const auto to = a_event->newContainer;
					SKSE::GetTaskInterface()->AddTask([to] {
						if (auto* a = RE::TESForm::LookupByID<RE::Actor>(to)) {
							Infect(a);
						}
					});
				}
				return RE::BSEventNotifyControl::kContinue;
			}
		};
	}

	void Install()
	{
		static Sink sink;
		RE::ScriptEventSourceHolder::GetSingleton()->AddEventSink<RE::TESContainerChangedEvent>(&sink);
	}

	std::optional<std::string_view> AttacksOnSight(RE::Actor* a_a, RE::PlayerCharacter* a_p)
	{
		if (auto t = a_a->GetActorRuntimeData().currentCombatTarget.get(); t && t.get() == a_p) {
			return "fighting you";
		}
		if (a_a->GetFactionReaction(a_p) == RE::FIGHT_REACTION::kEnemy) {
			return "an enemy by faction";
		}
		if (a_a->AsActorValueOwner()->GetActorValue(RE::ActorValue::kAggression) >= 2.0f) {
			return "attacks anyone";
		}
		// the engine's own answer is trusted only for those outside the law (bandits, necromancers...): it called four
		// calm Markarth guards hostile with no bounty (2026-10-28), who then talked to the player as usual
		if (!a_a->GetCrimeFaction() && a_a->IsHostileToActor(a_p)) {
			return "hostile, outside the law";
		}
		return std::nullopt;
	}

	void OnFrame(RE::PlayerCharacter* a_p, float a_delta)
	{
		static float tick = 0.0f;
		if ((tick -= a_delta) > 0.0f) {
			return;
		}
		tick = kCheckEvery;
		Inconspicuous(a_p);
		Assassin(a_p);
		Infected();
		if (auto* power = Registry::Get().MetamorphosisPower(); power && Evolve::Has("metamorphosis") && !a_p->HasSpell(power)) {
			a_p->AddSpell(power);
			logger::info("metamorphosis: the power given");
		}
		if (auto* power = Registry::Get().InfectPower(); power && Evolve::Has("infected") && !a_p->HasSpell(power)) {
			a_p->AddSpell(power);
			logger::info("infected: the Infect power given");
		}
	}

	void OnInfectCast()
	{
		auto* p = RE::PlayerCharacter::GetSingleton();
		auto* item = Registry::Get().Infection();
		logger::info("infected: Infect cast");
		if (!p || !item || !Evolve::Has("infected")) {
			logger::info("infected: Infect cast, nothing grown (item {}, perk {})", item != nullptr, Evolve::Has("infected"));
			return;
		}
		if (Count(p, item) > 0) {
			Shift::Notify("You already carry an infection.");
			logger::info("infected: Infect cast, one is carried already");
			return;
		}
		const float cost = Dial("Protean_InfectCost", 50.0f);
		auto*       av = p->AsActorValueOwner();
		if (av->GetActorValue(RE::ActorValue::kStamina) < cost) {
			Shift::Notify("You are too tired to grow an infection.");
			return;
		}
		av->DamageActorValue(RE::ActorValue::kStamina, cost);
		p->AddObjectToContainer(item, nullptr, 1, nullptr);
		logger::info("infected: an Infection grown, stamina -{:.0f}", cost);
	}
}
