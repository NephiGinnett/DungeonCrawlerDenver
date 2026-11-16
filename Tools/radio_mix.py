"""Mix recorded radio-show voice tracks into finished, game-ready segments.

Script: Docs/radio-show-altitude-sickness.md. Each segment is one folder of audio files:

    RadioShow/
      cast.json                  optional: {"Scott": "dale", "CJ": "caller", ...}
      01_cold_open/
        1-Scott.flac             one file per person, all starting at the same moment
        2-CJ.flac                (a Craig/Discord multitrack recording gives exactly this)
        music_theme.wav          optional bed, looped, ducked under the voices
        sfx_static.wav           optional effects, placed with segment.json
        segment.json             optional, see below
      02_ad_summit/
        ...

Usage:
    python Tools/radio_mix.py studio                lines recorded in the Voice Studio (KMHR 5280 AM) ->
                                                    SourceArt/Voice/Radio/out/KMHR_S01.wav ..., one file per
                                                    station ID, and KMHR_full_show.wav. Music/effects for a
                                                    segment go in SourceArt/Voice/Radio/S01/ etc. (same rules)
    python Tools/radio_mix.py import <folder> [--replace]
                                                    a friend's takes named by line number (3.05.m4a) -> the
                                                    Voice Studio's folders. Never overwrites without --replace.
                                                    Scripts for each reader: python Tools/radio_sides.py
    python Tools/radio_mix.py show  <show_dir>    every segment folder -> <show_dir>/out/, plus full_show.wav
    python Tools/radio_mix.py mix   <segment_dir>   one segment -> <segment_dir>/../out/<name>.wav
    python Tools/radio_mix.py selftest              builds fake tracks in a temp folder and mixes them

Each voice track gets a sound by ROLE:
    am     - host and producer: AM-radio band, compressed (Dale, Terry)
    phone  - callers: narrow phone band with a little grit
    clean  - the announcer: full and warm, a touch of presence. Sounding nicer than the show is the joke.
    shout  - Brad from LIQUIDATOR: crushed and limited
A file's role comes from (first match wins): segment.json, cast.json, or a role word in the filename
(dale, terry, caller, announcer, brad...). Anything unmatched gets "am" with a warning.

segment.json (every field optional):
    {
      "tracks": {"2-CJ.flac": {"role": "phone", "gain_db": 2, "offset": 0.5}},
      "music":  {"gain_db": -16, "duck": true},
      "sfx":    {"sfx_static.wav": {"at": 0, "gain_db": -6}, "sfx_sting.wav": {"at": "end"}}
    }
"at": "end" places an effect right after the last voice ends (the sign-off's static swell).

Output: 48 kHz 16-bit mono WAV, loudness-normalised to -16 LUFS, ready to import into Unreal and put in
ADCCRover::RadioTracks.
"""
from __future__ import annotations

import json
import re
import subprocess
import sys
import tempfile
from pathlib import Path

FFMPEG = Path(r"D:\Tools\ffmpeg\ffmpeg.exe")
FFPROBE = Path(r"D:\Tools\ffmpeg\ffprobe.exe")
AUDIO_EXT = {".wav", ".flac", ".mp3", ".ogg", ".m4a", ".aac", ".opus"}

ROLE_CHAINS = {
    "am": "highpass=f=150,lowpass=f=5000,"
          "acompressor=threshold=0.08:ratio=4:attack=10:release=150:makeup=2",
    "phone": "highpass=f=300,lowpass=f=3400,"
             "acompressor=threshold=0.1:ratio=6:attack=5:release=100:makeup=2,"
             "acrusher=bits=12:mode=log:aa=1:mix=0.3",
    "clean": "highpass=f=80,equalizer=f=3000:t=q:w=1:g=2,"
             "acompressor=threshold=0.12:ratio=2.5:attack=15:release=200:makeup=1.5",
    "shout": "highpass=f=200,equalizer=f=2500:t=q:w=1:g=4,"
             "acompressor=threshold=0.05:ratio=12:attack=2:release=60:makeup=4,"
             "acrusher=bits=8:mode=log:aa=1:mix=0.25,alimiter=limit=0.8",
}

# Words in a filename or cast.json value that pick a role.
ROLE_ALIASES = {
    "dale": "am", "host": "am", "terry": "am", "producer": "am", "am": "am",
    "caller": "phone", "phone": "phone", "marjorie": "phone", "notgary": "phone", "gary": "phone",
    "announcer": "clean", "ad": "clean", "clean": "clean",
    "brad": "shout", "liquidator": "shout", "shout": "shout",
}


def run(cmd: list[str]) -> str:
    r = subprocess.run(cmd, capture_output=True, text=True, encoding="utf-8", errors="replace")
    if r.returncode != 0:
        sys.exit(f"ffmpeg failed:\n{' '.join(cmd)}\n\n{r.stderr[-3000:]}")
    return r.stdout


def duration(path: Path) -> float:
    out = run([str(FFPROBE), "-v", "error", "-show_entries", "format=duration",
               "-of", "default=nw=1:nk=1", str(path)])
    return float(out.strip() or 0)


def role_for(path: Path, override: dict, cast: dict) -> str:
    if "role" in override:
        return ROLE_ALIASES.get(override["role"].lower(), override["role"].lower())
    stem = path.stem.lower()
    for person, role in cast.items():
        if person.lower() in stem:
            return ROLE_ALIASES.get(role.lower(), role.lower())
    for word, role in ROLE_ALIASES.items():
        if len(word) > 2 and word in stem:
            return role
    print(f"  ! no role for {path.name}, using 'am' (add it to cast.json or segment.json)")
    return "am"


def load_json(path: Path) -> dict:
    return json.loads(path.read_text(encoding="utf-8")) if path.exists() else {}


def extras(folder: Path) -> tuple[list[Path], list[Path], dict]:
    """A folder's music bed(s), effects and segment.json."""
    if not folder.is_dir():
        return [], [], {}
    files = sorted(p for p in folder.iterdir() if p.suffix.lower() in AUDIO_EXT)
    return ([p for p in files if p.stem.lower().startswith("music")],
            [p for p in files if p.stem.lower().startswith("sfx")],
            load_json(folder / "segment.json"))


def mix_segment(seg: Path, out: Path) -> Path:
    """One folder of multitrack recordings (everyone recorded at once)."""
    music, sfx, cfg = extras(seg)
    cast = load_json(seg / "cast.json") or load_json(seg.parent / "cast.json")
    voices = [p for p in sorted(seg.iterdir()) if p.suffix.lower() in AUDIO_EXT and p not in music and p not in sfx]
    if not voices:
        sys.exit(f"{seg}: no voice tracks found")
    print(f"Mixing {seg.name}")
    plan = []
    for v in voices:
        o = cfg.get("tracks", {}).get(v.name, {})
        plan.append((v, role_for(v, o, cast), float(o.get("offset", 0)), o.get("gain_db", 0)))
    return render(plan, music, sfx, cfg, out)


def render(plan: list[tuple[Path, str, float, float]], music: list[Path], sfx: list[Path], cfg: dict,
           out: Path) -> Path:
    """plan: (file, role, start seconds, gain dB) per voice clip."""
    inputs: list[str] = []
    graph: list[str] = []
    vox_labels = []
    voice_end = 0.0

    for i, (v, role, offset, gain) in enumerate(plan):
        if role not in ROLE_CHAINS:
            sys.exit(f"{v.name}: unknown role '{role}' (use one of {', '.join(ROLE_CHAINS)})")
        voice_end = max(voice_end, offset + duration(v))
        print(f"  voice {v.name:<28} role={role:<6} at={offset:.2f}s")
        inputs += ["-i", str(v)]
        graph.append(f"[{i}:a]aresample=48000,aformat=channel_layouts=mono,{ROLE_CHAINS[role]},"
                     f"volume={gain}dB,adelay={int(max(offset, 0) * 1000)}:all=1[v{i}]")
        vox_labels.append(f"[v{i}]")
    voices = plan

    # Effects: work out placement first, so the total length (and the music bed) covers them.
    total = voice_end
    sfx_plan = []
    for s in sfx:
        o = cfg.get("sfx", {}).get(s.name, {})
        at = voice_end if o.get("at") == "end" else float(o.get("at", 0))
        total = max(total, at + duration(s))
        sfx_plan.append((s, at, o.get("gain_db", -4)))

    graph.append(f"{''.join(vox_labels)}amix=inputs={len(vox_labels)}:normalize=0:dropout_transition=0[vox]")
    final = []
    n = len(voices)

    if music:
        mcfg = cfg.get("music", {})
        inputs += ["-stream_loop", "-1", "-i", str(music[0])]
        fade_start = max(total - 2.0, 0)
        graph.append(f"[{n}:a]aresample=48000,aformat=channel_layouts=mono,volume={mcfg.get('gain_db', -16)}dB,"
                     f"atrim=0:{total:.3f},afade=t=out:st={fade_start:.3f}:d=2[mus]")
        n += 1
        if mcfg.get("duck", True):
            graph.append("[vox]asplit=2[voxout][voxkey]")
            graph.append("[mus][voxkey]sidechaincompress=threshold=0.02:ratio=8:attack=20:release=400[musd]")
            final += ["[voxout]", "[musd]"]
        else:
            final += ["[vox]", "[mus]"]
        if len(music) > 1:
            print(f"  ! only the first music file is used ({music[0].name})")
    else:
        final.append("[vox]")

    for j, (s, at, gain) in enumerate(sfx_plan):
        inputs += ["-i", str(s)]
        graph.append(f"[{n}:a]aresample=48000,aformat=channel_layouts=mono,volume={gain}dB,"
                     f"adelay={int(at * 1000)}:all=1[fx{j}]")
        final.append(f"[fx{j}]")
        print(f"  sfx   {s.name:<28} at={at:.2f}s")
        n += 1

    graph.append(f"{''.join(final)}amix=inputs={len(final)}:normalize=0:duration=longest,"
                 f"atrim=0:{total + 0.5:.3f},loudnorm=I=-16:TP=-1.5:LRA=11,aresample=48000[out]")

    out.parent.mkdir(parents=True, exist_ok=True)
    run([str(FFMPEG), "-y", "-hide_banner", *inputs, "-filter_complex", ";".join(graph),
         "-map", "[out]", "-ac", "1", "-ar", "48000", "-c:a", "pcm_s16le", str(out)])
    print(f"  -> {out}  ({duration(out):.1f}s)")
    return out


def mix_show(show: Path) -> None:
    segs = sorted(p for p in show.iterdir() if p.is_dir() and p.name != "out"
                  and any(f.suffix.lower() in AUDIO_EXT for f in p.iterdir()))
    if not segs:
        sys.exit(f"{show}: no segment folders with audio in them")
    outs = [mix_segment(s, show / "out" / f"{s.name}.wav") for s in segs]
    concat(outs, show / "out" / "full_show.wav")


# --- Studio mode: lines recorded one at a time in the Companion's Voice Studio ---------------------------------

REPO = Path(__file__).resolve().parent.parent
NPC_DIR = REPO / "SourceArt" / "Voice" / "NPC"
RADIO_DIR = REPO / "SourceArt" / "Voice" / "Radio"   # optional S01/ ... folders of music_*/sfx_*, and out/
SPEAKER_ROLES = {"Dale": "am", "Terry": "am", "Announcer": "clean", "Brad": "shout",
                 "Marjorie": "phone", "NotGary": "phone", "VantageCaller": "phone"}
STANDALONE_SEGMENT = 9   # station IDs: every line is its own file, not one segment
GAP = 0.3                # breath between lines
OVERLAP = -0.25          # a line that cuts in ("— but first") starts before the last one ends


def mix_studio() -> None:
    data = load_json(NPC_DIR / "radio_lines.json")
    if not data:
        sys.exit(f"No {NPC_DIR / 'radio_lines.json'}")
    roles = {s["id"]: SPEAKER_ROLES.get(s["id"], "am") for s in data["speakers"]}
    segments: dict[int, list[dict]] = {}
    for line in data["lines"]:            # file order is script order
        m = re.match(r"KMHR_S(\d+)_", line["row"])
        if m:
            segments.setdefault(int(m.group(1)), []).append(line)

    out_dir = RADIO_DIR / "out"
    outs, missing = [], []
    for seg, lines in sorted(segments.items()):
        takes = []
        for line in lines:
            f = NPC_DIR / line["speaker"] / f"VO_{line['row']}.wav"
            (takes if f.exists() else missing).append((line, f) if f.exists() else line["row"])
        if not takes:
            continue
        if seg == STANDALONE_SEGMENT:
            for line, f in takes:
                print(f"Mixing {line['row']}")
                render([(f, roles[line["speaker"]], 0.0, 0)], [], [], {}, out_dir / f"{line['row']}.wav")
            continue
        print(f"Mixing segment {seg:02d}")
        plan, t, prev_text = [], 0.0, ""
        for line, f in takes:
            text = line["text"]
            cuts_in = prev_text.rstrip().endswith("—") or re.match(r"^(\[[^\]]*\]\s*)?—", text)
            gap = 3.0 if "seconds of silence" in text else OVERLAP if cuts_in else GAP
            start = max(0.0, t + gap) if plan else 0.0
            plan.append((f, roles[line["speaker"]], start, 0))
            t, prev_text = start + duration(f), text
        music, sfx, cfg = extras(RADIO_DIR / f"S{seg:02d}")
        outs.append(render(plan, music, sfx, cfg, out_dir / f"KMHR_S{seg:02d}.wav"))

    if missing:
        print(f"\n{len(missing)} line(s) not recorded yet, skipped: {', '.join(missing[:12])}"
              f"{' ...' if len(missing) > 12 else ''}")
    if len(outs) > 1:
        concat(outs, out_dir / "KMHR_full_show.wav")
    elif not outs:
        print("Nothing recorded yet. Record lines in the Voice Studio under 'KMHR 5280 AM'.")


def import_takes(folder: Path, replace: bool) -> None:
    """Friends' recordings, named by line number (3.05.m4a, 3_05.wav, KMHR_S03_05.mp3, 3.05b.m4a), go to
    SourceArt/Voice/NPC/<Speaker>/VO_KMHR_S03_05.wav as 48 kHz mono WAV with silence trimmed and level evened,
    which is where the Voice Studio and `studio` mode look. Existing recordings are kept unless --replace."""
    data = load_json(NPC_DIR / "radio_lines.json")
    by_row = {l["row"]: l for l in data["lines"]}
    found: dict[str, Path] = {}
    adlibs, unmatched = [], []
    for f in sorted(folder.rglob("*")):
        if f.suffix.lower() not in AUDIO_EXT:
            continue
        # Strictest form first, so "New Recording 12 3.05" reads as 3.05, not 12.3.
        m = (re.search(r"S(\d{1,2})_(\d{1,2})(?!\d)", f.stem, re.I)
             or re.search(r"(?<![\d.])(\d{1,2})\.(\d{1,2})(?![\d.])", f.stem)
             or re.search(r"(?<!\d)(\d{1,2})[_\-](\d{2})(?!\d)", f.stem))
        row = f"KMHR_S{int(m.group(1)):02d}_{int(m.group(2)):02d}" if m else None
        if "adlib" in f.stem.lower() or "ad-lib" in f.stem.lower():
            adlibs.append(f)
        elif row in by_row:
            # Several takes of one line: the newest file wins.
            if row not in found or f.stat().st_mtime > found[row].stat().st_mtime:
                found[row] = f
        else:
            unmatched.append(f.name)

    done, kept = 0, []
    for row, f in sorted(found.items()):
        dest = NPC_DIR / by_row[row]["speaker"] / f"VO_{row}.wav"
        if dest.exists() and not replace:
            kept.append(row)
            continue
        dest.parent.mkdir(parents=True, exist_ok=True)
        if dest.exists():
            dest.replace(dest.with_suffix(".wav.bak"))
        # Trim silence at both ends (reversing to do the tail), then even out the level so a phone recording
        # sits next to a studio one.
        trim = "silenceremove=start_periods=1:start_threshold=-45dB:start_silence=0.1"
        run([str(FFMPEG), "-y", "-hide_banner", "-i", str(f), "-af",
             f"{trim},areverse,{trim},areverse,loudnorm=I=-20:TP=-2:LRA=11",
             "-ac", "1", "-ar", "48000", "-c:a", "pcm_s16le", str(dest)])
        print(f"  {f.name:<24} -> {by_row[row]['speaker']}/{dest.name}")
        done += 1
    if adlibs:
        dest_dir = RADIO_DIR / "Adlibs"
        dest_dir.mkdir(parents=True, exist_ok=True)
        for f in adlibs:
            (dest_dir / f"{folder.name}_{f.name}").write_bytes(f.read_bytes())
        print(f"  {len(adlibs)} ad-lib(s) copied to {dest_dir} to listen to")
    print(f"\nImported {done} line(s).")
    if kept:
        print(f"Already recorded, kept yours (use --replace to swap): {', '.join(kept)}")
    if unmatched:
        print(f"No line number found in: {', '.join(unmatched)}")


def concat(outs: list[Path], full: Path) -> None:
    """Segments back to back with a short breath between them."""
    inputs, labels = [], []
    for i, o in enumerate(outs):
        inputs += ["-i", str(o)]
        labels.append(f"[{i}:a]apad=pad_dur=0.4[s{i}]")
    graph = ";".join(labels) + ";" + "".join(f"[s{i}]" for i in range(len(outs))) + \
        f"concat=n={len(outs)}:v=0:a=1[out]"
    run([str(FFMPEG), "-y", "-hide_banner", *inputs, "-filter_complex", graph,
         "-map", "[out]", "-ac", "1", "-ar", "48000", "-c:a", "pcm_s16le", str(full)])
    print(f"\nFull show -> {full}  ({duration(full) / 60:.1f} min)")


def selftest() -> None:
    root = Path(tempfile.mkdtemp(prefix="radio_selftest_"))
    seg = root / "01_test"
    seg.mkdir()

    def gen(name: str, src: str, secs: float) -> None:
        run([str(FFMPEG), "-y", "-hide_banner", "-f", "lavfi", "-i", src, "-t", str(secs), str(seg / name)])

    # Stand-in "voices": pulsed tones so you can hear the phone/AM filtering if you play the result.
    gen("1-Scott.wav", "sine=f=220:sample_rate=48000,volume='if(lt(mod(t,1),0.6),0.5,0)':eval=frame", 6)
    gen("2-CJ.wav", "sine=f=330:sample_rate=48000,volume='if(gt(mod(t,1),0.5),0.5,0)':eval=frame", 5)
    gen("3-Zoe.wav", "sine=f=440:sample_rate=48000", 4)
    gen("brad_shout.wav", "anoisesrc=c=pink:a=0.3:r=48000", 2)
    gen("music_theme.wav", "sine=f=110:sample_rate=48000", 1.5)
    gen("sfx_static.wav", "anoisesrc=c=white:a=0.2:r=48000", 1)
    (root / "cast.json").write_text(json.dumps({"Scott": "dale", "CJ": "caller", "Zoe": "announcer"}))
    (seg / "segment.json").write_text(json.dumps({"sfx": {"sfx_static.wav": {"at": "end"}}}))
    seg2 = root / "02_second"
    seg2.mkdir()
    gen("terry.wav", "sine=f=250:sample_rate=48000", 2)
    (seg2 / "terry.wav").write_bytes((seg / "terry.wav").read_bytes())
    (seg / "terry.wav").unlink()

    mix_show(root)
    print(f"\nSelf-test OK. Files are in {root / 'out'}")


def main() -> None:
    if not FFMPEG.exists():
        sys.exit(f"ffmpeg not found at {FFMPEG}")
    if len(sys.argv) >= 2 and sys.argv[1] == "selftest":
        return selftest()
    if len(sys.argv) >= 2 and sys.argv[1] == "studio":
        return mix_studio()
    if len(sys.argv) >= 3 and sys.argv[1] == "import":
        return import_takes(Path(sys.argv[2]).resolve(), "--replace" in sys.argv)
    if len(sys.argv) < 3 or sys.argv[1] not in ("mix", "show"):
        sys.exit(__doc__)
    target = Path(sys.argv[2]).resolve()
    if sys.argv[1] == "show":
        mix_show(target)
    else:
        mix_segment(target, target.parent / "out" / f"{target.name}.wav")


if __name__ == "__main__":
    main()
