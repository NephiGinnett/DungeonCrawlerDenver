"""
Re-orient a crawler mesh's bones onto the SHARED rest frames (CJ's rig), keeping
its own joint positions, skin weights and geometry exactly as they are.

Companion to Tools/rebake_clip_to_shared_rest.py; read that file's docstring for
the "backwards-S spine" diagnosis. In short: animations store local rotations,
which only mean the same thing on rigs whose bones share the same axes. Clips are
re-baked onto CJ's axes; this script puts every BODY on CJ's axes too, so any clip
plays on any body.

Changing a bone's rest orientation does not move a single vertex: at rest every
bone's skinning matrix is identity whatever its axes. Only the head positions
matter for shape, and those are kept.

USAGE
    blender --background --python Tools/rebind_mesh_to_shared_frames.py -- \\
        <shared_rig.fbx> <mesh.fbx> <out.fbx>
"""
import bpy, sys, os

argv = sys.argv[sys.argv.index("--") + 1:]
shared_src, mesh_src, out_fbx = argv

bpy.ops.wm.read_factory_settings(use_empty=True)


def import_fbx(path):
    before = set(bpy.data.objects)
    bpy.ops.import_scene.fbx(filepath=path)
    new = [o for o in bpy.data.objects if o not in before]
    return next(o for o in new if o.type == 'ARMATURE'), new


ref, ref_objs = import_fbx(shared_src)
ref_rot = {b.name: b.matrix_local.to_3x3().normalized() for b in ref.data.bones}
for o in ref_objs:
    bpy.data.objects.remove(o, do_unlink=True)

arm, objs = import_fbx(mesh_src)
if arm.animation_data:
    arm.animation_data.action = None
for pb in arm.pose.bones:
    pb.matrix_basis.identity()

bpy.context.view_layer.objects.active = arm
for o in bpy.context.selected_objects:
    o.select_set(False)
arm.select_set(True)
bpy.ops.object.mode_set(mode='EDIT')
changed = 0
for eb in arm.data.edit_bones:
    r = ref_rot.get(eb.name)
    if r is None:
        continue
    eb.use_connect = False
    head, length = eb.head.copy(), eb.length
    m = r.to_4x4()
    m.translation = head
    eb.matrix = m          # sets head + orientation (Y axis = bone direction, roll from X/Z)
    eb.length = length     # keep this body's own bone length
    changed += 1
bpy.ops.object.mode_set(mode='OBJECT')
print("REORIENTED", changed, "bones")

# Unreal turns any armature object NOT named "Armature" into an extra root bone,
# which then fails to match the shared skeleton ("missing bones"). The rig-fit
# exports named theirs "SharedSkeleton" - the reason each _Fixed mesh ended up
# with a skeleton of its own.
arm.name = "Armature"
arm.data.name = "Armature"
os.makedirs(os.path.dirname(os.path.abspath(out_fbx)), exist_ok=True)
bpy.ops.export_scene.fbx(
    filepath=out_fbx, use_selection=False, add_leaf_bones=False, bake_anim=False,
    mesh_smooth_type='FACE', path_mode='COPY', embed_textures=True,
    object_types={'ARMATURE', 'MESH'}, global_scale=1.0, apply_scale_options='FBX_SCALE_NONE')
print("EXPORTED", out_fbx, os.path.getsize(out_fbx))
