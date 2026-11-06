# Crawler Companion

Each player's own window into Dungeon Crawler Denver: log in as your crawler, keep your DCC character sheet,
and watch your model. You can also let your crawler loose on your desktop.

**Easiest:** double-click **Crawler Companion** or **System Voice Studio** in this folder. To get desktop icons,
double-click **Create Desktop Shortcuts** once (run it again if the folder moves). The first launch installs
what it needs, and if Node.js is missing it opens the download page and says what to click. All of it goes
through `launch.bat`.

### Sharing with friends: the installer
`npm run dist` builds **`release\Crawler Companion Setup <version>.exe`** (about 240 MB, because every crawler's
model is inside). Send friends that one file. They double-click it and it installs for them alone (no admin
password), puts **Crawler Companion** on their desktop, and opens. It's unsigned, so the first time Windows
says *"Windows protected your PC"*: click **More info**, then **Run anyway**.

In the installed app, sheets save to `%APPDATA%\dcc-crawler-companion\sheets\`, and problems are logged to
`companion.log` in the same folder, which is the file to ask for if something breaks. The Voice Studio is left out
of the installer on purpose.

Two packaging gotchas, both handled now:
- electron-builder strips `examples` folders out of `node_modules`, and that's where three.js keeps its model
  loader. The loader is vendored in `src/vendor/three-addons/` so the installer and dev mode load the same file.
- Rebuilding while a test copy of the app is still running fails with "Access is denied" on the exe. Close it
  first (`taskkill /F /IM "Crawler Companion.exe"`).

By hand:
```
npm install   # once
npm start     # or: npm run studio
```

## Files

| Path | What |
|---|---|
| `main.js` | Electron main process: windows, login, sheet save/load, moving the buddy window |
| `preload.js` | The only bridge between the pages and the disk (`window.companion`) |
| `index.html`, `src/app.js` | Login screen and the character sheet |
| `src/ruleset.js` | DCC arithmetic (banded modifiers, Mana = INT score, d20 + rank + mod). **Mirrors `Source/.../Stats/DCCRuleset.cpp`; change both together.** |
| `src/sheet-template.js` | A new crawler's sheet, including their personal item |
| `src/viewer.js` | three.js model viewer and cross-crawler animation retargeting |
| `buddy.html`, `src/buddy.js` | The desktop buddy |
| `data/roster.json` | Who can log in, which model each one uses, the shared animation library, and the action list |
| `data/sheets/<id>.json` | Saved sheets. Plain JSON, so the DM can read and diff them |

## Adding a crawler or an action
- **Crawler:** add an entry to `data/roster.json` with an `id` that matches their in-game `CrawlerId`
  (and their bedroom PlayerStart tag). Any Meshy `.glb` works. For an FBX, convert it with
  `Tools/fbx_to_glb.py`.
- **Action:** add `{ id, label, clip, loop }` to `actions`. `clip` is an animation name from any `.glb` in
  `animationLibrary`. Every crawler can play every clip; borrowed clips are retargeted rotation-only against
  the skin's bind pose, so nobody's proportions change. See the note at the top of `src/viewer.js`.

## Self-test
`DCC_SNAPSHOT=Marcel npm start` logs in as Marcel, waits, writes `snapshot.png` and quits.
`DCC_SNAPSHOT_ACTION="cast"` also presses an action button first. This is how the app is checked without
anyone at the keyboard.

## The Table: playing together

On the DM's PC, double-click **Start The Table** (or run `npm run table`). A window opens listing the address
friends should use (for example `10.0.0.140:8770`), plus the DM key. Leave it open while you play. The first
time, Windows asks whether Node.js may use the network: allow **Private networks**.

Each player clicks **Table** in the Companion's top bar, types the address and presses **Connect**. Then:
- the **Party** panel shows everyone: online dot, level, health and mana bars
- every d20 roll appears in everyone's **Table log**, and pops up on their screen
- your sheet syncs to the Table, and the newest edit wins, so an old offline copy can't overwrite newer work
- as Zoe, entering the DM key gives the DM view, which can edit anyone's sheet
- the game can push real audience chatter (`{type:'chatter'}` with the `gameKey` from
  `server/table-config.json`); the first live line switches off the locally rolled feed

Same house or same Wi-Fi works as-is. Friends elsewhere need a private network such as **Tailscale** (free); use
the Tailscale address instead. Don't port-forward this to the open internet: it's built to trust five friends.
Sheets on the Table are saved in `server/table-data/`.

## Where this is going
See `Docs/ROADMAP.md` §5. Next is shared sheets through a small WebSocket relay, then a top-down map, then
live tokens pushed from the game.

## System Voice Studio (`npm run studio`)

Zoe's booth for voicing the System AI herself instead of using TTS.

- **Live**: your mic goes through the effects in real time. Move the sliders and hear the result; Glitch fires
  at once. Wear headphones.
- **Record** (R) a line from the teleprompter (↑/↓ change lines), then **Play** (Space) to hear exactly what
  will be exported: effects, boot-up intro, shutdown outro and the glitch track. **Raw** plays it untouched.
- **Glitch** (G): live, you hear it immediately. While recording or playing, it's marked on the take at that
  moment. Double-click the waveform to add one; click a mark to remove it.
- **Export line** (E) writes `SourceArt/Voice/SystemAI/zoe/VO_<Row>.wav`, the names the game's import expects.
  Raw takes are kept in `zoe/raw/`, so **Export all recorded** can re-render every line with new settings.
- Presets: The House, System AI (bright), Deep Ominous, Broken Broadcast, Full Robot, Clean, plus your own.

The effects are in `src/studio/`: `fx-worklet.js` (pitch, sub octave, ring mod, crush, glitches), `chain.js`
(the rest, and the offline render), and `sounds.js` (intro and outro, synthesised). The script comes from
`Tools/gen_onboarding_content.py --studio-lines`.
Self-test: `DCC_STUDIO_TEST="<in.wav>;<preset>;<out.wav>" npx electron .`
