# The Strange Mound's textures (2026-11-14): charred, cracked plates with living flesh showing through the cracks and
# in a few open sores, tiling seamlessly. Generated, not painted: the flesh colours are sampled from the Blade's arm
# texture (art/blade/textures/Arm_d.png) so the mound matches the sleeves.
#   blender -b --factory-startup --python art/mound/scripts/make_textures_m.py
# -> art/mound/textures/Mound_d.png (colour), Mound_n.png (normal map, from the height below)
import os
import numpy as np
import bpy

HERE = os.path.dirname(os.path.abspath(__file__))
ROOT = os.path.normpath(os.path.join(HERE, "..", "..", ".."))
OUT = os.path.join(ROOT, "art", "mound", "textures")
N = 1024
rng = np.random.default_rng(1114)


def periodic_noise(sigma):
    """White noise blurred by a Gaussian of sigma pixels, in frequency space: tiles by construction."""
    f = np.fft.fft2(rng.standard_normal((N, N)))
    ky = np.fft.fftfreq(N)[:, None]; kx = np.fft.fftfreq(N)[None, :]
    f *= np.exp(-2.0 * (np.pi * sigma) ** 2 * (kx ** 2 + ky ** 2))
    n = np.real(np.fft.ifft2(f))
    return (n - n.mean()) / (n.std() + 1e-9)


def cells(count, warp=0.0):
    """Distance to the nearest and second-nearest of scattered points, wrapping at the edges (tiles); the space is
    warped by noise first so the plates' edges wander instead of running straight."""
    pts = rng.random((count, 2)) * N
    yy, xx = np.mgrid[0:N, 0:N].astype(np.float32)
    if warp:
        xx = (xx + periodic_noise(40) * warp) % N
        yy = (yy + periodic_noise(40) * warp) % N
    d1 = np.full((N, N), 1e9, np.float32); d2 = np.full((N, N), 1e9, np.float32)
    for px, py in pts:
        dx = np.abs(xx - px); dx = np.minimum(dx, N - dx)
        dy = np.abs(yy - py); dy = np.minimum(dy, N - dy)
        d = np.sqrt(dx * dx + dy * dy)
        nd2 = np.where(d < d1, d1, np.minimum(d2, d))
        d1 = np.minimum(d1, d); d2 = nd2
    return d1, d2


def flesh_palette():
    img = bpy.data.images.load(os.path.join(ROOT, "art", "blade", "textures", "Arm_d.png"))
    px = np.array(img.pixels[:], np.float32).reshape(img.size[1], img.size[0], 4)[..., :3]
    lit = px[px.sum(axis=2) > 0.12]   # the painted flesh, not the black background
    return np.percentile(lit, 20, axis=0), np.percentile(lit, 85, axis=0)


def main():
    os.makedirs(OUT, exist_ok=True)
    dark_flesh, bright_flesh = flesh_palette()
    # cracks: the edges between plates (where the nearest two points are about as near), widened by noise
    d1, d2 = cells(30, warp=18.0)
    edge = d2 - d1
    width = 10.0 + 18.0 * np.clip(periodic_noise(50) * 0.5 + 0.5, 0, 1)        # cracks wide here, hairline there
    crack = np.clip(1.0 - edge / width, 0.0, 1.0) ** 1.1
    # sores: larger open patches where the char has split away
    sores = np.clip((periodic_noise(35) - 0.85) * 3.0, 0.0, 1.0)
    open_ = np.clip(crack + sores, 0.0, 1.0)
    # the char: black-brown crust, blotched with ash grey and scorched brown, blistered
    fine = periodic_noise(1.5); mid = periodic_noise(10); big = periodic_noise(60)
    ash = np.clip(big * 0.5 + 0.3, 0, 1)
    char = np.stack([0.07 + 0.05 * ash + 0.02 * mid, 0.055 + 0.045 * ash + 0.012 * mid, 0.05 + 0.045 * ash], -1)
    scorch = np.clip(mid * 0.6, 0, 1)[..., None] * np.array([0.06, 0.025, 0.01])  # brown, where it burnt less
    char += scorch
    char += (np.clip(fine - 2.3, 0, 1) * 0.08)[..., None]                       # ash flecks
    char *= (0.75 + 0.25 * np.clip(d1 / 50.0, 0, 1))[..., None]                 # plates darker toward their rims
    # the flesh: the arm's reds, fibrous
    fib = np.clip(0.5 + 0.35 * periodic_noise(4) + 0.25 * mid, 0, 1)[..., None]
    flesh = dark_flesh * (1 - fib) + bright_flesh * fib
    flesh = flesh * (1.3 + 0.9 * open_[..., None])                             # wet and brighter deep in the cracks
    a = open_[..., None]
    colour = np.clip(char * (1 - a) + flesh * a, 0, 1)
    # height: plates raised and domed, cracks sunk, sores sunk deeper
    height = 0.6 * np.clip(d1 / 45.0, 0, 1) ** 0.5 - 0.9 * crack - 0.6 * sores + 0.08 * mid + 0.03 * fine
    gy, gx = np.gradient(height)
    s = 6.0
    nrm = np.stack([-gx * s, -gy * s, np.ones_like(height)], -1)
    nrm /= np.linalg.norm(nrm, axis=-1, keepdims=True)
    save("Mound_d", colour)
    save("Mound_n", nrm * 0.5 + 0.5, flip_green=True)
    return OUT


def save(name, rgb, flip_green=False):
    if flip_green:   # Skyrim's normal maps: green points down the texture (DirectX), Blender's rows go up
        rgb = rgb.copy(); rgb[..., 1] = 1.0 - rgb[..., 1]
    img = bpy.data.images.new(name, N, N, alpha=False)
    rgba = np.concatenate([rgb, np.ones((N, N, 1), np.float32)], -1)
    img.pixels = rgba.astype(np.float32).ravel()
    img.filepath_raw = os.path.join(OUT, name + ".png"); img.file_format = 'PNG'
    img.save()
    print("wrote", img.filepath_raw)


main()
