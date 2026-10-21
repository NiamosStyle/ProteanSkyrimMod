"""Build data/creatures.json and docs/creature-table.md from research/raw (see dump_game_data.js).

Everything factual (race, health range, attacks, size flags) comes from the game data.
Everything in the CREATURES list below is a DESIGN decision awaiting the user's approval.

Run:  python -I tools/build_creature_table.py
"""
import json
import os
import re
import collections

ROOT = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
RAW = os.path.join(ROOT, "research", "raw")

# size: how big the animal is for the indoor rule.
#   small / medium  -> allowed anywhere
#   large_cave_ok   -> Troll, Horker: caves/mines/ruins yes, buildings no (user decision)
#   outdoor_only    -> Mammoth, Giant: open air only (user decision; Giant is an assumption to confirm)
# kind: how hard the form is to build (1 easiest .. 8 hardest), see docs/PLAN.md.
# race: the vanilla race whose model/skeleton/attacks the player copy is built from.
# npcs: NPC display names in the game data that look like this creature (variants share the race).
CREATURES = [
    # id, name, race, npc names, size, rank, item, notes
    ("wolf", "Wolf", "WolfRace", ["Wolf"], "medium", 1, "Wolf Heart", "Predator."),
    ("ice_wolf", "Ice Wolf", "WolfRace", ["Ice Wolf"], "medium", 1, "Ice Wolf Pelt Strip", "Same race as Wolf; own skin."),
    ("pit_wolf", "Pit Wolf", "WolfRace", ["Pit Wolf"], "medium", 1, "Pit Wolf Scarred Hide", "Same race as Wolf; own skin."),
    ("deer", "Deer", "DeerRace", [], "medium", 1, "Deer Tongue", "Prey; name 'Deer' is a base object, not an NPC name."),
    ("elk", "Elk", "ElkRace", [], "medium", 1, "Elk Antler Velvet", "Prey."),
    ("fox", "Fox", "FoxRace", [], "small", 1, "Fox Brush", "Tiny-ish; fast. FoxRace is also the game's placeholder race for template actors."),
    ("hare", "Hare", "HareRace", ["Rabbit"], "small", 1, "Hare Leg", "Tiny; both reference mods already do this one."),
    ("dog", "Dog", "DogRace", [], "medium", 1, "Dog Snout", "Livestock-like; friendly to most."),
    ("goat", "Goat", "GoatRace", [], "medium", 1, "Goat Horn Marrow", "Livestock."),
    ("cow", "Cow", "CowRace", [], "medium", 2, "Cow Udder", "Livestock."),
    ("bear", "Bear", "BearBlackRace", [], "medium", 2, "Bear Heart", "Race is the black bear; brown and snow bears have their own races."),
    ("cave_bear", "Cave Bear", "BearBrownRace", ["Cave Bear"], "medium", 2, "Cave Bear Liver", "Brown bear race."),
    ("snow_bear", "Snow Bear", "BearSnowRace", ["Snow Bear"], "medium", 2, "Snow Bear Paw Pad", ""),
    ("sabre_cat", "Sabre Cat", "SabreCatRace", ["Sabre Cat"], "medium", 2, "Sabre Cat Fang Root", ""),
    ("snowy_sabre_cat", "Snowy Sabre Cat", "SabreCatSnowyRace", ["Snowy Sabre Cat"], "medium", 2, "Snowy Sabre Cat Whisker Gland", ""),
    ("vale_sabre_cat", "Vale Sabre Cat", "DLC1SabreCatGlowRace", ["Vale Sabre Cat"], "medium", 2, "Vale Sabre Cat Glowing Gland", "Dawnguard."),
    ("death_hound", "Death Hound", "DLC1DeathHoundRace", ["Death Hound"], "medium", 2, "Death Hound Fang", "Dawnguard."),
    ("boar", "Boar", "DLC2BoarRace", ["Boar"], "medium", 2, "Boar Shoulder", "Dragonborn. Added at the user's request."),
    ("skeever", "Skeever", "SkeeverRace", [], "small", 3, "Skeever Tail Tip", "Tiny: camera check."),
    ("chicken", "Chicken", "ChickenRace", [], "small", 3, "Chicken Wing", "Tiny: camera check. Has no attacks at all."),
    ("ash_hopper", "Ash Hopper", "DLC2AshHopperRace", ["Ash Hopper"], "small", 3, "Ash Hopper Foreleg", "Dragonborn. Tiny: camera check."),
    ("mudcrab", "Mudcrab", "MudcrabRace", ["Mudcrab"], "small", 3, "Mudcrab Claw Meat", "Tiny: camera check."),
    ("albino_spider", "Albino Spider", "DLC2ExpSpiderBaseRace", ["Albino Spider"], "small", 3, "Albino Spider Gland", "Dragonborn Expedition spider. Its own race, NOT Frostbite."),
    ("chaurus", "Chaurus", "ChaurusRace", ["Chaurus"], "medium", 4, "Chaurus Egg Sac", ""),
    ("troll", "Troll", "TrollRace", ["Troll"], "large_cave_ok", 4, "Troll Fat", "Caves ok, buildings no."),
    ("frost_troll", "Frost Troll", "TrollFrostRace", ["Frost Troll"], "large_cave_ok", 4, "Frost Troll Sinew", "Caves ok, buildings no."),
    ("horse", "Horse", "HorseRace", [], "medium", 4, "Horse Flank", "NPCs/game must not try to ride or claim the player."),
    ("horker", "Horker", "HorkerRace", ["Horker"], "large_cave_ok", 5, "Horker Blubber", "Land and water."),
    ("mammoth", "Mammoth", "MammothRace", ["Mammoth"], "outdoor_only", 5, "Mammoth Snout Flesh", "Huge, outdoors only."),
    ("giant", "Giant", "GiantRace", ["Giant"], "outdoor_only", 5, "Giant Marrow", "Added at the user's request. Outdoors only is MY assumption: confirm."),
    ("werewolf", "Werewolf", "WerewolfBeastRace", ["Werewolf"], "medium", 6, "Werewolf Heart", "Needs OUR OWN race copy: no lycanthropy, no Companions conflict."),
    ("werebear", "Werebear", "DLC2WerebearBeastRace", ["Werebear"], "medium", 6, "Werebear Heart", "Dragonborn. Same caution as Werewolf."),
    ("slaughterfish", "Slaughterfish", "SlaughterfishRace", ["Slaughterfish"], "small", 7, "Slaughterfish Fin", "Swim-only: real swimming/diving, cannot leave water."),
    ("chaurus_hunter", "Chaurus Hunter", "DLC1ChaurusHunterRace", ["Chaurus Hunter"], "medium", 8, "Chaurus Hunter Wing Membrane", "Dawnguard. Needs real flight (vanilla race has no Flies flag)."),
    ("chaurus_hunter_fledgling", "Chaurus Hunter Fledgling", "DLC1ChaurusHunterRace", ["Chaurus Hunter Fledgling"], "medium", 8, "Fledgling Wing Bud", "Dawnguard. Same race as Chaurus Hunter."),
    # added 2026-10-21 (user): appended last so no earlier creature's FormIDs move
    ("frostbite_spider", "Frostbite Spider", "FrostbiteSpiderRace", ["Frostbite Spider"], "medium", 3, "Frostbite Spider Heart", "The regular one; the Large and Giant ones have their own races."),
]

races = json.load(open(os.path.join(RAW, "races.json"), encoding="utf-8"))
npcs = json.load(open(os.path.join(RAW, "npcs.json"), encoding="utf-8"))
spells = json.load(open(os.path.join(RAW, "spells.json"), encoding="utf-8"))


def spell_name(fk):
    key = fk.split(" ")[0]
    m = re.match(r"^(.+):0*([0-9A-Fa-f]+)$", key)
    if not m:
        return None
    k = m.group(1) + ":" + m.group(2).upper().rjust(6, "0")
    s = spells.get(k)
    return (s["edid"] if s else None)


def attacks_of(race_json):
    out = []
    for a in race_json.get("Attacks", []) or []:
        d = a["ATKD - Attack Data"]
        flags = [k for k, v in d["Attack Flags"].items() if v]
        sp = d["Attack Spell"]
        has_spell = isinstance(sp, str) and sp not in ("", "0", "NULL - Null Reference [00000000]")
        out.append({
            "event": a["ATKE - Attack Event"],
            "flags": flags,
            "spell": spell_name(sp) if has_spell else None,
            "damage_mult": round(d["Damage Mult"], 2),
            "stagger": round(d["Stagger"], 2),
        })
    return out


by_name = collections.defaultdict(list)
for n in npcs:
    nm = n["json"].get("FULL - Name", "")
    if nm:
        by_name[nm].append(n)

rows = []
for cid, name, race, nnames, size, rank, item, notes in CREATURES:
    nnames = nnames or [name]
    item = name + " Heart"   # user decision 2026-10-19: every item is "<Animal> Heart"
    r = races[race]["json"]
    d = r["DATA - "]
    flags = [k for k, v in d["Flags"].items() if v] + [k for k, v in d.get("Flags 2", {}).items() if v]
    recs = [x for nm in nnames for x in by_name.get(nm, [])]
    # same-race records only: template placeholders (FoxRace) tell us nothing about this creature
    recs = [x for x in recs if x["race"] == race] or recs
    levels = [x["json"]["ACBS - Configuration"]["Level"] for x in recs]
    hps = [x["json"].get("DNAM - Player Skills", {}).get("Health") for x in recs]
    hps = [h for h in hps if h]
    # Health ladder: one rung per level, using the vanilla NPC records for this creature (skipping
    # test/dead duplicates). Runtime picks the highest rung with level <= player level (or the lowest
    # rung if the player is below all of them): this is "the health this creature has at your level".
    good = [x for x in recs if not re.search(r"Dead|Test", x["json"].get("EDID - Editor ID", ""))]
    rungs = {}
    for x in good:
        lv = x["json"]["ACBS - Configuration"]["Level"]
        hp = x["json"].get("DNAM - Player Skills", {}).get("Health")
        if hp:
            rungs[lv] = min(hp, rungs.get(lv, hp))
    ladder = sorted(rungs.items())
    # Factions the creature's own records share (the player joins them while transformed).
    fsets = [set(f["Faction"] for f in x["json"].get("Factions", [])) for x in good if x["race"] == race]
    shared = sorted(set.intersection(*fsets)) if fsets else []
    zs = [x["json"].get("OBND - Object Bounds", {}).get("Z2") for x in good if x["race"] == race]
    zs = [z for z in zs if z]
    atk = attacks_of(r)
    special = [a for a in atk if a["flags"] or a["spell"]]
    rows.append({
        "id": cid,
        "name": name,
        "race": race,
        "race_form": "%s:%s" % (races[race]["file"], races[race]["id"]),
        "size_class": size,
        "difficulty_rank": rank,
        "unique_item": item,
        "notes": notes,
        "race_base_health": round(d["Starting Health"]),
        "race_base_stamina": round(d["Starting Stamina"]),
        "race_flags": flags,
        "skeleton": (r.get("ANAM - Male Skeletal Model") or "").replace("\\", "/"),
        "vanilla_npc_records": len(recs),
        "vanilla_level_range": [min(levels), max(levels)] if levels else None,
        "vanilla_health_range": [min(hps), max(hps)] if hps else None,
        "bound_height": max(zs) if zs else None,
        "health_ladder": ladder,
        "factions": shared,
        "attacks": atk,
        "attack_count": len(atk),
        "special_attacks": special,
    })

os.makedirs(os.path.join(ROOT, "data"), exist_ok=True)
with open(os.path.join(ROOT, "data", "creatures.json"), "w", encoding="utf-8") as f:
    json.dump({"generated_from": "vanilla Skyrim.esm/Update.esm/Dawnguard.esm/Dragonborn.esm",
               "item_model": "Clutter\\Ingredients\\HumanHeart.nif",   # vanilla Human Heart mesh (referenced, not copied)
               "item_model_source": "Skyrim.esm:0B18CD HumanHeart",
               "creatures": rows}, f, indent=1)

lines = ["# Creature table (generated by tools/build_creature_table.py)", "",
         "Facts come from the game data; item names, size classes and ranks are PROPOSALS for approval.", "",
         "Health: the race value is only a base. Real health is measured live in Phase 1.", "",
         "| # | Creature | Race | Size rule | Rank | Proposed item | Vanilla HP seen | Vanilla level | Attacks (special) |",
         "|---|---|---|---|---|---|---|---|---|"]
for i, c in enumerate(rows, 1):
    hp = "-" if not c["vanilla_health_range"] else "%d-%d" % tuple(c["vanilla_health_range"])
    lv = "-" if not c["vanilla_level_range"] else "%d-%d" % tuple(c["vanilla_level_range"])
    lines.append("| %d | %s | %s | %s | %d | %s | %s | %s | %d (%d) |" % (
        i, c["name"], c["race"], c["size_class"], c["difficulty_rank"], c["unique_item"], hp, lv,
        c["attack_count"], len(c["special_attacks"])))
lines += ["", "## Special attacks (power attacks, bashes, spell attacks) per creature", ""]
for c in rows:
    if not c["special_attacks"]:
        lines.append("- **%s**: none beyond normal attacks" % c["name"])
        continue
    parts = []
    for a in c["special_attacks"]:
        tag = ",".join(a["flags"] + (["spell:" + a["spell"]] if a["spell"] else []))
        parts.append("%s [%s]" % (a["event"].replace("attackStart_", "").replace("attackStart", ""), tag or "spell"))
    lines.append("- **%s**: %s" % (c["name"], "; ".join(parts)))
with open(os.path.join(ROOT, "docs", "creature-table.md"), "w", encoding="utf-8") as f:
    f.write("\n".join(lines) + "\n")
print("creatures:", len(rows), "unique items:", len({c['unique_item'] for c in rows}))
