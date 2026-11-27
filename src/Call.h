// Protean - a shapeshifter mod for Skyrim SE
// Copyright (C) 2026 NiamosStyle
//
// This program is free software: you can redistribute it and/or modify it under the terms of the GNU General
// Public License as published by the Free Software Foundation, either version 3 of the License, or (at your
// option) any later version. It is distributed WITHOUT ANY WARRANTY; without even the implied warranty of
// MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the LICENSE file or <https://www.gnu.org/licenses/>.

#pragma once

namespace protean::Call
{
	// Apex: the Call power. In animal form it plays that animal's noise and summons a pack of the same kind (small
	// animals 5, medium 3, large 2, mammoth and giant 1) for Protean_CallSeconds (180), placing the animals beside the player as
	// followers (teammates) that fight for the player; they are taken away when the time is up. Cooldown Protean_CallCooldown (60 s); costs
	// Protean_ChangeStaminaCost. From the animal's vanilla NPC record.

	void OnCast();                     // the player cast the Call power
	void OnFrame(float a_delta);       // counts the cooldown down
	bool OnPlayerHit(RE::Actor* a_target);  // true if it is a pack member (handled here: a stray bite is forgiven)
	void Reset();                      // a change of form or a load
}
