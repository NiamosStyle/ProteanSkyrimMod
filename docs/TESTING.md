# Testing Protean

What the mod should do is in `docs/FACTS.md`; test against that.

## One-time setup (already done on the dev PC)

- MO2 profile **Protean**, with the mod **Protean** enabled (`mods/Protean`, installed by
  `tools/build.bat "<mod folder>"`).
- `Data/SKSE/Plugins/Protean.cmd` must exist **before** the game starts (MO2's virtual file system doesn't show
  files created while the game runs). The install copies an empty one.
- `Protean.ini` `[Dev] Commands=1` turns on the dev channel. Release packages have it at 0 (`tools/package.py`
  does that).

## The dev channel

Append lines `<increasing number> <command> [arg]` to the installed `Protean.cmd`; numbers must rise across
batches too. Results go to `<Documents>/My Games/Skyrim Special Edition/SKSE/Protean.log` (the previous session's
log is kept as `Protean.1.log`). The full command list is `Run()` in `src/Dev.cpp`. The everyday ones:

- `starter`, `learn <id>`, `unlearn <id>`, `transform <id>`, `revert`, `dump`
- `evo [n]` (add n evolution points; no number prints the state), `buyperk <id>`, `boost` (a biomass boost, as if
  from a kill), `power <hex id>` (cast a power the way the game does; console casts don't reach Protean)
- `spawn <hex base FormID>`, `consume` (the nearest body), `kill` (**kills the player**)
- `moundhere [spot]`, `moundreset` (the Strange Mound)

## Automated scripts (game running, a save loaded, dev channel on)

```
python -I tools/devtest.py reactions        # spawn animals; how they treat you before/after transforming
python -I tools/devtest.py attacks          # does the wolf's graph accept its own attack events?
python -I tools/devtest.py face             # face stealing through the dev channel
python -I tools/perktest.py list            # the perk scenarios
python -I tools/perktest.py <name> | all    # assassin, stealth_consume, stalker2, stalker2_seen
```

`perktest.py` moves the player to the empty test cell and changes the character: **never save after running it.**

**Out of date, fix before relying on them:** `devtest.py core` still sends `hearts` (that command was removed when
hearts were, 2026-10-28), and `devtest.py death` tests the old rule that changing back could kill (removed
2026-10-22: Health now keeps its percentage). Check them with the user before rewriting.

## In-game checklist (built, never seen working)

From FACTS section 12 and HANDOFF's open items; tick them off in both.

1. **Shield blocking:** block hits, arrows and a spell from the front. All damage stopped, 5 Stamina a hit?
2. **Stalker rank 2:** in a fight, break line of sight and keep still. Do you fade, and do enemies give up after
   about 3 seconds?
3. **The spell tome:** on a fresh character, add the tome with the console and read it. Can you consume, learn a form
   and shapeshift?

## Things only a person can judge

Camera height and smoothness; how sounds, effects and animations look and feel; models (nothing floating, gear back
on after changing back); anything about how a fight *feels*. Ask the user to look, and keep it short.
