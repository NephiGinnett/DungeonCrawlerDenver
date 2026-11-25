"""Import a Tools/unify_rig.py result into Unreal, bound to the shared crawler skeleton (2026-11-25).

Run with the editor CLOSED (a commandlet; it saves what it makes):

    "D:/UE/UE_5.8/Engine/Binaries/Win64/UnrealEditor-Cmd.exe" "<project>.uproject" -run=pythonscript
        -script="Tools/import_unified.py --src SourceArt/Unified/SK_X.fbx --dest /Game/DCC/Characters/Unified/X"
        -unattended -nosplash

Uses the classic FBX importer (Interchange's FBX path is switched off for this run) because it takes an existing
skeleton directly. The mesh lands on /Game/DCC/Characters/Shared/SK_DCCCrawler_Skeleton, so every crawler clip plays on
it. Any material it makes is flagged for skeletal meshes, or it draws grey in the game (see CLAUDE.md).
"""
import argparse
import shlex
import sys

import unreal

SKELETON = "/Game/DCC/Characters/Shared/SK_DCCCrawler_Skeleton.SK_DCCCrawler_Skeleton"


def args():
    raw = sys.argv[1:]
    if len(raw) == 1 and " " in raw[0]:
        raw = shlex.split(raw[0], posix=False)
    ap = argparse.ArgumentParser()
    ap.add_argument("--src", required=True)
    ap.add_argument("--dest", required=True)
    return ap.parse_args([a.strip('"') for a in raw])


def main():
    a = args()
    import os
    if not os.path.isabs(a.src):
        # Relative to the project (the project folder has a space in its name, which splits a quoted argument).
        a.src = os.path.join(unreal.Paths.convert_relative_path_to_full(unreal.Paths.project_dir()), a.src)
    unreal.SystemLibrary.execute_console_command(None, "Interchange.FeatureFlags.Import.FBX False")
    skel = unreal.load_asset(SKELETON)
    if not skel:
        unreal.log_error("[import_unified] the crawler skeleton is missing")
        return
    # Start clean: an existing material in the folder is reused as-is, so a reimport would never pick up textures
    # that were missing the first time (the Mushroom Demon, 2026-11-25).
    lib0 = unreal.EditorAssetLibrary
    if lib0.does_directory_exist(a.dest):
        for p in lib0.list_assets(a.dest, recursive=True, include_folder=False):
            lib0.delete_asset(p)
    ui = unreal.FbxImportUI()
    ui.set_editor_property("import_mesh", True)
    ui.set_editor_property("import_as_skeletal", True)
    ui.set_editor_property("mesh_type_to_import", unreal.FBXImportType.FBXIT_SKELETAL_MESH)
    ui.set_editor_property("skeleton", skel)
    ui.set_editor_property("import_animations", False)
    ui.set_editor_property("import_materials", True)
    ui.set_editor_property("import_textures", True)
    ui.set_editor_property("create_physics_asset", True)
    sk = ui.get_editor_property("skeletal_mesh_import_data")
    sk.set_editor_property("import_morph_targets", False)
    sk.set_editor_property("update_skeleton_reference_pose", False)
    sk.set_editor_property("use_t0_as_ref_pose", False)

    task = unreal.AssetImportTask()
    task.filename = a.src
    task.destination_path = a.dest
    task.automated = True
    task.replace_existing = True
    task.save = True
    task.options = ui
    unreal.AssetToolsHelpers.get_asset_tools().import_asset_tasks([task])
    made = list(task.get_editor_property("imported_object_paths") or [])
    unreal.log(f"[import_unified] imported {len(made)} object(s) into {a.dest}")

    lib = unreal.EditorAssetLibrary
    mel = unreal.MaterialEditingLibrary
    for p in lib.list_assets(a.dest, recursive=True, include_folder=False):
        x = lib.load_asset(p)
        if isinstance(x, unreal.Material) and not x.get_editor_property("used_with_skeletal_mesh"):
            x.set_editor_property("used_with_skeletal_mesh", True)
            mel.recompile_material(x)
        if isinstance(x, unreal.SkeletalMesh):
            s = x.get_editor_property("skeleton")
            unreal.log(f"[import_unified] mesh {x.get_path_name()} skeleton {s.get_path_name() if s else None}")
        lib.save_loaded_asset(x, only_if_is_dirty=False)


main()
