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
