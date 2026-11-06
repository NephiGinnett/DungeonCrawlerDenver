"""Create or update the two onboarding DataTables in the running editor, from the authored JSON in Docs/.

    python Tools/import_onboarding_tables.py

DataTableTools.import_file only reads CSV, and these rows hold arrays of structs, so it's create -> add_rows ->
set_rows (CLAUDE.md, "MCP level-editing gotchas"). Re-runnable: existing tables get missing rows added and every
row rewritten from the JSON.
"""
import json
import sys
from pathlib import Path

sys.path.insert(0, str(Path(__file__).resolve().parent))
from mcp_call import McpSession, unwrap  # noqa: E402

ROOT = Path(__file__).resolve().parent.parent


def rv(result):
    """unwrap() gives the tool's JSON text; this gives its returnValue (a false-looking string is still truthy)."""
    v = unwrap(result)
    try:
        v = json.loads(v) if isinstance(v, str) else v
    except ValueError:
        return v
    return v.get("returnValue", v) if isinstance(v, dict) else v

TABLES = [
    ("/Game/DCC/UI", "DT_DCCSystemWindows", "/Script/DungeonCrawlerDenver.DCCSystemWindowDef", "Docs/DT_DCCSystemWindows.json"),
    ("/Game/DCC/Onboarding", "DT_DCCRandomEncounters", "/Script/DungeonCrawlerDenver.DCCRandomEncounterRow", "Docs/DT_DCCRandomEncounters.json"),
]


def main():
    s = McpSession()
    for folder, name, struct, src in TABLES:
        path = f"{folder}/{name}.{name}"
        rows = json.loads((ROOT / src).read_text(encoding="utf-8"))
        exists = rv(s.call_tool("AssetTools", "exists", {"path": f"{folder}/{name}"}))
        if not exists:
            rv(s.call_tool("AssetTools", "create_folder", {"path": folder}))
            print("create", name, rv(s.call_tool("DataTableTools", "create",
                  {"folder_path": folder, "asset_name": name, "schema": {"refPath": struct}})))
        have = set(rv(s.call_tool("DataTableTools", "list_rows", {"data_table": {"refPath": path}})) or [])
        missing = [r["Name"] for r in rows if r["Name"] not in have]
        if missing:
            rv(s.call_tool("DataTableTools", "add_rows", {"data_table": {"refPath": path}, "row_names": missing}))
        payload = {r["Name"]: {k: v for k, v in r.items() if k != "Name"} for r in rows}
        result = rv(s.call_tool("DataTableTools", "set_rows", {"data_table": {"refPath": path}, "values": json.dumps(payload)}))
        print(name, f"{len(rows)} rows ({len(missing)} new):", result)
        rv(s.call_tool("AssetTools", "save_assets", {"asset_paths": [f"{folder}/{name}"]}))


if __name__ == "__main__":
    main()
