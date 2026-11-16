"""Blender side of the prop-material fix (2026-11-13).

Meshy .glb files leave materials and images unnamed, so Unreal's importer names every
one Material_0 / Image_0..2. Importing several props into one folder made them all share
ONE material (whichever imported last), which is why the goggles, popcorn bucket and
trainer looked black and smeared.

This converts one .glb to FBX with a uniquely named material and textures:

    blender --background --python Tools/fix_prop_materials.py -- <src.glb> <out_dir> <Name>

writes <out_dir>/<Name>.fbx, using M_<Name> and T_<Name>_<Role>.png next to it.
Then import the FBX into /Game/DCC/Equipment/Props/<Name>/ and point the existing
SM_<Name> at M_<Name> (Tools/fix_prop_materials_ue.py does both over MCP).
"""
import sys
from pathlib import Path

import bpy

argv = sys.argv[sys.argv.index("--") + 1:]
src, out_dir, name = argv[0], Path(argv[1]), argv[2]
out_dir.mkdir(parents=True, exist_ok=True)

bpy.ops.wm.read_factory_settings(use_empty=True)
bpy.ops.import_scene.gltf(filepath=src)

roles = {"Base Color": "BaseColor", "Normal": "Normal", "Metallic": "Metallic", "Roughness": "Roughness"}
for i, mat in enumerate(bpy.data.materials):
    mat.name = f"M_{name}" if i == 0 else f"M_{name}_{i}"
    if not mat.node_tree:
        continue
    for node in mat.node_tree.nodes:
        if node.type != "TEX_IMAGE" or not node.image:
            continue
        # Name the image by what it feeds (follow links, through a normal-map node if any).
        role = "Tex"
        for link in node.outputs[0].links + (node.outputs[1].links if len(node.outputs) > 1 else []):
            to = link.to_node
            sock = link.to_socket.name
            if to.type == "NORMAL_MAP":
                role = "Normal"
            elif to.type == "SEPARATE_COLOR" or to.type == "SEPRGB":
                role = "ORM"
            elif sock in roles:
                role = roles[sock]
        img = node.image
        img.name = f"T_{name}_{role}"
        # Packed inside the .glb: write its exact bytes out (no colour transform, which
        # save_render would apply) and point the image at the file, so the FBX export
        # references a texture Unreal can find.
        ext = ".png" if (img.packed_file and img.packed_file.data[1:4] == b"PNG") else ".jpg"
        path = out_dir / f"T_{name}_{role}{ext}"
        if img.packed_file:
            path.write_bytes(img.packed_file.data)
            img.unpack(method="REMOVE")
        img.filepath = str(path)
        img.reload()
        print(f"[fix_prop_materials] texture {path.name}")

bpy.ops.export_scene.fbx(
    filepath=str(out_dir / f"{name}.fbx"),
    use_selection=False,
    object_types={"MESH"},
    mesh_smooth_type="FACE",
    path_mode="ABSOLUTE",
    embed_textures=False,
    bake_space_transform=False,
)
print(f"[fix_prop_materials] wrote {out_dir / (name + '.fbx')}")
