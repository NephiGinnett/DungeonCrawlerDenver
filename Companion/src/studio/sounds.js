// The System's own noises, synthesised from scratch so they cost no assets and match any voice setting.
//
//   bootSound      the intro: static crackle, data chirps, a rising sweep, then a "lock-on" ping
//   shutdownSound  the outro: a falling, crushed sweep into static and a final click
//   glitchSting    a standalone burst, for a button press with nothing else playing

function buffer(sr, seconds) {
  return new AudioBuffer({ length: Math.round(sr * seconds), numberOfChannels: 1, sampleRate: sr });
}

function rng(seed) {
  let x = seed | 0 || 1;
  return () => {
    x ^= x << 13; x ^= x >>> 17; x ^= x << 5;
    return ((x >>> 0) % 100000) / 100000;
  };
}

const crush = (v, bits) => {
  const step = 2 / Math.pow(2, bits);
  return Math.round(v / step) * step;
};

export function bootSound(sr) {
  const buf = buffer(sr, 1.15);
  const d = buf.getChannelData(0);
  const rand = rng(1234);
  const len = d.length;

  // Static crackle that fades out over the first half.
  for (let i = 0; i < len * 0.55; i++) {
    const t = i / (len * 0.55);
    if (rand() < 0.35) d[i] += (rand() * 2 - 1) * 0.22 * (1 - t);
  }

  // Data chirps: short square blips at random pitches, like a modem clearing its throat.
  let i = Math.round(sr * 0.04);
  while (i < len * 0.62) {
    const blip = Math.round(sr * (0.018 + rand() * 0.035));
    const f = 700 + rand() * 2600;
    for (let j = 0; j < blip && i + j < len; j++) {
      const env = Math.min(1, j / 40, (blip - j) / 40);
      d[i + j] += (Math.sin((2 * Math.PI * f * j) / sr) > 0 ? 1 : -1) * 0.12 * env;
    }
    i += blip + Math.round(sr * rand() * 0.04);
  }

  // Rising sweep with a tremolo, bit-crushed: the system powering up.
  const s0 = Math.round(len * 0.28), s1 = Math.round(len * 0.86);
  let ph = 0;
  for (let k = s0; k < s1; k++) {
    const t = (k - s0) / (s1 - s0);
    const f = 110 * Math.pow(9, t);
    ph += (2 * Math.PI * f) / sr;
    const trem = 0.6 + 0.4 * Math.sin((2 * Math.PI * 18 * k) / sr);
    d[k] += crush(Math.sin(ph) * 0.3 * t * trem, 6);
  }

  // Lock-on ping.
  const p0 = Math.round(len * 0.86);
  for (let k = p0; k < len; k++) {
    const t = (k - p0) / sr;
    d[k] += Math.sin(2 * Math.PI * 1320 * t) * 0.32 * Math.exp(-t * 14) + Math.sin(2 * Math.PI * 1980 * t) * 0.12 * Math.exp(-t * 20);
  }
  return buf;
}

export function shutdownSound(sr) {
  const buf = buffer(sr, 1.3);
  const d = buf.getChannelData(0);
  const rand = rng(4321);
  const len = d.length;

  // Falling sweep, getting more crushed as it dies.
  let ph = 0;
  const e0 = Math.round(len * 0.75);
  for (let k = 0; k < e0; k++) {
    const t = k / e0;
    const f = 900 * Math.pow(0.06, t);
    ph += (2 * Math.PI * f) / sr;
    d[k] += crush(Math.sin(ph) * 0.3 * (1 - t * 0.6), Math.max(2, Math.round(8 - t * 6)));
  }

  // Static that swells, then cuts.
  for (let k = Math.round(len * 0.35); k < Math.round(len * 0.9); k++) {
    const t = (k - len * 0.35) / (len * 0.55);
    if (rand() < 0.5) d[k] += (rand() * 2 - 1) * 0.18 * Math.sin(Math.PI * t);
  }

  // Final relay click.
  const c0 = Math.round(len * 0.9);
  for (let k = c0; k < Math.min(len, c0 + Math.round(sr * 0.012)); k++) {
    d[k] += (rand() * 2 - 1) * 0.5 * (1 - (k - c0) / (sr * 0.012));
  }
  return buf;
}

export function glitchSting(sr) {
  const buf = buffer(sr, 0.35);
  const d = buf.getChannelData(0);
  const rand = rng(Math.floor(Math.random() * 1e6) + 1);
  for (let k = 0; k < d.length; k++) {
    const t = k / d.length;
    const gate = Math.floor(k / (sr * 0.02)) % 3 !== 1 ? 1 : 0;
    d[k] = crush((rand() * 2 - 1) * 0.3 * gate * (1 - t) + Math.sin((2 * Math.PI * (2000 - 1500 * t) * k) / sr) * 0.12 * gate, 4);
  }
  return buf;
}
