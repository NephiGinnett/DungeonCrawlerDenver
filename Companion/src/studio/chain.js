// The Voice Studio's effects chain, built from Web Audio nodes around the per-sample worklet.
//
//   source -> [worklet: pitch, sub, ring, crush, glitches] -> metallic comb -> drive -> low cut -> high cut ->
//   presence -> chorus -> (echo send) -> (reverb send) -> output gain -> limiter
//
// The same builder serves the live monitor (your mic, now) and the offline render (a take, exactly), so what you
// hear while tweaking is what gets exported.

import { ensureWorklet } from './fx-worklet.js';
import { bootSound, shutdownSound } from './sounds.js';

export const DEFAULTS = {
  pitch: -3, sub: 0.35, ring: 0.25, ringFreq: 48,
  combMix: 0.3, combMs: 7, combFb: 0.55,
  bits: 16, down: 1, drive: 0.15,
  lowCut: 90, highCut: 9000, presence: 3,
  chorus: 0.25, echoMix: 0.12, echoMs: 180, echoFb: 0.3,
  reverbMix: 0.28, reverbSize: 2.4,
  glitchRate: 0.15, glitchTypes: ['stutter', 'tapestop', 'dropout', 'static', 'crush', 'warble'], seed: 7,
  gain: 1, intro: true, outro: true,
  // Character voice: small changes that make one reader sound like a different person. All zero = untouched.
  nasal: 0, body: 0, air: 0,
};

/** The settings a character's voice owns. They are saved per speaker and laid over the line's preset. */
export const VOICE_KEYS = ['pitch', 'nasal', 'body', 'air'];

export const PRESETS = {
  'The House': { ...DEFAULTS },
  'System AI (bright)': { ...DEFAULTS, pitch: 0, sub: 0.1, ring: 0.18, ringFreq: 70, combMix: 0.35, combMs: 4, reverbMix: 0.18, reverbSize: 1.6, presence: 5 },
  'Deep Ominous': { ...DEFAULTS, pitch: -5, sub: 0.6, ring: 0.12, ringFreq: 35, combMix: 0.2, reverbMix: 0.42, reverbSize: 4, highCut: 6500, glitchRate: 0.08 },
  'Broken Broadcast': { ...DEFAULTS, pitch: -1, sub: 0.15, ring: 0.3, bits: 9, down: 3, drive: 0.45, lowCut: 300, highCut: 4200, presence: 6, glitchRate: 0.6, reverbMix: 0.1 },
  'Full Robot': { ...DEFAULTS, pitch: -2, sub: 0.3, ring: 0.65, ringFreq: 60, combMix: 0.6, combMs: 10, combFb: 0.7, glitchRate: 0.25 },
  Clean: { ...DEFAULTS, pitch: 0, sub: 0, ring: 0, combMix: 0, drive: 0, chorus: 0, echoMix: 0, reverbMix: 0, glitchRate: 0, lowCut: 40, highCut: 18000, presence: 0, intro: false, outro: false },
};

/** A dark, slowly decaying impulse response: a big empty server hall. */
function makeImpulse(ctx, seconds) {
  const len = Math.max(1, Math.round(ctx.sampleRate * seconds));
  const buf = ctx.createBuffer(2, len, ctx.sampleRate);
  for (let c = 0; c < 2; c++) {
    const d = buf.getChannelData(c);
    let lp = 0;
    for (let i = 0; i < len; i++) {
      const t = i / len;
      lp += ((Math.random() * 2 - 1) - lp) * 0.35; // one-pole lowpass: darker tail
      d[i] = lp * Math.pow(1 - t, 3.2);
    }
  }
  return buf;
}

function driveCurve(amount) {
  const k = amount * 60;
  const n = 2048;
  const curve = new Float32Array(n);
  for (let i = 0; i < n; i++) {
    const x = (i / (n - 1)) * 2 - 1;
    curve[i] = k > 0 ? ((1 + k) * x) / (1 + k * Math.abs(x)) : x;
  }
  return curve;
}

/** Build the chain in a context. Returns { input, output, fx, set(settings), glitchNow(type), events(list) }. */
export async function buildChain(ctx, settings) {
  await ensureWorklet(ctx);
  const s = { ...DEFAULTS, ...settings };

  const input = ctx.createGain();
  const fx = new AudioWorkletNode(ctx, 'dcc-voice-fx', { numberOfInputs: 1, numberOfOutputs: 1, outputChannelCount: [1] });
  input.connect(fx);

  // Metallic comb: a very short feedback delay. This is most of the "robot" timbre.
  const combIn = ctx.createGain();
  const combDelay = ctx.createDelay(0.05);
  const combFb = ctx.createGain();
  const combWet = ctx.createGain();
  const combDry = ctx.createGain();
  fx.connect(combIn);
  combIn.connect(combDry);
  combIn.connect(combDelay);
  combDelay.connect(combFb);
  combFb.connect(combDelay);
  combDelay.connect(combWet);
  const afterComb = ctx.createGain();
  combDry.connect(afterComb);
  combWet.connect(afterComb);

  const shaper = ctx.createWaveShaper();
  shaper.oversample = '2x';
  const lowCut = ctx.createBiquadFilter();
  lowCut.type = 'highpass';
  const highCut = ctx.createBiquadFilter();
  highCut.type = 'lowpass';
  const presence = ctx.createBiquadFilter();
  presence.type = 'peaking';
  presence.frequency.value = 2800;
  presence.Q.value = 0.9;
  afterComb.connect(shaper).connect(lowCut).connect(highCut).connect(presence);

  // Character voice. Nasal is a honk around 1.1 kHz with the chest scooped out under it (talking through the
  // nose). Body is chest weight: a low shelf, up for deeper and fuller, down for thin and small. Air is breath
  // and sizzle on top, down for muffled.
  const nasalHonk = ctx.createBiquadFilter();
  nasalHonk.type = 'peaking';
  nasalHonk.frequency.value = 1100;
  nasalHonk.Q.value = 2.2;
  const nasalScoop = ctx.createBiquadFilter();
  nasalScoop.type = 'peaking';
  nasalScoop.frequency.value = 280;
  nasalScoop.Q.value = 1;
  const bodyShelf = ctx.createBiquadFilter();
  bodyShelf.type = 'lowshelf';
  bodyShelf.frequency.value = 220;
  const airShelf = ctx.createBiquadFilter();
  airShelf.type = 'highshelf';
  airShelf.frequency.value = 6500;
  presence.connect(nasalHonk).connect(nasalScoop).connect(bodyShelf).connect(airShelf);

  // Intro/outro stingers join here: after the voice-only processing, before the shared space.
  const post = ctx.createGain();
  airShelf.connect(post);

  // Chorus: a modulated short delay, for a synthetic, slightly-doubled sheen.
  const chorusDelay = ctx.createDelay(0.05);
  chorusDelay.delayTime.value = 0.018;
  const lfo = ctx.createOscillator();
  lfo.frequency.value = 0.6;
  const lfoDepth = ctx.createGain();
  lfoDepth.gain.value = 0.004;
  lfo.connect(lfoDepth).connect(chorusDelay.delayTime);
  lfo.start();
  const chorusWet = ctx.createGain();
  const dryBus = ctx.createGain();
  post.connect(dryBus);
  post.connect(chorusDelay).connect(chorusWet).connect(dryBus);

  // Echo and reverb sends.
  const echo = ctx.createDelay(1.5);
  const echoFb = ctx.createGain();
  const echoWet = ctx.createGain();
  dryBus.connect(echo);
  echo.connect(echoFb).connect(echo);
  echo.connect(echoWet);

  const reverb = ctx.createConvolver();
  const reverbWet = ctx.createGain();
  dryBus.connect(reverb).connect(reverbWet);

  const master = ctx.createGain();
  dryBus.connect(master);
  echoWet.connect(master);
  reverbWet.connect(master);

  const limiter = ctx.createDynamicsCompressor();
  limiter.threshold.value = -3;
  limiter.knee.value = 0;
  limiter.ratio.value = 20;
  limiter.attack.value = 0.002;
  limiter.release.value = 0.12;
  master.connect(limiter);

  // Web Audio's compressor adds automatic make-up gain, which drove test renders into the ceiling (27 clipped
  // samples in a 15 s line). A fixed trim after it leaves about 3 dB of headroom.
  const trim = ctx.createGain();
  trim.gain.value = 0.7;
  limiter.connect(trim);

  let lastReverbSize = null;
  let lastDrive = null;
  function set(next) {
    Object.assign(s, next);
    fx.port.postMessage({ settings: { pitch: s.pitch, sub: s.sub, ring: s.ring, ringFreq: s.ringFreq, bits: s.bits,
      down: s.down, glitchRate: s.glitchRate, glitchTypes: s.glitchTypes, seed: s.seed } });
    combDelay.delayTime.value = Math.max(0.001, s.combMs / 1000);
    combFb.gain.value = Math.min(0.92, s.combFb);
    combWet.gain.value = s.combMix;
    combDry.gain.value = 1 - s.combMix * 0.5;
    if (lastDrive !== s.drive) { shaper.curve = driveCurve(s.drive); lastDrive = s.drive; }
    lowCut.frequency.value = s.lowCut;
    highCut.frequency.value = s.highCut;
    presence.gain.value = s.presence;
    nasalHonk.gain.value = s.nasal * 10;
    nasalScoop.gain.value = -s.nasal * 5;
    bodyShelf.gain.value = s.body;
    airShelf.gain.value = s.air;
    chorusWet.gain.value = s.chorus;
    echo.delayTime.value = s.echoMs / 1000;
    echoFb.gain.value = Math.min(0.85, s.echoFb);
    echoWet.gain.value = s.echoMix;
    if (lastReverbSize !== s.reverbSize) { reverb.buffer = makeImpulse(ctx, s.reverbSize); lastReverbSize = s.reverbSize; }
    reverbWet.gain.value = s.reverbMix;
    master.gain.value = s.gain;
  }
  set(s);

  return {
    input, post, output: trim, fx, set,
    glitchNow: (type) => fx.port.postMessage({ glitchNow: type }),
    events: (list) => fx.port.postMessage({ events: list }),
  };
}

const INTRO_OVERLAP = 0.12;

/**
 * Render a take through the chain, offline, with the glitch track and the optional intro/outro.
 * glitches: [{ time (seconds into the raw take), type }]
 */
export async function renderTake(raw, settings, glitches = []) {
  const s = { ...DEFAULTS, ...settings };
  const sr = raw.sampleRate;
  const intro = s.intro ? bootSound(sr) : null;
  const outro = s.outro ? shutdownSound(sr) : null;
  const voiceStart = intro ? intro.duration - INTRO_OVERLAP : 0.05;
  const voiceEnd = voiceStart + raw.duration;
  const tail = Math.max(1.2, s.reverbSize * 0.8, s.echoMix > 0 ? 1.5 : 0);
  const total = voiceEnd + (outro ? outro.duration - 0.25 : 0) + tail;

  const ctx = new OfflineAudioContext(1, Math.ceil(total * sr), sr);
  const chain = await buildChain(ctx, s);
  chain.output.connect(ctx.destination);

  // Glitch track, in render time. The intro and outro bring their own: the voice "boots" with a stutter and
  // static, and "shuts down" on a tape stop.
  const events = glitches.map((g) => ({ time: voiceStart + g.time, type: g.type }));
  if (intro) events.push({ time: voiceStart + 0.03, type: 'stutter' }, { time: voiceStart, type: 'static' });
  if (outro) events.push({ time: Math.max(voiceStart, voiceEnd - 0.5), type: 'tapestop' });
  chain.events(events);

  const voice = ctx.createBufferSource();
  voice.buffer = raw;
  voice.connect(chain.input);
  voice.start(voiceStart);

  if (intro) {
    const src = ctx.createBufferSource();
    src.buffer = intro;
    src.connect(chain.post);
    src.start(0);
  }
  if (outro) {
    const src = ctx.createBufferSource();
    src.buffer = outro;
    src.connect(chain.post);
    src.start(voiceEnd - 0.25);
  }

  const rendered = await ctx.startRendering();
  return { buffer: trimTail(rendered), voiceStart };
}

/** Cut silence off the end (below about -60 dB), keeping a short fade. */
function trimTail(buf) {
  const d = buf.getChannelData(0);
  let end = d.length - 1;
  while (end > 0 && Math.abs(d[end]) < 0.001) end--;
  end = Math.min(d.length, end + Math.round(buf.sampleRate * 0.05));
  const out = new AudioBuffer({ length: Math.max(1, end), numberOfChannels: 1, sampleRate: buf.sampleRate });
  out.copyToChannel(d.subarray(0, end), 0);
  return out;
}
