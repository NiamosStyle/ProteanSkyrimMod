#pragma once

namespace protean
{
	// A piece of gear that was worn when the player transformed, so it can be put back exactly.
	struct WornItem
	{
		RE::FormID    base{ 0 };
		std::uint16_t uniqueId{ 0 };  // ExtraUniqueID of the exact stack (enchanted / tempered items), 0 = none
		bool          left{ false };
	};

	// Everything that must survive a save and load. Stored in the SKSE co-save (Protean.skse), so
	// removing the mod leaves no trace in the save proper beyond what the engine itself changed.
	struct State
	{
		std::set<std::string> known;   // creature ids whose heart the player has eaten
		std::string           active;  // creature id the player is currently transformed into, or empty

		// Saved at the moment of transforming, used to revert.
		RE::FormID                realRace{ 0 };
		float                     healthBonus{ 0.0f };
		std::vector<WornItem>     worn;
		RE::FormID                leftHand{ 0 };   // spell equipped in the left hand
		RE::FormID                rightHand{ 0 };  // spell equipped in the right hand
		RE::FormID                power{ 0 };      // equipped shout or power
		std::vector<RE::FormID>   addedFactions;   // factions the player joined (not ones they were already in)
		std::vector<RE::FormID>   witnesses;       // who saw the transformation
		std::vector<RE::FormID>   hostileWitnesses;  // the witnesses that were hostile, now in Protean_WitnessFaction

		// Phase 2: the move on each mouse button, per animal: creature id -> (LMB event, RMB event); empty = default attacks.
		std::map<std::string, std::pair<std::string, std::string>> moves;

		// Corpses that already had their heart added, so a taken heart is not added again.
		std::set<RE::FormID> looted;

		// Face stealing. The face of the last corpse copied is "banked" (one at a time) and can be put on and
		// taken off with the Stolen Face power. The player's own look is kept here while a stolen one is worn.
		RE::FormID              bankedNpc{ 0 };  // base NPC of the banked face, 0 = none
		bool                    disguised{ false };
		RE::FormID              ownRace{ 0 };    // the player's own race
		bool                    ownFemale{ false };
		float                   ownHeight{ 1.0f };
		float                   ownWeight{ 0.0f };
		std::uint8_t            ownTint[3]{ 0, 0, 0 };
		RE::FormID              ownFaceNpc{ 0 };  // the player's faceNPC pointer (normally none)
		std::vector<RE::FormID> disguiseFactions;  // factions joined with the stolen face
		std::vector<RE::FormID> exposed;           // who saw the theft; now in Protean_ExposedFaction

		static State& Get();
		void          Reset();
	};

	namespace Persist
	{
		void Install();  // registers the co-save callbacks
	}
}
