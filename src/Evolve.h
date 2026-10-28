#pragma once

#include "Registry.h"

namespace protean::Evolve
{
	// Evolution points are a running count of the biomass consumed (Consume; the user's design, 2026-10-28: earned
	// solely that way, hearts and kills give none): they only go up and are never spent. As in Growl (the user's pick,
	// 2026-10-28), each perk point costs one more than the last: 1, 2, 3, ... (earned at totals 1, 3, 6, 10, 15, ...; the
	// whole tree 253). 2026-10-25 to 28 they cost 1, 1, 2, 2, 3, 3 (hearts were scarce: the user found 253 far too slow
	// then). Renowned victims add a perk point directly (face points), as Sacrosanct rewards feeding on the powerful.
	// The perk screen shows Protean's level (perk points earned) and a bar to the next, like the werewolf tree.
	// Perk points buy Protean perks only.

	[[nodiscard]] std::uint32_t PointsEarned(std::uint32_t a_total);  // perk points the count has earned
	[[nodiscard]] std::uint32_t PointsAvailable();                    // earned + face points - perks bought

	// a body was consumed: a_points of biomass (small 1, people and middling animals 2, large 3, huge 5), counted per
	// kind (a creature id, "people" or "other") for the Protean page
	void OnConsume(std::string_view a_kind, std::string_view a_label, std::uint32_t a_points);
	void OnRenowned(RE::Actor* a_body);  // a person consumed: one of the renowned gives a perk point, once each
	[[nodiscard]] std::uint32_t NextPointAt(std::uint32_t a_total);   // the count that earns the next perk point
	void Add(std::uint32_t a_points, std::string_view a_why);
	void AddFacePoint(std::string_view a_why);

	// The perks are real perk records in Protean.esp, bought in the Custom Skills Framework menu (the Evolution power).
	// The player's perk list is the record of what was bought; Protean_PerkPoints is kept at the points left.
	[[nodiscard]] bool Has(std::string_view a_perk);  // the player has this perk
	bool               Buy(std::string_view a_perk);  // dev: spends one perk point without the menu
	void               OpenMenu();                    // the Evolution power was cast

	// The biomass boost (Unbound Form; the kill boost until 2026-10-28, user: consuming gives it, kills no longer do):
	// each body consumed adds a stack of stamina regeneration that lasts Protean_KillBoostSeconds.
	void                        AddBoost();
	[[nodiscard]] std::uint32_t BoostStacks();

	void OnFrame(float a_delta);  // main thread, every frame
	void OnGameLoaded();
	void Dump();  // to the log (dev channel)
}
