# One-off, run without Blender's window on Hammerfist.blend (2026-11-13, after the user's first look in game):
#   blender -b art/hammerfist/Hammerfist.blend --python art/hammerfist/fix_2026_11_13.py
# 1. The left arm was a mess stretched across the body (screenshots 85-91): mirror_h.py added new left-named bone
#    groups, but the names live on the mesh, so the copy kept the RIGHT arm's names and every point followed the right
#    arm's bones. It now renames the copy's own groups.
# 2. The fist 20% smaller (the user: larger than the pictures suggested; "balloon hands").
# Then the fist is painted and baked again, the left side mirrored again, and everything exported.
import bpy, traceback

T = bpy.data.texts


def patch(name, old, new):
    s = T[name].as_string()
    if new in s:
        return
    assert s.count(old) == 1, (name, old, s.count(old))
    T[name].clear(); T[name].write(s.replace(old, new))


try:
    patch("mirror_h.py", """    for g in src.vertex_groups:
        ob.vertex_groups.new(name=swap(g.name))""", """    # the groups' names live on the mesh, so the copy brought the RIGHT arm's names: rename them in place
    # (2026-11-13; adding new left-named groups left every point on the right arm's bones), through spare names first
    gs = list(ob.vertex_groups)
    assert len(gs) == len(src.vertex_groups), (len(gs), len(src.vertex_groups))
    for i, g in enumerate(gs):
        g.name = "__swap%d" % i
    for g, sg in zip(gs, src.vertex_groups):
        g.name = swap(sg.name)""")
    patch("make_fist.py", "P = dict(CENTER=(0.0, 2.0, 9.0), RADII=(15.5, 15.0, 18.0), KNUCKLE_SWELL=0.3, PALM=0.62, SUBDIV=6,",
          "# 2026-11-13: everything 0.8 of the first size (the user, in game: too big, \"balloon hands\")\n"
          "P = dict(CENTER=(0.0, 1.6, 7.2), RADII=(12.4, 12.0, 14.4), KNUCKLE_SWELL=0.3, PALM=0.62, SUBDIV=6,")
    patch("make_fist.py", "         LUMP=1.6, LUMP_SCALE=0.18, CORDS=1.4,", "         LUMP=1.3, LUMP_SCALE=0.225, CORDS=1.1,")
    patch("make_fist.py", "         PLATE_SCALE=0.085, PLATE=2.2, PLATE_STEP=1.8, CRACK=0.09, CRACK_DEPTH=1.6, ARMOUR_FROM=0.3,",
          "         PLATE_SCALE=0.106, PLATE=1.8, PLATE_STEP=1.45, CRACK=0.09, CRACK_DEPTH=1.3, ARMOUR_FROM=0.3,")
    patch("make_fist.py", "         SPIKES=11, SPIKE_LEN=(6.0, 13.0), SPIKE_R=2.8, SPIKE_SEED=4,",
          "         SPIKES=11, SPIKE_LEN=(4.8, 10.4), SPIKE_R=2.25, SPIKE_SEED=4,")
    patch("make_fist.py", "if any((v.co - s).length < 7.0 for s in spikes): continue", "if any((v.co - s).length < 5.6 for s in spikes): continue")
    ns = {}
    for name in ("make_fist.py", "make_skin_h.py", "bake_skin_h.py", "mirror_h.py", "export_game_h.py"):
        one = {}   # each script its own names (make_skin's "math" helper is not the math module)
        exec(T[name].as_string(), one)
        ns.update({k: v for k, v in one.items() if callable(v) or k == "P"})
    ns["P"] = None
    one = {}; exec(T["make_fist.py"].as_string(), one); ns["make_fist"] = one["make_fist"]; ns["P"] = one["P"]
    print("FIX fist", ns["make_fist"](ns["P"]))
    print("FIX skin", ns["make_skin_s"]()); ns["add_bone"]()
    print("FIX bake", ns["bake"]())
    print("FIX mirror", ns["mirror_all"]())
    for n in ("HFFleshL", "HFFistL", "HFFleshFL"):
        o = bpy.data.objects[n]
        print("FIX groups", n, len(o.vertex_groups), [g.name for g in o.vertex_groups if "Hand" in g.name])
    print("FIX export", ns["export_all"]())
    bpy.ops.wm.save_mainfile()
    print("FIX saved")
except Exception:
    print("FIX FAILED"); traceback.print_exc()
