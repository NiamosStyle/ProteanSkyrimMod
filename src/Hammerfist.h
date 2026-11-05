#pragma once

namespace protean::Hammerfist
{
	// The Hammerfist (2026-11-05; the user's brief: heavy and organic, mostly muscle, bits of bone and chitin in the
	// fists): a weapon for either hand like Rending Claws, with no model of its own. While a hand holds it drawn,
	// Protean puts on that hand's hidden arm (body slot 56 right, 60 left): the muscle from the shoulder down and a
	// huge plated, spiked fist; in 1st person it hangs the same in the 1st-person body. It fights as a mace (the game's
	// mace keyword), with the claws' damage (it follows the Stamina bar) times Protean_HammerfistDamageMult, and every
	// swing costs Protean_HammerfistSwingCost Stamina. It plays the claws' beast-race attacks (tools/build_claw_anims.py).

	void OnFrame(float a_delta);
	void Install();  // the swing listener
}
