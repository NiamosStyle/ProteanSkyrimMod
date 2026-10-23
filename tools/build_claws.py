"""Build the Rending Claws models: natural, horn-like claws on the fingertips, one model per hand and sex.

  python tools/build_claws.py     -> data/meshes/Protean/Claws/Claws_{R,L}_{M,F}.nif and Claws1st_{R,L}_{M,F}.nif
                                     (in the STYLE below), plus every style in data/meshes/Protean/Claws/<style>/

Each claw is skinned 100% to its finger's last bone (NPC R Finger02, 12, 22, 32, 42 ...), so it bends with the
finger in every animation. Positions come from the vanilla skeletons shipped with PyNifly (bind pose). The right
hand sits in body slot 59 (SBP_59_MOD_ARM_RIGHT), the left in 58, so each hand can wear claws on its own.
The game does not build slots 58/59 for the 1st-person view, so Claws1st_* hold the same claws as plain unskinned
shapes, each in its finger bone's own space and named "ProteanClaw|<bone>": the plugin (src/Claws.cpp) hangs each one
under that bone of the 1st-person skeleton.
History: 2026-10-22 bone-coloured knife blades (14 units). 2026-10-23 the user asked for something more natural and
less shiny: round horn claws with the game's bear-claw texture and a dull sheen, and the thumb's claw hooking toward
the palm (the thumb bends across the hand, not the way the fingers do).
"""
import math
import os
import sys

GAME = "C:/Games/steamapps/common/Skyrim Special Edition"
PYNIFLY = GAME + "/tools/pynifly/io_scene_nifly"
sys.path.insert(0, PYNIFLY)
from pyn.pynifly import NifFile, SkyPartition  # noqa: E402
from pyn.structs import TransformBuf  # noqa: E402
from pyn.nifconstants import ShaderFlags1, ShaderFlags2  # noqa: E402
from pyn.pynifly import PynBufferTypes  # noqa: E402

ROOT = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
OUT = os.path.join(ROOT, "data", "meshes", "Protean", "Claws")
SKELETONS = {"M": PYNIFLY + "/skeletons/Skyrim/skeleton.nif", "F": PYNIFLY + "/skeletons/Skyrim/skeleton_female.nif"}
SLOT = {"R": 59, "L": 58}
TEXTURE = "textures\\actors\\bear\\bearclaws.dds"  # base game: dark horn with lengthwise ridges
NORMAL = "textures\\actors\\bear\\bearclaws_n.dds"
UV_BOX = (78 / 256, 112 / 256, 106 / 256, 144 / 256)  # a patch inside one claw of that texture (either way up)
GLOSSINESS, SPECULAR = 8.0, 0.25  # a dull sheen
# length (game units, 1 ~ 1.43 cm), hook (curve, share of the length), base depth (back to belly), base half-width
STYLES = {"cat": (8.0, 0.45, 1.5, 0.45), "bear": (10.0, 0.25, 1.7, 0.6), "beast": (12.0, 0.35, 1.6, 0.5)}
STYLE = "bear"  # the one Protean.esp points at
LENGTH_FACTOR = {0: 0.7, 4: 0.85}  # thumb, little finger
SEGMENTS = 16
SIDES = 8


# ---- small vector helpers (no Blender here)
def add(a, b): return [a[i] + b[i] for i in range(3)]
def sub(a, b): return [a[i] - b[i] for i in range(3)]
def mul(a, s): return [a[i] * s for i in range(3)]
def dot(a, b): return sum(a[i] * b[i] for i in range(3))
def cross(a, b): return [a[1] * b[2] - a[2] * b[1], a[2] * b[0] - a[0] * b[2], a[0] * b[1] - a[1] * b[0]]
def length(a): return math.sqrt(dot(a, a))
def norm(a): return mul(a, 1.0 / length(a))


def bone_name(side, finger, joint):
    return "NPC %s Finger%d%d [%sF%d%d]" % (side, finger, joint, side, finger, joint)


def to_local(t, v):
    """A model-space point into the space of a bone whose global transform is t (x = S R v + T, R by rows)."""
    d = sub(v, list(t.translation))
    return [sum(t.rotation[j][i] * d[j] for j in range(3)) / t.scale for i in range(3)]


def inverse(t):
    """Inverse of a rigid transform (rotation rows R, translation T, scale S): x = S R v + T."""
    r = [list(t.rotation[i]) for i in range(3)]
    rt = [[r[j][i] for j in range(3)] for i in range(3)]
    s = 1.0 / t.scale
    tr = [-(rt[i][0] * t.translation[0] + rt[i][1] * t.translation[1] + rt[i][2] * t.translation[2]) * s for i in range(3)]
    out = TransformBuf()
    out.store(tr, [[rt[i][j] * s for j in range(3)] for i in range(3)], [s, s, s])
    return out


def claws_for(skel, side, style):
    """Each claw as (bone, verts, normals, uvs, faces) in model space, with smooth round sides."""
    length0, hook, depth, half_width = STYLES[style]
    pos = lambda n: list(skel.nodes[n].global_transform.translation)
    hand = pos("NPC %s Hand [%sHnd]" % (side, side))
    fingers = {i: [pos(bone_name(side, i, j)) for j in range(3)] for i in range(5)}
    across = sub(fingers[1][0], fingers[4][0])
    down = sub(fingers[2][2], fingers[2][0])
    palm = norm(cross(across, down))
    if dot(palm, sub(fingers[0][1], hand)) < 0:  # the thumb sits on the palm side
        palm = mul(palm, -1)
    palm_centre = add(mul(add(hand, fingers[2][0]), 0.5), mul(palm, 2.0))
    u0, v0, u1, v1 = UV_BOX
    claws = []
    for i in range(5):
        f1, f2 = fingers[i][1], fingers[i][2]
        seg = sub(f2, f1)
        tip = add(f2, mul(norm(seg), length(seg) * 0.9))
        d = norm(sub(tip, f2))
        # the way the claw hooks: the fingers' toward the palm, the thumb's across toward the palm's centre
        toward = palm if i else sub(palm_centre, tip)
        curl = norm(sub(toward, mul(d, dot(toward, d))))
        back = mul(curl, -1)
        sideways = norm(cross(d, back))
        base = add(add(f2, mul(sub(tip, f2), 0.15)), mul(back, 0.35))  # on the nail
        claw_len = length0 * LENGTH_FACTOR.get(i, 1.0)
        verts, normals, uvs, faces = [], [], [], []
        for k in range(SEGMENTS + 1):
            t = k / SEGMENTS
            c = add(add(base, mul(d, claw_len * t)), mul(curl, claw_len * hook * t ** 1.6))
            w = depth * (1 - t) ** 0.85 + 0.02  # back to belly
            h = half_width * (1 - t) ** 0.85 + 0.01
            for q in range(SIDES + 1):  # the last repeats the first, for the texture seam
                a = 2 * math.pi * q / SIDES
                s = math.cos(a)  # +1 back, -1 belly (a little narrower)
                radial = add(mul(back, w * 0.5 * s), mul(sideways, h * (1.0 if s > 0 else 0.75) * math.sin(a)))
                verts.append(add(c, radial))
                normals.append(norm(radial) if k < SEGMENTS else d)
                uvs.append((u0 + (u1 - u0) * q / SIDES, v0 + (v1 - v0) * t))
        ring = SIDES + 1
        for k in range(SEGMENTS):
            for q in range(SIDES):
                a, b = k * ring + q, k * ring + q + 1
                faces.append((a, b, b + ring))
                faces.append((a, b + ring, a + ring))
        centre = len(verts)  # cap the base
        verts.append(base); normals.append(mul(d, -1)); uvs.append(((u0 + u1) / 2, v0))
        for q in range(SIDES):
            faces.append((centre, q + 1, q))
        claws.append((bone_name(side, i, 2), verts, normals, uvs, faces))
    return claws


def set_shader(shape, skinned):
    shader = shape.shader.properties
    shader.bufType = PynBufferTypes.BSLightingShaderPropertyBufType
    shader.shaderflags1_set(ShaderFlags1.SPECULAR | ShaderFlags1.CAST_SHADOWS | ShaderFlags1.ZBUFFER_TEST
                            | (ShaderFlags1.SKINNED if skinned else 0))
    shader.shaderflags2_set(ShaderFlags2.ZBUFFER_WRITE)
    shader.Glossiness = GLOSSINESS
    shader.Spec_Str = SPECULAR
    shape.set_texture("Diffuse", TEXTURE)
    shape.set_texture("Normal", NORMAL)
    shape.save_shader_attributes()


def build_first_person(path, skel, claws):
    nif = NifFile()
    nif.initialize("SKYRIMSE", path)
    for bone, verts, normals, uvs, faces in claws:
        g = skel.nodes[bone].global_transform
        local = [tuple(to_local(g, v)) for v in verts]
        turned = [tuple(sum(g.rotation[j][i] * n[j] for j in range(3)) for i in range(3)) for n in normals]
        set_shader(nif.createShapeFromData("ProteanClaw|" + bone, local, faces, uvs, turned), False)
    nif.save()


def build(sex, side, style, folder):
    skel = NifFile(SKELETONS[sex])
    claws = claws_for(skel, side, style)
    verts, uvs, normals, faces, owner = [], [], [], [], []
    for bone, cv, cn, cu, cf in claws:
        start = len(verts)
        verts += [tuple(v) for v in cv]
        normals += [tuple(n) for n in cn]
        uvs += cu
        owner += [bone] * len(cv)
        faces += [(a + start, b + start, c + start) for a, b, c in cf]

    os.makedirs(folder, exist_ok=True)
    path = os.path.join(folder, "Claws_%s_%s.nif" % (side, sex))
    nif = NifFile()
    nif.initialize("SKYRIMSE", path)
    shape = nif.createShapeFromData("Claws_%s" % side, verts, faces, uvs, normals)
    shape.skin()
    bones = sorted(set(owner))
    for b in bones:
        shape.add_bone(b, skel.nodes[b].global_transform)
    shape.set_global_to_skin(TransformBuf())
    for b in bones:
        shape.set_skin_to_bone_xform(b, inverse(skel.nodes[b].global_transform))
        shape.setShapeWeights(b, [(v, 1.0) for v in range(len(verts)) if owner[v] == b])
    shape.set_partitions([SkyPartition(part_id=SLOT[side], flags=0, namedict=nif.dict)], [0] * len(faces))
    set_shader(shape, True)
    nif.save()
    build_first_person(os.path.join(folder, "Claws1st_%s_%s.nif" % (side, sex)), skel, claws)
    print("wrote", style, path, len(verts), "verts", len(faces), "tris")


if __name__ == "__main__":
    for sex in ("M", "F"):
        for side in ("R", "L"):
            build(sex, side, STYLE, OUT)
            for style in STYLES:  # all of them, to compare in game (dev 'clawstyle <name>')
                build(sex, side, style, os.path.join(OUT, style))
