#pragma once

namespace protean::Face
{
	// Face stealing: sneak next to a corpse and activate it to choose between looting it and copying its
	// appearance. Copying gives the player the corpse's face, body and race and puts them in the corpse's
	// factions. Whoever sees the theft is treated like a witness of a vampire feeding: they turn on the player
	// while the stolen face is worn. The most recent face stays "banked" and can be put on and taken off with
	// the Stolen Face power.

	void Register();  // kPostLoad: the Loot / Copy window (when SKSE Menu Framework is installed)
	void Install();   // kDataLoaded: INI

	// A dead actor was activated. Returns true when this started the Loot / Copy choice (the loot window is
	// then closed and replaced by ours). Never true for animals or when the player is not a shapeshifter.
	bool OnCorpseActivated(RE::Actor* a_corpse);

	// The Stolen Face power was cast: wear the banked face, or put the player's own back.
	void OnPower();

	// Copy a face now (used by the window and by the dev channel). a_corpse may be null.
	void Steal(RE::TESNPC* a_source, RE::Actor* a_corpse, bool a_witnessed);

	// End the disguise (the banked face is kept). Safe to call when not disguised.
	void TakeOff(std::string_view a_why);
	void SetIgnoreFaceFiles(bool a_on);  // dev: build faces from the data even when finished face files exist

	void OnFrame();       // every frame, main thread
	void OnGameLoaded();  // kPostLoadGame
	void BeforeLoad();    // kPreLoadGame / kNewGame: hand back anything borrowed from the player's record

	[[nodiscard]] bool Disguised();
	void               Dump();  // dev: log what the player's record looks like now
}
