"""Write one script sheet per voice actor for the KMHR radio show.

    python Tools/radio_sides.py

Reads the lines from SourceArt/Voice/NPC/radio_lines.json (what the Voice Studio edits) and the casting from
SourceArt/Voice/Radio/cast.json ({"Scott": ["Terry"], ...}). A character nobody is cast as gets its own sheet,
named after the character, so it can be handed to whoever takes it. Sheets go to Docs/RadioScripts/.

Every line has a short number, like 3.05 (segment 3, line 5). A reader names each recording with that number
(3.05.m4a) and `python Tools/radio_mix.py import <folder>` puts it in the right place. Lines already recorded
are ticked, so re-run this after a session to send people what is left.
"""
from __future__ import annotations

import json
import re
from pathlib import Path

REPO = Path(__file__).resolve().parent.parent
NPC_DIR = REPO / "SourceArt" / "Voice" / "NPC"
CAST_FILE = REPO / "SourceArt" / "Voice" / "Radio" / "cast.json"
OUT_DIR = REPO / "Docs" / "RadioScripts"

SEGMENTS = {1: "Cold open", 2: "Ad: Summit Compliance Group", 3: "The horse", 4: "Ad: LIQUIDATOR Energy",
            5: "The call-in hour", 6: "Ad: Bright Path Solutions", 7: "Dale's theory board", 8: "Sign-off",
            9: "Station IDs (each line stands alone)"}

# Who each character is, and how to play them. Shown at the top of the reader's sheet.
BIOS = {
    "Dale": ("The host of *Altitude Sickness*, a late-night conspiracy call-in show on KMHR 5280 AM. He has spent "
             "twenty years saying the city is being watched, and he is right, which does not help him.",
             "Gravel, total conviction, no pauses. He never laughs at his own jokes. He gets quieter, not louder, "
             "when something really scares him."),
    "Terry": ("Dale's producer and call screener. He has worked this show too long to be surprised by anything. "
              "He knows the station is owned by Mile High Recovery Solutions. He just never brought it up.",
              "Deadpan and flat, like he's eating something (sometimes he is: chew if the line says so). He is never "
              "mean to Dale. He has simply given up."),
    "Announcer": ("The voice of every sponsor. Reads the ads, and sometimes cuts into Dale's sentences to start one.",
                  "Too warm. A smile you can hear. The legal disclaimers are read as fast as you can and still be "
                  "understood. When cutting in, start the line as if you were always the one talking."),
    "Brad": ("The LIQUIDATOR Energy guy. Drank four cans and outran a repossession. He has not stopped running since.",
             "Maximum volume, the whole time. Back off the mic a little so it doesn't distort. The station ID line "
             "is the same guy, far away and out of breath."),
    "Marjorie": ("Marjorie from Aurora, a long-time listener. Saw the airport horse's eyes turn green, and has a "
                 "sticker to prove it.",
                 "Elderly and completely calm. She is reporting very strange things as if they were the weather. "
                 "She is never scared."),
    "NotGary": ("A night maintenance worker at the airport who went eleven floors below baggage claim. He is "
                "calling from somewhere he should not be.",
                "Whispering, urgent, out of breath, like you're hiding. He is polite even while being hunted."),
    "VantageCaller": ("A sponsor, Vantage Wellness, calling in on the phone line as if it were a listener. It knows "
                      "Dale's heart rate.",
                      "Sweet, bright and helpful, like a wellness app that became a person. Never threatening on "
                      "purpose; that's what makes it threatening."),
}

HOW_TO_RECORD = """\
## How to record

1. **One file per line.** Name each file with the line's number, like `3.05`. Your phone's voice memo app is fine:
   record, then rename it `3.05`. Any format works (m4a, wav, mp3).
2. **A quiet room, phone about a hand's width from your mouth.** Closets full of clothes are great. Kitchens are not.
3. **Leave a second of silence** before and after each line. Don't say the number out loud.
4. **Words in [brackets] are directions.** Don't read them out. Do what they say.
5. **Redoing a line?** Name it `3.05b`. The newest file for a number is the one used, so delete any take you don't
   want.
6. A line starting with "—" cuts someone off: come in hot. A line ending in "—" gets cut off: keep going as if
   you were going to finish.
7. Send the files back in one folder. Have fun with it. Ad-libs are welcome as extra files named like `3.05-adlib`.
"""


def load_json(p: Path) -> dict:
    return json.loads(p.read_text(encoding="utf-8")) if p.exists() else {}


def number(row: str) -> str:
    m = re.match(r"KMHR_S(\d+)_(\d+)$", row)
    return f"{int(m.group(1))}.{m.group(2)}" if m else row


def recorded(line: dict) -> bool:
    return (NPC_DIR / line["speaker"] / f"VO_{line['row']}.wav").exists()


def clip(text: str, n: int = 110) -> str:
    return text if len(text) <= n else text[:n].rsplit(" ", 1)[0] + " …"


def sheet(reader: str, ids: list[str], data: dict) -> tuple[str, int, int]:
    """Only the lines this reader still has to record, numbered, and nothing else (Zoe asked for bare sheets)."""
    names = {s["id"]: s["name"] for s in data["speakers"]}
    mine = [l for l in data["lines"] if l["speaker"] in ids]
    left = [l for l in mine if not recorded(l)]
    out = [f"# {reader}", "", "Name each recording with its number, like `3.05`.", ""]
    for l in left:
        who = f" ({names[l['speaker']]})" if len(ids) > 1 else ""
        out.append(f"**{number(l['row'])}**{who} {l['text']}  ")
        out.append("")
    return "\n".join(out) + "\n", len(mine), len(left)


def full_sheet(reader: str, ids: list[str], data: dict) -> tuple[str, int, int]:
    """The longer sheet with character notes, recording tips and cues. Not used by default; kept for reference."""
    names = {s["id"]: s["name"] for s in data["speakers"]}
    lines = data["lines"]
    mine = [l for l in lines if l["speaker"] in ids]
    left = [l for l in mine if not recorded(l)]
    out = [f"# KMHR 5280 AM — {reader}'s script", ""]
    out.append(f"**You play:** {', '.join(names[i] for i in ids)}  ")
    out.append(f"**Lines:** {len(mine)} in total, **{len(left)} still to record**"
               f"{' (ticked ones are done, skip them)' if len(left) < len(mine) else ''}")
    out += ["", "*Altitude Sickness* is a late-night conspiracy radio show in Denver. The host is right that the "
            "city has been bought by an off-world TV production, and every time he gets close to saying so, a "
            "sponsor cuts in. Play it straight: the joke works best when everyone means it.", ""]
    for i in ids:
        who, how = BIOS.get(i, ("", ""))
        out += [f"## {names[i]}", "", who, "", f"**How to play it:** {how}", ""]
    out += [HOW_TO_RECORD, "## Your lines", "",
            "Each line shows the line before it (your cue), so you know what you're answering.", ""]

    seg_now = None
    for idx, l in enumerate(lines):
        if l["speaker"] not in ids:
            continue
        seg = int(re.match(r"KMHR_S(\d+)_", l["row"]).group(1))
        if seg != seg_now:
            out += [f"### Segment {seg}: {SEGMENTS.get(seg, '')}", ""]
            seg_now = seg
        prev = lines[idx - 1] if idx > 0 and lines[idx - 1]["row"].startswith(f"KMHR_S{seg:02d}_") else None
        done = recorded(l)
        tick = " ✅ *recorded*" if done else ""
        out.append(f"**{number(l['row'])}**{tick} — *{names[l['speaker']]}*  ")
        if prev and seg != 9:
            out += [f"> cue, {names[prev['speaker']]}: {clip(prev['text'])}", ""]
        out += [f"{'~~' if done else ''}{l['text']}{'~~' if done else ''}", ""]
    return "\n".join(out) + "\n", len(mine), len(left)


def main() -> None:
    data = load_json(NPC_DIR / "radio_lines.json")
    if not data:
        raise SystemExit("No SourceArt/Voice/NPC/radio_lines.json")
    names = {s["id"]: s["name"] for s in data["speakers"]}
    cast = {k: v for k, v in load_json(CAST_FILE).items() if not k.startswith("_")}
    unknown = [i for ids in cast.values() for i in ids if i not in names]
    if unknown:
        raise SystemExit(f"cast.json names characters that don't exist: {unknown}. Ids: {list(names)}")
    taken = {i for ids in cast.values() for i in ids}
    readers = dict(cast)
    open_ids = {}
    for sid, name in names.items():
        if sid not in taken:
            readers[f"Open - {name}"] = [sid]
            open_ids[f"Open - {name}"] = sid

    OUT_DIR.mkdir(parents=True, exist_ok=True)
    for old in OUT_DIR.glob("*.md"):
        old.unlink()
    index = ["# KMHR 5280 AM — scripts by reader", "",
             "Generated by `python Tools/radio_sides.py` from the Voice Studio's lines. Cast people in "
             "`SourceArt/Voice/Radio/cast.json` and re-run; don't edit these by hand.", "",
             "| Sheet | Characters | Lines | Left to record |", "|---|---|---|---|"]
    for reader, ids in readers.items():
        title = names[open_ids[reader]] if reader in open_ids else reader
        text, total, left = sheet(title, ids, data)
        if not left:   # all recorded (or voiced by Tools/radio_tts.py): nothing to hand anyone
            print(f"{reader:<40} {total:>3} lines, all done (no sheet)")
            index.append(f"| {reader} | {', '.join(names[i] for i in ids)} | {total} | done |")
            continue
        fname = (f"Open_{open_ids[reader]}" if reader in open_ids
                 else re.sub(r"[^\w-]", "_", reader)) + ".md"
        (OUT_DIR / fname).write_text(text, encoding="utf-8")
        index.append(f"| [{reader}]({fname}) | {', '.join(names[i] for i in ids)} | {total} | {left} |")
        print(f"{fname:<40} {total:>3} lines, {left:>3} left")
    index += ["", "Getting the recordings in: put a reader's files in a folder and run "
              "`python Tools/radio_mix.py import <folder>`. Then `python Tools/radio_mix.py studio` mixes the show."]
    (OUT_DIR / "README.md").write_text("\n".join(index) + "\n", encoding="utf-8")
    print(f"-> {OUT_DIR}")


if __name__ == "__main__":
    main()
