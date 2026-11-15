"""Build the Tendril sleeve's models (2026-11-14; the user's reference sheet: a sleeve of flesh and tentacles with a weapon
growing past the hand, here a harpoon, head A: a spear point with three hooks raking back, like the whip's own barb).
Shaped in art/tendril/Tendril.blend (make_flesh_t, make_parts_t, finish_t; the concepts were make_concept_t).

  python -I tools/build_tendril_sleeve.py -> data/meshes/Protean/Tendril/Sleeve_<Rest|Drawn>_<R|L>_<M|F>.nif (3rd person)
                                              Sleeve1st_<R|L>_<M|F>.nif (1st person, drawn only)

Two versions, swapped by Protean (src/Tendril.cpp) on the hand holding the Tendril spell: "Rest" while weapons are put
away (the harpoon grows from the heel of the palm and lies along the fingers), "Drawn" while they are out (it stands out
of the palm, where the spell leaves from). One hidden sleeve slot per hand: right 48, left 61.
The flesh is the Blade's, made slimmer and sunk under the skin in twisting gaps, and wears the Blade's Arm textures;
the cords and tentacles the whip's sinew (Dragonborn's darktentacle01); the harpoon Hearthfire's horn (as the claws).
No textures of its own.
"""
import json
import os
import sys

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
import build_blade  # noqa: E402
import first_person_cut  # noqa: E402
from build_blade import NifFile, SKELETONS, skinned  # noqa: E402

ROOT = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
ART = os.path.join(ROOT, "art", "tendril")
OUT = os.path.join(ROOT, "data", "meshes", "Protean", "Tendril")
SLOT = {"R": 48, "L": 61}
TEX = {
    "flesh": ("textures\\Protean\\Blade\\Arm_d.dds", "textures\\Protean\\Blade\\Arm_n.dds", None),
    "sinew": ("textures\\dlc02\\dungeons\\apocrypha\\darktentacle01.dds", "textures\\dlc02\\dungeons\\apocrypha\\darktentacle01_n.dds",
              (30.0, 0.6)),   # wet flesh, as the whip's body
    "bone": ("textures\\_byoh\\clutter\\resources\\horn01.dds", "textures\\_byoh\\clutter\\resources\\horn01_n.dds",
             (8.0, 0.25)),    # the claws' dull horn
}
MODES = {"rest": "Rest", "drawn": "Drawn"}
FIRST_PERSON_DROP = ("UpperArm", "UpperarmTwist", "Clavicle")


def textured(shape, part):
    d, n, sheen = TEX[part]
    shape.set_texture("Diffuse", d)
    shape.set_texture("Normal", n)
    if sheen:
        sp = shape.shader.properties
        sp.Glossiness, sp.Spec_Str = sheen
    shape.save_shader_attributes()


def add_shape(nif, name, part, d, skel, slot, keep=None):
    tris = [t for t in d["tris"] if keep is None or all(keep(i) for i in t)]
    used = sorted({i for t in tris for i in t})
    if not used:
        return 0
    new = {old: k for k, old in enumerate(used)}
    weights = {}
    for k, old in enumerate(used):
        for b, w in d["weights"][old].items():
            weights.setdefault(b, []).append((k, w))
    shape = skinned(nif, name, [tuple(d["verts"][i]) for i in used], [tuple(new[i] for i in t) for t in tris],
                    [tuple(d["uvs"][i]) for i in used], [tuple(d["normals"][i]) for i in used], weights, skel, slot)
    textured(shape, part)
    return len(used)


def upper(d):
    """Each point's weight on the upper arm and shoulder (cut away in 1st person)."""
    return [sum(w for b, w in bw.items() if any(k in b for k in FIRST_PERSON_DROP)) for bw in d["weights"]]


def build(side, sex):
    skel = NifFile(SKELETONS[sex])
    with open(os.path.join(ART, "export_%s%s.json" % (side, sex))) as f:
        data = json.load(f)
    os.makedirs(OUT, exist_ok=True)
    for mode, title in MODES.items():
        path = os.path.join(OUT, "Sleeve_%s_%s_%s.nif" % (title, side, sex))
        nif = NifFile()
        nif.initialize("SKYRIMSE", path)
        counts = [add_shape(nif, "ProteanTendrilFlesh", "flesh", data["flesh"], skel, SLOT[side]),
                  add_shape(nif, "ProteanTendrilSinew", "sinew", data["sinew_" + mode], skel, SLOT[side]),
                  add_shape(nif, "ProteanTendrilBone", "bone", data["bone_" + mode], skel, SLOT[side])]
        nif.save()
        print("wrote", path, counts)
    # 1st person, drawn only (with weapons away the arms are off screen): cut at the elbow, the opening capped;
    # src/Tendril.cpp binds it to the 1st-person bones of the same names
    path = os.path.join(OUT, "Sleeve1st_%s_%s.nif" % (side, sex))
    nif = NifFile()
    nif.initialize("SKYRIMSE", path)
    fl = data["flesh"]
    up = upper(fl)
    fl1, caps = first_person_cut.cut(fl, lambda i: up[i] < 0.5)
    # the right hand's own spike, turned to where its throw leaves (art/tendril/scripts/make_parts_t.py THROW_*); the
    # left's sat right already (user, 2026-11-14)
    mode = "drawn1st" if side == "R" else "drawn"
    sw = data["sinew_" + mode]
    sup = upper(sw)
    counts = [add_shape(nif, "ProteanTendrilFlesh1st", "flesh", fl1, skel, SLOT[side]),
              add_shape(nif, "ProteanTendrilSinew1st", "sinew", sw, skel, SLOT[side], keep=lambda i: sup[i] < 0.5),
              add_shape(nif, "ProteanTendrilBone1st", "bone", data["bone_" + mode], skel, SLOT[side])]
    nif.save()
    print("wrote", path, counts, "(%d opening(s) capped)" % caps)


def build_none():
    """The 3rd-person pieces' 1st-person model: nothing. The game draws slot 61 in 1st person, so the left resting
    harpoon stood beside the 1st-person spike (user, screenshot 105, 2026-11-14); the 1st-person sleeve is Protean's own."""
    path = os.path.join(OUT, "Sleeve_None.nif")
    nif = NifFile()
    nif.initialize("SKYRIMSE", path, "BSFadeNode", "ProteanTendrilNone")
    nif.save()
    print("wrote", path, "(empty)")


if __name__ == "__main__":
    build_none()
    for side in ("R", "L"):
        for sex in ("M", "F"):
            build(side, sex)
