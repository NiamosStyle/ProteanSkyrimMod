#include "Stalker.h"

#include "Evolve.h"
#include "Feed.h"
#include "Move.h"
#include "Registry.h"
#include "Shift.h"

namespace protean::Stalker
{
	namespace
	{
		constexpr float kAmbushWindow = 1.5f;  // seconds from the swing to its hit that the ambush perk is held

		bool        g_on = false;     // Stalk is switched on
		float       g_tick = 0.0f;    // seconds to the next check of the Stalk power
		float       g_still = 0.0f;   // seconds the player has been still enough to fade
		bool        g_faded = false;  // Protean cast the fade and has not ended it
		float       g_ambush = 0.0f;  // seconds left of the ambush window (0 = none)
		std::string g_why;            // why the fade cannot start, as last logged

		float Dial(std::string_view a_name, float a_fallback) { return Registry::Get().Dial(a_name, a_fallback); }

		template <class F>
		void ForEachFade(RE::PlayerCharacter* a_p, F&& a_fn)
		{
			auto* spell = Registry::Get().StalkerFade();
			auto* list = a_p->AsMagicTarget()->GetActiveEffectList();
			if (!list || !spell) {
				return;
			}
			for (auto* ae : *list) {
				if (ae && ae->spell == spell && !ae->flags.all(RE::ActiveEffect::Flag::kDispelled)) {
					a_fn(ae);
				}
			}
		}

		bool FadeOn(RE::PlayerCharacter* a_p)
		{
			bool on = false;
			ForEachFade(a_p, [&](RE::ActiveEffect*) { on = true; });
			return on;
		}

		void End(RE::PlayerCharacter* a_p, std::string_view a_why)
		{
			ForEachFade(a_p, [](RE::ActiveEffect* a_ae) { a_ae->Dispel(true); });
			if (g_faded) {
				logger::info("stalker: visible again ({})", a_why);
			}
			g_faded = false;
			g_still = 0.0f;
		}

		void SetAmbush(RE::PlayerCharacter* a_p, bool a_on)
		{
			auto* perk = Registry::Get().StalkerAmbush();
			if (!perk || a_p->HasPerk(perk) == a_on) {
				return;
			}
			if (a_on) {
				a_p->AddPerk(perk);
			} else {
				a_p->RemovePerk(perk);
			}
		}

		enum class Gait
		{
			kStill,
			kWalk,
			kRun,
			kSprint
		};

		Gait Pace(RE::PlayerCharacter* a_p)
		{
			auto* pc = RE::PlayerControls::GetSingleton();
			auto* st = a_p->AsActorState();
			if (!pc || pc->data.moveInputVec.Length() <= 0.1f) {
				return Gait::kStill;
			}
			if (Move::SprintHeld() || (st && st->IsSprinting())) {
				return Gait::kSprint;
			}
			return st && st->IsRunning() ? Gait::kRun : Gait::kWalk;
		}

		// what keeps the player from fading now ("" = nothing)
		std::string Blocker(RE::PlayerCharacter* a_p)
		{
			if (!Evolve::Has("stalker") || !Shift::ActiveCreature()) {
				return "not a Stalker animal";
			}
			if (!g_on) {
				return "Stalk is off";
			}
			if (a_p->IsDead() || a_p->IsInCombat()) {
				return "in a fight";
			}
			if (a_p->IsInMidair()) {
				return "in the air";
			}
			if (Feed::Eating()) {
				return "eating";
			}
			if (a_p->AsActorValueOwner()->GetActorValue(RE::ActorValue::kStamina) <= 0.0f) {
				return "no stamina";
			}
			if (Pace(a_p) == Gait::kSprint) {
				return "sprinting";
			}
			return "";
		}
	}

	void OnFrame(RE::PlayerCharacter* a_p, float a_delta)
	{
		if (g_ambush > 0.0f && (g_ambush -= a_delta) <= 0.0f) {
			g_ambush = 0.0f;
			SetAmbush(a_p, false);
			logger::info("stalker: the ambush bite missed");
		}
		// the Stalk power is known only by an animal with the perk; stalking ends with the animal form
		const bool stalker = Shift::ActiveCreature() && Evolve::Has("stalker");
		if ((g_tick -= a_delta) <= 0.0f) {
			g_tick = 0.5f;
			if (auto* power = Registry::Get().StalkerPower(); power && stalker != a_p->HasSpell(power)) {
				if (stalker) {
					a_p->AddSpell(power);
				} else {
					a_p->RemoveSpell(power);
				}
			}
		}
		if (!stalker && g_on) {
			g_on = false;
			logger::info("stalker: Stalk off (no longer a Stalker animal)");
		}
		const auto why = Blocker(a_p);
		if (why != g_why && stalker) {
			logger::info("stalker: {}", why.empty() ? "still enough to fade" : why);
		}
		g_why = why;
		if (g_faded && !FadeOn(a_p)) {
			g_faded = false;  // the game ended it (an attack, an activation)
			g_still = 0.0f;
			logger::info("stalker: visible again (the game ended the fade)");
		}
		if (!why.empty()) {
			End(a_p, why);
			return;
		}
		const auto gait = Pace(a_p);
		if (!g_faded) {
			// the fade starts only from a stand or a walk; once faded, a run keeps it
			if (gait == Gait::kRun) {
				g_still = 0.0f;
			} else if ((g_still += a_delta) >= Dial("Protean_StalkerFadeSeconds", 2.0f)) {
				auto* spell = Registry::Get().StalkerFade();
				auto* caster = a_p->GetMagicCaster(RE::MagicSystem::CastingSource::kInstant);
				if (spell && caster) {
					caster->CastSpellImmediate(spell, false, a_p, 1.0f, false, 0.0f, a_p);
					g_faded = true;
					logger::info("stalker: faded out");
				}
			}
			return;
		}
		// only moving costs (the user's call, 2026-10-24: a lion creeping up on its prey; waiting lets Stamina come back)
		if (gait == Gait::kStill) {
			return;
		}
		auto* av = a_p->AsActorValueOwner();
		av->DamageActorValue(RE::ActorValue::kStamina,
			Dial("Protean_StalkerDrain", 2.0f) * (gait == Gait::kRun ? Dial("Protean_StalkerRunMult", 2.0f) : 1.0f) * a_delta);
		if (av->GetActorValue(RE::ActorValue::kStamina) <= 0.0f) {
			End(a_p, "no stamina");
		}
	}

	void OnPowerCast()
	{
		auto* p = RE::PlayerCharacter::GetSingleton();
		if (!p) {
			return;
		}
		if (!Shift::ActiveCreature()) {
			Shift::Notify("Stalk works only in animal form.");
			return;
		}
		g_on = !g_on;
		Shift::Notify(g_on ? "You stalk: keep still or walk to fade from sight." : "You stop stalking.");
		logger::info("stalker: Stalk {}", g_on ? "on" : "off");
		if (!g_on) {
			End(p, "Stalk off");
		}
	}

	void OnAttack(bool a_hidden)
	{
		auto* p = RE::PlayerCharacter::GetSingleton();
		auto* perk = Registry::Get().StalkerAmbush();
		if (!p || !a_hidden || !perk || perk->perkEntries.empty()) {
			return;
		}
		// the perk's one entry multiplies attack damage; its value is live data, set from the dial at each ambush
		if (auto* entry = static_cast<RE::BGSEntryPointPerkEntry*>(perk->perkEntries[0]); entry && entry->functionData) {
			if (auto* one = skyrim_cast<RE::BGSEntryPointFunctionDataOneValue*>(entry->functionData)) {
				one->data = Dial("Protean_StalkerAmbushMult", 2.0f);
			}
		}
		SetAmbush(p, true);
		g_ambush = kAmbushWindow;
		logger::info("stalker: ambush bite thrown");
		// the game ends the fade only when a bite lands (seen 2026-10-24: a miss left the wolf invisible), so the swing does
		End(p, "attacked");
	}

	bool Hidden() { return g_faded; }

	namespace
	{
		class Sink : public RE::BSTEventSink<RE::TESHitEvent>
		{
		public:
			RE::BSEventNotifyControl ProcessEvent(const RE::TESHitEvent* a_event, RE::BSTEventSource<RE::TESHitEvent>*) override
			{
				if (!a_event || !a_event->cause || !a_event->target) {
					return RE::BSEventNotifyControl::kContinue;
				}
				if (a_event->cause->IsPlayerRef() && a_event->target.get() != a_event->cause.get()) {
					std::string name = a_event->target->GetName();
					SKSE::GetTaskInterface()->AddTask([name] {
						if (auto* p = RE::PlayerCharacter::GetSingleton(); p && g_ambush > 0.0f) {
							g_ambush = 0.0f;
							SetAmbush(p, false);
							logger::info("stalker: ambush bite landed on '{}'", name);
						}
					});
				} else if (a_event->target->IsPlayerRef()) {
					SKSE::GetTaskInterface()->AddTask([] {
						if (auto* p = RE::PlayerCharacter::GetSingleton(); p && (g_faded || g_still > 0.0f)) {
							End(p, "hit");
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
		RE::ScriptEventSourceHolder::GetSingleton()->AddEventSink<RE::TESHitEvent>(&sink);
	}

	void Reset()
	{
		g_on = false;
		g_still = 0.0f;
		g_faded = false;
		g_ambush = 0.0f;
		g_why.clear();
	}

	void OnGameLoaded()
	{
		if (auto* p = RE::PlayerCharacter::GetSingleton()) {
			ForEachFade(p, [](RE::ActiveEffect* a_ae) { a_ae->Dispel(true); });
			SetAmbush(p, false);
		}
		Reset();
	}
}
