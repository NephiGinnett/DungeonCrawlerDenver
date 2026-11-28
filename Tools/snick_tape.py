"""THE SNICK TAPE: a PRIMETIME short. Hiss asks CJ and Vaughn about the snicks, they deny it, the tape rolls (2026-11-25).

Zoe: "render a video of CJ and Vaughn on Hiss Cranson's show (using the twin peaks set and nice lighting and sounds /
effects). Hiss should be asking Vaughn and CJ about the recent snicks (basically deep fakes) that were made of them and
after they deny it, the video should cut to the two of them in one of the premade assets we have emoting close
together like they're close." Hiss is the lizard man.

    python Tools/snick_tape.py voice     # voice the lines (Kokoro, offline) and write the timeline
    python Tools/snick_tape.py render    # run the game, which plays DCC.SnickTape and writes every frame
    python Tools/snick_tape.py cut       # ffmpeg: frames + captions + VHS snick + glitches + hearts + voices + music
    python Tools/snick_tape.py all

The timeline (Saved/SnickTape/timeline.json) is made from the real length of each voiced line, so the game's shots and
the cut's captions and sounds all land on the same frames. The game (Arcade/DCCSnickTape) reads it.
Voices: Hiss is his show voice (Tools/gen_talkshow_lines.py). CJ and Vaughn get their own Kokoro mixes here; they are
machine voices, not anyone's recording.
"""
from __future__ import annotations

import json
import math
import subprocess
import sys
import time
from pathlib import Path

ROOT = Path(__file__).resolve().parent.parent
WORK = ROOT / "Saved" / "SnickTape"
FRAMES = Path(r"D:\Builds\SnickTapeFrames")
OUT = Path(r"D:\Builds\Primetime_SnickTape.mp4")
ENGINE = Path(r"D:\UE\UE_5.8\Engine\Binaries\Win64\UnrealEditor.exe")
FFMPEG = Path(r"D:\Tools\ffmpeg\ffmpeg.exe")
KOKORO_DIR = Path(r"C:\Users\kmett\Downloads\TheFamiliar\voices")
FPS = 30

VOICES = {
    "Hiss": ("am_puck:0.55+am_adam:0.45", 1.06),
    "CJ": ("am_liam:0.6+am_michael:0.4", 1.0),
    "Vaughn": ("am_onyx:0.5+am_eric:0.5", 0.98),
}

# (shot, speaker, line, CJ clip, Vaughn clip). Clips are crawler anims (Content/DCC/Characters/Shared/AnimsFixed).
SCRIPT = [
    ("open", "Hiss", "Good evening, Denver, and welcome back to Primetime! I'm Hiss Cranson. Tonight, two crawlers the "
                     "whole System cannot stop talking about. CJ! Vaughn!", "Idle", "Idle"),
    ("cj", "CJ", "Hey, Hiss.", "HW_AgreeGesture", "Idle"),
    ("vaughn", "Vaughn", "Thanks for having us.", "Idle", "AO_AgreeGesture"),
    ("hiss", "Hiss", "Let's not dance around it. The snicks. Four million views overnight. The comments are, and I "
                     "quote, very supportive.", "Idle", "Idle"),
    ("cj", "CJ", "Those aren't real. That's not us.", "OA_Alert", "Idle"),
    ("vaughn", "Vaughn", "Totally fake. Somebody made those.", "Idle", "AO_AgreeGesture"),
    ("hiss_push", "Hiss", "Fake! Of course. Then you won't mind if we roll a little clip.", "Idle", "Idle"),
    ("two", "CJ", "Wait.", "DW_CallGesture", "Idle"),
    ("two", "Vaughn", "Hiss, don't.", "DW_CallGesture", "OA_Alert"),
]
SNICK = 13.0       # the tape
REACT = 2.4        # the frozen beat after it
OUTRO = ("hiss_end", "Hiss", "Fascinating. Completely fake, folks. Back after these messages from Mile High Recovery "
                             "Solutions.", "Idle", "Idle")
CARD = 3.5
GAP = 0.35         # air between lines
LEAD = 1.2         # before the first line


def spoken(text):
    return text


def voice():
    import numpy as np
    import soundfile as sf
    from kokoro_onnx import Kokoro
    kokoro = Kokoro(str(KOKORO_DIR / "kokoro-v1.0.onnx"), str(KOKORO_DIR / "voices-v1.0.bin"))
    WORK.mkdir(parents=True, exist_ok=True)
    styles = {}
    for who, (spec, _) in VOICES.items():
        style, total = None, 0.0
        for part in spec.split("+"):
            name, _, w = part.partition(":")
            v = kokoro.get_voice_style(name) * float(w or 1)
            style, total = (v if style is None else style + v), total + float(w or 1)
        styles[who] = style / total
    lines = SCRIPT + [OUTRO]
    for i, (_, who, text, _, _) in enumerate(lines):
        audio, rate = kokoro.create(text, voice=styles[who], speed=VOICES[who][1], lang="en-us")
        raw = WORK / f"raw_{i:02d}.wav"
        sf.write(str(raw), np.asarray(audio), rate, subtype="PCM_16")
        # Hiss on a stage mic; the guests a little drier.
        chain = ("equalizer=f=3000:t=q:w=1.5:g=3,aecho=0.8:0.5:40:0.12,loudnorm=I=-18:TP=-2:LRA=11" if who == "Hiss"
                 else "equalizer=f=2500:t=q:w=1.5:g=2,loudnorm=I=-18:TP=-2:LRA=11")
        subprocess.run([str(FFMPEG), "-y", "-v", "error", "-i", str(raw), "-af", chain, "-ac", "1", "-ar", "48000",
                        str(WORK / f"line_{i:02d}.wav")], check=True)
        raw.unlink()
        print(f"  {i:02d} {who:6s} {text[:60]}")
    timeline()


def duration(path):
    import soundfile as sf
    info = sf.info(str(path))
    return info.frames / info.samplerate


def timeline():
    entries, t = [], LEAD
    lines = SCRIPT
    for i, (shot, who, text, cj, vaughn) in enumerate(lines):
        d = duration(WORK / f"line_{i:02d}.wav")
        start = t - (LEAD if i == 0 else 0.0)
        end = t + d + GAP
        entries.append({"shot": shot, "start": round(start, 3), "end": round(end, 3), "speaker": who, "text": text,
                        "voice_at": round(t, 3), "wav": f"line_{i:02d}.wav", "cj": cj, "vaughn": vaughn})
        t = end
    snick_at = t + 0.2
    entries.append({"shot": "snick_wide", "start": round(t, 3), "end": round(snick_at + SNICK * 0.45, 3),
                    "cj": "HW_BoomDance", "vaughn": "DW_CardioDance"})
    entries.append({"shot": "snick_close", "start": round(snick_at + SNICK * 0.45, 3), "end": round(snick_at + SNICK, 3),
                    "cj": "AO_AllNightDance", "vaughn": "AO_AllNightDance"})
    t = snick_at + SNICK
    entries.append({"shot": "react", "start": round(t, 3), "end": round(t + REACT, 3), "cj": "Idle", "vaughn": "Idle"})
    t += REACT
    i = len(lines)
    d = duration(WORK / f"line_{i:02d}.wav")
    shot, who, text, cj, vaughn = OUTRO
    entries.append({"shot": shot, "start": round(t, 3), "end": round(t + d + 1.0, 3), "speaker": who, "text": text,
                    "voice_at": round(t + 0.2, 3), "wav": f"line_{i:02d}.wav", "cj": cj, "vaughn": vaughn})
    t += d + 1.0
    data = {"fps": FPS, "length": round(t + CARD, 3), "card_at": round(t, 3), "snick": [round(snick_at - 0.2, 3), round(snick_at + SNICK, 3)],
            "entries": entries}
    (WORK / "timeline.json").write_text(json.dumps(data, indent=1), encoding="utf-8")
    print(f"timeline: {data['length']:.1f} s, the tape from {data['snick'][0]:.1f} to {data['snick'][1]:.1f}")


def render():
    data = json.loads((WORK / "timeline.json").read_text(encoding="utf-8"))
    if FRAMES.exists():
        import shutil
        shutil.rmtree(FRAMES)
    FRAMES.mkdir(parents=True)
    cmd = [str(ENGINE), str(ROOT / "DungeonCrawlerDenver.uproject"),
           "/Engine/Maps/Entry?game=/Script/DungeonCrawlerDenver.DCCBenchGameMode",
           "-game", "-windowed", "-ResX=1920", "-ResY=1080", "-benchmark", f"-fps={FPS}", "-deterministic", "-nosound",
           "-log=SnickTape.log", f"-ExecCmds=stat none, DisableAllScreenMessages, DCC.SnickTape capture {FRAMES.as_posix()}"]
    print("rendering frames (slower than real time)...")
    started = time.time()
    proc = subprocess.Popen(cmd)
    proc.wait(timeout=60 * 90)
    frames = sorted(FRAMES.glob("frame_*.png"))
    print(f"{len(frames)} frames in {time.time() - started:.0f} s (want {int(data['length'] * FPS)})")


# ------------------------------------------------------------------------------------------------- the cut
SUB_FONT = "C\\:/Windows/Fonts/segoeuib.ttf"
TITLE_FONT = "C\\:/Windows/Fonts/impact.ttf"
SYMBOL_FONT = "C\\:/Windows/Fonts/seguisym.ttf"
MONO_FONT = "C\\:/Windows/Fonts/consolab.ttf"
SPEAKER_COLOUR = {"Hiss": "0xFFC850", "CJ": "0x8FD3FF", "Vaughn": "0xB8F28C"}


def sfx():
    """The few sounds the cut needs, made here: a static burst, a low sting, room tone."""
    def make(name, src, chain, length):
        out = WORK / name
        subprocess.run([str(FFMPEG), "-y", "-v", "error", "-f", "lavfi", "-i", src, "-t", str(length), "-af", chain,
                        "-ac", "2", "-ar", "48000", str(out)], check=True)
        return out
    make("static.wav", "anoisesrc=color=white:amplitude=0.5", "highpass=f=1200,lowpass=f=9000,tremolo=f=28:d=0.9,"
         "afade=t=in:d=0.02,afade=t=out:st=0.45:d=0.15", 0.6)
    make("sting.wav", "sine=f=98:sample_rate=48000", "volume=0.6,afade=t=in:d=0.01,afade=t=out:st=0.9:d=0.6", 1.5)
    make("sting2.wav", "sine=f=92.5:sample_rate=48000", "volume=0.6,afade=t=in:d=0.01,afade=t=out:st=1.4:d=0.8", 2.2)
    make("room.wav", "anoisesrc=color=brown:amplitude=0.05", "lowpass=f=400,volume=0.6", 120)


def esc_path(p):
    return p.replace("\\", "/").replace(":", "\\:")


def wrap_caption(text, width=90):
    """drawtext does not wrap. A line past about 95 characters runs off the right of the frame."""
    lines, cur = [], ""
    for word in text.split():
        trial = word if not cur else cur + " " + word
        if len(trial) <= width:
            cur = trial
        else:
            if cur:
                lines.append(cur)
            cur = word
    if cur:
        lines.append(cur)
    return "\n".join(lines)


def cut(song=ROOT / "Content" / "Songs" / "Swept Away.mp3", bed=ROOT / "Content" / "Songs" / "Casino Cascade.mp3"):
    data = json.loads((WORK / "timeline.json").read_text(encoding="utf-8"))
    sfx()
    frames = sorted(FRAMES.glob("frame_*.png"))
    if not frames:
        sys.exit("no frames; run render first")
    first = int(frames[0].stem.split("_")[1])
    length = len(frames) / FPS
    s0, s1 = data["snick"]
    card = data["card_at"]

    # Captions: one text file per line, so quotes and apostrophes need no escaping.
    vf = ["scale=1920:1080:flags=lanczos"]
    snick_on = f"between(t,{s0 + 0.35},{s1 - 0.1})"
    # The tape: warmer, noisier, smeared colour, a vignette, REC and a timestamp.
    vf += [f"eq=saturation=1.35:gamma_r=1.08:gamma_b=0.92:enable='{snick_on}'",
           f"rgbashift=rh=5:bh=-5:enable='{snick_on}'",
           f"noise=alls=16:allf=t+u:enable='{snick_on}'",
           f"vignette=angle=0.75:enable='{snick_on}'",
           f"drawtext=fontfile='{MONO_FONT}':text='REC':fontsize=44:fontcolor=0xFF3040:x=110:y=80:"
           f"enable='{snick_on}*lt(mod(t,1),0.6)'",
           f"drawtext=fontfile='{MONO_FONT}':text='SNICK  //  DO NOT SHARE':fontsize=34:fontcolor=white@0.85:x=w-text_w-110:y=86:"
           f"enable='{snick_on}'",
           f"drawtext=fontfile='{MONO_FONT}':text='%{{pts\\:gmtime\\:1764115200\\:%m.%d  %H\\\\\\:%M\\\\\\:%S}}':fontsize=34:"
           f"fontcolor=white@0.85:x=w-text_w-110:y=h-120:enable='{snick_on}'"]
    # Hearts drifting up out of the tape.
    for k in range(9):
        x0 = 0.30 + 0.05 * k + (0.03 if k % 2 else -0.02)
        start = s0 + 1.5 + k * 1.05
        rise = 4.5
        vf.append(f"drawtext=fontfile='{SYMBOL_FONT}':text='\u2665':fontsize={70 + 18 * (k % 3)}:fontcolor=0xFF5C9A:"
                  f"alpha='if(lt(t,{start}),0,max(0,1-(t-{start})/{rise}))':"
                  f"x=w*{x0:.3f}+30*sin((t-{start})*2.4):y=h*0.72-(t-{start})*h*0.12:"
                  f"enable='between(t,{start},{min(start + rise, s1 - 0.2)})'")
    # Glitches on the way in and out of the tape.
    for g in (s0, s1 - 0.15):
        on = f"between(t,{g},{g + 0.45})"
        vf += [f"rgbashift=rh=-24:gh=10:bh=24:enable='{on}'", f"noise=alls=70:allf=t:enable='{on}'",
               f"eq=contrast=1.6:brightness=0.08:enable='{on}'"]
    # Lower thirds for every line, PRIMETIME style for Hiss.
    for i, e in enumerate([e for e in data["entries"] if e.get("text")]):
        sub = WORK / f"sub_{i:02d}.txt"
        sub.write_text(wrap_caption(e["text"]), encoding="utf-8")
        name = WORK / f"name_{i:02d}.txt"
        name.write_text(("HISS CRANSON" if e["speaker"] == "Hiss" else e["speaker"].upper()), encoding="utf-8")
        a, b = e["voice_at"] - 0.05, e["end"] - 0.15
        on = f"between(t,{a},{b})"
        vf += [f"drawbox=x=0:y=h-248:w=iw:h=188:color=black@0.55:t=fill:enable='{on}'",
               f"drawtext=fontfile='{SUB_FONT}':textfile='{esc_path(str(name))}':fontsize=30:"
               f"fontcolor={SPEAKER_COLOUR[e['speaker']]}:x=140:y=h-228:enable='{on}'",
               f"drawtext=fontfile='{SUB_FONT}':textfile='{esc_path(str(sub))}':fontsize=36:fontcolor=white:"
               f"x=140:y=h-184:line_spacing=8:enable='{on}'"]
    # The PRIMETIME bug in the corner while we are in the Lodge.
    lodge = f"not(between(t,{s0},{s1}))*lt(t,{card})"
    vf.append(f"drawtext=fontfile='{TITLE_FONT}':text='PRIMETIME':fontsize=40:fontcolor=0xFFC850@0.9:x=w-text_w-60:y=50:"
              f"enable='{lodge}'")
    vf.append(f"drawtext=fontfile='{SUB_FONT}':text='LIVE':fontsize=24:fontcolor=0xFF3040:x=w-text_w-60:y=96:enable='{lodge}'")
    # The end card over black.
    vf += [f"drawbox=x=0:y=0:w=iw:h=ih:color=black:t=fill:enable='gte(t,{card})'",
           f"drawtext=fontfile='{TITLE_FONT}':text='PRIMETIME':fontsize=140:fontcolor=0xFFC850:x=(w-text_w)/2:y=h/2-130:"
           f"alpha='min(1,(t-{card})/0.8)':enable='gte(t,{card})'",
           f"drawtext=fontfile='{SUB_FONT}':text='with Hiss Cranson':fontsize=48:fontcolor=white:x=(w-text_w)/2:y=h/2+40:"
           f"alpha='min(1,max(0,(t-{card}-0.5)/0.8))':enable='gte(t,{card})'",
           "fade=t=in:st=0:d=1.0", f"fade=t=out:st={length - 0.6:.2f}:d=0.6"]
    (WORK / "video_filter.txt").write_text(",".join(vf), encoding="utf-8")

    # Audio: voices at their times, room tone and a quiet show bed in the Lodge, the song on the tape, static on the
    # glitches, a low two-note sting on "the snicks" and after the tape.
    inputs = ["-framerate", str(FPS), "-start_number", str(first), "-i", str(FRAMES / "frame_%05d.png")]
    parts, n = [], 1
    for e in data["entries"]:
        if not e.get("wav"):
            continue
        inputs += ["-i", str(WORK / e["wav"])]
        parts.append(f"[{n}:a]aformat=channel_layouts=stereo,adelay={int(e['voice_at'] * 1000)}:all=1[v{n}]")
        n += 1
    voices = n - 1
    inputs += ["-i", str(WORK / "room.wav")]; room = n; n += 1
    inputs += ["-ss", "8", "-i", str(bed)]; bedi = n; n += 1
    inputs += ["-ss", "35", "-i", str(song)]; songi = n; n += 1
    inputs += ["-i", str(WORK / "static.wav"), "-i", str(WORK / "static.wav")]; st1, st2 = n, n + 1; n += 2
    inputs += ["-i", str(WORK / "sting.wav"), "-i", str(WORK / "sting2.wav")]; sg1, sg2 = n, n + 1; n += 2
    snicks_line = next(e for e in data["entries"] if e.get("text", "").startswith("Let's not"))
    sting_at = snicks_line["voice_at"] + 1.9
    parts += [
        f"[{room}:a]atrim=0:{length:.2f},volume=0.5,volume=enable='between(t,{s0},{s1})':volume=0[room]",
        f"[{bedi}:a]atrim=0:{s0:.2f},volume=0.08,afade=t=in:d=1.5,afade=t=out:st={s0 - 4.0:.2f}:d=3.5[bed]",
        f"[{songi}:a]atrim=0:{s1 - s0:.2f},volume=0.8,afade=t=in:d=0.6,afade=t=out:st={s1 - s0 - 1.2:.2f}:d=1.0,"
        f"adelay={int(s0 * 1000)}:all=1[song]",
        f"[{st1}:a]volume=0.5,adelay={int(s0 * 1000)}:all=1[s1]",
        f"[{st2}:a]volume=0.5,adelay={int((s1 - 0.15) * 1000)}:all=1[s2]",
        f"[{sg1}:a]volume=0.35,adelay={int(sting_at * 1000)}:all=1[g1]",
        f"[{sg2}:a]volume=0.35,adelay={int((s1 + 0.6) * 1000)}:all=1[g2]",
    ]
    mix = "".join(f"[v{k}]" for k in range(1, voices + 1)) + "[room][bed][song][s1][s2][g1][g2]"
    parts.append(f"{mix}amix=inputs={voices + 7}:normalize=0:duration=longest,atrim=0:{length:.2f},"
                 f"afade=t=out:st={length - 1.2:.2f}:d=1.2[aout]")
    (WORK / "audio_filter.txt").write_text(";".join(parts), encoding="utf-8")
    # This ffmpeg build has no filter_complex_script. Both graphs fit on the command line.
    cmd = [str(FFMPEG), "-y", "-hide_banner", "-loglevel", "error", *inputs,
           "-filter_complex", (WORK / "audio_filter.txt").read_text(encoding="utf-8"),
           "-filter:v", (WORK / "video_filter.txt").read_text(encoding="utf-8"),
           "-map", "0:v", "-map", "[aout]", "-c:v", "libx264", "-preset", "slow", "-crf", "18", "-pix_fmt", "yuv420p",
           "-c:a", "aac", "-b:a", "192k", "-t", f"{length:.2f}", "-movflags", "+faststart", str(OUT)]
    OUT.parent.mkdir(parents=True, exist_ok=True)
    subprocess.run(cmd, check=True, cwd=str(WORK))
    print(f"wrote {OUT} ({length:.1f} s)")


if __name__ == "__main__":
    step = sys.argv[1] if len(sys.argv) > 1 else "all"
    if step in ("voice", "all"):
        voice()
    if step == "timeline":
        timeline()
    if step in ("render", "all"):
        render()
    if step in ("cut", "all"):
        cut()
