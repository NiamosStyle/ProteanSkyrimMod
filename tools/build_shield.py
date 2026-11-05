"""Build the Shield's models (the user's design, 2026-11-05): Dragonborn's Chitin Shield grown over with plates of dark
chitin, flesh cords from the forearm into its back, and a medium sleeve of flesh over the left hand, forearm and part
of the upper arm. Shaped and painted in art/shield/Shield.blend (make_shield, make_cords, make_shell, make_base_s,
make_flesh_s, make_female_s, make_skin_s, bake_skin_s, export_game_s), as the Blade's were.

  python -I tools/build_shield.py   -> data/meshes/Protean/Shield/Sleeve_M.nif, Sleeve_F.nif (3rd person),
                                       Sleeve1st_M.nif, Sleeve1st_F.nif (1st person), Shield.nif (empty),
                                       data/textures/Protean/Shield/*.dds

Like the Blade, everything seen is in one hidden sleeve (body slot 54) that Protean's plugin puts on while the shield
is drawn: the shield item itself has an empty model, so nothing hangs on the back when weapons are put away (the
user, 2026-11-05). The plates hang whole on the SHIELD node (it follows the left hand); the cords blend from the
forearm to SHIELD; the flesh keeps the game's arm weights.
"""
import json
import os
import subprocess
import sys

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
import build_blade  # noqa: E402
from build_blade import NifFile, SKELETONS, skinned, ShaderFlags2  # noqa: E402
from pyn.pynifly import NiStringExtraData  # noqa: E402

ROOT = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
ART = os.path.join(ROOT, "art", "shield")
EXPORT = os.path.join(ART, "export%s.json")   # "" male, "_F" female
BAKED = os.path.join(ART, "textures")
OUT = os.path.join(ROOT, "data", "meshes", "Protean", "Shield")
TEX_OUT = os.path.join(ROOT, "data", "textures", "Protean", "Shield")
TEX_GAME = "textures\\Protean\\Shield\\"
SLOT = 54
SPEC = {"Arm": 0.7, "Shell": 0.6}   # wet flesh, glossy chitin
FIRST_PERSON_DROP = ("NPC L UpperArm", "NPC L UpperarmTwist", "NPC L Clavicle")


def build_textures():
    from PIL import Image, ImageChops
    tmp = os.path.join(BAKED, "_game")
    os.makedirs(tmp, exist_ok=True)
    os.makedirs(TEX_OUT, exist_ok=True)
    jobs = []
    for part in ("Arm", "Shell"):
        Image.open(os.path.join(BAKED, part + "_d.png")).convert("RGB").save(os.path.join(tmp, part + "_d.png"))
        jobs.append((part + "_d.png", "BC1_UNORM"))
        r, g, b = Image.open(os.path.join(BAKED, part + "_n.png")).convert("RGB").split()
        if build_blade.FLIP_GREEN:
            g = ImageChops.invert(g)
        a = Image.new("L", r.size, int(255 * SPEC[part]))
        Image.merge("RGBA", (r, g, b, a)).save(os.path.join(tmp, part + "_n.png"))
        jobs.append((part + "_n.png", "BC3_UNORM"))
    for name, fmt in jobs:
        subprocess.run([build_blade.XTEXCONV, "-nologo", "-m", "0", "-f", fmt, "-o", TEX_OUT, os.path.join(tmp, name)],
                       check=True, stdout=subprocess.DEVNULL)
    print("wrote", len(jobs), "textures to", TEX_OUT)


def textured(shape, part):
    shape.set_texture("Diffuse", TEX_GAME + part + "_d.dds")
    shape.set_texture("Normal", TEX_GAME + part + "_n.dds")
    shape.save_shader_attributes()


def add_shape(nif, name, part, d, skel, keep=None):
    """One skinned piece from the Blender export; keep(i) picks the triangles whose points all pass."""
    tris = [t for t in d["tris"] if keep is None or all(keep(i) for i in t)]
    used = sorted({i for t in tris for i in t})
    new = {old: k for k, old in enumerate(used)}
    weights = {}
    for k, old in enumerate(used):
        for b, w in d["weights"][old].items():
            weights.setdefault(b, []).append((k, w))
    shape = skinned(nif, name, [tuple(d["verts"][i]) for i in used], [tuple(new[i] for i in t) for t in tris],
                    [tuple(d["uvs"][i]) for i in used], [tuple(d["normals"][i]) for i in used], weights, skel, SLOT)
    textured(shape, part)
    return len(used)


def build_sleeve(sex):
    skel = NifFile(SKELETONS[sex])
    with open(EXPORT % ("" if sex == "M" else "_F")) as f:
        data = json.load(f)
    os.makedirs(OUT, exist_ok=True)
    # 3rd person: the whole arm, the cords and the shield
    path = os.path.join(OUT, "Sleeve_%s.nif" % sex)
    nif = NifFile()
    nif.initialize("SKYRIMSE", path)
    n = add_shape(nif, "ProteanShieldSleeve", "Arm", data["flesh"], skel)
    m = add_shape(nif, "ProteanShieldShell", "Shell", data["shell"], skel)
    nif.save()
    print("wrote", path, n, "+", m, "verts")
    # 1st person: the flesh cut at the elbow (the upper arm would sit by the camera); src/Shield.cpp binds it to the
    # 1st-person skeleton's bones of the same names
    path = os.path.join(OUT, "Sleeve1st_%s.nif" % sex)
    nif = NifFile()
    nif.initialize("SKYRIMSE", path)
    fl = data["flesh"]
    upper = [sum(w for b, w in bw.items() if b.startswith(FIRST_PERSON_DROP)) for bw in fl["weights"]]
    n = add_shape(nif, "ProteanShieldSleeve1st", "Arm", fl, skel, lambda i: upper[i] < 0.5)
    m = add_shape(nif, "ProteanShieldShell1st", "Shell", data["shell"], skel)
    nif.save()
    print("wrote", path, n, "+", m, "verts")


def build_empty():
    """The shield item's own model: nothing (the shield is drawn by the sleeve, and only while drawn)."""
    path = os.path.join(OUT, "Shield.nif")
    nif = NifFile()
    nif.initialize("SKYRIMSE", path, "BSFadeNode", "ProteanShield")
    NiStringExtraData.New(nif, "Prn", "SHIELD", parent=nif.rootNode)
    nif.save()
    print("wrote", path, "(empty)")


if __name__ == "__main__":
    build_textures()
    build_sleeve("M")
    build_sleeve("F")
    build_empty()
