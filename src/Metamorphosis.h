#pragma once

namespace protean::Metamorphosis
{
	// Metamorphosis (the user's design, 2026-10-26): reshape yourself as at the start of the game. Cast out of combat,
	// in your own face and form, with full Stamina (all of it is spent): every bounty is cleared and the game's own full
	// character menu opens (race, face, body). That menu resets skills when the race changes, so the skills (levels,
	// progress, legendary counts), the character's progress to the next level, the perks, the perk points and the
	// base Health, Magicka and Stamina are noted first and put back once it closes (as ShowRaceMenu Alternative does).

	void Install();                 // kDataLoaded: watch the character menu close
	void OnCast();                  // the Metamorphosis power was cast
	void OnFrame(float a_delta);    // main thread: the put-back, a moment after the menu closes
}
