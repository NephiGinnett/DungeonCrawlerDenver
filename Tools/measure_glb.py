"""
Print the world-space bounding box of every mesh in a .glb, in metres.

WHY THIS EXISTS
    Meshy normalizes each generated asset to roughly the same bounding-box size
    regardless of what the thing actually is, so a prop arrives at character
    scale and has to be corrected on export (see Tools/glb_to_fbx.py's scale
    argument). Guessing that correction factor means importing, measuring in
    Unreal, deleting and re-importing. Measuring here first skips that loop.

USAGE
    blender --background --python Tools/measure_glb.py -- <src.glb>
"""

import bpy
import sys
import mathutils

argv = sys.argv[sys.argv.index("--") + 1:]
src = argv[0]

bpy.ops.wm.read_factory_settings(use_empty=True)
bpy.ops.import_scene.gltf(filepath=src)

for obj in bpy.data.objects:
    if obj.type != 'MESH':
        continue

    corners = [obj.matrix_world @ mathutils.Vector(c) for c in obj.bound_box]
    xs = [v.x for v in corners]
    ys = [v.y for v in corners]
    zs = [v.z for v in corners]

    print("MESH %s size_m X=%.3f Y=%.3f Z=%.3f" % (
        obj.name, max(xs) - min(xs), max(ys) - min(ys), max(zs) - min(zs)))
