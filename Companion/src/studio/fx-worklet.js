// The per-sample half of the Voice Studio's effects: things Web Audio has no built-in node for.
//
//   pitch shift   two-tap granular shifter over a history buffer (formant-free, slightly metallic, which suits us)
//   sub octave    a second shifter an octave below the first, mixed under it: the "ominous" body
//   ring mod      multiplies the voice by a sine; 30-80 Hz is the classic robot
//   bitcrush      sample-and-hold downsampling plus bit-depth quantising
//   glitches      stutter, tape stop, dropout, static, crush and warble: scheduled (from the glitch track), fired
//                 live (the Glitch button), or random (seeded, so the preview and the export glitch identically)
//
// Exported as a string and loaded through a Blob URL, so it works from file:// in a browser as well as in Electron.

export const FX_WORKLET_SOURCE = String.raw`
class Shifter {
  constructor(win) { this.win = win; this.phase = 0; }
  // Reads from the history ring buffer "h" whose newest sample is at index w-1.
  process(h, n, w, ratio) {
    const W = this.win;
    const read = (d) => {
      let p = w - 2 - d;
      while (p < 0) p += n;
      const i = Math.floor(p), f = p - i;
      return h[i % n] * (1 - f) + h[(i + 1) % n] * f;
    };
    const d1 = this.phase;
    const d2 = (this.phase + W / 2) % W;
    const g1 = 1 - Math.abs(2 * d1 / W - 1);
    const g2 = 1 - Math.abs(2 * d2 / W - 1);
    const y = read(d1) * g1 + read(d2) * g2;
    this.phase -= (ratio - 1);
    while (this.phase < 0) this.phase += W;
    while (this.phase >= W) this.phase -= W;
    return y;
  }
}

class DCCVoiceFx extends AudioWorkletProcessor {
  constructor() {
    super();
    this.s = { pitch: 0, sub: 0, ring: 0, ringFreq: 55, bits: 16, down: 1,
               glitchRate: 0, glitchTypes: ['stutter', 'tapestop', 'dropout', 'static', 'crush', 'warble'], seed: 7 };
    this.n = Math.round(sampleRate * 2.5);
    this.hist = new Float32Array(this.n);   // dry input
    this.outHist = new Float32Array(this.n); // processed output, for stutter and tape stop
    this.w = 0;
    this.main = new Shifter(Math.round(sampleRate * 0.05));
    this.subShift = new Shifter(Math.round(sampleRate * 0.07));
    this.ringPhase = 0;
    this.held = 0; this.holdCount = 0;
    this.t = 0;
    this.events = [];
    this.nextEvent = 0;
    this.active = null;
    this.env = 0;
    this.rng = 7;
    this.port.onmessage = (e) => {
      const m = e.data;
      if (m.settings) { Object.assign(this.s, m.settings); this.rng = (this.s.seed | 0) || 7; }
      if (m.events) { this.events = m.events.slice().sort((a, b) => a.time - b.time); this.nextEvent = 0; }
      if (m.glitchNow) this.start(m.glitchNow);
    };
  }

  random() { // xorshift: seeded, so random glitches land in the same places every render
    let x = this.rng | 0;
    x ^= x << 13; x ^= x >>> 17; x ^= x << 5;
    this.rng = x;
    return ((x >>> 0) % 100000) / 100000;
  }

  start(type) {
    const sr = sampleRate;
    const r = this.random();
    switch (type) {
      case 'stutter': {
        const slice = Math.round(sr * (0.06 + r * 0.05));
        const reps = 3 + Math.floor(this.random() * 3);
        this.active = { type, i: 0, len: slice * reps, slice, from: (this.w - slice + this.n) % this.n };
        break;
      }
      case 'tapestop':
        this.active = { type, i: 0, len: Math.round(sr * 0.5), pos: (this.w - 1 + this.n) % this.n, speed: 1 };
        break;
      case 'dropout':
        this.active = { type, i: 0, len: Math.round(sr * (0.05 + r * 0.07)) };
        break;
      case 'static':
        this.active = { type, i: 0, len: Math.round(sr * (0.12 + r * 0.1)) };
        break;
      case 'crush':
        this.active = { type, i: 0, len: Math.round(sr * (0.15 + r * 0.12)), held: 0 };
        break;
      case 'warble':
        this.active = { type, i: 0, len: Math.round(sr * (0.12 + r * 0.1)), ph: 0 };
        break;
      default:
        this.active = null;
    }
  }

  readOut(p) {
    const n = this.n;
    p = ((p % n) + n) % n;
    const i = Math.floor(p), f = p - i;
    return this.outHist[i] * (1 - f) + this.outHist[(i + 1) % n] * f;
  }

  process(inputs, outputs) {
    const input = inputs[0] && inputs[0][0];
    const out = outputs[0][0];
    if (!out) return true;
    const s = this.s, sr = sampleRate, n = this.n;
    const ratio = Math.pow(2, s.pitch / 12);
    const step = 2 / Math.pow(2, Math.max(1, s.bits));
    const down = Math.max(1, Math.round(s.down));

    for (let k = 0; k < out.length; k++) {
      const x = input ? input[k] : 0;
      this.hist[this.w] = x;
      this.w = (this.w + 1) % n;
      this.env = Math.max(Math.abs(x), this.env * 0.9995);

      // Voice: pitch, then the sub-octave underneath it.
      let y = Math.abs(ratio - 1) < 1e-4 ? x : this.main.process(this.hist, n, this.w, ratio);
      if (s.sub > 0) y += s.sub * this.subShift.process(this.hist, n, this.w, ratio * 0.5);

      // Robot: ring modulation.
      if (s.ring > 0) {
        this.ringPhase += 2 * Math.PI * s.ringFreq / sr;
        if (this.ringPhase > 2 * Math.PI) this.ringPhase -= 2 * Math.PI;
        y = y * (1 - s.ring) + y * Math.sin(this.ringPhase) * s.ring * 1.6;
      }

      // Grit: downsample and quantise.
      if (down > 1) {
        if (this.holdCount++ % down === 0) this.held = y;
        y = this.held;
      }
      if (s.bits < 16) y = Math.round(y / step) * step;

      // The glitch track, then live/random glitches.
      const tSec = this.t / sr;
      while (this.nextEvent < this.events.length && this.events[this.nextEvent].time <= tSec) {
        this.start(this.events[this.nextEvent].type);
        this.nextEvent++;
      }
      if (!this.active && s.glitchRate > 0 && this.env > 0.02 && s.glitchTypes.length &&
          this.random() < s.glitchRate / sr) {
        this.start(s.glitchTypes[Math.floor(this.random() * s.glitchTypes.length)]);
      }

      // The clean processed signal is always written to history, so a stutter repeats real speech.
      this.outHist[this.w] = y;

      const a = this.active;
      if (a) {
        const edge = Math.min(1, a.i / 64, (a.len - a.i) / 64); // tiny fades: glitches should crunch, not click
        let g = y;
        switch (a.type) {
          case 'stutter': {
            const j = a.i % a.slice;
            const fade = Math.min(1, j / 48, (a.slice - j) / 48);
            g = this.outHist[(a.from + j) % n] * fade;
            break;
          }
          case 'tapestop':
            a.speed = Math.max(0, 1 - a.i / a.len);
            a.pos += a.speed * a.speed;
            g = this.readOut(a.pos) * (0.4 + 0.6 * a.speed);
            break;
          case 'dropout':
            g = 0;
            break;
          case 'static':
            g = y * 0.25 + (this.random() * 2 - 1) * 0.28 * (this.random() < 0.6 ? 1 : 0.2);
            break;
          case 'crush':
            if (a.i % 10 === 0) a.held = Math.round(y / 0.25) * 0.25;
            g = a.held;
            break;
          case 'warble':
            a.ph += 2 * Math.PI * 900 / sr;
            g = y * Math.sin(a.ph) * 1.4;
            break;
        }
        y = y * (1 - edge) + g * edge;
        if (++a.i >= a.len) this.active = null;
      }

      out[k] = y;
      this.t++;
    }
    return true;
  }
}

registerProcessor('dcc-voice-fx', DCCVoiceFx);
`;

let blobUrl = null;
/** Make sure a context has the processor registered. */
export async function ensureWorklet(ctx) {
  if (!blobUrl) blobUrl = URL.createObjectURL(new Blob([FX_WORKLET_SOURCE], { type: 'application/javascript' }));
  if (!ctx.__dccFx) {
    await ctx.audioWorklet.addModule(blobUrl);
    ctx.__dccFx = true;
  }
}
