"""
Print each mesh's material slots and each material's base-color image, for
diagnosing an import that came out with mismatched or shared textures.

USAGE
    blender --background --python Tools/inspect_glb_materials.py -- <src.glb>
"""

import bpy
import sys

argv = sys.argv[sys.argv.index("--") + 1:]
src = argv[0]

bpy.ops.wm.read_factory_settings(use_empty=True)
bpy.ops.import_scene.gltf(filepath=src)

for obj in bpy.data.objects:
    if obj.type != 'MESH':
        continue
    print("MESH", obj.name, "slots:", len(obj.material_slots))
    for i, slot in enumerate(obj.material_slots):
        mat = slot.material
        if not mat:
            print("  slot %d: <none>" % i)
            continue
        images = []
        if mat.use_nodes:
            for node in mat.node_tree.nodes:
                if node.type == 'TEX_IMAGE' and node.image:
                    images.append(node.image.name)
        print("  slot %d: material=%s images=%s" % (i, mat.name, images))
