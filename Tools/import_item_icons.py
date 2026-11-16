"""Bring the item pictures into Unreal (2026-11-14). Editor open, MCP up:

    python Tools/import_item_icons.py

Imports SourceArt/UI/Icons/T_Icon_<Id>.png (Tools/render_item_icons.py) as /Game/DCC/UI/Icons/T_Icon_<Id>, set up as
UI textures (no mips, the UI texture group, kept sharp), and saves them. An icon that already exists is re-imported
over by deleting it first, so re-rendering and re-running this updates them. The menus load them by name
(DCCMenu::IconBrush), so a new icon needs no code.
"""
import json
import sys
from pathlib import Path

sys.path.insert(0, str(Path(__file__).resolve().parent))
from mcp_call import McpSession  # noqa: E402
import import_loot_content as L  # noqa: E402

ROOT = Path(__file__).resolve().parent.parent
SRC = ROOT / "SourceArt" / "UI" / "Icons"
FOLDER = "/Game/DCC/UI/Icons"
TX = "editor_toolset.toolsets.texture.TextureTools"
OB = "editor_toolset.toolsets.object.ObjectTools"
AT = "editor_toolset.toolsets.asset.AssetTools"


def main():
    s = McpSession()
    call = lambda ts, tool, args: L.rv(s.call_tool(ts, tool, args))
    done = []
    for png in sorted(SRC.glob("T_Icon_*.png")):
        name = png.stem
        path = f"{FOLDER}/{name}"
        if call(AT, "exists", {"path": path}):
            call(AT, "delete", {"path": path})
        call(TX, "import_file", {"folder_path": FOLDER, "asset_name": name, "source_file": str(png)})
        ok = call(OB, "set_properties", {"instance": {"refPath": f"{path}.{name}"}, "values": json.dumps({
            "LODGroup": "TEXTUREGROUP_UI", "MipGenSettings": "TMGS_NoMipmaps", "CompressionSettings": "TC_EditorIcon", "SRGB": True})})
        done.append(path)
        print(name, "ok" if ok else ok)
    print("save:", call(AT, "save_assets", {"asset_paths": done}))


if __name__ == "__main__":
    main()
