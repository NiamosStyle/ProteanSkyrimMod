"""Build the Blade's model: one huge bone scythe-blade growing from the right forearm (the user's design, 2026-11-03,
from Prototype's reference images in Prototype/).

  python tools/build_blade.py     -> data/meshes/Protean/Blade/Blade_<angle>.nif, one per angle in ANGLES

A weapon model hangs at the right hand's WEAPON node. A greatsword's blade runs along that node's +Y, across the fist,
its grip on -Y and its edges on +-X (read from the vanilla steel greatsword). Protean's blade instead runs out along
the forearm, past the knuckles. Step 1 (2026-11-03) is a rough block-out at two angles to compare in game:
  along  the forearm's own line, as in Prototype
  half   halfway between the forearm's line and a greatsword's
The blade lies in the plane of the forearm and a greatsword's line, its edge on the side a greatsword points to.
"""
import math
import os
import sys

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
from build_claws import NifFile, SKELETONS, TEXTURES, add, sub, mul, dot, cross, length, norm, set_shader  # noqa: E402

ROOT = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
OUT = os.path.join(ROOT, "data", "meshes", "Protean", "Blade")
ANGLES = {"along": 0.0, "half": 0.5}  # share of the way from the forearm's line to a greatsword's
BACK = 14.0       # the blade starts this far back along the forearm from the palm (game units, 1 ~ 1.43 cm)
LENGTH = 74.0     # whole length, base to tip: the tip near the knee with the arm hanging, as in Prototype
CURVE = 8.0       # how far the tip sweeps back from the edge (a sabre's curve)
WIDTH = (8.0, 13.0, 0.35)  # spine to edge at the base, at its widest, and where along the length the widest is
SPINE = 2.5       # thickness at the spine's base
SEGMENTS = 24


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
    verts, faces, uvs, normals = blade(share, skel)
    set_shader(nif.createShapeFromData("ProteanBlade:0", verts, faces, uvs, normals), False, "natural")
    nif.save()
    print("wrote", path, len(verts), "verts", len(faces), "tris")


if __name__ == "__main__":
    skel = NifFile(SKELETONS["M"])  # one weapon model serves both sexes, as in the game
    for name, share in ANGLES.items():
        build(name, share, skel)
