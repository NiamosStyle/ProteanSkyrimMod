"""Give Rending Claws the claw-attack animations: the game's own beast-race (Khajiit and Argonian) hand-to-hand
animations, renamed to the hand-to-hand names Open Animation Replacer swaps in while a hand holds the claws.

  python -I tools/build_claw_anims.py ["<MO2 mod folder of Protean>"]

Writes data/meshes/actors/character/[_1stperson/]animations/OpenAnimationReplacer/Protean/ (an OAR mod with one
submod, RendingClaws) and, given the mod folder, copies it there too. tools/package.py packs it.
The animation files are read from the game's archive (tools/bsa.py) and not kept in git (Bethesda data).
History: until 2026-10-23 they were copied from Feral (Nexus 34552); every one of those is a byte-for-byte copy of a
game file named in THIRD/FIRST below. Feral's 1st-person set lacked the forward power attacks; those come from the
game's too.
"""
import json
import os
import shutil
import sys

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
import bsa  # noqa: E402

ROOT = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
ANIMATIONS = "C:/Games/steamapps/common/Skyrim Special Edition/Data/Skyrim - Animations.bsa"
CLAW_WEAPON = "980"  # Protean.esp, tools/build_plugin.py CLAW_WEAPON
OAR = "animations/OpenAnimationReplacer/Protean"
# the name OAR replaces -> the game's beast-race animation
THIRD = {
    "h2h_attackleft.hkx": "beasth2h_attackleft.hkx",
    "h2h_attackright.hkx": "beasth2h_attackright.hkx",
    "h2h_attackpowerforwardlefthand.hkx": "beasth2h_attackpowerforwardlefthand.hkx",
    "h2h_attackpowerforwardrighthand.hkx": "beasth2h_attackpowerforwardrighthand.hkx",
    "h2h_attackrightpowerforward.hkx": "beasth2h_attackpowercombo.hkx",
    "h2h_equip.hkx": "beasth2h_equip.hkx",
    "h2h_unequip.hkx": "beasth2h_unequip.hkx",
    "h2h_idle.hkx": "beasth2h_idle.hkx",
    "sneakh2h_idle.hkx": "beasth2h_idle.hkx",
    "h2h_locomotionpose.hkx": "beasth2h_locomotionpose.hkx",
}
for way in ("back", "forward", "left", "right"):
    for hand in ("left", "right"):
        THIRD["h2h_run%sattack%s.hkx" % (way, hand)] = "beasth2h_run%sattack%s.hkx" % (way, hand)
FIRST = {
    "h2h_attackleft.hkx": "beasth2h_attackleft.hkx",
    "h2h_attackright.hkx": "beasth2h_attackright.hkx",
    "h2h_attackpowerforwardlefthand.hkx": "beasth2h_attackpowerforwardlefthand.hkx",
    "h2h_attackpowerforwardrighthand.hkx": "beasth2h_attackpowerforwardrighthand.hkx",
    "h2h_attackrightpowerforward.hkx": "beasth2h_attackpowercombo.hkx",
    "h2h_equip.hkx": "beasth2h_equip.hkx",
    "h2h_unequip.hkx": "beasth2h_unequip.hkx",
    "h2h_idle.hkx": "beasth2h_idle.hkx",
    "sneakh2h_idle.hkx": "beasth2h_idle.hkx",
    "h2h_1stp_run.hkx": "beasth2h_idle.hkx",
    "h2h_1stp_walk.hkx": "beasth2h_idle.hkx",
    "h2h_locomotionpose.hkx": "beasth2h_locomotionpose.hkx",
}


def build():
    """Write the OAR folders under data/; returns the files written (relative to data/)."""
    written = []
    for person, names in (("", THIRD), ("_1stperson/", FIRST)):
        rel = "meshes/actors/character/" + person + OAR
        root = os.path.join(ROOT, "data", rel)
        dst = os.path.join(root, "RendingClaws")
        if os.path.isdir(dst):
            shutil.rmtree(dst)
        os.makedirs(dst)
        with open(os.path.join(root, "config.json"), "w", encoding="utf-8") as f:
            json.dump({"name": "Protean", "description": "Rending Claws attack animations (%s person)" % ("1st" if person else "3rd"),
                       "author": "Protean"}, f, indent=2)
        with open(os.path.join(dst, "config.json"), "w", encoding="utf-8") as f:
            json.dump({"name": "Rending Claws", "description": "Claw attacks while the right hand holds Rending Claws.",
                       "priority": 15100,
                       "conditions": [{"condition": "IsEquipped", "requiredVersion": "1.0.0.0",
                                       "Form": {"pluginName": "Protean.esp", "formID": CLAW_WEAPON}, "Left hand": False}]},
                      f, indent=2)
        written += [rel + "/config.json", rel + "/RendingClaws/config.json"]
        for name, source in names.items():
            with open(os.path.join(dst, name), "wb") as f:
                f.write(bsa.read(ANIMATIONS, "meshes/actors/character/" + person + "animations/" + source))
            written.append(rel + "/RendingClaws/" + name)
        print("wrote", len(names), "animations to", dst)
    return written


if __name__ == "__main__":
    files = build()
    if len(sys.argv) > 1:
        for person in ("", "_1stperson/"):
            target = os.path.join(sys.argv[1], "meshes/actors/character", person + OAR, "RendingClaws")
            if os.path.isdir(target):
                shutil.rmtree(target)  # nothing left over from an older set
        for rel in files:
            target = os.path.join(sys.argv[1], rel)
            os.makedirs(os.path.dirname(target), exist_ok=True)
            shutil.copy2(os.path.join(ROOT, "data", rel), target)
        print("installed", len(files), "files in", sys.argv[1])
