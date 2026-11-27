// Protean - a shapeshifter mod for Skyrim SE
// Copyright (C) 2026 NiamosStyle
//
// This program is free software: you can redistribute it and/or modify it under the terms of the GNU General
// Public License as published by the Free Software Foundation, either version 3 of the License, or (at your
// option) any later version. It is distributed WITHOUT ANY WARRANTY; without even the implied warranty of
// MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the LICENSE file or <https://www.gnu.org/licenses/>.

#pragma once

namespace protean::Hooks
{
	// vtable hook on PlayerCharacter::Update (Actor::Update, SE/AE index 0xAD), same pattern as Portal
	// Spheres: one cheap per-frame callback on the main thread for everything that needs a tick.
	void Install();
}
