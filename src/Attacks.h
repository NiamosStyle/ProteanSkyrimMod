// Protean - a shapeshifter mod for Skyrim SE
// Copyright (C) 2026 NiamosStyle
//
// This program is free software: you can redistribute it and/or modify it under the terms of the GNU General
// Public License as published by the Free Software Foundation, either version 3 of the License, or (at your
// option) any later version. It is distributed WITHOUT ANY WARRANTY; without even the implied warranty of
// MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the LICENSE file or <https://www.gnu.org/licenses/>.

#pragma once

namespace protean::Attacks
{
	// While transformed, the attack buttons are translated into the animal's own attack events.
	// An animal's animation graph does not understand the player's generic "attackStart"; it only
	// accepts its own (attackStart_Attack1, attackStart_ForwardPower, ...), listed in its race record.
	//   tap  -> a normal attack      (left button = a right-side swipe, right button = a left-side one, as Skyrim's hands)
	//   hold -> a power attack        (after kHoldSeconds)
	// Off unless Protean.ini [Attacks] Translate=1 (default 1).

	void Install();                       // kDataLoaded: input sink + INI switch
	void OnFrame();                       // main thread, every frame (hold timing)
	void TestFire(bool a_power, bool a_rmb);  // dev: the attack a button press would start
	void Cancel();                       // transform / revert / load: forget a half-finished press, put back the race's reach
}
