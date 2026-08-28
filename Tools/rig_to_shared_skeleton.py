"""
Attempt to bind an unrigged mesh (OBJ/FBX/glTF) onto a copy of this project's
shared crawler skeleton, using Blender's automatic-weights parenting, and
export the result as a skinned FBX ready for Unreal import.

WHY THIS EXISTS
    A handful of Fab assets DCD wants to use as arena enemies (the
    DarkPaperVerse set, some of the free packs surfaced 2026-08-25) ship as
    OBJ - a format with no skeleton support at all - so they arrive with no
    rig and no animation. This is the same dead end the Smilestory monster
    batch hit (see Docs/asset-needs-meshy-pipeline.md's own §5). Rather than
    give up on animating them the way that pass did, this script tries the
    other route: bind them onto the skeleton every DCD character already
    shares (SK_DCCCrawler_Skeleton - 24 bones, root Hips) so the project's
    existing Walk/Run/attack anim set can drive them.

READ THIS BEFORE TRUSTING THE OUTPUT
    Automatic-weights parenting is a proximity heuristic (Blender assigns
    each vertex to whichever nearby bone dominates), not a real skinning
    pass. It works reasonably well on a mesh that is ALREADY built as a
    biped standing in a neutral pose at roughly human proportions - which is
    exactly what none of this is: these are stylized art pieces (a
    "Humanoid Cactus", a "Tin Guardsman Toy", a "Torsion Helix") never
    modeled with a rig in mind. Expect it to work fine on the ones that are
    already roughly biped-shaped, and expect visibly wrong deformation -
    stretching, tearing, limbs pulling the wrong geometry - on anything that
    isn't. This project has already hit a worse version of this exact
    problem once (the Debt Eater's own Meshy-generated Physics Asset
    deformed badly under simulation - see CLAUDE.md's "bUseRagdollOnDeath"
    note) and that was from a rig Meshy built ON PURPOSE for a biped. Treat
    every export from this script as a "try it and look," not a guaranteed
    result - inspect in Blender or Unreal before committing a Blueprint to
    it, and fall back to a static, non-animated prop (or a simple
    Blueprint-driven idle bob/spin instead of true skeletal animation) for
    anything that comes out wrong.

USAGE
    blender --background --python Tools/rig_to_shared_skeleton.py -- \\
        <skeleton_source.fbx> <target_mesh.obj|.fbx|.glb> <output.fbx> [target_height_m]

    <skeleton_source.fbx> - any FBX already containing the shared armature,
        e.g. SourceArt/BuffMinion/SK_BuffMinion.fbx. Its own mesh is deleted;
        only the armature is kept.
    <target_mesh> - the unrigged asset to bind. OBJ, FBX or glTF/GLB all
        import fine as a source mesh (only OBJ lacks skeleton support, which
        is exactly the thing this script is working around).
    [target_height_m] - optional, default 1.8. Fab assets are not scaled to
        match a human rig, so the mesh is uniformly rescaled to this height
        before binding. Lower it for something meant to read as squat/small
        (a "Fossilized Vespiary" is probably not 1.8m tall) - there is no way
        to guess this automatically, it needs a human judgement call per
        asset.
"""

import bpy
import sys
import os

argv = sys.argv[sys.argv.index("--") + 1:]
skeleton_src, target_src, dst = argv[0], argv[1], argv[2]
target_height_m = float(argv[3]) if len(argv) > 3 else 1.8

bpy.ops.wm.read_factory_settings(use_empty=True)

# -- Load the shared skeleton, strip its own mesh -----------------------------
bpy.ops.import_scene.fbx(filepath=skeleton_src)

armature = next((o for o in bpy.data.objects if o.type == 'ARMATURE'), None)
if armature is None:
    print("ERROR: no armature found in", skeleton_src)
    raise SystemExit(1)

for obj in list(bpy.data.objects):
    if obj.type == 'MESH':
        bpy.data.objects.remove(obj, do_unlink=True)

print("SKELETON: %s, %d bones" % (armature.name, len(armature.data.bones)))

# -- Import the target mesh, whatever format it's in --------------------------
ext = os.path.splitext(target_src)[1].lower()
if ext == '.obj':
    bpy.ops.wm.obj_import(filepath=target_src)
elif ext in ('.glb', '.gltf'):
    bpy.ops.import_scene.gltf(filepath=target_src)
elif ext == '.fbx':
    bpy.ops.import_scene.fbx(filepath=target_src)
else:
    print("ERROR: unrecognised target extension", ext)
    raise SystemExit(1)

mesh_objects = [o for o in bpy.data.objects if o.type == 'MESH']
if not mesh_objects:
    print("ERROR: target import produced no mesh - check the source file")
    raise SystemExit(1)

# Join everything the import produced into one mesh, so a multi-part asset
# (separate objects per material, which OBJ does constantly) binds as a
# single skinned mesh rather than several independently-parented pieces.
if len(mesh_objects) > 1:
    bpy.context.view_layer.objects.active = mesh_objects[0]
    for o in mesh_objects:
        o.select_set(True)
    bpy.ops.object.join()
    mesh_objects = [bpy.context.view_layer.objects.active]

target = mesh_objects[0]
target.name = "TargetMesh"

# -- Scale to the requested height, then sit it on the floor at the origin ----
bpy.context.view_layer.update()
bbox = [target.matrix_world @ __import__('mathutils').Vector(c) for c in target.bound_box]
min_z = min(v.z for v in bbox)
max_z = max(v.z for v in bbox)
height = max(max_z - min_z, 0.0001)

scale_factor = target_height_m / height
target.scale = (target.scale[0] * scale_factor,
                 target.scale[1] * scale_factor,
                 target.scale[2] * scale_factor)
bpy.context.view_layer.update()

bbox = [target.matrix_world @ __import__('mathutils').Vector(c) for c in target.bound_box]
min_z = min(v.z for v in bbox)
center_x = sum(v.x for v in bbox) / 8.0
center_y = sum(v.y for v in bbox) / 8.0

# Recentre so the mesh's own footprint sits at the world origin, matching
# where the armature's root (Hips, per the shared skeleton) already is -
# rough alignment, not a real retarget; expect to nudge this by hand for
# anything with an unusual silhouette.
target.location.x -= center_x
target.location.y -= center_y
target.location.z -= min_z

print("TARGET rescaled to height %.3fm, recentred at origin" % target_height_m)

# -- Bind with automatic weights ----------------------------------------------
bpy.ops.object.select_all(action='DESELECT')
target.select_set(True)
armature.select_set(True)
bpy.context.view_layer.objects.active = armature

bpy.ops.object.parent_set(type='ARMATURE_AUTO')

print("BOUND: %s -> %s (automatic weights)" % (target.name, armature.name))

# -- Export ---------------------------------------------------------------
os.makedirs(os.path.dirname(dst) or '.', exist_ok=True)

bpy.ops.object.select_all(action='SELECT')
bpy.ops.export_scene.fbx(
    filepath=dst,
    use_selection=True,
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
print("REMINDER: inspect this in Blender or Unreal before trusting it - see")
print("this script's own module docstring for why automatic weights on a")
print("never-rigged art asset is a real gamble, not a guaranteed fix.")
