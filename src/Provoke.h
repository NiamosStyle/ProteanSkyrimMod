#pragma once

namespace protean::Provoke
{
	// An animal the player bites back as one of its own (2026-10-26: a bear bitten by the ice wolf took it calmly). In
	// animal form the player joins the animal's factions; a creature sharing one counts the player as a friend and
	// shrugs hits off, and no faction can outweigh that. A creature hit that way is frenzied (Aggression 3, as Frenzy
	// spells do: it fights even friends) and set on the player, then put back when its fight ends, it dies, or the
	// player leaves animal form. The co-save keeps who is frenzied, so a save mid-fight cannot leave one so for good.

	void Install();               // kDataLoaded: watch hits
	void OnFrame(float a_delta);  // main thread
}
