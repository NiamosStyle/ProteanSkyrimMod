# Protean (working title)

Animal shapeshifting for Skyrim SE **1.7.104**. Eat a piece of a slain animal to learn its form, then
transform into it: real model, animations, attacks, size, health bonus, and the world treats you as that
animal unless it watched you change.

Status: **Phase 0 (planning and data)**. Nothing is playable yet. See `docs/PLAN.md`.

## Credits

This mod is a new build on the ideas of two existing mods. No files or scripts from them are reused.

- **Shapeshifting** by PeenCoach (Nexus SE 107629): the transform-by-race-swap approach, the
  werewolf-style "no inventory or spells, press again to revert" design.
- **Shapeshift - Hare** by Bobiguy (Nexus SE 142327): unequip-everything handling across armor slots,
  Equip Enchantment Fix use, the craftable spell tome.

## Requirements (planned)

Skyrim SE 1.7.104, SKSE64, Address Library, SkyUI, SKSE Menu Framework, Equip Enchantment Fix,
monster race crash fix. DevBench is for development only.

## Layout

```
docs/PLAN.md             decisions, phases, open questions
docs/creature-table.md   generated creature table (facts from game data + proposals)
data/creatures.json      the single table that drives every creature
tools/                   read-only extraction + table builder (regenerate with the two commands below)
research/raw/            raw dumps from the vanilla masters (not committed)
src/                     SKSE plugin (empty until Phase 1)
```

Regenerate the table (from the game folder so `xeditlib` resolves):

```
cd "C:/Games/steamapps/common/Skyrim Special Edition" && node C:/Modding/Protean/tools/dump_game_data.js
python -I C:/Modding/Protean/tools/build_creature_table.py
```
