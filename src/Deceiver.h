#pragma once

namespace protean::Deceiver
{
	// The Deceiver perks after Borrowed Visage (the user's design, 2026-10-26):
	// - Inconspicuous: in the player's own form, each friendly person nearby adds Sneak (Protean_InconspicuousPerPerson
	//   each, up to Protean_InconspicuousMax, within Protean_InconspicuousRange), as a temporary modifier.
	// - Assassin: in the player's own form, everyone near who is not hostile to the player is put in Protean_AssassinMark
	//   (the user, 2026-10-28; first built as only those sharing a stolen face's factions, the face required); the
	//   hidden perk Protean_Assassin, held meanwhile, multiplies sneak attacks on them by
	//   Protean_AssassinMult (the user: x10, "it should kill one NPC outright before the others start attacking").
	// - Infected: the Infect power (Protean_InfectCost stamina) puts one Infection in the player's pockets. Planted on
	//   someone (a reverse pickpocket), they join Protean_InfectedFaction, an ally of the player, and help in fights
	//   (Assistance: helps allies; Confidence at least brave) for Protean_InfectedHours. One at a time.

	void Install();                                      // kDataLoaded: watch items changing hands
	void OnFrame(RE::PlayerCharacter* a_p, float a_delta);  // main thread, after the player's update
	void OnInfectCast();                                 // the Infect power was cast
}
