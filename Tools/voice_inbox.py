"""
Voice Drop inbox (2026-11-22): the local half of VoiceDrop/ (the upload site friends record lines on).

    python Tools/voice_inbox.py catalog                 write VoiceDrop/catalog.json from SourceArt/Voice/NPC/*_lines.json
    python Tools/voice_inbox.py catalog --push          ...and send it to the live site (no redeploy needed)
    python Tools/voice_inbox.py pull                    bring every APPROVED take into the game, credit it, mark it shipped
    python Tools/voice_inbox.py pull --dry-run          show what pull would do, change nothing
    python Tools/voice_inbox.py credits                 list the credits the game will pay

The site and the password come from the environment, or from Saved/VoiceDrop.json ({"url": ..., "token": ...}):
    VOICE_DROP_URL    e.g. https://voice-drop.up.railway.app
    VOICE_DROP_TOKEN  the ADMIN_TOKEN set on the site

What pull does with each approved take:
  * converts it to SourceArt/Voice/NPC/<Speaker>/VO_<row>.wav (48 kHz mono, silence trimmed, level evened: the same
    chain as `radio_mix.py import`), keeping any older recording as .wav.bak;
  * adds a credit to Content/DCC/VoiceCredits/credits.json, which ships with the game. The next time that player
    name plays (on the host's machine), the game pays the tickets once and tells them which line shipped;
  * marks the take "shipped" on the site;
  * once every line of a character has shipped for the same player, adds a one-time full-set bonus credit
    (SET_BONUS on the site, 300 tickets unless changed there).
  * OPEN MIC WEEK (2026-11-23): counts each player's approved and shipped takes per week (Monday to Sunday, UTC) and
    adds a credit for every milestone reached: 1 line a Bronze box, 3 a Silver box and 50 tickets, 6 a Gold box and
    the OPEN MIC nameplate. Each milestone pays once per week.
  * Auditions: when Zoe has chosen a voice for an auditioning character in /admin (Hiss Cranson), that player gets the
    audition's prize once (the Hot Mic). A Hot Mic clip take is converted to SourceArt/Voice/NPC/_Clips/<player>.wav,
    the first 4 seconds, and plays when their amplified spell goes off.
"""

import argparse
import json
import os
import subprocess
import sys
import urllib.request
from pathlib import Path

REPO = Path(__file__).resolve().parent.parent
NPC_DIR = REPO / "SourceArt" / "Voice" / "NPC"
CAST = REPO / "SourceArt" / "Voice" / "Radio" / "cast.json"
CATALOG = REPO / "VoiceDrop" / "catalog.json"
CREDITS = REPO / "Content" / "DCC" / "VoiceCredits" / "credits.json"
INBOX = REPO / "Saved" / "VoiceInbox"
SETTINGS = REPO / "Saved" / "VoiceDrop.json"
FFMPEG = Path(r"D:\Tools\ffmpeg\ffmpeg.exe")


def load_json(path: Path, default):
    return json.loads(path.read_text(encoding="utf-8")) if path.exists() else default


def site():
    saved = load_json(SETTINGS, {})
    url = (os.environ.get("VOICE_DROP_URL") or saved.get("url") or "").rstrip("/")
    token = os.environ.get("VOICE_DROP_TOKEN") or saved.get("token") or ""
    if not url or not token:
        sys.exit("Set VOICE_DROP_URL and VOICE_DROP_TOKEN (or Saved/VoiceDrop.json with url and token).")
    return url, token


def call(method: str, path: str, body=None, raw=False):
    url, token = site()
    data = json.dumps(body).encode() if body is not None else None
    req = urllib.request.Request(url + path, data=data, method=method,
                                 headers={"x-admin-token": token, "Content-Type": "application/json",
                                          "User-Agent": "voice-inbox"})
    with urllib.request.urlopen(req, timeout=60) as r:
        payload = r.read()
    return payload if raw else json.loads(payload)


def build_catalog() -> dict:
    """Every line in every *_lines.json. A character cast to a reader in cast.json is left off the site."""
    cast = load_json(CAST, {})
    taken = {cid for reader, ids in cast.items() if not reader.startswith("_") for cid in ids}
    ai = load_json(REPO / "SourceArt" / "Voice" / "Radio" / "ai_takes.json", {})
    speakers: dict[str, dict] = {}
    for f in sorted(NPC_DIR.glob("*_lines.json")):
        data = load_json(f, {})
        names = {s["id"]: s.get("name", s["id"]) for s in data.get("speakers", [])}
        auditions = {s["id"]: s["audition"] for s in data.get("speakers", []) if s.get("audition")}
        for line in data.get("lines", []):
            sid = line["speaker"]
            if sid in taken:
                continue
            s = speakers.setdefault(sid, {"id": sid, "name": names.get(sid, sid), "open": True, "source": f.stem, "lines": []})
            if sid in auditions:
                s["audition"] = auditions[sid]
            s["lines"].append({
                "row": line["row"],
                "text": line["text"],
                # No real recording yet (none, or only a machine take): these show first on the site.
                "wanted": not (NPC_DIR / sid / f"VO_{line['row']}.wav").exists() or line["row"] in ai,
            })
    return {
        "title": "Voice Drop",
        "intro": "Lend a character your voice. Record right here, or upload a take from your phone. "
                 "When a line ships in the game, the name you play under gets tickets.",
        "speakers": sorted(speakers.values(), key=lambda s: (-sum(l["wanted"] for l in s["lines"]), s["name"])),
    }


def cmd_catalog(args):
    cat = build_catalog()
    CATALOG.write_text(json.dumps(cat, indent=2, ensure_ascii=False), encoding="utf-8")
    lines = sum(len(s["lines"]) for s in cat["speakers"])
    print(f"Wrote {CATALOG.relative_to(REPO)}: {len(cat['speakers'])} characters, {lines} lines.")
    if args.push:
        print("Live site:", call("POST", "/api/admin/catalog", cat))


def convert(src: Path, dest: Path, seconds: float = 0.0):
    dest.parent.mkdir(parents=True, exist_ok=True)
    if dest.exists():
        dest.replace(dest.with_suffix(".wav.bak"))
    trim = "silenceremove=start_periods=1:start_threshold=-45dB:start_silence=0.1"
    cut = ["-t", f"{seconds:.2f}"] if seconds > 0 else []
    r = subprocess.run([str(FFMPEG), "-y", "-hide_banner", "-i", str(src), "-af",
                        f"{trim},areverse,{trim},areverse,loudnorm=I=-20:TP=-2:LRA=11", *cut,
                        "-ac", "1", "-ar", "48000", "-c:a", "pcm_s16le", str(dest)],
                       capture_output=True, text=True, encoding="utf-8", errors="replace")
    if r.returncode != 0:
        raise RuntimeError(r.stderr[-1500:])


def cmd_pull(args):
    approved = call("GET", "/api/admin/submissions?status=approved")
    if not approved:
        print("No approved takes waiting. Approve some in /admin first.")
        return
    credits = load_json(CREDITS, {"credits": []})
    have = {c["id"] for c in credits["credits"]}
    INBOX.mkdir(parents=True, exist_ok=True)
    for s in approved:
        player = (s.get("gameName") or s["name"]).strip()
        if s.get("kind") == "clip":
            dest = NPC_DIR / "_Clips" / f"{player}.wav"
            print(f"Hot Mic clip by {s['name']} -> {dest.relative_to(REPO)}")
            if args.dry_run:
                continue
            raw = INBOX / f"{s['id']}{Path(s['file']).suffix or '.bin'}"
            raw.write_bytes(call("GET", f"/api/admin/audio/{s['id']}", raw=True))
            try:
                convert(raw, dest, seconds=4.0)
            except RuntimeError as e:
                print(f"  ffmpeg could not read it, left as approved:\n{e}")
                continue
            call("POST", f"/api/admin/submissions/{s['id']}", {"status": "shipped"})
            print("  shipped")
            continue
        dest = NPC_DIR / s["speaker"] / f"VO_{s['row']}.wav"
        print(f"{s['speakerName']} {s['row']} by {s['name']} -> {dest.relative_to(REPO)}  ({s['reward']} tickets to {player})")
        if args.dry_run:
            continue
        ext = Path(s["file"]).suffix or ".bin"
        raw = INBOX / f"{s['id']}{ext}"
        raw.write_bytes(call("GET", f"/api/admin/audio/{s['id']}", raw=True))
        try:
            convert(raw, dest)
        except RuntimeError as e:
            print(f"  ffmpeg could not read it, left as approved:\n{e}")
            continue
        if s["id"] not in have:
            credits["credits"].append({
                "id": s["id"], "player": player, "tickets": int(s["reward"]),
                "speaker": s["speakerName"], "speakerId": s["speaker"], "row": s["row"], "text": s["text"],
            })
            have.add(s["id"])
        CREDITS.parent.mkdir(parents=True, exist_ok=True)
        CREDITS.write_text(json.dumps(credits, indent=2, ensure_ascii=False), encoding="utf-8")
        call("POST", f"/api/admin/submissions/{s['id']}", {"status": "shipped"})
        print("  shipped")
    if not args.dry_run:
        add_set_bonuses(credits)
        add_event_rewards(credits)
        add_audition_prizes(credits)
        print(f"\nCredits: {CREDITS.relative_to(REPO)} ({len(credits['credits'])} in all). They pay out in the next build or run.")


def add_set_bonuses(credits):
    """Pay a one-time bonus to a player once every line of a character has shipped in their name."""
    public = call("GET", "/api/catalog")
    bonus = int(public.get("setBonus") or 0)
    if bonus <= 0:
        return
    speakers = call("GET", "/api/admin/catalog")["speakers"]
    by_name = {sp["name"]: sp["id"] for sp in speakers}
    have = {c["id"] for c in credits["credits"]}
    shipped = {}
    for c in credits["credits"]:
        if c["id"].startswith("set-"):
            continue
        sid = c.get("speakerId") or by_name.get(c.get("speaker"))
        if sid:
            shipped.setdefault((c["player"].lower(), sid), (c["player"], set()))[1].add(c["row"])
    for sp in speakers:
        rows = {l["row"] for l in sp["lines"]}
        if len(rows) < 2:
            continue
        for (key, sid), (player, done) in shipped.items():
            bid = f"set-{sid}-{key}"
            if sid == sp["id"] and rows <= done and bid not in have:
                credits["credits"].append({
                    "id": bid, "player": player, "tickets": bonus, "speaker": sp["name"], "speakerId": sid,
                    "row": "ALL", "text": f"every one of {sp['name']}'s lines. Full-set bonus!",
                })
                have.add(bid)
                print(f"  full set: {player} voiced all {len(rows)} of {sp['name']}'s lines, +{bonus} tickets")
    CREDITS.write_text(json.dumps(credits, indent=2, ensure_ascii=False), encoding="utf-8")


def add_event_rewards(credits):
    """OPEN MIC WEEK: a credit for every milestone each player reached, once per week."""
    from datetime import datetime, timedelta, timezone
    event = call("GET", "/api/admin/event")
    milestones = event.get("milestones", [])
    every = call("GET", "/api/admin/submissions?status=all")
    counts = {}
    for t in every:
        if t.get("kind") == "clip" or t["status"] not in ("approved", "shipped"):
            continue
        when = datetime.fromisoformat((t.get("approvedAt") or t["createdAt"]).replace("Z", "+00:00"))
        monday = (when - timedelta(days=when.weekday())).date().isoformat()
        player = (t.get("gameName") or t["name"]).strip()
        key = (player.lower(), monday)
        counts[key] = (player, counts.get(key, (player, 0))[1] + 1)
    # This week's line counts, for the game's OPEN MIC WEEK page (UI/DCCEventPanelWidget.cpp).
    today = datetime.now(timezone.utc)
    this_monday = (today - timedelta(days=today.weekday())).date().isoformat()
    credits["week"] = {"monday": this_monday,
                       "counts": {k: n for (k, m), (_, n) in counts.items() if m == this_monday}}
    have = {c["id"] for c in credits["credits"]}
    for (key, monday), (player, n) in sorted(counts.items()):
        for m in milestones:
            cid = f"evt-{monday}-{key}-{m['lines']}"
            if n >= m["lines"] and cid not in have:
                credit = {"id": cid, "player": player, "tickets": int(m.get("tickets", 0)), "speaker": "OPEN MIC WEEK",
                          "row": "EVENT", "text": f"{m['lines']} line(s) the week of {monday}: {m['reward']}"}
                if m.get("box"):
                    credit["box"] = m["box"]
                if m.get("title"):
                    credit["title"] = m["title"]
                credits["credits"].append(credit)
                have.add(cid)
                print(f"  OPEN MIC WEEK: {player} reached {m['lines']} ({m['reward']})")
    CREDITS.write_text(json.dumps(credits, indent=2, ensure_ascii=False), encoding="utf-8")


def add_audition_prizes(credits):
    """The chosen voice of an auditioning character gets its prize once (Hiss Cranson: the Hot Mic)."""
    auditions = call("GET", "/api/admin/auditions")
    speakers = {sp["id"]: sp for sp in call("GET", "/api/admin/catalog")["speakers"]}
    have = {c["id"] for c in credits["credits"]}
    for sid, a in auditions.items():
        chosen = (a or {}).get("chosen", "").strip()
        sp = speakers.get(sid, {})
        prize = (sp.get("audition") or {}).get("item", "")
        cid = f"cast-{sid}-{chosen.lower()}"
        if chosen and prize and cid not in have:
            credits["credits"].append({"id": cid, "player": chosen, "tickets": 0, "item": prize, "speaker": sp.get("name", sid),
                                       "row": "CAST", "text": f"chosen as the voice of {sp.get('name', sid)}"})
            have.add(cid)
            print(f"  {chosen} is the voice of {sp.get('name', sid)}: {prize}")
    CREDITS.write_text(json.dumps(credits, indent=2, ensure_ascii=False), encoding="utf-8")


def cmd_credits(args):
    credits = load_json(CREDITS, {"credits": []})["credits"]
    for c in credits:
        print(f"{c['player']:<16} {c['tickets']:>4} tickets  {c['speaker']} {c['row']}")
    print(f"{len(credits)} credit(s).")


def main():
    p = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    sub = p.add_subparsers(dest="cmd", required=True)
    c = sub.add_parser("catalog")
    c.add_argument("--push", action="store_true")
    c.set_defaults(fn=cmd_catalog)
    pl = sub.add_parser("pull")
    pl.add_argument("--dry-run", action="store_true")
    pl.set_defaults(fn=cmd_pull)
    sub.add_parser("credits").set_defaults(fn=cmd_credits)
    args = p.parse_args()
    args.fn(args)


if __name__ == "__main__":
    main()
