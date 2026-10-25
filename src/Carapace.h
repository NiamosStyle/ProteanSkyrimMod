#pragma once

namespace protean::Carapace
{
	// Subtle Carapace: armor (DamageResist) that grows with the player's level, rank 2's replacing rank 1's. It works in
	// every animal form, and in the player's own form only while no heavy armor and no light armor except fur, hide,
	// leather, chitin or dragonscale is worn on the body (clothes and robes are fine). Agile Defender multiplies it. A temporary modifier whose size the co-save keeps
	// (State::carapaceApplied), so a load never leaves it behind.

	void OnFrame(RE::PlayerCharacter* a_p, float a_delta);  // after the player's update
}
