#include "Attacks.h"
#include "Feed.h"

#include "Move.h"
#include "Registry.h"
#include "Shift.h"
#include "Stalker.h"

namespace protean::Attacks
{
	namespace
	{
		constexpr float kHoldSeconds = 0.4f;
		constexpr auto  kIni = "Data/SKSE/Plugins/Protean.ini";

		bool g_enabled{ true };

		struct Press
		{
			bool                                  down{ false };
			bool                                  fired{ false };
			bool                                  left{ false };
			std::chrono::steady_clock::time_point since{};
		};
		Press g_press;

		// Sending the attack event plays the move, but the engine only applies the attack's damage multiplier,
		// power-attack flag and on-hit spells when its attack data is set; an animal's AI sets it through the
		// attack action, which finds nothing for the player (measured live), so it is set here.
		// The player's own copies of an animal's attack data, with a wider hit cone. Wild animals of the same
		// race keep using the originals.
		std::unordered_map<RE::BGSAttackData*, RE::NiPointer<RE::BGSAttackData>> g_copies;

		// a_strikeAngle: Protean's cone for this form (0 = the attack's own), then times the Protean_StrikeAngleMult dial
		RE::BGSAttackData* PlayerCopy(RE::BGSAttackData* a_data, float a_strikeAngle, float a_mult)
		{
			const float angle = (a_strikeAngle > a_data->data.strikeAngle ? a_strikeAngle : a_data->data.strikeAngle) * a_mult;
			if (angle == a_data->data.strikeAngle) {
				return a_data;
			}
			auto& copy = g_copies[a_data];
			if (!copy) {
				copy.reset(RE::BGSAttackData::Create());
				if (!copy) {
					return a_data;
				}
				copy->event = a_data->event;
				copy->data = a_data->data;
			}
			copy->data.strikeAngle = angle;  // the dial may have changed since the copy was made
			return copy.get();
		}

		void SetAttackData(RE::Actor* a_actor, const std::string& a_event, float a_strikeAngle, float a_mult)
		{
			auto* race = a_actor->GetRace();
			auto* proc = a_actor->GetActorRuntimeData().currentProcess;
			if (!race || !race->attackDataMap || !proc || !proc->high) {
				return;
			}
			auto& map = race->attackDataMap->attackDataMap;
			if (auto it = map.find(RE::BSFixedString(a_event)); it != map.end() && it->second) {
				proc->high->attackData.reset(PlayerCopy(it->second.get(), a_strikeAngle, a_mult));
			}
		}

		// The race's reach is shared with every wild animal of that race and the engine reads no per-actor
		// value, so it is raised only while the player's attack plays and put back afterwards.
		constexpr float kReachSeconds = 1.5f;  // longest measured start-to-impact: 0.76 s (wolf lunge)

		struct Reach
		{
			RE::TESRace*                          race{ nullptr };
			float                                 original{ 0.0f };
			std::chrono::steady_clock::time_point until{};
		};
		Reach g_reach;

		void RestoreReach()
		{
			if (g_reach.race) {
				g_reach.race->data.unarmedReach = g_reach.original;
				g_reach = Reach{};
			}
		}

		void RaiseReach(RE::TESRace* a_race, float a_reach)
		{
			if (!a_race || a_reach <= 0.0f) {
				return;
			}
			if (g_reach.race != a_race) {
				RestoreReach();
				g_reach.race = a_race;
				g_reach.original = a_race->data.unarmedReach;
			}
			a_race->data.unarmedReach = std::max(a_reach, g_reach.original);
			g_reach.until = std::chrono::steady_clock::now() + std::chrono::milliseconds(static_cast<int>(kReachSeconds * 1000));
		}

		void LoadIni()
		{
			std::ifstream in(kIni);
			std::string   line;
			bool          inSection = false;
			while (std::getline(in, line)) {
				if (!line.empty() && line[0] == '[') {
					inSection = line.starts_with("[Attacks]");
				} else if (inSection && line.starts_with("Translate=")) {
					g_enabled = line.size() > 10 && line[10] == '1';
				}
			}
		}

		// Each animal has three moves: LMB = its first normal attack, RMB = its second (or the first when it has
		// only one), hold = its first power attack (second on RMB). Order comes from the animal's attack list.
		const Attack* Pick(const Creature& a_c, bool a_power, bool a_rmb)
		{
			// straight-ahead taps where the side-swipe rule below picks wrongly (bears: user)
			if (!a_power && !a_c.taps.empty()) {
				const auto& want = a_c.taps[a_rmb && a_c.taps.size() > 1 ? 1 : 0];
				if (auto it = std::ranges::find(a_c.attacks, want, &Attack::event); it != a_c.attacks.end()) {
					return &*it;
				}
			}
			std::vector<const Attack*> normal, power;
			for (const auto& a : a_c.attacks) {
				if (a.bash || a.event.empty()) {
					continue;
				}
				(a.power ? power : normal).push_back(&a);
			}
			// an attack should go at the enemy: the troll's power list starts with a leap backwards, the giant's
			// normal list with a swing behind it
			auto forwards = [](const Attack* a) {
				return a->event.find("Backward") == std::string::npos && a->event.find("BackAttack") == std::string::npos;
			};
			std::ranges::stable_partition(normal, forwards);
			std::ranges::stable_partition(power, forwards);
			const auto& pool = (a_power && !power.empty()) ? power : normal;
			if (pool.empty()) {
				return nullptr;
			}
			// Skyrim's left mouse button is the right hand: with both paws in the list (sabre cat, troll, horker),
			// the left button takes the first right-side swipe and the right button the first left-side one
			auto side = [&](std::string_view a_side) {
				return std::ranges::find_if(pool, [&](const Attack* x) { return x->event.find(a_side) != std::string::npos; });
			};
			if (auto r = side("Right"), l = side("Left"); r != pool.end() && l != pool.end()) {
				return a_rmb ? *l : *r;
			}
			return pool[(a_rmb && pool.size() > 1) ? 1 : 0];
		}

		// A spell attack (the spider's spit): the wild animal's AI casts it as a spell; here its animation is
		// played and the spell cast from the player. Measured on the spider (hands readied): "spitStanceStart"
		// rears it up, "MLH_SpellRelease_event" a moment later plays the spit and drops it back to its normal
		// stance; its own cast event is refused for the player. So the events in spellEvents are sent in turn,
		// spellStanceDelay apart, and the spell is cast spellDelay after the last.
		struct Spit
		{
			const Creature*                       c{ nullptr };
			std::size_t                           next{ 0 };  // the next event to send; past the end: cast
			std::chrono::steady_clock::time_point at{};
		};
		Spit g_spit;

		auto After(float a_seconds)
		{
			return std::chrono::steady_clock::now() + std::chrono::milliseconds(static_cast<int>(a_seconds * 1000));
		}

		void StartSpit(const Creature& a_c, RE::PlayerCharacter* a_p)
		{
			// costs half a power attack (user decision: it does about half the power bite's damage; the spell's own
			// 3 magicka was nothing). Measured: the spider's power bite took ~80 of 100 stamina = 40 (UESP's
			// unarmed base) x fPowerAttackStaminaPenalty (2) x the attack's stamina mult (1)
			auto*       av = a_p->AsActorValueOwner();
			auto*       gs = RE::GameSettingCollection::GetSingleton();
			auto*       pen = gs ? gs->GetSetting("fPowerAttackStaminaPenalty") : nullptr;
			const float cost = Registry::Get().Dial("Protean_SpitCostMult", 0.5f) * 40.0f * (pen ? pen->GetFloat() : 2.0f);
			if (av->GetActorValue(RE::ActorValue::kStamina) < cost) {
				logger::info("attack: '{}' spell needs {:.0f} stamina, has {:.0f}", a_c.id, cost, av->GetActorValue(RE::ActorValue::kStamina));
				RE::HUDMenu::FlashMeter(RE::ActorValue::kStamina);
				return;
			}
			av->DamageActorValue(RE::ActorValue::kStamina, cost);
			logger::info("attack: '{}' spell costs {:.0f} stamina", a_c.id, cost);
			if (auto* st = a_p->AsActorState(); st && !st->IsWeaponDrawn()) {
				a_p->DrawWeaponMagicHands(true);
			}
			g_spit = { &a_c, 0, std::chrono::steady_clock::now() };
		}

		void StepSpit()
		{
			auto* p = RE::PlayerCharacter::GetSingleton();
			const auto* c = g_spit.c;
			if (!p || p->IsDead() || c != Shift::ActiveCreature()) {
				g_spit = {};
				return;
			}
			Move::FaceCamera(p);
			if (g_spit.next < c->spellEvents.size()) {
				const auto& e = c->spellEvents[g_spit.next++];
				const bool  ok = p->NotifyAnimationGraph(e);
				logger::info("attack: '{}' spell step {} -> graph accepted: {}", c->id, e, ok);
				g_spit.at = After(g_spit.next < c->spellEvents.size() ? c->spellStanceDelay : c->spellDelay);
				return;
			}
			if (auto* caster = p->GetMagicCaster(RE::MagicSystem::CastingSource::kInstant)) {
				caster->CastSpellImmediate(c->rmbSpell, false, nullptr, 1.0f, false, 0.0f, p);
				logger::info("attack: '{}' cast {:08X}", c->id, c->rmbSpell->GetFormID());
			}
			g_spit = {};
		}

		void Fire(bool a_power)
		{
			const auto* c = Shift::ActiveCreature();
			auto*       p = RE::PlayerCharacter::GetSingleton();
			if (!c || !p) {
				return;
			}
			if (a_power && g_press.left && c->rmbSpell) {
				if (!g_spit.c) {
					StartSpit(*c, p);
				}
				return;
			}
			// g_press.left is the game's "Left Attack/Block" action, i.e. the right mouse button
			const Attack* a = Pick(*c, a_power, g_press.left);
			if (!a) {
				logger::info("attack: '{}' has no usable {} attack", c->id, a_power ? "power" : "normal");
				return;
			}
			if (auto* st = p->AsActorState(); st && !st->IsWeaponDrawn()) {
				p->DrawWeaponMagicHands(true);
			}
			Move::FaceCamera(p);  // the mouse turns only the camera, so turn to it before striking
			const bool hidden = Stalker::Hidden();  // the swing ends the fade (the game's rule): ask first
			bool       ok = p->NotifyAnimationGraph(a->event);
			if (!ok && !c->stance.empty()) {
				// the giant attacks only from its combat stance; from a walk, stop first (wild giants stand to strike)
				ok = (p->NotifyAnimationGraph(c->stance) && p->NotifyAnimationGraph(a->event)) ||
				     (p->NotifyAnimationGraph("moveStop") && p->NotifyAnimationGraph(c->stance) && p->NotifyAnimationGraph(a->event));
			}
			if (ok) {
				// the dials scale Protean's tuning; a form left at its race's own values scales from those
				const auto& reg = Registry::Get();
				const float angleMult = reg.Dial("Protean_StrikeAngleMult", 1.0f);
				const float reachMult = reg.Dial("Protean_ReachMult", 1.0f);
				auto*       race = p->GetRace();
				const float baseReach = c->reach > 0.0f ? c->reach : (race ? (g_reach.race == race ? g_reach.original : race->data.unarmedReach) : 0.0f);
				SetAttackData(p, a->event, c->strikeAngle, angleMult);
				RaiseReach(race, baseReach * reachMult);
				Stalker::OnAttack(hidden);
			}
			logger::info("attack: '{}' {} {} -> graph accepted: {}", c->id, a_power ? "power" : "normal", a->event, ok);
		}

		// The game's own attack on a stance creature (the giant) plays an empty-handed wave that leaves the graph busy
		// and refusing Protean's attack. Keep the attack buttons from the game's attack handler while one is active;
		// Protean's own sink still sees them. (Taking the fighting controls instead also stopped walking and Z.)
		struct AttackCanProcess
		{
			static bool thunk(RE::AttackBlockHandler* a_this, RE::InputEvent* a_event)
			{
				if (const auto* c = Shift::ActiveCreature(); g_enabled && c && !c->stance.empty()) {
					return false;
				}
				return func(a_this, a_event);
			}
			static inline REL::Relocation<decltype(thunk)> func;
			static constexpr std::size_t                   idx = 0x1;
		};

		class Sink : public RE::BSTEventSink<RE::InputEvent*>
		{
		public:
			static Sink* Get()
			{
				static Sink s;
				return &s;
			}

			RE::BSEventNotifyControl ProcessEvent(RE::InputEvent* const* a_event, RE::BSTEventSource<RE::InputEvent*>*) override
			{
				if (!g_enabled || !a_event || !Shift::ActiveCreature()) {
					return RE::BSEventNotifyControl::kContinue;
				}
				const auto* ue = RE::UserEvents::GetSingleton();
				for (auto* e = *a_event; e; e = e->next) {
					auto* b = e->AsButtonEvent();
					if (!b) {
						continue;
					}
					// With the game's attack controls taken (the giant), the button no longer carries its action name:
					// recognise it by the key it is bound to instead.
					auto bound = [&](const RE::BSFixedString& a_action) {
						auto* cm = RE::ControlMap::GetSingleton();
						return b->userEvent == a_action ||
						       (cm && b->GetIDCode() == cm->GetMappedKey(a_action, b->GetDevice()) && b->GetIDCode() != 0xFF);
					};
					// Activate is one of the controls taken in animal form, so it too is recognised by its key
					if (b->IsDown() && bound(ue->activate)) {
						// doors, containers, talking, picking things up: blocked in animal form (user decision); with
						// Survivor 2, eating a body or a plant in reach
						logger::info("activate pressed in animal form");
						SKSE::GetTaskInterface()->AddTask([] {
							if (Feed::OnActivate()) {
								return;
							}
							static auto last = std::chrono::steady_clock::time_point{};
							const auto  now = std::chrono::steady_clock::now();
							if (now - last > std::chrono::seconds(3)) {
								last = now;
								Shift::Notify("You cannot do that in animal form. Change back first.");
							}
						});
						continue;
					}
					if (b->IsDown() && bound(ue->shout)) {
						SKSE::GetTaskInterface()->AddTask([] { Shift::OnPowerKey(); });
						continue;
					}
					const bool isLeft = bound(ue->leftAttack);
					const bool isRight = bound(ue->rightAttack);
					if (!isLeft && !isRight) {
						continue;
					}
					if (b->IsDown()) {
						g_press = Press{ true, false, isLeft, std::chrono::steady_clock::now() };
					} else if (b->IsUp() && g_press.down) {
						if (!g_press.fired) {
							Fire(false);  // released before the hold time: a normal attack
						}
						g_press = Press{};
					}
				}
				return RE::BSEventNotifyControl::kContinue;
			}
		};
	}

	void Install()
	{
		LoadIni();
		if (auto* idm = RE::BSInputDeviceManager::GetSingleton()) {
			idm->AddEventSink<RE::InputEvent*>(Sink::Get());
		}
		REL::Relocation<std::uintptr_t> vtbl{ RE::VTABLE_AttackBlockHandler[0] };
		AttackCanProcess::func = vtbl.write_vfunc(AttackCanProcess::idx, AttackCanProcess::thunk);
		logger::info("attack translation {}", g_enabled ? "on" : "off");
	}

	void OnFrame()
	{
		if (g_reach.race && std::chrono::steady_clock::now() >= g_reach.until) {
			RestoreReach();
		}
		if (g_spit.c && std::chrono::steady_clock::now() >= g_spit.at) {
			StepSpit();
		}
		if (!g_press.down || g_press.fired) {
			return;
		}
		const float held = std::chrono::duration<float>(std::chrono::steady_clock::now() - g_press.since).count();
		if (held >= kHoldSeconds) {
			g_press.fired = true;
			Fire(true);  // still held after the hold time: a power attack
		}
	}

	void TestFire(bool a_power, bool a_rmb)
	{
		g_press = Press{ false, false, a_rmb, {} };
		Fire(a_power);
		g_press = Press{};
	}

	void Cancel()
	{
		g_press = Press{};
		g_spit = {};
		RestoreReach();
	}}
