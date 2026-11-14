# Protean Perk Tree: Spec

This document describes every perk in the Protean perk tree, what each one does, what it depends on, and how sure we are that we can build it. It is meant for review before any code is written. Nothing in your Skyrim project has been changed.

## How to read this

Each perk has a **status** that tells you how much work it will take:

- **Ready**: Skyrim or an existing mod already does most of the work. We mainly need to connect it.
- **Needs research**: The idea is clear, but we have to find the right game settings, animations, or existing mods first.
- **Needs invention**: Nothing like this exists yet, so we have to design and test it ourselves.

Perks marked **(stamina)** cost Stamina to use. This matches the star marks on your sketch.

Perks are described in plain English. Where a perk names an outside mod, that mod is listed under "Outside dependencies."

---

## What Protean 1.0.0 already does

Checked against the Protean folder (`C:\Modding\Protean`) on 2026-10-22. The perk tree should be built **on top of** Protean, using the controls Protean was designed to expose (its "integration" settings), not by rewriting Protean's insides. Protean's own notes ask for exactly this.

- **Becoming a shapeshifter:** a starting power called Instinct, taught by a spell tome that is currently console-only. Hearts only appear in corpses once the player has this power. So Unbound Form can simply grant it.
- **Hearts:** each of the 35 animals drops its own heart, but only until the player knows that form. Eating a known heart again does nothing. Repeat-feeding milestones (source B) need hearts to keep dropping, and a counter per animal.
- **Health in animal form:** already half the animal's health at the player's level. This is a setting the perk tree can change at any time.
- **Ready-made settings the perk tree can turn:** health bonus, out-of-combat healing, sprint speed and its Stamina drain, leap distance and height, flight speed, melee reach, strike angle, damage dealt while transformed, and armor while transformed.
- **Damage perks already work on animal attacks.** A normal "more attack damage" perk was measured raising an elk's kick from 5 to 6.25.
- **Changing form costs no Stamina** today.
- **As an animal, the player can't activate anything** (no doors, containers, or bodies). Eating a corpse in animal form will need an exception.
- **Menus are locked in animal form.** An earlier plan judged reusing the real Favorites menu fragile and risky.
- **Flight and leaps exist** for the Chaurus Hunter, ash hopper, and hare, built in Protean's own code with no outside mod. This is a strong head start for double jump and gliding.
- **Face stealing:** sneak to a body and activate it, then choose to copy its appearance. You copy its face, body, sex, and race, and you join its factions. One face is kept at a time, and the Stolen Face power puts it on or takes it off. Anyone who sees the theft turns hostile. It needs the same Instinct power as shapeshifting.
- **Making new animations isn't possible with the current tools.** Protean's flight research says so directly. Gliding, Tendril, and Devastator will have to reuse existing animations unless a new tool is added.

---

## Changes to Protean itself (approved in principle, not yet built)

These change how Protean handles health. They are small, and both touch the same code, so they'd be done in one session (about 80 to 85% confidence).

**1. Health keeps its percentage when changing form, both ways.**
If you're at 10% of your health when you change, you arrive at 10% of the new form's maximum. This works when changing into an animal and when changing back.
- Example: a player with 200 health becomes a form with 300 max health at 10% (20 health) and arrives at 30. Taking damage down to 10% and changing back leaves them at 20.
- Changing back can no longer kill you, so Protean's warning, the refusal to change back, the "danger line" on the health bar, and the out-of-combat top-up healing are no longer needed.
- Changing into an animal is no longer an instant heal.

**2. One health-bonus rule for every animal (decided).**
- **Bonus = 25% of your normal max health, plus a quarter of the gap** between the animal's health and yours. The gap part only counts when the animal has more health than you.
- Survivor raises the 25% to 50% at rank 1 and 75% at rank 2. The quarter of the gap stays the same.
- Both numbers (the percentage and the gap share) become settings, so the perk tree can change them.
- Why one rule: an earlier idea with separate rules for weaker and stronger animals made a form's bonus suddenly halve when the player's health passed the animal's. Six popular forms (Chaurus, Bear, Boar, Snowy Sabre Cat, Troll, Horse) sit at 253 to 289 health, right where mid-game players are, so leveling up would have weakened them.

Examples for a player with 200 health:

| Form | Default | Survivor 1 | Survivor 2 |
|---|---|---|---|
| Elk (50) | +50 | +100 | +150 |
| Bear (260) | +65 | +115 | +165 |
| Mammoth (931) | +233 | +283 | +333 |

---

## The three branches

The tree starts at **Unbound Form** and splits into three branches:

- **Beast**: animal-form perks, ending in the capstone **Apex** (no longer a dragon; to be redesigned).
- **Hunter**: combat, movement, and stamina perks, ending in the capstone **Devastator**.
- **Deceiver**: face-stealing and stealth perks, ending in **Infected** and **Assassin**.

---

## Progression

Perks are not unlocked with normal Skyrim perk points. Instead, the player earns **evolution points** by feeding, the way vampires and werewolves grow stronger. The name is a placeholder. Candidates include "Biomass" (a term from Prototype), "Vitae," or a name tied to Protean lore.

**Consuming (the user's redesign, 2026-10-28: "not Prototype enough"; built; tested in game by the user (reported 2026-11-02)):** in any body the player can consume a corpse: in their own form by sneaking and activating it, in animal form by activating it (animals cannot sneak). Red tentacles (the Devastator's) cover it for 2 s, then it disappears. A switch on Protean's F1 page (SKSE Menu Framework), off at first, decides whether its loot moves to the player's inventory or is lost; quest items always move. A warning box appears the first time. Every body heals 20% of maximum health (50% with Survivor rank 2) and restores a flat 50 Stamina at once, like a potion (the user, 2026-11-03; until then it gave a biomass boost stack, which kills give again). In combat consuming takes 1 s instead of 2 (same day). Built, untested. The first animal of a kind teaches its form (hearts are gone). A person gives their face with Borrowed Visage (worn at once in the own form; in animal form kept for the Stolen Face power) and is a crime if seen (1000 bounty with each witness's crime faction). Draugr can be consumed for the heal and the biomass boost only: no evolution points, not counted on the page (the user, 2026-11-03; built, tested in game the same day: boost given, heal applied, no points). Children, the other undead (vampires excepted), Dwarven machines and ghosts cannot be consumed, nor a body a quest marks as a quest object (any quest alias is logged).

**Evolution points (2026-10-28):** earned solely by consuming biomass: small creatures 1, people and middling animals 2, large creatures 3, giants and mammoths 5; creatures with no form by their height (the user kept the size weighting). The page lists the bodies consumed per kind. The earlier sources below (hearts, combat stacks) are gone; points already earned are kept.

**Progression like Growl and Sacrosanct (the user's picks, 2026-10-28):** each perk point costs one more evolution point than the last, as Growl's feedings do (1, 2, 3, ...; the whole tree 253, about 125 bodies). The perk screen shows a Protean level (perk points earned) and a bar to the next, like the werewolf tree, with the game's "Protean increased to N" message (Custom Skills Framework's level, ratio and showLevelup). Consuming one of the renowned people in the list below gives a perk point at once, once each (Sacrosanct: "feed on individuals of great strength and renown"); they are matched by name, so every record of the person counts. Hunger stages and rank titles were offered and not chosen.
Status (2026-11-05): tested in game by the user. Consuming people and spiders gave +2 each (log); at 190 points the perk points earned rose from 18 to 19, and the user saw the "Protean increased to 19" message.

Until 2026-10-28, evolution points came from four sources:

- **A) First-time feeding:** eating the heart of a creature type the player has never eaten before.
- **B) Repeat feeding milestones:** eating a growing number of hearts of the same type. The milestones are the 3rd, 5th, 10th, 15th and 20th heart (with the 1st, 6 points a kind; 2026-10-25, was 5, 10, 20). The game has to track the count for each creature type.
- **C) Combat stacks:** having 5 or more Unbound Form kill stacks active during one combat. This may be too easy, and we'll test it.
- **D) Face-taking:** killing and taking the face of an important named character from the list below. **This grants a full perk point right away, not just an evolution point.** It skips the rising cost entirely, which makes these kills a big reward.

**Perk cost:** Perk points cost 1, 1, 2, 2, 3, 3, ... evolution points (changed 2026-10-25 after the user's playtest: progress was far too slow, and the old costs of 1, 2, 3, ... totalled more than every heart in the game could give).

**Ranks cost points:** Each rank is bought separately, just like vanilla perks with scaled abilities. Survivor 1 costs one point and Survivor 2 costs another. Survivor, Predator, Stalker, and Subtle Carapace each have two ranks.

**Totals for this tree:** The tree has 18 perks and 22 purchasable ranks. Buying every rank costs 2 x (1 + 2 + ... + 11) = **132 evolution points** (hearts alone can give 35 kinds x 6 = 210).

**Vanilla skills (2026-10-25, the user's design):** animal bites and claw hits count as one-handed swords for One-Handed experience and perks (claws carry the sword keyword; bites get Armsman, Savage Strike and Fighting Stance through a hidden perk). Hits taken as an animal, or with Subtle Carapace on, count as Light Armor for experience, Agile Defender (multiplies the carapace) and Deft Movement; fur, hide, leather, chitin and dragonscale light body armor no longer turns the carapace off. Moving faded with Stalk gives Sneak experience; Stealth and Silence quiet it; Light Foot already works in any form. No magic skills.

### Named faces that grant a perk point

Stealing the face of any of these characters grants one automatic perk point. Not every character is reachable in every playthrough, which is intended: the player's choices decide which rewards they can collect.

**Base game**

| Character | Quest | Notes |
|---|---|---|
| Ulfric Stormcloak | Battle for Windhelm (Civil War, Imperial side) | Only Ulfric or Tullius in a playthrough |
| Galmar Stone-Fist | Battle for Windhelm | Dies alongside Ulfric |
| General Tullius | Battle for Solitude (Civil War, Stormcloak side) | Only Ulfric or Tullius in a playthrough |
| Legate Rikke | Battle for Solitude | Dies alongside Tullius |
| Mercer Frey | Blindsighted (Thieves Guild) | Body ends up in water but can be reached |
| Emperor Titus Mede II | Hail Sithis! (Dark Brotherhood) | |
| Astrid | Death Incarnate (Dark Brotherhood) | |
| Krev the Skinner | Purity of Revenge (Companions) | |
| Ancano | The Eye of Magnus (College of Winterhold) | |
| Madanach | No One Escapes Cidhna Mine | Only if the player chooses to kill him |
| Thonar Silver-Blood | The Forsworn Conspiracy / Cidhna Mine | Depends on choices |
| Calixto Corrium | Blood on the Ice | |
| Movarth Piquine | Laid to Rest | |
| Sinding | Ill Met by Moonlight (Hircine) | Only if the player sides with the hunters |
| Silus Vesuius | Pieces of the Past (Mehrunes Dagon) | Optional kill |
| Vigilant Tyranus | The House of Horrors (Molag Bal) | Optional kill |

**Dawnguard**

| Character | Quest | Notes |
|---|---|---|
| Harkon | Kindred Judgment | Check in game that he leaves a usable body |
| Arch-Curate Vyrthur | Touching the Sky | |

**Dragonborn**

| Character | Quest | Notes |
|---|---|---|
| The Ebony Warrior | Last Vigil | |
| Ralis Sedarys | End of the Kolbjorn Barrow quests | |
| Ildari Sarothril | The Final Descent (Neloth) | Details need checking in game |

**Not usable:** Alduin, Paarthurnax, and other dragons; Miraak (no body left); Karstaag (turns to ice); dragon priests (turn to ash); ghosts such as Haknir Death-Brand. None of them leave a human body to take a face from.

**Kill stacks:** Each Unbound Form kill stack lasts 60 to 120 seconds before it expires. The exact time will be tuned during testing. (2026-10-28 to 2026-11-03 a stack came from each body consumed; since 2026-11-03 from each kill again, and a body restores 50 Stamina instead.)

---

## Root

### Unbound Form
- **Requires:** nothing (starting perk)
- **What it does:** Unlocks shapeshifting, makes you immune to disease, and gives a short stamina-recovery boost after every kill. The boosts stack. Each boost lasts 60 to 120 seconds before it expires.
- **Heart items:** Taking this perk is the signal for the game to start placing a "[animal name] heart" item into each animal's loot when its corpse is searched. Consuming a heart teaches the player the "Shapeshift: [animal name]" power.
- **Shapeshift powers cost Stamina:** Right now, activating a form costs nothing. Each form should cost a small amount of Stamina, starting at 15 or 20. This can go higher if Stamina isn't draining enough during play.
- **Status:** Mostly ready. Hearts already exist and already wait for the Instinct power, so this perk can grant Instinct. New work: the Stamina cost on changing form, disease immunity, and the stacking kill boost.

---

## Beast branch

### Keen
- **Requires:** Unbound Form
- **What it does:** While in animal form, you can switch on night vision, Detect Life, and Detect Death.
- **Menu approach:** The Vampire Lord transformation already takes over the Favorites menu to give access to its own spells and abilities. Keen will copy this approach so the animal-form abilities appear there too.
- **Status:** Done, tested in game. Favorites (Q) in animal form lists only Revert Form and the three senses, as the Vampire Lord's does: the game picks that list by race, so Protean lends the animal's race and its own list while the menu is built. Pick a sense, press the power key (Z) to switch it on or off. Night vision is free; Detect Life and Detect Dead drain 2 Stamina a second each (`Protean_KeenDrain`) and go out on an empty bar. Changing back puts every sense out.

### Survivor ×2
- **Requires:** Keen
- **What it does:**
  - Rank 1: the health bonus rises from 25% of your health to 50%, plus a quarter of the size gap as before. See "Changes to Protean itself."
  - Rank 2: it rises to 75%, plus the same quarter of the gap. Predator animals also gain the ability to "activate" a corpse and eat it, like a werewolf, which restores health.
  - Idea to explore: herbivores could "activate" plants and mushrooms to eat them for the same health restoration. If this can't be done, herbivores instead get an extra health regeneration bonus outside of combat.
- **Changed 2026-10-28 (user):** rank 2's body-eating is replaced by consuming: with rank 2 a consumed body heals 50% instead of 20%. Eating plants stays.
- **Status:** Done, tested in game (2026-10-24). The health bonus is 50% / 75% (each rank adds `Protean_SurvivorHealthPct`). Rank 2: press Activate in animal form by a body (meat eaters) or a harvestable plant (plant eaters; omnivores both, diets from Wikipedia and the games' lore) to eat for 3 s standing still; a body heals 25% of maximum health (once per body), a plant 20% (it counts as harvested and grows back; the ingredient is not kept). Moving ends the meal without healing. Eating a person in sight of people turns them hostile until the change back. Sounds: the player's eating sound twice for a plant, the werewolf's feeding sound once for a body, with a hit's blood splashing twice from the animal's mouth. No eating animation: the animal just stands (user: plenty of vanilla actions have none). The few the game has (deer, goat, cow, sabre cats, were-forms) were dropped, as the deer's outlasted the meal and held the player in place for several seconds.

### Predator ×2
- **Requires:** Keen
- **What it does:**
  - Rank 1: more damage in animal forms.
  - Rank 2: attacks in animal form add poison.
- **Status:** Done, tested in game (2026-10-24). Rank 1: animal-form damage +2% per level, at most double (`AttackDamageMult`, set at the change). Rank 2: a power attack's hit in animal form poisons, 5 + 0.3 x level damage a second for 5 s (refreshed, not stacked; poison resistance counts; the undead and Dwarven machines are immune).

### Stalker ×2
- **Requires:** Keen
- **What it does:**
  - Rank 1: a stealth bonus.
  - Rank 2: enemies lose track of you faster.
- **Status:** Rank 1 done, tested in game (Stalk: see `src/Stalker.h`). Rank 2 built 2026-11-02, tested by script in game the same day (`tools/perktest.py stalker2 stalker2_seen`: fades and the enemy gives up after 3.0 s when unseen; no fade while seen) (the user's design): Stalk's fade may start in a fight while no enemy has the player in line of sight (rank 1 forbade any fight, which made "lose track sooner" useless); once faded, an enemy that has not seen the player for `Protean_StalkerLoseSeconds` (3) gives up the hunt (its combat and alarm are stopped). Being hit, attacking, sprinting or an empty bar still end the fade.

### Apex (capstone)
- **Requires:** Survivor, Predator, and Stalker, each taken at least once
- **What it does:** Lets you transform into a dragon, which you fully control.
- **Status:** Dragon dropped (the user, 2026-11-02): Play as a Dragon (Nexus 28854) and True Dragon Aspect were both tried in game and were jerky and buggy to move and attack with (the dragon's head also bent hard to one side); neither will be used, required or copied. The user will choose a new capstone.
- **Kept from the trial:** True Dragon Aspect's jump-and-glide flight (jump lowers gravity to glide; extra jumps in the air; Sneak drops fast) felt natural to the user for winged flight. An idea for winged forms; the chaurus hunter's bug-wing flight works and stays as it is.

---

## Hunter branch

### Rending Claws (stamina)
- **Requires:** Unbound Form
- **What it does:** Claws are equipped in each hand, like a weapon. Each hand can be unequipped on its own, so you can hold a shield, another weapon, or magic in one hand while the other keeps its claw. Each hand can also hold a different item, such as a claw in one hand and a Tendril in the other.
- **Stamina:** Costs a base amount of Stamina to use.
- **Look:** Our own natural horn claws. The attack animations are the game's own beast-race claw animations (Feral uses the same ones), so Feral is not needed; Open Animation Replacer is.
- **Status:** Done and tested in game (2026-10-23): damage follows the stamina bar, 5 Stamina a swing, werewolf hit sounds.
- **New look (2026-11-13, the user's sculpt reference `Prototype/Razor_claws_sculpt.jpg`; replaces the horn claws):**
  - The user's calls: the whole sleeve (from the shoulder), not just the forearm; the claws do not curl: each has one knuckle, which moves, and the rest of the blade is solid; length 80% of the reference to start.
  - Built: a sleeve of stringy sinew from the shoulder (the Blade's ragged edge and dark flesh), loose cords standing off the arm, tendons over the back of the hand; one long curved claw a finger, saw-toothed on the inner edge, bone with dark chitin along the back. Lengths: index 17.5, middle 19, ring 17 game units; the thumb's 10 and the little finger's 9. Each claw hangs whole on its finger's first bone. The fingers themselves stay, thin and dark, under the claws.
  - Measured (`tools/measure_claw_poses.py`): in every beast-race clip the claw tips stay at least 20 units above the ground; in the held stances they stay about 20 or more from the body's bones; during attacks they may brush the body.
  - Design calls made for the user (to overrule if wanted): the sleeve and claws now show only while the claws are drawn, as the Blade, Shield and Hammerfist do (before, the horn claws showed whenever equipped); the little finger carries a short spur rather than nothing; the female claws are 0.92 the size.
  - Files: `art/claws/` (Claws.blend, scripts run without Blender's window by `scripts/run.py`), `tools/build_claw_arms.py`, `src/Claws.cpp`. The perk, weapon, damage, Stamina and animations are unchanged.
  - Status: built and installed 2026-11-13. First look in game 2026-11-13 (user, screenshots 85-89): the right arm and claws show; the LEFT arm was a mess stretched across the body. Cause: the mirrored left meshes still carried the right arm's bone names (the names live on the mesh; the mirror script added new ones instead of renaming). Fixed and reinstalled the same day. Seen again 2026-11-13 (user): both arms right now; "I like the claws". 1st person: the open end of the sleeve's elbow cut could show; now closed with a cap (tools/first_person_cut.py), not yet seen.

### Proposed hand forms (the user's design, 2026-11-03; not built)
More shapes for the player's hands, as in Prototype. All are stronger than normal weapons and armour because they cost Stamina to use (the user: a fight should drain Stamina fast while many biomass stacks refill it fast).
- **Hammerfist** (requires Rending Claws): one-handed; counts as a mace for the vanilla perks. Every power attack staggers: a hit staggers the target, a miss strikes the ground and staggers enemies nearby. Costs Stamina like the claws.
  Brief (user, 2026-11-05, while away; Claude to act as the user in guiding the design): heavy and organic, mostly muscle, with bits of bone and chitin in the fists; both arms, male and female; the lessons of the Blade and Shield; references Prototype/Hammerfists_level_max.jpg and the Prototype 2 art.
  Design calls made by Claude for the user (2026-11-05; all open to change):
  - The biggest sleeve (with the Blade, as the user set): from the shoulder with the Blade's ragged organic edge, rounded swollen muscle cords, thickening down the forearm into the fist.
  - The fist: an egg of muscle about 2.5 times a hand, biggest at the knuckles, flatter on the palm side (it sank into the hip); big chitin plates over the back and knuckles (the Shield's approved chaurus chitin: greenish-black, glossy 80/1.0, pale lines in the bumps, near-black red cracks), bare muscle on the palm and sides, 7 crooked bone spikes from the knuckles and back (the Blade's bone, duller shine). Dark blood colours, no glow.
  - Animations: the claws' beast-race unarmed set (measured, tools/measure_fist_poses.py: the game's human unarmed guard holds the fists 17-22 units from the head, the beast stance about 50 and at hip height). Kept clear of the head in the held beast stances and of the ground in every beast pose; an attack's wind-up may brush the head for an instant (the Shield's rule).
  - A weapon for either hand like the claws, no model of its own; one hidden arm per hand (right body slot 56, left 60), worn while that hand holds it drawn, 3rd and 1st person (the upper arm cut in 1st person). The left arm is the right mirrored; the female arm is the male carried onto her body (the Blade's method); the fists are the same for both sexes.
  - Numbers (to tune in play): the game's mace keyword (the mace perks); damage = the claws' (it follows the Stamina bar) x1.25, a mace's lead over a sword in vanilla (steel 10 against 8), Protean_HammerfistDamageMult; 7 Stamina a swing (claws 5), Protean_HammerfistSwingCost; reach 0.8, stagger 1.0; One-Handed experience per hit as the claws.
  - Every power attack staggers: a hit staggers the one struck (magnitude 1); a power swing that strikes nothing for 0.35 s comes down on the ground a step ahead: enemies (not followers) within 300 stagger, with the dive's landing crash and a small camera shake.
  - Perk: under Rending Claws, placed for now (1.2, 2.75).
  Status (2026-11-05): built and installed (Blender art/hammerfist/Hammerfist.blend; tools/build_hammerfist.py; src/Hammerfist.cpp), not yet seen in game. To check: the look in 3rd and 1st person, both hands, male and female; clipping with the body when walking; the swings' Stamina and damage; the power-attack stagger and the ground slam.
  Tested in game 2026-11-13 (user, screenshots 90-91): the right arm and fist show; the LEFT was broken the same way as the claws' (same cause, fixed, not yet seen again). The user: the fist is larger than the renders suggested and the swings feel light, "balloon hands": reduce the size about 20% or slow the swings. Done: the fist is 0.8 of its first size, and the weapon's speed is 0.8, a mace's (was 1). Whether the speed setting slows these unarmed-style swings is not yet checked in game. The power attack into thin air shook the camera and played the crash, but the fist does not reach the ground in the animation: open (ideas offered to the user). Seen again 2026-11-13 (user): the left arm right now, the swings "swing better" (smaller fist, speed 0.8). 1st person: during a swing the camera looked into the open end of the sleeve's elbow cut, with loose flaps (screenshot 92); the cut end is now trimmed and closed with a cap that dips into the arm (tools/first_person_cut.py, also used for the claws), not yet seen. The user 2026-11-13: the ground slam is more tolerable with the slower swings, but in 1st person the power attacks are "totally in the air". Agreed ("all three"): a hand's own power attack is now the one-handed overhead power attack (the fist comes down to the ground; the left hand's a mirror image made by tools/build_hammer_anims.py), the both-hands power attack the warhammer's overhead smash, each retimed so the fist lands on the unarmed clip's HitFrame; and the crash and shake now come when the fist lands (its HitFrame) with no hit within 0.15 s, rather than a fixed time after the swing. The both-hands smash is told apart by the attack's name (logged: "hammerfist: power attack '<name>'"; to check). Built and installed, not yet seen in game. The both-hands smash keeps the unarmed combo's three hits: the first two come during the raise. Tried in game 2026-11-13 (user): every power attack "makes the body go crazy", the hit sound plays, then the body slowly relaxes into the stance. The log: the attack names are attackPowerStartForwardH2HRightHand / ...LeftHand / attackPowerStartH2HCombo (the both-hands one told apart correctly), and each slam came on its landing. Suspect: the clips as re-written by PyNifly's writer (fast motion; the glide it wrote is slow). Test installed: the right hand's and both hands' power attacks are now the game's own files untouched (their timing a little off the HitFrame); only the left (a mirror image) is still written. Needs the game restarted.
- **Blade** (requires Rending Claws): both arms become blades; a two-handed weapon using the Two-Handed skill. Its edge over a greatsword: it slices quickly. Costs Stamina like the claws.
  Look (the user, 2026-11-03, from Prototype reference images in `Prototype/`): **one** huge scythe-like blade growing from the right forearm (the hand disappears into it), the left hand resting near it in the two-handed poses; a **pale bone edge**; **as big as Prototype's** (tip near the knee with the arm hanging); dark red flesh with glowing veins over the forearm.
  Status (step 1 test, 2026-11-03, user's screenshots, sneaking): with the greatsword animations both hands are held together in front, so the "along" blade points straight forward like a lance, and the "half" blade stands up out of the fists like an ordinary curved sword. Neither looks like Prototype's hanging arm-blade. (The sheathed blade had first fallen to the feet: fixed with a Prn tag.)
  Status (one-handed test, 2026-11-03): the one-handed blade stayed on the back of the head even drawn (its model said to hang on the back, a greatsword's place; changed to the hip). The flesh sleeve (slot 57) did not show over gloves or clothes; it "looks cool with no clothes over it" (user). Made about twice as thick.
  Status (2026-11-03, screenshots 33-35): the one-handed blade works, hanging from the right hand; it should be wider and more curved (user). The thicker sleeve shows over the shrouded armour but still spotty through fine clothes, and the shrouded gloves poke through it. Made wider, more curved, and the sleeve thicker again.
  Status (2026-11-03, screenshot 36): good when swinging, but at the ready it looked held, not grown (the weapon's model hangs from the hand and turns with the wrist). Some clipping with clothes is acceptable; the sleeve could be a little bigger still (user). Next test: the weapon's model empty, the blade part of the sleeve, fixed to the forearm (ForearmTwist1); the user agreed. Later the plugin would put the sleeve on when drawn and take it off when sheathed (the blade grows and retracts).
  Status (2026-11-03, screenshot 37): with the blade in the sleeve it still seemed to grow from the fingers (it began just behind the palm). Its base moved back to near the elbow, the tip kept where it was.
  Status (2026-11-03): it starts at the elbow now, but on the arm's inner (medial) edge, not its middle (user). Moved onto the forearm's centre line.
  Status (2026-11-03): on the centre line it is "more in line, now" (user).
  Status (2026-11-03): the user asked to get the shape right in Blender first (official Blender Lab MCP; `art/blade/Blade.blend`, generator text `make_blade.py` inside it). Approved outline (draft 4): a bone crescent down the back of the forearm, about Prototype's size, ribbed near the elbow, a concave front edge, a spike above the elbow pointing up, and a talon tip after Mercer's blade in Prototype 2 ("Yes, this is good"). A slim straight point (draft 3) was rejected. Next: the flesh.
  Status (2026-11-04): flesh draft 1 (a lumpy mass over the arm, the hand melted in) was too smooth (user: "less smooth, a more twisted surface"). Draft 2, twisted cords spiralling down the arm and wandering by noise (`make_flesh.py` in the .blend): "Yes, this is much better". Known: a small gap near the elbow on the front. Next: textures (veins, glow, flesh blending into bone).
  Status (2026-11-04): the Blender model in game (Sleeve_M.nif from `export_game.py`, screenshots 40-44): "It looks pretty good"; the flesh and blade follow the arm at the ready. The area round the hand is bulky (user: "may be necessary to hide the fingers"). Seen in the screenshots: the blade glares near-white (placeholder horn + vertex colour). Mid-swing and bent-elbow views not yet seen (Free Camera Utility cannot be used while controlling the character).
  Status (2026-11-04): the hand slimmed a little ("just a little bit"); mid-swing screenshots 45-47: "Everything looks good". Flesh and blade follow the arm through the swings with no tearing seen. Next: textures.
  Status (2026-11-04): textures made in Blender (`make_skin.py`, baked by `bake_skin.py`): dark red-black flesh, red veins edged in black (red, not orange: user), ivory bone with fine cracks, the flesh creeping onto the blade. Veins dimmed and made to run along the arm (user: too bright, "squiggly" not "veiny"). A tongue of flesh above the wrist capped (user). User "not 100% happy with the texture" yet; installed for an in-game look. Unconfirmed: normal-map green direction (`FLIP_GREEN` in build_blade.py) and the glow.
  Status (2026-11-04): in-game look, screenshots 48-54: the arm glowed bright red and the bone glared white in daylight (fine in the dark); you could see under the sleeve at the shoulder (50). User: colouring "a little bit off"; wants "realistic flesh", "less glowy", within the mod's dark-blood, shadowy theme (as in the devastators), not Prototype's glow. Changed: the flesh grows out of the shoulder (thins and dips under the skin at its edge); no glow; wet dark red-brown muscle, near-black blood, dark veins; bone darker and bloodstained. Installed for a look.
  Status (2026-11-04): screenshots 55-57: bumps catch the light the right way (user: "Nothing looks super off"; normal-map direction settled); bone glare "much better". User asks for a more sinewy, corded-muscle look instead of blobs, without losing the current shape: make_flesh.py now holds two presets, "lumpy" (the approved shape, still the default) and "corded" (shown side by side in Blender).
  Status (2026-11-04): user on the corded preview: forearm-to-hand "way too swollen"; wants more tendons and muscle fibres, "almost like vines". Third preset "vines" (now the Blender default; not yet in game): slimmer forearm and hand, raised strands twisting round the arm with a second set twisting the other way. The bulk limit now fades in at the elbow and out at the wrist for corded/vines; lumpy is unchanged.
  Status (2026-11-04): user liked the vines render but saw two big balls at the fingertips. Cause: past the wrist the strands bunch up with nothing to wrap round. Vines now thins the hand and fades the strands toward the tip (TIP=14, TIP_MIN=0.15); awaiting the user's look.
  Status (2026-11-04): user approved the vines tip ("It looks good"); vines baked, built (flesh 9982 verts) and installed in MO2. Awaiting an in-game look.
  Status (2026-11-04): vines in game: user says the shape looks good and the blade looks perfect; the arm sleeve is "still too red" and should be a red so dark it is nearly black, like shiny dark blood. The arm alone is now darkened (ARM_VALUE=0.4 in make_skin.py; the blade is unchanged) and shinier (Arm spec 0.7). Installed; awaiting a look.
  Status (2026-11-04): user approved the darker arm in game: "I think it's perfect". The Blade look (vines shape, dark arm, bone blade) is final for the male third-person model.
  Status (2026-11-04, user test): the one-handed test Blade (one-handed stance, Two-Handed skill) blocks with the right mouse button when the left hand is empty. (An earlier left punch was with the Blade unequipped by mistake.) Next: keep the left hand empty while the Blade is out, so it plays as a two-handed weapon.
  Status (2026-11-04): built and installed: the Blade keeps the left hand empty like a greatsword (src/Blade.cpp): anything put in the left hand (weapon, spell, torch, shield) puts the Blade away; taking the Blade up empties the left hand. Not yet tested.
  Status (2026-11-04, user test): works: a spell (Tendril) into the left hand put the Blade away; taking up the Blade made the left hand drop a torch; blocking works. Shield not tested (the user has none).
  Status (2026-11-04): built and installed: the Blade perk (0xA05, under Rending Claws, placed for now) keeps the Blade in the inventory; the sleeve is hidden and Protean puts it on while the Blade is drawn (at the draw animation's weaponDraw, off at weaponSheathe); each swing costs 5 Stamina (Protean_BladeSwingCost). The test greatswords (0xA00, 0xA01) are gone and the test weapon is now "Blade". Not yet tested.
  Status (2026-11-04, user test): "Everything looks good": the perk gives the Blade, the arm grows on draw and goes on sheathe, swings cost Stamina, blocking and save/load fine.
  Status (2026-11-04): user confirmed the Blade did not show in 1st person. Built and installed: Sleeve1st_M.nif (the flesh cut at the elbow, and the blade), which the plugin hangs in the 1st-person body while drawn and binds to the 1st-person bones of the same names so it bends. Not yet tested.
  Status (2026-11-04, user test): 1st person works: the blade shows, pointed ahead and slightly left, and appears at the right time; but it was still out while the sheathe reached across the body as if to a scabbard. Now the arm grows only once fully drawn and goes back as soon as the sheathe starts (3rd person too).
  Status (2026-11-04): user: the reach across the body to sheathe is the weird part (the Blade grows from the arm; the claws just pop). Open Animation Replacer now swaps the sword draw and sheathe (1hm_equip/1hm_unequip, 3rd and 1st person) for the empty hands' own (h2h_equip/h2h_unequip) while the right hand holds the Blade (tools/build_claw_anims.py, submod Blade). Installed; not yet tested.
  Status (2026-11-04, user test): sheathing is instant, but the draw sometimes took 2 seconds. Cause found: the empty hands' draw animation has no BeginWeaponDraw/weaponDraw signals (the sword's has both). The draw is now the bound sword's (no reach, both signals, 3rd and 1st person); sheathe unchanged. Installed.
  Status (2026-11-04, user test): the draw still took several seconds after R. Cause (log: each draw began 3 s after the sheathe): the empty hands' sheathe has no BeginWeaponSheathe/weaponSheathe either, so the game waited out a timer. The Blade's copy now has both at its start.
  Status (2026-11-04, user test): "It is quick, now." Draw and sheathe both fast. Next: the flesh's edge on the back/shoulder is a straight line with right angles, not organic (with or without clothes).
  Status (2026-11-04): installed: the shoulder edge is now cut on the finer points along a wandering line that reaches out from the shoulder further in some directions (make_base.py REACH, make_flesh.py EDGE*), and the flesh rises out of the skin over a shorter band (TAPER 4). Not yet tested.
  Status (2026-11-04, user test): "It looks much better": the shoulder edge is organic now.
  Status (2026-11-04, user test, female via player.sexchange): the male model (used for both) mostly fits, but there are gaps near the shoulder (screenshots 59, 60: the flesh floats off her narrower shoulder by the neck). Next: a female model built on the female body.
  Status (2026-11-04): installed: Sleeve_F.nif and Sleeve1st_F.nif, the male flesh carried onto the game's female body (Blade.blend make_female.py: each point keeps its place over the skin; the female skeleton's arm bones match the male's). Same textures. The plugin hangs the female 1st-person model for a female player. Not yet tested.
  Status (2026-11-04, user test): "Yes, it looks good": the female arm fits, no gaps at the shoulder.
  Status (2026-11-04, user): no faint ring, no nubs at the hand tip, the 1st-person blade angle is fine, and the blade appearing on R is good: the looks are done. Next: the numbers (user: significantly stronger than the claws).
  Status (2026-11-04): numbers agreed with the user and installed: damage 1.7x Rending Claws' full-bar damage at the player's level (the vanilla greatsword-over-sword gap: 18 at level 1, 32 at 10, 83 at 30, 162 at 50), always full strength (user: only costs Stamina), Two-Handed skill and perks on top; sword speed 1.0 (slices quickly), greatsword reach 1.3 and stagger 1.1; 8 Stamina a swing. Dials Protean_BladeDamageMult (0xA07), Protean_BladeSwingCost. Not yet tested.
  Status (2026-11-04, user test): inventory showed claws 42, Blade 43, with +60% One-Handed perks and none in Two-Handed (the 1.7x lead and the perks cancel). User chose to keep it: the Blade rewards Two-Handed perks, as a greatsword would for a one-handed specialist.
- **Shield** (requires Subtle Carapace): left hand only, like a normal shield. Blocks all damage of a blocked hit for a flat 5 Stamina a hit. With no Stamina it blocks nothing, and the player can be staggered as in a guard break.
  Blocking rule (user, 2026-11-05): spells too, like a ward spell: a spell hitting the front while blocking with the Shield does nothing, for a flat 5 Stamina, as a blocked hit or arrow does; from behind or the sides spells and hits land; no ward shimmer (the normal block only).
Open: numbers (damage, speed, Stamina per swing), looks.
  Looks (user, 2026-11-05): inspired by Prototype's shield (Prototype/Shield_level_1_&_max.jpg, Shield_Menu.jpg); start from the shape of Dragonborn's Chitin Shield, add organic bits, and a sleeve so it looks grown out of the hand and arm.
  Status (2026-11-05): Blender draft (art/shield/Shield.blend: make_shield.py, make_base_s.py, make_flesh_s.py, make_cords.py): the Chitin Shield with sharper rim points and a face of jagged overlapping plates; the Blade's vine flesh on the left arm, from the hand to mid upper arm; flesh cords from the forearm into the shield's inside; the thumb covered. User: "It's looking good". Next: textures, export, in game.
  Status (2026-11-05): built and installed, not yet tested: the Shield perk (0xA0A, under Subtle Carapace, placed for now) gives a light Shield (armor 24.5, as the Chitin Shield; empty model). While it is worn and drawn, Protean puts on a hidden sleeve (body slot 54; tools/build_shield.py): the vine flesh (male and female), the cords and the plated shield, painted dark (flesh as the Blade's arm; glossy near-black chitin, dark red in the cracks); 1st person too (the upper arm cut). It blocks as an ordinary shield for now.
  Status (2026-11-05, user test, screenshots 61-65): grows and shows in both views. Problems: in the ready stance (not the sneaking one) the shield goes through the face (61; it is about a third bigger than the Chitin Shield); a seam can be seen through from some angles (62, 63); in 1st person it is all flat polygons, pale, not like Skyrim's textures (64, 65).
  Status (2026-11-05, user test): after the fixes (smaller, one closed shell, smooth within plates, darker): "It looks much better"; it blocks like a shield; equipping and unequipping (grows on draw, gone when put away) work as intended. Next: the Shield's own rule (blocked hits cost a flat 5 Stamina and do no damage; no Stamina, no block and a guard-break stagger).
  Status (2026-11-05): user: make it huge like Prototype's, worthy of its blocking, but try to keep it off the face. Measured from the game's animations where the head is, seen from the shield: in the ready stance (1hm_idle) almost in the shield's plane, ~52 from its middle, by the elbow (that is what clipped); in sneaking ready and blocking 26-40 behind it. Built and installed: about twice the area (from the shoulder to the shins with the arm hanging), grown past the fist and outward, bigger plates; every point kept 17 from the head in the ready and walking poses. Not yet tested.
  Status (2026-11-05, user test, screenshots 68-73): "It definitely feels bigger"; it went into the ground in several movements and positions (crouched, ready). 1st person accepted: blocking covers nearly the whole screen, "it should make you feel protected".
  Status (2026-11-05): measured 98 of the game's 1hm and sneak animations (tools/measure_shield_poses.py -> art/shield/shield_poses.json: the ground and the head as seen from the shield). Kept clear in every one, attacks, bashes and staggers too, the shield could be no bigger than draft 2; kept clear in the held stances (ready, walk, run, turn, block, sneaking idle), it can be bigger than draft 3. Built that (draft 4, about 100 x 110): the plain outline shrunk where needed before the plates grow; in quick attacks, bashes and staggers it may brush the ground or head for an instant. Installed; not yet tested.
  Status (2026-11-05, user test): draft 4 "feels right, now": the Shield's look and size are done. Next: its blocking rule.
  Status (2026-11-05, user, screenshots 74-76): the 1st-person texture is not up to Skyrim's standard (compared with the game's Chitin Shield: "just the level of detail"). Built and installed: the shell laid out as large flat pieces (front and back), 4096 texture, fine grain, pits and scratches carried into the surface bumps, a ribbed darker inner side with wet flesh in the grooves, BC7 compression (no banding). Not yet tested.
  Status (2026-11-05, user test, screenshots 77-79): the detail "is looking right"; the front still looks good (79); the red stripes on the inside (the ribs with flesh in the grooves) not liked (77, 78). Ribs and red taken out: the inside is now the same chitin as the front. Installed; not yet seen.
  Status (2026-11-05): user: the grey slightly more like the chaurus's texture. Built and installed: greenish-black chitin (olive, teal) with the chaurus's fine network of pale yellow-green lines (raised a little) and faint grooves along the shell; the cracks stay dark red. Not yet seen.
  Status (2026-11-05, user, screenshots 80-81): next to a chaurus the textures "don't look anything alike". Found: the chaurus's shell is set very glossy (its model: glossiness 80, specular 1.0) where ours had the claws' dull 8 and 0.25; its plates are long, overlapping, rimmed (ours round lumps); its fine lines are mostly in the bumps (ours painted pale); our crack red was vivid in daylight. Step 1 built (user's choice): the chaurus's shine with a mask (dull in the cracks and on the cords), the lines mostly in the bumps, the cracks near-black blood red. Installed; not yet seen. Step 2 if still needed: long overlapping rimmed plates.
  Status (2026-11-05, user, screenshots 82-83): "we can finally say the shield look is done". Final: draft 4 shape; chaurus-like greenish-black chitin, glossy (80 / 1.0, dull cracks), pale lines in the bumps, near-black red cracks. Step 2 (overlapping plates) not needed. Next: the blocking rule.
  Status (2026-11-05): blocking rule built and installed, not yet tested. Hits and arrows: a hidden perk (Protean_ShieldBlock, 0xA0F) sets the share of a blocked hit stopped to 1 while Stamina >= Protean_ShieldBlockCost (5), else 0; each blocked hit costs a flat 5 (the game's own block Stamina cost given back); without the Stamina the hit goes through and staggers. Spells: a hostile spell landing while blocking, its caster within Protean_ShieldWardAngle (60 degrees) of where the player faces, is refused (MagicTarget::AddTarget on the player) for 5 Stamina, paid once per spell per half second (streams). Every block, ward and guard break is logged.
  Status (2026-11-05, user test): still hurt while blocking. Log: blocked hits caught and charged 5, but Health fell between them (a thrall's sword: 160, 149, 146, 135, 131) with the percent-blocked perk held at 1; the spider's poison (Spider Bite, Spider Spit) landed while blocking; the ward never hooked (the MagicTarget part was looked for at the wrong place). Fixed, built, not yet installed (game running): a paid block's Health loss is given back for 0.3 s after the hit; the ward found through the runtime-safe accessor.
  Status (2026-11-05): the Health give-back not liked (user). Found: the game caps a block at 70% (fBlockMax 0.70; power attacks x0.66), which is why the percent-blocked perk could not stop a hit; armor rating does not change blocking (it cuts all damage, capped at 80%). User chose option 1, changing the hit itself (a patch on the game's call that applies a hit, checked before it is written and at each hit), and option 3 if that fails (a perk cancelling all incoming damage while blocking, hits from behind too). Option 1 built and installed, not yet tested.
  PINNED (2026-11-05, user away): waiting for the user's test of option 1 (the hit patch) and the ward fix. Next on return: read the log ("hit patch written", "hit seen", "blocked ... damage", "ward hooked", "warded").
  User, 2026-11-05: the shield and sleeve vanish when weapons are put away (like the Blade; not hung on the back); one look, no change with level.
  **Sleeve sizes for all the weapon forms (user, 2026-11-05):** the Blade and Hammerfist have the biggest sleeves (they are "heavy"); the Shield a medium one, covering the hand, forearm and parts of the upper arm; the Tendril's and Rending Claws' sleeves (when made) mostly only the hand and forearm.

### Tendril
- **Requires:** Rending Claws
- **What it does:** A whip of flesh that shoots out and hits the target, damaging their health and draining their stamina into the player. A light target is yanked off its feet to the player; a large one (troll, bear, giant, mammoth, grounded dragon) hauls the player to it instead.
- **Equipping:** A spell, taught by the perk, for either hand: a claw in one hand and the Tendril in the other works. (Decided in game: the spell-casting motion is fine, so no inventory item.)
- **Numbers:** 25 Stamina a lash, 3 seconds between lashes; a miss gives the Stamina back and needs no wait. Damage is half of Rending Claws' full-bar damage; the drain is 20 + half the player's level. Reach about 21 m.
- **Status:** Done and tested in game (2026-10-23). Built from the ideas of Pull Mastery and Xyn's Rope Arrow (studied, nothing copied), not from Miraak's tendrils, which turned out to be effects rather than a whip. Our own model: sinew skin from Apocrypha's tentacles, a horn barb. Throw sound: the vampire lord's swipe. An extra blood effect on hits did not show and was left out.

### Predator's Rush
- **Requires:** Unbound Form
- **What it does:** Adds a small speed bonus to normal movement, and a slightly larger speed bonus to sprinting. Sprinting still costs Stamina, so the player still pays to move really fast. Jumping should be high enough to land on the ledge of a one-story building.
- **Status:** Done, tested in game (2026-10-24). Human form: +10% on foot; a sprint that builds from 1.25x to 3x a normal sprint over 10 s (capped there for area loading); jump height 400 (vanilla 76), with fall damage starting that much higher so a big jump hurts no more than a normal one. Jump works while sprinting (vanilla forbids it) and the sprint's build-up carries through the air. Dials xx977-xx97B.

### Prey's Flight
- **Requires:** Predator's Rush
- **What it does:** Double jump, gliding, a diving slam, and no fall damage.
- **Status:** Done, tested in game (2026-10-24). No fall damage in any form. Human form: Jump again in mid-air (15 Stamina, 1.25x the player's jump); hold Jump while falling to glide (5 Stamina a second, eased in like Paraglider: down at 250, forward at 900 or more where the player faces); a power attack after 1 s in the air dives (30 Stamina) and the landing staggers everyone within ~8 m, followers too, with the dragon's landing crash and a camera shake. The glide pose is Protean's own wingsuit animation (Prototype's), written from scratch on the game's skeleton (`tools/build_glide_anim.py`, PyNifly's HKX writer), shown by Open Animation Replacer; red absorb wisps on the hands, the Vampiric Drain caster's wisps on the body, and the ice-cave wind. Running out of Stamina eases into a normal fall.

### Subtle Carapace ×2
- **Requires:** Unbound Form
- **What it does:** Armor bonus, with two ranks.
- **Status:** Done, tested in game (2026-10-24). Rank 1: 50 + 4 x level armor; rank 2: 100 + 7 x level (replaces rank 1's). It works in every animal form, and in human form only while no heavy armor, and no light armor other than fur, hide, leather, chitin or dragonscale, is worn on the body (clothes and robes are fine; the natural kinds allowed by the user 2026-10-25): the restriction that balances it. At level 50 rank 2 is about a well-made light armor set. The numbers are starting points for the user's playtest.
- **Changed 2026-11-05 (user):** also magic resistance, 10% at rank 1 and 20% at rank 2 (replaces rank 1's), under the same rule as the armor (`Protean_CarapaceMagic`, `Protean_Carapace2Magic`). Built and installed, untested.

### Regeneration
- **Requires:** Subtle Carapace
- **What it does:** A toggle. While it's on, Stamina drains and health rises by the same amount.
- **The payoff:** Its real benefit comes from stacking kill boosts from Unbound Form. With enough stacks, Stamina recovers faster than Regeneration drains it, so the toggle becomes sustainable.
- **Status:** Built 2026-10-27 (with Devastator): a toggle power, Stamina into Health at `Protean_RegenPerSecond` (8) a second. Tested in game by the user 2026-11-02, in combat, switched on and off twice (log: on/off at the right times; the amounts are not logged).

### Devastator (capstone, stamina)
- **Requires:** Rending Claws, Prey's Flight, and Subtle Carapace
- **What it does:** Unleashes bloody, powerful area attacks, inspired by Prototype.
- **Stamina:** Costs a full Stamina bar.
- **Activation rule:** Requires at least two stacks of the Unbound Form kill boost, so it can't be used to start a fight.
- **Status:** Built and tested in game (2026-10-27). The Devastator power: own form, full Stamina, two kill boosts. Weapons are put away, then the ritual pose (2 s) with the change-into-a-beast sound and red tentacles over the body while the camera circles; at the burst slow motion (0.15, 3.5 s), the camera pulls farther out and circles faster, 8 red tendrils from the body (thrown again through a 4.5 s hold), the red lurker splash and blood burst, a stab, thunder and explosion sounds, and every hostile actor within 30 m knocked down (PushActorAway) for 100 + 8 per level damage. Its kills feed no kill boost (user's choices).
  Status (2026-11-05): misfire seen by the user twice so far (sound, no pose, player standing still). Test with animation-event logging: 2 tries, the 1st (claws out, put away first) fine, the 2nd (hands already empty) misfired. Log of the 2nd: the pose was accepted at once (0.01 s), then the game stopped it the same instant (IdleStop, AnimObjectUnequip). Cause not proven.
  Fixed 2026-11-05: the pose waits for the player to stand (at most 0.6 s) and is asked for again if stopped at once. User test, 5 tries (3 with weapons away, 2 with claws drawn): the pose played every time. Log: one try waited 0.39 s for the player to stand, as running would; no retry was needed. Movement is the likely cause (not proven).

---

## Deceiver branch

### Borrowed Visage (stamina)
- **Requires:** Unbound Form
- **What it does:** Unlocks the face-stealer power.
- **Stamina:** Costs the same amount of Stamina as changing into an animal.
- **Changed 2026-10-28:** the face comes from consuming a person; the Loot / Copy appearance window is gone.
- **Status:** Built (2026-10-24), tested in game by the user (reported 2026-11-02). Face stealing (tested live earlier on Ulfric, Lydia and others) now needs this perk instead of the Instinct power; owning it gives the Stolen Face power. Putting a face on (stealing one, or wearing the kept one) costs `Protean_ChangeStaminaCost`; taking it off is free.

### Metamorphosis (stamina)
- **Requires:** Borrowed Visage
- **What it does:** Changes your face and race. Clears all bounties and crimes.
- **Stamina:** Costs 100% of your maximum Stamina, so it can't be used during combat.
- **Status:** Needs research. Protean already copies a face together with its race, but only as a disguise you can take off. Metamorphosis would make it permanent. Protean also already removes a bounty when nobody sees you change back from an animal, which is a useful starting point for clearing bounties.
- **Built (2026-10-26); tested in game 2026-11-02 (user):** the menu opened, race changed to Argonian and kept, the 15 skills the menu reset were put back (One-Handed 65, level 14). Clearing a bounty not yet seen (there was none). Redesigned by the user: a reshaping like Riften's face sculptor, race included. The Metamorphosis power (out of combat, own form and face, full Stamina, uses all of it) clears every bounty and opens the game's full character menu; the skills, perks, perk points and base attributes are noted first and put back when it closes, the way ShowRaceMenu Alternative (Nexus SE 2155) does.
- **Earlier research (2026-10-26):** No mod found that does exactly this on SE. Ones to learn from: RaceMenu (its source is public; Protean already uses the same calls to put a face on), Alternate Actors (original Skyrim: play as any NPC), and the tint-mask fix plugin (Nexus SE 122198: where the game drops face colour data). The game saves the player's own face record, so a permanent look may mostly mean not taking it back; faces drawn from a named NPC's finished face files would need putting on again after each load.

### Inconspicuous (stamina)
- **Requires:** Borrowed Visage
- **What it does:** Gives a stealth bonus that grows with the number of non-hostile people around you.
- **Status:** Built (2026-10-26), tested in game by the user (reported 2026-11-02). In your own form, +5 Sneak for each person within 15 m who is not hostile and not a follower, up to +30 (dials `Protean_InconspicuousPerPerson`, `Max`, `Range`).

### Infected
- **Requires:** Inconspicuous
- **What it does:** Reverse-pickpockets an infection onto someone. The infected fight for you.
- **Status:** Built (2026-10-26), untested. The Infect power (50 Stamina) puts one Infection item in your pockets; planted by reverse pickpocket, the person joins `Protean_InfectedFaction` (allied to the player), helps allies and stands their ground for one game day (`Protean_InfectedHours`). One person at a time (user's choices: fight as an ally; a day and one at a time). 2026-11-02, tested by the user: Infect grows the item (50 Stamina), planting it infects (the item lookup was broken until then). As a plain ally a guard still turned on the player for a crime (attacking Thongvor); the user chose: the infected never turn on the player, crimes included. Every half second an infected person targeting the player is called off (combat and alarm stopped), and one not already fighting someone else attacks the nearest actor fighting the player within 58 m. Live, that alone was a tug of war (the game turned the guard back on the wanted player every half second), so the infected also lose their crime faction (`Actor.SetCrimeFaction(None)`, their base's put back when cured): outside the law, they ignore crimes. Tested: the guard then fought Thongvor for the player. But hitting another guard, he kept defending his comrade (the same tug of war), so the infected also leave every faction but Protean's own; the base's factions are put back when cured (factions given in game are lost). Tested by the user 2026-11-02: the infected guard left 7 factions, attacked his comrade when the player hit one, never turned on the player, and fought for about 45 s until killed. Infected is done.

### Assassin
- **Requires:** Inconspicuous
- **What it does:** Gives a large sneak-attack bonus against a non-hostile target while you wear the face of someone from that target's faction.
- **Status:** Built (2026-10-26). Tested in game 2026-11-02 (user, Rending Claws on a calm Markarth guard): sneak attack, x10 applied, the game showed x20 (claws x2 times x10), about 613 damage, one hit. The rest of this line is the first design (stolen face); see the change notes. While a stolen face is worn, people near who share one of its factions and are not hostile are marked (`Protean_AssassinMark`); a hidden perk multiplies sneak attacks on them x10 (the user: it should kill one person outright before the rest react).
- **Changed (user, 2026-10-28):** "anyone who is not automatically hostile when they see you gets the x10", and no stolen face needed for now, "to see if it feels like Prototype". In the player's own form, everyone near who would not attack on sight is marked: not fighting the player, not an enemy by faction, Aggression under 2, and not an outlaw (no crime faction) the engine calls hostile. The engine's plain "hostile" was dropped: it named four calm Markarth guards hostile with no bounty.
- **Added 2026-10-28 (user: a sneak kill, consume and change into the person with no alerts if hidden before activating), built, tested in game by the user (reported 2026-11-02):** consuming a person in the own form while no person can see the player takes 1 s, is never witnessed (no crime, no exposure), calls off the combat and alarm of everyone near who cannot see the player, and puts the face on at once with no Stamina cost. Later the same day the assault the worn face did is forgiven and every alarm ended as well (Calcelmo, called off, attacked again until the face came off).
- **Stealth Consume (user, 2026-10-28), built; tested in game by the user (they reported it 2026-11-02, the result had not been written down):** a power that comes with Assassin. Own form, 25 Stamina, 60 s (Active Effects counts down). A person killed by the player's melee sneak attack while it lasts is consumed at once: 1 s, no box, the face put on free. Unseen, it is the unnoticed consumption above; seen, the usual crime. Its loot has its own switch on the Protean page, "Stealth Consume takes the loot" (on at first; off, the general loot switch decides). Co-save v26.

---

## Outside dependencies

These are the outside mods the tree relies on, and which perks use them:

| Dependency | Used by | Notes |
|---|---|---|
| Protean 1.0.0 (`C:\Modding\Protean`) | Every perk | Your own mod. Checked; see "What Protean 1.0.0 already does." |
| True Dragon Aspect (Nexus 142635) | (Apex, dropped) | Tried 2026-11-02 and rejected as too buggy. Its jump-and-glide flight is a reference idea for winged forms. |
| Feral (claws mod) | Rending Claws | Chosen as the base. We'll make our own, larger and fiercer, meshes. |
| Miraak's tendril (Dragonborn DLC) | Tendril | Possible source for the whip effect. Needs checking. |
| Paraglider (Nexus SE 53256) | Prey's Flight | Used as a reference for gliding. Gliding must work with only the player's body. A gliding animation must be found or built. |

**Important:** Your preference is to avoid requiring other mods unless they are truly essential. Where possible, we'll copy only the parts we need. Any mod we use will first be analyzed for how easy it is to learn from and rebuild as our own version.

---

## Build order

1. **Check the existing foundation.** Confirm what the Protean folder already does, then finish Unbound Form.
2. **Test the risky ideas first.** Make small prototypes for the three biggest unknowns: Prey's Flight (gliding and animation), Tendril, and Devastator. This is cheaper than finding out late that one of them doesn't work.
3. **Beast branch.** Keen, then Survivor, Predator, and Stalker, then Apex. Apex waits for the user's new capstone (the dragon was dropped 2026-11-02).
4. **Hunter branch.** Choose a claws mod first, then build Rending Claws, Tendril, Predator's Rush, Prey's Flight, Subtle Carapace, Regeneration, and finally Devastator.
5. **Deceiver branch.** Borrowed Visage, Metamorphosis, Inconspicuous, then Infected and Assassin.
6. **Final check.** Compare the mod against this document and list any mismatches.

Each branch gets its own working session. Risky prototypes happen before the branch session that depends on them.

---

## Open questions for you

1. ~~**Tendril source:**~~ Answered: our own, built on Pull Mastery's ideas (see Tendril).
2. **Evolution point name:** Which name do you want? "Biomass," "Vitae," or something from Protean lore?
3. **Checks in game:** Confirm that Harkon leaves a usable body, and confirm the details of Ildari Sarothril's quest.
4. **Perk cost curve:** The plan is 1, 2, 3, and so on, with each rank costing its own step. Does that match what you want?
5. **Stack time:** Should the kill stack timer be 60 seconds, 120 seconds, or somewhere between?
6. **Starting numbers:** The base Stamina cost for claws, the speed and jump bonuses, the Regeneration rate, and the Devastator damage are still blank. Want to pick starting values now, or during prototyping?

Answered so far:
- Apex requires Survivor, Predator, and Stalker, each taken at least once.
- Copy the parts we need from True Dragon Aspect rather than requiring it.
- Feral is the claws base, with our own larger meshes.
- Every mod gets analyzed for how easy it is to learn from before we use it.
- The stamina-cost list is correct, and Devastator costs a full bar and needs two kill-boost stacks.
- Tendril and Devastator take their feel from Prototype: whippy and fast for Tendril, bloody and powerful for Devastator.
- If plant eating can't be done, herbivores get an out-of-combat health regen bonus.
- Health keeps its percentage when changing into and out of an animal form.
- Health bonus: one rule for every animal, 25% of your health plus a quarter of the size gap. Survivor raises the 25% to 50%, then 75%.
- Perks cost 1 evolution point, then 2, then 3, and so on, with each rank bought separately.
- Stealing the face of a character on the named list grants an automatic perk point. It's fine that not every character is available in every playthrough.

Nothing in this document has been applied to your project. Let me know your answers, and I'll update the spec before any work begins.
