
# (Hammerfist copy, 2026-11-05)

# The Blade's female arm (2026-11-04): the male flesh (BladeFlesh) carried onto the game's female body. The female
# skeleton's arm bones sit where the male's do (only the hand is scaled, 0.85), so the gaps seen in game (user,
# screenshots 59 and 60: by the neck and shoulder) come from her narrower body. Each point of the flesh keeps where it
# stands over the male skin, now over the female skin: its nearest male skin point is found, then the nearest female
# skin point to that, and the flesh moves by the difference (smoothed over the flesh, so no point jumps alone).
# Same points, triangles and texture layout as the male flesh, so the same textures serve both.
import bpy, bmesh
from mathutils.bvhtree import BVHTree

FM = dict(SMOOTH=20)
MALE = ("MaleUnderwearBody:0", "HandMaleBig3rd")
FEMALE = ("FemaleUnderwearBody:0", "HandFemale3rd")


def tree(names):
    bm = bmesh.new()
    for n in names:
        ob = bpy.data.objects[n]
        me = ob.data.copy(); me.transform(ob.matrix_world)
        bm.from_mesh(me); bpy.data.meshes.remove(me)
    t = BVHTree.FromBMesh(bm); bm.free()
    return t


def make_female(p=FM):
    src = bpy.data.objects["RCFlesh"]
    male, female = tree(MALE), tree(FEMALE)
    me = src.data.copy()
    mw = src.matrix_world
    bm = bmesh.new(); bm.from_mesh(me)
    bm.verts.ensure_lookup_table()
    delta = []
    for v in bm.verts:
        co = mw @ v.co
        pm = male.find_nearest(co)[0]
        pf = female.find_nearest(pm)[0]
        delta.append(pf - pm)
    for _ in range(p["SMOOTH"]):
        delta = [(delta[v.index] + sum((delta[e.other_vert(v).index] for e in v.link_edges), delta[v.index] * 0)) /
                 (1 + len(v.link_edges)) for v in bm.verts]
    inv = mw.inverted_safe().to_3x3()
    # her hand (2026-11-13, the claws: the fingers stay apart here, so they must sit on her own finger bones): her hand
    # and finger bones are the male's moved and scaled (0.85). Each point of the hand goes where those bones carry it,
    # blended with the body's way above by how much of the point follows the hand.
    import json, os
    from mathutils import Vector
    bones = json.load(open(os.path.join(os.path.dirname(bpy.data.filepath), "finger_bones.json")))
    gname = {g.index: g.name.split(".")[0] for g in src.vertex_groups}
    dl = bm.verts.layers.deform.active
    for v in bm.verts:
        co = mw @ v.co
        tot, acc = 0.0, Vector((0, 0, 0))
        for gi, w in v[dl].items():
            b = gname.get(gi)
            if b in bones["M"] and w > 0:
                tm, tf, s = Vector(bones["M"][b]["t"]), Vector(bones["F"][b]["t"]), bones["F"][b]["s"]
                acc += (tf + (co - tm) * s) * w; tot += w
        share = min(1.0, sum(w for gi, w in v[dl].items() if gname.get(gi)))
        h = tot / share if share > 0 else 0.0
        target = co + delta[v.index]
        if tot > 0:
            target = target.lerp(acc / tot, min(1.0, h))
        delta[v.index] = target - co
    for v in bm.verts:
        v.co += inv @ delta[v.index]
    bm.to_mesh(me); bm.free()
    ob = bpy.data.objects.get("RCFleshF")
    if ob:
        old = ob.data; ob.data = me; bpy.data.meshes.remove(old)
    else:
        ob = bpy.data.objects.new("RCFleshF", me)
        bpy.data.scenes["Claws"].collection.objects.link(ob)
    ob.matrix_world = mw.copy()
    # the groups' names, in the male's order (the weights came with the copied points; clearing the groups wipes them)
    if [g.name for g in ob.vertex_groups] != [g.name for g in src.vertex_groups]:
        assert not ob.vertex_groups, "BladeFleshF has other groups: delete it and run again"
        for g in src.vertex_groups: ob.vertex_groups.new(name=g.name)
    sub = ob.modifiers.get("Smooth") or ob.modifiers.new("Smooth", 'SUBSURF'); sub.levels = 1
    mx = max(d.length for d in delta)
    return {"verts": len(me.vertices), "largest move": round(mx, 2)}
