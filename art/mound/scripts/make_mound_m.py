# The Strange Mound (2026-11-14): a heap of burnt flesh in Helgen's ruins whose touch infects the player with Protean
# (user: "a mass of burnt flesh that, when activated, infects the player"). A lumpy collapsed mound of charred crust
# split over living flesh (make_textures_m.py), with a few tendrils curling off it (the Tendril's sinew texture).
#   blender -b --factory-startup --python art/mound/scripts/make_mound_m.py -- [render]
# -> art/mound/Mound.blend (objects Mound, MoundTendrils, ScaleFigure); with "render", previews in %TEMP%\protean_previews
import os
import sys
import math
import bpy
import bmesh
from mathutils import Vector, Matrix, noise

HERE = os.path.dirname(os.path.abspath(__file__))
ROOT = os.path.normpath(os.path.join(HERE, "..", "..", ".."))
ART = os.path.join(ROOT, "art", "mound")
TILE = 50.0                     # game units of mound surface per repeat of the texture
SIZE = (46.0, 38.0, 30.0)       # the mound's half-width, half-depth, height (a person is ~128 tall)
SINK = 5.0                      # how far it sits below the ground
LUMPS = 7
TENDRILS = 5
SINEW_U = (0.01, 0.23)          # darktentacle01's sinew strip, as the Tendril
SINEW_TILE = 25.0


def reset():
    bpy.ops.wm.read_factory_settings(use_empty=True)


def material(name, image_path, colour):
    m = bpy.data.materials.new(name)
    m.diffuse_color = colour
    m.use_nodes = True
    nt = m.node_tree
    img = nt.nodes.new("ShaderNodeTexImage")
    img.image = bpy.data.images.load(image_path, check_existing=True)
    bsdf = nt.nodes.get("Principled BSDF")
    nt.links.new(img.outputs["Color"], bsdf.inputs["Base Color"])
    nt.nodes.active = img
    return m


def cube_uv(bm, uv, scale):
    """Each face projected flat along its main axis: a tiling texture with no stretching on the sides."""
    for f in bm.faces:
        n = f.normal
        ax = max(range(3), key=lambda i: abs(n[i]))
        a, b = [(1, 2), (0, 2), (0, 1)][ax]
        for loop in f.loops:
            co = loop.vert.co
            loop[uv].uv = (co[a] / scale, co[b] / scale)


def blob(bm, centre, radii, seed, rough):
    """A displaced sphere added to bm."""
    geom = bmesh.ops.create_uvsphere(bm, u_segments=48, v_segments=24, radius=1.0)
    verts = geom["verts"]
    for v in verts:
        d = v.co.normalized()
        p = Vector((d.x * radii[0], d.y * radii[1], d.z * radii[2]))
        n = noise.fractal(p * 0.045 + Vector((seed, seed * 2, seed * 3)), 0.6, 2.0, 4)
        sag = 1.0 - 0.15 * max(0.0, d.z) ** 2       # slumped, not domed
        p = p * sag + d * (n * rough)
        v.co = p + centre
    return verts


def mound():
    bm = bmesh.new()
    blob(bm, Vector((0, 0, 0)), (SIZE[0], SIZE[1], SIZE[2] + SINK), 1.0, 7.0)
    # lumps heaped on it, half sunk: limbs and bodies fused in the fire
    for k in range(LUMPS):
        ang = k * 2.39996 + 0.4
        r = 0.55 + 0.35 * ((k * 7) % 5) / 4
        c = Vector((math.cos(ang) * SIZE[0] * r, math.sin(ang) * SIZE[1] * r, SIZE[2] * (0.75 - 0.5 * r)))
        s = 10.0 + 8.0 * ((k * 3) % 4) / 3
        blob(bm, c, (s * 1.4, s, s * 0.8), 10.0 + k, 3.0)
    for v in bm.verts:   # into the ground
        v.co.z = max(v.co.z, -SINK) if v.co.z > -SINK - 0.01 else -SINK
    bm.normal_update()
    uv = bm.loops.layers.uv.new("UVMap")
    cube_uv(bm, uv, TILE)
    me = bpy.data.meshes.new("Mound")
    bm.to_mesh(me); bm.free()
    for p in me.polygons:
        p.use_smooth = True
    ob = bpy.data.objects.new("Mound", me)
    bpy.context.scene.collection.objects.link(ob)
    ob.data.materials.append(material("MoundFlesh", os.path.join(ART, "textures", "Mound_d.png"), (0.2, 0.05, 0.04, 1)))
    return ob


def surface_point(ob, direction):
    """Where a ray from above the mound's middle, leaning along direction, meets its surface."""
    o = Vector((0, 0, SIZE[2] * 3))
    hit, loc, nrm, _ = ob.ray_cast(o, (direction - o).normalized())
    return (loc, nrm) if hit else (direction, Vector((0, 0, 1)))


def tube(bm, uv, pts, radii, sides=8, u=SINEW_U, tile=SINEW_TILE, vband=None):
    rings, vs, run = [], [], 0.0
    x = prev = None
    for i, p in enumerate(pts):
        t = (pts[min(i + 1, len(pts) - 1)] - pts[max(i - 1, 0)]).normalized()
        if x is None:
            ref = Vector((0, 0, 1)) if abs(t.z) < 0.9 else Vector((1, 0, 0))
            x = t.cross(ref).normalized()
        else:
            # each ring turned from the last by the bend between them: a fixed "up" flipped where a tendril curled past
            # it and twisted the tube into a pinched joint there (user, 2026-11-14: "a segmented look")
            x = prev.rotation_difference(t) @ x
            x = (x - t * x.dot(t)).normalized()
        prev = t
        y = t.cross(x)
        rings.append([bm.verts.new(p + (x * math.cos(2 * math.pi * k / sides) + y * math.sin(2 * math.pi * k / sides)) * radii[i])
                      for k in range(sides)])
        if i:
            run += (p - pts[i - 1]).length
        vs.append(run / tile)
    if vband:   # stretched once along a band of the texture, not tiled
        vs = [vband[0] + (vband[1] - vband[0]) * x / max(vs[-1], 1e-6) for x in vs]
    uu = lambda k: u[0] + (u[1] - u[0]) * k / sides
    for i in range(len(rings) - 1):
        for k in range(sides):
            k1 = (k + 1) % sides
            f = bm.faces.new((rings[i][k], rings[i][k1], rings[i + 1][k1], rings[i + 1][k]))
            for loop, c in zip(f.loops, ((uu(k), vs[i]), (uu(k + 1), vs[i]), (uu(k + 1), vs[i + 1]), (uu(k), vs[i + 1]))):
                loop[uv].uv = (c[0], 1.0 - c[1])
    tip = bm.verts.new(pts[-1] + (pts[-1] - pts[-2]).normalized() * radii[-1])
    for k in range(sides):
        f = bm.faces.new((rings[-1][k], rings[-1][(k + 1) % sides], tip))
        for loop, c in zip(f.loops, ((uu(k), vs[-1]), (uu(k + 1), vs[-1]), ((uu(k) + uu(k + 1)) / 2, vs[-1] + 0.05))):
            loop[uv].uv = (c[0], 1.0 - c[1])


def tendrils(base):
    bm = bmesh.new()
    uv = bm.loops.layers.uv.new("UVMap")
    for k in range(TENDRILS):
        ang = k * 2.0 * math.pi / TENDRILS + 0.7
        out = Vector((math.cos(ang) * SIZE[0] * 0.55, math.sin(ang) * SIZE[1] * 0.55, 0))
        start, nrm = surface_point(base, out)
        start = start - nrm * 1.5                     # rooted in the flesh
        length = 30.0 + 18.0 * ((k * 3) % 4) / 3
        d = (nrm * 0.8 + out.normalized() * 0.6).normalized()
        axis = d.cross(Vector((0, 0, 1))).normalized()
        p, pts, rad, steps = start.copy(), [], [], 28
        r0 = 2.6 - 0.3 * (k % 3)
        for i in range(steps + 1):
            t = i / steps
            pts.append(p.copy()); rad.append(r0 * (1 - t) ** 1.1 + 0.15)
            d = Matrix.Rotation(0.03 + 0.55 * t ** 3, 3, axis) @ d       # rising, then curling over at the tip
            d = (Matrix.Rotation(((k % 2) * 2 - 1) * 0.05, 3, Vector((0, 0, 1))) @ d).normalized()
            p = p + d * (length / steps)
        tube(bm, uv, pts, rad, vband=(0.02, 0.98))   # once along it: the strip does not tile, every repeat left a seam ring (user: "segmented")
    me = bpy.data.meshes.new("MoundTendrils")
    bm.to_mesh(me); bm.free()
    for p in me.polygons:
        p.use_smooth = True
    ob = bpy.data.objects.new("MoundTendrils", me)
    bpy.context.scene.collection.objects.link(ob)
    ob.data.materials.append(material("MoundSinew", os.path.join(ROOT, "art", "tendril", "ref", "darktentacle01.dds"),
                                      (0.3, 0.1, 0.1, 1)))
    return ob


RIBS = 4
BONE_UV = (70 / 256, 340 / 512, 170 / 256, 460 / 512)   # horn01's pale lower part: scorched bone (u left, v, u right, v)


def ribs(base):
    """Charred ribs breaking out of one flank: a body under the burnt flesh."""
    bm = bmesh.new()
    uv = bm.loops.layers.uv.new("UVMap")
    for k in range(RIBS):
        y = -10.0 + 7.0 * k
        start, nrm = surface_point(base, Vector((-SIZE[0] * 0.75, y, 0)))
        start = start - nrm * 2.0
        d = (nrm + Vector((0, 0, 0.9))).normalized()
        axis = d.cross(Vector((0, 1, 0))).normalized() * -1
        p, pts, rad, steps = start.copy(), [], [], 20
        length = 26.0 - 3.0 * abs(k - 1.5)
        for i in range(steps + 1):
            t = i / steps
            pts.append(p.copy()); rad.append(1.5 * (1 - 0.6 * t) + 0.2)
            d = (Matrix.Rotation(-0.11, 3, axis) @ d).normalized()       # arching back over the mound
            p = p + d * (length / steps)
        tube(bm, uv, pts, rad, u=(BONE_UV[0], BONE_UV[2]), vband=(BONE_UV[1], BONE_UV[3]))
    me = bpy.data.meshes.new("MoundRibs")
    bm.to_mesh(me); bm.free()
    for p in me.polygons:
        p.use_smooth = True
    ob = bpy.data.objects.new("MoundRibs", me)
    bpy.context.scene.collection.objects.link(ob)
    ob.data.materials.append(material("MoundBone", os.path.join(ROOT, "art", "tendril", "ref", "horn01.dds"), (0.2, 0.18, 0.15, 1)))
    return ob


def scale_figure():
    """A plain 128-unit figure beside it (not exported), and the ground."""
    bpy.ops.mesh.primitive_cylinder_add(radius=12, depth=128, location=(SIZE[0] + 40, 0, 64))
    fig = bpy.context.object; fig.name = "ScaleFigure"
    bpy.ops.mesh.primitive_plane_add(size=600, location=(0, 0, 0))
    bpy.context.object.name = "Ground"


def render(name, target, rot_deg, dist, lens=40):
    sc = bpy.context.scene
    cam = bpy.data.objects.get("PreviewCam") or bpy.data.objects.new("PreviewCam", bpy.data.cameras.new("PreviewCam"))
    if cam.name not in sc.collection.objects:
        sc.collection.objects.link(cam)
    cam.data.lens = lens; cam.data.clip_end = 5000
    q = __import__("mathutils").Euler([math.radians(r) for r in rot_deg]).to_quaternion()
    cam.matrix_world = Matrix.Translation(Vector(target)) @ q.to_matrix().to_4x4() @ Matrix.Translation((0, 0, dist))
    sc.camera = cam
    sc.render.engine = 'BLENDER_WORKBENCH'
    sc.display.shading.light = 'STUDIO'
    sc.display.shading.color_type = 'TEXTURE'
    sc.display.shading.show_cavity = True
    sc.display.shading.show_specular_highlight = False
    sc.render.resolution_x = sc.render.resolution_y = 900
    out = os.path.join(os.environ.get("TEMP", "."), "protean_previews", name + ".png")
    os.makedirs(os.path.dirname(out), exist_ok=True)
    sc.render.filepath = out
    bpy.ops.render.render(write_still=True)
    sc.render.filepath = "_" * 1023   # the whole fixed-size field overwritten first: a shorter value
    sc.render.filepath = "//"         # alone leaves the temp path (with the Windows user name) after it
    return out


def main():
    reset()
    base = mound()
    tendrils(base)
    ribs(base)
    scale_figure()
    os.makedirs(ART, exist_ok=True)
    bpy.ops.wm.save_as_mainfile(filepath=os.path.join(ART, "Mound.blend"))
    if "render" in sys.argv[sys.argv.index("--") + 1:] if "--" in sys.argv else []:
        print("RENDER", render("mound_front", (0, 0, 15), (68, 0, 20), 230))
        print("RENDER", render("mound_side", (0, 0, 15), (75, 0, 110), 230))
        print("RENDER", render("mound_close", (0, 0, 12), (55, 0, 200), 120))
        bpy.ops.wm.save_mainfile()


main()
