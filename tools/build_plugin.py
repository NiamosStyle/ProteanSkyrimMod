"""Generate Protean.esp (via Spriggit YAML) and the plugin's runtime table from data/creatures.json.

  python -I tools/build_plugin.py [--enable wolf,deer,skeever] [--no-esp]

Writes:
  esp-yaml/                              Spriggit YAML source of the plugin (committed)
  data/Protean.esp                       the plugin (built by tools/spriggit-cli.sh deserialize)
  data/SKSE/Plugins/Protean/creatures.json   what the SKSE plugin reads at runtime

Local FormIDs are fixed by each creature's position in data/creatures.json, so enabling more creatures
later never renumbers the ones already in saves.
"""
import argparse
import json
import os
import shutil
import subprocess
import sys

ROOT = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
GAME = "C:/Games/steamapps/common/Skyrim Special Edition"
PLUGIN = "Protean.esp"

EFFECT, STARTER, REVERT, TOME, HEART_EFFECT = 0x800, 0x801, 0x802, 0x803, 0x804
BEAST_FACTION, WITNESS_FACTION = 0x805, 0x806
FACE_POWER, DISGUISE_FACTION, EXPOSED_FACTION = 0x807, 0x808, 0x809  # face stealing
# Integration markers (docs/INTEGRATION.md). Per form, by its index in data/creatures.json (stable):
# keyword Protean_Form_<id> = FORM_KEYWORD + index, its effect FORM_EFFECT + index, the hidden ability FORM_ABILITY + index.
KW_TRANSFORMED = 0x80A
EVOLUTION = 0x80B  # the power that opens the perk tree (Custom Skills Framework menu)
# The perk tree (docs/protean-perk-spec.md). Perk records are plain: Protean's plugin reads HasPerk and does the work.
# id -> (local id, name, description, x, y, links). Appended, never renumbered: saves remember the perks.
# Tree coordinates (Custom Skills Framework, vanilla-like spacing: neighbours about 1 unit apart so their names show
# when zoomed in): x positive = left, y positive = up. Beast left, Hunter middle, Deceiver right.
# The menu stretches the layout: TREE_SCALE_X across, TREE_SCALE_Y up (1 looked small next to the vanilla werewolf
# tree; 2.5 both ways was too tall). Moving the root below 0 made the whole tree draw bigger, so it stays at y 0.
TREE_SCALE_X = 1.0
TREE_SCALE_Y = 1.0
PERKS = {
    "unbound_form": (0x960, "Unbound Form",
                     "Take the form of any animal whose heart you have consumed. Immunity to all diseases. Passively "
                     "absorb the life force of recent kills to temporarily boost your Stamina recovery (15% boost for "
                     "90 seconds, stacks).", 0, 0,
                     ["keen", "rending_claws", "predators_rush", "subtle_carapace", "borrowed_visage"]),
    # Beast
    "keen": (0x961, "Keen", "Enhance your senses while wearing an animal's form. Open Favorites to access night vision, "
             "Detect Life, and Detect Dead (the Detect spells drain Stamina while on).", 1.8, 0.8,
             ["survivor", "predator", "stalker"]),
    "survivor": (0x962, "Survivor", "Increase the amount of Health you gain when wearing an animal's shape. Tougher "
                 "animals give higher bonuses.", 3.3, 1.9, ["apex"]),
    "predator": (0x963, "Predator", "Your animal forms deal more damage, 2% more for each level you have (at most double).",
                 2.3, 2.1, ["apex"]),
    "stalker": (0x964, "Stalker", "In animal form, open Favorites to switch on Stalk: stand still or walk for a moment "
                "out of a fight to fade from sight. Moving while hidden costs Stamina (running twice as much), standing "
                "still is free; sprinting, being hit or attacking ends it, and a bite from hiding does double damage.", 1.4, 1.9, ["apex"]),
    "apex": (0x965, "Apex", "Take the form of a dragon. Requires Survivor, Predator and Stalker.", 2.3, 3.4, []),
    # Hunter
    "rending_claws": (0x966, "Rending Claws", "Claws grow from your hands, a weapon for each hand. They strike hardest when your Stamina is full, and every swing costs Stamina.",
                      0.8, 0.9, ["tendril", "devastator"]),
    "tendril": (0x967, "Tendril", "Learn Tendril, a whip of flesh cast from either hand: it tears at health, drains "
                "Stamina into you, yanks light foes to you and hauls you to large ones. Each lash costs Stamina.",
                0.9, 2, []),
    "predators_rush": (0x968, "Predator's Rush", "In your own form you move faster and jump far higher, and a held sprint keeps building speed "
                       "until it is dangerously fast. Sprinting still costs Stamina.", 0, 1.1,
                       ["preys_flight"]),
    "preys_flight": (0x969, "Prey's Flight", ("No fall damage. In human form, jump again in mid-air, hold Jump to glide, and power attack in the air "
                     "to dive and stagger everyone around your landing."), 0, 2.2, ["devastator"]),
    "subtle_carapace": (0x96A, "Subtle Carapace", "A hidden carapace hardens your skin: 50 armor plus 4 per level, in animal "
                        "form, or in your own when you wear no body armor (clothes and robes are fine).", -0.8, 0.9,
                        ["regeneration", "devastator"]),
    "regeneration": (0x96B, "Regeneration", "A toggle: while on, stamina drains and health returns by the same amount.",
                     -0.9, 2, []),
    "devastator": (0x96C, "Devastator", "Unleash a devastating attack all around you. Costs all your stamina and needs "
                   "two kill boosts. Requires Rending Claws, Prey's Flight and Subtle Carapace.", 0, 3.4, []),
    # Deceiver
    "borrowed_visage": (0x96D, "Borrowed Visage", "Sneak next to a body and activate it to copy its face, body and race, "
                        "and join its factions. Cast Stolen Face to put the face on or take it off; putting it on costs "
                        "Stamina, like changing form. Anyone who sees the theft turns hostile.", -1.8, 0.8,
                        ["metamorphosis", "inconspicuous"]),
    "metamorphosis": (0x96E, "Metamorphosis", "Make a stolen face and race your own, and leave your crimes behind. "
                      "Costs all your stamina.", -3.3, 1.9, []),
    "inconspicuous": (0x96F, "Inconspicuous", "The more friendly people around you, the harder you are to notice.",
                      -1.9, 2, ["infected", "assassin"]),
    "infected": (0x970, "Infected", "Slip an infection into someone's pocket; the infected fight for you.", -1.4, 3.1, []),
    "assassin": (0x971, "Assassin", "Sneak attacks are far deadlier against someone who trusts the face you wear.",
                 -2.5, 3.3, []),
}
# Second ranks: their own records, chained from the first rank (NextPerk); the menu shows them on the same node.
# base id -> (local id, description)
RANK2 = {
    "survivor": (0x972, "Increase the amount of Health you gain while wearing an animal's form by an even greater "
                 "amount, and gain the ability to heal by eating (predators may feed on corpses, herbivores may feed on "
                 "plants, and omnivores may feed on either. Feeding on a person will cause witnesses to turn hostile)."),
    "predator": (0x973, "Your animal forms' power attacks also poison, 5 damage a second plus 0.3 per level, for 5 "
                 "seconds. The undead and Dwarven machines are immune."),
    "stalker": (0x974, "Enemies lose track of your animal forms sooner."),
    "subtle_carapace": (0x975, "The carapace hardens further: 100 armor plus 7 per level, under the same rule."),
}
# Rending Claws (tools/build_claws.py makes the models): an empty-handed weapon for either hand, and one hidden
# armor piece per hand that shows the blades. Protean's plugin puts a hand's claws on while that hand holds the weapon.
CLAW_WEAPON, CLAW_ARMA_R, CLAW_ARMA_L, CLAW_ARMO_R, CLAW_ARMO_L = 0x980, 0x981, 0x982, 0x983, 0x984
CLAW_SLOT_FLAG = {"R": "0x20000000", "L": "0x10000000"}  # body slot 59 (MOD_ARM_RIGHT), 58 (MOD_ARM_LEFT)
# Tendril (tools/build_tendril.py makes the model): a spell for either hand that the perk teaches (the user chose a
# spell over an inventory item, 2026-10-23: the casting motion is fine). Damage + absorb stamina; its projectile is the
# tendril, sent out like a flame spell's stream (the way Pull Mastery sends its chain; studied 2026-10-23, nothing
# copied). Protean sets the magnitudes, charges the stamina, keeps the cooldown and pulls (src/Tendril.cpp).
# 0x985 was the inventory item until then; not reused.
TENDRIL_PROJECTILE, TENDRIL_DAMAGE, TENDRIL_DRAIN, TENDRIL_SPELL = 0x986, 0x987, 0x988, 0x989
# Keen: three powers the animal's Favorites menu offers (src/Keen.cpp builds that menu like the Vampire Lord's); casting
# one switches its sense on or off. A sense is a self spell of the game's own effects that Protean casts and dispels:
# night eye (the Khajiit's), Detect Life (the spell's fire-and-forget effects), Detect Dead (the game has it only as
# a held spell, so its four effects are copied here as fire-and-forget ones).
KEEN_POWERS = {"night": 0x98A, "life": 0x98B, "dead": 0x98C}
KEEN_SENSES = {"night": 0x98D, "life": 0x98E, "dead": 0x98F}
KEEN_DEAD_EFFECTS = (0x990, 0x991, 0x992, 0x993)  # enemy outdoors, enemy indoors, friend outdoors, friend indoors
# Unbound Form's kill boost shown in Active Effects: an ability whose effect only states the total (the boost itself is a
# modifier on StaminaRateMult); Protean sets its magnitude.
KILL_BOOST_EFFECT, KILL_BOOST_SPELL = 0x97C, 0x97D
# Prey's Flight: the hidden marker while gliding, Open Animation Replacer's condition for the glide pose (three while
# the pose was chosen; xx99D, xx99E, xx9A0, xx9A1 were those spares, never released)
GLIDE_EFFECTS = [0x99C]
GLIDE_SPELLS = [0x99F]
# Predator rank 2: the poison a power attack in animal form leaves (Protean casts it on the hit, magnitude from the level)
PREDATOR_POISON_EFFECT, PREDATOR_POISON_SPELL = 0x9A2, 0x9A3
# Stalker rank 1: the fade (the game's own Invisibility effect in Protean's spell, so it shimmers and the game ends it
# on an attack) and the hidden perk held from a bite thrown from hiding to its hit (Protean sets its multiplier).
# xx9B2 was a test perk installed once (2026-10-24, never committed); not reused.
STALKER_FADE_SPELL, STALKER_AMBUSH_PERK = 0x9B3, 0x9B4
STALKER_POWER = 0x9B8  # Stalk: the toggle in the animal's Favorites menu, beside Keen's powers
# Skills (src/Skills.cpp): hidden perks Protean holds and sets live. Bites have no weapon for the One-Handed perks to
# check, so ANIMAL_ARMS (held in animal form) carries Armsman / Savage Strike (attack damage) and Fighting Stance (power
# attack stamina); DEFT (held in animal form or with the carapace) is Deft Movement's no-damage chance, drawn by Protean.
ANIMAL_ARMS_PERK, DEFT_PERK = 0x9BB, 0x9BC
VANILLA_INVISIBILITY = "01EA6A:Skyrim.esm"  # InvisibillityFFSelf, the Invisibility spell's effect
# the playable races and their vampire versions (Feral's list, without the two DLC races: Protean.esp masters only Skyrim.esm)
CLAW_RACES = ["013740:Skyrim.esm", "08883A:Skyrim.esm", "013741:Skyrim.esm", "08883C:Skyrim.esm", "097A3D:Skyrim.esm",
              "013742:Skyrim.esm", "08883D:Skyrim.esm",
              "013743:Skyrim.esm", "088840:Skyrim.esm", "013744:Skyrim.esm", "088844:Skyrim.esm", "013745:Skyrim.esm",
              "088845:Skyrim.esm", "013746:Skyrim.esm", "088794:Skyrim.esm", "013747:Skyrim.esm", "0A82B9:Skyrim.esm",
              "013748:Skyrim.esm", "088846:Skyrim.esm", "013749:Skyrim.esm", "088884:Skyrim.esm"]
# Perks that work. The rest are placeholders: Protean takes them back and returns the point.
BUILT = {"unbound_form", "rending_claws", "tendril", "keen", "predators_rush", "preys_flight", "survivor", "predator",
         "subtle_carapace", "stalker", "borrowed_visage"}
# second ranks not built yet, though their first rank is
RANK2_UNBUILT = {"stalker"}
FORM_KEYWORD, FORM_EFFECT, FORM_ABILITY = 0x880, 0x8C0, 0x900
# Dials read live by the SKSE plugin: name -> (local id, default, Float/Short, what it does)
GLOBALS = {
    "Protean_ActiveForm":          (0x940, 0,    "Short", "0 = human; otherwise the form's index + 1 (set by Protean, read-only)"),
    "Protean_HealthBonusPct":      (0x941, 0.25, "Float", "bonus health = this x the player's normal max health (plus the gap share)"),
    "Protean_HealthGapShare":      (0x942, 0.25, "Float", "bonus health also = this x how far the animal's health (at the player's level) exceeds the player's"),
    "Protean_SprintMult":          (0x943, 1.0,  "Float", "multiplies every animal's sprint boost"),
    "Protean_SprintStaminaPerSec": (0x944, 7.0,  "Float", "stamina drained per second of sprinting"),
    "Protean_LeapMult":            (0x945, 1.0,  "Float", "multiplies leap distance"),
    "Protean_LeapHeightMult":      (0x946, 1.0,  "Float", "multiplies leap height"),
    "Protean_FlySpeed":            (0x947, 600,  "Float", "flight speed, game units per second"),
    "Protean_SpitCostMult":        (0x948, 0.5,  "Float", "spit stamina cost as a share of a power attack's"),
    "Protean_ReachMult":           (0x949, 1.0,  "Float", "multiplies the forms' melee reach"),
    "Protean_StrikeAngleMult":     (0x94A, 1.0,  "Float", "multiplies the forms' strike cone"),
    "Protean_DamageMult":          (0x94B, 1.0,  "Float", "multiplies all damage the player deals while transformed"),
    "Protean_ArmorBonus":          (0x94C, 0,    "Float", "armor rating added while transformed"),
    "Protean_ChangeStaminaCost":   (0x94D, 15,   "Float", "stamina a player-requested change into a form costs (0 = free)"),
    "Protean_KillBoostSeconds":    (0x94E, 90,   "Float", "how long one kill's stamina-regeneration stack lasts"),
    "Protean_KillBoostPct":        (0x94F, 15,   "Float", "stamina regeneration each stack adds, in percentage points of the normal rate"),
    "Protean_KillBoostMax":        (0x950, 0,    "Float", "most stacks at once (0 = no limit)"),
    "Protean_PerkPoints":          (0x951, 0,    "Float", "Protean perk points to spend (kept by Protean; the perk menu spends them)"),
    "Protean_ShowPerkMenu":        (0x952, 0,    "Short", "set to 1 to open the Protean perk menu (Custom Skills Framework clears it)"),
    # Rending Claws damage on a full stamina bar = Max + PerLevel x level + PerLevelSq x level^2 (about 25% over a
    # one-handed specialist's sword: 11 at level 1, 19 at 10, 49 at 30, 95 at 50); on an empty bar EmptyShare of that,
    # never below Min; in between it follows the bar
    "Protean_ClawDamageMin":       (0x953, 6,    "Float", "Rending Claws damage never goes below this"),
    "Protean_ClawDamageMax":       (0x954, 10,   "Float", "Rending Claws full-bar damage before the level parts"),
    "Protean_ClawDamagePerLevel":  (0x955, 0.7,  "Float", "Rending Claws full-bar damage added per player level"),
    "Protean_ClawSwingCost":       (0x956, 5,    "Float", "stamina each Rending Claws swing costs; 0 = free"),
    "Protean_ClawDamagePerLevelSq": (0x957, 0.02, "Float", "Rending Claws full-bar damage added per player level squared"),
    "Protean_ClawEmptyShare":      (0x958, 0.3,  "Float", "Rending Claws damage on an empty stamina bar, as a share of the full-bar damage"),
    # Tendril (numbers agreed with the user 2026-10-23)
    "Protean_TendrilCost":         (0x959, 25,   "Float", "stamina each Tendril throw costs"),
    "Protean_TendrilCooldown":     (0x95A, 3,    "Float", "seconds between Tendril throws"),
    "Protean_TendrilDamageShare":  (0x95B, 0.5,  "Float", "Tendril damage as a share of Rending Claws' full-bar damage"),
    "Protean_TendrilDrain":        (0x95C, 20,   "Float", "stamina a Tendril hit drains (and gives you) before the level part"),
    "Protean_TendrilDrainPerLevel": (0x95D, 0.5, "Float", "Tendril drain added per player level"),
    "Protean_TendrilRange":        (0x95E, 1500, "Float", "how far the Tendril reaches, game units (1500 ~ 21 m)"),
    "Protean_TendrilPullMult":     (0x95F, 1.0,  "Float", "multiplies the Tendril's pull"),
    "Protean_KeenDrain":           (0x976, 2.0,  "Float", "stamina per second each of Keen's Detect Life and Detect Dead drains while on"),
    "Protean_RushSpeed":           (0x977, 0.1,  "Float", "Predator's Rush: walking and running speed added, as a share (0.1 = +10%)"),
    "Protean_RushSprintStart":     (0x978, 1.25, "Float", "Predator's Rush: sprint speed when a sprint begins, times a normal sprint"),
    "Protean_RushSprintTop":       (0x979, 3.0,  "Float", "Predator's Rush: the most a held sprint builds to, times a normal sprint"),
    "Protean_RushRampSeconds":     (0x97A, 10,   "Float", "Predator's Rush: seconds of unbroken sprinting to reach the top speed"),
    "Protean_RushJumpHeight":      (0x97B, 400,  "Float", "Predator's Rush: jump height (the game's fJumpHeightMin; 76 normally)"),
    "Protean_FlightJumpCost":      (0x994, 15,   "Float", "Prey's Flight: stamina the jump in mid-air costs"),
    "Protean_FlightJumpHeight":    (0x995, 1.25, "Float", "Prey's Flight: how high the jump in mid-air lifts you, as a share of your own jump"),
    "Protean_GlideCost":           (0x996, 5,    "Float", "Prey's Flight: stamina per second of gliding"),
    "Protean_GlideFallSpeed":      (0x997, 250,  "Float", "Prey's Flight: gliding fall speed, game units a second (a fall tops out near 1400)"),
    "Protean_GlideSpeed":          (0x99B, 900,  "Float", "Prey's Flight: the least speed a glide carries you forward, game units a second"),
    "Protean_SlamCost":            (0x998, 30,   "Float", "Prey's Flight: stamina the dive costs"),
    "Protean_SlamRadius":          (0x999, 550,  "Float", "Prey's Flight: the dive's landing staggers everyone this near, game units (550 ~ 8 m)"),
    "Protean_SlamMinAir":          (0x99A, 1.0,  "Float", "Prey's Flight: seconds in the air before a power attack dives"),
    # Survivor, Predator, Subtle Carapace (numbers agreed with the user 2026-10-24; tuning comes from playtesting)
    "Protean_SurvivorHealthPct":   (0x9A4, 0.25, "Float", "Survivor: health bonus share each rank adds to Protean_HealthBonusPct"),
    "Protean_PredatorDamagePerLevel": (0x9A5, 0.02, "Float", "Predator: animal-form damage added per player level, as a share (0.02 = +2%)"),
    "Protean_PredatorDamageMax":   (0x9A6, 1.0,  "Float", "Predator: the most damage it adds, as a share (1.0 = double)"),
    "Protean_PredatorPoison":      (0x9A7, 5,    "Float", "Predator rank 2: poison damage per second before the level part"),
    "Protean_PredatorPoisonPerLevel": (0x9A8, 0.3, "Float", "Predator rank 2: poison damage per second added per player level"),
    "Protean_PredatorPoisonSeconds": (0x9A9, 5,  "Float", "Predator rank 2: how long the poison lasts, seconds"),
    "Protean_CarapaceArmor":       (0x9AA, 50,   "Float", "Subtle Carapace rank 1: armor before the level part"),
    "Protean_CarapaceArmorPerLevel": (0x9AB, 4,  "Float", "Subtle Carapace rank 1: armor added per player level"),
    "Protean_Carapace2Armor":      (0x9AC, 100,  "Float", "Subtle Carapace rank 2: armor before the level part (replaces rank 1's)"),
    "Protean_Carapace2ArmorPerLevel": (0x9AD, 7, "Float", "Subtle Carapace rank 2: armor added per player level"),
    "Protean_FeedCorpseHeal":      (0x9AE, 0.25, "Float", "Survivor rank 2: share of maximum health a meal of a body heals"),
    "Protean_FeedPlantHeal":       (0x9AF, 0.2,  "Float", "Survivor rank 2: share of maximum health a plant heals (user: 20%)"),
    "Protean_FeedSeconds":         (0x9B0, 3,    "Float", "Survivor rank 2: seconds a meal takes, standing still"),
    "Protean_FeedReach":           (0x9B1, 250,  "Float", "Survivor rank 2: how near a body or plant must be, game units"),
    "Protean_StalkerFadeSeconds":  (0x9B5, 2,    "Float", "Stalker: seconds still or walking before the animal fades"),
    "Protean_StalkerDrain":        (0x9B6, 2,    "Float", "Stalker: stamina a second while faded and walking (standing is free)"),
    "Protean_StalkerAmbushMult":   (0x9B7, 2,    "Float", "Stalker: damage multiplier of a bite from hiding"),
    "Protean_StalkerRunMult":      (0x9B9, 2,    "Float", "Stalker: running while faded costs this times the walking drain"),
    "Protean_ComboStacks":         (0x9BA, 5,    "Float", "kill boost stacks at once that give an evolution point, once per fight (0 = off)"),
    "Protean_XpOneHanded":         (0x9BD, 1,    "Float", "multiplies the One-Handed skill uses of a bite or claw hit (vanilla: its listed damage)"),
    "Protean_XpLightArmor":        (0x9BE, 0.38, "Float", "Light Armor skill uses per hit that hurts, as an animal or with the carapace (vanilla, measured)"),
    "Protean_XpSneak":             (0x9BF, 0.62, "Float", "Sneak skill uses a second moving faded near someone unaware (vanilla, measured)"),
}
# Events the animal's graph was seen to refuse when sent to the player (probed live with the dev "anim" command).
BLOCKED = {
    "deer": ["attackStart_AttackLeft1", "attackStart_AttackLeft2", "attackStart_AttackRight1", "attackStart_AttackRight2",
             "attackStart_ForwardPower", "attackStart_ForwardPowerShort", "attackStart_SkeeverLungeLong",
             "attackStart_SkeeverLungeMedium", "attackStart_SkeeverLungeShort", "attackStart_StandingPower"],
    "wolf": ["attackStart_AttackLeft1", "attackStart_AttackLeft2", "attackStart_AttackRight1", "attackStart_AttackRight2"],
    # Phase 3 batch A (2026-10-20); Ice/Pit Wolf inherit the wolf's; dog, death hound, goat, cow refuse nothing
    "fox": ["attackStart_AttackLeft1", "attackStart_AttackLeft2", "attackStart_AttackRight1", "attackStart_AttackRight2"],
    "horker": ["attackStart_Attack2"],  # seen refused in play 2026-10-20
    "elk": ["attackStart_AttackLeft1", "attackStart_AttackLeft2", "attackStart_AttackRight1", "attackStart_AttackRight2",
            "attackStart_ForwardPower", "attackStart_ForwardPowerShort", "attackStart_SkeeverLungeLong",
            "attackStart_SkeeverLungeMedium", "attackStart_SkeeverLungeShort", "attackStart_StandingPower"],
    # Phase 4 step 1 (2026-10-20): horse refuses nothing; the sprint moves need a sprint, the howl is a shout
    "werewolf": ["AttackStartDualSprinting", "AttackStartHowlExplode", "AttackStartLeftSprinting", "AttackStartRightSprinting"],
    # 2026-10-21: refused every time in play and by probe; LMB falls to its straight bite (Attack1, accepted)
    "boar": ["attackStart_AttackRight1"],
}
# Melee tuning for the player in animal form (wild animals keep the vanilla values). Vanilla wolf: reach 64,
# strike cone 35 degrees, which made bites very hard to land (measured live); werewolf: 150 / 50.
MELEE = {
    "wolf": {"reach": 120, "strike_angle": 50},
    # the other hunters start from the wolf's values (not measured; adjust if one feels wrong in play)
    "fox": {"reach": 120, "strike_angle": 50},
    "dog": {"reach": 120, "strike_angle": 50},
    "death_hound": {"reach": 120, "strike_angle": 50},
    # batches B-D: the same floor for every fighter (a race with a longer reach keeps its own)
    **{c: {"reach": 120, "strike_angle": 50} for c in (
        "bear", "cave_bear", "snow_bear", "sabre_cat", "snowy_sabre_cat", "vale_sabre_cat",
        "mudcrab", "ash_hopper", "chaurus", "troll", "frost_troll", "horker", "horse", "boar")},
    # taps go straight ahead (user: they swiped left/right); their side swipes stay unused
    **{c: {"reach": 120, "strike_angle": 50, "taps": ["attackStart_Attack1", "attackStart_Attack2"]} for c in (
        "bear", "cave_bear", "snow_bear")},
    # Jump leaps far (user: "leap great distances"); its own lunges are refused for the player. ~14 m, ~3.5 m up
    "ash_hopper": {"reach": 120, "strike_angle": 50, "taps": ["attackStart_Attack1", "attackStart_Attack2"],
                   "leap": 1000, "leap_height": 250},
    # these sprint by themselves (gallop, were-form sprint); everyone else gets Protean's sprint boost
    "horse": {"reach": 120, "strike_angle": 50, "own_sprint": True},
    "werewolf": {"own_sprint": True},
    "werebear": {"own_sprint": True},
    # the same leap at half the distance and ~2 m up (user: 1000 was way too far, 250 too high)
    "hare": {"leap": 500, "leap_height": 140},
    # breathes water; on land the game itself suffocates it (measured). User decision: no shoreline wall, and it
    # may change on land and slither into the water
    "slaughterfish": {"reach": 120, "strike_angle": 50, "swims": True},
    # vanilla floats them at a fixed height over the ground; Jump takes off into real flight (Fly.cpp)
    "chaurus_hunter": {"reach": 120, "strike_angle": 50, "flies": True},
    "chaurus_hunter_fledgling": {"reach": 120, "strike_angle": 50, "flies": True},
    # RMB power attack = its poison spit (user decision), crSpider01PoisonSpit. Probed live: the stance rears it up,
    # the release event plays the spit (its BeginCastLeft is refused for the player)
    "frostbite_spider": {"reach": 120, "strike_angle": 50, "rmb_spell": {"file": "Skyrim.esm", "id": 0x0434DD},
                         "spell_events": ["spitStanceStart", "MLH_SpellRelease_event"]},
    # the mammoth race's reach is 128 (measured: hits landed only on people almost under it); its tusks reach
    # well past that, and the giant's race has 512
    "mammoth": {"reach": 400, "strike_angle": 90, "taps": ["attackStart_Attack1", "attackStart_Attack2"]},
    # the giant's graph walks and attacks only from its combat stance, which the AI puts wild giants in (measured:
    # no movement after transforming, and every attack refused, until combatIdleStart)
    # and walks properly only with the graph's iState at 1, its combat walk (measured: 0 = no walk or a split-legged one)
    "giant": {"stance": "combatIdleStart", "stance_state": 1},
}
# Survivor rank 2: what each form eats (meat = bodies, plants = harvestable plants, both), and the graph event that
# would start an eating animation ("" = it just stands; all empty now). Diets from Wikipedia for the real animals (2026-10-24: wolf,
# sabre-tooth cat, spider, walrus carnivores; bear, boar, fox, dog, rat, crab, chicken omnivores; deer, elk, goat, cow,
# horse, hare, mammoth herbivores) and from the games' lore for the made-up ones. The events are first guesses from
# the animals' behavior files.
DIET = {
    # no eating animations (user, 2026-10-24: many vanilla actions have none). The deer's grazing clip outlasted the
    # 3 s meal and held the player in place for 6-7 s; only deer/elk, goat, cow, sabre cats and the were-forms had one.
    **{c: ("meat", "") for c in ("wolf", "ice_wolf", "pit_wolf", "death_hound", "sabre_cat", "snowy_sabre_cat",
                                 "vale_sabre_cat", "horker", "albino_spider", "frostbite_spider", "chaurus",
                                 "chaurus_hunter", "chaurus_hunter_fledgling", "slaughterfish", "troll", "frost_troll",
                                 "werewolf")},
    **{c: ("both", "") for c in ("fox", "dog", "bear", "cave_bear", "snow_bear", "boar", "skeever", "mudcrab", "chicken",
                                 "giant", "werebear")},
    **{c: ("plants", "") for c in ("deer", "elk", "goat", "cow", "horse", "mammoth", "hare", "ash_hopper")},
}
# Protean's sprint boost (SpeedMult added while Sprint is held); everyone else gets the horse's gallop, +33% (user)
# Only four-legged animals sprint (user): none for two legs or more than four
SPRINT = {"elk": 40, "fox": 40, "hare": 40, "mammoth": 0, "horker": 0,
          **{c: 0 for c in ("troll", "frost_troll", "giant", "chicken", "frostbite_spider", "chaurus", "chaurus_hunter",
                            "chaurus_hunter_fledgling", "mudcrab", "ash_hopper", "slaughterfish")}}
ENABLED = ("wolf,deer,skeever,ice_wolf,pit_wolf,fox,dog,death_hound,elk,goat,cow,"
           "bear,cave_bear,snow_bear,sabre_cat,snowy_sabre_cat,vale_sabre_cat,"
           "hare,chicken,mudcrab,ash_hopper,chaurus,troll,frost_troll,horker,"
           "horse,werewolf,werebear,mammoth,giant,slaughterfish,chaurus_hunter,chaurus_hunter_fledgling,"
           "frostbite_spider,boar")
# Extra factions joined while transformed. The hold crime factions count PlayerWerewolfFaction as an enemy: it is how
# vanilla makes guards attack a player werewolf on sight (and citizens flee).
EXTRA_FACTIONS = {
    "werewolf": ["Skyrim.esm:091822"],  # PlayerWerewolfFaction
    "werebear": ["Skyrim.esm:091822"],
}
FIRST_CREATURE = 0x810  # heart = base + 2*index, power = base + 2*index + 1


def fid(n):
    return "%06X" % n


def write(path, text):
    os.makedirs(os.path.dirname(path), exist_ok=True)
    with open(path, "w", encoding="utf-8", newline="\n") as f:
        f.write(text)


def name_block(value):
    return "Name:\n  TargetLanguage: English\n  Value: %s\n" % yq(value)


def yq(s):
    return "'" + s.replace("'", "''") + "'"


# The vanilla NPC record whose skin and size the player gets, where the automatic choice is wrong.
NPC_PICK = {
    "mudcrab": "EncMudcrabMedium",  # Small/Medium/Large/Giant all exist; medium is size 1.0
}


def by_race(table, c, creatures):
    """A per-animal setting; variants sharing a race (Ice Wolf, Pit Wolf) inherit their race-mate's."""
    if c["id"] in table:
        return table[c["id"]]
    for other in creatures:
        if other["race"] == c["race"] and other["id"] in table:
            return table[other["id"]]
    return type(next(iter(table.values())))()


def body(c, npcs):
    """Skin, size and model height of the animal's own vanilla NPC record (the one named like it, an Enc* record
    preferred). Variants that share a race (Ice Wolf, Pit Wolf) differ only here. Skin None = the race's own."""
    def pick(named):
        rows = []
        for x in npcs:
            j = x["json"]
            if x["race"] == c["race"] and (not named or j.get("FULL - Name") == c["name"]):
                edid = j.get("EDID - Editor ID", "")
                bound = (j.get("OBND - Object Bounds") or {}).get("Z2", 0)
                # the plain Enc record first: EncMudcrab, not EncMudcrabGiant (twice the size)
                rows.append((not edid.startswith("Enc"), "TPLT - Template" in j, len(edid), edid,
                             j.get("WNAM - Worn Armor"), j.get("NAM6 - Height", 1.0), bound))
        return sorted(rows)

    rows = pick(True) or pick(False)  # the game calls hares "Rabbit"
    if c["id"] in NPC_PICK:
        rows = [r for r in rows if r[3] == NPC_PICK[c["id"]]] or rows
    if not rows:
        return {}
    _, _, _, edid, skin, height, bound = rows[0]
    if not bound:  # some records leave the bounds empty; any record of the same race has the model's
        bound = max(((x["json"].get("OBND - Object Bounds") or {}).get("Z2", 0) for x in npcs if x["race"] == c["race"]),
                    default=0) or c.get("bound_height") or 0
    out = {"height": round(height, 3), "npc": edid}
    if skin:
        f, i = skin.split(":")
        out["skin"] = {"file": f, "id": int(i, 16)}
    if bound:
        out["camera_height"] = round(0.9 * bound * height)
    return out


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("--enable", default=ENABLED)
    ap.add_argument("--no-esp", action="store_true")
    a = ap.parse_args()
    table = json.load(open(os.path.join(ROOT, "data", "creatures.json"), encoding="utf-8"))
    creatures = table["creatures"]
    enabled = [c.strip() for c in a.enable.split(",") if c.strip()]
    ids = [c["id"] for c in creatures]
    for e in enabled:
        if e not in ids:
            sys.exit("unknown creature id: " + e)

    y = os.path.join(ROOT, "esp-yaml")
    if os.path.isdir(y):
        shutil.rmtree(y)
    os.makedirs(y)

    write(os.path.join(y, "spriggit-meta.json"), json.dumps({
        "PackageName": "Spriggit.Yaml.Skyrim", "Version": "0.41.0",
        "Release": "SkyrimSE", "ModKey": PLUGIN}, indent=2))
    write(os.path.join(y, "RecordData.yaml"),
          "SpriggitSource:\n  PackageName: Spriggit.Yaml.Skyrim\n  Version: 0.41\n"
          "ModKey: %s\nGameRelease: SkyrimSE\nModHeader:\n  Flags:\n  - Small\n  Stats:\n    Version: 1.7\n"
          "  Author: NiamosStyle mods\n  Description: Protean - animal shapeshifting\n"
          "  MasterReferences:\n  - Master: Skyrim.esm\n    FileSize: 0\n" % PLUGIN)

    # One shared Script-archetype effect: every power is a self-cast spell using it; the SKSE plugin
    # tells them apart by spell FormID (same pattern as the Portal Spheres powers).
    write(os.path.join(y, "MagicEffects", "Protean_ShapeshiftEffect - %s_%s.yaml" % (fid(EFFECT), PLUGIN)),
          "FormKey: %s:%s\nEditorID: Protean_ShapeshiftEffect\n" % (fid(EFFECT), PLUGIN)
          + name_block("Shapeshift")
          + "Flags:\n- NoDuration\n- NoMagnitude\n- NoArea\n- Painless\n- NoHitEffect\nBaseCost: 1\n"
          "Archetype:\n  MutagenObjectType: MagicEffectArchetype\n  Type: Script\nCastType: FireAndForget\n"
          "SkillUsageMultiplier: 1\nDualCastScale: 1\n"
          "Description:\n  TargetLanguage: English\n  Value: ''\n")

    # Hearts need ONE effect: the inventory menu dereferences an ingestible's first effect, and a food
    # with none crashed the game (measured 2026-10-19). This one does nothing at all.
    write(os.path.join(y, "MagicEffects", "Protean_HeartEffect - %s_%s.yaml" % (fid(HEART_EFFECT), PLUGIN)),
          "FormKey: %s:%s\nEditorID: Protean_HeartEffect\n" % (fid(HEART_EFFECT), PLUGIN)
          + name_block("Shapeshifter's Hunger")
          + "Flags:\n- NoDuration\n- NoMagnitude\n- NoArea\n- Painless\n- NoHitEffect\nBaseCost: 0\n"
          "Archetype:\n  MutagenObjectType: MagicEffectArchetype\n  Type: Script\nCastType: FireAndForget\n"
          "SkillUsageMultiplier: 1\nDualCastScale: 1\n"
          "Description:\n  TargetLanguage: English\n  Value: ''\n")

    # ---- integration markers and dials (for a larger mod built on Protean; see docs/INTEGRATION.md) ----
    def keyword(local, editor_id):
        write(os.path.join(y, "Keywords", "%s - %s_%s.yaml" % (editor_id, fid(local), PLUGIN)),
              "FormKey: %s:%s\nEditorID: %s\n" % (fid(local), PLUGIN, editor_id))

    keyword(KW_TRANSFORMED, "Protean_Transformed")
    for name, (local, value, kind, _) in GLOBALS.items():
        write(os.path.join(y, "Globals", "%s - %s_%s.yaml" % (name, fid(local), PLUGIN)),
              "MutagenObjectType: Global%s\nFormKey: %s:%s\nEditorID: %s\nData: %s\n" % (kind, fid(local), PLUGIN, name, value))

    # (perk-reach test, 2026-10-21: a plain +25% ModAttackDamage perk raised an elk's kick 5 -> 6.25, so
    # ordinary damage perks reach the animal forms; the test perk is gone)

    # Witnesses. While transformed the player is in Protean_BeastFaction (plus the animal's own factions).
    # Anyone who was hostile to the player when the change happened is put in Protean_WitnessFaction, and
    # the two factions are enemies, so those few keep treating the player as the enemy they saw.
    def faction(local, editor_id, target):
        write(os.path.join(y, "Factions", "%s - %s_%s.yaml" % (editor_id, fid(local), PLUGIN)),
              "FormKey: %s:%s\nEditorID: %s\n" % (fid(local), PLUGIN, editor_id)
              + "Name:\n  TargetLanguage: English\n"
              "Relations:\n- Target: %s:%s\n  Reaction: Enemy\n" % (fid(target), PLUGIN)
              + "Flags:\n- HiddenFromPC\nVendorValues: {}\n")

    faction(BEAST_FACTION, "Protean_BeastFaction", WITNESS_FACTION)
    faction(WITNESS_FACTION, "Protean_WitnessFaction", BEAST_FACTION)
    # Face stealing: same idea. The player is in Protean_DisguiseFaction while wearing a stolen face; whoever saw
    # the theft is in Protean_ExposedFaction, its enemy.
    faction(DISGUISE_FACTION, "Protean_DisguiseFaction", EXPOSED_FACTION)
    faction(EXPOSED_FACTION, "Protean_ExposedFaction", DISGUISE_FACTION)

    def power(local, editor_id, title, desc):
        write(os.path.join(y, "Spells", "%s - %s_%s.yaml" % (editor_id, fid(local), PLUGIN)),
              "FormKey: %s:%s\nEditorID: %s\n" % (fid(local), PLUGIN, editor_id)
              + name_block(title)
              + "MenuDisplayObject: 0435A5:Skyrim.esm\nEquipmentType: 025BEE:Skyrim.esm\n"
              "Description:\n  TargetLanguage: English\n  Value: %s\n" % yq(desc)
              + "Type: LesserPower\nChargeTime: 0.5\nCastType: FireAndForget\n"
              "Effects:\n- BaseEffect: %s:%s\n  Data: {}\n" % (fid(EFFECT), PLUGIN))

    power(STARTER, "Protean_Instinct", "Shapeshifter's Instinct",
          "You can learn the forms of animals by eating their hearts. Cast to recall the forms you know.")
    power(REVERT, "Protean_RevertForm", "Revert Form",
          "Return to your own form.")
    power(FACE_POWER, "Protean_FacePower", "Shapeshift: Stolen Face",
          "Wear the face you last stole from a corpse, or put your own face back. Sneak next to a corpse and activate it to steal a face.")
    power(EVOLUTION, "Protean_Evolution", "Evolution",
          "Look into what you are becoming: spend Protean perk points, earned by feeding.")

    def perk(pid, local, title, desc, nxt=None):
        write(os.path.join(y, "Perks", "Protean_Perk_%s - %s_%s.yaml" % (pid, fid(local), PLUGIN)),
              "FormKey: %s:%s\nEditorID: Protean_Perk_%s\n" % (fid(local), PLUGIN, pid)
              + name_block(title)
              + "Description:\n  TargetLanguage: English\n  Value: %s\n" % yq(desc)
              + "NumRanks: 1\nPlayable: True\n"
              + ("NextPerk: %s:%s\n" % (fid(nxt), PLUGIN) if nxt else ""))

    for pid, (local, title, desc, _, _, _) in PERKS.items():
        perk(pid, local, title, desc, RANK2[pid][0] if pid in RANK2 else None)
    for pid, (local, desc) in RANK2.items():
        perk(pid + "_2", local, PERKS[pid][1], desc)

    # ---- Rending Claws
    write(os.path.join(y, "Weapons", "Protean_RendingClaws - %s_%s.yaml" % (fid(CLAW_WEAPON), PLUGIN)),
          "FormKey: %s:%s\nEditorID: Protean_RendingClaws\n" % (fid(CLAW_WEAPON), PLUGIN)
          + name_block("Rending Claws")
          + "EquipmentType: 013F44:Skyrim.esm\n"  # EitherHand (Feral's 013F45 is BothHands: one equip filled both)
          "Description:\n  TargetLanguage: English\n  Value: ''\n"
          # hits sound and look like a werewolf's swipe (FXMeleeWerewolfFleshImpactSet; chosen by ear in game over
          # the fist set Feral uses, the sabre cat's claw and a sword's slash)
          "ImpactDataSet: 0F23C4:Skyrim.esm\nAttackFailSound: 0C06D1:Skyrim.esm\n"
          "EquipSound: 0C06CF:Skyrim.esm\nUnequipSound: 0C06D0:Skyrim.esm\n"
          # the game's sword keyword: the One-Handed perks for swords reach the claws (the user's design, 2026-10-25)
          "Keywords:\n- 01E711:Skyrim.esm\n"
          "BasicStats:\n  Damage: 6\n"
          "Data:\n  Speed: 1.3\n  Reach: 0.7\n  NumProjectiles: 1\n  RangeMin: 500\n  RangeMax: 2000\n"
          "  AnimationAttackMult: 1\n  RumbleLeftMotorStrength: 0.5\n  RumbleRightMotorStrength: 1\n  RumbleDuration: 0.33\n"
          "Critical:\n  Damage: 3\n  PercentMult: 1\n  Flags:\n  - OnDeath\n"
          "DetectionSoundLevel: Normal\n")
    for side, arma, armo in (("R", CLAW_ARMA_R, CLAW_ARMO_R), ("L", CLAW_ARMA_L, CLAW_ARMO_L)):
        template = ("BodyTemplate:\n  FirstPersonFlags:\n  - %s\n  ArmorType: Clothing\n  ActsLike44: True\n"
                "Race: 000019:Skyrim.esm\n" % CLAW_SLOT_FLAG[side])
        write(os.path.join(y, "ArmorAddons", "Protean_ClawsAA_%s - %s_%s.yaml" % (side, fid(arma), PLUGIN)),
              "FormKey: %s:%s\nEditorID: Protean_ClawsAA_%s\n" % (fid(arma), PLUGIN, side) + template
              + "Priority:\n  Male: 10\n  Female: 10\n"
              "WeightSliderEnabled:\n  Male: False\n  Female: False\n"
              + "".join("%s:\n  Male:\n    File: Protean\\Claws\\Claws_%s_M.nif\n"
                        "  Female:\n    File: Protean\\Claws\\Claws_%s_F.nif\n" % (field, side, side)
                        for field in ("WorldModel", "FirstPersonModel"))  # the same model in 1st person
              + "AdditionalRaces:\n" + "".join("- %s\n" % r for r in CLAW_RACES))
        write(os.path.join(y, "Armors", "Protean_Claws_%s - %s_%s.yaml" % (side, fid(armo), PLUGIN)),
              "FormKey: %s:%s\nEditorID: Protean_Claws_%s\n" % (fid(armo), PLUGIN, side)
              + "MajorFlags:\n- NonPlayable\n" + template
              + "Description:\n  TargetLanguage: English\n  Value: ''\n"
              "Armature:\n- %s:%s\n" % (fid(arma), PLUGIN))

    # ---- Tendril
    write(os.path.join(y, "Projectiles", "Protean_TendrilProjectile - %s_%s.yaml" % (fid(TENDRIL_PROJECTILE), PLUGIN)),
          "FormKey: %s:%s\nEditorID: Protean_TendrilProjectile\n" % (fid(TENDRIL_PROJECTILE), PLUGIN)
          + "Model:\n  File: Protean\\Tendril\\Tendril.nif\n"
          "Flags:\n- Supersonic\n- PassThroughSmallTransparent\n"
          # speed and fade as Pull Mastery's chain: at 6000 and a 0.5 s fade the tendril hardly showed (user, 2026-10-23)
          "Type: Flame\nSpeed: 3600\nRange: %d\n" % GLOBALS["Protean_TendrilRange"][1]
          # no sound of its own: it played only once the tendril had come back; Protean plays the throw's sound
          + "FadeDuration: 5\nCollisionRadius: 1\nRelaunchInterval: 0.25\nMuzzleFlashModel: ''\nSoundLevel: 1\n")

    def tendril_effect(local, editor_id, title, archetype, desc):
        write(os.path.join(y, "MagicEffects", "%s - %s_%s.yaml" % (editor_id, fid(local), PLUGIN)),
              "FormKey: %s:%s\nEditorID: %s\n" % (fid(local), PLUGIN, editor_id)
              + name_block(title)
              # Alteration only so the magic menu lists it (with no school it was missing); no skill use comes of it
              + "Flags:\n- Hostile\n- Detrimental\n- NoDuration\n- NoArea\nBaseCost: 0\nMagicSkill: Alteration\n"
              "Archetype:\n  MutagenObjectType: MagicEffectArchetype\n  Type: %s\n  ActorValue: %s\n" % archetype
              + "Projectile: %s:%s\n" % (fid(TENDRIL_PROJECTILE), PLUGIN)
              + "ImpactData: 0F23C4:Skyrim.esm\n"  # flesh and blood, like the claws (a placeholder until previewed)
              "CastType: FireAndForget\nTargetType: Aimed\nSkillUsageMultiplier: 0\nDualCastScale: 1\n"
              "Description:\n  TargetLanguage: English\n  Value: %s\n" % yq(desc))

    tendril_effect(TENDRIL_DAMAGE, "Protean_TendrilDamageEffect", "Tendril", ("ValueModifier", "Health"),
                   "A whip of flesh lashes out for <mag> points of damage. It yanks a light foe to you, or hauls you "
                   "to a large one. Costs %d Stamina; %d seconds between lashes."
                   % (GLOBALS["Protean_TendrilCost"][1], GLOBALS["Protean_TendrilCooldown"][1]))
    tendril_effect(TENDRIL_DRAIN, "Protean_TendrilDrainEffect", "Tendril Drain", ("Absorb", "Stamina"),
                   "Drains <mag> points of Stamina and gives them to you.")
    write(os.path.join(y, "Spells", "Protean_TendrilSpell - %s_%s.yaml" % (fid(TENDRIL_SPELL), PLUGIN)),
          "FormKey: %s:%s\nEditorID: Protean_TendrilSpell\n" % (fid(TENDRIL_SPELL), PLUGIN)
          + name_block("Tendril")
          + "EquipmentType: 013F44:Skyrim.esm\nDescription:\n  TargetLanguage: English\n  Value: ''\n"
          "BaseCost: 0\nFlags:\n- ManualCostCalc\n- NoAbsorbOrReflect\n- NoDualCastModification\nType: Spell\nChargeTime: 0\n"
          "CastType: FireAndForget\nTargetType: Aimed\n"
          "Effects:\n"
          + "".join("- BaseEffect: %s:%s\n  Data:\n    Magnitude: %d\n" % (fid(e), PLUGIN, m)
                    for e, m in ((TENDRIL_DAMAGE, 6), (TENDRIL_DRAIN, 20))))  # Protean sets both before each throw

    # ---- Predator rank 2's poison: damage over time a power attack leaves; poison resistance counts
    write(os.path.join(y, "MagicEffects", "Protean_PredatorPoisonEffect - %s_%s.yaml" % (fid(PREDATOR_POISON_EFFECT), PLUGIN)),
          "FormKey: %s:%s\nEditorID: Protean_PredatorPoisonEffect\n" % (fid(PREDATOR_POISON_EFFECT), PLUGIN)
          + name_block("Predator's Poison")
          + "Flags:\n- Hostile\n- Detrimental\n- NoArea\nBaseCost: 0\n"
          "Archetype:\n  MutagenObjectType: MagicEffectArchetype\n  Type: ValueModifier\n  ActorValue: Health\n"
          "ResistValue: PoisonResist\nCastType: FireAndForget\nTargetType: TargetActor\nSkillUsageMultiplier: 0\nDualCastScale: 1\n"
          "Description:\n  TargetLanguage: English\n  Value: %s\n" % yq("Poison deals <mag> points of damage a second for <dur> seconds."))
    write(os.path.join(y, "Spells", "Protean_PredatorPoison - %s_%s.yaml" % (fid(PREDATOR_POISON_SPELL), PLUGIN)),
          "FormKey: %s:%s\nEditorID: Protean_PredatorPoison\n" % (fid(PREDATOR_POISON_SPELL), PLUGIN)
          + name_block("Predator's Poison")
          + "EquipmentType: 013F44:Skyrim.esm\nDescription:\n  TargetLanguage: English\n  Value: ''\n"
          "BaseCost: 0\nFlags:\n- ManualCostCalc\n- NoAbsorbOrReflect\nType: Spell\nChargeTime: 0\n"
          "CastType: FireAndForget\nTargetType: TargetActor\n"
          "Effects:\n- BaseEffect: %s:%s\n  Data:\n    Magnitude: 5\n    Duration: %d\n"  # Protean sets both before each cast
          % (fid(PREDATOR_POISON_EFFECT), PLUGIN, GLOBALS["Protean_PredatorPoisonSeconds"][1]))

    # ---- Stalker: the fade (lasts until Protean or the game ends it) and the ambush perk
    write(os.path.join(y, "Spells", "Protean_StalkerFade - %s_%s.yaml" % (fid(STALKER_FADE_SPELL), PLUGIN)),
          "FormKey: %s:%s\nEditorID: Protean_StalkerFade\n" % (fid(STALKER_FADE_SPELL), PLUGIN)
          + name_block("Stalker's Fade")
          + "EquipmentType: 013F44:Skyrim.esm\nDescription:\n  TargetLanguage: English\n  Value: ''\n"
          "BaseCost: 0\nFlags:\n- ManualCostCalc\n- NoAbsorbOrReflect\nType: Spell\nChargeTime: 0\n"
          "CastType: FireAndForget\nTargetType: Self\n"
          "Effects:\n- BaseEffect: %s\n  Data:\n    Duration: 3600\n" % VANILLA_INVISIBILITY)
    write(os.path.join(y, "Perks", "Protean_StalkerAmbush - %s_%s.yaml" % (fid(STALKER_AMBUSH_PERK), PLUGIN)),
          "FormKey: %s:%s\nEditorID: Protean_StalkerAmbush\n" % (fid(STALKER_AMBUSH_PERK), PLUGIN)
          + name_block("Stalker's Ambush")
          + "Description:\n  TargetLanguage: English\n  Value: ''\n"
          "NumRanks: 1\nPlayable: False\nHidden: True\nEffects:\n"
          "- MutagenObjectType: PerkEntryPointModifyValue\n  EntryPoint: ModAttackDamage\n"
          "  PerkConditionTabCount: 3\n  Modification: Multiply\n  Value: %s\n" % GLOBALS["Protean_StalkerAmbushMult"][1])

    # ---- the hidden skill perks (values set live by Protean)
    def hidden_perk(local, editor_id, title, entries):
        write(os.path.join(y, "Perks", "%s - %s_%s.yaml" % (editor_id, fid(local), PLUGIN)),
              "FormKey: %s:%s\nEditorID: %s\n" % (fid(local), PLUGIN, editor_id)
              + name_block(title)
              + "Description:\n  TargetLanguage: English\n  Value: ''\n"
              "NumRanks: 1\nPlayable: False\nHidden: True\nEffects:\n"
              + "".join("- MutagenObjectType: PerkEntryPointModifyValue\n  EntryPoint: %s\n"
                        "  PerkConditionTabCount: %d\n  Modification: Multiply\n  Value: 1\n" % e for e in entries))

    hidden_perk(ANIMAL_ARMS_PERK, "Protean_AnimalArms", "Animal One-Handed",
                [("ModAttackDamage", 3), ("ModPowerAttackStamina", 1)])
    hidden_perk(DEFT_PERK, "Protean_AnimalDeftMovement", "Animal Deft Movement", [("ModIncomingDamage", 3)])

    # ---- the glide markers (hidden): the Open Animation Replacer condition for the glide pose
    for i, (e, sp) in enumerate(zip(GLIDE_EFFECTS, GLIDE_SPELLS)):
        write(os.path.join(y, "MagicEffects", "Protean_GlideEffect%d - %s_%s.yaml" % (i + 1, fid(e), PLUGIN)),
              "FormKey: %s:%s\nEditorID: Protean_GlideEffect%d\n" % (fid(e), PLUGIN, i + 1)
              + name_block("Gliding")
              + "Flags:\n- Recover\n- HideInUI\n- NoArea\n- Painless\n- NoHitEffect\nBaseCost: 0\n"
              "Archetype:\n  MutagenObjectType: MagicEffectArchetype\n  Type: Script\nCastType: ConstantEffect\n"
              "SkillUsageMultiplier: 0\nDualCastScale: 1\n")
        write(os.path.join(y, "Spells", "Protean_Glide%d - %s_%s.yaml" % (i + 1, fid(sp), PLUGIN)),
              "FormKey: %s:%s\nEditorID: Protean_Glide%d\n" % (fid(sp), PLUGIN, i + 1)
              + name_block("Gliding")
              + "EquipmentType: 013F44:Skyrim.esm\nDescription:\n  TargetLanguage: English\n  Value: ''\n"
              "BaseCost: 0\nFlags:\n- ManualCostCalc\nType: Ability\nChargeTime: 0\nCastType: ConstantEffect\nTargetType: Self\n"
              "Effects:\n- BaseEffect: %s:%s\n  Data:\n    Magnitude: 0\n" % (fid(e), PLUGIN))

    # ---- the kill boost, as Active Effects shows it
    write(os.path.join(y, "MagicEffects", "Protean_KillBoostEffect - %s_%s.yaml" % (fid(KILL_BOOST_EFFECT), PLUGIN)),
          "FormKey: %s:%s\nEditorID: Protean_KillBoostEffect\n" % (fid(KILL_BOOST_EFFECT), PLUGIN)
          + name_block("Kill Boost")
          + "Flags:\n- Recover\n- NoArea\n- Painless\n- NoHitEffect\nBaseCost: 0\n"
          "Archetype:\n  MutagenObjectType: MagicEffectArchetype\n  Type: Script\nCastType: ConstantEffect\n"
          "SkillUsageMultiplier: 0\nDualCastScale: 1\n"
          "Description:\n  TargetLanguage: English\n  Value: Stamina regenerates <mag>% faster. Every kill adds to it for a while.\n")
    write(os.path.join(y, "Spells", "Protean_KillBoost - %s_%s.yaml" % (fid(KILL_BOOST_SPELL), PLUGIN)),
          "FormKey: %s:%s\nEditorID: Protean_KillBoost\n" % (fid(KILL_BOOST_SPELL), PLUGIN)
          + name_block("Kill Boost")
          + "EquipmentType: 013F44:Skyrim.esm\nDescription:\n  TargetLanguage: English\n  Value: ''\n"
          "BaseCost: 0\nFlags:\n- ManualCostCalc\nType: Ability\nChargeTime: 0\nCastType: ConstantEffect\nTargetType: Self\n"
          "Effects:\n- BaseEffect: %s:%s\n  Data:\n    Magnitude: 15\n" % (fid(KILL_BOOST_EFFECT), PLUGIN))

    # ---- Keen
    power(KEEN_POWERS["night"], "Protean_KeenNightPower", "Night Vision",
          "In animal form, see in the dark. Cast again to stop.")
    power(KEEN_POWERS["life"], "Protean_KeenLifePower", "Detect Life",
          "In animal form, sense the living through walls, at a steady cost in Stamina. Cast again to stop.")
    power(KEEN_POWERS["dead"], "Protean_KeenDeadPower", "Detect Dead",
          "In animal form, sense the dead and undead through walls, at a steady cost in Stamina. Cast again to stop.")
    power(STALKER_POWER, "Protean_StalkPower", "Stalk",
          "In animal form, while on, keep still or walk to fade from sight; moving while hidden costs Stamina. Cast again to stop.")
    # Detect Dead as fire-and-forget: the game's held Detect Undead effects (DetectUndead*ConcSelf), same looks and
    # target conditions, with a duration.
    dead_kw = {"enemy": ("013797", "01397A", "0F82F4"), "friend": ("0F82F4", "013797", "01397A")}
    for (side, indoors), local in zip((("enemy", False), ("enemy", True), ("friend", False), ("friend", True)),
                                      KEEN_DEAD_EFFECTS):
        place = "Interior" if indoors else "Exterior"
        cond = ("- MutagenObjectType: ConditionFloat\n  Data:\n    MutagenObjectType: IsInInteriorConditionData\n"
                + ("  ComparisonValue: 1\n" if indoors else ""))
        cond += "".join("- MutagenObjectType: ConditionFloat\n  Data:\n    MutagenObjectType: HasKeywordConditionData\n"
                        "    Keyword: %s:Skyrim.esm\n" % k for k in dead_kw[side])
        either = [("HasKeywordConditionData", "013796")]
        if side == "enemy":
            either += [("HasMagicEffectKeywordConditionData", "02482B"), ("GetDeadConditionData", None)]
        either += [("IsUndeadConditionData", None)]
        cond += "".join("- MutagenObjectType: ConditionFloat\n  Flags:\n  - OR\n  Data:\n    MutagenObjectType: %s\n" % t
                        + ("    Keyword: %s:Skyrim.esm\n" % k if k else "") + "  ComparisonValue: 1\n" for t, k in either)
        editor_id = "Protean_KeenDetectDead%s%s" % (side.capitalize(), place)
        write(os.path.join(y, "MagicEffects", "%s - %s_%s.yaml" % (editor_id, fid(local), PLUGIN)),
              "FormKey: %s:%s\nEditorID: %s\n" % (fid(local), PLUGIN, editor_id)
              + name_block("Detect Dead")
              + "MenuDisplayObject: 104F70:Skyrim.esm\n"
              "Flags:\n- Recover\n- NoHitEvent\n- FXPersist\n- NoRecast\n- Painless\n- NoDeathDispel\n"
              "BaseCost: 0\nMagicSkill: Alteration\nCastingLight: 03BA90:Skyrim.esm\n"
              "HitShader: %s:Skyrim.esm\nSpellmakingArea: 200\n" % ("016439" if side == "enemy" else "000146")
              + "Archetype:\n  MutagenObjectType: MagicEffectArchetype\n  Type: DetectLife\n"
              "CastType: FireAndForget\nSkillUsageMultiplier: 0\nDualCastScale: 1\n"
              "ImageSpaceModifier: 02FB1E:Skyrim.esm\nCastingSoundLevel: Silent\n"
              "Description:\n  TargetLanguage: English\n  Value: ''\nConditions:\n" + cond)

    # The senses Protean casts on the player. Night vision lasts until Protean dispels it (cast again, change back).
    # The detects put their glow on the creatures around, not on the player, so they are short pulses that Protean
    # repeats while the sense is on (as the Vampire Lord's Detect All Creatures: 2 s). Areas as the Detect Life
    # spell's: 200 ft outdoors, 100 indoors.
    def sense(local, editor_id, title, effects, duration):
        write(os.path.join(y, "Spells", "%s - %s_%s.yaml" % (editor_id, fid(local), PLUGIN)),
              "FormKey: %s:%s\nEditorID: %s\n" % (fid(local), PLUGIN, editor_id)
              + name_block(title)
              + "EquipmentType: 013F44:Skyrim.esm\nDescription:\n  TargetLanguage: English\n  Value: ''\n"
              "BaseCost: 0\nFlags:\n- ManualCostCalc\n- NoAbsorbOrReflect\n- AreaEffectIgnoresLOS\n"
              "Type: Spell\nChargeTime: 0\nCastType: FireAndForget\nTargetType: Self\n"
              "Effects:\n"
              + "".join("- BaseEffect: %s\n  Data:\n%s    Duration: %d\n" % (e, "    Area: %d\n" % a if a else "", duration)
                        for e, a in effects))

    sense(KEEN_SENSES["night"], "Protean_KeenNight", "Night Vision", [("06B10C:Skyrim.esm", 0)], 86400)
    sense(KEEN_SENSES["life"], "Protean_KeenLife", "Detect Life",
          [("1019D8:Skyrim.esm", 200), ("1019D9:Skyrim.esm", 100), ("1019DA:Skyrim.esm", 200), ("1019DB:Skyrim.esm", 100)], 2)
    sense(KEEN_SENSES["dead"], "Protean_KeenDead", "Detect Dead",
          [("%s:%s" % (fid(e), PLUGIN), a) for e, a in zip(KEEN_DEAD_EFFECTS, (200, 100, 200, 100))], 2)

    # The tree for Custom Skills Framework (Exit-9B, MIT): SKSE/Plugins/CustomSkills/Protean.json. No skill level, so it
    # looks like the vanilla beast trees; the points are Protean_PerkPoints, which Protean keeps up to date.
    def ref(local):
        return "%s|%X" % (PLUGIN, local)

    write(os.path.join(ROOT, "data", "SKSE", "Plugins", "CustomSkills", "Protean.json"), json.dumps({
        "$schema": "https://raw.githubusercontent.com/Exit-9B/CustomSkills/main/docs/schema/CustomSkill.json",
        "version": 1,
        "skydome": {"model": "DLC01/Interface/INTVampirePerkSkydome.nif", "cameraRightPoint": 2},
        "showMenu": ref(GLOBALS["Protean_ShowPerkMenu"][0]),
        "perkPoints": ref(GLOBALS["Protean_PerkPoints"][0]),
        "skills": [{
            "id": "protean",
            "name": "Protean",
            "description": "New perks come with feeding. Perk points take a little more feeding as you go.",
            "nodes": [{"id": pid, "perk": ref(local), "x": round(x * TREE_SCALE_X, 2), "y": round(yy * TREE_SCALE_Y, 2),
                       **({"links": links} if links else {})}
                      for pid, (local, _, _, x, yy, links) in PERKS.items()],
        }],
    }, indent=2))

    write(os.path.join(y, "Books", "Protean_SpellTomeInstinct - %s_%s.yaml" % (fid(TOME), PLUGIN)),
          "FormKey: %s:%s\nEditorID: Protean_SpellTomeInstinct\n" % (fid(TOME), PLUGIN)
          + "ObjectBounds:\n  First: -9, -11, -1\n  Second: 9, 11, 2\n"
          + name_block("Spell Tome: Shapeshifter's Instinct")
          + "Model:\n  File: Clutter\\Books\\SpellTomeAlterationLowPoly.nif\n"
          "BookText:\n  TargetLanguage: English\n  Value: \"<font face='$HandwrittenFont'><font size='40'><p align='center'>Shapeshifter's Instinct\"\n"
          "Keywords:\n- 0937A2:Skyrim.esm\n- 0937A5:Skyrim.esm\nFlags:\n- 0x4\n"
          "Teaches:\n  MutagenObjectType: BookSpell\n  Spell: %s:%s\n" % (fid(STARTER), PLUGIN)
          + "Value: 300\nWeight: 1\nInventoryArt: 02FBB3:Skyrim.esm\n"
          "Description:\n  TargetLanguage: English\n  Value: ''\n")

    npcs = json.load(open(os.path.join(ROOT, "research", "raw", "npcs.json"), encoding="utf-8"))
    runtime = []
    for index, c in enumerate(creatures):
        if c["id"] not in enabled:
            continue
        heart = FIRST_CREATURE + 2 * index
        pw = heart + 1
        bnd = "ObjectBounds:\n  First: -3, -6, -3\n  Second: 3, 6, 3\n"
        write(os.path.join(y, "Ingestibles", "Protean_Heart_%s - %s_%s.yaml" % (c["id"], fid(heart), PLUGIN)),
              "FormKey: %s:%s\nEditorID: Protean_Heart_%s\n" % (fid(heart), PLUGIN, c["id"])
              + bnd + name_block(c["unique_item"])
              + "Keywords:\n- 08CDEA:Skyrim.esm\n"
              "Model:\n  File: %s\n" % table["item_model"].replace("\\", "\\\\").join(['"', '"'])
              + "PickUpSound: 03EDBD:Skyrim.esm\nPutDownSound: 03EDC0:Skyrim.esm\nWeight: 0.5\nValue: 5\n"
              "Flags:\n- NoAutoCalc\n- FoodItem\nConsumeSound: 0B6435:Skyrim.esm\n"
              "Effects:\n- BaseEffect: %s:%s\n  Data: {}\n" % (fid(HEART_EFFECT), PLUGIN))
        power(pw, "Protean_Power_%s" % c["id"], "Shapeshift: %s" % c["name"],
              "Become a %s. Cast again, or use Revert Form, to return." % c["name"].lower())

        # The marker the player carries while in this form: a hidden ability whose effect has the keywords
        # Protean_Transformed and Protean_Form_<id> (conditions: HasMagicEffectKeyword).
        kw, fx, ab = FORM_KEYWORD + index, FORM_EFFECT + index, FORM_ABILITY + index
        keyword(kw, "Protean_Form_%s" % c["id"])
        write(os.path.join(y, "MagicEffects", "Protean_FormEffect_%s - %s_%s.yaml" % (c["id"], fid(fx), PLUGIN)),
              "FormKey: %s:%s\nEditorID: Protean_FormEffect_%s\n" % (fid(fx), PLUGIN, c["id"])
              + name_block("%s Form" % c["name"])
              + "Flags:\n- NoDuration\n- NoMagnitude\n- NoArea\n- HideInUI\n- Painless\n- NoHitEffect\nBaseCost: 0\n"
              "Archetype:\n  MutagenObjectType: MagicEffectArchetype\n  Type: Script\n"
              "SkillUsageMultiplier: 1\nDualCastScale: 1\n"
              "Description:\n  TargetLanguage: English\n  Value: ''\n"
              "Keywords:\n- %s:%s\n- %s:%s\n" % (fid(KW_TRANSFORMED), PLUGIN, fid(kw), PLUGIN))
        write(os.path.join(y, "Spells", "Protean_FormAbility_%s - %s_%s.yaml" % (c["id"], fid(ab), PLUGIN)),
              "FormKey: %s:%s\nEditorID: Protean_FormAbility_%s\n" % (fid(ab), PLUGIN, c["id"])
              + name_block("%s Form" % c["name"])
              + "EquipmentType: 013F44:Skyrim.esm\nDescription:\n  TargetLanguage: English\n  Value: ''\n"
              "Flags:\n- ManualCostCalc\nType: Ability\nChargeTime: 0.5\n"
              "Effects:\n- BaseEffect: %s:%s\n  Data: {}\n" % (fid(fx), PLUGIN))

        rf, rid = c["race_form"].split(":")
        runtime.append({
            "id": c["id"], "name": c["name"], "index": index,
            "race": {"file": rf, "id": int(rid, 16)},
            "heart": heart, "power": pw, "ability": ab,
            "size": c["size_class"],
            "attacks": [{"event": a["event"], "flags": a["flags"], "spell": a["spell"]} for a in c.get("attacks", []) if a["event"] not in by_race(BLOCKED, c, creatures)],
            "camera_height": round(0.9 * c["bound_height"]) if c.get("bound_height") else 60,
            "factions": [{"file": f.split(":")[0], "id": int(f.split(":")[1], 16)} for f in c["factions"] + EXTRA_FACTIONS.get(c["id"], [])],
            "ladder": c["health_ladder"],
            # vanilla NPC records of this creature: dead ones get a heart when looted
            "race_names": [c["race"]],
            **body(c, npcs),
            **by_race(MELEE, c, creatures),
            **({"sprint": SPRINT[c["id"]]} if c["id"] in SPRINT else {}),
            **({"diet": DIET[c["id"]][0], "feed_event": DIET[c["id"]][1]} if c["id"] in DIET else {}),
        })

    # Keywords that mark a location as a cave-like open interior (Troll/Horker may go there). Measured over
    # the 757 vanilla locations: 274 open, 271 building, 7 conflicting, 205 neither (mostly exteriors).
    open_names = {"LocTypeDungeon", "LocTypeMine", "LocTypeDraugrCrypt", "LocTypeFalmerHive", "LocTypeAnimalDen",
                  "LocTypeDragonLair", "LocTypeDragonPriestLair", "LocTypeSprigganGrove", "LocTypeHagravenNest",
                  "LocTypeWarlockLair", "LocTypeVampireLair", "LocTypeWerewolfLair", "LocTypeWerebearLair",
                  "LocTypeGiantCamp", "LocTypeDwarvenAutomatons", "LocTypeBanditCamp", "LocTypeForswornCamp",
                  "LocTypeMilitaryFort", "LocTypeShipwreck"}
    open_kw = []
    lk = os.path.join(ROOT, "research", "raw", "loc_keywords.json")
    if os.path.isfile(lk):
        for k, name in json.load(open(lk, encoding="utf-8"))["kw"].items():
            if name in open_names:
                f, i = k.split(":")
                open_kw.append({"file": f, "id": int(i, 16), "name": name})
    else:
        print("WARNING: research/raw/loc_keywords.json missing (run tools/dump_keywords.js); cave rule disabled")

    out = os.path.join(ROOT, "data", "SKSE", "Plugins", "Protean", "creatures.json")
    write(out, json.dumps({"plugin": PLUGIN, "effect": EFFECT, "starter": STARTER, "revert": REVERT,
                           "tome": TOME, "beastFaction": BEAST_FACTION, "witnessFaction": WITNESS_FACTION,
                           "facePower": FACE_POWER, "disguiseFaction": DISGUISE_FACTION, "exposedFaction": EXPOSED_FACTION,
                           "evolution": EVOLUTION,
                           "claws": {"weapon": CLAW_WEAPON, "right": CLAW_ARMO_R, "left": CLAW_ARMO_L},
                           "tendril": {"spell": TENDRIL_SPELL, "projectile": TENDRIL_PROJECTILE},
                           "keen": {"powers": KEEN_POWERS, "senses": KEEN_SENSES}, "killBoost": KILL_BOOST_SPELL, "glideMarkers": GLIDE_SPELLS,
                           "perks": {**{pid: v[0] for pid, v in PERKS.items()}, **{pid + "_2": v[0] for pid, v in RANK2.items()}},
                           "perksBuilt": sorted(BUILT | {pid + "_2" for pid in BUILT if pid in RANK2 and pid not in RANK2_UNBUILT}),
                           "predatorPoison": PREDATOR_POISON_SPELL,
                           "stalker": {"fade": STALKER_FADE_SPELL, "ambush": STALKER_AMBUSH_PERK, "power": STALKER_POWER},
                           "skills": {"arms": ANIMAL_ARMS_PERK, "deft": DEFT_PERK},
                           "globals": {name: v[0] for name, v in GLOBALS.items()},
                           "openInteriorKeywords": open_kw, "creatures": runtime}, indent=1))
    print("yaml written; creatures enabled:", ", ".join(enabled))

    if a.no_esp:
        return
    esp = os.path.join(ROOT, "data", PLUGIN)
    os.makedirs(os.path.dirname(esp), exist_ok=True)
    cmd = ["bash", GAME + "/tools/spriggit-cli.sh", "deserialize", "--InputPath", y, "--OutputPath", esp]
    print("running:", " ".join(cmd))
    r = subprocess.run(cmd, cwd=GAME, capture_output=True, text=True)
    print((r.stdout or "")[-1500:], (r.stderr or "")[-1500:])
    if r.returncode != 0 or not os.path.isfile(esp):
        sys.exit("spriggit deserialize failed")
    print("built", esp, os.path.getsize(esp), "bytes")


if __name__ == "__main__":
    main()
