#pragma once

namespace protean
{
	// One row of data/SKSE/Plugins/Protean/creatures.json (generated from data/creatures.json by
	// tools/build_plugin.py). Every creature is described by data; no creature has its own code.
	// One entry of the animal's attack list (from its race record): the animation-graph event that starts
	// it, and what kind of attack it is. The player's attack button is translated into these events.
	struct Attack
	{
		std::string event;
		bool        power{ false };
		bool        bash{ false };
		bool        rotating{ false };
		bool        hasSpell{ false };
	};

	struct Creature
	{
		std::string                       id;
		std::string                       name;
		RE::TESRace*                      race{ nullptr };
		RE::AlchemyItem*                  heart{ nullptr };
		RE::SpellItem*                    power{ nullptr };
		std::string                       size;      // small | medium | large_cave_ok | outdoor_only
		float                             cameraHeight{ 60.0f };  // game units above the root, for the third-person camera
		std::vector<RE::TESFaction*>      factions;  // the factions this animal's own records share
		std::vector<std::pair<int, int>>  ladder;    // (level, health) rungs from the vanilla NPC records
		std::vector<Attack>               attacks;   // the animal's attack events (race record)

		// Health this animal has at the given player level: the highest rung at or below the level,
		// or the lowest rung when the player is below all of them.
		[[nodiscard]] float HealthAtLevel(int a_level) const;
	};

	class Registry
	{
	public:
		static Registry& Get();

		bool Load();  // kDataLoaded; false if the plugin or table is missing

		[[nodiscard]] RE::SpellItem* Starter() const { return _starter; }
		[[nodiscard]] RE::SpellItem* Revert() const { return _revert; }
		[[nodiscard]] RE::TESFaction* BeastFaction() const { return _beast; }
		[[nodiscard]] RE::TESFaction* WitnessFaction() const { return _witness; }
		[[nodiscard]] const std::vector<RE::BGSKeyword*>& OpenInteriorKeywords() const { return _openKeywords; }
		[[nodiscard]] const Creature* ByPower(RE::FormID a_id) const;
		[[nodiscard]] const Creature* ByHeart(RE::FormID a_id) const;
		[[nodiscard]] const Creature* ByRace(const RE::TESRace* a_race) const;
		[[nodiscard]] const Creature* ById(std::string_view a_id) const;
		[[nodiscard]] const std::vector<Creature>& All() const { return _creatures; }

	private:
		RE::SpellItem*        _starter{ nullptr };
		RE::SpellItem*        _revert{ nullptr };
		RE::TESFaction*       _beast{ nullptr };
		RE::TESFaction*       _witness{ nullptr };
		std::vector<RE::BGSKeyword*> _openKeywords;
		std::vector<Creature> _creatures;
	};
}
