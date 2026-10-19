#pragma once

namespace protean::Dev
{
	// DEVELOPMENT ONLY. When Protean.ini has  [Dev] Commands=1  the plugin watches
	// Data/SKSE/Plugins/Protean.cmd for new lines of the form  "<sequence number> <command> [args]"
	// and runs each once (sequence numbers must increase). It lets an automated test drive the mod
	// without anyone pressing keys: transform / revert / learn / hearts / dump.
	// Commands:  learn <id> | unlearn <id> | transform <id> | revert | starter | hearts | dump | kill
	// Results go to Protean.log lines starting "dev:".
	void Init();   // kDataLoaded: reads the INI switch
	void Poll();   // every frame; does real work every 20th frame, and only when enabled
}
