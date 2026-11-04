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

### Proposed hand forms (the user's design, 2026-11-03; not built)
More shapes for the player's hands, as in Prototype. All are stronger than normal weapons and armour because they cost Stamina to use (the user: a fight should drain Stamina fast while many biomass stacks refill it fast).
- **Hammerfist** (requires Rending Claws): one-handed; counts as a mace for the vanilla perks. Every power attack staggers: a hit staggers the target, a miss strikes the ground and staggers enemies nearby. Costs Stamina like the claws.
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
- **Shield** (requires Subtle Carapace): left hand only, like a normal shield. Blocks all damage of a blocked hit for a flat 5 Stamina a hit. With no Stamina it blocks nothing, and the player can be staggered as in a guard break.
Open: numbers (damage, speed, Stamina per swing), looks.

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
