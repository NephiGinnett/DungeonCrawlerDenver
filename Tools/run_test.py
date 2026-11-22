"""
Run one arcade game as a test, take screenshots of its window, then close it.

    python Tools/run_test.py <GameMode> "<dev commands>" <first_shot_s> <shots> <gap_s> <name>

    python Tools/run_test.py DCCHauntGameMode "DCC.Belt 3 12, DCC.Click 14" 11 3 2.5 haunt_click
    python Tools/run_test.py DCCLobbyGameMode "DCC.LootOpen 22" 27 24 0.15 loot

Extra command-line flags go in the DCC_TEST_ARGS environment variable (e.g. "-GateSeed=13 -GateTheme=2").

<GameMode> is the class name without the prefix: DCCLobbyGameMode (the Den), DCCClimbGameMode (Fourteener),
DCCHauntGameMode (Dead Air), DCCKartGameMode (Repo Rally), DCCNeonGameMode, DCCGateGameMode (Gate Run),
DCCFrontDoorGameMode (main menu). Dev commands are comma-separated console commands; most DCC.* ones take a
delay in seconds as their last argument (see Arcade/DCCDevCommands.cpp).

Shots start <first_shot_s> seconds after the level is up ("Bringing up level for play" in the log). Output:
Saved/TestShots/<name>/b00.png ... and sheet.png (all shots on one image, three across). Log: Saved/Logs/<name>.log.

Rules this follows (CLAUDE.md, the 2026-11-13 notes):
  * Screenshots are of the Unreal windows only (Tools/editor_ctl.py shot), never the desktop.
  * Only the game this script started is closed, by its own process id. An editor Zoe has open is never touched.
  * The editor must be closed for a C++ build, but not for this: a -game window runs beside it.
  * A -game run on this PC plays as Zoe and writes her real profile (Saved/SaveGames/Profile_Zoe.sav). Say so in
    your report if the test earned or spent anything. Reset only when she asks (DCC.ProfileFix).
"""

import os

import subprocess
import sys
import time
from pathlib import Path

ROOT = Path(__file__).resolve().parent.parent
ENGINE = Path(r"D:/UE/UE_5.8/Engine/Binaries/Win64/UnrealEditor.exe")


def main() -> int:
    if len(sys.argv) < 7:
        print(__doc__)
        return 1
    mode, cmds, first, shots, gap, name = sys.argv[1], sys.argv[2], float(sys.argv[3]), int(sys.argv[4]), float(sys.argv[5]), sys.argv[6]
    out = ROOT / "Saved" / "TestShots" / name
    out.mkdir(parents=True, exist_ok=True)
    for old in out.glob("*.png"):
        old.unlink()
    log = ROOT / "Saved" / "Logs" / f"{name}.log"
    if log.exists():
        log.unlink()

    exec_cmds = "stat none" + (", " + cmds if cmds.strip() else "")
    game = subprocess.Popen([
        str(ENGINE), str(ROOT / "DungeonCrawlerDenver.uproject"),
        f"/Engine/Maps/Entry?game=/Script/DungeonCrawlerDenver.{mode}",
        "-game", "-windowed", "-ResX=1280", "-ResY=720", f"-log={name}.log", f"-ExecCmds={exec_cmds}",
        # Extra flags, e.g. DCC_TEST_ARGS="-GateSeed=13 -GateTheme=2".
        *os.environ.get("DCC_TEST_ARGS", "").split(),
    ], stdout=subprocess.DEVNULL, stderr=subprocess.DEVNULL)
    try:
        started = time.time()
        while time.time() - started < 300:
            if log.exists() and "Bringing up level for play" in log.read_text(encoding="utf-8", errors="ignore"):
                break
            if game.poll() is not None:
                print("The game closed before the level came up. Read", log)
                return 2
            time.sleep(1)
        time.sleep(first)
        for i in range(shots):
            before = set(out.glob("*.png"))
            subprocess.run([sys.executable, str(ROOT / "Tools" / "editor_ctl.py"), "shot", str(out)], capture_output=True, text=True)
            # editor_ctl shoots every Unreal window (an open editor too). Keep the one that looks like the game: the
            # 1280-wide window, else the newest.
            new = sorted(set(out.glob("*.png")) - before, key=lambda p: p.stat().st_mtime)
            pick = None
            try:
                from PIL import Image
                pick = next((p for p in new if 1270 <= Image.open(p).size[0] <= 1320), None)
            except ImportError:
                pass
            pick = pick or (new[-1] if new else None)
            if pick:
                os.replace(pick, out / f"b{i:02d}.png")
            for extra in new:
                if extra != pick and extra.exists():
                    extra.unlink()
            time.sleep(gap)
    finally:
        # Only the game this script started.
        subprocess.run(["taskkill", "/PID", str(game.pid), "/F"], capture_output=True)

    try:
        from PIL import Image
    except ImportError:
        print(f"{shots} shots in {out} (install Pillow for sheet.png)")
        return 0
    files = sorted(out.glob("b*.png"))
    if not files:
        print("No shots were taken. Read", log)
        return 3
    tiles = [Image.open(f).convert("RGB").resize((640, 375)) for f in files]
    cols = 3
    rows = (len(tiles) + cols - 1) // cols
    sheet = Image.new("RGB", (640 * cols, 375 * rows))
    for i, tile in enumerate(tiles):
        sheet.paste(tile, ((i % cols) * 640, (i // cols) * 375))
    sheet.save(out / "sheet.png")
    print(f"{len(files)} shots: {out / 'sheet.png'}   log: {log}")
    return 0


if __name__ == "__main__":
    sys.exit(main())
