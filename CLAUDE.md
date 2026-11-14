# Protean: notes for Claude

Animal shapeshifting for Skyrim SE 1.7.104 (SKSE plugin + light ESP). Version 1.0.0 is done and tagged
(`v1.0.0`). New work (for example the skill tree) builds **on** it: go through `docs/INTEGRATION.md`
(keywords, dials, mod events), not through Protean's internals. History and decisions: `docs/HANDOFF.md`
(read when needed, not up front).

## Working with the user

- Plain English; the user is not a modder. Give confidence ratings (%).
- **Plan first, wait for the user's "yes"** before each phase or sizable change.
- Keep live tests short; do what you can yourself through DevBench and the dev channel.
- Preview look-and-feel choices live (sounds, effects, timings) before building them in.
- Commit to git as you go; commit messages end with the Co-Authored-By / Claude-Session lines.
- Never hand-install third-party mods into `Data/`; the user installs mods with MO2.

## Layout

```
src/                 SKSE plugin (C++, CommonLibSSE-NG). Shift = transform/revert, Attacks, Move (steering,
                     sprint), Fly (flight, leaps), Camera, Hud, Face (face stealing), State (co-save), Dev
tools/build_creature_table.py   data/creatures.json from research/raw (game-data dumps)
tools/build_plugin.py           Protean.esp (via Spriggit YAML in esp-yaml/) + SKSE/Plugins/Protean/creatures.json;
                                per-animal tuning lives in its tables (MELEE, SPRINT, BLOCKED, GLOBALS...)
tools/build.bat [mod folder]    builds the DLL; with the folder, installs DLL + data there
tools/package.py <version>      dist/Protean-<version>.zip, dev channel off
docs/INTEGRATION.md  the contract for mods built on Protean
```

## Building and installing

- MO2 mod folder: `%LOCALAPPDATA%/ModOrganizer/Skyrim Special Edition/mods/Protean`.
  Test profile: MO2 `Protean`.
- `python -I tools/build_plugin.py`, then `cmd /c tools\build.bat "<mod folder>"`.
- **Installing fails while Skyrim runs.** If the game is running, build without the folder and install
  after the user closes it.
- `build.bat` copies the ESP and `creatures.json` only when the DLL relinks. After a data-only change,
  copy `data/Protean.esp` and `data/SKSE/Plugins/Protean/*` into the mod folder yourself.
- **New creatures are appended** to the list in `tools/build_creature_table.py`, never inserted: each
  creature's record IDs come from its position, and saves remember them.
- The co-save layout has a version (`State.cpp`, `kVersion`); add fields at the end and bump it.

## Testing live

- Skyrim tools live in the game folder `C:/Games/steamapps/common/Skyrim Special Edition/tools/`:
  DevBench (`bash tools/devbench-cli.sh alive|exec|call|notify`), Spriggit (`tools/spriggit-cli.sh`),
  xeditlib for reading game data (run node scripts from the game folder). The toolkit's own CLAUDE.md
  and KNOWLEDGEBASE(.local).md there hold the engine gotchas.
- Dev channel: with `[Dev] Commands=1` in the installed `Protean.ini`, append lines
  `<number> <command> [arg]` to the installed `SKSE/Plugins/Protean.cmd`; numbers must increase and stay
  below 2,147,483,647 (use `date +%s`, then +1 per line). Commands: see `Run()` in `src/Dev.cpp`
  (transform, revert, learn, anim, graph, dial, sound, spawn, attack, leap, fly, cc...).
- Log: `<Documents>/My Games/Skyrim Special Edition/SKSE/Protean.log` (Documents is under OneDrive on the dev machine).
- Release builds must have `Commands=0` (`tools/package.py` does it).
