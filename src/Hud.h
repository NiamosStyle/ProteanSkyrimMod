// Protean - a shapeshifter mod for Skyrim SE
// Copyright (C) 2026 NiamosStyle
//
// This program is free software: you can redistribute it and/or modify it under the terms of the GNU General
// Public License as published by the Free Software Foundation, either version 3 of the License, or (at your
// option) any later version. It is distributed WITHOUT ANY WARRANTY; without even the implied warranty of
// MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the LICENSE file or <https://www.gnu.org/licenses/>.

#pragma once

namespace protean::Hud
{
	// While transformed, keeps the player's health bar visible whenever health is below its maximum: the game fades
	// the bar out once health reaches the maximum without the animal's bonus, hiding how far there is to heal.
	// Drawn into the game's own HUD movie (_root.HUDMovieBaseInstance.Health); a HUD replacer that renames those
	// clips simply gets no change.

	void OnFrame(RE::PlayerCharacter* a_player);  // main thread, every frame
}
