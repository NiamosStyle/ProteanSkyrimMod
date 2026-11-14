
# (Hammerfist copy, 2026-11-05)

import bpy, bmesh, math
from mathutils import Vector, noise

F = dict(FORE=3.5, UPPER=1.2, HAND=2.5, BACKFILL=5.0, LUMP=0.7, LUMP_SCALE=0.3, HAND_SMOOTH=30, SUBDIV=1,
         CUTS=2, TAPER=8, SINK=0.6, CAP=10.0, CAP_SOFT=0.1, THUMB_TUCK=0.8, THUMB_KEEP=2.5, CORDS=5, TWIST=0.12, CORD=2.6, CORD_SHARP=3.0, WARP=1.8, WARP_SCALE=0.12)

# two looks (2026-11-04): "lumpy" is the one seen in game (approved shape); "corded" is many narrower muscle cords
# running along the arm with rounded tops and narrow grooves, and far fewer random lumps (user: "more sinewy")
PRESETS = {
    "lumpy": F,
    "corded": dict(F, CORDS=9, TWIST=0.04, CORD=2.2, CORD_SHARP=0.6, ROUND=True, WARP=0.7, LUMP=0.15, CAP_FADE=True),
    # "vines" (2026-11-04, user: the corded forearm and hand were far too swollen; more tendons and fibres,
    # almost like vines): a slimmer forearm and hand wrapped in thin raised strands that twist round the arm,
    # with a second set twisting the other way so they cross and braid
    "vines": dict(F, FORE=1.6, HAND=1.0, BACKFILL=2.5, LUMP=0.12, CORDS=6, TWIST=0.10, CORD=2.6,
                  CORD_SHARP=2.5, WARP=0.8, WARP_SCALE=0.15, CORDS2=4, TWIST2=-0.16, CORD2=0.5, CAP=9.0, CAP_FADE=True, HAND_CORD=1.0, TIP=14.0, TIP_MIN=0.15,
                  EDGE=True, EDGE_R=8.0, EDGE_FINGER=16.0, EDGE_FREQ=3.5, EDGE_SHARP=2.0, EDGE_NOISE=1.5, EDGE_SCALE=0.4, EDGE_RELAX=6, TAPER=4),
}
# the Hammerfist's arm (2026-11-05, the user's brief: heavy and organic, mostly muscle): the Blade's big sleeve with
# rounded muscle cords, swollen toward the wrist, no cap on the forearm's girth, thinned past the wrist where the fist
# takes over
PRESETS["muscle"] = dict(PRESETS["vines"], FORE=4.5, UPPER=2.4, HAND=2.0, BACKFILL=2.0, LUMP=0.35, LUMP_SCALE=0.22,
                         CORDS=7, TWIST=0.05, CORD=3.2, CORD_SHARP=0.7, ROUND=True, CORDS2=0, WARP=1.0, WARP_SCALE=0.12,
                         CAP=14.0, CAP_FADE=True, HAND_CORD=0.6, TIP=10.0, TIP_MIN=0.3, SWELL=0.45, THUMB_TUCK=0.0)
# the Rending Claws' arm (2026-11-13, the user's sculpt reference Prototype/Razor_claws_sculpt.jpg): stringy sinew, many
# thin tendons running down the arm with a second set crossing them; slim (the claws are not "heavy"); the fingers kept
# apart and thin, so the hand still works under the claws
PRESETS["sinew"] = dict(PRESETS["vines"], FORE=0.7, UPPER=0.7, HAND=0.7, FINGER=0.25, BACKFILL=0.5, LUMP=0.15, LUMP_SCALE=0.25,
                        CORDS=12, TWIST=0.03, CORD=1.6, CORD_SHARP=1.5, CORDS2=3, TWIST2=-0.10, CORD2=0.8,
                        WARP=1.7, WARP_SCALE=0.13, CAP=9.5, CAP_FADE=True, HAND_CORD=0.15, TIP=0, HAND_SMOOTH=0,
                        THUMB_TUCK=0.0, FINGERS_FREE=True)
STYLE = "sinew"

# arm bone positions (world, game units): shoulder-ish, elbow, wrist
UPPER_A, ELBOW, WRIST = Vector((12.97, -5.23, 106.52)), Vector((22.41, -5.09, 85.73)), Vector((28.94, 1.79, 72.79))

def around(co):
    """Distance along the arm (from the shoulder) and the angle round it."""
    segs = [(UPPER_A, ELBOW, 0.0), (ELBOW, WRIST, (ELBOW - UPPER_A).length)]
    best = None
    for a, b, s0 in segs:
        ax = (b - a); L = ax.length; ax = ax / L
        t = max(0.0, min(L, (co - a).dot(ax)))
        d = co - (a + ax * t)
        if best is None or d.length < best[0]:
            ref = Vector((0, -1, 0)); ref = (ref - ax * ref.dot(ax)).normalized()
            side = ax.cross(ref)
            best = (d.length, s0 + t, math.atan2(d.dot(side), d.dot(ref)))
    return best[1], best[2]

def around_both(co):
    """(weight, distance along, angle) for each arm segment, weights blending smoothly near the elbow."""
    segs = [(UPPER_A, ELBOW, 0.0), (ELBOW, WRIST, (ELBOW - UPPER_A).length)]
    out = []
    for a, b, s0 in segs:
        ax = (b - a); L = ax.length; ax = ax / L
        t = max(0.0, min(L, (co - a).dot(ax)))
        d = co - (a + ax * t)
        ref = Vector((0, -1, 0)); ref = (ref - ax * ref.dot(ax)).normalized()
        side = ax.cross(ref)
        out.append([1.0 / (d.length + 0.5) ** 6, s0 + t, math.atan2(d.dot(side), d.dot(ref))])
    tot = sum(o[0] for o in out)
    for o in out: o[0] /= tot
    return out

def make_flesh(p=None):
    p = p or PRESETS[STYLE]
    base = bpy.data.objects["RCBase"]
    me = base.data.copy()
    idx = {g.index: g.name for g in base.vertex_groups}
    bm = bmesh.new(); bm.from_mesh(me)
    # the game's hand and arm are separate pieces that only touch at the wrist: stitch them, or the wrist opens
    _dl = bm.verts.layers.deform.active
    _weld = [v for v in bm.verts if not p.get("FINGERS_FREE") or
             sum(w for gi, w in v[_dl].items() if "Finger" in idx[gi]) < 0.3]   # never the fingers to each other
    bmesh.ops.remove_doubles(bm, verts=_weld, dist=0.3)
    for _ in range(p["CUTS"]):   # more points so the cords have something to shape
        bmesh.ops.subdivide_edges(bm, edges=bm.edges[:], cuts=1, use_grid_fill=True, smooth=0.5)
    dl = bm.verts.layers.deform.active
    def share(v, keys):
        return sum(w for gi, w in v[dl].items() if any(idx[gi].startswith(k) for k in keys))
    if p.get("EDGE"):
        # the open edge on the back and chest followed the game body's big triangles: straight lines and right angles
        # (user, 2026-11-04). Cut it again here, on the finer points, along a wandering line: how much a point follows
        # the arm, give or take noise, so the flesh ends ragged and reaches out onto the body in places
        ARMK = ("NPC R Clavicle", "NPC R UpperArm", "NPC R Upperarm", "NPC R Forearm", "NPC R Hand", "NPC R Finger")
        # how far the flesh reaches from the shoulder depends on the direction: a few long fingers between short
        # stretches, and a fine wobble along the edge
        sc = p["EDGE_SCALE"]
        def field(v):
            d = v.co - UPPER_A
            n = noise.noise(d.normalized() * p["EDGE_FREQ"] + Vector((3.0, 1.0, 5.0)))
            reach = p["EDGE_R"] + p["EDGE_FINGER"] * min(1.0, max(0.0, n / 0.4)) ** p["EDGE_SHARP"]   # n is mostly within +-0.4
            reach += p["EDGE_NOISE"] * noise.noise(v.co * sc)
            return share(v, ARMK) >= 0.6 or d.length < reach
        keep = {v: field(v) for v in bm.verts}
        bmesh.ops.delete(bm, geom=[f for f in bm.faces if not all(keep[v] for v in f.verts)], context='FACES_ONLY')
        bmesh.ops.delete(bm, geom=[v for v in bm.verts if not v.link_faces], context='VERTS')
        # only the piece joined to the arm: noise leaves little islands of flesh on the back
        seen, parts = set(), []
        for f0 in bm.faces:
            if f0 in seen: continue
            part, todo = [], [f0]; seen.add(f0)
            while todo:
                f = todo.pop(); part.append(f)
                for e in f.edges:
                    for g in e.link_faces:
                        if g not in seen: seen.add(g); todo.append(g)
            parts.append(part)
        parts.sort(key=len)
        bmesh.ops.delete(bm, geom=[f for part in parts[:-1] for f in part], context='FACES_ONLY')
        bmesh.ops.delete(bm, geom=[v for v in bm.verts if not v.link_faces], context='VERTS')
        # round off the little steps left along the edge: each edge point moves toward the middle of its neighbours on it
        for _ in range(p["EDGE_RELAX"]):
            moves = []
            for v in bm.verts:
                nb = [e.other_vert(v) for e in v.link_edges if e.is_boundary]
                if len(nb) == 2:
                    moves.append((v, (nb[0].co + nb[1].co) / 2))
            for v, c in moves: v.co = v.co.lerp(c, 0.5)
    hand = [v for v in bm.verts if share(v, ("NPC R Hand", "NPC R Finger")) > 0.5]
    # the thumb stuck out as a big lump (user, 2026-11-04): pull it in toward the line of the forearm
    line = (WRIST - ELBOW).normalized()
    for v in bm.verts:
        th = share(v, ("NPC R Finger0",))
        if th > 0:
            on = WRIST + line * max(0.0, (v.co - WRIST).dot(line))
            v.co = v.co.lerp(on + (v.co - on).normalized() * p["THUMB_KEEP"], p["THUMB_TUCK"] * min(1.0, th * 1.5))
    for _ in range(p["HAND_SMOOTH"]):   # melt the fingers into one mass
        bmesh.ops.smooth_vert(bm, verts=hand, factor=0.5, use_axis_x=True, use_axis_y=True, use_axis_z=True)
    bm.normal_update()
    # how far each point is (in edges) from the open edge on the shoulder and chest: the flesh thins to nothing
    # there and dips just under the skin, so it grows out of the body rather than ending in a rim
    ring = {v: None for v in bm.verts}
    edge = [v for v in bm.verts if any(e.is_boundary for e in v.link_edges)]
    for v in edge: ring[v] = 0
    front, k = edge, 0
    while front and k < p["TAPER"]:
        k += 1; nxt = []
        for v in front:
            for e in v.link_edges:
                o = e.other_vert(v)
                if ring[o] is None: ring[o] = k; nxt.append(o)
        front = nxt
    back = Vector((0, -1, 0))
    moves, groove = [], []
    for v in bm.verts:
        fore = share(v, ("NPC R Forearm",))
        upper = share(v, ("NPC R UpperArm", "NPC R Upperarm", "NPC R Clavicle"))
        hd = share(v, ("NPC R Hand", "NPC R Finger"))
        fing = share(v, ("NPC R Finger",)) if p.get("FINGERS_FREE") else 0.0
        th = p["FORE"] * fore + p["UPPER"] * upper + p["HAND"] * (hd - fing) + p.get("FINGER", p["HAND"]) * fing
        hd_c = hd - fing   # the cords stop at the knuckles
        th += p["BACKFILL"] * (fore + hd * 0.4) * max(0.0, v.normal.dot(back)) ** 1.5   # toward the blade
        lump = noise.fractal(v.co * p["LUMP_SCALE"], 1.0, 2.0, 3)
        th *= 1.0 + p["LUMP"] * lump * (1 - 0.6 * hd)   # the hand a smoother mass
        # twisted cords spiralling down the arm, bent off true by noise so they wander and knot
        warp = p["WARP"] * noise.noise(v.co * p["WARP_SCALE"])
        cord = 0.0
        for wgt, s, ang in around_both(v.co):
            phase = p["CORDS"] * ang + p["TWIST"] * s * p["CORDS"] + warp
            if p.get("ROUND"):   # rounded cords, narrow grooves between them
                c = abs(math.sin(phase / 2)) ** p["CORD_SHARP"]
            else:                # narrow ridges, broad hollows
                c = (0.5 + 0.5 * math.cos(phase)) ** p["CORD_SHARP"]
            if p.get("CORDS2"):  # a second set twisting the other way, crossing over the first
                ph2 = p["CORDS2"] * ang + p["TWIST2"] * s * p["CORDS2"] - warp
                c = max(c, p["CORD2"] * (0.5 + 0.5 * math.cos(ph2)) ** p["CORD_SHARP"])
            cord += wgt * c
        th += p["CORD"] * (fore + upper * 0.6 + hd_c * p.get("HAND_CORD", 0.3)) * cord
        if p.get("SWELL"):   # the forearm thickens toward the wrist, into the fist
            from mathutils.geometry import intersect_point_line as _ipl
            _q, _k = _ipl(v.co, ELBOW, WRIST)
            th *= 1.0 + p["SWELL"] * max(0.0, min(1.0, _k)) ** 1.5 * fore
        if p.get("TIP"):   # past the wrist the strands have nothing to wrap round and bunched into two balls at
            # the end of the hand (user, 2026-11-04): thin everything out toward the tip instead
            past = (v.co - WRIST).dot(line)
            th *= max(p["TIP_MIN"], min(1.0, 1.0 - past / p["TIP"]))
        groove.append(max(0.0, min(1.0, (1 - cord) * 0.7 - lump * 0.6)))   # deep places, for the texture
        taper = 1.0 if ring[v] is None else ring[v] / p["TAPER"]
        moves.append((v, v.normal * (max(0.22 if fing > 0.5 else 0.3, th) * taper - p["SINK"] * (1 - taper))))
    for v, d in moves: v.co += d
    # nothing on the forearm grows far past the rest: a tongue of flesh stuck out above the wrist (user, 2026-11-04)
    from mathutils.geometry import intersect_point_line
    for v in bm.verts:
        q, k = intersect_point_line(v.co, ELBOW, WRIST)
        # the limit fades in past the elbow and out past the wrist (a hard start left a crease round the arm)
        f = min(max(0.0, min(1.0, (k + 0.05) / 0.2)), max(0.0, min(1.0, (1.3 - k) / 0.2))) if p.get("CAP_FADE") else float(0.0 < k < 1.15)
        if f > 0.0:
            kk = max(0.0, k); c = ELBOW.lerp(WRIST, kk); d = v.co - c; r = d.length
            cap = p["CAP"] + (1 - f * f * (3 - 2 * f)) * 15.0
            if r > cap:
                v.co = c + d * ((cap + (r - cap) * p["CAP_SOFT"]) / r)
    name = "RCFlesh"
    ob = bpy.data.objects.get(name)
    bm.to_mesh(me); bm.free()
    at = me.attributes.new("groove", 'FLOAT', 'POINT'); at.data.foreach_set("value", groove)
    if ob:
        old = ob.data; ob.data = me; bpy.data.meshes.remove(old)
    else:
        ob = bpy.data.objects.new(name, me)
        for g in base.vertex_groups: ob.vertex_groups.new(name=g.name)
        bpy.data.scenes["Claws"].collection.objects.link(ob)
    mat = bpy.data.materials.get("Flesh") or bpy.data.materials.new("Flesh")
    mat.diffuse_color = (0.30, 0.04, 0.05, 1)
    me.materials.clear(); me.materials.append(mat)
    for f in me.polygons: f.use_smooth = True
    sub = ob.modifiers.get("Smooth") or ob.modifiers.new("Smooth", 'SUBSURF')
    sub.levels = p["SUBDIV"]
    return ob
