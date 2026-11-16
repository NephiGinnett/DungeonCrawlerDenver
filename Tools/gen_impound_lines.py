"""The Mile High impound staff and everything they say during Repo Rally.

THE LINES LIVE IN  SourceArt/Voice/NPC/impound_lines.json.
Edit them in the System Voice Studio, or by hand. The game reads that file every time a race starts,
so a changed line is heard next round with no rebuild.

    python Tools/gen_impound_lines.py           # create the JSON if it is missing, and refresh the built-in copy
    python Tools/gen_impound_lines.py --merge   # add rows that are new in this script, keep everything else
    python Tools/gen_impound_lines.py --reset   # overwrite the JSON with the lines below (loses studio edits!)

The built-in copy, Source/DungeonCrawlerDenver/Arcade/DCCImpoundLines.inl, is the fallback when the JSON
is missing. Rebuild after refreshing it.

Recording: Voice Studio, pick a line, record, Export. The take is saved as
  SourceArt/Voice/NPC/<Speaker>/VO_<Row>.wav
A line with no recording is still shown as text.

They stand in the yard and explain the shift. Deadpan, clipboard, Mile High Recovery. Not the fort's drawl.
"""

import json
import sys
from pathlib import Path

ROOT = Path(__file__).resolve().parent.parent
JSON_PATH = ROOT / "SourceArt" / "Voice" / "NPC" / "impound_lines.json"
INL_PATH = ROOT / "Source" / "DungeonCrawlerDenver" / "Arcade" / "DCCImpoundLines.inl"

# Speaker id -> (display name, body: "Manny" or "Quinn", hat cosmetic id or "")
SPEAKERS = {
    "Rhee": ("Lot Boss Rhee", "Quinn", "TrafficCone"),
    "Jan": ("Clerk Jan", "Manny", ""),
    "Diaz": ("Flagger Diaz", "Manny", ""),
    # Radio only: never stands in the yard. Heard over the truck radio when a shift event happens
    # (ADCCArcadeKart::Radio -> ADCCRover::Client_RallyRadio). Rows RR_Disp_* are called by name from code.
    "Dispatch": ("Dispatch (Vonda)", "Quinn", ""),
    # Not yard staff. When a hook lands, the owner of that car may come out of the building and scream
    # one of these (ADCCRallyThug). Same car, same line. Record them loud.
    "Pryce": ("Pryce (car owner)", "Manny", ""),
    "Nia": ("Nia (car owner)", "Quinn", ""),
    "Holt": ("Holt (car owner)", "Manny", ""),
}

# (speaker, row, line). Rows must be unique; they name the recordings (VO_<Row>.wav).
# The first line of each person is the one a driver hears before the flag. The rest follow if they are still in earshot.
LINES = [
    ("Rhee", "RR_Rhee_01", "This is a shift, not a parade. Marked cars are loose in the city. Right mouse aims the hook. Q fires it. Pull the car onto your bumper and bring it here."),
    ("Rhee", "RR_Rhee_02", "The first ones are parked. Then they sit boxed in, and yanking one out scars it. Then they run. Some of them spook and leave the ground when you get close. A clean car pays. A scarred one does not."),
    ("Rhee", "RR_Rhee_03", "One truck tows one car. A second truck can throw a net, or block a runner, or take a different mark. Split up and you cover more of the city. The shift pays whoever banks the most."),
    ("Rhee", "RR_Rhee_04", "Go into debt and you are a mark too. Night Shift and the other crew will come find you, and Jan will not open the yard pump."),

    ("Jan", "RR_Jan_01", "Right mouse aims. Q fires the hook, or drops the car once it is on your bumper and you are in this yard. F switches, if your truck has a net. T is the radio. Leave it."),
    ("Jan", "RR_Jan_02", "The bays are the roles. Runner is faster and a shorter hook. Net throws a net. Winch reaches further and reels harder. Tank drinks less fuel. The tab covers it, and the tab can go red."),
    ("Jan", "RR_Jan_03", "Street gas always pumps, and it goes on your tab. The yard pump is free until you owe us. Then it is closed, and I will not pretend otherwise."),
    ("Jan", "RR_Jan_04", "A drop pays the tab. Clear it, or the other crews will treat your truck like a skip. Hook the wrong car and I fine you."),
    ("Jan", "RR_Jan_05", "Amber columns are the marks. The map in the corner shows the same cars, the gas, and anyone hunting you."),

    ("Diaz", "RR_Diaz_01", "Heavy traffic. It is not decoration, and it is not a ghost anymore. Hit it and you stop. Go around."),
    ("Diaz", "RR_Diaz_02", "I keep the beacons lit. If you cannot see the column, trust the map. Parked cars sit off the street. The ones that spook climb."),
    ("Diaz", "RR_Diaz_03", "A net on the road slows whatever drives through it. Slow is easier to reel onto the bumper. Then you still have to drive it home."),
    ("Diaz", "RR_Diaz_04", "W and S drive. A and D steer. Click boosts. Shift slides. Right mouse aims the hook. The flag drops when the flag drops."),

    # 2026-11-13: night-shift colour, and the new streets (ramps, skyways, pads, the lock).
    ("Rhee", "RR_Rhee_05", "Night shift pays double because nobody wants it. The city looks like a postcard from 1986 and drives like a pinball machine. Bring my cars back clean."),
    ("Rhee", "RR_Rhee_06", "Hold right mouse on a mark until the ring locks green. Then fire. A locked hook does not miss. A guessed one buys you a fine."),
    ("Rhee", "RR_Rhee_07", "Every one of those cars was a promise somebody made to a lender. Tonight we collect on promises. Do not get sentimental about the fuzzy dice."),
    ("Jan", "RR_Jan_06", "Cyan chevrons on the road are boost pads. The skyways have them all the way down the far side. Downhill and boosted is how you beat a runner home."),
    ("Jan", "RR_Jan_07", "The ramps are on the street because the city planner lost a bet. You can jump them with a car on your bumper. I would not. Paperwork."),
    ("Jan", "RR_Jan_08", "When the music comes up, the shift is almost over. Whatever is on your hook when it ends is not a drop. Get it home."),
    ("Diaz", "RR_Diaz_05", "Two skyways, nine metres up. Up the ramp, along the deck over the crossings, and down the long side with the pads. The rails keep you on. Mostly."),
    ("Diaz", "RR_Diaz_06", "Parked cars sit right in the lanes tonight. Somebody double-parked the whole city. Weave."),
    ("Diaz", "RR_Diaz_07", "Sirens out there are not for you. Yet. The horns are, usually."),

    ("Dispatch", "RR_Disp_Brief", "Evening, night shift. Tonight's list is on your screens: four cars that stopped paying. Hook them, reel them onto your bumper, tow them home to the yard, drop them. Quick and clean pays. Wrong car is a fine. Flag drops in fifteen."),
    ("Dispatch", "RR_Disp_Go", "Dispatch to all trucks. Flag is down. Marks are on the board, amber columns. Go get them."),
    ("Dispatch", "RR_Disp_Hooked", "Line's on. Reel it in. Keep it off the parked cars."),
    ("Dispatch", "RR_Disp_Bumper", "It's on your bumper. Bring it home to the yard."),
    ("Dispatch", "RR_Disp_Drop", "Drop logged. Clean work. Rhee might even nod."),
    ("Dispatch", "RR_Disp_DropWreck", "Drop logged. That car came in looking like modern art. Pay's docked."),
    ("Dispatch", "RR_Disp_Wrong", "That was not a mark. That was a citizen. Fine's on your tab."),
    ("Dispatch", "RR_Disp_Cut", "Line cut. No car, no pay."),
    ("Dispatch", "RR_Disp_Debt", "Your tab just went red, driver. Every crew in the city can hear that on the scanner."),
    ("Dispatch", "RR_Disp_Wave1", "Heads up. The easy ones are gone. Next marks are boxed in."),
    ("Dispatch", "RR_Disp_Wave2", "They're moving now. Runners on the grid. Hook and hold."),
    ("Dispatch", "RR_Disp_Wave3", "Some of these spook. Get too close and they lift off. Lock before they know you're there."),
    ("Dispatch", "RR_Disp_Last30", "Thirty seconds on the shift. Whatever's on your hook, get it home."),

    # Owners. Shouted at the truck that hooked their car. One cuss, one reason, per person.
    ("Pryce", "RR_Pryce_01", "Hey! That is my car, you son of a bitch! I am paid through Friday!"),
    ("Pryce", "RR_Pryce_02", "Put the hook down! That is how I get to work, and I have a shift at six!"),
    ("Nia", "RR_Nia_01", "Get off my car! My kid's seat is in the back, you asshole!"),
    ("Nia", "RR_Nia_02", "I am not a skip! The notice went to the old address! You don't get to take it!"),
    ("Holt", "RR_Holt_01", "I paid that note! The receipt is in the glove box you're dragging down the street!"),
    ("Holt", "RR_Holt_02", "Mile High can kiss my ass! Touch that bumper and I am not moving!"),
]


def cpp_string(text):
    return text.replace("\\", "\\\\").replace('"', '\\"')


def merge_missing():
    """Add speakers and rows that are in this script but not yet in the JSON. Never changes or removes an
    existing row, so Voice Studio edits survive. A row whose text here changed is left as the JSON has it."""
    data = json.loads(JSON_PATH.read_text(encoding="utf-8"))
    have_sp = {sp["id"] for sp in data["speakers"]}
    have_rows = {ln["row"] for ln in data["lines"]}
    added = 0
    for sid, (n, b, h) in SPEAKERS.items():
        if sid not in have_sp:
            data["speakers"].append({"id": sid, "name": n, "body": b, "hat": h})
    for sid, row, text in LINES:
        if row not in have_rows:
            data["lines"].append({"speaker": sid, "row": row, "text": text})
            added += 1
    JSON_PATH.write_text(json.dumps(data, indent=2, ensure_ascii=False) + "\n", encoding="utf-8")
    print(f"merged {added} new line(s) into {JSON_PATH.relative_to(ROOT)}")


def main():
    if "--merge" in sys.argv and JSON_PATH.exists():
        merge_missing()
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

    inl = ["// Generated by Tools/gen_impound_lines.py from SourceArt/Voice/NPC/impound_lines.json.",
           "// The game's fallback when that file is missing. Edit the JSON (or use the Voice Studio), not this.", ""]
    inl.append("static const FImpoundSpeaker GImpoundSpeakers[] = {")
    for sp in data["speakers"]:
        inl.append(f'\t{{ TEXT("{sp["id"]}"), TEXT("{cpp_string(sp["name"])}"), TEXT("{sp["body"]}"), TEXT("{sp.get("hat", "")}") }},')
    inl.append("};")
    inl.append("")
    inl.append("static const FImpoundLine GImpoundLines[] = {")
    for ln in data["lines"]:
        assert ln["speaker"] in speakers, ln
        inl.append(f'\t{{ TEXT("{ln["speaker"]}"), TEXT("{ln["row"]}"), TEXT("{cpp_string(ln["text"])}") }},')
    inl.append("};")
    INL_PATH.write_text("\n".join(inl) + "\n", encoding="utf-8", newline="\r\n")
    print(f"refreshed {INL_PATH.relative_to(ROOT)} ({len(data['lines'])} lines, {len(speakers)} speakers)")


if __name__ == "__main__":
    main()
