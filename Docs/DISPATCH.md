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

## Queued for whoever has MCP (2026-08-23, damage types + resistances)

Built this session, no MCP. `Docs/races-and-classes.md`'s "Notes / TODO" has the full writeup.
Short version: `ApplyDamage` (the one funnel every damage source already shares) now takes an
`EDCCDamageType`, defaulting to `Physical` so none of its 9 existing call sites needed to
change; Resistant/Vulnerable/Immune are merged from Equipment + the selected Class, counted so
two sources disagreeing on the same type cancel to Normal rather than one silently winning.
Development-Editor-build-verified, editor closed, real link. Not seen in PIE.

1. **Re-import `Docs/DT_DCCClasses.json`** (unchanged path, `/Game/DCC/Classes/DT_DCCClasses|`) -
   three of the twelve classes now carry a `DamageResistance` benefit (Underwater Welder: Cold
   Resistant; Crime Scene Cleaner: Poison + Necrotic Resistant; Noodle Stand Cook: Fire Resistant
   *and* Fire Vulnerable, deliberately - they cancel to Normal, which is the joke).
2. **Author `WeaponDamageType`/`ResistanceDamageType`+`ResistanceLevel` on a couple of
   `DT_DCCEquipment` rows** so there's something to actually test against - every row reads the
   defaults (Physical / Normal) today, so nothing is typed or resistant yet in practice.
3. **PIE test: hit a Crime-Scene-Cleaner-classed character with a Poison-typed attack** (once
   item 2 gives you one) **and confirm the damage is halved**, then try the same with no Class
   selected and confirm it isn't. Also worth deliberately testing the cancellation case (equip
   something Fire-Vulnerable on the Noodle Stand Cook, who's already Fire-Resistant from their
   Class, and confirm damage comes out unmodified, not double-reduced or doubled).

## Queued for whoever has MCP (2026-08-23, six of Zoe's chore-break notes)

No MCP this session (editor was not running). Development-Editor-build-verified with the
`DungeonCrawlerDenver` DebugGame compile-check, which also verified cleanly against every
concurrent change from the two other sessions running at the same time (class system, skill
breakdown UI) - nothing here conflicts with either. **Not yet deployed** - needs the usual
close-editor Development Editor build before any of this is live.

Zoe listed six things she'd noticed and asked them sorted by impact/effort, worked top-down, with
at most one more session to finish what needed MCP. Three were fixable in code alone and are done;
three are level/asset work that categorically needs the editor and are diagnosed + prepped below.
**This next session should be with Zoe present, MCP live, for exactly these three.**

### Done this session (code only, compiled, not yet deployed)

1. **Bartender (and every `ADCCDialogueNPC`) now responds to E**, not just proximity. It was
   ambient-only before - `ADCCDialogueNPC` is deliberately not an `ADCCCharacterBase` (see its own
   class comment), so `FindInteractionTargetUnderCursor` could never see it, and E on a bartender
   fell straight through to `AttemptPickupOrDrop`. Added `ADCCCharacterBase::
   FindDialogueNPCUnderCursor()` (same camera sweep as the ally/loot checks) and
   `ADCCDialogueNPC::TryPlayerInteract()` (says the next line, or skips an already-showing one
   ahead) into `HandleInteractPressed`'s chain, between the ally-conversation check and the loot
   check. The existing walk-up-and-it-talks behaviour is untouched - E is additive, so the "wide
   array of non-aggressive NPCs" ask can still use either convention per NPC.
2. **A charge-spawned minion embedding in terrain, real bug, found and fixed:**
   `UDCCBossComponent::SpawnChargeMinion` placed every minion at `GroundedZ` - the BOSS's own floor
   height, captured at the boss's own XY when the charge began - rather than tracing the ground at
   the minion's own `Offset` position. On any arena floor that isn't dead flat (and the code's own
   comment already says "the arena floor is not flat"), a minion spawned meters away over a rise
   lands embedded in it; over a dip, floating. That reads exactly like "spawns inside terrain,
   melee can't reach it, ranged still can" - melee proximity fails against an embedded capsule,
   ranged/skill targeting (distance + line of sight) still connects. Fixed with a straight-down
   trace at the actual spawn XY, same ground-snap pattern `ADCCCrowdSpawner` already uses. **If
   the NPC Zoe saw was placed by hand rather than charge-spawned, this fix doesn't touch it** -
   worth checking both in the next session.
3. **Debt Eater no longer ragdolls.** Its imported Physics Asset (`SK_DebtEater_PhysicsAsset`)
   stretches the body into something grotesque under `SetAllBodiesSimulatePhysics` - a bad
   constraint setup, not a code bug, and this project's enabled MCP toolset has no Physics Asset
   Editor tool to retune it with anyway. Took Zoe's own suggested fallback: added
   `ADCCCharacterBase::bUseRagdollOnDeath` (default true, untouched for everyone else) and
   `DeathDissolveEffect` (a soft Niagara reference). Debt Eater sets the bool false and points the
   effect at `/Game/Alien_Portal/FX/Smoke/NS_Smoke_01` (an existing pack asset, not new content).
   With ragdoll off, the mesh freezes in its last pose - no physics, so nothing to deform - while
   still blocking the Visibility trace the loot menu's `FindLootTargetUnderCursor` needs, so it
   stays fully lootable exactly like a ragdolled corpse. **Not seen in PIE - confirm the smoke
   reads as a death beat and doesn't look like a bug, and confirm looting still works on it.**

### Diagnosed and prepped, needs MCP - the buff/don minion texture bug

**Root cause confirmed, not guessed:** inspected both source `.glb`s directly with Blender
(`Tools/inspect_glb_materials.py`, new this session) rather than assuming. Each one is clean
independently - one mesh, one material named `Material_1`, one image named `texture_0` - Meshy
always uses those exact generic names (see CLAUDE.md's note on `texture_0` collisions elsewhere in
this project). **Both Buff Minion and Don Minion were imported into the same folder**
(`/Game/DCC/Characters/Minions/`), which only contains ONE `Material_1.uasset` / `texture_0.uasset`
pair for both meshes. Whichever import went second either reused or overwrote the first's material
asset instead of getting its own, which is exactly what a "patchwork/camo" mixed-skin look on one
character looks like - two different UV layouts sharing one baked texture. The lacrosse stick
being the wrong colour is very likely the same failure mode (a shared-name material collision on
import) hitting a different asset; nobody documented that prop's import, so it needs a look too.

**Prepped so the fix is a straight re-import, no more Blender needed:** re-converted each
character's source `.glb` independently into its own folder - `SourceArt/BuffMinion/SK_BuffMinion.fbx`
and `SourceArt/DonMinion/SK_DonMinion.fbx` - so a fresh import of each can't collide again. Also
split both characters' merged-animation `.glb`s (neither had been split before - `Minions/` has no
`Anims/` subfolder at all yet):
- `SourceArt/BuffMinion/Anims/`: `A_BuffMinion_Running.fbx`, `A_BuffMinion_Walking.fbx`.
- `SourceArt/DonMinion/Anims/`: `A_DonMinion_Running.fbx`, `A_DonMinion_Walking.fbx`, plus three
  flavour clips - `Angry_Ground_Stomp`, `Angry_To_Tantrum_Sit`, `Cough_While_Sleeping` - that read
  like a "Don" mob-boss character and are worth keeping as idle/taunt variety rather than discarding.

**Next-session recipe:** delete the existing shared `Minions/Material_1`, `texture_0`,
`texture_0_ncl1_1` (check nothing else references them first), re-import `SK_BuffMinion.fbx` and
`SK_DonMinion.fbx` each into their own subfolder, import the animation FBXes bound to each
mesh's own Skeleton, and check the lacrosse stick prop's material the same way if it's already
placed somewhere findable.

**A design question for Zoe, not decided here:** what these two minions are *for*. Nothing in
`Docs/DT_DCCEnemies.json` or `enemy-roster.md` has a Buff Minion or Don Minion entry, so there's no
stat block to wire them to a combat class the way `ADCCEnemyCharacter_DebtEater` was. They read as
plausible ambient/crowd NPCs (the "wide array of non-aggressive NPCs" ask) as easily as new combat
mobs - didn't want to invent gameplay numbers unprompted. Worth deciding before writing whichever
C++/Blueprint wiring fits.

### Still needed - replacing ally-body placeholders more broadly

Confirmed the specific complaint: `BP_DCCEnemy_HockeyWings`/`_ArmsOut`/`_DiceWinged`/`_OfficeAirplane`
(generic mobs) and even `ADCCEnemyCharacter_TestBoss` all render CJ/Scott/Kyle/Marcel's own
`SK_HockeyWings`/`SK_ArmsOut`/`SK_DiceWinged`/`SK_OfficeAirplane` meshes - literally the players'
own bodies worn by enemies and NPCs, which is exactly what read as "many NPCs still using my
allies' bodies." **The Meshy pipeline is not down - it's mid-flight with a large backlog already
generated**, sitting unconverted in `Content/Characters/`: `Dune Popcorn.glb`, `League Minion.glb`,
`League Minion Blue.glb`, `Minion eyes.glb`, `Scott Lacrosse.glb`, `debt collector.zip`,
`voldemortgage 2.glb`, `waterbottle.glb`, plus the already-imported-but-broken Buff/Don Minion above
and the not-yet-imported-at-all `Meshy_AI_Gaunt_undead_lich_ro_biped/`. That's roughly a session's
worth of Blender-convert + MCP-import + wiring on its own - didn't attempt all of it blind without
Zoe's steer on which NPCs each is meant to become.

### Still needed - the wider map

Not attempted - level layout/placement is categorically MCP-only, and this is the single biggest
item on Zoe's list. Recorded here so the next session has a concrete starting point rather than a
blank page:

- **Scale target, from Zoe's own words:** her cyberpunk playground scene read as "1 unit"; the
  real dungeon should read as 30-40x that footprint, wide rather than tall - "enough space for 5
  million people," even if players only ever walk through 1% of it early on.
- **Distant-skyline packs already installed and unused for this purpose:** `Synthwave_city` and
  `Megastructure_Scifi_World` each ship their own `Sky_system` (a sky dome/backdrop, not just
  props) plus a pre-built demo `Level`; `Sci_Fi_city` and `Dam_city` ship pre-built `Maps` too.
  Pulling one pack's distant-building dressing into the DCC level wholesale (visual-only collision,
  players never reach it) is far cheaper than hand-placing buildings one at a time, and reads
  immediately as "big city" rather than "box surrounded by sky."
- **The mirror trick Zoe suggested herself** ("mirrors for neighbouring territories"): duplicate the
  same distant-building cluster at intervals around a ring rather than authoring unique skyline per
  direction - cheap repetition sells scale the same way a game-industry matte painting does.
- Needs a decision on where this backdrop sits relative to the existing seven Floor 1 rooms
  (floor-01-design.md's coordinates) before placing anything, so the skyline reads as "the rest of
  the dungeon, out there" rather than floating disconnected from the playable rooms.

## Queued for whoever has MCP (2026-08-23, Skills wired into physical actions + a real bug fix)

Built this session, no MCP. `Docs/races-and-classes.md`'s "Notes / TODO" has the full writeup;
short version below. Development-Editor-build-verified, editor closed, real link. Not seen in PIE.

1. **Check whether `FirstImpressions` and `QuickWit` already exist as Skill rows before
   importing `Docs/DT_DCCSkills_ChecksBatch1.csv`** - a real bug this session found and fixed:
   `ResolveSocialCheck` (Persuade/Intimidate) had been marking two different Skill ids for the
   same action, this session's own newly-invented "Intimidate"/"Persuade" plus a pre-existing
   "FirstImpressions"/"QuickWit" pair the function already used before this session touched it.
   Fixed by standardising on the pre-existing ids everywhere. **If they already have real,
   tuned data, a blind CSV re-import will overwrite it** with this session's placeholder values
   - open the table and look first.
2. **Import `Docs/DT_DCCSkills_ChecksBatch2.csv`** - two new rows, Athletics and Heavy Armor
   Training.
3. **PIE test: sprint until stamina depletes, with and without Athletics granted**, and confirm
   the drain rate and max stamina both visibly change. Then **equip enough mass to cross 30kg
   and confirm stamina regen slows** while standing still - `MassKilograms` needs authoring on
   at least a couple of `DT_DCCEquipment` rows first (all read 0 today, so nothing is encumbering
   yet by default).
4. **Confirm Climbing didn't regress** - it was already fully wired to a real Skill Rank before
   this session touched anything nearby; a quick climb test is just insurance.

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
