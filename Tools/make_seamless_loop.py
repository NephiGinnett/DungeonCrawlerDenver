"""Make a clip loop seamlessly, in place (2026-11-13).

    blender --background --python Tools/make_seamless_loop.py -- <in.fbx> <out.fbx> [blend_frames=12]

Why: the climb clip (A_DCC_DW_ClimbStairs, a stair climb) carries the hips 85 cm up and 33 cm forward over
its cycle, and its limbs end 5-23 degrees off where they began. Looped, it snaps back every cycle. In the
game the climb itself moves the character, so the clip should stay in place.

What it does, to the armature's action:
  1. Hips location: removes the straight-line drift from the first key to the last, so the hips end where
     they started (the bob within the cycle is kept).
  2. Every channel: over the last `blend_frames`, eases the value onto the first frame's value, so the
     last frame flows into the first with no seam.
Exports with the same settings as Tools/rebake_clip_to_shared_rest.py, armature still named "Armature".
"""
import sys

import bpy

argv = sys.argv[sys.argv.index("--") + 1:]
src, dst = argv[0], argv[1]
blend = int(argv[2]) if len(argv) > 2 else 12

bpy.ops.wm.read_factory_settings(use_empty=True)
bpy.ops.import_scene.fbx(filepath=src)
arm = next(o for o in bpy.data.objects if o.type == "ARMATURE")
act = arm.animation_data.action
f0, f1 = act.frame_range


def curves(action):
    # Blender 4.4+ keeps fcurves in layered channelbags; older versions on the action.
    if hasattr(action, "fcurves") and len(action.fcurves):
        return list(action.fcurves)
    out = []
    for layer in getattr(action, "layers", []):
        for strip in layer.strips:
            for bag in strip.channelbags:
                out.extend(bag.fcurves)
    return out


def shift(fc, fn):
    for kp in fc.keyframe_points:
        d = fn(kp.co[0])
        kp.co[1] += d
        kp.handle_left[1] += d
        kp.handle_right[1] += d
    fc.update()


hips = arm.pose.bones[0].name
fcs = curves(act)
print("SEAMLESS", act.name, "frames", f0, f1, "curves", len(fcs), "root", hips)

# 1. Hips drift out.
for fc in fcs:
    if fc.data_path == f'pose.bones["{hips}"].location':
        a, b = fc.evaluate(f0), fc.evaluate(f1)
        drift = b - a
        shift(fc, lambda f, d=drift: -d * (f - f0) / max(1.0, f1 - f0))
        print("  hips axis", fc.array_index, "drift removed", round(drift, 4))

# Quaternions: keep the last frame in the same hemisphere as the first, so easing goes the short way.
quats = {}
for fc in fcs:
    if fc.data_path.endswith("rotation_quaternion"):
        quats.setdefault(fc.data_path, {})[fc.array_index] = fc
for path, comps in quats.items():
    if len(comps) == 4:
        dot = sum(comps[i].evaluate(f0) * comps[i].evaluate(f1) for i in range(4))
        if dot < 0:
            print("  flipped hemisphere on", path)

# 2. Ease every channel's tail onto its first value.
start = f1 - blend
for fc in fcs:
    gap = fc.evaluate(f0) - fc.evaluate(f1)
    if abs(gap) < 1e-6:
        continue

    def ease(f, g=gap):
        if f <= start:
            return 0.0
        t = min(1.0, (f - start) / blend)
        return g * (t * t * (3 - 2 * t))  # smoothstep

    shift(fc, ease)

# The bake samples the scene's range, not the action's: match them, and check the pose still moves.
sc = bpy.context.scene
sc.frame_start, sc.frame_end = int(f0), int(f1)
for f in (int(f0), int((f0 + f1) / 2), int(f1)):
    sc.frame_set(f)
    print("  check frame", f, [round(v, 3) for v in (arm.matrix_world @ arm.pose.bones[0].matrix).translation])

bpy.ops.export_scene.fbx(
    filepath=dst, use_selection=False, add_leaf_bones=False,
    bake_anim=True, bake_anim_use_all_bones=True, bake_anim_use_nla_strips=False,
    bake_anim_use_all_actions=False, bake_anim_force_startend_keying=True,
    bake_anim_simplify_factor=0.0, mesh_smooth_type="FACE", path_mode="COPY",
    embed_textures=False, object_types={"ARMATURE", "MESH"},
    global_scale=1.0, apply_scale_options="FBX_SCALE_NONE")
print("EXPORTED", dst)
