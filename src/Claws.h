#pragma once

namespace protean::Claws
{
	// Rending Claws: an empty-handed weapon for either hand. While a hand holds it, that hand wears its claw piece
	// (hidden armor in body slot 59 right / 58 left; models from tools/build_claws.py), so one hand can have claws and
	// the other something else. The pieces are added and taken away again, never kept in the inventory.

	// The game never builds slots 58/59 for the 1st-person view, so there each hand's blades (Claws1st_*.nif, plain
	// shapes named "ProteanClaw|<bone>") hang straight under the finger bones of the 1st-person skeleton.

	void Sync();                    // main thread: make the worn claw pieces match what the hands hold
	void OnFrame(float a_delta);  // re-hang 1st-person blades the game dropped; damage from stamina; the perk's claws
	void Give();  // one claw weapon for each hand in the inventory (the perk keeps them there; also dev 'claws')
	void SetStyle(const std::string& a_style);  // dev: models from Protean\Claws\<style>\ ("default": the usual ones)
}
