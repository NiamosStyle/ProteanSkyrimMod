#pragma once

namespace protean::Consume
{
	// Consuming a body (the user's design, 2026-10-28, to make Protean feel more like Prototype; it replaces the
	// hearts). In the player's own form, sneak and activate a dead body; in animal form (animals cannot sneak), press
	// Activate by one. Red tentacles (the Devastator's) cover it for Protean_ConsumeSeconds, then it is gone. Its loot
	// goes to the player when the Protean page says so (off at first); quest items always do.
	// Every body heals Protean_ConsumeHeal of maximum health (Survivor rank 2: Protean_ConsumeHealSurvivor), adds a
	// biomass boost (Unbound Form) and gives evolution points by its size (Evolve::OnConsume).
	// - An animal not known yet teaches its form.
	// - A person gives their face (Borrowed Visage): worn at once in the player's own form, kept for the Stolen Face
	//   power in animal form. Seen, it is a crime (Protean_ConsumeBounty with each witness's crime faction).
	// - Assassin: a person consumed in the player's own form while nobody can see them goes unnoticed: no crime, no
	//   witness, the alarm their death raised called off, and the face goes on free and at once.
	// The first time, a warning box asks first. A body a quest marks as a quest object is refused. Needs Instinct.
	// - Stealth Consume (Assassin's power, the user's design 2026-10-28): for Protean_StealthConsumeSeconds after it is
	//   cast (Protean_StealthConsumeCost Stamina, own form), a person the player kills with a melee sneak attack is
	//   consumed at once, in Protean_AssassinConsumeSeconds, with no box; their face goes on free. Unseen, it is
	//   Assassin's unnoticed consumption; seen, the usual crime. Its loot: the Protean page's own switch (on at first).

	[[nodiscard]] bool OnActivate(RE::Actor* a_body);  // own form, a dead body searched (Hooks): true = no loot window
	[[nodiscard]] bool OnAnimalActivate();             // animal form, Activate pressed: true = a body in reach is taken
	void               OnFrame(RE::PlayerCharacter* a_p, float a_delta);
	void               Cancel();                       // a load
	[[nodiscard]] bool Busy();                         // a body is being consumed
	bool               Nearest();                      // dev: consume the nearest body, no warning
	void               Install();                      // the hit watcher Stealth Consume needs
	void               OnStealthCast();                // the Stealth Consume power was cast
}
