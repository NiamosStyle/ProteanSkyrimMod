"""Phase 3 checks for one or more animals in a RUNNING game, no key presses needed.

  python -I tools/probe.py events fox,dog        # which attack events does each animal's graph accept?
  python -I tools/probe.py reach fox,dog         # at what distance do its bites land on a held target?

events: transforms into each animal, sends every attack event of its race (the full list, BLOCKED ignored)
        and prints the events the graph refused, ready to paste into BLOCKED in tools/build_plugin.py.
reach:  spawns a cow, holds it in place in front of the player and steps it closer, firing the left-button
        bite and the power attack at each distance; prints where hits start. Uses the shipped tuning, so it
        shows what the player gets.
Needs the starter save loaded, as a human; leaves the player human again.
"""
import json
import os
import re
import sys
import time

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
import devtest as d  # noqa: E402

ROOT = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
COW = "23A90"  # EncCow: harmless, unowned when spawned, does not fight back
DISTANCES = [200, 175, 160, 145, 130, 120, 110, 100, 90, 80]  # centre to centre; best run in QASmoke (no walls)


def table():
    full = {c["id"]: c for c in json.load(open(os.path.join(ROOT, "data", "creatures.json"), encoding="utf-8"))["creatures"]}
    return full


def become(cid):
    d.send("learn", cid, wait_for=r"dev: learn|learned")
    d.send("transform", cid, wait_for=r"dev: transform")
    time.sleep(3)


def back():
    d.send("revert", wait_for=r"dev: revert")
    time.sleep(3)


def events(ids):
    full = table()
    for cid in ids:
        become(cid)
        d.send("anim", "weaponDraw", wait_for=r"dev: anim")
        time.sleep(1.5)
        refused = []
        for a in full[cid]["attacks"]:
            ev = a["event"]
            t = d.send("anim", ev, wait_for=r"dev: anim")
            m = re.search(r"accepted by the graph: (\w+)", t)
            ok = bool(m and m.group(1) == "true")
            if not ok:
                refused.append(ev)
            print("  %-12s %-32s %s" % (cid, ev, "ok" if ok else "REFUSED"))
            time.sleep(2.5 if ok else 0.3)  # let an accepted move finish before the next one
        back()
        print('    "%s": %s,' % (cid, json.dumps(refused)))


def ref_of(text):
    m = re.search(r"\(ref ([0-9A-F]+)\)", text)
    return m.group(1) if m else None


HOME = None  # (x, y, z, yaw degrees): where the player is put back before every attack (a lunge carries it forward)


def player_coord(fn):
    r = d.bench("call", "ObjectReference", fn, "[]", '{"form":"0x14"}')
    return r.get("returned", 1e9) if isinstance(r, dict) else 1e9  # a hiccup counts as "not there yet"


def go_home():
    if not HOME:
        return
    x, y, z, yaw = HOME
    for _ in range(5):
        # the console's setpos; Papyrus SetPosition on the player was ignored after the first use (measured)
        for axis, v in (("x", x), ("y", y), ("z", z)):
            d.bench("exec", "player.setpos %s %.1f" % (axis, v))
        d.bench("exec", "player.setangle z %.1f" % yaw)
        time.sleep(0.7)
        px, py = player_coord("GetPositionX"), player_coord("GetPositionY")
        if abs(px - x) < 5 and abs(py - y) < 5:
            return
    print("  (could not put the player back on the spot)")


def reach(ids):
    for cid in ids:
        go_home()
        become(cid)
        cow = ref_of(d.send("spawn", COW, wait_for=r"dev: spawned"))
        if not cow:
            print("  %s: could not spawn the target" % cid)
            back()
            continue
        d.bench("exec", "%s.setrestrained 1" % cow)
        d.bench("exec", "%s.setav aggression 0" % cow)
        for kind in ("lmb", "power"):
            first = None
            for dist in DISTANCES:
                go_home()
                d.send("place", "%s %d" % (cow, dist), wait_for=r"dev: placed")
                time.sleep(0.8)
                off = d.log_size()
                d.send("attack", kind, wait_for=r"attack: ")
                time.sleep(2.0)
                # only the cow counts: a miss can still strike a fence or a rock (logged with no name)
                hit = re.search(r"hit: 'Cow' by (\w+) attack.*distance (\d+)", d.read_log_from(off))
                print("  %-12s %-6s distance %3d  %s" % (cid, kind, dist, ("HIT at %s" % hit.group(2)) if hit else "miss"))
                if hit and first is None:
                    first = dist
                d.bench("exec", "%s.restoreav health 1000" % cow)
            print("    %s %s: lands from %s" % (cid, kind, first))
        d.bench("exec", "%s.disable" % cow)
        back()


if __name__ == "__main__":
    if len(sys.argv) < 3 or sys.argv[1] not in ("events", "reach"):
        sys.exit(__doc__)
    if len(sys.argv) > 3:  # --home x,y,z,yaw
        HOME = tuple(float(v) for v in sys.argv[3].split(","))
    d.preflight()
    {"events": events, "reach": reach}[sys.argv[1]]([c.strip() for c in sys.argv[2].split(",") if c.strip()])
