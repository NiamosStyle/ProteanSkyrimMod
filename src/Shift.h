#pragma once

#include "Registry.h"

namespace protean::Shift
{
	void Notify(const std::string& a_text);

	// eating a heart: teach the form and add its power
	void Learn(const Creature& a_creature);

	// the player cast one of our powers (runs on the main thread)
	void OnPowerCast(RE::SpellItem* a_spell);

	// a dead animal was activated: add its heart if the player is a shapeshifter
	void OnCorpseActivated(RE::Actor* a_corpse);
	void AddHeart(RE::Actor* a_corpse);  // an animal's heart in its body, once (as the body is searched)

	// the animal was seen eating a person (Survivor 2): whoever saw it turns hostile until the player changes back,
	// as witnesses of the change do
	void SeenFeeding(RE::Actor* a_body);

	// A stolen face (the user's wish, 2026-10-26): crimes committed while it is worn are forgiven, as an animal's are,
	// when it comes off unseen. The bounties are noted when it goes on.
	void SnapshotFaceCrime();  // a stolen face put on
	void ForgiveFaceCrime();   // it came off: unseen, its crimes are forgiven; seen, they stay

	// the creature the player is currently transformed into, or null
	const Creature* ActiveCreature();

	void Transform(const Creature& a_creature);
	void Revert(std::string_view a_why);
	void  OnPowerKey();        // the power key went down: confirms a change back the player was just warned about (flying)

	// every 30th frame (main thread): auto-revert when the animal no longer fits where the player is
	void OnFrame();

	// after a game load: if the save was made transformed, put the runtime pieces back
	void OnGameLoaded();

	// before a game load or new game: give the player's record its own skin and height back
	void BeforeLoad();
}
