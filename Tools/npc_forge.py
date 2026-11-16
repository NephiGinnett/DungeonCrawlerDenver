"""NPC Forge: consistent, varied background humans from MakeHuman (MPFB2) in Blender (2026-11-14).

Zoe: "We just need a consistent tool to generate nice looking NPCs with small differences."

Every NPC comes from a seed and a role, so the same seed always gives the same person. The body (height, build,
age, proportions, ancestry, face) comes from MPFB's own randomizer, kept to adult ranges. The skin is picked to match
the body. Hair, eyebrows, eye colour and the outfit are drawn from per-role lists of the CC0 MakeHuman asset packs
(installed 2026-11-14 into Blender's MPFB user data; the zips are in D:\\Assets\\MakeHuman\\zips). The rig is MPFB's
"game_engine" rig, which uses Unreal's mannequin bone names, so the mannequin's clips retarget onto it.

Run with Blender's own Python and the user profile (MPFB is a user extension):
  blender --background --python Tools/npc_forge.py -- --roles casual,worker,office --count 8 --seed 100 \
      --preview SourceArt/NPCs/lineup.png [--export SourceArt/NPCs/fbx]

--export writes one FBX per NPC (mesh + game_engine armature, textures embedded) named NPC_<role>_<seed>.fbx,
plus npcs.json describing each one. Re-running with the same arguments reproduces the same people.
"""
import bpy, sys, os, json, math, random, argparse, importlib, zlib

argv = sys.argv[sys.argv.index('--') + 1:] if '--' in sys.argv else []
ap = argparse.ArgumentParser()
ap.add_argument('--roles', default='casual,worker,office')
ap.add_argument('--count', type=int, default=8)
ap.add_argument('--seed', type=int, default=100)
ap.add_argument('--preview', default='')
ap.add_argument('--export', default='')
ap.add_argument('--spacing', type=float, default=0.9)
args = ap.parse_args(argv)

MPFB = 'bl_ext.user_default.mpfb'
HumanService = importlib.import_module(MPFB + '.services.humanservice').HumanService
Rand = importlib.import_module(MPFB + '.services.randomizationservice').RandomizationService

# ---- what each role may wear ------------------------------------------------------------------------------
# Lists of MPFB asset names (folder names under data/clothes). One pick from each group present.
HAIR = {
    'male': ['short01', 'short02', 'short03', 'short04', 'cortu_short_messy_hair', 'culturalibre_hair_01', 'afro01', ''],
    'female': ['bob01', 'bob02', 'ponytail01', 'long01', 'braid01', 'toigo_blunt_bob', 'toigo_curled_under_bob',
               'toigo_inverted_bob', 'rehmanpolanski_hair_bun_brown', 'elvs_french_braid_variation', 'afro01', 'short02'],
}
OUTFITS = {
    'casual': {
        'male': [['male_casualsuit01'], ['male_casualsuit02'], ['male_casualsuit03'], ['male_casualsuit04'],
                 ['male_casualsuit05'], ['male_casualsuit06'], ['namuhekam_male_polo_shirt', 'cortu_cargo_pants'],
                 ['elvs_crude_t-shirt_male', 'toigo_wool_pants'], ['toigo_fisherman_sweater', 'cortu_cargo_pants']],
        'female': [['female_casualsuit01'], ['female_casualsuit02'], ['toigo_basic_tucked_t-shirt', 'toigo_wool_pants'],
                   ['toigo_fisherman_sweater', 'toigo_wool_pants'], ['joepal_crude_t-shirt_female', 'cortu_cargo_pants'],
                   ['toigo_turtleneck_halter_top', 'toigo_harem_pants']],
    },
    'worker': {
        'male': [['male_worksuit01'], ['namuhekam_male_polo_shirt', 'cortu_cargo_pants'], ['elvs_crude_t-shirt_male', 'cortu_cargo_pants']],
        'female': [['joepal_crude_t-shirt_female', 'cortu_cargo_pants'], ['toigo_fisherman_sweater', 'cortu_cargo_pants'], ['toigo_basic_tucked_t-shirt', 'cortu_cargo_pants']],
    },
    'office': {
        'male': [['male_elegantsuit01'], ['toigo_male_suit_3'], ['toigo_male_suit_tie_and_jacket'], ['toigo_male_double-breasted_suit']],
        'female': [['female_elegantsuit01'], ['toigo_female_suit'], ['toigo_female_suit_2'], ['toigo_female_double-breasted_suit']],
    },
}
SHOES = {
    'casual': {'male': ['shoes01', 'shoes02', 'shoes03', 'toigo_mj_cloth_shoes'], 'female': ['shoes04', 'shoes05', 'toigo_flats', 'toigo_ankle_boots_female']},
    'worker': {'male': ['toigo_ankle_boots_male', 'culturalibre_male_boots', 'shoes02'], 'female': ['toigo_ankle_boots_female', 'shoes05']},
    'office': {'male': ['shoes01', 'shoes03'], 'female': ['toigo_flats', 'toigo_stiletto_booties', 'shoes06']},
}
HATS = {'casual': ['jujube_newsboy_cap', 'fedora01', ''], 'worker': ['jujube_newsboy_cap', ''], 'office': ['fedora01', '']}
GLASSES = ['frankyaye_glasses_library_male', 'kwnet_at_optical_glasses', 'toigo_round_glasses_leopard', 'spamrakuen_tbm_glasses_frames_01']
EYES = ['brown', 'brownlight', 'blue', 'bluegreen', 'deepblue', 'green', 'lightblue', 'darkbrown']
EYEBROWS = ['eyebrow%03d' % i for i in range(1, 13)]
RACES = ['african', 'asian', 'caucasian']

DATA = os.path.join(os.environ['APPDATA'], 'Blender Foundation', 'Blender', '5.1', 'extensions', '.user', 'user_default', 'mpfb', 'data')


# ---- colour: baked into new textures so it survives the FBX trip into Unreal ---------------------------------
HAIR_COLOURS = {
    'african': [(0.025, 0.02, 0.018), (0.05, 0.035, 0.025), (0.09, 0.055, 0.035)],
    'asian': [(0.02, 0.018, 0.018), (0.04, 0.03, 0.025), (0.08, 0.05, 0.035)],
    'caucasian': [(0.03, 0.025, 0.02), (0.09, 0.055, 0.03), (0.18, 0.1, 0.05), (0.3, 0.1, 0.04), (0.55, 0.4, 0.22), (0.42, 0.3, 0.17)],
}
GREY = (0.5, 0.49, 0.47)
# Plain garments (light, neutral textures) that take a dye well, and the dyes.
DYEABLE = ['joepal_crude_t-shirt_female', 'elvs_crude_t-shirt_male', 'namuhekam_male_polo_shirt', 'toigo_basic_tucked_t-shirt',
           'toigo_fisherman_sweater', 'toigo_turtleneck_halter_top', 'jujube_newsboy_cap']
DYES = [(0.75, 0.2, 0.2), (0.2, 0.35, 0.7), (0.25, 0.55, 0.3), (0.85, 0.65, 0.2), (0.45, 0.25, 0.55), (0.9, 0.9, 0.9),
        (0.25, 0.25, 0.28), (0.85, 0.45, 0.6), (0.3, 0.6, 0.7), (0.6, 0.35, 0.2)]
TEX_OUT = os.path.join(os.path.dirname(os.path.abspath(__file__)), '..', 'SourceArt', 'NPCs', 'textures')


def recolour(obj, colour, mode, tag):
    """Swap the base colour texture on obj for a recoloured copy. mode 'hair' keeps the strands' light and dark
    but sets their hue; 'dye' multiplies the garment by the colour (white becomes the dye)."""
    import numpy as np
    os.makedirs(TEX_OUT, exist_ok=True)
    for slot in obj.material_slots:
        m = slot.material
        if not m or not m.use_nodes:
            continue
        for n in m.node_tree.nodes:
            if n.type != 'TEX_IMAGE' or not n.image or 'normal' in n.image.name.lower() or '_ao' in n.image.name.lower():
                continue
            src = n.image
            name = '%s_%s.png' % (os.path.splitext(src.name)[0], tag)
            path = os.path.abspath(os.path.join(TEX_OUT, name))
            if not os.path.exists(path):
                w, h = src.size
                px = np.empty(w * h * 4, dtype=np.float32)
                src.pixels.foreach_get(px)
                px = px.reshape(-1, 4)
                lum = px[:, :3] @ np.array([0.299, 0.587, 0.114], dtype=np.float32)
                c = np.array(colour, dtype=np.float32)
                if mode == 'hair':
                    mean = max(1e-3, float(lum[px[:, 3] > 0.5].mean()) if (px[:, 3] > 0.5).any() else float(lum.mean()))
                    px[:, :3] = np.clip(c[None, :] * (lum / mean)[:, None] * 1.1, 0.0, 1.0)
                else:
                    px[:, :3] = np.clip(px[:, :3] * c[None, :] * 1.15, 0.0, 1.0)
                out = bpy.data.images.new(name, w, h, alpha=True)
                out.pixels.foreach_set(px.ravel())
                out.filepath_raw = path
                out.file_format = 'PNG'
                out.save()
            n.image = bpy.data.images.load(path, check_existing=True)
            break


def exists(sub, name, ext='.mhclo'):
    return bool(name) and os.path.exists(os.path.join(DATA, sub, name, name + ext))


def pick(rng, items, sub='clothes'):
    good = [i for i in items if i == '' or exists(sub, i)]
    return rng.choice(good) if good else ''


def make_info(role, seed):
    rng = random.Random(seed * 7919 + zlib.crc32(role.encode()) % 1000)  # crc32, not hash(): hash() changes every run
    spec = Rand.get_default_phenotype_spec()
    ph = spec['phenotype']
    ph['discrete_age'] = False
    a = ph['attributes']
    a['age']['neutral'], a['age']['deviation'] = 0.62, 0.2        # 25 to ~70 years
    a['height']['neutral'], a['height']['deviation'] = 0.5, 0.22  # no giants
    a['weight']['neutral'], a['weight']['deviation'] = 0.5, 0.25
    a['muscle']['neutral'], a['muscle']['deviation'] = 0.5, 0.22
    a['proportions']['neutral'], a['proportions']['deviation'] = 0.55, 0.15
    a['cupsize']['deviation'] = 0.15
    macro = Rand.randomize_macro_info_dict(spec, rng)
    macro['age'] = max(0.5, macro['age'])
    sex = 'male' if macro['gender'] >= 0.5 else 'female'
    race = max(RACES, key=lambda r: macro['race'].get(r, 0.0))
    band = 'young' if macro['age'] < 0.62 else ('middleage' if macro['age'] < 0.8 else 'old')
    skin = '%s_%s_%s' % (band, race, sex)
    if not exists('skins', skin, '.mhmat'):
        skin = 'young_%s_%s' % (race, sex)

    clothes = list(rng.choice(OUTFITS[role][sex]))
    clothes = [c for c in clothes if exists('clothes', c)]
    shoe = pick(rng, SHOES[role][sex])
    if shoe:
        clothes.append(shoe)
    hair = pick(rng, HAIR[sex], 'hair')
    hat = pick(rng, HATS[role]) if rng.random() < 0.35 else ''
    if hat:
        clothes.append(hat)
    if rng.random() < 0.25 + 0.3 * (macro['age'] > 0.75):
        g = pick(rng, GLASSES)
        if g:
            clothes.append(g)

    info = HumanService._create_default_human_info_dict()
    info['phenotype'] = macro
    info['rig'] = 'game_engine'
    info['skin_mhmat'] = '%s/%s.mhmat' % (skin, skin)
    info['skin_material_type'] = 'MAKESKIN'
    info['eyes'] = 'low-poly/low-poly.mhclo'
    info['eyes_material_type'] = 'MAKESKIN'
    eb = pick(rng, EYEBROWS, 'eyebrows')
    info['eyebrows'] = '%s/%s.mhclo' % (eb, eb) if eb else ''
    info['eyelashes'] = 'eyelashes02/eyelashes02.mhclo'
    info['teeth'] = 'teeth_base/teeth_base.mhclo'
    info['hair'] = '%s/%s.mhclo' % (hair, hair) if hair else ''
    info['clothes'] = ['%s/%s.mhclo' % (c, c) for c in clothes]
    info['clothes_material_type'] = 'MAKESKIN'
    info['alternative_materials'] = {}
    hair_colour = rng.choice(HAIR_COLOURS[race])
    if band == 'old' or (band == 'middleage' and rng.random() < 0.35):
        mix = 0.85 if band == 'old' else 0.45
        hair_colour = tuple(a * (1 - mix) + b * mix for a, b in zip(hair_colour, GREY))
    dyes = {c: rng.choice(DYES) for c in clothes if c in DYEABLE}
    summary = {'role': role, 'seed': seed, 'hair_colour': [round(v, 3) for v in hair_colour], 'dyes': {k: list(v) for k, v in dyes.items()}, 'sex': sex, 'race': race, 'age_band': band, 'skin': skin, 'hair': hair,
               'clothes': clothes, 'eye_colour': rng.choice(EYES),
               'macro': {k: round(v, 3) for k, v in macro.items() if isinstance(v, float)}}
    return info, summary


def build(info):
    settings = HumanService.get_default_deserialization_settings()
    settings['subdiv_levels'] = 0
    before = set(bpy.data.objects)
    basemesh = HumanService.deserialize_from_dict(info, settings)
    made = [o for o in bpy.data.objects if o not in before]
    rig = next((o for o in made if o.type == 'ARMATURE'), None)
    return basemesh, rig, made


def set_eye_colour(made, colour):
    path = os.path.join(DATA, 'eyes', 'materials', colour + '.mhmat')
    for o in made:
        if o.type == 'MESH' and 'eye' in o.name.lower() and 'brow' not in o.name.lower() and 'lash' not in o.name.lower():
            for slot in o.material_slots:
                m = slot.material
                if not m or not m.use_nodes:
                    continue
                tex = os.path.join(DATA, 'eyes', 'materials', 'textures', colour + '_eye.png')
                for n in m.node_tree.nodes:
                    if n.type == 'TEX_IMAGE' and n.image and os.path.exists(tex):
                        n.image = bpy.data.images.load(tex, check_existing=True)
                        break


def main():
    for o in list(bpy.data.objects):
        bpy.data.objects.remove(o)
    roles = [r.strip() for r in args.roles.split(',') if r.strip()]
    people = []
    for i in range(args.count):
        role = roles[i % len(roles)]
        seed = args.seed + i
        info, summary = make_info(role, seed)
        basemesh, rig, made = build(info)
        set_eye_colour(made, summary['eye_colour'])
        for o in made:
            if o.type != 'MESH':
                continue
            low = o.name.lower()
            if summary['hair'] and summary['hair'].lower() in low:
                recolour(o, summary['hair_colour'], 'hair', 'h%03d%03d%03d' % tuple(int(v * 999) for v in summary['hair_colour']))
            for garment, dye in summary['dyes'].items():
                if garment.lower() in low:
                    recolour(o, dye, 'dye', 'd%03d%03d%03d' % tuple(int(v * 999) for v in dye))
        root = rig or basemesh
        root.location.x = (i - (args.count - 1) / 2.0) * args.spacing
        root.name = 'NPC_%s_%d' % (role, seed)
        people.append((summary, root, made))
        print('NPC', json.dumps(summary))

    if args.export:
        os.makedirs(args.export, exist_ok=True)
        # Centimetres, natively (2026-11-14). Exported in metres, Unreal's import converts by scaling the root bone by
        # 100: the mesh looks right, but every retargeted clip then moves the bones 100x too far and the body vanishes.
        # So the scene unit becomes 1 cm, everything is scaled up 100x and applied, and the FBX needs no conversion.
        bpy.context.scene.unit_settings.system = 'METRIC'
        bpy.context.scene.unit_settings.scale_length = 0.01
        tops = {o for _, _, made in people for o in made if o.parent is None and o.type in ('MESH', 'ARMATURE')}
        for o in tops:
            o.location = o.location * 100.0
            o.scale = o.scale * 100.0
        bpy.ops.object.select_all(action='DESELECT')
        for _, _, made in people:
            for o in made:
                if o.type in ('MESH', 'ARMATURE'):
                    o.select_set(True)
        bpy.context.view_layer.objects.active = next(iter(tops))
        bpy.ops.object.transform_apply(location=False, rotation=False, scale=True)
        for summary, root, made in people:
            bpy.ops.object.select_all(action='DESELECT')
            loc = root.location.copy()
            root.location = (0, 0, 0)
            for o in made:
                if o.type in ('MESH', 'ARMATURE'):
                    o.select_set(True)
            bpy.context.view_layer.objects.active = root
            name = 'NPC_%s_%d' % (summary['role'], summary['seed'])
            # Unreal drops an armature object named "Armature"; any other name becomes an extra root bone, which would
            # give every NPC a different skeleton (2026-11-14).
            own_name = root.name
            root.name = 'Armature'
            # Which texture each material slot uses: the FBX import does not keep texture links, so the Unreal side
            # (Tools/import_npcs.py) rebuilds the materials from this.
            slots = {}
            for o in made:
                if o.type != 'MESH':
                    continue
                for slot in o.material_slots:
                    m = slot.material
                    if not m or not m.use_nodes or m.name in slots:
                        continue
                    best = None
                    for n in m.node_tree.nodes:
                        if n.type == 'TEX_IMAGE' and n.image:
                            low = n.image.name.lower()
                            if 'normal' in low or low.endswith('_ao.png') or '_ao' in low or 'bump' in low or 'spec' in low:
                                continue
                            best = bpy.path.abspath(n.image.filepath) or best
                            if 'diffuse' in low or best:
                                break
                    slots[m.name] = best
            summary['materials'] = slots
            bpy.ops.export_scene.fbx(filepath=os.path.join(args.export, name + '.fbx'), use_selection=True,
                                     global_scale=1.0, apply_unit_scale=True, apply_scale_options='FBX_SCALE_NONE', add_leaf_bones=False,
                                     use_mesh_modifiers=True, mesh_smooth_type='FACE', path_mode='COPY', embed_textures=True,
                                     bake_anim=False, armature_nodetype='NULL')
            root.name = own_name
            root.location = loc
        with open(os.path.join(args.export, 'npcs.json'), 'w') as f:
            json.dump([p[0] for p in people], f, indent=1)
        print('EXPORTED', len(people))

    if args.preview:
        scene = bpy.context.scene
        try:
            scene.render.engine = 'BLENDER_EEVEE_NEXT'
        except TypeError:
            scene.render.engine = 'BLENDER_EEVEE'
        width = max(1.0, args.count * args.spacing)
        scene.render.resolution_x, scene.render.resolution_y = int(min(2400, 260 * args.count)), 760
        cam = bpy.data.objects.new('Cam', bpy.data.cameras.new('Cam'))
        scene.collection.objects.link(cam)
        cam.data.type = 'ORTHO'
        cam.data.ortho_scale = max(width + 0.4, 2.35 * scene.render.resolution_x / scene.render.resolution_y)
        cam.location = (0, -8, 0.92)
        cam.rotation_euler = (math.radians(90), 0, 0)
        scene.camera = cam
        for name, loc, energy in [('Key', (-3, -5, 4), 900), ('Fill', (4, -4, 2), 350), ('Rim', (0, 4, 4), 600)]:
            l = bpy.data.objects.new(name, bpy.data.lights.new(name, 'AREA'))
            l.data.energy = energy
            l.data.size = 5
            l.location = loc
            d = -l.location
            l.rotation_euler = d.to_track_quat('-Z', 'Y').to_euler()
            scene.collection.objects.link(l)
        world = bpy.data.worlds.new('W')
        scene.world = world
        world.use_nodes = True
        world.node_tree.nodes['Background'].inputs[0].default_value = (0.09, 0.07, 0.13, 1)
        floor = bpy.data.meshes.new('Floor')
        floor.from_pydata([(-50, -50, 0), (50, -50, 0), (50, 50, 0), (-50, 50, 0)], [], [(0, 1, 2, 3)])
        fo = bpy.data.objects.new('Floor', floor)
        scene.collection.objects.link(fo)
        fm = bpy.data.materials.new('FloorM')
        fm.diffuse_color = (0.12, 0.1, 0.16, 1)
        fm.use_nodes = True
        next(n for n in fm.node_tree.nodes if n.type == 'BSDF_PRINCIPLED').inputs['Base Color'].default_value = (0.12, 0.1, 0.16, 1)
        floor.materials.append(fm)
        scene.render.filepath = os.path.abspath(args.preview)
        os.makedirs(os.path.dirname(scene.render.filepath), exist_ok=True)
        bpy.ops.render.render(write_still=True)
        print('PREVIEW', scene.render.filepath)


main()
