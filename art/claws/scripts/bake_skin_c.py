
# (Shield copy, 2026-11-05) Bakes the Blade's painted surface (make_skin.py) into images for the game: colour, normal (surface detail) and
# glow, one set for the arm (BladeFlesh) and one for the blade (BladeDraft). PNGs land in art/blade/textures.
import bpy, os

OUT = os.path.join(os.path.dirname(bpy.data.filepath), "textures")
SIZES = {"RCFlesh": 2048, "RCClaw": 2048}   # the shell sharper up close (2026-11-05)
NAMES = {"RCFlesh": "Arm", "RCClaw": "Claw"}


def _bake(ob, kind, size):
    mat = ob.data.materials[0]
    nt = mat.node_tree
    name = "%s_%s" % (NAMES[ob.name], kind)
    img = bpy.data.images.get(name)
    if img:
        bpy.data.images.remove(img)
    img = bpy.data.images.new(name, size, size, alpha=False, float_buffer=False)
    if kind == "n":
        img.colorspace_settings.name = "Non-Color"
    tn = nt.nodes.new("ShaderNodeTexImage"); tn.image = img; tn.location = (1200, 400)
    for n in nt.nodes:
        n.select = False
    tn.select = True; nt.nodes.active = tn
    w = bpy.context.window_manager.windows[0]
    vl = w.view_layer
    for o in vl.objects:
        o.select_set(False)
    hidden = ob.hide_get(); ob.hide_set(False); ob.hide_render = False
    ob.select_set(True); vl.objects.active = ob
    bsdf = [n for n in nt.nodes if n.type == 'BSDF_PRINCIPLED'][0]
    old_em = tuple(bsdf.inputs["Emission Color"].default_value)
    try:
        with bpy.context.temp_override(window=w, active_object=ob, object=ob, selected_objects=[ob],
                                       selected_editable_objects=[ob]):
            if kind == "d":
                bpy.ops.object.bake(type='DIFFUSE', pass_filter={'COLOR'}, margin=8, use_clear=True)
            elif kind == "n":
                bpy.ops.object.bake(type='NORMAL', normal_space='TANGENT', margin=8, use_clear=True)
            elif kind == "s":  # the shine mask (2026-11-05): the SpecMask node as emission
                sm = nt.nodes.get("SpecMask")
                bsdf.inputs["Emission Color"].default_value = (1, 1, 1, 1)
                link = nt.links.new(sm.outputs[0], bsdf.inputs["Emission Strength"])
                try:
                    bpy.ops.object.bake(type='EMIT', margin=8, use_clear=True)
                finally:
                    nt.links.remove(link)
            else:  # glow: the vein mask in white, coloured later
                bsdf.inputs["Emission Color"].default_value = (1, 1, 1, 1)
                bpy.ops.object.bake(type='EMIT', margin=8, use_clear=True)
    finally:
        bsdf.inputs["Emission Color"].default_value = old_em
        nt.nodes.remove(tn)
        ob.select_set(False); ob.hide_set(hidden)
    os.makedirs(OUT, exist_ok=True)
    path = os.path.join(OUT, name + ".png")
    img.filepath_raw = path; img.file_format = 'PNG'; img.save()
    return path


def bake():
    sc = bpy.context.scene
    engine = sc.render.engine
    sc.render.engine = 'CYCLES'
    sc.cycles.samples = 1
    sc.cycles.device = 'CPU'
    arm = bpy.data.objects["RCFlesh"]
    sub = arm.modifiers.get("Smooth")
    if sub:
        sub.show_render = False   # bake the shape the game gets (no extra smoothing)
    out = []
    try:
        for ob in (arm, bpy.data.objects["RCClaw"]):
            for kind in ("d", "n") + (("s",) if ob.name == "RCClaw" else ()):   # no glow (user: less glowy)
                out.append(_bake(ob, kind, SIZES[ob.name]))
    finally:
        sc.render.engine = engine
    return out
