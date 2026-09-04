"""
Print the bone list of the first armature in a .glb, in hierarchy order.

USAGE
    blender --background --python Tools/list_bones.py -- <src.glb>
"""

import bpy
import sys

argv = sys.argv[sys.argv.index("--") + 1:]
src = argv[0]

bpy.ops.wm.read_factory_settings(use_empty=True)
bpy.ops.import_scene.gltf(filepath=src)

arm = next((o for o in bpy.data.objects if o.type == 'ARMATURE'), None)
if arm is None:
    print("NO_ARMATURE")
    raise SystemExit(0)

bones = arm.data.bones
print("BONE_COUNT:", len(bones))
roots = [b for b in bones if b.parent is None]

def walk(b, depth):
    print("  " * depth + b.name)
    for c in b.children:
        walk(c, depth + 1)

for r in roots:
    walk(r, 0)

print("ALL_OBJECTS:", [(o.name, o.type) for o in bpy.data.objects])
