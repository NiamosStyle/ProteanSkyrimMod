#pragma once

namespace protean::Move
{
	// Animals have no sideways or backwards walk animations, so as an animal the movement keys steer instead:
	// the animal turns to face the direction pressed (relative to the camera) and runs forward. The mouse turns
	// the camera only. Active only while transformed; a human strafes as normal.

	void Install();                                               // kDataLoaded: input sink
	void BeforeUpdate(RE::PlayerCharacter* a_player, float a_delta);  // main thread, before the player's update
	void AfterUpdate(RE::PlayerCharacter* a_player);                  // main thread, after the player's update
	void SetMode(int a_mode);                                     // dev: 1 = directional, 2 = tank
	void SetTurnScale(float a_scale);                             // dev: mouse turn while steering (0 = the game's setting)
	void SetLog(bool a_on);                                       // dev: log the mouse and camera every frame
	void FaceCamera(RE::PlayerCharacter* a_player);               // an attack goes where the camera looks
	void Reset();                                                 // revert / load: hand movement back to the game
}
