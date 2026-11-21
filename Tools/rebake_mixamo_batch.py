"""Bake a handful of Mixamo clips onto the shared crawler rest pose.

Mixamo bones match the crawler (Hips, RightHand, neck) after the mixamorig: prefix
is stripped. Fingers are left behind. The spine is paired by distance from the hips,
because this rig names Spine02 the bone off the hips and Spine the one at the shoulders.
The export keeps the crawler mesh, which Unreal's animation importer requires.

    blender --background --python Tools/rebake_mixamo_batch.py
"""
import os
import bpy
from mathutils import Matrix, Vector

ROOT = os.path.abspath(os.path.join(os.path.dirname(__file__), ".."))
TARGET = os.path.join(ROOT, "SourceArt", "CJ_HockeyWings", "fbx", "SK_HockeyWings.fbx")
SRC = os.path.join(ROOT, "SourceArt", "CombatAnims", "mixamo")
OUT = os.path.join(ROOT, "SourceArt", "CombatAnims", "fbx")

# (source file in mixamo/, output file name)
CLIPS = [
    ("Stabbing.fbx", "A_DCC_Stab.fbx"),
    ("Stable Sword Outward Slash.fbx", "A_DCC_Slash.fbx"),
    ("great sword attack.fbx", "A_DCC_Overhead.fbx"),
    ("Baseball Strike.fbx", "A_DCC_Blunt.fbx"),
    ("sword and shield impact.fbx", "A_DCC_Hit.fbx"),
    ("spell cast.fbx", "A_DCC_Cast.fbx"),
    ("Waving Gesture.fbx", "A_DCC_Wave.fbx"),
    ("great sword idle.fbx", "A_DCC_GS_Idle.fbx"),
    ("great sword walk.fbx", "A_DCC_GS_Walk.fbx"),
    ("great sword run.fbx", "A_DCC_GS_Run.fbx"),
    ("sword and shield idle.fbx", "A_DCC_SNS_Idle.fbx"),
    ("sword and shield walk.fbx", "A_DCC_SNS_Walk.fbx"),
    ("sword and shield run.fbx", "A_DCC_SNS_Run.fbx"),
]

def spine_chain(arm):
    """Spine bones from the hips outward. Meshy names them in reverse of Mixamo."""
    hips = next((b for b in arm.data.bones if b.name.lower() == "hips"), None)
    chain = []
    current = hips
    while current is not None:
        current = next((c for c in current.children if c.name.lower().startswith("spine")), None)
        if current is not None:
            chain.append(current)
    return chain


def import_fbx(path, tag):
    before = set(bpy.data.objects)
    bpy.ops.import_scene.fbx(filepath=path)
    new = [o for o in bpy.data.objects if o not in before]
    arm = next(o for o in new if o.type == "ARMATURE")
    arm.name = tag
    for o in new:
        if o is not arm:
            o.name = tag + "_" + o.name
    return arm, new


def strip_prefix(arm):
    for bone in arm.data.bones:
        if bone.name.startswith("mixamorig:"):
            bone.name = bone.name[len("mixamorig:"):]


def rot(m):
    return m.to_3x3().normalized().to_4x4()


def rest_local(b):
    return b.parent.matrix_local.inverted() @ b.matrix_local if b.parent else b.matrix_local.copy()


def match_bones(src, tgt):
    """Target bone name -> source bone name.

    Arms and legs match by name. The spine does not: on this crawler Spine02 is the
    bone off the hips and Spine is the one under the shoulders. Mixamo is the other
    way around, so those three are paired by distance from the hips.
    """
    strip_prefix(src)
    src_lower = {}
    for bone in src.data.bones:
        src_lower.setdefault(bone.name.lower(), bone.name)
    order, src_names = [], {}
    for bone in tgt.data.bones:
        if bone.name.lower().startswith("spine"):
            continue
        src_name = src_lower.get(bone.name.lower())
        if src_name:
            order.append(bone.name)
            src_names[bone.name] = src_name
    src_spines = spine_chain(src)
    tgt_spines = spine_chain(tgt)
    paired = min(len(src_spines), len(tgt_spines))
    print("SPINE_MAP", ", ".join(f"{src_spines[i].name}->{tgt_spines[i].name}" for i in range(paired)))
    for i in range(paired):
        order.append(tgt_spines[i].name)
        src_names[tgt_spines[i].name] = src_spines[i].name
    order.sort(key=lambda n: len(tgt.data.bones[n].parent_recursive))
    return order, src_names


def parent_posed(b, M):
    """Posed matrix of b's parent. A dropped bone (the neck) stays at its rest offset."""
    chain = []
    parent = b.parent
    while parent is not None and parent.name not in M:
        chain.append(parent)
        parent = parent.parent
    posed = M[parent.name].copy() if parent is not None else Matrix.Identity(4)
    for skipped in reversed(chain):
        posed = posed @ rest_local(skipped)
    return posed


def axis_height(bone):
    """How far the bone head sits from the armature origin.

    Mixamo's armature object is pitched 90 degrees, so the hip height is on Y
    and head_local.z is 0. Dividing by z used to blow the ratio up to millions
    and park the hips billions of units off the body.
    """
    head = bone.head_local
    return max(abs(head.x), abs(head.y), abs(head.z), 1e-3)


def src_vec_to_tgt(src, tgt, vector):
    """Armature-space vector on the Mixamo rig, expressed on the crawler rig.

    Rotation only. The object scale (0.01) is a unit conversion Blender already
    baked into the bone coordinates, and applying it again shrinks the motion.
    """
    src_rot = src.matrix_world.to_3x3().normalized()
    tgt_rot = tgt.matrix_world.to_3x3().normalized()
    return tgt_rot.inverted() @ (src_rot @ vector)


def shared_rot(obj, bone_matrix, tgt):
    """Bone orientation, moved into the target armature's space.

    Mixamo's armature object is pitched 90 degrees, so "down" in a greatsword
    grip is -Y inside that armature and -Z on the upright crawler. A delta taken
    in Mixamo's own axes points the crawler's forearms forward and up, and the
    hands cross over the head. Taking the delta after both poses are in the
    crawler's space keeps that grip below the shoulders.
    """
    world = rot(obj.matrix_world) @ rot(bone_matrix)
    return rot(tgt.matrix_world).inverted() @ world


def up_axis(bone):
    head = bone.head_local
    return max(range(3), key=lambda i: abs(head[i]))


def solve(src, tgt, order, src_names, hip_ratio, hip_motion, hip_correction):
    tb, sb = tgt.data.bones, src.data.bones
    M, basis = {}, {}
    for n in order:
        b = tb[n]
        sname = src_names[n]
        spb = src.pose.bones[sname]
        delta = shared_rot(src, spb.matrix, tgt) @ shared_rot(src, sb[sname].matrix_local, tgt).inverted()
        r = delta @ shared_rot(tgt, b.matrix_local, tgt)
        # One rotation for the whole pose, taken from the first frame's hips.
        # Turning each bone back to the bind pose on its own is a T-pose: the
        # crawler rest has the arms straight out. Turning the finished pose
        # keeps the weapon hold and the steps, and stands the hips upright.
        if hip_correction is not None:
            r = hip_correction @ r
        if b.parent:
            posed_parent = parent_posed(b, M)
            head = (posed_parent @ rest_local(b)).to_translation()
        else:
            # hip_motion is already the in-place offset from the source rest,
            # in source armature space. Mixamo walks travel forward; the crawler
            # capsule does that itself, so locomotion keeps the bob and not the trip.
            head = b.head_local + src_vec_to_tgt(src, tgt, hip_motion) * hip_ratio
        m = Matrix.Translation(head) @ r
        M[n] = m
        if b.parent:
            basis[n] = rest_local(b).inverted() @ posed_parent.inverted() @ m
        else:
            basis[n] = b.matrix_local.inverted() @ m
    return basis


def apply_pose(tgt, basis):
    for n, mb in basis.items():
        pb = tgt.pose.bones[n]
        pb.rotation_mode = "QUATERNION"
        loc, quat, _ = mb.decompose()
        pb.location, pb.rotation_quaternion, pb.scale = loc, quat, (1, 1, 1)


def bake_one(clip_name, out_name):
    bpy.ops.wm.read_factory_settings(use_empty=True)
    scene = bpy.context.scene
    tgt, tgt_objs = import_fbx(TARGET, "TGT")
    if tgt.animation_data:
        tgt.animation_data.action = None
    src, src_objs = import_fbx(os.path.join(SRC, clip_name), "SRC")
    action_src = src.animation_data.action
    f0, f1 = (int(round(x)) for x in action_src.frame_range)
    order, src_names = match_bones(src, tgt)
    print("BAKE", clip_name, "frames", f0, f1, "bones", len(order), "fps", scene.render.fps)
    for bone in tgt.data.bones:
        if bone.name.lower() == "head" and bone.parent:
            print("HEAD_PARENT", bone.parent.name, "mapped", bone.parent.name in src_names)
            break
    hips_name = next((n for n in order if n.lower() == "hips"), None)
    if len(order) < 15 or hips_name is None:
        raise RuntimeError("bone map failed for " + clip_name)
    src_hip = src.data.bones[src_names[hips_name]]
    tgt_hip = tgt.data.bones[hips_name]
    hip_ratio = axis_height(tgt_hip) / axis_height(src_hip)
    if not 0.2 <= hip_ratio <= 5.0:
        raise RuntimeError(f"hip ratio {hip_ratio} is not a body scale for {clip_name}")
    print("HIP_RATIO", round(hip_ratio, 3),
          "src_head", tuple(round(v, 2) for v in src_hip.head_local),
          "tgt_head", tuple(round(v, 2) for v in tgt_hip.head_local))
    scene.frame_set(f0)
    bpy.context.view_layer.update()
    src_hip_bone = src_names[hips_name]
    hip_at_start = src.pose.bones[src_hip_bone].matrix.to_translation().copy()
    # Walks, runs and idles stand on the crawler's hips. Attacks keep the
    # Mixamo pose, including the lunge.
    in_place = any(word in clip_name.lower() for word in ("walk", "run", "idle"))
    hip_correction = None
    if in_place:
        preview = solve(src, tgt, order, src_names, hip_ratio, Vector((0.0, 0.0, 0.0)), None)
        apply_pose(tgt, preview)
        bpy.context.view_layer.update()
        posed_hips = rot(tgt.pose.bones[hips_name].matrix)
        rest_hips = rot(tgt.data.bones[hips_name].matrix_local)
        hip_correction = rest_hips @ posed_hips.inverted()
    up_i = up_axis(src_hip)
    action = bpy.data.actions.new(os.path.splitext(out_name)[0])
    tgt.animation_data_create()
    tgt.animation_data.action = action
    hip_peak = 0.0
    for frame in range(f0, f1 + 1):
        scene.frame_set(frame)
        bpy.context.view_layer.update()
        now = src.pose.bones[src_hip_bone].matrix.to_translation()
        if in_place:
            moved = now - hip_at_start
            for axis in range(3):
                if axis != up_i:
                    moved[axis] = 0.0
            hip_motion = moved
        else:
            hip_motion = now - src_hip.head_local
        basis = solve(src, tgt, order, src_names, hip_ratio, hip_motion, hip_correction)
        apply_pose(tgt, basis)
        if in_place and frame == f0:
            # The broken axis put both wrists above the head. A locomotion clip
            # whose grip starts there is the bug, not a pose.
            bpy.context.view_layer.update()
            origin = tgt.matrix_world @ tgt.pose.bones[hips_name].head

            def above(name):
                return ((tgt.matrix_world @ tgt.pose.bones[name].head) - origin).z

            shoulder_z = above("LeftShoulder")
            left_z, right_z = above("LeftHand"), above("RightHand")
            print("WRISTS", round(left_z, 2), round(right_z, 2), "shoulder", round(shoulder_z, 2))
            if left_z >= shoulder_z or right_z >= shoulder_z:
                raise RuntimeError(
                    f"{clip_name} starts with a hand above the shoulder "
                    f"(L {left_z:.2f} R {right_z:.2f} shoulder {shoulder_z:.2f})")
        hip_peak = max(hip_peak, tgt.pose.bones[hips_name].location.length)
        for n in order:
            pb = tgt.pose.bones[n]
            pb.keyframe_insert("location", frame=frame)
            pb.keyframe_insert("rotation_quaternion", frame=frame)
            pb.keyframe_insert("scale", frame=frame)
    scene.frame_start, scene.frame_end = f0, f1
    if hip_peak > 80.0:
        raise RuntimeError(f"{clip_name} hip offset peaked at {hip_peak:.1f}, still exploded")
    print("HIP_PEAK", round(hip_peak, 2))
    for o in src_objs:
        bpy.data.objects.remove(o, do_unlink=True)
    # The shared skeleton's root bone is TGT. That is the armature object name from the
    # original import. Naming this Armature makes Unreal skip the node, and the clip
    # then has no TGT track to bind to.
    tgt.name = "TGT"
    tgt.data.name = "TGT"
    out_path = os.path.join(OUT, out_name)
    os.makedirs(OUT, exist_ok=True)
    bpy.ops.export_scene.fbx(
        filepath=out_path, use_selection=False, add_leaf_bones=False,
        bake_anim=True, bake_anim_use_all_bones=True, bake_anim_use_nla_strips=False,
        bake_anim_use_all_actions=False, bake_anim_force_startend_keying=True,
        bake_anim_simplify_factor=0.0, mesh_smooth_type="FACE", path_mode="COPY",
        embed_textures=False, object_types={"ARMATURE", "MESH"},
        global_scale=1.0, apply_scale_options="FBX_SCALE_NONE")
    print("EXPORTED", out_name, os.path.getsize(out_path))


def main():
    missing = [name for name, _ in CLIPS if not os.path.isfile(os.path.join(SRC, name))]
    if missing:
        raise SystemExit("missing source clips: " + ", ".join(missing))
    for src_name, out_name in CLIPS:
        bake_one(src_name, out_name)
    print("BATCH_DONE", len(CLIPS))


if __name__ == "__main__":
    main()
