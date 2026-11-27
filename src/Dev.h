// Protean - a shapeshifter mod for Skyrim SE
// Copyright (C) 2026 NiamosStyle
//
// This program is free software: you can redistribute it and/or modify it under the terms of the GNU General
// Public License as published by the Free Software Foundation, either version 3 of the License, or (at your
// option) any later version. It is distributed WITHOUT ANY WARRANTY; without even the implied warranty of
// MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the LICENSE file or <https://www.gnu.org/licenses/>.

#pragma once

namespace protean::Dev
{
	// DEVELOPMENT ONLY. When Protean.ini has  [Dev] Commands=1  the plugin watches
	// Data/SKSE/Plugins/Protean.cmd for new lines of the form  "<sequence number> <command> [args]"
	// and runs each once (sequence numbers must increase). It lets an automated test drive the mod
	// without anyone pressing keys: transform / revert / learn / hearts / dump.
	// Commands:  learn <id> | unlearn <id> | transform <id> | revert | starter | hearts | dump | kill |
	//            anim <graph event> | draw | scan | spawn <hex base FormID>
	// Results go to Protean.log lines starting "dev:".
	void Init();   // kDataLoaded: reads the INI switch
	void Poll();   // every frame; does real work every 20th frame, and only when enabled
}
