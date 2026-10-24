"""Prey's Flight's glide pose: a wingsuit glide (Prototype's), written from scratch on the game's character skeleton.

  python -I tools/build_glide_anim.py ["<MO2 mod folder of Protean>"] [--preview <file.png>]

Writes data/meshes/actors/character/animations/OpenAnimationReplacer/Protean/Glide/ (mt_jumpfall, left and right:
the fall loop, which is what plays while gliding) with an OAR config that swaps it in while Protean's hidden glide
marker is on (tools/build_plugin.py GLIDE_EFFECTS[0]). Given the mod folder, copies it there too; tools/package.py
packs it. Needs PyNifly's standalone HKX module (GPL-3.0, only run here, never shipped) from the Skyrim toolkit, and
Pillow for --preview (a stick figure from the side, front and above).

The pose is built upright on the skeleton's reference pose (model space: x right, y forward, z up), then the whole
body is pitched forward at the COM; a slow sway loops twice over the clip. Timing, track layout and binding are the
game's own fall's. Picked with the user 2026-10-24 (arms about where 25 deg past a borrowed flying pose looked right).
"""
import copy
import json
import math
import os
import shutil
import sys
import tempfile

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
import bsa  # noqa: E402

ROOT = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
GAME = "C:/Games/steamapps/common/Skyrim Special Edition"
PYNIFLY_HKX = GAME + "/tools/pynifly/io_scene_nifly/hkx"
ANIMATIONS = GAME + "/Data/Skyrim - Animations.bsa"
GLIDE_MARKER = "99C"  # Protean.esp, tools/build_plugin.py GLIDE_EFFECTS[0]
REL = "meshes/actors/character/animations/OpenAnimationReplacer/Protean/Glide"
NAMES = ("mt_jumpfall.hkx", "mt_jumpfallleft.hkx", "mt_jumpfallright.hkx")

POSE = dict(
    lean=65,         # body pitched forward from upright
    neck_up=20,      # neck and head tilted back so the eyes look ahead
    head_up=30,
    arm_out=32,      # upper arms raised further out to the sides than the rest pose (which hangs ~24 deg)
    arm_back=12,     # and swept back
    elbow=8,         # forearms bent a little
    leg_out=7,       # legs apart
    knee=8,          # knees a touch bent
    sway_roll=3.0,   # body roll each way
    sway_arm=2.5,    # arms flex up and down
)
X, Y = [1, 0, 0], [0, 1, 0]


# ---- quaternions (x, y, z, w) and forward kinematics
def qmul(a, b):
    ax, ay, az, aw = a
    bx, by, bz, bw = b
    return [aw * bx + ax * bw + ay * bz - az * by, aw * by - ax * bz + ay * bw + az * bx,
            aw * bz + ax * by - ay * bx + az * bw, aw * bw - ax * bx - ay * by - az * bz]


def qconj(q):
    return [-q[0], -q[1], -q[2], q[3]]


def qrot(q, v):
    return qmul(qmul(q, [v[0], v[1], v[2], 0.0]), qconj(q))[:3]


def qaxis(axis, deg):
    s = math.sin(math.radians(deg) / 2)
    return [axis[0] * s, axis[1] * s, axis[2] * s, math.cos(math.radians(deg) / 2)]


def world(skel, locs):
    """Local (translation, rotation) per bone -> model-space (position, rotation)."""
    out = []
    for i, p in enumerate(skel.parents):
        t, q = locs[i]
        if p < 0:
            out.append((list(t), list(q)))
        else:
            pp, pq = out[p]
            out.append(([a + b for a, b in zip(pp, qrot(pq, t))], qmul(pq, q)))
    return out


def bone(skel, prefix):
    return next(i for i, b in enumerate(skel.bones) if b.startswith(prefix))


def turn(skel, locs, i, axis, deg):
    """Turn bone i by deg about a model-space axis through its pivot; its children follow."""
    p = skel.parents[i]
    pq = world(skel, locs)[p][1] if p >= 0 else [0, 0, 0, 1]
    t, q = locs[i]
    locs[i] = (t, qmul(qmul(qmul(qconj(pq), qaxis(axis, deg)), pq), q))


def pose(skel, phase):
    s = math.sin(2 * math.pi * phase)
    locs = [(list(b.translation), list(b.rotation)) for b in skel.reference_pose]
    b = lambda prefix: bone(skel, prefix)  # noqa: E731
    arm = POSE["arm_out"] + POSE["sway_arm"] * s
    # arms (upright): out to the side about the forward axis, then back about the right axis
    turn(skel, locs, b("NPC R UpperArm"), Y, -arm)
    turn(skel, locs, b("NPC L UpperArm"), Y, arm)
    turn(skel, locs, b("NPC R UpperArm"), X, -POSE["arm_back"])
    turn(skel, locs, b("NPC L UpperArm"), X, -POSE["arm_back"])
    turn(skel, locs, b("NPC R Forearm"), Y, -POSE["elbow"])
    turn(skel, locs, b("NPC L Forearm"), Y, POSE["elbow"])
    # legs apart, knees soft
    turn(skel, locs, b("NPC R Thigh"), Y, -POSE["leg_out"])
    turn(skel, locs, b("NPC L Thigh"), Y, POSE["leg_out"])
    turn(skel, locs, b("NPC R Calf"), X, POSE["knee"])
    turn(skel, locs, b("NPC L Calf"), X, POSE["knee"])
    # head back, so the face looks ahead once the body is pitched
    turn(skel, locs, b("NPC Neck"), X, POSE["neck_up"])
    turn(skel, locs, b("NPC Head ["), X, POSE["head_up"])
    # the whole body: pitched forward, then a slow roll
    turn(skel, locs, b("NPC COM"), X, -POSE["lean"])
    turn(skel, locs, b("NPC COM"), Y, POSE["sway_roll"] * s)
    return locs


def preview(skel, locs, path):
    from PIL import Image, ImageDraw
    views = [("side (facing right)", lambda p: (p[1], p[2])), ("front", lambda p: (p[0], p[2])),
             ("top", lambda p: (p[0], p[1] + 60))]
    w = world(skel, locs)
    img = Image.new("RGB", (450 * len(views), 450), "white")
    d = ImageDraw.Draw(img)
    keep = [i for i, n in enumerate(skel.bones)
            if any(s in n for s in ["COM", "Pelvis", "Spine", "Neck", "Head [", "Clavicle", "UpperArm [", "Forearm [",
                                     "Hand [", "Thigh", "Calf", "Foot ["])]
    for v, (title, proj) in enumerate(views):
        cx = 225 + 450 * v
        d.text((cx - 60, 10), title, fill="black")
        for i in keep:
            p = skel.parents[i]
            while p >= 0 and p not in keep:
                p = skel.parents[p]
            if p >= 0:
                (x1, y1), (x2, y2) = proj(w[p][0]), proj(w[i][0])
                col = "red" if " R " in skel.bones[i] else ("blue" if " L " in skel.bones[i] else "black")
                d.line([(cx + 2.5 * x1, 380 - 2.5 * y1), (cx + 2.5 * x2, 380 - 2.5 * y2)], fill=col, width=4)
        hx, hy = proj(w[bone(skel, "NPC Head [")][0])
        d.ellipse([cx + 2.5 * hx - 12, 368 - 2.5 * hy, cx + 2.5 * hx + 12, 392 - 2.5 * hy], outline="black", width=3)
    img.save(path)


def build(preview_path=None):
    """Write the Glide folder under data/; returns the files written (relative to data/)."""
    sys.path.insert(0, PYNIFLY_HKX)
    import anim_skyrim as hkx
    with tempfile.TemporaryDirectory() as tmp:
        for name, inner in (("skeleton.hkx", "meshes/actors/character/character assets/skeleton.hkx"),
                            ("mt_jumpfall.hkx", "meshes/actors/character/animations/mt_jumpfall.hkx")):
            with open(os.path.join(tmp, name), "wb") as f:
                f.write(bsa.read(ANIMATIONS, inner))
        skel = hkx.load_skyrim_skeleton(os.path.join(tmp, "skeleton.hkx"))
        anim = copy.deepcopy(hkx.load_skyrim_animation(os.path.join(tmp, "mt_jumpfall.hkx")))
        n = anim.num_frames
        frames = [pose(skel, f / (n - 1) * 2.0) for f in range(n)]  # two sways; the last frame equals the first
        for ti, b in enumerate(anim.track_to_bone_indices or range(len(skel.bones))):
            tr = anim.tracks[ti]
            tr.translations = [list(frames[f][b][0]) for f in range(n)]
            tr.rotations = [list(frames[f][b][1]) for f in range(n)]
            tr.scales = [[1.0, 1.0, 1.0] for _ in range(n)]
        out = os.path.join(ROOT, "data", REL)
        if os.path.isdir(out):
            shutil.rmtree(out)
        os.makedirs(out)
        hkx.write_skyrim_animation(os.path.join(out, NAMES[0]), anim, ptr_size=8)  # SE: 64-bit
        hkx.load_skyrim_animation(os.path.join(out, NAMES[0]))  # reads back
        for name in NAMES[1:]:
            shutil.copy2(os.path.join(out, NAMES[0]), os.path.join(out, name))
        if preview_path:
            preview(skel, frames[0], preview_path)
    with open(os.path.join(out, "config.json"), "w", encoding="utf-8") as f:
        json.dump({"name": "Glide",
                   "description": "Prey's Flight: while gliding (Protean's hidden glide marker), the fall plays Protean's wingsuit glide.",
                   "priority": 2000000001, "interruptible": True,
                   "replaceOnLoop": False,  # replaceOnLoop on: a run showed now and then (testing 2026-10-24)
                   "conditions": [{"condition": "HasMagicEffect", "requiredVersion": "1.0.0.0",
                                   "Magic effect": {"pluginName": "Protean.esp", "formID": GLIDE_MARKER},
                                   "Active effects only": True}]}, f, indent=2)
    print("wrote the glide animation to", out)
    return [REL + "/" + name for name in NAMES + ("config.json",)]


if __name__ == "__main__":
    args = sys.argv[1:]
    shot = None
    if "--preview" in args:
        shot = args[args.index("--preview") + 1]
        del args[args.index("--preview"):args.index("--preview") + 2]
    files = build(shot)
    if args:
        target = os.path.join(args[0], REL)
        if os.path.isdir(target):
            shutil.rmtree(target)
        os.makedirs(target)
        for rel in files:
            shutil.copy2(os.path.join(ROOT, "data", rel), os.path.join(args[0], rel))
        print("installed", len(files), "files in", args[0])
