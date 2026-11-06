const { contextBridge, ipcRenderer } = require('electron');

contextBridge.exposeInMainWorld('companion', {
  roster: () => ipcRenderer.invoke('roster'),
  chatterLines: () => ipcRenderer.invoke('chatter-lines'),
  login: (id, pin) => ipcRenderer.invoke('login', id, pin),
  logout: () => ipcRenderer.invoke('logout'),
  whoami: () => ipcRenderer.invoke('whoami'),
  loadSheet: (id) => ipcRenderer.invoke('load-sheet', id),
  saveSheet: (id, sheet) => ipcRenderer.invoke('save-sheet', id, sheet),

  openBuddy: () => ipcRenderer.invoke('open-buddy'),
  closeBuddy: () => ipcRenderer.invoke('close-buddy'),
  buddyBounds: () => ipcRenderer.invoke('buddy-bounds'),
  buddyMove: (x) => ipcRenderer.send('buddy-move', x),
  buddyDrag: (dx, dy) => ipcRenderer.send('buddy-drag', dx, dy),
  buddyAction: (id) => ipcRenderer.send('buddy-action', id),

  on: (channel, fn) => {
    const allowed = ['buddy-closed', 'sheet-changed', 'buddy-action'];
    if (allowed.includes(channel)) ipcRenderer.on(channel, (_e, ...args) => fn(...args));
  },
});
