"""Generate Protean.esp (via Spriggit YAML) and the plugin's runtime table from data/creatures.json.

  python -I tools/build_plugin.py [--enable wolf,deer,skeever] [--no-esp]

Writes:
  esp-yaml/                              Spriggit YAML source of the plugin (committed)
  data/Protean.esp                       the plugin (built by tools/spriggit-cli.sh deserialize)
  data/SKSE/Plugins/Protean/creatures.json   what the SKSE plugin reads at runtime

Local FormIDs are fixed by each creature's position in data/creatures.json, so enabling more creatures
later never renumbers the ones already in saves.
"""
import argparse
import json
import os
import shutil
import subprocess
import sys

ROOT = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
GAME = "C:/Games/steamapps/common/Skyrim Special Edition"
PLUGIN = "Protean.esp"

EFFECT, STARTER, REVERT, TOME, HEART_EFFECT = 0x800, 0x801, 0x802, 0x803, 0x804
BEAST_FACTION, WITNESS_FACTION = 0x805, 0x806
FACE_POWER, DISGUISE_FACTION, EXPOSED_FACTION = 0x807, 0x808, 0x809  # face stealing
# Events the animal's graph was seen to refuse when sent to the player (probed live with the dev "anim" command).
BLOCKED = {
    "deer": ["attackStart_AttackLeft1", "attackStart_AttackLeft2", "attackStart_AttackRight1", "attackStart_AttackRight2",
             "attackStart_ForwardPower", "attackStart_ForwardPowerShort", "attackStart_SkeeverLungeLong",
             "attackStart_SkeeverLungeMedium", "attackStart_SkeeverLungeShort", "attackStart_StandingPower"],
    "wolf": ["attackStart_AttackLeft1", "attackStart_AttackLeft2", "attackStart_AttackRight1", "attackStart_AttackRight2"],
}
FIRST_CREATURE = 0x810  # heart = base + 2*index, power = base + 2*index + 1


def fid(n):
    return "%06X" % n


def write(path, text):
    os.makedirs(os.path.dirname(path), exist_ok=True)
    with open(path, "w", encoding="utf-8", newline="\n") as f:
        f.write(text)


def name_block(value):
    return "Name:\n  TargetLanguage: English\n  Value: %s\n" % yq(value)


def yq(s):
    return "'" + s.replace("'", "''") + "'"


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("--enable", default="wolf,deer,skeever")
    ap.add_argument("--no-esp", action="store_true")
    a = ap.parse_args()
    table = json.load(open(os.path.join(ROOT, "data", "creatures.json"), encoding="utf-8"))
    creatures = table["creatures"]
    enabled = [c.strip() for c in a.enable.split(",") if c.strip()]
    ids = [c["id"] for c in creatures]
    for e in enabled:
        if e not in ids:
            sys.exit("unknown creature id: " + e)

    y = os.path.join(ROOT, "esp-yaml")
    if os.path.isdir(y):
        shutil.rmtree(y)
    os.makedirs(y)

    write(os.path.join(y, "spriggit-meta.json"), json.dumps({
        "PackageName": "Spriggit.Yaml.Skyrim", "Version": "0.41.0",
        "Release": "SkyrimSE", "ModKey": PLUGIN}, indent=2))
    write(os.path.join(y, "RecordData.yaml"),
          "SpriggitSource:\n  PackageName: Spriggit.Yaml.Skyrim\n  Version: 0.41\n"
          "ModKey: %s\nGameRelease: SkyrimSE\nModHeader:\n  Flags:\n  - Small\n  Stats:\n    Version: 1.7\n"
          "  Author: NiamosStyle mods\n  Description: Protean - animal shapeshifting\n"
          "  MasterReferences:\n  - Master: Skyrim.esm\n    FileSize: 0\n" % PLUGIN)

    # One shared Script-archetype effect: every power is a self-cast spell using it; the SKSE plugin
    # tells them apart by spell FormID (same pattern as the Portal Spheres powers).
    write(os.path.join(y, "MagicEffects", "Protean_ShapeshiftEffect - %s_%s.yaml" % (fid(EFFECT), PLUGIN)),
          "FormKey: %s:%s\nEditorID: Protean_ShapeshiftEffect\n" % (fid(EFFECT), PLUGIN)
          + name_block("Shapeshift")
          + "Flags:\n- NoDuration\n- NoMagnitude\n- NoArea\n- Painless\n- NoHitEffect\nBaseCost: 1\n"
          "Archetype:\n  MutagenObjectType: MagicEffectArchetype\n  Type: Script\nCastType: FireAndForget\n"
          "SkillUsageMultiplier: 1\nDualCastScale: 1\n"
          "Description:\n  TargetLanguage: English\n  Value: ''\n")

    # Hearts need ONE effect: the inventory menu dereferences an ingestible's first effect, and a food
    # with none crashed the game (measured 2026-10-19). This one does nothing at all.
    write(os.path.join(y, "MagicEffects", "Protean_HeartEffect - %s_%s.yaml" % (fid(HEART_EFFECT), PLUGIN)),
          "FormKey: %s:%s\nEditorID: Protean_HeartEffect\n" % (fid(HEART_EFFECT), PLUGIN)
          + name_block("Shapeshifter's Hunger")
          + "Flags:\n- NoDuration\n- NoMagnitude\n- NoArea\n- Painless\n- NoHitEffect\nBaseCost: 0\n"
          "Archetype:\n  MutagenObjectType: MagicEffectArchetype\n  Type: Script\nCastType: FireAndForget\n"
          "SkillUsageMultiplier: 1\nDualCastScale: 1\n"
          "Description:\n  TargetLanguage: English\n  Value: ''\n")

    # Witnesses. While transformed the player is in Protean_BeastFaction (plus the animal's own factions).
    # Anyone who was hostile to the player when the change happened is put in Protean_WitnessFaction, and
    # the two factions are enemies, so those few keep treating the player as the enemy they saw.
    def faction(local, editor_id, target):
        write(os.path.join(y, "Factions", "%s - %s_%s.yaml" % (editor_id, fid(local), PLUGIN)),
              "FormKey: %s:%s\nEditorID: %s\n" % (fid(local), PLUGIN, editor_id)
              + "Name:\n  TargetLanguage: English\n"
              "Relations:\n- Target: %s:%s\n  Reaction: Enemy\n" % (fid(target), PLUGIN)
              + "Flags:\n- HiddenFromPC\nVendorValues: {}\n")

    faction(BEAST_FACTION, "Protean_BeastFaction", WITNESS_FACTION)
    faction(WITNESS_FACTION, "Protean_WitnessFaction", BEAST_FACTION)
    # Face stealing: same idea. The player is in Protean_DisguiseFaction while wearing a stolen face; whoever saw
    # the theft is in Protean_ExposedFaction, its enemy.
    faction(DISGUISE_FACTION, "Protean_DisguiseFaction", EXPOSED_FACTION)
    faction(EXPOSED_FACTION, "Protean_ExposedFaction", DISGUISE_FACTION)

    def power(local, editor_id, title, desc):
        write(os.path.join(y, "Spells", "%s - %s_%s.yaml" % (editor_id, fid(local), PLUGIN)),
              "FormKey: %s:%s\nEditorID: %s\n" % (fid(local), PLUGIN, editor_id)
              + name_block(title)
              + "MenuDisplayObject: 0435A5:Skyrim.esm\nEquipmentType: 025BEE:Skyrim.esm\n"
              "Description:\n  TargetLanguage: English\n  Value: %s\n" % yq(desc)
              + "Type: LesserPower\nChargeTime: 0.5\nCastType: FireAndForget\n"
              "Effects:\n- BaseEffect: %s:%s\n  Data: {}\n" % (fid(EFFECT), PLUGIN))

    power(STARTER, "Protean_Instinct", "Shapeshifter's Instinct",
          "You can learn the forms of animals by eating their hearts. Cast to recall the forms you know.")
    power(REVERT, "Protean_RevertForm", "Shapeshift: Revert Form",
          "Return to your own form.")
    power(FACE_POWER, "Protean_FacePower", "Shapeshift: Stolen Face",
          "Wear the face you last stole from a corpse, or put your own face back. Sneak next to a corpse and activate it to steal a face.")

    write(os.path.join(y, "Books", "Protean_SpellTomeInstinct - %s_%s.yaml" % (fid(TOME), PLUGIN)),
          "FormKey: %s:%s\nEditorID: Protean_SpellTomeInstinct\n" % (fid(TOME), PLUGIN)
          + "ObjectBounds:\n  First: -9, -11, -1\n  Second: 9, 11, 2\n"
          + name_block("Spell Tome: Shapeshifter's Instinct")
          + "Model:\n  File: Clutter\\Books\\SpellTomeAlterationLowPoly.nif\n"
          "BookText:\n  TargetLanguage: English\n  Value: \"<font face='$HandwrittenFont'><font size='40'><p align='center'>Shapeshifter's Instinct\"\n"
          "Keywords:\n- 0937A2:Skyrim.esm\n- 0937A5:Skyrim.esm\nFlags:\n- 0x4\n"
          "Teaches:\n  MutagenObjectType: BookSpell\n  Spell: %s:%s\n" % (fid(STARTER), PLUGIN)
          + "Value: 300\nWeight: 1\nInventoryArt: 02FBB3:Skyrim.esm\n"
          "Description:\n  TargetLanguage: English\n  Value: ''\n")

    runtime = []
    for index, c in enumerate(creatures):
        if c["id"] not in enabled:
            continue
        heart = FIRST_CREATURE + 2 * index
        pw = heart + 1
        bnd = "ObjectBounds:\n  First: -3, -6, -3\n  Second: 3, 6, 3\n"
        write(os.path.join(y, "Ingestibles", "Protean_Heart_%s - %s_%s.yaml" % (c["id"], fid(heart), PLUGIN)),
              "FormKey: %s:%s\nEditorID: Protean_Heart_%s\n" % (fid(heart), PLUGIN, c["id"])
              + bnd + name_block(c["unique_item"])
              + "Keywords:\n- 08CDEA:Skyrim.esm\n"
              "Model:\n  File: %s\n" % table["item_model"].replace("\\", "\\\\").join(['"', '"'])
              + "PickUpSound: 03EDBD:Skyrim.esm\nPutDownSound: 03EDC0:Skyrim.esm\nWeight: 0.5\nValue: 5\n"
              "Flags:\n- NoAutoCalc\n- FoodItem\nConsumeSound: 0B6435:Skyrim.esm\n"
              "Effects:\n- BaseEffect: %s:%s\n  Data: {}\n" % (fid(HEART_EFFECT), PLUGIN))
        power(pw, "Protean_Power_%s" % c["id"], "Shapeshift: %s" % c["name"],
              "Become a %s. Cast again, or use Revert Form, to return." % c["name"].lower())

        rf, rid = c["race_form"].split(":")
        runtime.append({
            "id": c["id"], "name": c["name"],
            "race": {"file": rf, "id": int(rid, 16)},
            "heart": heart, "power": pw,
            "size": c["size_class"],
            "attacks": [{"event": a["event"], "flags": a["flags"], "spell": a["spell"]} for a in c.get("attacks", []) if a["event"] not in BLOCKED.get(c["id"], [])],
            "camera_height": round(0.9 * c["bound_height"]) if c.get("bound_height") else 60,
            "factions": [{"file": f.split(":")[0], "id": int(f.split(":")[1], 16)} for f in c["factions"]],
            "ladder": c["health_ladder"],
            # vanilla NPC records of this creature: dead ones get a heart when looted
            "race_names": [c["race"]],
        })

    # Keywords that mark a location as a cave-like open interior (Troll/Horker may go there). Measured over
    # the 757 vanilla locations: 274 open, 271 building, 7 conflicting, 205 neither (mostly exteriors).
    open_names = {"LocTypeDungeon", "LocTypeMine", "LocTypeDraugrCrypt", "LocTypeFalmerHive", "LocTypeAnimalDen",
                  "LocTypeDragonLair", "LocTypeDragonPriestLair", "LocTypeSprigganGrove", "LocTypeHagravenNest",
                  "LocTypeWarlockLair", "LocTypeVampireLair", "LocTypeWerewolfLair", "LocTypeWerebearLair",
                  "LocTypeGiantCamp", "LocTypeDwarvenAutomatons", "LocTypeBanditCamp", "LocTypeForswornCamp",
                  "LocTypeMilitaryFort", "LocTypeShipwreck"}
    open_kw = []
    lk = os.path.join(ROOT, "research", "raw", "loc_keywords.json")
    if os.path.isfile(lk):
        for k, name in json.load(open(lk, encoding="utf-8"))["kw"].items():
            if name in open_names:
                f, i = k.split(":")
                open_kw.append({"file": f, "id": int(i, 16), "name": name})
    else:
        print("WARNING: research/raw/loc_keywords.json missing (run tools/dump_keywords.js); cave rule disabled")

    out = os.path.join(ROOT, "data", "SKSE", "Plugins", "Protean", "creatures.json")
    write(out, json.dumps({"plugin": PLUGIN, "effect": EFFECT, "starter": STARTER, "revert": REVERT,
                           "tome": TOME, "beastFaction": BEAST_FACTION, "witnessFaction": WITNESS_FACTION,
                           "facePower": FACE_POWER, "disguiseFaction": DISGUISE_FACTION, "exposedFaction": EXPOSED_FACTION,
                           "openInteriorKeywords": open_kw, "creatures": runtime}, indent=1))
    print("yaml written; creatures enabled:", ", ".join(enabled))

    if a.no_esp:
        return
    esp = os.path.join(ROOT, "data", PLUGIN)
    os.makedirs(os.path.dirname(esp), exist_ok=True)
    cmd = ["bash", GAME + "/tools/spriggit-cli.sh", "deserialize", "--InputPath", y, "--OutputPath", esp]
    print("running:", " ".join(cmd))
    r = subprocess.run(cmd, cwd=GAME, capture_output=True, text=True)
    print((r.stdout or "")[-1500:], (r.stderr or "")[-1500:])
    if r.returncode != 0 or not os.path.isfile(esp):
        sys.exit("spriggit deserialize failed")
    print("built", esp, os.path.getsize(esp), "bytes")


if __name__ == "__main__":
    main()
