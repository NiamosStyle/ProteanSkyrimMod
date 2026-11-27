// Protean - a shapeshifter mod for Skyrim SE
// Copyright (C) 2026 NiamosStyle
//
// This program is free software: you can redistribute it and/or modify it under the terms of the GNU General
// Public License as published by the Free Software Foundation, either version 3 of the License, or (at your
// option) any later version. It is distributed WITHOUT ANY WARRANTY; without even the implied warranty of
// MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the LICENSE file or <https://www.gnu.org/licenses/>.

#pragma once

namespace protean::Journal
{
	// A "Protean" page in SKSE Menu Framework (optional: without the mod there is no page): the evolution count, the
	// perk points, the switch "Take the loot when consuming" (the user, 2026-10-28) and the bodies consumed of each
	// kind (hearts until then; the user asked for a way to track them, 2026-10-26).

	void Register();                 // kPostLoad: every plugin is loaded, SKSEMenuFramework.dll among them
	void OnFrame(float a_delta);     // main thread: the page's copy of the numbers (it draws on the render thread)
}
