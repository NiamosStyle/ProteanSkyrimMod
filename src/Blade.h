#pragma once

namespace protean::Blade
{
	// The Blade is a one-handed weapon (the one-handed stance, so the arm hangs and the right mouse button blocks with
	// it) that plays as a two-handed one (the Two-Handed skill and perks). The user, 2026-11-04: like a greatsword, it
	// keeps the left hand empty: putting something in the left hand (a weapon, spell, torch or shield) puts the Blade
	// away, and taking the Blade up empties the left hand.

	void OnFrame(float a_delta);  // checks the hands a few times a second
}
