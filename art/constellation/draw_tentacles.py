"""Blender (headless) drawing of the Protean constellation: a writhing mass of tentacles.
Run:  blender --background --factory-startup --python draw_tentacles.py -- <out.png>
Shapes are listed back to front; each has an outline polygon, and lines of earlier shapes are hidden where a later
shape covers them. Coordinates: x right, y up, the picture fills -1..1."""
import bpy, sys, math

OUT = sys.argv[sys.argv.index("--") + 1]
RES = 2048
BASE_R = 0.0050


# ---- curve helpers ----------------------------------------------------------------------------------------------
def catmull(ctrl, n=40):
    """smooth curve through the control points (ends repeated)"""
    p = [ctrl[0]] + list(ctrl) + [ctrl[-1]]
    out = []
    for i in range(1, len(p) - 2):
        p0, p1, p2, p3 = p[i - 1], p[i], p[i + 1], p[i + 2]
        for k in range(n):
            t = k / n
            t2, t3 = t * t, t * t * t
            out.append(tuple(0.5 * (2 * b + (-a + c) * t + (2 * a - 5 * b + 4 * c - d) * t2 + (-a + 3 * b - 3 * c + d) * t3)
                             for a, b, c, d in zip(p0, p1, p2, p3)))
    out.append(ctrl[-1])
    return out


def closed_catmull(ctrl, n=40):
    p = list(ctrl) + list(ctrl[:3])
    return catmull(p, n)[n:-(n + 1)] if False else _closed(ctrl, n)


def _closed(ctrl, n):
    m = len(ctrl)
    out = []
    for i in range(m):
        p0, p1, p2, p3 = ctrl[(i - 1) % m], ctrl[i], ctrl[(i + 1) % m], ctrl[(i + 2) % m]
        for k in range(n):
            t = k / n
            t2, t3 = t * t, t * t * t
            out.append(tuple(0.5 * (2 * b + (-a + c) * t + (2 * a - 5 * b + 4 * c - d) * t2 + (-a + 3 * b - 3 * c + d) * t3)
                             for a, b, c, d in zip(p0, p1, p2, p3)))
    out.append(out[0])
    return out


def resample(pts, step=0.004):
    out = [pts[0]]
    carry = 0.0
    for a, b in zip(pts, pts[1:]):
        d = math.dist(a, b)
        if d == 0:
            continue
        t = step - carry
        while t <= d:
            out.append((a[0] + (b[0] - a[0]) * t / d, a[1] + (b[1] - a[1]) * t / d))
            t += step
        carry = d - (t - step)
    out.append(pts[-1])
    return out


def wobble(pts, amp, seed):
    out = []
    n = len(pts)
    for i, (x, y) in enumerate(pts):
        s = i / max(1, n - 1)
        w = amp * (math.sin(7.3 * s + seed) + 0.5 * math.sin(17.1 * s + 2.3 * seed))
        out.append((x + w, y + w * 0.7))
    return out


def inside(pt, poly):
    x, y = pt
    c = False
    j = len(poly) - 1
    for i in range(len(poly)):
        xi, yi = poly[i]
        xj, yj = poly[j]
        if (yi > y) != (yj > y) and x < (xj - xi) * (y - yi) / (yj - yi + 1e-12) + xi:
            c = not c
        j = i
    return c


# profiles: radius multiplier along a stroke, s in 0..1
def even(s):
    return 0.85 + 0.25 * math.sin(math.pi * s)


def swell(s):
    return 0.3 + 0.9 * math.sin(math.pi * s) ** 0.7


SHAPES = []   # back to front: {"strokes": [(pts, prof)], "poly": [...] or None}


def shape(strokes, poly):
    SHAPES.append({"strokes": strokes, "poly": poly})


# ---- a tentacle: a thick root, tapering, ending in a wide open curl; dash suckers along the inside of the curl ----
def tentacle(ctrl, w0, curl=1, turns=0.9, r0=0.12, suckers=0, scallop=0, seed=0.0, wob=0.005, taper=0.6):
    spine = resample(wobble(catmull(ctrl, 60), wob, seed))
    (x1, y1), (x2, y2) = spine[-2], spine[-1]
    ang = math.atan2(y2 - y1, x2 - x1)
    x, y = x2, y2
    total = turns * 2 * math.pi
    a = 0.0
    step = 0.004
    while a < total:
        rr = r0 * (1 - 0.45 * a / total)
        da = step / max(rr, 1e-4)
        a += da
        ang += curl * da
        x += math.cos(ang) * step
        y += math.sin(ang) * step
        spine.append((x, y))
    n = len(spine)
    left, right, widths, normals = [], [], [], []
    for i in range(n):
        a_ = spine[max(0, i - 1)]
        b_ = spine[min(n - 1, i + 1)]
        dx, dy = b_[0] - a_[0], b_[1] - a_[1]
        L = math.hypot(dx, dy) or 1
        nx, ny = -dy / L, dx / L
        s = i / (n - 1)
        w = w0 * (1 - s) ** taper + 0.002
        widths.append(w)
        normals.append((nx, ny))
        # the curl side's edge can carry the suckers seen side-on, as a row of bumps
        bl = br = 0.0
        if scallop and 0.08 < s < 0.62:
            bump = w * 0.26 * max(0.0, math.sin(s * scallop * math.pi * 2)) ** 0.6
            if curl > 0:
                bl = bump
            else:
                br = bump
        left.append((spine[i][0] + nx * (w / 2 + bl), spine[i][1] + ny * (w / 2 + bl)))
        right.append((spine[i][0] - nx * (w / 2 + br), spine[i][1] - ny * (w / 2 + br)))
    outline = left + right[::-1]
    strokes = [(outline, lambda s: 0.45 + 0.75 * (1 - abs(2 * s - 1)) ** 0.2)]
    if suckers:
        side = 1 if curl > 0 else -1       # the inside of the curl
        for k in range(suckers):
            s = 0.38 + 0.52 * k / max(1, suckers - 1)
            i = int(s * (n - 1))
            cx, cy = spine[i]
            nx, ny = normals[i]
            w = widths[i]
            px, py = cx + side * nx * w * 0.24, cy + side * ny * w * 0.24
            tx, ty = ny, -nx                 # along the spine
            la, lb = max(0.012, w * 0.20), max(0.005, w * 0.075)
            ring = []
            for t in range(29):
                th = t * 2 * math.pi / 28
                u, v = la * math.cos(th), lb * math.sin(th)
                ring.append((px + tx * u + nx * v, py + ty * u + ny * v))
            strokes.append((ring, lambda s: 0.6))
    shape(strokes, outline)


# ---- the mass: seven tentacles rising from below, back to front ---------------------------------------------------
# back: three lesser ones
tentacle([(-0.74, -1.15), (-0.80, -0.74), (-0.92, -0.46), (-0.86, -0.26)], 0.19, curl=-1, turns=0.72, r0=0.13,
         suckers=0, seed=3.1)
tentacle([(0.76, -1.15), (0.86, -0.78), (0.95, -0.52), (0.88, -0.34)], 0.18, curl=1, turns=0.75, r0=0.12,
         suckers=0, seed=3.7)
tentacle([(-0.22, -1.15), (-0.28, -0.50), (-0.36, 0.10), (-0.40, 0.52), (-0.32, 0.76)], 0.16, curl=-1, turns=0.75, r0=0.10,
         suckers=0, seed=4.4)
# the three great ones -- the tree's branches
tentacle([(-0.42, -1.15), (-0.48, -0.66), (-0.64, -0.26), (-0.70, 0.06), (-0.62, 0.26)], 0.32, curl=-1,
         turns=0.78, r0=0.21, suckers=0, seed=1.1)
tentacle([(0.44, -1.15), (0.52, -0.66), (0.70, -0.32), (0.72, -0.04), (0.66, 0.12)], 0.29, curl=1, turns=0.8,
         r0=0.19, suckers=0, seed=2.2)
tentacle([(-0.04, -1.15), (0.04, -0.62), (-0.14, -0.14), (-0.12, 0.26), (-0.04, 0.46)], 0.32, curl=-1,
         turns=0.75, r0=0.22, suckers=0, seed=0.3)
# front: one crossing the others, its suckers seen side-on as bumps
tentacle([(-0.32, -1.15), (-0.22, -0.84), (0.06, -0.60), (0.24, -0.46), (0.30, -0.26)], 0.22, curl=1,
         turns=0.7, r0=0.13, scallop=0, seed=6.1)

# ---- hide lines covered by shapes in front ----------------------------------------------------------------------
FINAL = []
for i, sh in enumerate(SHAPES):
    covers = [s["poly"] for s in SHAPES[i + 1:] if s["poly"]]
    for pts, prof in sh["strokes"]:
        run = []
        for p in pts:
            if any(inside(p, c) for c in covers):
                if len(run) > 2:
                    FINAL.append((run, prof))
                run = []
            else:
                run.append(p)
        if len(run) > 2:
            FINAL.append((run, prof))

# ---- build and render -------------------------------------------------------------------------------------------
bpy.ops.wm.read_factory_settings(use_empty=True)
scene = bpy.context.scene
for i, (pts, prof) in enumerate(FINAL):
    cu = bpy.data.curves.new("s%d" % i, "CURVE")
    cu.dimensions = "3D"
    cu.bevel_depth = BASE_R
    cu.bevel_resolution = 3
    cu.use_fill_caps = True
    sp = cu.splines.new("POLY")
    sp.points.add(len(pts) - 1)
    for k, (x, y) in enumerate(pts):
        sp.points[k].co = (x, y, 0.0, 1.0)
        sp.points[k].radius = prof(k / (len(pts) - 1))
    ob = bpy.data.objects.new("s%d" % i, cu)
    scene.collection.objects.link(ob)

cam_data = bpy.data.cameras.new("cam")
cam_data.type = "ORTHO"
cam_data.ortho_scale = 2.1
cam = bpy.data.objects.new("cam", cam_data)
cam.location = (0, 0, 5)
scene.collection.objects.link(cam)
scene.camera = cam
scene.render.engine = "BLENDER_WORKBENCH"
scene.display.shading.light = "FLAT"
scene.display.shading.color_type = "SINGLE"
scene.display.shading.single_color = (1, 1, 1)
scene.display.render_aa = "32"
scene.render.film_transparent = True
scene.view_settings.view_transform = "Standard"
scene.render.resolution_x = RES
scene.render.resolution_y = RES
scene.render.image_settings.file_format = "PNG"
scene.render.image_settings.color_mode = "RGBA"
scene.render.filepath = OUT
bpy.ops.render.render(write_still=True)
print("WROTE", OUT, len(FINAL), "strokes")
