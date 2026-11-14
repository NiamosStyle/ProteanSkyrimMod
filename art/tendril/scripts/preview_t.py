# Preview renders to files (no window needed), as the claws' preview_c.py.
import bpy, mathutils, os
OUT = os.path.join(os.environ.get("TEMP", "."), "protean_previews")
SCENE = "Blade"


def preview(name, show, target, rot_deg, dist, lens=50, size=(900, 900), color='MATERIAL'):
    sc = bpy.data.scenes[SCENE]
    for o in sc.objects:
        if o.type == 'MESH':
            o.hide_render = o.name not in show
    cam = bpy.data.objects.get("PreviewCam")
    if not cam:
        cam = bpy.data.objects.new("PreviewCam", bpy.data.cameras.new("PreviewCam"))
        sc.collection.objects.link(cam)
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
