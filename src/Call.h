#pragma once

namespace protean::Call
{
	// Apex: the Call power. In animal form it plays that animal's noise and summons a pack of the same kind (small
	// animals 5, medium 3, large 2, mammoth and giant 1) for Protean_CallSeconds (60), using the game's own Summon
	// Creature effect: followers that fight for the player and fade out. Cooldown Protean_CallCooldown (180 s); costs
	// Protean_ChangeStaminaCost. The effect and spell are made at runtime, one per kind, from the animal's vanilla NPC.

	void OnCast();                     // the player cast the Call power
	void OnFrame(float a_delta);       // counts the cooldown down
	void Reset();                      // a change of form or a load
}
