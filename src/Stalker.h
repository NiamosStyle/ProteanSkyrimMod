#pragma once

namespace protean::Stalker
{
	// Stalker rank 1: Stalk, a power in the animal's Favorites menu (beside Keen's), switches stalking on or off (the
	// user's call, 2026-10-24: fading whenever the animal stood still would cost stamina at every pause). While on, in
	// animal form, out of combat, standing still or walking for Protean_StalkerFadeSeconds fades the
	// player out (the game's own Invisibility effect, so it shimmers, and noise and touch still give the player away).
	// Only the fade costs: Protean_StalkerDrain stamina a second; running, sprinting, jumping, eating, being hit, a fight or an empty
	// Stamina bar end it, and so does attacking (the game's own rule). The bite thrown from hiding does
	// Protean_StalkerAmbushMult times the damage (a hidden perk held from the swing to its hit): animals cannot sneak, so
	// the game's own sneak attacks never come to them.

	void OnFrame(RE::PlayerCharacter* a_p, float a_delta);  // after the player's update
	void OnPowerCast();                                    // main thread: Stalk was cast
	void OnAttack(bool a_hidden);                          // an animal attack the graph took; a_hidden = Hidden() before it
	[[nodiscard]] bool Hidden();                           // faded out right now
	void Install();                                        // kDataLoaded: watch hits
	void Reset();                                          // before a game load
	void OnGameLoaded();                                   // nothing of a fade or an ambush outlives a save
}
