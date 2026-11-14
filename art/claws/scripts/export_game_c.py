
# (Hammerfist copy, 2026-11-05)

# (Shield copy, 2026-11-05) Writes the Shield's shapes for the game (tools/build_blade.py turns this into the nif): world positions (game units),
# normals, texture coordinates, triangles and bone weights (at most 4 a vertex). A vertex is split where the texture
# layout has a seam (one copy per side).
import bpy, json, os

OUT = os.path.join(os.path.dirname(bpy.data.filepath), "export.json")
OUT_F = os.path.join(os.path.dirname(bpy.data.filepath), "export_F.json")

def bone_name(g):
    return g.split(".")[0] if g.startswith("NPC ") else None   # the hand's groups came in as "... .001"

def shape(ob, weighted):
    me = ob.data   # without modifiers (the arm's Smooth): ~8,000 points rather than ~49,000
    me.calc_loop_triangles()
    uv = me.uv_layers.active.data
    names = [g.name for g in ob.vertex_groups]
    index, verts, normals, uvs, weights, tris = {}, [], [], [], [], []
    for t in me.loop_triangles:
        tri = []
        for li, vi in zip(t.loops, t.vertices):
            u, v = uv[li].uv
            n = me.corner_normals[li].vector   # the faceted plates keep their sharp edges (split normals)
            key = (vi, round(u, 5), round(v, 5), round(n.x, 3), round(n.y, 3), round(n.z, 3))
            if key not in index:
                index[key] = len(verts)
                mv = me.vertices[vi]
                verts.append(list(ob.matrix_world @ mv.co)); normals.append(list(n))
                uvs.append([u, 1.0 - v])   # the game counts down from the top
                if weighted:
                    w = {}
                    for g in mv.groups:
                        b = bone_name(names[g.group])
                        if b and g.weight > 0: w[b] = w.get(b, 0) + g.weight
                    top = sorted(w.items(), key=lambda x: -x[1])[:4]
                    s = sum(x[1] for x in top) or 1
                    weights.append({b: x / s for b, x in top})
            tri.append(index[key])
        tris.append(tri)
    d = dict(verts=verts, normals=normals, uvs=uvs, tris=tris)
    if weighted:
        d["weights"] = weights
    return d

def export(flesh, fist, out):
    data = dict(flesh=shape(bpy.data.objects[flesh], True), claw=shape(bpy.data.objects[fist], True))
    with open(out, "w") as f: json.dump(data, f)
    return {k: (len(v["verts"]), len(v["tris"])) for k, v in data.items()}


def export_all():
    """export_<side><sex>.json: R/L, M/F (the fists are the same for both sexes: they hang whole on the hand)."""
    d = os.path.dirname(bpy.data.filepath)
    jobs = {"RM": ("RCFlesh", "RCClaw"), "LM": ("RCFleshL", "RCClawL"), "RF": ("RCFleshF", "RCClawF"), "LF": ("RCFleshFL", "RCClawFL")}
    return {k: export(fl, fi, os.path.join(d, "export_%s.json" % k)) for k, (fl, fi) in jobs.items()}
