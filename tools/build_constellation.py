"""The Protean perk tree's constellation (the user's design, 2026-11-14): a writhing mass of tentacles, drawn as a
glowing white line picture like the vanilla skill pictures, behind the tree's stars.

Custom Skills Framework hangs a skill's tree on the skydome's Point00 node, and the picture there is the mesh
cImage00. In the vampire-lord skydome (DLC01/Interface/INTVampirePerkSkydome.nif) that is the werewolf picture, a
square mapped 0..1. So Protean's skydome is that model with cImage00's texture path swapped for ours, the same
length, so the model's bytes stay sound (as tools/build_devastator_fx.py does).

The drawing itself is art/constellation/draw_tentacles.py, run in Blender; its line render is art/constellation/
lines.png. This adds the vanilla-style glow, fades the tentacles out at the bottom edge and writes the texture.

  python -I tools/build_constellation.py          texture + skydome from lines.png
  python -I tools/build_constellation.py --draw   redraw lines.png in Blender first
"""
import os
import struct
import subprocess
import sys

from PIL import Image, ImageChops, ImageFilter

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
import bsa  # noqa: E402

DATA = "C:/Games/steamapps/common/Skyrim Special Edition/Data"
BLENDER = "C:/Program Files/Blender Foundation/Blender 5.2/blender.exe"
ROOT = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
ART = os.path.join(ROOT, "art", "constellation")
OUT = os.path.join(ROOT, "data")
SIZE = 512   # as the vanilla werewolf picture

TEXTURE = "textures\\Protean\\Interface\\ProteanConstellation01.dds"
SKYDOME_IN = ("Skyrim - Meshes0.bsa", "meshes\\dlc01\\interface\\intvampireperkskydome.nif")
SKYDOME = "meshes\\Protean\\Interface\\ProteanPerkSkydome.nif"
SKYDOME_MODEL = "Protean/Interface/ProteanPerkSkydome.nif"   # as Protean.json names it (under Data/Meshes)
SWAP = (b"textures\\dlc01\\interface\\INTWerewolfConstellation.dds", TEXTURE.encode())


def draw():
    lines = os.path.join(ART, "lines.png")
    subprocess.run([BLENDER, "--background", "--factory-startup", "--python",
                    os.path.join(ART, "draw_tentacles.py"), "--", lines], check=True)


def picture():
    """lines.png -> white RGBA with the art in alpha: the lines, a tight glow and a wide one, faded out at the bottom"""
    a = Image.open(os.path.join(ART, "lines.png")).convert("RGBA").split()[3].resize((SIZE, SIZE), Image.LANCZOS)
    g1 = a.filter(ImageFilter.GaussianBlur(SIZE / 170)).point(lambda v: min(255, int(v * 1.6)))
    g2 = a.filter(ImageFilter.GaussianBlur(SIZE / 55)).point(lambda v: min(255, int(v * 1.1)))
    alpha = ImageChops.add(ImageChops.add(a, g1), g2)
    fade = Image.linear_gradient("L").resize((SIZE, SIZE)).point(lambda v: 255 if v < 170 else int(255 * (255 - v) / 85))
    alpha = ImageChops.multiply(alpha, fade)
    white = Image.new("L", (SIZE, SIZE), 255)
    return Image.merge("RGBA", (white, white, white, alpha))


def dds(img):
    """uncompressed 32-bit DDS (A8R8G8B8) with the full mip chain"""
    w, h = img.size
    mips = []
    m = img
    while True:
        r, g, b, a = m.split()
        mips.append(Image.merge("RGBA", (b, g, r, a)).tobytes())   # stored B, G, R, A
        if m.size == (1, 1):
            break
        m = m.resize((max(1, m.size[0] // 2), max(1, m.size[1] // 2)), Image.LANCZOS)
    head = struct.pack("<4s7I44x", b"DDS ", 124, 0x1 | 0x2 | 0x4 | 0x8 | 0x1000 | 0x20000, h, w, w * 4, 0, len(mips))
    head += struct.pack("<2I4s5I", 32, 0x41, b"\0\0\0\0", 32, 0x00FF0000, 0x0000FF00, 0x000000FF, 0xFF000000)
    head += struct.pack("<4I4x", 0x1000 | 0x400000 | 0x8, 0, 0, 0)
    assert len(head) == 128
    return head + b"".join(mips)


def write(rel, data):
    path = os.path.join(OUT, rel)
    os.makedirs(os.path.dirname(path), exist_ok=True)
    with open(path, "wb") as f:
        f.write(data)
    print("wrote", rel, len(data))
    return rel.replace("\\", "/")


# cImage00's transform in the vanilla model (translation, rotation rows, scale), exactly as stored. The werewolf
# picture stands about 40 degrees off upright as the menu's camera sees it (measured 2026-11-14: our stars are drawn
# upright, the picture came out tilted clockwise), so ours is turned back about its own centre, around the camera's
# line of sight (the model's y axis: the picture lies flat in y = 91.9, in front of Point00 at y = 99.3).
CIMAGE00_XFORM = (
    (-28.432870864868164, -135.16433715820312, 45.464237213134766),
    ((-0.5000001192092896, 0.0, 0.8660253286361694), (0.8660253286361694, 0.0, 0.5000001192092896), (0.0, 1.0, 0.0)),
    0.30420517921447754)
CIMAGE00_CORNERS_UV = ((0, 1), (1, 1), (1, 0), (0, 0))


def upright(d):
    """Patch cImage00's transform so its picture's up is the camera's up (+z), turned about the picture's centre."""
    t, R, s = CIMAGE00_XFORM
    old = struct.pack("<3f9ff", *t, *R[0], *R[1], *R[2], s)
    assert d.count(old) == 1, "cImage00's transform found %d times" % d.count(old)
    # the picture's corners in the model, from its mesh (local verts are fixed in the vanilla file)
    sys.path.insert(0, "C:/Games/steamapps/common/Skyrim Special Edition/tools/pynifly/io_scene_nifly")
    import math
    import tempfile
    from pyn.pynifly import NifFile
    with tempfile.TemporaryDirectory() as tmp:
        p = os.path.join(tmp, "sky.nif")
        with open(p, "wb") as f:
            f.write(d)
        shape = [x for x in NifFile(p).shapes if x.name == "cImage00"][0]
        verts, uvs = [tuple(v) for v in shape.verts], [tuple(round(c) for c in uv) for uv in shape.uvs]

    def world(v):
        return tuple(sum(R[i][j] * v[j] for j in range(3)) * s + t[i] for i in range(3))
    W = {uv: world(v) for v, uv in zip(verts, uvs)}
    centre = tuple(sum(w[i] for w in W.values()) / 4 for i in range(3))
    up = tuple(a - b for a, b in zip(W[(0, 0)], W[(0, 1)]))        # bottom-left to top-left
    th = math.atan2(up[0], up[2])                                  # how far up leans toward +x, seen from the camera
    c, sn = math.cos(th), math.sin(th)
    rot = ((c, 0, -sn), (0, 1, 0), (sn, 0, c))                     # turns it back (x right, z up)
    R2 = tuple(tuple(sum(rot[i][k] * R[k][j] for k in range(3)) for j in range(3)) for i in range(3))
    tc = tuple(a - b for a, b in zip(t, centre))
    t2 = tuple(sum(rot[i][k] * tc[k] for k in range(3)) + centre[i] for i in range(3))
    print("cImage00 turned %.1f degrees" % math.degrees(th))
    new = struct.pack("<3f9ff", *t2, *R2[0], *R2[1], *R2[2], s)
    return d.replace(old, new)


def build():
    """Writes the files under data/; returns their paths there (for tools/package.py)."""
    out = [write(TEXTURE, dds(picture()))]
    archive, inner = SKYDOME_IN
    d = bsa.read(os.path.join(DATA, archive), inner)
    old, new = SWAP
    assert len(old) == len(new), (len(old), len(new))
    assert d.count(old) == 1, "expected the werewolf picture's path once, found %d" % d.count(old)
    out.append(write(SKYDOME, upright(d.replace(old, new))))
    return out


if __name__ == "__main__":
    if "--draw" in sys.argv:
        draw()
    build()
