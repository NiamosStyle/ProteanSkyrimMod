# Protean: what is actually in the mod (1.1.0)

**This is the source of truth.** It was built from the mod's own files, not from the other documents:
the plugin (`data/Protean.esp`), the creature and perk table (`data/SKSE/Plugins/Protean/creatures.json`), the perk
tree (`data/SKSE/Plugins/CustomSkills/Protean.json`), the settings file (`data/SKSE/Plugins/Protean.ini`), the SKSE
plugin's source (`src/`), and the release package (`dist/Protean-1.1.0-test.zip`). Files as of 2026-11-15.

When the mod changes, update this sheet first; every other document (README, Nexus page, INTEGRATION) takes its facts
from here. In-game descriptions are quoted exactly; numbers come from the plugin's settings (the `Protean_` values).
Distances: about 70 game units to a meter.

Each feature is marked:
- **Verified**: the notes record it working in game.
- **Not yet verified**: it is built, but no note says it was seen working. See the checklist at the end.

---

## 1. The package (1.1.0-test, 2026-11-15)

- `Protean.esp`: a **light plugin** (ESL-flagged ESP; it doesn't use a full load-order slot). Master: Skyrim.esm only.
- `SKSE/Plugins/Protean.dll` (the SKSE plugin), `Protean.ini` (settings; the developer channel is **off** in the
  package), `Protean/creatures.json`, `CustomSkills/Protean.json` (the perk tree).
- Meshes and textures for the claws, Hammerfist, Blade, Shield, Tendril, Devastator, the Strange Mound and the
  perk-tree constellation; animations and Open Animation Replacer conditions for the claws, Hammerfist, Blade and the
  glide.
- `README.md` and `docs/Protean-INTEGRATION.md` (the INTEGRATION copy in the package is dated 2026-11-03).
- The package's plugin, creature table and README are identical to the ones in `data/`.

## 2. Requirements

What the files depend on:
- **Skyrim SE 1.7.104** (the version it was built and tested on), **SKSE64** with its script files (`Scripts/*.pex`),
  **Address Library**. Face stealing calls three SKSE script functions (`Game.UpdateTintMaskColors`,
  `Game.UpdateHairColor`, `Actor.RegenerateHead`). Since 1.1.1 every Papyrus call is checked first (`src/Papyrus.cpp`):
  a missing function is skipped, logged and shown on screen once, instead of crashing the game.
- **Dawnguard and Dragonborn.** The plugin's only master is Skyrim.esm, but the code uses both DLCs at runtime
  (Death Hound, Chaurus Hunters, Boar, Ash Hopper, blood bursts, sounds), so both are needed.
- **Custom Skills Framework**: the perk tree.
- **Open Animation Replacer**: the claw, Hammerfist and Blade animations and the glide pose. OAR 3.2.1 crashes the
  game on every exit (a ghost actor it makes at startup; bisected 2026-11-19, happens with OAR alone). Fix,
  verified: `bLoadDefaultBehaviorsInMainMenu = false` in `OpenAnimationReplacer.ini`; claws, Hammerfist and glide
  still work with it off. The README tells players.
- **Optional: SKSE Menu Framework**: the Evolution journal page (see section 9).
- **Optional: Equip Enchantment Fix** ("1.6.629 and newer" build), only if an enchantment goes missing after changing
  back. Tested without it on 2026-11-15; nothing was lost.
- **The monster race crash fix** (Nexus SE mod 19899,
  https://www.nexusmods.com/skyrimspecialedition/mods/19899): prevents the crash when a player using a monster
  (creature) race enters an attack stance, which is exactly what Protean's animal forms do.

## 3. How a player starts

- **The Strange Mound** in Helgen's ruins: a heap of burnt flesh. It appears (placed by the plugin, no edit to the
  world) once a player who isn't yet a shapeshifter comes near Helgen, as long as the opening quest (Unbound) isn't
  running.
- "Search" it: tentacle sounds, a burst of blood, the mound poisons the player, a shapeshift shadow passes over them,
  and they gain **Unbound Form** for free. The mound then shrivels away. **Verified** (a new character, Realm of
  Lorkhan start).
- Not yet done: making the mound depend on Helgen's ruins existing rather than on the opening quest (a safety for
  other alternate starts).
- **Console fallback: "Spell Tome: Shapeshifter's Instinct"** (kept on purpose, 2026-11-18). Nothing places it in
  the world and there's no recipe; it's for players whose load order breaks Helgen or the Strange Mound, added with
  the console. Reading it teaches **only** the Shapeshifter's Instinct power (consuming and the forms); it doesn't give
  Unbound Form, so the player's first perk point goes on Unbound Form, which the Mound gives for free. Tried in game
  2026-11-25: Evolution appears with the first perk point (first body consumed).

## 4. Consuming bodies

- Power **Shapeshifter's Instinct** (given with Unbound Form): *"Sneak and activate a body to consume it (in animal
  form, just activate it). Consuming an animal teaches its form. Cast to recall the forms you know."*
- Consuming takes 2 seconds (1 second in combat).
- Every body restores **50 Stamina** at once and heals **20%** of max Health (**50%** with Survivor rank 2).
- **Consuming an animal teaches its form.** There are **no** special items to eat: the old plan of a unique heart or
  item per creature was dropped.
- Consuming a **person** that someone sees is a crime: **1,000 bounty**, witnesses within about 43 m.
- **Draugr** can be consumed but give no evolution points.
- **Not edible:** children, skeletons, ghosts and other undead without flesh (vampires and draugr are edible),
  Dwarven machines, and **dragons** (a dead dragon is bones; keyword ActorTypeDragon, so Alduin and the DLC dragons
  too; 2026-11-25).
- With **Borrowed Visage**, consuming a person takes their face. With **Assassin**, an unseen consume goes unnoticed.

## 5. Evolution points and perk points

- Consuming gives **evolution points** by the body's size: small animal **1**, medium animal **2**, large **3**,
  Mammoth/Giant size **5**, a person **2**, a draugr **0**. Creatures with no form (spriggans, Hagravens and so on)
  count by height (1, 2, 3 or 5).
- **Perk points cost 1 more each time:** the 1st costs 1 evolution point, the 2nd 2, the 3rd 3, and so on (totals
  1, 3, 6, 10, 15...). Each body tells you how many points remain until the next perk point.
- **Renowned people** each give **one perk point directly**, the first time each is consumed (21): Ulfric Stormcloak,
  Galmar, General Tullius, Legate Rikke, Mercer Frey, Titus Mede, Astrid, Krev the Skinner, Ancano, Madanach, Thonar,
  Calixto, Movarth, Sinding, Silus Vesuius, Tyranus, Harkon, Vyrthur, the Ebony Warrior, Ralis Sedarys, Ildari.
- Power **Evolution** opens the Protean perk tree: *"Look into what you are becoming: spend Protean perk points,
  earned by feeding."* The tree's own text: *"New perks come with consuming. Perk points take a little more biomass
  as you go."* It is given with the **first evolution point** (or a renowned person's point, or a perk owned), not
  before: a spell-tome player gets it on their first body (`src/Evolve.cpp`, the check after each change).
- **Kills** (not bodies) add a **biomass boost** with Unbound Form: +15% Stamina recovery for 90 seconds per kill,
  stacking with no limit. Devastator kills give none.
- With Unbound Form, Stamina is **never slowed**: no combat slowdown and no pause before it recovers.

## 6. Shapeshifting

- **35 forms**, one power each (**"Shapeshift: <Animal>"**). Cast it again or use **Revert Form** to change back.
- Changing costs **15 Stamina** and takes **1.5 seconds**: a shimmer, the Ethereal and Vampire Lord transformation
  sounds, and a burst of blood.
- **Health:** your Health keeps its *percentage* across a change, both ways, so changing back can't kill you. Animal
  form adds 25% of your own max Health, plus 25% of however much more Health the animal has than you. Survivor raises
  this (section 8).
- **Controls:** A/S/D steer and the mouse turns the camera. Tap left/right mouse for the animal's attacks, and hold for
  power attacks. Sprint costs Stamina (7 a second) for animals that can sprint. Camera switching, inventory and magic
  menus are locked, as with the werewolf.
- **Where:** small and medium animals go anywhere. **Troll, Frost Troll, Horker:** outdoors, or indoors only in
  cave-like places (dungeons, mines, caves, lairs, ruins...). **Mammoth, Giant:** outdoors only.
- **Changing back in the air** (flying) is refused once; cast again within 10 seconds to insist.
- **Witnesses:** people (not animals) who see you change know it was you, and so do people who see you change back.
  Crimes committed as an animal are held against you only by those who saw you change. Guards attack werewolves and
  werebears on sight, as in vanilla.

**The 35 forms** (size class: where they may go; diet: what Survivor rank 2 lets them eat):

| Form | Size | Diet | Special |
|---|---|---|---|
| Wolf, Ice Wolf, Pit Wolf | medium | meat | |
| Deer, Elk | medium | plants | |
| Fox | small | both | |
| Hare | small | plants | Jump = leap |
| Dog | medium | both | |
| Goat, Cow | medium | plants | |
| Bear, Cave Bear, Snow Bear | medium | both | |
| Sabre Cat, Snowy Sabre Cat, Vale Sabre Cat | medium | meat | |
| Death Hound | medium | meat | Dawnguard |
| Boar | medium | both | Dragonborn |
| Skeever, Chicken | small | both | |
| Ash Hopper | small | plants | Jump = a long leap (Dragonborn) |
| Mudcrab | small | both | |
| Chaurus | medium | meat | |
| Troll, Frost Troll | large: caves allowed | meat | |
| Horse | medium | plants | |
| Horker | large: caves allowed | meat | |
| Mammoth | outdoors only | plants | |
| Giant | outdoors only | both | fights bare-handed |
| Werewolf | medium | meat | guards attack on sight |
| Werebear | medium | both | guards attack on sight |
| Slaughterfish | small | meat | swims and breathes water |
| Chaurus Hunter, Chaurus Hunter Fledgling | medium | meat | fly: Jump takes off, 15 Stamina (Dawnguard) |
| Frostbite Spider | medium | meat | spits poison (right mouse) |

## 7. The weapons (all from perks)

| Weapon | Perk | Hands | Skill | Stamina a swing | Notes |
|---|---|---|---|---|---|
| Rending Claws | Rending Claws | either or both | One-Handed; counts as a sword (blade) for perks | 5 | full damage at full Stamina, about a third when empty |
| Hammerfist | Hammerfist | either or both | One-Handed (a mace) | 7 | damage 1.25x the claws'; strongest at full Stamina |
| Blade | Blade | right; left hand must stay free | Two-Handed | 8 | damage 1.7x the claws'; blocks |
| Shield | Shield | left forearm | Block | 5 a blocked hit | blocks everything from the front, spells too |
| Tendril | Tendril | either (a spell) | | 25 a lash, 3 s cooldown | a miss refunds it; range about 21 m |

The Hammerfist, Blade, Shield and Tendril grow out of the arm when drawn, each with its own flesh "sleeve"; the
claws appear as the draw finishes. Drawing and sheathing play a flesh-stab sound.

## 8. The perk tree (25 perks, 21 stars)

Three branches grow from **Unbound Form**. On the menu: **Beast** on the left, **Hunter** in the middle,
**Deceiver** on the right. Survivor, Predator, Stalker and Subtle Carapace have **2 ranks**. All 25 are built (none
are placeholders).

### Root
- **Unbound Form**: *"Take the form of any animal you have consumed. Immunity to all diseases. Every kill you make
  boosts your Stamina recovery for a while (15% for 90 seconds, stacks)."* (The text said "every body you consume"
  until 2026-11-25; the boost has always come from kills.)

### Beast branch (left)
- **Keen** (after Unbound Form): *"Enhance your senses while wearing an animal's form. Open Favorites to access night
  vision, Detect Life, and Detect Dead (the Detect spells drain Stamina while on)."* Drain: 2 Stamina a second.
  Verified.
- **Survivor** (after Keen). Rank 1: *"Increase the amount of Health you gain when wearing an animal's shape. Tougher
  animals give higher bonuses."* Rank 2: *"Even more Health in animal form, plus all of a tougher animal's extra
  Health, and you always arrive at full Health. Consuming a body heals half your Health, and as a herbivore or an
  omnivore you can eat plants to heal."* Verified.
- **Predator** (after Keen). Rank 1: *"Your animal forms deal more damage, 2% more for each level you have (at most
  double)."* Rank 2: *"Your animal forms' power attacks also poison, 5 damage a second plus 0.3 per level, for 5
  seconds. The undead and Dwarven machines are immune."* Verified.
- **Stalker** (after Keen). Rank 1: Stalk, toggled from Favorites; fade after standing still or walking for 2
  seconds out of a fight; moving drains 2 Stamina a second (running double); a bite from hiding does double damage.
  Rank 2: Stalk works in a fight once no enemy can see you; enemies give up after 3 seconds. Rank 1 verified; rank 2
  verified in game 2026-11-25.
- **Apex** (needs Survivor, Predator and Stalker): *"Animals treat you as one of their own in any form, your animal
  forms hit at least as hard as a two-hander of your level, and in animal form you can call a pack of your kind to
  fight for you."* Power **Call**: a pack for 3 minutes (60-second cooldown). Verified (including Call in caves).
  - **Kin is an invisible aura** (since 2026-11-19, user's choice): every animal of the creature table within 4000
    units gets a hidden Calm (like Kyne's Peace, no visible effect) each second. Left alone: the Call pack and other
    followers, summons, animals in their own fight (wolves still hunt deer), and any animal the player hit in the last
    60 s (it fights back). Verified: animals near the player live and ignore them. Before, Apex put the player in
    every animal's faction, which made Forsworn (goat), draugr, Falmer, skeletons, vampires (skeever, spider) and
    hagravens (chaurus, troll) friendly; that is taken off once, in human form. In animal form the player still
    joins that animal's own factions, as a real one has them.

### Hunter branch (middle)
- **Rending Claws** (after Unbound Form): *"Claws grow from your hands, a weapon for each hand. They strike hardest
  when your Stamina is full, and every swing costs Stamina."* They train One-Handed and count as a sword (blade) for
  perks. Verified.
- **Hammerfist** (after Rending Claws): *"Your arm swells with muscle into a huge fist armoured in chitin and spiked
  with bone, a weapon for each hand. It fights as a mace with the One-Handed skill and perks, strikes hardest when
  your Stamina is full, and every swing costs Stamina."* Verified (both arms, 2026-11-19).
- **Blade** (after Rending Claws): *"Draw the Blade and your right arm becomes one: flesh grows over it and a great
  bone blade bursts from the forearm. It fights with the Two-Handed skill and perks, blocks, and needs your left hand
  free. Every swing costs Stamina."* Verified.
- **Tendril** (after Rending Claws): *"Learn Tendril, a whip of flesh cast from either hand: it tears at health,
  drains Stamina into you, yanks light foes to you and hauls you to large ones. Each lash costs Stamina."* Verified.
- **Predator's Rush** (after Unbound Form): *"In human form you move faster and jump far higher, and a held sprint
  keeps building speed until it is dangerously fast. Sprinting still costs Stamina."* +10% on foot; sprint builds from
  1.25x to 3x over 10 seconds. Verified.
- **Prey's Flight** (after Predator's Rush): *"No fall damage. In human form, jump again in mid-air, hold Jump to
  glide, and power attack in the air to dive and stagger everyone around your landing."* Mid-air jump 15 Stamina,
  glide 5 a second, dive slam 30. Verified.
- **Subtle Carapace** (after Unbound Form). Rank 1: *"A hidden carapace hardens your skin: 50 armor plus 4 per level
  and 10% magic resistance, in animal form, or in your own when you wear no body armor (clothes and robes are
  fine)."* Rank 2: *"The carapace hardens further: 100 armor plus 7 per level and 20% magic resistance, under the
  same rule."* Hide, leather and similar "natural" armor also counts as no body armor. Verified (magic resistance
  2026-11-19).
- **Shield** (after Subtle Carapace): *"A shield of dark chitin grows from your left forearm while your weapons are
  drawn. Blocking with it stops all damage from the front, spells too, for 5 Stamina a hit. With no Stamina left it
  blocks nothing."* The shield appears in the showcase videos. Verified 2026-11-19: melee from within the game's
  block angle blocked for 5 Stamina, from the side or behind it lands; arrows and bolts from within 60 degrees are
  blocked by Protean's own rule (the Shield has no collision for the game's own arrow block), 5 Stamina each (an
  arrow passes the hit code twice; charged once). Spells: the ward. All verified in game 2026-11-25.
- **Regeneration** (after Subtle Carapace): *"Cast Regeneration to switch it on or off: while on, Stamina drains and
  Health returns by the same amount."* 8 a second. Verified.
- **Devastator** (needs Rending Claws, Prey's Flight and Subtle Carapace): *"In human form, with full Stamina and at
  least two biomass boosts, cast Devastator: tendrils burst from your body, knocking down and tearing into every enemy
  within 30 meters. Uses all your Stamina."* 100 damage + 8 per level. Verified.

### Deceiver branch (right)
- **Borrowed Visage** (after Unbound Form): *"Consume a person to take their face, body and race, and join their
  factions (consumed in animal form, the face is kept for later). Cast Stolen Face to put the face on or take it off;
  putting it on costs Stamina, like changing form. Anyone who sees the theft turns hostile."* Power **Shapeshift:
  Stolen Face**. Verified.
  - **The true face** (2026-11-19): the player's own look (head parts, sliders, hair colour, skin, every colour layer
    with its texture and label) is kept from the moment Protean is known, and retaken whenever RaceMenu closes
    (Metamorphosis included). It lives in the co-save and in `<SKSE log folder>/Protean-faces/<name>-<id>.txt`.
    Taking a stolen face off paints the true face back; after every load and every change back from an animal the
    face is compared with it and any drift is repaired, logged in detail and shown on screen ("Protean: your true face
    was restored..."). Not checked while the race differs (a vampire). Own colour layers are matched by texture only,
    after the colour-mask list is rebuilt for the look's race and sex (the game keeps the old list on a race or sex
    change). Verified both ways (male player in a female face, female player in a male face), including a save and load in
    the face.
    Cause it guards against: a save stores colour layers only as race-asset numbers; after loading a save made in a
    stolen face, the old position-based restore moved Mercer's war paint, eyeliner and dirt (drifts at Save33 and
    Save58). Dev: `tintdump/tintrestore <name>`, `truetake`, `truetints <dump>`, `truecheck`.
- **Metamorphosis** (after Borrowed Visage): *"Out of a fight, in your own face and with full Stamina, reshape
  yourself: choose a new race, face and body, keeping your skills and perks. Every bounty on you is cleared. Uses all
  your Stamina."* Verified.
- **Inconspicuous** (after Borrowed Visage): *"In human form, every friendly person within 15 meters adds 5 to your
  Sneak, up to 30: the bigger the crowd, the harder you are to notice."* Verified.
- **Infected** (after Inconspicuous): *"Cast Infect to grow an infection (costs Stamina), then slip it into someone's
  pocket: for a day they fight at your side. Only one person carries it at a time."* 50 Stamina, 24 hours. Verified
  (2026-11-19).
- **Assassin** (after Inconspicuous): *"In human form, sneak attacks do ten times the damage to anyone not hostile to
  you. Consume someone unseen and it goes unnoticed: no crime, the alarm is called off, and their face goes on free.
  Stealth Consume: for a minute, melee sneak attack kills are consumed at once."* Power **Stealth Consume**: 25
  Stamina, 60 seconds. Verified.

## 9. Other powers and extras

- **Revert Form**: *"Return to your own form."*
- **Keen's senses** (Favorites in animal form): Night Vision, Detect Life, Detect Dead.
- **Evolution journal page** (needs SKSE Menu Framework): shows the most-consumed creatures and progress, with
  switches for "Ask first" and whether Stealth Consume takes the loot.

## 10. Settings file (`Protean.ini`, re-read at every change)

- `[Camera]`: auto camera height, shoulder offset, starting zoom.
- `[Attacks] Translate=1`: the mouse buttons fire the animal's own attacks.
- `[Face]`: `Enabled`, `JoinFactions` (wearing a face puts you in that person's factions), `TakeOffOnMenu`,
  `WitnessRange` (3000, about 43 m).
- `[Dev] Commands`: developer channel. **0 in the release package** (1 in the working copy).

## 11. Problems found in the mod itself (not just the docs)

1. ~~Unbound Form's description~~ (said bodies, not kills): fixed 2026-11-25.
2. ~~"a ash hopper", "a elk", "a ice wolf"~~: fixed 2026-11-25.
3. **The spell tome** gives less than the Strange Mound (no free Unbound Form). Kept that way on purpose (decided
   2026-11-18) as the console fallback.
4. **The package ships `docs/Protean-INTEGRATION.md` dated 2026-11-03**, older than the perks added since (Hammerfist,
   Shield, Blade, the sleeves, the Strange Mound).

## 12. Still to verify in game (a short checklist)

1. ~~**Shield**~~: verified 2026-11-25 (spells from the front warded, 5 Stamina a hit; melee and arrows 2026-11-19).
2. ~~**Stalker rank 2**~~: verified 2026-11-25 in game.
4. **Dragons not edible** (2026-11-25): a dead dragon opens its loot window, with no consume prompt.
3. ~~**The spell tome**~~: verified 2026-11-25 (read it, consumed a deer, Evolution appeared with the perk point).


Verified 2026-11-19: Infected, Subtle Carapace magic resistance, Hammerfist left arm.

## 13. Decisions recorded

- 2026-11-18: Unbound Form's boost comes from **kills**; its description was corrected 2026-11-25.
- 2026-11-18: the spell tome **stays** as the console way in when the Strange Mound can't load, and keeps giving only
  Shapeshifter's Instinct (not made to match the Mound).
- 2026-11-18: Rending Claws train **One-Handed** and count as a **sword (blade)** for perks (confirmed by the author).
