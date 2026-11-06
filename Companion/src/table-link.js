// The Companion's connection to the Table (server/table-server.js). Reconnects on its own: a Wi-Fi blip or the
// DM restarting the server should not need anyone to click anything.

export function createTableLink(handlers = {}) {
  let ws = null;
  let address = '';
  let hello = null;
  let wanted = false;
  let retry = 0;
  let timer = null;
  let status = 'off';

  const setStatus = (s, detail = '') => {
    status = s;
    handlers.onStatus?.(s, detail);
  };

  function url(addr) {
    const a = addr.trim();
    if (/^wss?:\/\//.test(a)) return a;
    return `ws://${a.includes(':') ? a : `${a}:8770`}`;
  }

  function open() {
    clearTimeout(timer);
    if (!wanted || !address) return;
    setStatus('connecting');
    try {
      ws = new WebSocket(url(address));
    } catch (err) {
      setStatus('error', String(err.message || err));
      return;
    }
    ws.onopen = () => {
      retry = 0;
      ws.send(JSON.stringify({ type: 'hello', ...hello }));
    };
    ws.onmessage = (e) => {
      let m;
      try { m = JSON.parse(e.data); } catch { return; }
      if (m.type === 'welcome') setStatus('on', m.you.role);
      if (m.type === 'error') setStatus('error', m.message);
      handlers[`on_${m.type}`]?.(m);
    };
    ws.onclose = () => {
      ws = null;
      if (!wanted) { setStatus('off'); return; }
      setStatus('connecting', 'reconnecting');
      retry = Math.min(retry + 1, 6);
      timer = setTimeout(open, 1000 * 2 ** retry);
    };
    ws.onerror = () => { /* onclose follows and handles it */ };
  }

  return {
    connect(addr, helloMsg) {
      address = addr;
      hello = helloMsg;
      wanted = true;
      retry = 0;
      if (ws) ws.close(); else open();
    },
    disconnect() {
      wanted = false;
      clearTimeout(timer);
      if (ws) ws.close();
      setStatus('off');
    },
    send(msg) {
      if (ws && ws.readyState === WebSocket.OPEN) ws.send(JSON.stringify(msg));
    },
    status: () => status,
  };
}
