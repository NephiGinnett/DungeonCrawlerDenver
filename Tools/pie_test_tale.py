"""Drop into the generated dungeon in PIE and report whether the floor, enemies, and stairs exist.

    python Tools/pie_test_tale.py

There is no input injection, so this sets the onboarding stage and teleports onto the plaza arch.
That overlap is the same path a crawler takes after the first fight. Then it stands the crawler
near one enemy and watches whether that enemy closes the distance.
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


def stream_arch():
    """World Partition hides actors whose cell is unloaded. The plaza is the onboarding wing."""
    found = find("/Script/DungeonCrawlerDenver.DCCDungeonEntrance", pie=False)
    if found:
        return found
    rv(s.call_tool("EditorAppToolset", "SetCameraTransform", {"transform": {
        "location": {"x": -27600, "y": 26500, "z": 800},
        "rotation": {"pitch": -25, "yaw": 0, "roll": 0},
        "scale": {"x": 1, "y": 1, "z": 1},
    }}))
    end = time.time() + 40
    while time.time() < end:
        time.sleep(2)
        found = find("/Script/DungeonCrawlerDenver.DCCDungeonEntrance", pie=False)
        if found:
            return found
    return []


def main():
    if rv(s.call_tool("EditorAppToolset", "IsPIERunning", {})) is True:
        rv(s.call_tool("EditorAppToolset", "StopPIE", {}))
        time.sleep(2)

    editor_arches = stream_arch()
    print("editor arches", len(editor_arches))

    rv(s.call_tool("EditorAppToolset", "StartPIE", {
        "options": {"bSimulate": False, "playMode": "PlayMode_InViewPort", "warmupSeconds": 4},
    }))

    pawns = [p for p in find("/Script/DungeonCrawlerDenver.DCCCharacterBase") if "Enemy" not in p and "Ally" not in p]
    if not pawns:
        print("FAIL no player pawn")
        return 1
    pawn = pawns[0]
    end = time.time() + 90
    while time.time() < end and props(pawn + ".Onboarding", ["CrawlerId"]).get("CrawlerId") in (None, "None"):
        time.sleep(1)
    print("pawn", pawn)
    print("before", props(pawn + ".Onboarding", ["CrawlerId", "Stage", "UnlockedFeatures"]))

    # FirstFightWon is the archway's "the trial is over, the dungeon is open" gate.
    wrote = rv(s.call_tool("ObjectTools", "set_properties", {
        "instance": {"refPath": pawn + ".Onboarding"},
        "values": json.dumps({"Stage": "FirstFightWon"}),
    }))
    print("set stage", wrote, props(pawn + ".Onboarding", ["Stage"]))

    entrances = find("/Script/DungeonCrawlerDenver.DCCDungeonEntrance")
    if not entrances:
        print("FAIL no dungeon entrance in the level")
        return 1
    el = loc(entrances[0])
    print("arch", {k: round(v) for k, v in el.items()})
    move(pawn, el["x"], el["y"], el["z"] + 20)

    floor_ref = None
    end = time.time() + 90
    while time.time() < end:
        floors = find("/Script/DungeonCrawlerDenver.DCCTaleFloor")
        if floors:
            floor_ref = floors[0]
            here = loc(pawn)
            if here["x"] > 100000:
                break
        time.sleep(1)

    here = loc(pawn)
    print("crawler at", {k: round(v) for k, v in here.items()})
    if not floor_ref:
        print("FAIL tale floor was not spawned")
        for line in logs("DCC Tale")[-12:]:
            print("   ", line[-220:])
        return 1

    print("floor", floor_ref, {k: round(v) for k, v in loc(floor_ref).items()})
    enemies = find("/Script/DungeonCrawlerDenver.DCCEnemyCharacter")
    near = []
    for e in enemies:
        p = loc(e)
        if abs(p["x"] - 150000) < 20000 and abs(p["y"] - 150000) < 20000:
            near.append((e, p))
    print(f"enemies on the floor: {len(near)} (world enemies {len(enemies)})")
    for e, p in near[:8]:
        name = props(e, ["DisplayName"])
        print("   ", {k: round(v) for k, v in p.items()}, str(name)[:80])

    chests = [c for c in find("/Script/DungeonCrawlerDenver.DCCLootContainer")
              if abs(loc(c)["x"] - 150000) < 20000]
    print(f"chests on the floor: {len(chests)}")

    for line in logs("DCC Tale")[-15:]:
        print("   ", line[-220:])

    if here["z"] < 4000:
        print("FAIL crawler fell through the floor")
        return 1
    if len(near) < 4:
        print("FAIL expected a staffed floor")
        return 1

    # Stand within aggro of one enemy and see if it walks closer. A backgrounded
    # editor ticks slowly, so this polls for up to two minutes.
    # 1200 is inside aggro (1500) and outside melee and the Beholder's 900 reach,
    # so a working enemy has to walk, not just stand and swing.
    target, tp = near[0]
    move(pawn, tp["x"] + 1200, tp["y"], tp["z"])
    start = loc(target)
    moved = False
    crawler = loc(pawn)
    end = time.time() + 120
    while time.time() < end:
        time.sleep(2)
        now = loc(target)
        crawler = loc(pawn)
        dist = ((now["x"] - crawler["x"]) ** 2 + (now["y"] - crawler["y"]) ** 2) ** 0.5
        delta = ((now["x"] - start["x"]) ** 2 + (now["y"] - start["y"]) ** 2) ** 0.5
        print(f"chase dist={dist:.0f} moved={delta:.0f} enemy_z={now['z']:.0f} crawler_z={crawler['z']:.0f}")
        if delta > 100:
            moved = True
            break

    if crawler["z"] < 4000:
        print("FAIL crawler fell through the floor during the fight")
        return 1
    if not moved:
        print("FAIL enemy did not close on the crawler")
        return 1

    print("PASS floor built, crawler landed, enemy closed in")
    return 0


if __name__ == "__main__":
    sys.exit(main())
