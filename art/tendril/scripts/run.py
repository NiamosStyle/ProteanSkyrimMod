# Runs steps on Tendril.blend without Blender's window:
#   blender -b art/tendril/Tendril.blend --python art/tendril/scripts/run.py -- <step> [<step> ...] [nosave]
# A step is "<file>:<call>", e.g. "make_flesh_c:make_flesh()"; the file's names are kept for the later steps.
import bpy, sys, os, traceback
D = os.path.dirname(os.path.abspath(__file__))
ns = {"__name__": "tendril"}
steps = sys.argv[sys.argv.index("--") + 1:]
ok = True
for st in steps:
    if st == "nosave": continue
    f, _, call = st.partition(":")
    try:
        exec(compile(open(os.path.join(D, f + ".py"), encoding="utf-8").read(), f + ".py", "exec"), ns)
        if call:
            print("STEP", st, "->", eval(call, ns))
    except Exception:
        ok = False; print("STEP FAILED", st); traceback.print_exc(); break
if ok and "nosave" not in steps:
    bpy.ops.wm.save_mainfile()
    print("STEP saved")
