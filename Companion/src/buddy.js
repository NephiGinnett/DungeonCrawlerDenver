// The desktop buddy: your crawler, loose on your desktop.
//
// A tiny behaviour loop, nothing clever: stand around, then pick something to do. Sometimes that is a walk or a
// run to another spot along the bottom of the screen (the window itself moves), sometimes a workout, practice,
// a spell at nothing, a dance. Clicking the sheet's action buttons overrides whatever they were doing.

import { createViewer } from './viewer.js';
import { attachWheel, groupActions } from './wheel.js';

const bubble = document.getElementById('bubble');
const canvas = document.getElementById('buddy-canvas');
const viewer = createViewer(canvas, { transparent: true, groundGlow: false, interactive: false });

const LINES = {
  workout: ['Leg day. Every day is leg day down here.', 'Strength score goes up. Allegedly.'],
  practice: ['Practising. The System rolls for it later.', 'Skill marked.'],
  cast: ['Casting! Mana spent whether it works or not.', 'That was a spell. Probably.'],
  dance1: ['The audience loves this.', 'AI Favor +0. They are watching though.'],
  dance2: ['Chat is spamming emotes.'],
  dance3: ['Cardio counts as training, right?'],
  spin: ['Did you see that? Nobody saw that.'],
  idle: [],
};

let available = [];
let busyUntil = 0;
let moving = null; // { targetX, speed, dir }
let dragging = false;
let lastScreen = null;
let groups = [];
let rosterActions = [];

// Middle-click drag on the buddy: the same action wheel as the sheet. Wandering pauses while it is open.
const wheel = attachWheel(canvas, {
  getGroups: () => groupActions(groups, rosterActions.filter((a) => has(a.id)), (id) => doAction(id, 8)),
  onOpen: () => { moving = null; busyUntil = Infinity; },
  onClose: () => { if (busyUntil === Infinity) busyUntil = performance.now() + 4000; },
});

function say(id) {
  const options = LINES[id];
  if (!options || !options.length || Math.random() < 0.5) return;
  bubble.textContent = options[Math.floor(Math.random() * options.length)];
  bubble.classList.add('show');
  clearTimeout(say._t);
  say._t = setTimeout(() => bubble.classList.remove('show'), 3000);
}

function has(id) {
  return available.some((a) => a.id === id);
}

function doAction(id, seconds = 4 + Math.random() * 4) {
  if (!viewer.play(id)) return false;
  viewer.face(0);
  moving = null;
  busyUntil = performance.now() + seconds * 1000;
  say(id);
  return true;
}

async function wander() {
  const b = await window.companion.buddyBounds();
  if (!b) return;
  const minX = b.area.x;
  const maxX = b.area.x + b.area.width - b.width;
  const targetX = minX + Math.random() * (maxX - minX);
  const run = has('run') && Math.random() < 0.4;
  const id = run ? 'run' : has('walk') ? 'walk' : null;
  if (!id) return;
  viewer.play(id);
  const dir = targetX > b.x ? 1 : -1;
  viewer.face(dir);
  moving = { x: b.x, targetX, dir, speed: run ? 260 : 110 };
  busyUntil = Infinity;
}

function pickSomething() {
  const roll = Math.random();
  if (roll < 0.35) return wander();
  const pool = ['workout', 'practice', 'cast', 'stance', 'agree', 'spin', 'dance1', 'dance2', 'dance3'].filter(has);
  if (roll < 0.55 || !pool.length) return doAction('idle', 5 + Math.random() * 6);
  doAction(pool[Math.floor(Math.random() * pool.length)]);
}

let last = performance.now();
function tick(now) {
  requestAnimationFrame(tick);
  const dt = Math.min(0.1, (now - last) / 1000);
  last = now;
  if (dragging) return;

  if (moving) {
    moving.x += moving.dir * moving.speed * dt;
    window.companion.buddyMove(moving.x);
    if ((moving.dir > 0 && moving.x >= moving.targetX) || (moving.dir < 0 && moving.x <= moving.targetX)) {
      moving = null;
      doAction('idle', 3 + Math.random() * 4);
    }
    return;
  }

  if (now > busyUntil) pickSomething();
}

// Drag the buddy around by hand.
canvas.addEventListener('pointerdown', (e) => {
  if (e.button !== 0 || wheel.isOpen()) return; // middle button opens the wheel
  dragging = true;
  moving = null;
  lastScreen = { x: e.screenX, y: e.screenY };
  canvas.setPointerCapture(e.pointerId);
});
canvas.addEventListener('pointermove', (e) => {
  if (!dragging) return;
  window.companion.buddyDrag(e.screenX - lastScreen.x, e.screenY - lastScreen.y);
  lastScreen = { x: e.screenX, y: e.screenY };
});
canvas.addEventListener('pointerup', () => {
  dragging = false;
  doAction('idle', 3);
});

document.getElementById('close').addEventListener('click', () => window.companion.closeBuddy());

window.companion.on('buddy-action', (id) => doAction(id, 6));

(async () => {
  const roster = await window.companion.roster();
  const id = await window.companion.whoami();
  const me = roster.crawlers.find((c) => c.id === id);
  await viewer.load(me?.modelUrl || '', roster.animationLibrary);
  available = viewer.setActions(roster.actions);
  rosterActions = roster.actions;
  groups = roster.groups || [];
  doAction('idle', 3);
  requestAnimationFrame(tick);
})();
