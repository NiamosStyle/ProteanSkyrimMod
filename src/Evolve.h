#pragma once

#include "Registry.h"

namespace protean::Evolve
{
	// Evolution points are a running count of feedings: they only go up and are never spent. Perk points cost 1, 1, 2,
	// 2, 3, 3, ... of them (earned at totals 1, 2, 4, 6, 9, 12, 16, ...; the whole tree 132). Until 2026-10-25 each cost
	// one more than the last (253 for the tree, more than all 35 kinds' hearts could give; the user: far too slow). Named faces add perk points directly (they skip the count). Perk points
	// buy Protean perks only.

	[[nodiscard]] std::uint32_t PointsEarned(std::uint32_t a_total);  // perk points the count has earned
	[[nodiscard]] std::uint32_t PointsAvailable();                    // earned + face points - perks bought

	// a heart was eaten: the 1st, 3rd, 5th, 10th, 15th and 20th of a kind are each worth a point (6 a kind)
	void Feed(const Creature& a_creature);
	[[nodiscard]] std::uint32_t HeartPoints(std::uint32_t a_hearts);  // points so many hearts of a kind gave
	[[nodiscard]] std::uint32_t HeartPointsMax();                     // the most a kind gives (6)
	[[nodiscard]] std::uint32_t NextHeart(std::uint32_t a_hearts);    // the next heart that pays; 0 = none left
	[[nodiscard]] std::uint32_t NextPointAt(std::uint32_t a_total);   // the count that earns the next perk point
	void Add(std::uint32_t a_points, std::string_view a_why);
	void AddFacePoint(std::string_view a_why);

	// The perks are real perk records in Protean.esp, bought in the Custom Skills Framework menu (the Evolution power).
	// The player's perk list is the record of what was bought; Protean_PerkPoints is kept at the points left.
	[[nodiscard]] bool Has(std::string_view a_perk);  // the player has this perk
	bool               Buy(std::string_view a_perk);  // dev: spends one perk point without the menu
	void               OpenMenu();                    // the Evolution power was cast

	// The kill boost (Unbound Form): each kill adds a stack of stamina regeneration that lasts Protean_KillBoostSeconds.
	void                        OnKill();
	[[nodiscard]] std::uint32_t BoostStacks();

	void OnFrame(float a_delta);  // main thread, every frame
	void OnGameLoaded();
	void Dump();  // to the log (dev channel)
}
