"""
Convert a Meshy AI (or any glTF) .glb into FBX that Unreal can actually import.

WHY THIS EXISTS
    The MCP SkeletalMeshTools.import_file goes through FbxFactory, which accepts
    only .fbx and .obj - it rejects .glb outright, even though the editor's own
    drag-and-drop glTF importer handles it. So every Meshy delivery has to be
    transcoded first.

USAGE
    blender --background --python Tools/glb_to_fbx.py -- <src.glb> <dst.fbx> [0|1]

    The trailing flag bakes animation into the output. Leave it 0 for the
    character mesh; per-animation files come from Tools/split_glb_anims.py
    instead, because Unreal only imports ONE take per FBX.
"""

import bpy
import sys
import os

argv = sys.argv[sys.argv.index("--") + 1:]
src, dst = argv[0], argv[1]
want_anim = len(argv) > 2 and argv[2] == "1"

bpy.ops.wm.read_factory_settings(use_empty=True)
bpy.ops.import_scene.gltf(filepath=src)

actions = [a.name for a in bpy.data.actions]
print("IMPORTED_ARMATURES:", [o.name for o in bpy.data.objects if o.type == 'ARMATURE'])
print("IMPORTED_MESHES:", [o.name for o in bpy.data.objects if o.type == 'MESH'])
print("IMPORTED_ACTIONS:", len(actions), actions[:40])

os.makedirs(os.path.dirname(dst), exist_ok=True)

bpy.ops.export_scene.fbx(
    filepath=dst,
    use_selection=False,
    # Unreal generates its own leaf bones; Blender's extras just clutter the
    # skeleton and confuse retargeting later.
    add_leaf_bones=False,
    bake_anim=want_anim,
    bake_anim_use_all_bones=True,
    bake_anim_use_nla_strips=False,
    bake_anim_use_all_actions=want_anim,
    bake_anim_force_startend_keying=True,
    bake_anim_simplify_factor=0.0,
    mesh_smooth_type='FACE',
    path_mode='COPY',
    embed_textures=True,
    object_types={'ARMATURE', 'MESH'},
    # Meshy models already arrive at real-world scale in metres, which maps 1:1
    # onto Unreal centimetres through the FBX unit conversion. Touching either
    # of these produces a character either 100x too big or 100x too small.
    global_scale=1.0,
    apply_scale_options='FBX_SCALE_NONE',
)

print("EXPORTED:", dst, os.path.getsize(dst))
