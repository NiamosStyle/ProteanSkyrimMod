"""Zip the mod for an ordinary mod manager install (MO2 / Vortex): no dev channel, no tuning leftovers.

  python -I tools/package.py [version]        -> dist/Protean-<version>.zip

Contents: Protean.esp, SKSE/Plugins/Protean.dll, SKSE/Plugins/Protean.ini (Dev off), SKSE/Plugins/Protean/creatures.json,
SKSE/Plugins/CustomSkills/Protean.json (the perk tree; needs Custom Skills Framework).
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
# the claw models (tools/build_claws.py)
for name in ("Claws_R_M", "Claws_L_M", "Claws_R_F", "Claws_L_F"):
    files["meshes/Protean/Claws/%s.nif" % name] = os.path.join(ROOT, "data", "meshes", "Protean", "Claws", name + ".nif")
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
