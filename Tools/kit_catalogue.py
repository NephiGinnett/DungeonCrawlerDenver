"""Catalogue the meshes and maps in the place packs, for room building (2026-11-25).

Run with the editor CLOSED (a commandlet; it reads the asset registry and loads nothing):

    "D:/UE/UE_5.8/Engine/Binaries/Win64/UnrealEditor-Cmd.exe" "<project>.uproject" -run=pythonscript
        -script="Tools/kit_catalogue.py" -unattended -nosplash

Writes SourceArt/Kits/<Pack>.json: every static mesh with its size in cm (from the ApproxSize tag), triangle count,
Nanite flag and a rough role guessed from its shape (floor, wall, pillar, door, prop, big), plus the pack's maps.
Tools/kit_report.py turns those into the summary in Docs/kits.md.
"""
import json
import os
import re

import unreal

PACKS = [
    # Part H1 of Docs/grok-plan-2026-11-25-fab-packs.md
    "SciFi_Space_Station", "DerelictCorridor", "StarterBundle", "ModSciInteriors", "ModSci_Engineer",
    "ModSci_EngiProps", "ElderboomVillage", "Forest_village", "Inferno_World_Free", "Scene_Saloon",
    "StylizedVolClouds", "SuperGrid",
    # Part G
    "BroadcastStudio", "Scene_OldMine", "AbandonedPowerPlant", "Clinic", "MetroMaintenanceStation",
    # The Factory Environment was merged into these root folders (Docs/fab-copies-2026-11-25.md)
    "Meshes",
    # Props packs Claude may use while building
    "Gothic_props_1", "ModernStreetProps_01", "Barrels_Boxes", "StageProps", "Torn_Banner_Free",
]


def tag(data, name):
    v = data.get_tag_value(name)
    return str(v) if v is not None else ""


def size_of(data):
    raw = tag(data, "ApproxSize")
    m = re.findall(r"[\d.]+", raw)
    return [round(float(x), 1) for x in m[:3]] if len(m) >= 3 else None


def role(size, name):
    n = name.lower()
    if any(k in n for k in ("door", "gate")):
        return "door"
    if not size:
        return "unknown"
    x, y, z = sorted(size[:2], reverse=True) + [size[2]]
    big = max(size)
    if big > 3000:
        return "big"
    if z < 0.15 * x and x >= 150:
        return "floor"
    if z >= 150 and y < 0.2 * x and x >= 150:
        return "wall"
    if z >= 200 and x < 0.4 * z and y < 0.4 * z:
        return "pillar"
    return "prop"


def main():
    reg = unreal.AssetRegistryHelpers.get_asset_registry()
    root = unreal.Paths.convert_relative_path_to_full(unreal.Paths.project_dir())
    out_dir = os.path.join(root, "SourceArt", "Kits")
    os.makedirs(out_dir, exist_ok=True)
    ext = os.path.join(root, "Content", "__ExternalActors__")
    for pack in PACKS:
        path = "/Game/" + pack
        if not unreal.EditorAssetLibrary.does_directory_exist(path):
            unreal.log_warning(f"[kits] {pack}: missing")
            continue
        flt = unreal.ARFilter(package_paths=[path], recursive_paths=True)
        meshes, maps = [], []
        for d in reg.get_assets(flt):
            cls = str(d.asset_class_path.asset_name)
            obj = f"{d.package_name}.{d.asset_name}"
            if cls == "StaticMesh":
                s = size_of(d)
                meshes.append({
                    "path": obj, "name": str(d.asset_name), "size": s,
                    "tris": int(tag(d, "Triangles") or 0), "nanite": tag(d, "NaniteEnabled"),
                    "role": role(s, str(d.asset_name)),
                })
            elif cls == "World":
                rel = str(d.package_name)[len("/Game/"):]
                partitioned = os.path.isdir(os.path.join(ext, *rel.split("/")))
                maps.append({"path": obj, "partitioned": partitioned})
        meshes.sort(key=lambda m: (m["role"], m["name"]))
        with open(os.path.join(out_dir, pack + ".json"), "w", encoding="utf-8") as f:
            json.dump({"pack": pack, "maps": maps, "meshes": meshes}, f, indent=1)
        roles = {}
        for m in meshes:
            roles[m["role"]] = roles.get(m["role"], 0) + 1
        unreal.log(f"[kits] {pack}: {len(meshes)} meshes {roles}, {len(maps)} maps")


main()
