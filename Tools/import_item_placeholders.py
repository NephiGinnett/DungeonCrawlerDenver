"""Unreal side of the item placeholders (2026-11-14). Editor open, MCP up:

    python Tools/import_item_placeholders.py

1. Imports SourceArt/Placeholders/fbx/<Item>.fbx (Tools/build_item_placeholders.py) as
   /Game/DCC/Equipment/Placeholders/SM_<Item>, with its flat-colour materials, unless it is already there.
2. Points DT_DCCEquipment at them. Hand and head items get Mesh (what is held or worn). Body clothing and
   trinkets get GroundMesh only (what lies on the floor; worn clothing is a material change, never geometry).
   Rows are read first and written back whole, with only those fields changed.
3. Saves the table and the new meshes.
"""
import json
import sys
from pathlib import Path

sys.path.insert(0, str(Path(__file__).resolve().parent))
from mcp_call import McpSession, unwrap  # noqa: E402
import import_loot_content as L  # noqa: E402  (rv, ref, get_rows, set_rows helpers)

ROOT = Path(__file__).resolve().parent.parent
FBX = ROOT / "SourceArt" / "Placeholders" / "fbx"
FOLDER = "/Game/DCC/Equipment/Placeholders"
SM = "editor_toolset.toolsets.static_mesh.StaticMeshTools"
AT = "editor_toolset.toolsets.asset.AssetTools"

# Row -> (model, field). Rows that reuse a model or an installed mesh are listed explicitly.
REUSE = {
    "HighVisVest": ("HighVisibilitySafetyVest", "groundMesh"),
    "SecuritySidearm": ("/Game/UnrealBucket/Library/Weapon/Revolver/SM_Revolver.SM_Revolver", "mesh"),
}
WORN = {"MainHand", "OffHand", "Head"}


def main():
    s = McpSession()
    have = set(L.rv(s.call_tool(AT, "find_assets", {"folder_path": FOLDER, "name": ""})) or [])
    models = sorted(p.stem for p in FBX.glob("*.fbx"))
    for m in models:
        if any(h.endswith("/SM_" + m) or h.endswith("SM_" + m + ".SM_" + m) for h in have):
            continue
        r = L.rv(s.call_tool(SM, "import_file", {"folder_path": FOLDER, "asset_name": "SM_" + m, "source_file": str(FBX / (m + ".fbx"))}))
        print("import", m, "->", str(r)[:120])

    names = L.list_rows(s, L.EQUIPMENT)
    rows = L.get_rows(s, L.EQUIPMENT, sorted(names))
    payload = {}
    for name, row in rows.items():
        empty = lambda v: v in (None, "", "None")
        if name in REUSE:
            target, field = REUSE[name]
            path = target if target.startswith("/Game/") else f"{FOLDER}/SM_{target}.SM_{target}"
        elif name in models:
            field = "mesh" if row.get("slot") in WORN else "groundMesh"
            path = f"{FOLDER}/SM_{name}.SM_{name}"
        else:
            continue
        # Only fill gaps. A row that already has a real mesh keeps it, except the two that were
        # borrowing an unrelated one (a hand drill and a lamp standing in for guns).
        borrowed = name in ("NailGunPro", "FlareGunEmergency")
        if not empty(row.get(field)) and not borrowed:
            continue
        row = dict(row)
        row[field] = path
        payload[name] = row
    print("rows to update:", len(payload), sorted(payload))
    print("set_rows:", L.set_rows(s, L.EQUIPMENT, payload))

    back = L.get_rows(s, L.EQUIPMENT, sorted(payload))
    bad = [n for n in payload if (back.get(n, {}).get("mesh") or "None") in ("None", "") and (back.get(n, {}).get("groundMesh") or "None") in ("None", "")]
    print("read back, still empty:", bad or "none")
    saved = L.rv(s.call_tool(AT, "save_assets", {"asset_paths": [L.EQUIPMENT] + [f"{FOLDER}/SM_{m}" for m in models]}))
    print("save:", saved)


if __name__ == "__main__":
    main()
