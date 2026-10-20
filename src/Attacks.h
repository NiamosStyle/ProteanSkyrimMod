#pragma once

namespace protean::Attacks
{
	// While transformed, the attack buttons are translated into the animal's own attack events.
	// An animal's animation graph does not understand the player's generic "attackStart"; it only
	// accepts its own (attackStart_Attack1, attackStart_ForwardPower, ...), listed in its race record.
	//   tap  -> a normal attack      (left button prefers the animal's left-side attacks, right the right)
	//   hold -> a power attack        (after kHoldSeconds)
	// Off unless Protean.ini [Attacks] Translate=1 (default 1).

	void Install();                       // kDataLoaded: input sink + INI switch
	void OnFrame();                       // main thread, every frame (hold timing)
	void TestFire(bool a_power, bool a_rmb);  // dev: the attack a button press would start
	void Cancel();                       // transform / revert / load: forget a half-finished press, put back the race's reach
}
