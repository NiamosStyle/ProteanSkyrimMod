"""The Hammerfist's power attacks (2026-11-13, the user: the beast-race power punches stop in the air, which looked weird
in 1st person; "all three" of the proposal):
  - a one-hand power attack (either hand) is the one-handed weapons' overhead power attack (1hm_attackpower): the arm
    goes up over the head and comes down to the ground (the hand falls to 14 units over it; the punch stopped at 49).
    The game has it only for the right hand: the left hand's is a mirror image, made here.
  - the both-hands power attack is the warhammer's overhead smash (2hw_attackpower): both fists together, down to 10.
Each is retimed onto the clip it stands in for, so its fist lands on that clip's HitFrame and it ends with it: the
attack's signals (weaponSwing, HitFrame, PowerAttackStop ...) are the game's for the unarmed clip, at those times
(Skyrim's animation data), whatever motion plays. Measured from the game's data:
  h2h_attackpowerforward<right|left>hand: HitFrame 0.801, PowerAttackStop 1.667 (1.667 s)
  h2h_attackrightpowerforward (both hands): HitFrames 0.503, 1.001, 1.461, attackStop 2.167 (2.167 s)

  clips(person) -> {name: bytes} for tools/build_claw_anims.py (person "" or "_1stperson/")

Uses PyNifly's HKX module (GPL-3.0, only run here, never shipped), as tools/build_glide_anim.py does. The game's
animations are read from its archive and not kept in git.
"""
import copy
import math
import os
import sys
import tempfile

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
import bsa  # noqa: E402

ANIMATIONS = "C:/Games/steamapps/common/Skyrim Special Edition/Data/Skyrim - Animations.bsa"
PYNIFLY_HKX = "C:/Games/steamapps/common/Skyrim Special Edition/tools/pynifly/io_scene_nifly/hkx"
FPS = 30.0
# replaced name -> (source clip, the source's landing time is found from the hand that strikes, its landing in the
# target, the target's length, mirrored)
JOBS = {
    "h2h_attackpowerforwardrighthand.hkx": ("1hm_attackpower.hkx", "R", 0.801, 1.667, False),
    "h2h_attackpowerforwardlefthand.hkx": ("1hm_attackpower.hkx", "R", 0.801, 1.667, True),
    "h2h_attackrightpowerforward.hkx": ("2hw_attackpower.hkx", "R", 1.461, 2.167, False),
}
# 2026-11-13, first try in game (user): every one of these "made the body go crazy", then it slowly relaxed into the
# stance. The timing was right (the log: each slam on its landing). Suspect: PyNifly's writer with fast motion (the
# glide it wrote is slow). Test: the right hand's and both hands' are the game's own files, untouched (their fists
# land 0.07 s and 0.09 s before the HitFrame; the right one ends 0.3 s early); only the left (a mirror) is written.
COPY = {"h2h_attackpowerforwardrighthand.hkx", "h2h_attackrightpowerforward.hkx"}
# 2026-11-13, second try (user): the two game files look fine, the written left mirror still goes crazy. Test 2: the
# left slot holds the game's right-hand slam passed through PyNifly's reader and writer UNCHANGED (no retiming, no
# mirror; the right arm slams). Crazy -> the writer itself is at fault; fine -> my retiming/mirroring is.
ROUND_TRIP = set()
# Result (user, 2026-11-13): the round trip played fine, so the writer is sound and the fault was mine: the block
# duration. Havok reads a frame within a block as (time into the block) / blockDuration * (maxFramesPerBlock - 1),
# so blockDuration must be (maxFramesPerBlock - 1) frames long (the game's files: 255 / 30 = 8.5 s), whatever the
# clip's length. These clips had it set to the clip's own length (1.67 s): the game read frames ~5 times too fast,
# far past the end. (The glide had it 24 s against 25.5: 6% fast, unseen.)


def qmul(a, b):
    ax, ay, az, aw = a; bx, by, bz, bw = b
    return (aw * bx + ax * bw + ay * bz - az * by, aw * by - ax * bz + ay * bw + az * bx,
            aw * bz + ax * by - ay * bx + az * bw, aw * bw - ax * bx - ay * by - az * bz)


def qconj(q): return (-q[0], -q[1], -q[2], q[3])


def qrot(q, v):
    x, y, z, w = q
    def cr(a, b): return (a[1] * b[2] - a[2] * b[1], a[2] * b[0] - a[0] * b[2], a[0] * b[1] - a[1] * b[0])
    t = tuple(2 * c for c in cr((x, y, z), v)); c2 = cr((x, y, z), t)
    return tuple(v[i] + w * t[i] + c2[i] for i in range(3))


def qnorm(q):
    L = math.sqrt(sum(c * c for c in q)) or 1.0
    return tuple(c / L for c in q)


def nlerp(a, b, t):
    if sum(x * y for x, y in zip(a, b)) < 0:
        b = tuple(-c for c in b)
    return qnorm(tuple(a[i] + (b[i] - a[i]) * t for i in range(4)))


def mirror_q(q):
    """A rotation seen in a mirror across the body's middle (x -> -x): M R M with M = diag(-1, 1, 1)."""
    x, y, z, w = q
    return (x, -y, -z, w)


def swap_name(n):
    if n.startswith("NPC L "): return "NPC R " + n[6:].replace("[L", "[R", 1)
    if n.startswith("NPC R "): return "NPC L " + n[6:].replace("[R", "[L", 1)
    return n


def world(names, parents, local):
    w = [None] * len(names)
    for i in range(len(names)):
        t, q = local[i]; p = parents[i]
        w[i] = (t, q) if p < 0 else (tuple(w[p][0][k] + qrot(w[p][1], t)[k] for k in range(3)), qmul(w[p][1], q))
    return w


def to_local(names, parents, w):
    out = []
    for i in range(len(names)):
        t, q = w[i]; p = parents[i]
        if p < 0:
            out.append((t, q)); continue
        pt, pq = w[p]
        inv = qconj(pq)
        out.append((qrot(inv, tuple(t[k] - pt[k] for k in range(3))), qnorm(qmul(inv, q))))
    return out


def clips(person=""):
    sys.path.insert(0, PYNIFLY_HKX)
    import anim_skyrim as hkx
    out = {}
    with tempfile.TemporaryDirectory() as tmp:
        sp = os.path.join(tmp, "skeleton.hkx")
        with open(sp, "wb") as f:
            f.write(bsa.read(ANIMATIONS, "meshes/actors/character/character assets/skeleton.hkx"))
        skel = hkx.load_skyrim_skeleton(sp)
        names = list(skel.bones); parents = list(skel.parents)
        ref = [(tuple(t.translation[:3]), tuple(t.rotation[:4])) for t in skel.reference_pose]
        mirror_of = [names.index(swap_name(n)) if swap_name(n) in names else i for i, n in enumerate(names)]
        ref_w = world(names, parents, ref)
        hand = {"R": names.index("NPC R Hand [RHnd]"), "L": names.index("NPC L Hand [LHnd]")}
        for target, (source, side, land, length, mirrored) in JOBS.items():
            if target in ROUND_TRIP:
                p = os.path.join(tmp, "rt_" + source)
                with open(p, "wb") as f:
                    f.write(bsa.read(ANIMATIONS, "meshes/actors/character/" + person + "animations/" + source))
                an = hkx.load_skyrim_animation(p)
                q = os.path.join(tmp, "rt_out_" + target)
                hkx.write_skyrim_animation(q, an, ptr_size=8)
                with open(q, "rb") as f:
                    out[target] = f.read()
                print("  %s%s <- %s through PyNifly unchanged (test)" % (person, target, source))
                continue
            if target in COPY:
                out[target] = bsa.read(ANIMATIONS, "meshes/actors/character/" + person + "animations/" + source)
                print("  %s%s <- %s, the game's own file" % (person, target, source))
                continue
            p = os.path.join(tmp, source)
            with open(p, "wb") as f:
                f.write(bsa.read(ANIMATIONS, "meshes/actors/character/" + person + "animations/" + source))
            an = copy.deepcopy(hkx.load_skyrim_animation(p))
            assert not an.track_to_bone_indices and len(an.tracks) == len(names), "tracks are not one a bone"
            n0, fd = an.num_frames, an.frame_duration
            frames = []
            for fr in range(n0):
                frames.append([(tuple(tr.translations[fr][:3]), tuple(tr.rotations[fr][:4])) for tr in an.tracks])
            # the source's landing: the striking hand at its lowest
            hz = [world(names, parents, fr)[hand[side]][0][2] - fr[0][0][2] for fr in frames]
            lo = min(range(n0), key=lambda i: hz[i])
            src_land, src_len = lo * fd, (n0 - 1) * fd
            n = int(round(length * FPS)) + 1
            assert n <= 256, "one block"
            new = []
            for k in range(n):
                t = k / FPS
                s = t / land * src_land if t <= land else src_land + (t - land) / (length - land) * (src_len - src_land)
                f = min(n0 - 1.000001, s / fd)
                i, a = int(f), f - int(f)
                A, B = frames[i], frames[i + 1]
                local = [(tuple(A[b][0][c] + (B[b][0][c] - A[b][0][c]) * a for c in range(3)), nlerp(A[b][1], B[b][1], a))
                         for b in range(len(names))]
                if mirrored:
                    w = world(names, parents, local)
                    mw = [None] * len(names)
                    for b in range(len(names)):
                        m = mirror_of[b]          # this bone moves as its other-side twin did, seen in a mirror
                        t_, q_ = w[m]
                        # its turn away from the rest pose, mirrored, put on this bone's own rest turn
                        d = qmul(q_, qconj(ref_w[m][1]))
                        mw[b] = ((-t_[0], t_[1], t_[2]), qnorm(qmul(mirror_q(d), ref_w[b][1])))
                    local = to_local(names, parents, mw)
                new.append(local)
            an.num_frames, an.duration, an.frame_duration = n, (n - 1) / FPS, 1.0 / FPS
            an.num_blocks, an.max_frames_per_block = 1, 256
            an.block_duration = (an.max_frames_per_block - 1) / FPS
            for b, tr in enumerate(an.tracks):
                tr.translations = [list(new[k][b][0]) for k in range(n)]
                tr.rotations = [list(new[k][b][1]) for k in range(n)]
                tr.scales = [[1.0, 1.0, 1.0] for _ in range(n)]
            an.annotations = []
            path = os.path.join(tmp, "out_" + target)
            hkx.write_skyrim_animation(path, an, ptr_size=8)   # SE: 64-bit
            back = hkx.load_skyrim_animation(path)
            # check: where the striking hand (the other one, mirrored) is lowest, and when
            bh = hand["L" if mirrored else side]
            bz = []
            for fr in range(back.num_frames):
                loc = [(tuple(tr.translations[fr][:3]), tuple(tr.rotations[fr][:4])) for tr in back.tracks]
                bz.append(world(names, parents, loc)[bh][0][2] - loc[0][0][2])
            bl = min(range(len(bz)), key=lambda i: bz[i])
            print("  %s%s <- %s%s: %d frames, %.2f s; the %s hand lands at %.2f s (%.1f over the ground)" % (
                person, target, source, " mirrored" if mirrored else "", back.num_frames, back.duration,
                "left" if bh == hand["L"] else "right", bl / FPS, bz[bl]))
            with open(path, "rb") as f:
                out[target] = f.read()
    return out


if __name__ == "__main__":
    for person in ("", "_1stperson/"):
        clips(person)
