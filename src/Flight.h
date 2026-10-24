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
	void SetLooks(std::string_view a_what, std::string_view a_value);  // dev: "sound" <SNDR editor ID>, "shake" <strength>
}
