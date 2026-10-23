"""DEV ONLY: give Rending Claws the claw-attack animations, by copying Feral's (Nexus 34552) restored beast-race
animations into the installed Protean mod folder under an Open Animation Replacer submod of our own.

  python -I tools/install_claw_anims.py "<MO2 mod folder of Protean>"

The animation files are Feral's (restored from the game's own data), so they are not kept in this project or packed
by tools/package.py. Before a release, take them from the game archives instead (tools/bsa.py reads them).
Feral's 1st-person set has no power attacks forward, so those come from the game's own beast-race (Khajiit and
Argonian) claw animations, renamed to the hand-to-hand names OAR replaces.
"""
import json
import os
import shutil
import sys

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
import bsa  # noqa: E402

FERAL = "C:/Users/user/AppData/Local/ModOrganizer/Skyrim Special Edition/mods/Feral/meshes/actors/character"
CLAW_WEAPON = "980"  # Protean.esp, tools/build_plugin.py CLAW_WEAPON
ANIMATIONS = "C:/Games/steamapps/common/Skyrim Special Edition/Data/Skyrim - Animations.bsa"
FROM_GAME_1ST = {"h2h_attackpowerforwardlefthand.hkx": "beasth2h_attackpowerforwardlefthand.hkx",
                 "h2h_attackpowerforwardrighthand.hkx": "beasth2h_attackpowerforwardrighthand.hkx"}

mod = sys.argv[1]
for person in ("", "_1stperson/"):
    src = os.path.join(FERAL, person + "animations/OpenAnimationReplacer/Feral/ClawAttacks")
    root = os.path.join(mod, "meshes/actors/character", person + "animations/OpenAnimationReplacer/Protean")
    dst = os.path.join(root, "RendingClaws")
    os.makedirs(dst, exist_ok=True)
    with open(os.path.join(root, "config.json"), "w", encoding="utf-8") as f:
        json.dump({"name": "Protean", "description": "Rending Claws attack animations (%s person)" % ("1st" if person else "3rd"),
                   "author": "NiamosStyle mods"}, f, indent=2)
    with open(os.path.join(dst, "config.json"), "w", encoding="utf-8") as f:
        json.dump({"name": "Rending Claws", "description": "Claw attacks while the right hand holds Rending Claws.",
                   "priority": 15100,
                   "conditions": [{"condition": "IsEquipped", "requiredVersion": "1.0.0.0",
                                   "Form": {"pluginName": "Protean.esp", "formID": CLAW_WEAPON}, "Left hand": False}]},
                  f, indent=2)
    n = 0
    for name in os.listdir(src):
        if name.lower().endswith(".hkx"):
            shutil.copy2(os.path.join(src, name), os.path.join(dst, name))
            n += 1
    if person:
        for name, source in FROM_GAME_1ST.items():
            with open(os.path.join(dst, name), "wb") as f:
                f.write(bsa.read(ANIMATIONS, "meshes/actors/character/_1stperson/animations/" + source))
            n += 1
    print("copied", n, "animations to", dst)
