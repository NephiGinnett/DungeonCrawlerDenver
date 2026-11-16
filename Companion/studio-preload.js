// The Voice Studio's only access to the disk: the System AI's folder of finished lines, one folder per
// Fort Tiner speaker (SourceArt/Voice/NPC/<Speaker>), and one folder of raw takes.
const { contextBridge, ipcRenderer } = require('electron');

contextBridge.exposeInMainWorld('studio', {
  voiceFolder: 'SourceArt/Voice/SystemAI/zoe',
  saveVoice: (name, wav, folder) => ipcRenderer.invoke('studio-save', 'voice', name, wav, folder),
  saveRaw: (row, wav) => ipcRenderer.invoke('studio-save', 'raw', `VO_${row}_raw.wav`, wav),
  listRaw: () => ipcRenderer.invoke('studio-list-raw'),
  loadRaw: (name) => ipcRenderer.invoke('studio-load-raw', name),
  hasVoice: (name, folder) => ipcRenderer.invoke('studio-has-voice', name, folder),
  loadNpcLines: () => ipcRenderer.invoke('studio-npc-load'),
  saveNpcLines: (data) => ipcRenderer.invoke('studio-npc-save', data),
});
