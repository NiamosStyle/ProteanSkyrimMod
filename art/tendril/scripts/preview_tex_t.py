# Textured preview renders of the Tendril sleeve (needs preview_t and make_parts_t run first): the Blade's baked flesh,
# the whip's sinew and the claws' horn, as the game will draw them (art/tendril/ref holds copies of the game's two
# textures, not in git).
import bpy, os
from mathutils import Vector

HERE = os.path.dirname(bpy.data.filepath)
IMAGES = {"PrevFlesh": os.path.join(HERE, "..", "blade", "textures", "Arm_d.png"),
          "PrevSinew": os.path.join(HERE, "ref", "darktentacle01.dds"),
          "PrevBone": os.path.join(HERE, "ref", "horn01.dds")}


def tex_material(name):
    m = bpy.data.materials.get(name) or bpy.data.materials.new(name)
    m.use_nodes = True
    nt = m.node_tree
    img = nt.nodes.get("Img") or nt.nodes.new("ShaderNodeTexImage")
    img.name = "Img"
    img.image = bpy.data.images.load(os.path.abspath(IMAGES[name]), check_existing=True)
    bsdf = nt.nodes.get("Principled BSDF")
    if bsdf:
        nt.links.new(img.outputs["Color"], bsdf.inputs["Base Color"])
    nt.nodes.active = img
    return m


def dress(ob_name, mat):
    ob = bpy.data.objects[ob_name]
    ob.data.materials.clear(); ob.data.materials.append(mat)


def shots(sex=""):
    shoulder, elbow, wrist, a, up, side = arm_frame()
    centre, n, along = palm_frame()
    fl, si, bo = tex_material("PrevFlesh"), tex_material("PrevSinew"), tex_material("PrevBone")
    body = ["MaleUnderwearBody:0", "HandMaleBig3rd"] if not sex else ["FemaleUnderwearBody:0", "HandFemale3rd"]
    flesh = "TendrilFlesh" + ("F" if sex else "")
    dress(flesh, fl)
    out = []
    mid = (shoulder + wrist + a * 25) / 2 + Vector((0, 0, 8))
    for mode in ("rest", "drawn"):
        sfx = "_F" if sex else ""
        dress("TSinew_%s%s" % (mode, sfx), si); dress("TBone_%s%s" % (mode, sfx), bo)
        show = body + [flesh, "TSinew_%s%s" % (mode, sfx), "TBone_%s%s" % (mode, sfx)]
        tag = "tendril_tex_%s%s" % (mode, sfx)
        out.append(preview(tag + "_front", show, mid, (90, 0, 165), 115, color='TEXTURE'))
        out.append(preview(tag + "_back", show, mid, (80, 0, 30), 115, color='TEXTURE'))
        out.append(look(tag + "_hand", show, centre + along * 8, centre + along * 8 + Vector((0.35, 1.0, 0.1)).normalized() * 55))
    return out


def look(name, show, target, eye):
    import math
    q = (target - eye).to_track_quat('-Z', 'Y')
    return preview(name, show, target, [math.degrees(x) for x in q.to_euler()], (eye - target).length, color='TEXTURE')
