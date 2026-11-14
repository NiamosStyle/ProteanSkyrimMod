# The Rending Claws' claws (2026-11-13; the user's sculpt reference, Prototype/Razor_claws_sculpt.jpg): one long curved
# blade a finger, saw-toothed along the inner edge, each ONE solid piece on its finger's first bone, so it turns at the
# knuckle and nowhere else (the user: "they only have one knuckle ... the rest of the blade must be solid"). A knot of
# sinew at each root, tendons over the back of the hand to the knuckles, and loose cords standing off the arm.
# Lengths: the user's pick, 80% of the reference (the longest about 19 units); tools/measure_claw_poses.py shows them
# clear of the ground in every beast-race clip (22 long: still 16 up) and of the body in the stances held.
# Built in each finger bone's own frame (x: sideways, y: the back of the hand, z: along the finger).
import bpy, bmesh, math, json, os, random
from mathutils import Vector, Matrix, noise
from mathutils.bvhtree import BVHTree
from mathutils.kdtree import KDTree

HERE = os.path.dirname(bpy.data.filepath)
BONES = json.load(open(os.path.join(HERE, "finger_bones.json")))
HAND = "NPC R Hand [RHnd]"
#        name      bone                       length  root height (H) and thickness (W)
CLAWS = [("thumb", "NPC R Finger01 [RF01]", 10.0, 2.3, 1.05),
         ("index", "NPC R Finger10 [RF10]", 17.5, 3.1, 1.3),
         ("middle", "NPC R Finger20 [RF20]", 19.0, 3.4, 1.4),
         ("ring", "NPC R Finger30 [RF30]", 17.0, 3.1, 1.3),
         ("little", "NPC R Finger40 [RF40]", 9.0, 2.1, 1.0)]
C = dict(RINGS=96, SIDES=12, LIFT=10.0, CURVE=42.0, CURVE_POW=1.25, START=(0.0, 0.75, -1.0),
         TEETH=7, TEETH_FROM=0.26, TEETH_TO=0.84, TOOTH=0.30,
         KNOT=(1.25, 1.0, 1.7), KNOT_AT=(0.0, 0.5, 0.2), KNOT_LUMP=0.3,
         FEMALE=0.92,                       # her hand is 0.85 of his; the claws a little smaller with it
         TENDON_R=0.42, LOOP_SIDES=8)
# loose cords standing off the arm (the reference's long loops): (segment, from k, angle) -> (to k, angle), lift, radius
# segment 1 = forearm (k 0 at the elbow, 1 at the wrist), 0 = upper arm (0 at the shoulder, 1 at the elbow).
# angle: 0 the back of the arm, -pi/2 the outer side, +-pi the front, +pi/2 the inner side (kept off: it faces the body)
LOOPS = [(1, 0.06, -0.3, 0.92, -0.9, 3.2, 0.48),
         (1, 0.16, -1.9, 0.97, -2.3, 2.6, 0.42),
         (1, 0.34, -1.0, 0.99, -1.5, 1.9, 0.36),
         (1, 0.10, 0.5, 0.70, 0.1, 2.2, 0.38),
         (1, 0.45, -2.7, 0.95, -3.0, 1.6, 0.32),
         (0, 0.30, -0.6, 0.88, -1.1, 2.6, 0.45),
         (0, 0.42, -1.9, 0.95, -1.6, 2.0, 0.38)]
UPPER_A, ELBOW, WRIST = Vector((12.97, -5.23, 106.52)), Vector((22.41, -5.09, 85.73)), Vector((28.94, 1.79, 72.79))


def frame(sex, bone):
    b = BONES[sex][bone]
    return Matrix(b["r"]), Vector(b["t"])


class Build:
    def __init__(self):
        self.bm = bmesh.new()
        self.w = []          # per vertex: {bone: weight}
        self.attr = {k: [] for k in ("bone", "flesh", "root", "spine")}

    def add(self, cos, weights, **attrs):
        vs = []
        for i, co in enumerate(cos):
            vs.append(self.bm.verts.new(co))
            self.w.append(weights[i] if isinstance(weights, list) else weights)
            for k in self.attr:
                a = attrs.get(k, 0.0)
                self.attr[k].append(a[i] if isinstance(a, list) else a)
        return vs

    def tube(self, rings, close_start=True, close_end=True):
        """rings: lists of BMVerts, the same count each; quads between, the ends closed with a fan."""
        n = len(rings[0])
        for a, b in zip(rings, rings[1:]):
            for i in range(n):
                self.bm.faces.new((a[i], a[(i + 1) % n], b[(i + 1) % n], b[i]))
        # the ends closed with fans (always the same triangles, so the female copy matches point for point)
        for ring, flip in ((rings[0], True), (rings[-1], False)):
            for i in range(1, n - 1):
                tri = (ring[0], ring[i], ring[i + 1])
                self.bm.faces.new(tuple(reversed(tri)) if flip else tri)


def claw(B, sex, name, bone, L, H, W, p, sc):
    R, T = frame(sex, bone)
    L, H, W = L * sc, H * sc, W * sc
    n, K = p["RINGS"], p["SIDES"]
    pos = Vector(p["START"]) * sc
    rings = []
    step = L / n
    for i in range(n + 1):
        t = i / n
        a = math.radians(p["LIFT"] - p["CURVE"] * t ** p["CURVE_POW"])
        d = Vector((0, math.sin(a), math.cos(a)))      # along the claw
        up = Vector((0, math.cos(a), -math.sin(a)))    # toward its back (the outside of the curve)
        taper = max(0.0, 1.0 - t)
        h = H * taper ** 0.8 * (0.6 + 0.4 * min(1.0, t / 0.10))     # narrower where it goes into the knuckle
        w = W * taper ** 0.65 * (0.7 + 0.3 * min(1.0, t / 0.10))
        # saw teeth along the inner edge: from root to tip each tooth stands out at once, then falls away
        tooth = 0.0
        if p["TEETH_FROM"] < t < p["TEETH_TO"]:
            u = (t - p["TEETH_FROM"]) / (p["TEETH_TO"] - p["TEETH_FROM"]) * p["TEETH"]
            tooth = p["TOOTH"] * (u - math.floor(u))
        cos_, roots, spines = [], [], []
        for k in range(K):
            ph = 2 * math.pi * k / K
            c, s = math.cos(ph), math.sin(ph)
            inner = max(0.0, (-c - 0.2) / 0.8)                       # 1 on the cutting edge
            y = h / 2 * c * (1.0 - tooth * inner)
            # a wedge: thick along the back, thinning to nothing at the cutting edge
            x = w / 2 * s * (0.12 + 0.88 * ((1 + c) / 2) ** 0.55)
            local = pos + up * (y + h / 2 * 0.15) + Vector((x, 0, 0))
            cos_.append(T + R @ local)
            roots.append(max(0.0, 1.0 - t / 0.3))
            spines.append(max(0.0, min(1.0, (c - 0.1) / 0.6)) * (1.0 - 0.55 * t))
        rings.append(B.add(cos_, {bone: 1.0}, bone=1.0, root=roots, spine=spines))
        pos = pos + d * step
    B.tube(rings)
    # the knot of sinew the claw grows from
    kb = bmesh.new(); bmesh.ops.create_icosphere(kb, subdivisions=3, radius=1.0)
    kx, ky, kz = p["KNOT"]; at = Vector(p["KNOT_AT"])
    k = H / 2.5
    cos_ = []
    order = {}
    for v in kb.verts:
        dn = v.co.normalized()
        r = 1.0 + p["KNOT_LUMP"] * noise.fractal(dn * 1.7 + Vector((hash(name) % 7, 0, 0)), 1.0, 2.0, 2)
        r += 0.18 * abs(math.sin(math.atan2(dn.x, dn.y) * 4 + dn.z * 2))      # cords round it
        local = (at + Vector((dn.x * kx, dn.y * ky, dn.z * kz)) * r) * k
        order[v] = len(cos_); cos_.append(T + R @ local)
    vs = B.add(cos_, {bone: 1.0}, flesh=1.0)
    for f in kb.faces:
        B.bm.faces.new([vs[order[v]] for v in f.verts])
    kb.free()
    return T + R @ (Vector((0, 0.9, 0.0)) * k)   # where a tendon meets the knot


def cord(B, pts, radii, weights_at, sides):
    """A round cord along pts; weights_at(point) -> {bone: weight}."""
    rings = []
    prev_n = None
    for i, c in enumerate(pts):
        a = pts[max(0, i - 1)]; b = pts[min(len(pts) - 1, i + 1)]
        d = (b - a).normalized()
        n = prev_n - d * prev_n.dot(d) if prev_n is not None else d.orthogonal()
        n.normalize(); prev_n = n
        m = d.cross(n)
        wt = weights_at(c)
        ring = [c + (n * math.cos(2 * math.pi * k / sides) + m * math.sin(2 * math.pi * k / sides)) * radii[i] for k in range(sides)]
        rings.append(B.add(ring, wt, flesh=1.0))
    B.tube(rings)


def bezier(p0, p1, p2, n):
    return [(1 - t) ** 2 * p0 + 2 * (1 - t) * t * p1 + t * t * p2 for t in (i / n for i in range(n + 1))]


def make_claws(sex="M", flesh_name="RCFlesh", out="RCClaw", p=C):
    fl = bpy.data.objects[flesh_name]
    fm = fl.data
    names = [g.name for g in fl.vertex_groups]
    fb = bmesh.new(); fb.from_mesh(fm)
    tree = BVHTree.FromBMesh(fb)
    kd = KDTree(len(fm.vertices))
    for v in fm.vertices: kd.insert(v.co, v.index)
    kd.balance()
    def weights_at(co):
        i = kd.find(co)[1]
        w = {}
        for g in fm.vertices[i].groups:
            nm = names[g.group].split(".")[0]
            if nm.startswith("NPC ") and g.weight > 0.01: w[nm] = w.get(nm, 0.0) + g.weight
        top = sorted(w.items(), key=lambda x: -x[1])[:4]; s = sum(x[1] for x in top) or 1.0
        return {b: x / s for b, x in top}
    B = Build()
    sc = 1.0 if sex == "M" else p["FEMALE"]
    HR, HT = frame(sex, HAND)
    hs = 1.0 if sex == "M" else BONES["F"][HAND]["s"]
    # the claws, and a tendon over the back of the hand to each
    for i, (name, bone, L, H, W) in enumerate(CLAWS):
        knot = claw(B, sex, name, bone, L, H, W, p, sc)
        if name == "thumb": continue
        start = HT + HR @ (Vector(((1.5 - i) * 0.9, 1.4, 0.8)) * hs)
        hit = tree.find_nearest(start)
        start = hit[0] - hit[1] * 0.1
        mid = (start + knot) / 2
        hm = tree.find_nearest(mid)
        mid = hm[0] + hm[1] * 0.2
        pts = bezier(start, mid * 2 - (start + knot) / 2, knot, 14)
        radii = [p["TENDON_R"] * sc * (0.6 + 0.4 * math.sin(math.pi * t / 14)) for t in range(15)]
        cord(B, pts, radii, lambda c: {HAND: 1.0}, 6)
    # the loose cords on the arm
    segs = {0: (UPPER_A, ELBOW), 1: (ELBOW, WRIST)}
    def on_arm(seg, k, ang):
        a, b = segs[seg]
        ax = (b - a).normalized()
        ref = Vector((0, -1, 0)); ref = (ref - ax * ref.dot(ax)).normalized()
        side = ax.cross(ref)
        c = a.lerp(b, k); d = ref * math.cos(ang) + side * math.sin(ang)
        hit = tree.ray_cast(c + d * 40.0, -d)
        if hit[0] is None: return None, None
        return hit[0] - d * 0.7, d
    # and many thinner strands lying closer to the arm (the reference is all strings): seeded, so every build is the same
    rng = random.Random(p.get("STRAND_SEED", 7))
    strands = []
    for seg, count in ((1, 11), (0, 7)):
        for _ in range(count):
            k0 = rng.uniform(0.04, 0.5); k1 = min(0.99, k0 + rng.uniform(0.3, 0.6))
            a0 = rng.uniform(-3.6, 0.5); a1 = a0 + rng.uniform(-0.7, 0.7)
            strands.append((seg, k0, a0, k1, a1, rng.uniform(0.5, 1.3), rng.uniform(0.13, 0.2)))
    made = 0
    for seg, k0, a0, k1, a1, lift, r in LOOPS + strands:
        p0, d0 = on_arm(seg, k0, a0); p2, d2 = on_arm(seg, k1, a1)
        if p0 is None or p2 is None: continue
        out_d = (d0 + d2).normalized()
        p1 = (p0 + p2) / 2 + out_d * lift * 2
        n = 30
        pts = bezier(p0, p1, p2, n)
        radii = [r * 1.7 * (0.7 + 0.3 * math.sin(math.pi * t / n) ** 0.5) for t in range(n + 1)]
        cord(B, pts, radii, weights_at, p["LOOP_SIDES"])
        made += 1
    fb.free()
    bm = B.bm
    bmesh.ops.recalc_face_normals(bm, faces=bm.faces[:])
    me = bpy.data.meshes.new(out); bm.to_mesh(me); bm.free()
    for k, vals in B.attr.items():
        me.attributes.new(k, 'FLOAT', 'POINT').data.foreach_set("value", vals)
    ob = bpy.data.objects.get(out)
    if ob:
        ob.vertex_groups.clear()
        old = ob.data; ob.data = me; bpy.data.meshes.remove(old)
    else:
        ob = bpy.data.objects.new(out, me); bpy.data.scenes["Claws"].collection.objects.link(ob)
    groups = {}
    for i, w in enumerate(B.w):
        for b, x in w.items():
            g = groups.get(b) or groups.setdefault(b, ob.vertex_groups.new(name=b))
            g.add([i], x, 'REPLACE')
    mat = bpy.data.materials.get("ClawDraft") or bpy.data.materials.new("ClawDraft"); mat.diffuse_color = (0.55, 0.5, 0.42, 1)
    me.materials.clear(); me.materials.append(mat)
    for f in me.polygons: f.use_smooth = True
    me.set_sharp_from_angle(angle=math.radians(40))
    # only the claws keep hard edges: the knots, tendons and cords are round (their few sides showed as flat strips)
    se = me.attributes.get("sharp_edge")
    if se:
        fl_ = B.attr["flesh"]
        for e in me.edges:
            if fl_[e.vertices[0]] > 0.5: se.data[e.index].value = False
    return {"verts": len(me.vertices), "tris": sum(len(f.vertices) - 2 for f in me.polygons), "loops": made, "bones": sorted(groups)}
