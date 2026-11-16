"""
Check on, screenshot, save and close the Unreal editor (and test game windows) for this project.

    python Tools/editor_ctl.py status            # what is running, is the MCP link up, is PIE running
    python Tools/editor_ctl.py shot [out_dir]    # PNG of each Unreal window ONLY (never the rest of the desktop)
    python Tools/editor_ctl.py save              # save every unsaved asset, through the editor's MCP link
    python Tools/editor_ctl.py close             # stop PIE, save everything, then close the editor normally
    python Tools/editor_ctl.py close --nosave    # close normally; the editor asks about unsaved work itself
    python Tools/editor_ctl.py kill              # force-kill every UnrealEditor.exe. Last resort, loses unsaved work

WHY
---
Building C++ needs the editor closed (CLAUDE.md, "When Zoe has to close the editor"). Zoe often is not at the
keyboard to close it, and grabbing the whole screen to look at it captured her other windows (2026-11-13). So:
  * `shot` captures each Unreal window by its handle (PrintWindow), so it works even when the window is
    covered and never records anything else on screen.
  * `close` closes the way clicking X does. If anything is still unsaved, the editor's own "Save changes?"
    dialog appears and the editor stays open; this script reports that rather than forcing it.
  * `kill` exists for a hung editor and is never used without being asked.
"""

import ctypes
import ctypes.wintypes as wt
import json
import os
import subprocess
import sys
import time
import urllib.request
from pathlib import Path

ROOT = Path(__file__).resolve().parent.parent
sys.path.insert(0, str(Path(__file__).resolve().parent))

user32 = ctypes.windll.user32
gdi32 = ctypes.windll.gdi32
kernel32 = ctypes.windll.kernel32

# Real pixel sizes, not the 96-dpi virtualised ones.
try:
    user32.SetProcessDPIAware()
except Exception:
    pass

WM_CLOSE = 0x0010
PW_RENDERFULLCONTENT = 0x00000002


# --- processes and windows ------------------------------------------------------------------------------------

def unreal_processes():
    """[(pid, mem_kb)] for every UnrealEditor.exe (the editor, and -game test windows)."""
    out = subprocess.run(["tasklist", "/FI", "IMAGENAME eq UnrealEditor.exe", "/FO", "CSV", "/NH"],
                         capture_output=True, text=True).stdout
    procs = []
    for line in out.splitlines():
        parts = [p.strip('"') for p in line.split('","')]
        if len(parts) >= 5 and parts[0].lower().startswith("unrealeditor"):
            procs.append((int(parts[1]), parts[4].strip('"')))
    return procs


def windows_of(pid):
    """Visible top-level windows of a process: [(hwnd, title)]."""
    found = []

    @ctypes.WINFUNCTYPE(wt.BOOL, wt.HWND, wt.LPARAM)
    def callback(hwnd, _):
        owner = wt.DWORD()
        user32.GetWindowThreadProcessId(hwnd, ctypes.byref(owner))
        if owner.value == pid and user32.IsWindowVisible(hwnd):
            n = user32.GetWindowTextLengthW(hwnd)
            buf = ctypes.create_unicode_buffer(n + 1)
            user32.GetWindowTextW(hwnd, buf, n + 1)
            if buf.value:
                found.append((hwnd, buf.value))
        return True

    user32.EnumWindows(callback, 0)
    return found


def is_editor_title(title):
    return "Unreal Editor" in title


# --- MCP --------------------------------------------------------------------------------------------------------

def mcp_up():
    try:
        urllib.request.urlopen(urllib.request.Request("http://127.0.0.1:8000/mcp", method="GET"), timeout=2)
        return True
    except urllib.error.HTTPError:
        return True  # it answered, even if not to a bare GET
    except Exception:
        return False


def mcp(toolset, tool, args):
    from mcp_call import McpSession, unwrap
    session = McpSession()
    result = unwrap(session.call_tool(toolset, tool, args))
    if isinstance(result, str):
        try:
            result = json.loads(result)
        except ValueError:
            raise RuntimeError(result)
    if isinstance(result, dict) and ("code" in result and "message" in result):
        raise RuntimeError(result["message"])
    return result if isinstance(result, dict) else {"returnValue": result}


# --- screenshots ------------------------------------------------------------------------------------------------

class BITMAPINFOHEADER(ctypes.Structure):
    _fields_ = [("biSize", wt.DWORD), ("biWidth", wt.LONG), ("biHeight", wt.LONG), ("biPlanes", wt.WORD),
                ("biBitCount", wt.WORD), ("biCompression", wt.DWORD), ("biSizeImage", wt.DWORD),
                ("biXPelsPerMeter", wt.LONG), ("biYPelsPerMeter", wt.LONG), ("biClrUsed", wt.DWORD),
                ("biClrImportant", wt.DWORD)]


def capture_window(hwnd, path):
    """Renders just this window into a PNG, even if other windows cover it."""
    from PIL import Image
    rect = wt.RECT()
    user32.GetWindowRect(hwnd, ctypes.byref(rect))
    w, h = rect.right - rect.left, rect.bottom - rect.top
    if w <= 0 or h <= 0:
        return False
    hdc = user32.GetWindowDC(hwnd)
    mem = gdi32.CreateCompatibleDC(hdc)
    bmp = gdi32.CreateCompatibleBitmap(hdc, w, h)
    gdi32.SelectObject(mem, bmp)
    ok = user32.PrintWindow(hwnd, mem, PW_RENDERFULLCONTENT)
    header = BITMAPINFOHEADER(ctypes.sizeof(BITMAPINFOHEADER), w, -h, 1, 32, 0, 0, 0, 0, 0, 0)
    buf = ctypes.create_string_buffer(w * h * 4)
    gdi32.GetDIBits(mem, bmp, 0, h, buf, ctypes.byref(header), 0)
    gdi32.DeleteObject(bmp)
    gdi32.DeleteDC(mem)
    user32.ReleaseDC(hwnd, hdc)
    if not ok:
        return False
    Image.frombuffer("RGBA", (w, h), buf, "raw", "BGRA", 0, 1).convert("RGB").save(path)
    return True


# --- commands ---------------------------------------------------------------------------------------------------

def cmd_status():
    procs = unreal_processes()
    if not procs:
        print("No Unreal process is running. Safe to build.")
    for pid, mem in procs:
        titles = [t for _, t in windows_of(pid)]
        kind = "EDITOR" if any(is_editor_title(t) for t in titles) else "game/other"
        print(f"pid {pid}  {kind:10}  {mem:>12}  windows: {titles or ['(none visible)']}")
    up = mcp_up()
    print(f"MCP link (port 8000): {'up' if up else 'down'}")
    if up:
        try:
            print(f"PIE running: {mcp('EditorAppToolset', 'IsPIERunning', {}).get('returnValue')}")
        except Exception as e:
            print(f"PIE running: unknown ({e})")
    dlls = [p.name for p in (ROOT / "Binaries" / "Win64").glob("UnrealEditor-DungeonCrawlerDenver-*.dll")]
    if dlls:
        print(f"WARNING: numbered hot-reload DLLs present: {dlls}")


def cmd_shot(out_dir=None):
    out = Path(out_dir) if out_dir else ROOT / "Saved" / "EditorShots"
    out.mkdir(parents=True, exist_ok=True)
    stamp = time.strftime("%Y%m%d-%H%M%S")
    n = 0
    for pid, _ in unreal_processes():
        for i, (hwnd, title) in enumerate(windows_of(pid)):
            path = out / f"{stamp}_{pid}_{i}.png"
            if user32.IsIconic(hwnd):
                # Minimised: Windows has nothing but a title bar to give. Ask the editor to
                # render itself instead (it can, minimised or covered). Game windows can't.
                if is_editor_title(title) and mcp_up():
                    import base64
                    try:
                        image = mcp("EditorAppToolset", "CaptureEditorImage", {}).get("returnValue", {})
                    except RuntimeError:
                        image = {}
                    if image.get("data"):
                        path.write_bytes(base64.b64decode(image["data"]))
                        print(f"{path}  <- {title} (minimised; rendered by the editor)")
                        n += 1
                        continue
                print(f"(skipped '{title}': minimised, so there is nothing to see. Not restoring it on Zoe's screen.)")
                continue
            if capture_window(hwnd, path):
                print(f"{path}  <- {title}")
                n += 1
    if not n:
        print("Nothing captured (no visible Unreal windows).")


def cmd_save():
    if not mcp_up():
        print("MCP link is down; cannot save from here. Use File > Save All in the editor.")
        return False
    mcp("SceneTools", "save_assets", {"asset_paths": []})
    print("Saved all unsaved assets.")
    return True


def cmd_close(save=True, wait=90):
    editors = [(pid, hwnd, t) for pid, _ in unreal_processes() for hwnd, t in windows_of(pid) if is_editor_title(t)]
    if not editors:
        print("The editor is not running.")
        return True
    if mcp_up():
        try:
            if mcp("EditorAppToolset", "IsPIERunning", {}).get("returnValue"):
                mcp("EditorAppToolset", "StopPIE", {})
                print("Stopped PIE.")
        except Exception as e:
            print(f"Could not check PIE: {e}")
        if save:
            try:
                cmd_save()
            except Exception as e:
                print(f"Save failed ({e}); closing anyway, so the editor's own save prompt will appear.")
    for pid, hwnd, title in editors:
        user32.PostMessageW(hwnd, WM_CLOSE, 0, 0)
        print(f"Asked '{title}' (pid {pid}) to close.")
    pids = {pid for pid, _, _ in editors}
    deadline = time.time() + wait
    while time.time() < deadline:
        alive = {pid for pid, _ in unreal_processes()} & pids
        if not alive:
            print("Editor closed.")
            return True
        time.sleep(2)
    print("Still open after waiting. It is probably showing a 'Save changes?' or other dialog for Zoe to answer.")
    return False


def cmd_kill():
    for pid, _ in unreal_processes():
        subprocess.run(["taskkill", "/PID", str(pid), "/F"], capture_output=True)
        print(f"Killed pid {pid}.")


if __name__ == "__main__":
    args = sys.argv[1:] or ["status"]
    command = args[0]
    if command == "status":
        cmd_status()
    elif command == "shot":
        cmd_shot(args[1] if len(args) > 1 else None)
    elif command == "save":
        cmd_save()
    elif command == "close":
        sys.exit(0 if cmd_close(save="--nosave" not in args) else 1)
    elif command == "kill":
        cmd_kill()
    else:
        print(__doc__)
        sys.exit(2)
