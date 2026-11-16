"""Fort Tiner's townsfolk and everything they say.

THE LINES LIVE IN  SourceArt/Voice/NPC/fort_tiner_lines.json  (since 2026-11-13).
Edit them in the System Voice Studio (Edit line / New line / Delete line), or by hand. The game reads that file
every time a Fourteener round starts, so a changed line is heard next round with no rebuild.

    python Tools/gen_fort_tiner_lines.py           # create the JSON if it is missing, and refresh the built-in copy
    python Tools/gen_fort_tiner_lines.py --reset   # overwrite the JSON with the lines below (loses studio edits!)

The built-in copy, Source/DungeonCrawlerDenver/Arcade/DCCFortTinerLines.inl, is what the game falls back on when
the JSON is missing (a packaged build without the Voice folder). Rebuild after refreshing it.

Recording: in the Voice Studio, pick a Fort Tiner line, record, Export. The take is saved as
  SourceArt/Voice/NPC/<Speaker>/VO_<Row>.wav
and the game plays it whenever that NPC says that line. A line with no recording is still shown as text.

Fort Tiner is the base camp at the foot of Fourteener (the name is "fourteener" said with a drawl). The tone is
the dungeon tone bible's: friendly, a bit unhinged, original. Country accents encouraged.
"""

import json
import sys
from pathlib import Path

ROOT = Path(__file__).resolve().parent.parent
JSON_PATH = ROOT / "SourceArt" / "Voice" / "NPC" / "fort_tiner_lines.json"
INL_PATH = ROOT / "Source" / "DungeonCrawlerDenver" / "Arcade" / "DCCFortTinerLines.inl"

# The starting cast, used only to create the JSON (or --reset it).
# Speaker id -> (display name, body: "Manny" or "Quinn", hat cosmetic id or "")
SPEAKERS = {
    "Dusty": ("Sheriff Dusty", "Manny", "TenGallon"),
    "MaOre": ("Ma Ore", "Quinn", "MinersHelmet"),
    "Gus": ("Old Gus", "Manny", "MinersHelmet"),
    "Bea": ("Banjo Bea", "Quinn", "TenGallon"),
}

# (speaker, row, line). Rows must be unique; they name the recordings (VO_<Row>.wav).
LINES = [
    ("Dusty", "FT_Dusty_01", "Well howdy, crawler. Welcome to Fort Tiner. Population six, and one goat that thinks it's the mayor."),
    ("Dusty", "FT_Dusty_02", "Rule one of Fort Tiner: no climbin' on the fort. Rule two: climb everything else. That's what you're here for."),
    ("Dusty", "FT_Dusty_03", "The summit sits at fourteen thousand one hundred and fifteen feet. I've been up there twice. Came down the fast way once."),
    ("Dusty", "FT_Dusty_04", "If a crawler don't come back down by sundown, we ring the bell. Then we eat their beans. Out of respect."),

    ("MaOre", "FT_MaOre_01", "Prize counter's back in the Den, sugar. Win yourself some tickets up that rock and go treat yourself."),
    ("MaOre", "FT_MaOre_02", "That helmet? Lamp works fine. Battery don't. We're workin' on it. We've been workin' on it since eighteen fifty-nine."),
    ("MaOre", "FT_MaOre_03", "My late husband went down a mineshaft lookin' for silver. Found the System instead. Now he's a floor boss. We still write."),
    ("MaOre", "FT_MaOre_04", "Every inch you climb earns you points. Get to the top with everybody and the whole crew gets a bonus. Teamwork, darlin'."),

    ("Gus", "FT_Gus_01", "Folks think the treasure's up top. Heh. The treasure's DOWN, partner. Deep down. The mountain's just the lid."),
    ("Gus", "FT_Gus_02", "I dug a hole so deep last spring, somethin' down there dug back. Polite fella. Wanted to know if we was hirin'."),
    ("Gus", "FT_Gus_03", "Hear that rumblin'? That's the dungeon, stretchin' its legs under the Rockies. One day it's gonna come up for air."),
    ("Gus", "FT_Gus_04", "You want my advice? Rest on them ledges. The rock don't care how tired you are. The rock never cares."),

    ("Bea", "FT_Bea_01", "Equip your Climbin', then hold the left mouse to haul yourself up. Let go and you drop, so only let go over a ledge, hon."),
    ("Bea", "FT_Bea_02", "Climbin' burns stamina. When your arms start singin', find a ledge and let 'em rest a spell."),
    ("Bea", "FT_Bea_03", "I wrote a song about the last fella who skipped the ledges. It's a short song. Real short."),
    ("Bea", "FT_Bea_04", "Beat the clock to the summit and there's extra tickets in it for ya. Daylight's wastin', crawler!"),
]


def cpp_string(text):
    return text.replace("\\", "\\\\").replace('"', '\\"')


def main():
    if "--reset" in sys.argv or not JSON_PATH.exists():
        JSON_PATH.parent.mkdir(parents=True, exist_ok=True)
        data = {
            "speakers": [{"id": sid, "name": n, "body": b, "hat": h} for sid, (n, b, h) in SPEAKERS.items()],
            "lines": [{"speaker": sid, "row": row, "text": text} for sid, row, text in LINES],
        }
        JSON_PATH.write_text(json.dumps(data, indent=2, ensure_ascii=False) + "\n", encoding="utf-8")
        print(f"wrote {JSON_PATH.relative_to(ROOT)}")

    data = json.loads(JSON_PATH.read_text(encoding="utf-8"))
    speakers = {sp["id"]: sp for sp in data["speakers"]}
    rows = [ln["row"] for ln in data["lines"]]
    assert len(rows) == len(set(rows)), "duplicate row names"

    inl = ["// Generated by Tools/gen_fort_tiner_lines.py from SourceArt/Voice/NPC/fort_tiner_lines.json.",
           "// The game's fallback when that file is missing. Edit the JSON (or use the Voice Studio), not this.", ""]
    inl.append("static const FFortTinerSpeaker GFortTinerSpeakers[] = {")
    for sp in data["speakers"]:
        inl.append(f'\t{{ TEXT("{sp["id"]}"), TEXT("{cpp_string(sp["name"])}"), TEXT("{sp["body"]}"), TEXT("{sp.get("hat", "")}") }},')
    inl.append("};")
    inl.append("")
    inl.append("static const FFortTinerLine GFortTinerLines[] = {")
    for ln in data["lines"]:
        assert ln["speaker"] in speakers, ln
        inl.append(f'\t{{ TEXT("{ln["speaker"]}"), TEXT("{ln["row"]}"), TEXT("{cpp_string(ln["text"])}") }},')
    inl.append("};")
    INL_PATH.write_text("\n".join(inl) + "\n", encoding="utf-8", newline="\r\n")
    print(f"refreshed {INL_PATH.relative_to(ROOT)} ({len(data['lines'])} lines, {len(speakers)} speakers)")


if __name__ == "__main__":
    main()
