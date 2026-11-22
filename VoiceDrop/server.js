// Voice Drop (2026-11-22): a link Zoe can send anyone, players or not, to record lines for Dungeon Crawler Denver.
// Each take is saved, Zoe is pinged (Discord and/or ntfy), and she listens, approves and ships it from /admin.
// Tools/voice_inbox.py pulls approved takes into SourceArt/Voice/NPC and writes the credits the game pays out.
//
// Settings (environment variables):
//   ADMIN_TOKEN          required: the password for /admin and the admin API
//   DATA_DIR             where takes and the list live (default ./data). On Railway, mount a volume here.
//   PUBLIC_URL           this site's address, for the links in notifications (e.g. https://voice-drop.up.railway.app)
//   DISCORD_WEBHOOK_URL  optional: post each upload to a Discord channel
//   NTFY_TOPIC           optional: push each upload to the ntfy app (https://ntfy.sh/<topic>)
//   DEFAULT_REWARD       tickets a shipped line pays, unless set per take in /admin (default 25)
//   SET_BONUS            extra tickets once every line of a character has shipped for one player (default 300)
//   MAX_MB               largest take accepted (default 15)

const express = require("express");
const multer = require("multer");
const crypto = require("crypto");
const fs = require("fs");
const path = require("path");

const PORT = process.env.PORT || 8787;
const DATA_DIR = path.resolve(process.env.DATA_DIR || path.join(__dirname, "data"));
const UPLOADS = path.join(DATA_DIR, "uploads");
const DB_FILE = path.join(DATA_DIR, "submissions.json");
const CATALOG_FILE = path.join(DATA_DIR, "catalog.json");
const BUNDLED_CATALOG = path.join(__dirname, "catalog.json");
const ADMIN_TOKEN = process.env.ADMIN_TOKEN || "";
const PUBLIC_URL = (process.env.PUBLIC_URL || "").replace(/\/$/, "");
const DEFAULT_REWARD = parseInt(process.env.DEFAULT_REWARD || "25", 10);
const SET_BONUS = parseInt(process.env.SET_BONUS || "300", 10);
const MAX_MB = parseInt(process.env.MAX_MB || "15", 10);
const STATUSES = ["new", "approved", "shipped", "rejected"];

fs.mkdirSync(UPLOADS, { recursive: true });
if (!ADMIN_TOKEN) {
  console.warn("[voice-drop] ADMIN_TOKEN is not set: /admin is locked until it is.");
}

// ---- storage: one JSON file, written whole and atomically ------------------------------------------------------
function readJson(file, fallback) {
  try {
    return JSON.parse(fs.readFileSync(file, "utf8"));
  } catch {
    return fallback;
  }
}
function writeJson(file, value) {
  const tmp = file + ".tmp";
  fs.writeFileSync(tmp, JSON.stringify(value, null, 2));
  fs.renameSync(tmp, file);
}
let submissions = readJson(DB_FILE, []);
const save = () => writeJson(DB_FILE, submissions);

function catalog() {
  return readJson(CATALOG_FILE, null) || readJson(BUNDLED_CATALOG, { title: "Voice Drop", speakers: [] });
}
function findLine(speakerId, row) {
  const speaker = catalog().speakers.find((s) => s.id === speakerId);
  const line = speaker && speaker.lines.find((l) => l.row === row);
  return speaker && line ? { speaker, line } : null;
}

// ---- notifications ------------------------------------------------------------------------------------------------
async function notify(sub) {
  const who = sub.gameName && sub.gameName !== sub.name ? `${sub.name} (plays as ${sub.gameName})` : sub.name;
  const quote = sub.text.length > 120 ? sub.text.slice(0, 117) + "..." : sub.text;
  const link = PUBLIC_URL ? `${PUBLIC_URL}/admin#${sub.id}` : "/admin";
  const message = `New voice line from ${who} as ${sub.speakerName}: "${quote}"`;
  const jobs = [];
  if (process.env.DISCORD_WEBHOOK_URL) {
    jobs.push(fetch(process.env.DISCORD_WEBHOOK_URL, {
      method: "POST",
      headers: { "Content-Type": "application/json" },
      body: JSON.stringify({ content: `${message}\nListen: ${link}`, allowed_mentions: { parse: [] } }),
    }));
  }
  if (process.env.NTFY_TOPIC) {
    jobs.push(fetch(`https://ntfy.sh/${encodeURIComponent(process.env.NTFY_TOPIC)}`, {
      method: "POST",
      headers: { Title: "Voice Drop", Click: link, Tags: "microphone" },
      body: message,
    }));
  }
  const results = await Promise.allSettled(jobs);
  results.filter((r) => r.status === "rejected").forEach((r) => console.warn("[voice-drop] notify failed:", r.reason));
  if (!jobs.length) console.log("[voice-drop]", message);
}

// ---- the app -----------------------------------------------------------------------------------------------------
const app = express();
app.disable("x-powered-by");
app.set("trust proxy", true); // Railway and friends sit in front
app.use(express.json({ limit: "2mb" }));
app.use(express.static(path.join(__dirname, "public")));
app.get("/admin", (req, res) => res.sendFile(path.join(__dirname, "public", "admin.html")));
app.get("/healthz", (req, res) => res.json({ ok: true }));

app.get("/api/catalog", (req, res) => {
  const c = catalog();
  res.json({ title: c.title || "Voice Drop", intro: c.intro || "", reward: DEFAULT_REWARD, setBonus: SET_BONUS,
    speakers: c.speakers.filter((s) => s.open !== false) });
});

// Up to 40 takes an hour from one address: plenty for a session, not enough to fill the disk.
const recent = new Map();
function rateLimited(ip) {
  const now = Date.now();
  const list = (recent.get(ip) || []).filter((t) => now - t < 3600 * 1000);
  list.push(now);
  recent.set(ip, list);
  return list.length > 40;
}

const EXTENSIONS = { "audio/webm": ".webm", "video/webm": ".webm", "audio/ogg": ".ogg", "audio/mpeg": ".mp3", "audio/mp3": ".mp3",
  "audio/mp4": ".m4a", "audio/x-m4a": ".m4a", "audio/aac": ".aac", "audio/wav": ".wav", "audio/x-wav": ".wav", "audio/wave": ".wav",
  "audio/flac": ".flac", "audio/x-flac": ".flac", "video/mp4": ".m4a" };
const upload = multer({
  storage: multer.diskStorage({
    destination: UPLOADS,
    filename: (req, file, cb) => cb(null, crypto.randomBytes(12).toString("hex") + (EXTENSIONS[file.mimetype.split(";")[0]] || ".bin")),
  }),
  limits: { fileSize: MAX_MB * 1024 * 1024, files: 1 },
  fileFilter: (req, file, cb) => cb(null, Boolean(EXTENSIONS[file.mimetype.split(";")[0]])),
});

app.post("/api/submit", (req, res) => {
  if (rateLimited(req.ip)) {
    return res.status(429).json({ error: "That's a lot of takes in an hour. Take a breather and try again soon." });
  }
  upload.single("audio")(req, res, async (err) => {
    if (err) {
      const big = err.code === "LIMIT_FILE_SIZE";
      return res.status(400).json({ error: big ? `That file is over ${MAX_MB} MB.` : "That upload didn't work." });
    }
    const drop = () => req.file && fs.rm(req.file.path, () => {});
    if (!req.file) return res.status(400).json({ error: "No recording came through. Use mp3, m4a, wav, ogg, webm or flac." });
    const field = (k, max) => String(req.body[k] || "").trim().slice(0, max);
    const name = field("name", 60);
    const found = findLine(field("speaker", 60), field("row", 60));
    if (!name) { drop(); return res.status(400).json({ error: "Tell us your name first." }); }
    if (!found) { drop(); return res.status(400).json({ error: "That line isn't on the list any more." }); }
    if (req.body.consent !== "yes") { drop(); return res.status(400).json({ error: "Tick the box saying we can use it in the game." }); }
    const sub = {
      id: crypto.randomBytes(6).toString("hex"),
      createdAt: new Date().toISOString(),
      name,
      gameName: field("gameName", 60),
      contact: field("contact", 120),
      note: field("note", 500),
      speaker: found.speaker.id,
      speakerName: found.speaker.name,
      row: found.line.row,
      text: found.line.text,
      file: path.basename(req.file.path),
      mime: req.file.mimetype,
      size: req.file.size,
      status: "new",
      reward: DEFAULT_REWARD,
      adminNote: "",
    };
    submissions.push(sub);
    save();
    res.json({ ok: true, id: sub.id });
    notify(sub).catch((e) => console.warn("[voice-drop] notify failed:", e));
  });
});

// ---- admin -------------------------------------------------------------------------------------------------------
function admin(req, res, next) {
  const given = String(req.get("x-admin-token") || req.query.token || "");
  const ok = ADMIN_TOKEN && given.length === ADMIN_TOKEN.length &&
    crypto.timingSafeEqual(Buffer.from(given), Buffer.from(ADMIN_TOKEN));
  if (!ok) return res.status(401).json({ error: "Wrong admin password." });
  next();
}

app.get("/api/admin/submissions", admin, (req, res) => {
  const status = String(req.query.status || "");
  const list = status && status !== "all" ? submissions.filter((s) => s.status === status) : submissions;
  res.json([...list].reverse());
});

app.get("/api/admin/audio/:id", admin, (req, res) => {
  const sub = submissions.find((s) => s.id === req.params.id);
  if (!sub) return res.status(404).end();
  res.type(sub.mime.split(";")[0]);
  if (req.query.download) res.attachment(`${sub.row}_${sub.name.replace(/[^\w-]+/g, "_")}${path.extname(sub.file)}`);
  res.sendFile(path.join(UPLOADS, sub.file));
});

app.post("/api/admin/submissions/:id", admin, (req, res) => {
  const sub = submissions.find((s) => s.id === req.params.id);
  if (!sub) return res.status(404).json({ error: "Not found." });
  const b = req.body || {};
  if (b.status !== undefined) {
    if (!STATUSES.includes(b.status)) return res.status(400).json({ error: "Unknown status." });
    sub.status = b.status;
    if (b.status === "shipped") sub.shippedAt = new Date().toISOString();
  }
  if (b.reward !== undefined) sub.reward = Math.max(0, Math.min(1000, parseInt(b.reward, 10) || 0));
  if (b.gameName !== undefined) sub.gameName = String(b.gameName).trim().slice(0, 60);
  if (b.adminNote !== undefined) sub.adminNote = String(b.adminNote).slice(0, 500);
  save();
  res.json(sub);
});

app.delete("/api/admin/submissions/:id", admin, (req, res) => {
  const i = submissions.findIndex((s) => s.id === req.params.id);
  if (i < 0) return res.status(404).end();
  fs.rm(path.join(UPLOADS, submissions[i].file), () => {});
  submissions.splice(i, 1);
  save();
  res.json({ ok: true });
});

app.get("/api/admin/catalog", admin, (req, res) => res.json(catalog()));
app.post("/api/admin/catalog", admin, (req, res) => {
  const c = req.body;
  if (!c || !Array.isArray(c.speakers)) return res.status(400).json({ error: "A catalog needs a speakers list." });
  writeJson(CATALOG_FILE, c);
  res.json({ ok: true, speakers: c.speakers.length, lines: c.speakers.reduce((n, s) => n + (s.lines || []).length, 0) });
});

app.listen(PORT, () => console.log(`[voice-drop] listening on ${PORT}, data in ${DATA_DIR}`));
