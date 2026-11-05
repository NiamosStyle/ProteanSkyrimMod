#pragma once

namespace protean::Shield
{
	// The Shield (the user's design, 2026-11-05): a light shield for the left hand whose own model is empty. While it
	// is worn and drawn, Protean puts on its hidden sleeve (body slot 54): a medium sleeve of flesh over the left hand,
	// forearm and part of the upper arm, cords of flesh into the back of a shield of dark chitin plates. Put away, it
	// is gone (nothing hangs on the back). The perk keeps it in the inventory.

	// The block (user, 2026-11-05): a blocked hit, arrow or spell from the front does nothing for a flat
	// Protean_ShieldBlockCost Stamina; without that much Stamina it goes through and staggers (a guard break). Spells
	// are stopped like a ward's: hostile, from within Protean_ShieldWardAngle of where the player faces, while blocking.
	void Install();  // the hit listener and the ward (MagicTarget::AddTarget on the player)

	void OnFrame(float a_delta);
}
