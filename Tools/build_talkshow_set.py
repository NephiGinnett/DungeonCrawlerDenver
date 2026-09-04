"""
Builds a quick, low-effort "late-night talk show" set-dressing kit as a single
Blender Collection ("TalkShowSet"), saved to its own standalone .blend file.

Pure mood dressing for Zoe's rig-fitting sessions -- a desk, a couple of
chairs, a skyline-silhouette backdrop, and a glowing ON AIR sign. All objects
live inside one collection so it can be appended (File > Append > Collection)
into any of the rig_fit_check_*.blend files without touching whatever is
already in that scene (skeleton, mesh, camera, existing lights all untouched).

Run headless:
    blender --background --python Tools/build_talkshow_set.py -- <output.blend>
"""

import bpy
import sys
import math


def get_output_path():
    argv = sys.argv
    if "--" in argv:
        argv = argv[argv.index("--") + 1:]
    if not argv:
        raise SystemExit("Usage: blender --background --python build_talkshow_set.py -- <output.blend>")
    return argv[0]


def reset_scene():
    bpy.ops.wm.read_factory_settings(use_empty=True)


def make_material(name, color, emission_strength=0.0, roughness=0.6, metallic=0.0):
    mat = bpy.data.materials.new(name)
    mat.use_nodes = True
    bsdf = mat.node_tree.nodes.get("Principled BSDF")
    bsdf.inputs["Base Color"].default_value = (*color, 1.0)
    bsdf.inputs["Roughness"].default_value = roughness
    bsdf.inputs["Metallic"].default_value = metallic
    if emission_strength > 0.0:
        bsdf.inputs["Emission Color"].default_value = (*color, 1.0)
        bsdf.inputs["Emission Strength"].default_value = emission_strength
    return mat


def add_cube(name, collection, location, scale, material=None, rotation=(0, 0, 0)):
    bpy.ops.mesh.primitive_cube_add(size=1.0, location=location)
    obj = bpy.context.active_object
    obj.name = name
    obj.scale = scale
    obj.rotation_euler = rotation
    if material:
        obj.data.materials.append(material)
    relink(obj, collection)
    return obj


def add_cylinder(name, collection, location, radius, depth, material=None, rotation=(0, 0, 0)):
    bpy.ops.mesh.primitive_cylinder_add(radius=radius, depth=depth, location=location)
    obj = bpy.context.active_object
    obj.name = name
    obj.rotation_euler = rotation
    if material:
        obj.data.materials.append(material)
    relink(obj, collection)
    return obj


def relink(obj, collection):
    for c in list(obj.users_collection):
        c.objects.unlink(obj)
    collection.objects.link(obj)


def build_desk(collection, mat_wood, mat_panel, origin):
    ox, oy, oz = origin
    # Desktop slab
    add_cube("TalkShow_Desk_Top", collection,
              (ox, oy, oz + 0.95), (1.9, 0.55, 0.04), mat_wood)
    # Front modesty panel
    add_cube("TalkShow_Desk_Panel", collection,
              (ox, oy - 0.5, oz + 0.475), (1.85, 0.03, 0.9), mat_panel)
    # Two side legs
    for side in (-1, 1):
        add_cube(f"TalkShow_Desk_Leg_{'L' if side < 0 else 'R'}", collection,
                  (ox + side * 1.75, oy, oz + 0.45), (0.08, 0.5, 0.9), mat_wood)


def build_chair(name, collection, mat_seat, mat_frame, location, facing_z_deg=0):
    x, y, z = location
    rot = (0, 0, math.radians(facing_z_deg))
    # Central swivel column
    add_cylinder(f"{name}_Base", collection, (x, y, z + 0.22), 0.14, 0.44, mat_frame)
    # Seat
    add_cube(f"{name}_Seat", collection, (x, y, z + 0.46), (0.45, 0.45, 0.06), mat_seat, rot)
    # Backrest, angled slightly for a relaxed talk-show lean
    back_offset_x = -0.42 * math.sin(math.radians(facing_z_deg))
    back_offset_y = -0.42 * math.cos(math.radians(facing_z_deg))
    add_cube(f"{name}_Back", collection,
              (x + back_offset_x, y + back_offset_y, z + 0.85),
              (0.45, 0.05, 0.5), mat_seat, (math.radians(-8), 0, rot[2]))


def build_backdrop(collection, mat_curtain, mat_skyline, mat_window, origin, width=10.0, y=-3.2):
    ox, oy, oz = origin
    # Curtain wall
    add_cube("TalkShow_Backdrop_Curtain", collection,
              (ox, oy + y, oz + 2.2), (width / 2, 0.08, 2.4), mat_curtain)

    # Skyline silhouette strip in front of the curtain
    import random
    random.seed(7)
    n = 14
    x_start = ox - width / 2 + 0.4
    x_end = ox + width / 2 - 0.4
    for i in range(n):
        bw = random.uniform(0.35, 0.7)
        bh = random.uniform(0.8, 2.6)
        bx = x_start + (x_end - x_start) * (i / (n - 1))
        bldg = add_cube(f"TalkShow_Skyline_{i:02d}", collection,
                          (bx, oy + y + 0.35, oz + bh / 2), (bw / 2, 0.12, bh / 2), mat_skyline)
        # A couple of lit windows per building, only on taller ones
        if bh > 1.3:
            for w in range(random.randint(1, 3)):
                wx = bx + random.uniform(-bw * 0.3, bw * 0.3)
                wz = oz + random.uniform(0.3, bh - 0.3)
                add_cube(f"TalkShow_Window_{i:02d}_{w}", collection,
                          (wx, oy + y + 0.42, wz), (0.04, 0.02, 0.06), mat_window)


def build_on_air_sign(collection, mat_frame, mat_glow, location):
    x, y, z = location
    # Backing frame
    add_cube("TalkShow_OnAirSign_Frame", collection, (x, y, z), (0.55, 0.06, 0.28), mat_frame)

    # "ON AIR" text, extruded, mounted on the FRONT face of the frame (+Y side,
    # toward the character/camera) so it isn't hidden behind its own backing
    text_align = 'CENTER'
    bpy.ops.object.text_add(location=(x, y + 0.10, z - 0.08))
    txt = bpy.context.active_object
    txt.name = "TalkShow_OnAirSign_Text"
    txt.data.body = "ON AIR"
    txt.data.size = 0.22
    txt.data.extrude = 0.01
    txt.data.align_x = text_align
    txt.rotation_euler = (math.radians(90), 0, math.radians(180))
    txt.data.materials.append(mat_glow)
    relink(txt, collection)


def build_floor(collection, mat_floor, origin, size=7.0):
    ox, oy, oz = origin
    add_cube("TalkShow_Floor", collection, (ox, oy - 1.0, oz - 0.02), (size / 2, size / 2, 0.02), mat_floor)


def build_lighting(collection, origin):
    ox, oy, oz = origin

    key = bpy.data.lights.new("TalkShow_KeyLight", type='AREA')
    key.energy = 900
    key.color = (1.0, 0.85, 0.65)
    key.size = 2.5
    key_obj = bpy.data.objects.new("TalkShow_KeyLight", key)
    key_obj.location = (ox - 1.5, oy + 1.5, oz + 2.8)
    key_obj.rotation_euler = (math.radians(55), 0, math.radians(-35))
    collection.objects.link(key_obj)

    rim = bpy.data.lights.new("TalkShow_RimLight", type='AREA')
    rim.energy = 250
    rim.color = (0.55, 0.7, 1.0)
    rim.size = 1.5
    rim_obj = bpy.data.objects.new("TalkShow_RimLight", rim)
    rim_obj.location = (ox + 1.0, oy - 3.0, oz + 2.0)
    rim_obj.rotation_euler = (math.radians(70), 0, math.radians(150))
    collection.objects.link(rim_obj)

    # Soft overhead fill so the working area (character + desk) stays legible
    # while the backdrop stays moody -- this is a WORK scene, not a screenshot.
    fill = bpy.data.lights.new("TalkShow_FillLight", type='AREA')
    fill.energy = 350
    fill.color = (0.85, 0.9, 1.0)
    fill.size = 3.0
    fill_obj = bpy.data.objects.new("TalkShow_FillLight", fill)
    fill_obj.location = (ox, oy + 0.5, oz + 3.5)
    fill_obj.rotation_euler = (0, 0, 0)
    collection.objects.link(fill_obj)

    sign_spot = bpy.data.lights.new("TalkShow_SignSpot", type='SPOT')
    sign_spot.energy = 60
    sign_spot.color = (1.0, 0.25, 0.25)
    sign_spot.spot_size = math.radians(45)
    sign_obj = bpy.data.objects.new("TalkShow_SignSpot", sign_spot)
    sign_obj.location = (ox + 2.5, oy - 2.6, oz + 3.2)
    sign_obj.rotation_euler = (math.radians(120), 0, 0)
    collection.objects.link(sign_obj)


def main():
    output_path = get_output_path()
    reset_scene()

    scene_collection = bpy.context.scene.collection
    talkshow = bpy.data.collections.new("TalkShowSet")
    scene_collection.children.link(talkshow)

    mat_wood = make_material("TalkShow_Mat_Wood", (0.28, 0.16, 0.09), roughness=0.35)
    mat_panel = make_material("TalkShow_Mat_Panel", (0.05, 0.05, 0.06), roughness=0.5)
    mat_seat = make_material("TalkShow_Mat_Seat", (0.55, 0.08, 0.1), roughness=0.7)
    mat_frame = make_material("TalkShow_Mat_Frame", (0.03, 0.03, 0.03), roughness=0.4, metallic=0.6)
    mat_curtain = make_material("TalkShow_Mat_Curtain", (0.08, 0.05, 0.18), roughness=0.8)
    mat_skyline = make_material("TalkShow_Mat_Skyline", (0.02, 0.02, 0.035), roughness=0.6)
    mat_window = make_material("TalkShow_Mat_Window", (1.0, 0.85, 0.4), emission_strength=4.0)
    mat_glow = make_material("TalkShow_Mat_OnAirGlow", (1.0, 0.08, 0.08), emission_strength=6.0)
    mat_floor = make_material("TalkShow_Mat_Floor", (0.07, 0.07, 0.08), roughness=0.55)

    # Origin (0,0,0) is left clear -- that's where the character being rigged
    # already stands in the check scenes. Everything here is placed around it.
    origin = (0.0, 0.0, 0.0)

    build_floor(talkshow, mat_floor, origin)
    build_backdrop(talkshow, mat_curtain, mat_skyline, mat_window, origin)
    build_desk(talkshow, mat_wood, mat_panel, (origin[0] - 2.3, origin[1] - 1.6, origin[2]))
    build_chair("TalkShow_HostChair", talkshow, mat_seat, mat_frame,
                (origin[0] - 2.3, origin[1] - 2.35, origin[2]), facing_z_deg=180)
    build_chair("TalkShow_GuestChair", talkshow, mat_seat, mat_frame,
                (origin[0] + 1.4, origin[1] - 1.1, origin[2]), facing_z_deg=-25)
    build_on_air_sign(talkshow, mat_frame, mat_glow,
                       (origin[0] + 2.6, origin[1] - 3.0, origin[2] + 3.0))
    build_lighting(talkshow, origin)

    bpy.ops.wm.save_as_mainfile(filepath=output_path)
    print(f"Talk show set saved to: {output_path}")


if __name__ == "__main__":
    main()
