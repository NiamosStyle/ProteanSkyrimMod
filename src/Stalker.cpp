// Protean - a shapeshifter mod for Skyrim SE
// Copyright (C) 2026 NiamosStyle
//
// This program is free software: you can redistribute it and/or modify it under the terms of the GNU General
// Public License as published by the Free Software Foundation, either version 3 of the License, or (at your
// option) any later version. It is distributed WITHOUT ANY WARRANTY; without even the implied warranty of
// MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the LICENSE file or <https://www.gnu.org/licenses/>.

#include "Stalker.h"

#include "Evolve.h"
#include "Feed.h"
#include "Move.h"
#include "Registry.h"
#include "Shift.h"
#include "Skills.h"
#include "State.h"

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
		std::map<RE::FormID, float> g_unseen;  // rank 2: enemies after the player, and how long they have not seen it

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

		// Stealth and Silence while faded: the movement noise drops by Stealth's share, or to nothing with Silence (a
		// temporary modifier whose size the co-save keeps, State::stalkerNoiseApplied)
		void Quiet(RE::PlayerCharacter* a_p)
		{
			auto&       s = State::Get();
			auto*       av = a_p->AsActorValueOwner();
			const float own = av->GetActorValue(RE::ActorValue::kMovementNoiseMult) - s.stalkerNoiseApplied;
			const float want = g_faded ? -Skills::StealthShare(a_p) * std::max(own, 0.0f) : 0.0f;
			if (std::abs(want - s.stalkerNoiseApplied) > 0.001f) {
				av->ModActorValue(RE::ACTOR_VALUE_MODIFIER::kTemporary, RE::ActorValue::kMovementNoiseMult, want - s.stalkerNoiseApplied);
				s.stalkerNoiseApplied = want;
				logger::info("stalker: movement noise {:+.2f}", want);
			}
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

		// every living actor fighting the player right now
		template <class F>
		void ForEachHunter(RE::PlayerCharacter* a_p, F&& a_fn)
		{
			auto* pl = RE::ProcessLists::GetSingleton();
			if (!pl) {
				return;
			}
			const auto me = a_p->GetHandle();
			for (auto& h : pl->highActorHandles) {
				auto a = h.get();
				if (!a || a.get() == a_p || a->IsDead() || !a->IsInCombat()) {
					continue;
				}
				auto* cc = a->GetActorRuntimeData().combatController;
				auto  t = a->GetActorRuntimeData().currentCombatTarget.get();
				if ((t && t.get() == a_p) || (cc && cc->targetHandle == me)) {
					a_fn(a.get());
				}
			}
		}

		bool Sees(RE::Actor* a_a, RE::PlayerCharacter* a_p)
		{
			bool unused = false;
			return a_a->HasLineOfSight(a_p, unused);
		}

		// rank 2 (the user, 2026-11-02): the fade may start in a fight while no enemy has the player in sight
		bool UnseenInFight(RE::PlayerCharacter* a_p)
		{
			bool seen = false;
			ForEachHunter(a_p, [&](RE::Actor* a) { seen = seen || Sees(a, a_p); });
			return !seen;
		}

		// rank 2: faded, an enemy that has not seen the player for Protean_StalkerLoseSeconds gives up the hunt
		void LoseTrack(RE::PlayerCharacter* a_p, float a_delta)
		{
			if (!g_faded || !Evolve::Has("stalker_2")) {
				g_unseen.clear();
				return;
			}
			std::map<RE::FormID, float> now;
			std::vector<RE::Actor*>     quit;
			ForEachHunter(a_p, [&](RE::Actor* a) {
				// faded, the game's own detection fails too: either loses the player
				const bool seen = Sees(a, a_p) && a->RequestDetectionLevel(a_p) > 0;
				const float t = seen ? 0.0f : g_unseen[a->GetFormID()] + a_delta;
				if (t >= Dial("Protean_StalkerLoseSeconds", 3.0f)) {
					quit.push_back(a);
				} else {
					now[a->GetFormID()] = t;
				}
			});
			g_unseen = std::move(now);
			for (auto* a : quit) {
				a->StopCombat();
				a->StopAlarmOnActor();
				logger::info("stalker: '{}' {:08X} lost track of the player", a->GetName(), a->GetFormID());
			}
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
			if (a_p->IsDead()) {
				return "dead";
			}
			if (a_p->IsInCombat()) {
				if (!Evolve::Has("stalker_2")) {
					return "in a fight";
				}
				// faded already, the fight goes on around the player until the enemies give up
				if (!g_faded && !UnseenInFight(a_p)) {
					return "in a fight, seen";
				}
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
		Quiet(a_p);
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
			g_unseen.clear();
			return;
		}
		LoseTrack(a_p, a_delta);
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
		g_unseen.clear();
	}

	void OnGameLoaded()
	{
		if (auto* p = RE::PlayerCharacter::GetSingleton()) {
			ForEachFade(p, [](RE::ActiveEffect* a_ae) { a_ae->Dispel(true); });
			SetAmbush(p, false);
		}
		Reset();
		if (auto* p = RE::PlayerCharacter::GetSingleton()) {
			Quiet(p);  // not faded: whatever noise change the save held comes off
		}
	}
}
