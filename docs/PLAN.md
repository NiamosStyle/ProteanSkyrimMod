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

## Still open (ask before building)
- Approve the item names and size classes in `docs/creature-table.md`. Giant outdoors-only is an assumption.
- Behaviour when a big creature walks through a door into a place it is not allowed (proposal: auto-revert
  with a message).
- Small bonus for eating an item of an already-known form.

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
