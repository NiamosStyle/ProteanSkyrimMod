"""Where the spell leaves the hand: each skeleton's "NPC <R|L> MagicNode" in its hand's own space (2026-11-14).
The 1st-person skeleton holds the magic node somewhere else than the 3rd-person one; a piece skinned rigidly to the hand
must be placed by the 1st-person node to sit where the 1st-person spell starts.

  python -I tools/measure_magic_node.py   (prints both skeletons' node, in hand space and in the 3rd-person body's space)
"""
import os
import sys

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
import bsa  # noqa: E402
from build_claws import NifFile, SKELETONS  # noqa: E402

ARCHIVE = r"C:/Games/steamapps/common/Skyrim Special Edition/Data/Skyrim - Meshes0.bsa"
FIRST = r"meshes\actors\character\_1stperson\skeleton.nif"
THIRD = r"meshes\actors\character\character assets\skeleton.nif"


def load(inner):
    out = os.path.join(os.environ.get("TEMP", "."), "protean_" + inner.split("\\")[-2].replace(" ", "_") + "_skeleton.nif")
    with open(out, "wb") as f:
        f.write(bsa.read(ARCHIVE, inner))
    return NifFile(out)


def world(nif, name):
    """The node's transform up the parent chain: (rotation rows, translation, scale)."""
    node = nif.nodes[name]
    chain = []
    while node is not None:
        chain.append(node)
        node = node.parent
    R = [[1, 0, 0], [0, 1, 0], [0, 0, 1]]
    t = [0.0, 0.0, 0.0]
    s = 1.0
    for n in reversed(chain):
        x = n.transform
        r = [list(x.rotation[i]) for i in range(3)]
        tr = list(x.translation)
        # world = parent * local: t' = t + R*s*tr ; R' = R*r ; s' = s*x.scale
        t = [t[i] + s * sum(R[i][k] * tr[k] for k in range(3)) for i in range(3)]
        R = [[sum(R[i][k] * r[k][j] for k in range(3)) for j in range(3)] for i in range(3)]
        s *= x.scale
    return R, t, s


def in_space(of, frame):
    """A world transform expressed in another node's space: point and the three axes."""
    R, t, _ = frame
    d = [of[1][i] - t[i] for i in range(3)]
    p = [sum(R[k][i] * d[k] for k in range(3)) for i in range(3)]          # R^T d
    axes = [[sum(R[k][i] * of[0][k][j] for k in range(3)) for i in range(3)] for j in range(3)]  # R^T * each axis column
    return p, axes


def measure(side="R"):
    hand, magic = "NPC %s Hand [%sHnd]" % (side, side), "NPC %s MagicNode [%sMag]" % (side, side)
    res = {}
    for tag, nif in (("1st", load(FIRST)), ("3rd", load(THIRD))):
        h, m = world(nif, hand), world(nif, magic)
        p, axes = in_space(m, h)
        res[tag] = (p, axes, nif.nodes[magic].parent.name if nif.nodes[magic].parent else None)
    return res


def first_in_body(side="R"):
    """The 1st-person magic node carried onto the 3rd-person hand: where a piece skinned to the hand must reach in the
    body's (skin) space to sit at the 1st-person spell's start. Also the 3rd-person hand's own place, as a check."""
    p = measure(side)["1st"][0]
    R, t, s = world(load(THIRD), "NPC %s Hand [%sHnd]" % (side, side))
    return [t[i] + s * sum(R[i][k] * p[k] for k in range(3)) for i in range(3)], t


if __name__ == "__main__":
    for side in ("R", "L"):
        for tag, (p, axes, parent) in measure(side).items():
            print(side, tag, "parent", parent, "pos in hand space", [round(v, 2) for v in p],
                  "axes x,y,z", [[round(v, 2) for v in a] for a in axes])
        node, hand = first_in_body(side)
        print(side, "1st-person node in the body's space", [round(v, 3) for v in node], "hand", [round(v, 3) for v in hand])
