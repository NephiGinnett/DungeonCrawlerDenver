"""Placeholder models for equipment rows with no mesh (2026-11-14, Zoe: "a lot of items and weapons that don't have
models yet. Can you find placeholders / build them where appropriate").

Built from primitives in Blender, one clean style: simple shapes, flat colours, real-world size. One FBX per item in
SourceArt/Placeholders/fbx/<ItemId>.fbx, plus a contact sheet. Re-run to rebuild them all:
  blender --background --python Tools/build_item_placeholders.py -- [--sheet SourceArt/Placeholders/sheet.png]

Conventions, so they snap to the existing sockets:
  hand items  origin at the grip, the business end along +X (barrel, bolt, nail), up is +Z
  head items  origin at the middle of the head (about 10 cm radius)
  ground-only (Body clothing, Trinkets)  origin at the bottom centre, lying flat; DT row's GroundMesh, not Mesh
Sizes are metres here; the FBX export writes centimetres.
"""
import bpy, bmesh, sys, os, math
from mathutils import Vector, Euler, Matrix

argv = sys.argv[sys.argv.index('--') + 1:] if '--' in sys.argv else []
SHEET = argv[argv.index('--sheet') + 1] if '--sheet' in argv else ''
ONLY = argv[argv.index('--only') + 1].split(',') if '--only' in argv else None
ROOT = os.path.join(os.path.dirname(os.path.abspath(__file__)), '..', 'SourceArt', 'Placeholders')
OUT = os.path.join(ROOT, 'fbx')
os.makedirs(OUT, exist_ok=True)

MATS = {}


def mat(name, rgb, metal=0.0, rough=0.6, emit=0.0):
    key = (name, rgb, metal, rough, emit)
    if key in MATS:
        return MATS[key]
    m = bpy.data.materials.new('M_' + name)
    m.use_nodes = True
    b = next(n for n in m.node_tree.nodes if n.type == 'BSDF_PRINCIPLED')
    b.inputs['Base Color'].default_value = (*rgb, 1)
    b.inputs['Metallic'].default_value = metal
    b.inputs['Roughness'].default_value = rough
    if emit > 0:
        b.inputs['Emission Color'].default_value = (*rgb, 1)
        b.inputs['Emission Strength'].default_value = emit
    m.diffuse_color = (*rgb, 1)
    MATS[key] = m
    return m


PARTS = []


def _finish(obj, m, loc, rot):
    obj.location = loc
    obj.rotation_euler = Euler([math.radians(a) for a in rot])
    obj.data.materials.append(m)
    PARTS.append(obj)
    return obj


def box(size, loc=(0, 0, 0), m=None, rot=(0, 0, 0), bevel=0.0):
    bpy.ops.mesh.primitive_cube_add(size=1)
    o = bpy.context.object
    o.scale = size
    bpy.ops.object.transform_apply(scale=True)
    if bevel > 0:
        mod = o.modifiers.new('b', 'BEVEL'); mod.width = bevel; mod.segments = 2
        bpy.ops.object.modifier_apply(modifier='b')
    return _finish(o, m, loc, rot)


def cyl(r, h, loc=(0, 0, 0), m=None, rot=(0, 0, 0), verts=24, r2=None):
    if r2 is None:
        bpy.ops.mesh.primitive_cylinder_add(radius=r, depth=h, vertices=verts)
    else:
        bpy.ops.mesh.primitive_cone_add(radius1=r, radius2=r2, depth=h, vertices=verts)
    return _finish(bpy.context.object, m, loc, rot)


def sph(r, loc=(0, 0, 0), m=None, scale=(1, 1, 1), rot=(0, 0, 0)):
    bpy.ops.mesh.primitive_uv_sphere_add(radius=r, segments=24, ring_count=12)
    o = bpy.context.object
    o.scale = scale
    bpy.ops.object.transform_apply(scale=True)
    return _finish(o, m, loc, rot)


def tor(R, r, loc=(0, 0, 0), m=None, rot=(0, 0, 0)):
    bpy.ops.mesh.primitive_torus_add(major_radius=R, minor_radius=r, major_segments=32, minor_segments=10)
    return _finish(bpy.context.object, m, loc, rot)


def text(s, size, loc, m, rot=(0, 0, 0), depth=0.0006, align='CENTER'):
    bpy.ops.object.text_add()
    t = bpy.context.object
    t.data.body = s
    t.data.size = size
    t.data.extrude = depth
    t.data.align_x = align
    t.data.align_y = 'CENTER'
    bpy.ops.object.convert(target='MESH')
    return _finish(bpy.context.object, m, loc, rot)


def half_shell(r, loc, m, scale=(1, 1, 1), rot=(0, 0, 0), thickness=0.006):
    bpy.ops.mesh.primitive_uv_sphere_add(radius=r, segments=32, ring_count=16)
    o = bpy.context.object
    bm = bmesh.new(); bm.from_mesh(o.data)
    bmesh.ops.delete(bm, geom=[v for v in bm.verts if v.co.z < -1e-4], context='VERTS')
    bm.to_mesh(o.data); bm.free()
    o.scale = scale
    bpy.ops.object.transform_apply(scale=True)
    mod = o.modifiers.new('s', 'SOLIDIFY'); mod.thickness = thickness
    bpy.ops.object.modifier_apply(modifier='s')
    return _finish(o, m, loc, rot)


# ---- palette -----------------------------------------------------------------------------------------------
WOOD = mat('Wood', (0.36, 0.2, 0.09), rough=0.7)
DARKWOOD = mat('DarkWood', (0.2, 0.1, 0.05), rough=0.6)
STEEL = mat('Steel', (0.55, 0.56, 0.58), metal=1.0, rough=0.35)
GUNMETAL = mat('Gunmetal', (0.08, 0.08, 0.09), metal=0.8, rough=0.45)
BLACKPOLY = mat('BlackPoly', (0.03, 0.03, 0.035), rough=0.55)
ORANGE = mat('SafetyOrange', (1.0, 0.32, 0.03), rough=0.45)
YELLOW = mat('SafetyYellow', (1.0, 0.75, 0.03), rough=0.4)
HIVIS = mat('HiVis', (0.85, 1.0, 0.05), rough=0.5, emit=0.4)
SILVERTAPE = mat('Reflective', (0.85, 0.85, 0.88), metal=0.6, rough=0.2)
GOLD = mat('Gold', (1.0, 0.72, 0.25), metal=1.0, rough=0.25)
RUBY = mat('Ruby', (0.75, 0.02, 0.08), rough=0.1)
SAPPHIRE = mat('Sapphire', (0.05, 0.12, 0.8), rough=0.1)
WHITE = mat('White', (0.92, 0.92, 0.9), rough=0.6)
PAPER = mat('Paper', (0.93, 0.9, 0.82), rough=0.8)
GREENCHECK = mat('ChequeGreen', (0.72, 0.86, 0.72), rough=0.8)
INK = mat('Ink', (0.04, 0.04, 0.08), rough=0.6)
REDTAG = mat('NameTagRed', (0.8, 0.06, 0.08), rough=0.5)
NAVY = mat('Navy', (0.05, 0.08, 0.2), rough=0.8)
CHARCOAL = mat('Charcoal', (0.12, 0.12, 0.13), rough=0.8)
FLEECE = mat('FleeceGrey', (0.35, 0.37, 0.4), rough=0.95)
FLEECEBLUE = mat('FleeceBlue', (0.08, 0.25, 0.55), rough=0.95)
CANVAS = mat('Canvas', (0.55, 0.48, 0.34), rough=0.9)
GREASE = mat('Grease', (0.2, 0.18, 0.15), rough=0.5)
PARTY_PINK = mat('PartyPink', (1.0, 0.25, 0.7), rough=0.4)
PARTY_CYAN = mat('PartyCyan', (0.1, 0.85, 1.0), rough=0.4)
PARTY_LIME = mat('PartyLime', (0.5, 1.0, 0.2), rough=0.4)
HALO = mat('HaloGlow', (1.0, 0.85, 0.4), rough=0.3, emit=6.0)
LENS = mat('Lens', (0.7, 0.85, 0.9), metal=0.2, rough=0.05)
VELVET = mat('Velvet', (0.35, 0.02, 0.12), rough=1.0)
BOARD = mat('BoardGameBox', (0.15, 0.45, 0.85), rough=0.6)
DIEWHITE = mat('DieWhite', (0.95, 0.95, 0.92), rough=0.3)
DIERED = mat('DieRed', (0.8, 0.05, 0.08), rough=0.3)
LANYARD = mat('LanyardRed', (0.75, 0.1, 0.12), rough=0.7)
WAX = mat('SealWax', (0.6, 0.03, 0.05), rough=0.4)
CARDBOARD = mat('Cardboard', (0.6, 0.45, 0.28), rough=0.9)


# ---- the items ---------------------------------------------------------------------------------------------
def ConfettiCrossbow():
    box((0.52, 0.045, 0.05), (0.18, 0, 0.0), PARTY_PINK, bevel=0.006)          # stock / rail
    box((0.12, 0.04, 0.09), (-0.08, 0, -0.05), PARTY_PINK, rot=(0, -20, 0), bevel=0.006)  # grip
    box((0.18, 0.05, 0.06), (-0.17, 0, -0.01), PARTY_PINK, bevel=0.006)        # butt
    for s in (-1, 1):
        box((0.035, 0.3, 0.025), (0.41, s * 0.15, 0.0), PARTY_CYAN, rot=(0, 0, s * 18), bevel=0.008)  # limbs, swept back
        box((0.004, 0.2, 0.003), (0.34, s * 0.1, 0.0), WHITE, rot=(0, 0, -s * 35))  # string, drawn back to the bolt
    cyl(0.006, 0.4, (0.32, 0, 0.035), STEEL, rot=(0, 90, 0))                  # bolt
    for i, m in enumerate((PARTY_PINK, PARTY_CYAN, PARTY_LIME, YELLOW)):
        box((0.04, 0.003, 0.025), (0.14 + i * 0.012, 0, 0.05 + (i % 2) * 0.01), m, rot=(i * 45, 0, 0))  # fletching / confetti
    sph(0.025, (0.53, 0, 0.035), YELLOW)                                       # popper head


def HuntingRifleColorado():
    box((0.42, 0.04, 0.07), (-0.12, 0, -0.015), WOOD, bevel=0.01)              # butt stock
    box((0.36, 0.035, 0.045), (0.25, 0, 0.0), WOOD, bevel=0.008)               # fore-end
    box((0.06, 0.03, 0.09), (-0.0, 0, -0.06), WOOD, rot=(0, -15, 0), bevel=0.006)  # pistol grip
    cyl(0.011, 0.62, (0.58, 0, 0.03), GUNMETAL, rot=(0, 90, 0))               # barrel
    box((0.18, 0.04, 0.04), (0.08, 0, 0.035), GUNMETAL, bevel=0.004)           # receiver
    cyl(0.018, 0.3, (0.08, 0, 0.1), BLACKPOLY, rot=(0, 90, 0))                # scope
    for x in (0.0, 0.15):
        box((0.015, 0.02, 0.04), (x, 0, 0.07), BLACKPOLY)                      # scope rings
    cyl(0.006, 0.05, (0.04, 0.03, 0.05), STEEL, rot=(90, 0, 0))               # bolt handle
    sph(0.01, (0.04, 0.055, 0.05), STEEL)


def RiotShotgun():
    box((0.3, 0.045, 0.07), (-0.14, 0, -0.01), BLACKPOLY, bevel=0.01)          # stock
    box((0.06, 0.035, 0.1), (0.0, 0, -0.06), BLACKPOLY, rot=(0, -12, 0), bevel=0.006)
    box((0.22, 0.045, 0.06), (0.12, 0, 0.01), GUNMETAL, bevel=0.005)           # receiver
    cyl(0.015, 0.5, (0.47, 0, 0.03), GUNMETAL, rot=(0, 90, 0))                # barrel
    cyl(0.014, 0.42, (0.43, 0, -0.005), GUNMETAL, rot=(0, 90, 0))             # magazine tube
    cyl(0.024, 0.16, (0.38, 0, -0.005), ORANGE, rot=(0, 90, 0), verts=12)     # pump, the loss-prevention orange
    box((0.2, 0.047, 0.012), (0.12, 0, 0.045), ORANGE)                         # stripe
    text('LP', 0.03, (0.12, 0.024, 0.015), WHITE, rot=(90, 0, 0))


def NailGunPro():
    box((0.26, 0.06, 0.08), (0.08, 0, 0.06), YELLOW, bevel=0.012)              # body
    box((0.06, 0.045, 0.14), (0.0, 0, -0.04), BLACKPOLY, rot=(0, -10, 0), bevel=0.01)  # handle
    box((0.05, 0.03, 0.03), (0.23, 0, -0.0), BLACKPOLY, bevel=0.004)           # nose
    box((0.17, 0.022, 0.035), (0.14, 0, 0.0), GUNMETAL, rot=(0, 12, 0))       # nail magazine, under the body
    box((0.09, 0.05, 0.05), (-0.03, 0, -0.12), BLACKPOLY, bevel=0.008)         # battery
    text('PRO', 0.035, (0.09, 0.031, 0.06), BLACKPOLY, rot=(90, 0, 0))


def FlareGunEmergency():
    box((0.14, 0.035, 0.05), (0.06, 0, 0.03), ORANGE, bevel=0.01)              # frame
    cyl(0.022, 0.16, (0.15, 0, 0.035), ORANGE, rot=(0, 90, 0))                # fat barrel
    cyl(0.016, 0.01, (0.235, 0, 0.035), BLACKPOLY, rot=(0, 90, 0))
    box((0.045, 0.03, 0.1), (-0.0, 0, -0.03), ORANGE, rot=(0, -15, 0), bevel=0.008)
    box((0.03, 0.006, 0.025), (0.05, 0, -0.01), BLACKPOLY)                      # trigger guard hint


def DecorativeHardHat():
    half_shell(0.125, (0, 0, 0.04), YELLOW, scale=(1.1, 0.95, 0.85))
    cyl(0.155, 0.008, (0.02, 0, 0.04), YELLOW, verts=32, rot=(0, 0, 0)).scale = (1.15, 1.0, 1.0)
    box((0.2, 0.02, 0.02), (0, 0, 0.145), YELLOW, bevel=0.008)                 # ridge
    box((0.05, 0.002, 0.03), (0.0, -0.118, 0.1), WHITE, rot=(0, 0, 0))          # sticker
    text('SAFETY\nFIRST', 0.008, (0.0, -0.12, 0.1), INK, rot=(90, 0, 0))


def SomeoneElsesBifocals():
    for s in (-1, 1):
        tor(0.025, 0.0025, (0.12, s * 0.035, 0.02), GOLD, rot=(0, 90, 0))
        cyl(0.024, 0.002, (0.12, s * 0.035, 0.02), LENS, rot=(0, 90, 0))
        cyl(0.012, 0.0025, (0.121, s * 0.035, 0.008), LENS, rot=(0, 90, 0))   # the bifocal line
        cyl(0.0015, 0.13, (0.055, s * 0.07, 0.025), GOLD, rot=(0, 90, 0))     # arms back to the ears
    cyl(0.002, 0.03, (0.12, 0, 0.025), GOLD, rot=(90, 0, 0))                   # bridge


def NoiseCancellingHeadset():
    band = tor(0.12, 0.012, (0, 0, 0.0), BLACKPOLY, rot=(90, 0, 0))             # band over the top of the head only
    bpy.context.view_layer.objects.active = band
    bpy.ops.object.transform_apply(rotation=True)
    bm = bmesh.new(); bm.from_mesh(band.data)
    bmesh.ops.delete(bm, geom=[v for v in bm.verts if v.co.z < -0.01], context='VERTS')
    bm.to_mesh(band.data); bm.free()
    for s in (-1, 1):
        cyl(0.05, 0.035, (0, s * 0.11, 0.0), BLACKPOLY, rot=(90, 0, 0))
        cyl(0.042, 0.04, (0, s * 0.12, 0.0), CHARCOAL, rot=(90, 0, 0))
    cyl(0.004, 0.12, (0.06, -0.1, -0.04), BLACKPOLY, rot=(0, 70, -20))          # mic boom
    sph(0.01, (0.11, -0.08, -0.06), BLACKPOLY)


def CrownOfMiddleManagement():
    cyl(0.105, 0.05, (0, 0, 0.1), GOLD, verts=32)
    for i in range(8):
        a = i / 8 * 2 * math.pi
        cyl(0.022, 0.06, (0.1 * math.cos(a), 0.1 * math.sin(a), 0.15), GOLD, verts=4, r2=0.0, rot=(0, 0, math.degrees(a) + 45))
        sph(0.009, (0.106 * math.cos(a), 0.106 * math.sin(a), 0.1), RUBY if i % 2 else SAPPHIRE)
    box((0.004, 0.02, 0.05), (0.107, 0, 0.09), NAVY, rot=(0, 0, 0))            # a little necktie on the front
    text('MGR', 0.012, (0.107, 0, 0.115), INK, rot=(90, 0, 90))


def HaloOfMaximumEngagement():
    tor(0.11, 0.012, (0, 0, 0.21), HALO)
    for i in range(3):
        a = i / 3 * 2 * math.pi
        sph(0.012, (0.11 * math.cos(a), 0.11 * math.sin(a), 0.21), PARTY_PINK)  # notification dots


def folded(cloth, accent=None, accent2=None, text_s=None):
    """A folded garment lying flat: three stacked slabs with a collar, plus trim."""
    box((0.32, 0.26, 0.025), (0, 0, 0.0125), cloth, bevel=0.01)
    box((0.3, 0.24, 0.02), (0, 0, 0.035), cloth, bevel=0.009)
    box((0.29, 0.22, 0.018), (0.005, 0, 0.054), cloth, bevel=0.008)
    if accent:
        box((0.3, 0.025, 0.004), (0.005, -0.06, 0.064), accent)
        box((0.3, 0.025, 0.004), (0.005, 0.06, 0.064), accent)
    if accent2:
        box((0.04, 0.1, 0.004), (0.12, 0, 0.064), accent2)
    if text_s:
        text(text_s, 0.03, (-0.04, 0, 0.065), accent2 or WHITE, rot=(0, 0, 90))


def GreaseApron():
    folded(CANVAS, accent=None, accent2=GREASE)
    cyl(0.004, 0.4, (0.0, 0.14, 0.03), CANVAS, rot=(0, 90, 0))                 # ties
    sph(0.04, (0.06, -0.05, 0.064), GREASE, scale=(1.4, 1, 0.05))              # the stain


def ManagersBlazer():
    folded(NAVY, accent2=GOLD)
    for i in range(2):
        cyl(0.008, 0.004, (0.0 - i * 0.05, 0.03, 0.065), GOLD)
    box((0.06, 0.03, 0.004), (0.08, -0.07, 0.065), WHITE)                      # pocket square


def HighVisibilitySafetyVest():
    folded(HIVIS, accent=SILVERTAPE)


def BrandedFleeceVest():
    folded(FLEECE, accent2=WHITE, text_s='MHRS')


def MonogrammedExecutiveFleece():
    folded(FLEECEBLUE, accent2=GOLD, text_s='E.V.P.')


def PlateOfLimitedLiability():
    half_shell(0.2, (0, 0, 0.0), STEEL, scale=(1.0, 1.25, 0.45))
    box((0.02, 0.38, 0.01), (0.12, 0, 0.06), GOLD)
    text('LLC', 0.05, (0, 0, 0.095), GOLD, rot=(0, 0, 90))


def NameTagDave():
    box((0.09, 0.06, 0.003), (0, 0, 0.0015), REDTAG)
    box((0.08, 0.032, 0.0005), (0, -0.006, 0.003), WHITE)
    text('HELLO MY NAME IS', 0.0075, (0, 0.018, 0.0034), WHITE)
    text('DAVE', 0.018, (0, -0.006, 0.0034), INK)


def SeveranceCheckUncashed():
    box((0.16, 0.07, 0.001), (0, 0, 0.0005), GREENCHECK)
    text('PAY TO THE ORDER OF', 0.006, (-0.035, 0.018, 0.0012), INK)
    text('$ 0.47', 0.012, (0.05, 0.0, 0.0012), INK)
    box((0.06, 0.001, 0.0003), (0.04, -0.022, 0.0012), INK)


def CJBoardGame():
    box((0.3, 0.3, 0.06), (0, 0, 0.03), BOARD, bevel=0.003)
    box((0.3, 0.02, 0.06), (0, -0.14, 0.032), YELLOW)
    text("CJ's GAME NIGHT", 0.03, (0, 0.02, 0.061), WHITE)
    text('2-5 PLAYERS', 0.012, (0, -0.06, 0.061), YELLOW)


def MarcelLuckyDice():
    for i, (m, pip) in enumerate(((DIERED, WHITE), (DIEWHITE, INK))):
        x = -0.025 + i * 0.05
        box((0.03, 0.03, 0.03), (x, 0, 0.015), m, rot=(0, 0, 20 * i), bevel=0.004)
        for (px, py) in ((0, 0), (0.007, 0.007), (-0.007, -0.007)):
            sph(0.0028, (x + px, py, 0.0302), pip, scale=(1, 1, 0.3))


def ExpiredVisitorLanyard():
    box((0.055, 0.085, 0.002), (0, 0, 0.001), WHITE)
    box((0.055, 0.02, 0.0006), (0, 0.032, 0.0023), RUBY)
    text('VISITOR', 0.009, (0, 0.012, 0.0024), INK)
    text('EXPIRED', 0.008, (0, -0.012, 0.0024), RUBY, rot=(0, 0, 20))
    tor(0.11, 0.004, (0, 0.15, 0.002), LANYARD, rot=(0, 0, 0)).scale = (0.6, 1.0, 0.15)


def CustomerOfTheMonthPlaque():
    box((0.2, 0.25, 0.02), (0, 0, 0.01), DARKWOOD, bevel=0.004)
    box((0.15, 0.06, 0.003), (0, -0.07, 0.0215), GOLD)
    box((0.12, 0.12, 0.003), (0, 0.04, 0.0215), PAPER)
    text('CUSTOMER OF', 0.012, (0, -0.058, 0.0235), INK)
    text('THE MONTH', 0.012, (0, -0.08, 0.0235), INK)


def ring_in_box(gem, band, label=None):
    box((0.06, 0.06, 0.035), (0, 0, 0.0175), VELVET, bevel=0.004)
    box((0.06, 0.012, 0.045), (0, 0.032, 0.05), VELVET, rot=(-70, 0, 0), bevel=0.003)  # open lid
    tor(0.011, 0.0025, (0, -0.004, 0.045), band, rot=(90, 0, 0))
    if gem:
        sph(0.005, (0, -0.004, 0.058), gem)
    if label:
        box((0.012, 0.004, 0.012), (0, -0.004, 0.058), band)


def TenYearServiceRing():
    ring_in_box(SAPPHIRE, GOLD)


def NonTransferableSponsorSignet():
    ring_in_box(None, GOLD, label=True)


def DeedToDenverNotarised():
    cyl(0.025, 0.22, (0, 0, 0.025), PAPER, rot=(0, 90, 0))
    cyl(0.026, 0.02, (0, 0, 0.025), RUBY, rot=(0, 90, 0))                     # ribbon
    cyl(0.018, 0.006, (0, -0.026, 0.025), WAX, rot=(90, 0, 0))                # wax seal
    text('5280', 0.008, (0, -0.0295, 0.025), GOLD, rot=(90, 0, 0))


def SyndicateBadge():
    cyl(0.04, 0.006, (0, 0, 0.003), GOLD, verts=6)
    cyl(0.03, 0.002, (0, 0, 0.0065), NAVY, verts=6)
    text('S', 0.03, (0, 0, 0.0075), GOLD)


def HairnetOfHumility():
    half_shell(0.112, (0, 0, 0.02), WHITE, scale=(1.08, 0.98, 0.9), thickness=0.002)
    for i in range(10):
        a = i / 10 * 360
        tor(0.06, 0.0012, (0, 0, 0.02 + 0.05), WHITE, rot=(90, 0, a))          # the mesh, as crossing loops
    tor(0.113, 0.004, (0, 0, 0.022), WHITE)                                    # elastic band


def SafetyGoggles():
    box((0.03, 0.17, 0.055), (0.115, 0, 0.02), LENS, bevel=0.01)              # visor
    box((0.034, 0.18, 0.012), (0.115, 0, 0.05), BLACKPOLY, bevel=0.004)       # frame top
    box((0.034, 0.18, 0.012), (0.115, 0, -0.01), BLACKPOLY, bevel=0.004)      # frame bottom
    for s in (-1, 1):
        box((0.12, 0.012, 0.02), (0.05, s * 0.095, 0.02), BLACKPOLY, rot=(0, 0, s * 8))  # strap back to the head
    box((0.02, 0.001, 0.004), (0.13, -0.04, 0.03), WHITE, rot=(0, 0, 0))      # a scratch


def RepossessedRaincoat():
    folded(YELLOW, accent=BLACKPOLY)
    box((0.07, 0.04, 0.003), (-0.08, 0.07, 0.065), WHITE)                      # the repo tag
    text('REPO', 0.014, (-0.08, 0.07, 0.067), RUBY, rot=(0, 0, 90))


def TheBriefcaseClasp():
    box((0.06, 0.03, 0.012), (0, 0, 0.006), GOLD, bevel=0.003)
    box((0.04, 0.02, 0.006), (0, 0.022, 0.003), GOLD, bevel=0.002)
    cyl(0.004, 0.06, (0, 0.012, 0.008), STEEL, rot=(0, 90, 0))                # hinge pin
    box((0.03, 0.03, 0.003), (0.02, -0.005, 0.0015), mat('Leather', (0.25, 0.12, 0.05), rough=0.8), rot=(0, 0, 15))  # torn leather


def ConfiscatedSMG():
    box((0.24, 0.04, 0.07), (0.06, 0, 0.02), GUNMETAL, bevel=0.006)            # receiver
    cyl(0.011, 0.14, (0.24, 0, 0.035), GUNMETAL, rot=(0, 90, 0))              # barrel
    box((0.05, 0.035, 0.1), (-0.0, 0, -0.05), BLACKPOLY, rot=(0, -12, 0), bevel=0.006)  # grip
    box((0.03, 0.025, 0.16), (0.1, 0, -0.07), BLACKPOLY, rot=(0, 8, 0))       # magazine
    box((0.12, 0.02, 0.02), (-0.12, 0, 0.03), BLACKPOLY)                       # folded stock
    box((0.05, 0.002, 0.07), (0.05, 0.022, -0.04), PAPER, rot=(0, 15, 0))      # evidence tag
    text('EVIDENCE', 0.007, (0.05, 0.0235, -0.03), RUBY, rot=(90, -15, 0))


ITEMS = [
    ConfettiCrossbow, HuntingRifleColorado, RiotShotgun, NailGunPro, FlareGunEmergency,
    DecorativeHardHat, SomeoneElsesBifocals, NoiseCancellingHeadset, CrownOfMiddleManagement, HaloOfMaximumEngagement,
    GreaseApron, ManagersBlazer, HighVisibilitySafetyVest, BrandedFleeceVest, MonogrammedExecutiveFleece, PlateOfLimitedLiability,
    NameTagDave, SeveranceCheckUncashed, CJBoardGame, MarcelLuckyDice, ExpiredVisitorLanyard, CustomerOfTheMonthPlaque,
    TenYearServiceRing, NonTransferableSponsorSignet, DeedToDenverNotarised, SyndicateBadge,
    HairnetOfHumility, SafetyGoggles, RepossessedRaincoat, TheBriefcaseClasp, ConfiscatedSMG,
]


def clear():
    for o in list(bpy.data.objects):
        bpy.data.objects.remove(o)


def build(fn):
    PARTS.clear()
    fn()
    bpy.ops.object.select_all(action='DESELECT')
    for p in PARTS:
        p.select_set(True)
    bpy.context.view_layer.objects.active = PARTS[0]
    bpy.ops.object.join()
    o = bpy.context.object
    o.name = 'SM_' + fn.__name__
    bpy.ops.object.transform_apply(location=False, rotation=True, scale=True)
    return o


def main():
    clear()
    built = []
    for fn in ITEMS:
        if ONLY and fn.__name__ not in ONLY:
            continue
        o = build(fn)
        bpy.ops.object.select_all(action='DESELECT')
        o.select_set(True)
        bpy.ops.export_scene.fbx(filepath=os.path.join(OUT, fn.__name__ + '.fbx'), use_selection=True,
                                 # No unit scaling: Unreal reads these as metres and converts (2026-11-14: FBX_SCALE_UNITS imported at
                                 # 1/100 size, global_scale 100 at 100x).
                                 global_scale=1.0, apply_unit_scale=False, apply_scale_options='FBX_SCALE_NONE', mesh_smooth_type='FACE',
                                 bake_anim=False, axis_forward='X', axis_up='Z')
        dims = o.dimensions
        print('ITEM %-28s %5.1f x %5.1f x %5.1f cm' % (fn.__name__, dims.x * 100, dims.y * 100, dims.z * 100))
        built.append(o)

    if SHEET:
        # Contact sheet: each item in its own cell, scaled to fit, with its name.
        cols = 7
        cell = 1.0
        for i, o in enumerate(built):
            d = max(o.dimensions)
            s = 0.7 / max(d, 1e-3)
            o.scale = (s, s, s)
            o.rotation_euler = (math.radians(20), math.radians(-15), math.radians(-35))
            o.location = ((i % cols) * cell, 0, -(i // cols) * cell * 1.05)
            t = bpy.data.curves.new('lbl%d' % i, 'FONT')
            t.body = o.name[3:]
            t.size = 0.065
            t.align_x = 'CENTER'
            to = bpy.data.objects.new('lbl%d' % i, t)
            to.location = ((i % cols) * cell, 0, -(i // cols) * cell * 1.05 - 0.45)
            to.rotation_euler = (math.radians(90), 0, 0)
            to.data.materials.append(WHITE)
            bpy.context.scene.collection.objects.link(to)
        scene = bpy.context.scene
        try:
            scene.render.engine = 'BLENDER_EEVEE_NEXT'
        except TypeError:
            scene.render.engine = 'BLENDER_EEVEE'
        rows = (len(built) + cols - 1) // cols
        scene.render.resolution_x, scene.render.resolution_y = 1750, int(260 * rows)
        cam = bpy.data.objects.new('Cam', bpy.data.cameras.new('Cam'))
        scene.collection.objects.link(cam)
        cam.data.type = 'ORTHO'
        cam.data.ortho_scale = cols * cell + 0.2
        cam.location = ((cols - 1) * cell / 2, -10, -(rows - 1) * cell * 1.05 / 2 - 0.1)
        cam.rotation_euler = (math.radians(90), 0, 0)
        scene.camera = cam
        for name, loc, e in [('Key', (-3, -6, 4), 1500), ('Fill', (8, -5, 0), 600), ('Rim', (3, 5, 3), 900)]:
            l = bpy.data.objects.new(name, bpy.data.lights.new(name, 'AREA'))
            l.data.energy = e; l.data.size = 8
            l.location = loc
            l.rotation_euler = (-Vector(loc) + Vector(cam.location) * 0).to_track_quat('-Z', 'Y').to_euler()
            scene.collection.objects.link(l)
        w = bpy.data.worlds.new('W'); scene.world = w; w.use_nodes = True
        w.node_tree.nodes['Background'].inputs[0].default_value = (0.1, 0.08, 0.14, 1)
        scene.render.filepath = os.path.abspath(SHEET)
        bpy.ops.render.render(write_still=True)
        print('SHEET', scene.render.filepath)


if __name__ == '__main__':
    main()  # importable: Tools/render_item_icons.py reuses the builders
