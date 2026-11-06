"""Pull Meshy's preset animation library onto our crawler rig.

All six crawlers share one Meshy skeleton (CLAUDE.md, "CONSOLIDATED"), so ONE rig is enough: every clip
downloaded for it plays on everyone, in the Companion (rest-pose retarget) and in UE (shared skeleton).

    python Tools/meshy_animation_pull.py --list                  # the catalog, by category, with cost. Free.
    python Tools/meshy_animation_pull.py --all                   # how much everything would cost. Free (a dry run).
    python Tools/meshy_animation_pull.py --all --yes             # actually spend the credits and download
    python Tools/meshy_animation_pull.py --categories Fighting,Dancing --yes
    python Tools/meshy_animation_pull.py --ids 12,57,92 --yes

Nothing that costs credits happens without --yes. Pricing (docs.meshy.ai/en/api/pricing, checked 2026-11-05):
rigging 5 credits once, animation 3 credits per clip, 10 clips per request.

The API key is read from the MESHY_API_KEY environment variable, or from Tools/.meshy_key (one line, gitignored).
Zoe puts it there herself; this script never prints it.

Output:
  SourceArt/MeshyLibrary/<category>/<batch>.glb     merged clips (one file per request, up to 10 clips each)
  SourceArt/MeshyLibrary/manifest.json              what is in each file, and the rig used
Re-running skips everything already downloaded, so an interrupted pull resumes and never pays twice.
"""
import argparse
import base64
import json
import os
import re
import sys
import time
import urllib.error
import urllib.request
from pathlib import Path

ROOT = Path(__file__).resolve().parent.parent
OUT = ROOT / "SourceArt" / "MeshyLibrary"
MANIFEST = OUT / "manifest.json"
KEY_FILE = ROOT / "Tools" / ".meshy_key"
API = "https://api.meshy.ai/openapi/v1"

# CJ's own character file: textured, humanoid, 6.7 MB, so it fits the 100 MB data-URI limit.
RIG_SOURCE = ROOT / "SourceArt/CJ_HockeyWings/Meshy_AI_Hockey_Wings_biped/Meshy_AI_Hockey_Wings_biped_Character_output.glb"
RIG_HEIGHT_M = 1.70

COST_RIG, COST_PER_CLIP, BATCH = 5, 3, 10


def api_key():
    key = os.environ.get("MESHY_API_KEY") or (KEY_FILE.read_text().strip() if KEY_FILE.exists() else "")
    if not key:
        sys.exit("No Meshy API key. Put it in Tools/.meshy_key (one line) or set MESHY_API_KEY.")
    return key


def call(method, path, body=None, key=None):
    req = urllib.request.Request(f"{API}{path}", method=method,
                                 data=json.dumps(body).encode() if body is not None else None,
                                 headers={"Authorization": f"Bearer {key}", "Content-Type": "application/json"})
    try:
        with urllib.request.urlopen(req, timeout=120) as r:
            return json.loads(r.read())
    except urllib.error.HTTPError as e:
        detail = e.read().decode(errors="replace")[:400]
        sys.exit(f"Meshy said {e.code} for {method} {path}: {detail}")


def wait(kind, task_id, key, label):
    while True:
        t = call("GET", f"/{kind}/{task_id}", key=key)
        status = t.get("status")
        if status == "SUCCEEDED":
            return t
        if status in ("FAILED", "CANCELED"):
            sys.exit(f"{label} {status}: {t.get('task_error')}")
        print(f"  {label}: {status} {t.get('progress', 0)}%", end="\r", flush=True)
        time.sleep(5)


def catalog(key):
    data = call("GET", "/animations/library", key=key)
    items = data if isinstance(data, list) else data.get("result") or data.get("data") or data.get("animations") or []
    out = []
    for a in items:
        aid = a.get("action_id", a.get("id"))
        if aid is None:
            continue
        out.append({"id": int(aid), "name": a.get("name") or a.get("action_name") or f"action_{aid}",
                    "category": a.get("category") or a.get("type") or "Other"})
    return sorted(out, key=lambda a: (a["category"], a["id"]))


def load_manifest():
    if MANIFEST.exists():
        return json.loads(MANIFEST.read_text(encoding="utf-8"))
    return {"rig_task_id": None, "files": []}


def save_manifest(m):
    OUT.mkdir(parents=True, exist_ok=True)
    MANIFEST.write_text(json.dumps(m, indent=2), encoding="utf-8")


def slug(s):
    return re.sub(r"[^A-Za-z0-9]+", "_", s).strip("_") or "Other"


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("--list", action="store_true")
    ap.add_argument("--all", action="store_true")
    ap.add_argument("--categories", default="")
    ap.add_argument("--ids", default="")
    ap.add_argument("--yes", action="store_true", help="actually spend credits")
    args = ap.parse_args()

    key = api_key()
    cat = catalog(key)
    by_cat = {}
    for a in cat:
        by_cat.setdefault(a["category"], []).append(a)

    if args.list:
        for c, items in by_cat.items():
            print(f"{c:<24} {len(items):>4} clips   ~{len(items) * COST_PER_CLIP} credits")
        print(f"{'TOTAL':<24} {len(cat):>4} clips   ~{len(cat) * COST_PER_CLIP + COST_RIG} credits (incl. rig)")
        return

    wanted = []
    if args.all:
        wanted = cat
    elif args.categories:
        names = {c.strip().lower() for c in args.categories.split(",")}
        wanted = [a for a in cat if a["category"].lower() in names]
    elif args.ids:
        ids = {int(x) for x in args.ids.split(",") if x.strip()}
        wanted = [a for a in cat if a["id"] in ids]
    if not wanted:
        sys.exit("Nothing selected. Use --list to see categories, then --all, --categories or --ids.")

    manifest = load_manifest()
    have = {a["id"] for f in manifest["files"] for a in f["actions"]}
    todo = [a for a in wanted if a["id"] not in have]
    cost = len(todo) * COST_PER_CLIP + (0 if manifest["rig_task_id"] else COST_RIG)
    print(f"{len(wanted)} selected, {len(wanted) - len(todo)} already downloaded, {len(todo)} to fetch: about {cost} credits.")
    if not todo:
        return
    if not args.yes:
        print("Dry run. Add --yes to spend the credits.")
        return

    key_ok = manifest["rig_task_id"]
    if not key_ok:
        print("Rigging CJ once (5 credits); every crawler shares this skeleton...")
        uri = "data:model/gltf-binary;base64," + base64.b64encode(RIG_SOURCE.read_bytes()).decode()
        rid = call("POST", "/rigging", {"model_url": uri, "height_meters": RIG_HEIGHT_M}, key)["result"]
        wait("rigging", rid, key, "rig")
        manifest["rig_task_id"] = rid
        save_manifest(manifest)
        print(f"\n  rig ready: {rid}")

    # Batch within a category, so each file is a coherent set ("Fighting_03.glb").
    queue = {}
    for a in todo:
        queue.setdefault(a["category"], []).append(a)
    for category, items in queue.items():
        folder = OUT / slug(category)
        folder.mkdir(parents=True, exist_ok=True)
        existing = len([f for f in manifest["files"] if f["category"] == category])
        for n, start in enumerate(range(0, len(items), BATCH), start=existing + 1):
            batch = items[start:start + BATCH]
            name = f"{slug(category)}_{n:02d}.glb"
            print(f"{category}: {name} ({len(batch)} clips, {len(batch) * COST_PER_CLIP} credits)")
            tid = call("POST", "/animations", {"rig_task_id": manifest["rig_task_id"],
                                               "action_ids": [a["id"] for a in batch]}, key)["result"]
            task = wait("animations", tid, key, name)
            url = task["result"]["animation_glb_url"]
            urllib.request.urlretrieve(url, folder / name)
            manifest["files"].append({"file": f"SourceArt/MeshyLibrary/{slug(category)}/{name}",
                                      "category": category, "actions": batch, "task_id": tid})
            save_manifest(manifest)  # after every file, so an interruption loses at most one request
            print(f"\n  saved {name}")

    print("Done. Run: python Tools/meshy_library_to_companion.py")


if __name__ == "__main__":
    main()
