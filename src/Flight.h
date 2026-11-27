// Protean - a shapeshifter mod for Skyrim SE
// Copyright (C) 2026 NiamosStyle
//
// This program is free software: you can redistribute it and/or modify it under the terms of the GNU General
// Public License as published by the Free Software Foundation, either version 3 of the License, or (at your
// option) any later version. It is distributed WITHOUT ANY WARRANTY; without even the implied warranty of
// MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the LICENSE file or <https://www.gnu.org/licenses/>.

#pragma once

namespace protean::Flight
{
	// Prey's Flight (human form; the fall immunity in every form). Owning the perk:
	// - no fall damage, ever: the fall's starting height follows the player, so the game never counts a drop;
	// - Jump in the air jumps again, once per jump, a little higher than the first (Protean_FlightJumpCost stamina);
	// - Jump held while falling glides: the fall eases to Protean_GlideFallSpeed and the player is carried where they
	//   face at Protean_GlideSpeed or more, Protean_GlideCost stamina a second,
	//   until Jump is let go or the bar is empty;
	// - a power attack (attack held) in the air after Protean_SlamMinAir seconds dives (Protean_SlamCost stamina); the
	//   landing staggers everyone within Protean_SlamRadius, followers too.

	void Install();                                         // kDataLoaded: watch Jump and the attack keys
	void OnFrame(RE::PlayerCharacter* a_p, float a_delta);  // after the player's update
	void AfterAnimation(RE::PlayerCharacter* a_p);
	void KeepFall(RE::PlayerCharacter* a_p, float a_delta);  // a long jump's graph kept in its jump, then its fall          // after the frame's animation reached the skeleton
	void SetLooks(std::string_view a_what, std::string_view a_value);  // dev: "sound" <SNDR editor ID>, "shake" <strength>
}
