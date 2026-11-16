"""Enter the tale floor, clear it, and check the stairs build floor 2.

    python Tools/pie_test_tale_descend.py
"""
import json
import sys
import time
from pathlib import Path

sys.path.insert(0, str(Path(__file__).resolve().parent))
from mcp_call import McpSession, unwrap  # noqa: E402

s = McpSession()


def rv(r):
    v = unwrap(r)
    try:
        v = json.loads(v)
    except (TypeError, ValueError):
        return v
    return v.get("returnValue", v) if isinstance(v, dict) else v


def find(cls, pie=True):
    r = rv(s.call_tool("SceneTools", "find_actors", {
        "name": "", "tag": "", "collision_channels": [],
        "actor_type": {"refPath": cls},
    })) or []
    return [a["refPath"] for a in r if ("UEDPIE" in a["refPath"]) == pie]


def loc(ref):
    return rv(s.call_tool("ActorTools", "get_actor_transform", {"actor": {"refPath": ref}}))["location"]


def move(ref, x, y, z):
    t = rv(s.call_tool("ActorTools", "get_actor_transform", {"actor": {"refPath": ref}}))
    t["location"] = {"x": x, "y": y, "z": z}
    rv(s.call_tool("ActorTools", "set_actor_transform", {"actor": {"refPath": ref}, "xform": t}))


def props(ref, names):
    v = rv(s.call_tool("ObjectTools", "get_properties", {"instance": {"refPath": ref}, "properties": names}))
    return json.loads(v) if isinstance(v, str) else v


def logs(pattern):
    return [str(e) for e in (rv(s.call_tool("LogsToolset", "GetLogEntries", {"category": "", "pattern": pattern})) or [])]


def main():
    if rv(s.call_tool("EditorAppToolset", "IsPIERunning", {})) is True:
        rv(s.call_tool("EditorAppToolset", "StopPIE", {}))
        time.sleep(2)

    arches = find("/Script/DungeonCrawlerDenver.DCCDungeonEntrance", pie=False)
    if not arches:
        rv(s.call_tool("EditorAppToolset", "SetCameraTransform", {"transform": {
            "location": {"x": -27600, "y": 26500, "z": 800},
            "rotation": {"pitch": -25, "yaw": 0, "roll": 0},
            "scale": {"x": 1, "y": 1, "z": 1},
        }}))
        end = time.time() + 40
        while time.time() < end and not arches:
            time.sleep(2)
            arches = find("/Script/DungeonCrawlerDenver.DCCDungeonEntrance", pie=False)
    if not arches:
        print("FAIL no arch in the editor world")
        return 1

    rv(s.call_tool("EditorAppToolset", "StartPIE", {
        "options": {"bSimulate": False, "playMode": "PlayMode_InViewPort", "warmupSeconds": 4},
    }))
    pawns = [p for p in find("/Script/DungeonCrawlerDenver.DCCCharacterBase") if "Enemy" not in p and "Ally" not in p]
    if not pawns:
        print("FAIL no pawn")
        return 1
    pawn = pawns[0]
    end = time.time() + 90
    while time.time() < end and props(pawn + ".Onboarding", ["CrawlerId"]).get("CrawlerId") in (None, "None"):
        time.sleep(1)

    rv(s.call_tool("ObjectTools", "set_properties", {
        "instance": {"refPath": pawn + ".Onboarding"},
        "values": json.dumps({"Stage": "FirstFightWon"}),
    }))
    el = loc(find("/Script/DungeonCrawlerDenver.DCCDungeonEntrance")[0])
    move(pawn, el["x"], el["y"], el["z"] + 20)

    end = time.time() + 60
    while time.time() < end and loc(pawn)["x"] < 100000:
        time.sleep(1)
    here = loc(pawn)
    print("arrived", {k: round(v) for k, v in here.items()})
    if here["x"] < 100000:
        print("FAIL did not enter")
        for line in logs("DCC Tale")[-6:]:
            print("   ", line[-180:])
        return 1

    # Keep the crawler alive, then drop every floor enemy so the stairs unlock.
    rv(s.call_tool("ObjectTools", "set_properties", {
        "instance": {"refPath": pawn + ".Stats"},
        "values": json.dumps({"CurrentHitPoints": 99}),
    }))
    cleared = 0
    for e in find("/Script/DungeonCrawlerDenver.DCCEnemyCharacter"):
        p = loc(e)
        if abs(p["x"] - 150000) < 20000 and abs(p["y"] - 150000) < 20000:
            rv(s.call_tool("ObjectTools", "set_properties", {
                "instance": {"refPath": e + ".Stats"},
                "values": json.dumps({"CurrentHitPoints": 0}),
            }))
            cleared += 1
    print("cleared", cleared)

    floor = find("/Script/DungeonCrawlerDenver.DCCTaleFloor")[0]
    pad = props(floor + ".DownPad", ["RelativeLocation"])
    print("down pad", pad)
    base = loc(floor)
    rel = (pad or {}).get("RelativeLocation") or {}
    px = base["x"] + float(rel.get("x", rel.get("X", 0)))
    py = base["y"] + float(rel.get("y", rel.get("Y", 0)))
    pz = base["z"] + float(rel.get("z", rel.get("Z", 100)))
    print("pad world", round(px), round(py), round(pz))
    move(pawn, px, py, pz)

    end = time.time() + 25
    while time.time() < end and not any("Floor 2" in ln for ln in logs("Floor 2")):
        time.sleep(1)
    print("now", {k: round(v) for k, v in loc(pawn).items()})
    tale = logs("DCC Tale")
    for line in tale[-10:]:
        print("   ", line[-200:])
    if not any("Floor 2" in ln for ln in tale):
        print("FAIL stairs did not open floor 2")
        return 1
    floor2 = [e for e in find("/Script/DungeonCrawlerDenver.DCCEnemyCharacter")
              if abs(loc(e)["x"] - 150000) < 20000 and abs(loc(e)["y"] - 150000) < 20000
              and props(e + ".Stats", ["CurrentHitPoints"]).get("CurrentHitPoints", 0) > 0]
    print("living floor-2 enemies", len(floor2))
    print("PASS floor 2")
    return 0


if __name__ == "__main__":
    sys.exit(main())
