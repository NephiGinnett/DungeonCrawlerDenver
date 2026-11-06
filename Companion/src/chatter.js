// The audience, scrolling behind your crawler.
//
// Same table and same rules as the game's Chatter tab (Source/.../UI/DCCChatterTypes.h, ADCCGameState's roller):
// ~5% each for Sentiment, Equipment, Boss Battle, Appearance, Emote and Alien Language; running gags that come
// back for a while once they've been rolled; a small chance of a moderation notice; Benign filler for the rest.
//
// For now the lines are rolled here, from the game's own CSV, because nothing connects the Companion to a running
// game yet. When the Table relay exists (Docs/ROADMAP.md §5), the game's real chatter arrives through push() and
// the local roller switches off; the display does not change.

const COLOURS = ['#ff6a3d', '#48d8ff', '#c77dff', '#7dff9a', '#ffd23d', '#ff7dd1', '#7d9bff', '#ff9a7d'];
const BUCKET_ODDS = [
  ['Sentiment', 0.05], ['Equipment', 0.05], ['BossBattle', 0.05],
  ['Appearance', 0.05], ['Emote', 0.05], ['AlienLanguage', 0.05], ['RunningGag', 0.08],
];

// What chat calls each action when it reacts to it ({Event} in the templates).
const EVENT_NAMES = {
  cast: 'that spell', attack: 'that swing', shoot: 'that shot', spin: 'the spin jump', dance1: 'the dance break',
  dance2: 'the all-nighter', dance3: 'the cardio', workout: 'the squats', practice: 'the shadowboxing',
  climb: 'the climb', run: 'the sprint', stance: 'the stance', agree: 'the nod', look: 'the look around',
};

const pick = (arr) => arr[Math.floor(Math.random() * arr.length)];
const hashColour = (s) => COLOURS[[...s].reduce((h, c) => (h * 31 + c.charCodeAt(0)) >>> 0, 7) % COLOURS.length];

export function createChatter(container, { name, items = () => [] }) {
  let lines = [];
  let byBucket = new Map();
  let activeGag = null; // { id, until }
  let lastEvent = 'the intro';
  let timer = null;
  let running = false;
  let live = false; // true once real chatter from the game is being pushed in

  const list = document.createElement('div');
  list.className = 'chatter-list';
  container.append(list);

  function fill(template) {
    const item = pick(items()) || 'that thing';
    return template.replaceAll('{Name}', name).replaceAll('{Item}', item).replaceAll('{Event}', lastEvent);
  }

  function roll() {
    if (!lines.length) return null;
    if (activeGag && Date.now() < activeGag.until && Math.random() < 0.35) {
      const gagLines = lines.filter((l) => l.gag === activeGag.id);
      if (gagLines.length) return pick(gagLines);
    }
    if (Math.random() < 0.02 && byBucket.get('Moderated')?.length) return pick(byBucket.get('Moderated'));
    let r = Math.random();
    for (const [bucket, odds] of BUCKET_ODDS) {
      if ((r -= odds) < 0 && byBucket.get(bucket)?.length) {
        const line = pick(byBucket.get(bucket));
        if (line.gag) activeGag = { id: line.gag, until: Date.now() + 90_000 };
        return line;
      }
    }
    return pick(byBucket.get('Benign') || lines);
  }

  function push(handle, text) {
    const row = document.createElement('div');
    row.className = 'chatter-line';
    const who = document.createElement('b');
    who.textContent = handle;
    who.style.color = hashColour(handle);
    row.append(who, document.createTextNode(` ${text}`));
    list.append(row);
    while (list.children.length > 40) list.firstChild.remove();
  }

  function postOne() {
    const line = roll();
    if (line) push(line.handle || 'anonymous', fill(line.template));
  }

  function schedule() {
    clearTimeout(timer);
    if (!running || live) return;
    timer = setTimeout(() => { postOne(); schedule(); }, 1400 + Math.random() * 2600);
  }

  return {
    async load() {
      lines = await window.companion.chatterLines();
      byBucket = new Map();
      for (const l of lines) {
        if (!byBucket.has(l.bucket)) byBucket.set(l.bucket, []);
        byBucket.get(l.bucket).push(l);
      }
    },
    start() {
      if (running) return;
      running = true;
      container.classList.add('on');
      for (let i = 0; i < 6; i++) postOne(); // backfill, so it looks like it was already running
      schedule();
    },
    stop() {
      running = false;
      clearTimeout(timer);
      container.classList.remove('on');
    },
    /** Something just happened on screen: chat reacts with a quick burst. */
    react(actionId) {
      lastEvent = EVENT_NAMES[actionId] || lastEvent;
      if (!running || live) return;
      const burst = 2 + Math.floor(Math.random() * 3);
      for (let i = 0; i < burst; i++) setTimeout(postOne, 150 + i * 260);
    },
    /** For the future live link: a real line from the game. The first one switches the local roller off. */
    pushLive(handle, text) {
      live = true;
      clearTimeout(timer);
      push(handle, text);
    },
    isRunning: () => running,
  };
}
