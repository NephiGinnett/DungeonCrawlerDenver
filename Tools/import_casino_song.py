"""Import Content/Songs/Casino Cascade.mp3 as /Game/Songs/Casino_Cascade.

Run with the editor closed:

    UnrealEditor-Cmd.exe "<project>" -ExecutePythonScript="<this file>" -stdout -unattended -nosplash
"""
import unreal

ASSET = "/Game/Songs/Casino_Cascade"
SRC = r"C:\Users\kmett\Documents\Unreal Projects\DungeonCrawlerDenver\Content\Songs\Casino Cascade.mp3"

if unreal.EditorAssetLibrary.does_asset_exist(ASSET):
    unreal.log("Casino_Cascade already imported")
else:
    task = unreal.AssetImportTask()
    task.filename = SRC
    task.destination_path = "/Game/Songs"
    task.destination_name = "Casino_Cascade"
    task.automated = True
    task.save = True
    task.replace_existing = False
    unreal.AssetToolsHelpers.get_asset_tools().import_asset_tasks([task])
    unreal.EditorAssetLibrary.save_asset(ASSET, only_if_is_dirty=False)
    unreal.log("imported Casino_Cascade from " + SRC)
