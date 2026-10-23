"""Build the Tendril model: a whip of dark sinew with a hooked horn barb at its tip, as the Tendril spell's projectile.

  python tools/build_tendril.py     -> data/meshes/Protean/Tendril/Tendril.nif

Fired like Pull Mastery's chain (studied 2026-10-23, nothing of it copied): one long unmoving model, shot as a
"flame"-type projectile, so the game sends it out from the hand. +Y is forward, the hand at the origin, the barb at
the far end (LENGTH, a little over the spell's range).
Skin: the sinew strip of Dragonborn's Apocrypha tentacles (left quarter of darktentacle01.dds, tiles lengthwise).
Barb: Hearthfire's horn, as the claws (tools/build_claws.py).
"""
import math
import os
import sys

GAME = "C:/Games/steamapps/common/Skyrim Special Edition"
sys.path.insert(0, GAME + "/tools/pynifly/io_scene_nifly")
from pyn.pynifly import NifFile, PynBufferTypes  # noqa: E402
from pyn.nifconstants import ShaderFlags1, ShaderFlags2  # noqa: E402

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
from build_claws import add, sub, mul, dot, cross, norm, length  # noqa: E402

ROOT = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
OUT = os.path.join(ROOT, "data", "meshes", "Protean", "Tendril")
SINEW = ("textures\\dlc02\\dungeons\\apocrypha\\darktentacle01.dds", "textures\\dlc02\\dungeons\\apocrypha\\darktentacle01_n.dds",
         (0.01, 0.23))  # u range of the strip
HORN = ("textures\\_byoh\\clutter\\resources\\horn01.dds", "textures\\_byoh\\clutter\\resources\\horn01_n.dds",
        (70 / 256, 165 / 512, 120 / 256, 330 / 512))  # u left, v base, u right, v tip: the horn's dark half only
LENGTH = 1550.0     # game units (1 ~ 1.43 cm); the spell reaches 1500
RADIUS = (3.2, 1.6)  # at the hand, at the barb
TILE = 70.0         # units of length per repeat of the sinew texture
RING = 5.0          # units between rings
SIDES = 10
BARB = 16.0         # the barb's point
HOOKS = 3           # hooks raking back from the barb's base
SHEEN = {"body": (30.0, 0.6), "barb": (8.0, 0.25)}  # glossiness, specular: wet flesh; the claws' dull horn


def radius_at(y):
    """Tapering, with a slow ripple of muscle along it."""
    t = y / LENGTH
    r = RADIUS[0] + (RADIUS[1] - RADIUS[0]) * t
    return r * (1.0 + 0.10 * math.sin(2 * math.pi * y / 41.0) + 0.05 * math.sin(2 * math.pi * y / 13.0))


def centre_at(y):
    """A slight sway, so it reads as flesh and not a rod; none at the hand, where it leaves the arm."""
    a = 1.4 * min(1.0, y / 120.0)
    return [a * math.sin(2 * math.pi * y / 230.0), y, a * math.cos(2 * math.pi * y / 170.0) - a]


def tube(rings, sides, uv):
    """rings: [(centre, x axis, z axis, radius, v)]; uv(q, v) -> (u, v). Returns verts, normals, uvs, faces."""
    verts, normals, uvs, faces = [], [], [], []
    for c, ax, az, r, v in rings:
        for q in range(sides + 1):  # the last repeats the first, for the texture seam
            a = 2 * math.pi * q / sides
            radial = add(mul(ax, math.cos(a)), mul(az, math.sin(a)))
            verts.append(add(c, mul(radial, r)))
            normals.append(radial)
            uvs.append(uv(q, v))
    n = sides + 1
    for k in range(len(rings) - 1):
        for q in range(sides):
            a, b = k * n + q, k * n + q + 1
            faces.append((a, b + n, b))
            faces.append((a, a + n, b + n))
    return verts, normals, uvs, faces


def frame(d):
    """Two axes across the direction d."""
    up = [0.0, 0.0, 1.0] if abs(d[2]) < 0.9 else [1.0, 0.0, 0.0]
    ax = norm(cross(d, up))
    return ax, norm(cross(ax, d))


def body():
    u0, u1 = SINEW[2]
    rings = []
    steps = int(LENGTH / RING)
    for k in range(steps + 1):
        y = LENGTH * k / steps
        c = centre_at(y)
        d = norm(sub(centre_at(min(LENGTH, y + 1.0)), centre_at(max(0.0, y - 1.0))))
        ax, az = frame(d)
        rings.append((c, ax, az, radius_at(y) * (0.8 if k == 0 else 1.0), y / TILE))
    return tube(rings, SIDES, lambda q, v: (u0 + (u1 - u0) * q / SIDES, v))


def horn(base, d, curl, size, r0, hook):
    """A horn point from base along d, hooking toward curl."""
    u0, v0, u1, v1 = HORN[2]
    rings = []
    seg = 10
    for k in range(seg + 1):
        t = k / seg
        c = add(add(base, mul(d, size * t)), mul(curl, size * hook * t ** 1.6))
        nd = norm(add(d, mul(curl, 1.6 * hook * t ** 0.6)))
        ax, az = frame(nd)
        rings.append((c, ax, az, r0 * (1 - t) ** 0.85 + 0.03, t))
    verts, normals, uvs, faces = tube(rings, 8, lambda q, t: (u0 + (u1 - u0) * q / 8, v0 + (v1 - v0) * t))
    centre = len(verts)  # cap the base
    verts.append(base); normals.append(mul(d, -1)); uvs.append(((u0 + u1) / 2, v0))
    faces += [(centre, q, q + 1) for q in range(8)]
    return verts, normals, uvs, faces


def barb():
    tip = centre_at(LENGTH)
    d = norm(sub(tip, centre_at(LENGTH - 5.0)))
    ax, az = frame(d)
    parts = [horn(sub(tip, mul(d, 1.0)), d, az, BARB, radius_at(LENGTH) * 1.05, 0.15)]  # flush with the body
    for i in range(HOOKS):
        a = 2 * math.pi * (i + 0.25) / HOOKS
        out = add(mul(ax, math.cos(a)), mul(az, math.sin(a)))
        start = add(tip, mul(out, RADIUS[1] * 0.9))
        # each hook leans out and forward, then curls back toward the hand: it catches flesh on the way back
        parts.append(horn(start, norm(add(mul(out, 1.0), mul(d, 0.5))), mul(d, -1), BARB * 0.45, RADIUS[1] * 0.7, 0.9))
    verts, normals, uvs, faces = [], [], [], []
    for v, n, u, f in parts:
        s = len(verts)
        verts += v; normals += n; uvs += u
        faces += [(a + s, b + s, c + s) for a, b, c in f]
    return verts, normals, uvs, faces


def shader(shape, textures, sheen):
    s = shape.shader.properties
    s.bufType = PynBufferTypes.BSLightingShaderPropertyBufType
    s.shaderflags1_set(ShaderFlags1.SPECULAR | ShaderFlags1.CAST_SHADOWS | ShaderFlags1.ZBUFFER_TEST)
    s.shaderflags2_set(ShaderFlags2.ZBUFFER_WRITE)
    s.Glossiness, s.Spec_Str = sheen
    shape.set_texture("Diffuse", textures[0])
    shape.set_texture("Normal", textures[1])
    shape.save_shader_attributes()


def build(path):
    os.makedirs(os.path.dirname(path), exist_ok=True)
    nif = NifFile()
    nif.initialize("SKYRIMSE", path, "BSFadeNode", "Tendril")
    for name, (v, n, u, f), tex, sheen in (("TendrilBody", body(), SINEW, SHEEN["body"]),
                                           ("TendrilBarb", barb(), HORN, SHEEN["barb"])):
        shader(nif.createShapeFromData(name, [tuple(p) for p in v], f, u, [tuple(p) for p in n]), tex, sheen)
        print(" ", name, len(v), "verts", len(f), "tris")
    nif.save()
    print("wrote", path)


if __name__ == "__main__":
    build(os.path.join(OUT, "Tendril.nif"))
