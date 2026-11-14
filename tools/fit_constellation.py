"""Places the Protean perk tree's stars along the tentacles of its constellation picture, and draws a mock-up of the
perk menu to judge the fit before a game launch.

The menu's two mappings were measured from the user's screenshot (ScreenShot94, 2560x1600, 2026-11-14):
  tree:    screen X = 1280 - x * (73 - 6y),  Y = 797 - (119y - 9y^2)   (x, y = the node's coordinates in
           CustomSkills/Protean.json; the tree tilts away from the camera, so it narrows and squashes going up)
  picture: at the skydome's original size, 448 px per drawing unit, the drawing's centre at (1298, 648)
tools/build_constellation.py scales the picture by PICTURE_SCALE and shifts it so that the drawing point ROOT_AT
lands on the root star; here the stars are given as points in the drawing and turned into node coordinates.

  python -I tools/fit_constellation.py   prints the node coordinates and writes the mock-up
"""
import math
import os
import sys

ROOT = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
import build_constellation as bc  # noqa: E402

PX_PER_UNIT = 448 * bc.PICTURE_SCALE
ROOT_STAR = (1280, 797)


def picture_to_screen(p):
    return (ROOT_STAR[0] + (p[0] - bc.ROOT_AT[0]) * PX_PER_UNIT, ROOT_STAR[1] - (p[1] - bc.ROOT_AT[1]) * PX_PER_UNIT)


def screen_to_node(X, Y):
    d = 119 ** 2 - 36 * (797 - Y)
    assert d >= 0, "too high for the measured tree mapping"
    y = (119 - math.sqrt(d)) / 18
    x = (1280 - X) / (73 - 6 * y)
    return round(x, 2), round(y, 2)


def node_to_screen(x, y):
    return 1280 - x * (73 - 6 * y), 797 - (119 * y - 9 * y * y)


# ---- where each star sits in the picture (drawing units; placed by hand over build/constellation_grid.png, about
# 0.25 apart, which is ~60 px on screen, vanilla's spacing; constellations follow their picture loosely) -----------
STARS = {
    "unbound_form": bc.ROOT_AT,
    # Beast: up the left tentacle; Apex above its curl, touching the tall thin tentacle (user, 2026-11-14)
    "keen": (-0.58, -0.50),
    "stalker": (-0.50, -0.28),
    "predator": (-0.65, -0.10),
    "survivor": (-0.78, 0.12),
    "apex": (-0.43, 0.52),
    # Hunter: the centre tentacle and the front one's hook, Devastator in the centre curl. Siblings stand apart
    # so no line runs through another star (user: Tendril/Hammerfist and Shield/Regeneration read as chains)
    "rending_claws": (-0.22, -0.48),
    "predators_rush": (0.02, -0.48),
    "subtle_carapace": (0.26, -0.48),
    "tendril": (-0.32, -0.12),
    "preys_flight": (0.04, -0.20),
    "shield": (0.44, -0.28),
    "hammerfist": (-0.20, 0.06),
    "regeneration": (0.30, 0.04),
    "blade": (0.00, 0.30),
    "devastator": (0.20, 0.40),
    # Deceiver: up the right tentacle, Assassin in its curl, Metamorphosis on the low right one
    "borrowed_visage": (0.54, -0.52),
    "inconspicuous": (0.66, -0.18),
    "metamorphosis": (0.86, -0.40),
    "assassin": (0.56, 0.08),
    "infected": (0.82, 0.08),
}


def layout():
    """perk id -> (x, y) node coordinates"""
    return {k: screen_to_node(*picture_to_screen(p)) for k, p in STARS.items()}


def mock(path, links):
    from PIL import Image, ImageDraw
    W, H = 2560, 1600
    canvas = Image.new("RGBA", (W, H), (12, 8, 10, 255))
    pic = bc.picture()
    size = int(round(2.1 * PX_PER_UNIT))            # the texture spans 2.1 drawing units
    pic = pic.resize((size, size), Image.LANCZOS)
    r, g, b, a = pic.split()
    tint = Image.merge("RGBA", (r.point(lambda v: 255), g.point(lambda v: 190), b.point(lambda v: 90), a))
    cx, cy = picture_to_screen((0.0, 0.0))
    canvas.alpha_composite(tint, (int(cx - size / 2), int(cy - size / 2)))
    d = ImageDraw.Draw(canvas)
    pos = {k: node_to_screen(*v) for k, v in layout().items()}
    for k, lst in links.items():
        for o in lst:
            d.line([pos[k], pos[o]], fill=(235, 225, 255, 255), width=3)
    for k, (x, y) in pos.items():
        d.ellipse([x - 7, y - 7, x + 7, y + 7], fill=(255, 255, 255, 255))
        d.text((x + 10, y - 18), k, fill=(255, 255, 255, 255))
    canvas.convert("RGB").save(path)
    print("mock-up:", path)


if __name__ == "__main__":
    import build_plugin
    for k, v in layout().items():
        print("%-16s %6.2f %6.2f" % (k, *v))
    links = {k: v[5] for k, v in build_plugin.PERKS.items()}
    out = sys.argv[1] if len(sys.argv) > 1 else os.path.join(ROOT, "build", "constellation_mock.png")
    os.makedirs(os.path.dirname(out), exist_ok=True)
    mock(out, links)
