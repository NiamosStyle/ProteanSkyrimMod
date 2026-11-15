# The Tendril sleeve's other three arms and the export (2026-11-14), after the Hammerfist's make_female_h, mirror_h and
# export_game_h:
#   female()    each male piece carried onto the female body: every point keeps where it stood over the male skin,
#               now over the female skin (smoothed, so nothing tears); the harpoon moves whole, so it keeps its shape
#   mirror()    the left arm: the right mirrored across the body, faces turned, bone groups renamed R <-> L
#   export()    export_<side><sex>.json: for each version (rest, drawn) the flesh, the harpoon and the sinew, in game
#               units with weights (at most 4 a point); tools/build_tendril_sleeve.py makes the nifs
import bpy, bmesh, json, os, re
from mathutils import Vector
from mathutils.bvhtree import BVHTree

SCENE = "Blade"
MALE = ("MaleUnderwearBody:0", "HandMaleBig3rd")
FEMALE = ("FemaleUnderwearBody:0", "HandFemale3rd")
MODES = ("rest", "drawn", "drawn1st")   # drawn1st: the right 1st-person spike, turned to its throw (make_parts_t)


def pieces():
    """(male right object, its female name): the flesh is shared by both versions."""
    out = [("TendrilFlesh", "TendrilFleshF")]
    for m in MODES:
        out += [("TBone_" + m, "TBone_%s_F" % m), ("TSinew_" + m, "TSinew_%s_F" % m)]
    return out


def tree(names):
    bm = bmesh.new()
    for n in names:
        ob = bpy.data.objects[n]
        me = ob.data.copy(); me.transform(ob.matrix_world)
        bm.from_mesh(me); bpy.data.meshes.remove(me)
    t = BVHTree.FromBMesh(bm); bm.free()
    return t


def replace(name, me, src):
    ob = bpy.data.objects.get(name)
    if ob:
        # the groups cleared BEFORE the new points come in: clearing them after wipes the copied weights
        ob.vertex_groups.clear()
        old = ob.data; ob.data = me; bpy.data.meshes.remove(old)
    else:
        ob = bpy.data.objects.new(name, me)
        bpy.data.scenes[SCENE].collection.objects.link(ob)
    ob.matrix_world = src.matrix_world.copy()
    return ob


def same_groups(ob, src, rename=lambda n: n):
    # the groups' names live with the copied points' weights, by index: name them in the source's order
    gs = list(ob.vertex_groups)
    if not gs:
        for g in src.vertex_groups:
            ob.vertex_groups.new(name=rename(g.name))
        return
    assert len(gs) == len(src.vertex_groups), (ob.name, len(gs), len(src.vertex_groups))
    for i, g in enumerate(gs):
        g.name = "__swap%d" % i
    for g, sg in zip(gs, src.vertex_groups):
        g.name = rename(sg.name)


def carry(src_name, dst_name, male, female, rigid=False, smooth=20):
    src = bpy.data.objects[src_name]
    me = src.data.copy()
    mw = src.matrix_world
    bm = bmesh.new(); bm.from_mesh(me)
    bm.verts.ensure_lookup_table()
    delta = []
    for v in bm.verts:
        pm = male.find_nearest(mw @ v.co)[0]
        delta.append(female.find_nearest(pm)[0] - pm)
    if rigid:
        mean = sum(delta, Vector()) / max(1, len(delta))
        delta = [mean] * len(delta)
    else:
        for _ in range(smooth):
            delta = [(delta[v.index] + sum((delta[e.other_vert(v).index] for e in v.link_edges), Vector())) /
                     (1 + len(v.link_edges)) for v in bm.verts]
    inv = mw.inverted_safe().to_3x3()
    for v in bm.verts:
        v.co += inv @ delta[v.index]
    bm.to_mesh(me); bm.free()
    ob = replace(dst_name, me, src)
    same_groups(ob, src)
    copy_look(ob, src)
    return round(max(d.length for d in delta), 2)


def copy_look(ob, src):
    for mod in src.modifiers:
        if mod.type == 'SUBSURF':
            m = ob.modifiers.get(mod.name) or ob.modifiers.new(mod.name, 'SUBSURF'); m.levels = mod.levels
    ob.data.materials.clear()
    for m in src.data.materials:
        ob.data.materials.append(m)


def female():
    male, fem = tree(MALE), tree(FEMALE)
    return {d: carry(s, d, male, fem, rigid=s.startswith("TBone")) for s, d in pieces()}


def swap(name):
    """Left and right exchanged both ways ("NPC R Hand [RHnd]" <-> "NPC L Hand [LHnd]")."""
    name = re.sub(r"NPC ([LR]) ", lambda m: "NPC %s " % ("L" if m.group(1) == "R" else "R"), name)
    return re.sub(r"\[([LR])", lambda m: "[%s" % ("L" if m.group(1) == "R" else "R"), name)


def mirror_one(src_name, dst_name):
    src = bpy.data.objects[src_name]
    me = src.data.copy()
    bm = bmesh.new(); bm.from_mesh(me)
    for v in bm.verts:
        v.co.x = -v.co.x
    bmesh.ops.reverse_faces(bm, faces=bm.faces[:], flip_multires=False)
    bm.to_mesh(me); bm.free()
    ob = replace(dst_name, me, src)
    ob.matrix_world.translation.x = -ob.matrix_world.translation.x
    same_groups(ob, src, swap)
    copy_look(ob, src)
    return len(me.vertices)


def mirror():
    out = {}
    for m, f in pieces():
        out[m + "_L"] = mirror_one(m, m + "_L")
        out[f + "_L"] = mirror_one(f, f + "_L")
    return out


def bone_name(g):
    return g.split(".")[0] if g.startswith("NPC ") else None   # the hand's groups came in as "... .001"


def shape(ob):
    me = ob.data   # without modifiers
    me.calc_loop_triangles()
    uv = me.uv_layers.active.data
    names = [g.name for g in ob.vertex_groups]
    index, verts, normals, uvs, weights, tris = {}, [], [], [], [], []
    for t in me.loop_triangles:
        tri = []
        for li, vi in zip(t.loops, t.vertices):
            u, v = uv[li].uv
            n = me.corner_normals[li].vector
            key = (vi, round(u, 5), round(v, 5), round(n.x, 3), round(n.y, 3), round(n.z, 3))
            if key not in index:
                index[key] = len(verts)
                mv = me.vertices[vi]
                verts.append(list(ob.matrix_world @ mv.co)); normals.append(list((ob.matrix_world.to_3x3() @ n).normalized()))
                uvs.append([u, 1.0 - v])   # the game counts down from the top
                w = {}
                for g in mv.groups:
                    b = bone_name(names[g.group])
                    if b and g.weight > 0:
                        w[b] = w.get(b, 0) + g.weight
                top = sorted(w.items(), key=lambda x: -x[1])[:4]
                s = sum(x[1] for x in top) or 1
                weights.append({b: x / s for b, x in top})
            tri.append(index[key])
        tris.append(tri)
    assert all(weights), (ob.name, "a point with no weights")
    return dict(verts=verts, normals=normals, uvs=uvs, tris=tris, weights=weights)


def export():
    d = os.path.dirname(bpy.data.filepath)
    out = {}
    for side in ("R", "L"):
        for sex in ("M", "F"):
            data = {}
            fl = "TendrilFlesh" + ("F" if sex == "F" else "") + ("_L" if side == "L" else "")
            data["flesh"] = shape(bpy.data.objects[fl])
            for m in MODES:
                for part in ("TBone", "TSinew"):
                    ob = "%s_%s" % (part, m) + ("_F" if sex == "F" else "") + ("_L" if side == "L" else "")
                    data["%s_%s" % (part[1:].lower(), m)] = shape(bpy.data.objects[ob])
            path = os.path.join(d, "export_%s%s.json" % (side, sex))
            with open(path, "w") as f:
                json.dump(data, f)
            out[side + sex] = {k: len(v["verts"]) for k, v in data.items()}
    return out
