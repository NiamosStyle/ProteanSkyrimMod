// Protean - a shapeshifter mod for Skyrim SE
// Copyright (C) 2026 NiamosStyle
//
// This program is free software: you can redistribute it and/or modify it under the terms of the GNU General
// Public License as published by the Free Software Foundation, either version 3 of the License, or (at your
// option) any later version. It is distributed WITHOUT ANY WARRANTY; without even the implied warranty of
// MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the LICENSE file or <https://www.gnu.org/licenses/>.

#pragma once

namespace protean::Feed
{
	// Survivor rank 2: grazing in animal form, which heals. Activate (otherwise blocked in animal form) next to a plant
	// that could be harvested (plant eaters and omnivores): the animal eats for Protean_FeedSeconds, standing still, then
	// heals Protean_FeedPlantHeal of its maximum health. Moving, jumping or attacking stops the meal, with no heal. The
	// plant counts as harvested (its ingredient is not kept) and grows back on the game's timer. Bodies are consumed
	// instead (Consume, 2026-10-28; until then meat eaters ate them here).

	[[nodiscard]] bool OnActivate();  // main thread; true = a meal started (otherwise Activate stays blocked)
	void               OnFrame(RE::PlayerCharacter* a_p, float a_delta);
	void               Cancel();      // a change of form, a load
	[[nodiscard]] bool Eating();      // a meal is under way
}
