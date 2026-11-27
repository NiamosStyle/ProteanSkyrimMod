// Protean - a shapeshifter mod for Skyrim SE
// Copyright (C) 2026 NiamosStyle
//
// This program is free software: you can redistribute it and/or modify it under the terms of the GNU General
// Public License as published by the Free Software Foundation, either version 3 of the License, or (at your
// option) any later version. It is distributed WITHOUT ANY WARRANTY; without even the implied warranty of
// MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the LICENSE file or <https://www.gnu.org/licenses/>.

#pragma once

namespace protean::Blade
{
	// The Blade is a one-handed weapon (the one-handed stance, so the arm hangs and the right mouse button blocks with
	// it) that plays as a two-handed one (the Two-Handed skill and perks). The user, 2026-11-04: like a greatsword, it
	// keeps the left hand empty: putting something in the left hand (a weapon, spell, torch or shield) puts the Blade
	// away, and taking the Blade up empties the left hand.

	void OnFrame(float a_delta);  // checks the hands a few times a second

	// hangs (a_on) or takes away a skinned model in the 1st-person body, bound to its bones of the same names; the
	// pattern's {} is M or F by the player's sex. The game draws no added body slots in 1st person (Blade, Shield).
	void HangFirstPerson(RE::PlayerCharacter* a_p, const char* a_name, std::string_view a_pattern, bool a_on, const char* a_tag);

	// the sound of a sleeve growing out of the arm (a_grow) or sucking back in, shared by the Blade, Shield, Hammerfist
	// and Tendril (user, 2026-11-14: "something fleshy"); both hands at once sound once
	void SleeveSound(RE::PlayerCharacter* a_p, bool a_grow, const char* a_tag);
	// dev 'drawsound' / 'sheathesound' <SNDR editor ID, or "none">: to choose them by ear
	void SetSleeveSound(bool a_grow, const std::string& a_sound);

	// The grow (2026-11-14, a test first: user, "rapidly expand to normal size so that it looks like it grew from inside
	// the arm"): every skinned Protean piece on the player (1st and 3rd person) is drawn shrunk toward its own bones and
	// swells back to full size over a_seconds, by scaling its skin-to-bone transforms. Dev 'growtest [seconds]'. Every
	// draw (SleeveSound) does the same to each piece that newly appears, over 0.6 s.
	void GrowAll(RE::PlayerCharacter* a_p, float a_seconds);
	void UpdateGrow(float a_delta);  // every frame
}
