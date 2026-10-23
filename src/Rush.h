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
