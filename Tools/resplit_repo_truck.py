"""Rebuild the repo truck's three pieces at FULL detail (2026-11-14).

    blender --background --python Tools/resplit_repo_truck.py

Grok split Meshy's truck (1.6 M vertices) into Body / Boom / Magnet, but decimated it to ~51 k vertices on the
way, which made it look blurry. This keeps Grok's split and pivots and swaps in the full-detail geometry:

  1. Finds the transform from Meshy's unit-sized original to Grok's truck space (scale, a 90-degree turn, offset)
     by fitting the original onto Grok's assembled low-detail pieces (boom and magnet placed at the game's own
     knuckle and magnet offsets, ADCCRover::ApplyWreckerLook).
  2. Gives every original face to whichever low-detail piece is nearest.
  3. Exports each piece around the same pivot Grok used, so the crane animation still lines up.

Writes SourceArt/RepoRally/Wrecker/Full/SM_RepoTruck_{Body,Boom,Magnet}.fbx. Import with Nanite on.
"""
import math
import os

import bmesh
import bpy
import mathutils
from mathutils.bvhtree import BVHTree

P = r"C:/Users/kmett/Documents/Unreal Projects/DungeonCrawlerDenver"
ORIG = P + "/Content/__ExternalObjects__/Unsorted Models/Meshy_AI_cyberpunk_hover_repo_truck_20261114_image-to-3d-texture_fbx/Meshy_AI_cyberpunk_hover_repo_truck_20261114_image-to-3d-texture.fbx"
PIECES = P + "/SourceArt/RepoRally/Wrecker"
OUT = PIECES + "/Full"
os.makedirs(OUT, exist_ok=True)

# Piece pivots in the body's space, from the game (UE cm, body mesh space) -> Blender metres (FBX: UE Y = -Blender Y).
def ue_to_bl(x, y, z):
    return mathutils.Vector((x / 100.0, -y / 100.0, z / 100.0))
PIVOTS = {"Body": mathutils.Vector((0, 0, 0)), "Boom": ue_to_bl(9.8, -186.8, 345.2), "Magnet": ue_to_bl(1.3, -642.0, 295.1)}

bpy.ops.wm.read_factory_settings(use_empty=True)

def import_one(path):
    before = set(bpy.data.objects)
    bpy.ops.import_scene.fbx(filepath=path)
    return [o for o in set(bpy.data.objects) - before if o.type == "MESH"][0]

def world_verts(o):
    m = o.matrix_world
    return [m @ v.co for v in o.data.vertices]

# Grok's low-detail pieces, assembled in body space (world metres).
low = {}
for n in ["Body", "Boom", "Magnet"]:
    o = import_one(f"{PIECES}/SM_RepoTruck_{n}.fbx")
    vs = [v + PIVOTS[n] for v in world_verts(o)]
    tris = [tuple(p.vertices) for p in o.data.polygons]
    low[n] = (vs, tris)
    bpy.data.objects.remove(o)
all_low = [v for vs, _ in low.values() for v in vs]
lo_min = mathutils.Vector([min(v[i] for v in all_low) for i in range(3)])
lo_max = mathutils.Vector([max(v[i] for v in all_low) for i in range(3)])
print("LOW bbox", lo_min, lo_max)

orig = import_one(ORIG)
ov = world_verts(orig)
o_min = mathutils.Vector([min(v[i] for v in ov) for i in range(3)])
o_max = mathutils.Vector([max(v[i] for v in ov) for i in range(3)])
print("ORIG bbox", o_min, o_max)

# Candidate transforms: original +X or -X becomes the truck's long axis (+Y); fit scale and offset by bbox.
bvh_low = BVHTree.FromPolygons(all_low, [t for n in low for t in [tuple(i + sum(len(low[k][0]) for k in list(low)[:list(low).index(n)]) for i in tri) for tri in low[n][1]]])
best = None
for yaw in (90.0, -90.0):
    R = mathutils.Matrix.Rotation(math.radians(yaw), 4, "Z")
    rv = [R @ v for v in ov[::400]]
    r_min = mathutils.Vector([min(v[i] for v in rv) for i in range(3)])
    r_max = mathutils.Vector([max(v[i] for v in rv) for i in range(3)])
    s = ((lo_max - lo_min).x / (r_max - r_min).x + (lo_max - lo_min).y / (r_max - r_min).y + (lo_max - lo_min).z / (r_max - r_min).z) / 3.0
    t = (lo_min + lo_max) / 2.0 - s * (r_min + r_max) / 2.0
    err = 0.0
    for v in rv[::5]:
        hit = bvh_low.find_nearest(s * v + t)
        err += hit[3] if hit[0] else 1.0
    print("TRY yaw", yaw, "scale", round(s, 4), "offset", t, "error", round(err, 3))
    if best is None or err < best[0]:
        best = (err, R, s, t)
_, R, s, t = best
M = mathutils.Matrix.Translation(t) @ mathutils.Matrix.Scale(s, 4) @ R

# Classify faces by the nearest low piece.
bvhs = {n: BVHTree.FromPolygons(low[n][0], low[n][1]) for n in low}
me = orig.data
me.transform(M)
orig.matrix_world = mathutils.Matrix.Identity(4)
assign = []
for poly in me.polygons:
    c = poly.center
    best_n, best_d = "Body", 1e9
    for n, b in bvhs.items():
        hit = b.find_nearest(c)
        if hit[0] is not None and hit[3] < best_d:
            best_n, best_d = n, hit[3]
    assign.append(best_n)
print("FACES", {n: assign.count(n) for n in low})

# Split into three objects and export each around its pivot.
for n in ["Body", "Boom", "Magnet"]:
    o = orig.copy()
    o.data = orig.data.copy()
    bpy.context.collection.objects.link(o)
    bm = bmesh.new()
    bm.from_mesh(o.data)
    bm.faces.ensure_lookup_table()
    drop = [f for f, a in zip(bm.faces, assign) if a != n]
    bmesh.ops.delete(bm, geom=drop, context="FACES")
    bm.to_mesh(o.data)
    bm.free()
    o.data.transform(mathutils.Matrix.Translation(-PIVOTS[n]))
    # Centimetres in the file: with metres, Unreal imported the truck 100x too small.
    o.data.transform(mathutils.Matrix.Scale(100.0, 4))
    o.name = f"SM_RepoTruck_{n}"
    bpy.ops.object.select_all(action="DESELECT")
    o.select_set(True)
    bpy.context.view_layer.objects.active = o
    bpy.ops.export_scene.fbx(filepath=f"{OUT}/SM_RepoTruck_{n}.fbx", use_selection=True, object_types={"MESH"},
        mesh_smooth_type="FACE", path_mode="COPY", embed_textures=False, apply_scale_options="FBX_SCALE_ALL")
    print("EXPORTED", n, len(o.data.vertices), "verts")
