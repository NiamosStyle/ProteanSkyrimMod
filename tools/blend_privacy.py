"""Keep the user's Windows account name out of the .blend files (2026-11-14). A .blend is zstd-compressed, so a text
search of the repo never sees inside it: the preview renders had left C:\\Users\\<name>\\AppData\\Local\\Temp\\... as each
scene's render output, and Shield.blend a reference image there.

  blender -b <file.blend> --python tools/blend_privacy.py -- clean   (resets such paths, saves)
  blender -b <file.blend> --python tools/blend_privacy.py -- scan    (prints PRIVACY OK / PRIVACY HITS)

scan reads the saved file's every byte (decompressed) for a home-folder path and the names in $PROTEAN_PRIVATE
(comma-separated; set it in your own shell, never in a tracked file).
"""
import os
import re
import sys

import bpy

HOME = re.compile(rb"(?i)[a-z]:[\\/]+users[\\/]+(?!public)[^\\/\x00]+")


def risky(p):
    return bool(p) and bool(re.search(r"(?i)(^|[\\/])users[\\/]", p))   # absolute, or relative ("//..\..\Users\")


def wipe(owner, prop, value):
    """Set a path, first filling the field to its full length: Blender saves the whole fixed-size field, so a shorter
    value alone leaves the old path's tail stored after it."""
    big = owner.bl_rna.properties[prop].length_max or 1024
    old = getattr(owner, prop)
    setattr(owner, prop, b"_" * (big - 1) if isinstance(old, bytes) else "_" * (big - 1))
    setattr(owner, prop, value)


def clean():
    fixed = []
    # every path field is rewritten, risky or not: one already shortened may still hold an old path's tail
    for sc in bpy.data.scenes:
        wipe(sc.render, "filepath", "//")
        fixed.append("render " + sc.name)
    for coll in (bpy.data.images, bpy.data.sounds, bpy.data.fonts, bpy.data.cache_files, bpy.data.libraries):
        for x in coll:
            p = getattr(x, "filepath", "")
            if p:
                wipe(x, "filepath", "//" + os.path.basename(p.replace("\\", "/")) if risky(p) else p)
                fixed.append(x.name)
    # node groups appended from PyNifly's own .blend remember it (in the add-on folder under the user's AppData): swap
    # each for a plain local copy, which carries no such reference
    for ng in list(bpy.data.node_groups):
        ref = ng.library_weak_reference
        if ref is not None and risky(ref.filepath):
            name = ng.name
            new = ng.copy()
            ng.user_remap(new)
            bpy.data.node_groups.remove(ng)
            new.name = name
            fixed.append("node group " + name)
    # file browsers in the saved screens remember the last folder they showed
    for screen in bpy.data.screens:
        for area in screen.areas:
            for space in area.spaces:
                if space.type == 'FILE_BROWSER' and space.params is not None:
                    wipe(space.params, "directory", b"//")
                    fixed.append("file browser in " + screen.name)
    bpy.ops.wm.save_mainfile()
    print("PRIVACY cleaned", fixed)


def scan():
    d = open(bpy.data.filepath, "rb").read()
    if d[:4] == b"\x28\xb5\x2f\xfd":
        import io
        import zstandard
        d = zstandard.ZstdDecompressor().stream_reader(io.BytesIO(d)).read()
    words = [w.strip().encode() for w in os.environ.get("PROTEAN_PRIVATE", "").split(",") if w.strip()]
    hits = [m.group(0)[:40] for m in HOME.finditer(d)]
    for w in words:
        hits += [b"<private word>"] * len(re.findall(re.escape(w), d, re.I))
    print("PRIVACY HITS %d" % len(hits) if hits else "PRIVACY OK", len(d), "bytes scanned", hits[:5])


{"clean": clean, "scan": scan}[sys.argv[sys.argv.index("--") + 1]]()
