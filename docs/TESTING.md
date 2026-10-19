# Testing Protean

Everything below is in the `wip/offline` branch until it has been run live.

## One-time setup (already done on this PC)
- MO2 profile **Protean**, mod **Protean** enabled (`mods/Protean`, deployed by `tools/build.bat "<mod folder>"`).
- `Data/SKSE/Plugins/Protean.cmd` must exist **before** the game starts (MO2's virtual file system does not
  show files created while the game runs). The deploy copies an empty one.
- `Protean.ini` `[Dev] Commands=1` enables the test command channel. Set it to 0 for any release.

## Automated live tests (game running, save loaded)
```
cd C:/Modding/Protean
python -I tools/devtest.py core        # learn, transform, health/race/camera checks, revert, restore
python -I tools/devtest.py reactions   # spawn a wolf, scan how it treats you before/after transforming
python -I tools/devtest.py attacks     # does the wolf graph accept its own attack events?
python -I tools/devtest.py death       # KILLS the player: checks the revert-death rule and death camera
```
The harness writes commands into `Protean.cmd`, reads `Documents/My Games/Skyrim Special Edition/SKSE/Protean.log`
and cross-checks the plugin against DevBench (race, health, camera).

## Dev commands (Protean.cmd lines: `<increasing number> <command> [arg]`)
`starter` `hearts` `learn <id>` `unlearn <id>` `transform <id>` `revert` `dump` `scan` `spawn <hex base FormID>`
`anim <graph event>` `draw` `kill`

## Things only a human can judge (ask the user to look)
- Camera: height, distance, centred, smooth, first-person start vs third-person start.
- Death: sound, animation, death camera after the revert-kill.
- Looks: wolf model with no armor floating on it; gear back on after reverting.
- Real eating through the inventory menu (verified once), real shout-key revert (verified once).

## Known open questions to settle live
1. Does an Enemy relation (Witness vs Beast faction) really override the Friend relation of a shared
   CreatureFaction? (`reactions` scenario shows hostile flags before/after.)
2. Does the animal's graph accept `attackStart_*` events for the player, and does damage apply?
3. Crime: does attacking a guard/livestock as an animal create a bounty? (Reference mods remove bounties; we
   have not touched crime yet.)
4. Save and load while transformed (state restored from the co-save; camera/controls re-applied).
5. Corpse hearts: open a dead wolf as a shapeshifter and see whether the heart appears.

## Manual checks for Phase 2 (need a person)
- Transform (console: `player.addspell FE004811` after learning, or the dev channel), then press **B**: the Beast Moves
  window should open with the animal's moves; click LMB/RMB beside a move; the strip at the bottom should update.
- Keys **1-8** set the left mouse button, **Shift+1-8** the right one; tapping the button then fires that move.
- Press **E** as an animal: nothing opens, a short message appears (at most every 3 seconds).
- Assignments should survive save and load (they live in the co-save).
- Clicking inside the window must NOT make the animal attack.

## Face stealing (built offline, nothing tested live)
- Automated (no corpse needed): `python -I tools/devtest.py face` — steals Ulfric's face, takes it off, re-wears the bank with the
  Stolen Face power, then transforms into a wolf (which must end the disguise but keep the bank).
- Dev commands: `steal <hex NPC base id>` (witnesses count), `stealq <id>` (nobody sees it), `facepower`, `takeoff`, `facedump`.
- Give yourself the starter first (`starter`). The Stolen Face power is `FE004807` (`player.addspell FE004807`, also given... see PLAN.md).
- By hand: sneak next to a dead NPC and press E. The loot window should be replaced by a small window with
  **Loot / Copy appearance / Cancel**. Copy: look at yourself in third person — face, hair, body, sex, race should match the corpse.
  Walk up to a guard/townsperson while wearing it: they should treat you as one of their own faction.
  Have an NPC watch you do it: they should turn hostile ("You were seen!").
- Open questions to settle live: (1) does the player's face actually change (`faceNPC` and/or the shared head-part pointers,
  `[Face] CopyHeadData=` in Protean.ini)? (2) does an Exposed relation beat a shared faction? (3) save while disguised, quit, load.
