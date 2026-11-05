#pragma once

namespace protean::Shield
{
	// The Shield (the user's design, 2026-11-05): a light shield for the left hand whose own model is empty. While it
	// is worn and drawn, Protean puts on its hidden sleeve (body slot 54): a medium sleeve of flesh over the left hand,
	// forearm and part of the upper arm, cords of flesh into the back of a shield of dark chitin plates. Put away, it
	// is gone (nothing hangs on the back). The perk keeps it in the inventory.

	void OnFrame(float a_delta);
}
