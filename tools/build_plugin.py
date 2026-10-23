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
# Integration markers (docs/INTEGRATION.md). Per form, by its index in data/creatures.json (stable):
# keyword Protean_Form_<id> = FORM_KEYWORD + index, its effect FORM_EFFECT + index, the hidden ability FORM_ABILITY + index.
KW_TRANSFORMED = 0x80A
EVOLUTION = 0x80B  # the power that opens the perk tree (Custom Skills Framework menu)
# The perk tree (docs/protean-perk-spec.md). Perk records are plain: Protean's plugin reads HasPerk and does the work.
# id -> (local id, name, description, x, y, links). Appended, never renumbered: saves remember the perks.
# Tree coordinates (Custom Skills Framework, vanilla-like spacing: neighbours about 1 unit apart so their names show
# when zoomed in): x positive = left, y positive = up. Beast left, Hunter middle, Deceiver right.
# The menu stretches the layout: TREE_SCALE_X across, TREE_SCALE_Y up (1 looked small next to the vanilla werewolf
# tree; 2.5 both ways was too tall). Moving the root below 0 made the whole tree draw bigger, so it stays at y 0.
TREE_SCALE_X = 1.0
TREE_SCALE_Y = 1.0
PERKS = {
    "unbound_form": (0x960, "Unbound Form",
                     "You can shapeshift. Hearts drop from animals you kill, disease cannot touch you, and every kill "
                     "speeds your stamina's recovery for a while (the boosts stack).", 0, 0,
                     ["keen", "rending_claws", "predators_rush", "subtle_carapace", "borrowed_visage"]),
    # Beast
    "keen": (0x961, "Keen", "In animal form, switch on night vision, Detect Life and Detect Death.", 1.8, 0.8,
             ["survivor", "predator", "stalker"]),
    "survivor": (0x962, "Survivor", "Your animal forms gain more health: 50% of your own instead of 25%, plus a quarter "
                 "of the difference when the animal is stronger than you.", 3.3, 1.9, ["apex"]),
    "predator": (0x963, "Predator", "Your animal forms deal more damage.", 2.3, 2.1, ["apex"]),
    "stalker": (0x964, "Stalker", "Your animal forms are harder to detect.", 1.4, 1.9, ["apex"]),
    "apex": (0x965, "Apex", "Take the form of a dragon. Requires Survivor, Predator and Stalker.", 2.3, 3.4, []),
    # Hunter
    "rending_claws": (0x966, "Rending Claws", "Claws grow from your hands: a weapon for each hand, at a stamina cost.",
                      0.8, 0.9, ["tendril", "devastator"]),
    "tendril": (0x967, "Tendril", "A whip of flesh lashes out, tearing at health and stamina; you take part of the "
                "stamina it drains.", 0.9, 2, []),
    "predators_rush": (0x968, "Predator's Rush", "You move faster, sprint faster still, and leap higher.", 0, 1.1,
                       ["preys_flight"]),
    "preys_flight": (0x969, "Prey's Flight", "Jump again in mid-air, and glide.", 0, 2.2, ["devastator"]),
    "subtle_carapace": (0x96A, "Subtle Carapace", "A hidden carapace hardens your skin: more armor.", -0.8, 0.9,
                        ["regeneration", "devastator"]),
    "regeneration": (0x96B, "Regeneration", "A toggle: while on, stamina drains and health returns by the same amount.",
                     -0.9, 2, []),
    "devastator": (0x96C, "Devastator", "Unleash a devastating attack all around you. Costs all your stamina and needs "
                   "two kill boosts. Requires Rending Claws, Prey's Flight and Subtle Carapace.", 0, 3.4, []),
    # Deceiver
    "borrowed_visage": (0x96D, "Borrowed Visage", "Steal the face of a corpse and wear it.", -1.8, 0.8,
                        ["metamorphosis", "inconspicuous"]),
    "metamorphosis": (0x96E, "Metamorphosis", "Make a stolen face and race your own, and leave your crimes behind. "
                      "Costs all your stamina.", -3.3, 1.9, []),
    "inconspicuous": (0x96F, "Inconspicuous", "The more friendly people around you, the harder you are to notice.",
                      -1.9, 2, ["infected", "assassin"]),
    "infected": (0x970, "Infected", "Slip an infection into someone's pocket; the infected fight for you.", -1.4, 3.1, []),
    "assassin": (0x971, "Assassin", "Sneak attacks are far deadlier against someone who trusts the face you wear.",
                 -2.5, 3.3, []),
}
# Second ranks: their own records, chained from the first rank (NextPerk); the menu shows them on the same node.
# base id -> (local id, description)
RANK2 = {
    "survivor": (0x972, "Your animal forms gain still more health: 75% of your own, plus a quarter of the difference. "
                 "Predators can feed on corpses to heal."),
    "predator": (0x973, "Your animal forms' attacks also poison."),
    "stalker": (0x974, "Enemies lose track of your animal forms sooner."),
    "subtle_carapace": (0x975, "The carapace hardens further: more armor."),
}
# Perks that work. The rest are placeholders: Protean takes them back and returns the point.
BUILT = {"unbound_form"}
FORM_KEYWORD, FORM_EFFECT, FORM_ABILITY = 0x880, 0x8C0, 0x900
# Dials read live by the SKSE plugin: name -> (local id, default, Float/Short, what it does)
GLOBALS = {
    "Protean_ActiveForm":          (0x940, 0,    "Short", "0 = human; otherwise the form's index + 1 (set by Protean, read-only)"),
    "Protean_HealthBonusPct":      (0x941, 0.25, "Float", "bonus health = this x the player's normal max health (plus the gap share)"),
    "Protean_HealthGapShare":      (0x942, 0.25, "Float", "bonus health also = this x how far the animal's health (at the player's level) exceeds the player's"),
    "Protean_SprintMult":          (0x943, 1.0,  "Float", "multiplies every animal's sprint boost"),
    "Protean_SprintStaminaPerSec": (0x944, 7.0,  "Float", "stamina drained per second of sprinting"),
    "Protean_LeapMult":            (0x945, 1.0,  "Float", "multiplies leap distance"),
    "Protean_LeapHeightMult":      (0x946, 1.0,  "Float", "multiplies leap height"),
    "Protean_FlySpeed":            (0x947, 600,  "Float", "flight speed, game units per second"),
    "Protean_SpitCostMult":        (0x948, 0.5,  "Float", "spit stamina cost as a share of a power attack's"),
    "Protean_ReachMult":           (0x949, 1.0,  "Float", "multiplies the forms' melee reach"),
    "Protean_StrikeAngleMult":     (0x94A, 1.0,  "Float", "multiplies the forms' strike cone"),
    "Protean_DamageMult":          (0x94B, 1.0,  "Float", "multiplies all damage the player deals while transformed"),
    "Protean_ArmorBonus":          (0x94C, 0,    "Float", "armor rating added while transformed"),
    "Protean_ChangeStaminaCost":   (0x94D, 15,   "Float", "stamina a player-requested change into a form costs (0 = free)"),
    "Protean_KillBoostSeconds":    (0x94E, 90,   "Float", "how long one kill's stamina-regeneration stack lasts"),
    "Protean_KillBoostPct":        (0x94F, 15,   "Float", "stamina regeneration each stack adds, in percentage points of the normal rate"),
    "Protean_KillBoostMax":        (0x950, 0,    "Float", "most stacks at once (0 = no limit)"),
    "Protean_PerkPoints":          (0x951, 0,    "Float", "Protean perk points to spend (kept by Protean; the perk menu spends them)"),
    "Protean_ShowPerkMenu":        (0x952, 0,    "Short", "set to 1 to open the Protean perk menu (Custom Skills Framework clears it)"),
}
# Events the animal's graph was seen to refuse when sent to the player (probed live with the dev "anim" command).
BLOCKED = {
    "deer": ["attackStart_AttackLeft1", "attackStart_AttackLeft2", "attackStart_AttackRight1", "attackStart_AttackRight2",
             "attackStart_ForwardPower", "attackStart_ForwardPowerShort", "attackStart_SkeeverLungeLong",
             "attackStart_SkeeverLungeMedium", "attackStart_SkeeverLungeShort", "attackStart_StandingPower"],
    "wolf": ["attackStart_AttackLeft1", "attackStart_AttackLeft2", "attackStart_AttackRight1", "attackStart_AttackRight2"],
    # Phase 3 batch A (2026-10-20); Ice/Pit Wolf inherit the wolf's; dog, death hound, goat, cow refuse nothing
    "fox": ["attackStart_AttackLeft1", "attackStart_AttackLeft2", "attackStart_AttackRight1", "attackStart_AttackRight2"],
    "horker": ["attackStart_Attack2"],  # seen refused in play 2026-10-20
    "elk": ["attackStart_AttackLeft1", "attackStart_AttackLeft2", "attackStart_AttackRight1", "attackStart_AttackRight2",
            "attackStart_ForwardPower", "attackStart_ForwardPowerShort", "attackStart_SkeeverLungeLong",
            "attackStart_SkeeverLungeMedium", "attackStart_SkeeverLungeShort", "attackStart_StandingPower"],
    # Phase 4 step 1 (2026-10-20): horse refuses nothing; the sprint moves need a sprint, the howl is a shout
    "werewolf": ["AttackStartDualSprinting", "AttackStartHowlExplode", "AttackStartLeftSprinting", "AttackStartRightSprinting"],
    # 2026-10-21: refused every time in play and by probe; LMB falls to its straight bite (Attack1, accepted)
    "boar": ["attackStart_AttackRight1"],
}
# Melee tuning for the player in animal form (wild animals keep the vanilla values). Vanilla wolf: reach 64,
# strike cone 35 degrees, which made bites very hard to land (measured live); werewolf: 150 / 50.
MELEE = {
    "wolf": {"reach": 120, "strike_angle": 50},
    # the other hunters start from the wolf's values (not measured; adjust if one feels wrong in play)
    "fox": {"reach": 120, "strike_angle": 50},
    "dog": {"reach": 120, "strike_angle": 50},
    "death_hound": {"reach": 120, "strike_angle": 50},
    # batches B-D: the same floor for every fighter (a race with a longer reach keeps its own)
    **{c: {"reach": 120, "strike_angle": 50} for c in (
        "bear", "cave_bear", "snow_bear", "sabre_cat", "snowy_sabre_cat", "vale_sabre_cat",
        "mudcrab", "ash_hopper", "chaurus", "troll", "frost_troll", "horker", "horse", "boar")},
    # taps go straight ahead (user: they swiped left/right); their side swipes stay unused
    **{c: {"reach": 120, "strike_angle": 50, "taps": ["attackStart_Attack1", "attackStart_Attack2"]} for c in (
        "bear", "cave_bear", "snow_bear")},
    # Jump leaps far (user: "leap great distances"); its own lunges are refused for the player. ~14 m, ~3.5 m up
    "ash_hopper": {"reach": 120, "strike_angle": 50, "taps": ["attackStart_Attack1", "attackStart_Attack2"],
                   "leap": 1000, "leap_height": 250},
    # these sprint by themselves (gallop, were-form sprint); everyone else gets Protean's sprint boost
    "horse": {"reach": 120, "strike_angle": 50, "own_sprint": True},
    "werewolf": {"own_sprint": True},
    "werebear": {"own_sprint": True},
    # the same leap at half the distance and ~2 m up (user: 1000 was way too far, 250 too high)
    "hare": {"leap": 500, "leap_height": 140},
    # breathes water; on land the game itself suffocates it (measured). User decision: no shoreline wall, and it
    # may change on land and slither into the water
    "slaughterfish": {"reach": 120, "strike_angle": 50, "swims": True},
    # vanilla floats them at a fixed height over the ground; Jump takes off into real flight (Fly.cpp)
    "chaurus_hunter": {"reach": 120, "strike_angle": 50, "flies": True},
    "chaurus_hunter_fledgling": {"reach": 120, "strike_angle": 50, "flies": True},
    # RMB power attack = its poison spit (user decision), crSpider01PoisonSpit. Probed live: the stance rears it up,
    # the release event plays the spit (its BeginCastLeft is refused for the player)
    "frostbite_spider": {"reach": 120, "strike_angle": 50, "rmb_spell": {"file": "Skyrim.esm", "id": 0x0434DD},
                         "spell_events": ["spitStanceStart", "MLH_SpellRelease_event"]},
    # the mammoth race's reach is 128 (measured: hits landed only on people almost under it); its tusks reach
    # well past that, and the giant's race has 512
    "mammoth": {"reach": 400, "strike_angle": 90, "taps": ["attackStart_Attack1", "attackStart_Attack2"]},
    # the giant's graph walks and attacks only from its combat stance, which the AI puts wild giants in (measured:
    # no movement after transforming, and every attack refused, until combatIdleStart)
    # and walks properly only with the graph's iState at 1, its combat walk (measured: 0 = no walk or a split-legged one)
    "giant": {"stance": "combatIdleStart", "stance_state": 1},
}
# Protean's sprint boost (SpeedMult added while Sprint is held); everyone else gets the horse's gallop, +33% (user)
# Only four-legged animals sprint (user): none for two legs or more than four
SPRINT = {"elk": 40, "fox": 40, "hare": 40, "mammoth": 0, "horker": 0,
          **{c: 0 for c in ("troll", "frost_troll", "giant", "chicken", "frostbite_spider", "chaurus", "chaurus_hunter",
                            "chaurus_hunter_fledgling", "mudcrab", "ash_hopper", "slaughterfish")}}
ENABLED = ("wolf,deer,skeever,ice_wolf,pit_wolf,fox,dog,death_hound,elk,goat,cow,"
           "bear,cave_bear,snow_bear,sabre_cat,snowy_sabre_cat,vale_sabre_cat,"
           "hare,chicken,mudcrab,ash_hopper,chaurus,troll,frost_troll,horker,"
           "horse,werewolf,werebear,mammoth,giant,slaughterfish,chaurus_hunter,chaurus_hunter_fledgling,"
           "frostbite_spider,boar")
# Extra factions joined while transformed. The hold crime factions count PlayerWerewolfFaction as an enemy: it is how
# vanilla makes guards attack a player werewolf on sight (and citizens flee).
EXTRA_FACTIONS = {
    "werewolf": ["Skyrim.esm:091822"],  # PlayerWerewolfFaction
    "werebear": ["Skyrim.esm:091822"],
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


# The vanilla NPC record whose skin and size the player gets, where the automatic choice is wrong.
NPC_PICK = {
    "mudcrab": "EncMudcrabMedium",  # Small/Medium/Large/Giant all exist; medium is size 1.0
}


def by_race(table, c, creatures):
    """A per-animal setting; variants sharing a race (Ice Wolf, Pit Wolf) inherit their race-mate's."""
    if c["id"] in table:
        return table[c["id"]]
    for other in creatures:
        if other["race"] == c["race"] and other["id"] in table:
            return table[other["id"]]
    return type(next(iter(table.values())))()


def body(c, npcs):
    """Skin, size and model height of the animal's own vanilla NPC record (the one named like it, an Enc* record
    preferred). Variants that share a race (Ice Wolf, Pit Wolf) differ only here. Skin None = the race's own."""
    def pick(named):
        rows = []
        for x in npcs:
            j = x["json"]
            if x["race"] == c["race"] and (not named or j.get("FULL - Name") == c["name"]):
                edid = j.get("EDID - Editor ID", "")
                bound = (j.get("OBND - Object Bounds") or {}).get("Z2", 0)
                # the plain Enc record first: EncMudcrab, not EncMudcrabGiant (twice the size)
                rows.append((not edid.startswith("Enc"), "TPLT - Template" in j, len(edid), edid,
                             j.get("WNAM - Worn Armor"), j.get("NAM6 - Height", 1.0), bound))
        return sorted(rows)

    rows = pick(True) or pick(False)  # the game calls hares "Rabbit"
    if c["id"] in NPC_PICK:
        rows = [r for r in rows if r[3] == NPC_PICK[c["id"]]] or rows
    if not rows:
        return {}
    _, _, _, edid, skin, height, bound = rows[0]
    if not bound:  # some records leave the bounds empty; any record of the same race has the model's
        bound = max(((x["json"].get("OBND - Object Bounds") or {}).get("Z2", 0) for x in npcs if x["race"] == c["race"]),
                    default=0) or c.get("bound_height") or 0
    out = {"height": round(height, 3), "npc": edid}
    if skin:
        f, i = skin.split(":")
        out["skin"] = {"file": f, "id": int(i, 16)}
    if bound:
        out["camera_height"] = round(0.9 * bound * height)
    return out


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("--enable", default=ENABLED)
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

    # ---- integration markers and dials (for a larger mod built on Protean; see docs/INTEGRATION.md) ----
    def keyword(local, editor_id):
        write(os.path.join(y, "Keywords", "%s - %s_%s.yaml" % (editor_id, fid(local), PLUGIN)),
              "FormKey: %s:%s\nEditorID: %s\n" % (fid(local), PLUGIN, editor_id))

    keyword(KW_TRANSFORMED, "Protean_Transformed")
    for name, (local, value, kind, _) in GLOBALS.items():
        write(os.path.join(y, "Globals", "%s - %s_%s.yaml" % (name, fid(local), PLUGIN)),
              "MutagenObjectType: Global%s\nFormKey: %s:%s\nEditorID: %s\nData: %s\n" % (kind, fid(local), PLUGIN, name, value))

    # (perk-reach test, 2026-10-21: a plain +25% ModAttackDamage perk raised an elk's kick 5 -> 6.25, so
    # ordinary damage perks reach the animal forms; the test perk is gone)

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
    power(EVOLUTION, "Protean_Evolution", "Evolution",
          "Look into what you are becoming: spend Protean perk points, earned by feeding.")

    def perk(pid, local, title, desc, nxt=None):
        write(os.path.join(y, "Perks", "Protean_Perk_%s - %s_%s.yaml" % (pid, fid(local), PLUGIN)),
              "FormKey: %s:%s\nEditorID: Protean_Perk_%s\n" % (fid(local), PLUGIN, pid)
              + name_block(title)
              + "Description:\n  TargetLanguage: English\n  Value: %s\n" % yq(desc)
              + "NumRanks: 1\nPlayable: True\n"
              + ("NextPerk: %s:%s\n" % (fid(nxt), PLUGIN) if nxt else ""))

    for pid, (local, title, desc, _, _, _) in PERKS.items():
        perk(pid, local, title, desc, RANK2[pid][0] if pid in RANK2 else None)
    for pid, (local, desc) in RANK2.items():
        perk(pid + "_2", local, PERKS[pid][1], desc)

    # The tree for Custom Skills Framework (Exit-9B, MIT): SKSE/Plugins/CustomSkills/Protean.json. No skill level, so it
    # looks like the vanilla beast trees; the points are Protean_PerkPoints, which Protean keeps up to date.
    def ref(local):
        return "%s|%X" % (PLUGIN, local)

    write(os.path.join(ROOT, "data", "SKSE", "Plugins", "CustomSkills", "Protean.json"), json.dumps({
        "$schema": "https://raw.githubusercontent.com/Exit-9B/CustomSkills/main/docs/schema/CustomSkill.json",
        "version": 1,
        "skydome": {"model": "DLC01/Interface/INTVampirePerkSkydome.nif", "cameraRightPoint": 2},
        "showMenu": ref(GLOBALS["Protean_ShowPerkMenu"][0]),
        "perkPoints": ref(GLOBALS["Protean_PerkPoints"][0]),
        "skills": [{
            "id": "protean",
            "name": "Protean",
            "description": "New perks come with feeding. Each perk point takes one more feeding than the last.",
            "nodes": [{"id": pid, "perk": ref(local), "x": round(x * TREE_SCALE_X, 2), "y": round(yy * TREE_SCALE_Y, 2),
                       **({"links": links} if links else {})}
                      for pid, (local, _, _, x, yy, links) in PERKS.items()],
        }],
    }, indent=2))

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

    npcs = json.load(open(os.path.join(ROOT, "research", "raw", "npcs.json"), encoding="utf-8"))
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

        # The marker the player carries while in this form: a hidden ability whose effect has the keywords
        # Protean_Transformed and Protean_Form_<id> (conditions: HasMagicEffectKeyword).
        kw, fx, ab = FORM_KEYWORD + index, FORM_EFFECT + index, FORM_ABILITY + index
        keyword(kw, "Protean_Form_%s" % c["id"])
        write(os.path.join(y, "MagicEffects", "Protean_FormEffect_%s - %s_%s.yaml" % (c["id"], fid(fx), PLUGIN)),
              "FormKey: %s:%s\nEditorID: Protean_FormEffect_%s\n" % (fid(fx), PLUGIN, c["id"])
              + name_block("%s Form" % c["name"])
              + "Flags:\n- NoDuration\n- NoMagnitude\n- NoArea\n- HideInUI\n- Painless\n- NoHitEffect\nBaseCost: 0\n"
              "Archetype:\n  MutagenObjectType: MagicEffectArchetype\n  Type: Script\n"
              "SkillUsageMultiplier: 1\nDualCastScale: 1\n"
              "Description:\n  TargetLanguage: English\n  Value: ''\n"
              "Keywords:\n- %s:%s\n- %s:%s\n" % (fid(KW_TRANSFORMED), PLUGIN, fid(kw), PLUGIN))
        write(os.path.join(y, "Spells", "Protean_FormAbility_%s - %s_%s.yaml" % (c["id"], fid(ab), PLUGIN)),
              "FormKey: %s:%s\nEditorID: Protean_FormAbility_%s\n" % (fid(ab), PLUGIN, c["id"])
              + name_block("%s Form" % c["name"])
              + "EquipmentType: 013F44:Skyrim.esm\nDescription:\n  TargetLanguage: English\n  Value: ''\n"
              "Flags:\n- ManualCostCalc\nType: Ability\nChargeTime: 0.5\n"
              "Effects:\n- BaseEffect: %s:%s\n  Data: {}\n" % (fid(fx), PLUGIN))

        rf, rid = c["race_form"].split(":")
        runtime.append({
            "id": c["id"], "name": c["name"], "index": index,
            "race": {"file": rf, "id": int(rid, 16)},
            "heart": heart, "power": pw, "ability": ab,
            "size": c["size_class"],
            "attacks": [{"event": a["event"], "flags": a["flags"], "spell": a["spell"]} for a in c.get("attacks", []) if a["event"] not in by_race(BLOCKED, c, creatures)],
            "camera_height": round(0.9 * c["bound_height"]) if c.get("bound_height") else 60,
            "factions": [{"file": f.split(":")[0], "id": int(f.split(":")[1], 16)} for f in c["factions"] + EXTRA_FACTIONS.get(c["id"], [])],
            "ladder": c["health_ladder"],
            # vanilla NPC records of this creature: dead ones get a heart when looted
            "race_names": [c["race"]],
            **body(c, npcs),
            **by_race(MELEE, c, creatures),
            **({"sprint": SPRINT[c["id"]]} if c["id"] in SPRINT else {}),
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
                           "evolution": EVOLUTION,
                           "perks": {**{pid: v[0] for pid, v in PERKS.items()}, **{pid + "_2": v[0] for pid, v in RANK2.items()}},
                           "perksBuilt": sorted(BUILT),
                           "globals": {name: v[0] for name, v in GLOBALS.items()},
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
