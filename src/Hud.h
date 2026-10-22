#pragma once

namespace protean::Hud
{
	// While transformed, keeps the player's health bar visible whenever health is below its maximum: the game fades
	// the bar out once health reaches the maximum without the animal's bonus, hiding how far there is to heal.
	// Drawn into the game's own HUD movie (_root.HUDMovieBaseInstance.Health); a HUD replacer that renames those
	// clips simply gets no change.

	void OnFrame(RE::PlayerCharacter* a_player);  // main thread, every frame
}
