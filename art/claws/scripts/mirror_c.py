
# The left arm and fist (2026-11-05): the right ones mirrored across the body (the skeleton's arms are mirror images in
# the bind pose), faces turned the right way out, bone groups renamed R -> L. Same texture layout, so the same textures.
import bpy, bmesh, re

def swap(name):
    """Left and right exchanged both ways ("NPC R Hand [RHnd]" <-> "NPC L Hand [LHnd]", "[Rft ]" <-> "[Lft ]")."""
    name = re.sub(r"NPC ([LR]) ", lambda m: "NPC %s " % ("L" if m.group(1) == "R" else "R"), name)
    return re.sub(r"\[([LR])", lambda m: "[%s" % ("L" if m.group(1) == "R" else "R"), name)

def mirror(src_name, dst_name):
    src = bpy.data.objects[src_name]
    me = src.data.copy()
    bm = bmesh.new(); bm.from_mesh(me)
    for v in bm.verts:
        v.co.x = -v.co.x
    bmesh.ops.reverse_faces(bm, faces=bm.faces[:], flip_multires=False)
    bm.to_mesh(me); bm.free()
    ob = bpy.data.objects.get(dst_name)
    if ob:
        # the groups cleared BEFORE the new points come in: clearing them after wipes the copied weights (the Blade's
        # female arm, 2026-11-04; the left Hammerfist arm, 2026-11-05)
        ob.vertex_groups.clear()
        old = ob.data; ob.data = me; bpy.data.meshes.remove(old)
    else:
        ob = bpy.data.objects.new(dst_name, me); bpy.data.scenes["Claws"].collection.objects.link(ob)
    ob.matrix_world = src.matrix_world.copy()
    ob.matrix_world.translation.x = -ob.matrix_world.translation.x
    # the same groups in the same order (the weights came with the copied points), named for the left
    for g in src.vertex_groups:
        ob.vertex_groups.new(name=swap(g.name))
    for mod in src.modifiers:
        if mod.type == 'SUBSURF':
            m = ob.modifiers.get(mod.name) or ob.modifiers.new(mod.name, 'SUBSURF'); m.levels = mod.levels
    ob.data.materials.clear()
    for m in src.data.materials: ob.data.materials.append(m)
    return len(me.vertices)

def mirror_all():
    return {d: mirror(s, d) for s, d in (("RCFlesh", "RCFleshL"), ("RCClaw", "RCClawL"), ("RCFleshF", "RCFleshFL"),
                                         ("RCClawF", "RCClawFL"))}
