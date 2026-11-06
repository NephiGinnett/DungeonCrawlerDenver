// The Voice Studio's only access to the disk: one folder of finished lines, one folder of raw takes.
const { contextBridge, ipcRenderer } = require('electron');

contextBridge.exposeInMainWorld('studio', {
  voiceFolder: 'SourceArt/Voice/SystemAI/zoe',
  saveVoice: (name, wav) => ipcRenderer.invoke('studio-save', 'voice', name, wav),
  saveRaw: (row, wav) => ipcRenderer.invoke('studio-save', 'raw', `VO_${row}_raw.wav`, wav),
  listRaw: () => ipcRenderer.invoke('studio-list-raw'),
  loadRaw: (name) => ipcRenderer.invoke('studio-load-raw', name),
  hasVoice: (name) => ipcRenderer.invoke('studio-has-voice', name),
});
