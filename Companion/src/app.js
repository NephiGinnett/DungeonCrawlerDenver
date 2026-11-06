import { STATS, modifier, fmtMod, enhancedScore, maxMana, maxHp, evade, carryKg, xpForLevel, skillCheck } from './ruleset.js';
import { newSheet } from './sheet-template.js';
import { createViewer } from './viewer.js';
import { attachWheel, groupActions } from './wheel.js';
import { createChatter } from './chatter.js';
import { createTableLink } from './table-link.js';

const $ = (sel, root = document) => root.querySelector(sel);
const el = (tag, attrs = {}, ...children) => {
  const n = document.createElement(tag);
  for (const [k, v] of Object.entries(attrs)) {
    if (k === 'class') n.className = v;
    else if (k.startsWith('on')) n.addEventListener(k.slice(2), v);
    else if (v !== undefined && v !== null) n.setAttribute(k, v);
  }
  for (const c of children.flat()) if (c !== null && c !== undefined) n.append(c.nodeType ? c : document.createTextNode(String(c)));
  return n;
};

let roster = null;
let me = null; // roster entry
let sheet = null;
let viewer = null;
let saveTimer = null;
let buddyOpen = false;
let wheelActions = [];
let wheel = null;
let chatter = null;
let partySheets = {};   // id -> sheet, from the Table
let partyOnline = [];   // [{ id, role }]
let tableRolls = [];    // shared dice log

const CHATTER_PREF = 'dcc.chatter.on';
const chatterWanted = () => { try { return localStorage.getItem(CHATTER_PREF) === '1'; } catch { return false; } };

async function setupChatter() {
  chatter?.stop();
  $('#chatter').replaceChildren();
  chatter = createChatter($('#chatter'), {
    name: me.name,
    items: () => Object.values(sheet.gear || {}).filter(Boolean).map((g) => g.name),
  });
  await chatter.load();
  setChatter(chatterWanted());
}

function setChatter(on) {
  if (on) chatter?.start(); else chatter?.stop();
  $('#chatter-toggle').classList.toggle('on', on);
  try { localStorage.setItem(CHATTER_PREF, on ? '1' : '0'); } catch { /* per-viewer convenience only */ }
}

function doAction(id) {
  viewer?.play(id);
  window.companion.buddyAction(id);
  chatter?.react(id);
}

// --- Login --------------------------------------------------------------------------------------------

async function showLogin() {
  $('#sheet-screen').classList.add('hidden');
  $('#login').classList.remove('hidden');
  roster = await window.companion.roster();
  const cards = $('#crawler-cards');
  cards.replaceChildren();
  let pending = null;

  for (const c of roster.crawlers) {
    const card = el('button', { class: 'crawler-card', style: `--accent:${c.colour}` },
      el('div', { class: 'initial' }, c.name.slice(0, 1)),
      el('div', { class: 'card-name' }, c.name),
      el('div', { class: 'card-sub' }, c.modelUrl ? 'Model ready' : 'No model yet'));
    card.addEventListener('click', async () => {
      $('#login-error').textContent = '';
      if (c.hasPin) {
        pending = c;
        $('#pin-row').classList.remove('hidden');
        $('#pin').value = '';
        $('#pin').focus();
        return;
      }
      await tryLogin(c, '');
    });
    cards.append(card);
  }

  $('#pin-go').onclick = () => pending && tryLogin(pending, $('#pin').value);
  $('#pin').onkeydown = (e) => e.key === 'Enter' && pending && tryLogin(pending, $('#pin').value);
}

async function tryLogin(c, pin) {
  const result = await window.companion.login(c.id, pin);
  if (!result.ok) {
    $('#login-error').textContent = result.reason;
    return;
  }
  me = c;
  sheet = (await window.companion.loadSheet(c.id)) || newSheet(c.id, c.name);
  await showSheet();
}

// --- Sheet ---------------------------------------------------------------------------------------------

function save() {
  $('#save-state').textContent = 'Saving...';
  clearTimeout(saveTimer);
  saveTimer = setTimeout(async () => {
    sheet.updatedAt = Date.now();
    const ok = await window.companion.saveSheet(me.id, sheet);
    table.send({ type: 'sheet', sheet });
    $('#save-state').textContent = ok ? 'Saved' : 'Could not save';
  }, 400);
}

/** Set a dotted path on the sheet ("stats.str", "sponsors.1.name"), then save and refresh derived values. */
function setPath(path, value) {
  const keys = path.split('.');
  let o = sheet;
  for (let i = 0; i < keys.length - 1; i++) o = o[keys[i]] ??= {};
  o[keys.at(-1)] = value;
  save();
  refreshDerived();
}

function getPath(path) {
  return path.split('.').reduce((o, k) => (o == null ? undefined : o[k]), sheet);
}

function field(path, { type = 'text', label, cls = '', placeholder = '' } = {}) {
  const input = el(type === 'textarea' ? 'textarea' : 'input', {
    class: `fld ${cls}`,
    type: type === 'textarea' ? undefined : type,
    placeholder,
  });
  const v = getPath(path);
  input.value = v ?? '';
  input.addEventListener('input', () => setPath(path, type === 'number' ? Number(input.value) : input.value));
  return label ? el('label', { class: 'fld-wrap' }, el('span', { class: 'fld-label' }, label), input) : input;
}

function panel(title, ...children) {
  return el('div', { class: 'sys-window panel' }, el('div', { class: 'sys-header' }, `[ ${title} ]`), ...children);
}

function toast(html, colour = 'var(--cyan)') {
  const t = $('#roll-toast');
  t.innerHTML = html;
  t.style.setProperty('--accent', colour);
  t.classList.remove('hidden');
  clearTimeout(t._timer);
  t._timer = setTimeout(() => t.classList.add('hidden'), 3800);
}

function rollFor(label, { rank = 0, stat = 'str', disadvantage = false }) {
  const mod = modifier(enhancedScore(sheet, stat));
  const r = skillCheck({ rank: Number(rank) || 0, mod, disadvantage });
  const dice = r.dice.length > 1 ? `${r.dice[0]} / ${r.dice[1]} → ${r.die}` : `${r.die}`;
  const crit = r.die === 20 ? ' — NATURAL 20' : r.die === 1 ? ' — natural 1' : '';
  toast(`<div class="roll-title">${label}${crit}</div><div class="roll-total">${r.total}</div>
         <div class="roll-math">d20 ${dice} + rank ${r.rank} + ${stat.toUpperCase()} ${fmtMod(mod)}${disadvantage ? ' (disadvantage)' : ''}</div>`,
    r.die === 20 ? 'var(--gold)' : r.die === 1 ? 'var(--red)' : 'var(--cyan)');
  if (/cast|spell/i.test(label)) window.companion.buddyAction('cast');
  table.send({ type: 'roll', label, total: r.total, natural: r.die,
    detail: `d20 ${dice} + rank ${r.rank} + ${stat.toUpperCase()} ${fmtMod(mod)}` });
}

function identityPanel() {
  return panel('CRAWLER',
    el('div', { class: 'identity' },
      el('div', { class: 'id-name' }, field('name', { cls: 'big' })),
      el('div', { class: 'grid4' },
        field('crawlerNumber', { label: 'Crawler #' }),
        field('pronouns', { label: 'Pronouns' }),
        field('race', { label: 'Race' }),
        field('class', { label: 'Class', placeholder: 'Floor 3' }),
        field('level', { label: 'Level', type: 'number' }),
        field('xp', { label: 'XP', type: 'number' }),
        field('floor', { label: 'Floor', type: 'number' }),
        field('favor', { label: 'AI Favor', type: 'number' })),
      el('div', { class: 'xpbar' }, el('div', { class: 'xpfill', id: 'xpfill' }), el('span', { id: 'xptext' }))));
}

function vitalsPanel() {
  const hp = el('div', { class: 'hp-slots', id: 'hp-slots' });
  return panel('VITALS',
    el('div', { class: 'vitals' },
      el('div', { class: 'vital' }, el('div', { class: 'vital-label' }, 'HEALTH'), hp, el('div', { class: 'vital-num', id: 'hp-num' })),
      el('div', { class: 'vital' }, el('div', { class: 'vital-label' }, 'MANA'),
        el('div', { class: 'bar mana' }, el('div', { class: 'fill', id: 'mana-fill' })), el('div', { class: 'vital-num', id: 'mana-num' })),
      el('div', { class: 'grid4 small' },
        el('div', { class: 'derived' }, el('span', {}, 'EVADE'), el('b', { id: 'd-evade' })),
        el('div', { class: 'derived' }, el('span', {}, 'DR'), field('dr', { type: 'number', cls: 'tiny' })),
        el('div', { class: 'derived' }, el('span', {}, 'MOVE'), field('move', { type: 'number', cls: 'tiny' })),
        el('div', { class: 'derived' }, el('span', {}, 'CARRY'), el('b', { id: 'd-carry' })))));
}

function statsPanel() {
  const rows = STATS.map((s) =>
    el('div', { class: 'stat' },
      el('div', { class: 'stat-abbr' }, s.abbr),
      el('div', { class: 'stat-name' }, s.name),
      field(`stats.${s.key}`, { type: 'number', cls: 'stat-score' }),
      el('div', { class: 'stat-enh', id: `enh-${s.key}` }),
      el('button', { class: 'stat-mod', id: `mod-${s.key}`, title: `Roll a ${s.name} check`,
        onclick: () => rollFor(`${s.name} check`, { stat: s.key }) })));
  return panel('STATS', el('p', { class: 'hint' }, 'Score is unenhanced (potions, levels). Gear and buffs make the enhanced value. Click a bonus to roll d20 + it.'), ...rows);
}

/** A small editable table bound to an array on the sheet. */
function tablePanel(title, path, columns, blank, { rollable } = {}) {
  const body = el('tbody');
  const render = () => {
    body.replaceChildren();
    (getPath(path) || []).forEach((row, i) => {
      const tr = el('tr');
      for (const col of columns) {
        let cell;
        if (col.type === 'stat') {
          cell = el('select', { class: 'fld' }, ...STATS.map((s) => el('option', { value: s.key }, s.abbr)));
          cell.value = row[col.key] || 'str';
          cell.addEventListener('change', () => setPath(`${path}.${i}.${col.key}`, cell.value));
        } else if (col.type === 'check') {
          cell = el('input', { type: 'checkbox', class: 'chk' });
          cell.checked = Boolean(row[col.key]);
          cell.addEventListener('change', () => setPath(`${path}.${i}.${col.key}`, cell.checked));
        } else {
          cell = field(`${path}.${i}.${col.key}`, { type: col.type || 'text', cls: col.cls || '' });
        }
        tr.append(el('td', {}, cell));
      }
      const actions = el('td', { class: 'row-actions' });
      if (rollable) actions.append(el('button', { class: 'btn tiny', title: 'Roll', onclick: () => rollFor(row.name || title, row) }, 'd20'));
      actions.append(el('button', { class: 'btn tiny ghost', title: 'Remove', onclick: () => { getPath(path).splice(i, 1); save(); render(); } }, '×'));
      tr.append(actions);
      body.append(tr);
    });
  };
  render();
  return panel(title,
    el('table', { class: 'tbl' }, el('thead', {}, el('tr', {}, ...columns.map((c) => el('th', {}, c.label)), el('th'))), body),
    el('button', { class: 'btn add', onclick: () => { (getPath(path) || setPath(path, []) || getPath(path)).push(structuredClone(blank)); save(); render(); } }, `+ Add`));
}

function gearPanel() {
  const slots = [['head', 'Head'], ['body', 'Body'], ['mainHand', 'Main hand'], ['offHand', 'Off hand'], ['feet', 'Feet'], ['trinket', 'Trinket']];
  return panel('GEAR',
    el('div', { class: 'gear' }, ...slots.map(([key, label]) => {
      const g = sheet.gear[key];
      const name = el('input', { class: 'fld', placeholder: 'empty' });
      name.value = g?.name || '';
      name.addEventListener('input', () => {
        sheet.gear[key] = name.value ? { ...(sheet.gear[key] || {}), name: name.value } : null;
        save();
        refreshDerived();
      });
      const bonus = g?.bonus ? Object.entries(g.bonus).filter(([, v]) => v).map(([k, v]) => `${k.toUpperCase()} ${fmtMod(v)}`).join(', ') : '';
      return el('div', { class: 'gear-slot' }, el('span', { class: 'fld-label' }, label), name,
        el('span', { class: 'gear-note' }, [g?.dice, bonus, g?.note].filter(Boolean).join(' · ')));
    })));
}

function companionsPanel() {
  return panel('PET & MOUNT',
    el('div', { class: 'grid2' },
      field('pet.name', { label: 'Pet' }), field('pet.kind', { label: 'Kind' }),
      field('mount.name', { label: 'Mount / vehicle' }), field('mount.kind', { label: 'Kind' })));
}

function sponsorsPanel() {
  return panel('SPONSORS',
    el('div', { class: 'sponsors' }, ...[0, 1, 2].map((i) =>
      el('div', { class: 'sponsor' }, field(`sponsors.${i}.name`, { placeholder: `Sponsor ${i + 1}` }),
        field(`sponsors.${i}.terms`, { type: 'textarea', placeholder: 'Terms (read them)' })))));
}

function notesPanel() {
  return panel('NOTES', field('notes', { type: 'textarea', cls: 'notes' }), el('div', { class: 'grid2' }, field('kills', { label: 'Kills', type: 'number' })));
}

function refreshDerived() {
  if (!sheet) return;
  for (const s of STATS) {
    const enh = enhancedScore(sheet, s.key);
    const base = Number(sheet.stats[s.key]) || 0;
    const m = modifier(enh);
    const enhEl = $(`#enh-${s.key}`);
    if (enhEl) enhEl.textContent = enh !== base ? `→ ${enh}` : '';
    const modEl = $(`#mod-${s.key}`);
    if (modEl) modEl.textContent = fmtMod(m);
  }

  const hpMax = maxHp(sheet);
  const hp = sheet.hp == null ? hpMax : Math.min(sheet.hp, hpMax);
  const slots = $('#hp-slots');
  if (slots) {
    slots.replaceChildren(...Array.from({ length: hpMax }, (_, i) =>
      el('button', { class: `slot ${i < hp ? 'full' : ''}`, title: 'Click to set health',
        onclick: () => { sheet.hp = i + 1 === hp ? i : i + 1; save(); refreshDerived(); } })));
  }
  $('#hp-num').textContent = `${hp} / ${hpMax}`;

  const manaMax = maxMana(sheet);
  const mana = sheet.mana == null ? manaMax : Math.min(sheet.mana, manaMax);
  $('#mana-fill').style.width = `${manaMax ? (mana / manaMax) * 100 : 0}%`;
  $('#mana-num').textContent = `${mana} / ${manaMax}`;

  $('#d-evade').textContent = evade(sheet);
  $('#d-carry').textContent = `${carryKg(sheet)} kg`;

  const lvl = Math.max(1, Number(sheet.level) || 1);
  const need = xpForLevel(lvl);
  const capped = lvl >= 5;
  $('#xpfill').style.width = capped ? '100%' : `${Math.min(100, ((Number(sheet.xp) || 0) / need) * 100)}%`;
  $('#xptext').textContent = capped ? 'MAX (tutorial cap)' : `${sheet.xp || 0} / ${need} XP`;
}

async function showSheet() {
  $('#login').classList.add('hidden');
  $('#sheet-screen').classList.remove('hidden');
  document.documentElement.style.setProperty('--me', me.colour);

  const s = $('#sheet');
  s.replaceChildren(
    el('div', { class: 'col' }, identityPanel(), vitalsPanel(), statsPanel(), gearPanel()),
    el('div', { class: 'col' },
      partyPanel(),
      tablePanel('ATTACKS', 'attacks', [
        { key: 'name', label: 'Attack' }, { key: 'rank', label: 'Rank', type: 'number', cls: 'tiny' },
        { key: 'stat', label: 'Stat', type: 'stat' }, { key: 'dice', label: 'Dice', cls: 'small' },
        { key: 'damageType', label: 'Type', cls: 'small' }, { key: 'effect', label: 'Effect' },
      ], { name: '', rank: 0, stat: 'str', dice: '1d6', damageType: '', effect: '' }, { rollable: true }),
      tablePanel('SKILLS', 'skills', [
        { key: 'name', label: 'Skill' }, { key: 'rank', label: 'Rank', type: 'number', cls: 'tiny' },
        { key: 'stat', label: 'Stat', type: 'stat' }, { key: 'type', label: 'Type', cls: 'small' },
        { key: 'marked', label: 'Used', type: 'check' },
      ], { name: '', rank: 0, stat: 'str', type: 'Utility', marked: false }, { rollable: true }),
      tablePanel('SPELLS', 'spells', [
        { key: 'name', label: 'Spell' }, { key: 'rank', label: 'Rank', type: 'number', cls: 'tiny' },
        { key: 'stat', label: 'Stat', type: 'stat' }, { key: 'mana', label: 'Mana', type: 'number', cls: 'tiny' },
        { key: 'effect', label: 'Effect' },
      ], { name: '', rank: 1, stat: 'int', mana: 10, effect: '' }, { rollable: true }),
      tablePanel('INVENTORY', 'inventory', [
        { key: 'item', label: 'Item' }, { key: 'quantity', label: 'Qty', type: 'number', cls: 'tiny' }, { key: 'notes', label: 'Notes' },
      ], { item: '', quantity: 1, notes: '' }),
      tablePanel('BUFFS & DEBUFFS', 'debuffs', [
        { key: 'name', label: 'Effect' }, { key: 'notes', label: 'Notes' },
      ], { name: '', notes: '' }),
      companionsPanel(), sponsorsPanel(), notesPanel()));
  refreshDerived();
  renderParty();

  // The model
  // Transparent, so the chatter feed can scroll behind the crawler.
  if (!viewer) viewer = createViewer($('#model-canvas'), { autoRotate: false, transparent: true });
  setupChatter();
  window.__viewer = viewer; // for the DCC_SNAPSHOT self-test
  if (!wheel) {
    wheel = attachWheel($('.model-stage'), { getGroups: () => groupActions(roster.groups, wheelActions, doAction) });
    window.__wheel = wheel;
  }
  $('#model-status').textContent = me.modelUrl ? 'Loading model...' : 'No model yet. The System is still deciding what you look like.';
  $('#model-status').classList.remove('hidden');
  try {
    const result = await viewer.load(me.modelUrl, roster.animationLibrary);
    const actions = viewer.setActions(roster.actions);
    wheelActions = actions;
    const bar = $('#action-buttons');
    bar.replaceChildren(...actions.map((a) =>
      el('button', { class: 'btn action', onclick: () => doAction(a.id) }, a.label)));
    if (result.hasModel) $('#model-status').classList.add('hidden');
  } catch (err) {
    $('#model-status').textContent = `Could not load the model: ${err.message || err}`;
  }
}

// --- The Table (server/table-server.js) -----------------------------------------------------------------------
//
// Optional. Without it the Companion works alone, exactly as before. Connected, your sheet syncs to the Table,
// everyone's rolls appear in a shared log, the Party panel shows the whole group, and the game's real audience
// chatter (once the game is wired up) replaces the locally rolled feed.

const TABLE_PREF = 'dcc.table.address';

const table = createTableLink({
  onStatus(state, detail) {
    const b = $('#table-btn');
    b.dataset.state = state;
    b.textContent = state === 'on' ? `Table: ${detail === 'dm' ? 'DM' : 'on'}` : state === 'connecting' ? 'Table: …' : 'Table: off';
    $('#table-status').textContent =
      state === 'on' ? `Connected${detail === 'dm' ? ' with the DM view' : ''}.` :
      state === 'connecting' ? (detail === 'reconnecting' ? 'Lost the Table; trying again…' : 'Connecting…') :
      state === 'error' ? detail : 'Not connected. The Companion works fine on its own.';
    // Out of the way once it worked; it sits over the Party panel otherwise.
    if (state === 'on') setTimeout(() => $('#table-panel').classList.add('hidden'), 1200);
  },
  on_welcome(m) {
    partySheets = m.sheets || {};
    partyOnline = m.online || [];
    tableRolls = m.rolls || [];
    const theirs = partySheets[me.id];
    if (theirs && (theirs.updatedAt || 0) > (sheet.updatedAt || 0)) adoptSheet(theirs);
    else table.send({ type: 'sheet', sheet });
    renderParty();
  },
  on_sheet(m) {
    partySheets[m.id] = m.sheet;
    if (m.id === me?.id && (m.sheet.updatedAt || 0) > (sheet.updatedAt || 0)) adoptSheet(m.sheet);
    renderParty();
  },
  on_presence(m) {
    partyOnline = m.online || [];
    renderParty();
  },
  on_roll(m) {
    tableRolls.push(m.roll);
    if (tableRolls.length > 60) tableRolls.shift();
    renderParty();
    if (m.roll.by !== me?.id) partyToast(m.roll);
  },
  on_chatter(m) {
    chatter?.pushLive(String(m.handle || 'viewer'), String(m.text || ''));
  },
});

/** Someone else (the DM, or this player on another PC) changed this sheet more recently: take theirs. */
function adoptSheet(next) {
  sheet = next;
  window.companion.saveSheet(me.id, sheet);
  showSheet();
}

/** Another player's roll: built with textContent, never innerHTML, since the text came over the network. */
function partyToast(roll) {
  const t = $('#roll-toast');
  const colour = roll.natural === 20 ? 'var(--gold)' : roll.natural === 1 ? 'var(--red)' : 'var(--cyan)';
  t.replaceChildren(
    el('div', { class: 'roll-title' }, `${roll.by}: ${roll.label}${roll.natural === 20 ? ' — NATURAL 20' : ''}`),
    el('div', { class: 'roll-total' }, roll.total),
    el('div', { class: 'roll-math' }, roll.detail));
  t.style.setProperty('--accent', colour);
  t.classList.remove('hidden');
  clearTimeout(t._timer);
  t._timer = setTimeout(() => t.classList.add('hidden'), 3800);
}

function partyPanel() {
  return panel('PARTY', el('div', { id: 'party' }), el('div', { class: 'fld-label', style: 'margin-top:10px' }, 'Table log'),
    el('div', { id: 'table-log', class: 'table-log' }));
}

function renderParty() {
  const host = $('#party');
  if (!host || !roster) return;
  const onlineIds = new Set(partyOnline.map((o) => o.id));
  const connected = table.status() === 'on';
  host.replaceChildren(...roster.crawlers.map((c) => {
    const s = c.id === me?.id ? sheet : partySheets[c.id];
    const hpMax = s ? maxHp(s) : 0;
    const hp = s ? (s.hp == null ? hpMax : Math.min(s.hp, hpMax)) : 0;
    const mMax = s ? maxMana(s) : 0;
    const mana = s ? (s.mana == null ? mMax : Math.min(s.mana, mMax)) : 0;
    const on = c.id === me?.id || onlineIds.has(c.id);
    return el('div', { class: `party-row${on ? ' online' : ''}`, style: `--accent:${c.colour}` },
      el('span', { class: 'party-dot', title: on ? 'Online' : 'Offline' }),
      el('span', { class: 'party-name' }, s?.name || c.name),
      el('span', { class: 'party-lvl' }, s ? `L${s.level || 1}` : ''),
      el('span', { class: 'party-bars' },
        el('span', { class: 'pbar hp' }, el('span', { style: `width:${hpMax ? (hp / hpMax) * 100 : 0}%` })),
        el('span', { class: 'pbar mp' }, el('span', { style: `width:${mMax ? (mana / mMax) * 100 : 0}%` }))),
      el('span', { class: 'party-num' }, s ? `${hp}/${hpMax}` : '—'));
  }));
  if (!connected) host.append(el('p', { class: 'hint' }, 'Connect to the Table (top right) to see everyone live.'));

  const logHost = $('#table-log');
  logHost.replaceChildren(...tableRolls.slice(-8).reverse().map((r) =>
    el('div', { class: `log-row${r.natural === 20 ? ' crit' : r.natural === 1 ? ' fumble' : ''}` },
      el('b', {}, r.by), ` ${r.label}: `, el('b', {}, r.total))));
  if (!tableRolls.length) logHost.append(el('span', { class: 'hint' }, 'No rolls yet.'));
}

function connectTable() {
  const address = $('#table-address').value.trim();
  if (!address || !me) return;
  try { localStorage.setItem(TABLE_PREF, address); } catch { /* convenience only */ }
  table.connect(address, { id: me.id, role: 'player', dmKey: $('#table-dmkey').value.trim() || undefined });
}

$('#table-btn').addEventListener('click', () => {
  $('#table-panel').classList.toggle('hidden');
  $('#table-dm-row').classList.toggle('hidden', me?.id !== 'Zoe');
  try { $('#table-address').value ||= localStorage.getItem(TABLE_PREF) || ''; } catch { /* ignore */ }
});
$('#table-connect').addEventListener('click', connectTable);
$('#table-address').addEventListener('keydown', (e) => e.key === 'Enter' && connectTable());
$('#table-disconnect').addEventListener('click', () => { table.disconnect(); partyOnline = []; renderParty(); });

// --- Buttons -------------------------------------------------------------------------------------------

$('#chatter-toggle').addEventListener('click', () => setChatter(!chatter?.isRunning()));

$('#logout').addEventListener('click', async () => {
  chatter?.stop();
  table.disconnect();
  await window.companion.logout();
  buddyOpen = false;
  $('#buddy-toggle').textContent = 'Desktop buddy';
  me = null;
  sheet = null;
  showLogin();
});

$('#buddy-toggle').addEventListener('click', async () => {
  if (buddyOpen) await window.companion.closeBuddy();
  else await window.companion.openBuddy();
  buddyOpen = !buddyOpen;
  $('#buddy-toggle').textContent = buddyOpen ? 'Hide buddy' : 'Desktop buddy';
});

window.companion.on('buddy-closed', () => {
  buddyOpen = false;
  $('#buddy-toggle').textContent = 'Desktop buddy';
});

showLogin();
