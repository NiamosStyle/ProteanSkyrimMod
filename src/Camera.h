// Protean - a shapeshifter mod for Skyrim SE
// Copyright (C) 2026 NiamosStyle
//
// This program is free software: you can redistribute it and/or modify it under the terms of the GNU General
// Public License as published by the Free Software Foundation, either version 3 of the License, or (at your
// option) any later version. It is distributed WITHOUT ANY WARRANTY; without even the implied warranty of
// MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the LICENSE file or <https://www.gnu.org/licenses/>.

#pragma once

namespace protean::Camera
{
	// Animal skeletons have no third-person camera point, so the engine anchors the camera at the
	// animal's feet (measured: camera Z == player Z). While transformed we give the camera our own
	// node, at head height, on the player's 3D. No skeleton files are shipped or replaced.

	void Begin(float a_height);  // transform: start overriding (height in game units above the root)
	void End();                  // revert: stop overriding and let the engine re-bind its own node
	void OnFrame(RE::PlayerCharacter* a_player);  // every frame, main thread
	void Reset();                // game load / revert callback: forget everything
}
