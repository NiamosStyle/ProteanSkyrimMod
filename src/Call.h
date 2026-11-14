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
