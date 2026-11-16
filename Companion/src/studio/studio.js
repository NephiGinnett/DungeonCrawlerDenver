// System Voice Studio: record Zoe's own voice for the System AI, shape it into something robotic and ominous,
// glitch it on cue, and save each line straight to where the game's import step expects it.
//
// Two ways to hear the effects:
//   Live      the mic runs through the chain in real time. Move a slider and hear it immediately; Glitch fires
//             at once. This is for finding the voice.
//   Play      a recorded take, rendered offline through the identical chain plus the intro/outro and the glitch
//             track. This is exactly what Export writes.

import { DEFAULTS, PRESETS, VOICE_KEYS, buildChain, renderTake } from './chain.js';
import { bootSound, shutdownSound, glitchSting } from './sounds.js';
import { encodeWav } from './wav.js';

const $ = (s) => document.querySelector(s);
const api = window.studio || null; // Electron (npm run studio); null when opened as a plain page in a browser
const GLITCH_TYPES = ['stutter', 'tapestop', 'dropout', 'static', 'crush', 'warble'];

const KNOBS = [
  // The first four are a character's own voice: on a cast line they save to that speaker automatically.
  ['VOICE', [
    ['pitch', 'Pitch', -12, 12, 0.1, 'st'],
    ['nasal', 'Nasal', 0, 1, 0.01, ''],
    ['body', 'Deep / chest', -9, 9, 0.5, 'dB'],
    ['air', 'Breathy / airy', -9, 9, 0.5, 'dB'],
    ['sub', 'Sub octave', 0, 1, 0.01, ''],
  ]],
  ['ROBOT', [
    ['ring', 'Ring mod', 0, 1, 0.01, ''],
    ['ringFreq', 'Ring freq', 10, 400, 1, 'Hz'],
    ['combMix', 'Metal', 0, 1, 0.01, ''],
    ['combMs', 'Metal size', 1, 25, 0.5, 'ms'],
    ['combFb', 'Metal ring', 0, 0.92, 0.01, ''],
  ]],
  ['GRIT', [
    ['bits', 'Bit depth', 3, 16, 1, 'bit'],
    ['down', 'Downsample', 1, 12, 1, '×'],
    ['drive', 'Drive', 0, 1, 0.01, ''],
  ]],
  ['TONE', [
    ['lowCut', 'Low cut', 20, 800, 5, 'Hz'],
    ['highCut', 'High cut', 1500, 18000, 100, 'Hz'],
    ['presence', 'Presence', -12, 12, 0.5, 'dB'],
  ]],
  ['SPACE', [
    ['chorus', 'Chorus', 0, 1, 0.01, ''],
    ['echoMix', 'Echo', 0, 1, 0.01, ''],
    ['echoMs', 'Echo time', 40, 900, 10, 'ms'],
    ['echoFb', 'Echo repeats', 0, 0.85, 0.01, ''],
    ['reverbMix', 'Reverb', 0, 1, 0.01, ''],
    ['reverbSize', 'Hall size', 0.3, 6, 0.1, 's'],
  ]],
  ['GLITCH', [
    ['glitchRate', 'Random rate', 0, 2, 0.01, '/s'],
    ['seed', 'Variation', 1, 99, 1, ''],
  ]],
  ['OUTPUT', [
    ['gain', 'Level', 0.2, 2.5, 0.01, '×'],
  ]],
];

// --- State ---------------------------------------------------------------------------------------------------

// The System AI's lines, then the Fort Tiner townsfolk's (each with a speaker and its own folder), then a free take.
// Fort Tiner and the impound staff come from SourceArt/Voice/NPC/*_lines.json, which this studio can edit.
const lines = [...(window.DCC_LINES || []), { row: 'FreeTake', text: '(Anything you like. Saved as VO_FreeTake.)' }];
let npcData = null;
const lineFor = (row) => lines.find((x) => x.row === row);
const folderLabel = (l) => (l?.folder ? `SourceArt/Voice/${l.folder}` : api?.voiceFolder);
const takes = new Map(); // row -> { raw: AudioBuffer, glitches: [{time, type}], exported: bool }
const exportedOnDisk = new Set(); // rows whose VO_<row>.wav exists, raw take or not
let current = lines[0]?.row || 'FreeTake';
let settings = loadSettings();
let userPresets = loadUserPresets();

let ctx = null;          // realtime context, for live, playback and the meter
let micStream = null;
let micSource = null;
let liveChain = null;
let liveOn = false;
let recorder = null;
let recordStartedAt = 0;
let pendingGlitches = [];
let playing = null;      // { source, startedAt, offset, processed, voiceStart }
let analyser = null;

function loadSettings() {
  try { return { ...DEFAULTS, ...JSON.parse(localStorage.getItem('dcc.studio.settings') || '{}') }; } catch { return { ...DEFAULTS }; }
}
function saveSettings() {
  try { localStorage.setItem('dcc.studio.settings', JSON.stringify(settings)); } catch { /* convenience only */ }
}
function loadUserPresets() {
  try { return JSON.parse(localStorage.getItem('dcc.studio.presets') || '{}'); } catch { return {}; }
}

function status(msg) { $('#status').textContent = msg; }

async function audio() {
  if (!ctx) {
    ctx = new AudioContext({ latencyHint: 'interactive' });
    analyser = ctx.createAnalyser();
    analyser.fftSize = 1024;
  }
  if (ctx.state === 'suspended') await ctx.resume();
  return ctx;
}

async function mic() {
  await audio();
  if (!micStream) {
    // Noise suppression on (a room hum through a ring modulator is miserable), the rest off: echo
    // cancellation and auto gain both fight the effects.
    micStream = await navigator.mediaDevices.getUserMedia({
      audio: { echoCancellation: false, noiseSuppression: true, autoGainControl: false, channelCount: 1 },
    });
    micSource = ctx.createMediaStreamSource(micStream);
    micSource.connect(analyser);
  }
  return micStream;
}

// --- Rack ----------------------------------------------------------------------------------------------------

function fmt(v, step) {
  return step >= 1 ? String(Math.round(v)) : Number(v).toFixed(step < 0.1 ? 2 : 1);
}

function buildRack() {
  const host = $('#knobs');
  host.replaceChildren();
  for (const [title, knobs] of KNOBS) {
    const h = document.createElement('h3');
    h.textContent = title;
    host.append(h);
    for (const [key, label, min, max, step, unit] of knobs) {
      const row = document.createElement('div');
      row.className = 'knob';
      const l = document.createElement('label');
      l.textContent = label;
      const r = Object.assign(document.createElement('input'), { type: 'range', min, max, step, value: settings[key] });
      const o = document.createElement('output');
      o.textContent = `${fmt(settings[key], step)}${unit}`;
      r.addEventListener('input', () => {
        settings[key] = Number(r.value);
        o.textContent = `${fmt(settings[key], step)}${unit}`;
        applySettings();
        if (VOICE_KEYS.includes(key)) scheduleVoiceSave();
      });
      row.append(l, r, o);
      host.append(row);
    }
    if (title === 'GLITCH') {
      const chips = document.createElement('div');
      chips.className = 'chips';
      for (const t of GLITCH_TYPES) {
        const c = document.createElement('button');
        c.className = `chip${settings.glitchTypes.includes(t) ? ' on' : ''}`;
        c.textContent = t;
        c.title = 'Include this glitch in the random ones';
        c.addEventListener('click', () => {
          settings.glitchTypes = settings.glitchTypes.includes(t)
            ? settings.glitchTypes.filter((x) => x !== t)
            : [...settings.glitchTypes, t];
          c.classList.toggle('on');
          applySettings();
        });
        chips.append(c);
      }
      host.append(chips);
    }
  }
  $('#intro').checked = settings.intro;
  $('#outro').checked = settings.outro;
}

function applySettings() {
  saveSettings();
  liveChain?.set(settings);
}

function buildPresetList() {
  const sel = $('#preset');
  sel.replaceChildren(new Option('Presets…', ''));
  for (const name of Object.keys(PRESETS)) sel.append(new Option(name, `b:${name}`));
  for (const name of Object.keys(userPresets)) sel.append(new Option(`★ ${name}`, `u:${name}`));
}

$('#preset').addEventListener('change', (e) => {
  const v = e.target.value;
  if (!v) return;
  const p = v.startsWith('b:') ? PRESETS[v.slice(2)] : userPresets[v.slice(2)];
  settings = { ...DEFAULTS, ...p };
  buildRack();
  applySettings();
  status(`Preset: ${v.slice(2)}`);
});

// Electron has no prompt(), so naming a preset is an inline field.
$('#save-preset').addEventListener('click', () => {
  $('#preset-name').classList.toggle('hidden');
  $('#preset-name').focus();
});
$('#preset-name').addEventListener('keydown', (e) => {
  if (e.key === 'Escape') $('#preset-name').classList.add('hidden');
  if (e.key !== 'Enter') return;
  const name = e.target.value.trim();
  if (!name) return;
  userPresets[name] = { ...settings };
  try { localStorage.setItem('dcc.studio.presets', JSON.stringify(userPresets)); } catch { /* ignore */ }
  buildPresetList();
  e.target.value = '';
  $('#preset-name').classList.add('hidden');
  status(`Saved preset "${name}".`);
});

$('#intro').addEventListener('change', (e) => { settings.intro = e.target.checked; applySettings(); });
$('#outro').addEventListener('change', (e) => { settings.outro = e.target.checked; applySettings(); });

// --- Lines -----------------------------------------------------------------------------------------------------

function lineGroup(l) {
  if (l.row === 'FreeTake') return 'Booth';
  if (l.npc) return l.group || 'Cast';
  return 'System AI';
}

function lineCaption(l) {
  if (l.speaker) return `${l.speaker} · ${l.row.replace(/^[A-Z]+_[A-Za-z0-9]+_/, '')}`;
  return l.row.replace(/^Encounter_/, '⚔ ').replace(/^Tab_/, '▤ ');
}

function renderLines() {
  const host = $('#lines');
  host.replaceChildren();
  let lastGroup = '';
  for (const l of lines) {
    const group = lineGroup(l);
    if (group !== lastGroup) {
      const h = document.createElement('div');
      h.className = 'line-group';
      h.textContent = group;
      host.append(h);
      lastGroup = group;
    }
    const take = takes.get(l.row);
    const b = document.createElement('button');
    b.className = `line-btn${l.row === current ? ' on' : ''}`;
    const dot = document.createElement('span');
    // Done = exported where the game reads it. That is true even with no raw take kept in the studio
    // (a line exported before imports were saved as raw), so check the file itself too.
    dot.className = `dot${take?.exported || exportedOnDisk.has(l.row) ? ' done' : take ? ' raw' : ''}`;
    b.append(dot, lineCaption(l));
    b.addEventListener('click', () => select(l.row));
    host.append(b);
  }
}

function select(row) {
  stopPlayback();
  const previous = lineFor(current);
  current = row;
  const l = lines.find((x) => x.row === row);
  // Moving between the System AI and a townsperson switches to that group's preset (NPCs: Clean),
  // so a friend's country accent is not run through the robot voice by accident. A character's own voice
  // (pitch, nasal, chest, air) goes on top, so switching to Dale's line makes you sound like Dale.
  if (l?.preset && (previous?.folder || '') !== (l.folder || '') && PRESETS[l.preset]) {
    const voice = speakerFor(l)?.voice || {};
    settings = { ...DEFAULTS, ...PRESETS[l.preset], pitch: 0, nasal: 0, body: 0, air: 0, ...voice };
    buildRack();
    applySettings();
    const tuned = Object.values(voice).some((v) => v);
    status(`Preset: ${l.preset}${tuned ? ` + ${l.speaker}'s voice` : ''} (for ${l.speaker || 'this line'})`);
  }
  $('#prompter').innerHTML = '';
  const id = document.createElement('span');
  id.className = 'row-id';
  id.textContent = l?.speaker ? `${l.speaker}  ·  VO_${row}` : `VO_${row}`;
  $('#prompter').append(id, document.createTextNode(l?.text || ''));
  endEdit();
  $('#line-tools').classList.toggle('hidden', !(l?.npc && api));
  renderLines();
  drawWave();
}

// --- Character lines: edit, add, delete -------------------------------------------------------------------------

function rebuildNpcLines() {
  for (let i = lines.length - 1; i >= 0; i--) if (lines[i].npc) lines.splice(i, 1);
  if (!npcData) return;
  const names = Object.fromEntries(npcData.speakers.map((s) => [s.id, s.name]));
  const groups = {
    'fort_tiner_lines.json': 'Fort Tiner',
    'impound_lines.json': 'Mile High Impound',
  };
  // The radio show is long, so it gets one heading per segment (rows are KMHR_S<segment>_<line>).
  const RADIO_SEGMENTS = ['Cold open', 'Ad: Summit Compliance', 'The horse', 'Ad: LIQUIDATOR', 'Call-in hour',
    'Ad: Bright Path', 'Theory board', 'Sign-off', 'Station IDs'];
  const groupFor = (l) => {
    const seg = l.file === 'radio_lines.json' && /^KMHR_S(\d+)_/.exec(l.row);
    if (seg) return `KMHR 5280 AM · ${seg[1]} ${RADIO_SEGMENTS[parseInt(seg[1], 10) - 1] || ''}`.trim();
    // Car owners who yell during a repo. Separate from the yard staff so they are easy to record.
    if (l.file === 'impound_lines.json' && ['Pryce', 'Nia', 'Holt'].includes(l.speaker)) {
      return 'Repo Rally · Angry owners';
    }
    return groups[l.file] || 'Cast';
  };
  const at = lines.findIndex((l) => l.row === 'FreeTake');
  const npcLines = npcData.lines.map((l) => ({
    row: l.row, text: l.text, speaker: names[l.speaker] || l.speaker, speakerId: l.speaker,
    folder: `NPC/${l.speaker}`, preset: 'Clean', npc: true, group: groupFor(l), file: l.file,
  }));
  lines.splice(at < 0 ? lines.length : at, 0, ...npcLines);
}

async function loadNpcLines() {
  if (!api?.loadNpcLines) return;
  try { npcData = await api.loadNpcLines(); } catch { npcData = null; }
  rebuildNpcLines();
}

// --- Character voices: the VOICE knobs on a cast line save to that speaker --------------------------------------

const speakerFor = (l) => (l?.npc && npcData ? npcData.speakers.find((s) => s.id === l.speakerId) : null);
let voiceSaveTimer = null;

/** Tuning Pitch, Nasal, Deep or Airy on a character's line saves it as their voice, shortly after you stop. */
function scheduleVoiceSave() {
  const l = lineFor(current);
  const speaker = speakerFor(l);
  if (!speaker || !api) return;
  clearTimeout(voiceSaveTimer);
  voiceSaveTimer = setTimeout(async () => {
    speaker.voice = Object.fromEntries(VOICE_KEYS.filter((k) => settings[k]).map((k) => [k, settings[k]]));
    const ok = await api.saveNpcLines(npcData);
    status(ok ? `Saved ${l.speaker}'s voice. Every line of theirs uses it.` : 'Could not save the voice.');
  }, 700);
}

async function resetVoice() {
  const l = lineFor(current);
  const speaker = speakerFor(l);
  if (!speaker) return;
  for (const k of VOICE_KEYS) settings[k] = 0;
  speaker.voice = {};
  buildRack();
  applySettings();
  const ok = await api.saveNpcLines(npcData);
  status(ok ? `${l.speaker} is back to your natural voice.` : 'Could not save the voice.');
}

async function saveNpcLines(message, selectRow) {
  const ok = await api.saveNpcLines(npcData);
  if (!ok) { status('Could not save the lines (check the row names).'); return false; }
  rebuildNpcLines();
  select(selectRow);
  status(message);
  return true;
}

function endEdit() {
  $('#line-edit').classList.add('hidden');
  $('#prompter').classList.remove('hidden');
  for (const id of ['#line-edit-btn', '#line-new', '#line-delete']) $(id).classList.remove('hidden');
  for (const id of ['#line-save', '#line-cancel']) $(id).classList.add('hidden');
}

function beginEdit() {
  const l = lineFor(current);
  if (!l?.npc) return;
  $('#line-edit').value = l.text;
  $('#line-edit').classList.remove('hidden');
  $('#prompter').classList.add('hidden');
  for (const id of ['#line-edit-btn', '#line-new', '#line-delete']) $(id).classList.add('hidden');
  for (const id of ['#line-save', '#line-cancel']) $(id).classList.remove('hidden');
  $('#line-edit').focus();
}

$('#line-edit-btn').addEventListener('click', beginEdit);
$('#line-cancel').addEventListener('click', () => select(current));
$('#line-save').addEventListener('click', async () => {
  const text = $('#line-edit').value.replace(/\s+/g, ' ').trim();
  const entry = npcData?.lines.find((x) => x.row === current);
  if (!entry || !text) return;
  entry.text = text;
  const recorded = takes.has(current) || await api.hasVoice(`VO_${current}.wav`, lineFor(current)?.folder);
  await saveNpcLines(recorded
    ? 'Saved. This line already has a recording, so re-record it to match the new words.'
    : 'Saved. The game uses it from the next round.', current);
});

$('#voice-reset').addEventListener('click', resetVoice);

$('#line-new').addEventListener('click', async () => {
  const l = lineFor(current);
  if (!l?.npc || !npcData) return;
  // A radio-show line is a script: the new line goes right after this one, in the same segment, because
  // radio_mix.py plays lines in file order. Everyone else's lines group by speaker.
  const radio = l.file === 'radio_lines.json';
  const sample = radio ? npcData.lines.find((x) => x.row === l.row) : npcData.lines.find((x) => x.speaker === l.speakerId);
  const prefix = sample ? sample.row.replace(/\d+$/, '') : `${l.speakerId}_`;
  const used = npcData.lines.filter((x) => x.row.startsWith(prefix)).map((x) => parseInt(x.row.slice(prefix.length), 10) || 0);
  const row = `${prefix}${String((used.length ? Math.max(...used) : 0) + 1).padStart(2, '0')}`;
  const after = radio
    ? npcData.lines.findIndex((x) => x.row === l.row)
    : npcData.lines.map((x) => x.speaker).lastIndexOf(l.speakerId);
  npcData.lines.splice(after + 1, 0, {
    speaker: l.speakerId, row, text: '(New line. Click Edit line to write it.)', file: sample?.file,
  });
  if (await saveNpcLines(`Added ${row}. Write it, then record it.`, row)) beginEdit();
});

let deleteArmed = null;
$('#line-delete').addEventListener('click', async () => {
  const l = lineFor(current);
  if (!l?.npc || !npcData) return;
  if (deleteArmed !== current) {
    deleteArmed = current;
    status(`Click Delete line again to remove ${current}. Any recording of it stays on disk.`);
    setTimeout(() => { if (deleteArmed === current) deleteArmed = null; }, 4000);
    return;
  }
  deleteArmed = null;
  const i = lines.findIndex((x) => x.row === current);
  const neighbour = lines[i + 1]?.npc ? lines[i + 1].row : (lines[i - 1]?.row || 'FreeTake');
  npcData.lines = npcData.lines.filter((x) => x.row !== current);
  await saveNpcLines(`Deleted ${l.row}.`, neighbour);
});

// --- Live ------------------------------------------------------------------------------------------------------

async function setLive(on) {
  try {
    if (on) {
      await mic();
      if (!liveChain) {
        liveChain = await buildChain(ctx, settings);
        micSource.connect(liveChain.input);
      }
      liveChain.output.connect(ctx.destination);
      liveOn = true;
    } else if (liveChain) {
      liveChain.output.disconnect();
      liveOn = false;
    }
  } catch (err) {
    status(`Microphone unavailable: ${err.message || err}`);
    liveOn = false;
  }
  $('#live').classList.toggle('on', liveOn);
  $('#live-warn').classList.toggle('hidden', !liveOn);
}

$('#live').addEventListener('click', () => setLive(!liveOn));

// --- Recording -------------------------------------------------------------------------------------------------

async function toggleRecord() {
  if (recorder) {
    recorder.stop();
    return;
  }
  stopPlayback();
  try {
    await mic();
  } catch (err) {
    status(`Microphone unavailable: ${err.message || err}`);
    return;
  }
  const chunks = [];
  recorder = new MediaRecorder(micStream);
  pendingGlitches = [];
  recorder.ondataavailable = (e) => e.data.size && chunks.push(e.data);
  recorder.onstop = async () => {
    recorder = null;
    $('#rec').classList.remove('on');
    $('#rec').textContent = 'Record';
    try {
      const raw = await ctx.decodeAudioData(await new Blob(chunks).arrayBuffer());
      const mono = toMono(raw);
      takes.set(current, { raw: mono, glitches: pendingGlitches, exported: false });
      status(`Recorded ${mono.duration.toFixed(1)} s${pendingGlitches.length ? `, ${pendingGlitches.length} glitch(es) marked` : ''}. Press Play to hear it processed.`);
      api?.saveRaw(current, encodeWav(mono));
    } catch (err) {
      status(`Could not read the recording: ${err.message || err}`);
    }
    renderLines();
    drawWave();
  };
  recorder.start();
  recordStartedAt = ctx.currentTime;
  $('#rec').classList.add('on');
  $('#rec').textContent = 'Stop recording';
  status('Recording… press G to drop a glitch exactly here.');
}

function toMono(buf) {
  if (buf.numberOfChannels === 1) return buf;
  const out = new AudioBuffer({ length: buf.length, numberOfChannels: 1, sampleRate: buf.sampleRate });
  const d = out.getChannelData(0);
  for (let c = 0; c < buf.numberOfChannels; c++) {
    const src = buf.getChannelData(c);
    for (let i = 0; i < d.length; i++) d[i] += src[i] / buf.numberOfChannels;
  }
  return out;
}

$('#rec').addEventListener('click', toggleRecord);

$('#import').addEventListener('click', () => $('#import-file').click());
$('#import-file').addEventListener('change', async (e) => {
  const file = e.target.files[0];
  if (!file) return;
  await audio();
  const raw = toMono(await ctx.decodeAudioData(await file.arrayBuffer()));
  takes.set(current, { raw, glitches: [], exported: false });
  // Keep it like a recorded take, so it is still here (and still marked) next time the studio opens.
  api?.saveRaw(current, encodeWav(raw));
  status(`Imported ${file.name} (${raw.duration.toFixed(1)} s).`);
  e.target.value = '';
  renderLines();
  drawWave();
});

// --- Playback --------------------------------------------------------------------------------------------------

function stopPlayback() {
  if (playing) {
    try { playing.source.stop(); } catch { /* already stopped */ }
    playing = null;
  }
}

async function play(processed) {
  stopPlayback();
  const take = takes.get(current);
  if (!take) { status('Nothing recorded for this line yet.'); return; }
  await audio();
  let buffer = take.raw;
  let voiceStart = 0;
  if (processed) {
    status('Rendering…');
    const r = await renderTake(take.raw, settings, take.glitches);
    buffer = r.buffer;
    voiceStart = r.voiceStart;
    status('');
  }
  const source = ctx.createBufferSource();
  source.buffer = buffer;
  source.connect(ctx.destination);
  source.start();
  playing = { source, startedAt: ctx.currentTime, processed, voiceStart };
  source.onended = () => { if (playing?.source === source) playing = null; };
}

$('#play-fx').addEventListener('click', () => play(true));
$('#play-raw').addEventListener('click', () => play(false));
$('#stop').addEventListener('click', () => { stopPlayback(); if (recorder) recorder.stop(); });

async function playBuffer(buf) {
  await audio();
  stopPlayback();
  const src = ctx.createBufferSource();
  src.buffer = buf;
  src.connect(ctx.destination);
  src.start();
  playing = { source: src, startedAt: ctx.currentTime, processed: false, voiceStart: 0, sting: true };
}
$('#intro-test').addEventListener('click', async () => playBuffer(bootSound((await audio()).sampleRate)));
$('#outro-test').addEventListener('click', async () => playBuffer(shutdownSound((await audio()).sampleRate)));

// --- Glitch ----------------------------------------------------------------------------------------------------

function chosenGlitch() {
  const v = $('#glitch-type').value;
  return v === 'random' ? GLITCH_TYPES[Math.floor(Math.random() * GLITCH_TYPES.length)] : v;
}

async function glitch() {
  const type = chosenGlitch();
  $('#glitch').classList.add('flash');
  setTimeout(() => $('#glitch').classList.remove('flash'), 150);

  if (recorder) {
    pendingGlitches.push({ time: ctx.currentTime - recordStartedAt, type });
  } else if (playing && !playing.sting) {
    // Marked against the raw take's own timeline, whichever version is playing.
    const take = takes.get(current);
    const t = ctx.currentTime - playing.startedAt - (playing.processed ? playing.voiceStart : 0);
    if (take && t >= 0 && t <= take.raw.duration) {
      take.glitches.push({ time: t, type });
      drawWave();
      status(`Glitch (${type}) added at ${t.toFixed(2)} s. Play again to hear it in place.`);
    }
  }

  if (liveOn && liveChain) liveChain.glitchNow(type);
  else if (!recorder && !playing) playBuffer(glitchSting((await audio()).sampleRate)); // so the button always does something
}
$('#glitch').addEventListener('click', glitch);

// --- Waveform --------------------------------------------------------------------------------------------------

const wave = $('#wave');
function drawWave() {
  const w = (wave.width = wave.clientWidth * devicePixelRatio);
  const h = (wave.height = wave.clientHeight * devicePixelRatio);
  const g = wave.getContext('2d');
  g.clearRect(0, 0, w, h);
  const take = takes.get(current);
  if (!take) {
    g.fillStyle = '#7f95b3';
    g.font = `${13 * devicePixelRatio}px Bahnschrift, sans-serif`;
    g.fillText('No take yet. Record (R) or import a file.', 12 * devicePixelRatio, h / 2);
    return;
  }
  const d = take.raw.getChannelData(0);
  const per = Math.max(1, Math.floor(d.length / w));
  g.fillStyle = '#48d8ff';
  for (let x = 0; x < w; x++) {
    let max = 0;
    for (let i = x * per; i < (x + 1) * per && i < d.length; i++) max = Math.max(max, Math.abs(d[i]));
    const bar = Math.max(1, max * h * 0.95);
    g.fillRect(x, (h - bar) / 2, 1, bar);
  }
  g.font = `${10 * devicePixelRatio}px Bahnschrift, sans-serif`;
  for (const gl of take.glitches) {
    const x = (gl.time / take.raw.duration) * w;
    g.fillStyle = 'rgba(199,125,255,0.9)';
    g.fillRect(x - devicePixelRatio, 0, 2 * devicePixelRatio, h);
    g.fillText(gl.type, x + 3 * devicePixelRatio, 12 * devicePixelRatio);
  }
}

function waveTime(e) {
  const take = takes.get(current);
  const r = wave.getBoundingClientRect();
  return take ? ((e.clientX - r.left) / r.width) * take.raw.duration : null;
}

wave.addEventListener('click', (e) => {
  const take = takes.get(current);
  const t = waveTime(e);
  if (!take || t == null) return;
  const tol = (6 / wave.clientWidth) * take.raw.duration;
  const i = take.glitches.findIndex((gl) => Math.abs(gl.time - t) < tol);
  if (i >= 0) {
    take.glitches.splice(i, 1);
    drawWave();
  }
});
wave.addEventListener('dblclick', (e) => {
  const take = takes.get(current);
  const t = waveTime(e);
  if (!take || t == null) return;
  take.glitches.push({ time: t, type: chosenGlitch() });
  drawWave();
});
new ResizeObserver(drawWave).observe(wave);

// --- Export ----------------------------------------------------------------------------------------------------

async function exportRow(row) {
  const take = takes.get(row);
  if (!take) return false;
  const { buffer } = await renderTake(take.raw, settings, take.glitches);
  const wav = encodeWav(buffer);
  const name = `VO_${row}.wav`;
  if (api) {
    await api.saveVoice(name, wav, lineFor(row)?.folder);
  } else {
    const a = document.createElement('a');
    a.href = URL.createObjectURL(new Blob([wav], { type: 'audio/wav' }));
    a.download = name;
    a.click();
  }
  take.exported = true;
  return true;
}

$('#export').addEventListener('click', async () => {
  status('Rendering and saving…');
  const ok = await exportRow(current);
  status(ok ? `Saved VO_${current}.wav${api ? ` to ${folderLabel(lineFor(current))}` : ' (downloaded)'}.` : 'Nothing to export yet.');
  renderLines();
});

$('#export-all').addEventListener('click', async () => {
  let n = 0;
  for (const row of takes.keys()) {
    status(`Exporting ${row}…`);
    if (await exportRow(row)) n++;
  }
  status(`Exported ${n} line(s) with the current settings.`);
  renderLines();
});

// --- Keys and meter --------------------------------------------------------------------------------------------

window.addEventListener('keydown', (e) => {
  if (e.target.matches('input[type=text], textarea, select')) return;
  const k = e.key.toLowerCase();
  if (k === 'g') { e.preventDefault(); glitch(); }
  else if (k === 'r') { e.preventDefault(); toggleRecord(); }
  else if (k === 'l') { e.preventDefault(); setLive(!liveOn); }
  else if (k === ' ') { e.preventDefault(); playing ? stopPlayback() : play(true); }
  else if (k === 'e') { e.preventDefault(); $('#export').click(); }
  else if (k === 'arrowdown' || k === 'arrowup') {
    e.preventDefault();
    const i = lines.findIndex((l) => l.row === current);
    const next = lines[Math.min(lines.length - 1, Math.max(0, i + (k === 'arrowdown' ? 1 : -1)))];
    if (next) select(next.row);
  }
});

const meterData = new Float32Array(1024);
(function meter() {
  requestAnimationFrame(meter);
  if (!analyser || !micStream) return;
  analyser.getFloatTimeDomainData(meterData);
  let peak = 0;
  for (const v of meterData) peak = Math.max(peak, Math.abs(v));
  $('#meter').style.width = `${Math.min(100, peak * 140)}%`;
})();

// --- Start -----------------------------------------------------------------------------------------------------

async function restoreRawTakes() {
  if (!api) return;
  for (const l of lines) {
    try {
      if (await api.hasVoice(`VO_${l.row}.wav`, l.folder)) exportedOnDisk.add(l.row);
    } catch { /* keep going */ }
  }
  const names = await api.listRaw();
  for (const name of names) {
    try {
      await audio();
      const raw = toMono(await ctx.decodeAudioData(await api.loadRaw(name)));
      const row = name.replace(/^VO_/, '').replace(/_raw\.wav$/, '');
      takes.set(row, { raw, glitches: [], exported: await api.hasVoice(`VO_${row}.wav`, lineFor(row)?.folder) });
    } catch { /* a bad file should not stop the rest loading */ }
  }
  renderLines();
  drawWave();
}

$('#save-target').textContent = api ? `Saving to ${api.voiceFolder}` : 'Browser mode: exports download';
buildRack();
buildPresetList();
select(current);
loadNpcLines().then(() => { renderLines(); restoreRawTakes(); });

// Self-test hook (see main.js, DCC_STUDIO_TEST).
window.__studio = {
  async processFile(arrayBuffer, presetName, glitches = []) {
    await audio();
    const raw = toMono(await ctx.decodeAudioData(arrayBuffer));
    const s = presetName ? { ...DEFAULTS, ...PRESETS[presetName] } : settings;
    const { buffer } = await renderTake(raw, s, glitches);
    return encodeWav(buffer);
  },
};
