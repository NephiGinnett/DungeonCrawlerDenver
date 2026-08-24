# Instructions for a dispatched Claude session

**Read this first, before touching anything.** It describes what a session working
on this project without Zoe present can and cannot do, and the specific traps that
have already cost this project multiple wasted sessions.

---

## The one rule that matters most

**A build "succeeding" is not evidence your change is running.**

After ANY build, check `Binaries/Win64/` for a numbered DLL:

```bash
ls "Binaries/Win64/" | grep -E "0001|patch_"
```

If `UnrealEditor-DungeonCrawlerDenver-0001.dll` exists, the editor was open, UBT
could not relink, and it **hot-reloaded instead**. The editor keeps executing the
old code. `Result: Succeeded` prints either way.

This has silently wasted three separate sessions on this project. Somebody fixed a
bug, reported it fixed, and the user kept hitting it for two more days.

If a numbered DLL appears: delete it, confirm no `UnrealEditor` process is running,
and build again. Do not proceed as though the change is live.

---

## What you CAN do without Zoe

| Task | Notes |
|---|---|
| Write and edit C++ | The bulk of useful work |
| **Compile-check** | `DungeonCrawlerDenver` / `DebugGame` — works with the editor open, ~2-4 min |
| Read/search any file on disk | Including `Content/` file sizes, which is how the texture problem was found |
| Edit `Config/*.ini` | Takes effect on next editor start, no build needed |
| Edit `Docs/*.md`, CSVs in `Docs/` | Source-side only; importing them needs MCP |
| Convert art with Blender | `D:\Blender\blender.exe --background --python Tools/glb_to_fbx.py -- <src> <dst> 0` |
| Run git | Commit only if asked |

### The compile-check command

```bash
"D:/UE/UE_5.8/Engine/Build/BatchFiles/Build.bat" DungeonCrawlerDenver Win64 DebugGame -Project="C:/Users/kmett/Documents/Unreal Projects/DungeonCrawlerDenver/DungeonCrawlerDenver.uproject" -WaitMutex
```

This builds the **standalone game target**, which the editor never loads — that is
exactly why it is safe with the editor running. It verifies compilation and linking.
**It does not deploy anything.**

---

## What you CANNOT do without Zoe

| Blocked | Why | What to do instead |
|---|---|---|
| **Deploy code** | The `DungeonCrawlerDenverEditor` / `Development` build needs the editor fully closed | Compile-check, then write it into a handoff |
| **Anything in the editor** | MCP (`mcp__unreal-mcp__*`) needs the editor running AND a live session; it drops on every restart and cannot be reconnected non-interactively | Queue it in the handoff |
| Placing actors, editing Blueprints, importing assets, DataTable edits, texture settings | All MCP | Same |
| **PIE / testing** | You cannot run the game or click anything | Add logging that answers the question on the user's next run |
| Browse Zoe's Fab library | Account-side behind Epic auth | Ask her to download; only `Content/` is visible |

**When you cannot verify something, instrument it.** The established pattern here:
add a `UE_LOG` that prints the actual inputs off the **live actor**, prefixed with a
searchable tag (`[DCC Cast]`, `[DCC Anim]`). Then one PIE run answers the question
instead of a session of theorising. Two such diagnostics are already deployed.

---

## Project-specific traps, learned the hard way

1. **Read the subclass, not just the base class.** `ADCCCharacterBase::CorpseLifeSpan`
   is 0, but `ADCCEnemyCharacter` overrides it to 8. A fix was reported based on the
   base class and was wrong for every enemy in the game.

2. **Read the placed instance, not the class default.** Blueprint CDOs and the
   actors serialised into the level have disagreed twice. `bIsAlly` was `true` on
   four CDOs and `false` on the four placed actors, and the party spent a playtest
   being beaten up by their own allies.

3. **Blueprint values beat C++ defaults where explicitly set.** A C++ constructor
   default reaches a Blueprint only for properties that Blueprint has *not*
   overridden. Animation slots read `None` (the default) so C++ defaults land;
   `StartingSkillIds` is authored, so C++ values there are inert or harmful.

4. **`ObjectTools.set_properties` works.** An older note said it was broken; the
   calls were malformed. It takes `values` as a **JSON-encoded string**. Soft object
   pointers take a plain path string, not a `{"refPath": ...}` object.

5. **Always read a value back after writing it.** Do not trust return codes. Two
   "fixes" to the pickup collisions were recorded as done and were not.

6. **Never use Live Coding.** It reports success on reflection changes and then
   crashes the editor on PIE.

---

## Current state (2026-08-18)

**Deployed:** everything up to Aug 17 21:39 — HUD, interaction menu, revives and
downed state, loot-on-body, Balloon Payment, cheats, ranged weapon support, AI scan
throttling, skeleton guard, audio hooks, `ADCCGameMode`.

**Compile-verified, NOT deployed:** gear-inspection panel, `DCCHost` / `DCCWhoAmI`,
`HurtCameraShake` hook.

**Applied, effective next editor start:** `Config/DefaultDeviceProfiles.ini` capping
environment textures at 2048. Cave_Camp is 1.76 GB with 1713 MB of textures, twelve
over 40 MB — the cause of 7 FPS with GPU 99% and memory pressure.

### Both gameplay bugs, diagnosed (2026-08-18)

One PIE run plus MCP reads off the live actors settled both. **This is the single
reconciled account** — earlier drafts of this section, and the corrections appended to
them, contradicted each other in three places and have been removed. Do not append
another "correction to the above"; edit this in place instead.

The evidence for everything below is the raw log, verbatim:

```
[DCC Cast] Mob_Brute_Courtyard  INT=8  mana=8/8  bUseSkills=true  table=DT_DCCSkills  skills: (knows nothing)
[DCC Cast] Scott_OfficePlaza_Demo  INT=12  mana=12/12  bUseSkills=true  table=DT_DCCSkills  skills: Critic(cost 0) NumberCruncher(cost 0)
[DCC Cast] TestBoss_InternManager  INT=4  mana=4/4  bUseSkills=true  table=DT_DCCSkills  skills: ParkingLotSpecial(cost 0) QuickWit(cost 0)
[DCC Cast] Crawler_Kyle  INT=5  mana=5/5  bUseSkills=true  table=DT_DCCSkills  skills: (knows nothing)

[DCC Anim] Scott_OfficeAirplane_Demo: refusing 'A_DCC_QA_Alert' (skeleton 'SK_DCCCrawler_Skeleton') on mesh 'SK_OfficeAirplane' (skeleton 'SK_OfficeAirplane_Skeleton'). Wrong rig.
[DCC Anim] Kyle_ArmsOut_Demo: refusing 'A_DCC_OA_Alert' (skeleton 'SK_DCCCrawler_Skeleton') on mesh 'SK_ArmsOut' (skeleton 'SK_ArmsOut_Skeleton'). Wrong rig.
[DCC Anim] Marcel_DiceWinged_Demo: refusing 'A_DCC_OA_Alert' (skeleton 'SK_DCCCrawler_Skeleton') on mesh 'SK_DiceWinged' (skeleton 'SK_DiceWinged_Skeleton'). Wrong rig.
```

That is the complete set under both filters for that run — nothing else matched.

---

#### `[DCC Cast]` — two unrelated causes wearing one symptom

**Read the input and the output as separate things.** `StartingSkillIds` is the
authored *input*; `KnownSkills` is the *runtime output* that `GrantStartingSkills()`
produces from it. `[DCC Cast]` prints `KnownSkills`. Conflating the two cost a pass:
an MCP read showing `StartingSkillIds` populated does **not** contradict a log line
saying `(knows nothing)`.

**Ruled out by the log itself, for all four actors including both failures:**

- `table=DT_DCCSkills` on **every** line. The table is assigned everywhere. Any
  "missing table" / `!SkillTable` theory is dead — and no further PIE run or log
  re-read is needed, because the answer is already in the lines above.
- `bUseSkills=true` and a full mana pool on every line. Neither gates this.
- Missing or misnamed **rows** are out too: the two working actors resolve their rows
  off that same table, and a missing row logs rather than failing silently.

**Verified over MCP against the CDOs and the live instances:**

| Actor | Blueprint | Authored `StartingSkillIds` | Status |
|---|---|---|---|
| `Mob_Brute_Courtyard` | `BP_DCCEnemy_Brute` | **empty on CDO *and* placed instance** | **Solved** — nothing to grant |
| `TestBoss_InternManager` | `BP_DCCEnemy_ArmsOut` | `ParkingLotSpecial`, `QuickWit` @ rank 2 | Working, matches CDO exactly |
| `Scott_OfficePlaza_Demo` | not inspected | — | Working |
| `Crawler_Kyle` | `BP_DCCEnemy_ArmsOut` | not read on *that* instance | **Open** |

**Brute — solved, and it needs no theory.** `StartingSkillIds` is empty on the class
default *and* on the placed actor, so `GrantStartingSkills()` correctly grants nothing.
`POST_BUILD_TASKS.md` §3 records an intended loadout (`Headbutt`, `KickItOver`) that
was planned and never applied. Applying it is the entire fix.

**Kyle — open, but narrowed to one question.** He is a `BP_DCCEnemy_ArmsOut` actor and
that loadout demonstrably works, since `TestBoss_InternManager` runs the same two
skills off the same Blueprint. Table, `bUseSkills` and mana are all confirmed good on
his own log line. With rows, table and flags eliminated, what remains is the authored
input on **that specific placed actor**:

> Read `StartingSkillIds` off the instance that logs as `Crawler_Kyle` — not off the
> Blueprint CDO, and not off a different Kyle actor. Expect it to be empty. That is
> trap #2 again.

A later MCP read found *an* `ArmsOut` actor matching "Kyle" with `KnownSkills`
correctly populated, which is why this is recorded as open rather than solved: the
level contains more than one Kyle (see below), and the wrong one was almost certainly
inspected.

---

#### `[DCC Anim]` — the Blueprints are already correct; the instances are not

The three ally **Blueprints were repointed** to the `_Shared` meshes on 2026-08-16, and
each was confirmed by read-back. **Repointing them again is a no-op and must not be
queued as work.**

Yet the log shows all three actors still rendering pre-merge meshes — `SK_OfficeAirplane`,
`SK_ArmsOut`, `SK_DiceWinged` — each on its own pre-merge skeleton, so alert clips built
against `SK_DCCCrawler_Skeleton` are refused with "Wrong rig".

**The open question is therefore not the Blueprint, it is the placed instance.** A
`SkeletalMeshAsset` override serialised onto the actor beats the Blueprint value —
trap #2, one level deeper than the `bIsAlly` case. Read the mesh off each placed actor
and clear the override, rather than editing the Blueprint again.

Corroboration that the merge itself is sound: **CJ is absent from the failure list.**
`SK_HockeyWings` was already on the shared skeleton, so his alert plays.

**Unexplained: `A_DCC_QA_Alert` does not exist.** The log names it, but the only Alert
clips on disk are `A_DCC_OA_Alert` (shared) and `A_OA_Alert` (pre-merge original) — the
shared set uses only `AO`, `DW`, `HW` and `OA` prefixes. There is no `QA` anything in
the project.

The lead: `DCCCrawlerAnimSet.cpp:15` sets the C++ default to
`/Game/DCC/Characters/Shared/Anims/A_DCC_OA_Alert`, and Kyle and Marcel both log
exactly that — they are running the C++ default. **Only Scott logs `QA`**, so something
is overriding his alert slot with a reference to an asset that is missing or was
renamed. Per trap #3, an authored Blueprint or instance value beats the C++ default.
Check Scott's alert slot on the Blueprint *and* on the instance for a dangling
reference. Do not assume the log is wrong — it was transcribed verbatim from the
Output Log, and an earlier note in this file wrongly dismissed it as a transcription
error.

---

#### Actor naming: the level holds near-duplicate characters

Both diagnostics print `GetActorNameOrLabel()` — verified in source at
`DCCEnemyCharacter.cpp:150` and `DCCCharacterBase.cpp:2102`. There is no second name
field involved, so the two logs naming different actors means they **are** different
actors: `Scott_OfficePlaza_Demo` and `Scott_OfficeAirplane_Demo` are two characters,
as are `Crawler_Kyle` and `Kyle_ArmsOut_Demo`.

Worth auditing and culling on its own merits: duplicates double the AI cost, they make
every future diagnostic ambiguous, and they are what made Kyle hard to pin down above.
`Crawler_*` (placed 2026-08-16 in the personal rooms) and `*_Demo` are almost certainly
two generations of the same cast.

**Waiting on MCP:** per-asset texture caps, reparent `BP_ThirdPersonGameMode` to
`ADCCGameMode`, import Voldemortgage from `SourceArt/Voldemortgage/`, four pickup
collisions, re-import `DT_DCCEquipment.csv`, build rooms 5-7, and the two fixes the
PIE run above diagnosed:

- **Author `BP_DCCEnemy_Brute`'s `StartingSkillIds`** (`Headbutt`, `KickItOver` per
  `POST_BUILD_TASKS.md` §3). Verified empty on CDO and instance; this is the whole
  Brute fix, and the only fully-diagnosed item on this list.
- **Clear the per-instance mesh override** on `Scott_OfficeAirplane_Demo`,
  `Kyle_ArmsOut_Demo` and `Marcel_DiceWinged_Demo` so they render the `_Shared` meshes
  their Blueprints already point at. **Do not re-edit the Blueprints — they were
  repointed on 2026-08-16 and confirmed; that edit is a no-op.**
- **Trace Scott's dangling `A_DCC_QA_Alert` reference.** No such asset exists. Kyle and
  Marcel run the C++ default (`A_DCC_OA_Alert`), so Scott alone is overridden to
  something missing or renamed.
- **Read `StartingSkillIds` off the actor that logs as `Crawler_Kyle`** — that exact
  instance, not the CDO and not another Kyle. Everything else is eliminated.

---

## Queued for whoever has MCP (2026-08-23, class Build System - Class Benefit component)

Built this session, no MCP: `Classes/DCCClassComponent` - the Third Floor Class selection
system, applying a selected Class's Stat/Skill/Move-Speed bonuses and starter equipment kit at
runtime. `Docs/races-and-classes.md`'s "Notes / TODO" section is the live tracker; check there
first. Development-Editor-build-verified, editor closed, real link. Not yet seen in PIE.

1. **Import `Docs/DT_DCCClasses.json`** as `/Game/DCC/Classes/DT_DCCClasses` - this is JSON, not
   CSV (the `Benefits` field is an array of structs, same reason `DT_DCCQuests` is JSON), so it
   needs the create → add_rows → set_rows route, not a plain CSV import. `UDCCClassComponent`'s
   constructor already looks for the table at that exact path.
2. **Import `Docs/DT_DCCSkills_ChecksBatch1.csv` first if it hasn't been already** - the Class
   table's `SkillBonus` benefits (Swimming, Diving, Perception, etc.) reference those Skill ids
   and will silently no-op against rows that don't exist yet.
3. **PIE test: call `SelectClass` on a test character** (admin panel doesn't have a button for
   this yet - use a Blueprint/console call, or ask for one) **for a few different classes and
   confirm**: Stat bonuses show up in the Character tab, granted Skills show up as genuinely
   trained (not grey/untrained), Lord of Dance's Move Speed bonus is visible, and the starter
   kit auto-equips (will silently no-op until item 4 below is also done).
4. **Author `DT_DCCEquipment` rows for the starter kits** - the per-class list and `[needs mesh]`
   flags are in `Docs/races-and-classes.md`'s "Starter kits" section. None of this is blocking;
   `Equip()` already no-ops gracefully on an unknown id.
5. **PIE test the new roll-breakdown feature**: as any player, land an attack or a Persuade/
   Intimidate check, then click that line in the Feed (marked with a small `*`) and confirm the
   breakdown panel shows Starting Score / Equipment / Class sources correctly, sums to the right
   modifier, and clicking the same line again closes it. Only your own rolls should be clickable
   - confirm a teammate's or an enemy's roll does nothing when clicked.

## Queued for whoever has MCP (2026-08-23, class Build System - Skill Check primitive)

Built this session, no MCP: `Docs/races-and-classes.md`'s "Notes / TODO" section is now the
live tracker for this work - check there first for current status before reading further here.
Short version: `ResolveSkillCheck` (Rank + Advantage/Disadvantage) turned out to mostly already
exist; the real gap was Persuade/Intimidate not using it, now fixed. Development-Editor-build-
verified, editor closed, real link. Not yet seen in PIE.

1. **Import `Docs/DT_DCCSkills_ChecksBatch1.csv`** into the existing skill table (same one
   `DCCSkillComponent::SkillTable` already points at) - 11 new rows: Intimidate, Persuade,
   Perception, Navigation, Weather-Reading, Animal Handling, Lore: Creepypasta, Lore:
   Xenobiology, Mechanics, Swimming, Diving. Skip any that already exist under a different id.
2. **PIE test: Persuade and Intimidate an NPC with and without the new Skill granted**, and
   confirm the Rank is actually added to the roll (compare a Rank-0/untrained attempt, which
   should show Disadvantage, against a granted-and-Ranked attempt).
3. **Grant a test character a Rank in Intimidate or Persuade** (admin panel `DCCGrantSkill`,
   already wired) and confirm `RunSkillAdvancement` still correctly marks/advances it same as
   any cast Skill - this shares the exact same per-character tracking, just exercised from a
   new call site.

## Queued for whoever has MCP (2026-08-23, chat/feed system)

Built this session, no MCP: the Feed/Chatter/Party/Allies/Whispers chat system per Zoe's own
spec. `Docs/story-premise.md`'s new "System AI wants something" section explains the fictional
split between AI Favor (unchanged) and the new Chatter/Viewer Popularity concept - read that
before touching either. **Development-Editor-build-verified, editor closed throughout, real
link (no numbered hot-reload DLL).** Never seen in PIE.

New/changed, source only:
- `UI/DCCChatTypes.h` (new) - `EDCCChatChannel`, `FDCCChatMessage`.
- `UI/DCCChatterTypes.h/.cpp` (new) - `EDCCChatterBucket`, `FDCCChatterLineDef` DataTable row,
  token substitution.
- `UI/DCCChatInputWidget.h/.cpp` (new) - a C++-only Slate `SEditableTextBox` wrapped in a
  `UUserWidget`, no WBP asset. This is how free-text chat entry works inside the otherwise
  Canvas-drawn HUD - see the class comment for why a WBP wasn't an option this session.
- `Combat/DCCGameState.h/.cpp` - `ChatMessages` (replicated, same pattern as `EventLog`),
  `AddChatMessage`, `RollChatterBatch` (rolls a batch on-demand, not on a timer - see its own
  comment), `LogRoll` (Feed entries now optionally carry Instigator/RollTarget/crit flags).
- `Characters/DCCCharacterBase.h/.cpp` - `IsDead()`/`bHasDied` is now actually replicated (it
  wasn't before - a real, separate gap this closes: clients previously had no reliable signal
  that a downed crawler had died rather than stabilized). `Server_SendChatMessage`,
  `Server_RequestChatterBatch`, one-final-message-after-death rule, gameplay input locked out
  on death (attack/interact/throw/stow/skill-select all check `bHasDied` now). `[K]` toggles
  Feed visibility (Pinned/Minimized/Hidden).
- `UI/DCCHUD.h/.cpp` - five new/changed Tab menu pages. **The old `Chat` tab (the NPC small-talk
  picker) is renamed `Talk`** to free up the name - it's unchanged behaviourally, just relabeled,
  because the enum value `EDCCMenuTab::Chat` would have read as the new player-chat feature
  otherwise. Feed tab is a bigger scrollback of the same always-on log. Chatter/Allies/Whispers
  are new. Feed now colour-codes nat 20 (green) / nat 1 (red), and has a new "My Rolls" filter
  (`[L]` to cycle, now five presets not four).
- `Docs/DT_DCCChatterLines.csv` (new) - starter Chatter content pool, ~40 lines across 8 buckets.
  Not meant to be complete; add more rows freely, no code change needed to use them.

**Queued MCP work:**
1. **Import `Docs/DT_DCCChatterLines.csv`** as `/Game/DCC/UI/DT_DCCChatterLines` - plain CSV
   import, `FDCCChatterLineDef` row struct. `ADCCGameState`'s constructor looks for it at that
   exact path (self-healing on next restart if imported mid-session, same pattern as every other
   content table in this project).
2. **PIE test, full loop**: open the Tab menu, check Feed/Chatter/Party/Allies/Whispers all draw;
   send a Party message from one client and confirm another client sees it; set a Whispers target
   name and confirm only the two participants' clients show that thread; fist-bump someone and
   confirm they appear on the Allies tab; open Chatter and confirm a batch of lines appears (needs
   item 1 done first, or the table reads None and nothing rolls).
3. **Kill a crawler in PIE and confirm the one-final-message rule** - they should get exactly one
   more send on Party/Allies/Whispers, then the send row should grey out with "that final message
   has already gone out." Also confirm the Tab menu stays reachable after death if it was already
   open (a deliberate change to `DrawHUD`'s early-return - see its own comment).
4. **Visual pass on the chat input box** - `UDCCChatInputWidget` uses Slate's stock look with no
   styling applied; confirm it's legible against the Canvas HUD's dark panels and isn't a jarring
   white box.
5. **Sanity-check the crit colour coding** - only the basic attack roll is wired to `LogRoll`
   (`ADCCCharacterBase`'s `PerformAttack`-equivalent). Skill checks still log through the plain
   `Log()` and won't show green/red or count toward "My Rolls" yet - a natural follow-up once this
   is confirmed working, not done this session.

## Queued for whoever has MCP (2026-08-23, after the full reconciliation pass)

**Read CLAUDE.md's "✅ Reconciliation pass complete" section first** — it lists every bug fixed
and every open question this list below assumes. This supersedes the 2026-08-22 list that used to
be here; that session's work (loot menu, chests, ally auto-gear, `[DCC Talk]`) is still current
and still items 1-4 below, just folded into one list with everything found since.

**Everything in `Source/` is now committed and Development-Editor-build-verified, editor closed
throughout.** `Content/` (the binary assets, including Zoe's own recent manual map edits) is
deliberately NOT committed — review that yourself before committing it; it was not this session's
call to make. Priority order below is "cheapest/most-isolated first," not "most important first" —
item 3 (the ally bug) is probably the one Zoe actually cares most about.

1. **Re-import `Docs/DT_DCCEquipment.csv`** into `/Game/DCC/Equipment/DT_DCCEquipment` — same
   schema, eight new rows total now (`ConfettiCrossbow`, `NailGunPro`, `FlareGunEmergency`,
   `SeveranceCheckUncashed`, `HighVisibilitySafetyVest`, `ComplaintFolderOverstuffed`). Plain
   re-import, no restart needed first. **Also re-import `Docs/DT_DCCAchievements.csv`** — two new
   rows (`EstateSale`, `AssetRecovery`) using a new stat, `ContainersLooted`, that the loot menu's
   Loot All/Stow now actually records.
2. **Place the six curated chests** somewhere reachable — `ADCCLootContainer_KitchenDrawer`,
   `_Armory`, `_Quartermaster`, `_ManagementLockbox`, `_LostAndFound`, `_SafetyLocker`
   (`Items/DCCLootContainerVariants.h`). Pantry or Bar are the least crowded rooms.
3. **PIE, filter the log on `[DCC Talk]`, try to talk to an ally.** Names exactly which of four
   gates refused the attempt. If `IsHostileTo=1`, read `bIsAlly` off that specific placed
   instance next — trap #2 (Blueprint CDO true, placed instance false) is the leading theory,
   especially since Zoe's own recent manual map edits could have reintroduced it.
4. **PIE-test the new loot menu** — E on a corpse or a placed chest should show Check Inventory /
   Loot All / Stow / Leave, not the old "Search the body." Compiled and deployed, never seen.
5. **PIE-test the achievement reward-claim flow** — the HUD already draws a "Pending Rewards (N)"
   affordance and calls `RequestClaimReward`; unlock something and confirm clicking it actually
   pays XP/Favor and clears the pending flag. Never exercised. Loot All/Stow on five containers
   should also unlock `EstateSale` (new this pass, likewise never exercised).
6. **PIE-test universal climbing again** — `ADCCClimbPoint::FindOrCreateForSurfaceHit` (the
   function that makes any wall climbable) did not exist in git until this pass; it's never been
   run. Known-good loop per CLAUDE.md: look at any wall, click, climbs or shows a "too far"/
   "nothing to climb" notice.
7. **Multiplayer-test the intro director with more than one client**, if/when it's ever placed in
   a level (grep found it placed nowhere yet) — the just-fixed
   `TeleportAllCrawlersToDungeon` needs a real multi-connection session to confirm, not just PIE
   with one player.
8. **A decision, not a task: is the procedural dungeon generator (`Rooms/DCCDungeonGenerator`)
   wanted?** It's real, working, and completely unwired — nothing spawns an instance, and it
   contradicts `floor-01-design.md`'s explicit hand-authored premise. Committed so the work isn't
   lost. If the answer is "no," it can just stay dormant; if "yes," it needs a placed instance, a
   NavMeshBoundsVolume-per-cell fix, and the frontier-trigger race-condition fix before it's safe
   to turn on with more than one player. **Confirmed dormant as of 2026-08-23 — Zoe's call, leave
   it alone until she says otherwise.**
8a. **Place a paired `ADCCOathgate`** (new this session, `Rooms/DCCOathgate.h`) — two instances,
   each with `PairedGate` set to the other. Leave the default `EnemyPool` (Debt Eater/Fine
   Print/Arbiter) on the "entrance" side; clear `EnemyPool` to empty on the "exit" side so it only
   teleports back. Anywhere reachable works — this is explicitly a DM testing tool first, unlimited
   use, no cooldown beyond a 3s re-trigger debounce. Never placed or PIE-tested.
8b. **Place `ADCCQuestMarker_BarEntrance`** (new this session) in the Bar, roughly x9200/y5000 per
   `floor-01-design.md` — same pattern as Orientation/EastLedge.
8c. **Re-import `Docs/DT_DCCQuests.json`** — six new quests (`TheOathgate`, `BountyCollectors` →
   `RepeatCustomer`, `NetworkingOpportunity`, `FreeWeights`, `SecondHelping`), all optional side
   content alongside the existing seven-quest main chain, all `bAutoStart: false`. None PIE-tested.
9. **Place a `DCCCrowdSpawner` in the Bar** — the ambient-crowd system (Tier 2 of
   `population-design.md`) has been code-complete since before this session; its only blocker
   (Bar geometry) has existed since 2026-08-19.
10. **Place one or more Nuclear Family members** (`ADCCEnemy_NuclearDad/Mom/Kid`,
    `Docs/enemy-roster.md`'s "Rocky Flats" hazard trio) somewhere they can actually be fought —
    built, spawnable via the admin debug panel, never placed in a room/encounter.
11. **Ask Zoe whether to download either free weapon pack** named in CLAUDE.md's reconciliation
    section (Free Fantasy Weapon Sample Pack on Fab, ASIS Free Gun Weapons Pack on itch.io) —
    `ConfettiCrossbow` ships with no mesh at all and the other two ranged rows reuse an unrelated
    placeholder mesh until one lands.
12. **PIE-test three more never-run features from this same session** (all pure C++, nothing to
    place):
    - **Death saves.** Get downed and watch the event feed - a roll every 3s, with pips on the
      downed screen. Confirm 3 successes stabilises at 1 HP with no rescuer, and that a rescuer's
      "Help Them Up" still works mid-sequence and cuts the sequence short.
    - **CJ's first aid.** Down a party member near CJ with nobody else fighting - he should walk
      over and start the same revive channel a player does. Never seen running.
    - **Selfie mode.** Press `C`, hold the pose, confirm a screenshot actually lands in
      `Saved/Screenshots/` (the `FScreenshotRequest` call is the one piece of this genuinely
      unverified - it should work, matches the console `shot` command's own mechanism, but has
      not been watched happen).

---

## Queued for whoever has MCP (2026-08-18, evening)

The main session has **no MCP**; dispatch demonstrably does. Treat that as the
division of labour: main session takes C++, dispatch takes anything inside the
editor. These are all editor-side and all reported by Zoe from live play.

**Edit the versions in HER level, not the base classes.** Her words, and it is the
right instruction — every one of these has a per-instance answer.

1. **Physics warnings, third recurrence.** `DCCPickup_GreaseBarrel`, `_Keg`,
   `_Crate`, `_StrongboxCrate` still log *"but it has ComplexAsSimple collision"* on
   every PIE, and `_Keg` now repeats mid-session. Two previous sessions recorded this
   as fixed via `StaticMeshTools.generate_convex_collisions`; it was not.
   **Read the collision setting back after writing it, and save the asset** — an
   unsaved change looks identical to a successful one until the next launch.

2. **Random loot on the enemies actually placed in the level.** `ADCCEnemyCharacter`'s
   constructor already gives every enemy a 55% / 3-12 gold `Coins` entry, so the base
   default is fine — the question is whether the placed instances and the Blueprints
   in `/Game/DCC/Characters/` have `LootTable` entries of their own, and whether
   anything overrode it to empty. Vary it per enemy type rather than leaving them all
   on the same coin drop.
   Note the loot rules changed: drops are now rolled at death and held **on the
   corpse** until searched (`E` → "Search the body"), not scattered on the floor.

3. **Two Kyles, two Scotts.** Confirmed above as genuinely distinct actors. The
   `Crawler_*` set was placed 2026-08-16 in the personal rooms; the `*_Demo` set
   predates it. Audit and cull the duplicates — they double AI cost and confuse every
   diagnostic.

4. **The three wrong-rig actors are instance-level.** The Blueprints were already
   repointed and verified. Clear the per-instance `SkeletalMeshAsset` override on the
   placed actors, not the Blueprint.

## Working style

Build first, explain at checkpoints. Zoe is learning game dev — explain the *why*
behind UE idioms when you explain at all, but do not narrate every step.

Prefer asking her to do a quick manual step over spending an hour on an automatic
workaround. Her time at the keyboard is the scarce resource, not yours.

**Be honest about what is unverified.** This project has been damaged more by
confident wrong claims than by slow progress. If you did not see it work, say so.
