"""Devastator's look in red (the user's design, 2026-10-27): Dragonborn's tentacle shader and the Benthic Lurker's
splash, whose green comes from a reflection map (cubemaps\\Apocrypha) and the splash's goo gradient, not from the
tentacles' own grey textures. This copies those textures out of the game's archives in blood red (in
the compressed blocks themselves, so every mip and cube face keeps its shading), and copies the three models with
their paths pointed at the red files. Paths are swapped for ones of the same length, so the models' bytes stay sound.

  python -I tools/build_devastator_fx.py
"""
import os
import struct
import sys

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
import bsa  # noqa: E402

DATA = "C:/Games/steamapps/common/Skyrim Special Edition/Data"
ROOT = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
OUT = os.path.join(ROOT, "data")

# (archive, path inside, path out under data/), red copies of textures
TEXTURES = [
    ("Skyrim - Textures3.bsa", "textures\\cubemaps\\apocrypha.dds", "textures\\Protean\\DevCubeRed.dds"),
    ("Skyrim - Textures4.bsa", "textures\\dlc02\\effects\\gradients\\gradlurkergoo.dds",
     "textures\\Protean\\Devastator\\GradLurkerGooRed00.dds"),
    ("Skyrim - Textures4.bsa", "textures\\dlc02\\effects\\tentacletile.dds",
     "textures\\Protean\\Devastator\\TentacleTileRed.dds"),
]
# texture paths inside the models -> the red copies (same length)
SWAPS = [
    (b"textures\\cubemaps\\Apocrypha.dds", b"textures\\Protean\\DevCubeRed.dds"),
    (b"textures\\dlc02\\effects\\gradients\\GradLurkerGoo.dds", b"textures\\Protean\\Devastator\\GradLurkerGooRed00.dds"),
]
MODELS = [
    ("Skyrim - Meshes0.bsa", "meshes\\dlc02\\effects\\tentacledebris01.nif", "meshes\\Protean\\Devastator\\TentacleDebris01.nif"),
    ("Skyrim - Meshes0.bsa", "meshes\\dlc02\\effects\\tentacledebris02.nif", "meshes\\Protean\\Devastator\\TentacleDebris02.nif"),
    ("Skyrim - Meshes0.bsa", "meshes\\actors\\dlc02\\benthiclurker\\character assets\\benthiclurkerprojectileexplosion.nif",
     "meshes\\Protean\\Devastator\\RedSplash.nif"),
]


def swap565(c):
    """The colour's brightness as blood red (the green-yellow glow became yellow when only red and green swapped)."""
    r, g, b = ((c >> 11) & 31) / 31, ((c >> 5) & 63) / 63, (c & 31) / 31
    v = max(r, g, b)
    nr, ng, nb = v, v * 0.12, v * 0.08
    return (round(nr * 31) << 11) | (round(ng * 63) << 5) | round(nb * 31)


def redden(dds):
    """A DXT1 or DXT5 texture in red, block by block (all faces, all mips)."""
    four = dds[84:88]
    assert dds[:4] == b"DDS " and four in (b"DXT1", b"DXT5"), "unexpected texture format %r" % four
    d = bytearray(dds)
    step, col = (8, 0) if four == b"DXT1" else (16, 8)
    for at in range(128, len(d) - step + 1, step):
        c0, c1, idx = struct.unpack_from("<HHI", d, at + col)
        n0, n1 = swap565(c0), swap565(c1)
        if four == b"DXT1":
            four_colour = c0 > c1
            if four_colour and n0 <= n1:
                if n0 == n1:
                    idx = 0  # one colour all through
                else:
                    n0, n1, idx = n1, n0, idx ^ 0x55555555  # 0<->1, 2<->3
            elif not four_colour and n0 > n1:
                n0, n1 = n1, n0
                idx = sum(((((idx >> (2 * k)) & 3) ^ (1 if ((idx >> (2 * k)) & 3) < 2 else 0)) << (2 * k)) for k in range(16))
        struct.pack_into("<HHI", d, at + col, n0, n1, idx)
    return bytes(d)


def write(rel, data):
    path = os.path.join(OUT, rel)
    os.makedirs(os.path.dirname(path), exist_ok=True)
    with open(path, "wb") as f:
        f.write(data)
    print("wrote", rel, len(data))
    return rel.replace("\\", "/")


def build():
    """Writes the files under data/; returns their paths there (for tools/package.py)."""
    out = []
    for archive, inner, rel in TEXTURES:
        out.append(write(rel, redden(bsa.read(os.path.join(DATA, archive), inner))))
    for archive, inner, rel in MODELS:
        d = bsa.read(os.path.join(DATA, archive), inner)
        for old, new in SWAPS:
            assert len(old) == len(new)
            d = d.replace(old, new)
        out.append(write(rel, d))
    return out


if __name__ == "__main__":
    build()
