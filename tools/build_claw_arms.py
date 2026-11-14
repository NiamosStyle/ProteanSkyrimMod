"""Build the Rending Claws' models (2026-11-13; the user's sculpt reference Prototype/Razor_claws_sculpt.jpg): a sleeve
of stringy sinew from the shoulder down, and one long, saw-toothed claw a finger, each solid and turning only at its
knuckle. Shaped and painted in art/claws/Claws.blend by the scripts in art/claws/scripts (run.py; make_base_c,
make_flesh_c, make_claws, make_skin_c, bake_skin_c, make_female_c, mirror_c, export_game_c), the way the Blade, the
Shield and the Hammerfist were. They replace the small horn claws of tools/build_claws.py (2026-10-23), whose helpers
this still uses.

  python -I tools/build_claw_arms.py  -> data/meshes/Protean/Claws/Claws_<R|L>_<M|F>.nif (3rd person),
                                         Claws1st_<R|L>_<M|F>.nif (1st person), data/textures/Protean/Claws/*.dds

One hidden piece per hand, as before (right body slot 59, left 58; Protean.esp's claw pieces already point at these
files). The flesh keeps the game's arm and finger weights; each claw and its knot hang whole on that finger's first
bone; the cords take the weights of the flesh they lie on.
"""
import json
import os
import subprocess
import sys

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
import build_blade  # noqa: E402
import first_person_cut  # noqa: E402
from build_blade import NifFile, SKELETONS, skinned  # noqa: E402

ROOT = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
ART = os.path.join(ROOT, "art", "claws")
BAKED = os.path.join(ART, "textures")
OUT = os.path.join(ROOT, "data", "meshes", "Protean", "Claws")
TEX_OUT = os.path.join(ROOT, "data", "textures", "Protean", "Claws")
TEX_GAME = "textures\\Protean\\Claws\\"
SLOT = {"R": 59, "L": 58}
SPEC = {"Arm": 0.7, "Claw": 0.6}   # where no shine mask is baked
SHINE = {"Claw": (70.0, 0.9, (0.94, 1.0, 0.87))}   # the chitin along the claws' backs (the Shield's shine, a little less)
FIRST_PERSON_DROP = ("UpperArm", "UpperarmTwist", "Clavicle")


def build_textures():
    from PIL import Image, ImageChops
    tmp = os.path.join(BAKED, "_game")
    os.makedirs(tmp, exist_ok=True)
    os.makedirs(TEX_OUT, exist_ok=True)
    jobs = []
    for part in ("Arm", "Claw"):
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
        subprocess.run([build_blade.XTEXCONV, "-nologo", "-m", "0", "-f", fmt, "-o", TEX_OUT, os.path.join(tmp, name)],
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
    path = os.path.join(OUT, "Claws_%s_%s.nif" % (side, sex))
    nif = NifFile()
    nif.initialize("SKYRIMSE", path)
    n = add_shape(nif, "ProteanClawsArm", "Arm", data["flesh"], skel, SLOT[side])
    m = add_shape(nif, "ProteanClawsClaws", "Claw", data["claw"], skel, SLOT[side])
    nif.save()
    print("wrote", path, n, "+", m, "verts")
    # 1st person: cut at the elbow; src/Claws.cpp binds it to the 1st-person bones of the same names
    path = os.path.join(OUT, "Claws1st_%s_%s.nif" % (side, sex))
    nif = NifFile()
    nif.initialize("SKYRIMSE", path)
    counts = []
    for name, part, key in (("ProteanClawsArm1st", "Arm", "flesh"), ("ProteanClawsClaws1st", "Claw", "claw")):
        d = data[key]
        upper = [sum(w for b, w in bw.items() if any(k in b for k in FIRST_PERSON_DROP)) for bw in d["weights"]]
        # the cut end closed (2026-11-13: an open sleeve end showed in 1st person, screenshot 92)
        d1, caps = first_person_cut.cut(d, lambda i, upper=upper: upper[i] < 0.5)
        counts.append(add_shape(nif, name, part, d1, skel, SLOT[side]))
        print("  1st person %s: %d opening(s) capped" % (key, caps))
    nif.save()
    print("wrote", path, counts[0], "+", counts[1], "verts")


if __name__ == "__main__":
    build_textures()
    for side in ("R", "L"):
        for sex in ("M", "F"):
            build_arm(side, sex)
