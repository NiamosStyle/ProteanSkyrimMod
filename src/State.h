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
		RE::FormID                bodySkin{ 0 };     // the player's own record skin and height while an animal's are worn
		float                     bodyHeight{ 0.0f };  // 0 = the animal's body is not on
		float                     sprintBoost{ 0.0f };  // SpeedMult the sprint has added now (taken off again on load)
		float                     armorBonus{ 0.0f };   // DamageResist added at the change (Protean_ArmorBonus then)
		float                     damageBonus{ 0.0f };  // AttackDamageMult added at the change (Protean_DamageMult - 1)

		// Phase 2: the move on each mouse button, per animal: creature id -> (LMB event, RMB event); empty = default attacks.
		std::map<std::string, std::pair<std::string, std::string>> moves;

		// Corpses that already had their heart added, so a taken heart is not added again.
		std::set<RE::FormID> looted;

		// Crime gold (per crime faction) when the player turned into an animal; anything gained as the animal is
		// forgiven on turning back if nobody saw the change.
		struct CrimeSnap
		{
			RE::FormID   faction{ 0 };
			std::int32_t violent{ 0 };
			std::int32_t nonViolent{ 0 };
		};
		std::vector<CrimeSnap> crime;

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

		// A full appearance: everything the game needs to build a face and body. Kept for the stolen face (bank) and
		// for the player's own look while a stolen one is worn, so neither needs the original NPC after a load.
		struct Tint
		{
			std::uint8_t type{ 0 };   // RE::TintMask::Type
			std::uint8_t index{ 0 };  // nth mask of that type
			std::uint8_t rgba[4]{ 0, 0, 0, 0 };
			float        alpha{ 0.0f };
			std::string  texture;     // mask texture path, matched first
		};
		struct Look
		{
			bool                         valid{ false };
			RE::FormID                   race{ 0 };
			bool                         female{ false };
			float                        height{ 1.0f };
			float                        weight{ 0.0f };
			std::uint8_t                 body[3]{ 0, 0, 0 };  // body tint colour
			RE::FormID                   hairColor{ 0 };
			RE::FormID                   faceDetails{ 0 };
			RE::FormID                   skin{ 0 };     // NPC skin (0 = the race's)
			RE::FormID                   faceNpc{ 0 };  // use this NPC's finished face files (0 = build the face from the data)
			std::vector<RE::FormID>      headParts;
			std::array<float, 19>        morphs{};
			std::array<std::int32_t, 4>  presets{};
			bool                         hasFaceData{ false };
			std::vector<Tint>            tints;
		};
		Look own;   // the player's own look, valid while disguised
		Look bank;  // the stolen look

		// Evolution points and the perks (src/Evolve.cpp). The count only goes up; perk points are worked out from it.
		std::uint32_t                        evoTotal{ 0 };
		std::uint32_t                        facePoints{ 0 };  // perk points given directly by named faces
		std::map<std::string, std::uint32_t> feeds;            // creature id -> hearts eaten
		std::vector<float>                   boosts;           // kill boost stacks: seconds left on each
		float                                boostApplied{ 0.0f };    // StaminaRateMult added now
		float                                diseaseApplied{ 0.0f };  // ResistDisease added now
		float                                rushApplied{ 0.0f };     // SpeedMult Predator's Rush added now
		float                                carapaceApplied{ 0.0f };  // DamageResist Subtle Carapace added now
		std::set<RE::FormID>                 eaten;                    // bodies already eaten (Survivor 2)
		bool                                 comboSpent{ false };      // this fight's kill-boost point is taken (until out of combat)
		// The temporary modifiers of the actor values Protean changes (kTempChecked), as they were at the save. A load
		// lost some of them (2026-10-24: the health bonus and AttackDamageMult were gone after a load as the wolf,
		// DamageResist was kept); Shift::OnGameLoaded puts back Protean's part when exactly that much is missing.
		std::vector<std::pair<std::uint32_t, float>> savedTemp;

		static State& Get();
		void          Reset();
	};

	// the actor values whose temporary modifier Protean changes, checked after a load
	inline constexpr RE::ActorValue kTempChecked[] = { RE::ActorValue::kHealth, RE::ActorValue::kAttackDamageMult,
		RE::ActorValue::kDamageResist, RE::ActorValue::kSpeedMult, RE::ActorValue::kStaminaRateMult,
		RE::ActorValue::kResistDisease, RE::ActorValue::kWaterBreathing };

	namespace Persist
	{
		void Install();  // registers the co-save callbacks
	}
}
