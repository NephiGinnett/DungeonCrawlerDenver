"""PRIMETIME: Hiss Cranson's talk show in the Den, and everything he says (2026-11-23).

Zoe: "How about making a talk show NPC with many voice lines and they can introduce the players and show them the fame
meter and everything like that."

THE LINES LIVE IN  SourceArt/Voice/NPC/talkshow_lines.json. The game reads it when the Den loads (ADCCTalkShowHost), so
an edited line is heard next visit with no rebuild. Each line has a "tag" that says where it can go in a segment.

    python Tools/gen_talkshow_lines.py            # write the JSON (keeps it if present, adds new rows)
    python Tools/gen_talkshow_lines.py --reset    # overwrite the JSON with the lines below
    python Tools/gen_talkshow_lines.py --voice    # voice every line with no recording yet (Kokoro, offline)
    python Tools/gen_talkshow_lines.py --voice --replace   # redo every take

Takes land where the Voice Studio puts recordings: SourceArt/Voice/NPC/Hiss/VO_<row>.wav. A friend's recording
replaces a machine take the same way it does for the radio (radio_mix.py import). Machine takes are listed in
SourceArt/Voice/Radio/ai_takes.json, same as the radio's.

A segment, when somebody walks up to the stage: an OPEN, then for each crawler in the room an INTRO (their own when
they have one, INTRO_ANY otherwise) and a reaction to their Fame (FAME_LOW / FAME_MID / FAME_HIGH, or CHEAT if they
have Integrity strikes), then two FAME explainers, a TIP, and a CLOSE. Between segments, an IDLE now and then.
The host is original to this game: smarmy, sponsored, delighted by everything, in the System's universe.
"""
from __future__ import annotations

import argparse
import json
import re
import subprocess
import tempfile
from pathlib import Path

REPO = Path(__file__).resolve().parent.parent
NPC_DIR = REPO / "SourceArt" / "Voice" / "NPC"
JSON_PATH = NPC_DIR / "talkshow_lines.json"
AI_LIST = REPO / "SourceArt" / "Voice" / "Radio" / "ai_takes.json"
KOKORO_DIR = Path(r"C:\Users\kmett\Downloads\TheFamiliar\voices")
FFMPEG = Path(r"D:\Tools\ffmpeg\ffmpeg.exe")

# Hiss Cranson is an AUDITION on Voice Drop (2026-11-23): everyone may send the sample line once; Zoe picks the voice
# in /admin and unlocks the rest for them. The chosen voice gets the Hot Mic.
SPEAKER = {"id": "Hiss", "name": "Hiss Cranson", "body": "office_311",
           "audition": {"row": "TS_Open_01", "prize": "The Hot Mic (Epic)", "item": "HotMic"}}
VOICE = ("am_puck:0.55+am_adam:0.45", 1.06)  # bright, a little too warm, never stops smiling

L = []
def add(tag, row, text):
    L.append({"speaker": "Hiss", "row": row, "tag": tag, "text": text})

# ---- OPEN ------------------------------------------------------------------------------------------------------------
add("OPEN", "TS_Open_01", "Good evening, Denver, and welcome to Primetime! I'm Hiss Cranson, and yes, the hiss is silent. Except when it isn't.")
add("OPEN", "TS_Open_02", "We are live! We are always live. There is no off switch. I asked.")
add("OPEN", "TS_Open_03", "Welcome back to Primetime, brought to you by Mile High Recovery Solutions. If you can read this, your car is already ours.")
add("OPEN", "TS_Open_04", "Hello, hello, hello! Tonight's audience is enormous, invisible, and extremely hungry for content.")
add("OPEN", "TS_Open_05", "Primetime is back, baby! The only show filmed in a safe room, which, legally, is not safe.")
add("OPEN", "TS_Open_06", "Welcome to the Safe Room, the coziest holding pen in the System. Let's meet tonight's crawlers!")
add("OPEN", "TS_Open_07", "Lights, camera, liability! It's Primetime!")
add("OPEN", "TS_Open_08", "Ladies, gentlemen, and whatever is watching from the third moon, welcome to Primetime.")

# ---- INTRO, one set per crawler ------------------------------------------------------------------------------------
add("INTRO_CJ", "TS_Intro_CJ_01", "Give it up for CJ! Hockey jersey, real fire, zero regrets.")
add("INTRO_CJ", "TS_Intro_CJ_02", "It's CJ, everybody! The jersey is on fire and so, frankly, are the ratings.")
add("INTRO_CJ", "TS_Intro_CJ_03", "CJ is here! Sponsors love a man who dresses for the penalty box.")
add("INTRO_Scott", "TS_Intro_Scott_01", "Scott, everybody! Scott! Somebody get Scott a chair, he looks like he has a plan.")
add("INTRO_Scott", "TS_Intro_Scott_02", "Put your hands together for Scott, who I'm told has filed a complaint about this introduction already.")
add("INTRO_Scott", "TS_Intro_Scott_03", "Here's Scott! Calm, collected, and only a little bit airborne.")
add("INTRO_Marcel", "TS_Intro_Marcel_01", "Marcel is in the building! Marcel, the audience asked me to tell you they have notes.")
add("INTRO_Marcel", "TS_Intro_Marcel_02", "Give it up for Marcel! Every wall in this city is a canvas if you are brave enough.")
add("INTRO_Marcel", "TS_Intro_Marcel_03", "Marcel! Our resident artist, vandal, and part time tow truck enthusiast.")
add("INTRO_Vaughn", "TS_Intro_Vaughn_01", "And here's Vaughn! The Integrity Office would like a word. Several words. Many forms.")
add("INTRO_Vaughn", "TS_Intro_Vaughn_02", "Vaughn, everybody! The audience counted their tickets twice when he walked in.")
add("INTRO_Vaughn", "TS_Intro_Vaughn_03", "Vaughn is here! Our producers have asked me to keep my wallet in view at all times.")
add("INTRO_Kyle", "TS_Intro_Kyle_01", "Kyle! Kyle is here! The tube spat him out and he landed on his feet. Mostly.")
add("INTRO_Kyle", "TS_Intro_Kyle_02", "Give it up for Kyle, a man the gacha machine calls by his first name.")
add("INTRO_Kyle", "TS_Intro_Kyle_03", "Here comes Kyle, everybody. Lock up the snacks.")
add("INTRO_Zoe", "TS_Intro_Zoe_01", "And the one who built this whole place, Zoe! Please don't delete me, Zoe.")
add("INTRO_Zoe", "TS_Intro_Zoe_02", "Zoe is in the room! The System runs on her code and, I'm told, a great deal of coffee.")
add("INTRO_Zoe", "TS_Intro_Zoe_03", "Give it up for Zoe, our showrunner, our architect, and the reason I have a face.")
add("INTRO_ANY", "TS_Intro_Any_01", "And joining us, a crawler with a name I have been told not to mispronounce. Give it up!")
add("INTRO_ANY", "TS_Intro_Any_02", "A new face! The audience loves a new face. They love it more when it is a little scared.")
add("INTRO_ANY", "TS_Intro_Any_03", "Welcome, welcome! Find a seat, find a game, find out what Fame can do for you.")

# ---- FAME reactions --------------------------------------------------------------------------------------------------
add("FAME_LOW", "TS_FameLow_01", "Fame, still a little low. Don't worry. Nobody is famous until something explodes near them.")
add("FAME_LOW", "TS_FameLow_02", "The Fame meter says, and I quote, who? We can fix that. Go do something stupid on camera.")
add("FAME_LOW", "TS_FameLow_03", "Fresh off the bench! The first Fame level pays a loot box. Go earn it.")
add("FAME_MID", "TS_FameMid_01", "Look at that Fame! The audience knows your name. Some of them can even spell it.")
add("FAME_MID", "TS_FameMid_02", "A respectable Fame level. Respectable is just famous with good posture.")
add("FAME_MID", "TS_FameMid_03", "That Fame is climbing. Gold boxes are coming. I can smell the shrink wrap.")
add("FAME_HIGH", "TS_FameHigh_01", "That is a serious Fame level. Sponsors have started naming yachts after you.")
add("FAME_HIGH", "TS_FameHigh_02", "Look at this star! The audience is chanting. The audience is always chanting, but tonight it's for you.")
add("FAME_HIGH", "TS_FameHigh_03", "Legendary boxes territory. Please sign my forehead.")
add("CHEAT", "TS_Cheat_01", "Our records show a few strikes from the Integrity Office. The audience loves a villain. The house does not.")
add("CHEAT", "TS_Cheat_02", "Ooh, an Integrity strike! Folks, while that nameplate says cheater, you are all allowed to hit them. It's in the contract.")
add("CHEAT", "TS_Cheat_03", "A cheater on Primetime! Ratings up, trust down, and the fines are delicious.")

# ---- FAME explainers -------------------------------------------------------------------------------------------------
add("FAME", "TS_Fame_01", "Look down in the corner. That's the live feed. How many people are watching you right now, and how excited they are.")
add("FAME", "TS_Fame_02", "Under the viewer count is your Fame bar. Kills, tricks, revives, jackpots, they all fill it. Fill it up and a loot box shows up.")
add("FAME", "TS_Fame_03", "Every Fame level pays a loot box. Bronze first, then Silver, Gold, Platinum, Legendary. Every tenth level, Celestial.")
add("FAME", "TS_Fame_04", "When the hype bar turns pink, the room is on fire, and everything you do is worth double Fame. Strike while it's hot.")
add("FAME", "TS_Fame_05", "The chat in the corner is real. Well, it's real to them. Do something big and they will absolutely talk about it.")
add("FAME", "TS_Fame_06", "Saving a friend who's down is the best Fame in the house. The audience loves a rescue more than a kill. Weird crowd.")
add("FAME", "TS_Fame_07", "Your loot boxes wait in the Tab menu, under loot boxes. Open them on camera. Always on camera.")
add("FAME", "TS_Fame_08", "Fame follows you into every game. Gate runs, the rally, the casino. The audience doesn't care where, only how loud.")

# ---- TIPS ------------------------------------------------------------------------------------------------------------
add("TIP", "TS_Tip_01", "New to Repo Rally? There's a practice pad now. No clock, no pay, all the tow truck ballet you can stand.")
add("TIP", "TS_Tip_02", "In Repo Rally, lift the car overhead and hit space as the ring closes. Perfect timing climbs the trick ladder. The yard pays for style.")
add("TIP", "TS_Tip_03", "The Friend In A Tube machine does ten-pulls now. Ten friends, one guaranteed rare, keep your favorites, evaporate the rest. Sad, but efficient.")
add("TIP", "TS_Tip_04", "Want your damage numbers on fire? The prize counter sells damage skins. Inferno, Frostbite, Past Due. Very tasteful. Very loud.")
add("TIP", "TS_Tip_05", "Spend your stat points! Every point above four makes that stat hit five percent harder. The math department insisted I say that.")
add("TIP", "TS_Tip_06", "Your class perks are a commitment now. First pick is free. Changing your mind costs gold. Commitment is sexy.")
add("TIP", "TS_Tip_07", "The casino keeps a regulars card. Play enough and the house gives a little back. Never enough. A little.")
add("TIP", "TS_Tip_08", "Gate Run tip! Each gate has a theme, a hazard, and contracts in the corner. Finish a contract and the payout gets fancy.")
add("TIP", "TS_Tip_09", "If the floor glows, don't stand on it. That's not a tip, that's a life philosophy.")
add("TIP", "TS_Tip_10", "Bleeding out? Press space to roll your death saves now. The dice still decide. The dice always decide.")

# ---- CLOSE -----------------------------------------------------------------------------------------------------------
add("CLOSE", "TS_Close_01", "That's our show! Stand on a pad to vote for the next game. Majority wins. Democracy, but with tow trucks.")
add("CLOSE", "TS_Close_02", "Go out there and be watchable! I'll be right here. I'm always right here. I can't leave the desk.")
add("CLOSE", "TS_Close_03", "Thank you, Denver! Remember, the cameras love you, and the cameras never blink.")
add("CLOSE", "TS_Close_04", "This has been Primetime. Tell your friends. Tell your enemies. Tell the third moon.")
add("CLOSE", "TS_Close_05", "Back after these messages from Mile High Recovery Solutions. Please do not run from the messages.")

# ---- IDLE ------------------------------------------------------------------------------------------------------------
add("IDLE", "TS_Idle_01", "Stick around, folks. Next segment in a moment. My makeup person is reapplying my smile.")
add("IDLE", "TS_Idle_02", "If you're just joining us, the Fame board is behind me. Look how famous everyone is. Look how famous I am.")
add("IDLE", "TS_Idle_03", "Somebody tell the audience to stop throwing popcorn at the camera. Thank you.")
add("IDLE", "TS_Idle_04", "I have been asked by the network to remind you all that I am a beloved personality.")
add("IDLE", "TS_Idle_05", "Fun fact, this desk is load-bearing. Please do not lean on it during the apocalypse.")
add("IDLE", "TS_Idle_06", "Still here! Still smiling! Still under contract!")


def write_json(reset: bool) -> dict:
    data = {"speakers": [SPEAKER], "lines": L}
    if JSON_PATH.exists() and not reset:
        old = json.loads(JSON_PATH.read_text(encoding="utf-8"))
        have = {l["row"] for l in old.get("lines", [])}
        old["lines"] += [l for l in L if l["row"] not in have]
        old["speakers"] = [SPEAKER]
        data = old
    JSON_PATH.parent.mkdir(parents=True, exist_ok=True)
    JSON_PATH.write_text(json.dumps(data, indent=2, ensure_ascii=False) + "\n", encoding="utf-8")
    print(f"{JSON_PATH.relative_to(REPO)}: {len(data['lines'])} lines")
    return data


def spoken(text: str) -> str:
    t = re.sub(r"\[[^\]]*\]", "", text)
    return re.sub(r"\s+", " ", t).strip()


def voice(data: dict, replace: bool) -> None:
    import numpy as np
    import soundfile as sf
    from kokoro_onnx import Kokoro
    kokoro = Kokoro(str(KOKORO_DIR / "kokoro-v1.0.onnx"), str(KOKORO_DIR / "voices-v1.0.bin"))
    spec, speed = VOICE
    style, total = None, 0.0
    for part in spec.split("+"):
        name, _, w = part.partition(":")
        v = kokoro.get_voice_style(name) * float(w or 1)
        style, total = (v if style is None else style + v), total + float(w or 1)
    style = style / total
    ai = json.loads(AI_LIST.read_text(encoding="utf-8")) if AI_LIST.exists() else {}
    made = 0
    for line in data["lines"]:
        dest = NPC_DIR / line["speaker"] / f"VO_{line['row']}.wav"
        if dest.exists() and not replace:
            continue
        audio, rate = kokoro.create(spoken(line["text"]), voice=style, speed=speed, lang="en-us")
        with tempfile.TemporaryDirectory() as tmp:
            raw = Path(tmp) / "raw.wav"
            sf.write(str(raw), np.asarray(audio), rate, subtype="PCM_16")
            dest.parent.mkdir(parents=True, exist_ok=True)
            # A little presence and a touch of room, so he sounds like he is on a stage.
            chain = "equalizer=f=3000:t=q:w=1.5:g=3,aecho=0.8:0.5:40:0.12,loudnorm=I=-18:TP=-2:LRA=11"
            r = subprocess.run([str(FFMPEG), "-y", "-v", "error", "-i", str(raw), "-af", chain,
                                "-ac", "1", "-ar", "48000", "-c:a", "pcm_s16le", str(dest)], capture_output=True, text=True)
            if r.returncode:
                raise SystemExit(r.stderr)
        ai[line["row"]] = line["speaker"]
        made += 1
        print(f"  {line['row']}  {spoken(line['text'])[:70]}")
    AI_LIST.parent.mkdir(parents=True, exist_ok=True)
    AI_LIST.write_text(json.dumps(dict(sorted(ai.items())), indent=2) + "\n", encoding="utf-8")
    print(f"{made} line(s) voiced for Hiss ({spec}, speed {speed}).")


if __name__ == "__main__":
    ap = argparse.ArgumentParser()
    ap.add_argument("--reset", action="store_true")
    ap.add_argument("--voice", action="store_true")
    ap.add_argument("--replace", action="store_true")
    a = ap.parse_args()
    d = write_json(a.reset)
    if a.voice:
        voice(d, a.replace)
