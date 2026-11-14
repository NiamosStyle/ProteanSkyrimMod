# The Tendril sleeve's flesh (2026-11-14): the Blade's flesh (BladeFlesh), made slimmer, and sunk under the skin in
# twisting gaps so it looks to go in and out of the arm (user: "a little more slender... ok if it doesn't completely
# cover up the arm"). Every point is set between the arm's skin (FleshBase, the game's arm) and where the Blade's
# flesh had it; where the gap field says so, it dives just under the skin instead.
import bpy, math
from mathutils import Vector, noise
from mathutils.bvhtree import BVHTree

SLIM = 0.5        # share of the Blade flesh's thickness kept
SINK = 0.4        # how far under the skin a gap's flesh lies
BANDS = 5         # flesh bands twisting round the arm, gaps between them
TWIST = 0.09      # radians of turn per unit along the arm
GAP = 0.15        # the gap field's level: higher, wider gaps
FEATHER = 0.5     # how softly a band dives under the skin at its edges
FULL_FROM = 0.62  # share of the way down the arm (shoulder to wrist) from which the flesh covers fully: the
                  # harpoon's root on the forearm sits in solid flesh
HAND_GAP = True   # gaps over the back of the hand too


def evaluated(ob):
    dg = bpy.context.evaluated_depsgraph_get()
    return ob.evaluated_get(dg)


def make_flesh(slim=SLIM, gap=GAP):
    src = bpy.data.objects["BladeFlesh"]
    skin = bpy.data.objects["FleshBase"]
    se = evaluated(skin).to_mesh()
    bvh = BVHTree.FromPolygons([skin.matrix_world @ v.co for v in se.vertices], [tuple(f.vertices) for f in se.polygons])
    evaluated(skin).to_mesh_clear()
    me = src.data.copy()   # the points as shaped, without the Smooth modifier (~9,000, as the Blade's export), weights kept
    shoulder, elbow, wrist, a, up, side = arm_frame()
    total = (elbow - shoulder).length + (wrist - elbow).length
    for v in me.vertices:
        p = src.matrix_world @ v.co
        near, nrm, _, _ = bvh.find_nearest(p)
        if near is None:
            continue
        off = p - near
        out = off.dot(nrm)
        # where along the arm and round it
        s1 = (p - shoulder).dot((elbow - shoulder).normalized())
        if s1 < (elbow - shoulder).length:
            ax, c, along = (elbow - shoulder).normalized(), shoulder, s1
        else:
            ax, c = a, elbow
            along = (elbow - shoulder).length + (p - elbow).dot(a)
        d = p - (c + ax * (p - c).dot(ax))
        ref = (up - ax * up.dot(ax)).normalized(); sd = ax.cross(ref)
        ang = math.atan2(d.dot(sd), d.dot(ref))
        t = along / total
        band = math.cos(BANDS * ang + TWIST * along * BANDS) + 0.55 * noise.noise(p * 0.18)
        cover = min(1.0, max(0.0, (band + 1 - 2 * gap) / FEATHER + 0.5))  # 1 = flesh, 0 = under the skin
        full = min(1.0, max(0.0, (t - FULL_FROM) / 0.08)) * min(1.0, max(0.0, (1.02 - t) / 0.06))
        if not HAND_GAP and t > 1.0:
            full = 1.0
        cover = max(cover, full)
        h = max(0.0, out) * slim * cover - SINK * (1 - cover)
        v.co = src.matrix_world.inverted() @ (near + nrm * h)
    name = "TendrilFlesh"
    ob = bpy.data.objects.get(name)
    if ob:
        old = ob.data; ob.data = me; bpy.data.meshes.remove(old)
    else:
        ob = bpy.data.objects.new(name, me)
        bpy.data.scenes[SCENE].collection.objects.link(ob)
    ob.matrix_world = src.matrix_world.copy()
    # the groups' names, in BladeFlesh's order (the weights came with the copied points; clearing the groups wipes them)
    if [g.name for g in ob.vertex_groups] != [g.name for g in src.vertex_groups]:
        assert not ob.vertex_groups, "TendrilFlesh has other groups: delete it and run again"
        for g in src.vertex_groups:
            ob.vertex_groups.new(name=g.name)
    sub = ob.modifiers.get("Smooth") or ob.modifiers.new("Smooth", 'SUBSURF'); sub.levels = 1
    fl = bpy.data.materials.get("ConceptFlesh") or bpy.data.materials.new("ConceptFlesh")
    fl.diffuse_color = (0.55, 0.30, 0.28, 1)
    me.materials.clear(); me.materials.append(fl)
    for f in me.polygons: f.use_smooth = True
    return name, len(me.vertices), len(ob.vertex_groups)
