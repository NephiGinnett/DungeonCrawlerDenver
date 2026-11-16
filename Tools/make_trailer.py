"""Render an arcade ad: a 45-second camera cut, with a song, as an MP4.

    python Tools/make_trailer.py
    python Tools/make_trailer.py --start 20 --song "Content/Songs/Swept Away.mp3" --out D:/Builds/Fourteener_Trailer.mp4
    python Tools/make_trailer.py --rally
    python Tools/make_trailer.py --encode-only      # re-cut the last frames with new music, title or timing

What it does:
  1. Runs the game in the round with the matching capture command (DCC.Trailer or DCC.RallyTrailer), fixed at
     30 fps of game time (-benchmark -fps=30), which writes every frame as a 1920x1080 PNG and quits.
     Rendering is slower than real time and that is fine: the timing comes from the frame count, not the clock.
  2. ffmpeg (D:/Tools/ffmpeg) turns the frames into video, adds cinema bars, a fade in from black, the title card
     fading in over the last shot, and the song from --start, faded in and out.

Needs the editor closed (it runs the game from the project's editor build).
"""

import argparse
import shutil
import subprocess
import sys
import time
from pathlib import Path

ROOT = Path(__file__).resolve().parent.parent
ENGINE = Path(r"D:\UE\UE_5.8\Engine\Binaries\Win64\UnrealEditor.exe")
FFMPEG = Path(r"D:\Tools\ffmpeg\ffmpeg.exe")
FPS = 30

TITLE_FONT = r"C\:/Windows/Fonts/impact.ttf"
SUB_FONT = r"C\:/Windows/Fonts/segoeuib.ttf"
BAR = 120             # letterbox height in pixels (top and bottom)

# Fourteener is the default. --rally swaps the round, the capture command, the title and the song.
CLIMB = {
    "game": "/Script/DungeonCrawlerDenver.DCCClimbGameMode",
    "command": "DCC.Trailer",
    "frames": Path(r"D:\Builds\TrailerFrames"),
    "length": 45.0,
    "title": "DUNGEON CRAWLER DENVER",
    "subtitle": "FOURTEENER  -  climb together. fall apart.",
    "title_in": 39.0,
    "song": ROOT / "Content" / "Songs" / "Swept Away.mp3",
    "start": 20.0,
    "out": Path(r"D:\Builds\Fourteener_Trailer.mp4"),
    "log": "Trailer.log",
}
RALLY = {
    "game": "/Script/DungeonCrawlerDenver.DCCKartGameMode",
    "command": "DCC.RallyTrailer",
    "frames": Path(r"D:\Builds\RallyTrailerFrames"),
    "length": 52.0,
    "title": "DUNGEON CRAWLER DENVER",
    "subtitle": "REPO RALLY  -  hook it. tow it. drop it.",
    "title_in": 45.0,
    "song": ROOT / "Content" / "Songs" / "Repo Rally.mp3",
    "start": 0.0,
    "out": Path(r"D:\Builds\RepoRally_Trailer.mp4"),
    "log": "RallyTrailer.log",
}
# Dead Air: a fixed mine (-MineSeed) that has a cart run, powder, firedamp and a ghost; the HUD is in the frames
# (extraction bars and the Scroller feed are HUD), so it captures at 1920x1080 directly.
HAUNT = {
    "game": "/Script/DungeonCrawlerDenver.DCCHauntGameMode",
    "command": "DCC.HauntTrailer",
    "frames": Path(r"D:\Builds\HauntTrailerFrames"),
    "length": 64.0,
    "title": "DUNGEON CRAWLER DENVER",
    "subtitle": "DEAD AIR  -  film the ghosts. get back to the cage.",
    "title_in": 59.0,
    "song": ROOT / "Content" / "Songs" / "Platinum Dust Mine.mp3",
    "start": 0.0,
    "out": Path(r"D:\Builds\DeadAir_Trailer.mp4"),
    "log": "HauntTrailer.log",
    "res": (1920, 1080),
    "extra": ["-MineSeed=9001"],
    "bars": False,
}
TITLE_FADE = 2.5


def render(spec):
    frames_dir = spec["frames"]
    if frames_dir.exists():
        shutil.rmtree(frames_dir)
    frames_dir.mkdir(parents=True)
    log = ROOT / "Saved" / "Logs" / spec["log"]
    cmd = [str(ENGINE), str(ROOT / "DungeonCrawlerDenver.uproject"),
           f"/Engine/Maps/Entry?game={spec['game']}",
           "-game", "-windowed", f"-ResX={spec.get('res', (1280, 720))[0]}", f"-ResY={spec.get('res', (1280, 720))[1]}",
           "-benchmark", f"-fps={FPS}", "-deterministic", "-nosound", f"-log={spec['log']}"] + spec.get("extra", []) + [
           f"-ExecCmds=stat none, {spec['command']} capture {frames_dir.as_posix()}"]
    print("rendering frames (slower than real time; a few minutes)...")
    started = time.time()
    proc = subprocess.Popen(cmd)
    try:
        proc.wait(timeout=60 * 60)
    except subprocess.TimeoutExpired:
        proc.kill()
        sys.exit("the game did not finish within an hour; see Saved/Logs/Trailer.log")
    frames = sorted(frames_dir.glob("frame_*.png"))
    print(f"{len(frames)} frames in {time.time() - started:.0f} s (want {int(spec['length'] * FPS)})")
    if len(frames) < spec["length"] * FPS * 0.9:
        sys.exit(f"too few frames; see {log}")


def encode(spec, song, start, out):
    frames = sorted(spec["frames"].glob("frame_*.png"))
    if not frames:
        sys.exit("no frames; run without --encode-only first")
    first = int(frames[0].stem.split("_")[1])
    length = len(frames) / FPS
    title_in = spec["title_in"]
    title_alpha = f"if(lt(t,{title_in}),0,min(1,(t-{title_in})/{TITLE_FADE}))"
    sub_alpha = f"if(lt(t,{title_in + 1.2}),0,min(1,(t-{title_in + 1.2})/{TITLE_FADE}))"
    video = ",".join([
        "scale=1920:1080:flags=lanczos",
        *([f"drawbox=x=0:y=0:w=iw:h={BAR}:color=black:t=fill", f"drawbox=x=0:y=ih-{BAR}:w=iw:h={BAR}:color=black:t=fill"]
          if spec.get("bars", True) else []),
        "fade=t=in:st=0:d=1.2",
        f"drawtext=fontfile='{TITLE_FONT}':text='{spec['title']}':fontsize=110:fontcolor=white:alpha='{title_alpha}'"
        f":x=(w-text_w)/2:y=(h-text_h)/2-60:shadowcolor=black@0.6:shadowx=4:shadowy=4",
        f"drawtext=fontfile='{SUB_FONT}':text='{spec['subtitle']}':fontsize=42:fontcolor=0xFFD27A:alpha='{sub_alpha}'"
        f":x=(w-text_w)/2:y=(h/2)+40:shadowcolor=black@0.6:shadowx=2:shadowy=2",
        f"fade=t=out:st={length - 0.8:.2f}:d=0.8",
    ])
    audio = f"afade=t=in:st=0:d=1.0,afade=t=out:st={max(0.0, length - 3.0):.2f}:d=3.0"
    cmd = [str(FFMPEG), "-y", "-hide_banner", "-loglevel", "error",
           "-framerate", str(FPS), "-start_number", str(first), "-i", str(spec["frames"] / "frame_%05d.png"),
           "-ss", str(start), "-t", f"{length:.2f}", "-i", str(song),
           "-vf", video, "-af", audio,
           "-c:v", "libx264", "-preset", "slow", "-crf", "17", "-pix_fmt", "yuv420p",
           "-c:a", "aac", "-b:a", "192k", "-shortest", "-movflags", "+faststart", str(out)]
    Path(out).parent.mkdir(parents=True, exist_ok=True)
    subprocess.run(cmd, check=True)
    print(f"wrote {out}")


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("--rally", action="store_true", help="Repo Rally ad instead of Fourteener")
    ap.add_argument("--haunt", action="store_true", help="Dead Air ad instead of Fourteener")
    ap.add_argument("--seed", type=int, default=None, help="Dead Air: mine layout seed")
    ap.add_argument("--song", default=None)
    ap.add_argument("--start", type=float, default=None, help="where in the song the ad's audio starts, seconds")
    ap.add_argument("--out", default=None)
    ap.add_argument("--encode-only", action="store_true")
    args = ap.parse_args()
    spec = RALLY if args.rally else HAUNT if args.haunt else CLIMB
    if args.seed is not None:
        spec["extra"] = [f"-MineSeed={args.seed}"]
    song = args.song or spec["song"]
    start = spec["start"] if args.start is None else args.start
    out = args.out or spec["out"]
    if not args.encode_only:
        render(spec)
    encode(spec, song, start, out)


if __name__ == "__main__":
    main()
