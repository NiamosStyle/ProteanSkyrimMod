#pragma once

#include "Registry.h"

namespace protean::Evolve
{
	// Evolution points are a running count of feedings: they only go up and are never spent. A perk point is earned
	// each time the count reaches the next triangular number (1, 3, 6, 10, ...), like the werewolf's feedings, but each
	// step is one feeding longer, not two. Named faces add perk points directly (they skip the count). Perk points
	// buy Protean perks only.

	[[nodiscard]] std::uint32_t PointsEarned(std::uint32_t a_total);  // perk points the count has earned
	[[nodiscard]] std::uint32_t PointsAvailable();                    // earned + face points - perks bought

	// a heart was eaten: the first of a kind is worth a point, then the 5th, 10th and 20th of the same kind
	void Feed(const Creature& a_creature);
	void Add(std::uint32_t a_points, std::string_view a_why);
	void AddFacePoint(std::string_view a_why);

	[[nodiscard]] bool Has(std::string_view a_perk);  // bought at least once
	bool               Buy(std::string_view a_perk);  // spends one perk point; says why when it cannot

	// The kill boost (Unbound Form): each kill adds a stack of stamina regeneration that lasts Protean_KillBoostSeconds.
	void                        OnKill();
	[[nodiscard]] std::uint32_t BoostStacks();

	void OnFrame(float a_delta);  // main thread, every frame
	void OnGameLoaded();
	void Dump();  // to the log (dev channel)
}
