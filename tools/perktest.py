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


def s_stalker2_seen():
    """Stalker rank 2: in a fight, with the enemy looking right at the wolf, Stalk must not fade."""
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
    # 8 m ahead with its back turned it may never see the player: close, facing it
    send("place", foe + " 250", wait_for=r"dev: placed")
    call("Actor", "StartCombat", '[{"form":"0x14"}]', "0x" + foe)
    check("the player is in a fight", wait_true(lambda: call("Actor", "IsInCombat", "[]", "0x14"), 10))
    check("the bandit sees the wolf", wait_true(lambda: call("Actor", "HasLOS", '[{"form":"0x14"}]', "0x" + foe), 5))
    off = dt.log_size()
    send("power", STALK_POWER, wait_for=r"stalker: Stalk")
    t = wait_log(off, r"stalker: faded out", 5)
    check("did not fade while seen", "stalker: faded out" not in t, t[-400:])
    check("the reason given is being seen", "in a fight, seen" in t, t[-400:])
    send("revert")
    clear_actors()


def sneak_attack(kind):
    """Human, sneaking, dagger drawn: one real sneak attack on a fresh victim of that kind, its back turned and its AI
    off (so it cannot notice). Returns (ref, damage dealt, log text)."""
    con("player.additem 1397E 1")  # Iron Dagger
    con("player.equipitem 1397E")
    # crouched before the victim exists, so it never glimpses the player standing
    if call("Actor", "IsSneaking", "[]", "0x14") is not True:  # it toggles: a second call stands the player up
        call("Actor", "StartSneaking", "[]", "0x14")
    call("Actor", "DrawWeapon", "[]", "0x14")
    time.sleep(2)
    foe = person(kind)
    if not foe:
        return None, None, ""
    F = "0x" + foe
    call("Actor", "EnableAI", "[false]", F)
    send("place", foe + " 90 away", wait_for=r"dev: placed")
    time.sleep(6)
    check("sneaking, unseen", call("Actor", "IsSneaking", "[]", "0x14") is True
          and call("Actor", "IsDetectedBy", '[{"form":"%s"}]' % F, "0x14") is False)
    hp0 = call("Actor", "GetActorValue", '["Health"]', F)
    off = dt.log_size()
    call("Debug", "SendAnimationEvent", '[{"form":"0x14"},"attackStart"]')
    t = wait_log(off, r"hit: '", 5)
    time.sleep(1)
    hp1 = call("Actor", "GetActorValue", '["Health"]', F)
    t = log_since(off)
    return foe, (hp0 - hp1) if hp0 is not None and hp1 is not None else None, t


def s_assassin():
    """Assassin: a sneak attack on a citizen gets x10, one on a bandit (attacks on sight) does not."""
    setup()
    own_perk("assassin")
    _, dmg_f, t = sneak_attack("farmer")
    check("the farmer was sneak attacked", "sneak attack on 'Farmer'" in t, t[-300:])
    check("x10 applied to the farmer", "Assassin x10 yes" in t)
    _, dmg_b, t = sneak_attack("bandit")
    check("the bandit was sneak attacked", "sneak attack on 'Bandit'" in t, t[-300:])
    check("no x10 on the bandit", "Assassin x10 no" in t and "attacks anyone" in t)
    ok = dmg_f is not None and dmg_b is not None and dmg_b > 0 and 8 <= dmg_f / dmg_b <= 12
    check("the farmer took about 10 times the damage", ok, "farmer %s, bandit %s" % (dmg_f, dmg_b))
    clear_actors()


def s_stealth_consume():
    """Stealth Consume: cast it, sneak-kill a person, and the body is consumed at once."""
    setup()
    own_perk("assassin")
    con("player.restoreav stamina 500")
    off = dt.log_size()
    send("power", STEALTH_POWER, wait_for=r"stealth consume: (cast|refused)")
    t = wait_log(off, r"stealth consume: (cast|refused)", 5)
    check("the power was cast", "stealth consume: cast" in t, t[-300:])
    foe, _, t = sneak_attack("farmer")
    t = wait_log(off, r"consume: '.*' consumed", 10)
    check("the sneak attack was noticed", "stealth consume: sneak attack on 'Farmer'" in t, t[-500:])
    check("the farmer was killed by it", "killed by a sneak attack" in t)
    check("the body was consumed", re.search(r"consume: 'Farmer' .* consumed", t) is not None)
    check("nobody saw it", "seen by '" not in t)
    if foe:
        time.sleep(1)
        check("the body is gone", call("ObjectReference", "IsDisabled", "[]", "0x" + foe) is not False)
    clear_actors()


SCENARIOS = {
    "assassin": s_assassin,
    "stealth_consume": s_stealth_consume,
    "stalker2": s_stalker2,
    "stalker2_seen": s_stalker2_seen,
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
