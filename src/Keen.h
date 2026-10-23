#pragma once

namespace protean::Keen
{
	// The Favorites menu in animal form, as the Vampire Lord has it. The game shows a Vampire Lord only the powers it
	// knows from one list, because its race is the "vampire race" default object (tested live: setting the race alone
	// was enough). While an animal's Favorites menu is built, both default objects are pointed at the animal's race
	// and Protean's own list (Revert Form and the Keen powers), then put back.

	// Keen's senses: night vision (free), Detect Life and Detect Dead (Protean_KeenDrain stamina a second each while
	// on). Each is a power, known only while an animal owns the perk; casting it switches the sense on or off.
	// Changing back, or an empty Stamina bar for the two detects, puts them out.

	void Install();                                           // kDataLoaded: wrap the Favorites menu
	bool OnPowerCast(RE::SpellItem* a_spell);                 // main thread: true if it was a Keen power
	void OnFrame(RE::PlayerCharacter* a_p, float a_delta);    // after the player's update
	void Reset();                                             // before a game load: the detects are off
}
