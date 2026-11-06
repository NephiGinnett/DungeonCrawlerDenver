// The action wheel: the same middle-button gesture as The Familiar's emote wheel.
//
// Press the middle button over the crawler and the wheel opens around the cursor. Drag over a category to open
// it, drag onto a move and let go to do it. Let go on a category and the wheel stays open there, for ordinary
// clicking; let go anywhere else and it closes. Esc closes it too.

const STYLE = `
.dcc-wheel { position: fixed; inset: 0; z-index: 50; pointer-events: none; font-family: "Bahnschrift", "Segoe UI", sans-serif; }
.dcc-wheel .ring { position: absolute; width: 0; height: 0; }
.dcc-wheel .ring::before {
  content: ""; position: absolute; left: calc(-1 * var(--r)); top: calc(-1 * var(--r));
  width: calc(2 * var(--r)); height: calc(2 * var(--r)); border-radius: 50%;
  border: 1.5px solid rgba(72, 216, 255, 0.55); box-shadow: 0 0 24px rgba(72, 216, 255, 0.35), inset 0 0 30px rgba(72, 216, 255, 0.12);
  background: radial-gradient(circle, rgba(3, 10, 26, 0.88) 0 58%, rgba(3, 10, 26, 0.55) 72%, transparent 73%);
  animation: dcc-wheel-in 0.16s ease-out;
}
@keyframes dcc-wheel-in { from { transform: scale(0.6); opacity: 0; } to { transform: scale(1); opacity: 1; } }
.dcc-wheel .cat {
  position: absolute; width: 46px; height: 46px; margin: -23px 0 0 -23px; border-radius: 50%; pointer-events: auto;
  background: rgba(3, 10, 26, 0.95); border: 1.5px solid rgba(72, 216, 255, 0.6); color: #48d8ff; font-size: 20px;
  cursor: pointer; display: grid; place-items: center; transition: transform 0.1s, box-shadow 0.1s;
}
.dcc-wheel .cat .n { position: absolute; top: 46px; white-space: nowrap; font-size: 10px; letter-spacing: 0.12em; text-transform: uppercase; color: #dff1ff; text-shadow: 0 0 4px #000; }
.dcc-wheel .cat.on, .dcc-wheel .cat:hover { transform: scale(1.15); background: #48d8ff; color: #021020; box-shadow: 0 0 16px rgba(72, 216, 255, 0.8); }
.dcc-wheel .items { position: absolute; transform: translate(-50%, -50%); display: flex; flex-direction: column; gap: 3px; pointer-events: auto; max-height: 150px; overflow-y: auto; padding-right: 2px; }
.dcc-wheel .item {
  font: 600 11px/1 "Bahnschrift", "Segoe UI", sans-serif; letter-spacing: 0.08em; text-transform: uppercase;
  min-width: 96px; padding: 5px 10px; color: #dff1ff; background: rgba(72, 216, 255, 0.1);
  border: 1px solid rgba(72, 216, 255, 0.4); cursor: pointer;
}
.dcc-wheel .item.hot, .dcc-wheel .item:hover { background: #48d8ff; color: #021020; }
.dcc-wheel .hint { position: absolute; transform: translate(-50%, -50%); color: #7f95b3; font-size: 10px; letter-spacing: 0.12em; text-transform: uppercase; text-align: center; width: 110px; }
`;

export function attachWheel(target, { getGroups, onOpen, onClose } = {}) {
  if (!document.getElementById('dcc-wheel-style')) {
    const s = document.createElement('style');
    s.id = 'dcc-wheel-style';
    s.textContent = STYLE;
    document.head.append(s);
  }

  const root = document.createElement('div');
  root.className = 'dcc-wheel';
  root.hidden = true;
  document.body.append(root);

  let open = false;
  let dragging = false;
  let centre = { x: 0, y: 0 };
  let activeCat = null;

  function radius() {
    return Math.max(70, Math.min(108, Math.min(window.innerWidth, window.innerHeight) / 2 - 36));
  }

  function render() {
    const groups = getGroups().filter((g) => g.items.length);
    const R = radius();
    root.replaceChildren();
    const ring = document.createElement('div');
    ring.className = 'ring';
    ring.style.left = `${centre.x}px`;
    ring.style.top = `${centre.y}px`;
    ring.style.setProperty('--r', `${R + 26}px`);
    root.append(ring);

    groups.forEach((g, i) => {
      const a = -Math.PI / 2 + (i / groups.length) * Math.PI * 2;
      const b = document.createElement('button');
      b.className = `cat${g.id === activeCat ? ' on' : ''}`;
      b.dataset.cat = g.id;
      b.style.left = `${centre.x + Math.cos(a) * R}px`;
      b.style.top = `${centre.y + Math.sin(a) * R}px`;
      b.textContent = g.icon;
      const n = document.createElement('span');
      n.className = 'n';
      n.textContent = g.name;
      b.append(n);
      b.addEventListener('click', (e) => {
        e.stopPropagation();
        activeCat = g.id === activeCat ? null : g.id;
        render();
      });
      root.append(b);
    });

    const cat = groups.find((g) => g.id === activeCat);
    if (!cat) {
      const hint = document.createElement('div');
      hint.className = 'hint';
      hint.style.left = `${centre.x}px`;
      hint.style.top = `${centre.y}px`;
      hint.textContent = 'Drag to a category';
      root.append(hint);
      return;
    }
    const list = document.createElement('div');
    list.className = 'items';
    list.style.left = `${centre.x}px`;
    list.style.top = `${centre.y}px`;
    for (const it of cat.items) {
      const b = document.createElement('button');
      b.className = 'item';
      b.textContent = it.label;
      b.addEventListener('click', (e) => {
        e.stopPropagation();
        it.run();
        close();
      });
      list.append(b);
    }
    root.append(list);
  }

  function show(x, y) {
    const R = radius() + 30;
    centre = {
      x: Math.min(Math.max(x, R), window.innerWidth - R),
      y: Math.min(Math.max(y, R), window.innerHeight - R),
    };
    activeCat = null;
    open = true;
    root.hidden = false;
    render();
    onOpen?.();
  }

  function close() {
    if (!open) return;
    open = false;
    dragging = false;
    root.hidden = true;
    root.replaceChildren();
    onClose?.();
  }

  target.addEventListener('mousedown', (e) => {
    if (e.button !== 1) return;
    e.preventDefault();
    e.stopPropagation();
    dragging = true;
    show(e.clientX, e.clientY);
  }, true);

  window.addEventListener('mousemove', (e) => {
    if (!dragging) return;
    const el = document.elementFromPoint(e.clientX, e.clientY);
    const cat = el?.closest?.('.dcc-wheel .cat');
    if (cat?.dataset.cat && cat.dataset.cat !== activeCat) {
      activeCat = cat.dataset.cat;
      render();
    }
    for (const b of root.querySelectorAll('.item.hot')) b.classList.remove('hot');
    el?.closest?.('.dcc-wheel .item')?.classList.add('hot');
  });

  window.addEventListener('mouseup', (e) => {
    if (e.button !== 1 || !dragging) return;
    e.preventDefault();
    dragging = false;
    const el = document.elementFromPoint(e.clientX, e.clientY);
    const item = el?.closest?.('.dcc-wheel .item');
    const cat = el?.closest?.('.dcc-wheel .cat');
    if (item) item.click();
    else if (!cat) close();
  }, true);

  // A left click outside an open wheel dismisses it.
  window.addEventListener('mousedown', (e) => {
    if (open && !dragging && e.button === 0 && !e.target.closest?.('.dcc-wheel .cat, .dcc-wheel .item')) close();
  });
  window.addEventListener('auxclick', (e) => { if (e.button === 1) e.preventDefault(); }, true); // no autoscroll
  window.addEventListener('keydown', (e) => { if (e.key === 'Escape') close(); });

  return { show, close, isOpen: () => open };
}

/** Roster actions grouped into wheel categories. */
export function groupActions(groups, actions, run) {
  return (groups || []).map((g) => ({
    ...g,
    items: actions.filter((a) => a.group === g.id).map((a) => ({ label: a.label, run: () => run(a.id) })),
  }));
}
