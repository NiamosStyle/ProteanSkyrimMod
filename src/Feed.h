#pragma once

namespace protean::Feed
{
	// Survivor rank 2: feeding in animal form, which heals. Activate (otherwise blocked in animal form) next to a body
	// (meat eaters) or a plant that could be harvested (plant eaters): the animal eats for Protean_FeedSeconds, standing
	// still, then heals Protean_FeedCorpseHeal / Protean_FeedPlantHeal of its maximum health. Moving, jumping or
	// attacking stops the meal, with no heal. One meal per body (State::eaten); a plant counts as harvested (its
	// ingredient is not kept) and grows back on the game's timer. Eating a person seen by people turns them hostile,
	// like seeing the change itself (Shift::SeenFeeding).

	[[nodiscard]] bool OnActivate();  // main thread; true = a meal started (otherwise Activate stays blocked)
	void               OnFrame(RE::PlayerCharacter* a_p, float a_delta);
	void               Cancel();      // a change of form, a load
}
