# One-off: Claws.blend from Hammerfist.blend (the bodies, hands, skeletons and the shared paint scripts), without the
# Hammerfist's own shapes.
import bpy
for o in list(bpy.data.objects):
    if o.name.startswith("HF"):
        me = o.data; bpy.data.objects.remove(o)
        if me.users == 0: bpy.data.meshes.remove(me)
sc = bpy.data.scenes["Hammerfist"]; sc.name = "Claws"
wm = bpy.context.window_manager
print("PROBE windows", len(wm.windows), "ctx.window", bpy.context.window, "ctx.scene", bpy.context.scene.name,
      "areas", [a.type for w in wm.windows for a in w.screen.areas])
for im in list(bpy.data.images):
    if im.users == 0: bpy.data.images.remove(im)
bpy.ops.wm.save_as_mainfile(filepath=r"C:\Modding\Protean\art\claws\Claws.blend")
print("PROBE saved", [o.name for o in bpy.data.objects if o.type == 'MESH'])
