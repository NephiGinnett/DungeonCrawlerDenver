"""
The uniform bone maker (2026-11-25, Zoe: "I hope we can actually use some of these enemies ... Maybe we can make a
uniform bone maker").

Takes a character that ALREADY has a rig of its own (3ds Max Biped "Bip001-...", Mixamo "mixamorig:...", the UE
Mannequin "pelvis/spine_01/...", or Meshy "Hips/Spine02/...") and rebuilds it on the project's shared crawler skeleton
(SK_DCCCrawler_Skeleton: 24 bones, root Hips), keeping the model's own skin weights. The result imports into Unreal
bound to that skeleton, so every crawler animation (walk, run, the Mixamo swings, hit reactions, emotes) plays on it.

How:
  1. Every source bone is classified by name (side + body part), so it maps onto one of our 24 bones. Bones with no
     place of their own (fingers, twist bones, FX sockets, weapon bones, tails) fold into their nearest mapped parent:
     their weights move to that bone.
  2. A copy of our skeleton is built with each joint where the source's matching joint is (the model keeps its own
     proportions), and each bone keeps our skeleton's roll, so our animations turn it the same way they turn a crawler.
     Joints the source lacks (a middle spine bone, toes, the head tip) are placed between their neighbours.
  3. The model is turned to face the way our crawlers face (-Y in Blender) and stood on the floor.
  4. Exported as FBX the way the project's skeletal meshes are (centimetres, no leaf bones), with the armature object
     named "Armature" so Unreal does not add an extra root bone.

For a mesh with NO rig at all, use Tools/rig_to_shared_skeleton.py (automatic weights) instead.

Usage:
    blender --background --python Tools/unify_rig.py -- <source .blend|.fbx|.glb> <out.fbx> [--height <m>] [--preview <png>]
    --preview renders a still of the result in three poses of a crawler walk, to check the skinning by eye.
"""
from __future__ import annotations

import math
import os
import re
import sys

import bpy
from mathutils import Matrix, Vector

REPO = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
CRAWLER_FBX = os.path.join(REPO, "SourceArt", "BuffMinion", "SK_BuffMinion.fbx")
TARGET_HEIGHT = 2.0  # metres, top of the head; --height to change it (a boss is taller)
WALK_FBX = os.path.join(REPO, "SourceArt", "Shared_Rebaked", "A_DCC_DW_Walking.fbx")

# Our skeleton, parent first.
OURS = [
    ("Hips", None), ("LeftUpLeg", "Hips"), ("LeftLeg", "LeftUpLeg"), ("LeftFoot", "LeftLeg"), ("LeftToeBase", "LeftFoot"),
    ("RightUpLeg", "Hips"), ("RightLeg", "RightUpLeg"), ("RightFoot", "RightLeg"), ("RightToeBase", "RightFoot"),
    ("Spine02", "Hips"), ("Spine01", "Spine02"), ("Spine", "Spine01"),
    ("LeftShoulder", "Spine"), ("LeftArm", "LeftShoulder"), ("LeftForeArm", "LeftArm"), ("LeftHand", "LeftForeArm"),
    ("RightShoulder", "Spine"), ("RightArm", "RightShoulder"), ("RightForeArm", "RightArm"), ("RightHand", "RightForeArm"),
    ("neck", "Spine"), ("Head", "neck"), ("head_end", "Head"), ("headfront", "Head"),
]
# Bones that keep our skeleton's exact rest orientation (only their joint moves): the body keeps its own posture (a
# hunched monster stays hunched) and the clip's small torso motion rides on top. Limbs instead point at their next
# joint, so a clip swings an arm or a leg the way it swings a crawler's.
TORSO = {"Hips", "Spine02", "Spine01", "Spine", "neck", "Head", "LeftShoulder", "RightShoulder"}
CHILD = {"Hips": "Spine02", "Spine02": "Spine01", "Spine01": "Spine", "Spine": "neck", "neck": "Head", "Head": "head_end",
         "LeftUpLeg": "LeftLeg", "LeftLeg": "LeftFoot", "LeftFoot": "LeftToeBase",
         "RightUpLeg": "RightLeg", "RightLeg": "RightFoot", "RightFoot": "RightToeBase",
         "LeftShoulder": "LeftArm", "LeftArm": "LeftForeArm", "LeftForeArm": "LeftHand",
         "RightShoulder": "RightArm", "RightArm": "RightForeArm", "RightForeArm": "RightHand"}


def argv():
    a = sys.argv[sys.argv.index("--") + 1:] if "--" in sys.argv else []
    preview = None
    global TARGET_HEIGHT
    if "--height" in a:
        i = a.index("--height")
        TARGET_HEIGHT = float(a[i + 1])
        del a[i:i + 2]
    if "--preview" in a:
        i = a.index("--preview")
        preview = a[i + 1]
        del a[i:i + 2]
    return a[0], a[1], preview


# ---- 1. classify a source bone ---------------------------------------------------------------------------------------

SKIP = re.compile(r"finger|thumb|index|middle|ring|pinky|twist|fx|weapon|prop|socket|ik|pole|tail|ponytail|hair|cloth|"
                  r"jaw|eye|brow|lip|tongue|cape|skirt|bag|nub|end$|_end|attach|dummy|helper|ear|horn|wing", re.I)


def side_of(name: str) -> str:
    n = name.replace("Bip001", "").replace("mixamorig:", "")
    if re.search(r"(^|[^a-z])(l|left)([^a-z]|$)|left|_l$|\.l$|-l-|[^a-z]l[^a-z]", n, re.I) and not re.search(r"right", n, re.I):
        if re.search(r"(^|[-_. ])l([-_. ]|$)|left|_l$|\.l$", n, re.I):
            return "Left"
    if re.search(r"(^|[-_. ])r([-_. ]|$)|right|_r$|\.r$", n, re.I):
        return "Right"
    return ""


def part_of(name: str) -> str | None:
    n = re.sub(r"^(Bip\d+|mixamorig:)", "", name, flags=re.I).lower()
    n = n.replace("-", " ").replace("_", " ").replace(".", " ")
    if SKIP.search(n):
        return None
    if re.search(r"clavicle|shoulder|collar", n):
        return "Shoulder"
    if re.search(r"forearm|lowerarm|lower arm|elbow", n):
        return "ForeArm"
    if re.search(r"upperarm|upper arm|(^|\s)arm(\s|$)", n):
        return "Arm"
    if re.search(r"hand|wrist|palm", n):
        return "Hand"
    if re.search(r"thigh|upleg|upperleg|up leg|upper leg", n):
        return "UpLeg"
    if re.search(r"calf|shin|lowerleg|lower leg|knee|(^|\s)leg(\s|$)", n):
        return "Leg"
    if re.search(r"toe|ball", n):
        return "ToeBase"
    if re.search(r"foot|ankle", n):
        return "Foot"
    if re.search(r"pelvis|hips|hip(\s|$)|root hip", n):
        return "Hips"
    if re.search(r"neck", n):
        return "neck"
    if re.search(r"head", n):
        return "Head"
    if re.search(r"spine|chest|torso|abdomen|back", n):
        return "Spine*"
    return None


def classify(arm) -> dict[str, str]:
    """Source bone name -> our bone name, for the bones that have a place of their own."""
    out: dict[str, str] = {}
    spine: list = []
    for b in arm.data.bones:
        part = part_of(b.name)
        if not part:
            continue
        if part == "Spine*":
            spine.append(b)
            continue
        if part in ("Hips", "neck", "Head"):
            # The first (highest in the chain) wins: Biped's "Bip001" and "Bip001-Pelvis" are both hip-ish.
            if part not in out.values():
                out[b.name] = part
            continue
        side = side_of(b.name)
        if not side:
            continue
        ours = side + part
        if ours not in out.values():
            out[b.name] = ours
    # The spine: lowest to highest becomes Spine02, Spine01, Spine (our names run top-down).
    spine.sort(key=lambda b: (arm.matrix_world @ b.head_local).z)
    names = ["Spine02", "Spine01", "Spine"]
    if len(spine) == 1:
        out[spine[0].name] = "Spine02"
    elif len(spine) == 2:
        out[spine[0].name], out[spine[1].name] = "Spine02", "Spine"
    elif spine:
        picks = [spine[0], spine[len(spine) // 2], spine[-1]]
        for b, n in zip(picks, names):
            out[b.name] = n
    # Biped puts a "Bip001" above the pelvis; make sure Hips is the pelvis joint when both exist.
    return out


def fold_target(bone, mapped: dict[str, str]) -> str | None:
    """Where a bone's weights go: its own mapped name, else the nearest mapped ancestor."""
    b = bone
    while b is not None:
        if b.name in mapped:
            return mapped[b.name]
        b = b.parent
    return None


# ---- the work --------------------------------------------------------------------------------------------------------

def relink_images(folder: str):
    """Packs often ship a .blend whose images point at a folder that is not in the download (the Mushroom Demon's look
    in "Texture/Sec Texture", but its files are in "Third Texture"). Find each missing image by file name anywhere
    under the source's folder, so the textures make it into the FBX."""
    found: dict[str, str] = {}
    for root, _dirs, files in os.walk(folder):
        for f in files:
            found.setdefault(f.lower(), os.path.join(root, f))
    for img in bpy.data.images:
        if img.source != "FILE" or not img.filepath:
            continue
        current = bpy.path.abspath(img.filepath)
        if os.path.exists(current):
            continue
        hit = found.get(os.path.basename(current.replace("\\", "/")).lower())
        if hit:
            img.filepath = hit
            img.reload()
            print(f"[unify] relinked image {img.name} -> {hit}")
        else:
            print(f"[unify] image {img.name} is missing and was not found near the source")


def load_source(path: str):
    if path.lower().endswith(".blend"):
        bpy.ops.wm.open_mainfile(filepath=path)
    else:
        bpy.ops.wm.read_factory_settings(use_empty=True)
        if path.lower().endswith(".fbx"):
            bpy.ops.import_scene.fbx(filepath=path)
        else:
            bpy.ops.import_scene.gltf(filepath=path)
    relink_images(os.path.dirname(os.path.abspath(path)))
    arms = [o for o in bpy.data.objects if o.type == "ARMATURE"]
    if not arms:
        raise SystemExit("No armature in the source: use Tools/rig_to_shared_skeleton.py for unrigged meshes.")
    arm = max(arms, key=lambda a: len(a.data.bones))
    meshes = [o for o in bpy.data.objects if o.type == "MESH" and any(m.type == "ARMATURE" and m.object == arm for m in o.modifiers)]
    if not meshes:
        meshes = [o for o in bpy.data.objects if o.type == "MESH" and o.parent == arm]
    arm.data.pose_position = "REST"
    # Bring it to metres: Max and some packs work in centimetres, so a 2 m monster arrives 200 units tall.
    bpy.context.view_layer.update()
    zs = [(arm.matrix_world @ b.head_local).z for b in arm.data.bones]
    raw = max(zs) - min(zs)
    # Packs disagree about units (cm, m, Max units), so every model is scaled to TARGET_HEIGHT instead of guessing.
    # Bones stop at the head joint, about 88% of full height on a biped.
    scale = TARGET_HEIGHT * 0.88 / max(raw, 1e-4)
    if scale != 1.0:
        for o in [arm] + meshes:
            if o.parent is None or o.parent not in [arm] + meshes:
                o.matrix_world = Matrix.Scale(scale, 4) @ o.matrix_world
        bpy.context.view_layer.update()
        print(f"[unify] scaled x{scale:g} (bones spanned {raw:.2f} units)")
    for o in [arm] + meshes:
        o.animation_data_clear() if o.animation_data else None
    # Shape keys block applying modifiers later; keep the basis only.
    for m in meshes:
        if m.data.shape_keys:
            m.shape_key_clear()
    return arm, meshes


def joint_positions(arm, mapped: dict[str, str]) -> dict[str, Vector]:
    pos: dict[str, Vector] = {}
    for src, ours in mapped.items():
        pos[ours] = (arm.matrix_world @ arm.data.bones[src].head_local).copy()
    # A Hips with no pelvis: the thighs' midpoint, a little up.
    if "Hips" not in pos and "LeftUpLeg" in pos and "RightUpLeg" in pos:
        pos["Hips"] = (pos["LeftUpLeg"] + pos["RightUpLeg"]) / 2 + Vector((0, 0, 0.03))
    height = max(v.z for v in pos.values()) - min(v.z for v in pos.values())
    up = Vector((0, 0, 1))
    if "Spine02" not in pos:
        pos["Spine02"] = pos["Hips"] + up * height * 0.07
    top = pos.get("Spine", pos.get("neck", pos["Spine02"] + up * height * 0.25))
    pos.setdefault("Spine", top if "Spine" in pos else pos["Spine02"].lerp(pos.get("neck", top), 0.75))
    pos.setdefault("Spine01", pos["Spine02"].lerp(pos["Spine"], 0.5))
    if "neck" not in pos:
        pos["neck"] = pos["Spine"] + up * height * 0.05
    if "Head" not in pos:
        pos["Head"] = pos["neck"] + up * height * 0.05
    for side in ("Left", "Right"):
        if side + "Shoulder" not in pos and side + "Arm" in pos:
            pos[side + "Shoulder"] = pos["Spine"].lerp(pos[side + "Arm"], 0.25)
        for a, b in (("Arm", "Shoulder"),):
            pass
        if side + "ToeBase" not in pos and side + "Foot" in pos:
            pos[side + "ToeBase"] = pos[side + "Foot"] + Vector((0, -height * 0.06, -height * 0.04))
    pos["head_end"] = pos["Head"] + up * height * 0.12
    return pos


def forward_of(pos: dict[str, Vector]) -> Vector:
    """Which way the model faces: from the feet toward the toes."""
    f = Vector((0, 0, 0))
    for side in ("Left", "Right"):
        if side + "Foot" in pos and side + "ToeBase" in pos:
            f += pos[side + "ToeBase"] - pos[side + "Foot"]
    f.z = 0
    if f.length < 1e-5:
        # No toes: the right hand is on the -X side when facing -Y.
        if "LeftHand" in pos and "RightHand" in pos:
            r = pos["RightHand"] - pos["LeftHand"]
            r.z = 0
            f = Vector((r.y, -r.x, 0))  # right x up = forward
    return f.normalized() if f.length > 1e-5 else Vector((0, -1, 0))


def build_armature(pos: dict[str, Vector], height: float):
    """Our skeleton, its joints moved to pos, each bone keeping our roll."""
    # Read our bones' orientation from the crawler file, in a scratch scene.
    before = set(bpy.data.objects)
    bpy.ops.import_scene.fbx(filepath=CRAWLER_FBX)
    new = [o for o in bpy.data.objects if o not in before]
    ref = next(o for o in new if o.type == "ARMATURE")
    for o in new:
        if o.type == "MESH":
            bpy.data.objects.remove(o, do_unlink=True)
    bpy.ops.object.select_all(action="DESELECT")
    ref.select_set(True)
    bpy.context.view_layer.objects.active = ref
    bpy.ops.object.transform_apply(location=True, rotation=True, scale=True)
    ref_z = {}
    ref_dir = {}
    ref_len = {}
    bpy.ops.object.mode_set(mode="EDIT")
    for eb in ref.data.edit_bones:
        ref_z[eb.name] = eb.z_axis.copy()
        ref_dir[eb.name] = (eb.tail - eb.head).normalized()
        ref_len[eb.name] = (eb.tail - eb.head).length
    ref_height = max(eb.head.z for eb in ref.data.edit_bones) - min(eb.head.z for eb in ref.data.edit_bones)
    for name, _ in OURS:
        eb = ref.data.edit_bones.get(name)
        if eb is None:
            continue
        head = pos.get(name)
        if head is None:
            continue
        eb.use_connect = False
        eb.head = head
        child = CHILD.get(name)
        # The hips keep our skeleton's own direction (the body's facing hangs off it), and so does any bone whose
        # child joint sits almost on top of it (a Biped's pelvis and first spine joint nearly coincide).
        if name not in TORSO and child and child in pos and (pos[child] - head).length > height * 0.03:
            eb.tail = pos[child]
        else:
            eb.tail = head + ref_dir[name] * ref_len[name] * (height / ref_height)
        eb.align_roll(ref_z[name])
    if "headfront" in ref.data.edit_bones:
        eb = ref.data.edit_bones["headfront"]
        eb.head = pos["Head"]
        eb.tail = pos["Head"] + ref_dir["headfront"] * ref_len["headfront"] * (height / ref_height)
        eb.align_roll(ref_z["headfront"])
    bpy.ops.object.mode_set(mode="OBJECT")
    ref.name = "Armature"
    ref.data.name = "Armature"
    return ref


def rebind(meshes, src_arm, new_arm, mapped):
    for m in meshes:
        bones = {b.name: b for b in src_arm.data.bones}
        idx_to_target = {}
        for vg in m.vertex_groups:
            b = bones.get(vg.name)
            idx_to_target[vg.index] = fold_target(b, mapped) if b else None
        weights: dict[str, dict[int, float]] = {}
        for v in m.data.vertices:
            for g in v.groups:
                t = idx_to_target.get(g.group)
                if t and g.weight > 0:
                    weights.setdefault(t, {})
                    weights[t][v.index] = weights[t].get(v.index, 0.0) + g.weight
        m.vertex_groups.clear()
        for t, vw in weights.items():
            vg = m.vertex_groups.new(name=t)
            for vi, w in vw.items():
                vg.add([vi], min(1.0, w), "REPLACE")
        for mod in list(m.modifiers):
            if mod.type == "ARMATURE":
                m.modifiers.remove(mod)
        world = m.matrix_world.copy()
        m.parent = new_arm
        m.matrix_world = world
        mod = m.modifiers.new("Armature", "ARMATURE")
        mod.object = new_arm


def main():
    src, out, preview = argv()
    arm, meshes = load_source(src)
    bpy.context.view_layer.update()
    mapped = classify(arm)
    # Hips must be the pelvis-level joint: prefer a bone named pelvis over Biped's "Bip001" above it.
    print("[unify] mapped:", ", ".join(f"{k}->{v}" for k, v in mapped.items()))
    missing = [n for n, _ in OURS if n not in mapped.values()]
    print("[unify] synthesised:", ", ".join(missing))
    pos = joint_positions(arm, mapped)
    height = max(v.z for v in pos.values()) - min(min(v.z for v in pos.values()), 0)
    fwd = forward_of(pos)
    new_arm = build_armature(pos, height)
    rebind(meshes, arm, new_arm, mapped)
    bpy.data.objects.remove(arm, do_unlink=True)
    for o in list(bpy.data.objects):
        if o.type not in ("MESH", "ARMATURE"):
            bpy.data.objects.remove(o, do_unlink=True)
    # Face -Y, feet on the floor at the origin.
    yaw = math.atan2(fwd.x, -fwd.y)  # angle from -Y to fwd
    turn = Matrix.Rotation(-yaw, 4, "Z")
    for o in [new_arm] + meshes:
        if o.parent is None:
            o.matrix_world = turn @ o.matrix_world
    bpy.context.view_layer.update()
    low = min((m.matrix_world @ Vector(c)).z for m in meshes for c in m.bound_box)
    hips = new_arm.matrix_world @ new_arm.data.bones["Hips"].head_local
    new_arm.location -= Vector((hips.x, hips.y, low))
    bpy.context.view_layer.update()
    print(f"[unify] faced {math.degrees(yaw):.0f} deg round to -Y; height {height:.2f} m; {len(meshes)} mesh(es)")

    if preview:
        render_preview(new_arm, meshes, preview, height)

    # Export the project's way: centimetres, no leaf bones, "Armature" as the root object.
    scene = bpy.context.scene
    scene.unit_settings.system = "METRIC"
    scene.unit_settings.scale_length = 0.01
    for o in [new_arm] + meshes:
        o.select_set(True)
    new_arm.scale = (100, 100, 100)
    new_arm.location *= 100
    bpy.context.view_layer.objects.active = new_arm
    bpy.ops.object.select_all(action="DESELECT")
    new_arm.select_set(True)
    for m in meshes:
        m.select_set(True)
    bpy.ops.object.transform_apply(location=False, rotation=True, scale=True)
    bpy.ops.export_scene.fbx(filepath=out, use_selection=True, object_types={"ARMATURE", "MESH"}, add_leaf_bones=False,
                             bake_anim=False, apply_unit_scale=True, apply_scale_options="FBX_SCALE_NONE",
                             mesh_smooth_type="FACE", path_mode="COPY", embed_textures=True)
    print("[unify] wrote", out)


def render_preview(arm, meshes, png, height):
    """Three frames of a crawler walk on the new rig, plus its rest pose. Unreal plays a clip by setting each bone's
    absolute local rotation (its translation is retargeted to the body's own proportions), so the preview does the
    same: walk rest-local x walk pose, re-expressed against the new rig's rest."""
    before = set(bpy.data.objects)
    try:
        bpy.ops.import_scene.fbx(filepath=WALK_FBX)
    except Exception as e:  # noqa: BLE001
        print("[unify] no preview clip:", e)
        return
    imported = [o for o in bpy.data.objects if o not in before]
    walk = next((o for o in imported if o.type == "ARMATURE"), None)
    for o in imported:
        if o.type == "MESH":
            bpy.data.objects.remove(o, do_unlink=True)
    if not walk or not walk.animation_data or not walk.animation_data.action:
        print("[unify] the walk clip carried no action")
        return
    act = walk.animation_data.action
    walk.hide_render = True

    def rest_local(a, name):
        b = a.data.bones[name]
        m = b.matrix_local if b.parent is None else b.parent.matrix_local.inverted() @ b.matrix_local
        return m.to_quaternion()

    names = [n for n, _ in OURS if n in arm.data.bones and n in walk.data.bones]
    arm.data.pose_position = "POSE"
    for pb in arm.pose.bones:
        pb.rotation_mode = "QUATERNION"
    scene = bpy.context.scene
    scene.render.engine = "BLENDER_WORKBENCH"
    scene.display.shading.light = "STUDIO"
    scene.display.shading.color_type = "TEXTURE"
    scene.render.resolution_x, scene.render.resolution_y = 520, 640
    cam_data = bpy.data.cameras.new("Cam")
    cam = bpy.data.objects.new("Cam", cam_data)
    scene.collection.objects.link(cam)
    scene.camera = cam
    cam.location = Vector((height * 1.5, -height * 2.3, height * 0.7))
    direction = Vector((0, 0, height * 0.5)) - cam.location
    cam.rotation_euler = direction.to_track_quat("-Z", "Y").to_euler()
    stem, ext = os.path.splitext(png)
    arm.data.pose_position = "REST"
    scene.render.filepath = f"{stem}_rest{ext}"
    bpy.ops.render.render(write_still=True)
    arm.data.pose_position = "POSE"
    lo, hi = int(act.frame_range[0]), int(act.frame_range[1])
    for i, f in enumerate([lo, lo + (hi - lo) // 3, lo + 2 * (hi - lo) // 3]):
        scene.frame_set(f)
        for n in names:
            wp = walk.pose.bones[n]
            absolute = rest_local(walk, n) @ wp.matrix_basis.to_quaternion()
            arm.pose.bones[n].rotation_quaternion = rest_local(arm, n).inverted() @ absolute
        bpy.context.view_layer.update()
        scene.render.filepath = f"{stem}_{i}{ext}"
        bpy.ops.render.render(write_still=True)
    for pb in arm.pose.bones:
        pb.rotation_quaternion = (1, 0, 0, 0)
    bpy.data.objects.remove(walk, do_unlink=True)
    print("[unify] preview frames written next to", png)


main()
