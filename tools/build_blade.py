"""Build the Blade's model: one huge bone scythe-blade growing from the right forearm (the user's design, 2026-11-03,
from Prototype's reference images in Prototype/).

  python tools/build_blade.py     -> data/meshes/Protean/Blade/Blade_<angle>.nif, one per angle in ANGLES,
                                     and Sleeve_M.nif (the flesh over the right arm)

A weapon model hangs at the right hand's WEAPON node. A greatsword's blade runs along that node's +Y, across the fist,
its grip on -Y and its edges on +-X (read from the vanilla steel greatsword). Protean's blade instead runs out along
the forearm, past the knuckles. Step 1 (2026-11-03) is a rough block-out at two angles to compare in game:
  along  the forearm's own line, as in Prototype
  half   halfway between the forearm's line and a greatsword's
The blade lies in the plane of the forearm and a greatsword's line, its edge on the side a greatsword points to.
In the greatsword stance both hands are held together in front, so "along" pointed ahead like a lance and "half" stood
up like a sword (user's screenshots, 2026-11-03); the next test holds "along" in the one-handed stance, arm hanging.

The sleeve: the game's own male right arm and hand (body and hands meshes, read from the archives), pushed out along
their normals into a thick, lumpy mass (thickest on the forearm, as in Prototype), keeping the game's skin weights so
it bends with the arm. It sits in body slot 57, beside the claws' 59. Male only for now (a test).
"""
import math
import os
import sys

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
from build_claws import NifFile, SKELETONS, TEXTURES, add, sub, mul, dot, cross, length, norm, set_shader  # noqa: E402
from pyn.pynifly import NiStringExtraData, SkyPartition  # noqa: E402
from build_claws import inverse, TransformBuf  # noqa: E402
import bsa  # noqa: E402

ROOT = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
OUT = os.path.join(ROOT, "data", "meshes", "Protean", "Blade")
ANGLES = {"along": 0.0, "half": 0.5}  # share of the way from the forearm's line to a greatsword's
BACK = 14.0       # the blade starts this far back along the forearm from the palm (game units, 1 ~ 1.43 cm)
LENGTH = 74.0     # whole length, base to tip: the tip near the knee with the arm hanging, as in Prototype
CURVE = 20.0      # how far the tip sweeps back from the edge (a sabre's curve; 8 looked nearly straight, user 2026-11-03)
WIDTH = (12.0, 20.0, 0.4)  # spine to edge at the base, at its widest, and where along the length the widest is
# (8 / 13 looked too narrow, user 2026-11-03)
SPINE = 3.5       # thickness at the spine's base
SEGMENTS = 24
MESHES = ["C:/Games/steamapps/common/Skyrim Special Edition/Data/Skyrim - Meshes%d.bsa" % i for i in (0, 1)]
ARM = "meshes\\actors\\character\\character assets\\"
SINEW = ("textures\\dlc02\\dungeons\\apocrypha\\darktentacle01.dds",
         "textures\\dlc02\\dungeons\\apocrypha\\darktentacle01_n.dds", (0.01, 0.23))  # the tendril's skin
SLEEVE_SLOT = 57
FOREARM = ("NPC R Forearm [RLar]", "NPC R ForearmTwist1 [RLt1]", "NPC R ForearmTwist2 [RLt2]")
UPPER = ("NPC R UpperArm [RUar]", "NPC R UpperarmTwist1 [RUt1]", "NPC R UpperarmTwist2 [RUt2]")
# how far the flesh stands out from the skin (units). At 3.5 / 1.5 / 1.6 it hid under clothes; at 7 / 4 / 4.5 it still
# showed spotty through fine clothes and the shrouded gloves poked through; at 10 / 6 / 6.5 it could be "a little bit
# bigger, still" (user, 2026-11-03). The hand is a lump the blade grows from.
THICK = {"forearm": 12.0, "upper": 7.0, "hand": 10.0}
BLADE_BONE = "NPC R ForearmTwist1 [RLt1]"  # the forearm's wrist end: turns with the forearm, not the wrist
TILE = 25.0       # units of arm per repeat of the texture


def weapon_space(skel):
    """The forearm's line and a greatsword's line, both in the WEAPON node's space."""
    w = skel.nodes["WEAPON"].global_transform
    to_weapon = lambda d: norm([sum(w.rotation[j][i] * d[j] for j in range(3)) for i in range(3)])
    pos = lambda n: list(skel.nodes[n].global_transform.translation)
    forearm = to_weapon(sub(pos("NPC R Hand [RHnd]"), pos("NPC R Forearm [RLar]")))
    return forearm, [0.0, 1.0, 0.0]


def blade(share, skel):
    forearm, sword = weapon_space(skel)
    a = norm(add(mul(forearm, 1 - share), mul(sword, share)))
    e = norm(sub(sword, mul(a, dot(sword, a)))) if share < 1 else [1.0, 0.0, 0.0]  # toward the edge
    n = norm(cross(a, e))  # through the blade's thickness
    u0, v0, u1, v1 = TEXTURES["horn"][2]
    w0, w1, widest = WIDTH
    verts, uvs, faces = [], [], []
    ring = 7  # back of the spine, spine +, middle +, edge, middle -, spine -, and the first again for the texture seam
    for k in range(SEGMENTS + 1):
        t = k / SEGMENTS
        spine = add(mul(a, -BACK + LENGTH * t), mul(e, -CURVE * t * t))
        width = w0 + (w1 - w0) * t / widest if t < widest else w1 * ((1 - t) / (1 - widest)) ** 0.8
        th = SPINE * (1 - t) ** 0.5 + 0.15
        section = [mul(e, -0.6 * th), mul(n, th / 2), add(mul(e, 0.55 * width), mul(n, th * 0.3)), mul(e, width),
                   add(mul(e, 0.55 * width), mul(n, -th * 0.3)), mul(n, -th / 2)]
        section.append(section[0])
        for q, s in enumerate(section):
            verts.append(add(spine, s))
            uvs.append((u0 + (u1 - u0) * q / (ring - 1), v0 + (v1 - v0) * t))
    for k in range(SEGMENTS):
        for q in range(ring - 1):
            p, r = k * ring + q, k * ring + q + 1
            faces.append((p, r, r + ring))
            faces.append((p, r + ring, p + ring))
    centre = len(verts)  # cap the base
    verts.append(add(mul(a, -BACK), mul(e, w0 * 0.3))); uvs.append(((u0 + u1) / 2, v0))
    for q in range(ring - 1):
        faces.append((centre, q + 1, q))
    # smooth normals from the faces around each corner (the texture seam's two copies share theirs)
    normals = [[0.0, 0.0, 0.0] for _ in verts]
    for f in faces:
        fn = cross(sub(verts[f[1]], verts[f[0]]), sub(verts[f[2]], verts[f[0]]))
        for i in f:
            normals[i] = add(normals[i], fn)
    for k in range(SEGMENTS + 1):
        first, last = k * ring, k * ring + ring - 1
        normals[first] = normals[last] = add(normals[first], normals[last])
    normals = [tuple(norm(v)) if length(v) > 1e-9 else tuple(a) for v in normals]
    return [tuple(v) for v in verts], faces, uvs, normals


def build(name, share, skel):
    os.makedirs(OUT, exist_ok=True)
    path = os.path.join(OUT, "Blade_%s.nif" % name)
    nif = NifFile()
    nif.initialize("SKYRIMSE", path, "BSFadeNode", "ProteanBlade")
    # where the sheathed blade hangs, as on the game's greatswords; without it it fell to the feet (step 1, 2026-11-03)
    # "along" is held one-handed now, and a one-handed sword hangs at the hip: drawn, it stayed on the back otherwise
    NiStringExtraData.New(nif, "Prn", "WeaponSword" if name == "along" else "WeaponBack", parent=nif.rootNode)
    verts, faces, uvs, normals = blade(share, skel)
    set_shader(nif.createShapeFromData("ProteanBlade:0", verts, faces, uvs, normals), False, "natural")
    nif.save()
    print("wrote", path, len(verts), "verts", len(faces), "tris")


def game_mesh(name):
    for archive in MESHES:
        try:
            data = bsa.read(archive, ARM + name)
        except KeyError:
            continue
        path = os.path.join(OUT, "_" + name)  # a scratch copy for PyNifly to open, deleted below
        with open(path, "wb") as f:
            f.write(data)
        return path
    raise KeyError(name)


def lumps(v):
    """0..1, a slow bumpy field over the arm."""
    n = (math.sin(v[0] * 0.55 + 1.3) * math.sin(v[1] * 0.47 + 0.4) + math.sin(v[2] * 0.33 + 2.1) * math.sin(v[0] * 0.21 - v[1] * 0.3))
    return 0.5 + 0.25 * n


def sleeve(skel):
    shoulder = list(skel.nodes[UPPER[0]].global_transform.translation)
    hand = list(skel.nodes["NPC R Hand [RHnd]"].global_transform.translation)
    axis = norm(sub(hand, shoulder))
    across = norm(cross(axis, [0.0, 1.0, 0.0]))
    up = cross(across, axis)
    u0, u1 = SINEW[2]
    verts, normals, uvs, faces, weights = [], [], [], [], {}
    for name, shape_name in (("malebody_1.nif", "MaleUnderwearBody:0"), ("malehands_1.nif", "HandMaleBig3rd")):
        path = game_mesh(name)
        shape = [x for x in NifFile(path).shapes if x.name == shape_name][0]
        g2s = list(shape.global_to_skin.translation)
        own = {}  # vertex -> {bone: weight}
        for bone, vw in shape.bone_weights.items():
            for v, w in vw:
                own.setdefault(v, {})[bone] = w
        keep = {}
        for v, bw in own.items():
            arm = sum(w for b, w in bw.items() if " R " in b and ("Arm" in b or "arm" in b or "Hand" in b or "Finger" in b))
            if arm < 0.5 or shape.verts[v][0] <= 0:
                continue
            fore = sum(bw.get(b, 0) for b in FOREARM)
            upper = sum(bw.get(b, 0) for b in UPPER)
            rest = max(0.0, 1.0 - fore - upper)  # the hand and fingers
            p = [shape.verts[v][i] - g2s[i] for i in range(3)]
            th = (THICK["forearm"] * fore + THICK["upper"] * upper + THICK["hand"] * rest) * min(1.0, arm)
            th *= 0.6 + 0.8 * lumps(p)
            keep[v] = len(verts)
            verts.append(tuple(add(p, mul(list(shape.normals[v]), th))))
            normals.append(tuple(shape.normals[v]))
            d = sub(p, shoulder)
            angle = math.atan2(dot(d, up), dot(d, across)) / (2 * math.pi) + 0.5
            uvs.append((u0 + (u1 - u0) * angle, dot(d, axis) / TILE))
            for b, w in bw.items():
                weights.setdefault(b, []).append((keep[v], w))
        for t in shape.tris:
            if all(i in keep for i in t):
                faces.append(tuple(keep[i] for i in t))
        os.remove(path)
    return verts, normals, uvs, faces, weights


def skinned(nif, name, verts, faces, uvs, normals, weights, skel):
    shape = nif.createShapeFromData(name, verts, faces, uvs, normals)
    shape.skin()
    for b in sorted(weights):
        shape.add_bone(b, skel.nodes[b].global_transform)
    shape.set_global_to_skin(TransformBuf())
    for b in sorted(weights):
        shape.set_skin_to_bone_xform(b, inverse(skel.nodes[b].global_transform))
        shape.setShapeWeights(b, weights[b])
    shape.set_partitions([SkyPartition(part_id=SLEEVE_SLOT, flags=0, namedict=nif.dict)], [0] * len(faces))
    set_shader(shape, True, "natural")
    return shape


def build_sleeve(skel):
    path = os.path.join(OUT, "Sleeve_M.nif")
    nif = NifFile()
    nif.initialize("SKYRIMSE", path)
    verts, normals, uvs, faces, weights = sleeve(skel)
    shape = skinned(nif, "ProteanSleeve", verts, faces, uvs, normals, weights, skel)
    shape.set_texture("Diffuse", SINEW[0])
    shape.set_texture("Normal", SINEW[1])
    shape.save_shader_attributes()
    # the blade, fixed to the forearm: as the weapon's model it hung from the hand and turned with the wrist, so at
    # the ready it looked held, not grown (user's screenshot 36, 2026-11-03)
    w = skel.nodes["WEAPON"].global_transform
    to_global = lambda v, t: tuple(sum(w.rotation[i][j] * v[j] for j in range(3)) + t[i] for i in range(3))
    bv, bf, bu, bn = blade(ANGLES["along"], skel)
    skinned(nif, "ProteanBladeEdge", [to_global(v, w.translation) for v in bv], bf, bu,
            [to_global(n, (0, 0, 0)) for n in bn], {BLADE_BONE: [(i, 1.0) for i in range(len(bv))]}, skel)
    nif.save()
    print("wrote", path, len(verts), "+", len(bv), "verts")


def build_empty():
    """The weapon's own model: nothing to see (the blade is in the sleeve); the weapon still hits."""
    path = os.path.join(OUT, "Blade_along.nif")
    nif = NifFile()
    nif.initialize("SKYRIMSE", path, "BSFadeNode", "ProteanBlade")
    NiStringExtraData.New(nif, "Prn", "WeaponSword", parent=nif.rootNode)
    nif.save()
    print("wrote", path, "(empty)")


if __name__ == "__main__":
    skel = NifFile(SKELETONS["M"])  # one weapon model serves both sexes, as in the game
    for name, share in ANGLES.items():
        if name != "along":
            build(name, share, skel)
    build_empty()  # "along" is now drawn by the sleeve
    build_sleeve(skel)
