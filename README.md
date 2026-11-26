# Protean

Become a shapeshifter for Skyrim SE **1.7.104**. Consume the dead to grow stronger, take the form of any animal
you have consumed, steal the faces of the people you devour, and grow claws, blades and tendrils from your own
flesh. Inspired by the Prototype games.

Version **1.1.0**. 35 animal forms, 21 perks in three branches, 5 weapons.

## How to start

Go to the ruins of **Helgen**. Once you're past the opening, a **Strange Mound**, a heap of burnt flesh, waits
there. Search it. It poisons you and a shadow passes over you: you gain **Unbound Form** and the power
**Shapeshifter's Instinct**.

## Consuming

- **Sneak and activate a body to consume it** (in animal form, just activate it). It takes 2 seconds (1 in combat).
- Every body restores **50 Stamina** and heals **20%** of your Health.
- **Consuming an animal teaches you its form.**
- Dragons, skeletons, ghosts and Dwarven machines can't be consumed: there's no flesh left to take.
- Consuming a person in front of witnesses is a crime (1,000 bounty).

## Evolving

- Bodies give **evolution points** by their size: small animals 1, medium animals and people 2, large animals 3,
  mammoths and giants 5 (draugr give nothing).
- Evolution points become **perk points**, each costing one more than the last (1, 2, 3...).
- **21 renowned people** each give a perk point outright the first time you consume them: Ulfric Stormcloak,
  General Tullius, Mercer Frey, Harkon, Ancano, the Ebony Warrior and more.
- Cast **Evolution** to open the Protean perk tree.

## Shapeshifting

- **35 forms**, each with its own power: wolves (3), deer, elk, fox, hare, dog, goat, cow, bears (3), sabre cats (3),
  Death Hound, boar, skeever, chicken, ash hopper, mudcrab, chaurus, trolls (2), horse, horker, mammoth, giant,
  werewolf, werebear, slaughterfish, Chaurus Hunters (2) and the frostbite spider.
- Changing costs **15 Stamina** and takes 1.5 seconds. Cast the power again, or use **Revert Form**, to change back.
- Your Health keeps its percentage when you change, so changing back can't kill you. Animal forms add bonus Health,
  more for tougher animals.
- **Controls:** A/S/D steer and the mouse turns the camera. Tap the mouse buttons for the animal's attacks, hold for
  power attacks. Sprinting costs Stamina.
- **Specials:** the slaughterfish swims and breathes water; the Chaurus Hunters fly (Jump to take off); the hare and
  ash hopper leap; the frostbite spider spits poison (right mouse); the giant fights bare-handed.
- **Where:** small and medium animals go anywhere. Trolls and horkers fit into caves, mines and ruins but not
  buildings. Mammoths and giants stay outdoors.
- **The world:** people who see you change know it was you. Guards attack werewolves and werebears on sight.

## The perk tree

Three branches grow from **Unbound Form** (immunity to disease, and every kill speeds up your Stamina recovery for a
while). Four perks have two ranks.

**Beast:** your animal forms.
- **Keen:** night vision, Detect Life and Detect Dead from an animal's Favorites.
- **Survivor:** more Health in animal form; rank 2 makes consuming a body heal half your Health (instead of 20%)
  and lets plant-eaters graze to heal.
- **Predator:** animal attacks that grow with your level; rank 2 poisons with power attacks.
- **Stalker:** fade from sight when still or walking, and strike from hiding for double damage.
- **Apex:** animals treat you as one of their own, your animal forms hit at least as hard as a two-handed weapon of
  your level, and you can **call a pack** to fight for you.

**Hunter:** weapons, movement and toughness.
- **Rending Claws:** claws in either hand. They use One-Handed and count as swords for perks.
- **Hammerfist:** a huge armoured fist (One-Handed, as a mace).
- **Blade:** your right arm becomes a great bone blade (Two-Handed).
- **Tendril:** a whip of flesh that tears at Health, drains Stamina, and yanks foes to you or hauls you to big ones.
- **Predator's Rush:** faster on foot, far higher jumps, and a sprint that keeps building speed.
- **Prey's Flight:** no fall damage, a jump in mid-air, gliding, and a diving slam.
- **Subtle Carapace:** armor and magic resistance that grow with your level, in animal form or with no body armor.
- **Shield:** a chitin shield on your left forearm that blocks everything from the front, spells too.
- **Regeneration:** turn Stamina into Health.
- **Devastator:** tendrils burst from your body, knocking down and tearing into every enemy within 30 meters.

**Deceiver:** faces and stealth.
- **Borrowed Visage:** consume a person to take their face, body and race, and join their factions.
- **Metamorphosis:** reshape your own race, face and body, and clear every bounty.
- **Inconspicuous:** friendly crowds make you harder to notice.
- **Infected:** plant an infection in someone's pocket and they fight at your side for a day.
- **Assassin:** ten-times sneak attacks on the unsuspecting, unseen consuming, and **Stealth Consume**.

Most weapons and powers cost Stamina. Protean is built around keeping your Stamina bar full.

## Requirements

- Skyrim SE **1.7.104** (the version Protean is built and tested on)
- SKSE64
- Address Library for SKSE Plugins
- Dawnguard and Dragonborn
- The monster race crash fix: https://www.nexusmods.com/skyrimspecialedition/mods/19899
- Custom Skills Framework (the perk tree)
- Open Animation Replacer (the weapon animations and the glide)

**Optional:** SKSE Menu Framework (the Evolution journal page, with a few switches).

**If an enchantment stops working after you change back** (the item is worn but its effect is missing until you
re-equip it), install **Equip Enchantment Fix** (the "1.6.629 and newer" build). Protean puts your gear back on for
you, and gear equipped by a mod rather than by hand is where the game sometimes drops enchantments. It was tested
without the fix and nothing was lost, so it is not required.

**A crash log every time you quit the game?** That comes from Open Animation Replacer, not Protean (it happens
with OAR alone, even quitting from the main menu). It is harmless, but to stop it: open
`SKSE/Plugins/OpenAnimationReplacer.ini` (in Mod Organizer it is usually in the **Overwrite** folder) and set
`bLoadDefaultBehaviorsInMainMenu = false`. Protean's animations work the same with it off.

## Credits

Protean is a new build on the ideas of these mods. No files or scripts from them are reused.

- **Shapeshifting** by PeenCoach (Nexus SE 107629): the transform-by-race-swap approach, the werewolf-style
  "no inventory or spells, press again to revert" design.
- **Shapeshift - Hare** by Bobiguy (Nexus SE 142327): unequip-everything handling across armor slots.
- **Pull Mastery** by xzax (Nexus SE 35134) and **Xyn's Simple Rope Arrow** (Nexus SE 77707): the ideas behind
  Tendril (a long model fired as a flame-type projectile; a negative knock that pulls the target in). Studied only;
  Tendril's model, records and code are Protean's own.
- **Feral** (Nexus SE 34552): showed that the game's own beast-race animations make good claw attacks. Protean takes
  them from the game's archive, not from Feral.
- **Skyrim's Paraglider** (Nexus SE 53256, MIT): how a glide eases in, and resetting the fall to avoid fall damage.
  Studied only.
- **PyNifly** by BadDog (GPL-3.0): its HKX writer is used at build time to write Protean's own glide animation.
  None of its code ships with Protean.

## Permissions

- **Code** (the SKSE plugin's source, the build tools and the plugin they generate): MIT licence. Use it, change it
  and share it, keeping the copyright notice. Source: https://github.com/NiamosStyle/ProteanSkyrimMod
- **Art** (meshes, textures, animations and their Blender sources): please **ask first** before using them in
  another mod. Ask through Nexus. Parts made from Skyrim's own files remain Bethesda's.
- Translations and patches are welcome. Please tell me and credit Protean.

**For mod authors:** `docs/Protean-INTEGRATION.md` lists everything a larger mod can read, listen to and tune.
