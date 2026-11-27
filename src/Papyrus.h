// Protean - a shapeshifter mod for Skyrim SE
// Copyright (C) 2026 NiamosStyle
//
// This program is free software: you can redistribute it and/or modify it under the terms of the GNU General
// Public License as published by the Free Software Foundation, either version 3 of the License, or (at your
// option) any later version. It is distributed WITHOUT ANY WARRANTY; without even the implied warranty of
// MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the LICENSE file or <https://www.gnu.org/licenses/>.
#pragma once

namespace protean::Papyrus
{
	// True when the game's scripts have the function a_fn on script a_class (a static function, or a member function
	// on the script or one it extends). Every Papyrus call Protean makes asks this first: the game's own dispatch
	// looks the function up and uses the result without checking it, so a missing function is a crash, not an error
	// (seen 2026-11-26: SKSE's Game.UpdateTintMaskColors missing on a player's game, SKSE's script files absent or
	// overwritten). A missing function is logged and shown on screen once; the call is then skipped.
	bool Has(const char* a_class, const char* a_fn, bool a_static);
}
