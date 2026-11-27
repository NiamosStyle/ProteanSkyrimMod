// Protean - a shapeshifter mod for Skyrim SE
// Copyright (C) 2026 NiamosStyle
//
// This program is free software: you can redistribute it and/or modify it under the terms of the GNU General
// Public License as published by the Free Software Foundation, either version 3 of the License, or (at your
// option) any later version. It is distributed WITHOUT ANY WARRANTY; without even the implied warranty of
// MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the LICENSE file or <https://www.gnu.org/licenses/>.

#pragma once

namespace protean::Apex
{
	// Apex (capstone, user 2026-11-14). Three effects:
	//  1. Kin: an invisible aura (user, 2026-11-19): every animal of the creature table within Protean_ApexAuraRange
	//     gets a hidden Calm (as vanilla Kyne's Peace, no shader) every second, in any form. Not the Call pack or other
	//     followers, summons, animals the player hit in the last Protean_ApexAuraForgetSeconds, or animals fighting
	//     something else. (Until 2026-11-19 the player joined every animal's faction instead, which made Forsworn,
	//     Falmer, draugr and vampires friendly through those factions' relations; that is taken off once, in human form.)
	//     In animal form the player still joins that animal's own factions (Shift), as a real one has them.
	//  2. Damage: an animal's hit never falls below a two-hander's (Predator.cpp).
	//  3. Call: a power in animal form (Call.cpp).

	[[nodiscard]] bool Owned();
	void               OnFrame(RE::PlayerCharacter* a_p, float a_delta);  // once a second: the old-kin clean-up and the kin aura
}
