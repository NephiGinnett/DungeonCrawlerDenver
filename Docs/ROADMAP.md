# Dungeon Crawler Denver: Roadmap and Progress Tracker

**This is the file to read first when coming back to the project.** It holds where we are, what is owed,
and where we're going. CLAUDE.md holds how things work. Add a dated line to the progress log at the end of
every session.

Started 2026-11-05, when we came back after two months away and agreed the project had got "lost in the
sauce" of map building. The fix is to build **a game loop people can play start to finish**, the way Elyra's
Tales already has one, and to stop adding geography until that loop exists.

---

## 1. The vision, in three products

| # | Product | What it is | Status |
|---|---|---|---|
| 1 | **The game** (UE 5.8) | 5-player co-op crawler. Wake up at home, the World Dungeon opens, fight alone, regroup in the safe room, descend. | Systems are deep. The loop was missing; it's being built now (§3). |
| 2 | **Crawler Companion** (`Companion/`) | Desktop app: log in as your crawler, DCC-format character sheet, your 3D model doing basic actions, and an optional desktop buddy. | **v0.1 working** (§4). |
| 3 | **The Table** (future) | A Foundry/Roll20-style top-down page that shows every crawler's token moving live as the game plays. | Planned (§5). Architecture choices made now so it stays possible. |

Later: a hosted **brain** (a local LLM on Zoe's PC or a server) that generates items, classes and flavour.
That's deliberately out of scope until the loop is fun without it. See §6.

---

## 2. What we learned from Elyra's Tales (why that game works and this one didn't yet)

Elyra's Tales (`Downloads\TheFamiliar\game`, Godot) has a game loop. This project had systems. What
Tales does that we should copy:

1. **One clear spine.** Go down, and every fifth floor you surface in a town. You always know the next step.
2. **Short runs that pay.** One tale a day pays in full. A run is a session, not an evening.
3. **Gear you can see.** Wand, robe and charm, always on screen, with upgrades from chests and bosses.
4. **Companions who fight beside you**, each with one recognisable trick.
5. **Hubs with people in them.** The towns have shops, missions boards and people to talk to.
6. **Little minigames for variety**: sparring (parry left/right/overhead), the rank trial (a 30-second timed
   arena), chests that bite.
7. **Failure is forgiven, by the narrator.** "She rewrites the story to save you, 1 to 3 times a tale."
   That maps exactly onto DCC's **AI Favor** (a reroll or an extra action from the audience).

What it means here: **Floor 1 becomes a 30-minute run with a hub.** Safe room, then 3 to 4 rooms, then a
boss, then a loot box, then back to the safe room. The towns (Tailings) and the mine are content for
later floors. They don't come first.

---

## 3. The game: the first ten minutes (being built now)

The flow Zoe asked for, beat by beat, and the piece of code that does each one:

| Beat | What the player sees | Built by | State |
|---|---|---|---|
| Wake up | Their own bedroom, decorated as theirs. **No HUD at all.** | `ADCCOnboardingSettings` (StartingFeatures = 0) + bedroom PlayerStarts tagged `CJ`, `Scott`... | Code ✅, level ❌ |
| Personal item | It glows and bobs somewhere in the room. Walking into it picks it up and equips it: CJ's board game, Scott's lacrosse stick... | `ADCCPersonalItem` + `DT_DCCEquipment` rows | Code ✅, rows ✅ (3 of 6), placement ❌ |
| The announcement | A few seconds later the System speaks for the **first time in the game**: the World Dungeon is open. | `WorldDungeonAnnouncement` window + voice | Code ✅, text ✅, voice ✅ |
| The arrow | A ridiculous 20 m golden arrow appears over the dungeon entrance, bobbing. Only they see it, and only after their own announcement. | `ADCCSkyBeacon` | Code ✅, placement ❌ |
| Entering | An isekai-style System window ("You Have Been Registered"), then "The Rules, Abridged". | `ADCCDungeonEntrance` + `WelcomeCrawler`/`TheRules` | Code ✅, voice ✅ |
| Alone | Each player gets their own trial room. A random encounter is rolled from a weighted table of 11 tongue-in-cheek fights. | `ADCCSoloTrial` + `DT_DCCRandomEncounters` | Code ✅, data ✅, rooms ❌ |
| After the fight | Two glowing pillars open: **SAFE ROOM** or **ONWARD**. | `ADCCSoloTrial` exits | Code ✅ |
| Safe room | Vitals appear, then the full interface is issued. Each menu page explains itself the first time it opens, with a voice line. | `ADCCOnboardingTrigger` + `SafeRoomWelcome`/`InterfaceIssued`/`Tab_*` | Code ✅, voice ✅, placement ❌ |

**Design rules that are already in the code:**
- **No onboarding actor in a level means no gating.** Sandbox and test levels keep the full HUD.
- Stages are **per crawler**: five people never wake up in the same order.
- Taking **ONWARD** still issues the HUD. Nobody plays a floor without a health bar for being brave.
- A respawn keeps your progress: the game mode remembers each player's stage.
- A sixth player, or a missing table, never strands anyone. There's an overflow destination, and an empty
  trial opens its exits straight away.

### Owed in the editor (needs Zoe at the keyboard, or an MCP session with the editor open)

Do these in order; each one is quick:

1. **Start the editor** (the code is built, Development Editor, no hot-reload DLLs).
2. **Fix the stretched crawlers (§7):** run `DCC.FixCrawlerRetargeting` in the console, then save `SK_DCCCrawler_Skeleton`.
3. **Import the two DataTables** from `Docs/` with `create` → `add_rows` → `set_rows` (see CLAUDE.md):
   - `/Game/DCC/UI/DT_DCCSystemWindows` (row struct `DCCSystemWindowDef`) ← `Docs/DT_DCCSystemWindows.json`
   - `/Game/DCC/Onboarding/DT_DCCRandomEncounters` (row struct `DCCRandomEncounterRow`) ← `Docs/DT_DCCRandomEncounters.json`
   - Re-import `Docs/DT_DCCEquipment.csv` (3 new personal-item rows, same `add_rows` + `set_rows` path).
4. **Import the voice lines:** `SourceArt/Voice/SystemAI/thehouse/VO_*.wav` → `/Game/DCC/Audio/SystemAI/` with the same names (§8).
5. **Build the Home wing.** Five bedrooms well away from the existing map (it's World Partition, so
   distance is free). Furnish each from `Modern_House_V1` / `Beach_house` / `Modern_villa_kit`.
   In each room put a `PlayerStart` with **Player Start Tag** = the crawler's name, plus an
   `ADCCPersonalItem` (Owner Crawler Id + Equipment Item Id + a mesh). Add **one** `ADCCOnboardingSettings`
   anywhere in the level.
6. **The street and the entrance.** A doorway out of the wing to a plaza, with an `ADCCDungeonEntrance` and an
   `ADCCSkyBeacon` about 30 m above it.
7. **Five solo trial rooms.** Five `ADCCSoloTrial`s at least 3000 units apart (enemy aggro radius is up to
   2000), each with floor + `NavMeshBoundsVolume`. Set **Safe Room Destination** to a `TargetPoint` in the
   safe room. Add them to the entrance's **Trials** array.
8. **The safe room.** Two `ADCCOnboardingTrigger`s just inside the door: the first with
   `MinimumStage = FirstFightWon`, `bAdvanceStage → SafeRoom`, `WindowRows = [SafeRoomWelcome]`; the
   second, a step further in, with `WindowRows = [InterfaceIssued]`, `bAdvanceStage → Complete`.
   **Ideal safe room: `GymEAP`** (it's already installed). That covers "working out and practicing" literally.
9. PIE with 2 players, check the flow end to end, then `save_assets` + Ctrl+Shift+S (World Partition actors).

### Known gaps (honest list)
- **Personal items for Vaughn, Kyle and Zoe aren't decided.** Rows exist only for CJ (board game), Scott
  (lacrosse stick) and Marcel (lucky dice, my guess from his model; confirm it). Tell me theirs.
- The personal items need meshes. Nothing installed is obviously a lacrosse stick or a board-game box. Meshy
  props, or a box mesh with a texture, would do for now.
- The existing `ADCCIntroDirector` (bedroom → door → building collapse cutscene) overlaps this flow. Recommended:
  keep the collapse as the **announcement moment**: the System speaks, the city falls, the arrow appears.
  That needs one hook from the director into `AdvanceTo(Announced)`, and it's a decision for Zoe.
- If you die *inside* the solo trial, the trial room isn't reset for your respawn yet.
- Seen-tutorial flags are per session (they aren't saved to disk), so a returning player sees the page
  explainers again after a restart.

---

## 4. Crawler Companion (`Companion/`)

**Run it:** `cd Companion && npm start` (first time: `npm install`).

What v0.1 does, verified by screenshots on 2026-11-05:
- **Login screen**: pick your crawler (optional PIN in `data/roster.json`).
- **DCC-format sheet**: identity (crawler #, race, class, level, floor, AI Favor), Health slots, Mana,
  Evade/DR/Move/Carry, five stats (unenhanced score, enhanced value, banded modifier), gear slots, attacks,
  skills (with the "used" mark), spells, inventory, buffs/debuffs, pet and mount, three sponsor boxes, notes.
  Clicking a stat or a row rolls `d20 + rank + mod` as the books describe. It autosaves to
  `Companion/data/sheets/<id>.json`, and you can only write your own sheet.
- **Your model**: loaded straight from the Meshy source art, with 15 actions: stand, walk, run, cast, attack,
  shoot, work out, practice, stance, climb, nod, show off, three dances. **Every crawler can play every clip**
  (§7 explains how this works without stretching anyone).
- **Desktop buddy**: a transparent always-on-top window. Your crawler wanders along the bottom of the screen,
  works out, practises, casts, dances, and makes occasional System-flavoured remarks. Drag to move it.
  Pressing an action on the sheet makes the buddy do it.
- Models: CJ, Scott, Marcel, Kyle, Zoe, and Vaughn (converted from FBX this session with `Tools/fbx_to_glb.py`).

The sheet layout follows the official sheet as far as it's publicly described (identity, five stats, Max Mana
and AI Favor, attacks, skills, gear, inventory, pet, mount/vehicle, three sponsor boxes). Field names follow
the community Obsidian plugin's schema, so export is possible later. **The core rulebook isn't out until
2026-11-23.** When it is, check the Health and Evade formulas (still our placeholders, in both
`Companion/src/ruleset.js` and `Stats/DCCRuleset.cpp`) and the full sheet layout.

Next for the Companion:
- [ ] Achievements and loot boxes on the sheet (read from the game's tables).
- [ ] Portrait/token capture: render the model to a PNG and use it as the Table token.
- [ ] A DM view: every sheet at once, read-only.
- [ ] Phase 2 sync (§5).

---

## 5. The Table: getting to a live top-down map

The idea's viable as long as it's built in this order. Each phase is useful by itself.

| Phase | What | How | Size |
|---|---|---|---|
| 1 | Local sheets + buddy | Done (v0.1) | ✅ |
| 2 | **Shared sheets.** Everyone sees the party; the DM sees everything; dice rolls show up for the whole table. | `Companion/server/table-server.js` ("Start The Table.bat"): a WebSocket relay on Zoe's PC. Sheets sync (newest wins), shared dice log, presence, a DM key, and a game key for pushing chatter and tokens. | ✅ 2026-11-05, tested end to end |
| 3 | **The map.** A top-down image of each floor with a token per crawler. | Render each floor once from above (SceneCapture or `CaptureViewport`, orthographic) and save the PNG plus its world bounds. The Companion draws tokens over it. | 1 session |
| 4 | **Live tokens.** Tokens move as the game plays. | UE's `WebSockets` module: the server game mode sends `{id, x, y, yaw, hp}` 4 times a second to the relay, and the relay fans it out. Positions are already server-authoritative, so there's nothing to reconcile. | 1 to 2 sessions |
| 5 | Tabletop sessions without UE | The same map and tokens, moved by hand by the DM, for nights without the game. | later |

Choices made now so this stays possible: crawler identity is a stable string (`CJ`, `Scott`...), the same
in the game (`UDCCOnboardingComponent::CrawlerId`, joining with `?Crawler=CJ`) and in the Companion. The
sheet is plain JSON keyed by that ID.

---

## 6. The brain (later, optional)

Same rule as the narrator (`Docs/ai-narrator-integration.md`): **the LLM proposes and the game decides.**
Generated items and classes are JSON rows validated against the DataTable structs, and they wait in a DM
approval queue before they exist. Never in a live fight. The Familiar's brain (`brain/`, Python + KoboldCpp)
is the working reference for running a local model on this PC.

---

## 7. "The character models look distorted": diagnosis and fix

**Root cause (found 2026-11-05):** every Meshy clip keys **translation on all 24 bones**, not just rotation.
A translation key is a bone length. The crawlers share one skeleton and share every clip, so whenever Marcel
plays a clip that was authored on CJ, Marcel's bones are pulled to CJ's lengths. The August Blender rig-fit
corrected each mesh's bind pose, but translation keys overwrite the bind pose the moment any animation plays.

A second, related finding: the bone transforms in Meshy's `.glb` files are **the first frame of some clip,
not the bind pose**. They differ between characters by as much as 130° at the hip. Only the skin's
inverse-bind matrices hold the real rest pose. Anything that reads "rest pose" from the nodes is wrong.

**Fixes:**
- **UE:** `DCC.FixCrawlerRetargeting` (new console command, `Characters/DCCRetargetFix.cpp`) sets the
  shared skeleton's per-bone translation retargeting: Hips = AnimationScaled, everything else = Skeleton.
  The standard UE remedy: bone lengths come from each mesh, motion comes from the clip. **Run it once,
  save the skeleton, PIE-check all four allies.** Not yet run.
- **Companion:** borrowed clips are retargeted rotation-only, relative to each rig's inverse-bind rest pose,
  with Hips motion scaled to body height. Verified by screenshot against each clip played on its own crawler.

---

## 8. The System AI's voice

Generated offline with Kokoro (borrowed from The Familiar's `voices/`) by `Tools/gen_onboarding_content.py`.
That script is the **single source of truth** for every System window and encounter line; edit the wording
there and re-run it.

Four candidates, each reading the same two lines, are in `SourceArt/Voice/SystemAI/_audition/`:

| Voice | Blend | Character |
|---|---|---|
| concierge | bf_emma + af_nova, 1.06× | Hotel front desk that knows your blood type. Corporate cheer over menace. |
| gameshow | am_puck + am_michael, 1.08× | Daytime TV host, all teeth |
| **thehouse** (chosen 2026-11-05) | bm_george + bm_fable, 0.96× | Friendly Stakes' The House: slower, lower, already knows how this ends |
| hostess | bf_isabella + af_bella | Warmer, the friendliest possible threat |

**Zoe picked The House.** It's now `DEFAULT_VOICE`, and all 24 lines are rendered in
`SourceArt/Voice/SystemAI/thehouse/`. **Import that folder**, not `concierge/`. The concierge renders are
left on disk for comparison and can be deleted. To re-render after editing the wording, run
`python Tools/gen_onboarding_content.py --voice`.

---

## 9. Content suggestions: what we can build with models we already have

Everything here has an **existing model in `Content/` or `SourceArt/`** unless it's marked 🆕 (needs a Meshy
generation). The guide is the tone bible: corporate cheer over menace, Denver-specific, punch at the
format, not at grief.

### Enemies (tongue-in-cheek, mostly ready)
| Name | Model | Gimmick |
|---|---|---|
| Miller Moth (Seasonal) | Swarm08 ✅ | Denver's real June moth plague. Swarms the lights; break the lamps to scatter them. |
| Kombucha Mother | Slime ✅ | Splits when hit; the babies are "the culture". |
| Storage Unit Lot #4471 | ChestMonster ✅ | Mimic. The System auctions it before the fight; winning the bid gives it +HP and better loot. |
| Xeriscape Enforcement Officer | Cactus ✅ | Fines you (gold) for standing on grass tiles. |
| Recreational Mushroom (Unlicensed) | Mushroom ✅ | Spore cloud reverses your controls for 3 s. |
| HOA President (Snapping) | TurtleShell ✅ | Turtles up; only hits from behind (a "variance request") count. |
| Smart Doorbell, Always Recording | Beholder ✅ | Ranged; records your moves, then announces your next one to the audience. |
| Goblin Tax Preparers | Smilestory_Goblin ✅ (FBX, import) | Come in threes and "file" for half your gold unless killed in order. |
| Skeleton Crew (Minimum Staffing) | Smilestory_Bonecrawler ✅ | Only ever two of them; kill one and another clocks in. |
| Parking Enforcement Sentinel | Smilestory_TombSentinel ✅ | Boots your feet in place (root) for overstaying in one spot. |
| DoorDash Hound | MotorcycleDog ✅ (static, needs rigid anim) | Fast hit-and-run; leaves a bag behind that might be food (heal) or might bite. |
| Rental Scooter Ostrich | RobotOstrich ✅ (static) | Charges in straight lines; you can ride it after you beat it (the Rover system already exists). |
| Sprinkler Heron (Denver Water) | MechanicalHeron ✅ (static) | Turret that soaks the floor, and wet floors conduct the next spark. |
| LIQUIDATOR Energy Mascot | Super_Can_Mascot ✅ | Sponsor mini-boss; drinking its drop gives you +speed and −sleep. |
| Influencer Seraph | Paragon Muriel ✅ (fully animated) | Boss: heals whoever has the most AI Favor, including you. |
| Wellness Coach | Paragon Zinx ✅ | Boss: "buffs" the party with mandatory positivity that drains mana. |
| Lava-Spitting Llamas | 🆕 | Classic DCC energy. A llama-trek company from the foothills. Spit leaves lava puddles. |
| The Hoarder of Aurora | 🆕 boss | **Arena made of junk**: every prop is a throwable pickup (the carry/throw system already exists). He throws furniture; you throw it back. |
| Zoo Escapee Monkeys | 🆕 | Steal your equipped trinket and run. Chase them to get it back. |
| Casserole Neighbours | Nuclear Family ✅ | Already built: "They brought a casserole. They brought it at you." |

### Places (each pack is already installed)
- **Home wing:** `Modern_House_V1`, `Beach_house`, `Modern_villa_kit` (bedrooms, kitchens, garages).
- **Safe room:** `GymEAP` (gym, for workouts and practice) or `Diner` (hot food, a bartender who becomes the Game Guide).
- **Floor themes:** `NY_Subway` → *RTD Light Rail, Forever* · `Thai_Market` → *Federal Blvd Night Market* ·
  `Modular_Sewer_Kit` → *Cherry Creek Storm Drains* · `Dam_city` → *Chatfield Reservoir* ·
  `Synthwave_city` → *Colfax After Dark* · `Viking_village` → *Renaissance Faire (Larkspur)* ·
  `Egyptian_Temple_kit` → *Museum of Nature & Science, After Hours* · `Ice_Station` → *A-Basin, Closed for the Season*.

### Items and loot
- Personal items (one per crawler, equipped from the start).
- Denver consumables: Green Chile (fire resist), Altitude Adjustment (oxygen can, +stamina), Rockies
  Bobblehead (trinket, nods at every crit), Light Rail Day Pass (one free teleport to the safe room),
  Lift Ticket (one free climb).
- Real weapons from `Weapon_Pack` and `Atris_swords`; class icons from `RPGClassIcons` for the sheet and hotbar.
- Loot boxes already exist (Bronze → Celestial). Give each a System unboxing line.

### Game-y things (the Elyra's Tales lessons)
- **Rank Trial**: the 100-man gauntlet already exists. Turn it into a timed score attack with S/A/B ranks
  and a leaderboard read out by the System.
- **Sparring minigame** in the safe room gym: an NPC swings left/right/overhead; parry by direction. It levels
  Martial Arts and costs no art.
- **Mission board** in the safe room: the three quest tiers from `next-evolution-plan.md`, offered as cards.
- **AI Favor saves**: at 0 HP with Favor available, the System "rewrites" the death once a floor, live, for ratings.
- **Daily** (session) challenge, read out at session start: "Today's sponsor would like to see someone die
  to a mushroom."

---

## 10. A much wider animation library (Zoe's request, 2026-11-05)

**The problem:** each crawler shipped with 3 to 8 Meshy clips, so the whole party shares 24, with no real idle,
no hit reaction, no death, no sitting, no emotes beyond a nod and three dances. The Companion now plays any clip
on any crawler correctly (rest-pose-relative retarget, §7), so **every clip we add is immediately usable by
everyone**. The game side needs `DCC.FixCrawlerRetargeting` run first.

Sources, best fit first:

| # | Source | Size | Fit with our rigs | Cost | Notes |
|---|---|---|---|---|---|
| 1 | **Meshy's own animation library** | several hundred presets (IDs run 0 to 696); Fighting alone is 146 | **Perfect**: the same auto-rig that made our 24-bone skeletons, so zero retargeting | Mostly Pro (a few free per category) | Auto-rig a crawler in Meshy, pick clips in the Animate panel, export. Also an API (`action_id`, up to 10 clips a request), and the catalog is public JSON, so a script could pull a curated set for all six crawlers at once. **Start here.** |
| 2 | **Mixamo** | thousands | Very close: `mixamorig:Hips/Spine/Spine1/Spine2/LeftArm...` map almost 1:1 onto Meshy's `Hips/Spine/Spine01/Spine02/LeftArm...` | Free (Adobe account) | Needs a name map (about 24 lines). Good for idles, hit reactions, deaths, sitting and emotes. |
| 3 | **Quaternius Universal Animation Library 1 + 2** | about 260 clips | Retarget needed: a 65-bone UE5-style skeleton, and we use the 24 bones it shares with ours | **CC0** (Standard free, Pro $9.99) | Clean, game-ready locomotion and combat. Safe to ship anywhere. |
| 4 | **UE5 mannequin animations** (Game Animation Sample, the Third Person template) | 500+ | Game side only, through UE's IK Rig + IK Retargeter (manual chains, since the auto-retargeter doesn't know Meshy rigs) | Free | High-quality locomotion for in-game movement, where blending matters most. |
| 5 | **Our own mocap** | unlimited | Through a BVH/VRM pipeline | Free | **XR Animator is already installed** (The Familiar folder) and turns webcam video into motion. Act a move out, capture it, retarget it. Best for signature moves (CJ's Fire Fingers, Scott's lacrosse wind-up). |

**Plan:**
1. Pick a shortlist of about 40 clips that fill real gaps: idle, idle variants, a hit reaction, a death, a stagger,
   a sit, a cast, a two-handed swing, a block, a dodge roll, a cheer, a taunt, a facepalm, a shrug, a point, a wave,
   a sprint and a crouch-walk.
2. Pull them from Meshy (#1) for one crawler. Because all six share one skeleton, one download covers everyone.
3. Companion: add the `.glb` to `animationLibrary` and the actions to `roster.json`. They're in the wheel the same day.
4. Game: split with `Tools/split_glb_anims.py`, import against `SK_DCCCrawler_Skeleton`, and finally build the
   proper AnimBlueprint (idle/walk/run blend plus montage slots) that CLAUDE.md has wanted since August.
5. Mixamo (#2) for whatever Meshy lacks; write the bone-name map once and reuse it.

## 11. Progress log

- **2026-11-05**: Came back after two months. Built the onboarding systems in C++ (`Onboarding/`:
  per-crawler stages, HUD gating, Solo Leveling-style System windows, personal items, sky beacon, dungeon
  entrance, solo trials with a weighted random encounter table, safe-room triggers), plus crawler identity
  in the game mode (`?Crawler=CJ` or tagged bedroom starts). Development Editor build ✅ (two builds, no
  numbered DLLs). Diagnosed the stretched-model bug (translation keys on shared clips) and added
  `DCC.FixCrawlerRetargeting`. Wrote all onboarding text and 24 System AI voice lines (Kokoro, offline) with
  4 audition voices. Built Crawler Companion v0.1 (login, DCC sheet, 3D model with 15 actions, desktop
  buddy). Converted Vaughn to `.glb`. **Not yet done:** anything in the editor (§3 list); nothing committed.
- **2026-11-05 (later)**: Zoe chose **The House** as the System AI's voice. All 24 lines were re-rendered in it.
- **2026-11-05 (later still)**: Zoe heard the first House take as slow and robotic. Two causes: it was spoken one
  sentence at a time (which resets the intonation every sentence), and it was slowed to 0.96×. Synthesis now
  works a paragraph at a time, and four faster House takes (`house_*`) are in `_audition/`, waiting for a pick
  before re-rendering. Companion: a real standing idle (`Idle_Stand`, built from Agree_Gesture's neutral first
  frame plus a breath; "Alert" is now "Look around"); a middle-click-drag action wheel, like The Familiar's, on
  both the sheet and the buddy; and an audience chatter feed behind the model, from the game's own
  `DT_DCCChatterLines.csv` with the same bucket odds and running gags. That feed is rolled locally for now;
  `pushLive()` is the hook for real in-game chatter once the relay exists.
- **2026-11-05 (evening)**: Zoe decided to **voice the System AI herself**, processed, rather than use any TTS
  voice. Built the System Voice Studio (`cd Companion && npm run studio`): live monitoring through the effects,
  a 24-line teleprompter, recording, a glitch button (live, or marked onto a take), synthesised boot-up and
  shutdown intro/outro, six presets, and export straight to `SourceArt/Voice/SystemAI/zoe/`. Offline renders
  were verified by measurement (no clipping, after adding an output trim). **The mic and live paths have not been
  tested; they need Zoe at a microphone.** The §3 import step should use `zoe/` once she has recorded.
- **2026-11-05 (night)**: Added one-click launchers (desktop shortcuts with icons, folder launchers that set
  themselves up, and a shortcut maker for friends), then a real **installer**: `npm run dist` builds
  `Companion/release/Crawler Companion Setup 0.1.0.exe` (240 MB, every model bundled). Verified by running the
  packaged app as Scott. Packaging bug found and fixed: electron-builder strips `examples/` from node_modules,
  which silently removed three.js's model loader. Added an animation-expansion plan (§10).
- **2026-11-05 (late)**: **The Table, phase 2 of §5, is done.** A WebSocket relay (`Start The Table.bat`), plus a
  Companion **Table** button, a Party panel, a shared dice log, sheet sync and live chatter. Tested end to end:
  server, a scripted "Scott" and a scripted "game", with CJ in the real Companion. CJ saw Scott online, saw Scott's
  natural-20 roll, and the game's chatter line arrived in CJ's feed. Installer rebuilt with it. **Next for the
  Table:** phase 3, the top-down map image, and the UE side pushing chatter and tokens (needs the editor and a
  `WebSockets` module dependency, which is a reflection-free C++ change).
- **2026-11-05 (later still): THE INTRO IS PLAYABLE IN PIE.** Editor started, then over MCP:
  - Imported `DT_DCCSystemWindows` and `DT_DCCRandomEncounters`; added the 3 personal-item rows to `DT_DCCEquipment`.
  - Built the whole area (`Tools/build_onboarding_wing.py`, 159 actors under outliner `DCC/Onboarding`, around
    x=-30000 y=30000): six furnished bedrooms with tagged starts and personal items, Home Street, the plaza with
    the archway entrance and sky arrow, six solo trial arenas with nav, and the safe room with both triggers.
  - **Real bug found and fixed:** `BP_ThirdPersonGameMode` was parented to plain `GameModeBase`, so
    `ADCCGameMode` (spawn selection, late-joiner routing, DCCHost, and now crawler identity) had **never run in
    play**. Reparented to `DCCGameMode`; its pawn, HUD, GameState and controller settings were kept, verified by
    read-back.
  - `Tools/pie_test_onboarding.py` walked the flow and verified each beat: spawned as CJ in CJ's bedroom with HUD
    0, the board game equipped as a trinket, the announcement, the door teleported him alone into Trial 1, and the
    encounter rolled "Off-Duty Mall Cops", correctly renamed. The System windows render in game.
  - **Not tested yet:** killing the trial enemies through to the exits, the safe room and the HUD unlock (no input
    injection; needs a real play), and two players at once.
  - **Waiting on Zoe (the editor needs a human for these):** (1) click **Import** on the "24 changes to source
    content" prompt (the voice lines); (2) **File > Save All**, because the World Partition actors and the
    equipment rows exist only in memory until then; (3) console: `DCC.FixCrawlerRetargeting`, then save
    `SK_DCCCrawler_Skeleton`. The skeleton's `BoneTree` is read-only over MCP; every bone currently reads
    "Animation", which confirms the stretching diagnosis.
  - **Next:** the player pawn uses one mesh for everyone (Zoe's), so CJ looks like Zoe. The crawler's own mesh
    should follow `CrawlerId`. The bedrooms are overexposed (point light plus auto exposure in a white box).
  - Gotcha: the editor throttles itself when it isn't the foreground window, so game time runs roughly 4x slower
    than real time. Wait on state in scripts, never on fixed sleeps.
- **2026-11-05 (night 2): each crawler wears their own body in game.** Zoe did the three editor steps (voices
  imported, Save All, retarget fix run and saved). Added `UDCCOnboardingComponent::CrawlerMeshes` (an editable
  CrawlerId-to-mesh map: CJ→SK_HockeyWings, Scott/Kyle/Marcel→their `_Fixed` rig-fit meshes, Vaughn→SK_Vaughn_Shared,
  Zoe→SK_Zoe) and `ApplyCrawlerLook()`, which runs on the server and through `OnRep_CrawlerId` on clients. It swaps
  the mesh, re-hangs gear (`UDCCEquipmentComponent::RefreshAttachments`), sets the nameplate name, and restarts the
  loop animation (`ADCCCharacterBase::RestartLoopAnimation`; without it the swapped body stood in T-pose because
  the loop cache still said "idle"). PIE-verified: spawned as CJ in `SK_HockeyWings`, animating, board game
  equipped, intro through to a Collections Agents fight.
  - Investigated and **ruled out** a suspected identity race: the restore succeeds on its first attempt, in game
    time. A backgrounded editor barely ticks PIE (one "next tick" took 17 real seconds), which made it look late.
    The restore now retries until possessed anyway, as insurance.
  - The bedroom lights are warmed (900 intensity, warm colour). **In memory only: needs Save All.**
  - Not yet seen: the body swap on a second, networked client (the OnRep path), and the jersey texture at full
    resolution (the test screenshot caught it still streaming).
