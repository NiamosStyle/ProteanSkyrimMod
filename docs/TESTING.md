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
