"""Where the 1st-person hands point while the Tendril is ready, read live from the running game through DevBench
(2026-11-14; the right 1st-person spike looked angled too low beside the thrown tendril).

  python -I tools/measure_aim_live.py [samples]   (game running, 1st person, Tendril readied; run from anywhere)

Reads the 1st-person hand's fixed child nodes (knuckles, thumb base, magic node) and the player's aim, fits the hand's
rotation to the 1st-person skeleton's own layout, and prints for each hand the aim in hand space and in the body's
(3rd-person bind) space, the direction a 1st-person spike must point to lie along the throw.
"""
import json
import math
import os
import subprocess
import sys

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
import measure_magic_node as mm  # noqa: E402

GAME = r"C:/Games/steamapps/common/Skyrim Special Edition"
CHILDREN = ["Finger00 [{s}F00]", "Finger10 [{s}F10]", "Finger20 [{s}F20]", "Finger30 [{s}F30]", "Finger40 [{s}F40]",
            "MagicNode [{s}Mag]"]


def call(script, fn, args, self_=None):
    """A Papyrus call through DevBench's REST API (as tools/devbench-cli.sh call; bash from Python crashed on it)."""
    import urllib.request
    port = 8920
    rt = os.path.join(GAME, "Data", "SKSE", "Plugins", "devbench", "runtime.json")
    if os.path.isfile(rt):
        port = json.load(open(rt)).get("port", port)
    body = {"action": "call", "script": script, "function": fn, "args": args}
    if self_:
        body["self"] = self_
    req = urllib.request.Request("http://127.0.0.1:%d/api/tool/papyrus" % port, json.dumps(body).encode(),
                                 {"Content-Type": "application/json"})
    out = json.load(urllib.request.urlopen(req, timeout=10))
    if "returned" not in out:
        sys.exit("DevBench %s.%s %s failed: %s" % (script, fn, args, out))
    return out["returned"]


def node(name):
    return [call("NetImmerse", "GetNodeWorldPosition" + ax, [{"form": "0x14"}, name, True]) for ax in "XYZ"]


def _norm(v):
    n = math.sqrt(sum(x * x for x in v))
    return [x / n for x in v]


def _cross(a, b):
    return [a[1] * b[2] - a[2] * b[1], a[2] * b[0] - a[0] * b[2], a[0] * b[1] - a[1] * b[0]]


def _frame(p):
    """An orthonormal frame (columns) from the hand's points: toward the middle knuckle, then across the knuckles."""
    x = _norm(p[2])
    across = [p[1][i] - p[4][i] for i in range(3)]
    z = _norm(_cross(x, across))
    y = _cross(z, x)
    return [[x[i], y[i], z[i]] for i in range(3)]


def kabsch(a, b):
    """The rotation R with b ~ R a (point lists, both centred on the hand), from the two frames: R = Fb Fa^T."""
    Fa, Fb = _frame(a), _frame(b)
    return [[sum(Fb[i][k] * Fa[j][k] for k in range(3)) for j in range(3)] for i in range(3)]


def mul(M, v):
    return [sum(M[i][k] * v[k] for k in range(3)) for i in range(3)]


def tmul(M, v):
    return [sum(M[k][i] * v[k] for k in range(3)) for i in range(3)]


def main(samples=3):
    first = mm.load(mm.FIRST)
    third = mm.load(mm.THIRD)
    for s in ("R", "L"):
        hand = "NPC %s Hand [%sHnd]" % (s, s)
        h1 = mm.world(first, hand)
        local = [mm.in_space(mm.world(first, "NPC %s " % s + c.format(s=s)), h1)[0] for c in CHILDREN]
        R3, _, _ = mm.world(third, hand)
        for k in range(samples):
            hp = node(hand)
            live = [[p[i] - hp[i] for i in range(3)] for p in (node("NPC %s " % s + c.format(s=s)) for c in CHILDREN)]
            R = kabsch(local, live)
            fit = max(math.dist(mul(R, l), v) for l, v in zip(local, live))
            pitch = math.radians(angle("X")); yaw = math.radians(angle("Z"))
            ahead = [math.sin(yaw) * math.cos(pitch), math.cos(yaw) * math.cos(pitch), -math.sin(pitch)]
            aim = tmul(R, ahead)
            body = mul(R3, aim)
            print(s, "sample", k, "fit error %.2f" % fit, "aim in hand space", [round(x, 3) for x in aim],
                  "in body space", [round(x, 3) for x in body])


def angle(axis):
    return call("ObjectReference", "GetAngle" + axis, [], {"form": "0x14"})


if __name__ == "__main__":
    main(int(sys.argv[1]) if len(sys.argv) > 1 else 3)
