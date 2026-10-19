# Phase 2 plan (APPROVED 2026-10-19: Option A + B): attacks and the transformed-only menu

**Status:** built on branch `wip/offline` (Moves.cpp, Attacks.cpp), compiles, NOT yet tested in game.

Written 2026-10-19 while the user was away. Nothing here is built except the attack-button translation
prototype (branch `wip/offline`, untested). Plain English first, technical notes after.

## What the problem really is

1. **The attack button.** An animal's animation graph does not understand the player's generic "attack"
   signal. The wolf graph only accepts its own signals (`attackStart_Attack1`, `attackStart_Attack2`,
   `attackStart_ForwardPower`, `attackStart_StandingPower`). So without help, pressing attack as a wolf
   would do nothing. The mod must translate the button into the animal's own attacks.
   *Prototype built:* tap = a normal attack from the animal's list, hold = a power attack. **Untested.**
2. **"Special attacks".** Vanilla animals do not have bespoke special moves. What exists (from the game
   data, see `docs/creature-table.md`) is: power attacks, bashes, and a few attacks that cast a spell
   (for example the Chaurus Hunter's spit). So the "special attack menu" is a list of each animal's power
   attacks, bashes and spell attacks.
3. **The menu.** The normal Favorites menu is closed to animal forms (the mod locks menus, like the
   werewolf). A separate menu is needed.

## Options for the menu (recommendation first)

| Option | What it is | Needs | Verdict |
|---|---|---|---|
| **A. A small "Beast Moves" window drawn by SKSE Menu Framework (recommended)** | A panel listing the animal's moves. Click one, then choose Left or Right mouse button. A tiny always-on HUD strip shows what is on each button. | SKSE Menu Framework, which you already have | Cheapest, no new dependency, can be built and tested quickly. Looks like an overlay, not like the vanilla menu. |
| B. Hotkeys 1-8, like favorite hotkeys | Number keys pick the move for the next attack. | Nothing | Very simple, fits Skyrim habits, but no browsing and nothing on screen. Could be added on top of A. |
| C. Reuse the real Favorites menu | Fill the vanilla favorites list with the moves | A Scaleform/SkyUI hook | Most "native" but fragile with SkyUI versions and risky. Not recommended. |

Recommendation: **A, with B as a bonus.**

## How a move gets used (all options)

- Each move is one of the animal's own animation-graph events, taken from its race record (already in the
  runtime table as `attacks`). Left/right mouse tap fires the move assigned to that button, falling back to
  a normal attack when nothing is assigned.
- Spell attacks (for example spit) need a casting step on top of the animation; that is checked per animal
  in Phase 3 rather than assumed.
- The attack list for each animal is data, so adding an animal never needs new code.

## What I would build in Phase 2

1. Finish and verify the attack-button translation on the three test animals (Wolf, Deer, Skeever).
2. The Beast Moves window and HUD strip (Option A), plus number-key quick select (Option B).
3. Save the player's assignments per animal in the co-save.
4. Tests through the dev command channel where possible (the harness already has an `attacks` scenario).

## Risks / unknowns (be honest)

- It is not yet known whether the engine will even run the animal's attack animation for the player. The
  first live test is a one-line probe (`anim attackStart_Attack1`), which the harness already does.
- Hit detection for animal attacks uses the race's unarmed damage and the attack data's damage multiplier.
  That should just work, but damage values need a sanity check live.
- Deer, Elk, Chicken and Hare have few or no real attacks (Chicken and Hare have none at all). For those
  the answer may be "no attacks, just run", which matches how the animals behave.

## Confidence

About 65% that the translation works as designed on the first live test; about 90% that SKSE Menu Framework
can host the window (the API supports windows and HUD elements, and Portal Spheres already uses it).
