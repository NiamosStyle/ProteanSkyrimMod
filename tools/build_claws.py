"""Build the Rending Claws models: knife-like bone blades on the fingertips, one model per hand and sex.

  python tools/build_claws.py     -> data/meshes/Protean/Claws/Claws_{R,L}_{M,F}.nif

Each blade is skinned 100% to its finger's last bone (NPC R Finger02, 12, 22, 32, 42 ...), so it bends with the
finger in every animation. Positions come from the vanilla skeletons shipped with PyNifly (bind pose). The right
hand sits in body slot 59 (SBP_59_MOD_ARM_RIGHT), the left in 58, so each hand can wear claws on its own.
Shape agreed with the user from Blender previews (2026-10-22): 14 units (~20 cm), the little finger's 80%, the
thumb's half; a slight talon curve toward the palm; the sharp edge on the palm side; bone colour.
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
TEXTURE = "textures\\clutter\\ivory01.dds"
NORMAL = "textures\\clutter\\ivory01_n.dds"
BLADE_LEN = 14.0
LENGTH_FACTOR = {0: 0.5, 4: 0.8}  # thumb, little finger
SEGMENTS = 16


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


def inverse(t):
    """Inverse of a rigid transform (rotation rows R, translation T, scale S): x = S R v + T."""
    r = [list(t.rotation[i]) for i in range(3)]
    rt = [[r[j][i] for j in range(3)] for i in range(3)]
    s = 1.0 / t.scale
    tr = [-(rt[i][0] * t.translation[0] + rt[i][1] * t.translation[1] + rt[i][2] * t.translation[2]) * s for i in range(3)]
    out = TransformBuf()
    out.store(tr, [[rt[i][j] * s for j in range(3)] for i in range(3)], [s, s, s])
    return out


def blades(skel, side):
    """Triangles of all five blades: list of (verts[3], uvs[3], bone) with flat faces."""
    pos = lambda n: list(skel.nodes[n].global_transform.translation)
    hand = pos("NPC %s Hand [%sHnd]" % (side, side))
    fingers = {i: [pos(bone_name(side, i, j)) for j in range(3)] for i in range(5)}
    across = sub(fingers[1][0], fingers[4][0])
    down = sub(fingers[2][2], fingers[2][0])
    palm = norm(cross(across, down))
    if dot(palm, sub(fingers[0][1], hand)) < 0:  # the thumb sits on the palm side
        palm = mul(palm, -1)
    tris = []
    for i in range(5):
        f1, f2 = fingers[i][1], fingers[i][2]
        seg = sub(f2, f1)
        tip = add(f2, mul(norm(seg), length(seg) * 0.9))
        d = norm(sub(tip, f2))
        sideways = norm(cross(d, palm))
        back = norm(cross(d, sideways))  # toward the back of the hand
        if dot(back, palm) > 0:
            back = mul(back, -1)
        base = add(add(f2, mul(sub(tip, f2), 0.15)), mul(back, 0.35))  # on the nail
        blade_len = BLADE_LEN * LENGTH_FACTOR.get(i, 1.0)
        rings = []
        for k in range(SEGMENTS + 1):
            t = k / SEGMENTS
            c = add(add(base, mul(d, blade_len * t)), mul(back, -blade_len * 0.22 * t ** 1.7))  # talon curve to the palm
            w = 1.5 * (1 - t) ** 0.75 + 0.02   # back to edge
            h = 0.13 * (1 - t) ** 0.5 + 0.01   # half-thickness
            if k == SEGMENTS:
                rings.append([c])
            else:
                rings.append([add(c, mul(back, w * 0.25)), add(c, mul(sideways, h)), add(c, mul(back, -w * 0.75)),
                              add(c, mul(sideways, -h))])
        bone = bone_name(side, i, 2)
        for k in range(SEGMENTS):
            a, b = rings[k], rings[k + 1]
            v0, v1 = k / SEGMENTS, (k + 1) / SEGMENTS
            for q in range(4):
                u0, u1 = q / 4, (q + 1) / 4
                if len(b) == 1:
                    tris.append(([a[q], a[(q + 1) % 4], b[0]], [(u0, v0), (u1, v0), ((u0 + u1) / 2, v1)], bone))
                else:
                    tris.append(([a[q], a[(q + 1) % 4], b[(q + 1) % 4]], [(u0, v0), (u1, v0), (u1, v1)], bone))
                    tris.append(([a[q], b[(q + 1) % 4], b[q]], [(u0, v0), (u1, v1), (u0, v1)], bone))
        r = rings[0]  # cap the base
        tris.append(([r[0], r[2], r[1]], [(0, 0), (0.5, 0), (0.25, 0.05)], bone))
        tris.append(([r[0], r[3], r[2]], [(0, 0), (0.25, 0.05), (0.5, 0)], bone))
    return tris


def build(sex, side):
    skel = NifFile(SKELETONS[sex])
    tris = blades(skel, side)
    verts, uvs, normals, faces, owner = [], [], [], [], []
    for corners, uv, bone in tris:
        n = norm(cross(sub(corners[1], corners[0]), sub(corners[2], corners[0])))
        start = len(verts)
        for c, t in zip(corners, uv):
            verts.append(tuple(c)); uvs.append(t); normals.append(tuple(n)); owner.append(bone)
        faces.append((start, start + 1, start + 2))

    path = os.path.join(OUT, "Claws_%s_%s.nif" % (side, sex))
    os.makedirs(OUT, exist_ok=True)
    nif = NifFile()
    nif.initialize("SKYRIMSE", path)
    shape = nif.createShapeFromData("Claws_%s" % side, verts, faces, uvs, normals)
    shape.skin()
    bones = sorted(set(owner))
    for b in bones:
        shape.add_bone(b, skel.nodes[b].global_transform)
    ident = TransformBuf()
    shape.set_global_to_skin(ident)
    for b in bones:
        shape.set_skin_to_bone_xform(b, inverse(skel.nodes[b].global_transform))
        shape.setShapeWeights(b, [(v, 1.0) for v in range(len(verts)) if owner[v] == b])
    shape.set_partitions([SkyPartition(part_id=SLOT[side], flags=0, namedict=nif.dict)], [0] * len(faces))

    shader = shape.shader.properties
    shader.bufType = PynBufferTypes.BSLightingShaderPropertyBufType
    shader.shaderflags1_set(ShaderFlags1.SPECULAR | ShaderFlags1.SKINNED | ShaderFlags1.CAST_SHADOWS
                            | ShaderFlags1.ZBUFFER_TEST)
    shader.shaderflags2_set(ShaderFlags2.ZBUFFER_WRITE)
    shape.set_texture("Diffuse", TEXTURE)
    shape.set_texture("Normal", NORMAL)
    shape.save_shader_attributes()
    nif.save()
    print("wrote", path, len(verts), "verts", len(faces), "tris", bones)
    return path


if __name__ == "__main__":
    for sex in ("M", "F"):
        for side in ("R", "L"):
            build(sex, side)
