// Protean - a shapeshifter mod for Skyrim SE
// Copyright (C) 2026 NiamosStyle
//
// This program is free software: you can redistribute it and/or modify it under the terms of the GNU General
// Public License as published by the Free Software Foundation, either version 3 of the License, or (at your
// option) any later version. It is distributed WITHOUT ANY WARRANTY; without even the implied warranty of
// MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the LICENSE file or <https://www.gnu.org/licenses/>.

#pragma once

namespace protean::Devastator
{
	// Devastator (the user's design, 2026-10-27, after Prototype's devastator attacks): the power, cast in the player's
	// own form with full Stamina (all spent) and Protean_DevastatorStacks biomass boosts, plays the ritual pose and the sound of
	// the change into a beast while the camera slowly circles the player; then, in a moment of slow motion, tendrils burst from the body in
	// every direction (for the look) and every hostile actor within Protean_DevastatorRadius is knocked down and takes
	// Protean_DevastatorDamage + Protean_DevastatorDamagePerLevel x level. The damage is the player's but not a weapon's.
	// Kills feed the boost (again since 2026-11-03), the Devastator's own excepted (the user's choice).

	void OnCast();                         // the Devastator power was cast
	void OnFrame(RE::PlayerCharacter* a_p);  // main thread, every frame (it keeps real time: slow motion slows the game's)
	void Reset();                          // a load: no attack goes on
	bool Struck(RE::FormID a_actor);       // the actor was struck by a burst in the last few seconds
}

namespace protean::Regeneration
{
	// Regeneration (Hunter): a toggle power. While on, Stamina drains Protean_RegenPerSecond a second and Health returns
	// by the same amount, whenever Health is below full and there is Stamina to give.

	void OnCast();
	void OnFrame(RE::PlayerCharacter* a_p, float a_delta);
}
