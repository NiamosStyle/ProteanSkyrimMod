"""Drive Protean in a RUNNING game, no key presses needed, and check the results.

  python -I tools/devtest.py core        # starter, hearts, learn, transform, revert, health/race/camera checks
  python -I tools/devtest.py reactions   # spawn animals, scan how they treat you before/after transforming
  python -I tools/devtest.py attacks     # does the wolf's graph accept its own attack events?
  python -I tools/devtest.py death       # damage past your normal maximum, then revert (KILLS the player)

Requires: the game running with a save loaded, the Protean mod enabled, and Protean.ini [Dev] Commands=1.
It talks to the plugin through Data/SKSE/Plugins/Protean.cmd (the file must exist BEFORE the game starts,
because MO2's virtual file system does not show files created later) and reads Protean.log for results.
Cross-checks the plugin's own claims against DevBench (race, health, camera) wherever it can.
"""
import json
import os
import re
import subprocess
import sys
import time

MO2_MOD = r"C:\Users\user\AppData\Local\ModOrganizer\Skyrim Special Edition\mods\Protean"
CMD = os.path.join(MO2_MOD, "SKSE", "Plugins", "Protean.cmd")
LOG = r"C:\Users\user\OneDrive\Documents\My Games\Skyrim Special Edition\SKSE\Protean.log"
GAME = "C:/Games/steamapps/common/Skyrim Special Edition"

FAILS = []


def bench(*args):
    """Run tools/devbench-cli.sh and return parsed JSON (or the raw text)."""
    r = subprocess.run(["bash", "tools/devbench-cli.sh", *args], cwd=GAME, capture_output=True, text=True, timeout=60)
    out = r.stdout.strip()
    try:
        return json.loads(out)
    except ValueError:
        return out or r.stderr.strip()


def log_size():
    return os.path.getsize(LOG) if os.path.exists(LOG) else 0


def read_log_from(offset):
    with open(LOG, "rb") as f:
        f.seek(offset)
        return f.read().decode("utf-8", "replace")


_seq = [int(time.time())]


def send(cmd, arg="", wait_for=None, timeout=15):
    """Append one command line; optionally wait until the log shows `wait_for` (regex). Returns the new log text."""
    _seq[0] += 1
    off = log_size()
    with open(CMD, "a", encoding="utf-8") as f:
        f.write("%d %s %s\n" % (_seq[0], cmd, arg))
    end = time.time() + timeout
    text = ""
    while time.time() < end:
        time.sleep(0.5)
        text = read_log_from(off)
        if wait_for is None:
            if "dev:" in text:
                break
        elif re.search(wait_for, text):
            break
    return text


def check(name, ok, detail=""):
    print(("PASS  " if ok else "FAIL  ") + name + (("  -- " + detail) if detail else ""))
    if not ok:
        FAILS.append(name)


def player():
    d = bench("inspect", "player")
    return d if isinstance(d, dict) else {}


def camera():
    d = bench("tool", "camera", '{"action":"get"}')
    return d if isinstance(d, dict) else {}


def player_z():
    r = bench("call", "ObjectReference", "GetPositionZ", "[]", '{"form":"0x14"}')
    return r.get("returned") if isinstance(r, dict) else None


def preflight():
    if not os.path.exists(CMD):
        sys.exit("Protean.cmd does not exist in the mod folder: create it, then RESTART the game once.")
    a = bench("alive")
    if "RUNNING" not in str(a):
        sys.exit("the game is not running with a save loaded: " + str(a)[:200])
    t = send("dump", wait_for=r"dev: dump")
    if "dev: dump" not in t:
        sys.exit("the plugin did not answer: is [Dev] Commands=1 in Protean.ini, and Protean.dll loaded?")


def scenario_core():
    preflight()
    send("starter")
    send("hearts")
    send("learn", "wolf", wait_for=r"learned|dev: learn")
    p0 = player()
    hp0 = p0.get("actorValues", {}).get("health", {}).get("max")
    race0 = p0.get("race", {}).get("name")
    print("before: race %s, max health %s" % (race0, hp0))

    t = send("transform", "wolf", wait_for=r"health bonus")
    time.sleep(3)
    p1 = player()
    check("race is Wolf", p1.get("race", {}).get("name") == "Wolf", str(p1.get("race")))
    cur = p1.get("actorValues", {}).get("health", {}).get("current")
    check("health went up by the bonus", cur is not None and hp0 is not None and cur > hp0, "now %s, was %s" % (cur, hp0))
    t2 = read_log_from(max(0, log_size() - 4000))
    check("camera node created", "created 'ProteanCam'" in t2)
    z = player_z()
    cam = camera()
    if z is not None and "camZ" in cam:
        check("camera is above the animal's feet", cam["camZ"] - z > 20, "camera is %.0f above the feet" % (cam["camZ"] - z))
    check("third person", cam.get("pov") == "third", str(cam.get("pov")))

    send("revert", wait_for=r"dev: revert done")
    time.sleep(2)
    p2 = player()
    check("race is back", p2.get("race", {}).get("name") == race0, str(p2.get("race")))
    check("health is back", p2.get("actorValues", {}).get("health", {}).get("max") == hp0)
    t3 = read_log_from(max(0, log_size() - 3000))
    check("engine camera node re-bound", "re-bound the engine's own Camera3rd" in t3 or "toggling the camera" in t3)
    send("dump", wait_for=r"dev: dump")


def scenario_attacks():
    preflight()
    send("starter")
    send("transform", "wolf", wait_for=r"health bonus")
    time.sleep(2)
    send("draw", wait_for=r"dev: draw")
    for ev in ("attackStart_Attack1", "attackStart_ForwardPower", "attackStart", "attackPowerStart"):
        t = send("anim", ev, wait_for=r"dev: anim")
        m = re.search(r"dev: anim '(\S+)' accepted by the graph: (\w+)", t)
        print("  %-28s -> %s" % (ev, m.group(2) if m else "no answer"))
        time.sleep(1.5)
    print("Look at the game too: did the wolf visibly attack for the creature events?")
    send("revert", wait_for=r"dev: revert done")


def scenario_reactions():
    preflight()
    send("starter")
    # base objects: EncWolf 0x23ABE, EncDeer 0x2397B? (verify), EncSkeever, EncBandit..: spawn by FormID
    for base in ("23ABE",):
        send("spawn", base, wait_for=r"dev: spawn")
    time.sleep(2)
    t = send("scan", wait_for=r"dev: scan done")
    print("--- before transforming ---\n" + "\n".join(l for l in t.splitlines() if "scan" in l))
    send("transform", "wolf", wait_for=r"health bonus")
    time.sleep(3)
    t = send("scan", wait_for=r"dev: scan done")
    print("--- after transforming into a wolf ---\n" + "\n".join(l for l in t.splitlines() if "scan" in l))
    print("Expect: the spawned wolf is no longer hostile unless it counted as a witness.")
    send("revert", wait_for=r"dev: revert done")


def scenario_death():
    preflight()
    send("starter")
    send("transform", "wolf", wait_for=r"health bonus")
    time.sleep(2)
    base = player().get("actorValues", {}).get("health", {}).get("max") or 100
    bench("exec", "player.damageav health %d" % (int(base) + 5))
    time.sleep(1)
    t = send("revert", wait_for=r"killing the player|dev: revert done")
    check("the revert killed the player", "killing the player" in t)
    print("Check in game: death sound and the normal death camera.")


if __name__ == "__main__":
    name = sys.argv[1] if len(sys.argv) > 1 else "core"
    {"core": scenario_core, "attacks": scenario_attacks, "reactions": scenario_reactions, "death": scenario_death}[name]()
    print("\nRESULT:", "ALL PASSED" if not FAILS else "FAILED: " + ", ".join(FAILS))
    sys.exit(1 if FAILS else 0)
