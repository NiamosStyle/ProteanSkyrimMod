# Protean handoff (state at 1.0.0, 2026-10-21)

What exists, what was decided and why, and what is known and accepted. Read the section you need.

## Done

- 35 forms (table: `docs/INTEGRATION.md` section 4). Each is the vanilla race, skin and size; attacks are
  the race's own, sent to the animation graph. Health bonus = half the animal's health at the player's level.
- Controls: A/S/D steer (the mouse turns the camera at a person's rate), tap/hold mouse buttons for
  attacks/power attacks, sprint for four-legged animals (stamina), Jump = flight (Chaurus Hunters) or a
  leap (ash hopper, hare).
- Changing: player-asked changes play Become Ethereal sound + shimmer (WerewolfTrans02FXS), the Vampire
  Lord's transformation sound at 1 s, the Vampire Lord's blood burst at 1.5 s as the body switches.
  Forced changes (death, load, too big indoors) are instant.
- Changing back is refused once (cast again within 10 s, or press Z again) when it would kill the player
  (damage taken beyond their own maximum) or drop them from the sky.
- HUD: the health bar shows a black tick at the danger line, turns orange-yellow below it, stays visible
  while hurt; out of combat health tops up at 2%/s until changing back is safe.
- World: witnesses of the change know; werewolf/werebear join PlayerWerewolfFaction so guards attack on
  sight (vanilla); mammoth/giant outdoors only; troll/horker caves but not buildings.
- Face stealing (Stolen Face power).
- Integration: keywords, Protean_ActiveForm, 13 dials, mod events (`docs/INTEGRATION.md`); all verified
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
  once give one a fight; named faces will add perk points directly.
- Perk tree via Custom Skills Framework (user installs it): `data/SKSE/Plugins/CustomSkills/Protean.json` from the
  PERKS table in `tools/build_plugin.py`; points in `Protean_PerkPoints`; the Evolution power opens it. All 18 perks
  are placed; Unbound Form, Keen, Rending Claws and Tendril work (Unbound Form: Instinct, disease immunity, kill boost +15% stamina regen per kill for 90 s,
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
