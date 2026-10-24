# Protean integration reference

For a larger mod built on Protean: how to tell what the player is, how to react when it changes, and
how to tune what Protean does. Everything here is in `Protean.esp` (a light plugin) or the SKSE plugin.

Form IDs are given as `xxNNN`: the local ID inside `Protean.esp`. In Papyrus use
`Game.GetFormFromFile(0xNNN, "Protean.esp")`; in a plugin that masters `Protean.esp`, the records are
referenced directly. The local IDs never change: a new creature is always appended, never inserted.

## 1. What the player is

| Marker | Where | Use it for |
|---|---|---|
| Keyword `Protean_Transformed` | xx80A | Any animal form. Condition: `HasMagicEffectKeyword Protean_Transformed` |
| Keyword `Protean_Form_<id>` | table below | One specific form. Condition: `HasMagicEffectKeyword Protean_Form_wolf` |
| Global `Protean_ActiveForm` | xx940 | `0` = human, otherwise the form's number (table below). Condition: `GetGlobalValue` |

The keywords sit on a hidden constant ability (`Protean_FormAbility_<id>`) that the player carries only
while in that form. Protean adds and removes it; do not add it yourself. Do not condition on the
animal's *race* (`GetIsRace WolfRace`): that is true for every wild wolf as well.

## 2. When it changes: SKSE mod events

Papyrus: `RegisterForModEvent("Protean_Transformed", "OnProteanTransformed")`, then
`Event OnProteanTransformed(string eventName, string formId, float formNumber, Form sender)`.
`sender` is the player.

| Event | `strArg` | `numArg` | Sent |
|---|---|---|---|
| `Protean_Transformed` | form id, e.g. `wolf` | form number | after the body has changed and the markers are on |
| `Protean_Reverted` | the form left | its number | after the human body is back and the markers are off |
| `Protean_Learned` | form id | form number | when a heart teaches a new form |

A change the player asks for plays 1.5 s of effects first; the event comes when the body actually
changes. Forced changes (death, loading a save, too big for an interior) are immediate.

## 3. Dials: change what Protean does

Globals in `Protean.esp`, read live every time they are used, so a perk's script fragment, a quest
script or the console can change them at any time.

| Global | ID | Type | Default | Effect |
|---|---|---|---|---|
| `Protean_ActiveForm` | xx940 | Short | 0 | 0 = human; otherwise the form's index + 1 (set by Protean, read-only) |
| `Protean_HealthBonusPct` | xx941 | Float | 0.25 | bonus health = this x the player's normal max health ... |
| `Protean_HealthGapShare` | xx942 | Float | 0.25 | ... plus this x how far the animal's health (at the player's level) exceeds the player's; 0 if it does not |
| `Protean_SprintMult` | xx943 | Float | 1.0 | multiplies every animal's sprint boost |
| `Protean_SprintStaminaPerSec` | xx944 | Float | 7.0 | stamina drained per second of sprinting |
| `Protean_LeapMult` | xx945 | Float | 1.0 | multiplies leap distance |
| `Protean_LeapHeightMult` | xx946 | Float | 1.0 | multiplies leap height |
| `Protean_FlySpeed` | xx947 | Float | 600 | flight speed, game units per second |
| `Protean_SpitCostMult` | xx948 | Float | 0.5 | spit stamina cost as a share of a power attack's |
| `Protean_ReachMult` | xx949 | Float | 1.0 | multiplies the forms' melee reach |
| `Protean_StrikeAngleMult` | xx94A | Float | 1.0 | multiplies the forms' strike cone |
| `Protean_DamageMult` | xx94B | Float | 1.0 | multiplies all damage the player deals while transformed |
| `Protean_ArmorBonus` | xx94C | Float | 0 | armor rating added while transformed |
| `Protean_ChangeStaminaCost` | xx94D | Float | 15 | stamina a change into a form the player asks for costs (refused if too tired); 0 = free |
| `Protean_KillBoostSeconds` | xx94E | Float | 90 | how long one kill's stamina-regeneration stack lasts (needs the Unbound Form perk) |
| `Protean_KillBoostPct` | xx94F | Float | 15 | stamina regeneration each stack adds, percentage points of the normal rate |
| `Protean_KillBoostMax` | xx950 | Float | 0 | most stacks at once; 0 = no limit |
| `Protean_ClawDamageMin` | xx953 | Float | 6 | Rending Claws damage never goes below this |
| `Protean_ClawDamageMax` | xx954 | Float | 10 | Rending Claws damage on a full stamina bar, before the level parts (full bar = this + PerLevel x level + PerLevelSq x level²: 11 at 1, 19 at 10, 49 at 30, 95 at 50) |
| `Protean_ClawDamagePerLevel` | xx955 | Float | 0.7 | Rending Claws full-bar damage added per player level |
| `Protean_ClawSwingCost` | xx956 | Float | 5 | stamina each Rending Claws swing costs; 0 = free |
| `Protean_ClawDamagePerLevelSq` | xx957 | Float | 0.02 | Rending Claws full-bar damage added per player level squared |
| `Protean_ClawEmptyShare` | xx958 | Float | 0.3 | Rending Claws damage on an empty stamina bar, as a share of the full-bar damage (between the two it follows the bar) |
| `Protean_TendrilCost` | xx959 | Float | 25 | stamina each Tendril lash costs (given back on a miss) |
| `Protean_TendrilCooldown` | xx95A | Float | 3 | seconds between Tendril lashes (none after a miss) |
| `Protean_TendrilDamageShare` | xx95B | Float | 0.5 | Tendril damage as a share of Rending Claws' full-bar damage |
| `Protean_TendrilDrain` | xx95C | Float | 20 | stamina a Tendril hit drains and gives the player, before the level part |
| `Protean_TendrilDrainPerLevel` | xx95D | Float | 0.5 | Tendril drain added per player level |
| `Protean_TendrilRange` | xx95E | Float | 1500 | reach in game units (~21 m); the projectile record's range is set from it when the plugin is built |
| `Protean_TendrilPullMult` | xx95F | Float | 1.0 | multiplies both pulls: the yank's force (15) and the haul's speed (2200 units/s) |
| `Protean_KeenDrain` | xx976 | Float | 2 | stamina per second each of Keen's Detect Life and Detect Dead drains while on |
| `Protean_RushSpeed` | xx977 | Float | 0.1 | Predator's Rush: walking and running speed added, as a share (0.1 = +10%) |
| `Protean_RushSprintStart` | xx978 | Float | 1.25 | Predator's Rush: sprint speed when a sprint begins, times a normal sprint |
| `Protean_RushSprintTop` | xx979 | Float | 3.0 | Predator's Rush: the most a held sprint builds to, times a normal sprint |
| `Protean_RushRampSeconds` | xx97A | Float | 10 | Predator's Rush: seconds of unbroken sprinting to reach the top speed |
| `Protean_RushJumpHeight` | xx97B | Float | 400 | Predator's Rush: jump height (the game's `fJumpHeightMin`, 76 normally) |
| `Protean_FlightJumpCost` | xx994 | Float | 15 | Prey's Flight: stamina the jump in mid-air costs |
| `Protean_FlightJumpHeight` | xx995 | Float | 1.25 | Prey's Flight: the jump in mid-air, as a share of the player's own jump height |
| `Protean_GlideCost` | xx996 | Float | 5 | Prey's Flight: stamina per second of gliding |
| `Protean_GlideFallSpeed` | xx997 | Float | 250 | Prey's Flight: gliding fall speed, game units a second |
| `Protean_SlamCost` | xx998 | Float | 30 | Prey's Flight: stamina the dive costs |
| `Protean_SlamRadius` | xx999 | Float | 550 | Prey's Flight: the dive's landing staggers everyone this near (~8 m), followers too |
| `Protean_SlamMinAir` | xx99A | Float | 1.0 | Prey's Flight: seconds in the air before a power attack dives |
| `Protean_GlideSpeed` | xx99B | Float | 900 | Prey's Flight: the least speed a glide carries the player forward, where they face |

Notes:
- **Health bonus** is worked out when the player changes, so new values take effect at the next change. Health keeps its
  percentage of the maximum across a change, both ways.
- **Armor and damage** are applied when the player changes and taken off when they change back, so a
  new value takes effect at the next change. Everything else applies at once.
- **Damage:** ordinary perks with the `Mod Attack Damage` entry point already reach every form's
  attacks (measured: a +25% perk raised an elk's kick from 5 to 6.25). `Protean_DamageMult` is a
  Protean-wide multiplier on top. Perks conditioned on weapon types or worn gauntlets will not apply:
  an animal holds nothing.
- **Armor:** perks that boost worn armor pieces do nothing for an animal (it wears none). Raise
  `Protean_ArmorBonus`, or the `DamageResist` actor value directly, instead.
- **Stamina:** the forms use the player's own stamina. Sprint drain and spit cost have their own dials;
  power attacks cost what the game's power-attack rule says (40 x `fPowerAttackStaminaPenalty`).
- Per-form values (reach, cone, sprint strength, leap, taps) live in
  `SKSE/Plugins/Protean/creatures.json`; the dials scale them.

## 4. The forms

| Form | id | Number | Power | Heart | Hidden ability | Keyword |
|---|---|---|---|---|---|---|
| Wolf | `wolf` | 1 | xx811 | xx810 | xx900 | xx880 |
| Ice Wolf | `ice_wolf` | 2 | xx813 | xx812 | xx901 | xx881 |
| Pit Wolf | `pit_wolf` | 3 | xx815 | xx814 | xx902 | xx882 |
| Deer | `deer` | 4 | xx817 | xx816 | xx903 | xx883 |
| Elk | `elk` | 5 | xx819 | xx818 | xx904 | xx884 |
| Fox | `fox` | 6 | xx81B | xx81A | xx905 | xx885 |
| Hare | `hare` | 7 | xx81D | xx81C | xx906 | xx886 |
| Dog | `dog` | 8 | xx81F | xx81E | xx907 | xx887 |
| Goat | `goat` | 9 | xx821 | xx820 | xx908 | xx888 |
| Cow | `cow` | 10 | xx823 | xx822 | xx909 | xx889 |
| Bear | `bear` | 11 | xx825 | xx824 | xx90A | xx88A |
| Cave Bear | `cave_bear` | 12 | xx827 | xx826 | xx90B | xx88B |
| Snow Bear | `snow_bear` | 13 | xx829 | xx828 | xx90C | xx88C |
| Sabre Cat | `sabre_cat` | 14 | xx82B | xx82A | xx90D | xx88D |
| Snowy Sabre Cat | `snowy_sabre_cat` | 15 | xx82D | xx82C | xx90E | xx88E |
| Vale Sabre Cat | `vale_sabre_cat` | 16 | xx82F | xx82E | xx90F | xx88F |
| Death Hound | `death_hound` | 17 | xx831 | xx830 | xx910 | xx890 |
| Boar | `boar` | 18 | xx833 | xx832 | xx911 | xx891 |
| Skeever | `skeever` | 19 | xx835 | xx834 | xx912 | xx892 |
| Chicken | `chicken` | 20 | xx837 | xx836 | xx913 | xx893 |
| Ash Hopper | `ash_hopper` | 21 | xx839 | xx838 | xx914 | xx894 |
| Mudcrab | `mudcrab` | 22 | xx83B | xx83A | xx915 | xx895 |
| Chaurus | `chaurus` | 24 | xx83F | xx83E | xx917 | xx897 |
| Troll | `troll` | 25 | xx841 | xx840 | xx918 | xx898 |
| Frost Troll | `frost_troll` | 26 | xx843 | xx842 | xx919 | xx899 |
| Horse | `horse` | 27 | xx845 | xx844 | xx91A | xx89A |
| Horker | `horker` | 28 | xx847 | xx846 | xx91B | xx89B |
| Mammoth | `mammoth` | 29 | xx849 | xx848 | xx91C | xx89C |
| Giant | `giant` | 30 | xx84B | xx84A | xx91D | xx89D |
| Werewolf | `werewolf` | 31 | xx84D | xx84C | xx91E | xx89E |
| Werebear | `werebear` | 32 | xx84F | xx84E | xx91F | xx89F |
| Slaughterfish | `slaughterfish` | 33 | xx851 | xx850 | xx920 | xx8A0 |
| Chaurus Hunter | `chaurus_hunter` | 34 | xx853 | xx852 | xx921 | xx8A1 |
| Chaurus Hunter Fledgling | `chaurus_hunter_fledgling` | 35 | xx855 | xx854 | xx922 | xx8A2 |
| Frostbite Spider | `frostbite_spider` | 36 | xx857 | xx856 | xx923 | xx8A3 |

Numbers skip any creature in the table that is not enabled (the albino spider), so they are stable,
not contiguous.

## 5. Other records

| Record | ID | What |
|---|---|---|
| `Protean_Instinct` (lesser power) | xx801 | Starting power: lists the forms known |
| `Protean_RevertForm` (lesser power) | xx802 | Change back; equipped in the voice slot while transformed |
| `Protean_SpellTomeInstinct` (book) | xx803 | Teaches the starting power |
| `Protean_BeastFaction` | xx805 | The player is in it while transformed |
| `Protean_WitnessFaction` | xx806 | People who were hostile when they saw the change |
| `Protean_FacePower` (lesser power) | xx807 | Stolen Face |
| `Protean_DisguiseFaction` / `Protean_ExposedFaction` | xx808 / xx809 | Face stealing |
| `Protean_TendrilSpell` (spell) | xx989 | Tendril, taught by its perk (xx967); either hand |
| `Protean_TendrilDamageEffect` / `Protean_TendrilDrainEffect` | xx987 / xx988 | Damage Health / Absorb Stamina; Protean sets the magnitudes from the player's level |
| `Protean_TendrilProjectile` | xx986 | The tendril itself (flame-type projectile, model `meshes/Protean/Tendril/Tendril.nif`) |
| `Protean_KeenNightPower` / `LifePower` / `DeadPower` (lesser powers) | xx98A / xx98B / xx98C | Keen's senses; known only while an animal owns the perk (xx961) |
| `Protean_KeenNight` / `Protean_KeenLife` / `Protean_KeenDead` (self spells) | xx98D / xx98E / xx98F | What Protean casts: the game's night eye; 2 s Detect Life / Detect Dead pulses repeated while on |
| `Protean_KeenDetectDead*` (effects) | xx990-xx993 | The game's held Detect Undead effects as fire-and-forget ones |
| `Protean_KillBoost` (ability) / `Protean_KillBoostEffect` | xx97D / xx97C | Shows Unbound Form's kill boost in Active Effects (magnitude = the current total) |
| `Protean_Glide1` (ability) / `Protean_GlideEffect1` | xx99F / xx99C | Hidden, on the player while gliding: Open Animation Replacer's condition for the glide pose |

## 6. Rules a larger mod should know

- Protean owns the player's race, gear, voice slot and camera while transformed. Do not change them
  from outside during a form; listen for `Protean_Reverted` first.
- The player's maximum health gains a bonus while transformed. Health keeps its percentage of the maximum
  when changing in and out (never below 1%), so changing back cannot kill.
- The player's collision body stays human-sized in every form (a vanilla limitation, accepted).
- The co-save (`Protean`) keeps the state; Protean leaves nothing in the save proper once human again.
- Tendril pulls: a hit on a Small or Medium race knocks the target off its feet toward the player; on a Large or
  Extra Large race (trolls, bears, giants, mammoths, grounded dragons) it hauls the player to the target instead,
  with no fall damage from the haul. Nothing flying, swimming, following the player or essential is pulled
  (it is still damaged). Its hits are ordinary hostile magic: crime and aggression as for any spell.
- Predator's Rush (human form) sets the game settings `fJumpHeightMin` (to `Protean_RushJumpHeight`) and
  `fJumpFallHeightMin` (raised by as much, so a big jump hurts no more than a normal one) while it works, and puts
  them back otherwise. It also starts a jump while sprinting, which the game does not allow.
- Prey's Flight: no fall damage in any form (the fall's start height follows the player down). In human form a jump in
  mid-air, a glide (Jump held while falling) and a dive (a power attack in the air) whose landing staggers everyone
  near. With Rush or Flight Protean keeps long jumps in their jump and fall animations, and puts a player the engine
  wrongly holds "on the ground" in mid-air (gravity off, a slow float) back in the air.
- The glide pose is Protean's own animation (`meshes/actors/character/animations/OpenAnimationReplacer/Protean/Glide/`,
  replacing `mt_jumpfall` while `Protean_GlideEffect1` is on). An animation mod that replaces the fall at a higher
  OAR priority than 2,000,000,001 would hide it.
- Hearts go into an animal's body as it is searched (Protean hooks the NPC base form's activation), so the loot window
  shows them the first time.
- Favorites in animal form works like the Vampire Lord's: it lists only Revert Form and the Keen powers the player
  knows. While that menu is being built, Protean points the default objects `VampireRace` and `VampireSpells` at the
  animal's race and its own list, then puts them back; a mod that reads those two during the Favorites menu sees
  Protean's values.
