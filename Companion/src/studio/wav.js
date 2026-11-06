// 16-bit PCM mono WAV, which is what UE imports as a SoundWave without complaint.

export function encodeWav(buffer) {
  const data = buffer.getChannelData(0);
  const sr = buffer.sampleRate;
  const bytes = new ArrayBuffer(44 + data.length * 2);
  const v = new DataView(bytes);
  const str = (o, s) => { for (let i = 0; i < s.length; i++) v.setUint8(o + i, s.charCodeAt(i)); };
  str(0, 'RIFF');
  v.setUint32(4, 36 + data.length * 2, true);
  str(8, 'WAVE');
  str(12, 'fmt ');
  v.setUint32(16, 16, true);
  v.setUint16(20, 1, true);      // PCM
  v.setUint16(22, 1, true);      // mono
  v.setUint32(24, sr, true);
  v.setUint32(28, sr * 2, true);
  v.setUint16(32, 2, true);
  v.setUint16(34, 16, true);
  str(36, 'data');
  v.setUint32(40, data.length * 2, true);
  let o = 44;
  for (let i = 0; i < data.length; i++, o += 2) {
    const s = Math.max(-1, Math.min(1, data[i]));
    v.setInt16(o, s < 0 ? s * 0x8000 : s * 0x7fff, true);
  }
  return bytes;
}
