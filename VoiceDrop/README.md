# Voice Drop

A link you can send anyone (players or not) to record lines for Dungeon Crawler Denver. They pick a character,
record each line in the browser (or upload a file from their phone), and send it. You get a ping. You listen and
approve in `/admin`. `Tools/voice_inbox.py pull` brings approved takes into the game and writes the credits; the
game pays the tickets the first time that player name plays.

```
friend's browser ──> Voice Drop (Railway) ──> Discord / ntfy ping to you
                          │
          you: /admin, listen, Approve (set tickets, fix the game name)
                          │
   python Tools/voice_inbox.py pull ──> SourceArt/Voice/NPC/<Speaker>/VO_<row>.wav
                                    ──> Content/DCC/VoiceCredits/credits.json  (ships with the game)
                                    ──> take marked "shipped" on the site
                          │
          the game: that player name plays ──> "+25 tickets: your voice line shipped (...)", once
```

## Put it online (Railway)

1. Push this repo to GitHub (it is already there if you use the usual remote).
2. On railway.app: **New Project → Deploy from GitHub repo**, pick the repo, then in the service's
   **Settings → Root Directory** type `VoiceDrop`. Railway sees `package.json` and runs `npm start`.
3. **Add a volume** to the service (right-click the service → Attach volume), mount path `/data`.
   Without it, every redeploy loses the takes.
4. **Variables** (service → Variables):
   - `ADMIN_TOKEN`: a long password only you know. It opens `/admin`.
   - `DATA_DIR` = `/data`
   - `PUBLIC_URL`: the address Railway gives you (Settings → Networking → Generate Domain), e.g. `https://voice-drop-production.up.railway.app`
   - `DISCORD_WEBHOOK_URL` (optional): Discord channel → Edit channel → Integrations → Webhooks → New webhook → Copy URL.
   - `NTFY_TOPIC` (optional): any hard-to-guess word. Install the ntfy app on your phone and subscribe to that topic
     for a push on every upload. Anyone who knows the topic can read the pings, so make it unguessable.
   - `DEFAULT_REWARD` (optional, default 25): tickets per shipped line.
5. Send people `PUBLIC_URL`. Open `PUBLIC_URL/admin` yourself.

Any host that runs Node 18+ with a persistent disk works the same way (Render, Fly.io, a VPS). Set the same variables.

## Your side, on this PC

Once, save the site and password so the tool can reach it (this file is in `Saved/`, which git ignores):

```
Saved/VoiceDrop.json   {"url": "https://voice-drop-production.up.railway.app", "token": "your ADMIN_TOKEN"}
```

Then:

| Command | What it does |
|---|---|
| `python Tools/voice_inbox.py catalog` | Rebuild `VoiceDrop/catalog.json` (the characters and lines on the site) from `SourceArt/Voice/NPC/*_lines.json`. Characters cast to a reader in `SourceArt/Voice/Radio/cast.json` are left off. |
| `python Tools/voice_inbox.py catalog --push` | ...and send it to the live site, no redeploy needed. |
| `python Tools/voice_inbox.py pull --dry-run` | Show what would come in. |
| `python Tools/voice_inbox.py pull` | Bring every **approved** take into the game, write the credit, mark it shipped. |
| `python Tools/voice_inbox.py credits` | List the credits the game will pay. |

Credits pay on the host's machine, to the player whose in-game name matches the credit (not case sensitive), once.
If someone typed a different name than they play under, fix "Game name" in `/admin` before you pull.

## Run it locally

```
cd VoiceDrop
npm install
set ADMIN_TOKEN=test
npm start          # http://localhost:8787 and http://localhost:8787/admin
```
