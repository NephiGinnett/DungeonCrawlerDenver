"""
Bind the shared skeleton to a character's mesh, then correct its joint
positions to match that character's OWN original (Meshy-fitted) skeleton
exactly - not by arithmetic, by copying absolute world-space joint
coordinates directly. This is the technique verified to work cleanly on
Marcel (2026-08-25 overnight session) after a cruder length-arithmetic
attempt produced visibly broken deformation.
"""
import bpy, sys, os, mathutils

argv = sys.argv[sys.argv.index("--") + 1:]
cj_skeleton_src, own_skeleton_src, out_blend, render_prefix = argv[0], argv[1], argv[2], argv[3]

bones_of_interest = ["Hips", "LeftUpLeg", "LeftLeg", "LeftFoot", "LeftToeBase",
                      "RightUpLeg", "RightLeg", "RightFoot", "RightToeBase",
                      "LeftArm", "LeftForeArm", "LeftHand",
                      "RightArm", "RightForeArm", "RightHand", "Spine02"]

bpy.ops.wm.read_factory_settings(use_empty=True)

# 1. Shared skeleton (CJ-derived)
bpy.ops.import_scene.fbx(filepath=cj_skeleton_src)
bpy.data.objects["Armature"].name = "SharedSkeleton"
for o in list(bpy.data.objects):
    if o.type == 'MESH':
        bpy.data.objects.remove(o, do_unlink=True)

# 2. Character's own mesh + own skeleton (same file)
bpy.ops.import_scene.fbx(filepath=own_skeleton_src)
target_mesh = max((o for o in bpy.data.objects if o.type == 'MESH'), key=lambda o: len(o.data.vertices))
target_mesh.name = "TargetMesh"
own_arm = next(o for o in bpy.data.objects if o.type == 'ARMATURE' and o.name != "SharedSkeleton")

# 3. Read ground-truth joint positions off the character's own skeleton BEFORE touching it
bpy.context.view_layer.update()
positions = {}
for name in bones_of_interest:
    b = own_arm.pose.bones.get(name)
    if b:
        head_w = own_arm.matrix_world @ b.head
        tail_w = own_arm.matrix_world @ b.tail
        positions[name] = (tuple(head_w), tuple(tail_w))

# 4. Detach mesh from its own armature, delete that armature and any throwaway mesh
bpy.context.view_layer.objects.active = target_mesh
target_mesh.select_set(True)
bpy.ops.object.parent_clear(type='CLEAR_KEEP_TRANSFORM')
for m in list(target_mesh.modifiers):
    if m.type == 'ARMATURE':
        target_mesh.modifiers.remove(m)
bpy.data.objects.remove(own_arm, do_unlink=True)
for o in list(bpy.data.objects):
    if o.type == 'MESH' and o is not target_mesh:
        bpy.data.objects.remove(o, do_unlink=True)

# 5. Bind to the shared skeleton with automatic weights (starting point, will be
#    correctly-proportioned once step 6 moves the joints)
shared = bpy.data.objects["SharedSkeleton"]
shared.show_in_front = True
shared.data.display_type = 'STICK'
bpy.ops.object.select_all(action='DESELECT')
target_mesh.select_set(True)
shared.select_set(True)
bpy.context.view_layer.objects.active = shared
bpy.ops.object.parent_set(type='ARMATURE_AUTO')

# 6. Apply the ground-truth joint positions
bpy.ops.object.mode_set(mode='EDIT')
eb = shared.data.edit_bones
inv = shared.matrix_world.inverted()
applied = []
for name, (head_w, tail_w) in positions.items():
    b = eb.get(name)
    if b:
        b.head = inv @ mathutils.Vector(head_w)
        b.tail = inv @ mathutils.Vector(tail_w)
        applied.append(name)
bpy.ops.object.mode_set(mode='OBJECT')
print("Applied ground-truth joints to:", applied)

# 7. Render bind pose + a posed test for verification
import math
cam_data = bpy.data.cameras.new("Cam")
cam = bpy.data.objects.new("Cam", cam_data)
bpy.context.scene.collection.objects.link(cam)
cam.location = (0, -3.2, 0.9)
cam.rotation_euler = (math.radians(85), 0, 0)
bpy.context.scene.camera = cam

light_data = bpy.data.lights.new("Sun", type='SUN')
light = bpy.data.objects.new("Sun", light_data)
bpy.context.scene.collection.objects.link(light)
light.rotation_euler = (math.radians(45), 0, math.radians(45))
light.data.energy = 3.0

scene = bpy.context.scene
scene.render.engine = 'BLENDER_EEVEE_NEXT' if hasattr(bpy.types, 'BLENDER_EEVEE_NEXT') else 'BLENDER_EEVEE'
scene.render.resolution_x = 640
scene.render.resolution_y = 640

scene.render.filepath = render_prefix + "_bind.png"
bpy.ops.render.render(write_still=True)

bpy.context.view_layer.objects.active = shared
bpy.ops.object.mode_set(mode='POSE')
def rotate_bone(name, euler_deg):
    pb = shared.pose.bones.get(name)
    if pb:
        pb.rotation_mode = 'XYZ'
        pb.rotation_euler = tuple(math.radians(d) for d in euler_deg)
rotate_bone("LeftUpLeg", (60, 0, 0))
rotate_bone("LeftLeg", (-70, 0, 0))
rotate_bone("RightArm", (0, 0, -70))
rotate_bone("LeftArm", (0, 0, 70))
bpy.context.view_layer.update()
scene.render.filepath = render_prefix + "_posed.png"
bpy.ops.render.render(write_still=True)

# reset to rest pose before saving
bpy.ops.pose.select_all(action='SELECT')
bpy.ops.pose.transforms_clear()
bpy.ops.object.mode_set(mode='OBJECT')

os.makedirs(os.path.dirname(out_blend), exist_ok=True)
bpy.ops.wm.save_as_mainfile(filepath=out_blend)
print("SAVED:", out_blend)
