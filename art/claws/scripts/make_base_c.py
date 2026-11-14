
# (Hammerfist copy, 2026-11-05: the right arm with the shoulder, the Blade's big sleeve)

# The base the arm's flesh is grown from: the game's right arm and hand, now with the shoulder (2026-11-04: the sleeve
# ended at the top of the arm and you could see under it; it should grow out of the shoulder).
import bpy, bmesh

B = dict(KEEP=0.2, REACH=26.0)  # a vertex is kept when this much of it follows the right arm, hand or collarbone,
# or (2026-11-04) when it is within REACH of the shoulder, on the right side: room for make_flesh.py to cut the ragged
# edge itself (EDGE)
from mathutils import Vector
SHOULDER = Vector((12.97, -5.23, 106.52))

ARM = ("NPC R Clavicle", "NPC R UpperArm", "NPC R Upperarm", "NPC R Forearm", "NPC R Hand", "NPC R Finger")

def make_base(p=B):
    w = bpy.context.window_manager.windows[0]
    vl = w.view_layer
    parts = []
    for n in ("MaleUnderwearBody:0", "HandMaleBig3rd"):
        src = bpy.data.objects[n]
        o = src.copy(); o.data = src.data.copy(); o.modifiers.clear(); o.parent = None
        o.matrix_world = src.matrix_world.copy()
        bpy.data.scenes["Claws"].collection.objects.link(o)
        o.data.transform(o.matrix_world); o.matrix_world.identity()
        parts.append(o)
    for o in vl.objects: o.select_set(False)
    for o in parts: o.hide_set(False); o.select_set(True)
    vl.objects.active = parts[0]
    with bpy.context.temp_override(window=w, active_object=parts[0], selected_objects=parts, selected_editable_objects=parts):
        bpy.ops.object.join()
    ob = parts[0]
    names = {g.index: g.name for g in ob.vertex_groups}
    me = ob.data
    bm = bmesh.new(); bm.from_mesh(me)
    dl = bm.verts.layers.deform.active
    drop = [v for v in bm.verts
            if v.co.x <= 0 or (sum(wt for gi, wt in v[dl].items() if names[gi].startswith(ARM)) < p["KEEP"]
                               and (v.co - SHOULDER).length > p["REACH"])]
    bmesh.ops.delete(bm, geom=drop, context='VERTS')
    bm.to_mesh(me); bm.free()
    old = bpy.data.objects.get("RCBase")
    if old:
        om = old.data; bpy.data.objects.remove(old); bpy.data.meshes.remove(om)
    ob.name = "RCBase"; me.name = "RCBase"
    ob.hide_set(True)
    return len(me.vertices)
