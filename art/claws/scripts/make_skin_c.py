# The Rending Claws' surface (2026-11-13): the sleeve painted as the Blade's arm (make_skin.py's flesh, as dark); the
# claws bone, grimy and blood-dark at the root, with dark chaurus chitin along each claw's back (as the Shield's and the
# Hammerfist's plates), fine grooves running down them; the knots, tendons and loose cords flesh. Then bake_skin_c.
import bpy, bmesh
exec(bpy.data.texts["make_skin.py"].as_string())   # node helpers, flesh_group, bone_group, surface, fresh, unwrap, S

CHITIN_DARK, CHITIN_MID, CHITIN_LIGHT = (0.005, 0.007, 0.007, 1), (0.016, 0.022, 0.020, 1), (0.040, 0.050, 0.040, 1)
K = dict(CHITIN=0.92, STRIA=5.0, STRIA_BUMP=0.25, GRAIN=2.2, GRAIN_BUMP=0.3, ROOT_DARK=0.85)


def make_skin_c(p=S, c=K):
    fg = flesh_group(p)
    arm = bpy.data.objects["RCFlesh"]
    m = fresh("RCArmSkin"); nt = m.node_tree
    at = nt.nodes.new("ShaderNodeAttribute"); at.attribute_name = "groove"; at.location = (-300, 0)
    gn = nt.nodes.new("ShaderNodeGroup"); gn.node_tree = fg; gn.location = (300, 0)
    nt.links.new(at.outputs["Fac"], gn.inputs["Groove"])
    hsv = nt.nodes.new("ShaderNodeHueSaturation"); hsv.location = (600, 0)
    hsv.inputs["Saturation"].default_value = p["ARM_SAT"]; hsv.inputs["Value"].default_value = p["ARM_VALUE"]
    nt.links.new(gn.outputs["Color"], hsv.inputs["Color"])
    bsdf = surface(m, hsv.outputs["Color"], None, gn.outputs["Height"]); bsdf.inputs["Roughness"].default_value = 0.3
    arm.data.materials.clear(); arm.data.materials.append(m)
    # the claws
    cl = bpy.data.objects["RCClaw"]
    m = fresh("RCClawSkin"); nt = m.node_tree
    tc = node(nt, "ShaderNodeTexCoord", -1600, 300); obj = tc.outputs["Object"]
    def attr(name, y):
        a = node(nt, "ShaderNodeAttribute", -1400, y); a.attribute_name = name
        return a.outputs["Fac"]
    a_flesh, a_root, a_spine = attr("flesh", -800), attr("root", -400), attr("spine", 0)
    bg = bone_group(p)
    bgn = node(nt, "ShaderNodeGroup", -900, -400); bgn.node_tree = bg
    nt.links.new(math(nt, -1100, -400, 'MULTIPLY_ADD', a_root, c["ROOT_DARK"], 0.08), bgn.inputs["Base"])
    # the chitin along the back: mottled greenish black
    mot = node(nt, "ShaderNodeTexNoise", -1100, 300); mot.inputs["Scale"].default_value = 0.6; mot.inputs["Detail"].default_value = 8
    nt.links.new(obj, mot.inputs["Vector"])
    chit = ramp(nt, -900, 300, [(0.35, CHITIN_DARK), (0.55, CHITIN_MID), (0.75, CHITIN_LIGHT)])
    nt.links.new(mot.outputs["Fac"], chit.inputs["Fac"])
    edge = node(nt, "ShaderNodeTexNoise", -1100, 100); edge.inputs["Scale"].default_value = 1.2
    nt.links.new(obj, edge.inputs["Vector"])
    # where the chitin ends is ragged: the back's share, pushed about by noise, cut sharp
    share = math(nt, -700, 100, 'MULTIPLY_ADD', edge.outputs["Fac"], 0.5, math(nt, -900, 100, 'SUBTRACT', a_spine, 0.25))
    cmask = node(nt, "ShaderNodeMapRange", -500, 100)
    cmask.inputs["From Min"].default_value = 0.42; cmask.inputs["From Max"].default_value = 0.55
    nt.links.new(share, cmask.inputs["Value"])
    cm = math(nt, -300, 100, 'MULTIPLY', cmask.outputs["Result"], c["CHITIN"])
    col = mix(nt, -100, 0, cm, bgn.outputs["Color"], chit.outputs["Color"])
    # the knots, tendons and cords: flesh
    fn = node(nt, "ShaderNodeTexNoise", -1100, -800); fn.inputs["Scale"].default_value = 0.5
    nt.links.new(obj, fn.inputs["Vector"])
    fgn = node(nt, "ShaderNodeGroup", -700, -800); fgn.node_tree = fg
    nt.links.new(fn.outputs["Fac"], fgn.inputs["Groove"])
    fl = node(nt, "ShaderNodeHueSaturation", -500, -800); fl.inputs["Value"].default_value = p["ARM_VALUE"]
    nt.links.new(fgn.outputs["Color"], fl.inputs["Color"])
    col = mix(nt, 100, 0, a_flesh, col, fl.outputs["Color"])
    # bumps: the bone's own, fine grain, and grooves down the claw
    grain = node(nt, "ShaderNodeTexNoise", -1100, 600); grain.inputs["Scale"].default_value = c["GRAIN"]; grain.inputs["Detail"].default_value = 10
    nt.links.new(obj, grain.inputs["Vector"])
    sm = node(nt, "ShaderNodeMapping", -1300, 850); sm.inputs["Scale"].default_value = (c["STRIA"], c["STRIA"], 0.12)
    nt.links.new(obj, sm.inputs["Vector"])
    stri = node(nt, "ShaderNodeTexNoise", -1100, 850); stri.inputs["Scale"].default_value = 1.0; stri.inputs["Detail"].default_value = 3
    nt.links.new(sm.outputs["Vector"], stri.inputs["Vector"])
    h = math(nt, 100, -400, 'MULTIPLY_ADD', grain.outputs["Fac"], c["GRAIN_BUMP"], bgn.outputs["Height"])
    h = math(nt, 250, -450, 'MULTIPLY_ADD', stri.outputs["Fac"], c["STRIA_BUMP"], h)
    notf = math(nt, 250, -700, 'SUBTRACT', 1.0, a_flesh)
    h = math(nt, 400, -500, 'MULTIPLY', h, notf)
    h = math(nt, 550, -550, 'MULTIPLY_ADD', fgn.outputs["Height"], a_flesh, h)
    bsdf = surface(m, col, None, h); bsdf.inputs["Roughness"].default_value = 0.3
    # shine: chitin glossy, bone half as much, flesh dull
    shine = math(nt, 300, -1100, 'MULTIPLY_ADD', cm, 0.5, 0.5)
    shine = math(nt, 450, -1100, 'MULTIPLY', shine, math(nt, 300, -1300, 'MULTIPLY_ADD', a_flesh, -0.7, 1.0))
    sm_ = node(nt, "ShaderNodeMath", 750, -1100, operation='MAXIMUM'); sm_.name = "SpecMask"; sm_.label = "SpecMask"
    nt.links.new(shine, sm_.inputs[0]); sm_.inputs[1].default_value = 0.0
    bump = [n for n in nt.nodes if n.type == 'BUMP'][0]
    bump.inputs["Strength"].default_value = 0.8; bump.inputs["Distance"].default_value = 0.15
    cl.data.materials.clear(); cl.data.materials.append(m)
    unwrap_sleeve(arm); unwrap(cl, margin=0.003)
    return "ok"


def copy_uv(src="RCClaw", dst="RCClawF"):
    """The female claws are the male's point for point: the same texture layout."""
    a, b = bpy.data.objects[src].data, bpy.data.objects[dst].data
    assert len(a.loops) == len(b.loops), (len(a.loops), len(b.loops))
    uv = [0.0] * (2 * len(a.loops)); a.uv_layers.active.data.foreach_get("uv", uv)
    lay = b.uv_layers.active or b.uv_layers.new(name=a.uv_layers.active.name)
    lay.data.foreach_set("uv", uv)
    return len(a.loops)
