"""Import a .glb straight into Unreal with every PBR map (2026-11-23).

Zoe: "Meshy's output by the time it reaches our game ... is just a little unpolished. Maybe we aren't doing it right."
We weren't, partly. The old road (.glb -> Blender -> FBX -> MCP SkeletalMeshTools.import_file) keeps no material
links, so every model was rebuilt with a colour-only material: no normal map, no roughness, no metalness. This uses
Unreal's own glTF importer (Interchange), which brings in the textures and builds a proper material for each.

Run it with the editor CLOSED (a commandlet; it saves the new assets):

    "D:/UE/UE_5.8/Engine/Binaries/Win64/UnrealEditor-Cmd.exe" "<project>.uproject" -run=pythonscript
        -script="Tools/import_glb_pbr.py --src <file.glb> --dest /Game/DCC/Imported/<Name>" -unattended -nosplash

Or from the editor's Python console / MCP ProgrammaticToolset with the same arguments in sys.argv.

    --compare   also make <Name>_OldWay: the same mesh with a colour-only material, the way the old road ended up,
                for a side-by-side (DCC.ImportCompare in the game).

Also worth knowing (found the same day): Meshy's rigged / animated exports ("..._biped_Character_output.glb", the
merged animations) carry ONLY a colour map and an emissive map. Its un-rigged texture export carries the full set. So
for a character, export the textured model with PBR from Meshy, and rig it separately (onto the UE Mannequin), or the
detail is gone before Unreal sees it.
"""
import argparse
import shlex
import sys

import unreal


def args():
    raw = sys.argv[1:]
    if len(raw) == 1 and " " in raw[0]:
        raw = shlex.split(raw[0], posix=False)
    ap = argparse.ArgumentParser()
    ap.add_argument("--src", required=True)
    ap.add_argument("--dest", required=True)
    ap.add_argument("--compare", action="store_true")
    return ap.parse_args([a.strip('"') for a in raw])


def main():
    a = args()
    name = a.dest.rstrip("/").split("/")[-1]
    task = unreal.AssetImportTask()
    task.filename = a.src
    task.destination_path = a.dest
    task.automated = True
    task.replace_existing = True
    task.save = True
    unreal.AssetToolsHelpers.get_asset_tools().import_asset_tasks([task])
    made = list(task.get_editor_property("imported_object_paths") or [])
    unreal.log(f"[import_glb_pbr] imported {len(made)} object(s) into {a.dest}")
    for p in made:
        unreal.log(f"[import_glb_pbr]   {p}")

    lib = unreal.EditorAssetLibrary
    assets = [lib.load_asset(p) for p in lib.list_assets(a.dest, recursive=True, include_folder=False)]
    meshes = [x for x in assets if isinstance(x, unreal.SkeletalMesh)] or [x for x in assets if isinstance(x, unreal.StaticMesh)]
    textures = [x for x in assets if isinstance(x, unreal.Texture2D)]
    mats = [x for x in assets if isinstance(x, unreal.MaterialInterface)]
    unreal.log(f"[import_glb_pbr] meshes {[m.get_name() for m in meshes]}  textures {[t.get_name() for t in textures]}  materials {[m.get_name() for m in mats]}")
    if not meshes:
        unreal.log_error("[import_glb_pbr] no mesh came in")
        return

    # Unreal's glTF materials are instances of an engine master (/InterchangeAssets/gltf/...M_GLTF) that is not flagged
    # for skeletal meshes, so on a character the game draws the grey default material instead ("missing usage flag
    # SkeletalMesh"). Give the project its own copy of each master with the flag on, and point the instances at it.
    if isinstance(meshes[0], unreal.SkeletalMesh):
        mel = unreal.MaterialEditingLibrary
        for m in mats:
            if not isinstance(m, unreal.MaterialInstance):
                if isinstance(m, unreal.Material) and not m.get_editor_property("used_with_skeletal_mesh"):
                    m.set_editor_property("used_with_skeletal_mesh", True)
                    mel.recompile_material(m)
                    lib.save_loaded_asset(m, False)
                continue
            parent = m.get_editor_property("parent")
            base = parent.get_base_material() if parent else None
            if not base or base.get_path_name().startswith("/Game/"):
                continue
            ours_path = f"/Game/DCC/Materials/Imported/{base.get_name()}_DCC"
            ours = lib.load_asset(ours_path) if lib.does_asset_exist(ours_path) else lib.duplicate_asset(base.get_path_name(), ours_path)
            if not ours.get_editor_property("used_with_skeletal_mesh"):
                ours.set_editor_property("used_with_skeletal_mesh", True)
                ours.set_editor_property("used_with_morph_targets", True)
                mel.recompile_material(ours)
                lib.save_loaded_asset(ours, False)
            if parent == base:
                mel.set_material_instance_parent(m, ours)
                lib.save_loaded_asset(m, False)
                unreal.log(f"[import_glb_pbr] {m.get_name()} now uses {ours_path} (skeletal mesh flag on)")

    if a.compare:
        # The colour map: the one a glTF material instance uses as its base colour.
        base = None
        for m in mats:
            if isinstance(m, unreal.MaterialInstance):
                for pv in m.get_editor_property("texture_parameter_values"):
                    pname = str(pv.get_editor_property("parameter_info").get_editor_property("name")).lower()
                    if "base" in pname and "color" in pname and pv.get_editor_property("parameter_value"):
                        base = pv.get_editor_property("parameter_value")
                        break
            if base:
                break
        base = base or textures[0]
        unreal.log(f"[import_glb_pbr] colour map for the old way: {base.get_name()}")
        mel = unreal.MaterialEditingLibrary
        old_path = f"{a.dest}_OldWay"
        if lib.does_directory_exist(old_path):
            lib.delete_directory(old_path)  # a rerun: start the comparison copy over
        tools = unreal.AssetToolsHelpers.get_asset_tools()
        mat = tools.create_asset(f"M_{name}_OldWay", old_path, unreal.Material, unreal.MaterialFactoryNew())
        sample = mel.create_material_expression(mat, unreal.MaterialExpressionTextureSample, -300, 0)
        sample.set_editor_property("texture", base)
        mel.connect_material_property(sample, "RGB", unreal.MaterialProperty.MP_BASE_COLOR)
        mat.set_editor_property("used_with_skeletal_mesh", True)
        mel.recompile_material(mat)
        lib.save_loaded_asset(mat, False)
        src_mesh = meshes[0]
        dup = lib.duplicate_asset(src_mesh.get_path_name(), f"{old_path}/{src_mesh.get_name()}_OldWay")
        if isinstance(dup, unreal.SkeletalMesh):
            dup.modify()
            fresh = []
            for slot in dup.get_editor_property("materials"):
                fresh.append(unreal.SkeletalMaterial(material_interface=mat, material_slot_name=slot.get_editor_property("material_slot_name")))
            dup.set_editor_property("materials", fresh)
        else:
            for i in range(len(dup.get_editor_property("static_materials"))):
                dup.set_material(i, mat)
        lib.save_loaded_asset(dup, False)
        unreal.log(f"[import_glb_pbr] old-way copy: {dup.get_path_name()}")


main()
