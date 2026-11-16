"""Continue a running tale-floor PIE session: confirm a hit, then clear the stairs room and descend.

    python Tools/pie_test_tale_stairs.py

Expects pie_test_tale.py to have left PIE running on floor 1.
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


def find(cls):
    r = rv(s.call_tool("SceneTools", "find_actors", {
        "name": "", "tag": "", "collision_channels": [],
        "actor_type": {"refPath": cls},
    })) or []
    return [a["refPath"] for a in r if "UEDPIE" in a["refPath"]]


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
    if rv(s.call_tool("EditorAppToolset", "IsPIERunning", {})) is not True:
        print("FAIL PIE is not running")
        return 1

    pawns = [p for p in find("/Script/DungeonCrawlerDenver.DCCCharacterBase") if "Enemy" not in p and "Ally" not in p]
    pawn = pawns[0]
    before = props(pawn + ".Stats", ["CurrentHitPoints", "MaxHitPoints"])
    print("hp before", before)

    # Give the enemy that already closed in a moment to swing. A backgrounded editor ticks slowly.
    end = time.time() + 45
    hit = False
    while time.time() < end:
        time.sleep(3)
        now = props(pawn + ".Stats", ["CurrentHitPoints", "MaxHitPoints"])
        print("hp", now)
        if now.get("CurrentHitPoints", 0) < before.get("CurrentHitPoints", 0):
            hit = True
            break
    print("took damage" if hit else "no damage yet")

    enemies = find("/Script/DungeonCrawlerDenver.DCCEnemyCharacter")
    floor_enemies = []
    for e in enemies:
        p = loc(e)
        if abs(p["x"] - 150000) < 20000 and abs(p["y"] - 150000) < 20000:
            floor_enemies.append(e)
    print("clearing", len(floor_enemies), "floor enemies so the stairs can open")
    for e in floor_enemies:
        ok = rv(s.call_tool("ObjectTools", "set_properties", {
            "instance": {"refPath": e + ".Stats"},
            "values": json.dumps({"CurrentHitPoints": 0}),
        }))
        after = props(e + ".Stats", ["CurrentHitPoints"])
        if after.get("CurrentHitPoints", 1) != 0:
            print("  hp write failed", ok, after, e[-40:])

    floors = find("/Script/DungeonCrawlerDenver.DCCTaleFloor")
    if not floors:
        print("FAIL floor actor missing")
        return 1
    floor = floors[0]
    pad = props(floor + ".DownPad", ["RelativeLocation"])
    print("down pad relative", pad)
    base = loc(floor)
    rel = (pad or {}).get("RelativeLocation") or {}
    # SetWorldLocation stores the pad in world space relative to the floor actor.
    px = base["x"] + rel.get("x", rel.get("X", 0))
    py = base["y"] + rel.get("y", rel.get("Y", 0))
    pz = base["z"] + rel.get("z", rel.get("Z", 0))
    print("pad world", round(px), round(py), round(pz))
    here = loc(pawn)
    move(pawn, px, py, max(pz, here["z"]))

    end = time.time() + 30
    descended = False
    while time.time() < end:
        time.sleep(1)
        tale = [ln for ln in logs("DCC Tale") if "Floor 2" in ln or "layout seed" in ln]
        if any("Floor 2" in ln for ln in tale):
            descended = True
            break
    print("crawler now", {k: round(v) for k, v in loc(pawn).items()})
    for line in logs("DCC Tale")[-8:]:
        print("   ", line[-200:])
    if not descended:
        print("FAIL did not reach floor 2")
        return 1
    print("PASS descended to floor 2")
    return 0


if __name__ == "__main__":
    sys.exit(main())
