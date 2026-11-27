// Protean - a shapeshifter mod for Skyrim SE
// Copyright (C) 2026 NiamosStyle
//
// This program is free software: you can redistribute it and/or modify it under the terms of the GNU General
// Public License as published by the Free Software Foundation, either version 3 of the License, or (at your
// option) any later version. It is distributed WITHOUT ANY WARRANTY; without even the implied warranty of
// MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the LICENSE file or <https://www.gnu.org/licenses/>.

#pragma once

namespace protean::Tendril
{
	// Tendril: a spell (the perk teaches it) that lashes out a whip of flesh: damage health + absorb stamina (vanilla
	// effects, magnitudes set here from the player's level). Every throw costs stamina and starts a cooldown; a cast
	// begun while either is short is interrupted. A hit pulls: a light target is yanked off its feet to the player
	// (a negative knock, as Pull Mastery does); a large one pulls the player to it instead, along a short arc.
	// Nothing is pulled that flies, swims, follows the player or is essential.

	void OnFrame(RE::PlayerCharacter* a_p, float a_delta);  // after the player's update: gate, magnitudes, the pull
	void OnCast(RE::SpellItem* a_spell);                     // main thread: the player cast a spell
	void OnHit(RE::Actor* a_target);                         // main thread: the Tendril's damage landed on a_target
	bool PulledRecently(RE::FormID a_id);                    // yanked by the Tendril in the last few seconds
	void Give();                                             // teach the spell (the perk does it; also dev 'tendril')
	void SetLooks(std::string_view a_what, const std::string& a_value);  // dev: "sound" <SNDR editor ID> | "blood" <hex ARTO/EFSH>
}
