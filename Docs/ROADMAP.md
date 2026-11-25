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
| The arrow | A ridiculous 20 m golden arrow hangs over the dungeon entrance from the moment they wake, so the street ends on it. It hides once they go in. | `ADCCSkyBeacon` | Code ✅ |
| Entering | An isekai-style System window ("You Have Been Registered"), then "The Rules, Abridged". | `ADCCDungeonEntrance` + `WelcomeCrawler`/`TheRules` | Code ✅, voice ✅ |
| Alone | Each player gets their own trial room. A random encounter is rolled from a weighted table of 11 tongue-in-cheek fights. | `ADCCSoloTrial` + `DT_DCCRandomEncounters` | Code ✅, data ✅, rooms ❌ |
| After the fight | Two glowing pillars open: **SAFE ROOM** or **ONWARD**. | `ADCCSoloTrial` exits | Code ✅ |
| Safe room | Vitals appear, then the full interface is issued. Each menu page explains itself the first time it opens, with a voice line. | `ADCCOnboardingTrigger` + `SafeRoomWelcome`/`InterfaceIssued`/`Tab_*` | Code ✅, voice ✅, placement ❌ |

**Design rules that are already in the code:**
- **No onboarding actor in a level means no gating.** Sandbox and test levels keep the full HUD.
- **ONWARD and the plaza arch open the generated World Dungeon** (`ADCCTaleFloor`, 2026-11-06). Three short floors, then the stairs send you home. `DCC.EnterTale` does the same from the console.
- **A tale floor is a run, not a corridor** (2026-11-12, Elyra's `run.gd` cut down). Each floor cleared pays 25×floor gold and 60×floor XP and heals the party by 30%. Clearing floor 3 adds 100 gold, 150 XP and 5 Favor. Everyone in the run gets a results window. Empty seats, up to 3 in total, are filled by AI **stand-ins** (the ally crawler classes, minus whoever a player is). They follow the party along the grid and leave when the run ends.
- **Nobody dies for good on a tale floor.** When a downed crawler's death saves run out, the System **rewrites** it (2 per crawler per run: back up at 50% HP, +2 Favor). Once both are spent, the crawler is sent home alive with everything they have. When the last player leaves, by dying out or by the SURFACE pad, the run ends.
- **Quick play skips onboarding.** `DCC.QuickPlay` in the console sends every connected crawler to floor 1 with the full HUD. Set `dcc.AutoQuickPlay 1` and every crawler does that automatically 1.5 s after they spawn.
- **Home Street is a canyon** (2026-11-06). `ADCCOnboardingSettings` dims the bedroom and street lights at BeginPlay and raises 18 m walls around the street, so the camera is not drawing the city. The arrow is visible from the waking stage. The entrance trigger sits in front of the east wall.
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
  save the skeleton, PIE-check all four allies.** ~~Not yet run.~~ **2026-11-13: it HAD been applied.** A smoke
  test logged that the skeleton asset already carried these modes, and CJ's spine is still bent, so this was
  not the (whole) cause. It is now also enforced at runtime (`DCCRetarget::EnsureForMesh`). The bent spine is
  still open; next suspects are each mesh's bind pose versus the pose the clips were authored against.

**The bent spine / twisted hips: real root cause (found 2026-11-13).** The Meshy rigs share bone *names*,
not bone *axes*. Each auto-rig orients its bones differently at rest. Measured against CJ's rig (the source
of `SK_DCCCrawler_Skeleton`): Scott's Hips are rotated 144°, Spine02 143°, the upper legs 153°, and the feet up to
47°. Kyle and Marcel are similar. A clip stores each bone's local rotation in its own rig's axes, and UE plays
it verbatim. So any clip played on a body from another rig spins Hips, Spine02 and the legs by the axis
difference. Every crawler idles in Scott's `A_DCC_OA_Alert`, so CJ was always bent. Translation retargeting
cannot help, because it never touches rotation. Bodies were affected too: only `SK_HockeyWings` and
`SK_OfficeAirplane_Fixed` are on CJ's axes. `SK_ArmsOut_Fixed`, `SK_DiceWinged_Fixed`, Vaughn, Zoe and every
`_Shared` mesh except Scott's are 130–140° off at the hips.

**Fix: put everything on CJ's axes.**
- `Tools/rebake_clip_to_shared_rest.py` re-bakes a clip onto CJ's rest: per bone, the source's world-space
  change from rest is applied to CJ's rest, and Hips height is scaled. All 20 non-CJ clips are re-baked into
  `SourceArt/Shared_Rebaked/A_DCC_*.fbx`. `--render` writes before/after PNGs.
- `Tools/rebind_mesh_to_shared_frames.py` turns a body's bones onto CJ's axes while keeping its own joint
  positions, weights and vertices. At rest, no vertex moves. Output is in `SourceArt/Shared_Rebound/`.
- Checked in Blender. Scott's Alert re-baked on CJ matches Scott's own body playing it. Re-baked clips on
  rebound Kyle and Zoe, played the way UE plays them, stand straight.
- **In UE (done 2026-11-13, built and seen in game):** all 24 clips are in
  `/Game/DCC/Characters/Shared/AnimsFixed/`. The 20 non-CJ clips are re-baked, and CJ's 4 are copied unchanged. Bodies
  `SK_{ArmsOut,DiceWinged,OfficeAirplane,Vaughn,Zoe}_Rebound` all sit directly on `SK_DCCCrawler_Skeleton` (no more
  per-body `_Fixed_Skeleton`), with materials and sockets copied from the old meshes. C++ (anim set, onboarding looks,
  allies, Nuclear Family, crowd) points at the new assets. The player and ally Blueprint defaults were re-pointed and
  saved. The gear bench hides the old `Shared/Anims` folder. Before/after: `Docs/images/cj-spine-before-after.png`.
- **Left over:** 8 placed actors in `Lvl_ThirdPerson` (the old RPG level) still store the old clips and meshes as
  per-instance overrides. MCP cannot save World Partition actors. Fix: open the level in the editor, select the
  `BP_DCCEnemy_*` allies and reset Mesh and animation fields to default, then save. The old `Shared/Anims`,
  `*_Shared` and `*_Fixed` assets can be deleted once nothing references them. Socket transforms were copied as-is,
  so expect held props to need a nudge on the bench.
- **Also found:** importing a mesh onto the shared skeleton reset its per-bone translation modes to `Animation`.
  The asset was restored from git, and `DCCRetarget::EnsureForMesh` enforces the modes at runtime anyway. FBX
  armatures must be named `Armature`, because any other name becomes an extra root bone in UE.
- **Rule from now on:** a new Meshy clip goes through `rebake_clip_to_shared_rest.py`, and a new Meshy body goes
  through `rebind_mesh_to_shared_frames.py`, before either is imported against the shared skeleton.
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
- **2026-11-06: the World Dungeon is a generated floor, and a fight on it is real.** Ported Elyra's room-and-corridor
  layout (`Rooms/DCCDungeonGrid`, `Rooms/DCCTaleFloor`) instead of placing more geography or porting the Godot
  bestiary. Enemies are the ones already in the project (Debt Eater, Beholder, Thornback, mold, Shell Company,
  intern, notary, and the rest of that roster). They walk the grid with movement input, because a runtime floor
  has no navmesh and `MoveToActor` was the reason generated fights stood still. The far room holds The Interim
  Manager (Voldemortgage on floor 3) plus Debt Eaters, and those stairs stay locked until that room is clear.
  Three floors, then home. Development Editor build was a real link (`UnrealEditor-DungeonCrawlerDenver.dll`
  17:30, no numbered DLL). PIE: the plaza arch dropped CJ onto floor 1 (6 rooms, 361 open cells, one chest, eight
  enemies, feet stayed on the floor). A Debt Eater closed 600 units, then a Debt Eater and a Beholder hit CJ
  for real (misses against Evade 14, Last Call fizzled and spent its mana). CJ went down to death saves because
  the test stood him in that fight. **Not seen:** walking to the stairs, floor 2, or floor 3. The editor was
  closed before that check. To play: reopen, finish the trial and take ONWARD, walk the plaza arch after the
  first fight, or run `DCC.EnterTale`.
- **2026-11-06 (evening): the opening was drawing the city, and CJ's room was blown out.** The bedroom bulbs were
  still the engine default (5000, unitless) in white boxes, and the third-person camera looked over the 3 m
  houses at the rest of the map. Lumen and ray tracing are unchanged. At BeginPlay the onboarding settings dim
  every point light in the home wing (rooms 500, the street 250, warm, no shadows) and raise an 18 m dark
  canyon around Home Street that stops just past the plaza arch. The sky arrow shows from the moment they wake,
  and its light is 1800. Tale-floor blocks no longer cast shadows, and those room lights are 4000. Development
  Editor link at 18:14 (`UnrealEditor-DungeonCrawlerDenver.dll`, 3,762,176 bytes, no numbered DLL). **Not seen
  in PIE** — the editor was left closed. Reopen to play. Still not seen from earlier today: walking the dungeon
  stairs, floor 2, or floor 3.
- **2026-11-12: the World Dungeon is a playable run.** Zoe asked for "Elyra's Tales, but DCD". The floor layout
  was already ported, so this pass ported the run loop on top of it (all in `Rooms/DCCTaleFloor`):
  - Pay per floor, a heal on the stairs, a finale bonus, and a results window.
  - AI stand-ins in empty party seats. Allies now follow along the grid (`TickFollowLeader`), because a runtime
    floor has no navmesh.
  - Narrator rewrites in place of permanent death (`ADCCCharacterBase::ReviveQuietly`, hooked in `RollDeathSave`).
    Before this, a crawler who bled out on a tale floor stayed dead for the rest of the session, because nothing
    respawns players.
  - `DCC.QuickPlay` and the `dcc.AutoQuickPlay` cvar, so a run no longer waits on the owed bedroom and trial
    placement.

  Development Editor build was a real link (`UnrealEditor-DungeonCrawlerDenver.dll` 16:43, 3,796,480 bytes, no
  numbered DLL). **Not seen in PIE.** To test: open the editor, press Play, and type `DCC.QuickPlay`. Walk to
  the blue room and kill the boss and its adds, which opens STAIRS DOWN. Repeat for floors 2 and 3, and check
  the results window. Still owed after that: gates (`gate_mode.gd`: wave-hold arenas, a board in the hub).
- **2026-11-12 (later): item pop-ups.** Zoe asked for Elyra's item system as "a video game style pop up". It is
  ported from `notices.gd`, with rarity colours after `enchanting.gd`, in `UI/DCCHUD_ItemToasts.cpp`. A card
  slides in on the right for each new thing: a bag item, an equipped piece, a potion, a loot box, or gold
  (several coins in a row fold into one card).
  - Each card has a drawn icon in a rarity frame, the name in the rarity colour, the rarity and slot, and stat
    lines. Gear also gets a comparison against what is in that slot: green UPGRADE, red Weaker, or "Replaces X".
  - Up to three show at once and the rest queue. Cards time out on real time, so they still clear during a
    pause.
  - The HUD diffs the owning crawler's replicated state each frame instead of using an RPC, so every source gets
    cards with no new plumbing: pickups, chest and corpse loot, tale payouts, achievement boxes, and the personal
    item. It shows even before the HUD is issued, because picking up your personal item in the bedroom deserves
    a card.

  Development Editor link with no numbered DLL. **Not seen in PIE.**
- **2026-11-12 (evening): a party menu, and the World Dungeon works for clients.**
  - **F2 opens the party menu** (`UI/DCCSessionMenuWidget`, plain C++ Slate, no widget asset; helpers in
    `Session/DCCSession`). It does these things:
    - Pick your crawler. This is remembered in GameUserSettings and sent as `?Crawler=` when you join; the host's
      pick applies to their pawn at once.
    - **Host this game**: `UWorld::Listen` opens the running game on port 7777 without a reload, falling back
      to `open <map>?listen`.
    - **Invite codes**: IPv4 and port packed into Crockford base32 (`K7M2Q-9TB4X`), one per reachable address,
      Tailscale first, then the same-Wi-Fi one. Each has a Copy invite button.
    - **Join** takes a code or a plain address.
    - **Who's here**: crawler, player name and ping.
    - **Start the run (everyone)** for the host. It becomes "Bring everyone into the run" while a floor is live,
      and it skips anyone already on the floor.
    - **Leave game** for clients.
  - A launched (non-PIE) game opens on the party menu. So does any session after a failed join or a dropped
    connection, which shows the reason. `DCC.Party` toggles the menu from the console.
  - **The tale floor replicates a layout, not geometry.** `ADCCTaleFloor` is now replicated (`LayoutSeed`,
    `LayoutFloor`, `bLayoutLive`). Every client rebuilds the same floors, walls, lights, signs and crates from the
    seed (`BuildGeometry`, and `SpawnRoomDressing` on its own random stream). Only enemies and chests come from
    the server. Before this, a client would have got blocks with no colours, blank signs, and possibly no floor
    collision.

  Development Editor link, no numbered DLL. **Not seen in PIE.** To test: Play with Number of Players = 2 (Listen
  Server), press F2 on the host, then Start the run. Still owed for friends on other PCs: **a packaged build**.
  Nobody else has a copy of the game yet.
- **2026-11-12 (night): first packaged build.** `D:\Builds\DungeonCrawlerDenver-2026-11-12.zip` contains the
  Development Win64 build, the VC++ prerequisites and `HOW_TO_PLAY_TOGETHER.txt`. BuildCookRun took 1h 10m and
  the build is 3.0 GB unzipped.
  - `DefaultGame.ini` now has packaging settings: cook `Lvl_ThirdPerson`, and always cook `/Game/DCC` and the
    Ice_Station crates, which are loaded by path at runtime. The project name is now "Dungeon Crawler Denver".
  - **The first cook crashed on a real bug.** Console names are case-insensitive, so the `dcc.QuickPlay` cvar
    and the `DCC.QuickPlay` command collided, which is fatal at startup. The editor would have hit it too. The
    cvar is now `dcc.AutoQuickPlay`.
  - Smoke test: the exe launches, the map loads in 2 s, there are 0 errors in the log, and the game mode is the
    DCC one (`[DCC Spawn]`), with CJ's body applied.
  - Not checked: the party menu on screen, and a real two-PC connection.
- **2026-11-12 (late): the loot box screen.** **O** opens it; the code is `UI/DCCHUD_LootBoxScreen.cpp`, with
  input captured by `UI/DCCLootScreenInput`. It is modelled on a hero-shooter loot box screen:
  - **The row:** every sealed box sits in a row along the bottom as a gift box in its tier colour, cheapest
    first. The selected box is bigger and bobs. Choose with A/D, the arrow keys, the mouse wheel, or a click.
  - **Opening:** hold Space (or hold the mouse on the selected box) for 0.75 s. The box shakes harder and a bar
    fills. Keep holding and the next box starts charging once the reveal lands, so one long hold opens a stack.
  - **Open all:** hold R for 1.2 s to open everything at once, as one burst with a grid of cards (32 shown,
    then "+N more").
  - **The reveal:** the box shakes itself apart, then a flash and rays in its tier colour. The contents fly out
    as cards with the item pop-ups' rarity colours and drawn icons, plus the name, rarity, slot and stats.
  - **Server side:** `ADCCCharacterBase::Server_OpenLootBoxBatch` opens one box or a batch. Gear goes into the
    **bag**, not onto the floor (`TryOpenLootBox` now takes a looter), gold goes into the purse, and boons apply
    at once. It replies with `Client_LootBoxReveal(FDCCLootBoxReveal)`.
  - **Where boxes open (a rule change):** a Safe Room, **or anywhere nothing hostile is within 25 m**
    (`CanOpenLootBoxesHere`). The World Dungeon has no Safe Room, so a cleared room counts.
  - Item pop-ups pause while the screen is open, because it shows its own reveal.
  - `DCC.GiveBoxes [n]` gives everyone n boxes of each tier, for testing or for the DM.
  - The older Tab-menu "claim all" reveal, a line of box props in the world, still exists and is untouched.

  Development Editor link, no numbered DLL. **Not seen in PIE.**
  - **Content gap:** `DT_DCCLootBoxTables` holds only 2-3 item entries per tier, drawn from about 30 equipment
    rows and 8 boons, so reveals will repeat a lot. More box content is the obvious next step.
- **2026-11-13: loot content, batch 1.** `Tools/gen_loot_content.py` is now the source of truth for box loot.
  Running it writes the JSON in `Docs/` and a readable `Docs/loot-catalogue.md`, and it refuses to write if any
  mesh path or table reference is wrong.
  - **43 new pieces of gear** across all six tiers: Bronze 9, Silver 8, Gold 8, Platinum 7, Legendary 7,
    Celestial 4. Every one uses a real mesh from an installed pack (forge swords and hammers, Atris blades,
    Weapon_Pack, Viking helmets and shields, gym weights, a kettle, a grenade). Two are cursed
    (Customer of the Month Plaque; Sponsor's Signet). Two have reach (a spear and a halberd). The grenade is
    thrown, and three glow. The voice follows tone bible section 5, and every Legendary item cites its terms of
    service.
  - **12 new boons**, 20 in total.
  - **Rarity for the 28 existing items.** The CSV never had a Rarity column, so all existing gear read as
    Common, with no colour in the pop-ups or the loot reveal.
  - **All six box tables rewritten.** Each has 10-17 gear entries, mostly from its own tier, some from the tier
    below, and a long shot from the tier above. A box averages about 2 pieces of gear plus 0.5-1.8 boons, plus
    gold that scales from 5-25 (Bronze) to 600-1200 (Celestial).
  - **Owed, with the editor open:** `python Tools/import_loot_content.py`. It adds and writes the new gear,
    then applies rarity by **reading each existing row and writing it back whole**, because set_rows may
    replace rather than merge. It then rewrites the boons and box tables, reads back a few rows to check, and
    saves. Nothing has been imported yet.
  - Grip offsets are zero, so new weapons may sit oddly in the hand until they are nudged.
  - **The bag holds 12 items** (`UDCCInventoryComponent::Capacity`), so a stack of boxes overflows it fast. The
    overflow lands at the crawler's feet and nothing is lost, but the bag may want to be bigger.
- **2026-11-13: the gear bench (F3, or `DCC.Bench`).** This is DCD's version of The Familiar's pose bench. Code is in
  `Source/.../Bench/`.
  - **What it does:** an orbit camera on your own crawler, plus a left-hand panel.
    - **Body:** swap to any crawler's body.
    - **Animation:** every clip on that body's skeleton, found through the asset registry, with a filter box.
      Play, pause, scrub, step a frame, and speed.
    - **Gear:** equip any row of DT_DCCEquipment.
    - **Nudge:** move, turn or scale one of four things:
      - an item's grip on every body;
      - an item's grip on this body only (this wins over the every-body grip);
      - a socket on this body (it moves everything held there);
      - a bone on this body, layered on top of whatever animation plays.
    - The selected socket or bone gets an axis gizmo. Changes show at once; **Save** writes them.
  - **Where the numbers live:** `Content/DCC/Bench/DCCBench.json`. It is staged into packaged builds as a plain
    file (`DirectoriesToAlwaysStageAsUFS`). A packaged build cannot write into its own pak, so it saves to
    `Saved/DCCBench.json` instead and reads that first; copy that file over the Content one to keep it.
  - **How the game uses them (`Bench/DCCBenchData`):**
    - Grips replace the DataTable's GripOffset in `UDCCEquipmentComponent::RebuildVisuals`, and add a scale the
      table never had.
    - Sockets are written onto the body's mesh asset when it is loaded or put on.
    - Bones are applied in `USkeletalMeshComponent`'s bone-transforms-finalized callback, bound on every
      character in BeginPlay. That callback runs after the pose is evaluated and before socket attachments and
      rendering read it, so a nudged hand carries its sword. Ragdolls are skipped.
    - The DataTable and mesh assets themselves are never edited.
  - Development Editor link, no numbered DLL. **Not seen in PIE.**
- **2026-11-13: saves, the hub and shops, and skill gear.**
  - **Saves** (`Session/DCCSaveSystem`, `UDCCCrawlerSave`): each crawler's progress lives on the host as
    `Saved/SaveGames/DCC_<Crawler>.sav`.
    - **What's kept:** level, XP, gold, ability scores, skills and ranks, Favor, bag, pack, stowed remains,
      loot boxes, equipped gear, achievements, class, and onboarding stage.
    - Properties are exported by name through reflection, so new fields on those structs are saved
      automatically. Unknown keys are skipped and missing ones keep their defaults.
    - **Load** happens when identity is restored on possession. **Save** happens every 60 s (a world
      subsystem), on Logout, when a run finishes, after a shop trade, in pawn EndPlay, and at world shutdown.
    - Only the pawn that loaded a crawler may write that crawler's save. That stops a fresh pawn or a respawn
      corpse from clobbering it.
    - Console: `DCC.Save`, `DCC.WipeSave <Crawler>`.
  - **Concourse Zero** (`Rooms/DCCHub`): Denver International, repossessed, reopened as Crawler Services.
    - Built at runtime and deterministically, so every machine builds the same walls locally.
    - Contents: a Safe Room volume, two shopkeepers (`Dialogue/DCCHubVendors`, with stock in constructors so
      clients agree), a Departures gate into the World Dungeon, the sponsor kiosk sign (O), and signs.
    - Quick play (`DCC.QuickPlay`, or the `dcc.AutoQuickPlay` cvar) now lands here. Every run, and the
      SURFACE pad, returns here. The party menu's Start the run uses `ADCCTaleFloor::EnterEveryone`.
  - **The shop screen** (`UI/DCCShopWidget`) opens automatically when Shop is picked on a vendor, replacing
    the old scroll list.
    - Buy cards show rarity, slot, stats, an upgrade/weaker comparison, and the item's skill with your Rank.
    - Buying puts gear in the bag (the old menu force-equipped it). Selling pays half value.
    - Server-checked via `Server_ShopBuy` / `Server_ShopSell`.
  - **Skills, per the Crawlers Preview** ("the name of the weapon is also the name of the Skill"):
    - **Weapons:** `FDCCEquipmentDef::WeaponSkillId`. Rank adds to the attack roll, and Rank 5 adds a damage
      die. Untrained attacks roll at Disadvantage and call `RecordUntrainedAttempt`, which is how skills are
      learned.
    - **Explosives** (`SplashRadius > 0`, `ADCCCharacterBase::ThrowExplosive`):
      - Before the throw, an Int handling check with `HandlingSkillId`. A natural roll at or under
        `FumbleOnNatural` detonates at your feet (Goblin Explosives 1-4, or 1-3 at Rank 5). A fail is a dud.
      - Then a Throwing attack. A miss scatters by the margin in feet.
      - The blast hits everyone in the radius, party and thrower included, at half damage at the edge.
      - Sparks, smoke and fire Niagara effects plus a light flash.
      - Each explosive has a cooldown (`AttackCooldownOverride`).
    - **First Aid:** `FDCCConsumableDef::LinkedSkillId` / `HealPerSkillRank`, an Int check that adds heal per
      Rank.
  - **Content** (`Tools/gen_loot_content.py`):
    - 18 skill items: a weapon for every Attack Skill, plus 9 explosives across Explosives Handling, Goblin
      Explosives and Incendiary Device Handling.
    - 3 First Aid kits and 12 book-skill rows.
    - Weapon skills tagged on existing and batch-1 gear. The batch-1 grenade is now a real explosive.
    - Explosives added to the box tables.
  - **Owed, with the editor open:** `python Tools/gen_loot_content.py`, then
    `python Tools/import_loot_content.py`. Until then, shop rows whose items are not in the tables are simply
    hidden, and weapons have no skill.
  - Development Editor link, no numbered DLL. **Nothing here has been seen in PIE.**
- **2026-11-13 (later): roguelike runs, and the arcade.**
  - **The World Dungeon is an endless roguelike** (`Rooms/DCCTaleFloor`).
    - There is no last floor. Clearing a floor grants every player a box on a climbing ladder: floors 1-2 Bronze,
      3-4 Silver, 5-6 Gold, 7-8 Platinum, 9-10 Legendary, then Celestial. Gold and XP are paid too.
    - Each floor deeper: enemy level +1 (and +1 more every two floors), STR/CON/DEX +1 every two floors, one
      more fight room every three floors, bigger groups, and better chests. Every fifth floor is a boss floor
      (Voldemortgage).
    - The stairs carry a sign that says what clearing this floor pays and the threat below.
    - The SURFACE pad means cashing out: +20 gold per floor cleared and a little Favor. Dying out gets no
      cash-out bonus. That is the push-your-luck.
  - **Skills grow by use:** every floor clear runs `UDCCSkillComponent::RunSkillAdvancement`. Marked skills roll
    to rank up, and skills tried untrained may be learned.
  - **Per-game profiles** (`Session/DCCProfile`): `Profile_<Player>.sav` on the host, one number per game per
    stat. The stats are `Crawler.BestDepth/Runs`, `Climb.BestHeight/Summits/FastestSummit`,
    `Kart.BestLap/Races/Wins` and `Neon.BestDistance/Runs`. These are separate from the DCD character save,
    and the player's name is the key.
    - `ADCCGameMode::RestoreOnboarding` now makes name and crawler agree: after travel the crawler is recovered
      from the name, the host falls back to their party-menu pick, and the name is then set to the crawler.
  - **The arcade** (`Source/.../Arcade/`):
    - **The Den** is the lobby. Stand on a game's pad to vote. When everyone has voted it starts in 5 s; once
      anyone has voted, it starts in 20 s at most. The panel shows every game's leaderboard.
    - The games:
      - **Dungeon Crawler Denver:** the main level.
      - **Fourteener:** co-op climbing up a ledged peak to "14,115 ft".
      - **Repo Rally:** three laps in rovers against four autopilot repo men.
      - **Neon Mile:** endless streamed synthwave road with seeded traffic; a crash or leaving the road sends
        you back to the start, and there is a 150 s limit.
    - Each non-DCD game runs on `/Engine/Maps/Entry`. It is chosen with `?game=` (`DCCArcade::Travel`, server
      travel, seamless) and builds its world, sky and PlayerStarts at runtime. Each one returns to the lobby
      by itself.
    - A launched (non-PIE) game starts in the lobby once (`-nolobby` skips this). The party menu has an Arcade
      lobby button for the host. `DCC.Arcade [Game]` travels from the console.
    - `/Engine/Maps/Entry` was added to MapsToCook.
  - Development Editor link, no numbered DLL. **None of this has been seen running.** If server travel
    misbehaves in Play-in-Editor, try `net.AllowPIESeamlessTravel 1`, or test in a standalone or packaged
    build.
  - **The originals rule:** these are original games in those genres (co-op climbing, kart racing, synthwave
    driving), not recreations of specific games. Keep their names, characters and art our own.
- **2026-11-13 (evening): new direction. An arcade of minigames, with DCD as gate roguelike runs.** Zoe: rather than
  a long-term RPG, use the gates for roguelike runs and prefer other minigames.
  - **Arcade DCD now starts in Concourse Zero.** The travel URL carries `?Hub=1`, and `ScheduleQuickPlay` skips
    the bedroom and city. That makes DCD shop, then gate, then endless run.
  - **Dead Air** (`Arcade/DCCArcadeHaunt`) is an original found-footage minigame. The crew films the old
    mansion on Capitol Hill for a paranormal stream.
    - **The house:** the dungeon generator's layout from a replicated seed, roofed and dark, with flickering
      bulbs, a flashlight on every crawler (built locally), and drifting cold-light anomalies. The thing in the
      deepest room is worth the most. One monster per room walks the grid.
    - **Recording:** V toggles `ADCCCharacterBase::bRecordingFootage`, which is replicated. Footage per 0.2 s
      is the sum, over subjects in a 25° cone within 25 m with clear line of sight, of value × centredness ×
      closeness.
      - Subject values: anomaly 4, the deep thing 15, monster 10, a monster attacking someone 25, a teammate
        hurt in the last 2 s 30, a teammate downed 20.
      - Each player has 120 s of tape. Dying halves your footage.
    - **Upload:** after 5 minutes, only crew alive inside the van (the foyer) upload. Views = footage × 137.
      Profile stats: `Haunt.BestViews/TotalViews/Plays`.
    - **HUD:** a body-cam overlay while recording (REC, tape, a live shot-quality meter, views on tape), plus
      the arcade panel.
  - **All arcade games now respawn dead players** after 6 s (`ADCCArcadeArena`). Nothing else in the project
    respawns players.
  - The lobby now has five pads: DCD, Dead Air, Fourteener, Repo Rally and Neon Mile.
  - Development Editor link, no numbered DLL. **Not seen running.**
- **2026-11-13 (night): character select before the Den.** A launched game now opens on the **front door**:
  - `ADCCFrontDoorGameMode` and `ADCCArcadeFrontDoor` run on the Entry map: a lit stage with your crawler on it.
  - `UDCCCharacterSelectWidget` lists the six crawlers down the left, with each one's best records from `DCCProfile`.
    Click a name or press Up/Down to swap the body on stage. Enter, or the button, travels to the Den.
  - The pick is remembered, so a returning player just presses Enter.
  - `DefaultEngine.ini` now sets `GameDefaultMap=/Engine/Maps/Entry` and `LocalMapOptions=?game=...DCCFrontDoorGameMode`.
    The editor startup map is unchanged. A launch on any other level still goes through the front door (`MaybeStartInLobby`).
  - New `DCCSession::SwitchCrawler`, now also used by the F2 party menu. It saves the old crawler, then sets the body
    **and the player name**. Before this, an F2 switch changed only the body, and it reverted on the next travel
    because identity follows the name.
  - Development Editor link, no numbered DLL. **Not seen running**: the camera framing and lighting are untested.
    In PIE, try `DCC.Arcade FrontDoor`.
- **2026-11-13 (night, later): the main menu. No typing needed.** Zoe asked that every action be a polished button.
  The front door's character select is now a main menu (`UDCCCharacterSelectWidget`):
  - Six crawler chips. The body on the stage swaps as you pick.
  - **PLAY** goes to the Den.
  - **HOST A PARTY** listens, then goes to the Den. The party menu opens there with the invite ready to copy.
  - **JOIN A PARTY** joins the invite on the clipboard. `DCCSession::FindInviteIn` finds the code inside the
    whole pasted message.
  - **REJOIN LAST PARTY**, **POSE BENCH** (opens the F3 gear bench on the stage), and **QUIT**.
  - The invite text now says to copy the message and click JOIN A PARTY. The F2 party menu gained
    "Join the invite I copied" and "Main menu" (solo) buttons.
  - **Smoke-tested** by launching `-game` and taking screenshots. It boots into the menu, CJ is on stage, and
    the buttons render. Host, join and play travel were not clicked through.
- **2026-11-13: the pose bench as its own program.** This follows The Familiar's `Pose bench.lnk` pattern.
  - Double-click **`Pose bench.lnk`** in the project folder. It runs `Tools/pose_bench.bat`, which starts the engine in game mode
    on `ADCCBenchGameMode`: the front door stage with the gear bench open and nothing else.
  - Save writes `Content/DCC/Bench/DCCBench.json`, the same file the game reads.
  - The bench's new Quit button (or Esc) exits. It warns once if there are unsaved changes. In game, the same
    button reads "Close".
  - Verified by launching the shortcut. The bench opens on CJ and the log shows `DCCBenchGameMode`.
  - Startup takes about 70 s, because it is the editor's engine running uncooked content.
- **2026-11-13 (spine fix)**: Found the real cause of the twisted hips and backwards-S spine: the Meshy rigs disagree on
  bone axes, not just bone lengths (§7). Re-baked 20 clips and rebound 5 bodies onto CJ's axes, repointed code and
  Blueprints, built, and confirmed CJ standing straight on the gear bench. Not committed.
- **2026-11-13: playtest fixes (Zoe played Neon Mile).**
  - **The party menu stopped opening at every level load.** Before, every launched-game level opened it, so it appeared
    at the start of every minigame. Now it opens on its own only after a failed join, or once after HOST A PARTY with
    the invite.
  - **Front door: crawlers faced away.** The stage now turns the body to the camera every frame. A start rotation was
    not enough, because something turns the pawn after it spawns.
  - **Neon Mile, spaced for the loose rover handling:**
    - The road is wider (half-width 1150, lanes 680 apart).
    - Traffic is at most one car per 1000-unit slot, so cars are about 750+ units apart and never block all three lanes.
    - Traffic starts at chunk 3 and fills from 30% to 75%. From 1.2 km a slot can hold two cars side by side, with one lane open.
    - A smaller crash margin.
  - **Neon Mile, decorated:**
    - The traffic looks like cars: cabin, tail lights and underglow.
    - A neon grid ground, glowing posts, palms, and a skyline of towers with neon rims.
    - A neon arch with the distance every 1 km.
    - A striped retro sun that stays at the end of the road.
    - Thinner fog.
    - Scenery uses a new `bCollide=false` `Block()` option.
  - The DebugGame compile succeeds. The Development Editor build waits for the game window to close. **Not seen in game yet.**
- **2026-11-13: pickups, gauges, moving traffic.**
  - **Personal items (CJ's board game and the others) now go into the bag**, with a feed line and the item card.
    They used to equip themselves silently. If the bag is full, the item is equipped instead, so it is never lost.
  - Ground gear also gets a feed line now, plus a notice when the bag is full. Before, a full bag left the item on the
    floor without a word.
  - **New vehicle instrument cluster** (`UI/DCCHUD_VehicleGauges.cpp`), replacing the rough tick dial:
    - a speed arc in km/h (cm/s x 0.036), with a scale and a needle cap;
    - a boost ring and a status label;
    - an altimeter tape on a log scale (a ground trace from under the rover), with climb rate;
    - Slate fonts, so the text stays crisp.
  - **Neon Mile traffic now drives.** New `ADCCNeonTrafficCar` (a reflection change, so the editor must restart):
    - It moves at about 50 km/h. Only its start, start time and speed replicate. Each machine moves it from the
      shared server clock.
    - The server keeps the stretches 7 to 11 chunks ahead of every rover stocked with traffic.
    - Boost pads (glowing chevrons) are on about two in five stretches. They are checked on the driver's machine and
      on the server, and both call `GrantBoost`.
  - The DebugGame compile succeeds. The Development Editor build waits for the editor to close. **Not seen in game.**
- **2026-11-13: Fort Tiner, points, tickets, the prize counter, round-end vote, editor tool.**
  - **`Tools/editor_ctl.py`** has five commands:
    - `status`: what is running, whether MCP is up, whether PIE is running.
    - `shot`: captures only the Unreal windows, via PrintWindow by handle; never the desktop.
    - `save`: saves through MCP `save_assets([])`.
    - `close [--nosave]`: a normal close; the editor asks about unsaved work itself.
    - `kill`: only on request.
  - **Tickets** are the arcade's own money (profile `Arcade.Tickets`). Payouts:
    - Fourteener: points become tickets.
    - Repo Rally: by finishing place.
    - Neon Mile: 1 per 25 m of each run.
    - Dead Air: views / 100.
  - **Cosmetics**: `Arcade/DCCCosmetics` has 13 items across Head, Back, Armor and Aura, built from engine shapes.
    Each item follows a head or back socket but is placed in the character's own facing.
    - `UDCCWalletComponent` on every crawler mirrors the profile, replicates what you wear, and handles buy, wear
      and take-off requests on the server.
    - The **Prize Counter** is a gold pad in the Den, plus a PRIZES button on the main menu.
  - **Round-end vote** (arena base): PLAY AGAIN or BACK TO THE DEN buttons. Majority wins; a tie or no votes
    goes to the Den. It is decided once everyone votes, or after 20+ s. Used by Fourteener, Repo Rally, Neon Mile
    and Dead Air.
  - **Fourteener points**:
    - 1 per 100 ft of new height;
    - summit 100;
    - first up 50;
    - beat-the-clock bonus;
    - whole crew up 50 each.
  - **Fort Tiner** (base camp at the foot):
    - a log palisade with its gate facing the peak, two watchtowers;
    - a trading post, bunkhouse and assay office;
    - a flickering campfire, a flag, a well, and the goat (the mayor);
    - players start around the fire.
    - **Four townsfolk** (`ADCCFortTinerNPC`): Sheriff Dusty, Ma Ore, Old Gus and Banjo Bea. Their lines live in
      `Tools/gen_fort_tiner_lines.py`, which also writes the Voice Studio script.
  - **NPC voice lines without import:** the studio exports to `SourceArt/Voice/NPC/<Speaker>/VO_<Row>.wav`.
    `ADCCDialogueNPC::PlayVoiceFor` plays the file at runtime (`Dialogue/DCCVoiceLines`, 16-bit PCM, about
    25 m falloff) and keeps the text up for as long as the line plays.
    - Packaged builds: copy `SourceArt/Voice/NPC` to `<Game>/Voice/NPC`. This is not automated yet.
  - **Bug found by the Fourteener smoke test: every arcade game spawned players at `/Engine/Maps/Entry`'s own
    PlayerStart near the origin**, which is inside the mountain in Fourteener. With nobody in the world yet, every
    start ties on clearance and the first one iterated wins. `ADCCArcadeArena::SpawnStarts` now removes any start
    it did not place.
  - **Verified by `-game` launch and window screenshot:** CJ starts inside Fort Tiner by the campfire, and Sheriff
    Dusty and Banjo Bea stand at the gate in their ten-gallon hats.
  - **Not verified:** the Den shop, the round-end vote, the ticket payouts, and a recorded line actually playing.
    The Voice Studio changes passed a syntax check only.
- **2026-11-13: hold-to-climb, fall damage, and Fourteener hazards, routes and co-op.**
  - **Climbing works Zoe's way now, everywhere:**
    1. Equip Climbing (hotbar).
    2. Hold the left mouse to grab the wall in front and climb. Holding while walking into a wall also grabs it.
    3. Release the button, or run out of stamina, and you drop. Right mouse held climbs down.
    - Without Climbing equipped, a wall is never grabbed; clicking one shows a hint.
    - `ADCCCharacterBase::TickHoldToClimb`, `TryGrabWallAhead` and `HintEquipClimbingIfWall`.
  - **Fall damage** (`ADCCCharacterBase::Landed`, server): free up to 6 m, then it rises to all of your HP at 20 m.
    `FallSafeUntil` suppresses it.
  - **Fourteener mountain:** ledges on every face.
    - The express face (toward the fort) has one every 12 m and double the rockfall.
    - The three switchback faces have one every 6 m, staggered, plus ice.
    - **High Camp** (the shelf at mid height) is a checkpoint, and you respawn there (`OnRespawned`).
  - **Hazards:**
    - **Rockfall:** replicated as start and time. A rock shakes at the top for 1.5 s, then falls and stops at the
      first ledge. A hit costs 15% HP and knocks you off.
    - **Gusts:** a 3 s warning, then 4 s of triple climb cost on one face.
    - **Ice:** double climb cost.
  - **Co-op:**
    - Standing on a ledge above a climber ropes them in: cheaper climbing, no fall damage, and +1 point per 2 s
      for the belayer. A rope is drawn between you.
    - Two or more resting on one ledge for 3 s pitch a camp: +25 each, double stamina regen there, and it becomes
      the team checkpoint.
  - **Not verified with real input:** the hold-to-climb feel, the hazards hitting climbers, belays and camps.
    The launch test shows the round running with no errors.
- **2026-11-13: climbing smoothness, a climb test, Fourteener art, and editable NPC lines.**
  - **`DCC.ClimbTest [0-3|all]`** (`Arcade/DCCClimbTest.cpp`) automates a climb:
    - It climbs each face to the summit with infinite stamina and hazards off (`dcc.ClimbHazards 0`).
    - It logs jumps, stuck time, drops, frames with the capsule in rock, and frames with the camera in rock.
    - Run it with `-ExecCmds="DCC.ClimbTest all"`.
  - **Root cause of the top-of-ledge glitches:** generic walls got an unrotated 5 m `ADCCClimbPoint` box centred on
    the surface. That box blocked Visibility traces (so you could grab or stand on air), climbers were held against
    the box rather than the rock, the top was capped 18 m above the grab, and the box's top and bottom never
    replicated. The fix:
    - Plane-mode climb points (`WallNormal`/`WallPoint`, replicated, no collision).
    - The lip is found to the centimetre with no cap.
    - The dismount goes over the lip.
    - Swept climbing.
    - Easing onto the wall (`ClimbStepToward`, 7 m/s) removed an 81 cm grab snap and a 191 cm High Camp snap.
  - **Ledges are crawled over** (`BeginLedgeCrawl`): out along the underside, round the lip, onto the top. The path
    runs on the server and on the climber's own machine. The hidden ledge collision is as deep as the boulder.
  - **Climbing camera:** pulls out, faces the wall, skips collision tests, and eases back afterwards.
  - **HUD:**
    - The chat starts minimized.
    - The arcade status box shows for 60 s, then only alerts (`!`-prefixed lines) as a slim banner.
    - NPC speech wraps, and the nearest speaker's line shows as a subtitle.
  - **Fourteener art** (`ADCCArcadeArena::Prop`/`Paint`, real pack meshes, cook dirs added in `DefaultGame.ini`):
    - Mountain: rock outcrop ledges; cliff-rock tiles on every face; no summit overhang; tents and supplies at
      High Camp; a cairn and brazier at the summit; boulders; pines; grass; a distant range.
    - Fort Tiner: Viking log cabins, wooden watchtowers with flags, braziers, a blacksmith (forge, anvil, bellows,
      grinder, rack), carts, a pulley, tables, crates, barrels and tents.
  - **NPC lines live in `SourceArt/Voice/NPC/fort_tiner_lines.json`.** The game reads them every round.
    - The Voice Studio has Edit line, New line and Delete line, saving straight to that file (with a .bak).
    - `gen_fort_tiner_lines.py` now seeds the JSON and refreshes the compiled fallback `.inl` from it.
  - **Verified:** the climb test passes all four faces with no jumps or drops; screenshots of the fort, the climb
    and the summit; the studio starts with no errors.
  - **Not verified:** the studio's line editing has not been clicked through.
- **2026-11-13 (later): Fourteener items, a bigger mountain, a trimmed HUD, and the trailer.**
  - **The mountain is twice the size:** 96 m tall (`TopZ` 9600), base faces 24 m wide (`WideHalf` 1200), the
    upper half 14 m (`NarrowHalf` 700). High Camp is at 48 m. Fort Tiner moved out to match (`FortCenter`).
  - **Climbing items** (`Arcade/DCCClimbItems`), all Fourteener-only:
    - Tools, used with **Q** and cycled with **T**: Grappling Hook (pulls you up to the ledge above), Piton (a
      private rest point on the wall), Trail Mix (refills stamina).
    - Boosts, which last the round: Chalk Bag (cheaper climbing for a while), Crampons (ice does not slip), Wind
      Cloak (gusts do not shove), Hard Hat (absorbs one rock), Safety Line (one free fall).
    - Artifacts are trophies kept in the profile (`Trophies`), banked only if you **survive the round**: the
      Ceremonial Knife (evidence to stall the mining company, a keepsake for the main game), Platinum Ore
      (sold at the Prize Counter for 150 tickets), the Assay Ledger and the Claim Lantern.
  - **Chests** (`Arcade/DCCSupplyChest`): random climbing items on the ledges, an Outfitter's Crate at base
    camp with every item, and artifact caches high up and at the summit. Each player can loot each chest once.
  - **Prize Counter:** a trophy vault with SELL buttons for anything that has a ticket value.
  - **Minimal minigame HUD:** no chat feed, quest tracker or Tab hint in arcade games. Health shows only where
    the game can hurt you; stamina, hotbar and the tool panel only in Fourteener.
  - **Lighting:** the sun and summit light were softened (they washed out the crawlers). Trailer bots no longer
    show "personCharacter" nameplates.
  - **Gear bench: Save now writes only the entry on screen** (that item's grip, or that socket or bone). Other
    entries in `DCCBench.json` stay as they were on disk; Quit still warns about other unsaved changes.
  - **Trailer pipeline:** `DCC.Trailer capture <dir>` plus `Tools/make_trailer.py`, which renders frames at
    30 fps and has ffmpeg (`D:\Tools\ffmpeg`) add the letterbox, title and `Content/Songs/Swept Away.mp3` into
    `D:\Builds\Fourteener_Trailer.mp4`. Waiting on Zoe's hat fixes before the final render.
- **2026-11-13 (evening): summit celebration, climbing tuning, bench cosmetics, ice.**
  - **Summit:** `Arcade/DCCFireworks` plays a synthesized fanfare for everyone, shows a big "<NAME> REACHED THE
    SUMMIT!" banner, and sets off ten bursts of fireworks over the peak (lit instanced sparks plus synthesized
    pops; no pack has firework art or sounds). The climber gets +150 XP and a Climbing skill mark. There is also
    +2 XP for every new 100 ft climbed.
  - **Climbing is harder and rewards Rank** (`DCCRuleset`). Cost per second is `17 - 0.6*Rank` (min 6), and speed
    is `70 + 8*Str + 14*Rank` cm/s. A Rank 1 novice spends about 70% of a Con 4 bar on a 6 m pitch; Rank 10 about
    a quarter, at twice the speed.
  - **Ledge fixes:**
    - Climbing up beside an outcrop now crawls over it.
    - The crawl-over no longer treats a chest's pickup sphere as "in the way". That was the face 0 stall.
  - **Gear bench:**
    - Category buttons (All, Hats & cosmetics, Main Hand, Off Hand, Head, Body, Trinket), a search box, and A-Z
      sorting.
    - The arcade cosmetics (hats, packs, shoulders, auras) are listed and can be tried on and nudged. They save as
      grips keyed `Cosmetic.<Id>`, and the wallet applies them everywhere.
  - **Ice patches** are now rounded glazes painted with `Ice_Station/MI_ice_01`, not blank pale rectangles.
  - **Climb test:** stays on the summit 8 s, walks across ledges and High Camp, logs what is above any stall, and
    reports stamina per 6 m pitch. Faces 0, 1 and 3 summit. Known: the bot can grab a ledge slab's outer face
    from below and drop back about 5 m (harmless, but worth a look). Face 2 has not been rerun since the fixes.
- **2026-11-13 (night): cosmetics follow the head, and the bench can take things off.**
  - Hats and other head cosmetics now turn and tilt with the head bone, and packs with the upper spine (auras
    stay level). The wallet compares each anchor bone's current rotation with its rest pose, so it works on every
    Meshy body even though their bones point different ways.
  - Gear bench, **ON THIS BODY**: click the body (or the Show/hide button) to list everything it has on, gear and
    cosmetics, each with **Nudge** and **Remove**.
  - Not yet seen with an animation playing. If a hat sits tilted on one body, a "Grip: this body" nudge fixes it.
- **2026-11-13 (late): Repo Rally got a Fourteener-style kit.** Chosen because the race was the thinnest of the
  arcade games (oval, four repo men, laps, tickets by place) and a race maps cleanly onto that kit. Dead Air
  already has its footage loop. Neon Mile was just playtested and already has traffic and boost pads.
  - **Items** (`Arcade/DCCRallyItems`), all Repo Rally:
    - Tools, used with **Q** and cycled with **F** (the car's **T** is the radio): Tow Cable (slows the car
      ahead), Wheel Boot (a clamp dropped behind you), Lien (spins out the car leading you).
    - Boosts, which work the moment you drive over them: Nitro (a long boost), Studded Tires (the next wall,
      spike strip, boot or tow truck does not stop you), Quiet Plates (the repo men stop shoving you).
    - Seized goods in the infield: Pink Slip and Lot Manifest are keepsakes; Seized Chain sells for 80 tickets
      and the VIN Plate for 45. They bank into the profile's `Trophies` only if you **finish** (place above 0).
      A DNF, and the lot keeps what you were carrying.
  - **Pads** are per driver, so friends never race for the only copy. Orange pads restock every 18 s. The cyan
    pit crate, a step in from the start, hands out one of everything once. The gold infield pad is the seized
    goods, once. The repo men do not pick items up.
  - **The kit lives on the rover**, mirrored from the arena. While driving, the controller possesses the car
    and the crawler is unpossessed, so a wallet on the crawler would not replicate to the driver. Ticket
    notices during the race leave from the car the same way.
  - **Hazards**, replicated as where and when so every machine draws the same thing: a tow truck crosses a
    straight after a three-second warning, and a spike strip covers part of a straight (the other side is
    clean). The repo men shove when they pull alongside. Two players in a line draft: both a little faster,
    and the leader banks up to 40 extra tickets.
  - **The lot is dressed** with impounded synthwave cars, a fence, lamps and a Mile High Impound office.
    Scenery does not collide, so a detour is not a trap. The infield goods sit far from the racing line.
  - **Payout** is one notice: DNF 10 tickets, else 60 / 45 / 35 / 25 by place, plus the draft bonus, plus 25
    if you brought cargo home. The standings panel stays up for the whole race. The prize counter sells
    seized goods that have a ticket value.
  - **Verified:** DebugGame of the standalone game target compiled and linked
    (`DungeonCrawlerDenver-Win64-DebugGame.exe`). That binary is not what the editor runs.
  - **Not verified in game.** New `UFUNCTION`s, `UPROPERTY`s and `FDCCRallyBoot` need the editor closed and a
    Development Editor build before they exist in PIE. Live Coding will not pick them up.
- **2026-11-13 (late, again): the impound has a staff, and the grid fits the party.**
  - **Lot Boss Rhee, Clerk Jan and Flagger Diaz** (`Arcade/DCCImpoundNPC`, lines in
    `SourceArt/Voice/NPC/impound_lines.json`). The Voice Studio lists them under **Mile High Impound**,
    recorded on the Clean preset (your voice, no robot). Exports land in `SourceArt/Voice/NPC/<Speaker>/`.
    They stand off the asphalt
    beside the grid. Rhee says what the race is, Jan says what the pads and the seized goods are, Diaz says
    what the tow truck and the spike strip do. A driver hears whoever is nearest. The first line is the one
    that lands before the flag; the lights wait 22 seconds so there is time for it. No recordings yet, so the
    lines are text until someone records them.
  - **Multiplayer:** the party travels together (listen server, up to 16). Everyone gets their own car, their
    own pads and their own tickets. Four of you start in the front rows with the repo men right behind, which
    is the race as it was. A fifth and onward push the repo men back a row, so nobody spawns in the same
    square. Drafting is between two players. The repo men still do not pick items up.
  - **Not verified in game.** Same editor restart as the kit.
- **2026-11-13 (night, later): Repo Rally is a city shift.** Zoe corrected the "it is a race, not a repo" pass.
  The job is repossessing cars across a city, and the race is who nabs them fastest.
  - **The reel.** Drive up on a marked car and press **Q**. Stay with it. Fatigue fills faster the slower
    the mark is actually moving, and being hooked drags the mark down, so it gets easier as it tires.
    A net, traffic, or a second truck on the same hook all speed that up. If the line stretches too far, it snaps,
    and a mark you let go of catches its breath. Two trucks finish one car faster. Splitting up covers more of
    the city. A nab's value is higher when it was quick, and the crew on the hook splits it.
  - **The city** is a 5-by-5 industrial grid, about 800 m across, built from blocks: warehouses, stacks,
    sodium lights, two street gas stations, and a dispatch yard in the southwest block. Heavy traffic is
    ghost cars that steal your speed when you drive through them, and that slow a mark you herd into them.
    Amber columns (and a percent once the reel has started) mark the targets. The minimap is bottom left.
  - **Roles, this shift only, not saved.** Bays by the gate: Runner (faster, weaker hook), Net (Q throws a
    net after you switch to it with F), Winch (longer line, harder pull), Tank (fuel lasts, boosts last).
    You start with a tab of 48. Street gas and the bays bill the tab, and the tab can go red. The yard pump
    is free until you owe, then Jan closes it. In debt, Night Shift hunts you and a second debtor pulls the
    other crew off the marks. A rival who does more of the reeling than you do takes the car.
  - **Three minutes.** Tickets are by banked score, plus a place bonus. Rhee, Jan, and Diaz say the rules.
    Jan's party hat is gone. The lines are in `impound_lines.json` (Voice Studio, Clean preset, no new gear).
  - **Look.** Grungy industrial blockout. The synthwave car meshes are not used. The shopping list, with
    scale and import paths, is `Docs/RepoRally_Models.md`. Style shots are in `SourceArt/RepoRally/reference/`.
  - **Not seen in game.** New rover fields (`RallyDuty`, `RallyPlate`, `RallyStrain`, plus the earlier
    `RallyEngine` / `RallyBoostExtra` / `RallyBody`) need the editor closed and a Development Editor build.
    DebugGame of the standalone target does not deploy into a running editor.
- **2026-11-13 (night, later still): the hook is a shot, and the drop is at the yard.** Zoe replaced the fatigue reel. The city, gas, debt, roles, nets, beacons, and minimap stay.
  - **The shot.** Right mouse raises a short aim camera (the mouse owns it; it does not recenter). Q fires along that camera. The server sweeps from the truck. A marked car starts a reel onto the back bumper. Any other car is a fine (score and tab). A wall or a miss is just a miss. A miss that passes close to a skittish car spooks it.
  - **The tow.** The line pulls the mark onto the bumper. It pulls faster when the mark is slow, and a net still slows a runner. Pulling through the cars boxed around a mark scars it and drags. Boosting while it is on the bumper scars it too. Q in the yard, while it is docked, drops it. Q anywhere else cuts the line and pays nothing. Pay is speed from hook to drop, plus condition. A wreck (damage over 80) pays poorly.
  - **The waves.** The shift starts with parked cars. Around 40s they are boxed in, around 85s they drive, around 130s some of them leave the ground when a truck gets close or a hook misses. One line per mark. A second truck nets, blocks, or takes a different car. Rival crews only hunt debtors. They do not take marks.
  - **Words.** The gate, the rules board, the status lines, the kit blurbs, the lobby pitch, and Rhee, Jan, and Diaz all say aim, bumper, yard. Lines regenerated into `impound_lines.json`. No new recordings.
  - **Not seen in game.** `Server_FireHook` and `bRallyHeld` are new reflected fields, on top of the city fields that were already waiting on an editor restart. DebugGame of the standalone target does not deploy into a running editor.
- **2026-11-13 (late): prop textures fixed; Fourteener feel, sound and light (compiled, NOT yet deployed).**
  - **Black prop textures fixed and saved:** Dune Popcorn, Pokemon Master Trainer, Minion Eyes, Scott's lacrosse
    stick, the water bottle. Cause: Meshy's unnamed glTF materials all imported as one `Material_0` (see
    CLAUDE.md). Each now has `Props/<Name>/M_<Name>` with its own texture; the original meshes point at them.
  - **Falling** (`Arcade/DCCClimbAmbience.cpp`): gravity x1.75, air control 0.12, lateral friction while falling.
  - **Climbing look:**
    - Climbers sit 18 cm further off the wall (faces were sinking into the cliff tiles).
    - The crawl-over arc clears the lip by 6 cm instead of 25.
    - The body tilts with the path over a ledge (`TickClimbTilt`), belly to the rock, up to 80 degrees.
  - **Edge detection:** with no rock behind you at chest or waist height, you let go and fall (no more climbing on
    air past the mountain's corners).
  - **Hazards are heard, not read:**
    - `Warn()` no longer posts text.
    - Rocks crack loose and land where they are in the world (synthesized, 3D, audible up to about 120 m).
    - A landing within 20 m shakes the screen (`AddScreenShake`, zero-sum control-rotation wobble).
    - A gust whooshes past, loud on its own face.
  - **Ambience:**
    - Endless synthesized wind, louder with height and gusts.
    - Birds below 45% height.
    - StarterContent fire crackle at the campfire and the summit brazier.
    - `Swept Away` six seconds in, then again every few minutes.
  - **Light, every arena:**
    - Sun x1.5 and sky fill x0.55 (real shading instead of flat).
    - Fog x0.3 (it was bleaching everything past a few metres).
    - A post-process volume: exposure -0.4, bloom 0.25, contrast 1.08, saturation 1.1, slight vignette.
  - Needs: editor closed, Development Editor build, then a look in game. The tilt shows only for the climber
    and the host (other players see the crawl without the lean).
- **2026-11-13 (night 2): climb pace and loop, taller mountain, hotbar, Den decor.**
  - **Fourteener is 160 m** (`TopZ` 16000, High Camp at 80 m), with ledges every 8 m (express 15 m), 4.2 m deep.
    The round is 15 minutes.
  - **Overhangs between ledges:** 45-degree wedges you cannot crawl over, so you traverse around them.
  - **Climbing pace:**
    - Speed is `25 + 4*Str + 5*Rank` cm/s (a Rank 1 novice moves about 46 cm/s); stamina cost per second is
      `5.2 - 0.18*Rank`.
    - The climb clip's play rate follows real vertical speed (`TickClimbAnimRate`; 29 cm/s = 1x). It pauses when
      you hang still and runs backwards on the way down.
    - The ledge crawl moves at 2.2x wall speed.
  - **Seamless climb loop:** `A_DCC_ClimbLoop`, made by `Tools/make_seamless_loop.py` from the re-baked stair clip.
    The hips drift (85 cm up, 33 cm forward per cycle) is removed, and the last 12 frames ease into the first.
    `DCCCrawlerAnimSet` prefers it, and falls back to the stair clip.
  - **Hotbar:** 1 = first skill, 2 and 3 = quick items, 4 and 5 = next skills. Quick slots start empty and fill
    with the first useful pickups (Fourteener tools via `GiveItem`, consumables via `AddConsumable`). The key uses
    the item; an empty item frees its slot.
  - **Arcade hands:** hand-socket gear is hidden in the arcade arenas. The save is untouched.
  - **The Den:** each vote pad is dressed as its game (`Arcade/DCCDenDecor.cpp`).
  - **Gust:** no text any more (it bypassed `Warn`); fixed in code, needs a build.
  - **Repo Rally:** reviewed and functional in code (hook, reel, tow, drop, waves, debt, roles, payouts). Zoe wants a
    vaporwave city, dense decor, solid traffic to steer around, a tow camera that frames both cars, and an
    on-screen drop prompt. Not started: Grok was mid-edit in the same files.
  - **KMHR 5280 AM radio show** (*Altitude Sickness* with Dale Brandvold): a conspiracy call-in show with sponsor ads
    that keep cutting in. The script and cut guide are in `Docs/radio-show-altitude-sickness.md`. All 102 lines are in
    the Voice Studio under "KMHR 5280 AM" (`SourceArt/Voice/NPC/radio_lines.json`). `Tools/radio_mix.py studio`
    turns recorded lines into radio-processed segments under `SourceArt/Voice/Radio/out/`, for the rover's
    `RadioTracks`. Nothing recorded yet. The rover still plays only `RadioTracks[0]`; shuffling needs a small code change.
- **2026-11-13 (night 3): Repo Rally taken over from Grok (Zoe's call); arcade hotbar trimmed.**
  - **The city is vaporwave:**
    - Synthwave and Cyberpunk towers and domes (with collision).
    - Neon lane edges and centre dashes, lamps (1 in 4 lit) and palms along every curb.
    - Billboards, a hologram and a light on every block corner.
    - A neon arch gate and a glowing drop pad.
    - The big striped sun and mountains past the wall, under a pink dusk sky.
    - Seen in game, with no missing meshes.
  - **Traffic you steer around:**
    - Three traffic cars per street (was one). They are solid to trucks but pass through each other
      (`ADCCRover::TrafficShell`).
    - Two parked cars per street segment, in the lanes, with hidden collision.
  - **Tow camera:** while a car is hooked, the boom lifts and pulls back (up to 36 m) and centres on the midpoint
    between the truck and the car.
  - **Drop prompt:** centred, while a car is on the line. "[Q] DROP IT" in the yard (it pays); otherwise
    "BRING IT TO THE YARD" or "REELING IT IN...", with "[Q] cuts it loose (no pay)".
  - **Not seen yet:** the camera, the prompt and the traffic collision. They need someone driving, and no input
    can be faked here.
  - **Arcade hotbar:** shows only 1 to 3. The main game keeps 4 and 5.
  - **Voice Studio character voices:** the VOICE rack now has Pitch (0.1 st steps), Nasal, Deep / chest and Breathy / airy
    (`Companion/src/studio/chain.js`). On a cast line, tuning any of them saves to that speaker's `voice` in its
    `*_lines.json` (the game ignores the field), and picking any of their lines loads it. "Reset voice" clears it. The
    KMHR cast has starting voices; the Fort Tiner and impound casts start neutral.
  - **KMHR reader scripts:** `python Tools/radio_sides.py` writes one sheet per reader to `Docs/RadioScripts/`
    (casting in `SourceArt/Voice/Radio/cast.json`; Zoe has Dale, the Announcer and Marjorie). Lines are numbered like
    3.05, recorded ones are ticked. Friends name files by number; `python Tools/radio_mix.py import <folder>` files them
    for the studio and never overwrites an existing take without `--replace`.
- **2026-11-13 (late night): Fourteener hunger and strands; Repo Rally traffic, briefing and night.**
  - **Fourteener:**
    - Stamina drains `7.8 - 0.25*Rank`/s, about one 8 m pitch per bar for a novice.
    - The climb clip plays at 1x for 70 cm/s, so it is no longer faster than you move.
    - **Hunger:** drains over about 6 min (twice as fast on the wall). It caps max stamina at 35-100%, and
      starving halves recovery.
    - **Food:** Granola Bar, Green Chile Burrito, Jerky, Apple, and Trail Mix (which now feeds you too). Every
      ledge without a chest has a food cache, and there are three at the fort. Food fills quick slots.
      Everyone has a kit from the start, so hunger always runs.
    - **Gusts:** shove you ~1 m/s along the face, can tear you off below 20% stamina (a Wind Cloak halves the
      shove and holds you on), and roar on your own face with a slight shake.
    - **Sound:** far rockfall is quieter (falloff 250/5000).
    - **Shape:** ledges are 5.6 m deep and 9 m wide. Four winding strands (spires stepping round the trunk to
      64/80/112/144 m) and three rock layers (40/110/135 m). Overhang wedges are slimmer, with 2.4 m of
      headroom over the ledge below.
    - **Signs:** the route board and a new inner gate sign face the fort; the backwards outer sign is gone.
    - **Crawl fix:** the "is there a top?" trace starts just above what you bumped into, and ignores
      start-inside hits. Before, a start point inside the next rock gave a fake flat top. Tops must be under
      ~30 degrees of slope.
    - **Climb test:** every face is clean (no jumps, drops or rock clipping). The bot now stops under wedges by
      design: it climbs straight up, and wedges are meant to be traversed around.
  - **Repo Rally:**
    - **Play-again vote:** works while driving (`ADCCRover::Server_ArcadeVote`).
    - **AI cars move** (traffic, crews, running marks). They had no controller, so the floating-pawn movement
      ignored them. They get an AIController (`TickTraffic`), and the rover's keyboard branch now requires a
      PlayerController.
    - **Traffic:** drives the grid in the right-hand lane, turning at crossings with no U-turns, from load.
      Verified in the log.
    - **Parked cars** sit curbside.
    - **Low fuel** (<35%): pulsing green beams at both stations, and an edge arrow with the distance.
    - **Briefing:** from load to 12 s after the flag, a "TONIGHT'S MARKS" panel, pulsing markers or edge arrows
      on every mark, and Dispatch's `RR_Disp_Brief` on the radio. The pre-flag rules text is cut to the
      countdown.
    - **Hook aim:** zoomed view (58 FOV) and a pulsing green lock ring with plate and distance. Q fires at the
      locked mark.
    - **Ramps and boosts:** kicker ramps, two 9 m neon skyways with boosted descents, and chevron boost pads.
    - **Night:** sun below the horizon, stars, a moon, pinned exposure, more bloom.
    - **Sound:** city rumble and hum, pass-bys, horns, sirens. `/Game/Songs/Repo_Rally` is timed to end with
      the shift.
    - **Voice:** 22 new lines in `impound_lines.json`, including a Dispatch radio voice (`RR_Disp_*`, played
      when recorded). `gen_impound_lines.py --merge` adds rows without touching studio edits.
    - **Start camera:** the crawler spawns behind the truck, not inside it.
  - **Voice studio:**
    - Imported takes are saved as raw takes now (they were lost on restart).
    - A line shows done whenever its exported WAV exists. Gus's four lines were exported but unmarked.
  - **Trailer for Repo Rally:** held until Zoe says it is polished enough.
  - **Terry is an AI voice:** `python Tools/radio_tts.py <Character>` voices a KMHR character's unrecorded lines
    with Kokoro (offline), with their studio voice applied, into the studio's folders. Terry's 20 lines are done
    and listed in `SourceArt/Voice/Radio/ai_takes.json`. Segments 1 and 3 now mix complete.
- **2026-11-13 (night, later): Repo Rally street life, and the ad.**
  - **Street life** (`ADCCArcadeKart::DressStreetLife`): courtyards (garden, plaza pad, a landing pad and a
    hover car), a storefront in the sidewalk gap of each block face, and a fence around the yard with the
    gate left open. Nothing in the pass collides, and it stays off the lanes. The towers already fill the
    corners, so the new props sit only in the gaps those towers leave.
  - **The ad** (`DCC.RallyTrailer`, `Tools/make_trailer.py --rally`): the same 45-second still-frame cut as
    Fourteener. The gate and the city, the four bays, Kyle on a hook, Zoe on the skyway, the drop. Song is
    `Content/Songs/Repo Rally.mp3`. Output `D:\Builds\RepoRally_Trailer.mp4`.
- **2026-11-14: Fourteener stamina made fair and hard; sideways and wind fixed.**
  - **Why one bar lasted the mountain:** Fourteener used the crawler's real DCD stats. A levelled crawler's bar is
    ~360 (Constitution and gear), three times the 120 it was tuned for, and higher Strength also climbs faster.
  - **Fair bar** (`ADCCCharacterBase::ClimbStaminaReferenceMax`, set to 120 by Fourteener):
    - Climbing costs and recovery scale by `MaxStamina / 120`.
    - The up-climb is charged per metre (speed / 46 cm/s x 0.70).
    - Everyone gets about one 8 m pitch plus the crawl over the ledge per bar, and a rest takes ~9 s for everyone.
    - Measured in the climb test: 77% of the bar per pitch, plus about 25% for the crawl.
  - **Sideways on the wall** cost nothing and ran at 6 m/s (flying mode's default speed). It is now 70% of your
    climb speed, brakes hard, and costs climbing stamina.
  - **Gusts** now push you 1.7 m/s along the face (0.85 with a Wind Cloak), faster than you can traverse against.
    The push is applied on the server and the climber's machine alike (`FDCCClimbAmbience`), so prediction no
    longer erases it. Below 30% stamina a gust can tear you off.
  - **Getting into a car is immediate.** The "getting in" beat was the 3-4 s agree-gesture clip, played rooted.
  - Not started (Grok is on Repo Rally): the 1-in-3 angry owner who bails out and fights.
- **2026-11-14 (later): the repo truck's look, glowing marks, camera.**
  - **The truck looked mangled** for two reasons:
    - **The UV channel.** Grok's import put its lightmap on UV channel 0, the texture channel (`LightMapCoordinateIndex` 0).
      Clean re-imports of the same FBX pieces are in `DCC/Vehicles/RepoTruck/Clean/` (lightmap on channel 1), and the
      rover uses those.
    - **A magenta Fresnel rim** in `M_RepoTruck` lit nearly every face of the faceted mesh.
    - **New material:** `M_RepoTruckClean` (base colour, normal, metallic and roughness from Meshy's maps, plus a
      gentle "NeonMarkings" emissive at x5).
    - Neon underglow (pink sides, cyan ends, two small lights under the chassis).
  - **Marks glow:** `M_MarkGlow` (additive, unlit, Fresnel rim) as an overlay material, each mark in its own colour
    from its plate (`ADCCArcadeKart::MarkColour`). It pulses and brightens as the reel closes. The beacon, the
    briefing list and the hook lock use the same colour.
  - **Mark markers persist all shift** (only the "TONIGHT'S MARKS" text panel ends after the briefing).
  - **Camera:** firing the hook leaves the aim view. The aim zoom returns to a fixed 90 FOV (a shared static could
    get stuck narrow). The tow camera pulls back and up behind the truck instead of recentring between the two
    cars. The wrecker gets a longer, higher chase.
  - **Trailer:** `python Tools/make_trailer.py --rally` (Grok's `DCC.RallyTrailer`) with `Content/Songs/Repo Rally.mp3`,
    written to `D:\Builds\RepoRally_Trailer.mp4`.
- **2026-11-14 (night): full-detail repo truck, the camera bug, and the Repo Rally ad.**
  - **Full-detail truck:** the truck is now Meshy's original, not Grok's ~51 k-vertex simplification.
    `Tools/resplit_repo_truck.py` fits the 1.6 M-vertex original onto Grok's pieces (scale 5.9, a 90-degree turn),
    hands every face to the nearest piece, and exports Body/Boom/Magnet around Grok's pivots, in centimetres.
    They are imported to `DCC/Vehicles/RepoTruck/Full/` with Nanite and `M_RepoTruckClean`, and the rover uses them.
  - **Truck lights:** a key light and a rear fill (the paint is near-black at night), plus headlights.
  - **Camera bug, mine:** the aim-zoom FOV block had been inserted between the camera's if / else-if branches, so
    the chase and tow cameras never ran. After a hook shot the view stayed in the aim pose. It is moved after the
    chain. The wrecker's chase arm is 14 m, raised 4 m. Measured with a temporary `[DCC Cam]` log (removed).
  - **Ad:** `D:\Builds\RepoRally_Trailer.mp4` (1080p, 57 MB) plus `_720p.mp4` (8 MB), with `Repo Rally.mp3`.
    The close pass along the truck's side is still darkish.

### 2026-11-14 (later): Repo Rally brighter, towing that respects walls, real menus
- **Brighter night.** The street grade is now pinned at EV100 -0.5 (was 0.3..1.2 with a -0.6 bias) and the sky fill is 2.6
  (was 0.22). `DCC.RallyExposure <ev>` changes it live for tuning (lower is brighter).
- **Towed cars no longer clip into buildings.** On the bumper, the mark used to be snapped every frame to a point 7 m behind
  the truck with no collision check, so every turn swung it through the roadside. It is now a trailer: its nose hangs under
  the magnet, its body trails along the truck's path, the move is swept and slides along walls. The reel-in is swept too.
- **Menus** (`UI/DCCHUD_RallyMenus.cpp`; Repo Rally no longer uses the shared centred status box):
  WORK ORDER before the flag (marks with distance, the four steps with key caps, what pays, controls, a big countdown);
  during the shift the clock + wave top middle, THE BOARD (marks, state, reel bar, scars) top left, CREW standings top right,
  YOUR TRUCK (fuel, tab, banked, tool, what is on the line, radio) bottom right, the map restyled to match;
  SHIFT REPORT at the end (drops, fastest, fines, tab, banked, tickets, your own line) above the vote buttons.
  Data: replicated `ADCCArcadeKart::Crew`, `Board`, `FlagAt`, `RallyWave`, filled in `RefreshBoard`.
- Not hands-on tested: towing around a corner (code only). Screens checked in a solo round.

### 2026-11-14 (afternoon): Repo Rally owners fight back, truck hull, graffiti, menu polish
- **Owners fight back** (`Arcade/DCCRallyThug.*`, `Arcade/DCCRallyThugs.cpp`). About one hook in three (never in the first 20 s):
  a parked or boxed-in mark's owner comes out of the building. A Brawler (Quinn body) runs at the truck and punches it; a
  Shooter (Manny with the boilerplate rifle) stops at ~32 m and fires. Driving into one above ~30 km/h knocks it flat.
  A driving mark's driver instead stays in and shoots at the truck that has the line, until the drop. Tracers, muzzle
  flashes and a synthesized bang/thud (`ADCCRallyThug::PlayHit`, via `ADCCArcadeKart::Multicast_RallyHit`).
  The HUD tags them ANGRY OWNER / ARMED OWNER with whose car it was.
- **Truck hull.** `FTruck::Hull` 0..100, replicated on the crew row. Owners, shots and rival rams (12) take it. Under 50 the
  truck slows; at 0 it is WRECKED: the magnet lets go of the tow and will not fire. **Sitting at a gas station repairs
  20 hull a second** (free; the yard pump repairs too unless you owe). The station roof hides while your own truck is under
  it, because the chase camera sits above it. A red screen edge flashes when you take a hit.
- **Graffiti.** The Cyberpunk pack's vandalism decals, sprayed on the real tower faces (a trace from the street finds the
  wall), Brutalist wall grime, puddles in the gutters, tags on the station aprons. The sidewalk kiosks, benches, bins,
  columns, tag boards and courtyard planters now collide.
- **Menu polish.** Panels have a shadow, a tinted header and corner brackets. THE BOARD: numbered colour tiles, state pills,
  distance plus an arrow toward each mark, the reel bar, scars. CREW: place medallions, hull and fuel bars per driver,
  a DEBT tag. YOUR TRUCK: hull and fuel bars, tab and banked chips, REPAIRING / WRECKED status. The work order lists the
  owners and the hull.
- **Ad fix.** The drag-out shot ended 20 m too far and drove the truck into the far planter; it now stops mid-street.
- Dev check: `DCC.RallyTrouble brawler|shooter|hurt|station [delay]`.
- Not hands-on tested: running an owner over, and a driver shooting from a hooked runner (code only).

### 2026-11-14 (later still): HUD polish pass ("huds and meters should be the easiest to nail")
- **Crisp text.** Repo Rally's panels drew the engine's bitmap fonts scaled up (soft). They now draw Slate text from the
  engine's Roboto asset (`ADCCHUD::RallyText` / `RallyTextSize`, drop-in for DrawText / GetTextSize). Found on the way:
  Canvas draws **nothing** for `FCoreStyle::GetDefaultFontStyle` (no font asset behind it), so the speedometer's number
  had never been showing. The gauge uses the Roboto asset now too.
- **Speedometer** (`UI/DCCHUD_VehicleGauges.cpp`, every vehicle game): a solid dash pod on the bottom edge with a neon
  rim, 34 lit speed segments with a glow, a 20-segment boost row, the number and BOOST READY - LMB inside. Drawn with
  solid triangles (`DCCGauges::Band`; `RenderCore` added to Build.cs for `GWhiteTexture`), not stacks of lines. The
  altimeter only slides in while you are in the air (it used to sit there reading 0).
- **CITY map:** dark blocks between neon streets, the yard in green, G stations, marks as diamonds in their board
  colour and number, crews and angry owners in red, other players cyan, you as an arrow that turns with the truck,
  and a legend inside the panel.
- **Marks in the world** carry the board's number tile and colour; off-screen marks are a solid chevron with the number
  and distance (no more names piling up on the screen edge); tags push down instead of overlapping. Low fuel uses
  the same chevron and a GAS pill.
- **Toasts** are a card with an amber edge, under the shift clock in Repo Rally (off the road). The tow prompt is a card
  with a Q key cap. The hook lock label is a pill. The work order's marks use the numbered tiles; its bottom rows
  no longer collide with the footer.

### 2026-11-14 (evening): NPC Forge, item placeholders, character shade
- **NPC Forge** (`Tools/npc_forge.py`, Blender + MakeHuman's MPFB 2.0.18). A seed and a role (casual / worker / office) give
  the same person every time: MPFB's randomizer for the body (kept to adult ranges), a skin matching age, sex and
  ancestry, hair recoloured by ancestry and greying with age, dyed plain tops, a role outfit, sometimes a hat or glasses.
  The 13 CC0 MakeHuman asset packs (~1 GB) are in `D:\Assets\MakeHuman\zips` and installed into Blender's MPFB user data.
  `--preview` renders a lineup, `--export` writes FBX + `npcs.json`. The first 12 (seeds 300-311) are in
  `/Game/DCC/Characters/NPCs/` (`Tools/import_npcs.py`: one shared skeleton, textures at 1K, master `M_NPCPart`).
- **Retarget:** `DCC.NpcRetarget` (editor only, `Arcade/DCCNpcRetarget.cpp`; run with
  `UnrealEditor.exe <project> -ExecCmds="DCC.NpcRetarget"`) builds `IK_DCC_Manny`, `IK_DCC_NPC` and `RTG_Manny_To_NPC` in
  code and batch-retargets 12 mannequin clips (idle, walk, jog, three punches, two deaths, rifle idle/jog/fire, hit) into
  `NPCs/Anims` as `NPC_*`. Build.cs adds UnrealEd/IKRig/IKRigEditor for editor builds only.
- **Repo Rally owners are NPC Forge people now** (picked from the plate), on the retargeted clips; the mannequins are the fallback.
- **Item placeholders:** 31 models built in Blender (`Tools/build_item_placeholders.py`, contact sheet in
  `SourceArt/Placeholders/sheet.png`), imported to `/Game/DCC/Equipment/Placeholders` with flat-colour instances of
  `M_DCCPlaceholder` (`Tools/import_item_placeholders.py`, `Tools/colour_item_placeholders.py`). Guns, crossbow, nail
  and flare guns, hard hat, goggles, hairnet, headset, bifocals, crown, halo get `Mesh`. Clothing and trinkets get the new
  **`FDCCEquipmentDef::GroundMesh`** (folded garments, a ring in its box, a cheque...) so they show on the floor but
  nothing is bolted to a body. SecuritySidearm uses UnrealBucket's revolver. Every DT_DCCEquipment row now has a model.
  Grips and head offsets are untuned (F3 bench).
- **Loot batch imported:** `Tools/import_loot_content.py` (owed since 2026-11-13) ran; it needed `rv()` to unwrap the
  JSON string `get_rows` returns. DT_DCCEquipment is now 106 rows.
- **Character shade** (Zoe: characters looked "washed out grayscale... brighter and fluorescent than their surroundings").
  `ADCCArcadeArena::ApplyCharacterShade` puts `M_CharacterShade` (translucent, unlit) on every character as an overlay:
  a blend toward the scene's own colour and a soft rim in its light. Automatic from each arena's FSky (darker scene =
  stronger; tint and rim from FogColour); `FSky::ShadeStrength` / `ShadeRimStrength` override it, 0 turns it off.
- **Gotchas found:** Blender FBX for Unreal must be centimetres natively (scene unit 0.01, scale x100, apply): metres
  make Unreal scale the root bone x100, the mesh looks fine and every retargeted clip flings the body out of sight.
  Static meshes: `apply_unit_scale=False, FBX_SCALE_NONE`. A material used as a skeletal overlay or on a skeletal
  mesh needs `bUsedWithSkeletalMesh` set in the editor, or it does not draw in the game.
- Not checked yet: grips on the new guns, head items on the five crawler bodies, owners up close.

### 2026-11-14 (night): Stress Test, a damage check in the Den
- **Dead Air** was already the haunted-house game (found footage in the Capitol Hill mansion, `Arcade/DCCArcadeHaunt`). It is on a vote pad. It has not had a play pass.
- **Stress Test** (`Audit`, `Arcade/DCCArcadeAudit`) is new. Forty seconds on The Quarterly, a construct that records damage and does not fall down. Fists, weapons, spells, throws and explosives all count. The party total is the rating (80 / 180 / 340 per crawler: Meets, Exceeds, Standing Ovation). Tickets pay the rating plus a little for what you personally landed. The Den board keeps `Audit.BestDamage`.
- The Den's room is wider so the sixth pad fits. `DCC.Arcade Audit` goes straight in.
- Not seen in game yet. This needs the editor opened on the build from this session.

### 2026-11-14 (later): angry owners yell, Fourteener stamina eased
- **Repo Rally owners.** Hooking a car brings someone out. Most of the time they only scream (a Yeller): they run up, shake a fist, and leave. About one hook in three, after the first 20 seconds, they still fight (brawler or rifle) or, if the car was driving, shoot from the seat. Driving over any of them still knocks them flat. Yellers do not take hull.
- **The words** are six lines for Pryce, Nia and Holt in `SourceArt/Voice/NPC/impound_lines.json`. The Voice Studio lists them under **Repo Rally · Angry owners**. The same plate always gets the same line. Text shows on a toast and over their head before anyone records. A recording at `SourceArt/Voice/NPC/<Name>/VO_<Row>.wav` is heard on the next hook. `DCC.RallyTrouble yeller` spawns one.
- **Fourteener stamina** drains 15% slower. `ClimbStaminaMultiplier` now starts at 0.85, so ice, gusts, a belay and chalk ease off by the same amount.

### 2026-11-14 (later): PLAY into the Den accepts input again
- Play (dev) loads the front door, and PLAY seamless-travels to the Den. The menu sets the viewport to ignore input. Travel swaps in a new controller before the old HUD can undo that, so the Den draws and ticks while WASD and the mouse are dropped.
- The new HUD's BeginPlay, on any world that is not the front door, puts the viewport back in game input and hides the cursor. The Den's own pads still open the prize counter and the chop shop when you step on them.

### 2026-11-14 (night): a package the party can join
- `Tools/package_game.ps1` cooks a Development Win64 game, copies `SourceArt/Voice/NPC` in beside it, and zips it without the debugger symbols. The cook itself lands in `D:\Builds\DCD\Windows`. An older folder next to that is the November 12 game, and the script used to zip that by mistake.
- **Send** `D:\Builds\DungeonCrawlerDenver-2026-11-14.zip` (3.71 GB). `D:\Builds\SHARE_THIS.txt` says the same thing. Everyone unzips and runs `DungeonCrawlerDenver.exe`. You click **HOST A PARTY** and paste the invite. They copy that message, type the crawler password (it starts as `bidet`), and click **JOIN A PARTY**.
- Cook directories now include the mannequin clips, the Repo Rally street props, the graffiti decals, and the other art the games load by path. A launch of this build reached the front door (`DCCFrontDoorGameMode`, map up in half a second). A second PC has not joined it yet.

### 2026-11-14 (night, later): outline marks, a bigger city, a truck and gear that do something
- **Marks.** They stay on for the whole shift as a coloured rim (`M_MarkGlow`, strength about 0.4, a little thicker while the line strains). Nothing sits over the roof. Rival wreckers still have a short red column. The minimap diamonds are the same.
- **City.** Streets are 240 m apart and three blocks out from the middle. The asphalt is 36 m wide, with four cars on each street. Marks park on the sidewalk. Extra gardens, holos and lamps fill the faces. Kiosks, benches, bins and columns still stop a truck. The new dressing does not.
- **Hull.** A real wall takes hull, about 8 from a bump and about 28 from a hard hit. Chop Shop plating cuts that, and it shortens the shake. Studded tires still shrug one hit. A gas station still patches the truck.
- **Chop Shop.** Each level is larger: plating and the tank 20%, the engine 15%, the line 25%, the pit 50%. The engine also raises flight speed, the tank makes a flight last longer, and plating shortens a flight crash. YOUR TRUCK lists the five levels.
- **In and out.** Repo Rally and Neon Mile start with you beside the vehicle. E gets in. The end of a round puts you back on your feet. Enter or Escape closes the work order for that flag, and the next shift brings it back. On foot, the corner says E get in your truck.
- **Ticket booth.** The hobby jetpack hops, then hovers until a short burn runs out. The trail pack eases a climb, the cape shortens a fall, the miner's helmet is a lamp, the shoulder plates turn some of a hit, the halo regens stamina faster, and the sponsor orbs walk a little faster. Hats stay looks. The counter also sells the confetti crossbow (80), the nail gun (120) and the flare gun (100). Fists still reach 2 m. Martial arts was left as it is.
- Linked into `UnrealEditor-DungeonCrawlerDenver.dll` at 9:27 PM (one DLL, no hot-reload copy). Not seen in a round yet. The friend zip from earlier tonight does not include this.

### 2026-11-14 (night, later still): Meet the Whoops, a spare can, the Arrears Coach
- **Radio.** `Meet the Whoops` joins the KMHR playlist once `/Game/Songs/Meet_the_Whoops` exists. [T] still turns the station on and off. [C], while it is on, goes to the next song already in progress. On foot, [C] is still the selfie.
- **The show.** *Altitude Sickness* is not on the cab. The Voice Studio has the 102 lines and recorded takes, and `SourceArt/Voice/Radio/out/KMHR_full_show.wav` (13 Nov, plus segment files, no S06) is the mixed show. The rover's playlist is still the songs.
- **Yard.** Rhee, Jan and Diaz already stay at the impound for the shift. They do not walk. Jan sells one spare can for 14 on the tab. It dumps itself when the tank hits empty. The Chop Shop long-range tank also holds 30 more per level.
- **Arrears Coach.** One a shift, about half a minute after the flag. It pays about two and a half times, and a stock engine tows it at under half pace. Engine level 3 is close to normal. It does not respawn.
- Source is written. Play (dev) was open, so this is not in the DLL she is running.

### 2026-11-14 (night, last): a longer climb, a heavier truck, a little more city
- **Fourteener.** Climbing lasts about 15% longer than the last ease (`ClimbStaminaMultiplier` 0.85 to 0.74). Stamina does not come back in the air or during a stall. It starts again after you have been on the ground for just under a second. A ledge you reached refills slowly. A camp still refills fast.
- **The truck.** It sits on its tires (the hover offset was being overwritten every frame). Throttle catches harder, the wind-up to cruise is a little longer, lifting off coasts less, and a hard corner scrubs a little speed. Jumps come down a bit sooner.
- **The city.** Six cars on each street, still in the lanes. A higher deck runs along the south street. Crossing arches and a few more parked flyers are decoration and do not collide.
- **Also in this link,** because the game was closed: Meet the Whoops on the cab (`/Game/Songs/Meet_the_Whoops`), [C] for the next song, Jan's spare can, the bigger tank, and the Arrears Coach. The radio show is still not on the cab.
- Future plans are in `Docs/grok to do.md`. Nothing on that list was started.
- Linked into `UnrealEditor-DungeonCrawlerDenver.dll` at 10:45 PM (one DLL, no hot-reload copy). Not seen in a round. The friend zip does not include this.

### 2026-11-14 (late): the prize counter redone, passwords, skill bar, Chop Shop, birds, a menu style
- **Menu style.** `UI/DCCMenuStyle` is one shared look for the System's windows: dark glass, rounded corners, a neon edge, spaced capitals, and glass buttons that light up on hover. The prize counter, the main menu and the party menu (F2) use it. New menus should use `DCCMenu::Window`/`Header`/`Button`, not flat `WhiteBrush` panels.
- **Item pictures.** `Tools/render_item_icons.py` (Blender) renders 29 icons to `SourceArt/UI/Icons`. `Tools/import_item_icons.py` (MCP) brings them in as `/Game/DCC/UI/Icons/T_Icon_<Id>`. Menus load them by name with `DCCMenu::IconBrush(Id)`, so a new icon needs no code.
- **Prize counter.** It has tabs (Cosmetics, Trophies, Chop Shop), tiles with pictures, and a detail panel with a large picture and the one action button. Grok's booth guns are kept in the Cosmetics tab.
- **Passwords.** Every crawler has one, and it starts as `bidet`. The host checks it on join (`?Pw=` carries a hash, never the password). Each PC remembers a password once it has been typed. CHANGE is on the main menu and in the party menu.
- **Skill bar.** A small bar shows while a skill is used (climbing, driving, shooting) and fills toward the next Rank.
- **Chop Shop.** It is paid in scrap, earned in Repo Rally the way tickets are. It has a booth and pad in the Den and a tab on the counter.
- **Guides once.** Work orders, the rover guide and the tutorial explainers show once for each player on each PC (`DCCSession::HasSeenGuide`).
- **Fourteener birds.** Two giant birds circle the peak. From 45 s in, every 11 to 18 s, one of them dives at a climber above 9 m (a red aim line first, 12% max HP, knocks you off the wall) or drops a rock. Test it with `DCC.BirdAttack [drop]`. `dcc.ClimbHazards 0` turns them off.
- **Package.** A new friend zip with all of the above, and everything Grok linked tonight, was started at 11:35 PM. Check `D:\Builds\package_status.txt`.

### 2026-11-15 (just after midnight): build stamp, guns kept, a cluttered city with people in it
- **Build stamp.** The main menu shows `BUILD <stamp>` at the bottom. `Tools/package_game.ps1` writes `[DCCBuild] Stamp=yyyy-MM-dd.HHmm` into `Config/DefaultGame.ini` before it cooks, and the zip is named after it. A host refuses a join from a different build ("Your game is a different version from the host's ... Get the latest build"). The editor reports `dev`, and a `dev` host lets anyone in, so testing is never blocked. Code: `Session/DCCBuildStamp`, the check is in `ADCCGameMode::InitNewPlayer`.
- **Booth guns are kept.** Buying a gun used to put one row in the bag and the gun in your hand. Buying a second gun replaced the first, and the first was gone. Guns are now owned on the profile like cosmetics. The counter says EQUIP for a gun you own and IN YOUR HANDS for the one you hold. Zoe's save holds the flare gun. The confetti crossbow she bought before this fix was lost and has to be bought again (or granted).
- **Repo Rally city.** `Arcade/DCCRallyClutter.cpp` puts a shopfront row on every block face (it collides), signs and AC units on the walls, bags, boxes and crates against them, alleys, and wires over the streets. About 6,700 pieces as instanced meshes (one component per mesh, not one actor per prop). 78 NPC Forge people walk the sidewalks and break into a jog when a truck comes close. They are local to each machine (cosmetic, not replicated).
- `DCC.Peek x y z yaw pitch [delay]` (dev) puts a still camera at a point relative to the arena. The first Peek of a run gets overridden while the round starts, so give it a later delay. It is for screenshots and trailer framing.

### 2026-11-15 (night): Gate Run, a new minigame
- **What it is.** A shining gate (the Alien_Portal pack's portal, lights and sparks) opens over a dark plaza after 8 s. Walk into it. Inside is a run of 3 to 10 connected rooms. The layout is `UDCCDungeonGrid`, the World Dungeon's own, built from a replicated seed on every machine. The room count sets the rank (3-4 E, 5 D, 6-7 C, 8 B, 9 A, 10 S), and the rank sets enemy levels and stats.
- **Rooms.** FIGHT. TRIAL: step in the ring for three waves 9 s apart, then a chest. VAULT: either a 140 kg pressure plate (two crawlers, or one plus the keg in the room) or a Lights Out breaker row, opening an `ADCCPuzzleDoor`. Forcing with E always works and costs Favor. A solved vault scores more and drops a chest. TREASURE: a guarded chest. THE GATEKEEPER is in the farthest room: The Interim Manager (Voldemortgage at rank A and S), scaled up, with Debt Eater adds. Killing it opens an exit portal in its room. Step through to clear the gate.
- **The clock** is 2.5 min plus 45 s a room. If it runs out the gate collapses, and what was cleared still pays. Score is room points plus 2 per second left. Tickets go to everyone (6 to 90). Profile stats are `Gate.BestScore`, `Gate.Clears`, `Gate.BestRank` and `Gate.Runs`. A death respawns you in the entry room.
- Held gear shows here (`ADCCArcadeArena::ShowsHeldGear`). Every other arcade game still hides it, so a bought gun is invisible in the Den and the other minigames.
- Code: `Arcade/DCCArcadeGate.h/.cpp`, `ADCCGateGameMode`, and a row in `DCCArcade::Games()`. The Den picks it up automatically.
- **Dev.** `DCC.GateIn [room] [delay]` and `DCC.GateClear [room] [delay]` (no argument means every room). An argument starting with `-` is eaten by `-ExecCmds`, and the first command in an `-ExecCmds` list is often lost, so lead with a throwaway one.
- **Verified 2026-11-15** in a -game run: the plaza, the portal, the enemies in the boss room, all clears (two vaults, fights, trial, treasure, Gatekeeper), the exit, tickets, the end vote, and back to the Den with the record on the board. That test wrote 84 tickets and a 2,354-point Gate record onto Zoe's profile. Not tested by hand yet: real fights, the plate puzzle and the breaker puzzle.
- **Seen, not fixed.** The Gatekeeper (HockeyWings body) and the Debt Eaters stood in a T-pose. Their clips are assigned, so `CanPlayOnThisSkeleton` is probably refusing them after the rig-fit skeleton change. That would affect the World Dungeon too.

### 2026-11-15 (later): gates pay as you go, one hat at a time, chests creak, enemies off the crawlers' bodies
- **Gate tickets.** Every room pays the party as it falls: fight 3, treasure 2, vault 4, trial 6, Gatekeeper 12, each plus the rank (0 for E up to 5 for S). Kills pay 1 each, batched every 5 s ("+10 tickets: 10 gate kills"). The finish pays less than before (4 to 60, from the time bonus and the rank), because the rooms already paid. Verified in a run.
- **One thing on your head.** A cosmetic hat going on puts Head-slot gear (hard hat, crown, hairnet...) in your bag, and Head gear going on takes the cosmetic hat off. Cosmetic hats already replaced each other. This is `UDCCWalletComponent::StowHeadGear` / `OnGearChanged`. Built, not tried in game.
- **Chest sound.** `Items/DCCChestSound` synthesizes a latch click followed by a wooden hinge creak. It plays when a Fourteener supply chest opens for you and when the loot menu opens on any loot chest (not on a body). Built, not heard yet.
- **Enemies are not crawlers any more.** The Interim Manager (the Gatekeeper's body) wore CJ, and the Nuclear Family wore three other crawlers. They are now NPC Forge people (`Characters/DCCForgeBody`: an office worker in a suit at 1.3x, two casuals, and a kid at 0.72) with the Forge clips.
- **Fixed the T-pose and the floating.** C++-spawned enemies (the World Dungeon's and the gate's) never had a Blueprint set up their body. They stayed in anim-Blueprint mode with no Blueprint (a T-pose), with the mesh at the capsule's centre (a metre in the air). `ADCCCharacterBase::BeginPlay` now switches a body with clips but no anim Blueprint to single-node, and drops a body left at the capsule centre to the capsule's feet. Seen in a run: the Gatekeeper punches, the Debt Eaters move, and both stand on the floor.
- The Repo Rally walkers now use all twelve Forge bodies.
- **Test leftovers on Zoe's profile.** The gate tests added about 136 tickets and a Gate Run record.
- **Weapons show everywhere again** (Zoe: "we don't need to hide weapons anymore. I just didn't want them distracting in the beginning"). `ADCCArcadeArena::ShowsHeldGear` now defaults to true. Seen: the flare gun is in hand in the Den.

### 2026-11-15 (morning): Dead Air moves underground
- **The place.** Dead Air is now filmed in the Lucky Tuesday, a silver mine under the mountain (shaft No. 2, closed 1893, the shift never came up). It is still the dungeon layout from a replicated seed, and the scoring, tape, deaths and uploads are unchanged. It only looks and sounds different.
- **Drifts** have rails and ties down the middle and a timber set every other cell. **Stopes** have a cart (Desert_cave), barrels and crates (Cave_Camp), spoil rocks against the walls (kept small; a 3 m boulder blocked the camera), stalactites from the roof, and timber props in the corners. Lanterns gutter warm orange. The deepest stope is red.
- **The cage** replaces the van: an iron frame with a waist rail, a crossbar and the hoist rope, plus the sign. Footage uploads only for crew standing in it when the clock runs out. The text says cage everywhere ("until the cage goes up", "GET BACK TO THE CAGE", "tape left in the mine").
- **Ghost miners** are in about two out of three stopes. Each is a Forge worker on a slowed idle, lit cold blue, with a blue-tinted rim overlay (`M_CharacterShade` MID). It flickers in and out and vanishes if you get within 4.5 m. It is worth 9 on camera, but only while it is actually showing.
- **Sound.** Three knocks in the rock (synthesized) somewhere near you every 14 to 30 s.
- **Helmet lamps** replace the flashlights: on the helmet, carbide yellow, dimmer (4500).
- **Dev.** `DCC.MineGo <stope|driftN|ghost> [delay]` puts everyone there. Seen in -game runs: the cage, stopes, the deepest stope, a ghost miner standing in its light, and monsters closing in. Not seen yet: how the blue overlay reads on a ghost, and the knock sound. A third-person camera in a one-cell drift gets pushed in close, so a drift may feel cramped. Worth a hand test.
- The Den's Dead Air pitch now reads "a silver mine under the mountain ... Be in the cage when it goes up."
- **Wider drifts** (Zoe asked for them): Dead Air's cells are 5.6 m (`TileSize` 400 to 560) and the roof is 4.8 m, so the third-person camera has room. The cage signs now face the crew when they spawn (they read backwards before).
- **The plan for traps and ghost stories is in `Docs/dead-air-mine-plan.md`.** Ten traps, each with a warning and a way past (rotten planks over a winze, firedamp and blue lamps, hanging-wall rockfalls signalled by three knocks, a runaway cart, a flooded sump, powder crates, blackdamp, the phantom bell, the lamp-snuffer, the cave-in clock). Five ghost stories told through notes on bodies (The Platinum Man, The Knife in the Lunch Pail, The Tommyknockers' Due, The Canary, The Bell at 4:15), plus loose notes and a three-phase build order. Not built yet.

### 2026-11-15 (late morning): Dead Air's notes, stories and first traps (phases 1 and 2 of `Docs/dead-air-mine-plan.md`)
- **Bodies with notes.** Each round, 3 of the 5 stories are in the mine (all of their notes), plus 3 loose notes. Bodies are placed from the seed on every machine, so no new replicated actor is needed. A body is a Forge worker laid flat on its back (the death clip ends kneeling, so a frozen idle pose is turned 90 degrees), with a paper scrap and a faint warm light. Walk within 2.3 m to read it. A parchment card shows the story title, how far into it you are, and the note (`ADCCHUD::DrawMineNote`, 10 s). Reading while recording is +25 footage. The note text lives in `Arcade/DCCMineNotes.h`.
- **Stories.** The status panel shows "Notes: <story> 2/5 ...". When the crew has read every note in a story, the System's line goes to the event log, and a payoff ghost (or, for The Canary, a small warm light) stands still in plain view, worth 30 on camera. It appears in the deepest stope, the cage or a middle stope, depending on the story.
- **Rockfalls warned by three knocks.** The knocks are no longer random ambience. Each one is a rock coming down 3.5 s later, where the knocks were, with a dust column first. A rock that lands on you costs a quarter of your health. Rocks fall every 20 to 35 s near a random crawler, never in the cage, and every 4 to 7 s in the last 45 s (the cave-in). Rubble is cleared after 25 s.
- **The phantom bell.** Every 80 to 120 s (while more than 50 s are left) the cage bell rings three times and the status line insists the cage is going up. It lies. Finishing The Bell at 4:15 makes it honest: it rings once, at 30 s left.
- **Dev.** `DCC.MineGo body` stands you at the first body, and `DCC.MineGo readall` reads every note (story completion).
- **Seen in -game runs:** notes reading (Platinum Man 1/5, Knife 1/5), the card, the status line, a body lying in the lamplight, a rock landing, and three stories completing with their lines and "(told)". **Not seen yet:** the payoff ghosts on screen, the bell ringing (it first rings after 70 s or more), and the dust overlay on the bodies (it did not visibly grey them).
- **Still to build (phase 3 of the plan):** rotten planks and the sump level, the runaway cart, powder crates, firedamp with a lamp toggle, blackdamp and the canary that leads to good air, the tommyknockers' ledge (food for earlier warnings), and the lamp-snuffer.

### 2026-11-15 (midday): Dead Air as a dark cave inside Mount Fourteener, and the rest of the traps
- **Mount Fourteener.** The mine is now "the Lucky Tuesday mine, inside Mount Fourteener" (title, Den pitch, a MOUNT FOURTEENER sign over the cage). Marks left by everyone who came after the miners are on the tunnel walls: a 1931 USGS bench mark, a 1967 School of Mines field study, SPOOKSEEKERS WUZ HERE 04, a resort survey stake, a 1988 Alpine Rescue grid, Rourke's 1892 claim, "TO FORT TINER VIA No. 1 ADIT (COLLAPSED)", and "the 14,115 ft summit is 3,960 ft above you". There are seven new loose notes from later visitors (the 1931 survey finding the mountain taller inside than out, a 1958 uranium co-op, a 1967 field trip, a 1988 rescue tag, a 2004 ghost show, a 2009 resort survey, an altimeter reading 15,600). Four loose notes a round.
- **Dark textured cave.** The walls are lined with Cave_Camp boulders, flattened against the faces (instanced, one component per mesh). The floor is one paper-thin `MI_rock_02` tile per cell (long strips smeared the texture). The fog is darker. Some crystal-like streaks remain on the floor: polish.
- **Corridors are two cells wide** (`UDCCDungeonGrid::Dig`). That is why earlier tunnel dressing and trap placement found nothing (they looked for one-wide drifts). `ADCCArcadeHaunt::CorridorSpan` finds the middle line of a straight two-wide stretch. Rails, timber sets, carts, planks and gas all use it. `TileSize` went back to 440 (tunnels 8.8 m wall to wall, about 7 m between the rocks). The 560 from this morning made them 11 m halls.
- **Traps** (all in `DCCArcadeHaunt.cpp`, all replicated as `FDCCMineEvent`s, so everyone sees the same moment):
  - **Rotten planks** (2 a round). Two crawlers on them, or one standing still for 2 s, and they give way: 15% damage, and you land in **the sump**, a flooded tunnel under the mine with a ladder that puts you back beside the hole. Seen working.
  - **Runaway cart.** On the longest straight stretch of rails, every 40 to 60 s when someone is near: 3.5 s of rumble and squeal, then it runs the length. A hit is 20% for crew (and hurts monsters), a knockback, and it sets off powder crates. Marked "ORE RUN, STAND CLEAR". The run is laid out; the cart's run was not seen.
  - **Powder crates** (3, "CLEAR CREEK GIANT POWDER"). A fight beside one (35% a hit), a rock landing near it, or the cart sets it off: 30% to crew and 80 to monsters within 4.5 m, a knockback, a flash and a boom. Chain reactions too. Not seen yet.
  - **Firedamp** (2 pockets). Lit lamps burn blue inside one, and the HUD warns. Four seconds of a lit lamp and it flashes (20%, knockback), then it is quiet for 25 s. **L** switches your helmet lamp (`UDCCWalletComponent::Server_SetMineLamp`, replicated `LampsOff`). Seen: the blue-lamp warning and the flash.
  - **Blackdamp.** One stope is marked BAD AIR. The HUD's canary goes quiet, the edges of the screen darken, and after 8 s it costs 4% a second.
  - **The canary** (when its story is down here). A small warm light leads toward the nearest unread note. Seen.
  - **The knockers' ledge.** A tin plate "for the knockers". Stand at it 3 s to leave your crust, and every rockfall warns 2 s earlier for the rest of the round (+15 footage on camera).
  - **The lamp-snuffer.** Every 60 to 90 s a ghost blows out the lamp of someone near it for 8 s, and stays visible while it does (a good shot). Seen firing.
  - Trap moments on camera (a blast, a flash, the cart, the planks going, a ghost at a lamp) score as footage.
- **Dev.** `DCC.MineGo plank|cart|crate|gas|ledge|badair|body|ghost|readall|driftN|<stope>`.
- **Not seen yet:** the cart actually running, a powder blast, blackdamp damage, the ledge, and the payoff ghosts. Damage stacks up when everything happens at once (Zoe dropped to 25/125 in one test), so tuning should follow a real playtest.

### 2026-11-15 (afternoon): Mine Outfitters, depth and air, lightsticks, and traps you can see
- **Credits and the Outfitters.** Dead Air pays **credits** at the end of a round: uploaded views / 150, plus 1 for every note you read, plus 8 for every story the crew finished (`UDCCWalletComponent::AwardCredits`, profile `Mine.Credits`). They are spent at the **Mine Outfitters**, a fourth tab on the counter (OUTFITTERS, with a CREDITS balance). It opens from a pad inside the cage, or from the Den and the main menu's PRIZES. The upgrades sit in `DCCChopShop` with `bMine = true`, so they share the Chop Shop's levels, pricing and level pips:
  - **Air Tanks** (3): how deep you can go. Depth is walking distance from the cage, 0 to 1. Base reach is 45%, then 65%, 85% and all of it. Past your reach the AIR gauge drains over 12 s, and empty costs 5% a second. Red lanterns on timber posts mark each air line in the tunnels ("AIR LINE: 2 TANKS PAST HERE").
  - **Better Lamp** (3): carbide, brighter carbide, an electric cap lamp (white), an arc lamp. Brighter, further and wider each level.
  - **Lightsticks** (3): 2 + 2 a level each round. **G** throws one in an arc. It glows green where it lands for the rest of the round (SoulCave's glowstick material), and a ghost within 9 m of one stays in frame.
  - **Hard Hat** (2): -30% a level from rock, carts and blasts. **Rope & Harness** (1): planks give way and the rope catches you, so no sump. **Gas Meter** (2): level 1 reads firedamp distance on the HUD, level 2 halves gas and bad-air damage. **Spare Reels** (3): +30 s of tape a level.
  - Icons were rendered in Blender (`Tools/render_item_icons.py`) and imported; the editor was opened for the import and closed again.
- **HUD:** DEPTH n% (your tanks reach n%), an AIR gauge, LIGHTSTICKS n [G], the gas meter, and the lamp and canary lines.
- **Traps you can see:** planks are Cave_Camp plank models. Firedamp pockets drift with spores (Alien_Cave_biome NS_spores) in a faint blue light. The bad-air stope has a low murk (NS_Smoke_01) and a sickly light. Dust (NS_smoke) falls from the roof before a rockfall. Powder blasts throw fire, sparks and smoke (Legendary_Forge). Firedamp flashes are fire. A snuffed lamp gets a puff of cold breath. A crust appears on the knockers' plate once someone leaves one.
- **Dev:** `DCC.Credits <n>`, `DCC.MineGo stick`, `DCC.Counter 3`.
- **Seen in -game runs:** the Outfitters tab with icons, the HUD (depth 68% against a 45% reach, air draining, the low-air warning naming the Outfitters), a lightstick throw (2 to 1), spores at a gas pocket with the blue-lamp warning, and the bad-air vignette. **Not tested:** buying an upgrade with credits, a round's credit payout, the harness, or the hard hat's damage cut.

### 2026-11-15 (late afternoon): the REC light, data extraction, and the Scroller
- **REC light.** A small red bulb with a red glow blinks over the head of anyone who is recording. It is drawn on every machine (from the replicated `bRecordingFootage`), so the crew can see who is filming. Built, but not seen with someone recording.
- **Data extraction.** When the clock runs out the monsters are cleared, and for 6 s everyone in the cage gets a green EXTRACTING n% bar over their head, under a DATA EXTRACTION banner, with a pulsing light in the cage. Anyone who did not make it gets TAPE LOST. Seen.
- **The Scroller** (Zoe: "use the scroller app program to just flick through scenes on a fake social media"). The feed idea comes from The Familiar's `scroller/` (one video fills a phone, it loops, you flick, it auto-scrolls). It is rebuilt in the game rather than reused, because that app plays real video files in a browser; no code or content was copied. There is one post per uploaded tape, best first, each in a phone frame:
  - The "video" is the player's best moment (the recording stretch that scored most), replayed through their own camera. Every machine records the round ten times a second (bodies and each crew member's camera; `Arcade/DCCHauntReel.cpp`), and stand-ins replay it into a portrait scene capture with a lamp. Trap moments (blasts, flashes) replay on cue, and each post loops.
  - On the post: #1 TRENDING (or #n ON THE FEED), likes, comments and shares counting up, a viewer comment drifting past, @name, what it was (GHOST MINER, RUNAWAY CART...), tags, and what the rank paid, plus a progress bar.
  - **Popularity** = views x (1 + 0.08 x Charisma) + AI Favor x 25 + 400 a note read + 1,500 a story told (+ a little luck). **Rank pays** tickets 30 / 20 / 12 / 6 and credits 15 / 10 / 6 / 3, on top of the normal payout.
  - The mouse wheel, arrows or J/K flick between posts, and the feed auto-scrolls every 10 s. Then the trending board shows in the status panel, and the vote.
- **Dev:** `DCC.MineGo end` gives everyone a little footage and ends the round at once.
- **Seen in a -game run:** extraction, a one-post feed (Zoe #1, 1.2K likes, +30 tickets, +15 credits), the trending board, the vote. **Not seen:** a feed with several posts and flicking, a post with a real ghost or trap in it, or the REC light. The reel's multi-camera code is still in `DCCHauntReel.cpp` (posts use one camera).

### 2026-11-15 (evening): the status panel moves down, a Dead Air ad, and the Repo Rally ad gets a fight
- **The location/status panel** now sits just under the health bar (`ADCCHUD`, panel and slim banner at Y=84).
- **Dead Air ad** (`Arcade/DCCHauntTrailer`, `DCC.HauntTrailer [capture <dir>]`, `python Tools/make_trailer.py --haunt`). 64 s, all five crawlers together in a real Dead Air round: the cage, a walk down the rails, a ghost miner, the runaway cart (everyone dives off the rails), a fight by the powder and the blast, firedamp turning every lamp blue, three knocks and a rockfall, back to the cage, DATA EXTRACTION, then the Scroller with each crawler's post and comments piling up. Song: `Content/Songs/Platinum Dust Mine.mp3` (Zoe's). Out: `D:\Builds\DeadAir_Trailer.mp4`.
  - The mine is built before ExecCmds run, so a fixed layout comes from the command line: `-MineSeed=N` (or the `dcc.MineSeed` CVar). Seed **9001** has a cart run, powder, firedamp and a ghost.
  - The frames include the HUD (`FScreenshotRequest` with UI), because the extraction bars and the feed are HUD. `ADCCHUD::bTrailerMinimal` draws only those while it rolls.
- **Comments now stack on Scroller posts in the real game too**: a new one every ~1.1 s, newest at the bottom, up to five.
- **Cart runs are in most mines now.** A run needed 3+ straight span cells, which almost no layout had (5 of 6 seeds tested had no cart). It now needs 2, and each end reaches a tile further.
- Puppets without a player (the trailer's crew) record and upload under their display names.
- **Repo Rally ad** is 52 s now: a new 34-42 s beat in the street. The mark's owner charges Kyle's wrecker and yells (red caption over his head), his friend fires two shots at the truck, Kyle shoots back and drops him, and the owner runs. The drop moved to 42-52 s.

### 2026-11-15 (evening, later): ghosts that look like ghosts, and solid props
- **Ghost miners** wear the hologram material now (`Cyberpunk_City/.../M_hologram`, `bUsedWithSkeletalMesh` switched on and saved, same for `Ice_Station/.../M_hologram_effect`): see-through, glowing pale blue, faint scan lines, no clothes showing. They float a hand's width off the floor, bob slowly, and their glow swells and thins with the flicker instead of switching on and off. Falls back to the old cold overlay if the material is missing.
- **Things you can see are things you bump into.** `ADCCArcadeArena::Prop` now gives a fitted box to any mesh asked to be solid that ships with no collision shape (logged once per mesh as `[DCC Solid]`; none of the current props needed it). Made solid:
  - Dead Air: spoil heaps, the runaway cart (parked or running), and the rock faces and loose stone along every wall.
  - Fourteener: the summit camp (tents, barrels, crates, rock pile, brazier, scree), the abandoned cart, the forge's bellows, rack and pulley, the braziers by the flags.
  - The Den: the chest, weapon rack, braziers, crates, statues, TV, trees, tent, car, wheel, palm and lamp.
  - Left alone on purpose: Repo Rally's street dressing (trucks would snag on it), the Gate portal (you walk into it), ladders, flags, small sacks and pebbles, distant mountains.
- **The "solid block" in the ghost was an anomaly orb**, a chest-high glowing cube in the same stope, drawn from the same random cells, so it often landed inside him. Ghosts now avoid any orb's cell and its neighbours (the random stream is drawn the same number of times either way, so every machine still agrees), and orbs are glowing spheres (cold spots) instead of cubes. Note: this shifts later random draws, so a given `-MineSeed` lays out a little differently than before; 9001 still has every trap.

### 2026-11-15 (night): Gate Run rooms and its boss, Fort Tiner's Trading Post, hunger on the stamina bar
- **Gate Run has four new room kinds and a real boss** (`Arcade/DCCArcadeGate`, `Characters/DCCEnemyCharacter_Gate`). New gate cast on NPC Forge bodies:
  - **The Gatekeeper**: boss of every gate, violet-tinted, boss charge.
  - **Champions**: gold-tinted, with rolled names like "Two-Hat Ernie, Employee of the Month".
  - **Gate Shades**: see-through violet hologram figures, fast and weak.
- Rooms (each appears once before anything repeats; a 10-room gate holds all of them):
  - **THE CHAMPION**: one named fighter in a gold ring with banners; a chest when it falls.
  - **QUIET ROOM**: an abandoned break room, until someone reaches the middle and shades come out of the walls.
  - **REST SHRINE**: the Warden (a Forge body by a pillar of light) heals and refills the whole party, once.
  - **THE GAUNTLET**: 2 m floor tiles glow amber, then burn red, in diagonal waves (server clock, the same on every machine). Cross on dark tiles to the green pad.
- **The Gatekeeper's hall**: pillars, fire bowls and a rune circle, plus a boss bar. Its slam picks someone, a red disc closes in for 1.6 s, then a white flash, damage and a knock-up for anyone still inside. It slams faster and calls shades at two-thirds and one-third health.
- **A boss bar any arcade game can use**: `ADCCArcadeArena::BossName` / `BossHealth`.
- **Testing aids**: `-GateSeed=N` fixes the gate (13 = rank B, 8 rooms, every new kind). `DCC.Tp x y z [yaw] [delay]` moves the local player relative to the arena.
- **Fort Tiner's Trading Post sells kit for tickets.** Stand on the lit pad by the Trading Post and the counter opens on a new TRADING POST tab, shown only in Fourteener rounds. It has the 12 climbing tools, boosts and food (2-22 tickets), which go straight into this round's kit (`UDCCWalletComponent::Server_BuySupply`, prices in `DCCClimbItems::SupplyPrice`). 11 new item icons were rendered and imported.
- **Hunger lives on the stamina bar.** It eats in from the right as a dark notched amber band (red when starving), labelled peckish / hungry / STARVING. The separate food bar is gone.
- **Bug fixed: hunger drained about 15 times too slowly.** `TickKits` runs four times a second but used one frame's delta, so "six minutes to starving" was about an hour and a half. It now uses the real step, so expect to actually need food in Fourteener.
- Verified in -game runs (seed 13): gauntlet tiles, shrine heal, champion ring, the boss hall, slam disc and flash, boss bar, ambush shades, Trading Post pad and tab. Not verified: a full boss kill with phases 2-3, buying an item by clicking, and hunger over a long climb.

### 2026-11-15 (late night): internet invites, itch.io updates, credits, and a content check
- **Internet invite with no accounts** (`Session/DCCUPnP`). When you host, the game asks the home router (UPnP) to forward UDP 7777 to this PC and to say the house's public address. The party menu then shows an "Internet" invite, after Tailscale and before same-Wi-Fi, plus one line saying what the router answered. It talks only to the router, on a background thread; nothing goes to an outside service. It falls back with a plain reason when the router has UPnP off, refuses, or the provider uses carrier-grade NAT (then use Tailscale). The forward is removed on quit. Not yet tried against a real router from outside the house.
- **Updates through itch.io.** `Tools/publish_itch.ps1` packages, then `butler push`es `D:\Builds\DCD\Windows` to the page in `Tools/itch_target.txt` (`user/project:windows`), with the build stamp as the version and no .pdb files. Only changed parts upload. Players install with the itch app, which updates them. Butler lives at `D:\Tools\butler` (`butler login` once). The package now ships `CREDITS.txt`, `.itch.toml` (tells the itch app to launch DungeonCrawlerDenver.exe) and a how-to that covers the internet invite and updates.
- **`CREDITS.txt`** (project root): Zoe, each crawler by name, the games, music, voices (Kokoro), art sources (Meshy, MakeHuman/MPFB, Epic's Paragon, every Fab pack by name) and the Unreal notice. Composers of the songs other than P Worthy are not named; fill them in.
- **NSFW check, all clean.** Text in Source, Docs, Config, Tools, SourceArt and Companion (the only hits were "breastplate" and the NPC Forge's sex field). Asset names in Content. A rendered thumbnail of every skeletal mesh in the project (89; the other 16 are Epic's Paragon skins). The city packs' billboard, poster and sign textures. All 12 NPC Forge people are fully dressed. Not adult, but worth knowing: DonMinion/BuffMinion look like *Despicable Me*'s Minions, so replace them before anything goes public.
- **First itch.io build is up:** build 2026-11-15.1756 on `zoebubble/dungeon-crawler-denver:windows` (4.02 GiB; future pushes send only the changes). The packaged game was launched and reached the main menu. The publish script skips the build's local `Saved` folder (test saves and logs) and the manifest files. Packaging's zip step now calls Windows' own `tar` by full path, because Git Bash's `tar` read `D:` as a remote host and failed.

### 2026-11-16: first real session (Zoe and Anke), network fixes, and this week's plan
- **Joining from the same house failed** until Anke's firewall allowed the game for Public networks. The how-to now says to tick both. Xfinity's gateway does not offer UPnP, so the automatic internet invite cannot work on Zoe's network; the router check no longer repeats every 3 s (`DCCUPnP::Start` runs once per game).
- **Guests froze on the host** (walking and driving). On foot: arena floors and props are built separately on each PC, so a guest's "I'm standing on X" named something the host did not have, and the host threw the moves away (`TimeStamp expired` and `Unable to resolve default guid` in the log). `ADCCCharacterBase::SetBase` now never uses a locally built piece as a movement base. Driving: `UFloatingPawnMovement` never sent anything to the host. The driver now sends the truck's position ~20/s (`ADCCRover::Server_DriveSync`), and the host corrects the driver with `Client_ForceTransform` when it teleports the truck itself.
- **Profile sync** (`DCCProfile::MergeFromClient` / `AdoptFromHost`, `UDCCWalletComponent::Server_SyncProfile`): a guest's profile travels with them. The host adds what changed since the copy the guest last received (amounts by difference, bests by best, levels by max, items by union) and sends the merged copy back every few seconds while it changes. A revision number decides which copy is stale.
- **Fourteener eased:** climb stamina x0.5 (was 0.74), hunger drains half as fast, and birds only attack climbers past a quarter of the way from High Camp to the summit, every 35-55 s (first after 90 s).
- Verified with two copies on one PC: no movement rejections, profile sent and merged. Driving needs a real session.
- **The plan for the week is `Docs/week-plan-2026-11-16.md`:** next session works first time; Dead Air (a real cage that is safe from everything, leaving early, a clean end when all are down, the camera explained, a selfie camera); a style guide and an original enemy roster; bird shadows.

### 2026-11-16 (later): Dead Air cage, early ride up, dressing passes, style guide, bird shadows
- **The cage** (`ADCCArcadeHaunt::BuildCage` / `TickCage`): a real mine cage in the middle of the first room, with bars on four sides, a gate toward the way out, a grated floor and roof, and hoist cables. It is the "made it back" zone now (`VanBox`); the whole room is `FoyerBox`, which traps and drifts avoid. Inside, `Stats->bInvulnerable` is on, so claws, traps, blasts, gas and rock all stop at the bars. Monsters never target anyone in it, and one that gets through the gate is put back outside.
- **Leaving early:** once the crew has been out and everyone standing is back inside, a SEND THE CAGE UP panel (`UDCCCageVoteWidget`, `UDCCWalletComponent::Server_CageVote`) ends the round when all agree.
- **Everyone down:** when every crawler is down, the round ends 3 s later with "EVERYONE'S DOWN".
- **Camera card** for the first minute of a round until you record (V, the red light, what rates, tape, X, C, L, G, the cage). **Selfie cam on X** (`ToggleFacingCamera`): the camera swings round to film your face while you keep moving. The old C pose-selfie stays.
- **Dressing passes** (`Arcade/DCCHauntDressing.cpp`, all instanced, from the seed). Textures: rock on every roof cell and wall face, wet seep patches, wood on the timbers. Props: wall clutter (barrels, crates, rope coils, cases, baskets, wheels, sacks), tools and ladders tipped against the rock, canvas vent ducts, air and water pipes, hanging lamps on chains every few sets, company signs, pulleys and rope. The stopes get scaffolds, the shift boss's table, ore heaps, boulders, stores, roof beams and drips. Filler: pebbles, papers, bottles and plates in every cell.
- **Packaging fix:** an audit of every asset the code loads by name found folders missing from `DirectoriesToAlwaysCook` (Alien_Portal's floor material and portal meshes, smoke and spore effects, diner props, Serath's animations). They are added, along with the dressing's folders.
- **Unity builds off** for the module (`bUseUnity = false`): many files keep same-named private helpers, and adding a .cpp kept gluing them together into compile errors.
- **Style guide** (`Docs/style-guide.md`) and **arcade enemy roster** (`Docs/enemy-roster-arcade.md`). The three Minion look-alike bodies (Intake Manager, Notary, Security) are NPC Forge people now.
- **Fourteener birds** cast a shadow that sweeps down the rock onto their target instead of a red line.
- **Join timeouts** explain the firewall fix in the menu.
- **Trailers:** the Repo Rally skyway shot is a high chase from behind instead of the truck's side, and the rifleman shot ends a second sooner (51 s). Dead Air was re-rendered with the cage and the dressing.

### 2026-11-16 (night): new-player guide, starter kits, a linear mine, mine audio, Den music, signs on boards
- **New-player guide** (`ADCCHUD::DrawNewPlayerGuide`): a red dot and a pulsing hint walk a new crawler through opening the menu (Tab), opening a loot box, then equipping something. Progress is per crawler in `GameUserSettings.ini` `[DCCGuide]`. It shows in the Den and the World Dungeon, not inside the minigames.
- **Starter kits** (`Session/DCCStarterKits`, `UDCCStarterKitWidget`): the first time a crawler enters the World Dungeon they pick BRUISER, TANK, MAGIC, HEALER or RANGER. Each kit is a weapon, gear, sometimes a skill, a stack of potions, and 25 tickets, equipped at once. The choice is stored in the profile (`Dungeon.StarterKit`), so it is offered once.
- **Entering the World Dungeon** records `EDCCAchievementStat::EnteredWorldDungeon`. **Still owed in the editor:** a `DT_DCCAchievements` row for it (Threshold 1).
- **Fort Tiner's dev chest** is gone.
- **Dead Air's mine is linear now** (`UDCCDungeonGrid::FStyle`): small stopes (2 to 3 cells) strung on one winding drift, no loops, no shortcut. The World Dungeon keeps the old shape. `-MineSeed=9001` logs the layout as ASCII under `[DCC Mine]`.
- **Mine audio** (`Arcade/DCCHauntAudio.cpp`): a generated rumble bed that deepens with depth, plus drips, timber creaks, distant rumbles and knocking. *Platinum Dust Mine* plays under it very quietly, **once `Content/Songs/Platinum Dust Mine.mp3` is imported** as `/Game/Songs/Platinum_Dust_Mine` (editor step, still owed).
- **Tape warnings:** "30 seconds of tape left", "10 seconds", and "TAPE FULL", each said once.
- **The Den** rotates four songs.
- **No more floating text.** `ADCCArcadeArena::Sign` puts every sign on a dark board. A free-standing sign also gets two posts down to the ground, found by tracing; a wall sign gets none. Signs that already have a board pass `bBoard = false`. Dead Air's wall marks hang on planks. Repo Rally's graffiti is painted flat on its billboards and on plywood sheets tied to the yard fence. The tags that floated over the fence, on the sidewalks and at the gas stations are gone; the decal graffiti covers those places.

### 2026-11-16 (day): spray tags, and Dead Air's own creatures
- **Spray tags** (`Arcade/DCCSpray`, `ADCCSprayTag`). The Prize Counter sells a **Spray Can** (60 tickets). Buying it opens the Windows picture picker; the picture is cropped square, cut to 48 x 48 and banded to six levels per colour, then kept on that PC (`Saved/Sprays/Mine_<name>.rgb`). CHANGE PICTURE is free after that, and the counter shows your tag. **Z** sprays it where you look, on foot or from the truck (`UDCCWalletComponent::Server_Spray` / `ADCCRover::Server_Spray`). The server spawns a replicated decal (the Cyberpunk graffiti material, its `Decal` texture swapped), so everyone sees it. A tag on something that moves rides along with it. The host keeps each arena's tags (`Saved/Sprays/World_<arena>.json`, 60 per arena, oldest painted over) and puts them back next session. The host sees **CLEAR ALL SPRAYS** at the counter (two clicks). Checked: a tag sprays, comes back after a restart, and reads upright and unmirrored (quadrant test). Dev: `DCC.Spray <picture> <yaw> <pitch> [delay]`.
- **Dead Air creatures** (`Characters/DCCEnemyCharacter_DeadAir`), replacing the dungeon wildlife in the stopes: the **lamp-eater** (violet, see-through, only hunts crawlers whose lamp is on, via the new `ADCCEnemyCharacter::AcceptsTarget` hook), **Tommyknockers** (never move; knock three times and drop rock where you stand, via `ADCCArcadeHaunt::QueueRock`), **slag dogs** in threes (the Slime body, soot-black with ember edges; no dog exists in the packs), and **the shift boss** in the deepest stope (big, slow, d10, red lamp). Checked: all spawn on seed 9001. Not yet checked in a fight.
- **Owed in the editor:** the mine dressing's rock and prop materials need *Used with Instanced Static Meshes* (MI_Cave_Rock_Small, MI_Cave_Rock_Small_Moss, M_Cave_Chain, MI_medium_rock_01, MI_small_rock, MI_props, MI_tools, MI_brasero, MI_water_01). The editor patches this when it runs the game, but a packaged build draws them in the default material.
- **Repo Rally trailer, tag beat** (`ADCCRallyTrailerDirector::SprayBeat`, 24-34 s, replacing the skyway shot): Marcel sprays the dispatch office wall twice and Vaughn tags a rival crew's tow truck, each tag popping on, then the camera pulls back on the finished wall. The tag is an original pixel doodle, a little guy peeking over a wall above "(YOUR JPG HERE)" (`SourceArt/Itch/tag_your_jpg_here.png`). The music crossfades from *Repo Rally* to *FREAKS* at 24 s; FREAKS starts at 60.5 s so its big hit (62.1 s) lands on the first tag (`make_trailer.py`: `song2`, `switch_at`, `song2_start`, `song2_gain`). `DCCSpray::DecodePicture` and `PaintTag` were split out so the trailer can paint without a purchase.
- **Trailers live in Google Drive** ("Dungeon Crawler Denver - Trailers", in the connected zoes21bots account): the latest Fourteener, Dead Air and Repo Rally ads. `make_trailer.py` now also writes a 720p copy under 7 MB (`D:\Builds\Web\<Name>_drive.mp4`), the size Claude's Drive upload accepts; each new render replaces the old file there. The Repo Rally ad was re-rendered with the tag beat and the switch to FREAKS.
- **Tags that mean something** (`ADCCArcadeArena::OnTagged`, called by `DCCSpray::ServerSpray` with the painter, or the truck's driver). **Dead Air:** a tag on the cage (within a stride of `VanBox`) pays **+25 footage**, once a round each, and the achievement *Property Defacement*. **Fourteener:** a tag in the top tenth of the climb (local Z at least 0.9 x `TopZ`) earns *Proof of Summit* **once ever** (Zoe: not every round): a crawler who already has it gets no achievement and no announcement. Checked across a save and restart. Both announce to everyone. Checked in game: the cage tag paid out; a summit tag counted, and a mid-mountain tag did not.
- **Editor session done** (Claude opened and closed the editor): `DT_DCCAchievements` has *Welcome Aboard* (EnteredWorldDungeon), *Property Defacement* and *Proof of Summit* (also in `Docs/DT_DCCAchievements.csv`); the five base materials behind the mine dressing (`M_Master_Props`, `M_water_02`, `M_Cave_Rock_MASTER`, its moss variant, `M_Cave_Chain`) are *Used with Instanced Static Meshes*; *Platinum Dust Mine* is imported at `/Game/Songs/Platinum_Dust_Mine` (from a WAV, via `UnrealEditor-Cmd -run=pythonscript`, since no MCP tool imports sound). A mine run now logs neither warning.

### 2026-11-16 (afternoon): textures, bounties, balance, an equipment plan, and a simplification pass
- **Texture pass, Fourteener:** the fort's ground and the trail are packed soil laid in 4 m tiles (one big block smeared the texture); the palisade logs and gate posts are bark (Ancient Civilization's kit material, which has no wind, so the posts do not sway), with darker bark tips; the gate lintel, the route board, the sign backings, the tower decks, the Trading Post counter and the well's beam are planks; the well is stone. Three pack folders added to `DirectoriesToAlwaysCook`.
- **Bounties** (`Arcade/DCCBounties`, the Prize Counter's new **BOUNTIES** tab): seven cross-game goals, each paid once in tickets: Dressed for It, High Camp, Souvenir, Proof (summit tag), Going Viral, Property Defacement (cage tag), Shift's Over. Most are read off the profile; the tags and the shift boss going down set a flag. The wallet re-reads them every 2 s on a server timer (its tick is off when nothing cosmetic is worn) and tells you when one finishes. Checked: the tab shows, and High Camp and Going Viral read DONE from existing records. Not checked: CLAIM paying out.
- **Balance:** slag dogs (about 15 to 7 damage a second for a pack of three), Tommyknocker every 18-26 s.
- **Equipment plan:** `Docs/equipment-plan.md` (currencies, what each game gives, gaps, and the next step: mountaineering levels at the Trading Post).
- **Simplification:** the creature tint, hologram and glow helpers were copied in both the Gate Run and Dead Air enemy files; they are one shared `Characters/DCCCreatureLook` now.
- **Test pollution, flagged:** dev test runs on this PC play as Zoe and paid into Zoe's real profile (tickets went from 384 to 1,638; Fourteener best height 14,115 ft came from teleport tests). A copy of the profile before any cleanup is at `Saved/SaveGames/Profile_Zoe.backup-2026-11-16.sav.bak`.
- **Hit stutter** (`ADCCCharacterBase::HitStutterSeconds`, 0.08 s): whoever is hit stops dead for a beat before the flinch and the shove (Zoe, after Elyra's Tales' hit-stop: "a small stutter / pausing the defender ... otherwise leave it as is"). Per character through `CustomTimeDilation`, run from the hit-reaction multicast, so everyone sees it and nobody else's game pauses. Checked: 25 hits in a Gate Run fight, nothing stuck frozen.
- **Profile tools (dev):** `DCC.Profile <Key>` lists a profile, `DCC.ProfileFix <Key> <Game.Stat> <Value|->` sets or removes one record. Used to undo today's test runs on Zoe (tickets back to 384, the teleport Fourteener records removed).

### 2026-11-16 (evening): reactions, and the grappling hook on everyone's belt
- **Reactions** (`Characters/DCCReactions`, Zoe: "players could have more expressive forms maybe? It's a bit dull"): a pixel-art speech bubble pops over a crawler's head, often with a move to match. Eight: Cheer (star), Alert (!), Confused (?), Love (heart), Tired (sweat drop), Down (skull), Party (note), Cold (snowflake). The bubbles are drawn from masks in code, no imported art. They fire on their own at the big moments: a kill, the summit, an achievement, a finished bounty (Cheer); an enemy picking you as its target, the ghost that snuffs your lamp in Dead Air (Alert); a revive (Love); a hard fall, running out of breath (Tired); going down (Skull); spraying a tag or dancing (Party); now and then above High Camp (Cold). Automatic ones wait out a 3 s cooldown so a fight is not a storm of bubbles. **B** opens the reworked menu: the eight reactions with their icons first, then the moves with readable names ("Boom Dance", not `A_DCC_HW_BoomDance`). Bubbles are drawn last, over every panel. Checked in the Den: they pop and the moves play. Dev: `DCC.React <1-8> [delay]`.
- **Belt grappling hook** (Zoe: "always on your belt and just on cool down after use and gets better with upgrades"): on Fourteener, **G** while climbing yanks you up the wall and the line winds back in. Level 0: 6 m, 40 s. Three levels at the Trading Post for tickets (60, 120, 180), each +2 m and 8 s faster, kept forever (`DCCChopShop::Grapple`, the shop's new ticket currency, `bTickets`). The HUD shows "[G] Grappling Hook ready" or the countdown. The one-use hook is gone from the Trading Post; one found in a chest winds your belt hook straight back in. Checked: a throw lifted the auto-climber to the next ledge, a second throw 3 s later was refused, a throw after the cooldown worked. Not checked: buying a level. Dev: `DCC.Grapple [delay]`.
- **SETTINGS tab** in the Tab menu (Zoe: "can you make it all adjustable to different keys / different emotes showing up at those moments?"). **Keys** (`UI/DCCControls`, saved in GameUserSettings.ini `[DCCKeys]`): grappling hook (G), spray your tag (Z), the reactions menu (B), and eight quick-reaction hotkeys (unbound). Click a key, press the new one, Esc cancels; keys the game already uses elsewhere, or that another remappable action has, are refused with a note saying what has them. The HUD reads these keys every frame, so a change works at once, and works from the truck. **Reactions** (`DCCReactions::EMoment`, saved in `[DCCReactions]`): for each moment (a kill, a big win, spotted, revived, hurt or winded, going down, a tag sprayed, cold up high) a button cycles the bubble (or none) and another cycles the move (12 moves, or none). The choices go to the host (`Server_SetReactionChoices`), so you react your way in anyone's game; a moment set to no bubble and no move is off. Reset to defaults puts both back. Checked: the tab draws and fits at 720p. Not checked: clicking (setting a key, cycling a choice). Dev: `DCC.MenuTab <index> [delay]` (10 is SETTINGS).
- **The Den's panel is a strip now** (Zoe: "Do you see how long this menu is? ... add the high scores somewhere else and just add a small logo for each game and the number of players underneath"). `ADCCHUD::DrawDenStrip`: one pixel-art logo per game (`UI/DCCGameLogos`, drawn in code: a sword, a ghost, a peak, the tow magnet, a bolt, rising bars, a portal arch, each on a tile in the game's colour) with the number of players on its pad underneath; the leading game glows; one countdown line under it when a game is about to start. The Den replicates `PadVotes`, `PadRecords`, `CountdownLine` and `LeadingPad` instead of building text lines. **Records** moved onto each game's pad board in the Den, under its description ("most views: Zoe 5,480 views"), kept current as people play. `ADCCArcadeArena::Sign` now returns the text actor so a sign can change. Checked: the strip, a vote lighting Dead Air with a 1, the countdown line, and the record on Dead Air's board.
- **The Den strip slides** (Zoe: "have the strip come down from (off) the top of the screen when a player steps on the platform and then have it roll back up when they step off"): it comes down while **anyone** is on a game's pad (so a friend's vote is never a countdown you cannot see) or a game is about to start, and rolls back up off the top when every pad is empty (~0.3 s each way). **The pads press down** ("make it so that the platforms depress when you stand on them"): each pad is a 14 cm dais (was 6) with no collision; under a player it sinks 10 cm quickly and springs back slower when they step off, and the Den dressing lying on it rides with it (`ADCCArcadeLobby::TickPads`, local on every machine, so it is instant). Checked: the strip hidden, down, then rolling up; a pad pressed from a low angle.

### 2026-11-16 (late): the loot box screen, with a real chest and the real items
- **A real chest** (Zoe: "an actual chest that shakes and then glows instead of the icon, and similarly show the actual items awarded instead of just an icon"). `UI/DCCLootStage` is a small photo studio the HUD spawns 2 km above the world (local only): a turning chest for the row of boxes, a reveal chest that shakes harder and harder, glows in the box's colour, hops, and bursts into the open chest (`SM_Chest` / `SM_Chest_Open`), and six booths that photograph each item from its real model (the ground model if it has one, else the worn one; the old icon only when there is no model). Each camera writes a texture the HUD draws.
- **Why it hangs above the world, and why it looked broken at first:** height fog thickens exponentially downward, so a studio 600 m underground was solid fog colour; the cameras also skip the arena's sky light, bounce light, fog and grain, and each one draws only its own subject plus a black wall on its own lighting channel (a lit floor or wall caught the glow and showed as a hard square). Lamps are in candelas and placed relative to each shot, so a ring and a chest are lit alike. Exposure is `DCC.LootExposure` (10).
- **Text:** card lines wrap to the card's width instead of running off it, a long description ends in "..." at the card's bottom, and cards narrow so six (or a batch row of seven) always fit the screen; "1 BOX OPENED" instead of "1 BOXES".
- Checked at 720p: the row of chests, the shake and glow, the burst, cards showing a scroll, a sword, a hammer, a badge and an axe, a 7-item batch row on screen. Dev: `DCC.LootOpen [delay]` (gives boxes if you have none: **this writes Zoe's real profile**), `DCC.LootDump [delay] [suffix] [exposure]` saves each studio camera's picture to `Saved/LootDump`, `DCC.LootSource 1` shows plain colours.

### 2026-11-16 (late night): fewer keys - the belt, the B menu, and buttons
- **Why** (Zoe: "I feel we have 1000 different keys and could better consolidate a few into a selected item + click rather than a dedicated key"): on foot a crawler had about 27 keys. Two clashed in Dead Air: L turned the lamp off *and* cycled the log filter, and the lightstick was hard-wired to G, the grappling hook's (remappable) key.
- **The belt** (`ADCCCharacterBase::GetBelt`, `FDCCBeltEntry`, drawn by `ADCCHUD::DrawHotbar`): the number keys 1-9 reach every slot, built fresh from what you have in this game. Skills arm as before (in the arcade only where used: Climbing on Fourteener). Things you **aim** (a lightstick, the spray can, a throw) arm with their number and the **next left click** uses them once; right click puts them away; the prompt line says "<item> ready: click to use it". Everything else (Fourteener tools and food, the grappling hook, Dead Air's lamp and body cam, stow in bag) works **on its number**. On a wall every slot works on its number, because the left mouse is the climb. Slots show a count, a cooldown, on/off/REC, or "aim"/"click!". Fourteener: Climbing, Hook, each tool. Dead Air: Body Cam, Lamp, Lightstick. Anywhere you own one: Spray Can. Carrying something: Throw, Stow in bag.
- **Keys gone on foot:** Q, T (tools), G (hook, lightstick), L (lamp), V (body cam), R (throw), F (stow), Z (spray; it still works from a truck), C and X (now rows in the **B menu**: "Photo pose" and "Selfie camera: on/off"; Esc cancels the photo pose), O, K, L (now **buttons under the Tab menu's tabs**: LOOT BOXES, PARTY, POSE BENCH, FEED, LOG). F2 and F3 still work as shortcuts. **P and M** do nothing in the arcade games (they belong to the dungeon runs). **Y/N questions** take a click for yes and a right click for no. The truck keeps its own keys. SETTINGS hides the retired hook key and calls Z "Spray your tag from a truck"; its taken-key list knows the belt and the truck keys.
- Hints rewritten everywhere they named an old key (Fort Tiner's board, supply chests, the spray can, the Concourse kiosk, loot box toasts, the new-player guide, Dead Air's body cam card and lamp lines, the climb kit panel).
- Checked in game: Dead Air's belt (Body Cam, Lamp, Lightstick x2, Spray Can); slot 1 turned the body cam to REC; the lamp slot turned the lamp off; the lightstick armed, showed its prompt, and a click threw it (2 left -> 1); the B menu with the two camera rows; Fourteener's belt (Climbing, Hook, Spray Can); the Tab menu buttons; the Den (Spray Can only). **Not checked:** Throw and Stow with something in hand, Fourteener tools on the belt (none were carried), clicking the Tab menu buttons, answering a climb question by click. The scroll wheel picks aimed belt slots (see 2026-11-17).
- Dev: `DCC.Belt <1-9> [delay]` presses a belt key, `DCC.Click [delay]` a left click, `DCC.Emotes [delay]` opens the B menu. **`Tools/run_test.py`** runs one game with dev commands, takes window-only shots into `Saved/TestShots/<name>/` with a contact sheet, and closes only the game it started.

### 2026-11-17 (overnight, Grok)
- **Scroll wheel** (`ADCCCharacterBase::HandleScroll`, `CycleBeltAim`, dev `DCC.Wheel`). With no menu open and the interaction list empty, the wheel steps aimed belt slots and can put the last one away. An armed skill still blocks it. Seen in `Saved/TestShots/wheel`: the lightstick armed, the next step armed the spray can, and the click sprayed. That spray wrote Zoe's profile (Property Defacement, +250 tickets, views to 3425).
- **Fourteener outfit** (`ADCCArcadeClimb::GiveFullKit`, dev `DCC.Outfit`). Seen in `Saved/TestShots/tools` and again in `tools2`: nine belt slots, the piton refused near the ground and stayed at 1, Trail Mix was eaten and the later slots closed up. The kit is for the round. Quick slots are not in the crawler save.
- **Throw and stow** (`DCC.Bottle`, `DCC.Interact`, belt slots). Seen in `Saved/TestShots/carry`: House Tequila picked up, Throw armed, a click threw it, a second bottle, Stow cleared the belt. `stowsave` saw Stow again. The crawler save on disk did not change in those runs. The harness kills the process, so the exit save does not run, and `DCC.Save` has no delay so it fired at startup. A stowed bottle surviving a real exit was not checked.
- **Dead Q/T climb tools removed.** `UseClimbTool`, `NextClimbTool`, `Server_UseClimbTool`, `Server_NextClimbTool`, `ClimbToolIndex`, and `ADCCArcadeClimb::NextTool` are gone. `UseTool`, `UseToolId`, `GiveItem`, `GiveFullKit`, and `ClimbTools` stay. Development Editor build at 8:01 PM, Result: Succeeded, no numbered DLL. `tools2` still passed. Rally's truck keys were left as they are.
- **Dead Air, one fight each.** `ADCCArcadeHaunt::DebugGoTo` now stands 160 units west of the nearest living monster and, if `TeleportTo` fails, forces the move and logs `[DCC Mine] dev go`. Before that fix the lamp run and the first dogs run never left the cage (`Saved/TestShots/lamp`, `dogs`). Lamp-eater: one miss while the lamp was on, at the cage bars, then the lamp went off and it stopped. Slag dogs after the fix (`dogs2`): in the stope, they hit her, she retired one, then she went down. Tommyknocker (`tommy2`, lamp off): she hit it for 6 and 8 and retired it; it hit her for 4. A roof knock has no log line, so a knock was not confirmed. The first tommy try (`tommy`) had a lamp-eater walk in while the lamp was on. Shift boss (`boss`): he hit her (13, 13, 13, 11, 6, 7, then 7) until she went down. Her clicks opened the note "The Bell at 4:15", so she never landed a hit on him. Each downed round ended and added 1 Haunt play, with 0 views.
- **Gatekeeper** (`ADCCArcadeGate::DebugEnterAll`, same teleport fallback, log `[DCC Gate] dev in`). Seen in `Saved/TestShots/gate` and `gate2`: rank D, 5 rooms, she arrived in the boss room. She hit the Gatekeeper, it called the shades, it raised its hands for the charge, that charge was interrupted, and slams were logged. The shades then put her down. The second phase line did not appear in either run. No gate kills, so no gate tickets, and the round did not wrap. The first gate run crossed the 60 second autosave and rewrote `DCC_Zoe.sav` at 8:19:55 PM. Hit points are not in that save (load refills them). The exported fields are still her crawler: level 5, 1500 XP, the flare gun equipped, no bottle.
- **Comments.** Belt comments that still named Q or T now say the belt. They rode in the 8:01 PM build. Left as written: the rally truck's [Q] and [F] lines, and the hint "Equip Climbing (hotbar) and hold the left mouse button to climb."

### 2026-11-17: one food slot on the belt
- Zoe: "make food sit on one spot and make it so you can press a button ... to cycle through what you have". All food and drink (Fourteener food and pack drinks/snacks) now share **one belt slot**. Its number eats the food it shows; the **middle mouse button** picks the next one. The slot reads "x1  2/5" (how many of this one, which of how many) with a small pixel mouse in its corner, middle button lit gold, as the button prompt (Zoe: "show a diagram type symbol of the mmb like other games might"; `MiddleMouseIcon` in DCCHUD.cpp, drawn in code). When the last of one is eaten, the slot moves on to the next food, not back to the first. `ADCCCharacterBase::GetFoods`, `CycleFood`; dev `DCC.NextFood [delay]`.
- Checked on Fourteener with a full kit: 5 foods in slot 4 (the belt went from 9 slots to 5), the middle button stepped Trail Mix -> Granola Bar -> Green Chile, pressing 4 ate the chile and the slot showed Jerky 3/4. This test run wrote Zoe's profile (one Fourteener round started).

### 2026-11-17: the summoning tubes (a friend gacha in the Den)
- Zoe: "tubes that come down from the ceiling ... use tickets (100 for now) to summon an ally (an ai controlled copy of one of the friends at base level) that comes down the tube ... assign each player a rarity based on their popularity score ... a fun gacha system where allies are sometimes spawned as rarer varieties that glow slightly or come with different weapons."
- **Where:** four glass tubes along the Den's south wall between the Prize Counter and the Chop Shop, hanging from a neon canopy (the Den has no ceiling), each on a three-tier gold and plum base with studs and a glowing landing disc. A podium stands front-right of each tube, so you watch the tube, not your own back. `ADCCArcadeLobby::BuildTubes`, `TickTubes`.
- **A pull:** stand at a podium, **E** opens the panel: your tickets, your allies (x of 2), every friend with their rarity, their chance and their shiny chance, and **SUMMON - 100 TICKETS** / **CLOSE** buttons (Esc closes; walking away closes it). The tube lights in the rarity's colour, a beam runs down it, the friend floats down inside (2.4 s), lands, and walks out to you. Then a banner: the name big in the rarity colour, "LEGENDARY  *  SHINY  *  armed with ...". The whole Den sees the tube, and the log says "Zoe summoned a SHINY Legendary CJ from the tubes."
- **Rarity is popularity** (`DCCSummons::ComputeTiers`): Dead Air total views + 1500 per win (Fourteener summits, gate clears, Repo Rally wins), from the host's profiles. The most popular friend is Legendary, the next Epic, two Rare, the rest Common. A pull rolls the rarity first (Common 55, Rare 28, Epic 13, Legendary 4), then a friend of it; you never pull yourself.
- **Variants:** Shiny (a glow and tint in the rarity colour; 8% for Common up to 26% for Legendary) and Armed (18%: a random main-hand weapon from the gear table). Non-shiny Rare and up carry a faint light of their colour.
- **The allies** (`ADCCSummonedAlly`): a level 1 copy in that friend's own body, following whoever summoned it (`ADCCEnemyCharacter::FollowOwner`). At most 2 each. They come with you into Gate Run and Dead Air (spawned beside you when the game starts, `DCCSummons::TickArena`) and last until they go down. Kept on the host for as long as the party plays (not saved). In the Den they walk straight to you (no navmesh there, `bFollowStraight`); in Gate Run and Dead Air they use the rooms' grid (`GetChaseGrid`).
- Checked in game: the panel and odds; free test pulls of Kyle (Common, armed with a Contractor Sledgehammer), a Shiny Legendary CJ (gold glow), Marcel and Scott (Rare); the beam, the descent and the banner; both allies walking out and following when the player moved away; both arriving beside the player in Gate Run after travelling there. **Not checked:** a paid pull (the ticket path), clicking SUMMON with a real mouse, a joined player's view, allies fighting in Gate Run or Dead Air, arriving in Dead Air.
- Dev: `DCC.Podium <1-4> [delay]` (stand at a podium, panel open), `DCC.SummonFree <1-4> [delay]` (a pull that costs nothing, so tests leave the profile alone), `DCC.GoTo <Game> [delay]` (travel mid-test).

### 2026-11-17 (later): tube cut-outs, the fast drop, Shiny and Shadow, and the gacha ad
- **The tubes** are fourteen curved glass panes now, with three left out as a doorway facing the room, gold posts either side and a lintel over it (Zoe: "come out of a cut out in the glass"). A friend **shoots down** the tube (0.55 s, speeding up), bounces as they land, and walks out through the gap (`ADCCArcadeLobby::TubeExit`, `ADCCSummonedAlly::ExitDirection`). The beam, the light and the busy time are shorter to match (2.8 s); the banner appears as they land.
- **Variants** (Zoe: "shining variants and shadow versions, just the regular models but with a shiny overlay"): `DCCSummons::EVariant`. **Shiny**: the regular body with a glowing shimmer overlay in the rarity's colour (`DCCCreatureLook::Shimmer`, the hologram material as an overlay rather than a replacement) plus a light. **Shadow** (10%): the regular body under a near-black overlay with lit edges, plus a violet light. Armed can come with either. Banners read "RARE * SHINY *" or "COMMON // SHADOW //". The panel lists the shadow chance.
- Banners are queued, one per pull, each shown when its friend lands, on game time (quick pulls each get a turn).
- **The ad** (`Arcade/DCCGachaTrailer`, `DCC.GachaTrailer [capture <dir>]`, `python Tools/make_trailer.py --gacha`): 40 s, FREAKS from 49.1 s so its big hit lands on the button-mashing. Marcel walks up, presses, Kyle shoots down and steps out, Marcel turns to a heap of gold coins and ticket rolls and nods, then mashes the button: fifteen friends rain down all four tubes (Shadow Vaughn, Shiny CJ, armed Kyle, Shiny Scott, Shadow Zoe, Shadow Kyle, Shiny Zoe, Shiny Vaughn, Shadow CJ...), follow him in a conga line, and dance round him; the camera rises for the title. Full quality `D:\Builds\Gacha_Trailer.mp4`; the 720p copy is in Drive ("Summoning Tubes Trailer (2026-11-17).mp4" in the Trailers folder).

### 2026-11-17 (evening): F.I.A.T., louder Shiny, themed cards
- **Renamed** (Zoe: "Don't call it the summoning tubes, maybe more like, FIAT - Friend in a tube"): the Den's sign reads **F.I.A.T.** over "FRIEND IN A TUBE - 100 tickets: a friend comes down to fight beside you"; the panel is "F.I.A.T. - FRIEND IN A TUBE - TUBE n"; the log says "Zoe pulled a SHINY Legendary CJ out of F.I.A.T."; the ad's subtitle is "F.I.A.T. - FRIEND IN A TUBE. 100 tickets. one more pull." (Code names stay DCCSummons / the tubes.)
- **Shiny, louder:** a much brighter shimmer, a strong light in the rarity's colour, and a white-gold halo light over the head.
- **Themed name cards:** Shiny is gold with a double pulsing rim, a glint sweeping across, the name glowing, twinkling stars, "* S H I N Y *". Shadow is near black with violet edges, scanlines, the name glitching in magenta and cyan, "// S H A D O W //". Plain pulls keep the rarity-coloured card.
- **The ad:** Marcel's pile of gold and tickets is spent from the top down as he pulls, gone by the last one; once he starts mashing, his hand plays at 2.8x. Now `D:\Builds\FIAT_Trailer.mp4`; in Drive as "FIAT - Friend In A Tube Trailer (2026-11-17).mp4" (the earlier "Summoning Tubes" copy is in Drive's trash).

### 2026-11-18 (Grok, short-term plan)

Night is finished. Development Editor link succeeded (`UnrealEditor-DungeonCrawlerDenver.dll`, no numbered DLL). Shots are under `Saved/TestShots/`. Driving numbers are still inline in `DCCRover.cpp`, not one tunables struct.

- **A1 Safe Room.** Player-facing "Den" on the lobby sign, the arena title, the front-door sign, menus, hints, credits, and the package script now say Safe Room. Code names stay (`DrawDenStrip`, `VotesDen`, `ADCCArcadeLobby`). Shot `achieve_night`: the neon sign reads THE SAFE ROOM, and the pads read NEON MILE, REPO RALLY, FOURTEENER, DEAD AIR. Front door, round-over, and the recorded voice line were not shot. The wav still says Den until it is re-recorded.
- **A2 Unequip.** Cause: the inventory click only calls Equip, which replaces a slot. Nothing called Unequip. `DrawEquipment` on the Character tab lists Main Hand, Off Hand, Head, Body, Trinket, with UNEQUIP (`Off|<slot>`) on a filled row. `DCCUnequipSlot` removes the item and refuses a cursed one. It does not put the item back in the bag. Shot `menu_night` b01: after `DCC.Hit Off|0`, every slot read empty. Log: `[DCC Equip] unequip slot 0 'FireAxe' removed`.
- **A3 Achievement popup.** `DCCHUD_Achievements.cpp` `DrawAchievementPopups`. Slides in 0.35s, holds to 4.0s, out by 4.40s, one at a time. Reward is the real one (XP, favor, loot name). Shot `achieve_night`: "First Light" / +40 XP, then "Still Standing" / +25 XP +2 favor. Glass is the crystal tint, readable on the sky, a little pale.
- **A4 Dead Air posts.** `DCCHauntReel.cpp` `BuildReel`. Every player is an uploader. One player gets 4 posts, two get 2, three or more get at least 1. Clip length 5s, each post stays up 10s. Rank pay is still 30/20/12 then 6 tickets and 15/10/6 then 3 credits. Solo is 68 tickets and 34 credits. Shot `haunt_night`: body-cam card includes "What you upload shows on the Social tab." Then 1/4 RUNAWAY CART (+30 tickets, +15 credits), 2/4 GHOST MINER, 3/4 IT WENT FOR THEM, 4/4 THE DEEPEST STOPE (+6 tickets, +3 credits), each with a card (ghost, monster, trap, notes, stories, views, likes). Log: `[DCC Scroller] 4 posts` all Zoe. A second player was not in the test, so the 2-and-2 split was not seen. This round had views (5,480), so a zero-view post was not seen. Wrote `Saved/SaveGames/Profile_Zoe.sav`.
- **A5 Far mountains.** `ADCCArcadeClimb::GetSky`: fog density 0.008 to 0.045 (0.0024 to 0.0135 after the 0.3 multiply), fog start 14000, volumetric fog off so the start distance works. Opaque filler blocks inside the five range shells. No Content edits. Log `[DCC Range]` once. Shot `climb_night` b01: the peak face is a solid cliff. A close peek of one far shell still showed the black inside of the one-sided mesh, with a dark fill block in it. A hazy view from the fort was not a clean shot.
- **B1 Social.** Menu tab 11 (Settings stays 10). Chatter, Allies, and Whispers are off the tab bar. Shot `menu_follow` b00, after the Dead Air round: CHANNEL 1.7k subscribers, 21.9k views, best post 5.4k, and the four uploads (Deepest Stope, It Went For Them, Ghost Miner, Runaway Cart). Friends: Zoe, Scott, CJ, Marcel, Vaughn, Kyle. Shot `menu_night`: whisper thread "With Scott" / "Zoe: hey". Log `[DCC Social] whisper to Scott: hey`.
- **B2 Attack styles.** `DCCAttackStyle.cpp`. Knife stabs, axe is overhead, empty hands punch. Every style still borrows a clip. Log `menu_night`: ChefsKnife Stab rate 1.45 borrowed `A_DCC_DW_ChargedSlash`; FireAxe Overhead rate 1.00 same clip; none Punch rate 1.00 `A_DCC_OA_BoxingWarmup`. The swing was behind the menu, so the motion was not judged by eye. Mixamo is still yours.
- **B3 Truck.** No boost meter. While the line is docked, Q holds the car overhead and spins it. A and D move the magnet and do not steer the truck during a hold or a struggle. Tilt drifts at 28 deg/s away from level, and the magnet pulls back at 90 deg/s. Under 25 degrees it scores. Q on a clean hold banks `clamp(score/100, 0.15, 2.5)` onto the drop pay and adds 12 fuel (cap 1.6 times the rated tank). Past 70 degrees for 0.35s it drops. First test launched the wrecker to z 4332 because the ground trace treated the held car as the road. `UpdateGroundAndAir` and `ProbeGroundAhead` now ignore `RallyTow`. Reshoot `rally_overhead`: the white car is up over the crane, tilted like a board, and four seconds later it is steeper. Truck z stayed 176. Log `rally_flip`: pose flip, then struggle held, then `flip dropped tilt 100 score 277`. Shot `rally_flip` b02 is that steep drop, the underside filling the frame. Fuel log: start 100 to 160, rated tank stays 100. Eight gas cans placed (+28, cap 1.6x). Not driven, so these were not felt: weight (cruise 5.8s to 9.4s, coast 1.6 to 0.48, steer 1.25 at a stop to 0.38 at cruise, scrub 0.12 to 0.04), drift (grip falls to 0.22 at 0.55/s, back at 1.6/s, slide up to 0.78), wall scrape (a hit under 0.62 keeps sliding), picking up a can, the clamp close-up, the struggle bar while seated, and the pay multiplier at the yard.
- **B4 Slabs.** Ledge rocks are uniform height 320 and sunk into the face. Overhangs are unstretched and have a rock under them. The hidden collision slabs stay. Log `[DCC Slab]` for every face. Shot `climb_night` b01: rocks sit on the cliff, and one small object still reads as floating. Nobody climbed, so the wrap onto the next face was not seen. No before-shot of the old pancakes.
- **B5 Sheet and looks.** `DrawStats`: STR INT CON DEX CHA, score in the box, banded modifier in a gold token (Zoe's were 14, 14, 16, 10, 12, all +4). Health, mana, stamina, evade, level, XP, can lift, and a WEAPON line. Skills show rank, mana, cooldown, and a BELT button that reorders the skill to the front of the belt. No new key. LOOK in Settings: CRYSTAL, NEON, TABLETOP, saved as `[DCCUi] Style` 0/1/2, ids UiCrystal, UiNeon, UiTabletop, not gated. Shot `menu_night`: crystal sheet and the three LOOK buttons, CRYSTAL lit. Shot `menu_follow`: skills with BELT on each row after the first, then tabletop parchment on the skills page and the character page (FireAxe, Overhead, stand-in clip, UNEQUIP). Log `[DCC Ui] style 2 cosmetic UiTabletop`. Neon was not clicked. Inventory was not opened in each look. The BELT button was not clicked. The test left LOOK on Tabletop. Click CRYSTAL to put the frost back.

### 2026-11-18 (later): leave, practice swing, allies, bag

Development Editor link succeeded. No numbered DLL.

- **Safe Room button.** Tab menu quick row, and the F2 party menu. Two clicks inside 6 seconds. The whole party travels. Hidden in the Safe Room, on the front door, and in the pose bench. Log `bail_room`: `bail armed`, then `Zoe bailed back to the Safe Room`, then travel to the lobby. Second shot is the neon SAFE ROOM sign. The first shot is the rally work order, which covered the button row. The click still landed. Wrote `DCC_Zoe.sav`. `Profile_Zoe.sav` was not touched. F2 was not shot on its own. A second player was not in the test.
- **Practice swing.** A left click with nobody in reach plays the weapon clip. No damage, no roll, no skill mark. A real hostile in range is still a real attack. Log `swing_bag`: `[DCC Swing] practice, no target and no damage`, axe overhead clip. The pose itself was not a clear frame. Cooldown, Climbing equipped, and a real fight were not checked.
- **Gear back in the bag.** Taking a piece off, or equipping over it, puts the old piece in the bag. A full bag still takes that one piece. A missing gear row still comes back, named by its id. This covers the bench, the shop, and booth guns, not only the Tab button. The A2 note above (unequip does not bag the item) is the old behavior. Log `swing_bag`: `FireAxe back in the bag (9 now)`, then `ChefsKnife back in the bag (10 now)` and `unequip slot 0 'ChefsKnife' removed`. The inventory page showed both rows.
- **Allies follow.** A summoned friend comes into every game and stands with you until they go down. A fall or a car can still lose one, and that death forgets them. Log `ally_follow`: free pull (no tickets) of Common Kyle with a Confetti Crossbow, then travel to Dead Air, then `Zoe's Kyle came along`. The cage shots show a second body at the right edge. The lobby frame caught the tube, not Kyle walking out. Wrote `DCC_Zoe.sav` again at 8:52 PM. `Profile_Zoe.sav` is still 11/17 9:07 PM.

### 2026-11-18 (later): item cards once

The flash cards were treating the save load as a fresh gain. A new space spawns an empty body, the HUD noted that empty bag, then the save filled it and every owned piece announced again. Cards now wait until that carry-in has sat still, remember those gear ids for the session, and flash an id only the first time it is gained or equipped after that. Log `item_cards`: lobby settled `8 in the bag, 1 worn`, travel to Dead Air, settled again with the same line, and no `[DCC Item] NEW ITEM` / `EQUIPPED` / `GOLD` lines. Travel rewrote `DCC_Zoe.sav` at 9:06 PM (same size). `Profile_Zoe.sav` was not touched. A brand-new pickup during play was not staged.

### 2026-11-18 (later): yard run and trick chain

While a mark is overhead, the truck drives itself along the streets to the turn-in yard and eases to a stop inside it. A and D stay on the magnet. A level hold (under 25 degrees) plays a flip on its own, about every couple of seconds: front flip, barrel roll, back flip, corkscrew, then it repeats. Space pops the next one early. The chain adds to the same payout Q already banks. Q while level banks it (then Q drops it if you are in the yard). Q while tilted, or a tilt past about 70, still drops the car. The chain pauses when you go past 25 and is lost if it falls. Log `rally_yard`: seated at the street crossing, `yard path dist 37947` down to `yard path in the lot, dist 6898`, and `trick 1 Front flip`. Nobody was on the magnet, so the tilt ran away and the log then says `flip dropped`. The 9:31:00 frame is the YARD RUN card on the neon street. The 9:31:12 frame is the car pitched over the truck, TOO TILTED, still rolling. A clean mid-flip pose was not a separate frame. Did not rewrite `DCC_Zoe.sav` or `Profile_Zoe.sav`.
- **Itch.** `Tools/publish_itch.ps1` cooked and butler pushed build **2026-11-18.2135** to `zoebubble/dungeon-crawler-denver:windows` (butler build #2433612, 129.94 MiB patch). Zip: `D:\Builds\DungeonCrawlerDenver-2026-11-18.2135.zip`. The previous live build was 2026-11-18.0617.

### 2026-11-18 (night): the yard run stops for parked cars

The outer-lane probes no longer drive up a ramp. Log `rally_lane` held the road (Z stayed about 202) from the Ix=+1 crossing down to Y=33780, then `cap 0` forever in front of a parked car. Zoe stopped that test. A parked car does not move, so a full stop there never ends. Not rebuilt tonight. Next path is a curb crawl: ride the empty strip outside the parked cars (about 19 m off the centre, sidewalk furniture is out near 27 m), step further out to pass one car, and reverse a few metres instead of waiting. Side changes only near a crossing, and only to get off a skyway's ramp side.

Queued from the same night. Built the next day. See the 2026-11-19 log. The yard curb crawl was not part of that build.

### 2026-11-19: series, mountain, gates, bodies, phone, camp

Development Editor link succeeded twice. The DLL is `UnrealEditor-DungeonCrawlerDenver.dll`, 8,074,240 bytes, 12:34 PM. No numbered DLL and no patch DLL. Not playtested. Not published. The yard path was not touched.

- **Unlocks.** Fourteener, Neon Mile, and Dungeon Crawler Denver stay open. Dead Air and Stress Test open after `Climb.Summits` is at least 1. Gate Run and Repo Rally also need `Haunt.BestViews` above 0 and `Audit.BestDamage` above 0, so a wipe or a miss does not open them. A player named Zoe can stand on any pad. A locked pad does not count as a vote, the logo dims, and the wallet says why.
- **Quest line.** For about the first 80 seconds of a round, a gold banner states the goal. Finishing it once writes `Quest.<game>` and grants XP: Fourteener +80 on top of the existing 150, and Dead Air, Stress Test, Gate Run, and Repo Rally +120. Repo Rally pays at the 10th delivered car. Neon Mile shows a line and pays no quest XP.
- **Fourteener rocks.** The ledge slab and the overhang wedge are no longer hidden, and they take `Mi_rock_cliff_02`. This is the likely invisible stump. Nobody has climbed it since the link.
- **Gate Run paint.** Plaza, rooms, walls, and the boss trim use concrete and tile materials already in the project. Fort Tiner's own gate was not repainted.
- **Dead Air bodies.** Three stories, first and last note only, plus one loose note, spaced farther apart. Warm rim and a note light until this player has found that note (`Haunt.Found0/1/2`). First find is +12 XP. Filming one adds footage. Not seen in a round.
- **Body cam.** While recording, the wheel zooms and the wheel click is the selfie. Food cycle stays on the wheel when the cam is off. A spray, an emote, a reaction, a death in frame, and a hit on the camera are meant to reach the reel. Not filmed in a round.
- **Phone.** A friend's name on the Social page opens that profile's posts. Whisper is a button on that page. Posts exist only if that `Profile_<Name>.sav` is on this machine.
- **Camp.** Fort Tiner's JSON has two new lines each from Dusty, Ma Ore, Gus, and Bea about the drones and the climb. After a summit, one more line each tells Vaughn finding the Lucky Tuesday under the pasture. Those four stay silent until `Climb.Summits` is at least 1. The first summit also puts a camp-radio line on the Fourteener alert for 20 seconds. This is talk and a radio call, not a staged scene of Vaughn walking into a hole.

### 2026-11-19 (afternoon): combat swings and aimed shots

Development Editor link succeeded. The DLL is `UnrealEditor-DungeonCrawlerDenver.dll`, 8,094,208 bytes, 2:05 PM. No numbered DLL and no patch DLL. Not playtested. Not published.

- **Aim.** Right mouse still zooms a ranged weapon. Left click fires only while that button is held. A click without aim asks you to hold right mouse, and it does not shoot. A friend under the cursor still gets a greeting. Until you aim, the mark is small and reads "Hold right mouse to aim." While aiming, the crosshair tightens and turns red on a target.
- **Swings.** From the two zips in `Content/Unsorted`: stab, outward slash, greatsword overhead, baseball strike, sword-and-shield impact, spell cast, and a wave. Axes and hammers idle, walk, and run with the greatsword. Swords use the sword-and-shield loops. Guns and throws still use the archery shot. Punches still use the boxing warm-up. Death stays a ragdoll.
- **Left aside.** Sitting Talking, Sitting Laughing, and Female Standing Pose were not imported. They fit Elyra's Tales better than a fight.
- **Import.** All 13 sequences saved under `/Game/DCC/Characters/Shared/AnimsFixed` and bound to `SK_DCCCrawler_Skeleton`. Not seen on a body.

### 2026-11-20: startup crash in the skeleton check

Development Editor link succeeded. The DLL is `UnrealEditor-DungeonCrawlerDenver.dll`, 8,093,184 bytes, 8:41 AM. No numbered DLL. A Den launch stayed up past the old crash and showed Zoe in the Safe Room. Not published.

- **Crash.** After character select the first locomotion tick called `CanPlayOnThisSkeleton` on the hammer's greatsword idle and read address `0xffffffffffffffff` while building a soft object path. The same check had also been handed the nameplate and the capsule, which are not animations. A static clip cache was keeping raw `TObjectPtr`s the garbage collector does not see, so after a travel those slots could come back as components.
- **Fix.** The check now ignores anything that is not a live animation and any skeleton pointer that is not a live `USkeleton`, and tries again next tick. Weapon clips are loaded fresh instead of cached. Retargeting refuses to write bone modes until `BoneTree` is the same length as the reference skeleton, because that write has no bounds check.

### 2026-11-20: jobs, bleed-out, quests, gate rooms

Development Editor link succeeded. The DLL is `UnrealEditor-DungeonCrawlerDenver.dll`, 8,125,952 bytes, 10:49 AM. No numbered DLL. Den and Gate Run were opened. Zoe did not pick a job and did not go down. `Profile_Zoe.sav` is still 11/19 1:34 PM and `DCC_Zoe.sav` is still 11/19 5:21 PM. Nothing was earned or spent.

- **Classes.** The starter kit is the class. Seven Earth jobs, one per game: Repo Driver (melee), Ridge Hand (hits taken), Call Sign (spells), Line Coach (heals), Mile Runner (ranged shots), Gate Warden (a smaller mix of melee and soaking), Floor Walker (a little of swings, shots, spells, and heals). Each level up to 5 teaches the next real skill. The percent grows with level and covers that whole kind of action. The Character page lists them. She has not picked one, so the page showed the seven rows. Picking one is once, from that page or the dungeon cards, and it still grants the kit gear and 25 tickets.
- **Bleed-out.** While downed, Space or the button on the down screen fails the death saves immediately. A tale floor can still rewrite that ending. Friends can still press E if you wait. Not staged, because that would record a death on Zoe.
- **Quests.** Every game, including the dungeon, uses the same one-line banner and the same 100 XP. Neon Mile completes at 500 metres. The dungeon completes when a floor is cleared.
- **Floors.** The Den and Neon Mile roads are 4 m synthwave tiles on the old collision slab. Stress Test is concrete tile. Rally asphalt was left flat.
- **Gate Run.** Four new rooms: the Toll (hold the booth), the Outage (lights, then two waves), the Clock (three waves that stack), and the Holding Pen (clear the ring; the fence burns). A gate with two or more middle rooms always includes at least one of them, and a longer gate includes about half. Log `gate_rounds2`: rank D, 5 rooms, seed 36450, `start fight gauntlet clock boss`. She stayed in the plaza.
- **Itch.** `Tools/publish_itch.ps1` cooked and butler pushed build **2026-11-20.1051** to `zoebubble/dungeon-crawler-denver:windows` (butler build #2448724, from #2433612, 132.49 MiB patch). Zip: `D:\Builds\DungeonCrawlerDenver-2026-11-20.1051.zip`. The previous live build was 2026-11-18.2135.

### 2026-11-20 (afternoon): combat idle and walk stood upright again

Selecting a crawler who is holding an axe, hammer, or sword played the new Mixamo loops, and those loops were leaning the body off the capsule. One-shot swings looked fine because the camera follows them. The skeleton asset and the body meshes were not part of it. `SK_DCCCrawler_Skeleton.uasset` is still 12,733 bytes from 11/13 9:01 PM.

The 11/19 rebake divided the crawler's hip height by the Mixamo hip's Z. Mixamo's armature is pitched 90 degrees, so that Z is 0 and the hips landed billions of units away. Idle, walk, and run also kept the source combat stance, which is what skews a selected character once hip translation is stripped at runtime.

`Tools/rebake_mixamo_batch.py` now scales by the longest hip axis (ratio about 0.70). The first repair turned every bone back to the bind pose. That bind pose is a T-pose (arms straight out), so a selected crawler slid around in it while the capsule still walked. The bake now turns the whole pose by one rotation, taken from the first frame's hips, so the hips stand upright and the arms keep the weapon hold. Attacks still keep the swing and the lunge. Walks step. `DCC.ImportCombatAnims` replaced all 13 sequences again at 3:07 PM, on `SK_DCCCrawler_Skeleton`.

### 2026-11-20 (evening): greatsword hands were over the head

The 3:07 PM greatsword idle put both wrists above the head. Mixamo's armature object is pitched 90 degrees, and the arm deltas were taken in that armature's own axes, so "arms down" landed as "arms forward and up." The source clip holds the sword low, in front of the hips, with the upper arms pointing down.

`Tools/rebake_mixamo_batch.py` now takes those deltas after both poses are in the crawler's space. A locomotion clip whose wrists start above the shoulders fails the bake. On the rebaked idle the wrists are at 0.25 and 0.34 above the hips, shoulders at 0.61. They sit in front of the waist. These arms are shorter than Mixamo's, so the hands do not reach the thighs. Seen on CJ's mesh in Blender, front and side. `DCC.ImportCombatAnims` replaced all 13 sequences at 8:39 PM, each on `SK_DCCCrawler_Skeleton`, and saved them. Not seen in the running game. The editor was closed for the import.

### 2026-11-20 (evening): loot boxes lean toward your skills

Opening a box still rolls the tier's table. Gear whose weapon skill or handling skill the opener already knows is more likely, and the rest of the gear in that box gives up the difference, so the box does not get bigger. Gold and anything with no skill stay at the authored chance. Rank 5 is a bit over three times the pull of a skill they do not have. The event log adds "It leaned toward your Axe." when a box had a lean. Development Editor link succeeded. The DLL is `UnrealEditor-DungeonCrawlerDenver.dll`. No numbered DLL. Not opened in a box yet.

### 2026-11-20 (evening): greatsword hold taken back off axes and hammers

Zoe rejected the Mixamo greatsword idle, walk, and run. Axes, hammers, sledges, and mallets use the body's own loops again, the same ones they used before those clips were added. Sword-and-shield idle, walk, and run stay. The overhead swing stays `A_DCC_Overhead`. The greatsword assets are still on disk under `AnimsFixed` and are no longer selected. Development Editor link succeeded in 18.79 seconds. The DLL is `UnrealEditor-DungeonCrawlerDenver.dll`, 8,130,560 bytes, 3:52 PM. No numbered DLL. Not seen on a body yet.

### 2026-11-20 (evening): Casino Cascade, off the Safe Room

The Den is the Safe Room. The class is still `ADCCArcadeLobby`. The sign already said THE SAFE ROOM.

Development Editor link succeeded. The DLL is `UnrealEditor-DungeonCrawlerDenver.dll`, 8,248,832 bytes, 5:00 PM. No numbered DLL. A `-game` look as Zoe opened the Safe Room, then `DCC.CasinoGo` stood her in the east doorway. Shots are in `Saved/TestShots/casino_room/`. She did not sit at a table. `Profile_Zoe.sav` is still 11/19 1:34 PM, 8,906 bytes.

- **Room.** A ceiling covers the Safe Room and the casino, so the sky stays outside. The synthwave floor tiles are gone. Both rooms are carpet, with a gold square on every other tile. The east wall is a door. Casino Cascade is through it.
- **Tables.** Skee-Ball, air hockey, roulette, blackjack, and a wheel. Six seats at each. A hand is whoever sat down. Two people can play blackjack. The rest of the room can keep voting. Someone in a seat does not count as a missing vote. If the vote does finish, the whole party still travels, seats included.
- **People.** Lane, Puck, Vale, June, Otto, and Bud at the bar. E on one of them is Play game, Request rules, or Shop. Shop is Cascade Cola (2), House Red (4), and Legal Bud (6). Roulette, blackjack, and the wheel stake 5 tickets, or the house comps a broke crawler. Skee-Ball and air hockey do not stake. While you are sitting, the wheel and E run the table, Esc stands you up, and everyone else sees dots over your head.

### 2026-11-20 (night): Casino Cascade, games on the walls

The five games sit on the walls and the aisle from the door to the east wall stays open. Development Editor link succeeded. The DLL is `UnrealEditor-DungeonCrawlerDenver.dll`, 8,278,016 bytes, 9:46 PM. No numbered DLL. A `-game` look as Zoe used `DCC.CasinoGo` and `DCC.CasinoSpot` 0 through 4. Shots are in `Saved/TestShots/casino_pass3/`. She did not sit. `Profile_Zoe.sav` did not change (8,933 bytes, 6:45 PM).

The installed packs have no skee-ball lane, air-hockey table, roulette wheel, or prize wheel. Those play surfaces are built in the room, and they move from the replicated hand (a skee ball, the puck, the roulette ball, the wheel). This look did not play a hand, so the throws and the paid spins were not on screen. Around them, the furniture is from the packs: the diner bar, stools, bottles, glasses, mugs and plates, beach ceiling lamps, the couch, plants, and the neon strips.

- **Skee-Ball.** Six lanes on the north wall. Each has a ball waiting and a ring target (50 in the centre, out to 10). The sign says "Three balls. The rings pay 10 to 50." This is the one that reads from across the room.
- **Air hockey.** Southwest, off the door. Wood cabinet, pale ice, gold and blue rails, a goal at each end, a puck and two mallets. Visible from the aisle. The shot puts it to the side of the frame.
- **Roulette and blackjack.** Diner stools on the open side, a wood top, green felt. Vale's sign is "Red, black, or green. Five tickets." June's is "June deals. Five tickets, or the house comps." June's table reads. The roulette pockets are still small next to the sign.
- **The wheel.** A large gold disc on the east wall, north of the CASINO CASCADE sign, with colored bars and Otto beside it. The bars lie flat on the disc and idle. They are not pie slices yet, and the pays are hard to read from the door.
- **The bar.** Diner counter along the south wall, stools, bottles and glasses on the top, purple neon, and the BAR AND BUD sign. Bud stands at the open end.

### 2026-11-20 (night): Play a casino game from the table

Walk up to a game and press E. That sits you and opens the same panel as before. The next E confirms the highlighted row. The wheel changes the row. Esc stands you up. Looking at Lane, Puck, Vale, June, Otto, or Bud still opens their menu, so rules and Bud's drinks stay where they were. Standing in the aisle does not sit you. Sitting does not take tickets. Roulette, blackjack, and the wheel still stake 5 when the hand actually starts, or the house comps a broke crawler.

Development Editor link succeeded. The DLL is `UnrealEditor-DungeonCrawlerDenver.dll`, 8,280,576 bytes, 10:08 PM. No numbered DLL. A `-game` look as Zoe stood in the doorway (no prompt), pressed E on a skee lane (the panel opened on Roll), scrolled to Leave, and stood back up. The lane then read `[E] Play Skee-Ball`. The middle of the room stayed quiet. At the wheel the prompt read `[E] Play The Wheel`. Shots are in `Saved/TestShots/casino_play/`. She did not roll and she did not stake. `Profile_Zoe.sav` did not change (8,933 bytes, 6:45 PM).

The wheel and the roulette wheel still need a visual pass. This change does not touch them.

### 2026-11-20 (late): Casino Cascade song, pit achievements, Esc and Tab

`Content/Songs/Casino Cascade.mp3` is imported as `/Game/Songs/Casino_Cascade` (157.6 seconds). It is not on the Den playlist. The Den playlist (`Meet_the_Whoops`, `Swept_Away`, `Repo_Rally`, `Platinum_Dust_Mine`) plays at volume 0.10. Inside the pit, including the doorway, that track is muted and only Casino Cascade plays, at 0.34. Walking back into the Den stops the casino song and starts the next Den track.

Development Editor link succeeded. The DLL is `UnrealEditor-DungeonCrawlerDenver.dll`, 8,302,080 bytes, 11:06 PM. No numbered DLL. A `-game` look as Zoe stood in the doorway, then teleported back to the Den. The log is `Saved/Logs/casino_song2.log`: Den volume 0.10, Casino Cascade on (158 s, 0.34), then song off and `Meet_the_Whoops` at 0.10. She did not sit and she did not stake. `Profile_Zoe.sav` was already 9,038 bytes at 10:25 PM before these runs, and it did not change.

The prize disc now faces the room and spins as a wheel, and the roulette pockets are taller. A close standing look of the slices and pockets was not taken on this DLL.

- **Achievements.** Six rows are added in code, so they count before the live DataTable is reimported. Tickets the house pays add up: On The Board at 25 (40 XP, 5 favor), Hot Table at 100 (80 XP, 10), The Room Noticed at 500 (150 XP, 20). Hands that beat the table: Against The House at 1 (40 XP, 8), Regular At The Pit at 10 (100 XP, 15). Skee-Ball scores pay tickets and do not count as a hand. One payout of 50 or more is The Loud One (80 XP, 20). A push does not count. None of these were earned in the test.
- **Menus.** Esc and Tab close whatever is in front. The character sheet, the emote list, talk lists, the loot list, and the vendor list say so on the footer. A casino panel says "Esc or Tab to stand." If the sheet is open while you are seated, the first press closes the sheet and leaves you in the seat. The cosmetic shop, the gear bench, Bud's shop widget, and the party menu also take Esc and Tab in code. Those four screens were not opened in this look. The sheet and the emote list were: Esc closed the sheet, Tab closed the emote list without opening the sheet, and the next Tab opened it. Shots are in `Saved/TestShots/casino_song/`.

### 2026-11-20 (late): friend build on itch

`Tools/publish_itch.ps1` cooked, zipped, and pushed. Cook log: 0 errors, 30 warnings, AutomationTool ExitCode=0. Zip is `D:\Builds\DungeonCrawlerDenver-2026-11-20.2312.zip` (4,619,186,810 bytes). Butler reused 93% of the previous upload and sent a 137 MiB patch. The windows channel is build #2454381, version `2026-11-20.2312`, and itch has finished processing it. The main menu shows that stamp. A host on this build will refuse a join from `2026-11-20.1051`.

### 2026-11-21: Gate Run's way home stands dark until the floor is clear

The exit ring in the Gatekeeper's hall is built with the rooms (`DrawPortal` frame only). The shine, lights and sparks come on when `bExitOpen` is set, which is still the Gatekeeper going down. Stepping through still waits for that. A dull WAY HOME board marks the cold ring. Not seen in a run yet.

### 2026-11-21: OBJECTIVES shows the starter quest lines

The OBJECTIVES card reads `DCCArcade::BoardSteps`. A new profile gets THE ASCENT (leave Fort Tiner, then High Camp, then the summit), and beside it THE STRIP (just drive the Neon Mile) and THE RUN (take Departures). After a summit the card moves to THE TAPE and THE AUDIT, then THE GATE and THE YARD. Inside a game the card is only that game's current step, or a one-line handoff while the next door is still shut. A finished line goes quiet. Floor quests stay on the same card in the Den, the hub, and a live dungeon run. Repo Rally draws the card mid-left. The gold QUEST line for the first 80 seconds repeats the current step. Development Editor link succeeded. The DLL is `UnrealEditor-DungeonCrawlerDenver.dll`, 8,318,976 bytes, 1:35 AM. No numbered DLL. Not seen in a run yet.

### 2026-11-21 (Claude): a simplification pass - less text everywhere
Zoe: "perform quite a bit of polish on this game to make it a little less over-engineered", and of Dead Air's cage: "this for example is way too much. So much text on top, on bottom, on the walls outside the cage". The rule now: if you already know how to play, nothing should be reading at you.
- **Top of the screen:** the multi-line status panel is one slim line, the game's name then its key numbers ("FOURTEENER  Zoe 9,002 ft  14:51 left", "DEAD AIR  4:50 to the cage  0 views", "GATE RUN  8:28 left  rooms 0/7"). Warnings ("!" lines: the bell, the roof, rockfall, debt, everyone up) still pop in an amber strip under it. Each game's timer moved onto its key line.
- **How-to text is a one-time tip** (`ADCCArcadeArena::GetTip`, `ADCCHUD::DrawArcadeTip`): one line, ten seconds, once per game per session. Fourteener, Neon Mile, Dead Air, Gate Run and the Audit have one. Dead Air's eight-line body cam card is retired (the tip says it).
- **Health bar** only when you are hurt or were hit in the last six seconds. Mana and XP meters (dungeon runs) are untouched.
- **Dead Air's corner:** just the AIR gauge; "deeper than your tanks reach", the gas meter and the canary only when they are telling you something. LAMP ON and LIGHTSTICKS are gone (the belt shows them).
- **Walls:** the cage's sentence-long boards are gone ("THE CAGE", "LUCKY TUESDAY MINE", "MINE OUTFITTERS" stay). The Safe Room pads lose their one-line pitch; name and record stay.
- **OBJECTIVES** is one line in the Safe Room, "NEXT  THE YARD: Tow 10 cars in one shift.", and nothing inside a game. The leftover dungeon-onboarding quests ("Orientation") no longer show in the arcade: floor quests show only in a real dungeon run.
- **The menu: five tabs** (CHARACTER, SKILLS, SOCIAL, ACHIEVEMENTS, SETTINGS) instead of nine, and two buttons (LOOT BOXES, PARTY, plus SAFE ROOM when you can leave) instead of five. CHARACTER has the bag in a second panel beside the sheet; the class is one line. SOCIAL has the party under the channel; identical uploads fold into one row with "x3". SETTINGS has MORE (pose bench, feed, log) at the bottom, and hides quick-reaction key rows with no key until asked.
- **Bug fixed:** Fourteener's belt showed Martial Arts instead of Climbing (the arcade showed whichever skill was first in the list, and the BELT button had reordered it). The arcade's skill slot is now Climbing when you know it.
- Repo Rally's work order: "YOU KNOW THE JOB" is two lines, not four.
- Checked in shots: Dead Air, Fourteener, Gate Run, Neon Mile, Repo Rally's work order, the Safe Room, and CHARACTER / SOCIAL / SETTINGS. Not checked by hand: clicking the new SETTINGS buttons, a warning strip during a real bell, the health bar appearing on a hit. Grok's earlier work is committed separately (78949e0) before this pass; this pass is not committed yet.
- Noticed, not changed: the bag reads "Stowed 14 / 12" (over capacity); a phantom "DESKTOP-..." crew entry in Dead Air's status data (no longer shown).

### 2026-11-21 (Claude): Casino Cascade's show games rebuilt, and Gate Run's textures fixed
Zoe: "The wheel and roulette both need work, and ski ball could use a bit more, like seeing the ball go up the table instead of just thrown. It should all be a lot more fun." And: "The gates have a weird stone floor for the main area and then no texture updates elsewhere."
- **The Wheel:** twelve slices (each pay twice), four bands each, a gold frame, twelve brass pegs, a red flapper that kicks and clicks on every peg, and 24 glowing rim bulbs (chase while spinning, flash on the winner). The spin runs 5.6 s with a long slow crawl at the end. The winning slice blinks. Bust thuds, a win chimes. The seats moved back (radius 820) so you can see the whole wheel.
- **Roulette:** a 37-pocket wheel in the real order (red and black alternate, one green 0), a wooden bowl with staves, a track, eight brass diamonds, and a turning brass cross. The ball runs the track the other way for seven laps, drops over the diamonds, bounces pocket to pocket with clacks, and settles in the pocket the server rolled, which then blinks.
- **Skee-Ball:** each lane is a cabinet now. The ball rolls up a sloped lane (slowing as it climbs), hops a lip, flies to a target tilted back at 38 degrees, and drops into the hole for its ring. Five rings around the 50, each with its hole. A thud on landing, a chime for 40 or 50. You stand beside your lane, not on it, so the over-the-shoulder camera sees the roll.
- **Payout waits for the spin.** Roulette and the wheel used to pay (and say the result) the instant the server rolled, so the ticket pop spoiled the spin. Now the line reads "No more bets." / "The wheel is turning." until the spin stops (`RevealAt`, `PendingPays` on `ADCCCasinoCascade`).
- **Sitting turns the camera to the game** (`Sit` calls `ClientSetRotation`, with a pitch per game). Before, sitting at the wheel left you looking back across the room.
- **Sounds:** procedurally synthesised ticks, clacks, rolls and chimes (`CasinoSound`). Each one stops after its length. Procedural waves otherwise run forever, and the first try piled up voices until the casino song restarted every frame.
- **The room:** the rainbow tile carpet became one deep wine field with a small navy-and-gold motif. Two explanatory wall signs removed.
- **Gate Run:** `ADCCArcadeArena::TiledBlock` builds a slab from patches so a pack texture repeats at its real scale. `Paint` stretches one texture across a whole cube, which is why the plaza showed one giant stone tile and room floors read as flat colour. The plaza is now a tiled Ice Station floor, the towers are stacked in storeys, room floors are tiled per cell, walls are one panel per cell, and the dungeon ceiling is a plain dark lid. Room lights are softer and half white, so the floor textures show.
- Dev: `DCC.CasinoSit <table> [delay]` sits the local crawler at a game.
- Checked in shots: the wheel spinning, slowing and landing; roulette from the seat; a skee ball landing in the 40 hole and its ring flashing; the plaza, two rooms and the walls. Not checked: the sounds (test runs have no ears), a full skee roll frame by frame (the screenshots are slower than the ball; the log traced its path), and other players watching. Test runs played as Zoe and spent 5 tickets a hand at the wheel and roulette. Not committed.

### 2026-11-21 (Claude): a lever for the wheel, slot machines, and Mother Lode scratchcards
Zoe: "It'd be nice if you actually got to pull a lever to spin the wheel or something. Or if we could have scratchcards and reels."
- **The wheel's lever:** a brass post with a red glowing knob in front of the wheel. The menu's last play row is "Pull the lever"; picking a wedge no longer starts the spin. The arm swings down, the wheel takes off as it bottoms out, and the arm springs back. The table waits 40 s for a pull (it was 6 s).
- **Four slot machines** on the north wall between the skee lanes and roulette. Walk up and press E (2 tickets). The lever on the right swings, three reels spin, and they stop left to right with a clack each and a little kick back. Symbols: cherries, lemons, bells, BAR, diamonds, 7. Three alike pays 3x to 50x (three 7s is the jackpot); a pair pays 1x (two cherries 2x). The sign shows WIN n or NO LUCK, a win flashes the topper and chimes. The server rolls the reels and pays only when the last reel stops.
- **Mother Lode scratchcards** from the teal machine beside the slots (2 tickets). A card opens on screen with nine silver cells; hold the left mouse button and rub to scratch (a brush of 26 px; most of a cell gone and the rest flakes off). Three alike pays that many tickets (1, 2, 5, 10, 25 or 100). The server deals the card whole (45% winners, every other amount at most twice, so losing cards tease). Buttons: SCRATCH ALL, LATER/DONE. An unfinished card pays out when you close it or buy the next. Mouse look is frozen and clicks never swing while the card is up.
- No comps on the machines: a free pull that can pay would be a ticket tap.
- New verbs on the existing `Server_CasinoAction` (5 pull, 6 buy a card, 7 settle a card), so no new RPCs. State: `Slots` and `Scratchcards` on `ADCCCasinoCascade`.
- Dev: `DCC.CasinoMachine <0-3 slot, 4 cards> [delay]` walks up to a machine and uses it.
- Checked in shots: two pulls (WIN 4 with two cherries, WIN 2 with two sevens), a card bought, revealed (no three alike) and closed, and the wheel lever pulled from a seat (landed on 1, paid 5). Not checked: rubbing with a real mouse (only SCRATCH ALL was clicked by the test), and two players at one machine. Test runs played as Zoe: spent 4 tickets on reels and 2 on a card, won 6, plus the wheel stake and win. Not committed.

### 2026-11-21 (Claude): sprays now stay put between games
Zoe: "I had hoped that sprays might persist between games! Is that currently possible?" It was meant to (the host saves each arena's tags in `Saved/Sprays/World_<arena>.json` and puts them back when the arena loads), but almost nothing was ever kept:
- **The bug:** a tag painted on something Movable was attached to it as if it were a truck, and attached tags are not saved. Every wall, floor and prop the arcade builds at runtime is a Movable mesh, so every tag was "riding a vehicle" and dropped at save. Only one tag (on a Dead Air prop) had ever survived.
- **The fix:** a tag rides along only on a pawn (the trucks, the traffic) or something under physics. Tags go back 4 s after the arena loads, when the runtime walls exist, and each is checked against a real surface first, so last run's Gate dungeon tags are not hung in the air. A spray made before the old tags are back now brings them back first instead of saving over them.
- Checked: sprayed a test tag on the Den floor, quit, relaunched: "1 tag(s) put back" and the tag was on the floor. Zoe's spray files were backed up before and restored after, so the test tag is not in her Den. Tags lost before this fix are not recoverable (they were never written). Not committed.

### 2026-11-21 (Claude): companions play Repo Rally (first of the "full AI players" games)
Zoe: "How about companions joining games with you? I've seen the bots join a game of repo rally with me but I'm not sure they have proper controls and like, guidance to play through each game." Asked what companions should be outside the fight games, she chose **full AI players**: they play each game themselves. Before this, a F.I.A.T. companion had one brain everywhere ("walk at my owner, hit what is hostile"), so in Repo Rally it ran on foot after your truck.
- **Repo Rally:** each summoned companion gets its own wrecker in the next yard bay (its body rides in the cab, out of sight, and its walking brain rests while attached). `ADCCArcadeKart::TickCompanions` is its head: pick the nearest free mark (companions do not double up, and switch to a much nearer one on a replan), drive the streets to it (row then column, crossing to crossing, out through the yard gate first), lead a running mark, fire the magnet inside 2200 (its line), crawl away while reeling so the mark trails onto the bumper, hold the clamp struggle (a steady hand: the balance is capped inside the safe band), tow home through the yard gate onto the drop pad, and drop. Same `FireHook` / `UseTool` / `Deliver` as a player, so scoring is the same.
- It says what it is doing on its player's truck radio: "Going after Hot Check.", "Hooked Hot Check.", "It's on the hitch. Taking it to the yard.", "Dropped Hot Check at the yard."
- It is on the CREW board and the shift report under its own name. It does not take a place in the ticket payout; instead its player gets **4 tickets per drop** it made.
- Stuck against a ramp, a pillar or traffic for ~4 s: it hops 25 m on along its own route. The city has a few spots where this happens (one is the skyway ramp near x -22800, y -60000).
- A log line per companion every 10 s (`[DCC Rally] companion ...`) for test runs.
- Checked: two full shifts, each with one companion: Kyle and Marcel each hooked, held the clamp, towed across the city and dropped a car (Marcel topped the report, 1 drop in 46 s, 85 banked). Seen: 3 to 6 stuck hops per shift. Not checked: two companions at once (the second free summon in the test did not land), and a companion working beside a real driving player. Not committed.
- Next games (not started): **Neon Mile** (race their own car on the racing line), **Dead Air** (ride the cage down, film creatures, back before the bell), **Fourteener** (climb their own line to the summit). Gate Run and The Audit already suit the fighting brain; to check that companions keep up between rooms.

### 2026-11-22 (Claude): companions play Dead Air
- **Errands for allies** (`ADCCEnemyCharacter::bHasErrand` and friends, `TickErrand`): a game can send an ally to a spot (over the grid where there is one) and have it face something, in place of following its player. Fights still come first, unless `bErrandAvoidFights`. Pushing against clutter for 2 s it hops; at 5 s it steps past. The leash to its player is off while on an errand.
- **Dead Air** (`ADCCArcadeHaunt::TickMineCrew`): each companion gets a camera and its own row on the footage board. It picks the best anomaly in reach that it has not filmed (and no other companion is on), walks out through the cage gate (the bars are not on the grid), films from about 6.5 m for 7 s, and moves on. It films a creature in plain sight from 10 to 18 m for up to 8 s, then leaves the creatures alone for 15 s. Tape only rolls with the subject in sight. A shot it gets no closer to in 10 s is given up. It heads for the cage when the clock (plus the walk back), a full tape, or 30% health says so, and says why on its player's feed.
- It is a camera, not a pick: it does not start fights, the mine's monsters do not hunt it (and drop it if they had locked on), and it only runs from one that is actually after it. The cage protects it like a player. Blasts and carts treat it as crew (`IsCrawler`), not as an 80-damage monster.
- Footage: scored by the same camera code as a player's (`Score` now loops players and companions), at 35%. It uploads only if it made the cage; its player gets 1 ticket per 100 views, at most 40 a round.
- Bugs found on the way (each from a test run): hazards one-shot it as a "monster"; it sat in the cage filming a creature through the bars; monsters camped the gate because they were hunting it; its sight check hit the creature it was filming, so tape never rolled; it walked into clutter on the way home and missed the cage.
- Checked: a full round with the test player sent into a stope (`DCC.MineGo 4`): CJ went 46 m into the mine, filmed, came back with 29 s left and uploaded 57,205 views. It still gives up on about a dozen unreachable shots a round. Not checked: two companions at once, or alongside a real player filming. Not committed.
- Navigation in general (Zoe, 2026-11-22: "Grok was also really struggling with the navigation"): the runtime-built arenas have no navmesh, so AI uses a coarse grid (Dead Air, Gate) or straight lines (Den, Rally trucks on autopilot). That is the common root of the stuck NPCs and stuck cars. A runtime navmesh would be the real fix for walkers.

### 2026-11-22 (Claude): Voice Drop, a link anyone can record lines on
Zoe: "I'd really like players to be able to upload their voice lines and even friends who haven't played the game ... a program I can host on railway ... have the game award those tickets ... when the voice line is shipped to the game (I can port them over. I just need it to notify me that they've uploaded)".
- **`VoiceDrop/`** (Node 18+, express and multer, no database: one JSON file and a folder of takes under `DATA_DIR`). The page: name, the name you play as, an optional contact, a consent tick, pick a character, then record each line in the browser (MediaRecorder) or upload a file, listen back, send. `[bracketed]` directions show in amber. Up to 40 takes an hour per address, 15 MB each.
- **Notifications** per upload: a Discord webhook (`DISCORD_WEBHOOK_URL`) and/or a phone push through ntfy (`NTFY_TOPIC`).
- **`/admin`** (password `ADMIN_TOKEN`): listen, download, set the tickets and the in-game name, Approve / Mark shipped / Reject / Delete.
- **`Tools/voice_inbox.py`**: `catalog` builds the character and line list from `SourceArt/Voice/NPC/*_lines.json` (characters cast in `cast.json` are left off; lines with no recording show first) and `--push` sends it to the live site. `pull` brings every approved take into `SourceArt/Voice/NPC/<Speaker>/VO_<row>.wav` (the `radio_mix.py import` chain), writes a credit to `Content/DCC/VoiceCredits/credits.json`, and marks it shipped.
- **The game** (`DCCVoiceCredits` in `DCCWalletComponent.cpp`): on the host, each credit is paid once to the player whose name matches ("+25 tickets: your voice line shipped (Brad from LIQUIDATOR: ...)"), remembered in their profile (`Voice/Credit_<id>`). A visitor is paid only after their profile has merged. `Content/DCC/VoiceCredits` is staged into packaged builds.
- Deploy steps are in `VoiceDrop/README.md` (Railway: root directory `VoiceDrop`, a volume at `/data`, the variables).
- Checked locally: catalog (14 characters, 81 lines), a take uploaded, a bad line refused, the admin password enforced, the notification fired (to the log, no webhook set), approve, `pull` converted it and wrote the credit, the site showed it shipped, the game paid it once and not again. The test take and credit were removed afterwards; Zoe's profile kept the 1 test ticket. Not deployed: that needs Zoe's Railway account. Not tested: recording in a real browser with a mic (the page loads and lists lines; the recorder is standard MediaRecorder), Discord and ntfy for real. Not committed.

### 2026-11-22 (Claude): a real navmesh in the arcade arenas
- The arenas are built at runtime on an empty map, so there was never a navmesh: walkers used a coarse grid (Dead Air, Gate) or walked straight at their target (the Den), and got stuck. Now `RecastNavMesh` generates at runtime (`RuntimeGeneration=Dynamic` in `DefaultEngine.ini`), and each arena registers navmesh bounds around its own geometry 3 s after it starts and again as it grows, for 40 s (`ADCCArcadeArena::UpdateNavBounds`). The bounds volume gets its size from a box collision shape, because the brush a volume normally uses is only built in the editor.
- Repo Rally and Neon Mile opt out (`WantsNavmesh`): huge maps, and their AI drive.
- Decoration with no collision no longer affects navigation, so the casino's spinning parts never dirty the mesh.
- AI (`ADCCEnemyCharacter::CanUseNavmesh`, checked once a second): with a navmesh under it, following, chasing and errands use `MoveTo`. With none, or when a navmesh route fails (then for 8 s), the old grid or straight line.
- Checked: bounds registered in the Den, Dead Air and Gate Run (plaza and dungeon in one box). A Dead Air companion walked by navmesh, filmed its whole tape and was back at the cage with 101 s left (29 s on the grid). Gate creatures still reached and downed the test player. Not checked: frame rate with the mesh building (it is async), creatures in Dead Air now preferring the mesh over the grid in a long round, the Den's followers. The Repo Rally trucks still drive by street routes; a navmesh does not help cars. Not committed.

### 2026-11-22 (Claude): Dead Air's footage has sound
Zoe: "can you make sure that our game properly captures audio and includes it in the dead air filming?" The reel's posts are not video: every machine records positions and camera angles ten times a second and re-stages the clips with stand-ins through scene captures (`DCCHauntReel.cpp`). No sound was ever recorded, so every post was silent.
- **The round's sound is logged** (`ADCCArcadeHaunt::SoundLog`, every machine, like `Recording`): the knocks (rockfall, planks), the cart, the powder blast, the firedamp flash, the bell, and any character sound played through `PlayCharacterSound` (via the new `ADCCArcadeArena::OnWorldSound` hook).
- **The creatures have a voice now.** They had none: their `AttackSound` was never set, so a fight was silent live and on tape. Every machine is told of each attack (`ADCCArcadeArena::OnAttackSeen`, called from `Multicast_PlayAttack`); in Dead Air a monster's lunge snarls (three generated voices) and a blow that lands thumps. Heard live, and logged.
- **The reel plays it back.** While a post plays, the sounds logged inside its stretch play again on cue, placed as the clip's camera heard them and carried over to the viewer's ears (`ReelToListener`): a blast to the left of the shot is to your left. A post that loops plays its sounds again.
- **The mine's air under it.** During the feed, the mine's bed (at the depth of the clip's camera) and its drips, creaks, taps and rumbles play around that camera. The song stays off: found footage.
- Dev: `DCC.HauntEnd [delay]` runs the clock out now.
- Checked by log (test runs have no ears): a 70 s round with the test player in a stope, filming. Before the creature voices, 4 sounds were logged and only one post had any. After, 54 were logged and every post replays 4 to 6, each post with its camera as the ear. Not heard by anyone yet: worth a listen in a real round. Not checked: two players (each machine logs and plays its own, so it should match). Not committed.

### 2026-11-22 (Claude): Voice Drop shows what is taken
Zoe: "I approved 7 of Anke's for Flagger Diaz yet they still seem to be showing as if they're available. Also, it's just not that clear what's been done and what hasn't". The site only knew the catalog's static "wanted" flag, so approved takes changed nothing on the public page.
- **Status comes from the takes now** (`progress()` in `server.js`): each line is open, in review (a take is waiting), taken (approved), or done (shipped). `/api/catalog` sends it; `/api/admin/progress` too.
- **Casting:** the first reader approved or shipped for a character is its voice. That character's remaining lines are reserved for that name; anyone else sees "Reserved for X" with no record button. Typing the cast name unlocks them.
- **Public page:** characters with open lines first, badges ("Open for anyone", "Voiced by X", "Complete"), counts, a progress bar, and lines grouped: still to record, waiting for review, taken, in the game. The ticket box hides when nothing is left for the viewer.
- **Admin:** a Progress tab (now the default): every character, who voices it, a bar, and each line's state. A line in review opens the New tab.
- Checked locally with test data: Anke's 7 Diaz takes approved, Diaz reads Complete and Voiced by Anke, no record buttons; a shipped Jan line cast Jan to Lee (0 buttons for others, 7 for "Lee"); a waiting Rhee take reads in review. Needs a push and a Railway redeploy to go live.

### 2026-11-22 (Claude): Gate Run gets themes, hazards and contracts; the level cap goes to 20
Zoe: "Please build out the gate run quite a bit for me :) we need much more stylized arenas, variety, etc." and "it kind of seems like I start at level 5 and never move from there ... I'd like to see balance and how rewarding finishing quests actually is."
- **Why levels stood still:** `UDCCStatsComponent::MaxLevel` was still the Floor 1 tutorial cap of 5, so every XP point past it was thrown away. It is 20 now (the HUD no longer says "tutorial cap"). The one-time series quest XP went from 100 to 250.
- **Themes** (`Arcade/DCCGateThemes.h/.cpp`): every gate rolls one of seven from its seed. Each has its own floor, wall and trim materials, light tint, ceiling, pillars, wall clutter, a centrepiece in fight rooms, ambient effects, a floor hazard and a favoured set of creatures (60% of spawns). The plaza board and the status strip name it, and the fog and sun take its colour.
  - THE DATA CENTER (Ice Station, live cables), THE STORM DRAIN (sewer kit, sludge), THE FOUNDRY (forge, slag), THE MAUSOLEUM (Egyptian and Sun temples, curse glyphs), THE AFTER HOURS (synthwave club, laser floor), THE OVERGROWTH (jungle ruins, thornbeds), THE HIVE (alien biomass, spore beds).
- **Hazards:** 1 to 3 patches in the open fight rooms (never in doorways or the middle). They cycle: live 3.5 s, quiet 2 s, a 1 s flicker before going live. A lit rim shows when one is live. They hurt crawlers a little and foes more, so walking a crowd through one is the play.
- **Contracts:** three side goals a run, picked from the seed among those the rooms allow: Exterminator, Lure Job (3 foes die in the hazard), Untouched, Beat the Clock (40% of the time left), Full Sweep, Clean Hands (solve a vault), Belt Holder (the Champion), Hot Feet (the gauntlet unburned). Each pays every crawler in the gate 60 + 30 x rank XP and 4 + rank tickets. A CONTRACTS card sits top right and ticks them off.
- **XP in the gate:** every room pays XP as it falls (18 + 7 x rank; the Gatekeeper 90 + 35 x rank), on top of tickets and the killer's own kill XP. At the end each player sees one line: "GATE CLEARED +501 XP from rooms and contracts (1 of 3 contracts) Level 6".
- **Foes are pitched at the party:** level max(their own, 1 + rank, party average - 2 + rank / 2), the boss 2 more, so a high-level crawler still gets a fight and real XP.
- **Testing:** `-GateTheme=N` (0-6) fixes the theme; `DCC.GateIn 100` puts you at the first hazard. `Tools/run_test.py` takes extra flags in `DCC_TEST_ARGS`.
- Checked in -game runs: all seven themes built (shots of Storm Drain, Foundry, Mausoleum, After Hours, Overgrowth, Data Center; Hive by log), the contracts card ticking and failing, a gate cleared with the summary line, XP landing and saving across games, a hazard rim lit. **Zoe's profile:** the test runs took her crawler from level 5 to level 6 (about 1,000 XP in) and added gate tickets. Not checked: a full hand-played run for balance, hazard damage on foes in a real fight, Lure Job completing, the theme board's position on the plaza at every rank.

### 2026-11-22 (Claude): level-ups - an XP bar, stat points, class spells to 20, a System window
Zoe: "the classes and level ups need a bit more work. Like an exp bar, stat raises between levels, new spells, isekai style pop up boxes like the inventory".
- **XP bar in every game** (`ADCCHUD::DrawXpStrip`): a slim gold bar along the bottom with LV, "401 / 2800 XP", a rising "+120 XP" flash when XP lands, and "3 STAT POINTS [Tab]" while points wait.
- **Stat points** (`UDCCStatsComponent::StatPointsSpent`, saved): 1 a level past the first, 1 more every fifth level (23 by level 20). Only what was spent is stored, so crawlers who levelled before this have theirs waiting. The Character page shows "N STAT POINTS TO SPEND" and a green + on each score (`Server_RaiseAbility`, +1 to the Unenhanced score; health and mana ceilings move, current values do not refill).
- **Class lessons to level 20** (`Classes/DCCBasicClasses`): each class now teaches a spell at 7, 10, 13, 16 and 20 (its ultimate). Class bonuses grow at full rate to 5, half rate after (level 20 counts as 12); Ridge Hand's damage cut is capped at 45%.
- **23 new spells** (`Skills/DCCSpellBook`): defined in code; `UDCCSkillComponent::FindSkill` falls back to them when DT_DCCSkills has no row of that id (a table row wins, so one can be retuned in the editor). All Damage or Heal, cone / area / single target, with pack VFX. Repo Driver: Final Notice, Hostile Takeover, Total Repossession. Ridge Hand: Avalanche Warning, Whiteout. Call Sign: Feedback Loop, Radio Silence, Interference, Station Identification. Line Coach: Timeout, Penalty Flag, Two-Minute Drill, Halftime Speech. Mile Runner: Hot Lap, Photo Finish, Slipstream, Victory Lap. Gate Warden: Lockdown. Floor Walker: Clean-Up on Aisle Five, Customer Service, Grand Reopening. Shared: Second Wind, Hold the Line.
- **Spells on the belt in Gate Run** (`ADCCArcadeArena::ShowsSpellBelt`): the three newest Damage/Heal spells. Before this no arcade game but Fourteener put any skill on the belt.
- **The level-up window** (`ADCCCharacterBase::HandleLevelUp`, `Characters/DCCCharacterLevelUp.cpp`): every level from any source (kills, rooms, contracts, quests) opens the gold System window: LEVEL UP, the new level (or "Level 6 > 8"), max health before and after, the stat point, the lesson just learned, the class bonus, and the next lesson. A burst of levels within 0.6 s is one window. E dismisses it.
- Dev: `DCC.Xp <amount> [delay]`.
- Checked in a -game run (After Hours gate): 2000 XP took the test crawler from 6 to 7, learned Second Wind, the window showed every line, the Character page showed the + buttons and 7 points, the XP strip showed LV 7 and the points. That run was not saved (killed before any travel), so Zoe's save is still level 6, 540 XP. Not checked: clicking a + (the hit box is the same kind the class picker uses), casting a new spell in a fight, the window popping during a fight.

### 2026-11-22 (Claude): Gate Run chests roll real loot; hazards placed more often
Zoe: "make sure that loot is randomized from appropriate tables. It seems like I've gotten the same chest on the first level of gate run several times".
- **Why:** `ADCCArcadeGate::SpawnChest` picked one curated chest class by rank (rank E and D always got the Emergency Equipment Locker), and the curated chests hold the same three items every time.
- **Now** (`Items/DCCGearRoller`): any of the five chest models, filled from DT_DCCEquipment: 1 to 4 different items, rarity weighted by the gate's rank and the chest's grade (treasure room 0, trial and Champion 1, a forced vault 1, a solved vault 2, the Gatekeeper 3), plus gold that scales the same way. Rows with no model are skipped. The chest is named for its best item ("Legendary Cache"). Celestial only turns up in the best chests of high gates.
- **The Gatekeeper drops a grade 3 chest** beside the way home. It dropped nothing before.
- **Hazards** were too fussy about placement (most gates rolled none). They now only avoid the doorway cell itself and the middle 4.7 m.
- Checked by log over five random gates: six chests, 17 different items, best rarity from Gold to Legendary; hazard counts 4 and 6 in the bigger gates. Not checked: opening one of the new chests by hand.
- **Asked about, not done:** the spells all share one cast animation and six generic pack effects (fire, sparks, smoke, snow, motes, the hit flash), and class lessons are a straight line, not a branching tree. Zoe asked for an itch.io update only if both were true, so nothing was uploaded.

### 2026-11-22 (Claude): every spell has its own show; class perk trees
Zoe: "Do the spells have different animations / spell effects? ... if classes actually have interesting skill trees please update to itch", then "make sure our skill tree options are humerous, in the tone of dungeon crawler carl / the TTRPG".
- **Spell shows** (`Skills/DCCSpellFx`): a local, unreplicated actor every machine spawns from `UDCCSkillComponent::PlaySkillEffects`. Each spell is built from glowing shapes (the Megastructure pack's emissive master; the Cyberpunk hologram drew nothing) that fly, grow and fade: Beam, a jittering feedback beam, Bolt, a giant rubber Stamp slammed down from above, Fan, shockwave rims, a Cage of bars, a Dome of stacked rings, hail (Rain), Orbit, Pillars of light, confetti (Fountain), Broadcast. Each has its own two colours, a point-light flash, and a comic word over the caster ("PAST DUE", "SOLD!", "REPOSSESSED", "SKREEEE", "YOU'RE LISTENING TO DCC-FM", "FLAG ON THE PLAY", "WET FLOOR", "GRAND REOPENING"...). The table's old spells got looks too (Static Burst, Ball Lightning, Fire Fingers, Lien Enforcement, Mark and Collect, First Aid).
- **Cast moves:** `Multicast_PlayCast` now carries the skill id. Each spell plays its own clip from Shared/AnimsFixed (overhead slam, charged slash, air squat, spin jump, archery shot, call gesture, agree gesture, boom dance, cardio dance, all-night dance, boxing warm-up), squeezed to about a second.
- **Mana costs rescaled:** mana is the INT score (usually 3 to 14), and the table's own spells cost 0 (cooldown-gated), so the spell book's 8 to 26 were uncastable. Now 2 to 4, and 6 for the level 20 finishers.
- **Perk trees** (`Classes/DCCClassPerks`): every basic class has four tiers (levels 4, 8, 12, 16), two perks each, 56 in all, written as the System AI's commentary, most a trade (Aggressive Collections: melee +15%, heals -10%: "You no longer knock. Your Yelp rating has entered a death spiral."). Effects: melee, ranged, spell, heal, damage taken (still capped at 45% overall), mana cost, cooldowns, spell area, HP per kill, XP. Picked on the SKILLS tab (click; click the other to swap, free). Saved on `UDCCClassComponent::Perks`. Applied in `DCCBasicClasses` percents, `UDCCSkillComponent::FindSkill` (cost, wait, area), the kill XP path (XP and heal) and Gate Run's XP. The level-up window and the XP strip say when a perk is waiting.
- Dev: `DCC.Cast <SkillId> [delay]` (teaches it, fills mana, casts at the nearest foe), `DCC.Perk <tier> <option> [delay]`. Film effects with `slomo 0.15` in the command list.
- Checked in -game runs: Final Notice, Hostile Takeover, Whiteout, Grand Reopening, Lockdown, Station Identification, Victory Lap and Second Wind each drawn as designed (shots at slow motion); the perk tree on SKILLS with two picks; the merged level-up window. Not checked by hand: clicking a perk with the mouse, a spell's look on a remote client.
- **Itch:** this build was packaged and pushed with `Tools/publish_itch.ps1` (see the next entry for the result).

### 2026-11-22 (Claude): friend build on itch, 2026-11-22.1718
`Tools/publish_itch.ps1` packaged (PACKAGE_OK, zip `D:\Builds\DungeonCrawlerDenver-2026-11-22.1718.zip`, 4,619,317,509 bytes) and pushed. Butler reused 93% and sent a 131 MiB patch; windows channel build #2472046, version `2026-11-22.1718`. It carries Gate Run's themes, hazards, contracts and rolled chests, the level cap of 20, the XP bar, stat points, the class spells, perk trees and spell shows. A host on this build refuses a join from `2026-11-20.2312`, so everyone needs the update. Not committed to git.


### 2026-11-22 (late, Claude): after the first night online with Vaughn
Zoe hosted, Vaughn joined over Tailscale (a shared machine plus a grant that lets shared users reach only UDP 7777). The notes from that night, and what was done:
- **The Integrity Office** (`Session/DCCIntegrity`). Where the money came from: Mother Lode scratch cards paid back 2.14 tickets on a 2-ticket card (45% win rate), so buying them as fast as possible printed tickets (1,440 cards in one night, about 3 a second at peak). Vaughn's stat edits were memory edits on his own PC; the host never accepted them (the log has no admin action and no level or score change from him). One real hole: F1's DM panel opened for guests, and its server calls (gold, level, invulnerable, spawn, refill, equip, grant skill) had no host check. Now:
  - Cards win 38% (pays back 1.81, the house keeps ~10%, like the slots at 0.90).
  - DM panel and its server calls: the host's own crawler only (`IsHostsOwnCrawler`). A guest gets "The DM panel belongs to the host", and a strike if their game calls one anyway.
  - Rate limits (`DCCIntegrity::Allow`) on casino actions (6/s, burst 12), summons, loot boxes, shops and stat points. Over the limit: refused, one strike per 30 s of spam.
  - Every 5 s a guest's game reports its own scores, level, gold and tickets (`Server_IntegrityReport`). Higher than the host's numbers, three reports in a row with the host's numbers not moving: a strike ("editing their own Strength (99 on their screen, 4 on the books)").
  - A visiting profile may bring at most 3,000 tickets on a first visit and 1,500 earned away since the last; more is capped and a strike.
  - Punishment, never a ban: strike 1 a public notice and a 10% fine; strike 2 a 25% fine and five minutes of garnished winnings; strike 3+ half the tickets, ten minutes garnished, and CHEATER on the nameplate (UNDER REVIEW before that). Strikes persist in the profile (`Integrity.Strikes`).
  - Achievements: **Tampering With The Evidence** (1 strike, +25 favor, 0 XP) and **Repeat Offender** (3).
  - **Audit button** in the F1 panel (host only, two clicks): level 1, every score 2, no gold, 10 tickets, and the profile revision jumps 1000 so the host's copy wins the guest's next merge.
- **Vaughn reset** on Zoe's PC (`D:\Itch Games\...\Saved\SaveGames`): `DCC_Vaughn.sav` deleted (fresh crawler next visit), `Profile_Vaughn.sav` patched in place (tickets 2,316 -> 10, Dead Air credits 546 -> 0, revision 25 -> 1025 so this copy beats his PC's). Backups: `D:\Builds\SaveBackups\2026-11-22-before-vaughn-audit`.
- **Gate Run audio** (`Arcade/DCCGateAudio.cpp`): a room tone per theme (hum, air, a throb), a pack ambience loop per theme (hallway, drips, steam vent, cave wind, slum interior, fountain, sparks), theme details (server chirps, drips, an anvil, bass through a wall, birds, chitter), footsteps for everyone by stride and body size, a thump where a blow lands and a whoosh on a miss, a sizzle near live hazards, and a drum pulse while a hostile is near (faster for the Gatekeeper). Made in code like Dead Air's mine. Not heard by anyone yet: the test runs prove it runs, not how it sounds.
- **Bleed out** now rolls the remaining death saves about a second apart instead of counting as three failures. The prompt reads "SPACE roll the death saves now".
- **One-handed swings:** `DCCAttackStyle::FromId` gives the two-handed clips (greatsword overhead, bat swing) only to two-handed gear (sledge, Thunder Hammer, fire axe, halberd, longsword, bat, lacrosse stick, kettlebell). Hatchets, hammers, mallets, pans, maces, skillets and the rest use the one-handed slash (`Chop`, the body's own loops); knives, chisels and the spear stab.
- **Repo Rally radios:** every truck's radio is near-silent unless this machine's player is driving or riding in it (`ADCCRover::UpdateRadioListener`).
- **Climbing bounce (guest):** the host stops correcting a guest's position while they climb or crawl a ledge (`bIgnoreClientMovementErrorChecksAndCorrection`); both machines move the body the same way, and the corrections in between were the bounce. **Jetpack (guest):** only the owner's machine thrusts, and the host accepts their position while airborne. Both need a second player to confirm.
- **Gacha ten-pull:** 900 tickets (ten for nine), at least one Rare or better, a pick screen of ten cards, keep up to your free ally slots, the rest evaporate. Dev: `DCC.SummonTenFree <tube> [delay]`. **Casino regulars' card:** every 100 tickets played pays 8 back.
- Checked in -game runs: the ten-pull screen (pick two, keep, both came down the tube), a Gate Run fight with the new audio code and the new downed prompt. Not checked: anything that needs a guest (the strikes, the report, the radio, the climb and jetpack fixes), and the audio by ear. Development Editor build succeeded, no numbered DLL. Not on itch yet, not committed.

### 2026-11-23 (Claude): the audience, the trick ring, practice, readable menus, and sound fixes
Zoe's notes after the Vaughn night, round two. All built (Development Editor, no numbered DLL); checked in -game runs as noted.
- **Live audience** (`Arcade/DCCAudience`). `ADCCGameState::LiveViewers` and `Hype` (replicated), ticked every 0.5 s on the server. Moments (kill, down, revive, trick, perfect trick, jackpot of 25+, achievement, level up, Gate room clear, Gatekeeper down) raise viewers and hype, give the crawler a little AI Favor, and set chat off with reaction lines written in `DCCAudience.cpp`. Between moments the chat keeps talking from the existing `DT_DCCChatterLines` table (every 9 s cold, 2.5 s hot). At full hype a sponsor tips whoever set it off 5 tickets, at most once per 90 s each, and the hype drops to 75%. The HUD draws LIVE, the viewer count, the hype bar and the last four chat lines bottom right in every game (moderated lines stay in the Chatter tab). Chat no longer quotes feed lines with code names in them. Seen in a Gate Run and the Den ("LoanOfficerLarry: ding" on a level up).
- **Repo Rally trick ring.** While a car is overhead a ring closes on a target; Space or a click as they meet. Within 0.07 s is PERFECT (1.5x points), within 0.17 s NICE, else MISS. Each hit lands the next rung: Front Flip 15, Barrel Roll 25, Back Flip 40, Corkscrew 60, Double Corkscrew 90, The Helicopter 130, The Full Repossession 180. A miss drops back to the first rung and keeps the points. The style points become the pay multiplier only at the yard (1 + style/250, at most x5). Gone: the tilt that could drop the car, mid-tow banking, four trick keys. The guest's own screen times the press, clamped to 300 ms behind the host's clock. Companions press on the beat. Checked by log (five PERFECT presses climbing the ladder); the ring graphic itself is not yet seen in a screenshot, because the test driver could not be seated in a truck.
- **Rally Practice**: a new Den pad (`ADCCKartPracticeGameMode` / `ADCCArcadeKartPractice`). No shift clock, no fuel burn, no tickets, scrap or quest. Leave from the Tab menu.
- **Perks commit.** The first pick in a tier is free; changing it costs 60/120/180/240 gold (`DCCClassPerks::SwapFee`). The SKILLS page draws each perk as a card with PICK, KEPT or SWAP Ng.
- **Stat points count.** `UDCCRuleset::GetScorePowerMultiplier`: +5% damage or healing per point of the deciding score above 4, on melee (`ResolveAttack`) and spells. The CHARACTER page lists what each stat does and the score where its bonus next steps up. Enemies' raised scores in Gate Run hit a little harder too.
- **Menus scale with the screen** (`FUiScaleScope`, `AddUiHitBox`): the Tab menu, the DM panel, System windows, the F.I.A.T. panel and the ten-pull grow with screen height (over 700 px), times SETTINGS > MENU SIZE (small, normal, large, huge).
- **Sound.** Chests: a chunky latch, the creak, the lid thud, coins and a rising chime (`DCCChestSound`). Footsteps: a quiet scuff every stride for everyone in every game (`DCCFootsteps`, in `Characters/DCCSynthAudio`). NPC voices 1.8x louder with a wider full-volume radius. `DCCSynth::PlayPcm` stops a code-made sound when its samples run out (they never end by themselves; footsteps and hits would have piled up). Gate Run's sounds go through it.
- Dev: `DCC.ClickAt <x> <y> [delay]` clicks the canvas through the real hit test; `DCC.RallyTrick <n> [delay]` now presses the ring n times on the beat.
- Not on itch yet, not committed.
- **Fame replaces sponsor tips** (same day, Zoe: "a progress / exp bar that awards prizes or lootboxes at various levels progressing in rarity"). Each audience moment earns Fame (kill 3, trick 3, perfect trick 8, room 6, revive 12, jackpot 10, achievement 8, level up 10, Gatekeeper 40), doubled at full hype, at most 150 a minute. Level n needs 100 + 60n. Rewards, as loot boxes into the crawler's own boxes: Bronze, Bronze, Silver, Silver, Gold, Silver, Gold, Platinum, Gold, Legendary, then Celestial every 10th level, Legendary every 5th, Platinum or Gold between (`DCCAudience::FameReward`). Saved on the arcade profile (`Audience.Fame`, `Audience.FameLevel`), mirrored to the owner's wallet for the HUD. The LIVE corner shows FAME, the bar and the next box; a level-up shows the achievement toast and a SponsorDesk chat line. Seen: three level-ups filled the bar about 40% (the test wrote ~40 Fame to Zoe's profile). Not seen yet: a Fame level-up paying a box.
- **Damage numbers by player, and damage skins** (Zoe: "color code the damage numbers ... maybe we could even have damage skins"). `ADCCCharacterBase::NoteDamageDealtTo` now multicasts the hit with its dealer (`Multicast_DamageNumber`, unreliable), and `ADCCFloatingText::SpawnDamage` matches it to the plain number from the health change, so a hit shows once. Each player's numbers are their colour (`DCCDamageSkins::PlayerColour`: CJ orange, Scott blue, Marcel green, Vaughn purple, Kyle yellow, Zoe pink, anyone else from their name); your own are a size bigger; a summoned ally's are its summoner's colour, a little dimmer. Damage skins are prize-counter cosmetics in a new Numbers slot (`UI/DCCDamageSkins`): Inferno, Frostbite, Toxic Waste, Gold Standard, Neon Mile, Dead Air, Past Due, 250 to 500 tickets, each with a colour, a glow layer behind, a prefix or suffix, and its own motion (flicker, drift, wobble, pop, glitch, stamp). Dev: `DCC.TrySkin <Id> [delay]` wears one unsaved. Seen: Inferno numbers ("17!") in a Gate Run fight.
- **Open season on cheaters**, one way (Zoe: "the cheater should only be able to take damage and not give it"): any player may hit a player with the CHEATER tag (3+ strikes); the cheater cannot hit other players (`IsHostileTo`). Allies and mobs do not join in. The full-audit announcement says so. Not tested (needs two players).
- **PRIMETIME with Hiss Cranson** (Zoe: "a talk show NPC with many voice lines ... introduce the players and show them the fame meter"). A stage at the west end of the Den (`ADCCArcadeLobby::BuildTalkShow`): platform, back wall, the Dam City desk, the Beach House couch, PRIMETIME and ON AIR signs, and a FAME BOARD that lists everyone by Fame level, refreshed every second (`TickTalkShow`; `UDCCWalletComponent::FameLevel` now replicates to everyone). Hiss (`Arcade/DCCTalkShowHost`, an NPC Forge office body) runs a segment when you are within 13 m: an opener, each crawler introduced by name with a Fame reaction (or a CHEAT line when they have strikes), two Fame explainers, a tip about something new, a sign-off; a quip 30 s later, the next segment after 75 s. 70 lines in `Tools/gen_talkshow_lines.py` -> `SourceArt/Voice/NPC/talkshow_lines.json`, tagged OPEN / INTRO_<name> / INTRO_ANY / FAME_LOW / FAME_MID / FAME_HIGH / CHEAT / FAME / TIP / CLOSE / IDLE, all voiced with Kokoro (`--voice`; machine takes listed in `ai_takes.json`, replaceable by a friend's recording). Seen in a Den run: the opener, Zoe's intro, her Fame reaction and an explainer, each with its voice.

- **The host is Hiss Cranson** (renamed from Vance Primetime the same day): speaker id `Hiss`, voices in `SourceArt/Voice/NPC/Hiss/`, his self-introduction rewritten and re-voiced ("I'm Hiss Cranson, and yes, the hiss is silent. Except when it isn't."). The show is still PRIMETIME.
- **Voice Drop rewards audit.** The game side is sound (credits load from `Content/DCC/VoiceCredits/credits.json`, pay once per player name on the host, remembered in the profile). The reason nobody has been paid: nothing was ever pulled on this PC. There is no `Saved/VoiceDrop.json` (site address and admin token), no credits file, and Anke's 7 approved Diaz takes are not in `SourceArt/Voice/NPC/Diaz`. Fix: create `Saved/VoiceDrop.json`, run `python Tools/voice_inbox.py pull`, set each take's game name to the crawler they play as, then ship a build.
- **OPEN MIC WEEK, the Hiss Cranson audition, and the Hot Mic** (Zoe: "the gold box and nameplate should be the highest reward and then Hiss Cranson's voice will be the only one to get the Hot Mic ... let players only submit one voice line for him ... the rest stay locked until a player is chosen. Then I manually unlock the section").
  - **Voice Drop site** (`VoiceDrop/server.js`, `public/index.html`, `public/admin.html`): a weekly track (Monday to Sunday, UTC) counted from each player's approved and shipped takes: 1 line a Bronze box, 3 a Silver box and 50 tickets, 6 a Gold box and the OPEN MIC nameplate. `/api/event?name=` and a banner with milestones, a bar and days left. A catalog speaker with `audition` (Hiss: sample row `TS_Open_01`, prize the Hot Mic) takes the sample line once per person; every other line is locked. In /admin, each audition take has "Choose X as the voice"; the Progress tab has "Unlock the rest for X" and "Clear the choice" (`/api/admin/auditions`, stored in `auditions.json` under DATA_DIR). Once chosen the audition closes; once unlocked only the chosen voice can record his lines. The holder gets a Hot Mic clip upload on the page (`/api/clip`); clips are approved like takes.
  - **`Tools/voice_inbox.py pull`** now also writes event milestone credits (`evt-<monday>-<player>-<n>`, with `box`, `tickets`, `title`), the audition prize (`cast-Hiss-<player>`, `item: HotMic`), and turns an approved clip into `SourceArt/Voice/NPC/_Clips/<player>.wav` (first 4 s). The catalog marks machine takes as wanted, so Hiss's 70 Kokoro placeholders are open for real voices.
  - **Game:** credits can pay a loot box, a nameplate title (shown under the name for everyone, `UDCCWalletComponent::NameTitle`) and the Hot Mic (granted as an owned id on the profile). **The Hot Mic** is a belt slot: press to go live (a feedback whine), and the next damage or heal spell within 8 s is 40% stronger over a 30% wider area and plays the holder's clip (or an air horn) for everyone; 45 s between uses (`Server_HotMic`, `ConsumeHotMic`, `Multicast_HotMic`).
  - Checked end to end on a local copy of the site with test data: one audition per person, other Hiss lines refused, the audition closing once a voice was chosen, the clip refused for a non-holder, Hiss lines opening only for the chosen voice after unlock, Marcel's week at 3 of 6; `pull` wrote 3 line credits, Bronze, Silver + 50 and the Hot Mic; the game (playing as Marcel) paid all of them once, put Hot Mic on the belt, and going live then casting Whiteout played the clip. Test files, credits and Marcel's test saves were removed afterwards. `Tools/run_test.py` takes `DCC_TEST_URL` (e.g. `?Crawler=Marcel`) to test as another crawler.
  - **To go live:** push the repo and redeploy the Railway service (the site's code changed), then `python Tools/voice_inbox.py catalog --push` (needs `Saved/VoiceDrop.json`).
- **Model import, fixed** (Zoe: "Meshy's output by the time it reaches our game ... is just a little unpolished. Maybe we aren't doing it right."). Two findings and a new path:
  - Every rigged Meshy file on disk (all six crawlers, the wraith) carries only a colour map and an emissive map: Meshy drops the normal and metal/roughness maps from its rigged and animated exports. The unrigged "texture" exports have them all. So part of the flat look was decided in Meshy.
  - The old road (glb -> Blender -> FBX -> MCP import) kept no material links; we rebuilt colour-only materials.
  - **`Tools/import_glb_pbr.py`**: Unreal's glTF importer (Interchange) run as a commandlet with the editor closed. It brings every map in, and repoints the materials at `/Game/DCC/Materials/Imported/M_GLTF_DCC`, a project copy of the engine's glTF master with the skeletal-mesh flag on (without it the game drew the grey default material: "missing usage flag SkeletalMesh"). `--compare` also makes a colour-only copy. `DCC.ImportCompare` stands both in front of you with a white key light.
  - Tested on `Downloads/Meshy_AI_Character_output.glb` (an alchemist, rigged, 101k triangles, 2K colour, normal, metal/roughness and emissive): both versions drawn side by side in the Den (shots in `Saved/TestShots/importcmp3`). The new one has real metal on the buckles and flasks and depth in the coat; the old one is flat. Test assets are in `/Game/DCC/ImportTest` (about 60 MB) and can be deleted.
  - Not done: rigging an import onto the UE Mannequin (the next step for Hiss), and re-doing the existing crawlers, whose source files never had the maps (that needs Meshy's textured exports of the same models).
- **Cillinan at the prize counter** (Zoe: "Can you import that character into the game as a shop keeper ... at the prize counter? His name is Cillinan"). The Meshy alchemist, imported with full PBR to `/Game/DCC/Characters/Cillinan` (`Tools/import_glb_pbr.py`), stands behind the counter on a hidden 30 cm step (Zoe: "perfect where he is ... behind the counter there is great"; do not move him forward), facing the gold pad, in his own sculpted pose with a slow breathing bob (no Mannequin rig yet). `ADCCShopkeeperNPC` (`Arcade/DCCShopkeeperNPC`): walk within 7.5 m and he greets you, pitches two things and drops a quip; then a mutter now and then, a fresh pitch after 45 s. 18 lines in `Tools/gen_cillinan_lines.py` -> `SourceArt/Voice/NPC/cillinan_lines.json` (GREET / PITCH / QUIP), voiced with Kokoro (an older British voice), green captions. Seen and heard in a Den run.

### 2026-11-23 (late): first Slate screen
- The Tab menu's CHARACTER page is now Slate (`UI/DCCCharacterSheetWidget`), shown by `ADCCHUD::ShowCharacterSheet` while that tab is open; the Canvas page stays as a fallback. Three looks (`DCC.SheetLook 0-2`: System Glass, Neon Arcade, Broadcast) with OFL fonts in `Content/DCC/UI/Fonts` (staged via DefaultGame.ini). Waiting on Zoe's pick of a look and/or a Fab UI kit before porting the other pages.
- Same night: the whole HUD now paints through Slate (`UI/DCCHudPaint`): `ADCCHUD` hides AHUD's DrawRect/DrawText/GetTextSize/DrawLine/DrawTexture and records them; a click-through Slate layer replays them in the look's fonts with rounded panels. Canvas-only parts (gauge, minimap, mine reel, loot box screen) use `FCanvasOnly`. One look for everything (`UI/DCCLook`, the SETTINGS style: Glass, Neon, Broadcast; Tabletop retired). Subtitles are styled by speaker (`DCCLook::VoiceFor`: Hiss gets a lower third, the System cyan, Cillinan green). Item icons are rendered from each item's mesh by `DCC.MakeItemIcons` into `Content/DCC/UI/ItemIcons/<Id>.png` (106 of them); a hand-made `T_Icon_<Id>` still wins.
- Later the same night: **loot box rarities look different** (`ADCCHUD::DrawTierAura`: halo for all, turning rays from Gold, sparkles from Platinum, colour-cycling Celestial; procedural textures in `UI/DCCFxTextures`). **OPEN MIC WEEK page** (`UI/DCCEventPanelWidget`, Tab menu button OPEN MIC WEEK, `DCC.EventPanel [delay] [lines]`), in the purple-and-gold gacha style Zoe sent; weekly counts come from `credits.json` "week", written by `voice_inbox.py`; the RECORD A LINE button needs `[DCCVoiceDrop] Url` in DefaultGame.ini. **Pose bench FIT** buttons (Smaller/Bigger/Slimmer/Wider/Taller/Shorter/Thinner/Deeper, Auto-fit sizes head and body pieces to the body's shoulders). **Dead Air rebuild plan**: `Docs/dead-air-primetime-plan.md` (Hiss's show, segments, interview, send-offs), waiting on Zoe's answers.
- **PRIMETIME first playable** (Dead Air rebuilt as Hiss's show, `Docs/dead-air-primetime-plan.md`): `Arcade/DCCHauntPrimetime.cpp` adds the show to `ADCCArcadeHaunt` (on unless `dcc.DeadAirClassic 1` / `-DeadAirClassic`). Opening in a studio built 300 m above the mine; 4 levels down the Lucky Tuesday (Gauntlet, Lights Out, Hold Out, then the finale in the deepest stope); after each, survivors go up to the couch and answer Charm / Banter / Defy (`Server_PrimetimeAnswer`); Hiss's per-guest mood gives Producer's Pet or Marked, the team's average gives a Blessing (Overtime, Lighting Department, Extra Takes) or a Twist (Special Guest, Tight Schedule, Budget Cuts). The out sit in the studio audience among NPC Forge extras and watch a surviving crewmate. 86 new Hiss lines (P_ tags in `talkshow_lines.json`, Kokoro). HUD in `UI/DCCHUD_Primetime.cpp`; Hiss's voice is a broadcast (`ADCCHUD::ShowBroadcast`). Dev: `DCC.Primetime <0-3>`, `DCC.Answer <0-2>`. Fixed on the way: a teleport counted as a fall (`ADCCCharacterBase::ForgetFall`). Not yet tried with a second player, nor the audience view after dying.
- 2026-11-25 (night): **the uniform bone maker.** `Tools/unify_rig.py` rebuilds an already-rigged model on the crawler skeleton (Biped, Mixamo, Mannequin, Meshy rigs), `Tools/import_unified.py` imports it headless, and `ADCCEnemyCharacter_Unified` subclasses give it the crawler animation set. First two: **Mushroom Demon** and **Demon Bull** (`/Game/DCC/Characters/Unified/`), seen walking and fighting in the Safe Room via `DCC.SpawnEnemy`. The Mushroom Demon's textures did not come through (its material references a texture folder the blend does not link); the bull's did. New Fab packs added tonight are listed in `Docs/fab-library-audit-2026-11-24.md`; `Polytope_Studio/.../T_Quinn_01_CCRCCPlastic_MSK.uasset` was unreadable at import time (likely mid-download).

### 2026-11-25 (Grok): two new Den games, compile-checked only

Dungeon Crawler Tower Defense (`Siege`) and The Champions' Ring (`Ring`) are in the arcade list and always open. E talks to the arena (towers, posts, the ring's three pads). No new keys. Spell and melee damage pick up the tower-defense buffs. The Safe Room and Casino Cascade grew caption TVs. SETTINGS has a Credits button. Pack copy record: `Docs/fab-copies-2026-11-25.md`.

The balance script (`Tools/gen_siege_waves.py`) says a solo crawler loses the wall on wave 13, two players on wave 21, five on wave 33. Those DPS numbers were not measured with `DCC.Fight`. Ring ladder is 50 stages in `Content/DCC/Ring/ladder.json`.

Compile-checked with the standalone DebugGame target, then the Development Editor DLL was linked at 8:36 (`UnrealEditor-DungeonCrawlerDenver.dll`, no numbered hot-reload DLL). The old DLL was left beside it as `UnrealEditor-DungeonCrawlerDenver.dll.stuck-old` because the leftover mushroom-demon `-game` still had it open. That process is gone. No playtest, so nothing was written to Profile_Zoe.sav. PRIMETIME and the Fab spell catalogue (plan A1-A2) are still waiting. The crystal cannon and the announcer are the only new-pack effects wired in.

**Update 2026-11-25 morning (Claude):** the Development Editor build is done (06:38, clean link, no numbered hot-reload DLLs). The stuck pid 30704 was Claude's own Mushroom Demon test run. It had exited but Windows kept its handles, so it still held the Live Coding lock and the DLL file. The build used `-NoHotReloadFromIDE`, and the old DLL was renamed to `UnrealEditor-DungeonCrawlerDenver.dll.stuck-old` so the link could write a new one. Delete the `.stuck-old` files after the next reboot. Siege and Ring are now in the editor's code.

**Playtested 2026-11-25 (Claude, one crawler, as Zoe: +2 and +3 tickets, +250 XP from the Ring's series quest).** Siege: waves 1-3 marched (Mushroom Demons among them), the wall held, the crawler fought at the wall. Ring: E on the center podium started stage 1, cleared in ~8 s, paid. Fixed: the gold QUEST box sat under the title strip whenever the health bar showed (every arcade game, `DrawSeriesQuestBanner`); the Ring's planet hologram was full size 4 m from the podium and swallowed the camera (now 3 m, by the banner); the Ring title read "Stage 1  Stage 1". Still open for Grok: all 50 ladder stages are named "Stage N" (`Content/DCC/Ring/ladder.json` wants real names); towers, ally posts, the crystal, camps and the tournament were not exercised.

### 2026-11-25 (Claude): groundwork for world-making
- `Tools/kit_catalogue.py` lists every mesh (size, triangles, role) and map in 23 place and prop packs: `SourceArt/Kits/*.json`.
- `Arcade/DCCSets`: stream a pack's finished sample map into a game as a ready-made room, stripped of its sun and sky, with
  its bounds. Dev preview `DCC.Set`, `DCC.SetView`, `DCC.SetWalk`; `run_test.py` gained `DCC_TEST_WAITFOR`.
- Toured 15 sets in game. Ready to use whole: the Broadcast Studio (PRIMETIME), the Metro, the Command Center, the sci-fi
  interiors and hallways, the engineering bays, the Power Plant hall (tower defense), Elderboom village, the Inferno lava
  crater (Ring champion stage). Showrooms, pieces only: Old Mine, Saloon, Clinic. Verdicts and grids: `Docs/kits.md`.
- Also this session: the QUEST box no longer sits under the arcade title when the health bar shows; the Ring's hologram
  is shrunk and moved; the Ring title no longer says "Stage N  Stage N".

### 2026-11-25 (Claude): tower defense moves into the Power Plant
- `ADCCArcadeSiege::BuildLocal` now streams the Abandoned Power Plant level in as the map (`DCCSets::Load`, turned 90
  degrees). Enemies form up in the yard, file in through the annex's side doorway, cross the annex and snake through the
  32 x 22 m hall to the Wall at the far end. Two towers in the annex, two in the hall; camp 1 is the annex's dead end,
  camp 2 the yard nook through the annex's other doorway. Players start beside the Wall, off the road (on it, marchers
  stop to fight and never reach the Wall). Work lights down the hall; fog 0.02 -> 0.006 (it was soup indoors).
- **For Grok:** the layout is in set coordinates (`HallPoint(U, V)` in `DCCArcadeSiege.cpp`); `GetStartTransforms`,
  `BuildLocal`'s first half and `GetSky`'s fog changed. Gameplay code is untouched. Move a tower or camp by editing
  its `H(U, V, Z)`; the comment above `HallPoint` says where the walls, doorways and pillars are.
- `DCCSets` fixes found on the way: the ready callback was reading its own captures after clearing its timer
  (options came through empty, then a crash); actors are destroyed after the scan, not during it; the floor is the most
  common lowest surface, not the one under the middle (basement holes); probes trace the Pawn channel.
  New option `RemoveMeshes` (the Power Plant's closed doors block walking, so they are removed).
- Seen in test runs: waves walk in, cross the hall and hit the Wall; the camps sit where planned. Not yet checked:
  allies on posts and the crystal from the new tower spots, a full multiplayer round, and the first-load shader wait.
- Evening: **working doors in every set** (`UDCCSetDoorComponent`, on by default). They open as anyone walks up and
  shut 2 s after; narrow ones swing, wide ones slide along the wall. The Power Plant's 6 doors work, and a wave opens
  the side door on its own. Grok's plan has Part I for the rest of the simple things (door sounds, lockdowns, loose
  props, seats, lamps that flicker).

### 2026-11-25 (Claude): Epic Online Services, for joining from anywhere
- `Session/DCCOnline`: each PC signs in to EOS by itself (an anonymous device id, no Epic account, no login screen).
  HOST A PARTY lists the party; JOIN A PARTY shows open parties as buttons (REFRESH, USE A COPIED INVITE as backup);
  the game runs over EOS's relay, so nobody forwards ports or installs Tailscale. The main menu shows whether EOS is
  signed in. Built on the engine's `OnlineServicesEOSGS` + `SocketSubsystemEOS` (plugins enabled in the .uproject;
  config in `DefaultEngine.ini`: `[OnlineServices.EOS]`, `[OnlineServices.Lobbies]`, the GameNetDriver definition).
- **Owed:** the Client Secret in `DefaultEngine.ini` (`ClientSecret=PASTE_CLIENT_SECRET_HERE`). Until then the menu
  says "Online is not set up yet" and everything runs as before over IP (tested: hosting falls back to IpNetDriver).
- **Not tested yet:** sign-in, the party list and a real join. Two copies of the game on one PC share one device id,
  so the first real test needs a friend on another PC. Dev: `DCC.Host [delay]` hosts from wherever you are.
- The first editor build of that file failed with C3861 on `EOS_Platform_GetConnectInterface(*Platform)`. MSVC drops the
  extern "C" getter instead of using the handle's conversion. `DCCOnline::Start` now copies the handle out first.
  Sign-in itself is still untested.

### 2026-11-25 (Grok): spell pass

The morning note that the Fab spell catalogue was still waiting is out of date. Part A of `Docs/grok-plan-2026-11-25-fab-packs.md` is in.

- **Catalogue** (`Docs/vfx-catalogue.md`): 137 Niagara systems in the six effect folders. Spline systems and projectiles are listed and not used as a point cast. There is no point-spawn ice system, so ice spells wear the air aura and a shockwave.
- **One table** (`Skills/DCCSpellBook.cpp`). `ApplyPresentation` fills cast, impact and height after the skill table or the book is resolved, and before class perks. A DataTable row still wins on numbers. No new scale property on the skill row. Scales are plain floats, applied when the system is spawned. Looping auras are switched off after 1.4 s, impacts after 1.8 s. The 2026-11-22 mesh spell show still plays on top.
- **Starters in the book:** Static Burst, Ball Lightning, Lien Enforcement, and Mark and Collect were not in `DT_DCCSkills`. They are in the book now, so a class that teaches them has a cast. A later table row of the same name still wins.
- **Hits and deaths (A3).** Weapon hits spawn a puff and skip it past about 25 m: fire uses `NS_Explosion_Small`, piercing uses the magic slash, blunt and fists use the concrete impact. Debt Eaters and the unified bone-maker monsters (ragdoll off) pop the small explosion instead of portal smoke. World explosions use the Free Spells explosion when they are fire, and the small dirt blast otherwise.
- **Sheet.** `DCC.ShowFx` spawns one system on the local pawn and cuts it off after 2.2 s. Thirty systems on the pose bench: `Saved/TestShots/spell_fx2/sheet.png`. The first pass (`spell_fx`) put them behind her while shaders were still compiling, so that sheet is not the one to read. At scale 1, a fire aura, a lightning buff and Level Up fill the room. A shockwave and the slash are about two body-heights. The small explosion is a loose sparkle. Ordinary casts were pulled down near 0.3 after that. The level-20 shows (Total Repossession, Whiteout, Station Identification, Halftime Speech, Victory Lap, Lockdown, Grand Reopening) stay large. The Free Spells explosion itself was still compiling on the sheet, so its size is still a guess.
- Linked into `UnrealEditor-DungeonCrawlerDenver.dll` at 16:05 (9,796,096 bytes). No numbered hot-reload DLL. `DCC.ShowFx` pays nothing. `Profile_Zoe.sav` is still the 08:35 file, 15,137 bytes. This was not a combat playtest.

Claude's later notes were read and not started. Ring stages are still named "Stage N" (Part H5). Part I after the doors Claude already built (door sounds, lockdowns, loose props, seats, flickering lamps) is still open.

### 2026-11-25 (Grok): friend build 2026-11-25.1640

Git `433845f` is on `main` (tower defense, the Ring, the spell pass, set doors, EOS parties). The Fab sample heroes stayed off git. The friend build is stamped `2026-11-25.1640` and was pushed to `zoebubble/dungeon-crawler-denver:windows`.

The first cook failed on one empty file, `Content/Polytope_Studio/Demo/Characters/Mannequins/Textures/Quinn/T_Quinn_01_CCRCCPlastic_MSK.uasset` (0 bytes, the same as the vault copy; the night log already called it unreadable). It was replaced from that pack's Modular Armors copy (12.4 MB, a real package). Nothing else in `Content` was empty. The second cook succeeded. Zip: `D:\Builds\DungeonCrawlerDenver-2026-11-25.1640.zip` (about 4.5 GB). EOS sign-in is in this build and still needs a friend on another PC.

