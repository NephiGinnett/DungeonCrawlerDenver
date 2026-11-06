"""Convert a character FBX to .glb for the Crawler Companion (three.js reads glTF, not FBX).

    D:\\Blender\\blender.exe --background --python Tools/fbx_to_glb.py -- <src.fbx> <dst.glb>

Mesh, skin and textures only; animations come from the other crawlers' clips, retargeted in the viewer.
"""
import sys

import bpy

args = sys.argv[sys.argv.index("--") + 1:]
src, dst = args[0], args[1]

bpy.ops.wm.read_factory_settings(use_empty=True)
bpy.ops.import_scene.fbx(filepath=src, automatic_bone_orientation=False)

# Drop any baked action so the export is the bind pose, not frame 1 of something.
for obj in bpy.data.objects:
    if obj.animation_data:
        obj.animation_data.action = None

bpy.ops.export_scene.gltf(filepath=dst, export_format="GLB", export_animations=False, export_skins=True)
print("wrote", dst)
