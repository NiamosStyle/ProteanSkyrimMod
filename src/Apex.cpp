#include "Apex.h"

#include "Evolve.h"
#include "Registry.h"
#include "Shift.h"
#include "State.h"

namespace protean::Apex
{
	namespace
	{
		struct Skip
		{
			const char*   file;
			std::uint32_t local;
			const char*   why;
		};
		constexpr Skip kSkip[] = {
			{ "Skyrim.esm", 0x13, "CreatureFaction: every monster, not only animals" },
			{ "Skyrim.esm", 0x2E893, "PredatorFaction: its members are enemies of PreyFaction" },
			{ "Skyrim.esm", 0x2E894, "PreyFaction: predators hunt its members" },
			{ "Skyrim.esm", 0x3E093, "SprigganPredatorFaction" },
			{ "Skyrim.esm", 0x3E095, "SprigganPreyFaction" },
			{ "Skyrim.esm", 0x1BCC0, "BanditFaction (a pit wolf's)" },
			{ "Dragonborn.esm", 0x1D9C9, "DLC2RieklingFaction (a boar's)" },
			{ "Dawnguard.esm", 0x18C4F, "DLC1VampireCompanionFaction (a death hound's)" },
			{ "Skyrim.esm", 0x91822, "PlayerWerewolfFaction: guards attack its members on sight" },
		};

		bool Skipped(const RE::TESFaction* a_f)
		{
			const auto* file = a_f->GetFile(0);
			const auto  local = a_f->GetLocalFormID();
			for (const auto& s : kSkip) {
				if (s.local == local && file && _stricmp(file->fileName, s.file) == 0) {
					return true;
				}
			}
			return false;
		}

		float g_wait{ 0.0f };

		// animals the player hit, and when: the aura leaves them alone for a while (a calm breaks when it is hit, and
		// would otherwise be put back on the animal the player is fighting)
		std::mutex                                                     g_hitLock;
		std::map<RE::FormID, std::chrono::steady_clock::time_point> g_hit;

		class HitSink : public RE::BSTEventSink<RE::TESHitEvent>
		{
		public:
			RE::BSEventNotifyControl ProcessEvent(const RE::TESHitEvent* a_event, RE::BSTEventSource<RE::TESHitEvent>*) override
			{
				if (a_event && a_event->cause && a_event->cause->IsPlayerRef() && a_event->target && !a_event->target->IsPlayerRef()) {
					std::lock_guard lock(g_hitLock);
					g_hit[a_event->target->GetFormID()] = std::chrono::steady_clock::now();
				}
				return RE::BSEventNotifyControl::kContinue;
			}
		};

		bool HitRecently(RE::FormID a_id, float a_seconds)
		{
			std::lock_guard lock(g_hitLock);
			const auto      it = g_hit.find(a_id);
			if (it == g_hit.end()) {
				return false;
			}
			if (std::chrono::steady_clock::now() - it->second > std::chrono::duration<float>(a_seconds)) {
				g_hit.erase(it);
				return false;
			}
			return true;
		}

		// Until 2026-11-19 Apex put the player in every animal's own faction, and many of those are friends of people and
		// monsters (Goat: Forsworn; Skeever, Spider: draugr, Falmer, skeletons, vampires; Chaurus: Falmer, hagravens...),
		// so they all left the player alone (Mercer, 2026-11-19). Taken off once, in human form only, and never the
		// factions of the form the player is in now (Shift's addedFactions).
		void ClearOldKin(RE::PlayerCharacter* a_p)
		{
			auto& s = State::Get();
			if (s.apexFactionsCleared || !s.active.empty() || Registry::Get().ByRace(a_p->GetRace())) {
				return;
			}
			int  left = 0;
			auto leave = [&](RE::TESFaction* f) {
				if (f && a_p->IsInFaction(f) && std::ranges::find(s.addedFactions, f->GetFormID()) == s.addedFactions.end()) {
					a_p->RemoveFromFaction(f);
					logger::info("apex: left the old kin faction {:08X} '{}'", f->GetFormID(), f->GetName());
					++left;
				}
			};
			for (const auto& c : Registry::Get().All()) {
				for (auto* f : c.factions) {
					if (f && !Skipped(f)) {
						leave(f);
					}
				}
				leave(c.kin);
			}
			leave(Registry::Get().KinFaction());
			s.apexFactionsCleared = true;
			logger::info("apex: the old faction kin cleared ({} faction(s) left); the kin aura takes its place", left);
		}

		// The kin aura: every animal of the creature table within range is calmed towards the player (a hidden Calm, as
		// Kyne's Peace without its shader), recast each second for 3 s so it never lapses while the animal is near. Left
		// alone: the Call pack and other followers, summons, animals the player hit lately, and animals busy fighting
		// something else (wolves still hunt deer).
		void Aura(RE::PlayerCharacter* a_p)
		{
			auto* spell = Registry::Get().ApexCalm();
			const float range = Registry::Get().Dial("Protean_ApexAuraRange", 4000.0f);
			const float forget = Registry::Get().Dial("Protean_ApexAuraForgetSeconds", 60.0f);
			auto*       pl = RE::ProcessLists::GetSingleton();
			auto*       caster = a_p->GetMagicCaster(RE::MagicSystem::CastingSource::kInstant);
			if (!spell || range <= 0.0f || !pl || !caster) {
				return;
			}
			int calmed = 0, stood = 0;
			for (auto& h : pl->highActorHandles) {
				auto a = h.get();
				if (!a || a.get() == a_p || a->IsDead() || a->IsPlayerTeammate() || a->IsCommandedActor()) {
					continue;
				}
				if (!Registry::Get().ByRace(a->GetRace()) || a_p->GetPosition().GetDistance(a->GetPosition()) > range ||
					HitRecently(a->GetFormID(), forget)) {
					continue;
				}
				auto target = a->GetActorRuntimeData().currentCombatTarget.get();
				const bool onPlayer = target && target.get() == a_p;
				if (a->IsInCombat() && target && !onPlayer) {
					continue;  // its own fight
				}
				caster->CastSpellImmediate(spell, false, a.get(), 1.0f, false, 0.0f, a_p);
				++calmed;
				if (onPlayer) {
					a->StopCombat();
					++stood;
					logger::info("apex: '{}' {:08X} was after the player: calmed", a->GetName(), a->GetFormID());
				}
			}
			static int last = -1;
			if (calmed != last) {
				logger::info("apex: kin aura on {} animal(s) within {:.0f}", calmed, range);
				last = calmed;
			}
		}
	}

	bool Owned() { return Evolve::Has("apex"); }

	void OnFrame(RE::PlayerCharacter* a_p, float a_delta)
	{
		g_wait -= a_delta;
		if (g_wait > 0.0f) {
			return;
		}
		g_wait = 1.0f;
		if (!a_p) {
			return;
		}
		// the Call power is known only by an animal with the perk
		if (auto* power = Registry::Get().CallPower()) {
			const bool want = Owned() && Shift::ActiveCreature();
			if (want != a_p->HasSpell(power)) {
				if (want) {
					a_p->AddSpell(power);
				} else {
					a_p->RemoveSpell(power);
				}
			}
		}
		static bool sinkAdded = false;
		if (!sinkAdded) {
			static HitSink sink;
			RE::ScriptEventSourceHolder::GetSingleton()->AddEventSink<RE::TESHitEvent>(&sink);
			sinkAdded = true;
		}
		ClearOldKin(a_p);
		if (Owned()) {
			Aura(a_p);
		}
	}
}
