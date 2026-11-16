"""Item pictures for the menus (2026-11-14, Zoe: "can you have a small image of the item accompany the name to the side
or when you scroll over it?").

Renders a 256 px PNG with a transparent background for everything the Den counter shows:
  cosmetics   rebuilt from the same basic shapes the game uses (parsed out of Source/.../Arcade/DCCCosmetics.cpp)
  booth guns  the placeholder models (Tools/build_item_placeholders.py)
  trophies    small models made here (Fourteener artifacts, Repo Rally seized goods)
  chop shop   one model per truck upgrade
into SourceArt/UI/Icons/T_Icon_<Id>.png. Tools/import_item_icons.py brings them into /Game/DCC/UI/Icons.

  blender --background --factory-startup --python Tools/render_item_icons.py [-- --only Id,Id]
"""
import bpy, sys, os, re, math
from mathutils import Vector

HERE = os.path.dirname(os.path.abspath(__file__))
ROOT = os.path.join(HERE, '..')
sys.path.insert(0, HERE)
import build_item_placeholders as B  # noqa: E402  (shape helpers, palette, gun builders)

argv = sys.argv[sys.argv.index('--') + 1:] if '--' in sys.argv else []
ONLY = argv[argv.index('--only') + 1].split(',') if '--only' in argv else None
OUT = os.path.join(ROOT, 'SourceArt', 'UI', 'Icons')
os.makedirs(OUT, exist_ok=True)

# ---- cosmetics, parsed from the game's own catalogue ------------------------------------------------------
NAMED = {
    'Brown': (0.32, 0.17, 0.07), 'DarkBrown': (0.12, 0.06, 0.03), 'Gold': (1.0, 0.75, 0.2), 'White': (0.95, 0.95, 0.95),
}


def parse_cosmetics():
    src = open(os.path.join(ROOT, 'Source', 'DungeonCrawlerDenver', 'Arcade', 'DCCCosmetics.cpp'), encoding='utf-8').read()
    for m in re.finditer(r'const FLinearColor (\w+)\(([-\d.f, ]+)\);', src):
        NAMED[m.group(1)] = tuple(float(v.strip().rstrip('f')) for v in m.group(2).split(','))[:3]
    items = {}
    for block in re.finditer(r'Add\(TEXT\("(\w+)"\).*?\}\);', src, re.S):
        parts = []
        for p in re.finditer(r'\{\s*TEXT\("(\w+)"\),\s*FVector\(([^)]*)\),\s*FVector\(([^)]*)\),\s*(FRotator::ZeroRotator|FRotator\(([^)]*)\)),\s*(FLinearColor\(([^)]*)\)|(\w+))', block.group(0)):
            shape = p.group(1)
            size = [float(v.strip().rstrip('f')) for v in p.group(2).split(',')]
            off = [float(v.strip().rstrip('f')) for v in p.group(3).split(',')]
            rot = [float(v.strip().rstrip('f')) for v in p.group(5).split(',')] if p.group(5) else [0, 0, 0]
            if p.group(7):
                col = tuple(float(v.strip().rstrip('f')) for v in p.group(7).split(','))[:3]
            else:
                col = NAMED.get(p.group(8), (0.7, 0.7, 0.7))
            parts.append((shape, size, off, rot, col))
        if parts:
            items[block.group(1)] = parts
    # The two auras are built in loops in C++; the same rings here.
    items['UsedHalo'] = [('Sphere', [5, 5, 5], [math.cos(2 * math.pi * i / 12) * 15, math.sin(2 * math.pi * i / 12) * 15, 16], [0, 0, 0], (2.5, 2.0, 0.6)) for i in range(12)]
    items['SponsorOrbs'] = [('Sphere', [8, 8, 8], [math.cos(2 * math.pi * i / 3) * 28, math.sin(2 * math.pi * i / 3) * 28, 4], [0, 0, 0],
                             [(0.3, 2.0, 2.5), (2.5, 0.4, 2.0), (2.5, 2.0, 0.3)][i]) for i in range(3)]
    return items


def build_cosmetic(parts):
    for shape, size, off, rot, col in parts:
        emit = max(col) > 1.0
        rgb = tuple(min(1.0, c / max(1.0, max(col))) for c in col)  # an over-bright glow colour keeps its hue
        m = B.mat('C_%d_%d_%d' % tuple(int(c * 100) for c in rgb) + ('E' if emit else ''), rgb, rough=0.45, emit=0.8 if emit else 0.0)
        d = [s / 100.0 for s in size]          # cm -> m, Unreal basic shapes are 100 units across
        loc = (off[0] / 100.0, off[1] / 100.0, off[2] / 100.0)
        # Unreal FRotator(Pitch, Yaw, Roll) -> Blender XYZ euler (roll about X, pitch about Y, yaw about Z)
        brot = (rot[2], rot[0], rot[1])
        if shape == 'Cube':
            B.box((d[0], d[1], d[2]), loc, m, rot=brot)
        elif shape == 'Sphere':
            B.sph(0.5, loc, m, scale=(d[0], d[1], d[2]), rot=brot)
        elif shape == 'Cylinder':
            o = B.cyl(0.5, 1.0, loc, m, rot=brot, verts=32)
            o.scale = (d[0], d[1], d[2])
        elif shape == 'Cone':
            o = B.cyl(0.5, 1.0, loc, m, rot=brot, verts=32, r2=0.0)
            o.scale = (d[0], d[1], d[2])


# ---- trophies and chop shop: small models made for the icons ----------------------------------------------
def PlatinumOre():
    m = B.mat('Platinum', (0.85, 0.88, 0.95), metal=1.0, rough=0.25)
    for i, (x, y, z, r) in enumerate(((0, 0, 0.04, 0.05), (0.04, 0.02, 0.03, 0.035), (-0.035, 0.025, 0.035, 0.03), (0.0, -0.04, 0.03, 0.03))):
        o = B.sph(r, (x, y, z), m, scale=(1.2, 0.9, 0.8))
        o.data.polygons.foreach_set('use_smooth', [False] * len(o.data.polygons))
    B.sph(0.02, (0.02, 0.0, 0.08), B.mat('Rock', (0.3, 0.27, 0.25), rough=0.9))


def CeremonialKnife():
    B.box((0.16, 0.025, 0.004), (0.06, 0, 0), B.mat('Obsidian', (0.05, 0.05, 0.08), metal=0.3, rough=0.15))
    B.cyl(0.012, 0.08, (-0.06, 0, 0), B.mat('Bone', (0.85, 0.8, 0.65), rough=0.6), rot=(0, 90, 0))
    B.tor(0.014, 0.004, (-0.02, 0, 0), B.GOLD, rot=(0, 90, 0))
    B.sph(0.008, (-0.1, 0, 0), B.RUBY)


def AssayLedger():
    leather = B.mat('Leather', (0.25, 0.12, 0.05), rough=0.8)
    B.box((0.14, 0.1, 0.025), (0, 0, 0.0125), leather, bevel=0.003)
    B.box((0.135, 0.095, 0.02), (0.003, 0, 0.0125), B.PAPER)
    B.text('1859', 0.025, (0, 0, 0.0255), B.GOLD)


def ClaimLantern():
    brass = B.mat('Brass', (0.8, 0.55, 0.2), metal=1.0, rough=0.35)
    B.cyl(0.04, 0.012, (0, 0, 0.006), brass)
    B.cyl(0.03, 0.09, (0, 0, 0.057), B.mat('LampGlass', (1.0, 0.75, 0.35), rough=0.1, emit=4.0))
    B.cyl(0.035, 0.015, (0, 0, 0.108), brass)
    B.tor(0.03, 0.003, (0, 0, 0.13), brass, rot=(90, 0, 0))


def PinkSlip():
    B.box((0.15, 0.1, 0.002), (0, 0, 0.001), B.mat('Pink', (0.95, 0.55, 0.7), rough=0.8), rot=(0, 0, 8))
    B.text('TITLE', 0.02, (0, 0.02, 0.0025), B.INK, rot=(0, 0, 8))
    B.box((0.06, 0.002, 0.0005), (0.02, -0.025, 0.0025), B.INK, rot=(0, 0, 8))


def SeizedChain():
    steel = B.mat('Chain', (1.0, 0.82, 0.35), metal=1.0, rough=0.3)
    for i in range(5):
        B.tor(0.025, 0.007, (i * 0.038 - 0.076, 0, 0.01), steel, rot=(90 if i % 2 else 0, 0, 0))


def LotManifest():
    B.box((0.12, 0.17, 0.006), (0, 0, 0.003), B.CARDBOARD, bevel=0.003)
    B.box((0.11, 0.15, 0.002), (0, -0.005, 0.007), B.PAPER)
    B.box((0.05, 0.02, 0.008), (0, 0.075, 0.009), B.STEEL)
    for i in range(5):
        B.box((0.08, 0.002, 0.0005), (0, 0.04 - i * 0.022, 0.0082), B.INK)


def VINPlate():
    B.box((0.16, 0.04, 0.003), (0, 0, 0.0015), B.mat('Alu', (0.8, 0.84, 0.9), metal=1.0, rough=0.35), bevel=0.002)
    B.text('1HGCM82633A', 0.011, (0, 0, 0.0032), B.INK)
    for x in (-0.07, 0.07):
        B.cyl(0.004, 0.004, (x, 0, 0.003), B.STEEL)


def Plating():
    plate = B.mat('Plate', (0.62, 0.66, 0.74), metal=0.9, rough=0.4)
    B.box((0.2, 0.14, 0.012), (0, 0, 0.006), plate, bevel=0.006)
    for x in (-0.08, 0.08):
        for y in (-0.055, 0.055):
            B.sph(0.007, (x, y, 0.013), B.STEEL)
    B.box((0.2, 0.012, 0.004), (0, 0, 0.014), B.HIVIS, rot=(0, 0, 35))


def Engine():
    block = B.mat('Block', (0.55, 0.12, 0.08), metal=0.5, rough=0.45)
    B.box((0.16, 0.11, 0.09), (0, 0, 0.045), block, bevel=0.006)
    for i in range(3):
        B.cyl(0.018, 0.04, (-0.05 + i * 0.05, 0, 0.11), B.STEEL)
    B.box((0.18, 0.02, 0.02), (0, -0.065, 0.06), B.GUNMETAL)
    B.cyl(0.03, 0.02, (0.09, 0, 0.045), B.GUNMETAL, rot=(0, 90, 0))


def Tank():
    can = B.mat('JerryCan', (0.9, 0.55, 0.15), rough=0.45)
    B.box((0.12, 0.05, 0.16), (0, 0, 0.08), can, bevel=0.008)
    B.box((0.06, 0.012, 0.02), (-0.02, 0, 0.17), can, bevel=0.004)
    B.cyl(0.012, 0.03, (0.045, 0, 0.17), B.STEEL)
    B.box((0.1, 0.002, 0.12), (0, 0.026, 0.08), B.mat('CanX', (0.7, 0.4, 0.1), rough=0.5), rot=(0, 40, 0))


def Line():
    B.cyl(0.07, 0.08, (0, 0, 0.04), B.GUNMETAL, rot=(90, 0, 0))
    B.cyl(0.055, 0.082, (0, 0, 0.04), B.mat('Cable', (0.15, 0.6, 0.85), metal=0.4, rough=0.5), rot=(90, 0, 0))
    B.cyl(0.004, 0.12, (0.1, 0, 0.06), B.STEEL, rot=(0, 70, 0))
    B.tor(0.02, 0.005, (0.16, 0, 0.03), B.HIVIS, rot=(90, 0, 0))


def PitCrew():
    wrench = B.mat('Wrench', (0.75, 0.78, 0.82), metal=1.0, rough=0.3)
    B.box((0.18, 0.02, 0.008), (0, 0, 0.004), wrench, rot=(0, 0, 35))
    B.tor(0.02, 0.008, (0.075, 0.052, 0.004), wrench)
    B.box((0.16, 0.025, 0.012), (0, 0, 0.012), B.mat('Grip', (0.2, 0.9, 0.4), rough=0.6), rot=(0, 0, -35))


# ---- Mine Outfitters (Dead Air kit, 2026-11-15) ------------------------------------------------------------
def AirTank():
    steel = B.mat('Tank', (0.25, 0.55, 0.75), metal=0.8, rough=0.35)
    B.cyl(0.045, 0.2, (0, 0, 0.1), steel)
    B.sph(0.045, (0, 0, 0.2), steel)
    B.cyl(0.015, 0.03, (0, 0, 0.255), B.STEEL)
    B.cyl(0.03, 0.012, (0, 0, 0.275), B.mat('Valve', (0.8, 0.15, 0.1), rough=0.4), rot=(90, 0, 0))
    B.box((0.006, 0.07, 0.006), (0.05, 0, 0.2), B.GUNMETAL)


def MineLamp():
    brass = B.mat('LampBrass', (0.75, 0.55, 0.25), metal=1.0, rough=0.35)
    B.cyl(0.05, 0.03, (0, 0, 0.06), brass, rot=(90, 0, 0))
    B.cyl(0.04, 0.005, (0, -0.018, 0.06), B.mat('LampLens', (1.0, 0.92, 0.7), rough=0.1, emit=6.0), rot=(90, 0, 0))
    B.box((0.02, 0.04, 0.08), (0, 0.03, 0.03), brass)
    B.tor(0.035, 0.004, (0, 0.05, 0.085), B.STEEL)


def Lightsticks():
    glow = B.mat('Glow', (0.2, 0.9, 0.25), rough=0.2, emit=1.2)
    for i, ang in enumerate((-20, 5, 30)):
        B.cyl(0.012, 0.16, (i * 0.035 - 0.035, 0, 0.012), glow, rot=(90, 0, ang))
        B.cyl(0.013, 0.02, (i * 0.035 - 0.035, 0, 0.012), B.mat('Cap', (0.15, 0.15, 0.17), rough=0.6), rot=(90, 0, ang))


def HardHat():
    shell = B.mat('Hat', (0.95, 0.7, 0.1), rough=0.4)
    B.sph(0.08, (0, 0, 0.0), shell, scale=(1.0, 1.15, 0.75))
    B.cyl(0.1, 0.008, (0, 0.01, 0.004), shell)
    B.box((0.012, 0.13, 0.012), (0, 0, 0.06), shell)
    B.cyl(0.018, 0.012, (0, -0.075, 0.035), B.mat('HatLamp', (1.0, 0.9, 0.6), emit=4.0), rot=(90, 0, 0))


def Harness():
    rope = B.mat('Rope', (0.8, 0.65, 0.4), rough=0.9)
    for i in range(4):
        B.tor(0.07 - i * 0.004, 0.009, (0, 0, 0.01 + i * 0.016), rope)
    B.tor(0.025, 0.006, (0.09, 0, 0.03), B.STEEL, rot=(90, 0, 30))


def GasMeter():
    B.box((0.11, 0.06, 0.15), (0, 0, 0.075), B.mat('Meter', (0.2, 0.25, 0.3), metal=0.4, rough=0.5), bevel=0.006)
    B.cyl(0.035, 0.006, (0, -0.031, 0.095), B.PAPER, rot=(90, 0, 0))
    B.box((0.003, 0.002, 0.03), (0.008, -0.035, 0.1), B.mat('Needle', (0.9, 0.1, 0.1)), rot=(0, 30, 0))
    B.cyl(0.008, 0.004, (-0.03, -0.031, 0.035), B.mat('LED', (0.3, 0.6, 1.0), emit=5.0), rot=(90, 0, 0))


def TapeReel():
    reel = B.mat('Reel', (0.2, 0.2, 0.22), metal=0.6, rough=0.4)
    B.cyl(0.08, 0.008, (0, 0, 0.004), reel)
    B.cyl(0.055, 0.02, (0, 0, 0.018), B.mat('Film', (0.25, 0.12, 0.08), rough=0.3))
    B.cyl(0.08, 0.008, (0, 0, 0.032), reel)
    for i in range(3):
        a = math.radians(i * 120)
        B.cyl(0.015, 0.01, (math.cos(a) * 0.045, math.sin(a) * 0.045, 0.037), B.mat('Hole', (0.05, 0.05, 0.05)))


SPECIALS = {
    'AirTank': AirTank, 'MineLamp': MineLamp, 'Lightsticks': Lightsticks, 'HardHat': HardHat, 'Harness': Harness,
    'GasMeter': GasMeter, 'TapeReel': TapeReel,
    'PlatinumOre': PlatinumOre, 'CeremonialKnife': CeremonialKnife, 'AssayLedger': AssayLedger, 'ClaimLantern': ClaimLantern,
    'PinkSlip': PinkSlip, 'SeizedChain': SeizedChain, 'LotManifest': LotManifest, 'VINPlate': VINPlate,
    'Plating': Plating, 'Engine': Engine, 'Tank': Tank, 'Line': Line, 'PitCrew': PitCrew,
    'ConfettiCrossbow': B.ConfettiCrossbow, 'NailGunPro': B.NailGunPro, 'FlareGunEmergency': B.FlareGunEmergency,
}


# ---- Fourteener's Trading Post (2026-11-15) ------------------------------------------------------------------
def GrapplingHook():
    B.cyl(0.008, 0.12, (0, 0, 0.06), B.STEEL)
    for ang in (0, 120, 240):
        a = math.radians(ang)
        B.cyl(0.006, 0.07, (math.cos(a) * 0.03, math.sin(a) * 0.03, 0.02), B.STEEL, rot=(0, 60, ang))
    B.tor(0.012, 0.004, (0, 0, 0.125), B.STEEL, rot=(90, 0, 0))
    B.tor(0.05, 0.006, (0.05, 0, 0.05), B.mat('HookRope', (0.75, 0.6, 0.35), rough=0.9))


def Piton():
    B.box((0.012, 0.03, 0.13), (0, 0, 0.065), B.STEEL, rot=(0, 15, 0))
    B.tor(0.02, 0.005, (-0.01, 0, 0.13), B.mat('Carabiner', (1.0, 0.45, 0.1), metal=0.7, rough=0.35), rot=(90, 0, 0))


def TrailMix():
    bag = B.mat('Bag', (0.85, 0.6, 0.3), rough=0.7)
    B.box((0.1, 0.05, 0.12), (0, 0, 0.06), bag, bevel=0.01)
    for i, (x, y, c) in enumerate(((-0.02, -0.03, (0.4, 0.22, 0.1)), (0.02, -0.03, (0.9, 0.85, 0.7)), (0.0, -0.035, (0.85, 0.1, 0.1)))):
        B.sph(0.012, (x, y, 0.12 + i * 0.008), B.mat('Nut%d' % i, c, rough=0.6))


def GranolaBar():
    B.box((0.14, 0.045, 0.02), (0, 0, 0.01), B.mat('Wrapper', (0.25, 0.55, 0.3), rough=0.4), bevel=0.004)
    B.box((0.04, 0.046, 0.021), (0.05, 0, 0.01), B.mat('Oats', (0.8, 0.65, 0.35), rough=0.8))


def GreenChileBurrito():
    B.cyl(0.035, 0.15, (0, 0, 0.035), B.mat('Foil', (0.8, 0.82, 0.85), metal=1.0, rough=0.35), rot=(0, 90, 0))
    B.cyl(0.033, 0.03, (0.085, 0, 0.035), B.mat('Tortilla', (0.9, 0.8, 0.55), rough=0.7), rot=(0, 90, 0))
    B.cyl(0.02, 0.005, (0.1, 0, 0.035), B.mat('Chile', (0.35, 0.65, 0.2), rough=0.5), rot=(0, 90, 0))


def Jerky():
    meat = B.mat('Jerky', (0.4, 0.15, 0.08), rough=0.7)
    for i, ang in enumerate((-15, 10, 35)):
        B.box((0.13, 0.03, 0.006), (0, i * 0.012 - 0.012, 0.004 + i * 0.006), meat, rot=(0, 0, ang))


def Apple():
    B.sph(0.05, (0, 0, 0.05), B.mat('AppleRed', (0.85, 0.08, 0.08), rough=0.35), scale=(1.0, 1.0, 0.92))
    B.cyl(0.004, 0.025, (0, 0, 0.1), B.mat('Stem', (0.3, 0.18, 0.08)))
    B.sph(0.015, (0.015, 0, 0.105), B.mat('Leaf', (0.2, 0.6, 0.2), rough=0.5), scale=(1.2, 0.4, 0.2))


def ChalkBag():
    B.cyl(0.045, 0.09, (0, 0, 0.045), B.mat('ChalkBagCloth', (0.15, 0.4, 0.75), rough=0.8))
    B.tor(0.045, 0.006, (0, 0, 0.09), B.mat('Fleece', (0.95, 0.95, 0.95), rough=0.9))
    B.sph(0.03, (0, 0, 0.095), B.mat('Chalk', (1.0, 1.0, 1.0), rough=1.0), scale=(1.0, 1.0, 0.4))


def Crampons():
    frame = B.STEEL
    B.box((0.13, 0.05, 0.006), (0, 0, 0.03), frame)
    for i in range(5):
        for side in (-1, 1):
            B.cyl(0.004, 0.03, (-0.05 + i * 0.025, side * 0.02, 0.012), frame, r2=0.0, rot=(180, 0, 0))
    B.tor(0.03, 0.004, (0, 0, 0.05), B.mat('Strap', (0.85, 0.2, 0.15), rough=0.6), rot=(0, 0, 0))


def WindCloak():
    cloth = B.mat('Cloak', (0.4, 0.7, 0.55), rough=0.8)
    B.box((0.13, 0.008, 0.16), (0, 0, 0.08), cloth, rot=(8, 0, 0))
    B.box((0.08, 0.04, 0.012), (0, 0.01, 0.16), cloth)
    B.sph(0.008, (0, -0.008, 0.155), B.GOLD)


def SafetyLine():
    rope = B.mat('SafetyRope', (1.0, 0.45, 0.2), rough=0.85)
    for i in range(3):
        B.tor(0.06 - i * 0.006, 0.007, (0, 0, 0.007 + i * 0.012), rope)
    B.tor(0.022, 0.005, (0.075, 0, 0.02), B.STEEL, rot=(90, 0, 20))


SPECIALS.update({
    'GrapplingHook': GrapplingHook, 'Piton': Piton, 'TrailMix': TrailMix, 'GranolaBar': GranolaBar,
    'GreenChileBurrito': GreenChileBurrito, 'Jerky': Jerky, 'Apple': Apple, 'ChalkBag': ChalkBag,
    'Crampons': Crampons, 'WindCloak': WindCloak, 'SafetyLine': SafetyLine,
})


# ---- the shot --------------------------------------------------------------------------------------------
def setup_scene():
    scene = bpy.context.scene
    try:
        scene.render.engine = 'BLENDER_EEVEE_NEXT'
    except TypeError:
        scene.render.engine = 'BLENDER_EEVEE'
    scene.render.resolution_x = scene.render.resolution_y = 256
    scene.render.film_transparent = True
    scene.render.image_settings.file_format = 'PNG'
    scene.render.image_settings.color_mode = 'RGBA'
    scene.view_settings.view_transform = 'Standard'
    cam = bpy.data.objects.new('Cam', bpy.data.cameras.new('Cam'))
    scene.collection.objects.link(cam)
    cam.data.type = 'ORTHO'
    scene.camera = cam
    lights = []
    # A warm key, a soft fill, and a cyan rim from behind: the menus are dark glass with cyan edges.
    for name, loc, energy, colour in [('Key', (-2, -3, 3), 600, (1.0, 0.95, 0.9)), ('Fill', (3, -1, 1), 180, (0.9, 0.9, 1.0)),
                                       ('Rim', (1, 4, 2), 900, (0.35, 0.85, 1.0))]:
        l = bpy.data.objects.new(name, bpy.data.lights.new(name, 'AREA'))
        l.data.energy = energy
        l.data.size = 3
        l.data.color = colour
        l.location = loc
        l.rotation_euler = (-Vector(loc)).to_track_quat('-Z', 'Y').to_euler()
        scene.collection.objects.link(l)
        lights.append(l)
    world = bpy.data.worlds.new('W')
    scene.world = world
    world.use_nodes = True
    world.node_tree.nodes['Background'].inputs[0].default_value = (0.05, 0.05, 0.08, 1)
    world.node_tree.nodes['Background'].inputs[1].default_value = 0.6
    return scene, cam, lights


def frame(cam, objs):
    pts = [o.matrix_world @ Vector(c) for o in objs for c in o.bound_box]
    lo = Vector((min(p.x for p in pts), min(p.y for p in pts), min(p.z for p in pts)))
    hi = Vector((max(p.x for p in pts), max(p.y for p in pts), max(p.z for p in pts)))
    centre = (lo + hi) / 2
    size = (hi - lo).length
    # A three-quarter view from the front-left and a little above.
    direction = Vector((-0.55, -0.75, 0.42)).normalized()
    cam.location = centre + direction * (size * 4 + 1)
    cam.rotation_euler = (-direction).to_track_quat('-Z', 'Y').to_euler()
    cam.data.ortho_scale = size * 1.12
    cam.data.clip_end = size * 20 + 10


def clear_items():
    for o in list(bpy.data.objects):
        if o.type in ('MESH', 'FONT', 'CURVE'):
            bpy.data.objects.remove(o)


def main():
    for o in list(bpy.data.objects):
        bpy.data.objects.remove(o)
    scene, cam, _ = setup_scene()
    jobs = [(k, ('cos', v)) for k, v in parse_cosmetics().items()] + [(k, ('fn', f)) for k, f in SPECIALS.items()]
    for item_id, (kind, what) in jobs:
        if ONLY and item_id not in ONLY:
            continue
        clear_items()
        B.PARTS.clear()
        if kind == 'cos':
            build_cosmetic(what)
        else:
            what()
        bpy.context.view_layer.update()  # scales set after creation are not in matrix_world until this
        objs = [o for o in bpy.data.objects if o.type == 'MESH']
        if not objs:
            print('SKIP', item_id)
            continue
        frame(cam, objs)
        scene.render.filepath = os.path.abspath(os.path.join(OUT, 'T_Icon_%s.png' % item_id))
        bpy.ops.render.render(write_still=True)
        print('ICON', item_id)


main()
