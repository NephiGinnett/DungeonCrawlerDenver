"""Turn a pulled Meshy library (Tools/meshy_animation_pull.py) into Companion actions.

    python Tools/meshy_library_to_companion.py

Reads SourceArt/MeshyLibrary/manifest.json and the clip names actually inside each downloaded .glb, then writes
Companion/data/meshy-library.json: the files to load, and one action per clip, grouped by Meshy's category so
the middle-click wheel gets a section per category. The installer picks the files up automatically.
"""
import json
import re
import struct
from pathlib import Path

ROOT = Path(__file__).resolve().parent.parent
MANIFEST = ROOT / "SourceArt" / "MeshyLibrary" / "manifest.json"
OUT = ROOT / "Companion" / "data" / "meshy-library.json"

LOOPING = re.compile(r"idle|walk|run|dance|loop|breath|stand|sit|jog|crawl|swim|fly", re.I)
ICONS = {"fight": "⚔", "danc": "★", "idle": "◎", "walk": "➜", "run": "➜", "emot": "☺", "sport": "♥", "work": "♥"}


def glb_clip_names(path):
    with open(path, "rb") as f:
        f.read(12)
        length, = struct.unpack("<I", f.read(4))
        f.read(4)
        doc = json.loads(f.read(length))
    return [a.get("name", "") for a in doc.get("animations", [])]


def icon_for(category):
    low = category.lower()
    return next((v for k, v in ICONS.items() if k in low), "◆")


def main():
    manifest = json.loads(MANIFEST.read_text(encoding="utf-8"))
    files, actions, groups = [], [], {}
    for entry in manifest["files"]:
        path = ROOT / entry["file"]
        if not path.exists():
            continue
        files.append(entry["file"])
        category = entry["category"]
        gid = "meshy_" + re.sub(r"\W+", "_", category.lower()).strip("_")
        groups.setdefault(gid, {"id": gid, "icon": icon_for(category), "name": category})
        # Prefer the clip names in the file itself; the catalog names are a fallback.
        names = glb_clip_names(path) or [a["name"] for a in entry["actions"]]
        for i, clip in enumerate(names):
            label = entry["actions"][i]["name"] if i < len(entry["actions"]) else clip
            actions.append({
                "id": f"m{entry['actions'][i]['id'] if i < len(entry['actions']) else len(actions)}",
                "label": re.sub(r"[_]+", " ", label).strip()[:28],
                "clip": clip,
                "loop": bool(LOOPING.search(label)),
                "group": gid,
            })
    OUT.write_text(json.dumps({"files": files, "groups": list(groups.values()), "actions": actions}, indent=2),
                   encoding="utf-8")
    print(f"wrote {OUT.relative_to(ROOT)}: {len(files)} files, {len(actions)} actions, {len(groups)} wheel categories")


if __name__ == "__main__":
    main()
