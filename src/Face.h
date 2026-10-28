#pragma once

namespace protean::Face
{
	// Face stealing: a person consumed (Consume) gives the player their face, body and race and puts them in the
	// person's factions. Whoever sees the theft is treated like a witness of a vampire feeding: they turn on the player
	// while the stolen face is worn. The most recent face stays "banked" and can be put on and taken off with the Stolen
	// Face power. (Until 2026-10-28 a sneaking search offered Loot / Copy appearance instead.)

	void Install();   // kDataLoaded: INI

	// A person was consumed (Borrowed Visage): with a_wear the face goes on now (a_free: no Stamina, Assassin; too
	// tired, it is only kept); otherwise it is kept for the Stolen Face power.
	void FromBody(RE::Actor* a_body, bool a_wear, bool a_free, bool a_witnessed);

	// The Stolen Face power was cast: wear the banked face, or put the player's own back.
	void OnPower();

	// Copy a face now (used by FromBody and by the dev channel). a_corpse may be null. a_pay: the Stamina is charged.
	void Steal(RE::TESNPC* a_source, RE::Actor* a_corpse, bool a_witnessed, bool a_pay = true);

	// End the disguise (the banked face is kept). Safe to call when not disguised.
	void TakeOff(std::string_view a_why);
	void SetIgnoreFaceFiles(bool a_on);  // dev: build faces from the data even when finished face files exist

	void OnFrame();       // every frame, main thread
	void OnGameLoaded();  // kPostLoadGame
	void BeforeLoad();    // kPreLoadGame / kNewGame: hand back anything borrowed from the player's record

	[[nodiscard]] bool Disguised();
	void               Dump();  // dev: log what the player's record looks like now
}
