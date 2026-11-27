// Protean - a shapeshifter mod for Skyrim SE
// Copyright (C) 2026 NiamosStyle
//
// This program is free software: you can redistribute it and/or modify it under the terms of the GNU General
// Public License as published by the Free Software Foundation, either version 3 of the License, or (at your
// option) any later version. It is distributed WITHOUT ANY WARRANTY; without even the implied warranty of
// MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the LICENSE file or <https://www.gnu.org/licenses/>.

#pragma once

namespace protean::Skills
{
	// Protean's fighting counts toward the game's own skills and perks (the user's design, 2026-10-25):
	// - One-Handed: animal bites and Rending Claws hits give One-Handed experience as vanilla weapon hits do, by their
	//   listed damage (times Protean_XpOneHanded). The claws carry the game's sword keyword, so the game's own One-Handed
	//   perks reach them; bites have no weapon for those perks to check, so a hidden perk held in animal form copies
	//   Armsman, Fighting Stance and Savage Strike.
	// - Light Armor: hits taken in animal form, or in the player's own form while Subtle Carapace works and no light
	//   body armor is worn (that gives its own), give Light Armor experience when they hurt (Protean_XpLightArmor uses).
	//   Agile Defender multiplies the carapace; Deft Movement's 10% chance to take no damage from a weapon hit is copied
	//   onto a hidden perk held in animal form or while the carapace works.
	// - Sneak: moving while faded (Stalker) near someone who has not noticed the player gives Sneak experience
	//   (Protean_XpSneak a second); Stalker::OnFrame quiets the fade by Stealth's share, or fully with Silence.
	//   Light Foot needs nothing: the pressure plates only ask whether the player has it.

	void OnFrame(RE::PlayerCharacter* a_p, float a_delta);  // after the player's update
	void OnAnimalAttack(bool a_power);                    // an animal attack the graph took (Savage Strike: standing power)
	[[nodiscard]] float AgileDefender(RE::PlayerCharacter* a_p);  // the best Agile Defender multiplier owned (1 = none)
	[[nodiscard]] float StealthShare(RE::PlayerCharacter* a_p);   // movement noise taken off while faded: 0 to 1
	void Install();                                       // kDataLoaded: watch hits
	void OnGameLoaded();                                  // the hidden perks follow the state again
}
