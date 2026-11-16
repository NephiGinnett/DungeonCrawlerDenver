"""Colours for the item placeholders in Unreal (2026-11-14). Editor open, MCP up:

    python Tools/colour_item_placeholders.py

The MCP FBX import brings meshes without materials, so the colours are rebuilt here from the palette in
Tools/build_item_placeholders.py (the mat(...) lines):
  1. /Game/DCC/Equipment/Placeholders/Materials/M_DCCPlaceholder: Colour (vector) -> Base Colour, Metallic, Roughness,
     Colour * Glow -> Emissive.
  2. One instance per palette entry, MI_<Name>.
  3. Every SM_ in the folder gets its slots set by slot name (the FBX keeps "M_<Name>" as the slot name).
Re-runnable: existing assets are reused.
"""
import json
import re
import sys
from pathlib import Path

sys.path.insert(0, str(Path(__file__).resolve().parent))
from mcp_call import McpSession  # noqa: E402
import import_loot_content as L  # noqa: E402

ROOT = Path(__file__).resolve().parent
FOLDER = "/Game/DCC/Equipment/Placeholders"
MATS = FOLDER + "/Materials"
MT = "editor_toolset.toolsets.material.MaterialTools"
MI = "editor_toolset.toolsets.material_instance.MaterialInstanceTools"
OB = "editor_toolset.toolsets.object.ObjectTools"
SM = "editor_toolset.toolsets.static_mesh.StaticMeshTools"
AT = "editor_toolset.toolsets.asset.AssetTools"


def palette():
    src = (ROOT / "build_item_placeholders.py").read_text(encoding="utf-8")
    out = {}
    for m in re.finditer(r"mat\('(\w+)', \(([\d.]+), ([\d.]+), ([\d.]+)\)((?:, \w+=[\d.]+)*)\)", src):
        name = m.group(1)
        kw = dict(re.findall(r"(\w+)=([\d.]+)", m.group(5) or ""))
        out[name] = {"rgb": [float(m.group(i)) for i in (2, 3, 4)], "metal": float(kw.get("metal", 0.0)),
                     "rough": float(kw.get("rough", 0.6)), "emit": float(kw.get("emit", 0.0))}
    return out


def ref(p):
    return {"refPath": p if "." in p.rsplit("/", 1)[1] else f"{p}.{p.rsplit('/', 1)[1]}"}


def as_ref(v):
    return v if isinstance(v, dict) and "refPath" in v else {"refPath": v}


def main():
    s = McpSession()
    call = lambda ts, tool, args: L.rv(s.call_tool(ts, tool, args))
    master = f"{MATS}/M_DCCPlaceholder"
    if not call(AT, "exists", {"path": master}):
        mat = call(MT, "create_material", {"folder_path": MATS, "asset_name": "M_DCCPlaceholder"})
        mref = ref(master)

        def node(cls, x, y, props):
            e = as_ref(call(MT, "add_expression", {"material_or_function": mref, "expression_class": {"refPath": f"/Script/Engine.{cls}"}, "x": x, "y": y}))
            call(OB, "set_properties", {"instance": e, "values": json.dumps(props)})
            return e
        colour = node("MaterialExpressionVectorParameter", -600, -100, {"ParameterName": "Colour", "DefaultValue": {"R": 0.8, "G": 0.8, "B": 0.8, "A": 1}})
        metal = node("MaterialExpressionScalarParameter", -600, 100, {"ParameterName": "Metallic", "DefaultValue": 0.0})
        rough = node("MaterialExpressionScalarParameter", -600, 200, {"ParameterName": "Roughness", "DefaultValue": 0.6})
        glow = node("MaterialExpressionScalarParameter", -600, 320, {"ParameterName": "Glow", "DefaultValue": 0.0})
        mul = as_ref(call(MT, "add_expression", {"material_or_function": mref, "expression_class": {"refPath": "/Script/Engine.MaterialExpressionMultiply"}, "x": -300, "y": 320}))
        print("base", call(MT, "connect_to_output", {"expression": colour, "output_name": "", "material_property": "MP_BaseColor"}))
        print("metal", call(MT, "connect_to_output", {"expression": metal, "output_name": "", "material_property": "MP_Metallic"}))
        print("rough", call(MT, "connect_to_output", {"expression": rough, "output_name": "", "material_property": "MP_Roughness"}))
        print("mulA", call(MT, "connect_expressions", {"from_expression": colour, "from_output_name": "", "to_expression": mul, "to_input_name": "A"}))
        print("mulB", call(MT, "connect_expressions", {"from_expression": glow, "from_output_name": "", "to_expression": mul, "to_input_name": "B"}))
        print("emit", call(MT, "connect_to_output", {"expression": mul, "output_name": "", "material_property": "MP_EmissiveColor"}))
        print("recompile", call(MT, "recompile", {"material_or_function": mref}))

    pal = palette()
    for name, p in pal.items():
        inst = f"{MATS}/MI_{name}"
        if not call(AT, "exists", {"path": inst}):
            call(MI, "create", {"folder_path": MATS, "asset_name": f"MI_{name}", "parent": ref(master)})
        r, g, b = p["rgb"]
        call(MI, "set_vector_parameter", {"instance": ref(inst), "name": "Colour", "value": {"r": r, "g": g, "b": b, "a": 1.0}})
        call(MI, "set_scalar_parameter", {"instance": ref(inst), "name": "Metallic", "value": p["metal"]})
        call(MI, "set_scalar_parameter", {"instance": ref(inst), "name": "Roughness", "value": p["rough"]})
        call(MI, "set_scalar_parameter", {"instance": ref(inst), "name": "Glow", "value": p["emit"]})
    print("instances:", len(pal))

    meshes = [m for m in (call(AT, "find_assets", {"folder_path": FOLDER, "name": "SM_"}) or []) if "/Materials/" not in m]
    done = 0
    for m in meshes:
        slots = call(SM, "get_material_slots", {"mesh": ref(m)}) or []
        for slot in slots:
            key = slot[2:] if slot.startswith("M_") else slot
            key = key.split(".")[0]
            if key in pal:
                call(SM, "set_material", {"mesh": ref(m), "slot_name": slot, "material": ref(f"{MATS}/MI_{key}")})
                done += 1
            else:
                print("  no palette entry for slot", slot, "on", m)
    print("slots set:", done, "on", len(meshes), "meshes")
    assets = meshes + [master] + [f"{MATS}/MI_{n}" for n in pal]
    print("save:", call(AT, "save_assets", {"asset_paths": assets}))


if __name__ == "__main__":
    main()
