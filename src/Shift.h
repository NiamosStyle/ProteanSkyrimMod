#pragma once

#include "Registry.h"

namespace protean::Shift
{
	void Notify(const std::string& a_text);

	// an animal consumed: teach the form and add its power (nothing when it is known)
	void Learn(const Creature& a_creature);

	// the player cast one of our powers (runs on the main thread)
	void OnPowerCast(RE::SpellItem* a_spell);

	// the animal was seen consuming a person: whoever saw it turns hostile until the player changes back, as
	// witnesses of the change do
	void SeenFeeding(RE::Actor* a_body);

	// A stolen face (the user's wish, 2026-10-26): crimes committed while it is worn are forgiven, as an animal's are,
	// when it comes off unseen. The bounties are noted when it goes on.
	void SnapshotFaceCrime();  // a stolen face put on
	void ForgiveFaceCrime();   // it came off: unseen, its crimes are forgiven; seen, they stay
	void ForgiveAll(std::string_view a_note);  // Metamorphosis: every bounty cleared, the hunt called off

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
