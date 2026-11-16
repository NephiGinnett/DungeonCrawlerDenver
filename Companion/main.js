// Dungeon Crawler Denver - Crawler Companion
//
// Two windows:
//   - the sheet window: log in as your crawler, read and edit your DCC character sheet, watch your model
//   - the buddy: a small transparent always-on-top window where your crawler hangs out on your desktop,
//     wanders, works out, practices, and occasionally casts something at nothing in particular
//
// Everything is local for now. Sheets are plain JSON under data/sheets/ so the DM can read, diff and commit
// them. The live table (a Foundry/Roll20-style top-down map that follows the game) plugs in through
// src/sync.js later; see Companion/README.md for the plan.

const { app, BrowserWindow, ipcMain, screen } = require('electron');
const path = require('path');
const fs = require('fs');
const { pathToFileURL } = require('url');

// Two layouts. From the repo, models and the chatter table are read from the project itself (../SourceArt,
// ../Docs). In the installed app (npm run dist) the same files are bundled under resources/ at the same
// relative paths, so roster.json works unchanged in both. Sheets can't be written into an installed app's own
// folder, so there they go to the user's AppData instead.
const REPO_ROOT = app.isPackaged ? process.resourcesPath : path.resolve(__dirname, '..');
const DATA_DIR = path.join(__dirname, 'data');
const SHEETS_DIR = app.isPackaged ? path.join(app.getPath('userData'), 'sheets') : path.join(DATA_DIR, 'sheets');

let sheetWindow = null;
let buddyWindow = null;
let loggedInId = null;

function readJson(file, fallback) {
  try {
    return JSON.parse(fs.readFileSync(file, 'utf8'));
  } catch {
    return fallback;
  }
}

function roster() {
  return readJson(path.join(DATA_DIR, 'roster.json'), { crawlers: [], animationLibrary: [], actions: [] });
}

/** A repo-relative asset path as a file:// URL the renderer can load, or '' when the file is missing. */
function assetUrl(relative) {
  if (!relative) return '';
  const abs = path.join(REPO_ROOT, relative);
  return fs.existsSync(abs) ? pathToFileURL(abs).href : '';
}

function createSheetWindow() {
  sheetWindow = new BrowserWindow({
    width: 1320,
    height: 860,
    minWidth: 980,
    minHeight: 640,
    backgroundColor: '#05070f',
    title: 'Crawler Companion',
    icon: path.join(__dirname, 'assets', 'companion.ico'),
    autoHideMenuBar: true,
    webPreferences: {
      preload: path.join(__dirname, 'preload.js'),
      contextIsolation: true,
      nodeIntegration: false,
    },
  });
  sheetWindow.loadFile('index.html');
  sheetWindow.on('closed', () => {
    sheetWindow = null;
    if (buddyWindow) buddyWindow.close();
  });
}

function createBuddyWindow() {
  if (buddyWindow) {
    buddyWindow.focus();
    return;
  }
  const area = screen.getPrimaryDisplay().workArea;
  const width = 300;
  const height = 380;
  buddyWindow = new BrowserWindow({
    width,
    height,
    x: area.x + area.width - width - 40,
    y: area.y + area.height - height,
    frame: false,
    transparent: true,
    resizable: false,
    alwaysOnTop: true,
    skipTaskbar: true,
    hasShadow: false,
    webPreferences: {
      preload: path.join(__dirname, 'preload.js'),
      contextIsolation: true,
      nodeIntegration: false,
    },
  });
  buddyWindow.loadFile('buddy.html');
  buddyWindow.on('closed', () => {
    buddyWindow = null;
    if (sheetWindow) sheetWindow.webContents.send('buddy-closed');
  });
}

// --- IPC ---------------------------------------------------------------------------------------------

ipcMain.handle('roster', () => {
  const r = roster();
  // Clips pulled from Meshy's library (Tools/meshy_animation_pull.py + meshy_library_to_companion.py), if any.
  const meshy = readJson(path.join(DATA_DIR, 'meshy-library.json'), { files: [], actions: [], groups: [] });
  return {
    crawlers: r.crawlers.map((c) => ({
      id: c.id,
      name: c.name,
      colour: c.colour,
      hasPin: Boolean(c.pin),
      modelUrl: assetUrl(c.model),
    })),
    animationLibrary: [...r.animationLibrary, ...meshy.files].map(assetUrl).filter(Boolean),
    actions: [...r.actions, ...meshy.actions],
    groups: [...(r.groups || []), ...meshy.groups],
  };
});

// The game's own audience chatter table (Docs/DT_DCCChatterLines.csv, the source of DT_DCCChatterLines), so the
// Companion's feed is the same crowd, the same running gags and the same handles as the in-game Chatter tab.
function parseCsv(text) {
  const rows = [];
  let row = [];
  let cell = '';
  let quoted = false;
  for (let i = 0; i < text.length; i++) {
    const ch = text[i];
    if (quoted) {
      if (ch === '"' && text[i + 1] === '"') { cell += '"'; i++; }
      else if (ch === '"') quoted = false;
      else cell += ch;
    } else if (ch === '"') quoted = true;
    else if (ch === ',') { row.push(cell); cell = ''; }
    else if (ch === '\n' || ch === '\r') {
      if (ch === '\r' && text[i + 1] === '\n') i++;
      row.push(cell); cell = '';
      if (row.some((c) => c !== '')) rows.push(row);
      row = [];
    } else cell += ch;
  }
  if (cell || row.length) { row.push(cell); rows.push(row); }
  const [header, ...body] = rows;
  return body.map((r) => Object.fromEntries(header.map((h, i) => [h, r[i] ?? ''])));
}

ipcMain.handle('chatter-lines', () => {
  try {
    return parseCsv(fs.readFileSync(path.join(REPO_ROOT, 'Docs', 'DT_DCCChatterLines.csv'), 'utf8'))
      .map((r) => ({ bucket: r.Bucket, template: r.Template, handle: r.SenderHandle, gag: r.GagId }));
  } catch {
    return [];
  }
});

ipcMain.handle('login',(_e, id, pin) => {
  const c = roster().crawlers.find((x) => x.id === id);
  if (!c) return { ok: false, reason: 'Unknown crawler.' };
  if (c.pin && String(pin || '') !== String(c.pin)) return { ok: false, reason: 'That is not your PIN.' };
  loggedInId = id;
  return { ok: true };
});

ipcMain.handle('logout', () => {
  loggedInId = null;
  if (buddyWindow) buddyWindow.close();
  return true;
});

ipcMain.handle('whoami', () => loggedInId);

ipcMain.handle('load-sheet', (_e, id) => readJson(path.join(SHEETS_DIR, `${id}.json`), null));

ipcMain.handle('save-sheet', (_e, id, sheet) => {
  if (!id || id !== loggedInId) return false; // you can only write your own sheet
  fs.mkdirSync(SHEETS_DIR, { recursive: true });
  const file = path.join(SHEETS_DIR, `${id}.json`);
  const tmp = `${file}.tmp`;
  fs.writeFileSync(tmp, JSON.stringify(sheet, null, 2), 'utf8');
  fs.renameSync(tmp, file); // never leave a half-written sheet behind
  if (buddyWindow) buddyWindow.webContents.send('sheet-changed', sheet);
  return true;
});

ipcMain.handle('open-buddy', () => {
  createBuddyWindow();
  return true;
});

ipcMain.handle('close-buddy', () => {
  if (buddyWindow) buddyWindow.close();
  return true;
});

// The buddy walks around your desktop by moving its own window. The renderer decides where to go and
// how fast; main only clamps to the screen.
ipcMain.handle('buddy-bounds', () => {
  if (!buddyWindow) return null;
  const b = buddyWindow.getBounds();
  const area = screen.getDisplayMatching(b).workArea;
  return { x: b.x, y: b.y, width: b.width, height: b.height, area };
});

ipcMain.on('buddy-move', (_e, x) => {
  if (!buddyWindow) return;
  const b = buddyWindow.getBounds();
  const area = screen.getDisplayMatching(b).workArea;
  const clamped = Math.max(area.x, Math.min(area.x + area.width - b.width, Math.round(x)));
  buddyWindow.setBounds({ x: clamped, y: b.y, width: b.width, height: b.height });
});

// Dragging a frameless window by hand.
ipcMain.on('buddy-drag', (_e, dx, dy) => {
  if (!buddyWindow) return;
  const b = buddyWindow.getBounds();
  buddyWindow.setBounds({ x: b.x + dx, y: b.y + dy, width: b.width, height: b.height });
});

// The sheet window can ask the buddy to do something ("cast" from the sheet makes the desktop crawler cast).
ipcMain.on('buddy-action', (_e, actionId) => {
  if (buddyWindow) buddyWindow.webContents.send('buddy-action', actionId);
});

// Self-test: DCC_SNAPSHOT=<crawlerId> logs in, waits for the model, saves a screenshot to
// Companion/snapshot.png and quits. Used to check the app renders without anyone at the keyboard.
async function snapshotSelfTest(id) {
  // DCC_SNAPSHOT_OUT lets the installed app's self-test write somewhere writable.
  const out = process.env.DCC_SNAPSHOT_OUT || path.join(__dirname, 'snapshot.png');
  sheetWindow.webContents.once('did-finish-load', async () => {
    await sheetWindow.webContents.executeJavaScript(
      `document.querySelectorAll('.crawler-card')[${JSON.stringify(roster().crawlers.findIndex((c) => c.id === id))}]?.click()`
    ).catch(() => {});
    setTimeout(async () => {
      if (process.env.DCC_SNAPSHOT_EVAL) {
        await sheetWindow.webContents.executeJavaScript(process.env.DCC_SNAPSHOT_EVAL).catch((e) => console.log('eval failed', e));
        await new Promise((r) => setTimeout(r, 400));
      }
      if (process.env.DCC_SNAPSHOT_ACTION) {
        const wanted = JSON.stringify(process.env.DCC_SNAPSHOT_ACTION.toLowerCase());
        await sheetWindow.webContents.executeJavaScript(
          `[...document.querySelectorAll('.btn.action')].find(b => b.textContent.toLowerCase() === ${wanted})?.click()`
        ).catch(() => {});
        await new Promise((r) => setTimeout(r, Number(process.env.DCC_SNAPSHOT_ACTION_WAIT || 1200)));
      }
      // DCC_SNAPSHOT_SEQ: a JSON list of JS snippets. Each one runs, then the page is captured as
      // snapshot_<n>.png, all in one launch (much faster and steadier than one launch per frame).
      if (process.env.DCC_SNAPSHOT_SEQ) {
        const steps = JSON.parse(process.env.DCC_SNAPSHOT_SEQ);
        for (let i = 0; i < steps.length; i++) {
          const result = await sheetWindow.webContents.executeJavaScript(steps[i]).catch((e) => `failed: ${e}`);
          if (result !== undefined) console.log(`step ${i}:`, result);
          await new Promise((r) => setTimeout(r, Number(process.env.DCC_SNAPSHOT_STEP_WAIT || 500)));
          const shot = await sheetWindow.webContents.capturePage();
          fs.writeFileSync(path.join(path.dirname(out), `snapshot_${i}.png`), shot.toPNG());
        }
      }
      const image = await sheetWindow.webContents.capturePage();
      fs.writeFileSync(out, image.toPNG());
      const log = await sheetWindow.webContents.executeJavaScript('document.querySelector("#model-status")?.textContent || ""').catch(() => '');
      console.log(`snapshot written: ${out} | model status: ${log}`);
      app.quit();
    }, Number(process.env.DCC_SNAPSHOT_WAIT || 9000));
  });
}

// --- System Voice Studio (npm run studio) -----------------------------------------------------------------------
// Zoe's booth for recording the System AI herself. Finished lines go to SourceArt/Voice/SystemAI/zoe/ with the
// exact VO_<Row>.wav names the game's import step expects; raw takes go to zoe/raw/ so a line can be re-processed
// with new settings later without re-recording it.

const VOICE_DIR = path.join(REPO_ROOT, 'SourceArt', 'Voice', 'SystemAI', 'zoe');
const RAW_DIR = path.join(VOICE_DIR, 'raw');
const safeName = (n) => path.basename(String(n)).replace(/[^\w.-]/g, '_'); // never a path, only a file name
// NPC lines (Fort Tiner, Tools/gen_fort_tiner_lines.py) save beside the System AI's, one folder per speaker.
// Only "NPC/<Name>" is accepted, so a line can never write anywhere else.
const voiceDirFor = (folder) => (/^NPC\/[A-Za-z0-9_]+$/.test(String(folder || ''))
  ? path.join(REPO_ROOT, 'SourceArt', 'Voice', ...String(folder).split('/'))
  : VOICE_DIR);

function createStudioWindow() {
  const win = new BrowserWindow({
    width: 1440,
    height: 900,
    minWidth: 1100,
    minHeight: 680,
    backgroundColor: '#05070f',
    title: 'System Voice Studio',
    icon: path.join(__dirname, 'assets', 'studio.ico'),
    autoHideMenuBar: true,
    webPreferences: { preload: path.join(__dirname, 'studio-preload.js'), contextIsolation: true, nodeIntegration: false },
  });
  win.loadFile('studio.html');
  return win;
}

ipcMain.handle('studio-save', (_e, kind, name, wav, folder) => {
  const dir = kind === 'raw' ? RAW_DIR : voiceDirFor(folder);
  fs.mkdirSync(dir, { recursive: true });
  fs.writeFileSync(path.join(dir, safeName(name)), Buffer.from(wav));
  return true;
});
ipcMain.handle('studio-list-raw', () => (fs.existsSync(RAW_DIR) ? fs.readdirSync(RAW_DIR).filter((f) => f.endsWith('_raw.wav')) : []));
ipcMain.handle('studio-load-raw', (_e, name) => {
  const b = fs.readFileSync(path.join(RAW_DIR, safeName(name)));
  return b.buffer.slice(b.byteOffset, b.byteOffset + b.byteLength);
});
ipcMain.handle('studio-has-voice', (_e, name, folder) => fs.existsSync(path.join(voiceDirFor(folder), safeName(name))));

// Townsfolk and lot staff (Tools/gen_fort_tiner_lines.py, Tools/gen_impound_lines.py). The game reads these
// every round, so the studio can edit, add and delete lines with no rebuild. Each file is kept as .bak.
const NPC_LINE_FILES = {
  'fort_tiner_lines.json': path.join(REPO_ROOT, 'SourceArt', 'Voice', 'NPC', 'fort_tiner_lines.json'),
  'impound_lines.json': path.join(REPO_ROOT, 'SourceArt', 'Voice', 'NPC', 'impound_lines.json'),
  // The KMHR radio show (Docs/radio-show-altitude-sickness.md). Line order is script order; Tools/radio_mix.py
  // stitches the recordings into segments for the rover radio.
  'radio_lines.json': path.join(REPO_ROOT, 'SourceArt', 'Voice', 'NPC', 'radio_lines.json'),
};
ipcMain.handle('studio-npc-load', () => {
  const speakers = [];
  const lines = [];
  for (const [file, filePath] of Object.entries(NPC_LINE_FILES)) {
    if (!fs.existsSync(filePath)) continue;
    const data = JSON.parse(fs.readFileSync(filePath, 'utf8'));
    for (const speaker of data.speakers || []) speakers.push({ ...speaker, file });
    for (const line of data.lines || []) lines.push({ ...line, file });
  }
  return speakers.length ? { speakers, lines } : null;
});
ipcMain.handle('studio-npc-save', (_e, data) => {
  const ok = data && Array.isArray(data.speakers) && Array.isArray(data.lines)
    && data.speakers.every((s) => s && /^[A-Za-z0-9_]+$/.test(s.id) && typeof s.name === 'string' && NPC_LINE_FILES[s.file])
    && data.lines.every((l) => l && /^[A-Za-z0-9_]+$/.test(l.row) && typeof l.text === 'string' && l.text.length < 1000
      && data.speakers.some((s) => s.id === l.speaker))
    && new Set(data.lines.map((l) => l.row)).size === data.lines.length;
  if (!ok) return false;
  const buckets = {};
  for (const speaker of data.speakers) {
    // A character's voice from the studio (pitch, nasal, body, air): plain numbers only. The game ignores it.
    const voice = Object.fromEntries(Object.entries(speaker.voice || {})
      .filter(([k, v]) => ['pitch', 'nasal', 'body', 'air'].includes(k) && Number.isFinite(v) && Math.abs(v) <= 12));
    (buckets[speaker.file] ??= { speakers: [], lines: [] }).speakers.push({
      id: speaker.id, name: speaker.name, body: speaker.body || '', hat: speaker.hat || '',
      ...(Object.keys(voice).length ? { voice } : {}),
    });
  }
  for (const line of data.lines) {
    const speaker = data.speakers.find((s) => s.id === line.speaker);
    const file = line.file || speaker?.file;
    if (!buckets[file]) return false;
    buckets[file].lines.push({ speaker: line.speaker, row: line.row, text: line.text });
  }
  for (const [file, bucket] of Object.entries(buckets)) {
    const dest = NPC_LINE_FILES[file];
    fs.mkdirSync(path.dirname(dest), { recursive: true });
    if (fs.existsSync(dest)) fs.copyFileSync(dest, `${dest}.bak`);
    const tmp = `${dest}.tmp`;
    fs.writeFileSync(tmp, `${JSON.stringify(bucket, null, 2)}\n`, 'utf8');
    fs.renameSync(tmp, dest);
  }
  return true;
});

// Self-test: DCC_STUDIO_TEST=<in.wav>;<preset>;<out.wav> renders a file through a preset with no one at the mic.
async function studioSelfTest(win, spec) {
  const [input, preset, output] = spec.split(';');
  win.webContents.once('did-finish-load', async () => {
    await new Promise((r) => setTimeout(r, 1500));
    const bytes = [...fs.readFileSync(input)];
    try {
      const wav = await win.webContents.executeJavaScript(
        `window.__studio.processFile(new Uint8Array(${JSON.stringify(bytes)}).buffer, ${JSON.stringify(preset)},
          [{ time: 1.2, type: 'stutter' }, { time: 2.6, type: 'static' }]).then((b) => Array.from(new Uint8Array(b)))`
      );
      fs.writeFileSync(output, Buffer.from(wav));
      const shot = await win.webContents.capturePage();
      fs.writeFileSync(path.join(__dirname, 'studio_snapshot.png'), shot.toPNG());
      console.log(`studio test wrote ${output} (${wav.length} bytes)`);
    } catch (err) {
      console.log('studio test failed', err);
    }
    app.quit();
  });
}

// The studio is Zoe's tool and writes into the repo, so the installed (friends') app never opens it.
const STUDIO_MODE = !app.isPackaged && (process.argv.includes('--studio') || Boolean(process.env.DCC_STUDIO_TEST));

app.whenReady().then(() => {
  if (STUDIO_MODE) {
    const win = createStudioWindow();
    win.webContents.on('console-message', (_e, level, message) => {
      if (level >= 2) console.log(`[studio] ${message}`);
    });
    if (process.env.DCC_STUDIO_TEST) studioSelfTest(win, process.env.DCC_STUDIO_TEST);
    return;
  }
  createSheetWindow();
  // Warnings and errors also go to %APPDATA%\Crawler Companion\companion.log: an installed app has no console,
  // and "send Zoe that file" is a far easier support request than "describe what went wrong".
  const logFile = path.join(app.getPath('userData'), 'companion.log');
  const log = (line) => {
    console.log(line);
    try { fs.appendFileSync(logFile, `${new Date().toISOString()} ${line}\n`); } catch { /* logging must never break the app */ }
  };
  sheetWindow.webContents.on('console-message', (_e, level, message, lineNo, source) => {
    if (level >= 2) log(`[renderer] ${message} (${source}:${lineNo})`);
  });
  sheetWindow.webContents.on('render-process-gone', (_e, d) => log(`[renderer gone] ${d.reason}`));
  if (process.env.DCC_SNAPSHOT) snapshotSelfTest(process.env.DCC_SNAPSHOT);
});
app.on('window-all-closed', () => app.quit());
