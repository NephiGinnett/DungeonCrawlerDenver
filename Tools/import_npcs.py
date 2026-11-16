"""Unreal side of NPC Forge (2026-11-14). Editor open, MCP up:

    python Tools/import_npcs.py [SourceArt/NPCs/fbx]

For the NPCs Tools/npc_forge.py exported (FBX + npcs.json):
  1. Imports each as /Game/DCC/Characters/NPCs/<Name>/SK_<Name>. The first one's skeleton is the shared one: every
     other NPC is imported onto it, so one set of retargeted animations drives them all.
  2. Textures: the FBX import keeps no texture links, so each slot's diffuse (from npcs.json) is shrunk to 1024 px
     (plenty for a background NPC, and Interchange imports a 4K PNG very slowly) and imported once to
     /Game/DCC/Characters/NPCs/Textures.
  3. Materials: one master, M_NPCPart (texture colour, alpha as an opacity mask for hair, lashes and brows, two-sided),
     and one instance per texture. Every slot is pointed at its instance.
Skips anything already imported, so it can be re-run after a partial run.
"""
import json
import os
import re
import sys
from pathlib import Path

from PIL import Image

sys.path.insert(0, str(Path(__file__).resolve().parent))
from mcp_call import McpSession  # noqa: E402
import import_loot_content as L  # noqa: E402

ROOT = Path(__file__).resolve().parent.parent
SRC = Path(sys.argv[1]) if len(sys.argv) > 1 else ROOT / "SourceArt" / "NPCs" / "fbx"
SMALL = ROOT / "SourceArt" / "NPCs" / "tex1k"
BASE = "/Game/DCC/Characters/NPCs"
TEX = BASE + "/Textures"
MATS = BASE + "/Materials"

SK = "editor_toolset.toolsets.skeletal_mesh.SkeletalMeshTools"
TX = "editor_toolset.toolsets.texture.TextureTools"
MT = "editor_toolset.toolsets.material.MaterialTools"
MI = "editor_toolset.toolsets.material_instance.MaterialInstanceTools"
OB = "editor_toolset.toolsets.object.ObjectTools"
AT = "editor_toolset.toolsets.asset.AssetTools"


def ref(p):
    leaf = p.rsplit("/", 1)[1]
    return {"refPath": p if "." in leaf else f"{p}.{leaf}"}


def as_ref(v):
    return v if isinstance(v, dict) and "refPath" in v else {"refPath": v}


def safe(name):
    return re.sub(r"[^A-Za-z0-9_]", "_", name)


def main():
    s = McpSession()
    call = lambda ts, tool, args: L.rv(s.call_tool(ts, tool, args))
    exists = lambda p: bool(call(AT, "exists", {"path": p}))
    people = json.loads((SRC / "npcs.json").read_text())

    # ---- master material ---------------------------------------------------------------------------------
    master = f"{MATS}/M_NPCPart"
    if not exists(master):
        call(MT, "create_material", {"folder_path": MATS, "asset_name": "M_NPCPart"})
        mref = ref(master)
        print("blend", call(OB, "set_properties", {"instance": mref, "values": json.dumps({"BlendMode": "BLEND_Masked", "TwoSided": True})}))
        tex = as_ref(call(MT, "add_expression", {"material_or_function": mref, "expression_class": {"refPath": "/Script/Engine.MaterialExpressionTextureSampleParameter2D"}, "x": -500, "y": 0}))
        print("tex", call(OB, "set_properties", {"instance": tex, "values": json.dumps({"ParameterName": "BaseColor", "Texture": "/Engine/EngineResources/DefaultTexture.DefaultTexture"})}))
        rough = as_ref(call(MT, "add_expression", {"material_or_function": mref, "expression_class": {"refPath": "/Script/Engine.MaterialExpressionScalarParameter"}, "x": -500, "y": 300}))
        call(OB, "set_properties", {"instance": rough, "values": json.dumps({"ParameterName": "Roughness", "DefaultValue": 0.72})})
        print("base", call(MT, "connect_to_output", {"expression": tex, "output_name": "RGB", "material_property": "MP_BaseColor"}))
        print("mask", call(MT, "connect_to_output", {"expression": tex, "output_name": "A", "material_property": "MP_OpacityMask"}))
        print("rough", call(MT, "connect_to_output", {"expression": rough, "output_name": "", "material_property": "MP_Roughness"}))
        print("recompile", call(MT, "recompile", {"material_or_function": mref}))
        call(AT, "save_assets", {"asset_paths": [master]})

    # ---- textures and instances -------------------------------------------------------------------------
    SMALL.mkdir(parents=True, exist_ok=True)
    instance_for = {}
    for person in people:
        for slot, path in (person.get("materials") or {}).items():
            if not path or path in instance_for or not os.path.exists(path):
                continue
            stem = safe(Path(path).stem)
            small = SMALL / (stem + ".png")
            if not small.exists():
                im = Image.open(path)
                im.thumbnail((1024, 1024))
                im.save(small)
            tpath = f"{TEX}/T_{stem}"
            if not exists(tpath):
                call(TX, "import_file", {"folder_path": TEX, "asset_name": f"T_{stem}", "source_file": str(small)})
            ipath = f"{MATS}/MI_{stem}"
            if not exists(ipath):
                call(MI, "create", {"folder_path": MATS, "asset_name": f"MI_{stem}", "parent": ref(master)})
                call(MI, "set_texture_parameter", {"instance": ref(ipath), "name": "BaseColor", "value": ref(tpath)})
            instance_for[path] = ipath
    print("textures/instances:", len(instance_for))

    # ---- meshes -----------------------------------------------------------------------------------------
    saved = [master] + list(set(instance_for.values())) + [f"{TEX}/T_{safe(Path(p).stem)}" for p in instance_for]
    skel = None  # the first NPC's skeleton; every other NPC is imported onto it
    for person in people:
        name = f"NPC_{person['role']}_{person['seed']}"
        folder = f"{BASE}/{name}"
        mesh = f"{folder}/SK_{name}"
        if not exists(mesh):
            args = {"folder_path": folder, "asset_name": f"SK_{name}", "source_file": str(SRC / (name + ".fbx")),
                    "import_materials": False, "import_textures": False, "import_animations": False, "create_physics_asset": True}
            if skel:
                args["skeleton"] = {"refPath": skel}
            r = call(SK, "import_file", args)
            print("import", name, "->", str(r)[:100])
        if not skel:
            got = call(SK, "get_skeleton", {"mesh": ref(mesh)})
            skel = got["refPath"] if isinstance(got, dict) else str(got)
            print("shared skeleton:", skel)
        for slot in call(SK, "get_material_slots", {"mesh": ref(mesh)}) or []:
            key = next((k for k in person.get("materials", {}) if safe(k) == safe(slot)), None)
            path = person["materials"].get(key) if key else None
            if path in instance_for:
                call(SK, "set_material", {"mesh": ref(mesh), "slot_name": slot, "material": ref(instance_for[path])})
            else:
                print("  no texture for slot", slot, "on", name)
        saved.append(mesh)
    print("save:", call(AT, "save_assets", {"asset_paths": saved}))


if __name__ == "__main__":
    main()
