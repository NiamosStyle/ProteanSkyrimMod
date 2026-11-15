# The Tendril sleeve's sinew and bone, for the game (2026-11-14; the concepts are make_concept_t.py). Two versions:
#   "rest"   weapons away: the harpoon grows out of the heel of the palm and lies along the fingers
#   "drawn"  weapons out, 1st person only: it stands straight out of the palm through the 1st-person spell's starting
#            point, so the thrown tendril leaves from the spike (user, 2026-11-14: at the palm's centre it sat below
#            where the tendril came out). In 3rd person the "rest" one stays on with weapons out too (user, same day).
# (user: "look like 2 when relaxed and 1 when you draw your weapon"). Each makes two objects, TBone_<mode> (the
# harpoon, head A: a spear point with three hooks raking back, like the whip's barb) and TSinew_<mode> (cords wound
# down the forearm onto the shaft, and small tentacles curling off the shoulder), with texture coordinates and the
# game's bone weights.
# Textures, all the game's or Protean's own already: the cords and tentacles wear the whip's sinew (darktentacle01's
# left strip, tiling along them), the harpoon the claws' and whip's horn (Hearthfire's horn01), the flesh the Blade's
# Arm textures (TendrilFlesh keeps BladeFlesh's layout).
import bpy, bmesh, math
from mathutils import Vector, Matrix
from mathutils.bvhtree import BVHTree
from mathutils.kdtree import KDTree

SCENE = "Blade"
SHAFT_R = 2.3        # the shaft's radius at its root (tapering to the head)
POINT = 15.0         # the head's length
PALM_OUT = 20.0      # drawn: how far the point stands out of the palm
# drawn: the 1st-person skeleton's "NPC R MagicNode [RMag]" (where the spell leaves the hand) carried onto the
# 3rd-person hand, in the body's space: tools/measure_magic_node.py. 8.4 units along the hand from the wrist, 3.4 out
# of the palm; the palm's centre, where the harpoon stood first, is 4.6 along.
MAGIC_1ST = Vector((28.204, 6.431, 64.578))
FINGER_OUT = 30.0    # rest: how far past the palm's centre along the fingers
TENTACLES = 6        # off the shoulder (8 of 16-28 units were too big: user, 2026-11-14)
TENT_LEN = (8.0, 14.0)
TENT_R = 1.3
CORDS = 5
SINEW_U = (0.01, 0.23)   # the sinew strip of darktentacle01.dds (as the whip, tools/build_tendril.py)
SINEW_TILE = 25.0        # units of cord per repeat
HORN_UV = (70 / 256, 165 / 512, 120 / 256, 330 / 512)   # u left, v base, u right, v tip: horn01's dark half
HAND = "NPC R Hand [RHnd]"
FOREARM = "NPC R ForearmTwist1 [RLt1]"   # the forearm's wrist end: turns with the forearm, not the wrist


# ---------------------------------------------------------------------------------------------------- the arm
def bones():
    arm = bpy.data.objects["MaleBody_1.nif:ARMATURE"]
    mw = arm.matrix_world
    return lambda n: mw @ arm.data.bones[n].head_local


def arm_frame():
    h = bones()
    shoulder, elbow, wrist = h("NPC R UpperArm [RUar]"), h("NPC R Forearm [RLar]"), h(HAND)
    a = (wrist - elbow).normalized()
    up = Vector((0, 0, 1)); up = (up - a * up.dot(a)).normalized()
    return shoulder, elbow, wrist, a, up, a.cross(up).normalized()


def palm_frame():
    """The palm's centre, its outward normal (the side a spell leaves from), the hand's line (wrist to fingers)."""
    h = bones()
    w, f1, f2, f4 = h(HAND), h("NPC R Finger10 [RF10]"), h("NPC R Finger20 [RF20]"), h("NPC R Finger40 [RF40]")
    along = (f2 - w).normalized()
    n = along.cross((f1 - f4).normalized()).normalized()
    if (h("NPC R Finger02 [RF02]") - w).dot(n) < 0:   # the thumb tip curls toward the palm side
        n = -n
    return w + (f2 - w) * 0.55, n, along


class Flesh:
    """TendrilFlesh as a surface to lay things on, and its weights to borrow."""
    def __init__(self, name="TendrilFlesh"):
        ob = bpy.data.objects[name]
        me = ob.data
        self.co = [ob.matrix_world @ v.co for v in me.vertices]
        self.bvh = BVHTree.FromPolygons(self.co, [tuple(f.vertices) for f in me.polygons])
        self.kd = KDTree(len(self.co))
        for i, c in enumerate(self.co):
            self.kd.insert(c, i)
        self.kd.balance()
        names = {g.index: g.name for g in ob.vertex_groups}
        self.w = [{names[g.group].split(".")[0]: g.weight for g in v.groups if g.weight > 0 and names[g.group].startswith("NPC ")}
                  for v in me.vertices]

    def top(self, at, a, up, slab=3.0):
        """How far the flesh stands above the line through at, toward up."""
        return max([(c - at).dot(up) for c in self.co if abs((c - at).dot(a)) < slab] or [0.0])

    def surface(self, p, c, lift):
        d = (p - c).normalized()
        hit = self.bvh.ray_cast(c + d * 40, -d, 40)[0]
        return (hit + d * lift) if hit else p

    def weights(self, p, k=4):
        """The flesh's weights near p, blended by distance."""
        tot = {}
        near = self.kd.find_n(p, k)
        s = 0.0
        for _, i, dist in near:
            f = 1.0 / (dist + 0.3)
            s += f
            for b, w in self.w[i].items():
                tot[b] = tot.get(b, 0.0) + w * f
        return {b: w / s for b, w in tot.items()}


# ---------------------------------------------------------------------------------------------------- building
class Builder:
    def __init__(self):
        self.verts, self.wts, self.faces = [], [], []   # faces: (indices, corner uvs)

    def vert(self, co, w):
        self.verts.append(co.copy()); self.wts.append(w)
        return len(self.verts) - 1

    def tube(self, pts, radii, sides, wfn, tile=SINEW_TILE, u=SINEW_U, cap_end=True):
        rings, vs, run = [], [], 0.0
        for i, p in enumerate(pts):
            t = (pts[min(i + 1, len(pts) - 1)] - pts[max(i - 1, 0)]).normalized()
            ref = Vector((0, 0, 1)) if abs(t.z) < 0.9 else Vector((1, 0, 0))
            x = t.cross(ref).normalized(); y = t.cross(x)
            w = wfn(p, i / (len(pts) - 1))
            rings.append([self.vert(p + (x * math.cos(2 * math.pi * k / sides) + y * math.sin(2 * math.pi * k / sides)) * radii[i], w)
                          for k in range(sides)])
            if i:
                run += (p - pts[i - 1]).length
            vs.append(run / tile)
        uu = lambda k: u[0] + (u[1] - u[0]) * k / sides
        for i in range(len(rings) - 1):
            r0, r1 = rings[i], rings[i + 1]
            for k in range(sides):
                k1 = (k + 1) % sides
                self.faces.append(((r0[k], r0[k1], r1[k1], r1[k]),
                                   ((uu(k), vs[i]), (uu(k + 1), vs[i]), (uu(k + 1), vs[i + 1]), (uu(k), vs[i + 1]))))
        if cap_end:
            t = (pts[-1] - pts[-2]).normalized()
            tip = self.vert(pts[-1] + t * radii[-1], wfn(pts[-1], 1.0))
            r = rings[-1]
            for k in range(sides):
                self.faces.append(((r[k], r[(k + 1) % sides], tip),
                                   ((uu(k), vs[-1]), (uu(k + 1), vs[-1]), ((uu(k) + uu(k + 1)) / 2, vs[-1] + 0.05))))

    def horn(self, base, d, wdir, length, width, thick, wfn, n=14):
        """A flat, tapering horn with a central ridge; textured with horn01, dark at its base."""
        nrm = d.cross(wdir).normalized()
        u0, v0, u1, v1 = HORN_UV
        prof = [(0.5, 0.0), (0.15, 0.5), (-0.15, 0.5), (-0.5, 0.0), (-0.15, -0.5), (0.15, -0.5)]
        us = [0.0, 0.33, 0.5, 1.0, 0.5, 0.33]
        rows = []
        for i in range(n + 1):
            t = i / n
            c = base + d * (length * t)
            wd = width * (1 - t) ** 0.8 * (1 + 0.25 * math.sin(math.pi * min(1, t * 2)))
            th = thick * (1 - t) ** 0.6 + 0.08
            rows.append([self.vert(c + wdir * (px * wd) + nrm * (py * th), wfn(c, t)) for px, py in prof])
        uv = lambda k, t: (u0 + (u1 - u0) * us[k], v0 + (v1 - v0) * t)
        for i in range(n):
            for k in range(6):
                k1 = (k + 1) % 6
                self.faces.append(((rows[i][k], rows[i][k1], rows[i + 1][k1], rows[i + 1][k]),
                                   (uv(k, i / n), uv(k1, i / n), uv(k1, (i + 1) / n), uv(k, (i + 1) / n))))
        tip = self.vert(base + d * (length * 1.02), wfn(base + d * length, 1.0))
        for k in range(6):
            k1 = (k + 1) % 6
            self.faces.append(((rows[n][k], rows[n][k1], tip), (uv(k, 1.0), uv(k1, 1.0), uv(k, 1.0))))

    def object(self, name, materials):
        me = bpy.data.meshes.new(name)
        me.from_pydata([tuple(v) for v in self.verts], [], [f[0] for f in self.faces])
        uvl = me.uv_layers.new(name="UVMap")
        for poly, (_, uvs) in zip(me.polygons, self.faces):
            for li, (u, v) in zip(poly.loop_indices, uvs):
                uvl.data[li].uv = (u, 1.0 - v)   # the export turns v back (the game counts from the top)
        for p in me.polygons:
            p.use_smooth = True
        ob = bpy.data.objects.get(name)
        if ob:
            ob.vertex_groups.clear()
            old = ob.data; ob.data = me; bpy.data.meshes.remove(old)
        else:
            ob = bpy.data.objects.new(name, me)
            bpy.data.scenes[SCENE].collection.objects.link(ob)
        ob.matrix_world = Matrix.Identity(4)
        groups = {}
        for i, w in enumerate(self.wts):
            for b, x in w.items():
                if b not in groups:
                    groups[b] = ob.vertex_groups.new(name=b)
                groups[b].add([i], x, 'REPLACE')
        for m in materials:
            ob.data.materials.append(m)
        return ob


def material(name, colour):
    m = bpy.data.materials.get(name) or bpy.data.materials.new(name)
    m.diffuse_color = colour
    return m


# ---------------------------------------------------------------------------------------------------- the parts
def make_parts(mode="drawn"):
    fl = Flesh()
    shoulder, elbow, wrist, a, up, side = arm_frame()
    centre, n, along = palm_frame()
    L = (wrist - elbow).length
    if mode == "drawn":
        out = (MAGIC_1ST - wrist).dot(n)                  # the node's height above the palm's line
        root = MAGIC_1ST - n * (out + 1.5) - along * 1.0  # rooted in the hand, under the node
        tip = MAGIC_1ST + n * (PALM_OUT - out)
        harpoon_w = lambda p, t: {HAND: 1.0}   # the whole harpoon rides on the hand
    else:
        nf = (n - a * n.dot(a)).normalized()   # the palm side of the forearm
        at = elbow + a * (L * 0.7)
        root = at + nf * (fl.top(at, a, nf) - SHAFT_R * 1.2)
        tip = centre + along * FINGER_OUT + n * 3.0

        def harpoon_w(p, t):   # behind the wrist on the forearm, ahead of it on the hand, blended across the wrist
            h = min(1.0, max(0.0, ((p - wrist).dot(a) + 2.0) / 5.0))
            return {HAND: h, FOREARM: 1 - h} if 0 < h < 1 else ({HAND: 1.0} if h >= 1 else {FOREARM: 1.0})
    ax = (tip - root).normalized()
    u = (up - ax * up.dot(ax)).normalized() if abs(up.dot(ax)) < 0.95 else (side - ax * side.dot(ax)).normalized()
    sd = ax.cross(u).normalized()
    neck = tip - ax * POINT

    # the harpoon
    bone = Builder()
    m = 16
    bone.tube([root + (neck - root) * (i / m) for i in range(m + 1)],
              [SHAFT_R * (1.0 - 0.45 * i / m) * (1 + 0.08 * math.sin(i * 1.7)) for i in range(m + 1)], 12, harpoon_w,
              tile=(neck - root).length * 1.6, u=(HORN_UV[0], HORN_UV[2]), cap_end=False)
    bone.tube([neck - ax * 1.2, neck + ax * 1.2], [SHAFT_R * 0.85, SHAFT_R * 0.75], 12, harpoon_w, tile=20.0,
              u=(HORN_UV[0], HORN_UV[2]), cap_end=False)   # a collar under the head
    bone.horn(neck, ax, sd, POINT, 5.5, 3.0, harpoon_w)
    bone.horn(neck, ax, u, POINT, 4.0, 3.0, harpoon_w)
    for k in range(3):   # three hooks raking back from the head's base
        ang = 2 * math.pi * k / 3 + 0.5
        o = u * math.cos(ang) + sd * math.sin(ang)
        bone.horn(neck + ax * 3.5 + o * 1.6, (-ax * 1.5 + o).normalized(), o.cross(ax).normalized(), 10.0, 3.4, 1.9, harpoon_w)

    # sinew: cords wound down the forearm and onto the shaft
    sin = Builder()
    def cord_w(p, t):
        if t > 0.8:   # over the shaft: ride with it
            return harpoon_w(p, t)
        return fl.weights(p)
    for c in range(CORDS):
        ph = 2 * math.pi * c / CORDS
        N = 40
        out = []
        for i in range(N + 1):
            t = i / N
            ang = ph + t * 3.2
            c0 = elbow - a * 3 + a * (L + 3) * t
            out.append((fl.surface(c0 + (up * math.cos(ang) + side * math.sin(ang)) * 6.0, c0, 0.0) - c0).length)
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
                s = root + (neck - root) * (0.15 + 0.55 * k)
                q = s + (u * math.cos(ang * 1.4) + sd * math.sin(ang * 1.4)) * SHAFT_R * 1.05
                p = p.lerp(q, min(1, k * 1.6))
            pts.append(p); rad.append(0.8 + 0.3 * math.sin(math.pi * t) - 0.3 * max(0, t - 0.7))
        sin.tube(pts, rad, 8, cord_w)
    # tentacles curling off the shoulder: each rides on the weights where it grows
    ua = (elbow - shoulder).normalized()
    sup = Vector((0, 0, 1)); sup = (sup - ua * sup.dot(ua)).normalized()
    sside = ua.cross(sup)
    for c in range(TENTACLES):
        ang = -1.5 + 3.0 * c / (TENTACLES - 1)
        o = sup * math.cos(ang) + sside * math.sin(ang)
        c0 = shoulder + ua * (2 + 4 * (c % 3))
        base = fl.surface(c0 + o * 6.0, c0, -1.0)
        wb = fl.weights(base)
        length = TENT_LEN[0] + (TENT_LEN[1] - TENT_LEN[0]) * ((c * 37) % 5) / 4
        r0 = TENT_R - 0.15 * (c % 3)
        d = (o * 0.7 - ua * 0.7).normalized()
        axis = d.cross(ua).normalized()
        twist = (c % 2) * 2 - 1
        p, pts, rad, steps = base.copy(), [], [], 24
        for i in range(steps + 1):
            t = i / steps
            pts.append(p.copy()); rad.append(r0 * (1 - t) ** 1.1 + 0.12)
            d = Matrix.Rotation(0.05 - (0.02 + 0.5 * t ** 3), 3, axis) @ d
            d = (Matrix.Rotation(twist * 0.04, 3, o) @ d).normalized()
            p = p + d * (length / steps)
        sin.tube(pts, rad, 8, lambda q, t, wb=wb: wb)

    b = bone.object("TBone_" + mode, [material("ConceptBone", (0.86, 0.80, 0.66, 1))])
    s = sin.object("TSinew_" + mode, [material("ConceptSinew", (0.45, 0.16, 0.15, 1))])
    return {b.name: len(b.data.vertices), s.name: len(s.data.vertices)}


def make_all():
    return [make_parts("rest"), make_parts("drawn")]
