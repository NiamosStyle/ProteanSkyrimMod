#pragma once

namespace protean::Keen
{
	// The Favorites menu in animal form, as the Vampire Lord has it. The game shows a Vampire Lord only the powers it
	// knows from one list, because its race is the "vampire race" default object (tested live: setting the race alone
	// was enough). While an animal's Favorites menu is built, both default objects are pointed at the animal's race
	// and Protean's own list (Revert Form, and later Keen's senses), then put back.

	void Install();  // kDataLoaded: wrap the Favorites menu
}
