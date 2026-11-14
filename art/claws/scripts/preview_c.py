# Preview renders to files (no window needed): preview(name, show=[objects], target, rot_deg, dist)
import bpy, mathutils, os
OUT = r"C:\Users\user\AppData\Local\Temp\claude\8ebe18ea4c2d0b20\90dd2234-655e-4455-a606-f11d4cc1e8a7\scratchpad"

def preview(name, show, target, rot_deg, dist, lens=50, size=(1000, 1000), color='MATERIAL'):
    sc = bpy.data.scenes["Claws"]
    for o in sc.objects:
        if o.type == 'MESH':
            o.hide_render = o.name not in show
            o.hide_viewport = False
            try: o.hide_set(o.name not in show)
            except RuntimeError: pass
    cam = bpy.data.objects.get("PreviewCam")
    cam.data.lens = lens; cam.data.clip_end = 5000
    q = mathutils.Euler([r * 3.14159265 / 180 for r in rot_deg]).to_quaternion()
    cam.matrix_world = (mathutils.Matrix.Translation(mathutils.Vector(target)) @ q.to_matrix().to_4x4()
                        @ mathutils.Matrix.Translation((0, 0, dist)))
    sc.camera = cam
    sc.render.engine = 'BLENDER_WORKBENCH'
    sc.display.shading.light = 'STUDIO'
    sc.display.shading.color_type = color
    sc.display.shading.show_cavity = True
    sc.render.resolution_x, sc.render.resolution_y = size; sc.render.resolution_percentage = 100
    sc.render.film_transparent = False
    os.makedirs(OUT, exist_ok=True)
    sc.render.filepath = os.path.join(OUT, name + ".png")
    bpy.ops.render.render(write_still=True, scene=sc.name)
    return sc.render.filepath

MALE = ["MaleUnderwearBody:0", "HandMaleBig3rd"]
def views(tag, show):
    out = []
    out.append(preview(tag + "_front", show, (24, 0, 86), (90, 0, 180), 95))
    out.append(preview(tag + "_side", show, (24, 0, 86), (90, 0, 90), 95))
    out.append(preview(tag + "_back", show, (24, 0, 86), (90, 0, 0), 95))
    out.append(preview(tag + "_hand", show, (30, 5, 62), (80, 0, 125), 42))
    return out
