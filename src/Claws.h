#pragma once

namespace protean::Claws
{
	// Rending Claws: an empty-handed weapon for either hand. While a hand holds it DRAWN, that hand wears its claw
	// piece (hidden armor in body slot 59 right / 58 left), so one hand can have claws and the other something else.
	// The pieces are added and taken away again, never kept in the inventory.
	// The look (2026-11-13, the user's sculpt reference; tools/build_claw_arms.py): a sleeve of stringy sinew from the
	// shoulder down and one long saw-toothed claw a finger, each solid, turning only at its knuckle. Before: small horn
	// claws on the fingertips (tools/build_claws.py), worn whenever equipped.

	// The game never builds slots 58/59 for the 1st-person view, so there each hand's model (Claws1st_*.nif, skinned,
	// cut at the elbow) hangs in the 1st-person body, bound to its bones (Blade::HangFirstPerson).

	void Sync();                    // main thread: make the worn claw pieces match what the hands hold
	void OnFrame(float a_delta);  // the pieces and the 1st-person models follow the hands; damage from stamina; the perk's claws
	void Give();  // one claw weapon for each hand in the inventory (the perk keeps them there; also dev 'claws')
	void SetStyle(const std::string& a_style);  // dev: models from Protean\Claws\<style>\ ("default": the usual ones)
}
