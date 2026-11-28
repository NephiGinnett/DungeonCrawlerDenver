"""Merge a glTF scene into one mesh, pieces kept where they stand, for import as one static mesh (2026-11-25).

A Fab scene (.glb) is often dozens of separate nodes. Imported straight into Unreal, each becomes its own asset at the
origin and the room has to be put back together by hand. This bakes every node's transform into its geometry, joins
them into one object (one material slot per material, textures kept), puts the floor's centre at the origin, and writes
a .glb with the images embedded. Then Tools/import_glb_pbr.py imports it as one static mesh with full PBR.

    D:/Blender/blender.exe --background --python Tools/merge_glb_scene.py -- <in.glb> <out.glb> [--width metres]

--width scales it so the larger floor side is that many metres (glTF is in metres; many Fab scenes are not to scale).
"""
import sys

import bpy
from mathutils import Vector


def main():
    argv = sys.argv[sys.argv.index("--") + 1:]
    src, dst = argv[0], argv[1]
    width = float(argv[argv.index("--width") + 1]) if "--width" in argv else 0.0

    bpy.ops.wm.read_factory_settings(use_empty=True)
    bpy.ops.import_scene.gltf(filepath=src)
    meshes = [o for o in bpy.context.scene.objects if o.type == "MESH"]
    print(f"[merge] {len(meshes)} mesh objects")

    # Bake parents and transforms into the geometry, then join.
    bpy.ops.object.select_all(action="DESELECT")
    for o in meshes:
        o.select_set(True)
    bpy.context.view_layer.objects.active = meshes[0]
    bpy.ops.object.parent_clear(type="CLEAR_KEEP_TRANSFORM")
    for o in meshes:
        if o.data.users > 1:
            o.data = o.data.copy()  # instanced meshes must be single-user to apply transforms
    bpy.ops.object.transform_apply(location=True, rotation=True, scale=True)
    bpy.ops.object.join()
    room = bpy.context.view_layer.objects.active
    room.name = "Room"
    for o in [o for o in bpy.context.scene.objects if o.type != "MESH"]:
        bpy.data.objects.remove(o, do_unlink=True)

    # Floor centre to the origin, lowest point at z = 0.
    corners = [room.matrix_world @ Vector(c) for c in room.bound_box]
    lo = Vector((min(c.x for c in corners), min(c.y for c in corners), min(c.z for c in corners)))
    hi = Vector((max(c.x for c in corners), max(c.y for c in corners), max(c.z for c in corners)))
    size = hi - lo
    print(f"[merge] raw size {size.x:.2f} x {size.y:.2f} x {size.z:.2f}")
    room.location = Vector((-(lo.x + hi.x) / 2, -(lo.y + hi.y) / 2, -lo.z))
    bpy.ops.object.transform_apply(location=True, rotation=False, scale=False)
    if width > 0:
        k = width / max(size.x, size.y)
        room.scale = (k, k, k)
        bpy.ops.object.transform_apply(location=False, rotation=False, scale=True)
        print(f"[merge] scaled x{k:.3f} to {size.x * k:.2f} x {size.y * k:.2f} x {size.z * k:.2f} m")

    bpy.ops.export_scene.gltf(filepath=dst, export_format="GLB", use_selection=False, export_apply=True,
                              export_image_format="AUTO")
    print(f"[merge] wrote {dst} ({len(room.data.materials)} materials)")


main()
