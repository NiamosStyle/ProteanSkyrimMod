// Protean - a shapeshifter mod for Skyrim SE
// Copyright (C) 2026 NiamosStyle
//
// This program is free software: you can redistribute it and/or modify it under the terms of the GNU General
// Public License as published by the Free Software Foundation, either version 3 of the License, or (at your
// option) any later version. It is distributed WITHOUT ANY WARRANTY; without even the implied warranty of
// MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the LICENSE file or <https://www.gnu.org/licenses/>.

#pragma once

namespace protean::Predator
{
	// Predator: rank 1 adds animal-form damage, Protean_PredatorDamagePerLevel per player level up to
	// Protean_PredatorDamageMax (animal bites do not grow with the player's level on their own). Rank 2: a power attack in
	// animal form leaves a poison (damage over time, poison resistance counts; the undead and Dwarven machines are
	// immune), refreshed rather than stacked by the next one.

	// Apex lifts the cap: the bite never falls below a two-hander's hit at the player's level
	// (Protean_ApexDamageBase + Protean_ApexDamagePerLevel x level, user 2026-11-14), found by dividing by the race's
	// base unarmed damage.
	[[nodiscard]] float DamageShare(RE::PlayerCharacter* a_p, const RE::TESRace* a_race);  // AttackDamageMult to add at a change into an animal

	void Install();  // kDataLoaded: watch hits
}
