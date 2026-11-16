"""Voice a KMHR radio character with Kokoro (offline, on this PC) instead of a person.

    python Tools/radio_tts.py Terry              every line of Terry's not recorded yet
    python Tools/radio_tts.py Terry --replace    redo all of Terry's lines, keeping the old ones as .wav.bak
    python Tools/radio_tts.py Terry --only 3.05  one line

Lines come from SourceArt/Voice/NPC/radio_lines.json (what the Voice Studio edits). Directions in [brackets] are
not spoken. Output goes where a studio recording would: SourceArt/Voice/NPC/<Speaker>/VO_<row>.wav, with the
character's studio voice (nasal, chest, air, pitch) applied by ffmpeg, so `radio_mix.py studio` uses it like any
take. A friend's recording replaces one later: `radio_mix.py import <folder> --replace`.

Generated rows are listed in SourceArt/Voice/Radio/ai_takes.json, so it's always clear which lines are a machine.
Kokoro's model files are borrowed from The Familiar, same as Tools/gen_onboarding_content.py.
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
AI_LIST = REPO / "SourceArt" / "Voice" / "Radio" / "ai_takes.json"
KOKORO_DIR = Path(r"C:\Users\kmett\Downloads\TheFamiliar\voices")
FFMPEG = Path(r"D:\Tools\ffmpeg\ffmpeg.exe")

# Kokoro voice blend ("name:weight+name:weight") and speed per character.
VOICES = {
    "Terry": ("am_michael:0.7+am_onyx:0.3", 0.94),   # flat, a little low, in no hurry
    "Brad": ("am_puck:0.7+am_eric:0.3", 1.15),
    "NotGary": ("am_echo:0.6+am_adam:0.4", 1.05),
    "VantageCaller": ("af_bella:0.6+af_nicole:0.4", 1.0),
    "Dale": ("am_onyx:0.7+am_michael:0.3", 1.0),
    "Marjorie": ("af_sarah:0.6+af_nicole:0.4", 0.92),
    "Announcer": ("af_nova:0.5+af_bella:0.5", 1.05),
}


def voice_filter(voice: dict) -> str:
    """The Voice Studio's character controls (chain.js), as ffmpeg filters."""
    f = []
    pitch = voice.get("pitch", 0)
    if pitch:
        ratio = 2 ** (pitch / 12)
        f += [f"asetrate=24000*{ratio:.5f}", "aresample=24000", f"atempo={1 / ratio:.5f}"]
    if voice.get("nasal"):
        f += [f"equalizer=f=1100:t=q:w=2.2:g={voice['nasal'] * 10:.2f}",
              f"equalizer=f=280:t=q:w=1:g={-voice['nasal'] * 5:.2f}"]
    if voice.get("body"):
        f.append(f"bass=f=220:g={voice['body']}")
    if voice.get("air"):
        f.append(f"treble=f=6500:g={voice['air']}")
    return ",".join(f)


def spoken(text: str) -> str:
    """What Kokoro reads: no [directions], dashes as pauses."""
    t = re.sub(r"\[[^\]]*\]", "", text)
    t = t.replace("—", ",").replace("…", "...").replace("KMHR", "K. M. H. R.")
    return re.sub(r"\s+", " ", t).strip(" ,")


def main() -> None:
    ap = argparse.ArgumentParser()
    ap.add_argument("speaker")
    ap.add_argument("--replace", action="store_true")
    ap.add_argument("--only", default="", help="one line number, like 3.05")
    args = ap.parse_args()

    data = json.loads((NPC_DIR / "radio_lines.json").read_text(encoding="utf-8"))
    speaker = next((s for s in data["speakers"] if s["id"].lower() == args.speaker.lower()), None)
    if not speaker:
        raise SystemExit(f"No speaker {args.speaker}. Ids: {[s['id'] for s in data['speakers']]}")
    sid = speaker["id"]
    spec, speed = VOICES.get(sid, ("am_michael", 1.0))
    only = ""
    if args.only:
        seg, _, n = args.only.partition(".")
        only = f"KMHR_S{int(seg):02d}_{int(n):02d}"

    import numpy as np
    import soundfile as sf
    from kokoro_onnx import Kokoro
    kokoro = Kokoro(str(KOKORO_DIR / "kokoro-v1.0.onnx"), str(KOKORO_DIR / "voices-v1.0.bin"))
    style, total = None, 0.0
    for part in spec.split("+"):
        name, _, w = part.partition(":")
        v = kokoro.get_voice_style(name) * float(w or 1)
        style, total = (v if style is None else style + v), total + float(w or 1)
    style = style / total

    ai = json.loads(AI_LIST.read_text(encoding="utf-8")) if AI_LIST.exists() else {}
    vf = voice_filter(speaker.get("voice", {}))
    made = 0
    for line in data["lines"]:
        if line["speaker"] != sid or (only and line["row"] != only):
            continue
        dest = NPC_DIR / sid / f"VO_{line['row']}.wav"
        if dest.exists() and not (args.replace or only):
            continue
        audio, rate = kokoro.create(spoken(line["text"]), voice=style, speed=speed, lang="en-us")
        with tempfile.TemporaryDirectory() as tmp:
            raw = Path(tmp) / "raw.wav"
            sf.write(str(raw), np.asarray(audio), rate, subtype="PCM_16")
            dest.parent.mkdir(parents=True, exist_ok=True)
            if dest.exists():
                dest.replace(dest.with_suffix(".wav.bak"))
            chain = ",".join(x for x in [vf, "loudnorm=I=-20:TP=-2:LRA=11"] if x)
            r = subprocess.run([str(FFMPEG), "-y", "-v", "error", "-i", str(raw), "-af", chain,
                                "-ac", "1", "-ar", "48000", "-c:a", "pcm_s16le", str(dest)],
                               capture_output=True, text=True)
            if r.returncode:
                raise SystemExit(r.stderr)
        ai[line["row"]] = sid
        made += 1
        print(f"  {line['row']}  {spoken(line['text'])[:70]}")

    AI_LIST.parent.mkdir(parents=True, exist_ok=True)
    AI_LIST.write_text(json.dumps(dict(sorted(ai.items())), indent=2) + "\n", encoding="utf-8")
    print(f"\n{made} line(s) voiced for {speaker['name']} ({spec}, speed {speed}).")


if __name__ == "__main__":
    main()
