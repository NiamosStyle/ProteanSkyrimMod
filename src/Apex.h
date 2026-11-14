#pragma once

namespace protean::Apex
{
	// Apex (capstone, user 2026-11-14). Three effects:
	//  1. Kin: while the perk is owned the player belongs to every animal's own species faction, in any form (human
	//     too), so animals treat the player as one of their own. Not joined: the broad groups (CreatureFaction, Predator,
	//     Prey, Spriggan), Bandit, Riekling, the Dawnguard vampire hounds' faction and PlayerWerewolfFaction (guards
	//     attack its members on sight).
	//  2. Damage: an animal's hit never falls below a two-hander's (Predator.cpp).
	//  3. Call: a power in animal form (Call.cpp).

	[[nodiscard]] bool Owned();
	void               OnFrame(RE::PlayerCharacter* a_p, float a_delta);  // once a second: join any species faction missing
}
