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
}
