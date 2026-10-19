#pragma once

#include "Registry.h"

namespace protean::Moves
{
	// The "Beast Moves" menu (Phase 2). While transformed the normal menus are locked, so the animal gets its own:
	//   - a window (default key B) listing the animal's attacks; click LMB or RMB beside a move to put it on that button
	//   - number keys 1-8: put move N on the left mouse button; hold Shift to put it on the right mouse button
	//   - a thin strip at the bottom of the screen showing what each mouse button does
	// "LMB" is the game's Right Attack/Block action, "RMB" is Left Attack/Block (that is how Skyrim names them).
	// Needs SKSE Menu Framework; without it the number keys still work.

	enum class Slot
	{
		kLmb,
		kRmb
	};

	void Register();  // kPostLoad: window + HUD strip (when the framework is installed)
	void Install();   // kDataLoaded: input sink + INI

	// The move assigned to a mouse button for this animal, or null (= the animal's default attacks).
	[[nodiscard]] const Attack* Assigned(const Creature& a_c, Slot a_slot);

	// true while the Beast Moves window is open (clicks in it must not count as attacks)
	[[nodiscard]] bool WindowOpen();

	// Readable name for an attack ("Forward Power (power)").
	[[nodiscard]] std::string Label(const Attack& a_attack);

	// Moves offered for an animal: its attacks, one entry per event.
	[[nodiscard]] std::vector<const Attack*> List(const Creature& a_c);
}
