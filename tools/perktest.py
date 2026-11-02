"""Perk scenarios: drive the RUNNING game through DevBench and Protean's dev channel, and check the log.

  python -I tools/perktest.py list            # the scenarios
  python -I tools/perktest.py <name> [...]    # run some; "all" runs every one

Needs the game running with a save loaded and Protean.ini [Dev] Commands=1 (see tools/devtest.py). Each scenario
moves the player to the empty test cell (qasmoke), turns on immortal mode (damage is taken, death is not: the test
player is fragile) and grants what it needs. NEVER save the game after running these: they change the character.
"""
import re
import sys
import os
import time

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))  # python -I leaves the script's folder out
import devtest as dt  # noqa: E402
from devtest import bench, check, send

# Protean.esp local ids (tools/build_plugin.py)
STEALTH_POWER, INFECT_POWER, METAMORPHOSIS_POWER = "9ED", "9C8", "9CC"
DEVASTATOR_POWER, REGEN_POWER, STALK_POWER = "9CD", "9CE", "9B8"
INFECTION = 0x9C7

# Skyrim.esm base actors (found with the console's help command)
BASES = {"bandit": "1068FE", "farmer": "BD759", "guard": "215D5", "falmer": "23A99", "galmar": "14128"}


def con(cmd):
    """Run a console command; return its printed text."""
    r = bench("exec", cmd)
    if isinstance(r, dict):
        return "\n".join(str(x) for x in r.get("lines", []))
    return str(r)


def call(script, fn, args="[]", form=None):
    r = bench("call", script, fn, args, '{"form":"%s"}' % form) if form else bench("call", script, fn, args)
    return r.get("returned") if isinstance(r, dict) else None


def pav(name):
    return call("Actor", "GetActorValue", '["%s"]' % name, "0x14")


def spawn(base):
    """Spawn a base actor about 8 m ahead; return its reference id (hex string)."""
    t = send("spawn", base, wait_for=r"dev: spawned|dev: spawn: no base")
    m = re.search(r"\(ref ([0-9A-F]{8})\)", t)
    return m.group(1) if m else None


def log_since(off):
    return dt.read_log_from(off)


def wait_log(off, pattern, timeout=10):
    end = time.time() + timeout
    while time.time() < end:
        t = log_since(off)
        if re.search(pattern, t):
            return t
        time.sleep(0.5)
    return log_since(off)


def own_perk(pid):
    """Own a Protean perk (points are added first when short)."""
    send("evo", "500", wait_for=r"evolution|dev")
    t = send("buyperk", pid, wait_for=r"evolution")
    return t


def setup(cell=True):
    dt.preflight()
    if cell:
        con("coc qasmoke")
        wait_loaded()
    t = con("tim")
    if "Immortal Mode -> Off" in t:  # it was on already: put it back on
        con("tim")
    send("revert")


def wait_true(fn, timeout=10):
    end = time.time() + timeout
    while time.time() < end:
        if fn() is True:
            return True
        time.sleep(0.5)
    return False


def wait_loaded(timeout=60):
    end = time.time() + timeout
    time.sleep(3)
    while time.time() < end:
        s = bench("state")
        if isinstance(s, dict) and s.get("playerLoaded"):
            time.sleep(2)
            return True
        time.sleep(1)
    return False


def clear_actors():
    """Disable whatever earlier scenarios left standing near the player."""
    for ref in SPAWNED:
        con("%s.disable" % ref)
    SPAWNED.clear()


SPAWNED = []


def person(kind):
    ref = spawn(BASES[kind])
    if ref:
        SPAWNED.append(ref)
    return ref


# ---- scenarios -------------------------------------------------------------------------------------


def s_stalker2():
    """Stalker rank 2: in a fight, out of sight, the wolf fades; the enemy gives up after 3 s."""
    setup()
    own_perk("stalker")
    own_perk("stalker_2")
    send("learn", "wolf", wait_for=r"dev: learn")
    send("transform", "wolf", wait_for=r"health bonus|dev: transform")
    time.sleep(3)
    foe = person("bandit")
    check("a bandit was spawned", foe is not None)
    if not foe:
        return
    call("Actor", "StartCombat", '[{"form":"0x14"}]', "0x" + foe)
    fighting = wait_true(lambda: call("Actor", "IsInCombat", "[]", "0x14"), 10)
    check("the player is in a fight", fighting)
    # out of sight: under the floor, held there
    call("Actor", "EnableAI", "[false]", "0x" + foe)
    con("%s.setpos z %f" % (foe, (dt.player_z() or 0) - 1500))
    time.sleep(1)
    check("still in a fight with the bandit hidden", call("Actor", "IsInCombat", "[]", "0x14") is True)
    off = dt.log_size()
    send("power", STALK_POWER, wait_for=r"stalker: Stalk")
    t = wait_log(off, r"stalker: faded out", 10)
    check("faded out in a fight", "stalker: faded out" in t, t[-400:])
    t = wait_log(off, r"lost track of the player", 10)
    check("the enemy lost track", "lost track of the player" in t)
    m = re.findall(r"\[(\d\d):(\d\d):(\d\d)\.(\d+)\].*(faded out|lost track)", t)
    if len(m) >= 2:
        secs = [int(h) * 3600 + int(mi) * 60 + int(s) + int(ms) / 1000 for h, mi, s, ms, _ in m[:2]]
        check("gave up about 3 s after the fade", 2.5 <= secs[1] - secs[0] <= 4.5, "%.1f s" % (secs[1] - secs[0]))
    send("revert")
    clear_actors()


SCENARIOS = {
    "stalker2": s_stalker2,
}

if __name__ == "__main__":
    names = sys.argv[1:] or ["list"]
    if names == ["list"]:
        for k, f in SCENARIOS.items():
            print("%-14s %s" % (k, (f.__doc__ or "").strip().splitlines()[0]))
        sys.exit(0)
    for n in (SCENARIOS if names == ["all"] else names):
        print("\n=== %s ===" % n)
        SCENARIOS[n]()
    print("\nRESULT:", "ALL PASSED" if not dt.FAILS else "FAILED: " + ", ".join(dt.FAILS))
    sys.exit(1 if dt.FAILS else 0)
