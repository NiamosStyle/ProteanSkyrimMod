"""Build the Hammerfist's models (2026-11-05; the user's brief: heavy and organic, mostly muscle, with bits of bone and
chitin in the fists; Prototype/Hammerfists_level_max.jpg and the Prototype 2 art). Shaped and painted in
art/hammerfist/Hammerfist.blend (make_base_h, make_flesh_h, make_fist, make_female_h, mirror_h, make_skin_h, bake_skin_h,
export_game_h), the way the Blade and the Shield were.

  python -I tools/build_hammerfist.py  -> data/meshes/Protean/Hammerfist/Arm_<R|L>_<M|F>.nif (3rd person),
                                          Arm1st_<R|L>_<M|F>.nif (1st person), Hammerfist.nif (empty),
                                          data/textures/Protean/Hammerfist/*.dds

A weapon for either hand, like the claws, with no model of its own: what is seen is one hidden sleeve per hand (the arm's
muscle from the shoulder down and the fist), put on by Protean while that hand holds a Hammerfist drawn. Right body slot
56, left 60. The fist hangs whole on the hand; the flesh keeps the game's arm weights.
"""
import json
import os
import subprocess
import sys

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
import build_blade  # noqa: E402
import first_person_cut  # noqa: E402
from build_blade import NifFile, SKELETONS, skinned  # noqa: E402
from pyn.pynifly import NiStringExtraData  # noqa: E402

ROOT = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
ART = os.path.join(ROOT, "art", "hammerfist")
BAKED = os.path.join(ART, "textures")
OUT = os.path.join(ROOT, "data", "meshes", "Protean", "Hammerfist")
TEX_OUT = os.path.join(ROOT, "data", "textures", "Protean", "Hammerfist")
TEX_GAME = "textures\\Protean\\Hammerfist\\"
SLOT = {"R": 56, "L": 60}
SPEC = {"Arm": 0.7, "Fist": 0.6}   # where no shine mask is baked
SHINE = {"Fist": (80.0, 1.0, (0.94, 1.0, 0.87))}   # the Shield's chaurus chitin shine
FIRST_PERSON_DROP = ("UpperArm", "UpperarmTwist", "Clavicle")


# the game's textures at most this wide: painted at 4096, the fist and shell came to ~22 MB each, most of the release
# zip; 2048, like the other sleeves', is hard to tell apart in game (2026-11-14)
MAX_SIZE = 2048


def build_textures():
    from PIL import Image, ImageChops
    tmp = os.path.join(BAKED, "_game")
    os.makedirs(tmp, exist_ok=True)
    os.makedirs(TEX_OUT, exist_ok=True)
    jobs = []
    for part in ("Arm", "Fist"):
        Image.open(os.path.join(BAKED, part + "_d.png")).convert("RGB").save(os.path.join(tmp, part + "_d.png"))
        jobs.append((part + "_d.png", "BC7_UNORM"))
        r, g, b = Image.open(os.path.join(BAKED, part + "_n.png")).convert("RGB").split()
        if build_blade.FLIP_GREEN:
            g = ImageChops.invert(g)
        a = Image.new("L", r.size, int(255 * SPEC[part]))
        mask = os.path.join(BAKED, part + "_s.png")
        if os.path.exists(mask):
            a = Image.open(mask).convert("L").resize(r.size)
        Image.merge("RGBA", (r, g, b, a)).save(os.path.join(tmp, part + "_n.png"))
        jobs.append((part + "_n.png", "BC7_UNORM"))
    for name, fmt in jobs:
        subprocess.run([build_blade.XTEXCONV, "-nologo", "-m", "0", "-f", fmt, "-w", str(MAX_SIZE), "-h", str(MAX_SIZE),
                        "-o", TEX_OUT, os.path.join(tmp, name)],
                       check=True, stdout=subprocess.DEVNULL)
    print("wrote", len(jobs), "textures to", TEX_OUT)


def textured(shape, part):
    shape.set_texture("Diffuse", TEX_GAME + part + "_d.dds")
    shape.set_texture("Normal", TEX_GAME + part + "_n.dds")
    if part in SHINE:
        sp = shape.shader.properties
        sp.Glossiness, sp.Spec_Str, sp.Spec_Color = SHINE[part]
    shape.save_shader_attributes()


def add_shape(nif, name, part, d, skel, slot, keep=None):
    tris = [t for t in d["tris"] if keep is None or all(keep(i) for i in t)]
    used = sorted({i for t in tris for i in t})
    new = {old: k for k, old in enumerate(used)}
    weights = {}
    for k, old in enumerate(used):
        for b, w in d["weights"][old].items():
            weights.setdefault(b, []).append((k, w))
    shape = skinned(nif, name, [tuple(d["verts"][i]) for i in used], [tuple(new[i] for i in t) for t in tris],
                    [tuple(d["uvs"][i]) for i in used], [tuple(d["normals"][i]) for i in used], weights, skel, slot)
    textured(shape, part)
    return len(used)


def build_arm(side, sex):
    skel = NifFile(SKELETONS[sex])
    with open(os.path.join(ART, "export_%s%s.json" % (side, sex))) as f:
        data = json.load(f)
    os.makedirs(OUT, exist_ok=True)
    path = os.path.join(OUT, "Arm_%s_%s.nif" % (side, sex))
    nif = NifFile()
    nif.initialize("SKYRIMSE", path)
    n = add_shape(nif, "ProteanHammerfistArm", "Arm", data["flesh"], skel, SLOT[side])
    m = add_shape(nif, "ProteanHammerfistFist", "Fist", data["fist"], skel, SLOT[side])
    nif.save()
    print("wrote", path, n, "+", m, "verts")
    # 1st person: the flesh cut at the elbow; src/Hammerfist.cpp binds it to the 1st-person bones of the same names
    path = os.path.join(OUT, "Arm1st_%s_%s.nif" % (side, sex))
    nif = NifFile()
    nif.initialize("SKYRIMSE", path)
    fl = data["flesh"]
    upper = [sum(w for b, w in bw.items() if any(k in b for k in FIRST_PERSON_DROP)) for bw in fl["weights"]]
    # the cut end closed (2026-11-13: the camera looked into the open sleeve during a swing, screenshot 92)
    fl1, caps = first_person_cut.cut(fl, lambda i: upper[i] < 0.5)
    n = add_shape(nif, "ProteanHammerfistArm1st", "Arm", fl1, skel, SLOT[side])
    print("  1st person: %d opening(s) capped" % caps)
    m = add_shape(nif, "ProteanHammerfistFist1st", "Fist", data["fist"], skel, SLOT[side])
    nif.save()
    print("wrote", path, n, "+", m, "verts")


def build_empty():
    """The weapon's own model: nothing (the fist is drawn by the sleeve)."""
    path = os.path.join(OUT, "Hammerfist.nif")
    nif = NifFile()
    nif.initialize("SKYRIMSE", path, "BSFadeNode", "ProteanHammerfist")
    NiStringExtraData.New(nif, "Prn", "WeaponMace", parent=nif.rootNode)
    nif.save()
    print("wrote", path, "(empty)")


if __name__ == "__main__":
    build_textures()
    for side in ("R", "L"):
        for sex in ("M", "F"):
            build_arm(side, sex)
    build_empty()
