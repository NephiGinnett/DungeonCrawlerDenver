// The Table: one small relay on Zoe's PC that every Crawler Companion connects to.
//
//   npm run table            (or double-click "Start The Table.bat")
//
// What it carries today:
//   sheets     each player's sheet. Only that player (or the DM) may write it; everyone can read it.
//   rolls      every d20 anyone rolls, so the whole party sees the dice.
//   presence   who's connected right now.
// Designed to carry next (Docs/ROADMAP.md section 5), from the game itself:
//   chatter    the real in-game audience feed, which replaces the Companion's locally rolled one
//   tokens     crawler positions for the top-down map
//
// Trust model, in plain terms: this is for five friends on a LAN or a private network like Tailscale, not the
// open internet. A player can only write their own sheet; the DM and the game sign in with keys from
// server/table-config.json, generated the first time this runs.

const fs = require('fs');
const os = require('os');
const path = require('path');
const crypto = require('crypto');
const { WebSocketServer } = require('ws');

const HERE = __dirname;
const DATA = path.join(HERE, 'table-data');
const SHEETS = path.join(DATA, 'sheets');
const CONFIG = path.join(HERE, 'table-config.json');
const ROSTER = JSON.parse(fs.readFileSync(path.join(HERE, '..', 'data', 'roster.json'), 'utf8'));

fs.mkdirSync(SHEETS, { recursive: true });

function loadConfig() {
  if (!fs.existsSync(CONFIG)) {
    const fresh = {
      port: 8770,
      dmKey: crypto.randomBytes(4).toString('hex'),
      gameKey: crypto.randomBytes(8).toString('hex'),
      note: 'dmKey: type it into the Companion as Zoe to get the DM view. gameKey: for the UE server to push chatter and tokens.',
    };
    fs.writeFileSync(CONFIG, JSON.stringify(fresh, null, 2));
  }
  return JSON.parse(fs.readFileSync(CONFIG, 'utf8'));
}

const config = loadConfig();
const PORT = Number(process.env.TABLE_PORT || config.port || 8770);
const crawlerIds = new Set(ROSTER.crawlers.map((c) => c.id));

// --- State ---------------------------------------------------------------------------------------------------

const sheets = {};
for (const f of fs.readdirSync(SHEETS)) {
  if (!f.endsWith('.json')) continue;
  try { sheets[f.slice(0, -5)] = JSON.parse(fs.readFileSync(path.join(SHEETS, f), 'utf8')); } catch { /* skip a bad file */ }
}
const rolls = []; // last 60
const clients = new Map(); // ws -> { id, role }

const saveTimers = {};
function persistSheet(id) {
  clearTimeout(saveTimers[id]);
  saveTimers[id] = setTimeout(() => {
    const file = path.join(SHEETS, `${id}.json`);
    fs.writeFileSync(`${file}.tmp`, JSON.stringify(sheets[id], null, 2));
    fs.renameSync(`${file}.tmp`, file);
  }, 500);
}

function online() {
  return [...clients.values()].filter((c) => c.role !== 'game').map((c) => ({ id: c.id, role: c.role }));
}

function broadcast(msg, except) {
  const text = JSON.stringify(msg);
  for (const ws of clients.keys()) {
    if (ws !== except && ws.readyState === ws.OPEN) ws.send(text);
  }
}

const log = (...a) => console.log(new Date().toLocaleTimeString(), ...a);

// --- Server --------------------------------------------------------------------------------------------------

const wss = new WebSocketServer({ port: PORT, maxPayload: 2 * 1024 * 1024 });

wss.on('connection', (ws, req) => {
  let me = null;
  const reply = (m) => ws.readyState === ws.OPEN && ws.send(JSON.stringify(m));

  ws.on('message', (raw) => {
    let m;
    try { m = JSON.parse(raw); } catch { return; }

    if (m.type === 'hello') {
      if (m.role === 'game') {
        if (m.key !== config.gameKey) return reply({ type: 'error', message: 'Wrong game key.' });
        me = { id: 'game', role: 'game' };
      } else {
        if (!crawlerIds.has(m.id)) return reply({ type: 'error', message: `Unknown crawler "${m.id}".` });
        const dm = m.dmKey && m.dmKey === config.dmKey;
        me = { id: m.id, role: dm ? 'dm' : 'player' };
      }
      clients.set(ws, me);
      log(`+ ${me.id} (${me.role}) from ${req.socket.remoteAddress}`);
      reply({ type: 'welcome', you: me, sheets, rolls, online: online() });
      broadcast({ type: 'presence', online: online() }, ws);
      return;
    }

    if (!me) return reply({ type: 'error', message: 'Say hello first.' });

    switch (m.type) {
      case 'sheet': {
        const id = m.sheet?.id;
        if (!id || (id !== me.id && me.role !== 'dm')) return reply({ type: 'error', message: 'You can only change your own sheet.' });
        // Newest edit wins, so reconnecting with an old offline copy can't roll back a newer one.
        if (sheets[id] && (sheets[id].updatedAt || 0) > (m.sheet.updatedAt || 0)) return reply({ type: 'sheet', id, sheet: sheets[id] });
        sheets[id] = m.sheet;
        persistSheet(id);
        broadcast({ type: 'sheet', id, sheet: m.sheet }, ws);
        break;
      }
      case 'roll': {
        const roll = { by: me.id, label: String(m.label || 'Roll').slice(0, 80), total: Number(m.total) || 0,
          detail: String(m.detail || '').slice(0, 160), natural: Number(m.natural) || 0, at: Date.now() };
        rolls.push(roll);
        if (rolls.length > 60) rolls.shift();
        broadcast({ type: 'roll', roll });
        break;
      }
      case 'chatter':
      case 'token':
        // Only the game (or the DM, for testing) speaks for the world.
        if (me.role !== 'game' && me.role !== 'dm') return;
        broadcast({ ...m, type: m.type }, ws);
        break;
      default:
        break;
    }
  });

  ws.on('close', () => {
    if (me) log(`- ${me.id}`);
    clients.delete(ws);
    broadcast({ type: 'presence', online: online() });
  });

  // Dead connections (a laptop lid closed) are noticed and dropped.
  ws.isAlive = true;
  ws.on('pong', () => { ws.isAlive = true; });
});

setInterval(() => {
  for (const ws of wss.clients) {
    if (!ws.isAlive) { ws.terminate(); continue; }
    ws.isAlive = false;
    ws.ping();
  }
}, 15000);

wss.on('listening', () => {
  const addresses = Object.values(os.networkInterfaces()).flat()
    .filter((a) => a && a.family === 'IPv4' && !a.internal).map((a) => a.address);
  console.log('');
  console.log('  THE TABLE IS OPEN');
  console.log('  -----------------');
  console.log('  Friends type one of these into the Companion\'s "Table" box:');
  for (const a of addresses) console.log(`      ${a}:${PORT}`);
  console.log(`      localhost:${PORT}   (on this PC)`);
  console.log('');
  console.log(`  DM key (log in as Zoe, enter it for the DM view): ${config.dmKey}`);
  console.log('  Keep this window open while you play. Close it to end the session.');
  console.log('');
});

wss.on('error', (err) => {
  console.error(err.code === 'EADDRINUSE' ? `Port ${PORT} is already in use. Is the Table already running?` : err);
  process.exit(1);
});
