// Protean - a shapeshifter mod for Skyrim SE
// Copyright (C) 2026 NiamosStyle
//
// This program is free software: you can redistribute it and/or modify it under the terms of the GNU General
// Public License as published by the Free Software Foundation, either version 3 of the License, or (at your
// option) any later version. It is distributed WITHOUT ANY WARRANTY; without even the implied warranty of
// MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the LICENSE file or <https://www.gnu.org/licenses/>.

#pragma once

namespace protean::Provoke
{
	// An animal the player bites back as one of its own (2026-10-26: a bear bitten by the ice wolf took it calmly). In
	// animal form the player joins the animal's factions; a creature sharing one counts the player as a friend and
	// shrugs hits off, and no faction can outweigh that. A creature hit that way is frenzied (Aggression 3, as Frenzy
	// spells do: it fights even friends) and set on the player, then put back when its fight ends, it dies, or the
	// player leaves animal form. The co-save keeps who is frenzied, so a save mid-fight cannot leave one so for good.

	void Install();               // kDataLoaded: watch hits
	void OnFrame(float a_delta);  // main thread
}
