"""Push the loot and skill content from Tools/gen_loot_content.py into the running editor, then save.

    python Tools/gen_loot_content.py      # (re)writes the JSON first
    python Tools/import_loot_content.py

What it does, in order:
  1. DT_DCCSkills: adds the books' skills the gear uses (Club, Axe, Dagger, Longsword, Warhammer, Polearm,
     Improvised Weapons, Throwing, Explosives Handling, Goblin Explosives, Incendiary Device Handling,
     First Aid). Only those rows are written; existing skills are left alone.
  2. DT_DCCEquipment: adds the batch-1 gear and the skill gear and writes them in full.
  3. DT_DCCEquipment: sets Rarity and WeaponSkillId on gear that already existed. Those rows are READ first
     and written back whole with only those fields changed, because set_rows may replace a row rather than
     merge into it, and the existing rows hold data (meshes, grips) the CSV never had.
  4. DT_DCCConsumables, DT_DCCBoons, DT_DCCLootBoxTables: missing rows added, every row rewritten from the
     JSON (it holds every row of those tables).
  5. Reads a few rows back to prove the writes landed, then saves everything.

Re-runnable. Needs the editor open with the MCP server up (Tools/mcp_call.py), running a build that has
the 2026-11-13 skill fields (WeaponSkillId, SplashRadius...), or set_rows will reject the unknown fields.
"""
import json
import sys
from pathlib import Path

sys.path.insert(0, str(Path(__file__).resolve().parent))
from mcp_call import McpSession, unwrap  # noqa: E402

ROOT = Path(__file__).resolve().parent.parent
DOCS = ROOT / "Docs"

EQUIPMENT = "/Game/DCC/Equipment/DT_DCCEquipment"
SKILLS = "/Game/DCC/Skills/DT_DCCSkills"
CONSUMABLES = "/Game/DCC/Items/DT_DCCConsumables"
BOONS = "/Game/DCC/Items/DT_DCCBoons"
BOXES = "/Game/DCC/Items/DT_DCCLootBoxTables"


def rv(result):
    v = unwrap(result)
    try:
        v = json.loads(v) if isinstance(v, str) else v
    except ValueError:
        return v
    v = v.get("returnValue", v) if isinstance(v, dict) else v
    # get_rows returns its JSON as a string inside returnValue: unwrap that layer too (2026-11-14).
    if isinstance(v, str) and v[:1] in "{[":
        try:
            return json.loads(v)
        except ValueError:
            pass
    return v


def ref(package):
    return {"refPath": f"{package}.{package.rsplit('/', 1)[1]}"}


def list_rows(s, package):
    return set(rv(s.call_tool("DataTableTools", "list_rows", {"data_table": ref(package)})) or [])


def add_missing(s, package, names):
    have = list_rows(s, package)
    missing = [n for n in names if n not in have]
    if missing:
        rv(s.call_tool("DataTableTools", "add_rows", {"data_table": ref(package), "row_names": missing}))
    return missing


def set_rows(s, package, payload):
    return rv(s.call_tool("DataTableTools", "set_rows", {"data_table": ref(package), "values": json.dumps(payload)}))


def get_rows(s, package, names):
    rows = rv(s.call_tool("DataTableTools", "get_rows", {"data_table": ref(package), "row_names": list(names)}))
    if isinstance(rows, list):  # some versions return [{name, value}]
        rows = {r.get("name") or r.get("rowName"): r.get("value", r) for r in rows}
    return rows or {}


def find_key(row, name):
    """get_rows returns camelCase at the top level; accept either case."""
    for k in row:
        if k.lower() == name.lower():
            return k
    return name


def write_whole(s, package, rows, label):
    added = add_missing(s, package, [r["Name"] for r in rows])
    result = set_rows(s, package, {r["Name"]: {k: v for k, v in r.items() if k != "Name"} for r in rows})
    print(f"{label}: {len(rows)} rows written ({len(added)} added):", result)


def load(name):
    return json.loads((DOCS / name).read_text(encoding="utf-8"))


def main():
    s = McpSession()

    # 1. Skills the gear needs (only these rows).
    write_whole(s, SKILLS, load("DT_DCCSkills_BookSkills.json"), "DT_DCCSkills (book skills)")

    # 2. New gear.
    write_whole(s, EQUIPMENT, load("DT_DCCEquipment_LootBatch1.json"), "DT_DCCEquipment (batch 1)")
    write_whole(s, EQUIPMENT, load("DT_DCCEquipment_SkillGear.json"), "DT_DCCEquipment (skill gear)")

    # 3. Existing gear: read, change only the patched fields, write back whole.
    patch = load("DT_DCCEquipment_Patch.json")
    present = list_rows(s, EQUIPMENT)
    targets = [n for n in patch if n in present]
    current = get_rows(s, EQUIPMENT, targets)
    if not current:
        print("  could not read existing rows back; patch NOT applied (nothing overwritten).")
    else:
        payload = {}
        for name in targets:
            row = dict(current.get(name) or {})
            if not row:
                continue
            for field, value in patch[name].items():
                row[find_key(row, field)] = value
            payload[name] = row
        print(f"  rarity / weapon skill set on {len(payload)} existing rows:", set_rows(s, EQUIPMENT, payload))
    skipped = sorted(set(patch) - set(targets))
    if skipped:
        print("  not in the table, skipped:", ", ".join(skipped))

    # 4. Tables whose JSON holds every row.
    write_whole(s, CONSUMABLES, load("DT_DCCConsumables.json"), "DT_DCCConsumables")
    write_whole(s, BOONS, load("DT_DCCBoons.json"), "DT_DCCBoons")
    write_whole(s, BOXES, load("DT_DCCLootBoxTables.json"), "DT_DCCLootBoxTables")

    # 5. Prove it.
    def check(package, name, field, want):
        row = get_rows(s, package, [name]).get(name) or {}
        got = row.get(find_key(row, field))
        print(f"  check {name}.{field} = {got}", "OK" if got == want else f"MISMATCH (wanted {want})")

    check(EQUIPMENT, "EndOfQuarter", "Rarity", "Celestial")
    check(EQUIPMENT, "FireAxe", "WeaponSkillId", "Axe")
    check(EQUIPMENT, "GoblinBombProbablyStable", "HandlingSkillId", "GoblinExplosives")
    check(SKILLS, "ExplosivesHandling", "CastingAbility", "Intelligence")
    check(CONSUMABLES, "FirstAidKitCompliant", "LinkedSkillId", "FirstAid")

    print("save:", rv(s.call_tool("AssetTools", "save_assets",
                                  {"asset_paths": [EQUIPMENT, SKILLS, CONSUMABLES, BOONS, BOXES]})))


if __name__ == "__main__":
    main()
