"""
Split a Meshy "Merged_Animations" .glb into one FBX per animation clip.

WHY THIS EXISTS
    Unreal imports exactly ONE animation take per FBX file. A merged-animations
    export containing four clips therefore yields a single AnimSequence and
    silently drops the rest - which looks like the import worked.

    Worse, an armature-only FBX imports as literally nothing: no error, just
    "produced no assets". Unreal's skeletal-mesh import path needs geometry
    present to recognise the file, so the mesh has to ride along in every clip.
    Each import consequently also produces a throwaway SkeletalMesh to delete.

USAGE
    blender --background --python Tools/split_glb_anims.py -- <src.glb> <outdir> [prefix]

    Then import each FBX with SkeletalMeshTools.import_file, binding to the
    character's existing Skeleton asset. Importing as TMP_<name> produces
    TMP_<name> (junk mesh, delete) and TMP_<name>_Anim (the AnimSequence, keep).
"""

import bpy
import sys
import os

argv = sys.argv[sys.argv.index("--") + 1:]
src, outdir = argv[0], argv[1]
prefix = argv[2] if len(argv) > 2 else "A"

bpy.ops.wm.read_factory_settings(use_empty=True)
bpy.ops.import_scene.gltf(filepath=src)

arm = next((o for o in bpy.data.objects if o.type == 'ARMATURE'), None)
if arm is None:
    print("ERROR: no armature in", src)
    raise SystemExit(1)

if not arm.animation_data:
    arm.animation_data_create()

os.makedirs(outdir, exist_ok=True)

for act in list(bpy.data.actions):
    arm.animation_data.action = act

    start, end = act.frame_range
    bpy.context.scene.frame_start = int(start)
    bpy.context.scene.frame_end = int(end)

    safe = "".join(c if (c.isalnum() or c == '_') else '_' for c in act.name)
    dst = os.path.join(outdir, "%s_%s.fbx" % (prefix, safe))

    bpy.ops.export_scene.fbx(
        filepath=dst,
        use_selection=False,
        add_leaf_bones=False,
        bake_anim=True,
        bake_anim_use_all_bones=True,
        bake_anim_use_nla_strips=False,
        # False on purpose: exactly one action per file is the entire point.
        bake_anim_use_all_actions=False,
        bake_anim_force_startend_keying=True,
        bake_anim_simplify_factor=0.0,
        object_types={'ARMATURE', 'MESH'},
        global_scale=1.0,
        apply_scale_options='FBX_SCALE_NONE',
    )

    print("EXPORTED_ACTION:", act.name, "->", dst, "frames", int(start), int(end))
