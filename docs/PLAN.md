# Plan

## Decisions made by the user
- Becoming a shapeshifter: a **spell tome** (craftable at a tanning rack, like the Hare mod).
- Learn a form by **eating a unique item per creature**; one power per animal; press again to revert.
- Real **swimming** for Slaughterfish (cannot leave the water); real **flight** for Chaurus Hunters.
- Big creatures blocked indoors: Troll/Horker allowed in caves, Mammoth outdoors only.
- Health bonus = half the creature's max health at the player's current level; dies on revert if damage
  taken exceeds normal max health.
- Extras added: **Boar**, **Giant** (35 creatures total).
- Approach: **SKSE plugin (C++)** for witness detection, flight, and the transformed-only attack menu;
  Papyrus only where it is the better tool.
- Project folder: `C:/Modding/Protean`.

- **Items:** every unlock item is named "<Animal> Heart" and uses the vanilla Human Heart mesh
  (`Clutter\Ingredients\HumanHeart.nif`, Skyrim.esm:0B18CD), referenced in place, no asset copied.
  Plan: food (ALCH) records so they show "Eat" and stay out of alchemy; confirm in the Phase 1 spike.
- **Giant:** outdoors only, like the Mammoth.
- **Too-big places:** a form that is too big for where the player is gets auto-reverted with a message
  (applies when entering such a place while transformed, and blocks transforming there).
- **Eating a known heart again:** does nothing for now. Bonus feature deferred to later.

## Still open (ask before building)
- Nothing blocking Phase 1. Size classes in `docs/creature-table.md` stand as approved.

## Decisions added 2026-10-19 (remote answers)
- **Crime:** normal vanilla rules. Attacking humans is a crime as usual (the wolf is still the player); attacks on
  animals are not (the game never counted them). Nothing is suppressed or frozen.
- **Activating things as an animal is blocked:** no doors, no containers, no dialogue, no picking things up
  (the Activate control is locked while transformed; pressing E shows a short message). Menus stay locked as before.
- **Phase 2 menu approved:** Beast Moves window (key B) + HUD strip + number keys (1-8 = left mouse button,
  Shift+1-8 = right mouse button), assignments saved per animal in the co-save.
- **Flight v1 approved:** hovering with the wing-idle animation and the ground-dive animation (see FLIGHT-RESEARCH.md).
- **Starter tome: console only for now** (`player.additem FE004803 1` or `player.addspell FE004801`). No vendor,
  no crafting recipe, no SPID.

## Phases
1. Core system, 3 creatures: Wolf (predator), Deer (prey), Skeever (tiny).
2. Special attacks and the transformed-only attack menu (SKSE Menu Framework is installed).
3. Remaining land creatures, by adding rows to `data/creatures.json`.
4. Hard cases: Slaughterfish, Chaurus Hunter (+Fledgling), Mammoth, Giant, Werewolf, Werebear, Horse.
5. Polish: tome distribution, MCM/menu settings, README, zip for the mod manager.

Each phase: plain-English plan, user approves, build, test live with DevBench, show results, user approves.

## Findings that shape the design
- Reference mods swap the player's race (`SetRace`) to a custom playable copy of the creature race.
- Race "Starting Health" is a base only (wolf race 12, wolf NPCs 22-137). Measure live.
- Vanilla creatures have power attacks, bashes and a few spell attacks, not bespoke "special moves".
- Chaurus Hunter race has no Flies flag: flight is built from scratch.
- Slaughterfish race is swim-only (no Walks flag): land behaviour must be spiked early.
- Equip Enchantment Fix has no build named for 1.7.104; the "1.6.629 and newer" build must be tested.

## Status (2026-10-19, end of the first session)
Verified live on SE 1.7.104: eating a heart teaches the form; the voice-slot power transforms; race swap is
fine; the health bonus works; revert restores race, health and gear; the revert-death rule kills.
Fixed after live failures: hearts need one effect (an effect-less food crashed the inventory menu);
animal skeletons have no third-person camera node, so the mod attaches its own.
Built but NOT yet tested live (branch `wip/offline`): witness factions, indoor size rule with auto-revert,
camera height measured from the model, attack-button translation, dev command channel, test harness.

## Design notes worth keeping
- NPC reactions come from FACTIONS: the player joins the animal's own factions plus Protean_BeastFaction.
  Predator/Prey are enemies, animals are friends with their own kind, humans have no relation to animals.
  Anyone hostile who SAW the change goes into Protean_WitnessFaction (enemy of BeastFaction).
- Indoor rule: 757 vanilla locations measured: 274 clearly cave-like, 271 clearly buildings, 7 conflicting
  (orc strongholds, mines tagged as settlements), 205 neither (mostly exteriors). Cave-like keywords win.
  An interior cell with no location counts as a building (conservative).
- Corpse hearts are keyed by RACE. Ice Wolf / Pit Wolf share the Wolf race, so Phase 3 must key variants by
  base NPC to give them their own hearts.
- Vanilla "special attacks" are power attacks, bashes and a few spell attacks, not bespoke moves.


## Face stealing (requested 2026-10-19, built offline on wip/offline, untested)
Sneak beside a corpse and activate it: a small window offers Loot / Copy appearance / Cancel. Copy gives the player the corpse's face,
hair, body, sex and race and puts them in the corpse's factions (plus Protean_DisguiseFaction). The most recent face stays banked
(one slot); the new **Stolen Face** power (0x807) puts it on / takes it off. Whoever sees the theft is treated like a witness of a
vampire feeding: they go hostile and stay in Protean_ExposedFaction (enemy of the disguise faction) until the face comes off.
- Only memory is changed, never marked changed in the save: the player's own face stays in the save; the stolen one is re-applied after a load
  (co-save v4: disguised flag, bank = source NPC id, own race/sex/height/weight/tint, joined factions, exposed actors).
- Appearance method (uncertain, ~50%): set the player base record's `faceNPC` to the corpse's root face NPC, copy sex/height/weight/tint,
  and (INI `CopyHeadData=1`) share the corpse's head-part / face-data / tint-layer pointers. Pointers are handed back before any game load.
- Transforming into an animal while disguised takes the face off first; the bank is kept.
- Not done on purpose: no bounty for witnesses (they only turn hostile), no kneel animation (sneaking counts as kneeling), no vendor/recipe.

## Attack-event probe results (live, 2026-10-19)
Wolf accepts Attack1/2, ForwardPower, ForwardPowerShort, StandingPower, SkeeverLunge*; refuses AttackLeft/Right 1/2.
Deer accepts only Attack1 and Attack2. Skeever accepts everything in its list. Refused events are listed in BLOCKED in tools/build_plugin.py.
Every Phase 3 animal gets the same probe (animtest script: transform, send each event with `anim`, read "accepted by the graph").

## Design preference (user, 2026-10-19)
Each animal needs just three moves: a LMB attack, a RMB attack and a power attack. A long move list is not wanted unless the moves
are really different (so pruning refused/duplicate events is the goal; the Beast Moves window stays for the few animals with real variety).

## Beast Moves menu removed (user, 2026-10-19)
The window / HUD strip / number keys were dropped ("don't like the look of it, not necessary"). Fixed defaults instead (Attacks.cpp Pick):
LMB = first normal attack, RMB = second normal attack (or the first if only one), hold = first power attack (second on RMB).
co-save still reads the old 'moves' block so older saves load.

### Face stealing: how the look is copied (rewritten 2026-10-20, verified live)
- The player's head is always built from the player's own record, and its colours come from the player's tint masks
  (painted into the face texture by `Game.UpdateTintMaskColors`). Pointing `faceNPC` at the NPC only worked for the face
  geometry; colours stayed the player's, and sharing the NPC's head-data pointers crashed on quit and hung loads.
- Now: a full `Look` snapshot (race, sex, height, weight, body colour, hair colour, face details, skin, head parts,
  face sliders, colour layers) is taken of the face record (template resolved; race from the same record) and written
  into the player's own record with owned copies. Colour layers are painted onto the player's masks (matched by texture,
  then type and position) BEFORE the race switch / model rebuild, because the body takes its colour at rebuild time.
  Then `RegenerateHead`, `UpdateTintMaskColors`, `UpdateHairColor` via Papyrus dispatch, repeated over ~90 frames.
- NPCs with finished face files (FaceGeom + FaceTint) keep `faceNPC` set for the geometry. Their colour layers are not
  in memory (tintLayers null), so they are read from the plugin record (TINI/TINC/TINV) via TESFile.
- The player's own look is snapshotted the same way; both looks live in the co-save (v6).
- Verified: generic Orc bandit, Ulfric, Lydia (also forced through the from-data path: identical), Forsworn Shaman
  (template face): match except war paint (3 of 26 layers had no matching mask on a Redguard player). Taking off restores.
- Save/quit/load and F5/F9 while wearing a stolen face verified live 2026-10-20 (Orc bandit, war paint kept); the Esc take-off safety is now off by default ([Face] TakeOffOnMenu=0).

### Crime rules on changing back (verified live 2026-10-20)
- Unseen change into the animal and unseen change back: the bounty gained as the animal is removed (40 gold test).
- Seen change back (a person detects the player within ~57 m): the bounty stays and those people attack.
- Only people count as seeing either change (horses, dogs, wolves do not). An animal that was already hostile and saw
  the change stays hostile (Witness faction), by design.
- Open: wolf attacks are hard to land as the player (user feedback). Look at how the game aims creature attacks
  (attack angle / reach / magnetism) before Phase 3.
