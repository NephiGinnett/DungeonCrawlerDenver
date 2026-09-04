"""
Render a quick front-view preview of a .glb's mesh to PNG, for visual inspection
without an Unreal session (no MCP CaptureAssetImage available).

USAGE
    blender --background --python Tools/render_glb_preview.py -- <src.glb> <dst.png>
"""

import bpy
import sys
import math

argv = sys.argv[sys.argv.index("--") + 1:]
src, dst = argv[0], argv[1]

bpy.ops.wm.read_factory_settings(use_empty=True)
bpy.ops.import_scene.gltf(filepath=src)

# Drop any stray non-character junk mesh (e.g. Meshy's reference Icosphere).
junk = bpy.data.objects.get("Icosphere")
if junk is not None:
    bpy.data.objects.remove(junk, do_unlink=True)

meshes = [o for o in bpy.data.objects if o.type == 'MESH']
if not meshes:
    print("NO_MESH")
    raise SystemExit(1)

import mathutils
all_min = mathutils.Vector((1e9, 1e9, 1e9))
all_max = mathutils.Vector((-1e9, -1e9, -1e9))
for obj in meshes:
    for c in obj.bound_box:
        wc = obj.matrix_world @ mathutils.Vector(c)
        all_min.x, all_min.y, all_min.z = min(all_min.x, wc.x), min(all_min.y, wc.y), min(all_min.z, wc.z)
        all_max.x, all_max.y, all_max.z = max(all_max.x, wc.x), max(all_max.y, wc.y), max(all_max.z, wc.z)

center = (all_min + all_max) / 2
size = all_max - all_min
radius = max(size.x, size.y, size.z)

# Light
light_data = bpy.data.lights.new(name="Sun", type='SUN')
light_data.energy = 3.0
light_obj = bpy.data.objects.new(name="Sun", object_data=light_data)
bpy.context.collection.objects.link(light_obj)
light_obj.rotation_euler = (math.radians(60), 0, math.radians(30))

light_data2 = bpy.data.lights.new(name="Fill", type='SUN')
light_data2.energy = 1.2
light_obj2 = bpy.data.objects.new(name="Fill", object_data=light_data2)
bpy.context.collection.objects.link(light_obj2)
light_obj2.rotation_euler = (math.radians(60), 0, math.radians(-150))

# Camera, front-ish 3/4 view
cam_data = bpy.data.cameras.new("Cam")
cam_obj = bpy.data.objects.new("Cam", cam_data)
bpy.context.collection.objects.link(cam_obj)
dist = radius * 2.2
cam_obj.location = (center.x + dist * 0.6, center.y - dist * 0.9, center.z + size.z * 0.15)
direction = center - cam_obj.location
rot_quat = direction.to_track_quat('-Z', 'Y')
cam_obj.rotation_euler = rot_quat.to_euler()
bpy.context.scene.camera = cam_obj

scene = bpy.context.scene
try:
    scene.render.engine = 'BLENDER_EEVEE_NEXT'
except Exception:
    try:
        scene.render.engine = 'BLENDER_EEVEE'
    except Exception:
        scene.render.engine = 'CYCLES'
scene.render.resolution_x = 768
scene.render.resolution_y = 1024
scene.render.filepath = dst
scene.render.image_settings.file_format = 'PNG'

bpy.ops.render.render(write_still=True)
print("RENDERED:", dst)
