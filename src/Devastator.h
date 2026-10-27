#pragma once

namespace protean::Devastator
{
	// Devastator (the user's design, 2026-10-27, after Prototype's devastator attacks): the power, cast in the player's
	// own form with full Stamina (all spent) and Protean_DevastatorStacks kill boosts, plays a crouch-and-rise while the
	// camera pulls out and slowly circles the player; then, in a moment of slow motion, tendrils burst from the body in
	// every direction (for the look) and every hostile actor within Protean_DevastatorRadius is knocked down and takes
	// Protean_DevastatorDamage + Protean_DevastatorDamagePerLevel x level. The damage is the player's but not a weapon's:
	// its kills feed no kill boost.

	void OnCast();                         // the Devastator power was cast
	void OnFrame(RE::PlayerCharacter* a_p);  // main thread, every frame (it keeps real time: slow motion slows the game's)
	[[nodiscard]] bool Struck(RE::FormID a_id);  // hit by a Devastator a moment ago: its death is not a kill-boost kill
	void Reset();                          // a load: no attack goes on
}

namespace protean::Regeneration
{
	// Regeneration (Hunter): a toggle power. While on, Stamina drains Protean_RegenPerSecond a second and Health returns
	// by the same amount, whenever Health is below full and there is Stamina to give.

	void OnCast();
	void OnFrame(RE::PlayerCharacter* a_p, float a_delta);
}
