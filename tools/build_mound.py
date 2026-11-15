"""Build the Strange Mound (2026-11-14): the heap of burnt flesh in Helgen's ruins whose touch infects the player with
Protean. Shaped in art/mound/Mound.blend (art/mound/scripts/make_mound_m.py, textures make_textures_m.py).

  python -I tools/build_mound.py -> data/meshes/Protean/Mound/Mound.nif, data/textures/Protean/Mound/Mound_d/_n.dds

Three shapes: the mound (its own generated textures), the tendrils (the Tendril's sinew, Dragonborn's darktentacle01)
and the ribs (Hearthfire's horn01, as the claws). An invisible box gives it collision: the crosshair finds an activator
only by its collision, so without one "Search" could never be aimed at it. The box's body takes a vanilla static's
settings (a rock pile's: static layer, no mass) rather than settings made up here.
"""
import json
import os
import subprocess
import sys

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
import bsa  # noqa: E402
import build_blade  # noqa: E402
from build_claws import NifFile  # noqa: E402
from pyn.nifdefs import bhkBoxShapeProps, bhkRigidBodyProps  # noqa: E402
from pyn.nifconstants import ShaderFlags1, ShaderFlags2, HAVOC_SCALE_FACTOR  # noqa: E402
from pyn.pynifly import PynBufferTypes  # noqa: E402

ROOT = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
ART = os.path.join(ROOT, "art", "mound")
OUT = os.path.join(ROOT, "data", "meshes", "Protean", "Mound", "Mound.nif")
TEX_OUT = os.path.join(ROOT, "data", "textures", "Protean", "Mound")
MAX_SIZE = 1024
TEX = {
    "mound": ("textures\\Protean\\Mound\\Mound_d.dds", "textures\\Protean\\Mound\\Mound_n.dds", (30.0, 1.0)),  # wet in the cracks (the mask)
    "sinew": ("textures\\dlc02\\dungeons\\apocrypha\\darktentacle01.dds", "textures\\dlc02\\dungeons\\apocrypha\\darktentacle01_n.dds", (30.0, 0.6)),
    "bone": ("textures\\_byoh\\clutter\\resources\\horn01.dds", "textures\\_byoh\\clutter\\resources\\horn01_n.dds", (8.0, 0.25)),
}
ARCHIVE = r"C:/Games/steamapps/common/Skyrim Special Edition/Data/Skyrim - Meshes1.bsa"
DONOR = "meshes\\landscape\\rocks\\wetrocks\\rockpilem01wet.nif"   # a static rock pile: its body's settings
MEAT = "meshes\\clutter\\ingredients\\venison.nif"                 # a box of flesh: its surface material (footsteps, hits)


def build_textures():
    from PIL import Image
    tmp = os.path.join(ART, "textures", "_game")
    os.makedirs(tmp, exist_ok=True)
    os.makedirs(TEX_OUT, exist_ok=True)
    Image.open(os.path.join(ART, "textures", "Mound_d.png")).convert("RGB").save(os.path.join(tmp, "Mound_d.png"))
    # the normal map is already the game's way up (make_textures_m.py); its alpha is the shine mask
    r, g, b = Image.open(os.path.join(ART, "textures", "Mound_n.png")).convert("RGB").split()
    a = Image.open(os.path.join(ART, "textures", "Mound_s.png")).convert("L")
    Image.merge("RGBA", (r, g, b, a)).save(os.path.join(tmp, "Mound_n.png"))
    for name in ("Mound_d.png", "Mound_n.png"):
        subprocess.run([build_blade.XTEXCONV, "-nologo", "-m", "0", "-f", "BC7_UNORM", "-w", str(MAX_SIZE), "-h", str(MAX_SIZE),
                        "-o", TEX_OUT, name], cwd=tmp, check=True, stdout=subprocess.DEVNULL)
    print("wrote 2 textures to", TEX_OUT)


def vanilla(inner):
    path = os.path.join(os.environ.get("TEMP", "."), "protean_mound_" + os.path.basename(inner))
    for archive in (ARCHIVE, ARCHIVE.replace("Meshes1", "Meshes0")):
        try:
            raw = bsa.read(archive, inner)
            break
        except KeyError:
            continue
    else:
        sys.exit("not in the game's mesh archives: " + inner)
    with open(path, "wb") as f:
        f.write(raw)
    return NifFile(path).rootNode.collision_object


def donor_body():
    co = vanilla(DONOR)
    return co.body.properties, co.properties.flags


def textured(shape, part):
    d, n, (gloss, spec) = TEX[part]
    sh = shape.shader.properties
    sh.bufType = PynBufferTypes.BSLightingShaderPropertyBufType
    sh.shaderflags1_set(ShaderFlags1.SPECULAR | ShaderFlags1.CAST_SHADOWS | ShaderFlags1.ZBUFFER_TEST)
    sh.shaderflags2_set(ShaderFlags2.ZBUFFER_WRITE)
    sh.Glossiness, sh.Spec_Str = gloss, spec
    shape.set_texture("Diffuse", d)
    shape.set_texture("Normal", n)
    shape.save_shader_attributes()


def build():
    with open(os.path.join(ART, "export.json")) as f:
        data = json.load(f)
    os.makedirs(os.path.dirname(OUT), exist_ok=True)
    nif = NifFile()
    nif.initialize("SKYRIMSE", OUT, "BSFadeNode", "ProteanMound")
    counts = {}
    for part, name in (("mound", "ProteanMound"), ("sinew", "ProteanMoundTendrils"), ("bone", "ProteanMoundRibs")):
        d = data[part]
        shape = nif.createShapeFromData(name, [tuple(v) for v in d["verts"]], [tuple(t) for t in d["tris"]],
                                        [tuple(u) for u in d["uvs"]], [tuple(n) for n in d["normals"]])
        textured(shape, part)
        counts[part] = len(d["verts"])
    # the collision: a box round it all, on a body set up as the rock pile's (static), placed at the box's middle
    # round the heap only: the tendrils above it are not solid (a box to their tips was an invisible ceiling)
    lo = [min(v[i] for v in data["mound"]["verts"]) for i in range(3)]
    hi = [max(v[i] for v in data["mound"]["verts"]) for i in range(3)]
    sf = HAVOC_SCALE_FACTOR
    box = bhkBoxShapeProps()
    meat = vanilla(MEAT).body.shape.properties
    box.bhkMaterial = meat.bhkMaterial
    box.bhkRadius = meat.bhkRadius
    for i in range(3):
        box.bhkDimensions[i] = (hi[i] - lo[i]) / 2.0 / sf
    cshape = nif.add_shape(box)
    rock, flags = donor_body()
    body = bhkRigidBodyProps()
    for name, _ in bhkRigidBodyProps._fields_:
        if name not in ("bufSize", "bufType", "shapeID"):
            try:
                setattr(body, name, getattr(rock, name))
            except (AttributeError, TypeError):
                pass
    body.bufType = PynBufferTypes.bhkRigidBodyTBufType   # a box needs the body's own transform (its middle)
    body.shapeID = cshape.id
    for i in range(3):
        body.translation[i] = (lo[i] + hi[i]) / 2.0 / sf
    body.translation[3] = 0.0
    body.rotation[0] = body.rotation[1] = body.rotation[2] = 0.0
    body.rotation[3] = 1.0
    coll = nif.rootNode.add_collision(None, flags=flags)
    coll.add_body(body)
    nif.save()
    print("wrote", OUT, counts, "box", [round(hi[i] - lo[i], 1) for i in range(3)])


def check():
    """Read it back: the shapes, their textures and the collision."""
    nif = NifFile(OUT)
    for s in nif.shapes:
        print("  shape", s.name, len(s.verts), "verts,", s.textures.get("Diffuse") if hasattr(s, "textures") else "")
    co = nif.rootNode.collision_object
    print("  collision", type(co.body).__name__, type(co.body.shape).__name__, list(co.body.shape.properties.bhkDimensions),
          "layer", co.body.properties.collisionFilter_layer, "mass", co.body.properties.mass)


if __name__ == "__main__":
    build_textures()
    build()
    check()
