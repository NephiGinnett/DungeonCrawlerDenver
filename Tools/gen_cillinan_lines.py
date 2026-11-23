"""Cillinan, the alchemist behind the Den's prize counter, and what he says (2026-11-23).

Zoe: "Can you import that character into the game as a shop keeper somewhere? Maybe at the prize counter? His name is
Cillinan." The model is a Meshy alchemist brought in with every PBR map by Tools/import_glb_pbr.py
(/Game/DCC/Characters/Cillinan).

THE LINES LIVE IN  SourceArt/Voice/NPC/cillinan_lines.json (read by ADCCShopkeeperNPC when the Den loads).
    python Tools/gen_cillinan_lines.py           # write the JSON (keeps it, adds new rows)
    python Tools/gen_cillinan_lines.py --reset   # overwrite it
    python Tools/gen_cillinan_lines.py --voice   # voice lines with no recording yet (Kokoro, offline)

Tags: GREET (when someone walks up), PITCH (what the counter sells), QUIP (between customers). He is a real alchemist
the System put in retail, dry and a little tired, and fond of his customers despite himself.
"""
from __future__ import annotations

import argparse
import json
import sys
from pathlib import Path

REPO = Path(__file__).resolve().parent.parent
sys.path.insert(0, str(REPO / "Tools"))
import gen_talkshow_lines as tts  # noqa: E402  (the same Kokoro voicing, a different voice)

JSON_PATH = REPO / "SourceArt" / "Voice" / "NPC" / "cillinan_lines.json"
SPEAKER = {"id": "Cillinan", "name": "Cillinan", "body": "Cillinan"}
VOICE = ("bm_george:0.6+bm_fable:0.4", 0.95)  # older, British, unhurried

L = []
def add(tag, row, text):
    L.append({"speaker": "Cillinan", "row": row, "tag": tag, "text": text})

add("GREET", "CL_Greet_01", "Ah. A customer. Mind the vials, some of them are opinions.")
add("GREET", "CL_Greet_02", "Welcome to the prize counter. I am Cillinan. Everything here is for sale, including, eventually, me.")
add("GREET", "CL_Greet_03", "Tickets? Wonderful. Tickets are the only currency I accept, and the only one that has never once lied to me.")
add("GREET", "CL_Greet_04", "Step up, step up. Do not touch the green one. The green one touches back.")
add("GREET", "CL_Greet_05", "Back again? Good. The shelves were getting lonely, and so, I admit, was I.")

add("PITCH", "CL_Pitch_01", "Damage skins, fresh from the still. Inferno for the dramatic, Frostbite for the smug, Past Due for the cruel.")
add("PITCH", "CL_Pitch_02", "The spray can is the finest thing I sell. Your picture, on their wall, forever. Or until the host scrubs it.")
add("PITCH", "CL_Pitch_03", "Hats, halos, and a jetpack of questionable provenance. All of it looks better on a winner.")
add("PITCH", "CL_Pitch_04", "The Hot Mic is not for sale. Believe me, I have offered. Only the voice of Hiss Cranson holds it.")
add("PITCH", "CL_Pitch_05", "Truck upgrades are next door at the Chop Shop. Scrap, not tickets. I do not touch scrap. It gets in the robes.")
add("PITCH", "CL_Pitch_06", "The friend tubes are over there. Ten at a time is cheaper. I mention this purely as a professional courtesy.")
add("PITCH", "CL_Pitch_07", "Stand on the gold pad and the whole counter opens up. Browse. Linger. Spend.")

add("QUIP", "CL_Quip_01", "This one is antifreeze. That one is antifreeze with a nicer label.")
add("QUIP", "CL_Quip_02", "Do not drink anything on the top shelf. Do not ask why the top shelf is warm.")
add("QUIP", "CL_Quip_03", "I was a real alchemist, once. Then the System arrived and asked if I could do retail.")
add("QUIP", "CL_Quip_04", "Fill your Fame bar and the boxes come. I sell the outfit you will be famous in.")
add("QUIP", "CL_Quip_05", "If you see a bubbling flask, no you didn't.")
add("QUIP", "CL_Quip_06", "Hiss Cranson owes me forty tickets and a cravat. Tell him I said so. Loudly.")


if __name__ == "__main__":
    ap = argparse.ArgumentParser()
    ap.add_argument("--reset", action="store_true")
    ap.add_argument("--voice", action="store_true")
    ap.add_argument("--replace", action="store_true")
    a = ap.parse_args()
    data = {"speakers": [SPEAKER], "lines": L}
    if JSON_PATH.exists() and not a.reset:
        old = json.loads(JSON_PATH.read_text(encoding="utf-8"))
        have = {l["row"] for l in old.get("lines", [])}
        old["lines"] += [l for l in L if l["row"] not in have]
        old["speakers"] = [SPEAKER]
        data = old
    JSON_PATH.write_text(json.dumps(data, indent=2, ensure_ascii=False) + "\n", encoding="utf-8")
    print(f"{JSON_PATH.relative_to(REPO)}: {len(data['lines'])} lines")
    if a.voice:
        tts.VOICE = VOICE
        tts.voice(data, a.replace)
