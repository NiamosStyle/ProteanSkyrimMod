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
HAMMERFIST_WEAPON = "A13"  # Protean.esp, tools/build_plugin.py HAMMERFIST_WEAPON
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

# The Blade (the user, 2026-11-04): it grows from the arm, so drawing and sheathing it must not reach across the body to
# the hip as for a sword. While the right hand holds it, the sword's draw and sheathe are the empty hands' own (the arm
# just comes up, as with the claws); its attacks, blocking and stance stay the sword's.
BLADE_WEAPON = "A02"  # Protean.esp, tools/build_plugin.py BLADE_ONEHAND
# The draw is the bound sword's (conjured into the hand: no reach either). The empty hands' draw carries no
# BeginWeaponDraw / weaponDraw signals, and without them the draw sometimes took 2 seconds to count as done (user,
# 2026-11-04); the bound sword's has both, in 3rd and 1st person. The sheathe stays the empty hands' (instant).
BLADE = {"1hm_equip.hkx": "1hm_boundswordequip.hkx", "1hm_unequip.hkx": "h2h_unequip.hkx"}
# The empty hands' sheathe carries none of the signals the sword's sheathe waits for (BeginWeaponSheathe,
# weaponSheathe), so the game sat in "sheathing" until a fallback timer ran out and the next draw waited for it
# (the user, 2026-11-04: still several seconds after pressing R). The Blade's copy gets them at its start.
SIGNALS = {"1hm_unequip.hkx": [(0.0, "BeginWeaponSheathe"), (0.03, "weaponSheathe")]}
PYNIFLY_HKX = "C:/Games/steamapps/common/Skyrim Special Edition/tools/pynifly/io_scene_nifly/hkx"


def with_signals(raw, signals):
    """The clip raw (bytes of a game .hkx) with the annotations added; uses PyNifly's HKX module (GPL-3.0, only run
    here, never shipped), as tools/build_glide_anim.py does."""
    import tempfile
    if PYNIFLY_HKX not in sys.path:
        sys.path.insert(0, PYNIFLY_HKX)
    import anim_skyrim as hkx
    from anim_fo4 import Annotation
    with tempfile.TemporaryDirectory() as tmp:
        path = os.path.join(tmp, "clip.hkx")
        with open(path, "wb") as f:
            f.write(raw)
        anim = hkx.load_skyrim_animation(path)
        anim.annotations = list(anim.annotations) + [Annotation(time=t, text=x) for t, x in signals]
        anim.annotations.sort(key=lambda a: a.time)
        hkx.write_skyrim_animation(path, anim, ptr_size=8)  # SE: 64-bit
        back = hkx.load_skyrim_animation(path)
        assert [(round(a.time, 2), a.text) for a in back.annotations] == [(round(t, 2), x) for t, x in sorted(signals)], back.annotations
        with open(path, "rb") as f:
            return f.read()
SUBMODS = ("RendingClaws", "Blade", "HammerfistRight", "HammerfistLeft", "HammerfistBoth")
# The Hammerfist's power attacks come down to the ground (2026-11-13; tools/build_hammer_anims.py): a hand's own power
# attack while that hand holds a Hammerfist, the both-hands one while both do. Above Rending Claws' set (15100).
HAMMER_SUBMODS = (
    ("HammerfistRight", "Hammerfist right", 15102, (False,), "h2h_attackpowerforwardrighthand.hkx"),
    ("HammerfistLeft", "Hammerfist left", 15102, (True,), "h2h_attackpowerforwardlefthand.hkx"),
    ("HammerfistBoth", "Hammerfist both hands", 15103, (False, True), "h2h_attackrightpowerforward.hkx"),
)


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
            json.dump({"name": "Protean", "description": "Rending Claws and Blade animations (%s person)" % ("1st" if person else "3rd"),
                       "author": "Protean"}, f, indent=2)
        with open(os.path.join(dst, "config.json"), "w", encoding="utf-8") as f:
            # either hand: with a claw in the left hand alone, the clawed hand made a fist (the user, 2026-10-23)
            # the Hammerfist plays the same beast-race set (2026-11-05): its huge fists hang low there, clear of the
            # face, where the human unarmed guard holds them by the head (tools/measure_fist_poses.py)
            claw = [{"condition": "IsEquipped", "requiredVersion": "1.0.0.0",
                     "Form": {"pluginName": "Protean.esp", "formID": form}, "Left hand": left}
                    for form in (CLAW_WEAPON, HAMMERFIST_WEAPON) for left in (False, True)]
            json.dump({"name": "Rending Claws", "description": "Claw attacks while either hand holds Rending Claws or a Hammerfist.",
                       "priority": 15100,
                       "conditions": [{"condition": "OR", "requiredVersion": "1.0.0.0", "Conditions": claw}]},
                      f, indent=2)
        written += [rel + "/config.json", rel + "/RendingClaws/config.json"]
        for name, source in names.items():
            with open(os.path.join(dst, name), "wb") as f:
                f.write(bsa.read(ANIMATIONS, "meshes/actors/character/" + person + "animations/" + source))
            written.append(rel + "/RendingClaws/" + name)
        print("wrote", len(names), "animations to", dst)
        dst = os.path.join(root, "Blade")
        if os.path.isdir(dst):
            shutil.rmtree(dst)
        os.makedirs(dst)
        with open(os.path.join(dst, "config.json"), "w", encoding="utf-8") as f:
            json.dump({"name": "Blade", "description": "Draw and sheathe like empty hands while the right hand holds the Blade.",
                       "priority": 15101,
                       "conditions": [{"condition": "IsEquipped", "requiredVersion": "1.0.0.0",
                                       "Form": {"pluginName": "Protean.esp", "formID": BLADE_WEAPON}, "Left hand": False}]},
                      f, indent=2)
        written.append(rel + "/Blade/config.json")
        for name, source in BLADE.items():
            raw = bsa.read(ANIMATIONS, "meshes/actors/character/" + person + "animations/" + source)
            if name in SIGNALS:
                raw = with_signals(raw, SIGNALS[name])
            with open(os.path.join(dst, name), "wb") as f:
                f.write(raw)
            written.append(rel + "/Blade/" + name)
        print("wrote", len(BLADE), "animations to", dst)
        import build_hammer_anims
        made = build_hammer_anims.clips(person)
        for sub, title, prio, hands, name in HAMMER_SUBMODS:
            dst = os.path.join(root, sub)
            if os.path.isdir(dst):
                shutil.rmtree(dst)
            os.makedirs(dst)
            with open(os.path.join(dst, "config.json"), "w", encoding="utf-8") as f:
                json.dump({"name": title, "description": "The Hammerfist's power attack comes down to the ground.",
                           "priority": prio,
                           "conditions": [{"condition": "IsEquipped", "requiredVersion": "1.0.0.0",
                                           "Form": {"pluginName": "Protean.esp", "formID": HAMMERFIST_WEAPON},
                                           "Left hand": left} for left in hands]}, f, indent=2)
            with open(os.path.join(dst, name), "wb") as f:
                f.write(made[name])
            written += [rel + "/" + sub + "/config.json", rel + "/" + sub + "/" + name]
        print("wrote the Hammerfist's power attacks to", root)
    return written


if __name__ == "__main__":
    files = build()
    if len(sys.argv) > 1:
        for person in ("", "_1stperson/"):
            for sub in SUBMODS:
                target = os.path.join(sys.argv[1], "meshes/actors/character", person + OAR, sub)
                if os.path.isdir(target):
                    shutil.rmtree(target)  # nothing left over from an older set
        for rel in files:
            target = os.path.join(sys.argv[1], rel)
            os.makedirs(os.path.dirname(target), exist_ok=True)
            shutil.copy2(os.path.join(ROOT, "data", rel), target)
        print("installed", len(files), "files in", sys.argv[1])
