// Protean - a shapeshifter mod for Skyrim SE
// Copyright (C) 2026 NiamosStyle
//
// This program is free software: you can redistribute it and/or modify it under the terms of the GNU General
// Public License as published by the Free Software Foundation, either version 3 of the License, or (at your
// option) any later version. It is distributed WITHOUT ANY WARRANTY; without even the implied warranty of
// MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the LICENSE file or <https://www.gnu.org/licenses/>.

#pragma once

namespace protean::Rush
{
	// Predator's Rush (human form): a little faster on foot, and a sprint that keeps building the longer it is held,
	// from Protean_RushSprintStart to Protean_RushSprintTop times a normal sprint over Protean_RushRampSeconds. A higher
	// jump (the game's jump height setting, Protean_RushJumpHeight, while the perk works). Speed is a SpeedMult
	// modifier whose size the co-save keeps (State::rushApplied), so a load never leaves it behind.

	// Jump while sprinting (the game allows none): Protean starts the jump and keeps the sprint's speed through it.

	void Install();                                         // kDataLoaded: watch the Jump key
	void OnFrame(RE::PlayerCharacter* a_p, float a_delta);  // after the player's update
}
