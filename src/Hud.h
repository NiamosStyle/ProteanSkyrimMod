#pragma once

namespace protean::Hud
{
	// While transformed, the player's health bar shows the revert danger line: two tick marks where the bar's ends
	// will be when changing back would kill, and the whole bar turns a warning colour once health is below it.
	// Drawn into the game's own HUD movie (_root.HUDMovieBaseInstance.Health.HealthMeter_mc); a HUD replacer that
	// renames those clips simply gets no marks.

	void OnFrame(RE::PlayerCharacter* a_player);  // main thread, every frame
}
