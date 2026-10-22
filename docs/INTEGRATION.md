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

## 6. Rules a larger mod should know

- Protean owns the player's race, gear, voice slot and camera while transformed. Do not change them
  from outside during a form; listen for `Protean_Reverted` first.
- The player's maximum health gains a bonus while transformed. Health keeps its percentage of the maximum
  when changing in and out (never below 1%), so changing back cannot kill.
- The player's collision body stays human-sized in every form (a vanilla limitation, accepted).
- The co-save (`Protean`) keeps the state; Protean leaves nothing in the save proper once human again.
