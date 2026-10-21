#pragma once

namespace protean::Fly
{
	// Real flight for the flying animals (the Chaurus Hunter): vanilla only floats them at a fixed height over the
	// ground. Jump takes off; in the air the animal flies where the camera looks while a movement key is held
	// (swim-style, user decision) and hovers otherwise; touching the ground lands it.

	void Install();                                                  // kDataLoaded: input sink (Jump)
	void AfterUpdate(RE::PlayerCharacter* a_player, float a_delta);  // main thread, after the player's update
	bool Flying();
	bool Leaping();
	void Stop(const char* a_why);
	void SetTuning(int a_mode, float a_speed);                       // dev: 1 = in-air state, 2 = flying state
	void TakeOff();                                                  // dev: as if Jump were pressed
	void SetLeapTuning(float a_distance, float a_height);            // dev: multipliers, then leap
}
