"""tools/measure_claw_poses.py (2026-11-13), for the Rending Claws' long claws (art/claws). Each claw is one solid blade
on a finger's first bone (it turns at the knuckle only). For every 3rd frame of the beast-race unarmed clips the claws
play: where a claw of a given length ends (height over the ground, distance to the head and body), and how far each
knuckle bends from the bind pose. Prints a summary; writes art/claws/claw_poses.json (per frame, per finger: the
finger bone's frame, for the Blender side). Read-only on the game."""
import json, math, os, re, sys, tempfile
sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
import bsa
sys.path.insert(0, r"C:/Games/steamapps/common/Skyrim Special Edition/tools/pynifly/io_scene_nifly/hkx")
import anim_skyrim as hkx

A = "C:/Games/steamapps/common/Skyrim Special Edition/Data/Skyrim - Animations.bsa"
tmp = tempfile.mkdtemp()
LENGTHS = (12.0, 16.0, 19.0, 22.0)
FINGERS = {"thumb": "Finger01", "index": "Finger10", "middle": "Finger20", "ring": "Finger30", "little": "Finger40"}
BODY = ("NPC Head [Head]", "NPC Spine2 [Spn2]", "NPC Spine [Spn0]", "NPC Pelvis [Pelv]", "NPC R Thigh [RThg]",
        "NPC L Thigh [LThg]", "NPC R Calf [RClf]", "NPC L Calf [LClf]")


def load(inner, kind):
    p = os.path.join(tmp, os.path.basename(inner))
    with open(p, "wb") as f:
        f.write(bsa.read(A, inner))
    return hkx.load_skyrim_skeleton(p) if kind == "skel" else hkx.load_skyrim_animation(p)


def qmul(a, b):
    ax, ay, az, aw = a; bx, by, bz, bw = b
    return (aw * bx + ax * bw + ay * bz - az * by, aw * by - ax * bz + ay * bw + az * bx,
            aw * bz + ax * by - ay * bx + az * bw, aw * bw - ax * bx - ay * by - az * bz)


def qrot(q, v):
    x, y, z, w = q
    def cross(a, b): return (a[1] * b[2] - a[2] * b[1], a[2] * b[0] - a[0] * b[2], a[0] * b[1] - a[1] * b[0])
    t = tuple(2 * c for c in cross((x, y, z), v)); c2 = cross((x, y, z), t)
    return tuple(v[i] + w * t[i] + c2[i] for i in range(3))


def qconj(q): return (-q[0], -q[1], -q[2], q[3])
def norm(v):
    L = math.sqrt(sum(c * c for c in v)) or 1.0
    return tuple(c / L for c in v)
def dist(a, b): return math.sqrt(sum((a[k] - b[k]) ** 2 for k in range(3)))


skel = load(r"meshes\actors\character\character assets\skeleton.hkx", "skel")
names = list(skel.bones); parents = list(skel.parents)
ref = [(tuple(t.translation[:3]), tuple(t.rotation[:4])) for t in skel.reference_pose]
def find(part, side=None):
    for i, n in enumerate(names):
        if part in n and (side is None or n.startswith("NPC %s " % side)):
            return i
    raise KeyError(part)

def pose(local):
    world = [None] * len(names)
    for i in range(len(names)):
        t, q = local[i]; p = parents[i]
        world[i] = (t, q) if p < 0 else (tuple(world[p][0][k] + qrot(world[p][1], t)[k] for k in range(3)), qmul(world[p][1], q))
    return world

# each finger bone's own "along the finger" axis: toward its child in the reference pose
bones = {}
for side in "RL":
    for f, part in FINGERS.items():
        i = find(part, side)
        child = [j for j, p in enumerate(parents) if p == i][0]
        bones[(side, f)] = (i, norm(ref[child][0]), find("Hand", side))
body = [names.index(b) for b in BODY]

listing = sorted(bsa.entries(A)[0])
clips = [p for p in listing if re.search(r"\\character\\animations\\beasth2h_[^\\]*\.hkx$", p)
         and not re.search(r"1stp|equip|shout|horse", p)]
HELD = ("beasth2h_idle", "beasth2h_locomotionpose")
rows, frames = [], []
for inner in clips:
    try:
        an = load(inner, "anim")
    except Exception as e:
        print("skip", inner, e); continue
    t2b = an.track_to_bone_indices or list(range(len(an.tracks)))
    clip = os.path.basename(inner)[:-4]
    for fr in range(0, max(1, an.num_frames), 3):
        local = list(ref)
        for ti, tr in enumerate(an.tracks):
            if fr < len(tr.rotations):
                local[t2b[ti]] = (tuple(tr.translations[fr][:3]), tuple(tr.rotations[fr][:4]))
        world = pose(local)
        root_z = world[0][0][2]
        for (side, f), (i, axis, hand) in bones.items():
            t, q = world[i]
            d = qrot(q, axis)
            # the knuckle's bend: the finger's direction in the hand's frame, against the same in the reference pose
            rel = qrot(qconj(world[hand][1]), d)
            ref_world = pose(ref)   # cheap enough
            rel0 = qrot(qconj(ref_world[hand][1]), qrot(ref_world[i][1], axis))
            bend = math.degrees(math.acos(max(-1.0, min(1.0, sum(a * b for a, b in zip(rel, rel0))))))
            row = dict(clip=clip, side=side, finger=f, bend=bend, base=t[2] - root_z)
            for L in LENGTHS:
                tip = tuple(t[k] + d[k] * L for k in range(3))
                row["z%d" % L] = tip[2] - root_z
                row["b%d" % L] = min(dist(tip, world[b][0]) for b in body)
            rows.append(row)
            if side == "R":
                frames.append([clip, f, [round(c, 3) for c in t], [round(c, 5) for c in q], round(root_z, 2),
                               [[round(c, 2) for c in world[b][0]] for b in body]])

def summary(sel, label):
    print("---", label, len(sel), "samples")
    for f in FINGERS:
        s = [r for r in sel if r["finger"] == f]
        if not s: continue
        line = "%-7s bend avg %5.1f max %5.1f |" % (f, sum(r["bend"] for r in s) / len(s), max(r["bend"] for r in s))
        for L in LENGTHS:
            line += " L%d: low %6.1f body %5.1f |" % (L, min(r["z%d" % L] for r in s), min(r["b%d" % L] for r in s))
        print(line)

for side in "RL":
    summary([r for r in rows if r["side"] == side and r["clip"] in HELD], side + " held stances")
    summary([r for r in rows if r["side"] == side], side + " every beast clip")
worst = {}
for r in rows:
    if r["finger"] == "middle":
        worst[r["clip"]] = min(worst.get(r["clip"], 999), r["z19"])
print("lowest middle-claw tip (19 long) per clip:")
for c, z in sorted(worst.items(), key=lambda x: x[1])[:14]:
    print("  %-40s %6.1f" % (c, z))
out = os.path.join(os.path.dirname(os.path.dirname(os.path.abspath(__file__))), "art", "claws", "claw_poses.json")
json.dump(dict(frames=frames, body=list(BODY), axes={f: bones[("R", f)][1] for f in FINGERS}), open(out, "w"))
print("wrote", out, len(frames), "finger frames from", len(clips), "clips")
