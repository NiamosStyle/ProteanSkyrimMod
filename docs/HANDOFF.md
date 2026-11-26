# Protean handoff

What exists, what was decided and why, and what is known and accepted. Read the section you need.

**What the mod does today is in `docs/FACTS.md`** (the source of truth, built from the code and data). This file
holds the current state, the open items, and the history: decisions, engine findings and dated notes. Older
sections below describe the mod as it was on their date; where a rule has since changed, it is marked
**[Replaced]** or **[Removed]**.

## Current state (2026-11-18)

- **Version 1.1.0, in testing:** `dist/Protean-1.1.0-test.zip` (2026-11-15). Everything in FACTS is built: the
  Strange Mound start, consuming, evolution, 35 forms, face stealing, the full 21-perk tree (25 ranks) and the five
  weapons.
- **Not yet released.** The `skill-tree` branch is not merged into `main`; the repo
  (`github.com/NiamosStyle/Protean`) is private until the Nexus release.
- **Docs cleaned up 2026-11-18:** FACTS created; README rewritten for players; CLAUDE.md updated; old plans,
  research and the perk spec moving to `docs/archive/`.

## Open items

**Check in game** (built, never seen working; steps in FACTS section 12):
1. ~~Infected.~~ Verified 2026-11-19.
2. ~~Subtle Carapace magic resistance.~~ Verified 2026-11-19.
3. Shield blocking (all damage from the front, spells too; 5 Stamina a hit).
4. ~~Hammerfist left arm.~~ Verified 2026-11-19.
5. Stalker rank 2 (tested by script only).
5a. Dragons no longer edible (2026-11-25, the user's decision: a dead dragon is bones). Built and installed; a dead
   dragon should open its loot window as normal, with no consume prompt.

**Fix in the mod** (needs a rebuild):
6. Unbound Form's description: the Stamina-recovery boost comes from **kills**, not bodies consumed (the user's
   decision, 2026-11-18: the code is right, the text is wrong).
7. "a" before a vowel in the shapeshift power descriptions: "a ash hopper", "a elk", "a ice wolf" should use "an".
8. The spell tome (`Protean_SpellTomeInstinct`, xx803) **stays** (the user's decision, 2026-11-18): the console way in
   for a player whose load order breaks Helgen or the Strange Mound. Reading it teaches only Shapeshifter's
   Instinct; the Mound also gives Unbound Form for free, so a tome player spends their first perk point on it.
   Decided: the tome stays as it is (no free Unbound Form). Still to do: try the tome route once in game.
9. Strange Mound: appear when Helgen's ruins are enabled instead of when MQ101 is not running (a safety for untested
   alternate starts). Recommended, not done.

**Before release:**
10. Build a new package (it carries the new README; update INTEGRATION first).
11. Merge `skill-tree` into `main`.
12. Rewrite git history to clear old `.blend` versions that still hold a Windows account path (needs the user's OK
    and a force push), before the repo goes public.

**Ideas, postponed:**
13. Hammerfist "gorilla stance": hunched over, arms hanging heavily while the Hammerfists are out (a written
    animation, or the body bent at runtime).
14. The Blade's optional blood burst.


## Done at 1.0.0 (2026-10-21)

History. Several of these rules were replaced on 2026-10-22 and later: see FACTS for how the mod works now.

- 35 forms (table: `docs/INTEGRATION.md` section 4). Each is the vanilla race, skin and size; attacks are
  the race's own, sent to the animation graph. Health bonus = half the animal's health at the player's level.
  **[Replaced 2026-10-22:** 25% of the player's max + 25% of the gap; Health keeps its percentage. See FACTS.**]**
- Controls: A/S/D steer (the mouse turns the camera at a person's rate), tap/hold mouse buttons for
  attacks/power attacks, sprint for four-legged animals (stamina), Jump = flight (Chaurus Hunters) or a
  leap (ash hopper, hare).
- Changing: player-asked changes play Become Ethereal sound + shimmer (WerewolfTrans02FXS), the Vampire
  Lord's transformation sound at 1 s, the Vampire Lord's blood burst at 1.5 s as the body switches.
  Forced changes (death, load, too big indoors) are instant.
- Changing back is refused once (cast again within 10 s, or press Z again) when it would kill the player
  (damage taken beyond their own maximum) or drop them from the sky. **[Replaced 2026-10-22:** only when flying;
  changing back can no longer kill.**]**
- HUD: the health bar shows a black tick at the danger line, turns orange-yellow below it, stays visible
  while hurt; out of combat health tops up at 2%/s until changing back is safe. **[Removed 2026-10-22.]**
- World: witnesses of the change know; werewolf/werebear join PlayerWerewolfFaction so guards attack on
  sight (vanilla); mammoth/giant outdoors only; troll/horker caves but not buildings.
- Face stealing (Stolen Face power). **[Replaced:** a face is taken by consuming a person, with the Borrowed Visage
  perk. See FACTS.**]**
- Integration: keywords, Protean_ActiveForm, 13 dials (many more since: see INTEGRATION), mod events (`docs/INTEGRATION.md`); all verified
  live except watching the mod events arrive in a script.

## User decisions worth knowing

- Giant fights bare-handed (no club appearing from nowhere).
- No natural armour for were-forms (the `Protean_ArmorBonus` dial exists for a larger mod).
- Slaughterfish: no shoreline wall; it may change on land and slither into water; the game suffocates it on land.
- Spider spit = hold right mouse; costs half a power attack's stamina (it does about half the damage).
- Sprint: +33% (the horse's gallop) for four-legged animals, +40% elk/fox/hare, none for mammoth, horker,
  two-legged or many-legged forms; horse and were-forms keep their own.
- Taps go straight ahead for bears, ash hopper, mammoth (`taps` in `tools/build_plugin.py`).
- Albino spider dropped (its slot in the table stays, never enabled).

## Known and accepted

- The player's collision body stays human-sized in every form (vanilla behaviour; rebuilding it was judged
  too risky). Long animals' noses poke into walls; big ones fit through human gaps.
- A save once lacked powers given by console; since then, loading re-adds the power of every learned form.
- A crash at game exit (2026-10-21 09:43, SkyrimSE.exe+1193E50, UI code) was seen once; not reproduced.

## Engine findings (also in the game folder's KNOWLEDGEBASE.local.md)

- Flight/leaps: after `PlayerCharacter::Update`, set the character controller's gravity 0, state InAir,
  then `SetLinearVelocityImpl`; the engine keeps exactly what is set.
- The camera node's forward offset is ignored; distance comes from `fVanityModeMaxDist` (raised for big bodies).
- Taking the fighting controls (ControlMap kFighting) also stops some forms walking and the power key; the
  game's own attack is kept off the giant by hooking `AttackBlockHandler::CanProcess` instead.
- Giant graph: walk set chosen by `iState` (0 broken, 1 combat walk, 2 combat run set by the game).
- Some graph events need a state first: giant `combatIdleStart`; spider spit = `spitStanceStart` then
  `MLH_SpellRelease_event`; boar `attackStart_AttackRight1` and ash hopper lunges are refused for the player.
- A sound that follows the player's body is cut off when the race switch replaces the body: play it at
  the player's position instead.
- Perks with `Mod Attack Damage` reach animal attacks (+25% test perk: 5 -> 6.25).
- Bone turns made after `PlayerCharacter::Update` are overwritten by the frame's animation (only a wrist helper bone
  kept them, looking mangled); turn bones after `UpdateAnimation` (vfunc 0x7D).
- The fall loop has a fail-safe: when its clip ends without a landing the graph lands the player (a run showed every
  ~6 s of gliding with the 5.3 s clip). Keep a replacement fall clip longer than any fall.
- Animations written by PyNifly: keep them to ONE spline block (at most 256 frames). A 64 s clip of 8 blocks broke in
  game exactly where block 2 began (a 50 deg jump into a stiff pose) though PyNifly read it back fine. 10 fps is
  plenty for slow motion.
- A jump far higher than vanilla outlasts the jump animation: the graph drops out of its jump in mid-air (a run
  shows) unless `JumpFall` is sent at the top of the arc. A standing jump can also leave the controller in its
  OnGround state with nothing underneath: gravity off, sinking ~11 u/s (a "float"); set it back to InAir.
- Removing and re-adding an animation-graph event sink at a glide's start made the graph leave its fall for a moment;
  add a sink once per graph manager.
- Adding an item to an actor at its death (while it ragdolls) left a stiff body that could not be searched; add a
  heart as the body is searched (hook `TESNPC::Activate`, vfunc 0x37) instead.
- Outdoor wind loops are faint as a sound following the player; the dungeon "2D" loops are not.
- A control taken through ControlMap (Protean takes Activate in animal form) arrives in the input sink without its
  action name: match the key it is bound to (`ControlMap::GetMappedKey`), as for the giant's attack buttons.
- A load after a restart dropped the player's temporary modifiers on Health and AttackDamageMult (a save made as the
  wolf: +75 health and +0.02 gone; DamageResist +107 kept). Changing back then took the bonus off a second time
  (human maximum 25). The co-save now keeps those modifiers and puts Protean's part back (`Shift.cpp`
  RepairTemporary). Console `modav` changes the permanent modifier, not the temporary one.
- `AttackDamageMult` does multiply animal bites (5 -> 10 at x2). An elk spawned in an odd state (health -2363) took
  5 whatever the multiplier and misled the first test; check a test target's health first.
- `tai` (AI off) also stops magic effects ticking on the frozen actor: the poison did nothing until AI was back.
- A graph can refuse an idle event straight after a walk that it accepts standing (the deer's feeding): ask again
  for a moment.
- Most creatures' own eating sounds (goat chew, cow chew cud) were not heard when played at the player; the player's
  `ITMFoodEat` and `NPCWerewolfFeeding` were.
- Dev channel: numbers must rise across batches too; a script that restarted from `date +%s` each loop wrote lower
  numbers and every later command was skipped.

## Ideas not done

- Watching the mod events arrive (needs a listening script in the larger mod).
- The user's own MO2 install still has the dev channel on (`[Dev] Commands=1`); the release zip has it off.
- A quest NPC can still start a conversation with an animal (force greet: the woman who starts the Mistwatch quest
  talked to the deer, 2026-10-24). Closing dialogue in animal form is an idea; check how the game retries a force
  greet first.
- Eating a plant briefly shows the ingredient "added" message before Protean takes it back.

## Perk tree work (branch `skill-tree`, 2026-10-22)

Spec: `docs/protean-perk-spec.md`. Changes to Protean itself, all tested live unless noted:
- Health keeps its percentage across a change (both ways, never below 1%); bonus = `Protean_HealthBonusPct` (0.25)
  x own max + `Protean_HealthGapShare` (0.25) x how far the animal's health exceeds it. Revert warning (except
  flying), danger line and top-up healing removed. A revert sets health first, so it never passes through zero
  (the engine killed the player mid-fight otherwise).
- Gear put back after a revert no longer carries the prevent-removal lock.
- Evolution points (`src/Evolve.cpp`): a running count of feedings; perk points cost 1, 1, 2, 2, 3, 3... (totals 1, 2, 4,
  6, 9, 12...; 2026-10-25, was 1, 2, 3...); the 1st/3rd/5th/10th/15th/20th heart of a kind give points; 5 kill boosts at
  once give one a fight; named faces will add perk points directly. **[Replaced 2026-10-28:** perk points cost 1, 2,
  3...; every consumed body gives evolution points by its size; hearts and fight points are gone; 21 renowned people
  give a perk point each. See FACTS section 5.**]**
- Perk tree via Custom Skills Framework (user installs it): `data/SKSE/Plugins/CustomSkills/Protean.json` from the
  PERKS table in `tools/build_plugin.py`; points in `Protean_PerkPoints`; the Evolution power opens it. All 18 perks
  are placed **[now 21 perks, all built, no placeholders]**; Unbound Form, Keen, Rending Claws and Tendril work (Unbound Form: Instinct, disease immunity, kill boost +15% stamina regen per kill for 90 s,
  stacking); placeholders are taken back when the menu closes. Changing form costs 15 stamina.
- Rending Claws: done and tested in game (damage follows the stamina bar, 5 stamina a swing, werewolf hit
  sounds, claw animations in either hand via OAR). See `src/Claws.cpp`, `tools/build_claws.py`,
  `tools/build_claw_anims.py` (claw animations from the game archive).
- Tendril: done and tested in game. The perk teaches a spell for either hand (the user preferred the casting
  motion to an inventory item). 25 stamina and a 3 s cooldown; a miss gives both back. A hit yanks light
  targets in (negative `KnockExplosion`, force 15) or hauls the player to Large/Extra Large races. The throw
  sound (vampire lord swipe) is played by Protean at the cast: as the projectile's sound it came late. The
  extra blood art on a hit showed nothing in game (three tried) and was left as it is. See `src/Tendril.cpp`,
  `tools/build_tendril.py`; dev `tendril`, `tendrilproj`, `tendrilthrow`, `tendrilblood`, `tendrilimpact`.
- Keen: done and tested in game. Favorites (Q) works in animal form like the Vampire Lord's (tested: the game shows
  the Vampire Lord list because of the race alone), listing Revert Form and the three senses. Protean wraps the menu's
  creator and message handler, lends the animal's race and a runtime list to the `VampireRace`/`VampireSpells`
  default objects while they run, frees the Favorites key like Esc and gives the menu controls back while it is open
  (without that the menu froze). Detect Life/Dead glow on the creatures, not the player, so they are 2 s pulses
  Protean repeats each second while on (a day-long cast left glows that nothing could clear). See `src/Keen.cpp`.

- Predator's Rush and Prey's Flight: done and tested in game (2026-10-24); see the spec and `src/Rush.cpp`,
  `src/Flight.cpp`. Dev: `glidediag <frames>` (per-frame pose probe), `glidepose`, `glidearm`, `glidefx`,
  `glidesound <SNDR> [volume] [layers]`, `glidesteer`, `slamsound`, `slamshake`.
- Survivor, Predator, Subtle Carapace: done and tested in game (2026-10-24), numbers agreed as starting points for
  the user's playtest (Survivor 50/75%; Predator +2%/level to double, rank 2 poison 5 + 0.3/level a second for 5 s;
  Carapace 50 + 4/level, rank 2 100 + 7/level, animal form or no body armor). Survivor 2 feeding in `src/Feed.cpp`
  (diets in `DIET`, `tools/build_plugin.py`), Carapace in `src/Carapace.cpp`, Predator in `src/Predator.cpp`.
  Co-save v15 (carapaceApplied, eaten, savedTemp).

Live-testing notes: DevBench calls take ~1.5 s (time samples by wall clock). `devbench-cli.sh call CustomSkills
DebugReload '[]'` reloads the tree after copying the json. Moving cells (`coc`) right after a death reload crashed the
game twice (render code, Protean not in the stack).

## 2026-11-14
- Survivor 2: arrive at 100% health, whole surplus of tougher animals added. Apex (damage floor, kin, Call) built; Call's summon untested.
- Bug found and fixed (f5f6543): taking a stolen face off forgave the bounty but left the game's witness record, so witnesses (Alva, Brutius in Morthal) kept attacking. `ForgiveSince` now also calls `PlayerPayCrimeGold(false,false)` on the faction and restores the earlier bounty. Verified live by the user with a new crime.
- Logging: the previous session's log is kept as `Protean.1.log`; every load logs the Protean factions and who is hostile 3 s later.
- Apex verified by the user (kin, damage floor, Call incl. caves).
- **Perk tree constellation (user-verified):** seven smooth tentacles, three great ones for the branches, curls inward, no
  suckers. Drawn in Blender from hand-placed curves (`art/constellation/draw_tentacles.py`, render `lines.png`);
  `tools/build_constellation.py` makes the glow texture and Protean's own skydome: the vampire-lord skydome with
  `cImage00` (the picture CSF shows at `Point00`, was the werewolf) re-pathed to ours, turned 40 degrees upright and
  scaled 0.55 so the tentacles gather at the root star. `tools/fit_constellation.py` holds the hand-placed star
  positions (drawing units) and the screen mappings measured from the user's screenshots; `build_plugin.py` takes the
  node coordinates from it, and it draws `build/constellation_mock.png`. User tweaks: Apex touching the tall thin
  tentacle; siblings fanned so no line runs through another star.
- **Perk texts:** the menu shows only ~300 characters of a description: keep them under 280. "In human form" (works in
  a stolen face) replaced "your own form" on Inconspicuous, Assassin, Predator's Rush, Devastator.
- **Subtle Carapace natural armor:** the old list plus Shrouded (all), Thieves Guild, Guild Master's, Linwe's, Morag
  Tong, Forsworn/Old Gods (user: not the metal-studded leather-tempered sets such as the hold guards').
- **Stamina never slowed** from Unbound Form (user's core design: spend and refill, faster with every kill): the combat
  slowdown (`fCombatStaminaRegenRateMult`) is cancelled by a temporary StaminaRateMult modifier (co-save 28), and the
  recovery pause is held at zero. Measured in a fight: 11.7/s = full speed. Dials `Protean_StaminaNoCombatSlow`,
  `Protean_StaminaNoRegenDelay`.
- **Public identity:** the author is **NiamosStyle**; history rewritten so no real name, email or time zone remains;
  repo `github.com/NiamosStyle/Protean` (private until the Nexus release; merge `skill-tree` into `main` first).
  Never put a Windows user path or personal detail in tracked files.

## 2026-11-14/15: sleeves, Tendril sleeve, Strange Mound (all user-verified in game unless noted)
- **Tendril sleeve** (`art/tendril/`, `tools/build_tendril_sleeve.py`): flesh, cords, shoulder tentacles and a bone
  harpoon. Shown only while the Tendril is fully drawn (user: a sleeve left on when sheathed was "too incongruent");
  3rd person shows the palm harpoon. The "Rest" pieces/records (harpoon along the fingers) are unused but KEPT (user).
  1st person: the spike runs through the 1st-person MagicNode (`tools/measure_magic_node.py`); the right spike is turned
  16 deg (`THROW_*` in `make_parts_t.py`, measured live at sgtm 0.1 by `tools/measure_aim_live.py watch B`) because the
  right throw animation is not the mirrored left. Sleeve pieces have an empty 1st-person model (`Sleeve_None.nif`):
  the game draws slot 61 in 1st person.
- **Draw/sheathe sound** for every sleeve and the claws: `NPCKillStabIn` / `NPCKillStabOut` (user: "perfect");
  dev `drawsound` / `sheathesound`. **Grow**: each new skinned Protean piece grows from 15% to full over 0.6 s on a
  draw, by scaling its skin-to-bone transforms (`Blade::UpdateGrow`); dev `growtest [s]`.
- **Strange Mound** (the way into Protean): a heap of burnt flesh in Helgen's ruins at (18224.8, -77421.0, 8335.4)
  angle 0.286. "Search" it: Miraak tentacle-stab sound, Dawnguard blood burst + 10 blood splatters, the mound casts
  the Strange Infection poison at the player (real hit, so they cry out), the shapeshift shadow (WerewolfTrans02FXS),
  then `Evolve::Infect` (a free point spent on Unbound Form); it shrivels and is deleted. Placed at runtime by
  `src/Mound.cpp` (no cell edit) when a player without Protean comes within 8000 units and MQ101 is not running.
  Model `tools/build_mound.py` (box collision; needs BSX 130), art `art/mound/`. Co-save 29 (ref, used).
  Dev `moundhere [spot]`, `moundreset`. Verified with a new character (Realm of Lorkhan start).
  **Open:** gate on Helgen's ruins being enabled instead of MQ101 (safety for untested alternate starts) - recommended,
  not done.
- **Assassin fix**: Very Aggressive (2) no longer counts as "attacks on sight" for a friend/ally (a stolen face's
  people): bandits passing a bandit-faced player now take x10 (verified).
- **Textures**: Fist/Shell shrunk to 2048; release zip ~71 MB. `tools/package.py` now packs every Protean mesh/texture.
- **Privacy**: `.blend` files are zstd-compressed and hid `C:\Users\<account>` paths; `tools/blend_privacy.py`
  cleans/scans them. HEAD versions clean; OLD .blend versions in git history still hold it - rewrite history before
  the repo goes public (needs the user's OK + force push).
- Perk pace: the user is at Protean 26 / character 16 with the tree full; left as is for now (may tune animal
  evolution points later).
- **Equip Enchantment Fix dropped from the requirements** (2026-11-15, the user's decision): tested with it off -
  six enchanted worn items (Shrouded set, Amulet of Arkay, Silver-Blood ring) all kept every effect through a
  transform + change back, and through a save made in animal form loaded from the main menu, then changed back.
  The README now names it as the fix to install **if** anyone sees an enchantment missing after changing back
  (Protean re-equips by code in `Shift::RestoreGear`, the path where the engine is known to drop enchantments).
  If such a report ever comes in, point them to it first.

## 2026-11-19: face drift and the true face

- Mercer's war paint/eyeliner/dirt had drifted. Save scan (ReSaver `changeform 0x14`, tint table at the end of the
  player ACHR's unparsed tail, see KNOWLEDGEBASE.local.md): head parts and sliders never drifted; colour layers drifted
  at Save33 (10-27) and Save58 (11-15), both after saves made in a stolen face. Cause in code: TakeOff restored own
  colour layers with a type+position fallback; after loading a stolen-face save the list had 33 masks (one mislabelled)
  so layers landed on the wrong slots, and the next put-on copied the wrong face as "own".
- Fixes: exact-texture restore (ef3613f, 6f1f7b8: mask lists via GetPlayerRuntimeData - a hard-coded 1.6 offset
  crashed on 1.7.104); the true face (d5818dd, co-save v30), user's design. Mercer repaired from a Save1 dump and his
  true face set from Save1 (user's choice).
- ROOT CAUSE (found by the round-trip test): the game builds the player's colour-mask list only at chargen and on
  load (from race-asset numbers); a race/sex change keeps the old list. A save made in a female stolen face loads
  with the female list, and taking it off left a male player on it. Fix f619d29: RebuildMasks() makes the list the
  look's race+sex asset list before painting (main thread only). Round trip VERIFIED by the user 2026-11-19 (female
  Redguard face: 33 -> 31 -> 34 masks, all 34 own layers painted, truecheck unchanged). Female player wearing a male
  face also VERIFIED (Meiya, Wood Elf: 34 -> 30 masks, all 30 painted; her true face matches her character-creation
  save exactly; truecheck unchanged).

## 2026-11-19: Apex kin aura, Shield arrows

- Forsworn ignored Mercer: Apex had put the player in every animal's faction, and those factions are friends of
  people and monsters (Goat: Forsworn; Skeever and Spider: draugr, Falmer, skeletons, vampires; Chaurus: Falmer,
  hagravens; Troll: hagravens; Wolf: vampires). Found with dev `whyfriendly <ref>`. User chose an invisible calm aura
  (c802a54, 0e30d58): spell xxA3A / effect xxA39, globals xxA3B range, xxA3C forget seconds; old faction kin taken off
  once (co-save 31). First build killed every animal it touched: the Calm's actor value was None (-1); it must be
  Aggression like every vanilla Calm. The game reports the calm as a hit by the player, so the aura's own hit tracker
  skips its spell. A safety switch turns the aura off if an animal dies within 2 s of a calm. Verified: animals live
  and ignore the player; Forsworn hostile again.
- Shield: arrows never reached the melee-only patch, and the game never counts an arrow blocked (vanilla blocks arrows
  by the shield model's collision; ours has none). Patch now on all 8 calls of the apply-hit function (+6CA640) and a
  Protean rule blocks bows/crossbows within Protean_ShieldWardAngle. Each arrow passes twice (+6CD798, then +66A35D,
  same HitData); b9d5102 stops the second charge (recheck: 5 Stamina per arrow).
