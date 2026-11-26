# Protean: notes for Claude

A Prototype-inspired shapeshifter mod for Skyrim SE 1.7.104 (SKSE plugin + light ESP): consuming, 35 animal
forms, face stealing, a 21-perk tree and five flesh weapons. Version 1.0.0 is tagged (`v1.0.0`); **1.1.0** (the
perk tree, weapons and Strange Mound) is in testing (`dist/Protean-1.1.0-test.zip`).

## Read first

- **`docs/FACTS.md` is the source of truth** for what the mod actually does. Read it before working on any
  feature. It was built from the code, plugin and data files, not from older documents.
- **When the mod changes, update FACTS first**, then whatever else the change touches (see "When the mod
  changes" below). Every other document takes its facts from FACTS.
- `docs/HANDOFF.md`: the current state and open items on top, then decisions, engine findings and dated notes.
  Read the part you need, not the whole file.
- `docs/archive/` holds old plans, research and the original perk spec. **History only:** it may contradict the
  mod. Don't take facts from it.

## Working with the user

- Plain English; the user is not a programmer or modder. Explain technical terms or avoid them. Give confidence
  ratings (%).
- **Plan first, wait for the user's "yes"** before each phase or sizable change.
- The user wants to save Claude Code usage: keep sessions short and focused on the task asked.
- Keep live tests short; do what you can yourself through DevBench and the dev channel.
- Preview look-and-feel choices live (sounds, effects, timings) before building them in.
- Commit to git as you go; commit messages end with the Co-Authored-By / Claude-Session lines.
- Never hand-install third-party mods into `Data/`; the user installs mods with MO2.

## Map the connections before building (user's rule, 2026-11-19)

Protean leans on the game's own records and engine behaviour, and those carry connections that aren't visible from
where we touch them. Four bugs in one day came from that, each found only in the user's game, each a few minutes of
checking up front:

- **Faction kin:** joining the Goat, Skeever and Spider factions also made Forsworn, draugr, Falmer and vampires
  friendly, because those factions list them as friends.
- **Kin aura:** a Calm effect built without vanilla's actor value (Aggression) killed every animal it touched.
- **Face drift:** "changing race or sex rebuilds the colour slots" was assumed, never measured. It doesn't; a save
  made in a female face left a male player on the female list.
- **Shield arrows:** "one arrow, one hit" was assumed. Each arrow goes through the hit code twice.

So, **before writing a feature, and again before building on anything new mid-feature:**

1. **List what it touches in the game**, not in our code: every vanilla record it uses, copies, joins or imitates
   (faction, spell, effect, race, perk, package), every engine state it changes (race, sex, factions, actor values,
   face, equipment, AI) and every engine call or event it hooks or listens to.
2. **For each, find what else is connected:**
   - a record: read it whole (xelib from the game folder, or Spriggit), with everything it links to: a faction's
     relations, a spell's effects and conditions, a race's lists. Whatever we borrow, we get all of.
   - something we imitate: compare our record with the vanilla one **field by field** (name the vanilla model in a
     comment by the record in `tools/build_plugin.py`). Any field we leave out or set differently needs a reason.
   - an engine state: when does the game itself rebuild, save, reload or reset it (load, race/sex change, death,
     cell change, menus), and what else reads it? Assume nothing: measure it.
   - a hook or event: how many times it fires per action, on which threads, from which paths (melee, arrow,
     spell, script), and whether our own actions fire it too (our spells raise hit events).
   - our own code: which other Protean modules react to the same event or state (`grep` the sink type or the call).
3. **Write the assumptions down with how each was checked:** vanilla record, DevBench measurement, knowledgebase
   entry, or "unverified". Put the list in the plan shown to the user, and engine findings in HANDOFF.
4. **Unverified means measure first:** a short spike with DevBench, the dev channel or an offline read,
   before building on it. Most take minutes. When something can't be measured offline, say so and fold the check
   into the first live test.
5. **Test the transitions, not just the feature:** save and load in every state the feature creates, the other sex
   and another race, animal form and back. That is where state the engine rebuilds behind our back shows up.

## When the mod changes

1. `docs/FACTS.md`: the facts, numbers and in-game text.
2. `README.md`: if players would notice the change (it ships in the download).
3. `docs/INTEGRATION.md`: if you added or changed a setting (`Protean_` global), record, keyword or mod event.
   It ships in the download as `docs/Protean-INTEGRATION.md`.
4. `docs/HANDOFF.md`: the open items, and a dated note for decisions or engine findings.

## Layout

```
README.md            for players; ships in the download
docs/FACTS.md        what the mod actually is (source of truth)
docs/HANDOFF.md      current state, open items, decisions, engine findings
docs/INTEGRATION.md  for mods built on Protean: markers, events, every setting and record ID
docs/TESTING.md      setup, test scripts, in-game checklist
docs/archive/        history (may be out of date)
src/                 the SKSE plugin (C++, CommonLibSSE-NG):
                       core: main, Hooks, Registry (settings and the creature table), State (co-save),
                             Dev (dev channel), Hud, Journal (the Evolution page)
                       shapeshifting: Shift (transform/revert), Attacks, Move (steering, sprint),
                             Fly (flight, leaps), Camera
                       consuming and evolving: Consume, Feed, Evolve, Mound (the Strange Mound), Skills
                       faces: Face, Deceiver, Metamorphosis
                       perks and weapons: one file each, named after it (Keen, Predator, Stalker, Apex, Call,
                             Claws, Hammerfist, Blade, Shield, Tendril, Rush, Flight, Carapace, Devastator...)
data/                the mod's files as installed (Protean.esp, SKSE/Plugins/..., meshes, textures)
data/creatures.json  the table that drives every creature (generated)
esp-yaml/            Spriggit source of Protean.esp (generated by tools/build_plugin.py)
tools/               extraction, table, plugin, mesh and animation builders, packager, test scripts
```

## Building and installing

```
python -I tools/build_creature_table.py   # data/creatures.json from research/raw (game-data dumps)
python -I tools/build_plugin.py           # Protean.esp + SKSE/Plugins/Protean/creatures.json
tools\build.bat [MO2 mod folder]          # the DLL; with a folder, also installs everything there
python -I tools/package.py 1.1.0          # dist/Protean-1.1.0.zip, dev channel off
```

- `tools/build_plugin.py`: per-animal tuning and every `Protean_` setting live in its tables (MELEE, SPRINT,
  BLOCKED, GLOBALS...).
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
- The package takes `README.md` from the project's main folder and `docs/INTEGRATION.md` (shipped as
  `docs/Protean-INTEGRATION.md`).

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
