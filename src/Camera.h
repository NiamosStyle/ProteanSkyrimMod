#pragma once

namespace protean::Camera
{
	// Animal skeletons have no third-person camera point, so the engine anchors the camera at the
	// animal's feet (measured: camera Z == player Z). While transformed we give the camera our own
	// node, at head height, on the player's 3D. No skeleton files are shipped or replaced.

	void Begin(float a_height);  // transform: start overriding (height in game units above the root)
	void End();                  // revert: stop overriding and let the engine re-bind its own node
	void OnFrame(RE::PlayerCharacter* a_player);  // every frame, main thread
	void Reset();                // game load / revert callback: forget everything
}
