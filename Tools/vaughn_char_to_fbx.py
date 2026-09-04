"""
One-off: convert Vaughn's Meshy character glb to FBX, dropping the stray
'Icosphere' junk mesh Meshy included alongside the real character mesh
(confirmed via Tools/list_bones.py - not part of the 24-bone rig, not
attached to any bone, pure bounding-reference junk).

USAGE
    blender --background --python Tools/vaughn_char_to_fbx.py -- <src.glb> <dst.fbx>
"""

import bpy
import sys
import os

argv = sys.argv[sys.argv.index("--") + 1:]
src, dst = argv[0], argv[1]

bpy.ops.wm.read_factory_settings(use_empty=True)
bpy.ops.import_scene.gltf(filepath=src)

junk = bpy.data.objects.get("Icosphere")
if junk is not None:
    bpy.data.objects.remove(junk, do_unlink=True)
    print("REMOVED_JUNK_MESH: Icosphere")

print("IMPORTED_ARMATURES:", [o.name for o in bpy.data.objects if o.type == 'ARMATURE'])
print("IMPORTED_MESHES:", [o.name for o in bpy.data.objects if o.type == 'MESH'])

os.makedirs(os.path.dirname(dst), exist_ok=True)

bpy.ops.export_scene.fbx(
    filepath=dst,
    use_selection=False,
    add_leaf_bones=False,
    bake_anim=False,
    mesh_smooth_type='FACE',
    path_mode='COPY',
    embed_textures=True,
    object_types={'ARMATURE', 'MESH'},
    global_scale=1.0,
    apply_scale_options='FBX_SCALE_NONE',
)

print("EXPORTED:", dst, os.path.getsize(dst))
