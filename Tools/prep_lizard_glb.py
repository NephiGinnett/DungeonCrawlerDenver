"""Hiss Cranson's lizard body, as a static .glb with its four PBR maps (2026-11-25).

The download (Downloads/lizard-character.zip, unzipped) is an unrigged FBX plus loose BaseColor / Normal / Roughness /
Metallic PNGs. This builds one material from the PNGs, scales the body to --height metres (it is sculpted hunched,
so this is the top of the hunch), stands it on z = 0 facing -Y, and writes a .glb with the images embedded for
Tools/import_glb_pbr.py.

    D:/Blender/blender.exe --background --python Tools/prep_lizard_glb.py -- <lizardCharacter.fbx> <textures dir> <out.glb> [--height 1.95]
"""
import os
import sys

import bpy
from mathutils import Vector


def main():
    argv = sys.argv[sys.argv.index("--") + 1:]
    src, tex, dst = argv[0], argv[1], argv[2]
    height = float(argv[argv.index("--height") + 1]) if "--height" in argv else 1.95
    bpy.ops.wm.read_factory_settings(use_empty=True)
    bpy.ops.import_scene.fbx(filepath=src)
    body = next(o for o in bpy.context.scene.objects if o.type == "MESH")
    for o in [o for o in bpy.context.scene.objects if o is not body]:
        bpy.data.objects.remove(o, do_unlink=True)
    body.parent = None

    mat = bpy.data.materials.new("Hiss_Lizard")
    mat.use_nodes = True
    nodes, links = mat.node_tree.nodes, mat.node_tree.links
    bsdf = next(n for n in nodes if n.type == "BSDF_PRINCIPLED")

    def image(name, colour):
        path = next((os.path.join(tex, f) for f in os.listdir(tex) if name.lower() in f.lower()), None)
        if not path:
            return None
        node = nodes.new("ShaderNodeTexImage")
        node.image = bpy.data.images.load(path)
        node.image.colorspace_settings.name = "sRGB" if colour else "Non-Color"
        return node

    if (n := image("BaseColor", True)):
        links.new(n.outputs["Color"], bsdf.inputs["Base Color"])
    if (n := image("Roughness", False)):
        links.new(n.outputs["Color"], bsdf.inputs["Roughness"])
    if (n := image("Metallic", False)):
        links.new(n.outputs["Color"], bsdf.inputs["Metallic"])
    if (n := image("Normal", False)):
        nm = nodes.new("ShaderNodeNormalMap")
        links.new(n.outputs["Color"], nm.inputs["Color"])
        links.new(nm.outputs["Normal"], bsdf.inputs["Normal"])
    body.data.materials.clear()
    body.data.materials.append(mat)

    bpy.context.view_layer.objects.active = body
    body.select_set(True)
    bpy.ops.object.transform_apply(location=True, rotation=True, scale=True)
    pts = [body.matrix_world @ Vector(c) for c in body.bound_box]
    lo = Vector((min(p.x for p in pts), min(p.y for p in pts), min(p.z for p in pts)))
    hi = Vector((max(p.x for p in pts), max(p.y for p in pts), max(p.z for p in pts)))
    k = height / (hi.z - lo.z)
    body.location = Vector((-(lo.x + hi.x) / 2 * k, -(lo.y + hi.y) / 2 * k, -lo.z * k))
    body.scale = (k, k, k)
    bpy.ops.object.transform_apply(location=True, rotation=False, scale=True)
    print(f"[lizard] scaled x{k:.4f}: {(hi.x - lo.x) * k:.2f} x {(hi.y - lo.y) * k:.2f} x {height:.2f} m")
    bpy.ops.export_scene.gltf(filepath=dst, export_format="GLB", export_image_format="AUTO")
    print(f"[lizard] wrote {dst}")


main()
