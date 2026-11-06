"""Walk the first ten minutes in PIE without anyone at the controls, and report what happened at each beat.

    python Tools/pie_test_onboarding.py

There's no input injection over MCP, so the "walking" is teleporting the pawn onto each trigger. Overlap logic
fires the same way. Prints stage/HUD state after each beat and grabs a viewport capture into Saved/PIETest/.
"""
import base64
import json
import re
import sys
import time
from pathlib import Path

sys.path.insert(0, str(Path(__file__).resolve().parent))
from mcp_call import McpSession, unwrap  # noqa: E402

ROOT = Path(__file__).resolve().parent.parent
OUT = ROOT / "Saved" / "PIETest"
OUT.mkdir(parents=True, exist_ok=True)
s = McpSession()


def rv(r):
    v = unwrap(r)
    try:
        v = json.loads(v)
    except (TypeError, ValueError):
        return v
    return v.get("returnValue", v) if isinstance(v, dict) else v


def find(cls, pie=True):
    r = rv(s.call_tool("SceneTools", "find_actors", {"name": "", "tag": "", "collision_channels": [],
                                                     "actor_type": {"refPath": cls}})) or []
    return [a["refPath"] for a in r if ("UEDPIE" in a["refPath"]) == pie]


def loc(ref):
    return rv(s.call_tool("ActorTools", "get_actor_transform", {"actor": {"refPath": ref}}))["location"]


def move(ref, x, y, z):
    t = rv(s.call_tool("ActorTools", "get_actor_transform", {"actor": {"refPath": ref}}))
    t["location"] = {"x": x, "y": y, "z": z}
    rv(s.call_tool("ActorTools", "set_actor_transform", {"actor": {"refPath": ref}, "xform": t}))


def state(pawn):
    v = rv(s.call_tool("ObjectTools", "get_properties", {"instance": {"refPath": pawn + ".Onboarding"},
                                                         "properties": ["CrawlerId", "Stage", "UnlockedFeatures"]}))
    return json.loads(v) if isinstance(v, str) else v


def capture(name):
    r = unwrap(s.call_tool("EditorAppToolset", "CaptureEditorImage", {}))
    m = re.search(r"([A-Za-z0-9+/=]{5000,})", r if isinstance(r, str) else json.dumps(r))
    if m:
        (OUT / f"{name}.png").write_bytes(base64.b64decode(m.group(1)))


def logs(pattern):
    return [str(e) for e in (rv(s.call_tool("LogsToolset", "GetLogEntries", {"category": "", "pattern": pattern})) or [])]


STAGES = ['Waking', 'ItemFound', 'Announced', 'InDungeon', 'FirstFightWon', 'SafeRoom', 'Complete']


def wait_stage(pawn, stage, timeout=60):
    """The editor throttles itself when it isn't the foreground window, so game time runs several times slower
    than real time. Wait on the stage itself, never on a fixed sleep."""
    end = time.time() + timeout
    while time.time() < end:
        if STAGES.index(state(pawn).get('Stage', 'Waking')) >= STAGES.index(stage):
            return True
        time.sleep(1)
    return False


def beat(title, pawn):
    st = state(pawn)
    print(f"{title:<28} stage={st.get('Stage')} hud={st.get('UnlockedFeatures')} id={st.get('CrawlerId')} at={ {k: round(v) for k, v in loc(pawn).items()} }")


def main():
    if rv(s.call_tool("EditorAppToolset", "IsPIERunning", {})) is True:
        rv(s.call_tool("EditorAppToolset", "StopPIE", {}))
    rv(s.call_tool("EditorAppToolset", "StartPIE", {"options": {"bSimulate": False, "playMode": "PlayMode_InViewPort", "warmupSeconds": 4}}))
    pawn = [p for p in find("/Script/DungeonCrawlerDenver.DCCCharacterBase") if "Enemy" not in p and "Ally" not in p][0]
    # Identity lands on the pawn's first game tick, but a backgrounded editor ticks PIE very slowly, so wait for it.
    end = time.time() + 90
    while time.time() < end and state(pawn).get("CrawlerId") in (None, "None"):
        time.sleep(1)
    beat("spawned", pawn)
    body = rv(s.call_tool("ObjectTools", "get_properties", {"instance": {"refPath": pawn + ".CharacterMesh0"},
                                                            "properties": ["SkeletalMeshAsset"]}))
    print("    body:", str(body)[:140])
    capture("01_bedroom")

    cid = state(pawn).get("CrawlerId")
    items = [p for p in find("/Script/DungeonCrawlerDenver.DCCPersonalItem") if cid and cid in p] or \
        find("/Script/DungeonCrawlerDenver.DCCPersonalItem")
    # PIE actor names are UAID-based, so match by owner instead.
    for it in find("/Script/DungeonCrawlerDenver.DCCPersonalItem"):
        own = rv(s.call_tool("ObjectTools", "get_properties", {"instance": {"refPath": it}, "properties": ["OwnerCrawlerId"]}))
        if cid and f'"{cid}"' in str(own):
            items = [it]
    il = loc(items[0])
    move(pawn, il["x"], il["y"], il["z"] + 20)
    wait_stage(pawn, 'ItemFound', 20)
    beat("picked up item", pawn)
    eq = rv(s.call_tool("ObjectTools", "get_properties", {"instance": {"refPath": pawn + ".Equipment"}, "properties": ["Equipped"]}))
    print("    equipped:", str(eq)[:160])
    capture("02_item")
    wait_stage(pawn, 'Announced', 60)
    beat("after announcement", pawn)
    capture("03_announcement")

    ent = find("/Script/DungeonCrawlerDenver.DCCDungeonEntrance")[0]
    el = loc(ent)
    move(pawn, el["x"], el["y"], el["z"] + 20)
    wait_stage(pawn, 'InDungeon', 30)
    beat("through the door", pawn)
    capture("04_welcome")
    time.sleep(25)  # the trial's 3-second (game time) spawn delay
    spawned = logs("Spawn|Enemy|encounter|Onboarding")
    enemies = [p for p in find("/Script/DungeonCrawlerDenver.DCCEnemyCharacter")]
    near = [e for e in enemies if abs(loc(e)["y"] - loc(pawn)["y"]) < 2500 and abs(loc(e)["x"] - loc(pawn)["x"]) < 3000]
    print(f"{'trial fight':<28} enemies near the crawler: {len(near)}")
    for e in near[:6]:
        dn = rv(s.call_tool("ObjectTools", "get_properties", {"instance": {"refPath": e}, "properties": ["DisplayName"]}))
        print("    ", str(dn)[:120])
    capture("05_fight")
    print("onboarding log lines:")
    for line in [l for l in spawned if "Onboarding" in l][-8:]:
        print("   ", line[-150:])


if __name__ == "__main__":
    main()
