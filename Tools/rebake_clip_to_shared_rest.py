"""
Re-bake a Meshy animation clip onto the SHARED crawler rest pose (CJ's rig,
SK_DCCCrawler_Skeleton), so it plays correctly on every crawler in Unreal.

WHY THIS EXISTS (the "backwards-S spine", diagnosed 2026-11-13)
    The four Meshy rigs share bone NAMES and hierarchy, but not bone FRAMES.
    Each auto-rig orients its bones differently in the rest pose: measured
    against CJ's rig, Scott's Hips is rotated 144 deg, Spine02 143 deg, the
    upper legs 153 deg, feet up to 47 deg (Kyle/Marcel are similar).

    An animation stores each bone's LOCAL rotation absolutely, relative to
    its parent's frame - which only means the right thing on the rig it was
    authored on. Unreal plays it absolutely. So Scott's "Alert" idle (the one
    every crawler stands in) puts CJ's Hips into Scott's hip frame, and every
    vertex weighted to Hips/Spine02/legs is spun by the frame difference. That
    is the bent spine. Per-bone translation retargeting (DCC.FixCrawlerRetargeting)
    cannot help: it only touches translation, never rotation.

WHAT THIS DOES
    Rest-relative world-space retarget, the same maths the Companion uses:
        world_delta(bone)   = source_posed_world * source_rest_world^-1
        target_world(bone)  = world_delta * target_rest_world
    i.e. each bone moves AWAY FROM REST exactly as the source bone did, in
    world space, whatever axes either rig happens to use. Bone lengths come
    from the target; Hips translation is scaled by hip height.

USAGE
    blender --background --python Tools/rebake_clip_to_shared_rest.py -- \\
        <target_rig.fbx> <source_clip.fbx> <out.fbx|-> [--render <png_prefix> <frame>]

    <target_rig.fbx> : SourceArt/CJ_HockeyWings/fbx/SK_HockeyWings.fbx (the shared skeleton's source)
    <out.fbx>        : '-' to skip export (render only)
    --render         : writes <prefix>_before.png (what Unreal did: source local
                       rotations applied absolutely) and <prefix>_after.png (re-baked).
"""
import bpy, sys, os, math
from mathutils import Matrix, Vector

argv = sys.argv[sys.argv.index("--") + 1:]
target_src, clip_src, out_fbx = argv[0], argv[1], argv[2]
render_prefix, render_frame = None, None
if "--render" in argv:
    i = argv.index("--render")
    render_prefix, render_frame = argv[i + 1], int(argv[i + 2])

bpy.ops.wm.read_factory_settings(use_empty=True)
scene = bpy.context.scene


def import_fbx(path, tag):
    before = set(bpy.data.objects)
    bpy.ops.import_scene.fbx(filepath=path)
    new = [o for o in bpy.data.objects if o not in before]
    arm = next(o for o in new if o.type == 'ARMATURE')
    arm.name = tag
    for o in new:
        if o is not arm:
            o.name = tag + "_" + o.name
    return arm, new


tgt, tgt_objs = import_fbx(target_src, "TGT")
if tgt.animation_data:
    tgt.animation_data.action = None
src, src_objs = import_fbx(clip_src, "SRC")
src_action = src.animation_data.action
f0, f1 = (int(round(x)) for x in src_action.frame_range)
print("CLIP", src_action.name, f0, f1)

# Hide every source object; the target mesh is what we look at / export.
for o in src_objs:
    o.hide_render = True
    o.hide_set(True)

tb, sb = tgt.data.bones, src.data.bones
order = [b.name for b in tgt.data.bones if b.name in sb]  # bones are stored parent-first
order.sort(key=lambda n: len(tb[n].parent_recursive))


def rot(m):
    return m.to_3x3().normalized().to_4x4()


def rest_local(b):
    return b.parent.matrix_local.inverted() @ b.matrix_local if b.parent else b.matrix_local.copy()


def up(v):  # armature-space up axis is Z after FBX import
    return v.z


hip_ratio = up(tb["Hips"].head_local) / max(1e-6, up(sb["Hips"].head_local))
print("HIP_RATIO", round(hip_ratio, 3))


def solve(mode):
    """Return {bone: basis matrix} for the target at the current scene frame."""
    M, basis = {}, {}
    for n in order:
        b, s = tb[n], sb[n]
        spb = src.pose.bones[n]
        if mode == "absolute":
            # What Unreal did: copy the source LOCAL rotation verbatim; translation
            # comes from the target's own rest ("Skeleton" retarget mode), Hips from
            # the clip.
            src_local = (spb.parent.matrix.inverted() @ spb.matrix) if spb.parent else spb.matrix.copy()
            if b.parent:
                loc = rest_local(b).to_translation()
                pm = M[b.parent.name]
            else:
                loc = src_local.to_translation()
                pm = Matrix.Identity(4)
            m = pm @ (Matrix.Translation(loc) @ rot(src_local))
        else:
            delta = rot(spb.matrix) @ rot(s.matrix_local).inverted()
            r = delta @ rot(b.matrix_local)
            if b.parent:
                head = (M[b.parent.name] @ rest_local(b)).to_translation()
            else:
                head = spb.matrix.to_translation() * hip_ratio
            m = Matrix.Translation(head) @ r
        M[n] = m
        if b.parent:
            basis[n] = rest_local(b).inverted() @ M[b.parent.name].inverted() @ m
        else:
            basis[n] = b.matrix_local.inverted() @ m
    return basis


def apply(basis):
    for n, mb in basis.items():
        pb = tgt.pose.bones[n]
        pb.rotation_mode = 'QUATERNION'
        loc, q, _ = mb.decompose()
        pb.location, pb.rotation_quaternion, pb.scale = loc, q, (1, 1, 1)


def setup_render():
    scene.render.engine = 'BLENDER_WORKBENCH'
    scene.display.shading.color_type = 'TEXTURE'
    scene.display.shading.light = 'STUDIO'
    scene.render.resolution_x, scene.render.resolution_y = 900, 900
    meshes = [o for o in tgt_objs if o.type == 'MESH' and any(m.type == 'ARMATURE' for m in o.modifiers)]
    for o in tgt_objs:
        if o.type == 'MESH' and o not in meshes:
            o.hide_render = True  # stray helper geometry (e.g. a bounds sphere)
    scene.world = bpy.data.worlds.new("w")
    scene.world.color = (0.8, 0.8, 0.82)
    bpy.context.view_layer.update()
    pts = [o.matrix_world @ Vector(c) for o in meshes for c in o.bound_box]
    lo = Vector((min(p.x for p in pts), min(p.y for p in pts), min(p.z for p in pts)))
    hi = Vector((max(p.x for p in pts), max(p.y for p in pts), max(p.z for p in pts)))
    centre, height = (lo + hi) / 2, (hi.z - lo.z)
    cam_data = bpy.data.cameras.new("cam")
    cam_data.type = 'ORTHO'
    cam_data.ortho_scale = height * 1.35
    cam = bpy.data.objects.new("cam", cam_data)
    scene.collection.objects.link(cam)
    scene.camera = cam
    return cam, centre, height


def render(path, cam, centre, height):
    # Two views stitched by rendering twice: front (-Y) then side (+X).
    from bpy_extras.image_utils import load_image
    paths = []
    for tag, d, rz in (("front", Vector((0, -1, 0)), 0.0), ("side", Vector((1, 0, 0)), math.pi / 2)):
        cam.location = centre + d * height * 3
        cam.rotation_euler = (math.pi / 2, 0, rz)
        p = f"{path}_{tag}.png"
        scene.render.filepath = p
        bpy.ops.render.render(write_still=True)
        paths.append(p)
    print("RENDERED", paths)


if render_prefix:
    scene.frame_set(render_frame)
    cam, centre, height = setup_render()
    for mode, suffix in (("absolute", "before"), ("rebake", "after")):
        scene.frame_set(render_frame)
        apply(solve(mode))
        bpy.context.view_layer.update()
        render(f"{render_prefix}_{suffix}", cam, centre, height)

if out_fbx != "-":
    action = bpy.data.actions.new(os.path.splitext(os.path.basename(out_fbx))[0])
    tgt.animation_data_create()
    tgt.animation_data.action = action
    for f in range(f0, f1 + 1):
        scene.frame_set(f)
        apply(solve("rebake"))
        for n in order:
            pb = tgt.pose.bones[n]
            pb.keyframe_insert("location", frame=f)
            pb.keyframe_insert("rotation_quaternion", frame=f)
            pb.keyframe_insert("scale", frame=f)
    scene.frame_start, scene.frame_end = f0, f1
    # Export only the target rig + its mesh (Unreal needs geometry in an anim FBX).
    for o in src_objs:
        bpy.data.objects.remove(o, do_unlink=True)
    tgt.name = "Armature"  # any other name becomes an extra root bone in Unreal
    os.makedirs(os.path.dirname(os.path.abspath(out_fbx)), exist_ok=True)
    bpy.ops.export_scene.fbx(
        filepath=out_fbx, use_selection=False, add_leaf_bones=False,
        bake_anim=True, bake_anim_use_all_bones=True, bake_anim_use_nla_strips=False,
        bake_anim_use_all_actions=False, bake_anim_force_startend_keying=True,
        bake_anim_simplify_factor=0.0, mesh_smooth_type='FACE', path_mode='COPY',
        embed_textures=False, object_types={'ARMATURE', 'MESH'},
        global_scale=1.0, apply_scale_options='FBX_SCALE_NONE')
    print("EXPORTED", out_fbx, os.path.getsize(out_fbx))
