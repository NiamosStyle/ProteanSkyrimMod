#pragma once

namespace protean::Tendril
{
	// Tendril: a spell (the perk teaches it) that lashes out a whip of flesh: damage health + absorb stamina (vanilla
	// effects, magnitudes set here from the player's level). Every throw costs stamina and starts a cooldown; a cast
	// begun while either is short is interrupted. A hit pulls: a light target is yanked off its feet to the player
	// (a negative knock, as Pull Mastery does); a large one pulls the player to it instead, along a short arc.
	// Nothing is pulled that flies, swims, follows the player or is essential.

	void OnFrame(RE::PlayerCharacter* a_p, float a_delta);  // after the player's update: gate, magnitudes, the pull
	void OnCast(RE::SpellItem* a_spell);                     // main thread: the player cast a spell
	void OnHit(RE::Actor* a_target);                         // main thread: the Tendril's damage landed on a_target
	void Give();                                             // teach the spell (the perk does it; also dev 'tendril')
	void SetLooks(std::string_view a_what, const std::string& a_value);  // dev: "sound" <SNDR editor ID> | "blood" <hex ARTO/EFSH>
}
