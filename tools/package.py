"""Zip the mod for an ordinary mod manager install (MO2 / Vortex): no dev channel, no tuning leftovers.

  python -I tools/package.py [version]        -> dist/Protean-<version>.zip

Contents: Protean.esp, SKSE/Plugins/Protean.dll, SKSE/Plugins/Protean.ini (Dev off), SKSE/Plugins/Protean/creatures.json,
SKSE/Plugins/CustomSkills/Protean.json (the perk tree; needs Custom Skills Framework), the Rending Claws models and
attack animations (the animations need Open Animation Replacer).
Run tools/build_plugin.py (with the creatures you want enabled) and tools/build.bat first.
"""
import os
import re
import sys
import zipfile

ROOT = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
version = sys.argv[1] if len(sys.argv) > 1 else "0.1.0"
dll = os.path.join(ROOT, "build", "release", "Protean.dll")
files = {
    "Protean.esp": os.path.join(ROOT, "data", "Protean.esp"),
    "SKSE/Plugins/Protean.dll": dll,
    "SKSE/Plugins/Protean/creatures.json": os.path.join(ROOT, "data", "SKSE", "Plugins", "Protean", "creatures.json"),
    # the perk tree, shown by Custom Skills Framework (a requirement since the perk tree)
    "SKSE/Plugins/CustomSkills/Protean.json": os.path.join(ROOT, "data", "SKSE", "Plugins", "CustomSkills", "Protean.json"),
}
# every Protean model and texture: the claws, Blade, Shield, Hammerfist, the Tendril and its sleeve (tools/build_*.py).
# Until 2026-11-14 only the claw models were listed by name, so the skill tree's models and every texture were left out.
UNUSED = {"meshes/Protean/Blade/Blade_half.nif"}   # the Blade's other angle, tried 2026-11-03 and not chosen
for top in ("meshes", "textures"):
    base = os.path.join(ROOT, "data", top, "Protean")
    for folder, _, names in os.walk(base):
        for name in names:
            path = os.path.join(folder, name)
            rel = os.path.relpath(path, os.path.join(ROOT, "data")).replace("\\", "/")
            if rel not in UNUSED:
                files[rel] = path
# the claw attack animations (Open Animation Replacer; from the game's archive by tools/build_claw_anims.py)
sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
import build_claw_anims  # noqa: E402
for rel in build_claw_anims.build():
    files[rel] = os.path.join(ROOT, "data", rel)
# the glide pose (Open Animation Replacer; written from scratch by tools/build_glide_anim.py)
import build_glide_anim  # noqa: E402
for rel in build_glide_anim.build():
    files[rel] = os.path.join(ROOT, "data", rel)
# Devastator's red tentacles and splash (from the game's archive by tools/build_devastator_fx.py)
import build_devastator_fx  # noqa: E402
for rel in build_devastator_fx.build():
    files[rel] = os.path.join(ROOT, "data", rel)
# the perk tree's constellation and skydome (tools/build_constellation.py)
import build_constellation  # noqa: E402
for rel in build_constellation.build():
    files[rel] = os.path.join(ROOT, "data", rel)
# one entry per file however the builders spell their paths (the walk above already holds some of theirs)
files = {name.replace("\\", "/"): path for name, path in files.items()}
for name, path in files.items():
    if not os.path.isfile(path):
        sys.exit("missing " + path)

ini = open(os.path.join(ROOT, "data", "SKSE", "Plugins", "Protean.ini"), encoding="utf-8").read()
ini = re.sub(r"(\[Dev\][^\[]*?Commands=)1", r"\g<1>0", ini, flags=re.S)  # the dev channel is OFF in a release
assert "Commands=0" in ini, "could not switch the dev channel off"

os.makedirs(os.path.join(ROOT, "dist"), exist_ok=True)
out = os.path.join(ROOT, "dist", "Protean-%s.zip" % version)
with zipfile.ZipFile(out, "w", zipfile.ZIP_DEFLATED) as z:
    for name, path in files.items():
        z.write(path, name)
    z.writestr("SKSE/Plugins/Protean.ini", ini)
    z.write(os.path.join(ROOT, "README.md"), "README.md")
    z.write(os.path.join(ROOT, "docs", "INTEGRATION.md"), "docs/Protean-INTEGRATION.md")  # for mods built on it
print("wrote", out, os.path.getsize(out), "bytes")
