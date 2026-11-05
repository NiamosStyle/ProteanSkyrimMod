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
		RE::SpellItem*                    power{ nullptr };
		RE::SpellItem*                    ability{ nullptr };   // hidden marker carried while in this form (keywords
		                                                        // Protean_Transformed + Protean_Form_<id>)
		int                               index{ -1 };          // stable number of the form (Protean_ActiveForm = index + 1)
		std::string                       size;      // small | medium | large_cave_ok | outdoor_only
		float                             cameraHeight{ 60.0f };  // game units above the root, for the third-person camera
		std::vector<RE::TESFaction*>      factions;  // the factions this animal's own records share
		std::vector<std::pair<int, int>>  ladder;    // (level, health) rungs from the vanilla NPC records
		std::vector<Attack>               attacks;   // the animal's attack events (race record)
		RE::TESObjectARMO*                skin{ nullptr };      // the vanilla animal's own skin; null = the race's
		float                             height{ 1.0f };       // the vanilla animal's size (Fox 0.6, Ice Wolf 1.15)
		float                             reach{ 0.0f };        // player's melee reach in this form; 0 = the race's own
		float                             strikeAngle{ 0.0f };  // player's hit cone (degrees); 0 = each attack's own
		std::string                       stance;               // graph event for its fighting stance (giant: it walks
		                                                        // and attacks only from there); empty = none needed
		bool                              flies{ false };       // Jump takes off into real flight (Fly.cpp)
		bool                              ownSprint{ false };   // the race sprints by itself (horse, were-forms): no boost
		float                             sprint{ 33.0f };      // Protean's sprint: SpeedMult added (the horse's +33%); 0 = none
		float                             leap{ 0.0f };         // Jump leaps this far (game units) in an arc; 0 = no leap
		float                             leapHeight{ 0.0f };   // the arc's highest point above the start
		std::vector<std::string>          taps;                 // LMB / RMB tap events, overriding the side-swipe pick
		RE::SpellItem*                    rmbSpell{ nullptr };  // RMB power attack casts this instead (spider spit)
		std::vector<std::string>          spellEvents;          // graph events sent in turn to play its animation
		float                             spellStanceDelay{ 0.5f };  // seconds between those events
		float                             spellDelay{ 0.3f };   // seconds from the last event to the cast
		std::int32_t                      stanceState{ 0 };     // the graph's iState kept while in the stance (giant: 1,
		                                                        // its combat walk); 0 = left to the game
		std::string                       diet;                 // Survivor 2: meat | plants | both; empty = does not eat
		std::string                       feedEvent;            // graph event of its eating animation; empty = none
		bool                              swims{ false };       // a water animal: breathes water (on land the game suffocates it;
		                                                        // it may change on land and slither in, by user decision)

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
		[[nodiscard]] RE::SpellItem* FacePower() const { return _facePower; }
		[[nodiscard]] RE::SpellItem* Evolution() const { return _evolution; }  // opens the perk tree
		[[nodiscard]] RE::TESObjectWEAP* ClawWeapon() const { return _clawWeapon; }  // Rending Claws
		[[nodiscard]] RE::TESObjectARMO* ClawPiece(bool a_left) const { return a_left ? _clawLeft : _clawRight; }
		[[nodiscard]] RE::TESObjectWEAP* BladeWeapon() const { return _bladeWeapon; }  // the Blade (right hand, two-handed)
		[[nodiscard]] RE::TESObjectARMO* BladeSleeve() const { return _bladeSleeve; }  // its flesh and bone, worn while drawn
		[[nodiscard]] RE::TESObjectARMO* ShieldArmor() const { return _shieldArmor; }   // the Shield (left hand, empty model)
		[[nodiscard]] RE::TESObjectARMO* ShieldSleeve() const { return _shieldSleeve; }  // its flesh and plates, worn while drawn
		[[nodiscard]] RE::BGSPerk*       ShieldBlockPerk() const { return _shieldBlockPerk; }  // hidden: how much of a blocked hit it stops
		[[nodiscard]] RE::SpellItem* Tendril() const { return _tendril; }  // its effects: [0] damage health, [1] absorb stamina
		// Keen, in the order night vision, Detect Life, Detect Dead: the powers the Favorites menu offers, and the
		// senses (self spells) Protean casts and dispels
		[[nodiscard]] const std::array<RE::SpellItem*, 3>& KeenPowers() const { return _keenPowers; }
		[[nodiscard]] const std::array<RE::SpellItem*, 3>& KeenSenses() const { return _keenSenses; }
		[[nodiscard]] RE::SpellItem* KillBoostDisplay() const { return _killBoost; }  // shows the biomass boost in Active Effects
		[[nodiscard]] const std::vector<RE::SpellItem*>& GlideMarkers() const { return _glideMarkers; }  // OAR glide pose conditions
		[[nodiscard]] RE::SpellItem* PredatorPoison() const { return _predatorPoison; }  // Predator rank 2; its effect [0] damages health
		[[nodiscard]] RE::SpellItem* StalkerFade() const { return _stalkerFade; }        // Stalker: the game's Invisibility, cast on the player
		[[nodiscard]] RE::BGSPerk*   StalkerAmbush() const { return _stalkerAmbush; }    // Stalker: hidden, multiplies the bite from hiding
		[[nodiscard]] RE::SpellItem* StalkerPower() const { return _stalkerPower; }      // Stalker: Stalk, switches stalking on or off
		[[nodiscard]] RE::BGSPerk*   AnimalArmsPerk() const { return _animalArms; }     // hidden: One-Handed perks for bites
		[[nodiscard]] RE::BGSPerk*   DeftPerk() const { return _deft; }                 // hidden: Deft Movement, animal or carapace
		[[nodiscard]] RE::TESFaction* AssassinMark() const { return _assassinMark; }    // Assassin: who trusts the worn face
		[[nodiscard]] RE::BGSPerk*   AssassinPerk() const { return _assassinPerk; }     // Assassin: hidden, multiplies sneak attacks on them
		[[nodiscard]] RE::TESBoundObject* Infection() const { return _infection; }      // Infected: the item planted
		[[nodiscard]] RE::SpellItem* InfectPower() const { return _infectPower; }       // Infected: grows one
		[[nodiscard]] RE::TESFaction* InfectedFaction() const { return _infectedFaction; }  // Infected: the player's ally
		[[nodiscard]] RE::SpellItem* MetamorphosisPower() const { return _metamorphosisPower; }  // opens the character menu
		[[nodiscard]] RE::SpellItem* StealthPower() const { return _stealthPower; }    // Assassin's Stealth Consume
		[[nodiscard]] RE::SpellItem* StealthSpell() const { return _stealthSpell; }    // its timed effect, cast by Protean
		[[nodiscard]] RE::EffectSetting* StealthEffect() const { return _stealthEffect; }  // on the player while it lasts
		[[nodiscard]] RE::SpellItem* DevastatorPower() const { return _devastatorPower; }
		[[nodiscard]] RE::SpellItem* DevastatorTendril() const { return _devastatorTendril; }  // the burst's tendrils, for the look
		[[nodiscard]] RE::SpellItem* RegenPower() const { return _regenPower; }
		[[nodiscard]] RE::TESEffectShader* DevastatorShader() const { return _devastatorShader; }  // red tentacles over the body
		[[nodiscard]] RE::BGSExplosion* DevastatorSplash() const { return _devastatorSplash; }      // the red splash at the burst
		[[nodiscard]] RE::BGSPerk*   Perk(std::string_view a_id) const;          // a Protean perk by id, or null
		[[nodiscard]] const std::map<std::string, RE::BGSPerk*, std::less<>>& Perks() const { return _perks; }
		[[nodiscard]] bool PerkBuilt(std::string_view a_id) const { return _perksBuilt.contains(a_id); }  // else a placeholder
		[[nodiscard]] RE::TESFaction* DisguiseFaction() const { return _disguise; }
		[[nodiscard]] RE::TESFaction* ExposedFaction() const { return _exposed; }
		[[nodiscard]] const std::vector<RE::BGSKeyword*>& OpenInteriorKeywords() const { return _openKeywords; }
		[[nodiscard]] const Creature* ByPower(RE::FormID a_id) const;
		[[nodiscard]] const Creature* ByRace(const RE::TESRace* a_race) const;
		// The creature a (dead) actor counts as: among the creatures sharing its race, the one whose name equals
		// the actor's display name (Ice Wolf, Pit Wolf, Cave Bear...), else the first creature of that race.
		[[nodiscard]] const Creature* ByActor(const RE::Actor* a_actor) const;
		[[nodiscard]] const Creature* ById(std::string_view a_id) const;
		// a renowned person (creatures.json "renowned": parts of names, lower case): the part matched, or empty
		[[nodiscard]] std::string Renowned(std::string_view a_name) const;
		[[nodiscard]] const std::vector<Creature>& All() const { return _creatures; }

		// A dial in the plugin (Global, e.g. "Protean_SprintMult"), read live so another mod's perks or scripts
		// can change it; the fallback when it is missing. See docs/INTEGRATION.md.
		[[nodiscard]] float Dial(std::string_view a_name, float a_fallback) const;
		void                SetGlobal(std::string_view a_name, float a_value) const;
		// the dials' values in Protean.esp (a save keeps every global's value, and so kept old ones over a new
		// Protean.esp, 2026-10-27): the co-save records these, and on a load a dial still at the value its old
		// Protean.esp gave takes the new one (one changed by a mod, a script or the console stays)
		[[nodiscard]] const std::map<std::string, float>& DialDefaults() const { return _defaults; }
		void                SettleDials(const std::map<std::string, float>& a_saved) const;

	private:
		std::map<std::string, RE::TESGlobal*> _globals;
		std::vector<std::string>              _renowned;
		std::map<std::string, float>          _defaults;
		RE::SpellItem*        _starter{ nullptr };
		RE::SpellItem*        _revert{ nullptr };
		RE::TESFaction*       _beast{ nullptr };
		RE::TESFaction*       _witness{ nullptr };
		RE::SpellItem*        _facePower{ nullptr };
		RE::SpellItem*        _evolution{ nullptr };
		RE::TESObjectWEAP*    _clawWeapon{ nullptr };
		RE::TESObjectARMO*    _clawRight{ nullptr };
		RE::TESObjectARMO*    _clawLeft{ nullptr };
		RE::TESObjectWEAP*    _bladeWeapon{ nullptr };
		RE::TESObjectARMO*    _bladeSleeve{ nullptr };
		RE::TESObjectARMO*    _shieldArmor{ nullptr };
		RE::TESObjectARMO*    _shieldSleeve{ nullptr };
		RE::BGSPerk*          _shieldBlockPerk{ nullptr };
		RE::SpellItem*        _tendril{ nullptr };
		RE::SpellItem*        _killBoost{ nullptr };
		std::vector<RE::SpellItem*> _glideMarkers;
		RE::SpellItem*        _predatorPoison{ nullptr };
		RE::SpellItem*        _stalkerFade{ nullptr };
		RE::BGSPerk*          _stalkerAmbush{ nullptr };
		RE::SpellItem*        _stalkerPower{ nullptr };
		RE::BGSPerk*          _animalArms{ nullptr };
		RE::BGSPerk*          _deft{ nullptr };
		RE::TESFaction*       _assassinMark{ nullptr };
		RE::BGSPerk*          _assassinPerk{ nullptr };
		RE::TESBoundObject*   _infection{ nullptr };
		RE::SpellItem*        _infectPower{ nullptr };
		RE::TESFaction*       _infectedFaction{ nullptr };
		RE::SpellItem*        _metamorphosisPower{ nullptr };
		RE::SpellItem*        _stealthPower{ nullptr };
		RE::SpellItem*        _stealthSpell{ nullptr };
		RE::EffectSetting*    _stealthEffect{ nullptr };
		RE::SpellItem*        _devastatorPower{ nullptr };
		RE::SpellItem*        _devastatorTendril{ nullptr };
		RE::SpellItem*        _regenPower{ nullptr };
		RE::TESEffectShader*  _devastatorShader{ nullptr };
		RE::BGSExplosion*     _devastatorSplash{ nullptr };
		std::array<RE::SpellItem*, 3> _keenPowers{};
		std::array<RE::SpellItem*, 3> _keenSenses{};
		std::map<std::string, RE::BGSPerk*, std::less<>> _perks;
		std::set<std::string, std::less<>> _perksBuilt;
		RE::TESFaction*       _disguise{ nullptr };
		RE::TESFaction*       _exposed{ nullptr };
		std::vector<RE::BGSKeyword*> _openKeywords;
		std::vector<Creature> _creatures;
	};
}
