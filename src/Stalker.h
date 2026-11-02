#pragma once

namespace protean::Stalker
{
	// Stalker rank 1: Stalk, a power in the animal's Favorites menu (beside Keen's), switches stalking on or off (the
	// user's call, 2026-10-24: fading whenever the animal stood still would cost stamina at every pause). While on, in
	// animal form, out of combat, standing still or walking for Protean_StalkerFadeSeconds fades the
	// player out (the game's own Invisibility effect, so it shimmers, and noise and touch still give the player away).
	// Faded, only moving costs (the user's call, 2026-10-24): standing is free, walking costs Protean_StalkerDrain
	// stamina a second and running Protean_StalkerRunMult times that (a run keeps the fade, but cannot start it).
	// Sprinting, jumping, eating, being hit, a fight, an empty Stamina bar or attacking end it. The bite thrown from hiding does
	// Protean_StalkerAmbushMult times the damage (a hidden perk held from the swing to its hit): animals cannot sneak, so
	// the game's own sneak attacks never come to them.
	// Stalker rank 2 (the user, 2026-11-02): the fade may also start in a fight while no enemy has the player in sight,
	// and once faded, an enemy that has not seen the player for Protean_StalkerLoseSeconds gives up the hunt.

	void OnFrame(RE::PlayerCharacter* a_p, float a_delta);  // after the player's update
	void OnPowerCast();                                    // main thread: Stalk was cast
	void OnAttack(bool a_hidden);                          // an animal attack the graph took; a_hidden = Hidden() before it
	[[nodiscard]] bool Hidden();                           // faded out right now
	void Install();                                        // kDataLoaded: watch hits
	void Reset();                                          // before a game load
	void OnGameLoaded();                                   // nothing of a fade or an ambush outlives a save
}
