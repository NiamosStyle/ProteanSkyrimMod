#pragma once

namespace protean::Mound
{
	// The way into Protean (2026-11-14, the user's design): the Strange Mound, a heap of burnt flesh in Helgen's ruins.
	// "Search" it and it poisons the player (a real hit, so they cry out), the shapeshift's shadow passes over them, and
	// they have Unbound Form (Evolve::Infect); the mound shrivels and is gone for good.
	// It is placed by Protean, not by the plugin (no edit to Helgen's cell, so no clash with mods that change Helgen):
	// when the player comes near its spot without Protean, outside the Helgen intro.

	void Install();                                     // the activation sink
	void OnFrame(RE::PlayerCharacter* a_p, float a_delta);  // placing it, its slow pulse, the shrivel after a search
	void OnGameLoaded();

	// dev 'moundhere': the mound at the player's feet now (to line it up in game; logs the spot to copy into kSpot);
	// dev 'moundreset': the mound unused again, and the placed one removed
	void Here(RE::PlayerCharacter* a_p);
	void Reset();
}
