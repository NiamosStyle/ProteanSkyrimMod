# Concept models for the Tendril sleeve (2026-11-14): the Blade's arm flesh, tentacle cords wound down the forearm
# and curling off the shoulder (after the user's reference sheet), and a bone harpoon growing out of the forearm and
# out past the knuckles. make_concept(variant) builds one of three heads:
#   "barbed"  a spear point with three hooks raking back, as the thrown whip's own barb (tools/build_tendril.py)
#   "rows"    a narrow point with staggered pairs of barbs down the shaft, a fish spear
#   "toggle"  a whaler's head: one big curved fluke hooking back on one side
import bpy, bmesh, math
from mathutils import Vector, Matrix
from mathutils.bvhtree import BVHTree

SCENE = "Blade"
SHAFT_R = 2.3        # the shaft's radius at its root (it tapers to the head)
OUT_PAST = 30.0      # how far the head's point reaches past the wrist joint
POINT = 15.0         # the head's length


def arm_frame():
    arm = bpy.data.objects["MaleBody_1.nif:ARMATURE"]
    mw = arm.matrix_world
    b = arm.data.bones
    shoulder = mw @ b["NPC R UpperArm [RUar]"].head_local
    elbow = mw @ b["NPC R Forearm [RLar]"].head_local
    wrist = mw @ b["NPC R Hand [RHnd]"].head_local
    a = (wrist - elbow).normalized()
    up = Vector((0, 0, 1)); up = (up - a * up.dot(a)).normalized()   # the back of the forearm (palm in, bind pose)
    side = a.cross(up).normalized()
    return shoulder, elbow, wrist, a, up, side


def flesh_top(at, a, up, ob_name="BladeFlesh", slab=3.0):
    """How far the flesh stands above the forearm's line at a point: where the shaft must lie."""
    ob = bpy.data.objects[ob_name]
    dg = bpy.context.evaluated_depsgraph_get()
    me = ob.evaluated_get(dg).to_mesh()
    best = 0.0
    for v in me.vertices:
        d = ob.matrix_world @ v.co - at
        if abs(d.dot(a)) < slab:
            best = max(best, d.dot(up))
    ob.evaluated_get(dg).to_mesh_clear()
    return best


_BVH = {}


def surface(p, c, lift, ob_name="BladeFlesh"):
    """p pushed out from the axis point c to just above the flesh's outer surface (lift above it)."""
    if ob_name not in _BVH:
        ob = bpy.data.objects[ob_name]
        dg = bpy.context.evaluated_depsgraph_get()
        me = ob.evaluated_get(dg).to_mesh()
        _BVH[ob_name] = BVHTree.FromPolygons([ob.matrix_world @ v.co for v in me.vertices], [tuple(f.vertices) for f in me.polygons])
        ob.evaluated_get(dg).to_mesh_clear()
    d = (p - c).normalized()
    hit, _, _, _ = _BVH[ob_name].ray_cast(c + d * 40, -d, 40)
    return (hit + d * lift) if hit else p


def tube(bm, pts, radii, sides=10):
    rings = []
    for i, p in enumerate(pts):
        t = (pts[min(i + 1, len(pts) - 1)] - pts[max(i - 1, 0)]).normalized()
        ref = Vector((0, 0, 1)) if abs(t.z) < 0.9 else Vector((1, 0, 0))
        u = t.cross(ref).normalized(); w = t.cross(u)
        rings.append([bm.verts.new(p + (u * math.cos(2 * math.pi * k / sides) + w * math.sin(2 * math.pi * k / sides)) * radii[i])
                      for k in range(sides)])
    faces = []
    for r0, r1 in zip(rings, rings[1:]):
        for k in range(sides):
            faces.append(bm.faces.new((r0[k], r0[(k + 1) % sides], r1[(k + 1) % sides], r1[k])))
    faces.append(bm.faces.new(list(reversed(rings[0]))))
    faces.append(bm.faces.new(rings[-1]))
    return faces


def horn(bm, base, d, wdir, length, width, thick, curl=0.0, n=18):
    """A flat, tapering horn with a central ridge: from base along d, flat across wdir, its tip bending toward bend
    (wdir) by curl."""
    nrm = d.cross(wdir).normalized()
    rows = []
    for i in range(n + 1):
        t = i / n
        c = base + d * (length * t) + wdir * (curl * t * t)
        w = width * (1 - t) ** 0.8 * (1 + 0.25 * math.sin(math.pi * min(1, t * 2)))
        th = thick * (1 - t) ** 0.6 + 0.08
        rows.append([bm.verts.new(c + wdir * w * 0.5), bm.verts.new(c + wdir * w * 0.15 + nrm * th / 2),
                     bm.verts.new(c - wdir * w * 0.15 + nrm * th / 2), bm.verts.new(c - wdir * w * 0.5),
                     bm.verts.new(c - wdir * w * 0.15 - nrm * th / 2), bm.verts.new(c + wdir * w * 0.15 - nrm * th / 2)])
    faces = []
    for r0, r1 in zip(rows, rows[1:]):
        for k in range(6):
            faces.append(bm.faces.new((r0[k], r0[(k + 1) % 6], r1[(k + 1) % 6], r1[k])))
    faces.append(bm.faces.new(list(reversed(rows[0]))))
    return faces


def harpoon(bm, variant, root, tip, a, up, side):
    """Bone faces: the shaft, the head and its barbs."""
    neck = tip - a * POINT
    n = 16
    pts = [root + (neck - root) * (i / n) for i in range(n + 1)]
    rad = [SHAFT_R * (1.0 - 0.45 * i / n) * (1 + 0.08 * math.sin(i * 1.7)) for i in range(n + 1)]
    f = tube(bm, pts, rad, 12)
    f += tube(bm, [neck - a * 1.2, neck + a * 1.2], [SHAFT_R * 0.85, SHAFT_R * 0.75], 12)   # a collar under the head
    if variant == "rows":
        f += horn(bm, neck, a, side, POINT, 4.0, 2.6)
        f += horn(bm, neck, a, up, POINT * 0.9, 3.0, 2.4)
        for k in range(3):   # staggered barb pairs down the shaft, each raking back and out
            at = neck - a * (2 + k * 6.0)
            for s in (1, -1):
                o = (side if k % 2 == 0 else up) * s
                f += horn(bm, at + o * SHAFT_R * 0.5, (-a * 1.5 + o).normalized(), o.cross(a).normalized(), 6.5 - k * 0.8, 2.4, 1.4)
    elif variant == "toggle":
        f += horn(bm, neck, a, side, POINT, 5.0, 2.8)
        # one great fluke sweeping back and out, a hook to catch on
        d = (-a * 1.3 + side).normalized()
        w = (side - d * side.dot(d)).normalized()
        f += horn(bm, neck + a * 4.0 + side * 1.0, d, w, 15.0, 5.5, 2.4, curl=3.5)
        f += horn(bm, neck, a, up, POINT * 0.75, 2.2, 2.2)
    else:  # barbed: three hooks raking back from the head's base, as the whip's barb
        f += horn(bm, neck, a, side, POINT, 5.5, 3.0)
        f += horn(bm, neck, a, up, POINT, 4.0, 3.0)
        for k in range(3):
            ang = 2 * math.pi * k / 3 + 0.5
            o = up * math.cos(ang) + side * math.sin(ang)
            f += horn(bm, neck + a * 3.5 + o * 1.6, (-a * 1.5 + o).normalized(), o.cross(a).normalized(), 10.0, 3.4, 1.9, curl=0)
    return f


def curling(bm, base, out, back, length, r0, twist, n=24):
    """A tentacle from base: out, sweeping back and rolling up at the tip like the reference's."""
    pts, rad = [], []
    p, d = base.copy(), out.copy()
    axis = out.cross(back).normalized()
    step = length / n
    for i in range(n + 1):
        t = i / n
        pts.append(p.copy()); rad.append(r0 * (1 - t) ** 1.1 + 0.12)
        d = (Matrix.Rotation(0.05 - (0.02 + 0.5 * t ** 3), 3, axis) @ d)
        d = (Matrix.Rotation(twist * 0.04, 3, out) @ d).normalized()
        p = p + d * step
    return tube(bm, pts, rad, 10)


def sinew(bm, shoulder, elbow, wrist, a, up, side, root, neck):
    f = []
    L = (wrist - elbow).length
    # cords winding down the forearm and round the shaft, ending under the head's collar
    for c in range(5):
        ph = 2 * math.pi * c / 5
        N = 40
        # how far out the flesh is under each point, smoothed so the cord lies over the lumps, not into them
        out = []
        for i in range(N + 1):
            t = i / N
            ang = ph + t * 3.2
            c0 = elbow - a * 3 + a * (L + 3) * t
            d = up * math.cos(ang) + side * math.sin(ang)
            out.append((surface(c0 + d * 6.0, c0, 0.0) - c0).length)
        for _ in range(4):
            out = [max(out[max(i - 1, 0)], (out[max(i - 1, 0)] + out[i] + out[min(i + 1, N)]) / 3) for i in range(N + 1)]
        pts, rad = [], []
        for i in range(N + 1):
            t = i / N
            ang = ph + t * 3.2
            c0 = elbow - a * 3 + a * (L + 3) * t
            p = c0 + (up * math.cos(ang) + side * math.sin(ang)) * (out[i] + 0.3)
            if t > 0.7:   # over onto the shaft
                k = (t - 0.7) / 0.3
                s = root + (neck - root) * (0.3 + 0.55 * k)
                q = s + (up * math.cos(ang * 1.4) + side * math.sin(ang * 1.4)) * SHAFT_R * 1.05
                p = p.lerp(q, min(1, k * 1.6))
            pts.append(p); rad.append(1.1 + 0.4 * math.sin(math.pi * t) - 0.5 * max(0, t - 0.7))
        f += tube(bm, pts, rad, 8)
    # tentacles curling off the shoulder and down the upper arm
    ua = (elbow - shoulder).normalized()
    sup = Vector((0, 0, 1)); sup = (sup - ua * sup.dot(ua)).normalized()
    sside = ua.cross(sup)
    for c in range(8):
        ang = -1.6 + 3.2 * c / 7
        out = (sup * math.cos(ang) + sside * math.sin(ang))
        c0 = shoulder + ua * (2 + 4 * (c % 3))
        base = surface(c0 + out * 6.0, c0, -1.0)
        length = 16 + 12 * ((c * 37) % 5) / 4
        f += curling(bm, base, (out * 0.7 - ua * 0.7).normalized(), ua, length, 2.4 - 0.2 * (c % 3), (c % 2) * 2 - 1)
    return f


def make_concept(variant="barbed"):
    shoulder, elbow, wrist, a, up, side = arm_frame()
    L = (wrist - elbow).length
    # the shaft grows out of the forearm's flesh a third of the way down and rides along its back
    root = elbow + a * (L * 0.3) + up * (flesh_top(elbow + a * (L * 0.3), a, up) - SHAFT_R * 1.2)
    tip = wrist + a * OUT_PAST + up * (flesh_top(wrist, a, up) * 0.75)
    bm = bmesh.new()
    bone = set(harpoon(bm, variant, root, tip, a, up, side))
    sinew(bm, shoulder, elbow, wrist, a, up, side, root, tip - a * POINT)
    for f in bm.faces:
        f.material_index = 0 if f in bone else 1
    bmesh.ops.recalc_face_normals(bm, faces=bm.faces)
    name = "TendrilConcept"
    me = bpy.data.meshes.new(name)
    bm.to_mesh(me); bm.free()
    ob = bpy.data.objects.get(name)
    if ob:
        old = ob.data; ob.data = me; bpy.data.meshes.remove(old)
    else:
        ob = bpy.data.objects.new(name, me)
        bpy.data.scenes[SCENE].collection.objects.link(ob)
    bonem = bpy.data.materials.get("ConceptBone") or bpy.data.materials.new("ConceptBone")
    bonem.diffuse_color = (0.86, 0.80, 0.66, 1)
    sin = bpy.data.materials.get("ConceptSinew") or bpy.data.materials.new("ConceptSinew")
    sin.diffuse_color = (0.45, 0.16, 0.15, 1)
    fl = bpy.data.materials.get("ConceptFlesh") or bpy.data.materials.new("ConceptFlesh")
    fl.diffuse_color = (0.55, 0.30, 0.28, 1)
    me.materials.append(bonem); me.materials.append(sin)
    for f in me.polygons: f.use_smooth = True
    flesh = bpy.data.objects["BladeFlesh"]   # the concept shows the flesh in one plain colour
    flesh.data.materials.clear(); flesh.data.materials.append(fl)
    return name, variant


def shots(variants=("barbed", "rows", "toggle")):
    """Each head on the arm: the whole arm from the front and from behind, and the head close up (needs preview_t)."""
    shoulder, elbow, wrist, a, up, side = arm_frame()
    mid = (shoulder + wrist + a * 25) / 2
    show = ["MaleUnderwearBody:0", "HandMaleBig3rd", "BladeFlesh", "TendrilConcept"]
    out = []
    for v in variants:
        make_concept(v)
        out.append(preview("tendril_%s_front" % v, show, mid + Vector((0, 0, 8)), (90, 0, 165), 125))
        out.append(preview("tendril_%s_back" % v, show, mid + Vector((0, 0, 8)), (80, 0, 30), 125))
        out.append(preview("tendril_%s_head" % v, ["TendrilConcept", "HandMaleBig3rd", "BladeFlesh"],
                           wrist + a * 12, (75, 0, 120), 70))
    return out
