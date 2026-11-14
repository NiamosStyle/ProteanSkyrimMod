#include "Apex.h"

#include "Evolve.h"
#include "Registry.h"
#include "Shift.h"

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
		if (!Owned()) {
			return;
		}
		int joined = 0;
		for (const auto& c : Registry::Get().All()) {
			for (auto* f : c.factions) {
				if (f && !Skipped(f) && !a_p->IsInFaction(f)) {
					a_p->AddToFaction(f, 0);
					logger::info("apex: kin of the '{}' faction ({:08X}, {})", c.name, f->GetFormID(), c.id);
					++joined;
				}
			}
		}
		// Some animals have no faction of their own, only the broad groups left out above: mudcrab, ash hopper, horse,
		// fox, death hound, boar (found when mudcrabs still attacked, 2026-11-14). Each has a Protean faction of its own:
		// the player joins them all, and an animal that comes near is added to its own species' only, so fights between
		// kinds stay as vanilla has them. (A first try put every animal in ONE shared faction, which made them all allies
		// of each other; anyone still in it is taken back out.)
		auto* shared = Registry::Get().KinFaction();
		if (shared && a_p->IsInFaction(shared)) {
			a_p->RemoveFromFaction(shared);
		}
		for (const auto& c : Registry::Get().All()) {
			if (c.kin && !a_p->IsInFaction(c.kin)) {
				a_p->AddToFaction(c.kin, 0);
				logger::info("apex: kin of the '{}' (Protean faction {:08X})", c.name, c.kin->GetFormID());
				++joined;
			}
		}
		if (auto* pl = RE::ProcessLists::GetSingleton()) {
			int added = 0, released = 0;
			for (auto& h : pl->highActorHandles) {
				auto a = h.get();
				if (!a || a.get() == a_p) {
					continue;
				}
				if (shared && a->IsInFaction(shared)) {
					a->RemoveFromFaction(shared);
					++released;
				}
				const auto* c = a->IsDead() ? nullptr : Registry::Get().ByRace(a->GetRace());
				if (!c || !c->kin || a->IsInFaction(c->kin)) {
					continue;
				}
				a->AddToFaction(c->kin, 0);
				auto target = a->GetActorRuntimeData().currentCombatTarget.get();
				if (target && target.get() == a_p) {
					a->StopCombat();
				}
				++added;
			}
			if (added > 0 || released > 0) {
				logger::info("apex: {} nearby animal(s) made kin, {} taken out of the old shared faction", added, released);
			}
		}
		if (joined > 0) {
			// animals already after the player lose interest
			if (auto* pl = RE::ProcessLists::GetSingleton()) {
				pl->StopCombatAndAlarmOnActor(a_p, false);
			}
		}
	}
}
