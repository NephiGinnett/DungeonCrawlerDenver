"""Unreal side of the prop-material fix (2026-11-13). Run with the editor open (MCP on port 8000):

    python Tools/fix_prop_materials_ue.py [Name ...]

For each prop (SourceArt/PropFix/<Name>/, made by Tools/fix_prop_materials.py):
  1. imports <Name>.fbx as /Game/DCC/Equipment/Props/<Name>/SM_<Name>_Fixed, which also creates M_<Name>
     (the FBX texture links do not survive, so the material arrives untextured), unless already there;
  2. imports T_<Name>_BaseColor as a Texture2D (slow: ~2 min per 4K JPEG, Interchange compresses it);
  3. adds a TextureSample with it to M_<Name>, wires RGB to Base Color, recompiles;
  4. points the ORIGINAL SM_<Name> (what the game references) at M_<Name>, and saves.
"""
import json
import sys

from mcp_call import McpSession, unwrap

ROOT = r"C:\Users\kmett\Documents\Unreal Projects\DungeonCrawlerDenver\SourceArt\PropFix"
SM = "editor_toolset.toolsets.static_mesh.StaticMeshTools"
TX = "editor_toolset.toolsets.texture.TextureTools"
MT = "editor_toolset.toolsets.material.MaterialTools"
OB = "editor_toolset.toolsets.object.ObjectTools"
AT = "editor_toolset.toolsets.asset.AssetTools"
NAMES = ["DunePopcorn", "PokemonMasterTrainer", "MinionEyes", "ScottLacrosse", "WaterBottle"]


def val(r):
    u = unwrap(r)
    if isinstance(u, str):
        try:
            u = json.loads(u)
        except Exception:
            return u
    return u.get("returnValue", u) if isinstance(u, dict) else u


def ref(path):
    return {"refPath": path}


def main(names):
    s = McpSession()
    for n in names:
        folder = f"/Game/DCC/Equipment/Props/{n}"
        have = val(s.call_tool(AT, "find_assets", {"folder_path": folder, "name": ""})) or []
        if not any(p.endswith(f"/M_{n}") for p in have):
            val(s.call_tool(SM, "import_file", {"folder_path": folder, "asset_name": f"SM_{n}_Fixed",
                "source_file": f"{ROOT}\\{n}\\{n}.fbx", "import_materials": True, "import_textures": False}))
        tex_path = f"{folder}/T_{n}_BaseColor"
        if not any(p == tex_path for p in have):
            val(s.call_tool(TX, "import_file", {"folder_path": folder, "asset_name": f"T_{n}_BaseColor",
                "source_file": f"{ROOT}\\{n}\\T_{n}_BaseColor.jpg"}))
        mat = ref(f"{folder}/M_{n}.M_{n}")
        sample = val(s.call_tool(MT, "add_expression", {"material_or_function": mat,
            "expression_class": ref("/Script/Engine.MaterialExpressionTextureSample"), "x": -400, "y": 0}))
        sample_ref = sample if isinstance(sample, dict) else ref(sample)
        set_ok = val(s.call_tool(OB, "set_properties", {"instance": sample_ref,
            "values": json.dumps({"Texture": f"{tex_path}.T_{n}_BaseColor"})}))
        wired = val(s.call_tool(MT, "connect_to_output", {"expression": sample_ref, "output_name": "RGB", "material_property": "MP_BaseColor"}))
        val(s.call_tool(MT, "recompile", {"material_or_function": mat}))
        mesh = ref(f"/Game/DCC/Equipment/Props/SM_{n}.SM_{n}")
        slots = val(s.call_tool(SM, "get_material_slots", {"mesh": mesh})) or []
        assigned = [val(s.call_tool(SM, "set_material", {"mesh": mesh, "slot_name": slot, "material": mat})) for slot in slots]
        val(s.call_tool(AT, "save_assets", {"asset_paths": [folder, f"/Game/DCC/Equipment/Props/SM_{n}"]}))
        print(f"{n}: texture set {set_ok}, wired {wired}, slots {slots} -> {assigned}", flush=True)


if __name__ == "__main__":
    main(sys.argv[1:] or NAMES)
