# Preview with the baked images (as the game would show them): flat colour from the baked textures. Needs preview_c
# loaded first (run.py keeps each file's names for the later steps).
import bpy, os
TEX = os.path.join(os.path.dirname(bpy.data.filepath), "textures")


def textured(tag, pairs, extra, shots):
    keep = {}
    for obn, part in pairs.items():
        ob = bpy.data.objects[obn]; keep[obn] = ob.data.materials[0] if ob.data.materials else None
        m = bpy.data.materials.get("Preview_" + part) or bpy.data.materials.new("Preview_" + part)
        m.use_nodes = True; nt = m.node_tree; nt.nodes.clear()
        out = nt.nodes.new("ShaderNodeOutputMaterial"); bs = nt.nodes.new("ShaderNodeBsdfPrincipled")
        nt.links.new(bs.outputs[0], out.inputs[0])
        d = nt.nodes.new("ShaderNodeTexImage")
        d.image = bpy.data.images.load(os.path.join(TEX, part + "_d.png"), check_existing=False)
        nt.links.new(d.outputs["Color"], bs.inputs["Base Color"])
        nt.nodes.active = d
        if ob.data.materials: ob.data.materials[0] = m
        else: ob.data.materials.append(m)
    try:
        return [preview(tag + "_" + n, list(pairs) + extra, t, r, dist, color='TEXTURE') for n, t, r, dist in shots]
    finally:
        for obn, mat in keep.items():
            if mat: bpy.data.objects[obn].data.materials[0] = mat


SHOTS = [("front", (24, 0, 86), (90, 0, 180), 95), ("side", (24, 0, 86), (90, 0, 90), 95),
         ("back", (24, 0, 86), (90, 0, 0), 95), ("hand", (30, 5, 62), (80, 0, 125), 42),
         ("claws", (29, 8, 58), (95, 0, 60), 34)]
