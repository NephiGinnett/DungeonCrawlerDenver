"""Build the first-ten-minutes area in Lvl_ThirdPerson over MCP (Docs/ROADMAP.md section 3).

    python Tools/build_onboarding_wing.py            # build (re-running first removes what it built before)

Everything goes in outliner folders under DCC/Onboarding, far from the rest of the map (open space around
x=-30000, y=30000, checked empty with trace_world). Layout, west to east along "Home Street":

  six bedrooms (CJ, Scott, Marcel, Vaughn, Kyle, Zoe), each with a PlayerStart tagged with that name,
  furniture from Modern_House_V1, a light, and a glowing ADCCPersonalItem
  -> the plaza, with ADCCDungeonEntrance (an archway) and ADCCSkyBeacon above it
  six solo trial arenas to the south, 4000 apart (enemy aggro reaches 2000), each with nav and an ADCCSoloTrial
  the safe room, with two ADCCOnboardingTriggers (vitals, then the full interface)
  ADCCOnboardingSettings, which is what switches the HUD gating on

Blockout quality on purpose. Zoe prefers to nudge placement herself.
"""
import json
import sys
from pathlib import Path

sys.path.insert(0, str(Path(__file__).resolve().parent))
from mcp_call import McpSession, unwrap  # noqa: E402

s = McpSession()
ROOT_FOLDER = "DCC/Onboarding"
OX, OY = -30000.0, 30000.0  # Home Street origin
CUBE = "/Engine/BasicShapes/Cube.Cube"
CYL = "/Engine/BasicShapes/Cylinder.Cylinder"
SPHERE = "/Engine/BasicShapes/Sphere.Sphere"
MH = "/Game/Modern_House_V1/Mesh/"
DCC = "/Script/DungeonCrawlerDenver."
built = []


def rv(r):
    v = unwrap(r)
    try:
        v = json.loads(v)
    except (TypeError, ValueError):
        return v
    return v.get("returnValue", v) if isinstance(v, dict) else v


def xf(x, y, z, yaw=0.0, sx=1.0, sy=1.0, sz=1.0, pitch=0.0, roll=0.0):
    return {"location": {"x": x, "y": y, "z": z}, "rotation": {"pitch": pitch, "yaw": yaw, "roll": roll},
            "scale": {"x": sx, "y": sy, "z": sz}}


def place_mesh(label, mesh, folder, **t):
    r = rv(s.call_tool("SceneTools", "add_to_scene_from_asset", {"asset_path": mesh, "name": label, "xform": xf(**t)}))
    return _file(r, label, folder)


def place_class(label, cls, folder, **t):
    r = rv(s.call_tool("SceneTools", "add_to_scene_from_class", {"actor_type": {"refPath": cls}, "name": label, "xform": xf(**t)}))
    return _file(r, label, folder)


def _file(r, label, folder):
    ref = r.get("refPath") if isinstance(r, dict) else None
    if not ref:
        print("  ! failed:", label, str(r)[:200])
        return None
    rv(s.call_tool("SceneTools", "set_actor_folder", {"actor": {"refPath": ref}, "folder_path": f"{ROOT_FOLDER}/{folder}"}))
    built.append(ref)
    return ref


def setp(ref, values):
    if not ref:
        return None
    return rv(s.call_tool("ObjectTools", "set_properties", {"instance": {"refPath": ref}, "values": json.dumps(values)}))


def box_room(name, folder, cx, cy, sx, sy, h=300, door=None, roof=True, light=True):
    """Floor, four walls and an optional roof, from scaled engine cubes. door: 'S','N','E','W' leaves a 220-wide gap."""
    t = 20
    place_mesh(f"{name}_Floor", CUBE, folder, x=cx, y=cy, z=-10, sx=sx / 100, sy=sy / 100, sz=t / 100)
    walls = {
        "N": (cx, cy + sy / 2, sx, t), "S": (cx, cy - sy / 2, sx, t),
        "E": (cx + sx / 2, cy, t, sy), "W": (cx - sx / 2, cy, t, sy),
    }
    for side, (wx, wy, wlen_x, wlen_y) in walls.items():
        if door == side:
            along_x = wlen_x > wlen_y
            length = wlen_x if along_x else wlen_y
            piece = (length - 220) / 2
            for sign in (-1, 1):
                off = sign * (220 / 2 + piece / 2)
                px, py = (wx + off, wy) if along_x else (wx, wy + off)
                place_mesh(f"{name}_Wall{side}{'ab'[sign > 0]}", CUBE, folder, x=px, y=py, z=h / 2,
                           sx=(piece if along_x else t) / 100, sy=(t if along_x else piece) / 100, sz=h / 100)
        else:
            place_mesh(f"{name}_Wall{side}", CUBE, folder, x=wx, y=wy, z=h / 2, sx=wlen_x / 100, sy=wlen_y / 100, sz=h / 100)
    if roof:
        place_mesh(f"{name}_Roof", CUBE, folder, x=cx, y=cy, z=h + t / 2, sx=sx / 100, sy=sy / 100, sz=t / 100)
    if light:
        lamp = place_class(f"{name}_Light", "/Script/Engine.PointLight", folder, x=cx, y=cy, z=h - 40)
        # The engine default blew the white rooms out to pure white under auto exposure. Warm and dim reads as "home".
        setp(f"{lamp}.LightComponent0", {"Intensity": 900, "AttenuationRadius": max(sx, sy) * 0.95,
                                         "LightColor": {"R": 255, "G": 214, "B": 170, "A": 255}})


def clear_previous():
    folders = rv(s.call_tool("SceneTools", "get_folders", {})) or []
    for f in folders:
        path = f if isinstance(f, str) else f.get("path", "")
        if path.startswith(ROOT_FOLDER):
            for a in rv(s.call_tool("SceneTools", "get_actors_in_folder", {"folder_path": path})) or []:
                rv(s.call_tool("SceneTools", "remove_from_scene", {"actor": a}))
    print("cleared previous build")


# --- Bedrooms -------------------------------------------------------------------------------------------------

CRAWLERS = [
    # id, equipment row, item look (mesh, scale), furniture extras
    ("CJ", "CJBoardGame", (CUBE, (0.45, 0.35, 0.07)), ["SM_Table_1", "SM_Chair_1"]),
    ("Scott", "ScottLacrosseStick", (CYL, (0.05, 0.05, 1.1)), ["SM_Sofa_1"]),
    ("Marcel", "MarcelLuckyDice", (CUBE, (0.14, 0.14, 0.14)), ["SM_Table_1", "SM_Chair_1"]),
    ("Vaughn", "", (SPHERE, (0.25, 0.25, 0.25)), ["SM_Shelf_1"]),
    ("Kyle", "", (SPHERE, (0.25, 0.25, 0.25)), ["SM_Sofa_1"]),
    ("Zoe", "", (SPHERE, (0.25, 0.25, 0.25)), ["SM_Shelf_1", "SM_TV"]),
]


def bedrooms():
    for i, (cid, item_row, (item_mesh, item_scale), extras) in enumerate(CRAWLERS):
        folder = f"Home/{cid}"
        cx, cy = OX + i * 1000, OY + 900
        box_room(f"Bedroom_{cid}", folder, cx, cy, 800, 800, door="S")
        # Corner-pivot furniture: the bed's footprint is x 0..283, y -242..0.
        place_mesh(f"Bedroom_{cid}_Bed", MH + "SM_Bed.SM_Bed", folder, x=cx - 380, y=cy + 380, z=0)
        place_mesh(f"Bedroom_{cid}_Carpet", MH + "SM_Carpet.SM_Carpet", folder, x=cx + 130, y=cy + 200, z=1)
        place_mesh(f"Bedroom_{cid}_Lamp", MH + "SM_Lamp_1.SM_Lamp_1", folder, x=cx - 60, y=cy + 370, z=0)
        for k, ex in enumerate(extras):
            place_mesh(f"Bedroom_{cid}_{ex}", MH + f"{ex}.{ex}", folder, x=cx + 380 - k * 60, y=cy - 100 + k * 300, z=0, yaw=90)
        # Open floor in front of the bed. (The first try put the start inside the bed's footprint, x -380..-97,
        # y +138..+380, and the pawn refused to spawn: "SpawnActor failed because of collision".)
        start = place_class(f"Start_{cid}", "/Script/Engine.PlayerStart", folder, x=cx - 100, y=cy - 200, z=110, yaw=90)
        setp(start, {"PlayerStartTag": cid})
        item = place_class(f"PersonalItem_{cid}", DCC + "DCCPersonalItem", folder, x=cx + 150, y=cy + 260, z=80)
        setp(item, {"OwnerCrawlerId": cid, "EquipmentItemId": item_row or "None"})
        setp(f"{item}.ItemMesh", {"StaticMesh": item_mesh, "RelativeScale3D": {"X": item_scale[0], "Y": item_scale[1], "Z": item_scale[2]}})
        print(f"  bedroom {cid}")


# --- Street, plaza, entrance -----------------------------------------------------------------------------------

def street_and_plaza():
    folder = "Street"
    length = 7000
    place_mesh("HomeStreet_Floor", CUBE, folder, x=OX + length / 2 - 600, y=OY, z=-10, sx=length / 100, sy=10, sz=0.2)
    px = OX + 7000
    place_mesh("Plaza_Floor", CUBE, folder, x=px, y=OY, z=-10, sx=24, sy=24, sz=0.2)
    place_class("Street_Sun", "/Script/Engine.PointLight", folder, x=OX + 2500, y=OY, z=900)
    # The archway: two pillars and a lintel, with the entrance volume in the gap.
    gx = px + 1100
    for side in (-1, 1):
        place_mesh(f"Gate_Pillar{'LR'[side > 0]}", CUBE, folder, x=gx, y=OY + side * 320, z=300, sx=1, sy=1, sz=6)
    place_mesh("Gate_Lintel", CUBE, folder, x=gx, y=OY, z=640, sx=1.2, sy=7.4, sz=0.8)
    entrance = place_class("DungeonEntrance", DCC + "DCCDungeonEntrance", folder, x=gx, y=OY, z=200)
    beacon = place_class("SkyBeacon", DCC + "DCCSkyBeacon", folder, x=gx, y=OY, z=3200)
    settings = place_class("OnboardingSettings", DCC + "DCCOnboardingSettings", folder, x=OX, y=OY, z=200)
    return entrance, beacon, settings


# --- Solo trials and the safe room -----------------------------------------------------------------------------

def trials(safe_point):
    refs = []
    for k in range(6):
        folder = f"Trials/Trial{k + 1}"
        tx, ty = OX + k * 4000, OY - 8000
        # Trial-local layout (DCCOnboardingActors.cpp): arrival -700, enemies around +300, exits at +1100.
        box_room(f"Trial{k + 1}", folder, tx + 200, ty, 2600, 1600, h=500, roof=False)
        nav = place_class(f"Trial{k + 1}_Nav", "/Script/NavigationSystem.NavMeshBoundsVolume", folder,
                          x=tx + 200, y=ty, z=150, sx=13.5, sy=8.5, sz=3)
        trial = place_class(f"SoloTrial{k + 1}", DCC + "DCCSoloTrial", folder, x=tx, y=ty, z=0)
        setp(trial, {"SafeRoomDestination": safe_point})
        refs.append(trial)
    print("  trials:", len([r for r in refs if r]))
    return refs


def safe_room():
    folder = "SafeRoom"
    cx, cy = OX + 3000, OY - 3500
    box_room("SafeRoom", folder, cx, cy, 1600, 1600, h=400)
    place_class("SafeRoom_Light2", "/Script/Engine.PointLight", folder, x=cx - 500, y=cy, z=330)
    place_mesh("SafeRoom_Sofa", MH + "SM_Sofa_1.SM_Sofa_1", folder, x=cx + 400, y=cy + 700, z=0)
    place_mesh("SafeRoom_Table", MH + "SM_Table_1.SM_Table_1", folder, x=cx + 150, y=cy + 300, z=0)
    place_mesh("SafeRoom_TV", MH + "SM_TV.SM_TV", folder, x=cx + 75, y=cy - 780, z=120, yaw=180)
    arrival = place_class("SafeRoom_Arrival", "/Script/Engine.TargetPoint", folder, x=cx - 600, y=cy, z=100)
    t1 = place_class("SafeRoom_Trigger_Welcome", DCC + "DCCOnboardingTrigger", folder, x=cx - 600, y=cy, z=150, sx=1.5, sy=1.5)
    setp(t1, {"MinimumStage": "FirstFightWon", "bAdvanceStage": True, "AdvanceToStage": "SafeRoom", "WindowRows": ["SafeRoomWelcome"]})
    t2 = place_class("SafeRoom_Trigger_Interface", DCC + "DCCOnboardingTrigger", folder, x=cx, y=cy, z=150, sx=1.2, sy=3.5)
    setp(t2, {"MinimumStage": "SafeRoom", "bAdvanceStage": True, "AdvanceToStage": "Complete", "WindowRows": ["InterfaceIssued"]})
    return arrival


def main():
    clear_previous()
    bedrooms()
    entrance, beacon, settings = street_and_plaza()
    arrival = safe_room()
    trial_refs = trials(arrival)
    print("entrance trials:", setp(entrance, {"Trials": [r for r in trial_refs if r], "OverflowDestination": arrival}))
    # Onward from the trials: back to the main map's usual start, where Floor 1 begins.
    onward = place_class("Trials_OnwardPoint", "/Script/Engine.TargetPoint", "Trials", x=0, y=600, z=300)
    for r in trial_refs:
        setp(r, {"ContinueDestination": onward})
    print("saving level...", rv(s.call_tool("AssetTools", "save_assets", {"asset_paths": ["/Game/ThirdPerson/Lvl_ThirdPerson"]})))
    print(f"built {len(built)} actors")


if __name__ == "__main__":
    main()
