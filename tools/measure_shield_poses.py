"""tools/measure_shield_poses.py (2026-11-05), read by art/shield/Shield.blend make_shield.py. For every frame (every 3rd) of the game's poses with a shield up: the ground as a plane in SHIELD's frame (world
height = n . p + d for a point p of the shield) and the head's position in SHIELD's frame. Read-only."""
import json, os, re, sys, tempfile
sys.path.insert(0, r"C:\Modding\Protean\tools")
import bsa
sys.path.insert(0, r"C:/Games/steamapps/common/Skyrim Special Edition/tools/pynifly/io_scene_nifly/hkx")
import anim_skyrim as hkx
from build_claws import NifFile, SKELETONS

A = "C:/Games/steamapps/common/Skyrim Special Edition/Data/Skyrim - Animations.bsa"
tmp = tempfile.mkdtemp()


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


def qmat(q):  # columns = the rotated axes
    cols = [qrot(q, e) for e in ((1, 0, 0), (0, 1, 0), (0, 0, 1))]
    return [[cols[j][i] for j in range(3)] for i in range(3)]


skel = load(r"meshes\actors\character\character assets\skeleton.hkx", "skel")
names = list(skel.bones); parents = list(skel.parents)
ref = [(tuple(t.translation[:3]), tuple(t.rotation[:4])) for t in skel.reference_pose]
idx = {n: i for i, n in enumerate(names)}
nsk = NifFile(SKELETONS["M"])
def gt(n):
    g = nsk.nodes[n].global_transform
    return [list(r) for r in g.rotation], list(g.translation)
Rh, Th = gt("NPC L Hand [LHnd]"); Rs, Ts = gt("SHIELD")
RhT = [[Rh[j][i] for j in range(3)] for i in range(3)]
R_HS = [[sum(RhT[i][k] * Rs[k][j] for k in range(3)) for j in range(3)] for i in range(3)]
T_HS = [sum(RhT[i][k] * (Ts[k] - Th[k]) for k in range(3)) for i in range(3)]

listing = sorted(bsa.entries(A)[0])
clips = [p for p in listing if re.search(r"\\character\\animations\\(sneak1hm|1hm|sneakmt)_[^\\]*\.hkx$", p)
         and not re.search(r"1stp|equip|shout|horse", p)]
out = {"ground": [], "head": [], "clips": []}
for inner in clips:
    try:
        an = load(inner, "anim")
    except Exception as e:
        print("skip", inner, e); continue
    t2b = an.track_to_bone_indices or list(range(len(an.tracks)))
    clip = os.path.basename(inner)[:-4]
    lowest = None
    for fr in range(0, max(1, an.num_frames), 3):
        local = list(ref)
        for ti, tr in enumerate(an.tracks):
            if fr < len(tr.rotations):
                local[t2b[ti]] = (tuple(tr.translations[fr][:3]), tuple(tr.rotations[fr][:4]))
        world = [None] * len(names)
        for i in range(len(names)):
            t, q = local[i]; p = parents[i]
            world[i] = (t, q) if p < 0 else (tuple(world[p][0][k] + qrot(world[p][1], t)[k] for k in range(3)), qmul(world[p][1], q))
        ht, hq = world[idx["NPC L Hand [LHnd]"]]
        RH = qmat(hq)
        RS = [[sum(RH[i][k] * R_HS[k][j] for k in range(3)) for j in range(3)] for i in range(3)]
        TS = [ht[i] + sum(RH[i][k] * T_HS[k] for k in range(3)) for i in range(3)]
        root_z = world[0][0][2]
        out["ground"].append([round(RS[2][0], 4), round(RS[2][1], 4), round(RS[2][2], 4), round(TS[2] - root_z, 2), clip])
        hd = world[idx["NPC Head [Head]"]][0]
        d = [hd[k] - TS[k] for k in range(3)]
        out["head"].append([round(sum(RS[k][i] * d[k] for k in range(3)), 1) for i in range(3)] + [clip])
        h = TS[2] - root_z
        lowest = h if lowest is None else min(lowest, h)
    out["clips"].append([clip, an.num_frames, round(lowest or 0, 1)])
json.dump(out, open(os.path.join(os.path.dirname(os.path.dirname(os.path.abspath(__file__))), "art", "shield", "shield_poses.json"), "w"))
for c in sorted(out["clips"], key=lambda c: c[2])[:15]:
    print(c)
print(len(out["ground"]), "frames from", len(out["clips"]), "clips")
