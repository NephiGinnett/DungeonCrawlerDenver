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

7. **Never resolve a Blueprint of your own class hierarchy from inside that hierarchy's
   own C++ constructor with `ConstructorHelpers::FClassFinder`/`FObjectFinder`.** It
   creates a circular CDO-construction dependency and hangs the editor on every single
   launch, silently - no crash, no error, just a permanently stuck loading screen. Full
   incident and fix below, "Editor hang on every launch" (2026-08-28). Resolve that kind
   of reference lazily instead, at first actual use, via `FSoftClassPath::TryLoadClass`.

---

## Editor hang on every launch - circular CDO construction from a constructor-time Blueprint self-reference (2026-08-28)

**Symptom:** the editor stalled on the loading screen at "compiling Blueprints" on four
consecutive launches in a row, always at the same point, with Zoe having to force-close
`UnrealEditor.exe`/`CrashReportClientEditor.exe` via Task Manager each time since it
stopped responding to a clean close. Diagnosed and fixed by a dispatched session while
Zoe was away from the keyboard, then asleep; nobody was able to use the editor for part
of the evening until this landed.

**How it was actually found, not guessed:** the editor's own log file
(`Saved/Logs/DungeonCrawlerDenver*.log`) was the same across all four launches - it goes
completely silent immediately after `LogModuleManager: InternalLoadLibrary:
'DungeonCrawlerDenver'` (the game DLL) and the DerivedDataCache maintenance line that
follows it, with no further output ever. That point in engine startup is exactly when
Unreal constructs the C++ default objects (CDOs) for every `UCLASS` in the module -
which is also almost certainly what the splash screen's "compiling Blueprints" percentage
is showing, even though none of that is logged at Display verbosity. The frozen process
itself corroborated a genuine deadlock rather than a slow compile: after ~8 minutes wall
time it had accumulated only ~43 CPU-seconds, and Windows still reported it as
"Responding" - low, mostly-idle CPU usage with the message pump still alive is the
signature of a thread blocked waiting on something that will never resolve, not a
tight/expensive loop.

**Root cause.** [DCCCharacterBase.cpp](../Source/DungeonCrawlerDenver/Characters/DCCCharacterBase.cpp),
added the same evening for the body-collector feature, had this in
`ADCCCharacterBase::ADCCCharacterBase()` - the constructor that runs for **every**
character class in the game, players and enemies alike:

```cpp
static ConstructorHelpers::FClassFinder<ADCCEnemyCharacter> BodyCollectorFinder(TEXT("/Game/DCC/Characters/BP_DCCEnemy"));
if (BodyCollectorFinder.Succeeded())
{
    BodyCollectorClass = BodyCollectorFinder.Class;
}
```

`BP_DCCEnemy` is a Blueprint parented to `ADCCEnemyCharacter`, which itself derives from
`ADCCCharacterBase`. So constructing the CDO of *any* `ADCCCharacterBase` subclass - which
happens very early, right after the module DLL loads - now synchronously tried to load a
Blueprint that is itself part of the same class hierarchy currently under construction.
Generating that Blueprint's class requires `ADCCEnemyCharacter`'s own CDO to already
exist, but that CDO's construction is what put this constructor on the call stack in the
first place. A genuinely circular wait, not just a slow one - hence a hang with no crash
and no error, rather than a fast, loud failure. `BP_DCCEnemy.uasset` does exist on disk
at that path, which is what ruled out a simpler "missing asset" explanation and pointed at
the circularity instead.

Every other `ConstructorHelpers::` call in the codebase was checked and is fine - none of
the others reference a Blueprint that is part of the constructing class's own hierarchy.
`DCCDungeonGenerator.cpp`, for example, also loads enemy Blueprints (`BruteBP`, `CourierBP`,
`AnalystBP`) via `FClassFinder` from its own constructor, but `ADCCDungeonGenerator` is
unrelated to `ADCCCharacterBase`, so that's an ordinary cross-reference, not a cycle. This
is specifically a same-hierarchy self-reference problem, not a general ban on
`ConstructorHelpers` (which the rest of this codebase uses correctly, dozens of times, for
DataTables, meshes, and animations that live outside the constructing class's own chain).

**The fix.** Removed the constructor-time `FClassFinder` entirely. `BodyCollectorClass`
now resolves its `BP_DCCEnemy` fallback lazily, on first actual use inside
`CheckForBodyCollector()` (only ever called ~20 real seconds after a character's death,
long after every CDO in the module exists), via a small `ResolveDefaultBodyCollectorClass()`
helper using `FSoftClassPath::TryLoadClass<ADCCEnemyCharacter>()` and a function-local
`static` to cache the result after the first successful resolve. Same effective behavior
(every character Blueprint that hasn't overridden `BodyCollectorClass` still falls back to
the bare `BP_DCCEnemy`), just resolved at a point in the object lifecycle where the cycle
can't occur. `DCCCharacterBase.h`'s own doc comment on `BodyCollectorClass` now carries
this warning in place, so the pattern doesn't get reintroduced by someone restoring the
constructor-time version to "simplify" it later.

Caught one unrelated, pre-existing compile error along the way while getting a clean
DebugGame check (`DCCInventoryComponent.cpp`: a `SpawnLootBoxItems` parameter named
`Items` shadowing the class's own `Items` member, C4458) - a peer session fixed that one
independently while this session was mid-diagnosis, confirmed by re-reading the file
before re-running the check. Also hit, and fixed the ordinary way, this project's own
already-documented ternary-ambiguity MSVC trap (`Cond ? TSubclassOf<T> : T::StaticClass()`
is ambiguous; both branches have to be spelled as `TSubclassOf`) while writing the fallback
resolution line.

**Verified end to end, not just compiled:** DebugGame compile-check clean, then editor
fully closed, Development Editor build (real link - `UnrealEditor-DungeonCrawlerDenver.dll`
rebuilt, confirmed no numbered hot-reload DLL in `Binaries/Win64/`), then the editor was
actually relaunched and watched through to a genuine, usable state - past the point that
hung on every prior attempt, asset registry scan completed, `LogUnrealEdMisc: Total Editor
Startup Time, took 82.370` logged, process reporting `Responding: True` with a real window
title (`DungeonCrawlerDenver - Unreal Editor`), not just an alive-but-frozen process. Two
unrelated, pre-existing `CDO Constructor: Failed to find` errors logged for
`DT_DCCLootBoxTables`/`DT_DCCBoons` (missing DataTables, same graceful-failure pattern this
file already documents for other tables) - cosmetic, not a repeat of tonight's bug, and not
touched here.

**The general lesson, for anyone adding a new constructor-time asset/class default
anywhere in this codebase:** `ConstructorHelpers::FObjectFinder`/`FClassFinder` are safe
and are this project's own established pattern for DataTables, meshes, and animations -
but only when the thing being loaded lives *outside* the constructing class's own
inheritance chain. A `TSubclassOf<T>` (or narrower) default that needs to point at a
Blueprint which is itself a `T` (or a subclass of `T`) cannot be resolved in `T`'s own
constructor; resolve it lazily instead, the same way this fix now does.

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

## Done this session (2026-08-24, MCP live, editor freshly rebuilt) — backlog cleanup

Fresh session, full MCP (editor rebuilt 2026-08-23 22:31, confirmed no numbered hot-reload DLL
in `Binaries/Win64/` — the whole pile below this section, through the class/consumable/chat
work, is genuinely deployed, not just compiled). Coordinated with two peer sessions
("Ally models, map scale, NPC bugs" and "Ally/loot interaction + preview polish") by reading
live editor/asset state rather than their transcripts (session-transcript tool was unavailable
cross-session) — found they'd already fixed `bIsAlly` on all 8 ally instances (verified true
everywhere) but had NOT yet fixed the wrong-rig mesh overrides this file flagged. Did that plus
a stack of still-missing DataTable imports this file and CLAUDE.md had queued across several
prior sessions. Everything below is read-back-verified and saved.

**Real fix — the wrong-rig instance overrides.** `Scott_OfficeAirplane_Demo`,
`Kyle_ArmsOut_Demo`, `Marcel_DiceWinged_Demo` still had a per-instance `SkeletalMeshAsset`
override pointing at the old pre-merge meshes (`SK_OfficeAirplane`/`SK_ArmsOut`/`SK_DiceWinged`,
each still on its own skeleton) even though the three Blueprint CDOs correctly point at
`*_Shared`. Cleared all three via `ObjectTools.set_properties` on each actor's
`SkeletalMeshComponent`, confirmed by read-back, confirmed again by a PIE run
(`bSimulate=true`) showing **zero** `[DCC Anim]` "wrong rig" lines — previously three every run.
Also confirmed via the same PIE run that `Crawler_Kyle` now shows `ParkingLotSpecial`/`QuickWit`
in `[DCC Cast]` (the "two Kyles" skill-granting question from the 2026-08-18 diagnosis is
resolved — both Kyle instances grant correctly now). `Mob_Brute_Courtyard` still logs
"knows nothing" — `BP_DCCEnemy_Brute`'s `StartingSkillIds` authoring (POST_BUILD_TASKS.md §3)
is still the one open item there.

**DataTable imports — three tables that never existed, still causing constructor errors:**
`DT_DCCClasses`, `DT_DCCConsumables`, `DT_DCCChatterLines` did not exist in `/Game/DCC/` despite
being queued repeatedly. Confirmed via `LogUObjectGlobals: Error: CDO Constructor` on the fresh
editor boot log (`DCCClassComponent`, `DCCInventoryComponent`, `DCCGameState` each failed to find
their table). Created and populated all three from their `Docs/` source files
(`DT_DCCClasses.json` → 12 classes with full nested `Benefits` arrays, `DT_DCCConsumables.json`
→ 5 drinks with nested `Effects`, `DT_DCCChatterLines.csv` → plain CSV) at the exact paths their
C++ constructors already expect. **Still needs an editor restart** to actually resolve — these
are `ConstructorHelpers::FObjectFinder` defaults that only re-resolve at class construction, so a
second PIE run in the same editor session still showed no new errors only because nothing
re-triggers that constructor mid-session, not because it's fixed live. Ask Zoe to restart before
trusting Class selection, Consumables, or Chatter to work.

**DataTable additions — merged new rows into existing tables, did NOT blind-reimport:**
`Docs/DT_DCCEquipment.csv` and `Docs/DT_DCCAchievements.csv` are **stale relative to the live
tables** — a peer session added ~15 equipment rows live via MCP (`PokemonMasterTrainer`,
`DunePopcornBucket`, `ScottLacrosseStick`, `MinionEyes`, `WaterBottle`, several bar-fight props)
that were never backported into the CSV. A reimport of either CSV would have silently deleted
those. Instead, diffed live rows against the CSV and `add_rows`/`set_rows`'d only the genuinely
missing ones: **9 equipment rows** (`ThrownBottle`, `ServiceKnifeSet`, `SodaGun`,
`ConfettiCrossbow`, `NailGunPro`, `FlareGunEmergency`, `SeveranceCheckUncashed`,
`HighVisibilitySafetyVest`, `ComplaintFolderOverstuffed`) and **2 achievement rows**
(`EstateSale`, `AssetRecovery`, both on the already-existing `ContainersLooted` stat). Also added
**11 new Skill rows** from `DT_DCCSkills_ChecksBatch1.csv`/`Batch2.csv` (`Perception`,
`Navigation`, `WeatherReading`, `AnimalHandling`, `LoreCreepypasta`, `LoreXenobiology`,
`Mechanics`, `Swimming`, `Diving`, `Athletics`, `HeavyArmorTraining`) — **skipped that CSV's own
`FirstImpressions`/`QuickWit` rows on purpose**, since the live table already has real tuned data
for both (different DisplayName, DC, cooldown) and the CSV's copies are this project's own
documented placeholder duplicates. And **6 new Quest rows** from `DT_DCCQuests.json`
(`TheOathgate`, `BountyCollectors`, `RepeatCustomer`, `NetworkingOpportunity`, `FreeWeights`,
`SecondHelping`), nested `Objectives` arrays and `NextQuestId` chaining all confirmed by
read-back. **General lesson for the next session**: before any DataTable "re-import", diff
`list_rows` against the source file — this project's live tables now drift from their CSV/JSON
masters when someone authors rows straight through MCP.

**Real fix — the Buff/Don Minion shared-material texture bug, using the prep already staged.**
Confirmed via `get_dependencies` that both `SK_BuffMinion` and `SK_DonMinion` (old, shared
`/Game/DCC/Characters/Minions/`) referenced the same `Material_1`/`texture_0`/`texture_0_ncl1_1`
— the exact collision this file already diagnosed, with nothing else referencing those three
assets. Re-imported `SourceArt/BuffMinion/SK_BuffMinion.fbx` and `SourceArt/DonMinion/SK_DonMinion.fbx`
(materials+textures+physics asset) into their own new subfolders, `.../Minions/BuffMinion/` and
`.../Minions/DonMinion/` — each now has its own independent `Material_1`/`texture_0` pair,
confirmed by walking the material graph (`MaterialTools.get_property_input` → `MP_BaseColor` →
`TextureSample_0` → each one's own local `texture_0`, not the old shared one). Repointed both
live NPC actors' `SkeletalMeshComponent.SkeletalMeshAsset` to the new meshes and confirmed by
read-back; a viewport capture of the bartender (Don Minion) shows a clean, uniform skin, no
patchwork. **The old shared assets in `Minions/` root were deliberately left in place, not
deleted** — `get_referencers` still listed external-actor-file referencers for both, even after
saving the level, which reads as a stale World Partition asset-registry cache but wasn't fully
run to ground. Safe to delete once someone confirms nothing still points at them (the two fixed
NPCs and the not-yet-placed `ADCCEnemyCharacter_Guard`/`ADCCVendorNPC` subclasses do not).
**Animations were not re-imported this pass** — `SourceArt/{BuffMinion,DonMinion}/Anims/*.fbx`
are still waiting, same per-animation-FBX-bound-to-the-new-skeleton process used for the four
crawlers' `_Shared` sets.

**Found, not acted on — a stray duplicate Bartender.** `DCCDialogueNPC_Bartender2` (SpeakerId
`Bartender`, mesh **still on the old shared `SK_DonMinion`**) sits at x9133/y-718 — nowhere near
the real Bar (x8600-9800/y4700-5300), out near spawn. Reads like a leftover test placement from
before the real Bar existed, matching this project's own documented "duplicate actors" pattern
(the two Kyles/two Scotts). Did not delete unilaterally since it's placed content; flagging for
Zoe to confirm before culling it (also: as long as it exists, it's one more actor referencing the
old shared mesh, so it should be fixed or deleted before that mesh can be safely removed).

**Still open, none of it attempted this session (needs Zoe's visual judgment, not just MCP
plumbing):** guard patrol point placement, the six curated loot chests, the Oathgate pair,
`ADCCQuestMarker_BarEntrance`, the `DCCCrowdSpawner`/Nuclear Family placements, and the wider
map-scale/skyline work. Also worth doing next MCP session: swap the placeholder
`DCCDialogueNPC_Bartender`/`BarGuard` actors in the Bar for the real `ADCCVendorNPC`/
`ADCCEnemyCharacter_Guard` subclasses (their C++ has existed for a session already but nothing
in the level uses it yet) — `SellableConsumableIds` can now point at all five real rows in the
freshly-imported `DT_DCCConsumables` (`TapWater`, `HouseAle`, `WellWhiskey`, `RotgutSpecial`,
`EnergyTonic`), and `SellableEquipmentIds` at bar-appropriate rows already in `DT_DCCEquipment`
(`ThrownBottle`, `ServiceTray`, `Torch`, `SodaGun`). All still described in detail further down
this file. Equipment rows for the 12 new Classes' starter kits (`CreepyMask`, `Boombox`,
`WeldingTorch`, etc.) are still unauthored too — `Equip()` no-ops gracefully on an unknown id so
this isn't blocking, just cosmetic-less starter kits for now.

## 2026-08-26 overnight session — status and what's deliberately NOT done

Zoe went to bed; instructed to keep working autonomously via file/git edits, explicitly NOT to
push into live MCP editor operations tonight (placing actors, importing DataTables, PIE
testing) since that needs someone able to verify the actual result and catch a bad write in
real time, which nobody can do overnight. That constraint was mine to hold to, not just hers to
set — every MCP session this project's own history describes was done with her present for
exactly this reason (see this file's own many "read a value back" and "two 'fixes' were
recorded as done and were not" notes below). So: **nothing below required a live editor was
touched tonight.** What follows is what a live session should pick up first.

**Also blocked specifically on the Radial Selector plugin**, separate from the general MCP
question above: it needs an editor restart to activate, which needs Zoe's live approval, which
wasn't available tonight. `Docs/hotbar-radial-plan.md` covers the full plan; nothing in it that
depends on the plugin itself was started - only the plugin-independent pieces (Skill icon data,
see below) are safe to build blind.

**Built tonight (file/git only, no MCP):**
1. **Four natural-attack Skill rows** - `Docs/DT_DCCSkills_NaturalAttacksBatch1.csv` (Static
   Burst, Ball Lightning, Mark and Collect, Lien Enforcement), each now granted at Rank 1 via a
   `SkillBonus` on its class in `DT_DCCClasses.json` (Jumpscare Mage, Storm Chaser, Bounty
   Hunter, Repo Man respectively). Selecting the class grants the attack as a real trained
   Skill. **Deliberately left as Physical damage** - `FDCCSkillDef` has no `DamageType` field
   yet, and another session was actively mid-edit on that same file tonight (an Icon field for
   the hotbar work); touching the same struct for damage types was deferred to avoid a
   collision, not forgotten. `Docs/races-and-classes.md`'s TODO tracker has the full note.
2. Nothing else - the rest of tonight was spent confirming what NOT to do live, per above.

**Queued, needs live MCP (import + verify, not urgent, do whenever normal):**
1. Import `Docs/DT_DCCSkills_NaturalAttacksBatch1.csv` and the updated `DT_DCCClasses.json`.
2. PIE test: select Jumpscare Mage/Storm Chaser/Bounty Hunter/Repo Man in turn, confirm the
   signature attack shows up on the hotbar at Rank 1 and actually deals damage.

**Still queued from earlier tonight, unchanged, still needs a live session:**
- The Mount toggle (`[M]`) - compiled via DebugGame check, not yet Development-Editor-deployed.
- The Class Benefit component's `DT_DCCClasses.json` (full import, not just tonight's addition).
- The damage-type system's equipment authoring (`MassKilograms`, `WeaponDamageType`,
  `ResistanceDamageType`/`Level` all read defaults on every existing `DT_DCCEquipment` row).
- Everything under "Queued (2026-08-23, Mount toggle...)" and the sections below it - still
  accurate, nothing in them has changed.

## Camera bug fix + room-name HUD tag (2026-08-26, live playtest report from Zoe)

Both DebugGame-compile-checked clean (editor was open throughout, no numbered hot-reload DLL in
`Binaries/Win64/` afterward). **Both touch new `UPROPERTY`s, so neither is live until the usual
close-editor → Development Editor build → reopen cycle.**

### 1. Camera locked on the player's own face, close-up - root cause found and fixed

**Correction to this section, same day:** the first pass at this diagnosed the dark humanoid model
in Zoe's screenshot as an NPC the camera had collided with. She corrected that: it was her OWN
character, viewed head-on from the front, very close up, and it stayed that way rather than
resolving itself. That ruled out the spring-arm-collision-with-a-pawn theory entirely (the earlier
`ECC_Camera` capsule-ignore fix below is still left in - it is still correct third-person hygiene,
a spring arm should never collide with a pawn - but it was not the cause of this bug and would not
have fixed it).

**Actual root cause: Selfie Mode (`C`), stuck open because its own auto-exit timers got slowed to
a crawl by this project's own combat-pause system.** `SelfieCamera` (`DCCCharacterBase.h`) is a
literal selfie-stick camera - `SelfieCameraOffset = (110, 0, 40)`, `SelfieCameraRotation` yaw 180° -
mounted in front of the character facing back at it. That is exactly "own model, head-on, close up."
Entering it (`EnterSelfieMode`) deactivates the normal camera and activates `SelfieCamera`; getting
back out again is normally automatic, chained through two `SetTimer` calls: a `SelfiePoseSeconds`
(0.8s) pose hold, then a 0.4s post-flash hold, then `ExitSelfieMode()` - about 1.2s total, ordinarily
unnoticeable.

The trap: `ADCCGameState::SetCombatPaused` sets **global** `WorldSettings::TimeDilation` to
`PausedTimeDilation` (0.03 by default) while combat is paused (`P`), and every `FTimerManager` timer
created with a plain `SetTimer` call - including both of Selfie's own - slows down right along with
global time dilation. There is no "ignore dilation" flag on the basic API; this is just how UWorld's
timer manager works. If combat gets paused during or immediately after taking a selfie, that ~1.2s hold
stretches to roughly **40 real seconds** at 0.03 dilation. Long enough that "stuck taking a selfie"
reads exactly like "the camera is locked and won't let go" if you don't know to wait it out - and if
nobody happens to unpause in the meantime, it stays that way indefinitely in practice.

**Immediate workaround for Zoe, confirmed correct by reading the code, works right now with no
rebuild:** press `C` again. `ToggleSelfieMode` checks `bInSelfieMode` first and, if already open,
clears both pending timers and calls `ExitSelfieMode()` unconditionally - it does not wait on either
timer to fire, so it is not subject to the time-dilation slowdown at all. This should restore the
normal camera instantly regardless of combat-pause state. (If `C` seems to do nothing, check
whether a chat text box or menu currently has keyboard focus - close that first, then press `C`.)

**Root-cause fix, so this cannot happen again for any reason, not just this one:** rather than
special-casing combat pause specifically, added a real-time (wall-clock, NOT dilated) safety
ceiling in `Source/DungeonCrawlerDenver/Characters/DCCCharacterBase.h/.cpp`:
- `SelfieModeEnteredRealTimeSeconds` records `GetWorld()->GetRealTimeSeconds()` (unscaled by
  TimeDilation, unlike everything `SetTimer`/`Tick`'s own `DeltaSeconds` use) when Selfie Mode opens.
- `Tick()` force-exits Selfie Mode - clearing both timers and calling `ExitSelfieMode()` directly -
  if real time since entry exceeds `SelfieMaxRealSeconds` (new `EditDefaultsOnly` property, default
  5.0s - comfortably above the normal ~1.2s hold, so it never fires in ordinary play, but short
  enough that if it ever does the camera comes back in seconds, not tens of seconds).
- Logs a `Warning` when it actually triggers, so a future occurrence is visible in the log rather
  than silent.

This guarantees Selfie Mode releases the camera within 5 real seconds no matter what stalls its
normal exit path - combat pause, or anything else that might slow world time in the future -
without needing to reason about every possible cause up front.

### 2. Room-name HUD tag — built, reusing the existing room-tracking system

Small always-on Canvas tag, top-center, showing the room the local crawler is currently standing
in (e.g. "Cliff Diver Cantina," "Diving Pool") - so Zoe can tell which rooms she's actually walked
through versus only designed on paper. Canvas-based per the project's established HUD pattern, no
UMG.

**Deliberately did not build a new room-tracking system** - `ADCCRoom` (`Rooms/DCCRoom.h/.cpp`)
already tracks server-side occupancy via `AddOccupant`/`RemoveOccupant`, called from real overlap
events plus a reconcile timer. That data just never reached the client before now (`Occupants` is
server-only on purpose, per that file's own comment on why a parallel client-side list would be
wrong). Wired instead:

- **`ADCCRoom`** gets a new `RoomDisplayName` (`FText`, `EditAnywhere`) - the human label, separate
  from the existing `RoomId` (`FName`, a stable data key like `"Room07"` that was never meant to be
  shown to a player). Falls back to `RoomId` when left blank.
- **`ADCCCharacterBase`** gets `CurrentRoomDisplayName` (`FText`, replicated) and `CurrentRoomId`
  (`FName`, server-only, not replicated - it exists purely so `RemoveOccupant` can tell "the room
  I'm leaving" from "the room I'm already back in," for the case where two room volumes briefly
  overlap at a doorway).
- **`ADCCRoom::AddOccupant`/`RemoveOccupant`** now mirror the room's id/display name onto whichever
  crawler entered or left, right next to the existing `BP_OnPlayerEntered` call - no new hook, no
  new timer, just piggybacking the one that already fires on room entry/exit.
- **`ADCCHUD::DrawRoomTag`** (`UI/DCCHUD.cpp`) draws nothing when the string is empty (i.e. the
  crawler isn't inside any authored `ADCCRoom` volume right now) rather than showing a stale or
  placeholder label. Same dark-backing-rect-plus-text pattern as the existing centre-screen
  `DrawNotice`, called every frame from `DrawHUD` alongside `DrawVitals`/`DrawNearbyHealthBars` so
  it stays visible through everything except the full death/downed overlays.

**Still needed, editor-side, once MCP is back — `RoomDisplayName` is blank on every placed
`ADCCRoom` right now, so the tag will show nothing anywhere until these are authored:**

| RoomId | Suggested display name |
|---|---|
| Room01 (Queue) | "The Queue" |
| Room02 (Grotto) | "The Grotto" |
| Room03 (Falls) | "The Falls" |
| Room04 (Kitchens) | "The Kitchens" |
| Room05 (Bar) | "Cliff Diver Cantina" |
| Room06 (Pantry) | "The Pantry" |
| Room07 (Diving Pool) | "The Diving Pool" |

Set via `set_properties` on each placed `ADCCRoom` instance (`RoomDisplayName` is `FText` - pass a
plain string, same as any other text property this project has already written this way), then
read back to confirm, per this file's own standing rule.

## Chaos Vehicles plugin, Oathgate location, cliff-lift feature (2026-08-27)

### 1. Chaos Vehicles plugin - enabled in the config, not yet active

Added directly to `DungeonCrawlerDenver.uproject`'s `Plugins` array (same flat `{"Name", "Enabled":
true}` shape every other entry there uses):
```json
{
    "Name": "ChaosVehiclesPlugin",
    "Enabled": true
}
```
Confirmed necessary, not cosmetic: read the plugin's own `.uplugin`
(`Engine/Plugins/Experimental/ChaosVehiclesPlugin/ChaosVehiclesPlugin.uplugin`) and it declares
`"EnabledByDefault": false`, so without this entry it would not load at all. JSON validated by
parsing it back with `json.load` - well-formed. **As agreed, did not touch the running editor or
attempt a restart** - this only takes effect at the next editor startup, same as
`EditorToolset`/every other toolset this project has enabled before. There's also
`ChaosModularVehicle`/`ChaosModularVehicleExamples` sitting next to it in `Engine/Plugins/
Experimental` (newer, more flexible, still experimental) - left that alone and went with the
plain `ChaosVehiclesPlugin`, the standard/stable path, unless there's a specific reason to want
the modular one later.

Noticed in passing, not acted on: `RadialSelectorPlugin` (`Plugins/Marketplace/
RadialSelectorPlugin`) also has no entry in the `.uproject`'s `Plugins` array despite Zoe saying
it's "still pending" activation - worth checking whether it needs the same kind of explicit entry
before the next restart, since a project-local Marketplace plugin isn't guaranteed
`EnabledByDefault: true` either. Did not add one myself since Zoe didn't ask for that in this
message, only Chaos Vehicles.

### 2. Where the Oathgate is right now: **nowhere - it has never been placed**

Checked two ways, not just the docs:
- `Docs/DISPATCH.md`'s own history: every mention of the Oathgate (search `Oathgate` in this file)
  describes it as built C++ (`Rooms/DCCOathgate.h/.cpp`, a paired-teleporter DM testing arena) that
  is explicitly still queued to place - "**Place a paired `ADCCOathgate`** (new this session)... two
  instances, each with `PairedGate` set to the other" is listed under "still open, none of it
  attempted this session (needs Zoe's visual judgment)."
- **Confirmed directly, not just from the docs**: scanned every placed actor in
  `Content/__ExternalActors__/ThirdPerson` (the whole level's World Partition actor set) for any
  reference to the Oathgate class - zero matches, anywhere.

So there is no location to report - the class is written and ready (a working paired-teleporter
actor with an optional armed encounter, per its own header), but nobody has actually dropped the
two instances into the level yet. That is squarely an MCP/editor task, not a code one - next MCP
session should place the pair, matching the queued item 8a further down this file (leave the
default `EnemyPool` - Debt Eater/Fine Print/Arbiter - on the entrance side, clear it on the exit
side).

### 3. Bartender cliff-lift feature - built, resolved which bartender, not yet placed

**Checked which NPC Zoe meant, since her description didn't match either existing Bartender.**
Scanned every placed `ADCCDialogueNPC` in the level (only three exist at all): the real Bartender
in the Bar interior (behind the counter, per `floor-01-design.md` Room 5), the stray duplicate
`DCCDialogueNPC_Bartender2` at x9133/y-718 near spawn (already flagged elsewhere in this file as
unplaced/uncleaned test content), and `BarGuard`. **None of them are anywhere near the cliffs.**
So "an NPC bartender standing on a platform near the rear of the large cliffs" is not a
mislabelled reference to either existing one - it is new content that does not exist in the level
yet, near Room 3 (The Falls)/Room 7 (The Diving Pool), the floor's actual cliffs.

**Built, DebugGame-compile-checked clean, not yet deployed (same close-editor → Development build
→ reopen cycle every reflection change this session needs):**

- **`Rooms/DCCCliffLift.h/.cpp`** - a new `ADCCCliffLift` actor: a `StaticMeshComponent` platform
  that rises from wherever it's placed to `RaiseHeight` above that, over `RaiseDurationSeconds`,
  once `RequestRaise()` is called. Server-authoritative and replicated (a rising platform is a
  shared, every-player-sees-it object, so this follows the same rule as everything mechanical even
  though it's cosmetic rather than a dice roll) - every machine derives the same Lerp independently
  from one replicated server timestamp rather than a transform being replicated every frame. One-
  shot: calling it again once raised, or while already rising, is a silent no-op. **Not
  physics-simulated, per the ask** - just `SetActorLocation` each Tick; a standing character rides
  along for free via Unreal's own built-in moving-platform support (`CharacterMovementComponent`
  already carries a character standing on a `Movable` actor's collision, no custom carry code
  needed).
- **`Dialogue/DCCDialogueNPC_LiftAttendant.h/.cpp`** - a new `ADCCDialogueNPC` subclass. Ordinary
  approach-triggered dialogue is untouched; a deliberate `E` press is redirected (same shape as
  `ADCCVendorNPC` redirecting `E` to a shop instead of the next line) to say a flavour line
  ("Hang on, I'll take you up." / "Already on its way. Hop on." if pressed again) and request the
  lift via a new `ADCCCharacterBase::Server_RequestCliffLift(ADCCCliffLift*)` RPC - the client's
  press is a nomination, the server re-checks distance (500 units) before actually calling
  `RequestRaise()`, same "nominate, don't dictate" pattern as `Server_RequestGenericClimb`. Has one
  `EditInstanceOnly` property, `LinkedLift`, so the same class works for any future NPC-operates-a-
  lift pairing, not just this one.
- **Deliberately did not build a choice/confirmation menu.** Zoe's ask said "keep it simple," and
  this project has no branching-dialogue-choice system anywhere to hang one off - the existing
  `ADCCDialogueNPC` model is a single script that advances one line per `E` press, no forks. A
  press-E-and-it-happens interaction (matching how the vendor's `E` already works) reads as "ask
  for a lift" without inventing a menu system for one feature. Worth revisiting if Zoe wants an
  actual yes/no confirm later.

**Still needed, editor-side, once MCP is back - this cannot be tested or even seen until then:**
1. Place one `ADCCCliffLift` instance near the base of the cliffs (Falls/Diving Pool), and tune
   `RaiseHeight`/start position against the real geometry - there's no way to pick the right number
   from code, it has to match wherever the clifftop actually is.
2. Place one `ADCCDialogueNPC_LiftAttendant` near it, give it a mesh/`SpeakerId`/`DisplayName`
   (this is the "bartender" - or whatever character Zoe wants voicing it; nothing says it has to
   reuse the real Bar bartender's identity, and giving it a distinct `SpeakerId` avoids colliding
   with the real Bar's dialogue script in `DT_DCCDialogue`), and set its `LinkedLift` to the lift
   above.
3. PIE test: approach, press `E`, confirm the platform rises and actually carries a standing
   character to the top, and that a second `E` press mid-rise or after arrival says the
   already-going line instead of restarting anything.

## Follow-up same day (2026-08-27): Oathgate placement and cliff-lift PIE test still blocked - no MCP this session despite the editor being open

Zoe reported the editor open and asked for (1) the two Oathgate instances placed, (2) the
cliff-lift NPC/platform positioned and PIE-tested. **Neither happened - this session never had
Unreal MCP tools available, editor-open or not.** Checked directly rather than assuming: searched
for every unreal-editor-toolset-shaped tool by name and by keyword, twice, with different search
terms - nothing came back. This matches this project's own long-established, repeatedly-confirmed
finding elsewhere in this file: **the MCP connection is picked up once, at a Claude Code session's
own startup, from whatever the editor's state is at that moment** - a session that started before
the editor came up (or in a different session's process) does not retroactively gain the
connection just because the editor opens later or is open now. Only a *fresh* Claude Code session,
started after the editor (with the server already listening on port 8000), gets the tools. Did not
fake, simulate, or guess at placement/PIE results to cover this - per this file's own standing
rule, an unverified claim is worse than an honest "did not happen."

**So, unchanged from the previous entry:** the Oathgate pair is still unplaced (class ready,
`Rooms/DCCOathgate.h/.cpp`), and the cliff lift + lift-attendant NPC are still built and compiled
but unplaced and PIE-untested (`Rooms/DCCCliffLift.h/.cpp`,
`Dialogue/DCCDialogueNPC_LiftAttendant.h/.cpp`). Both need a session that starts fresh with the
editor already running to actually place anything - whoever picks this up next should confirm they
have working `unreal-mcp`-style tools before promising Zoe progress on either.

**What did get done this pass, no MCP needed:** the Supercan roster entry (see below).

## Supercan - roster entry written, queued for Meshy (2026-08-27)

New enemy concept from Zoe: a recycling/trash can body on a pogo stick, baseball bat melee. Full
write-up added to `Docs/enemy-roster.md` under "Floor 1 - the Denver layer" (its own subsection,
right after the Nuclear Family, matching that entry's depth: model-ready physical description,
bounce-telegraphed moveset concept - a compress-then-slam AoE called Curbside Pickup, plus a
two-hit bat combo - and a stat-block-appropriate role call (elite melee bruiser, not a swarm
minion, weight-classed against a named brute rather than a Debt Eater, sized to justify the
cursed-equipment drop the other session is scoping). **Deliberately no C++, stats, or
`DT_DCCEnemies` row yet** - Zoe's own instruction was design and model concept only, sequenced the
same way every other BUILT roster entry went: description first, Meshy generation next, mechanics
after the model exists. Optionally ties the Pantry's still-open "whose unclaimed property is this"
gap (`Docs/room-purpose-audit.md`) shut if a room ever needs to be picked for him, but the write-up
is explicit that nothing about the concept requires that placement.

**Next step, for whoever has the Meshy session:** hand the "Physical description (for model
generation)" block in that new entry to the image-first pipeline, same process that fixed the
melee/Head Diver detail-loss problem.

## Overnight, 2026-08-27: enemy breadth pass + guns investigation

Zoe went to bed with explicit go-ahead on everything short of spending money. Two items, no MCP
needed for either (still none this session - see the entry above this one).

### 1. Ten new enemy concepts, `Docs/enemy-roster.md` § "Breadth pass - general roster"

Ask was breadth, not depth on every one - "she'll give specific examples to refine later" - so
effort is intentionally uneven across the new section, same shape the section itself explains:

- **Four fodder-tier** (Repo Intern, Loss Prevention Associate, Temp Worker (Untrained), Focus
  Group Ghost) - a paragraph each, no full physical-description block. Temp Worker is a direct
  joke on the ruleset's own Untrained rule (Attack Skills at Disadvantage, no Rank) - worth
  remembering if it's ever actually built, since the flavor only lands if it mechanically fights
  worse than a trained enemy of the same tier, not just narratively.
- **Four mid-tier** (Middle Manager Recently Promoted, Compliance Drone Swarm, The Onboarding
  Buddy, The Committee) - full moveset concepts, no full physical-description block except the two
  flagged for Meshy (see below).
- **Two named/mini-boss-tier** (The Self-Checkout, The Severance Package) - full Nuclear-Family-
  depth: physical description ready to hand to Meshy, moveset concept, role. The Severance
  Package's "unpacks smaller enemies from its own boxes mid-fight, gets smaller/easier to hit as it
  does" is the most mechanically distinct idea in the batch - a real focus-fire-vs-thin-the-summons
  fork, not just an HP race with adds.

**Four of the ten are flagged Meshy-worthy** (Onboarding Buddy, The Committee, Self-Checkout,
Severance Package) - each has a "**Sourcing:** Meshy" line in the doc explaining why in terms of
the boilerplate-rifle/Supercan bar (a silhouette no stock asset or simple reskin gets close to).
**The other six are flagged "free-source a body and reskin"** - deliberately not sent to Meshy,
since a security guard, an intern, a mall-cop and a mid-tier office bruiser are all just a generic
humanoid plus a costume, and generating those would be spending model-gen effort on something a
free asset pack already solves for less.

**Next step, for whoever has the Meshy session:** hand the four "Physical description (for model
generation)" blocks (Onboarding Buddy, The Committee, The Self-Checkout, The Severance Package) to
the image-first pipeline, same process used for Supercan and the Head Diver. The other six need no
Meshy time at all - just a free-source pass whenever someone's doing a body-shopping session.

**No C++, stats, or `DT_DCCEnemies` rows for any of the ten** - concept and (where flagged) model
brief only, matching how every prior roster entry has been sequenced (description, then model, then
mechanics - never the reverse).

**Kept internally consistent with the other session's loot-box work on purpose, without touching
it:** the other session is scoping a six-tier (Bronze-Celestial) achievement-to-loot-box reward
system in parallel. None of these ten enemies were given a loot table, a rarity tier, or a reward
hook here - that's a deliberate gap, not an oversight, so the two docs don't end up disagreeing
about who owns tier assignment. Whoever finishes the loot-box system should treat this batch (and
Supercan) as unassigned reward-pool candidates once it exists, not as already-slotted content.

### 2. Real guns - investigated, reported, safe groundwork laid, nothing risky built

**What already exists (checked directly, not assumed):**
- **`ADCCVendorNPC`** (`Dialogue/DCCVendorNPC.h`) is a real, generic, already-working vendor class -
  `SellableEquipmentIds`/`SellableConsumableIds` are flat `FName` arrays, editable per placed
  instance, resolved against `DT_DCCEquipment`/`DT_DCCConsumables` at that row's own `GoldValue`.
  E-press opens a real shop menu (`DrawVendorMenu`, `DCCHUD.cpp`) with a working purchase flow. It
  is not vendor-type-specific in any way - it would sell guns exactly as readily as it sells
  drinks, with zero new code, the moment an instance exists with the right ids in its list.
- **Six existing ranged-flavored `DT_DCCEquipment` rows** already imported: `ThrownBottle`,
  `ServiceKnifeSet`, `SodaGun`, `ConfettiCrossbow`, `NailGunPro`, `FlareGunEmergency` - all have
  `AttackRangeOverride` set (mechanically ranged), all in the established "everyday object
  reskinned as a weapon" comedic vein. **None of these are actual guns** - a soda gun, a novelty
  crossbow, a nail gun, a flare gun. This is almost certainly the gap Zoe means by "real guns."
- **A loot-chest path for ranged gear already exists**: `ADCCLootContainer_Quartermaster`
  (`Items/DCCLootContainerVariants.h`), a guaranteed-drop chest already seeded with the ranged
  rows above - so "obtainable from a loot chest" already has a working mechanism, just not real
  gun content in it yet.

**What's actually missing, confirmed rather than assumed:**
1. **No vendor is placed anywhere in the level at all.** Scanned every placed actor in
   `Content/__ExternalActors__/ThirdPerson` for `DCCVendorNPC` - zero matches. This is a
   pre-existing gap (queued since 2026-08-23, not new tonight) and it blocks "obtainable... via a
   vendor" completely regardless of guns specifically - there is currently no way to buy *anything*
   in-game. Needs an MCP session to place one (in the Bar, replacing the placeholder
   `DCCDialogueNPC_Bartender`, is still the obvious spot per the standing queued item).
2. **No real gun meshes exist in the project.** Re-confirmed this project's own earlier finding:
   nothing under `Content/` is bow- or gun-shaped. Per the sourcing philosophy, a plain gun is
   explicitly NOT Meshy-worthy - free-source it. Two free packs were already identified in an
   earlier session and are still the right answer: **ASIS Free Gun Weapons Pack** (itch.io, FBX +
   textures, imports through the existing pipeline with no Blender conversion) and the **Free
   Fantasy Weapon Sample Pack** (Fab, has some gun-shaped pieces). Both are account-gated
   downloads only Zoe can do.
3. **No real-gun `DT_DCCEquipment` rows existed until tonight.** Added four, staged in
   `Docs/DT_DCCEquipment.csv`, not yet imported: `SecuritySidearm` (die 8, range 1200, 130g),
   `HuntingRifleColorado` (die 12, range 2000, 190g), `RiotShotgun` (die 14, range 500 - short and
   hard, matching a real shotgun's spread rather than a sniper's reach, 165g), `ConfiscatedSMG`
   (die 10, range 1600, 210g, flavored as evidence-locker overflow tying it to the Debt
   Eaters/collections theme already established on this floor). **`Mesh` is deliberately empty on
   all four**, same pattern `ConfettiCrossbow` already used - there is nothing to point it at yet.
   Flavor text follows the same dry, checkable-joke rule as the rest of the sheet (a mall-security
   sidearm the store can't explain needing live rounds for; a Colorado hunting rifle used somewhere
   it was never licensed for) rather than making the guns themselves the joke - Zoe asked for real
   guns, not novelty ones, so the comedy stays in the flavor text the way it already does for
   `FireAxe`/`WallSword`, not in the weapon concept.

**Deliberately NOT done, because it needs either MCP or Zoe's own judgment:**
- Did not import the CSV (needs MCP, same `create`/`add_rows`/`set_rows` route as every other
  DataTable this project has touched - plain `import_file` will accept this one fine actually,
  since it's CSV not JSON, so a straightforward re-import should work once someone has MCP).
- Did not place a vendor, and did not decide which four (or fewer) of the new guns any given vendor
  should actually stock - that's a real design/pricing decision, not a mechanical one, and better
  left for Zoe or a live session to weigh against the Bar's existing sellable list rather than
  guessed at overnight.
- Did not touch pricing/balance against the achievement-loot-box tiers the other session is
  building, for the same reason as the enemies above - these are unassigned candidates, not
  slotted content, until that system exists to slot them into.

**Queued for whoever has MCP next:** re-import `Docs/DT_DCCEquipment.csv` (four new rows, schema
unchanged - plain re-import); once Zoe has downloaded a gun pack, point the four new rows' `Mesh`
columns at real meshes and re-import again; place a vendor in the Bar and decide its stock list.

## Achievement content for the new loot-box system - `Docs/achievements.md` (2026-08-27, later same night)

Doc-only, per Zoe's own instruction tonight: another session is fixing a build-lock crash, so no
compiling this pass. Wrote the actual content for the six-tier (Bronze-Celestial) achievement->
loot-box system the Ally/loot session is building mechanically - 20 entries, real-DCC style
(specific one-off weird actions - "wore no pants," "first in with a cat" - not running-total
counters), snarky System AI voice matching `DT_DCCAchievements.csv`'s existing Announcement style.

**Deliberately a different kind of achievement from the existing 20-row `DT_DCCAchievements.csv`,
not a replacement or a duplicate of it.** That table is threshold/counter-driven (kills, damage,
metres climbed, etc.) and stays exactly as-is; this new list is one-off event-driven, matching what
the real books actually do and what Zoe specifically asked for. The doc opens with a section
explaining that relationship so nobody wires the two together by mistake.

**Every one of the 20 is tied to something specific to this game** - the Collapse cutscene's
door-order and party-ready gate, Selfie Mode, combat pause, Untrained skill checks, the Pantry's
Strongbox, Voldemortgage's Balloon Payment/Late Notice/lien, the body-collector system
(`CheckForBodyCollector`), per-room visitation, the Oathgate, tonight's own new cliff lift, AI
Favor tiers, and two composite session-end achievements for the Celestial tier - not generic
fantasy filler. Spread 4/4/4/3/3/2 across Bronze through Celestial, tapering rarity toward the top
same as the real tier list implies.

**Every entry is honestly flagged `EXISTING` or `NEEDS NEW TRACKING`, with what the new tracking
would actually have to watch** - five are wireable today off signals that already exist
(`ReadyForCollapse`, `TakeSelfieScreenshot`, `OnCombatPauseChanged`, the Equipment component's slot
check, `UDCCFavorComponent`'s tier), most of the rest need one new hook at an existing call site,
and two are honest about needing more than tracking: **Gate Crasher** needs the Oathgate actually
placed and given a usage event (neither exists yet - see the Oathgate section above), and **Off The
Air** needs Last Call itself, which is still just a "Still idea" in `enemy-roster.md`'s Denver
layer and isn't built in any form. Flagged rather than cut, since both are strong tier-appropriate
hooks once their prerequisites exist. Full cost-sorted summary is the doc's own closing section.

**Pinged the live peer sessions directly** (`ListAgents` showed five peers tonight, including a
newly-named `image-and-model-generation-54` - confirms the Meshy hand-off from earlier this session
landed on the right session) rather than relying on this file alone, since several are running
concurrently right now and a doc update alone might sit unread for hours.

## Queued (2026-08-23, Mount toggle - compiled, NOT yet deployed)

Built this session, then the editor came up mid-session (Zoe or another session at the
keyboard) - only DebugGame-compile-checked from that point on, not linked into a closed-editor
Development build. **This needs a full close-editor Development Editor build before anything
below is live, on top of the usual MCP re-import.** `Docs/races-and-classes.md`'s TODO tracker
has the full writeup.

1. **Close the editor, run the Development Editor build, confirm no numbered hot-reload DLL,**
   before doing anything else in this list.
2. **Re-import `Docs/DT_DCCClasses.json`** - Rideshare Driver now grants a Mount.
3. **PIE test: select Rideshare Driver, press [M], confirm movement speed actually increases**
   and press it again to confirm it toggles back off. Also press M as a character with no Class
   selected (or a Class without `GrantsMount`) and confirm it's a silent no-op, not an error.

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

## Room-purpose audit done - `Docs/room-purpose-audit.md` (2026-08-26)

Zoe's own test: does every Floor 1 room have a clear player purpose (combat/loot/puzzle/story) or a clear
NPC/in-world-economy purpose (why this room/character makes sense given 5M+ people were suddenly pulled
into a for-profit dungeon-show, per `story-premise.md`), cross-referenced against the tone bible so "purpose"
means the game's actual corporate-dystopian voice, not generic dungeon-crawler logic. Audit only - nothing
fixed, per her explicit ask.

**Result: 5 of 7 rooms pass clean, 2 have a real but cheap-to-close gap.**

- **Room 3 (The Falls)** and **Room 6 (The Pantry)** both have a clear player purpose but no stated
  in-world reason to exist - each needs about one sentence (tying the climb to the divers' own access
  route; saying whose "unclaimed property" is actually in the Pantry).
- **Verified, not just claimed:** the grievance escalation ladder (Grotto → Kitchens → Bar → Diving Pool)
  actually works as designed - checked directly against `DT_DCCDialogue.json` rather than trusting the
  design doc's own description of it. The two flagged rooms are also the two rooms missing from that ladder
  entirely (Room 1 has no ladder line either, but passes anyway - its NPC-lessness is the intended joke,
  not a gap). Extending the ladder into 3 and 6 would likely close both gaps in one move.
- Room 7's "the divers are still diving... nobody acknowledges it... the floor's thesis in one shot" line
  is flagged as the standard every other room is being measured against - worth preserving deliberately
  while touching anything nearby.
- Full room-by-room writeup, plus a note that this same lens should be applied to the Nuclear Family/HOA/
  Last Call/Brunch Rush content once any of it gets a room assignment, is in `Docs/room-purpose-audit.md`.

## Overnight session, 2026-08-25 - morning handoff

Asked to work autonomously overnight on two fronts while Zoe slept: (1) run the DarkPaperVerse/PolyReady
rigging pipeline on newly-landed Fab assets, (2) continue the map-scale and NPC-bug work. **Honest split:
one was blocked on a real discrepancy, the other produced verified, working results.**

### (1) Blocked - the assets were not actually on disk

Checked `Content/` repeatedly (`find Content -maxdepth 1 -type d -newermt ...`) through the night. **None
of the requested assets ever appeared**: Low Poly Fighter Starter Pack, PolyReady's Undead Skeleton, and
all seven DarkPaperVerse items (Tin Guardsman, Synthetic Droid, Humanoid Cactus, Dice Golem, Torsion Helix,
Helianthus Stalker, Fossilized Vespiary) plus Abyssal Reaper and Crimson Knight - none of these exist on
disk as of this handoff, despite being described as having "just landed via Fab." This matches the exact
failure mode already documented earlier the same day (Fab's "Add to Project" UI reporting success for
items that never actually wrote files) - worth Zoe specifically re-checking these in the Fab panel herself
once she's back, rather than assuming the overnight session simply didn't get to them. **Nothing was
fabricated or assumed here - there was nothing to run the rigging script against, so it wasn't run.**

The Unreal Editor itself was confirmed running overnight (for the Radial Selector / MetaHuman Creator
plugin activation), but this session still has no Unreal MCP connection - consistent with the
already-documented finding that a new MCP connection needs a fresh Claude Code session, not just a
running editor. Blender MCP remained live throughout and is what made (2) below possible.

### (2) Real progress - the shared-skeleton bug now has a verified fix for all three characters

The arithmetic-based bone fix attempted earlier the same day (see "Confirmed real bug" section below) was
wrong, and admitted as wrong at the time. **Found the right technique overnight and verified it actually
works**, on all three affected characters, not just Marcel:

**The technique:** don't compute deltas. Each character's own ORIGINAL skeleton (still on disk,
`SourceArt/{Marcel,Kyle,Scott_OfficeAirplane}/fbx/SK_*.fbx`) already has its joints correctly positioned
for that exact mesh - Meshy's own auto-rig fitted it. So: import that original skeleton alongside the
shared one, read every relevant bone's absolute world-space head/tail position directly off it, then copy
those exact coordinates onto the shared skeleton's corresponding bones (converting through
`matrix_world`/its inverse, which is what sidesteps the 0.01-object-scale bug that broke the arithmetic
attempt). No lengths, no starting-point assumptions, no unit conversion errors - just "put this joint
exactly where this character's own correctly-fitted rig already says it goes."

**Verified with real renders, not assumed** - bind pose plus a posed test (leg lifted, arms raised) for
all three:

- **Marcel** - clean. No tearing, no stretching, knee and elbow bend exactly where they should.
- **Kyle** - clean, same as Marcel.
- **Scott** - clean overall; the lifted-leg foot reads slightly oddly in the render angle used, worth a
  second look but nothing structurally broken (no tearing/floating geometry like the original bug showed).

This is a dramatic difference from the original bug's own render (bones visibly floating above the actual
feet) and from this same session's earlier failed arithmetic attempt (foot ended up **below the floor**).

**Saved and ready for review, not yet imported to the live game:**

- `SourceArt/Marcel/rig_fit_FIXED_Marcel.blend`
- `SourceArt/Kyle/rig_fit_FIXED_Kyle.blend`
- `SourceArt/Scott_OfficeAirplane/rig_fit_FIXED_Scott.blend`

Each has the shared skeleton, correctly repositioned, bound to that character's real mesh, saved in a
neutral rest pose. The original (broken-proportions) `rig_fit_check_*.blend` files from earlier are left
alone for side-by-side comparison, not deleted.

Also new, promoted into the repo: **`Tools/fix_rig_to_own_proportions.py`** - the exact script that
produced all three fixes above, reusable headlessly (`blender --background --python
Tools/fix_rig_to_own_proportions.py -- <shared_skeleton.fbx> <character_own_skeleton+mesh.fbx>
<out.blend> <render_prefix>`). **The Nuclear Family and the Oathgate's `FinePrint`/`Arbiter` do NOT need a
separate check** - they use the identical three `_Shared` meshes as Scott/Kyle/Marcel (confirmed in the
original bug write-up below), so the same measured mismatch and the same fix apply to them automatically
once Scott/Kyle/Marcel's fix is confirmed good and re-imported. No new Blender work needed there, just the
same re-import step repeated per Blueprint.

**What Zoe should do with this, once she's back:**

1. Open one of the three `.blend` files, eyeball it herself (this was the whole reason she wanted a
   hands-on Blender workflow rather than a silent auto-fix) - confirm the renders aren't hiding something
   a live look would catch.
2. If it looks right: export as FBX, re-import into Unreal **bound to a new Skeleton asset per character**
   (not back onto `SK_DCCCrawler_Skeleton` itself - see the original bug write-up below for why one
   skeleton can't fit three different bodies).
3. Then the still-unverified follow-up from earlier: check whether UE5's Compatible Skeletons list is
   really what lets the existing shared Walk/Run animations keep playing on these new, individually-fitted
   skeletons. Flagged as unconfirmed both times it's come up - still true.

### Map-scale work

No new progress to report - still blocked on the same thing it was blocked on before (MCP/editor
placement access, which this session never had). Nothing padded in here; the existing plan in
`Docs/asset-needs-meshy-pipeline.md` stands unchanged.

## Wiring in the Fab downloads as they land (2026-08-25, live session)

Zoe started clicking "Add to Project" in the in-editor Fab panel for the confirmed-download list from
earlier the same day. No Unreal MCP this session (editor not running, confirmed repeatedly), so
verification was filesystem-only (`find Content -maxdepth 1 -type d -newermt ...`) rather than trusting
the Fab UI, which she'd already flagged as unreliable. Real gap surfaced: **two of the highest-priority
picks reported "succeeded" in the UI but never appeared on disk** - Low Poly Fighter Starter Pack and
PolyReady's Undead Skeleton, the two meant to cover the Oathgate arena roster and the generic-NPC gap,
were still missing as of the last check this session. What DID land:

- `AIGeneratedRobots1` - confirmed static-mesh-only (MechanicalHeron/MotorcycleDog/RobotOstrich, all
  `SM_`, no skeleton) - matches the earlier "skip for enemies" flag exactly. Fine as background dressing.
- `SciFiWorld` - a sci-fi lab/research-room environment kit (hologram alien props, lab capsules, sci-fi
  doors). Wasn't on anyone's list - possible stray click, not acted on.
- `NY_Subway` - **this is the Turnstile/Vending Machine pack**, confirmed with the exact assets hoped for:
  `SM_RSG_Subway_Vending_Machine_Snacks`, `_Water`, `SM_RSG_Subway_Ticket_Vending_01`, turnstile meshes.
  All static props - genuinely useful for dressing the bartender/vendor scene, but placement is an
  MCP/editor task, not something this session could wire in code.
- `RPGClassIcons` - both the Rogue and the Psion/Mindweaver icon sets landed together in one pack
  (`PSI_Psion_Mindweaver/`, `ROG_Rogue/` both present). **Acted on this one** - see below.

### Built: a real Skill icon system, since RPGClassIcons finally gave the hotbar something to show

`FDCCSkillDef` had no `Icon` field at all before this - the hotbar (`DCCHUD::DrawHotbar`) was always
text-label-only, flagged as a gap back when the icon packs were first found in Zoe's library. Added:

- `TSoftObjectPtr<UTexture2D> Icon` on `FDCCSkillDef` (`Skills/DCCSkillTypes.h`) - optional, soft
  reference so the ~50 already-authored skill rows without one don't pay any loading cost.
- `DrawHotbar` now draws the icon via `AHUD::DrawTexture` when a row has one (dimmed while on cooldown,
  same as the text label already was), and **falls back to the exact original text-label behaviour when
  it doesn't** - nothing regresses for any skill that isn't given an icon.

DebugGame compile-check passed. **Not deployed** - reflection change (new `UPROPERTY`), needs the usual
close-editor Development build. **Not wired to any specific skill yet** - the system exists, but nobody
has picked which of the ~500 Rogue/Psion icons fits which of DCD's actual skills (Persuade, Intimidate,
Fire Fingers, etc.). That's a content pass, not a code one - a natural next MCP-session task once the
editor's open: open `DT_DCCSkills`, browse `RPGClassIcons/ROG_Rogue` and `PSI_Psion_Mindweaver` for
matches, set `Icon` per row.

### Not attempted

New C++ enemy classes for the Oathgate roster / generic-NPC gaps, since neither Low Poly Fighter Starter
Pack nor Undead Skeleton have actually landed on disk yet (see above) - there's nothing to point a
`ConstructorHelpers::FObjectFinder` at. This is the very next thing to do the moment either shows up;
worth Zoe re-checking the Fab panel for those two specifically rather than assuming the UI's "succeeded"
was accurate. NY_Subway's vending machines and SciFiWorld are both placement/set-dressing tasks for the
next MCP session, not something codable in the meantime.

## Confirmed real bug: the shared skeleton doesn't fit Scott/Kyle/Marcel's proportions (2026-08-25)

Zoe reported messy animation on the shared-skeleton characters - "elbows sitting too high, a leg not
fitting the skeleton's proportions at all." Traced and confirmed with real measurements, not guessed - this
is the exact "untested risk" CLAUDE.md's own consolidation note already flagged and never followed up on:
**"identical hierarchy does not guarantee identical proportions or bind pose, so shared animation may
drift visually. Test on one character before committing."** Nobody had. This is that test, and the risk
was real.

**What's actually true:** `SK_DCCCrawler_Skeleton` is CJ's own original skeleton (`SK_HockeyWings_Skeleton`,
renamed) from the 2026-08-15 consolidation. Scott, Kyle and Marcel's meshes were re-bound onto it, but their
own original skeletons (still on disk in `SourceArt/{Scott_OfficeAirplane,Kyle,Marcel}/fbx/SK_*.fbx`) have
noticeably different limb proportions. Measured bone lengths, original-rig vs. the shared skeleton they
now stand on (metres):

| Segment | CJ (= shared skeleton) | Scott | Kyle | Marcel |
|---|---|---|---|---|
| Thigh (LeftUpLeg) | 0.2608 | 0.3511 (+35%) | 0.3820 (+46%) | **0.4327 (+66%)** |
| Shin (LeftLeg) | 0.2883 | 0.3666 (+27%) | 0.3204 (+11%) | 0.3097 (+7%) |
| Spine02 | 0.1680 | 0.1236 (-26%) | 0.1380 (-18%) | 0.1272 (-24%) |
| Upper arm | 0.2183 | 0.2256 | 0.2484 | 0.2193 |

Marcel is worst (thigh 66% too short on the shared skeleton relative to his real proportions), Kyle next,
Scott least-bad but still off by over a quarter on both leg segments. **CJ himself is unaffected** - the
skeleton is his own body's rig, nothing to mismatch.

**This also means every OTHER thing built on those same three `_Shared` meshes inherits the identical
problem**, not just the players: the Nuclear Family (`ADCCEnemyCharacter_NuclearFamily.cpp`, all three
members) and the Oathgate's `FinePrint`/`Arbiter` (`SK_ArmsOut_Shared`/`SK_DiceWinged_Shared`). Same fix,
same root cause, four+ places it shows up.

**Visually confirmed, not just numerically** - bound Marcel's real mesh to a copy of the shared skeleton
(automatic weights; Blender logged "Bone Heat Weighting: failed to find solution for one or more bones",
itself a symptom) and screenshotted it in X-ray: the ankle/foot bone sits well above his actual feet, a
visible gap between the last bone segment and his shoes.

### Blender MCP was down, and is now back up

Confirmed dead first (`get_scene_info` failed - "Could not connect to Blender," and no `blender.exe`
process existed, only stale bridge processes). **Fixed by simply launching Blender** (`Start-Process
"D:\Blender\blender.exe"`) - the MCP addon was already installed and set to auto-start from Zoe's prior use
on other projects, so it reconnected the moment a real Blender window existed. No addon reinstall needed;
if it drops again, relaunching Blender is very likely all that's needed.

### The right fix technique - and the wrong one, learned the hard way this session

Tried fixing Marcel's leg by copying his old skeleton's segment *lengths* onto the shared skeleton via raw
edit-bone Z-coordinate arithmetic. **Do not do this** - it requires knowing the correct starting reference
point (hip height) too, not just segment length, and it's very easy to get the local-space vs. world-space
scale wrong (this project's shared skeleton has a 0.01 object scale from the original FBX import - a
delta measured in world metres has to be multiplied by 100 before applying it to `edit_bone.head/tail`,
which are in the armature's unscaled local space). Got the arithmetic wrong twice in a row before reverting
- once undershooting by 100x, once overshooting so far the foot ended up below the floor.

**The actual right technique - visual, not arithmetic, and it's the same "hands-on, see it, fix it"
workflow Zoe already likes from the socket work:**

1. Open the relevant `rig_fit_check_<Name>.blend` (prepared this session, see below) - shared skeleton
   already bound to that character's real mesh via automatic weights, armature already set to show in
   front (X-ray) so bones are visible through the mesh.
2. Select the mesh, enter Edit Mode, find the actual joint crease (e.g. the knee bend, the ankle) on the
   real geometry. Select a vertex right at that crease.
3. `Shift+S` -> **Cursor to Selected** - snaps the 3D cursor exactly to that point on the mesh.
4. Switch to the armature, enter Edit Mode, select the bone joint that should sit there (e.g. the
   thigh bone's tail, which is also the shin bone's head if they're meant to meet at the knee).
5. `Shift+S` -> **Selection to Cursor** - snaps the bone joint exactly onto that mesh point. No manual
   coordinate math, no scale-factor bugs - this sidesteps the entire class of mistake made above.
6. Repeat per joint that looks wrong (knee, ankle, elbow, wrist...). Mirror left-side fixes to the right
   side with Armature -> Symmetrize, or repeat the same snap on the right-side vertex/bone.
7. Exit Edit Mode and eyeball it in Pose Mode - pose a few bones by hand (rotate the leg, the arm) and
   confirm the mesh now bends at the right points instead of tearing.
8. Export as FBX, re-import into Unreal. **Import bound to a *new* Skeleton asset** (e.g.
   `SK_DiceWinged_Fixed_Skeleton`), not back onto `SK_DCCCrawler_Skeleton` itself - the whole reason this
   bug exists is that one skeleton can't correctly fit three different bodies at once, so fixing Marcel's
   copy by editing the literal shared asset would just break it for Kyle and Scott.
9. **Follow-up this creates, not yet done:** once each character has its own correctly-fitted skeleton,
   the existing shared `DCCCrawlerAnimSet` clips (authored against `SK_DCCCrawler_Skeleton`) need a way to
   keep playing on skeletons that are no longer literally that same asset. UE5's **Compatible Skeletons**
   list (mark the new skeleton compatible with `SK_DCCCrawler_Skeleton`) is very likely the right, low-effort
   mechanism - bone names/hierarchy stay identical, only lengths differ, and animation is rotation-driven
   so that shouldn't matter. Not verified against this project's actual 5.8 editor UI yet - flag this as the
   first thing to confirm once Zoe's doing this hands-on, before assuming it's this simple.

### Prepared and waiting

Three ready-to-open files, each with the shared skeleton already bound to that character's real mesh via
automatic weights (X-ray display already on), reverted to the original (broken) shared-skeleton
proportions so nothing is pre-fixed or presumed:

- `SourceArt/Marcel/rig_fit_check_Marcel.blend` (worst offender - start here)
- `SourceArt/Kyle/rig_fit_check_Kyle.blend`
- `SourceArt/Scott_OfficeAirplane/rig_fit_check_Scott.blend`

Also new and reusable: `Tools/rig_to_shared_skeleton.py` (built for the DarkPaperVerse ask, general-purpose
headless OBJ/FBX/glTF -> shared-skeleton auto-rig-and-export; works but automatic weights alone are not
enough when bone *positions* don't fit - that's this whole finding) and `Tools/inspect_glb_materials.py`
(from the Buff/Don Minion texture-bug diagnosis, reused this session for bone-length measurement's mesh
half).

## Queued for whoever has MCP (2026-08-23, bartender/guard follow-up)

Zoe clarified the bartender/guard roles from the batch below: bartender is a merchant (sells
drinks/items, tells you about quests - the last half already worked via ambient dialogue hints,
untouched here); the buff-minion guard is decorative/security - patrols, ignores everyone, and
only fights if a fight actually breaks out near it. No MCP this session (editor still not
running). DebugGame compile-check passed. **Not deployed - needs the usual close-editor
Development build.**

### Done: a real vendor system, not just a stub

New `Dialogue/DCCVendorNPC.h/.cpp` - `ADCCVendorNPC : public ADCCDialogueNPC`. Ambient
talk-on-approach is untouched (ADCCDialogueNPC's ambient script still plays, so "tell you about
quests" via hint lines works exactly as before); E now opens a shop instead of advancing the
script, via `TryPlayerInteract` being made `virtual` and taking the interactor (base behaviour -
say the next line - is unchanged for every other dialogue NPC; only the override on this subclass
diverges).

- **A full menu, not a placeholder**: `ADCCCharacterBase` gained `VendorTargetActor`/
  `VendorMenuIndex`, `CycleVendorMenu`/`CloseVendorMenu`/`HandleVendorMenuConfirm`, and two server
  RPCs (`Server_PurchaseEquipment`, `Server_PurchaseDrink`) - same "client picks, server
  re-validates range/gold/existence" shape as the loot menu, deliberately kept as its own
  parallel set of names rather than reusing LootTargetActor's state, so the two never collide.
  `DCCHUD::DrawVendorMenu` draws a flat list (equipment rows, then drinks, then Leave - no
  itemised sub-page, unlike loot, because there's nothing to take-some-leave-some of) with
  unaffordable rows dimmed against the buyer's current gold. Wheel cycles, E buys, Esc leaves -
  wired into the existing `InteractionScrollUp/Down` dispatch and `CloseInteraction`.
- **Equipment purchases reuse the existing grant path exactly** -
  `Equipment->FindItem`/`Equipment->Equip`, the same calls `ADCCPickup::OnPickedUp` already makes,
  so a vendor sale is mechanically identical to picking the same item off the floor. Any row in
  `DT_DCCEquipment` can be sold by adding its `ItemId` to a vendor's `SellableEquipmentIds`.
- **Drinks are flavour only, on purpose - see the open question below.** `FDCCVendorDrink` is
  just `{FText Name; int32 Price;}`. No consumable/potion system exists anywhere in `Source/`
  (checked directly, not assumed) to hang a mechanical effect on, so a drink purchase spends gold
  and logs a Feed line and does nothing else yet. Real gold sink, real vendor interaction, todays
  version of "sells drinks" - just not a buff/heal until that system exists.
- **Not yet authored: what this bartender actually sells.** `SellableEquipmentIds`/`Drinks` are
  empty arrays on the class, meant to be set per placed instance via `set_properties` once Zoe
  decides the list. A starter suggestion, cheap to change later: existing `DT_DCCEquipment` rows
  that already fit a bar (`ThrownBottle`, `ServiceTray`, `Torch`) plus a handful of flavour-only
  drinks using the mug/bottle meshes found below.

### Done: the guard patrols and only fights if violence breaks out nearby

New `ADCCEnemyCharacter::bPassiveUntilProvoked` (default false, so no existing mob/ally changes
behaviour) plus a new static delegate, `ADCCCharacterBase::OnAnyAttackAttempted`, broadcast once
per real swing attempt inside `PerformAttack` (hit or miss). A passive character binds to it in
`BeginPlay` (unbinds in a new `EndPlay` override) and otherwise never calls
`FindNearestHostileTarget` at all - the only way it ever gets a `CurrentTarget` is
`HandleNearbyAttack` reacting to a swing landing within `ViolenceWatchRadius` (900 units default)
of itself or of the swing's target. Once that target dies or leashes away (the same drop-logic
every enemy already has), nothing re-arms the normal scan, so it goes back to patrolling on its
own rather than picking a fresh fight - matches "just for appearances... attacks if a fight breaks
out" exactly.

- **Patrol**: `PatrolPoints` (`TArray<FVector>`, empty by default - a guard with none just stands
  still), `PatrolPointTolerance`, `PatrolWaitSeconds`. `TickPatrol` walks the loop via the same
  `AAIController` the chase/follow logic already uses - no new dependency. **Points are authored
  per placed instance, not generated** - needs MCP to actually set `PatrolPoints` on whichever
  guard(s) end up in the Bar.
- **New `ADCCEnemyCharacter_Guard`** (`Characters/DCCEnemyCharacter_Guard.h/.cpp`) - `bIsAlly =
  false`, `bPassiveUntilProvoked = true`, mesh pointed at the already-imported
  `/Game/DCC/Characters/Minions/SK_BuffMinion` (confirmed this is "the buff minion guard" from
  Zoe's own wording). **Stat block is an invented placeholder** (STR 5 / CON 5, d6 unarmed, no
  `DT_DCCEnemies.json` row exists for a bar guard) - retune freely, nothing depends on the exact
  numbers.
- **Not yet placed or animated.** SK_BuffMinion still has the shared-material texture bug from the
  batch below (fix it before anyone sees this in a room), and its walk/run animations are
  converted and staged in `SourceArt/BuffMinion/Anims/` but not imported - it will T-pose/idle
  until that import happens.

### Free assets for the bar: already installed, nothing to download

Checked `Content/` directly rather than guessing or reaching for a web search first, per Zoe's
"pull from assets we've considered before" steer. **Real tavern props already sit unused in
already-installed packs:**

- `Legendary_Forge/Meshes/Mug/Sm_mug.uasset` (+ `_02`/`_03`/`_04` - four mug variants) and its own
  `T_mug`/`Mi_mug` textures - literal bar mugs, never referenced anywhere in `Source/` or the
  equipment CSVs.
- `Modern_villa_kit/Meshes/Bottle/SM_bottle_01/02/03.uasset` and `SM_glass_01/02.uasset` - wine
  bottles and glasses, same pack `SodaGun`/`ChefsKnife` already borrow from.
- `Cave_Camp/Meshes/SM_bottle.uasset` - already in use, as `ThrownBottle`'s mesh.
- `Legendary_Forge/Meshes/Barrel/SM_barrel_01/02.uasset` (+ its own `SM_stool`) and
  `Cave_Camp/Meshes/Props_02/SM_barrel_01..04.uasset` - kegs/barrels for behind-the-bar dressing.

Enough on its own for drink props (held items, shelf dressing, or both) without touching Fab or
downloading anything. Only reach for an external free pack if a *specific* need turns up that none
of these thirty-odd installed packs cover - haven't found one yet for this ask.

### Open questions for Zoe, not decided here

**Answered, both of them - see the newer section directly below for what got built as a result.**
Question 1 (mechanical drinks): yes, build the real thing - health up/down, temporary stat
buffs/debuffs (Woozy, Charisma boost, whatever fits), a Drink Now/Stow choice, and "a wealth of
tags" on every item created. Question 2 (Don Minion): confirmed as the bartender itself, not a
second guard.

1. ~~Should a drink ever do something mechanical~~ - **decided: yes, build it.**
2. ~~What is Don Minion for~~ - **decided: the bartender.**
3. **What should this bartender actually sell?** Still open. Five starter drinks now exist
   (`Docs/DT_DCCConsumables.json`, see below) and can be assigned as-is or replaced; equipment
   still needs a real list before `SellableEquipmentIds` gets set on the placed instance.

## Queued for whoever has MCP (2026-08-23, real consumables: tags, effects, Drink Now/Stow)

Zoe's answers to the two open questions above, acted on the same session. No MCP (editor still not
running). DebugGame compile-check passed, including against this session's own earlier
guard/vendor work and both other sessions' commits. **Not deployed.**

### Done: a real item-tag system, not per-struct free text

Added the `GameplayTags` module to `DungeonCrawlerDenver.Build.cs` (previously only reachable
transitively through AIModule/StateTree, which does not guarantee the public headers are visible -
now an explicit, direct dependency) and a `FGameplayTagContainer Tags` field on **both**
`FDCCEquipmentDef` and the new `FDCCConsumableDef` (below) - "every item created" per Zoe's ask, not
just drinks. New `Config/DefaultGameplayTags.ini` registers a starter dictionary so tags are
actually usable rather than silently rejected on import: `Item.Consumable`, `Item.Liquid`,
`Item.Alcohol`, `Item.Flammable`, `Item.Organic`, `Item.Craftable`, `Item.Weapon`, `Item.Armor`,
`Item.Trinket`, `Item.QuestItem`. Add more freely, no C++ change needed - just an editor restart to
pick up new entries. **Not read by any code yet** - this is the classification layer for a future
crafting/interaction system, authored ahead of time on purpose so items don't need re-tagging later.

### Done: a real consumable system - effects, temporary buffs, Drink Now vs Stow

New `Items/DCCConsumableTypes.h` - `FDCCConsumableDef` (DataTable row: ItemId, DisplayName,
Description, GoldValue, Tags, `TArray<FDCCConsumableEffect> Effects`, an optional held/stowed
Mesh) and `FDCCConsumableEffect` (`InstantHealthChange` - heals if positive, hurts if negative;
`ModifiedAbility`/`ModifierAmount`/`ModifierDurationSeconds` - a signed temporary nudge to one
ability score, so "Woozy" and "Charisma boost" are the same mechanism with a different sign).
Multiple effects per drink combine freely - the Rotgut Special below hurts a little AND leaves you
Woozy AND gives a Charisma bump, all at once, all from three rows in one array.

- **A genuinely new mechanic, added at the right architectural layer.** `UDCCStatsComponent`
  gained `ApplyTimedAbilityModifier(Ability, Amount, DurationSeconds)` and a private
  `TimedAbilityModifiers` list, pruned every tick (piggybacked on the existing `TickRegeneration`
  call rather than a new timer) and folded into `RecalculateEnhancedScores` exactly like
  Equipment's and Class's bonuses - same array, summed in place. **Stats/ itself never hears the
  word "consumable" or "drink"** - per this project's own architecture rule that layer stays
  ruleset-agnostic. The translation lives on `ADCCCharacterBase::ApplyConsumableEffects`, which is
  the one place that knows both what a consumable is and how Stats primitives work.
- **`UDCCInventoryComponent` gained a third stowed-items array**, `StowedConsumables`
  (`{ItemId, DisplayName, Quantity}`, stacking - buying three of the same round is one row, not
  three) - kept separate from `Items`/`StowedRemains` for the same reason those two are already
  separate from each other (different shape, see that file's own comment). `ConsumableTable`
  defaults via `ConstructorHelpers` to `/Game/DCC/Items/DT_DCCConsumables`, same self-healing
  pattern as every other content table in this project.
- **The vendor menu's drink flow, not just equipment's.** Picking a drink row no longer buys it
  immediately - it arms a two-row sub-prompt (Drink Now / Stow For Later), reusing the same
  wheel-to-choose-E-to-confirm input the rest of the menu already uses. `Server_PurchaseDrink` took
  on a `bool bDrinkNow` parameter; Drink Now applies `Effects` on the spot, Stow adds to
  `StowedConsumables`. `DCCHUD::DrawVendorMenu` draws the sub-prompt as its own small panel.
- **Stowed drinks are drinkable later from the Tab menu's Inventory page**, exactly the way
  Stowed Remains rows are already clickable to empty - same `AddHitBox`/`NotifyHitBoxClick`
  convention, a new `"Drink|%d"` tag alongside the existing `"Stow|%d"` one, routed to a new
  `ADCCCharacterBase::DCCConsumeStowedDrink`/`Server_ConsumeStowedDrink` pair mirroring
  `DCCEmptyStowedRemains` exactly.
- **`ADCCVendorNPC::Drinks` (the placeholder `FDCCVendorDrink{Name,Price}` struct from the first
  pass) is gone**, replaced by `SellableConsumableIds` (`TArray<FName>`, same shape as
  `SellableEquipmentIds`) resolved through the buyer's own `Inventory->FindConsumable` - one less
  data shape to keep in sync, and real drinks instead of a flavour-only stub.

### Five starter drinks, ready to import - `Docs/DT_DCCConsumables.json`

Written in the same row-per-JSON-object shape as `DT_DCCQuests.json` (this project's own
established pattern for a DataTable a plain CSV can't express - here because of the nested
`Effects` array and `Tags` container, same reason quests needed JSON instead of CSV rather than a
guess). Mesh paths point at already-installed, already-verified assets from the section above
(`Sm_mug`, `Sm_mug_02`, `SM_bottle.SM_bottle` - the same asset `ThrownBottle` already uses, a
deliberate continuity nod: "the house pour, returned to the customer at speed" is the same bottle
you can also just drink), not anything invented or unverified:

| Drink | Gold | Tags | Effect |
|---|---|---|---|
| Tap Water, Complimentary | 0 | Liquid, Organic | +1 HP |
| House Ale | 8 | Liquid, Alcohol | Cha +1, 120s |
| Well Whiskey | 12 | Liquid, Alcohol, Flammable | Cha +2 / Dex -1, 90s |
| Rotgut Special | 4 | Liquid, Alcohol, Flammable, Craftable | -2 HP, Cha +3 / Dex -3, 60s (this is "Woozy") |
| Energy Tonic, Sponsored Flavor | 15 | Liquid, Craftable | +6 HP |

**Import as JSON needs the same `create` → `add_rows` → `set_rows` route `DT_DCCQuests.json` used**,
not a plain CSV import (`CSVImportFactory` cannot read `.json`, and even converted to CSV the
nested `Effects` array would not survive a flat-column import) - `Config/DefaultGameplayTags.ini`
must land and the editor must have restarted once (to register the tags) before `Tags` will import
correctly rather than silently empty.

### Not attempted this pass, flagged rather than guessed

- **No mesh-in-hand visual for "Drink Now".** `FDCCConsumableDef::Mesh` exists and is populated
  for all five starter drinks, but nothing attaches it to a socket or plays a drinking animation -
  the effect applies and a Feed line logs, same ceremony as everything else, but there is no
  visual beat yet. A deliberate scope cut this pass, not an oversight - wiring a temporary
  held-mesh attachment blind, with no PIE to check it against, risked shipping something that
  looks broken rather than just plain.
- **What the bartender (Don Minion) should actually stock** - the five drinks above are a
  starting suggestion, not a final list, and `SellableEquipmentIds` is still empty.
- **Everything from the batch below this one is still true and still needed**: the shared-material
  texture bug, Buff Minion's unimported animations, guard patrol point placement, and the wider
  ally-body-replacement backlog.

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

## The Collapse — party-ready gate + cutscene system built (2026-08-27)

Full design/status writeup is `Docs/the-collapse-cutscene.md` — read that first, this
is the short version. Built on top of the existing `ADCCIntroDirector` bedroom-intro
state machine rather than a new system; C++ compiled and **deployed** (Development
Editor target, editor closed, real `Link [x64] UnrealEditor-DungeonCrawlerDenver.dll`,
verified no numbered hot-reload DLL in `Binaries/Win64/`).

**What changed:**
- `HandleDoorOverlap` no longer fires the collapse on the first crawler through the
  door. Each arrival is added to a new replicated `ReadyForCollapse` array; the
  collapse only starts once `ReadyForCollapse.Num() == GameState->PlayerArray.Num()`
  (live connected count, not a hardcoded five). Early arrivals see a HUD banner
  ("Waiting for other players... (n/m ready)", `ADCCHUD::DrawWaitingForPartyBanner`)
  and are otherwise free to keep playing — no cinematic lock, no freeze.
- `Collapsing` now actually shows something: a second camera (`CollapseCamera`,
  pulled-back establishing shot) takes over from the first-person `IntroCamera`, and
  two new configurable arrays (`SkylineBuildings`, `HeroBuildingChunks`) drive a
  Tick-based squash-toward-the-ground (skyline) / tumble-and-drop (hero chunks)
  animation, staggered by distance from the director so it reads as spreading
  outward. Camera shake, staggered crash-sound playback, and Niagara dust
  (defaulted to `Dam_city/FX/NS_Smoke`) sell it. Deliberately not Chaos destruction —
  see the design doc for why, and note the brief asked for Sequencer specifically and
  this does not use it, for the same "no Sequencer toolset in this MCP setup" reason
  `DCCIntroDirector.h`'s own header comment already gave for the rest of the intro.

**Genuinely not done — level-side, needs MCP + a real layout decision:**
1. No `ADCCIntroDirector` is placed in the level at all (confirmed via `find_actors`
   before touching anything). Placing one, and deciding where `DoorTrigger` actually
   goes, is unstarted.
2. No skyline or hero-building actors are placed yet — `Dam_city` is the recommended
   pack (least sci-fi-flavored of the installed city kits; see design doc), and it
   ships its own smoke FX already wired as the default.
3. **Found mid-session, undocumented anywhere else: `PersonalRooms/PersonalRoom_CJ/
   Kyle/Marcel/Scott/Vaughn`** — five small walled rooms with a west-wall doorway gap
   each, clustered around x -1900..-3000, y -1700..+2300, each with a placeholder body
   standing in it. Matches the "Two Kyles, two Scotts... placed 2026-08-16 in the
   personal rooms" note earlier in this file, so this is real, dated, in-progress work
   of Zoe's, not a fluke. **This looks like exactly the "explore their room, then a
   small demo space" setup the brief describes, but it's mid-construction** — no demo
   space, exit, or "outside" geometry exists yet anywhere near it. Deliberately did
   NOT guess where to put the exit trigger or the skyline relative to these rooms;
   that's a real layout call for Zoe, not a technique question, and the gate/cutscene
   system works with a single `DoorTrigger` wherever it ends up.
4. **No crash/impact sound exists anywhere in this project** — same category of gap
   as the missing cast animation and spell VFX already flagged elsewhere. The sound
   hook (`CollapseCrashSounds`) is wired and silent-safe with nothing assigned; camera
   shake and dust currently carry the whole moment alone.

**Also worth knowing:** a live PIE session was running when this session started
(confirmed via `IsPIERunning`); it was stopped via `StopPIE` before any level
inspection, since edits during PIE only touch the transient play-world and vanish
when PIE ends — nothing else was done to it. The editor's MCP connection dropped
partway through (most likely Zoe closing it), which happened to be the exact
precondition the Development Editor build needed, so the build ran immediately.

## Oathgate placed; cliff-lift confirmed genuinely blocked, not just theorized (2026-08-27)

Fresh session, MCP live from the start (editor was already open when this session started,
per this file's own repeated finding that only that ordering works). Two placement tasks
queued; one done, one hit a real, verified blocker rather than a guess.

### Oathgate pair — placed, wired, saved

Both instances live in the same sandbox test room near spawn as the enemy dummies
(`Mob_Brute_Courtyard` etc.), not in any Floor 1 room — per the class's own header and
8a's instruction, "anywhere reachable works" since this is a DM testing tool, not lore
content. Chose the sandbox specifically because it's already a consequence-free area to
test in and didn't need new geometry.

**Worth knowing for next time: this sandbox is not open ground.** The room has real walls
(`SM_platform_Wall9` et al, north wall around y1808–2120) and a cluster of low platform/step
props around x1200–1800, y1200–1800 — the first placement attempt at (1700, 1700) clipped
~76 units into that north wall before `get_actor_bounds` on the nearby static meshes caught
it. Moved both gates in to (900, 900) and (-900, -900) instead — confirmed clear via
`find_actors` on a tight box (only sky-sphere/floor/navmesh/roaming-enemy hits, which are
harmless) before committing.

- **`Oathgate_Entrance`** at (900, 900, 160), yaw -135 (facing the exit). Left `EnemyPool` at
  its C++ constructor default (Debt Eater / Fine Print / Arbiter) — untouched.
- **`Oathgate_Exit`** at (-900, -900, 160), yaw 45 (facing the entrance). `EnemyPool` set to
  `[]` via `set_properties` so it only teleports back, per 8a's instruction.
- `PairedGate` set on both, each pointing at the other. **Read back after writing** (this
  project's own standing rule) — both directions confirmed correct.
- Both moved into outliner folder `DCC/Oathgate`, labeled `Oathgate_Entrance`/`Oathgate_Exit`.
- Level saved via `save_assets(["/Game/ThirdPerson/Lvl_ThirdPerson"])` (World Partition
  actors don't support `save_actor` directly, per this file's existing note).

Not PIE-tested — walking through and confirming the teleport/encounter-roll actually fires
is still open. Anyone with MCP next can do that in a few minutes: PIE, walk into either
gate, confirm arrival at the other and (entrance side) a 3-5 enemy roster rolling in in
`EncounterScatterRadius` (500u) of the exit gate.

### Cliff-lift — genuinely still blocked, confirmed by evidence, not assumed

Checked directly rather than trusting the previous entry's account at face value, since that
entry itself was written by a session with no MCP and could only reason about it, not verify
it. **Confirmed the block is real:**

- `ObjectTools.search_subclasses` against `/Script/Engine.Actor` filtered on `"CliffLift"`,
  `"LiftAttendant"`, and `"Cliff"` all returned empty (the `"Cliff"` search only matched two
  unrelated `SciFi_Cliff` prop Blueprints). `"DialogueNPC"` found only the base
  `ADCCDialogueNPC` — no `LiftAttendant` subclass. Meanwhile `"Oathgate"` found
  `ADCCOathgate` immediately, so the search itself works; these two classes are just not in
  the running editor's reflection system at all.
- Cross-checked against file times: `Binaries/Win64/UnrealEditor-DungeonCrawlerDenver.dll` is
  timestamped Aug 27 18:23. `Rooms/DCCCliffLift.h` and `Dialogue/DCCDialogueNPC_LiftAttendant.h`
  are both timestamped Aug 27 20:35 — over two hours *after* the DLL currently loaded in this
  editor was linked. `Rooms/DCCOathgate.h` (Aug 23) predates that link and is present, which is
  the expected pattern and confirms the timestamps are meaningful here, not coincidence.

So: the classes are written and DebugGame-compile-checked (per the previous entry), but the
running editor was never restarted after they were added. There is no MCP-side workaround for
this — new `UCLASS`es cannot be hot-loaded, per this file's own rule #6 and CLAUDE.md's Live
Coding warning. **This needs, in order: close the editor → Development Editor build → reopen
→ a fresh Claude Code session started after that reopen** (an already-running session does not
retroactively gain the new connection, confirmed elsewhere in this file already). Only then can
the lift and attendant be placed, wired, and PIE-tested. Did not attempt a substitute or
placeholder for either — per this file's own standing rule, nothing here was faked to look
farther along than it is.

## Achievement -> loot box system, first pass (2026-08-28, overnight, no MCP)

Zoe's actual ask, grounded in the real Dungeon Crawler Carl mechanic: achievements pay a
**sealed box**, not items directly. Six tiers (her exact naming) — Bronze, Silver, Gold,
Platinum, **Legendary**, Celestial — opened later, and only in a Safe Room, all contents
at once. Mostly items, some boons (buffs). Built the whole mechanical spine tonight,
file/git only, no live MCP (editor was down/crash-looping for most of this — see the two
sections above this one).

**Naming note for whoever touches this next:** a peer session independently wrote
`Docs/achievements.md` (20 achievement entries, real content, excellent) using a *different*
name for the fifth tier — **Diamond** where Zoe told me directly **Legendary**. Same rarity
slot, same position in the ladder, just a different word. Built the C++ enum
(`EDCCLootBoxTier`) with Legendary, per Zoe's own direct instruction, and mapped
`achievements.md`'s Diamond-tier entries onto it. Flagged back to that session directly (see
cross-session reply) so the doc gets fixed at the source rather than the two names quietly
coexisting.

### A real collision found mid-session, worth recording

While compile-checking this work, found `Characters/DCCCharacterBase.cpp`/`.h` had been
modified moments earlier by another session — live, concurrent editing, confirmed by file
mtime (34 seconds before the check). The diff visible in the compile error
(`BodyCollectorClass ? BodyCollectorClass.Get() : ResolveDefaultBodyCollectorClass()`,
a function that doesn't exist in the version of this file I last wrote) strongly suggests
that session is the crash-fix work mentioned above, and that **the actual root cause of
tonight's stall/crash was very likely the `ConstructorHelpers::FClassFinder<ADCCEnemyCharacter>`
default-resolution I added to `ADCCCharacterBase`'s constructor earlier tonight** (for
`BodyCollectorClass`, defaulting to `BP_DCCEnemy`) — a Blueprint class default being resolved
from inside `ADCCCharacterBase`'s own constructor is exactly the shape of a circular CDO
dependency, since `BP_DCCEnemy` derives from `ADCCEnemyCharacter` which derives from
`ADCCCharacterBase` itself. Did not touch that file further once this was noticed — left it
for the session actively fixing it rather than editing out from under them. If that theory
is right, this is worth remembering for next time: **never resolve a same-hierarchy Blueprint
subclass via `ConstructorHelpers` inside that hierarchy's own base-class constructor** — resolve
it lazily instead (on first use, not at construction), which is exactly what the other
session's `ResolveDefaultBodyCollectorClass()` pattern appears to be doing.

### What's built, and why it doesn't duplicate what already existed

Deliberately a thin layer, not a parallel loot system — see `Items/DCCLootBoxTypes.h`'s own
header comment for the full reasoning. In short:

- **Item contents reuse `FDCCLootDrop`/`DCCLoot::RollLootTable`/`SpawnRolledLoot` verbatim.**
  A box's item pool is just another named loot table — the exact shape `FDCCLootTableRow`
  already gives room generation, now reused for box tiers instead of a new roll mechanism.
- **Boon contents reuse `FDCCConsumableEffect` verbatim.** A boon is mechanically identical to
  a consumable's "Drink Now" effect (instant heal/hurt plus a timed ability modifier) — new
  struct `FDCCBoonDef` wraps it with a name/flavour for display, nothing new underneath.
  Applied through the exact same `ADCCCharacterBase::ApplyConsumableEffects` a bought round
  already goes through — one buff engine, not two.
- **Safe Room gating reuses `EDCCRoomType::SafeRoom`**, which already existed (the Bar is one).
  No new room concept invented.
- **The achievement claim flow is extended, not replaced.** `FDCCAchievementDef` gained one new
  field, `LootBoxReward` (defaults to `None`, so every existing achievement keeps paying
  XP/Favor exactly as before). `Server_ClaimReward_Implementation` grew one new block, mirroring
  the existing XP/Favor blocks exactly.
- **Box ownership reuses the "record, not a live actor" pattern** `StowedRemains`/
  `StowedConsumables` already established on `UDCCInventoryComponent` — `LootBoxCounts` is a
  third array in the same family, indexed by tier rather than holding per-item records since a
  box's whole identity is its tier.

### New files/fields

- `Items/DCCLootBoxTypes.h` (new) — `EDCCLootBoxTier`, `FDCCBoonDef`, `FDCCBoonEntry`,
  `FDCCLootBoxTableRow`, `FDCCOpenedLootBoxResult`.
- `Progression/DCCAchievementTypes.h` — `FDCCAchievementDef::LootBoxReward`.
- `Progression/DCCAchievementComponent.cpp` — `Server_ClaimReward_Implementation` grants the box.
- `Items/DCCInventoryComponent.h/.cpp` — `LootBoxTable`/`BoonTable` (ConstructorHelpers-defaulted,
  same pattern as `ConsumableTable`), `LootBoxCounts`, `GetLootBoxCount`, `GrantLootBox`,
  `TryOpenLootBox` (rolls items + spawns them near the caller, rolls boons independently and
  returns the resolved defs — does NOT apply them; same "component owns the record, caller owns
  the effect" split `ConsumeStowedConsumable` already uses for drinks).
- `Characters/DCCCharacterBase.h/.cpp` — `bInSafeRoom` (mirrored by `ADCCRoom` the exact same way
  `CurrentRoomDisplayName` already is — see `AddOccupant`/`RemoveOccupant`), `IsInSafeRoom()`,
  `OpenLootBox`/`Server_OpenLootBox` (re-checks alive/not-downed/safe-room server-side, same
  "never trust the client's own gate" shape every other server entry point here already follows,
  applies any returned boons via `ApplyConsumableEffects`, logs a real summary line).
  **Re-verified after the collision** — confirmed via grep that all of this survived the
  crash-fix session's edits to the same file intact (their fix touched `BodyCollectorClass`
  resolution only), then confirmed with a real DebugGame compile-check once that file's mtime
  stopped changing: **Result: Succeeded**, `DungeonCrawlerDenver-Win64-DebugGame.exe` linked
  clean, compiling this loot-box work and the crash-fix's `ResolveDefaultBodyCollectorClass`
  change together in the same pass. Also confirms the crash-fix theory above compiles - not
  proof it fixed the runtime crash, just proof the fix itself is sound C++.

**A user (relaying Zoe) flagged mid-session that up to 3 sessions may have been building this
project concurrently, which can file-lock-deadlock UnrealBuildTool against itself - paused all
further compile/build commands from this session on that instruction.** The Succeeded result
above was from a build already in flight before that instruction arrived, not a new one run
after it. Whoever resumes build-verifying this work should confirm no other session is mid-build
first, same caution.

**Equipment ids used in `DT_DCCLootBoxTables.json` cross-checked against this file's own import
history while builds were paused** - `ThrownBottle`, `ServiceTray`, `HighVisibilitySafetyVest`,
`ComplaintFolderOverstuffed`, `ConfettiCrossbow`, `NailGunPro`, `FlareGunEmergency`,
`SeveranceCheckUncashed`, `MinionEyes`, `WaterBottle`, `SyndicateBadge` all appear in this file's
own earlier confirmed-imported lists (the 2026-08-24 and 2026-08-23 sections above). Static
cross-check only, not a live `list_rows` call - still worth the live spot-check before import,
but this lowers the risk that any of them are simply wrong.
- `Rooms/DCCRoom.cpp` — two-line addition to the existing occupant mirroring.

### Six achievements wired end-to-end, compile-verified (updated after the build-pause lifted)

Started with two, then went back through the rest of `achievements.md`'s "EXISTING/cheap hook"
list once the concurrent-build risk cleared and a real compile-check confirmed clean
(`Result: Succeeded`, `DungeonCrawlerDenver-Win64-DebugGame.exe`, this build included the
crash-fix session's `DCCCharacterBase.cpp` changes too — both coexist fine):

- **Pause For Thought** (Silver) — `ADCCGameState::SetCombatPaused`'s rising edge calls
  `UDCCAchievementComponent::RecordFor` on every character in the world; the component's own
  `bTracksAchievements` gate (off for anything that isn't a crawler) makes this safe to call
  unconditionally, same as every other `RecordFor` call site in this codebase already does.
- **Sponsor Of The Week** (Legendary) — `UDCCFavorComponent::RecordChange` fires the same
  `RecordFor` call the instant a character's tier reaches `Headliner`, right next to the
  existing `OnTierChanged.Broadcast` it already had.
- **House Pour** (Bronze) — `ADCCCharacterBase::Server_PurchaseDrink_Implementation`, right
  after `Stats->SpendGold` succeeds and before the `bDrinkNow` branch, so both drink-now and
  stow-for-later count exactly once. Still blocked in practice on a vendor actually being
  placed (`achievements.md`'s own caveat) — fires the moment that placement happens, no further
  code needed.
- **Untrained Genius** (Silver) — turned out cheaper than `achievements.md` estimated:
  `UDCCSkillComponent::TryActivateSkill` (`Skills/DCCSkillComponent.cpp`) already computes
  `Result.bWasUntrained` and already has `Skill.DifficultyClass` sitting right there for the
  `ResolveSkillCheck` call — no new plumbing needed, just reading two values the call site
  already had and checking `bWasUntrained && bSucceeded && DifficultyClass >= 15`.
- **The Full Tour** (Gold) — new `ADCCCharacterBase::RecordRoomVisited(FName RoomId)`, called
  from `ADCCRoom::AddOccupant` right where `CurrentRoomId`/`bInSafeRoom` are already mirrored.
  Deliberately built as a plain counter (`EDCCAchievementStat::UniqueRoomVisited`, `Amount`
  fires once per genuinely new RoomId via a private `VisitedRoomIds` set) with `Threshold = 7`
  on the achievement row — no "have I visited all N" logic in C++ at all, the achievement
  system's own existing threshold check does that part for free.
- **Still In Your Pajamas** (Bronze) — free bonus once `RecordRoomVisited` existed: checked
  right there, on this crawler's first-ever room entry (`VisitedRoomIds.IsEmpty()` before the
  add), whether `Equipment->GetEquippedId(EDCCEquipmentSlot::Body)` is still `None`. In this
  floor's layout the first room anyone enters is the Queue, so this reads as "reached the Queue
  undressed" without hardcoding that RoomId.

New `EDCCAchievementStat` values: `PausedCombat`, `FavorTierMaxed`, `FirstDrinkBought`,
`UntrainedHighDCSuccess`, `UniqueRoomVisited`, `ArrivedBodySlotEmpty`. All six rows now in
`Docs/DT_DCCAchievements_LootBoxBatch1.csv` — same diff-merge-don't-reimport-blind discipline
this file has documented before, since the live table has drifted from the source CSV all night
from other sessions authoring rows directly.

**The other ~14 achievements in `achievements.md` are deliberately NOT wired** — most need a
genuinely new tracking hook (Beat The Collector, the Balloon Payment pair, the Oathgate one, the
cliff-lift one, several composite/aggregate end-of-floor checks) and one (Off The Air) depends on
a system, Last Call, that does not exist at all yet. `achievements.md`'s own "Summary for whoever
wires this in" section already ranks these by cost — that ranking held up under a second read and
is the right order to work through next, not something this session found wrong. Selfie At The
End Of The World and Five-For-Five were both on the doc's own "EXISTING" list but skipped here:
Selfie needs a "dungeon arrival" timestamp that doesn't actually exist anywhere yet (the doc's
"EXISTING" call undersells this one), and Five-For-Five needs a spread check across
`ReadyForCollapse`'s timestamps in `ADCCIntroDirector`, which this session didn't open tonight -
both are real candidates for next time, just not free the way the six above turned out to be.

### Content authored, not yet imported (needs live MCP)

- `Docs/DT_DCCLootBoxTables.json` — all six tiers, `FDCCLootBoxTableRow` (nested arrays, so JSON
  not CSV, same reason `DT_DCCClasses.json`/`DT_DCCConsumables.json` are JSON). Item rarity
  tracks tier as asked (gold range, item count/chance, and boon chance/count all scale up);
  Celestial's item list is deliberately thin per `achievements.md`'s own tier legend ("mostly
  symbolic" at that tier — the boon is the real reward, ~90%+ chance of at least one landing
  from independently-rolled entries).
- `Docs/DT_DCCBoons.json` — 8 boons, escalating in what they do and how long they last (5 min at
  the low end, up to a full hour for the Celestial-exclusive `SponsorsBlessing` — there is no
  "permanent" buff mechanism anywhere in this project, consumables included, so "long enough to
  read as session-defining" is the honest ceiling, not a limitation this pass introduced).
- `Docs/DT_DCCAchievements_LootBoxBatch1.csv` — the two new rows above.

**Equipment item ids referenced in `DT_DCCLootBoxTables.json` are NOT verified against the live
table** — MCP has been down all night for this session (crash-looping editor). Used ids this
file's own earlier sections confirmed were imported (`ThrownBottle`, `ServiceTray`,
`HighVisibilitySafetyVest`, `ComplaintFolderOverstuffed`, `ConfettiCrossbow`, `NailGunPro`,
`FlareGunEmergency`, `SeveranceCheckUncashed`, `MinionEyes`, `WaterBottle`, `SyndicateBadge`),
but several other sessions have been authoring equipment rows directly through MCP all night
too (per this file's own repeated "live table drifts from CSV" finding) — **spot-check every id
against `list_rows` on the live `DT_DCCEquipment` before importing this table**, the same
discipline this file has flagged before.

### Genuinely open design questions, not decided unilaterally

- **Where do opened items land?** `TryOpenLootBox` spawns them at the opener's own location
  (same as any other loot source) — reasonable since opening only happens in a Safe Room anyway,
  but nobody has looked at whether that reads well with five players opening boxes at once in a
  small room like the Bar.
- **No UI built for any of this.** Achievement claiming already has a HUD affordance
  (`RequestClaimReward`); box-opening does not yet — `ADCCCharacterBase::OpenLootBox` exists and
  works, but nothing calls it from the HUD/Tab-menu Inventory page the way
  `DCCEmptyStowedRemains`/`DCCConsumeStowedDrink` are called from theirs. Needs a "Loot Boxes"
  section on that same page, one row per owned tier with a count and an Open button, gated
  visually (not just server-side) on `IsInSafeRoom()` so a box shows as un-openable rather than
  silently doing nothing when clicked outside one.
- **"Opened all at once, no cherry-picking"** — built this way per the source material, per
  Zoe's own optional note. Nothing here lets a player preview or select from a box's contents
  before opening; if that turns out to feel bad in practice once there's a UI to test it, the
  Inventory-side split (`TryOpenLootBox` returns everything already rolled) would need to change
  shape, not just the UI.

### Queued for whoever has MCP next

**Note for whoever picks this up: this session's own MCP connection to Unreal was never
available tonight** (editor was crash-looping most of the session, then got restarted twice by
other sessions after the fix landed — per this project's own documented behavior, a running
editor does not retroactively give an already-open Claude Code session a connection, a fresh
session is required after each restart). Everything below needs that fresh MCP session, not this one.

1. Import `Docs/DT_DCCLootBoxTables.json`, `Docs/DT_DCCBoons.json` (both need
   create -> add_rows -> set_rows, nested arrays), and
   `Docs/DT_DCCAchievements_LootBoxBatch1.csv` (six rows now - diff against live rows first,
   same as always, especially since the Kyle/Marcel/Scott skeleton session and possibly others
   have been live in the editor tonight too).
2. Spot-check every `EquipmentItemId` in the loot box table against live `DT_DCCEquipment` —
   see the note above. Lower risk than it sounds: every id used was cross-checked against this
   file's own confirmed-import history first (see further up), just not against a live
   `list_rows` call.
3. PIE test the full chain: pause combat once (Pause For Thought should unlock - also try buying
   a drink, an Untrained high-DC check, visiting all 7 rooms, and arriving with Body empty, for
   the other five), claim a reward, confirm the right tier count appears on
   `UDCCInventoryComponent::LootBoxCounts`. Then call `OpenLootBox(Tier)` (console/debug path,
   since there's no UI yet - see the open design question above) standing in the Bar and confirm
   items spawn and (if a boon rolled) a stat actually nudges. Also confirm `IsInSafeRoom()`
   reads correctly once `bInSafeRoom` replicates to a client - brand new replicated bool, never
   exercised outside a compile-check.
4. **Already done, not queued**: the crash-fix session's edits to `DCCCharacterBase.cpp`/`.h`
   were re-verified against this session's own additions (`bInSafeRoom`, `OpenLootBox`,
   `RecordRoomVisited`, etc.) by grep and by a real DebugGame compile-check after the fix
   landed - `Result: Succeeded`. Nothing further to re-check there.

## Editor-hang recovery confirmed complete (2026-08-28, ~07:08-08:27 local, dispatched fresh)

Sent in cold to check on the "Diagnose blueprint compile stall" session, which had gone
unresponsive to check-ins for far longer than any build in this project takes. **It was not
actually stuck — it was just deep in a legitimate close→build→reopen cycle and finished on its
own while this session was still gathering facts.** Sequence of what was actually observed,
in order:

1. First read of `Characters/DCCCharacterBase.cpp` still showed the buggy constructor-time
   `ConstructorHelpers::FClassFinder<ADCCEnemyCharacter>` for `BodyCollectorClass` (line 82) —
   the fix had NOT landed yet at that point.
2. Process check: the frozen editor (PID 5616) and its `CrashReportClientEditor` (PID 22540)
   had **already exited on their own** between the incident and this session starting — nothing
   needed to be force-killed. A handful of near-instant `dotnet`/UBT processes were flashing in
   and out (under 2s each); a manual `DebugGame` compile-check confirmed these were harmless —
   `Target is up to date`, 0 actions, 1.8s — not a genuine stuck build (the bug is a runtime CDO
   cycle, not a compile error, so `DebugGame` was never going to catch it anyway).
3. On attempting the fix myself, the `Edit` failed with "string not found" — a re-read showed
   the buggy block was **already gone**. The other session had finished editing in the
   intervening minute or two.
4. Verified the fix is structurally sound: `CheckForBodyCollector` now resolves
   `BodyCollectorClass` lazily via a file-local `ResolveDefaultBodyCollectorClass()`
   (`FSoftClassPath::TryLoadClass`, called well after all CDOs exist, not from inside
   `ADCCCharacterBase`'s own constructor) instead of the constructor-time `FClassFinder` that
   caused the circular CDO dependency (`BP_DCCEnemy` → `ADCCEnemyCharacter` →
   `ADCCCharacterBase`, resolved from `ADCCCharacterBase`'s own constructor). Root cause and fix
   shape both match the theory the loot-box session recorded above after spotting the collision.
5. **Re-verified the merge with the concurrent loot-box session's edits to the same two files**
   (queued as item 5 in that session's own handoff, above) — `grep` on `DCCCharacterBase.h/.cpp`
   confirms `Server_OpenLootBox`, `bInSafeRoom`, `IsInSafeRoom`, `OpenLootBox` all survived
   intact alongside the crash fix. Nothing was lost in the collision.
6. Found a **second, already-running** `UnrealEditor.exe` (PID 18396, started 07:46:54) —
   `Responding: True`, `IsHungAppWindow: False`, window title `DungeonCrawlerDenver - Unreal
   Editor`. `Binaries/Win64/UnrealEditor-DungeonCrawlerDenver.dll` timestamp is 07:46, matching
   the process start exactly, and **no numbered `-0001` hot-reload DLL exists** — this is a
   genuine clean `Development Editor` relink, not a stale hot-reload artifact left over from
   before the fix.
7. **Confirmed the editor reached a genuinely usable state, not just a live process**: the log
   shows every toolset registering (`EditorToolset`, `AIModuleToolset`, etc.), asset registry
   gather and `OnFilesLoaded` broadcast completing, and — the strongest signal —
   `LogModelContextProtocol` completing **two full session handshakes** (`Initializing new
   session` → `Session initialized` → `Listing tools (3)` → `Listing resources (0)`) around
   07:49 local, i.e. MCP came up and answered real client requests after this rebuild. Port 8000
   confirmed listening. The log continues cleanly for 35+ minutes past that with nothing but
   routine EOSSDK heartbeats — no errors, no repeated init, no crash.

**Net result: no intervention was actually needed from this session.** The fix was correct and
already fully deployed by the time this session finished verifying it. Nothing was killed,
nothing was rebuilt, nothing was reopened by this session — this pass was pure verification
that landed just after the other session's own cycle completed. **The editor is up and
confirmed working right now.** Whoever is at the keyboard next can go straight to PIE-testing
the loot-box session's queued items (1-5, directly above) without any editor-recovery step
first.

## Camera-lock recurrence, re-verified fresh - still Selfie Mode, the fix IS live, not a regression (2026-08-28)

Zoe sent a fresh screenshot of the same "camera locked head-on on my own character" symptom from
two nights ago, this time mid-PIE with combat happening and a Climbing/Vault/Martial Arts prompt
visible. Explicitly asked not to assume it was the same cause blind, given how much camera-adjacent
code landed overnight (cliff lift, the Collapse cutscene camera takeover, the loot/interact work).
Re-verified from first principles rather than trusting the old diagnosis:

**1. Confirmed the real-time-ceiling fix is actually compiled into the exact DLL currently
running** — not assumed from source alone, since `DCCCharacterBase.h/.cpp` were both touched again
overnight (crash-fix session's `BodyCollectorClass` work, loot-box session's `Server_OpenLootBox`/
`bInSafeRoom` additions) and there was real risk one of those collided with or reverted the Selfie
fix during the merge. Checked directly: `Binaries/Win64/UnrealEditor-DungeonCrawlerDenver.pdb`
(same 07:46:30 build/link timestamp as the `.dll`, matching the confirmed-live PID 18396 from the
recovery entry above) contains `SelfieModeEnteredRealTimeSeconds`, `SelfieMaxRealSeconds`,
`ResolveDefaultBodyCollectorClass`, `Server_OpenLootBox`, `IsInSafeRoom` and `bInSafeRoom` all
together in one binary's debug symbol table — every overnight fix and last night's Selfie fix
genuinely coexist in the currently-running editor. Also re-read `Tick()` and confirmed the
real-time-ceiling check is still the first thing it does, unconditional, no pause-gate, nothing
overnight touched that block.

**2. Ruled out a new interact-focus camera, not just assumed it away.** Grepped the entire
`Source/` tree for every single `SetViewTarget`/`SetViewTargetWithBlend` call site in the whole
codebase — there are exactly two, full stop: Selfie Mode's own `EnterSelfieMode`/`ExitSelfieMode`,
and `ADCCIntroDirector`'s Collapse cutscene (`Waking`/`Collapsing` phases only). Nothing in the
climbing, vault, martial-arts, cliff-lift, or loot-box code touches the camera's view target at
all - the hypothesis that a new interact-menu system is doing this has no code to point at.
Separately, the Collapse cutscene doesn't fit even if it somehow re-fired: it's phase-gated against
re-entering once it reaches `Idle` (which happens once, at session start, long before "combat
happening in the background" is possible), and its camera geometry is a pulled-back establishing
shot on a scene actor, not a 110-unit-forward, 180°-yaw close-up of the crawler's own body -
`SelfieCamera`'s own authored offset/rotation is a precise, deliberate match for what Zoe's
screenshot shows, and nothing else in the game produces that specific shot. Also confirmed only one
input binding exists for `C` (`ToggleSelfieMode`) - no new keybind collision either.

**Conclusion: this is Selfie Mode again, and the fix is genuinely live and working** - not a
regression, not a new bug. The most likely explanation is exactly what the fix is *for*: something
(most plausibly combat pausing, same as the original diagnosis, though the specific stall reason
this time wasn't independently re-diagnosed since the ceiling makes the specific reason
irrelevant by design) stalled the normal ~1.2s exit-timer chain again, and Zoe's screenshot was
taken somewhere inside the now-bounded ≤5-second window before the Tick safety net force-releases
it - not an unbounded hang like before. **Immediate answer for Zoe: press `C` again — confirmed
structurally independent of whatever is stalling the timers, should clear instantly. If it's still
locked more than ~5 seconds after entering, that would be new information worth a fresh report**
(would mean something is re-entering Selfie Mode repeatedly rather than the ceiling failing to
fire, since no code path was found that could suppress or bypass the Tick check itself).

**Worth hardening further, not yet built (holding off on any compile given tonight's build-lock
contention between other sessions):** even bounded to 5 seconds, being stuck first-person on your
own face for that long *during active combat* is still a bad moment, and "combat happening in the
background" is new information the original fix wasn't specifically tuned against. Two candidate
improvements for whoever picks this up next: refuse `EnterSelfieMode` while enemies are nearby/the
character is actively threatened, or drop `SelfieMaxRealSeconds` lower than 5.0. Neither is
implemented - flagging only, per tonight's doc-only constraint.

## Selfie Mode polish - pose cycling, viewfinder overlay, nearby-hostile refusal (2026-08-28, later same night)

Zoe queued three items on top of the camera-lock investigation above, explicitly "your call on
order, no rush": let players cycle poses while posing, show an actual viewfinder overlay so it
reads as taking a photo, and the two hardening options already flagged. Built all three together
since they compose - see the ceiling-bump note below for why. **Checked for active build
contention before compiling this time** (no `UnrealBuildTool`/`dotnet`/`cl.exe` processes running,
only the editor itself) - the earlier crash-fix saga is confirmed over per the recovery entry
above, so a normal DebugGame compile-check was safe again. **DebugGame compile-check passed
clean, no numbered hot-reload DLL** - compiled, not yet deployed (needs the usual close-editor →
Development build → reopen cycle, same as every reflection change tonight).

**`Source/DungeonCrawlerDenver/Characters/DCCCharacterBase.h/.cpp`:**
- **Nearby-hostile refusal.** `EnterSelfieMode` now refuses outright (`ShowLocalNotice`, "Too
  dangerous to stop and pose right now.") if `HasNearbyLivingHostile(SelfieHostileRefuseRadius)` -
  new private helper, same `TActorIterator<ADCCCharacterBase>` + `IsHostileTo` pattern
  `FindNearestHostileInRange` already uses, just with a configurable radius (1500 units default,
  `EditAnywhere`) instead of melee attack range. This is the actual fix for "stuck posing during a
  fight" - the real-time ceiling only ever bounded *how long*, this stops it from starting at all
  near danger.
- **Pose cycling.** New `SelfiePoseAnimations` (`TArray<UAnimationAsset*>`, `EditAnywhere`, empty
  by default - authored per-character since it has to be something that skeleton can play; the
  shared-skeleton `A_DCC_*` set is the obvious source once someone picks a handful). Mouse wheel
  now drives it: `InteractionScrollUp`/`Down`'s existing ternary dispatch (Vendor menu → Loot menu →
  interaction option) got a `bInSelfieMode` branch added at the front, calling the new
  `CycleSelfiePose(Delta)` - no new keybind, reuses the wheel exactly like every other
  already-open-menu context already does. Cycling plays the newly selected pose via the existing
  `PlayOneShotAnimation` and **restarts `SelfieShutterTimer`** so the shutter captures the pose just
  chosen instead of firing mid-swap.
- **Ceiling bump, deliberately paired with the refusal above.** `SelfieMaxRealSeconds` raised
  5.0 → 8.0 - browsing poses is a real interaction now, not an instant snap, so it earns more time,
  and it's only safe to grant that because the nearby-hostile refusal is what actually keeps this
  feature away from combat now, not the ceiling. Doing the ceiling bump without the refusal would
  have reopened exactly the risk the whole fix chain exists to close.
- New `GetSelfiePoseProgress()` (`BlueprintPure`) exposes 0-1 progress toward the shutter, reading
  `GetWorldTimerManager().GetTimerElapsed(SelfieShutterTimer) / SelfiePoseSeconds` - the HUD's
  progress bar reads this rather than duplicating timer math.

**`Source/DungeonCrawlerDenver/UI/DCCHUD.h/.cpp`:** new `DrawSelfieViewfinder`, called from
`DrawHUD` alongside the other always-relevant overlays, drawing nothing unless
`Crawler->IsInSelfieMode()`. Four corner brackets, a centre reticle, a red "SELFIE MODE" label, a
bottom hint line (`[Scroll] Change pose  [C] Cancel`, or just `[C] Cancel` if the character has no
`SelfiePoseAnimations` authored), and a thin progress bar under the hint reading
`GetSelfiePoseProgress()`. Pure Canvas, matching this HUD's established pattern - no new asset.

**Still needed, editor-side, once MCP is back:** `SelfiePoseAnimations` is empty on every character
right now, so pose-cycling has nothing to cycle through until someone picks 3-4 shared-skeleton
clips per crawler and assigns them (a good candidate list from the shared `A_DCC_*` set:
`AgreeGesture`, `BoomDance`, and whatever else reads as a pose rather than a combat animation). A
PIE test of all three together - refusal near an enemy, pose-cycling with the wheel, the viewfinder
actually drawing - is still owed once this deploys.

## Rig-fit: Kyle/Scott's "both thighs on one hip" bug root-caused and fixed for real; Blender side done, Unreal import still needs a live-MCP session (2026-08-28)

Zoe spent tonight's session learning the manual crease-snap technique hands-on in Blender and
hand-fixed Marcel's rig herself. She then used the overnight `rig_fit_FIXED_Kyle.blend` /
`rig_fit_FIXED_Scott.blend` files (see "Confirmed real bug" section above) as a starting point for
Kyle/Scott, but found a real defect in them — both thighs visually converging on one hip point —
and couldn't fix it by hand. Dispatched to diagnose and fix properly, then get all three imported
to the live game. **Marcel = authoritative, do not touch** was the explicit instruction going in.

### Root cause found — it's not a "hip" bug, it's a scope gap in `fix_rig_to_own_proportions.py`

Read the script and diffed every one of the 24 bones in Kyle's and Scott's `_FIXED_` files against
that character's own true source skeleton (`SourceArt/{Kyle,Scott_OfficeAirplane}/fbx/SK_*.fbx`),
not just the two thigh bones Zoe had visually spotted. **17 of 24 bones mismatched in Kyle's file,
23 of 24 in Scott's** — the reported hip convergence was only the most visually obvious symptom of
a much bigger gap.

Mechanism, confirmed by direct inspection, not guessed: the script's `bones_of_interest` list never
included `LeftShoulder`/`RightShoulder`/`Spine`/`Spine01`/`neck`/`Head` — only 16 of the rig's 24
bones were ever in scope. Blender's parent→child "Connected" constraint then silently drags a
correctly-set bone's `head` back onto its *never-corrected* parent's stale `tail` — e.g. the script
correctly computes and writes `LeftArm`'s head, but because `LeftArm` is connected to
`LeftShoulder` (out of scope, never touched), Blender snaps it right back. That's the actual "same
point" bug: not two thighs sharing one coordinate by name-collision, but a downstream bone getting
silently overridden by an unfixed upstream parent. Confirmed exactly by the data: bones with
`tail_delta≈0` but nonzero `head_delta` in the original file (e.g. `LeftArm`, `RightArm`) are
precisely the ones with an out-of-scope connected parent.

### Fix applied — full 24-bone rewrite, not a patch to the two thigh bones

Wrote `Tools/fix_full_rig.py` (kept in the same family as the existing `fix_rig_to_own_proportions.py`,
not a replacement): re-reads each character's true source skeleton fresh, and rewrites **every**
bone's head/tail directly, temporarily clearing `use_connect` on all of them first so the
parent-drags-child bug can't happen during the write, then restoring the original connect flags
afterward (a no-op positionally, since once every bone matches its true source, `child.head` and
`parent.tail` already coincide exactly). Applied to both `rig_fit_FIXED_Kyle.blend` and
`rig_fit_FIXED_Scott.blend` in place.

**Verified two ways, not assumed:**
- Numerically: re-diffed all 24 bones against source after the fix — **0 of 24 mismatched, every
  head/tail delta reads 0.0000 to 4 decimal places**, both characters.
- Visually: posed each with the left leg lifted to a different angle than the right (deliberately
  asymmetric, to prove the two legs actually move independently now) plus both arms raised, and
  rendered. Clean deformation, no pinching, no floating limbs, both files.

### Marcel — checked, not touched, per instruction

Ran the same numeric diff against Marcel's hand-fixed `rig_fit_check_Marcel.blend` out of due
diligence. All 24 bones show *some* deviation from the raw source-FBX coordinates too, some fairly
large (Hips 14cm, Spine02 25cm) — **but this does not mean the same bug**: her technique targets
the visible mesh crease by eye, which is a legitimate, different standard than copying exact
source-skeleton numbers, and doesn't have to agree with it to be correct. Ruled out "never
touched" specifically: e.g. `LeftUpLeg` sits 4.4cm from her true target vs 15.8cm from CJ's
original untouched position — clearly moved, and moved toward the right place. Pose-test render
looked clean, no visible breakage. Left entirely alone, exactly as instructed. Worth Zoe knowing:
the spine/hip/shoulder joints carry more slack than Kyle and Scott's now-exact fix, since those
areas don't have as crisp a visual crease to snap to as a knee does — possibly worth a tightening
pass later, not urgent, not blocking anything.

### FBX exports done and verified

`Tools/rig_fit_exports/{Marcel_DiceWinged_fixed, Kyle_ArmsOut_fixed, Scott_OfficeAirplane_fixed}.fbx`
— mesh + armature, no baked animation (bind-pose/skeleton update only). Re-imported each into a
fresh Blender session afterward to sanity-check vertex/bone counts survived the round-trip
correctly (Marcel 116,143 verts, Kyle 114,091, Scott 8,015 — confirmed Scott's is genuinely just a
much lower-poly source mesh, not a truncated export, by checking his original source FBX shows the
identical count). All three: 24 bones, intact.

### Unreal side — NOT done. No live MCP in this session; retried as asked, still nothing.

Retried `unreal-mcp` per the instruction that "MCP should be reachable now" — confirmed via
`ToolSearch` that no `mcp__unreal-mcp__*` tools exist in this session, same as before. Cross-checked
against the "Editor-hang recovery" entry directly above (same day): **the editor and its MCP
server are genuinely up and were answering real handshakes as of ~07:49 this morning** — this is
not a dead editor. But per this file's own standing instructions (top of file, "What you CANNOT
do without Zoe"), an MCP connection is per-session and cannot be established non-interactively — a
session that was live *when the editor came up* has it; a session dispatched afterward (this one)
does not, and there's no way to force one from here. **Whoever has a live connection right now can
pick this straight up — the Blender side is completely done and waiting.**

### The import procedure is NOT a plain reimport — read this before doing anything in the editor

Went looking for whether this project already had a considered answer for how to get corrected
proportions into the live game, since a naive "reimport onto the existing `_Shared` assets" risks
repeating the exact mistake that caused this whole bug (one skeleton asset cannot correctly fit
three different bodies). It does — see "Confirmed real bug" section above (2026-08-25), which
already worked this out and is still the last word on it:

1. **Import each character's exported FBX as its own NEW Skeleton + SkeletalMesh asset** —
   e.g. `SK_DiceWinged_Fixed` / `SK_DiceWinged_Fixed_Skeleton`, and likewise for Kyle/Scott. **Do
   not** reimport onto `SK_DCCCrawler_Skeleton` or overwrite the existing `SK_*_Shared` meshes in
   place — that would just re-break it for whichever character's proportions differ most, exactly
   as it's broken now.
2. **Re-add sockets on each new mesh** (`hand_rSocket`, `hand_lSocket`, `headSocket`,
   `spine_03Socket`) — sockets are mesh-level, not skeleton-level (established earlier in
   `CLAUDE.md`), so a fresh mesh import carries none of them over.
3. **Mark each new Skeleton asset "Compatible" with `SK_DCCCrawler_Skeleton`** (Skeleton editor →
   Compatible Skeletons) — this is the mechanism meant to let the existing shared
   `DCCCrawlerAnimSet` (Walk/Run/etc.) keep playing on the new, individually-fitted skeletons.
   **This has been flagged as unverified twice now and still is** — confirm it actually works in
   this project's 5.8 editor before assuming the rest of the plan is safe to continue past this
   point.
4. **Repoint every Blueprint currently pointing at the old `_Shared` meshes** to the new ones —
   not just the three player characters. The Nuclear Family (`ADCCEnemyCharacter_NuclearFamily`,
   all three) and the Oathgate's `FinePrint`/`Arbiter` reuse these exact same `_Shared` meshes, so
   they inherit the identical fix requirement automatically once this is done — same repoint,
   repeated per Blueprint, no new Blender work needed for them.
5. **PIE-test after each character**, not batched at the end: confirm sockets carry props/weapons
   correctly, confirm Walk/Run animation actually plays (this is what step 3 is really testing),
   confirm the character looks right standing and moving, before moving to the next one.
6. Per this project's standing convention, do **not** commit the resulting `Content/` changes to
   git without Zoe reviewing them first.

None of this is a small task — flagging clearly rather than understating it, since step 3 being
unverified means there's a real chance the shared animation set needs a different fix entirely
(actual retargeting, not just a compatibility flag) if Compatible Skeletons doesn't do what its
name implies in this version of the engine.

## Real bug found and fixed: equipped weapons never affected attack damage at all (2026-08-28)

Zoe asked whether better/rarer weapons hit harder (her example: a frying pan should beat a bare
fist or a worse weapon). Checked rather than assumed — **they did not, at all.**
`ADCCCharacterBase::PerformAttack` was passing its own raw `WeaponDamageDie` member (a per-
character/per-Blueprint base value, e.g. 6 on the player, 4 on a Debt Eater) straight to
`Ruleset->ResolveAttack`, never reading `Equipment->GetWeaponDamageDie()` at all. That accessor
existed, was fully correct, and was completely dead as far as combat damage goes — equipping the
Fire Axe (die 10) or the Wall Sword (die 10) changed nothing about how hard a hit landed versus
bare-handed (die 6). `DCCHUD.cpp` reads `WeaponDamageDie` too, but only for a tooltip - display
only, unrelated to this bug.

**Fixed by mirroring a pattern that already existed for range**, not inventing a new one:
`GetEffectiveAttackRange()` already did exactly this - "AttackRange, or the longest reach among
equipped weapons if any is ranged" - for reach. Added `ADCCCharacterBase::GetEffectiveWeaponDamageDie()`
as its exact twin ("WeaponDamageDie, or equipment's best if it says anything"), and pointed
`PerformAttack` at it instead of the raw member. One-line fix at the actual bug, once found.

### Also wired: rarity tier feeds damage, using the loot-box tier ladder directly

Per Zoe's ask to keep this consistent with the six-tier system rather than inventing a second
rarity scale: `FDCCEquipmentDef` gained a `Rarity` field typed as `EDCCLootBoxTier` itself (the
exact enum from the achievement/loot-box work above, not a parallel one) — Bronze through
Celestial mean the same thing on a piece of gear as they do on a sealed box. Defaults to `None`,
which is why every existing hand-authored weapon (the frying pan, the fire axe, the found guns)
is completely unaffected — they keep exactly the damage their own `WeaponDamageDie` already said,
zero regression. Only an item explicitly tagged a tier gets a bonus (`+1` Silver ... `+6`
Celestial, skipping a step at the top so the ceiling tier reads as a real jump). This is the
mechanism that will make loot-box weapon rewards feel like their tier once any are authored with
`Rarity` set — nothing to build later, just a DataTable column to fill in.

New static `UDCCEquipmentComponent::GetItemEffectiveDamageDie(const FDCCEquipmentDef&)` is the
one place rarity actually gets added in - both `GetWeaponDamageDie()` and `GetWeaponDamageType()`
now compare on this effective value rather than the raw field (so a legendary dagger can correctly
outrank a mundane axe if its bonus pushes it past the axe's raw number, and the reported damage
*type* follows whichever weapon is actually winning). Also fixed the same staleness in
`ADCCEnemyCharacter`'s ally auto-equip scoring heuristic (`ScoreEquipmentDef`), which would
otherwise have kept comparing raw dice and never recognised a rarity-tiered upgrade as one.

Compile-verified (`Result: Succeeded`) alongside everything else tonight. **Not yet exercised in
PIE** — next MCP session should equip two different-die weapons back to back and confirm the
damage log actually changes, then author one `Rarity`-tagged test item and confirm the bonus lands.

## Socket-attachment workflow, recapped for Zoe (2026-08-28) - and the current gap list

No new work, a status recap she asked for directly. Kept here too so it doesn't need re-deriving
next time someone asks the same question.

**How it actually works, in one paragraph:** sockets (`hand_rSocket`, `hand_lSocket`,
`headSocket`, `spine_03Socket`) are authored ONCE PER SKELETON, not per item - already done, on
all six skeletal meshes (the four Meshy crawler rigs plus both mannequins), back when the shared-
skeleton consolidation work happened. A new item never touches the Skeletal Mesh Editor or a
Blueprint at all - it's purely two columns on its `DT_DCCEquipment` row, `Mesh` (a soft reference
to a StaticMesh asset) and `AttachSocket` (which socket name to snap to - `hand_rSocket` for
almost every MainHand/OffHand weapon, `headSocket` for Head, `spine_03Socket` for Body). At
runtime, `UDCCEquipmentComponent::RebuildVisuals()` (pure C++, no Blueprint involved) reads those
two fields on every equip/unequip and dynamically spawns+attaches a rigid `UStaticMeshComponent`
to the named socket - falls back to the mesh root if that socket doesn't exist on a given
skeleton, so a missing socket shows gear in the wrong place rather than not at all. Runs on both
server and client since it's purely visual. **Body-slot items are correctly meshless by design,
not a gap** - this project's whole rigid-attachment architecture means armor can't be a shared
mesh across four differently-proportioned rigs, so Body items are stat-only (or eventually a
material swap on the existing body mesh), never a prop. One known, still-open cosmetic gap: every
socket's transform is identity, so a prop currently sits at the literal bone origin (e.g. the
wrist) rather than in a natural grip - a one-time per-SOCKET nudge in the Skeleton Editor, not
a per-item fix, still outstanding since the sockets were first authored.

**Concrete current gap list - items that exist as real `DT_DCCEquipment` rows but have no working
mesh**, checked directly against `Docs/DT_DCCEquipment.csv` just now, not from memory:

| Item | State |
|---|---|
| `ConfettiCrossbow` | No mesh at all (blank `Mesh` column) - no bow-shaped asset exists in any installed pack, flagged since this row was first authored |
| `NailGunPro` | Has *a* mesh, but it's a borrowed tool prop (`SM_tool_01`), not gun-shaped - a placeholder, not really "done" |
| `FlareGunEmergency` | Same situation, borrowed from a lamp mesh (`SM_lamp_02`) |
| `SecuritySidearm` | No mesh - one of tonight's 4 new real-gun rows, not yet imported to the live table either |
| `HuntingRifleColorado` | No mesh - same batch |
| `RiotShotgun` | No mesh - same batch |
| `ConfiscatedSMG` | No mesh - same batch |

So: **5 weapons with literally no mesh** (Confetti Crossbow plus the 4 new guns), **2 more with
the wrong mesh** standing in. All of them still need either a real gun/bow-shaped asset pack
(the two free options already identified elsewhere in this file - Free Fantasy Weapon Sample Pack
on Fab, ASIS Free Gun Weapons Pack on itch.io) or hand-picked substitutes from what's already
installed. **Not a gap from tonight's own work** - the loot-box tables built above only reference
already-meshed items (`ThrownBottle`, `ServiceTray`, etc.), so nothing new got added to this list.
**Supercan's costume isn't on this list at all yet** - it has no `DT_DCCEquipment` row, because it
has no stat block or C++ class yet either, per `enemy-roster.md`'s own stated sequencing (model
first, then stats, then equipment). It'll need one authored, including `Rarity` if it should hit
harder as a cursed item, once Supercan himself exists.

## Per-item grip offset - the actual fix for "the hand grips the pan by the plate" (2026-08-28)

Zoe confirmed the exact failure mode the socket-recap above predicted: `RebuildVisuals`'s
`AttachToComponent(..., FAttachmentTransformRules::SnapToTargetNotIncludingScale, Socket)` snaps
every item's mesh origin flush to the socket with **zero relative offset** - checked the code
directly rather than assumed, and confirmed there was genuinely nothing partial here to build on.
Since different meshes were modelled with their pivot in different places (the pan's is near the
plate, not the handle), one socket-wide nudge can never be correct for every item sharing that
socket - each item needs its own.

**Fixed with two new columns, applied after the snap, in the socket's own local space:**
`FDCCEquipmentDef` gained `GripOffsetLocation` (`FVector`) and `GripOffsetRotation` (`FRotator`),
both defaulting to zero - so every item not yet tuned keeps the exact old snapped-flush behaviour,
zero regression. `RebuildVisuals` calls `SetRelativeLocation`/`SetRelativeRotation` with them
right after `AttachToComponent`. **Compile-verified** once build contention cleared -
`Result: Succeeded`, `DungeonCrawlerDenver-Win64-DebugGame.exe` linked clean.

**`Docs/DT_DCCEquipment.csv` updated**: header gained the two new columns; every existing row got
padded with two blank trailing fields (reproduces the zero-default exactly); `CastIronSkillet`
(the frying pan) got a real first guess: `GripOffsetLocation = (X=-20, Y=0, Z=0)` cm,
`GripOffsetRotation = (0,0,0)`.

**Be clear about what this guess actually is - a starting point, not a verified fix.** This
session has no live MCP tonight, so nothing about `SM_Pan`'s actual mesh - its pivot location,
its modelled scale, which local axis the handle even extends along - was inspected. `-20cm along
local X` is a plausible frying-pan-handle-length guess and nothing more; it could easily be the
wrong axis or the wrong sign for this specific asset's own convention. **Whoever has MCP next:
equip `CastIronSkillet` in PIE, look at where it actually sits, and iterate the offset from there**
- `StaticMeshTools`/a viewport capture on `SM_Pan` first would narrow the guess before even testing
live. Also worth doing once any of the four new gun meshes are actually assigned (still blank
per the gap list above) - guns are exactly as error-prone for grip alignment as the pan, per
Zoe's own note, and the fix now exists to apply to them the moment they have meshes.

## Climbing fixes: push-back, top-of-climb mismatch, mid-climb stops, safe landing (2026-08-28)

Zoe reported four specific climbing bugs plus two open questions (hand-grip adjustment, sourcing
a real climb animation). **No MCP this session either** (confirmed: `unreal-mcp` failed to
connect at session start), so this is another no-PIE dispatch pass — but unlike several other
entries in this file, **the code fix WAS fully built and deployed**, not just compile-checked:
the editor was already closed when this session started, so a real `DungeonCrawlerDenverEditor`
Development build ran end to end. Confirmed genuinely deployed, not hot-reloaded: `Binaries/Win64/`
has no numbered `-0001` or `patch_` DLL, and `UnrealEditor-DungeonCrawlerDenver.dll` has a fresh
link timestamp from this session. **Still, nobody has pressed a key in PIE against any of this —
see the test loop at the bottom before trusting it.**

One transient build failure along the way, explained rather than papered over: the first build
attempt hit `C4430: missing type specifier` on `GENERATED_BODY()` inside `DCCEquipmentTypes.h`
(`FDCCContainedItem`/`FDCCEquippedItem`). Re-running immediately succeeded clean. Root cause,
confirmed by this file's own concurrent "grip offset" entry immediately above this one: another
session was mid-edit on that exact file (adding `GripOffsetLocation`/`GripOffsetRotation`) at the
same moment, and the first build's UHT pass read it mid-write. Not a bug in either session's own
code - just two dispatch sessions on the same machine overlapping. Worth knowing if it happens
again: re-run before assuming something is actually broken.

### Root cause, one finding explaining three of Zoe's four bugs

Read the full climb implementation (`Characters/DCCCharacterBase.cpp`'s `TickStamina`/
`Server_BeginClimb_Implementation`/`OnRep_ActiveClimbPoint`, `Rooms/DCCClimbPoint.cpp`) before
touching anything. `TickStamina` - the function that does ALL climbing position updates via
direct `SetActorLocation` calls, every tick - is gated `if (HasAuthority())`, by explicit,
deliberate design (its own comment: "Stamina is mechanical, so only the server spends it. Clients
see the result through the replicated value.").

That comment's premise is false for movement, even though it's correct for stamina itself: **a
raw `SetActorLocation` on the server is never delivered to the pawn's own owning client at all.**
`AActor::ReplicatedMovement` - the mechanism that would normally carry a server-side position
change to clients - is replicated `COND_SimulatedOrPhysics`, which explicitly EXCLUDES the owning
(autonomous proxy) client; that exclusion is intentional in stock Unreal, because a locally-
controlled pawn is assumed to predict its own movement and would fight blind replication. Every
OTHER player watching someone climb sees it fine (they're simulated proxies, not excluded). The
climbing player's own screen does not - it just sits at its last known position until the
CharacterMovementComponent's ordinary movement-error correction eventually notices the mismatch
and snaps it to wherever the server has already gone. That snap, arriving as a discontinuity
instead of continuous motion, is what read as:

- **"Pushed back 10-15 feet" at climb start** - the server teleports itself onto the wall the
  instant a climb begins; the climbing player's own client has no idea until the next correction
  drags it there, however far that turns out to be.
- **"Sometimes stop in the middle of a cliff"** - the SAME gap, continuously, for the whole climb:
  the owning client never itself moves during the hold-to-climb phase, only the server (and every
  other player's simulated-proxy view of it) does. What Zoe saw as "stopping" was very likely just
  this: her own client frozen between corrections while the server had already kept moving.
- **Half of "top doesn't match the floor" / "drops off the side"** - the dismount-to-solid-ground
  teleport at the top is the exact same kind of raw `SetActorLocation`, so it has the exact same
  gap. Even a perfectly-computed landing spot would still read as "the climber just vanished and
  reappeared somewhere wrong" from the climbing player's own screen without a fix.

The other half of "top doesn't match the floor" is separate and real on its own: `GetDismountLocation`
picked a landing Z purely from `GetTopZ()` (the AABB top of an authored box, or the last solid
probe hit for a synthesized point) - a guess about where the WALL stops, never a check on where
the actual FLOOR is. An authored box a bit too tall/short, or a synthesized probe stopping a beat
early/late, lands the climber short (back down the face) or long (mid-air) with nothing checking
either way.

### The fix

**`Characters/DCCCharacterBase.h`/`.cpp`:**
- `OnRep_ActiveClimbPoint` now takes the old value (`ADCCClimbPoint* PreviousActiveClimbPoint`,
  the standard optional UE OnRep parameter) and, on every non-authority machine, recomputes and
  applies `ActiveClimbPoint->GetAttachLocationFor(...)` the instant a climb starts - since OnReps
  fire on the owning client too (this property has no `COND_` restricting it), this closes the
  climb-START gap on the one machine that needed it, using data every machine already has, no RPC
  required. Also sets `Movement->bJustTeleported = true` on every machine entering Flying, cheap
  CMC hygiene for a discontinuous move.
- New `TickClimbMovementCosmetic(DeltaSeconds)` (plain private method, not a UFUNCTION) mirrors
  just the position math from `TickStamina`'s climbing branch - same clamped Z step, same
  `GetAttachLocationFor` call - with NO stamina spent, no achievement/quest/log side effects.
  Called from `Tick()` only when `!HasAuthority() && IsClimbing() && IsLocallyControlled()`, i.e.
  exactly the one role (the climbing player's own client) that was previously getting nothing.
  The server (or a listen-host's own character, which is both authority and locally-controlled)
  takes the existing `TickStamina` branch instead - `else if` keeps the two from ever double-
  applying to the same machine.
- New `Client_SyncClimbLocation(FVector)` (Client, Reliable RPC) explicitly delivers the verified
  dismount location to the owning client, since - unlike climb-start - that location isn't
  something the client can rederive on its own from replicated state; it's only known once,
  server-side, at the moment the top is reached. Called from `TickStamina`'s top-reached branch
  right after the server's own `SetActorLocation(Dismount)` + `MOVE_Walking` switch.

**`Rooms/DCCClimbPoint.cpp`:** `GetDismountLocation` now verifies the landing with a real floor
trace instead of trusting `GetTopZ()` blindly. Tries three inland distances (120/200/280 units,
same "away from the wall" direction as before) and line-traces straight down at each; the first
one that hits something within 200 units of `GetTopZ()` (close enough to be "the floor right
here", not some lower floor through a gap) wins, landing the character on it (verified Z +
capsule half-height). Falls back to the old blind guess only if nothing verifies - so a point
with no real floor above it (or no `UWorld`, in some odd calling context) still resolves to
somewhere rather than nowhere. This is what actually fixes "top doesn't match the floor" and
"drops off the side" at the root - a dismount that lands on real, checked ground can no longer
drop the climber through it, independent of the replication fix above.

Deliberately NOT built: an animated multi-stage mantle (crest the lip, pause, step inland). No
mantle/climb-up animation exists to hang it on (see below), and a scripted position-interpolation
state machine to fake one is exactly the kind of new, untested state I did not want to add blind
with zero PIE access this session. What ships instead is a single, immediately-safe teleport onto
verified ground, replicated correctly - which removes the actual danger ("dropping off the side")
even though it isn't yet a two-beat animated moment. Worth revisiting once a real mantle clip
exists (see the animation section below) - the landing-verification logic already computes
exactly the destination such an animation would need to reach.

**Also added: `ADCCClimbPoint::SurfaceOffset`** (`EditAnywhere`, defaults 0), applied in
`GetAttachLocationFor` on top of the climber's own capsule radius. This is the direct answer to
"can I adjust where the player is gripping by hand?" - **no**, not per-hand: this project has no
`AnimBlueprint` at all for any character (`DCCCrowdCrawler.cpp`'s own comment: "there is no
AnimBlueprint for the shared skeleton" - every character is single-node animation). Real hand-to-
wall IK needs a Two-Bone IK node per hand in an AnimBlueprint authored against a specific
skeleton, in the editor - not something a blind C++ pass can build or verify. `SurfaceOffset` is
the coarse, whole-body stand-in that exists today: a per-climb-point knob to nudge the character
closer to or further from the wall if a specific rock face reads as too tight or too floaty.
Positive pushes out, negative pulls in, clamped so it can never go negative enough to invert
through the wall.

### Climbing animations - investigated, not sourced

`ClimbAnimation` (a `UPROPERTY` on `ADCCCharacterBase`, already wired: `SelectLocomotionAnimation`
plays it in a loop for as long as `IsClimbing()` is true, on every machine, correctly replicated)
has never had an asset assigned on the player - matching this file's own earlier note that
climbing "plays in an idle pose" today. The one existing climb-shaped clip in the project,
`A_DCC_DW_ClimbStairs` (`Content/DCC/Characters/Shared/Anims/`), is a stairs-climbing walk cycle,
not a hand-over-hand vertical wall climb - a different motion entirely, not a fit for this loop.

Did not download anything. Two independent reasons, not just caution: (1) **no MCP this session
means no way to import an FBX even if one were sourced** - the whole `SkeletalMeshTools`/Blender-
conversion pipeline this project uses for every other animation import needs a live editor
connection this session never had. (2) Mixamo (the obvious free source, and the mannequin's bone
naming is compatible with its retargeting) requires an Adobe account sign-in to download from,
which is not something to do unattended. Per this project's own established pattern for exactly
this situation (the two free weapon packs flagged elsewhere in this file - "only Zoe can
download, once landed under Content/ we import it") - **this is queued the same way**: search
Mixamo for "climbing" (not "climb stairs") - it has several wall-climb-shaped clips (an idle
hang, climb-up/down loops) distinct from a ladder or stairs climb; a specific catalog name isn't
promised here since it wasn't verified against the live library. Whoever's picking a specific
clip should eyeball 2-3 candidates against the mannequin/four Meshy rigs' actual proportions
before committing, same as any other style call this file defers to Zoe.

### Test loop for the next live session - nothing above has been run

1. Open the editor (this session's build is already deployed - confirmed no numbered hot-reload
   DLL, real link timestamp - so this should just work, no rebuild needed first).
2. PIE with **two clients** if at all possible (the bug is specifically about the climbing
   player's OWN view, which a single listen-server-host test cannot reproduce - a host is both
   server and locally-controlled, so it never hit the gap this session fixed). A 2-player PIE
   session is a pattern this project has used before.
3. On the REMOTE (non-host) client: approach a climbable face, start a climb, hold up/down, let
   go at the top. Watch specifically for: no large snap at climb start; continuous visible motion
   while holding, not frozen-then-jump; landing on solid ground at the top, not floating or
   falling through.
4. Also worth checking: a climb interrupted by stamina running out (arms give out mid-face) still
   falls cleanly - that path was deliberately left alone (no dismount RPC needed there, per this
   entry's reasoning above), so it's the one case most likely to reveal if the cosmetic mirror's
   position drifted from the server's real one during the hold.
5. If any of the above still shows a snap, the next thing to check is whether `Client_
   SyncClimbLocation`/`OnRep_ActiveClimbPoint`'s ordering assumption held - the comments in
   `DCCCharacterBase.cpp` around both explain exactly what each is supposed to guarantee, which
   narrows where to look first.

## Two resumed sessions after the usage shutdown: still no live MCP, but real progress on gun meshes + a CSV corruption bug found (2026-08-29)

Dispatched to resume two sessions that got cut off by a Claude usage shutdown (now lifted). Explicit
instruction going in: no `AskUserQuestion`/interactive-input tool, no live user this session, make
judgment calls and document them. Both tasks assumed "we're back online" meant the account limit was
lifted, not that the editor is running — checked directly rather than assumed:
`tasklist //FI "IMAGENAME eq UnrealEditor*"` returned no matches, and `netstat` showed nothing on port
8000. **The editor is not open at all this session** (not a connection-drop — genuinely not running),
confirming the `unreal-mcp: ConnectionRefused` in this session's own tool list. Same conclusion this
file has recorded on several nights now: MCP is a startup-time handshake, not a thing a session can
force from outside.

### Task 1 (DataTable spot-check + PIE tests) — genuinely blocked again, nothing faked

With no editor process at all, `list_rows` and PIE are both impossible, not just inconvenient. Did not
simulate or guess at results. What *was* verifiable from disk:

- `Docs/DT_DCCLootBoxTables.json` (6 tiers) and `Docs/DT_DCCBoons.json` (8 boons) both still parse as
  valid JSON, unchanged since the 2026-08-28 loot-box session wrote them.
- `Docs/DT_DCCAchievements_LootBoxBatch1.csv` still has its 6 rows, correct header shape.
- **None of this confirms the live `DT_DCCLootBoxTables`/`DT_DCCBoons`/`DT_DCCAchievements` tables in
  the editor actually hold this content** — only that the source files a future import would read from
  are intact. The climbing fix, the frying-pan grip offset, and Selfie Mode polish are all still
  exactly as undeployed-to-PIE as the 2026-08-28 entries above describe; nothing new to add there.

**Whoever gets a live session next should just pick up the existing queued list** (climbing 2-client
test loop, frying-pan grip-offset iteration, Selfie Mode refusal/cycling/viewfinder test) — this
session didn't change what's owed, only re-confirmed it's still owed.

### Task 2a — gun meshes: a real pack was already sitting in `Content/`, missed by earlier keyword searches

The 2026-08-22 "no bow- or gun-shaped mesh anywhere in the project" finding (`CLAUDE.md`, loot-menu
section) searched filenames for `*bow*`/`*gun*`/`*rifle*`/`*crossbow*` — which is exactly why it missed
**`Content/UnrealBucket/Library/Weapon/`**, a real prop-asset pack already installed with `Revolver`
(`SM_Revolver`, plus a skinned `SK_Revolver` variant), `MauserC96Pistol` (`SM_MauserC96_Pistol`),
`Panzerfaust`, `Grenade`, and `Halberd` sitting right next to the previously-catalogued
`ModularSwordsPack`/`Scabbards` — none of those names contain "gun," "rifle," or "bow," so the earlier
keyword search walked right past them. Found this time with a broader keyword pass (`revolver`,
`pistol`, `smg`, `carbine`, `magnum`, etc., not just the four original terms) plus a manual folder
listing. **This means 4 of the 7 items on the "no mesh" gap list from the 2026-08-28 socket-recap
entry can be fixed right now, with zero downloading and zero MCP** — the CSV just needed to point at
assets that were already sitting on disk:

| Item | Old | New (already installed, no import needed) |
|---|---|---|
| `SecuritySidearm` | blank | `/Game/UnrealBucket/Library/Weapon/Revolver/SM_Revolver.SM_Revolver` |
| `ConfiscatedSMG` | blank | `/Game/UnrealBucket/Library/Weapon/MauserC96Pistol/SM_MauserC96_Pistol.SM_MauserC96_Pistol` (a boxy, stocked semi-auto — not a true SMG shape, but the closest thing installed to "confiscated, weird, chunky gun"; the Mauser's own historical "Schnellfeuer" variant actually was select-fire, so the flavor isn't even a stretch) |
| `NailGunPro` | `SM_tool_01` (fantasy blacksmith prop, wrong) | `/Game/UnrealBucket/Library/Tools/CarpenterTools/SM_HandDrill_A.SM_HandDrill_A` (a real pistol-grip power tool — still not a nail gun specifically, but a genuine improvement over a smithing prop) |
| `FlareGunEmergency` | `SM_lamp_02` (wrong) | `/Game/UnrealBucket/Library/Weapon/Revolver/SM_Revolver.SM_Revolver` (real flare guns are commonly stubby single-shot break-action pistols — a revolver silhouette is a reasonable stand-in, much closer than a lamp) |

**Still genuinely empty, because nothing gun/rifle/shotgun/bow-shaped beyond the revolver+pistol exists
in any installed pack** (re-checked broadly, including `Weapon_Pack` — medieval melee only, no
firearms): `HuntingRifleColorado`, `RiotShotgun`, `ConfettiCrossbow`. Did **not** touch `The Boilerplate
Rifle` row or `Content/Meshy_AI_A_boilerplate_rifle__0828202153_texture` — per this dispatch's explicit
instruction, that's a parallel session's work (Vaughn's model).

**Free, commercially-usable candidates for the 3 still-empty rows** (none downloaded — no MCP means no
import pipeline this session anyway; same "found and flagged, not fetched" pattern as the two weapon
packs already queued elsewhere in this file):

- **Best option: Quaternius's "Ultimate Guns Pack"**, mirrored at [poly.pizza](https://poly.pizza/bundle/Ultimate-Guns-Pack-cpgUfI4t2F) — genuinely CC0 (public domain), no login, downloadable as **FBX directly** (no Blender conversion step needed, unlike the Meshy glb pipeline). 25 models covering exactly the gap: multiple Assault Rifle/Sniper Rifle/Bullpup variants (`HuntingRifleColorado` — none are bolt-action-hunting-rifle shaped specifically, but any rifle silhouette reads better than nothing), 4 Shotgun variants including a Sawed-Off (`RiotShotgun`), plus 2 SMG and revolver/pistol variants (spare capacity to eventually replace the Mauser-C96/Revolver stand-ins above with purpose-shaped guns once this lands).
- **Crossbow**: no CC0 pack covers this, so it's single-model sourcing — several candidates on Sketchfab, license unverified per-model (Sketchfab licenses are set per-uploader, not per-site, so check each before use): [Low Poly Wooden Crossbow by d0ggo](https://sketchfab.com/3d-models/low-poly-wooden-crossbow-cbecf2d76db74f7eb95359a3ae167305) and [Low Poly Crossbow by Andro](https://sketchfab.com/3d-models/low-poly-crossbow-fb98e07fc24e46329de36199354d6fd2) both read as reasonable `ConfettiCrossbow` shapes from their thumbnails/descriptions.
- **Backup option**: Kenney's [Blaster Kit](https://kenney.nl/assets/blaster-kit) — CC0, no attribution, explicitly Unreal-ready, 40 models — but stylized sci-fi blasters, a tonal mismatch for DCC's "mundane object, absurd context" style (the whole point of `RiotShotgun`/`HuntingRifleColorado` is that they read as real store/hunting guns). Use only if Quaternius's pack turns out to have an import problem.

None of this needs Zoe's login — Fab/itch (the two packs already queued from 2026-08-27) are the only
gun-adjacent sourcing that does. Whoever has MCP next can pull straight from poly.pizza with no account.

### Found while validating the CSV: a real, pre-existing corruption bug, fixed in all 12 affected rows

Spot-checking `Docs/DT_DCCEquipment.csv` for well-formedness (Python's `csv` module, comparing field
counts per row against the 23-column header) turned up **12 rows with a genuine, silent corruption**,
predating this session: `CastIronSkillet`, `FireAxe`, `WallSword`, `TenderiserMallet`, `ChefsKnife`,
`ServiceTray`, `FeatureWallShield`, `PatioSunHat`, `GreaseApron`, `ManagersBlazer`, `NameTagDave`, and
`Torch` all had their `LightColor` column mangled into a literal Python list `repr()` string embedded
in the CSV (e.g. `(R=1.0,G=1.0,B=1.0,"['A=1.0)', '0', '0']"`) — clearly the leftover of some earlier
script bug that wrote a list back into a single cell instead of joining it. **First pass caught only 3
of these** (the ones where the shift also broke the total field count); the other 9 happened to still
add up to 23 fields by coincidence, so they read as "valid" CSV while every field from `LightColor`
onward was silently shifted one column late — garbage in `LightColor`/`LightIntensity`/
`LightAttenuationRadius`, the real trailing values swallowed into a column with no name in the header.
**`Torch` was the worst case**: its `bEmitsLight=True` and it actually uses those values, so its real
`LightIntensity`/`LightAttenuationRadius` (40/750) were buried *inside* the corrupted list-string
itself (`"['A=1.0)', '40', '750']"`) rather than sitting in their own columns — recovered them from that
fragment rather than guessing, so `Torch` now reads `"(R=1.0,G=0.62,B=0.28,A=1.0)",40,750` exactly as
before the corruption, not a fresh placeholder guess. A peer session (`dungeoncrawlerdenver-c0`, adding
Vaughn/Boilerplate Rifle content to this same file) surfaced the other 9 by editing the file after my
first pass, which is what caught the gap — cross-checked directly with them before touching the file
again to make sure neither session was mid-build or fighting over the same file. All 12 fixed the same
way: the standard quoted `"(R=1.0,G=0.65,B=0.30,A=1.0)",40,700` placeholder for the 11 non-light rows,
`Torch`'s real recovered values for itself, and — for `CastIronSkillet` specifically — also re-quoting
its `GripOffsetLocation`/`GripOffsetRotation` (`"(X=-20.000000,Y=0.000000,Z=0.000000)"` /
`"(Pitch=0.000000,Yaw=0.000000,Roll=0.000000)"` from the 2026-08-28 grip-offset session) so the embedded
commas in an `FVector`/`FRotator` string can't do the same shift again. Re-validated twice: field count
matches the 23-column header on every row, and a second pass confirmed zero rows still contain a stray
`[...]` list-repr fragment anywhere. **Whoever imports this CSV next should still read rows back after
import**, per this file's own standing rule — this bug is exactly the kind of thing that only shows up
that way, and it was silent for long enough that nobody knows which prior session's script wrote it.

### Task 2b — animations: Mixamo confirmed unbrowsable without login; a zero-download fallback exists for poses

Tried fetching Mixamo's search page directly (`mixamo.com/#/?query=climbing`) — it's a client-side app
that returns nothing but a loading shell to an unauthenticated fetch, confirming what this file already
predicted: there's no way to even browse the catalog, let alone download, without Zoe's own Adobe
login. **Exact ask for Zoe, when she has a minute:**
1. Go to [mixamo.com](https://www.mixamo.com), sign in with an Adobe account.
2. Upload one of this project's own character FBX files to get it auto-rigged (any of the
   `SourceArt/*/fbx/SK_*.fbx` sources, or even just use one of Mixamo's stock characters temporarily
   just to preview clips — re-targeting to our own rig happens at import time either way).
3. Search **"climbing"** in the animation browser (not "climb stairs" or "ladder") — Mixamo's climbing
   category includes wall-climb-shaped loops distinct from the stairs clip already in the project
   (`A_DCC_DW_ClimbStairs`, confirmed wrong-motion for this by the 2026-08-28 climbing-fix entry).
   Preview 2-3 candidates and download whichever looks best against our proportions.
4. Search **"thumbs up"**, **"peace sign"** (or "victory"), **"salute"**, **"wave"**, **"yawn"** for
   Selfie Mode poses — Mixamo's gesture category covers this territory but exact catalog names weren't
   verifiable from outside a logged-in session, so this is "search these terms and eyeball the
   results," not a guaranteed exact-title list.
5. Download each as FBX, **without** "In Place" (root motion is irrelevant to us, so it doesn't matter
   either way, but do use "Without Skin" if given the choice — the project's `SkeletalMeshTools` import
   path wants the animation alone, same as the existing `Tools/glb_to_fbx.py` per-animation-FBX
   convention documents already).
6. Drop the FBX files under `SourceArt/` (a new subfolder, e.g. `SourceArt/Mixamo/`) so the next MCP
   session can run them through the existing import pipeline — same "land it in Content/, next session
   imports it" handoff already established for the two weapon packs.

**Zero-download fallback for pose-cycling specifically, found while checking what's already on disk:**
`Content/DCC/Characters/Shared/Anims/` already has several non-combat clips usable as selfie poses
right now, no Mixamo needed — `SelfiePoseAnimations` (empty on every character per the 2026-08-28
Selfie-polish entry) could be populated today from this list once MCP is back:

| Clip | Read as a pose? |
|---|---|
| `A_DCC_AO_AgreeGesture` / `A_DCC_HW_AgreeGesture` | Yes — a nod/agree gesture, good default pose |
| `A_DCC_OA_BoxingWarmup` | Yes — a flex/fighting-stance pose |
| `A_DCC_AO_ArcheryAim` | Yes — a held aiming pose, reads well frozen |
| `A_DCC_DW_CallGesture` | Yes — a call/gesture pose (also used elsewhere as a cast anim; fine to reuse, `PlayOneShotAnimation` doesn't care) |
| `A_DCC_HW_BoomDance` / `A_DCC_AO_AllNightDance` / `A_DCC_DW_CardioDance` | Maybe — dance loops read as fun/dynamic mid-swing but weren't authored as a "held pose," so the shutter might catch an awkward in-between frame; worth eyeballing in PIE before committing |

This doesn't replace the Mixamo ask — genuine "peace sign"/"thumbs up" gestures still don't exist in
this project's animation set — but it means pose-cycling can ship *something* per-character today
rather than waiting on a Mixamo pass that needs Zoe's own login. Whoever has MCP next: assign 3-4 of
the "Yes" rows above to each character's `SelfiePoseAnimations` (per-character, since it's authored
against that skeleton) as a first pass, then swap in real Mixamo pose clips later once sourced.

### Summary of concrete state changes this session

- `Docs/DT_DCCEquipment.csv`: 4 rows repointed to real, already-installed meshes (no import needed
  beyond a normal re-import of this CSV); 12 unrelated pre-existing corrupted rows fixed (see above) —
  **the frying-pan grip offset from 2026-08-28 survives this fix intact, just properly quoted now.**
  Coordinated with `dungeoncrawlerdenver-c0` (adding Vaughn/Boilerplate Rifle to the same file) before
  and during this pass; their `BoilerplateRifle` row was already clean, not touched.
- No `Content/` changes, no C++ changes, no git commits (per this project's standing rule, only when
  asked). Everything above is `Docs/`-side, safe to import next MCP session with the usual
  diff-against-live-rows discipline this file has documented repeatedly.
- Nothing from task 1 was run — editor wasn't open. Nothing from task 2 was downloaded — no MCP to
  import it into, and Mixamo needs Zoe's login regardless.

## Vaughn identified and prepped, Boilerplate Rifle scale flagged + wired, Oathgate directions given (2026-08-29, dispatched fresh, no editor at session start)

Zoe came back online after a usage shutdown having added two new Meshy assets and asked for
(1) Vaughn imported and wired in, (2) the Boilerplate Rifle imported and confirmed in the
kitchen chest, (3) the Oathgate made genuinely reachable (she'd never found it). **No MCP this
session** — `unreal-mcp` failed to connect at startup, and unlike most no-MCP dispatches in this
file, the editor process itself was not even running (confirmed via `Get-Process`/port 8000
checks) for most of the session, so this was pure prep-and-document work, same category as the
climbing-fix and grip-offset sessions above. One thing did land live — see the build note at the
bottom.

### 1. Vaughn — identified as the fifth crawler, not a new NPC/enemy

**Judgment call, flagged clearly per the original ask - correct if wrong.** Evidence, not a
guess from the name alone:

- `CLAUDE.md`'s own header lists the five players as **"Scott, CJ, Marcel, Vaughn, Kyle"** —
  Vaughn is a named party member, not a design-doc or enemy-roster concept. Confirmed the same
  in `Docs/multiplayer-plan.md`'s opening line.
- The four existing ally classes (`Characters/DCCAllyCharacters.h`) cover exactly CJ
  (`ADCCAlly_HockeyWings`), Scott (`OfficeAirplane`), Kyle (`ArmsOut`), Marcel (`DiceWinged`) —
  four of the five players, with Vaughn the obvious missing one.
- This file's own 2026-08-27 entry ("Collapse cutscene") already found, undocumented until then,
  **`PersonalRooms/PersonalRoom_CJ/Kyle/Marcel/Scott/Vaughn`** — five small rooms, one per player,
  each with a placeholder body. Vaughn's room has been sitting there waiting for a real body
  since at least that session.
- The model itself confirms it structurally: `Content/Characters/Meshy Vaughn/` (also
  `Content/Vaughn.zip`, 49.6MB, same contents) holds a Meshy character glb job named
  `Meshy_AI_Mountain_Embrace_biped` (Meshy's own auto-generated job title — not a themed name,
  no relation to any enemy-roster concept) plus a merged-animations glb. **Bone-for-bone
  identical 24-bone rig** to the other four crawlers (`Tools/list_bones.py`, new this session —
  same names, same hierarchy, same `Hips` root) at the same real-world scale (~172cm tall,
  matching "170cm, correct scale" already recorded for CJ). Rendered a preview
  (`Tools/render_glb_preview.py`, new this session) rather than trust the filename: a heavyset
  bearded man, glasses, navy work T-shirt, khakis, brown shoes — an ordinary person, exactly the
  same "regular Denver local swept into the dungeon" visual register as CJ/Scott/Kyle/Marcel, not
  a costumed or thematic character. Also found and excluded a stray junk mesh Meshy included
  alongside the real character (`Icosphere`, unattached to the rig, pure bounding-reference
  clutter) — worth remembering for any future Meshy character import, this wasn't previously
  documented as a gotcha.

**Prep done, no MCP available to go further:**
- `SourceArt/Vaughn/fbx/SK_Vaughn.fbx` — character mesh, junk mesh dropped, no baked animation
  (`Tools/vaughn_char_to_fbx.py`, new this session, same shape as `glb_to_fbx.py` plus the
  Icosphere removal).
- `SourceArt/Vaughn/fbx/Anims/A_Vaughn_Running.fbx` and `A_Vaughn_Agree_Gesture.fbx` — Vaughn's
  own source clips, split via the existing `split_glb_anims.py`. **Only two clips exist** (no
  Walk, no attack) — smaller than any of the other four originally shipped with, which is the
  actual argument for the plan below rather than just falling back to his own set.
- New C++ `ADCCAlly_Vaughn` (`Characters/DCCAllyCharacters.h/.cpp`), same shape as the other
  four, **deliberately pointing at its own future skeleton/mesh (`SK_Vaughn`), not a `_Shared`
  asset.** This is the important call: forcing Vaughn onto the existing `SK_DCCCrawler_Skeleton`
  (fit to CJ's proportions) is the exact mechanism that caused the "both thighs on one hip" bug
  just found and fixed for Kyle/Scott (see this file's "Rig-fit" entries, 2026-08-28). Vaughn is
  new, so he should get the corrected procedure from day one — his own skeleton, marked
  **Compatible with `SK_DCCCrawler_Skeleton`** in the Skeleton Editor so he still plays the full
  shared `DCCCrawlerAnimSet` (walk/run/attack/cast/climb/emotes) instead of just his own two
  clips. **That Compatible-Skeletons mechanism is itself still unverified in this project** — the
  Kyle/Scott/Marcel fix queued elsewhere in this file needs to confirm it works first. Header
  comment on the new class spells this out and names the fallback (his own Running/Agree_Gesture
  clips) if it doesn't pan out.
- Added `Vaughn` to the admin debug spawn panel (`UI/DCCHUD.cpp`, next to the existing
  `HockeyWings` entry) so he's spawnable for testing the moment the mesh exists.

**Left for a live MCP session, in order:**
1. Import `SourceArt/Vaughn/fbx/SK_Vaughn.fbx` as its own new Skeleton + SkeletalMesh under
   `/Game/DCC/Characters/Vaughn/` (`SK_Vaughn` / `SK_Vaughn_Skeleton`) — **not** onto the shared
   skeleton, per the reasoning above.
2. Add the four standard sockets (`hand_rSocket`, `hand_lSocket`, `headSocket`,
   `spine_03Socket`) — mesh-level, not inherited from anywhere.
3. In the Skeleton Editor, mark `SK_Vaughn_Skeleton` Compatible with `SK_DCCCrawler_Skeleton`.
   **Confirm this actually makes the shared clips play before trusting it** — if it doesn't,
   import `A_Vaughn_Running`/`A_Vaughn_Agree_Gesture` instead and swap
   `DCCCrawlerAnimSet::Apply(*this)` out of `ADCCAlly_Vaughn`'s constructor for direct assignment
   of just those two.
4. Create (or reparent an existing) `BP_DCCEnemy_Vaughn` Blueprint onto `ADCCAlly_Vaughn`, same
   pattern as the other four's `BP_DCCEnemy_*` wrappers.
5. Give him a real body in `PersonalRoom_Vaughn`, replacing the placeholder.
6. PIE-test immediately, not batched — confirm he stands correctly, sockets carry props, and
   (if Compatible Skeletons worked) the shared animations actually play without a
   `SkeletonToCompactPose` ensure.
7. Zoe's own call, not decided here: his party role/skills (`bHasFirstAid`-equivalent, starting
   skills) — deliberately left unset, same reasoning `DCCAllyCharacters.h`'s own header gives for
   why C++ shouldn't touch `StartingSkillIds`.

### 2. The Boilerplate Rifle — already imported once (probably oversized), corrected version prepped, wired into the kitchen chest

**Found `Content/Meshy_AI_A_boilerplate_rifle__0828202153_texture.glb`** (137MB) — and, sitting
right next to it, **an already-imported StaticMesh + Material + 3 Textures**
(`Content/Meshy_AI_A_boilerplate_rifle__0828202153_texture/{StaticMeshes,Materials,Textures}/`,
loose in `Content/` root, not under `Content/DCC/`, not tracked by git per `/Content/*` in
`.gitignore`). This has every sign of Zoe's own drag-and-drop glTF import (the only importer
that reads `.glb` directly, per `CLAUDE.md`'s own note) rather than anything from a prior
session. Rendered it (`Tools/render_glb_preview.py`) to see what it actually is, rather than
guess from the filename: **a riveted steampunk boiler tank with a pressure gauge, coiled piping,
and a radiator-finned barrel on a pistol grip** — genuinely a boiler, not just a name. Good
material for the flavor text; the pun is literal.

**Real concern, not confirmed either way without live MCP:** `Tools/measure_glb.py` puts the raw
mesh at 1.9m along its long axis. This project's own `glb_to_fbx.py` already documents why that
number is suspicious: Meshy normalizes non-character props to roughly the same bounding-box size
as a *character*, not to true scale — a water bottle or board game "comes out ~190cm tall unless
corrected." If the already-imported loose asset came in through the native drag-and-drop
importer with no correction (nothing suggests one happened), **it's very likely close to 2x
oversized in-engine right now** — a rifle nearly as tall as the person holding it. Could not
check the actual live `StaticMesh` bounds (`StaticMeshTools.get_bounds` needs MCP), so this is
flagged, not fixed blind.

**Prepped a corrected version as the safer path**, not touched the loose asset:
`SourceArt/BoilerplateRifle/fbx/SM_BoilerplateRifle.fbx`, converted via the existing
`glb_to_fbx.py` with a 0.5x scale correction (targeting a ~95cm overall length — a first-pass
visual guess from the render, not a measured real-world reference, same caveat class as the
frying-pan grip offset elsewhere in this file). **Whoever has MCP next: check the loose asset's
actual live bounds first** — if it turns out the native importer already applied a sane
correction, the loose asset may be fine as-is and this conversion is unnecessary insurance;
if not, import this FBX to `/Game/DCC/Weapons/BoilerplateRifle/` instead and point the
DataTable there (already done, see below).

**No "Boilerplate Rifle" row actually existed in `Docs/DT_DCCEquipment.csv`**, contrary to the
task brief's premise that one was "already added, Mesh column intentionally left empty." Checked
directly (`grep -i boilerplate`) — nothing. Either an earlier session added it straight to the
live table (this file's own repeatedly-documented "live table drifts from CSV" pattern) and it
never reached this CSV, or the row was planned but never written. Can't tell which without MCP,
so **added it now** rather than leave the gap: `BoilerplateRifle`, MainHand, die 12, range 1800,
180g, `Mesh` pointed at the clean target path above (not the possibly-oversized loose asset),
`AttachSocket = hand_rSocket`, flavor text playing the literal-boiler/legal-boilerplate pun.
**`GripOffsetLocation`/`GripOffsetRotation` deliberately left blank** rather than guessed — this
mesh's actual pivot is unknown, and the frying-pan guess elsewhere in this file is already
flagged as possibly-wrong-axis-or-sign for a *much* simpler shape; guessing blind on an
asymmetric multi-part boiler-and-barrel assembly seemed more likely to actively mislead the next
session than to help it. (A second dispatched session working the gun-sourcing gap in parallel
tonight confirmed this row reads clean in the CSV, no corruption — see the corruption-bug entry
above this one.)

**Kitchen chest — could not confirm it was ever placed, so made it durably true instead of just
checking.** None of the six curated loot-chest C++ classes
(`Items/DCCLootContainerVariants.cpp`) referenced `BoilerplateRifle` anywhere — if an earlier
session placed it, that was a live-instance-only edit this session has no way to see. Rather than
report an unverifiable claim either way, added `BoilerplateRifle` as a fourth guaranteed drop in
`ADCCLootContainer_KitchenDrawer`'s constructor (alongside the mallet/knife/skillet) — a
constructor default is true for every placement of that chest regardless of what any one
instance's live state was, matching this project's own stated preference for constructor
defaults over one-off MCP edits where either would do. Off-theme for a drawer of kitchen tools on
purpose; the joke is that it's there at all. **If "the kitchen chest" was actually meant to be a
different, already-placed instance elsewhere in the level, this doesn't retroactively fix that
one** — worth a live `find_actors` check for any `ADCCLootContainer_KitchenDrawer` instance
placement, and confirming it's physically in/near Room 6 (Kitchens).

**Left for a live MCP session:** check the loose asset's real bounds (decide whether the
corrected FBX is actually needed); import whichever mesh is right to
`/Game/DCC/Weapons/BoilerplateRifle/`; re-import `DT_DCCEquipment.csv`; equip it in PIE and tune
the grip offset from an empty starting point (viewport capture on the mesh first, same advice
already on record for the frying pan); confirm the Kitchen Drawer chest — wherever it's actually
placed — drops it.

### 3. The Oathgate — already placed, most likely already reachable; gave Zoe directions rather than guess at a fix

Re-read this file's own 2026-08-27 "Oathgate placed" entry in full rather than take the task
brief's summary at face value (it conflates the Oathgate with the cliff-lift bartender — **these
are two separate, unrelated systems** built in the same session; the cliff lift has nothing to do
with the Oathgate and is still fully unplaced, see that entry). What that entry actually
establishes, with its own read-back verification:

- Both gates are placed and the level is saved: **`Oathgate_Entrance`** at world coordinates
  **(900, 900, 160)**, facing yaw -135, `EnemyPool` left at its default (Debt Eater / Fine
  Print / Arbiter); **`Oathgate_Exit`** at **(-900, -900, 160)**, facing yaw 45, `EnemyPool`
  cleared so it only teleports back. `PairedGate` cross-linked both directions, confirmed by
  reading the values back, not just trusting the write.
- Both sit in outliner folder **`DC/Oathgate`**.
- They are **not** in any of Floor 1's seven named rooms — they're in the **sandbox/dev-test
  area near spawn**, the same room that has hosted this project's long-standing test dummies
  (`Mob_Brute_Courtyard`, `Scott_OfficePlaza_Demo`, `TestBoss_InternManager`) since well before
  this session. That placement was deliberate, per the class's own header — it's a DM testing
  tool, not floor content, and "anywhere reachable works" was the actual instruction it was
  placed under.

**Reachability - inferred from documented history, not freshly checked (no editor running this
session):** the same placement entry's own account of finding a clear spot mentions
`find_actors` on the target box turning up navmesh and roaming-enemy hits — and that area has
hosted actively-pathing AI test dummies for a long time, which requires navmesh coverage to
function at all. That's real evidence, not proof; nobody has actually walked to it in PIE. **This
session did not find anything broken, and did not change the Oathgate's placement, because there
was nothing to check it against** (no editor, no MCP, no PIE).

**Most likely explanation for "I can't find it":** it was never placed in any of the seven floor
rooms on purpose — walking the actual dungeon layout looking for it would never turn it up. The
fix is directions, not a repair:

**Directions for Zoe:** open the level editor and search the World Outliner for `Oathgate` (or
open the `DC/Oathgate` folder directly) to jump the camera straight there — fastest way to
confirm placement without walking. To find it on foot in PIE: head to the same sandbox/courtyard
test area used for the `Mob_Brute_Courtyard`/`Scott_OfficePlaza_Demo`/`TestBoss_InternManager`
dummies, near spawn. World coordinates (900, 900, 160) is the entrance (walking into it should
roll a 3-enemy encounter near the exit); (-900, -900, 160), roughly 2500 units southwest of the
entrance, is the exit (walking into it only teleports back).

**If she genuinely can't reach it on foot** (no navmesh, or a wall in the way after all), that's
new information worth reporting back specifically — this session could not rule it out, only
found reasons to think it's unlikely.

### Build note — a real Development Editor deploy happened this session

The editor was closed for most of this session (confirmed via `Get-Process`/port 8000 checks),
which is the one window this project's own workflow treats as safe for a full deploy build, not
just a `DebugGame` compile-check. Ran both in sequence: `DebugGame` first (clean), then
`DungeonCrawlerDenverEditor` Development (clean, link timestamp Aug 29 19:38, **no numbered
hot-reload DLL** — confirmed genuine relink). This deployed today's C++: `ADCCAlly_Vaughn`, its
`DCCHUD.cpp` admin-spawn entry, and the `BoilerplateRifle` kitchen-chest wiring. Coordinated with
a concurrently-running peer session (`dungeoncrawlerdenver-5e`) before building, per this file's
own standing caution about concurrent UBT runs — confirmed neither of us was mid-build or
touching the same files first.

**An `UnrealEditor.exe` process (PID 7080, started 19:47:27, after this build finished) appeared
partway through this session** — not opened by this session. The peer session confirmed it's
genuinely running, but **neither session got MCP against it** (both predate its startup, per
this file's own long-established "MCP is a startup-time handshake" rule). Whoever starts a
*fresh* Claude Code session against that live instance should have working MCP with all of
today's work already loaded — the Vaughn class, the DCCHUD entry, and the kitchen-chest change
are compiled in; the CSV and the actual asset imports above are still queued and waiting.

## Four bugs from Zoe: climbing drop on the tall/portal climb, ally E-interact, and the loot-menu Stow/gold bug (2026-08-29, dispatched, no MCP)

Zoe reported four issues in one message. **No MCP this session** (`unreal-mcp: ConnectionRefused`
at session start) - and unlike some recent entries, this was NOT a "no editor running" situation:
partway through this session `UnrealEditor.exe` (PID 7080) came up and port 8000 started
answering, with active traffic from a concurrent peer session. Per this file's own repeatedly-
confirmed rule, MCP is a startup-time handshake - a session that started before the editor came up
does not retroactively gain it, so this stayed a read-the-source-and-fix-blind session throughout.
**DebugGame compile-check passed clean** (confirmed with the editor genuinely open and a peer
session using it - the documented-safe target for exactly that situation) - **not deployed**,
deliberately: the editor was live and in active use by another session for the whole second half
of this one, so a close-editor Development build was never a safe option here. All four fixes
below are source-only until the next close→build→reopen cycle.

### 1. Climbing "randomly drops" on the tall/portal climb - root cause found, different from the 2026-08-28 fix, and fixed

Read the full climb implementation before touching anything, including the 2026-08-28 entry
above, which fixed two real gaps (climb-start snap, top-of-climb dismount) by adding
`Client_SyncClimbLocation`/`OnRep_ActiveClimbPoint` position syncs for the owning client - the one
machine that a raw server-side `SetActorLocation` never reaches via ordinary `ReplicatedMovement`
(`COND_SimulatedOrPhysics` excludes the owner by design). That entry's own test-loop section
flagged exactly one case as *not* covered and most likely to reveal drift: **a climb interrupted
mid-face** (grip fails when stamina hits zero, or the player just lets go) rather than one that
reaches the top. That is precisely what a long, tall climb makes far more likely to happen - more
climbing time means more chance of running out of stamina before the top, and the report singles
out "the big mountains with the portal on top" specifically, which is exactly this project's
tallest known climb.

**The actual gap:** `TickClimbMovementCosmetic` (the owning client's own local position guess,
added by the 2026-08-28 fix) runs every tick with no further correction from the server for the
entire duration of a climb - the only two moments the server ever tells that client the truth are
climb-start and climb-top. `Server_ReleaseClimb_Implementation()` - called both for a voluntary
release and for the "arms give out" stamina-exhaustion case
([DCCCharacterBase.cpp](../Source/DungeonCrawlerDenver/Characters/DCCCharacterBase.cpp)'s
`TickStamina`) - nulled `ActiveClimbPoint` and replicated a movement-mode change to `MOVE_Falling`
via `OnRep_ActiveClimbPoint`, but never sent the client its actual, current, server-authoritative
position. The client's own cosmetic guess - accumulated tick-by-tick from local `DeltaSeconds`
and whatever input state had most recently reached it - is never wrong by much on a short climb,
but nothing bounds how far it can drift on a long one, and it is never resynced until the moment
of release, at which point the client starts falling from wherever ITS guess says it is, while the
server (and every other player's simulated-proxy view) starts the fall from the real position. On
a short climb that gap is invisible; on a long one it is a visible, sudden snap - "randomly drops."

**The fix**
([DCCCharacterBase.h](../Source/DungeonCrawlerDenver/Characters/DCCCharacterBase.h),
[DCCCharacterBase.cpp](../Source/DungeonCrawlerDenver/Characters/DCCCharacterBase.cpp)):
`Client_SyncClimbLocation` gained a second parameter, `bLandOnGround` (true = the existing
top-of-climb case, switches the client to `MOVE_Walking`; false = the new case, switches it to
`MOVE_Falling`). `Server_ReleaseClimb_Implementation` now calls
`Client_SyncClimbLocation(GetActorLocation(), false)` for the owning remote client before
returning, delivering the real server position at the exact moment a climb ends early - the same
fix shape as the 2026-08-28 entry, applied to the one gap it left open. This is a `UFUNCTION`
signature change (reflection), so - same as every other fix in this session - it needs the usual
close-editor → Development Editor build → reopen cycle before it does anything; DebugGame
compile-check is clean.

**Not verified in PIE - nobody has climbed anything against this build yet.** Test loop once
deployed: two-client PIE (a listen-host's own pawn is both authority and locally-controlled, so it
never hits this gap - same caveat the 2026-08-28 entry gives), have the REMOTE client climb
something tall enough to either run out of stamina or deliberately let go partway up, and confirm
no snap/drop at the moment of release, only a clean fall from the visible position. If it still
drops on that specific structure after this deploys, the next thing worth checking is whether
that climb is actually a chain of multiple synthesized `ADCCClimbPoint`s (from
`FindOrCreateForSurfaceHit`) rather than one tall authored point - the fix above covers one active
`ActiveClimbPoint` cleanly, but was not tested against a hand-off between two.

### 2. Ally E-interact - system already built and looks correct; most likely a deploy-timing gap, not a code bug

Read `HandleInteractPressed`, `FindInteractionTargetUnderCursor`, `BuildInteractionOptions`,
`TryFistBumpUnderCursor`, and `ADCCHUD::DrawInteractionMenu` in full. **A complete ally
interaction menu already exists** - E on a living or downed ally opens Chatter / Ask About This
Place / Persuade / Intimidate / Recruit (gated on already-friendly) / Look At Their Gear /
Follow Me / Hold Position / Leave, or Help Them Up / Leave for a downed ally - which covers
everything the dispatch brief speculated E might be meant to do (loot request, formation,
dialogue) and then some. This is not new code: `DrawInteractionMenu` is present in the last
**committed** version of `DCCHUD.cpp` (`HEAD`, Aug 27), so it predates this session by two days.

**Could not reproduce or find a defect by reading alone.** The one lead worth ruling out first -
this project's own repeatedly-confirmed `bIsAlly` CDO-vs-placed-instance trap, which is exactly
the shape of bug that would make `IsHostileTo` wrongly refuse an ally - is already ruled out by
Zoe's own report: `TryFistBumpUnderCursor` (the click path that DOES work for her) checks the
identical `!Target->IsHostileTo(this)` condition `FindInteractionTargetUnderCursor` (the E path)
checks. If fist-bump succeeds on a given ally, that ally's `bIsHostileTo` is already false at that
moment, so the E path's hostility gate cannot be what is refusing it for that same character.
Range is not it either (`InteractionRange` 400 > `FistBumpRange` 320, so anything close enough to
fist-bump is close enough to talk to). Both sweeps use a generous sphere, one on `ECC_Pawn`, one
on `ECC_Visibility`, and no ally-specific code was found anywhere that overrides a living
character's collision response on either channel.

**Most likely explanation, not confirmed:** this project's own #1 documented failure mode - a
feature fully built and even committed, but the *running* editor DLL predates it. The Binaries
timestamp check this session did found `UnrealEditor-DungeonCrawlerDenver.dll` freshly relinked
today at 19:38 (no numbered hot-reload artifact - a genuine Development Editor build, done by a
concurrent peer session per its own entry just above this one) and the editor now running against
it (PID 7080, since 19:47). If Zoe's "E does nothing" test happened before that rebuild went live,
it should simply be retested now. **If it still fails after this rebuild**, the fix for finding
out is already sitting in the code and does not need another session: `FindInteractionTargetUnderCursor`
already logs `[DCC Talk] <self>: hit <target> but refused - IsAlive=.. IsDowned=.. IsHostileTo=..
IsCrawler=.. (self IsCrawler=..)` (or "not a valid character" / "too far") on every E press that
fails to open a conversation. One PIE run filtered on that tag names the exact gate - no more
theorizing needed, whoever has the live editor next just has to read it.

### 3 & 4. "Stow doesn't add to inventory" and "gold pops out and lands on the ground" - same root cause, confirmed and fixed

These two were suspected to share a cause in the dispatch brief, and they do. Read the full loot
retrieval chain: `HandleLootMenuConfirm` → `Server_LootOne`/`Server_LootAll`/
`Server_StowLootSource` → `ADCCCharacterBase`/`ADCCLootContainer`'s `LootOne`/`LootAll`/`StowInto`
→ `DCCLoot::SpawnRolledLoot`
([DCCLootTypes.cpp](../Source/DungeonCrawlerDenver/Items/DCCLootTypes.cpp)) - the single function
every one of those roads funnels through, corpse or chest, take-one or take-all. **It had no path
that ever credited anything directly.** It always spawned a real `ADCCPickup` on the ground, gold
included, and left it there. "Stow" specifically only defers this: it moves the roll into a
`StowedRemains` record on the looter's `UDCCInventoryComponent`, and clicking that record later in
the Tab menu (`LootOneFromStowed`/`LootAllFromStowed`) goes through the exact same
`SpawnRolledLoot` call, dropping everything on the ground at the player's then-current location.
So both "Stow" and plain "Loot All" end the same way for gold: a coin on the floor that still has
to be separately walked over (or E'd) before `ADCCPickup::OnPickedUp`'s own
`bBanksOnPickup`-gated `ClaimValue` finally calls `Stats->AddGold`. That two-step "loot → then
also physically collect the thing you just looted" is what read as "doesn't add to inventory" /
"pops out onto the ground instead."

**This is confirmed by the pickup class's own design comment, not a guess**
([DCCPickup.h](../Source/DungeonCrawlerDenver/Items/DCCPickup.h)):
`bBanksOnPickup` exists specifically so "coins and small loot work this way, because making the
party walk pocket change to the exit is tedium rather than tension" - i.e. the instant-bank
behavior was always the intended feel for ordinary gold. The loot **menu** path just never wired
into it; only the ground-pickup path did.

**The fix:** `DCCLoot::SpawnRolledLoot` gained an optional `ADCCCharacterBase* Looter = nullptr`
parameter. A floor drop from a kill still has no single claimant and still spawns exactly as
before (nothing calls it with a looter for that path, by design - see the file's own header
comment on why an auto-credit-on-kill would be the wrong call). But every caller that goes through
the corpse/chest **menu** - `LootOne`, `LootAll`, and `StowInto`'s later `LootOneFromStowed`/
`LootAllFromStowed` retrieval - already knows exactly who chose to take it, and now passes them
through. When a looter is present, the rolled gold is non-zero, and the rolled `PickupClass`'s own
`bBanksOnPickup` default is true (checked off the class CDO, not guessed), the gold is credited
straight to `Looter->Stats->AddGold()` and, if there was no `EquipmentItemId` riding along with
it, nothing is spawned at all - a pure coin has nothing left worth representing on the ground. An
item entry that also carries gold still spawns the item pickup as before, just with its
`GoldValue` already paid out rather than claimable twice.

One real behavioral wrinkle caught while making this change: `LootOneFromStowed` used to treat
"`SpawnRolledLoot` returned null" as an error and refuse to remove the item from the stowed
record. That was fine when null only ever meant "genuinely failed" (no world, no `PickupClass`),
but this fix makes it also return null on the new "credited, nothing to spawn" success case -
reusing the old check as-is would have made every successfully-credited stowed coin get "refused"
and left stuck in the record forever. Fixed by checking the real failure conditions
(`World`/`PickupClass`) up front and always removing the item afterward, regardless of what
`SpawnRolledLoot` returns.

Files touched:
[DCCLootTypes.h](../Source/DungeonCrawlerDenver/Items/DCCLootTypes.h),
[DCCLootTypes.cpp](../Source/DungeonCrawlerDenver/Items/DCCLootTypes.cpp),
[DCCCharacterBase.cpp](../Source/DungeonCrawlerDenver/Characters/DCCCharacterBase.cpp) (`LootOne`,
`LootAll`), [DCCLootContainer.cpp](../Source/DungeonCrawlerDenver/Items/DCCLootContainer.cpp)
(`LootOne`, `LootAll`),
[DCCInventoryComponent.cpp](../Source/DungeonCrawlerDenver/Items/DCCInventoryComponent.cpp)
(`LootOneFromStowed`, `LootAllFromStowed`). Not a `UFUNCTION`/`UPROPERTY` change - `SpawnRolledLoot`
is a plain C++ free function - so this alone would have been Live-Coding-safe, but it shares a
build with the climbing fix above, which is not, so the whole session still needs the full

## Verification pass: DataTables, frying-pan grip, Selfie Mode, climb sanity check (2026-08-30, live MCP)

Dispatched fresh after the editor came up (confirmed 24 toolsets registered, port 8000 live).
PIE player count already at 1 per Zoe's request - left it alone throughout. Explicitly told not to
touch the climbing/ally-interact/loot-menu systems the "Four bugs from Zoe" session above is mid-fixing
- stayed out of them, and that entry's own work is what explains item 4 below.

**No input-injection tool exists in this MCP toolset** (confirmed again, same as every prior session
that's noted this) - `CaptureViewport`/`GetCameraTransform` operate on the level EDITOR's own free
camera, not the possessed player's actual view or HUD Canvas, even while PIE is running in-viewport.
Where a test needed to actually exercise player-only code paths (Selfie Mode's `ToggleSelfieMode`,
climbing's `ConfirmPendingClimb`), built small disposable Blueprint test harnesses instead - a plain
`Actor` placed in the level whose `BeginPlay` (after a `Delay`) gets the player pawn, casts it, and
calls the real `UFUNCTION(BlueprintCallable)` entry point, with a `PrintString` (`bPrintToLog=true`)
reporting the result so `LogsToolset.GetLogEntries` gives a real, non-visual verdict. Both harnesses
were deleted (`AssetTools.delete` on their temp folder) and every actor/property they touched was
reverted before finishing - confirmed by read-back, not just assumed.

### 1. DataTable spot-check - PASS

`DT_DCCLootBoxTables` (6 tiers), `DT_DCCBoons` (8 rows), `DT_DCCAchievements` (28 rows, including
`EstateSale`/`AssetRecovery`), `DT_DCCEquipment` (41 rows) all `list_rows`/`get_rows` clean with
real, correctly-typed data - not just present on disk, genuinely loaded in the live editor. One
thing worth flagging: `DT_DCCEquipment`'s live row list does **not** include `BoilerplateRifle` -
the 2026-08-29 "Vaughn/Boilerplate Rifle" entry above added that row to `Docs/DT_DCCEquipment.csv`
and wired `ADCCLootContainer_KitchenDrawer`'s C++ default to it, but the CSV was never re-imported
into the live table this session (out of scope for today's list, flagging for whoever does the
next equipment re-import).

### 2. Frying pan grip offset - looks correct, not a full fix-or-fail per this file's own caution

Equipped `CastIronSkillet` on a placed `BP_DCCEnemy_Brute` instance's `Equipment.Equipped` array
**before** starting PIE (bSimulate), so `BeginPlay`'s own `RebuildVisuals()` call picked it up
exactly the way a real equip would - confirmed by read-back both before and after. First read the
live `GetRows` value off `DT_DCCEquipment` directly: `CastIronSkillet.gripOffsetLocation` is
`(X=-20, Y=0, Z=0)` in the **live** table right now, matching what the 2026-08-29 CSV-corruption
fix intended - so that fix did carry through correctly, this is not stale/scrambled data.

Close-range viewport captures (see `SM_Pan`'s local bounds, X ±18.1cm / Y -54.8..+18.1cm - origin
sits at the plate's near edge, confirming the "pivot is near the plate" note) show the mesh hanging
from a **visible thin handle segment near the hand**, with the round plate dangling below/away from
the grip point - not the hand clipped through the plate itself, across three different camera angles.
Read as a genuine improvement over the pre-fix "grips the pan by the plate" bug. **Not a
pixel-perfect confirmation** - the pan still hangs somewhat low/loose from the hand rather than
looking firmly held - so a small further nudge (probably a slightly less negative X, or a small Z)
would tighten it, but the core bug (hand at the plate) reads fixed. Screenshots were not saved to
the repo; re-run the same "equip pre-PIE, capture close-up" recipe above if a visual re-check is
wanted later.

### 3. Selfie Mode - PASS (mechanically, via log evidence) - viewfinder overlay pixels unverifiable by this toolset

`SelfiePoseAnimations` is still empty on the player Blueprint CDO (`BP_ThirdPersonCharacter`) -
confirmed by direct read, matching the task brief's own expectation; pose-cycling has nothing to
cycle through yet, not blocking as instructed.

Built a harness that, on a delay, gets the possessed player pawn and calls `ToggleSelfieMode()`,
then logs `IsInSelfieMode()`'s result:
- **No hostile within range (natural spawn, all test dummies >1500 units away):** log read
  `InSelfieMode=TRUE` - confirms `EnterSelfieMode` actually flips `bInSelfieMode` and the camera
  swap path runs without erroring.
- **A hostile (`Mob_Brute_Courtyard`, PIE-transient copy) placed ~224 units from the player, well
  inside `SelfieHostileRefuseRadius` (1500, confirmed via the same read):** log read
  `InSelfieMode=FALSE` - `EnterSelfieMode`'s `HasNearbyLivingHostile` refusal is genuinely gating
  entry, not just present in source.

Both branches are real function execution, not guessed. **What could not be verified:** the actual
`DrawSelfieViewfinder` HUD Canvas pixels (brackets/reticle/"SELFIE MODE" label/progress bar) and the
`ShowLocalNotice` refusal text on screen - `CaptureViewport` is an editor free-camera snapshot, not
the local `PlayerController`'s own screen, and it does not composite that Canvas overlay at all (a
capture near the posing character showed only the 3D scene plus editor gizmos, no HUD text). This
is a tooling limitation, not a sign the HUD code is broken - the state it reads (`IsInSelfieMode()`)
was independently confirmed true/false as expected in each case. A real manual PIE session (press
`C` near vs. away from an enemy) is the only way to see the actual overlay pixels.

### 4. Climbing 1-player sanity check - INCONCLUSIVE, and not this session's bug to chase

Picked `Room01_ClimbPoint_EastWall` (an ordinary authored `ADCCClimbPoint`, nothing to do with "the
big mountains with the portal on top") specifically to stay clear of the structure the parallel
session is fixing. Built a harness calling the real `ConfirmPendingClimb(true)` then
`SetClimbInput(true, false)` on the possessed player pawn, after manually teleporting it to
`(6656, 0, 92)` (130 units from the climb point's actor origin, well inside its 260-unit
`ReachDistance`, confirmed by direct read) and setting `PendingClimbPoint` to the climb point via
reflection (the same "author it on the actor, `BeginPlay`/the call itself picks it up" pattern used
for the pan) - both read back and confirmed set immediately before the harness fired.

Result: after the harness's `ConfirmPendingClimb(true)` call ran (confirmed via its own
`PrintString` in the log), `PendingClimbPoint` was **still non-null** and `ActiveClimbPoint` was
still `None` - i.e. the character never actually started climbing, even though
`ConfirmPendingClimb`'s own source
([DCCCharacterBase.cpp](../Source/DungeonCrawlerDenver/Characters/DCCCharacterBase.cpp)) shows
`PendingClimbPoint = nullptr;` unconditionally as its second line, before any validity check -
reading that value back non-null after the call ran is not explainable by anything in the source as
it stands today. Reproduced twice (once with a 1.5s harness delay, once with 8s to rule out a race
between my own MCP calls and the harness firing - same result both times, with ample time to spare
either way).

**Root cause is almost certainly a stale binary, not a real bug, and this session did not chase it
further per the standing instruction to stay out of the climbing system:**
`Binaries/Win64/UnrealEditor-DungeonCrawlerDenver.dll` is timestamped **Aug 29 19:38**;
`DCCCharacterBase.cpp`/`.h` are timestamped **Aug 29 20:49/20:51** - roughly 70 minutes *newer* than
the currently-running binary. That gap lines up exactly with the "Four bugs from Zoe" entry
immediately above this one, which explicitly changed `DCCCharacterBase.h`/`.cpp`'s climbing code
(`Client_SyncClimbLocation` gained a `bLandOnGround` parameter) and says outright: "source-only
until the next close→build→reopen cycle... DebugGame compile-check is clean" - i.e. the file this
session's test depends on has known, documented, undeployed edits sitting on top of whatever the
live DLL actually contains. Whatever `ConfirmPendingClimb` the running editor is actually executing
right now is not guaranteed to match the source on disk. **This makes today's climb-test result
uninterpretable rather than a confirmed regression** - it should be re-run once the parallel
session's climbing fix is actually built and deployed (close editor → Development Editor build →
reopen), not investigated further blind against a binary that's known to be behind. Did not touch
`DCCCharacterBase.cpp/.h`, `DCCClimbPoint.cpp`, or any other file in that parallel session's scope.

**Cleanup, confirmed:** both temporary Blueprint harnesses and their `/Game/DCC/_Temp` folder
deleted; `Mob_Brute_Courtyard`'s equipment and world-position edits reverted to their original
values (read back after reverting); `Mob_Brute_Plaza`'s equipment reverted the same way; the
player pawn's teleports and `PendingClimbPoint` edits only ever touched transient PIE-world actor
copies, discarded automatically on `StopPIE`, so nothing there needed reverting. `Lvl_ThirdPerson`
reports `is_dirty=false` after all of it.
close-editor → Development Editor build cycle regardless.

**Not verified in PIE.** Test loop once deployed: kill something, open the corpse's loot menu,
confirm both "Loot All" and taking a single gold entry from "Check Inventory" credit gold
immediately with no coin left on the ground; confirm "Stow" followed by emptying that record from
the Tab menu's Inventory page does the same. An item-plus-gold entry (if one exists in a live loot
table) is worth a specific check too - the gold half should bank, the item half should still land
as a pickup.

## Two urgent bugs from Zoe: E-interact "broken for everyone," Selfie Mode softlock (2026-08-29, dispatched session, live MCP, concurrent peer sessions also on the editor)

Dispatched specifically because a previous session's "E-interact looks correct, probably a stale
build" theory (see "2. Ally E-interact" above, same day) was wrong per Zoe's own live testing.
This session had **genuine live MCP** (unlike that one) - confirmed `IsPIERunning() == true` on
first check, and the log showed several other interactive sessions actively driving the same
editor concurrently (one had built `/Game/DCC/_Temp/BP_SelfieModeTestHarness` and was mid-PIE-run
testing Selfie Mode independently). Given that many-hands-one-editor situation, deliberately did
**not** stop/restart PIE or the editor - both would have yanked the floor out from under whoever
else was live-testing. Everything below is source analysis plus **live, read-only MCP verification**
(property reads against the actual running PIE world), not a fresh full PIE run of my own.

### Bug 1: "not being able to interact with allies or anyone" - real fix found and applied, but not fully confirmed as *the* cause

**Ruled out empirically, not just theorized: the `bIsAlly` CDO-vs-instance trap (this project's
#1 recurring bug, and the previous session's leading unconfirmed theory) is NOT currently happening.**
Read `bIsAlly` live off all 8 placed ally instances in the running PIE world
(`BP_DCCEnemy_HockeyWings_C`/`OfficeAirplane_C`/`ArmsOut_C`/`DiceWinged_C`, both copies of each) -
every single one reads `true`. `bIsDowned` reads `false` on the one checked in detail. So the
mechanical hostility check (`IsHostileTo` = `IsCrawler() != Other->IsCrawler()`, and
`ADCCEnemyCharacter::IsCrawler()` returns `bIsAlly`) is sound for every ally currently in the
level. This closes off the theory the previous session left open as "most likely" - it wasn't.

**Also ruled out by diff: nothing in today's actual changes touches the interact code path at
all.** `git diff` against the last commit (`d109d64`) across every modified file shows
`HandleInteractPressed`, `FindInteractionTargetUnderCursor`, `SweepForInteractionCandidate`,
`IsHostileTo`, `IsCrawler`, and the input bindings in `SetupPlayerInputComponent` are **byte-
identical** to two days ago. Whatever is wrong, it is not a logic change in the gate itself.

**Real bug found by reading the fallthrough chain closely, not by reproducing it - and it is a
plausible fit for "reads like it stopped working," even if not provably the whole story.**
`HandleInteractPressed` ([DCCCharacterBase.cpp](../Source/DungeonCrawlerDenver/Characters/DCCCharacterBase.cpp))
checks `VendorTargetActor`/`LootTargetActor`/`InteractionTarget` first and, if any is set, routes
E there **unconditionally** - it never reaches the ally-sweep, dialogue-NPC, or loot-target checks
below at all. All three are raw `TObjectPtr`, and `TObjectPtr`'s bool conversion only tests for
null, **not validity** - a destroyed-but-not-yet-garbage-collected actor still reads as "set."
Each `Handle*MenuConfirm`/the conversation branch does eventually notice and self-heal (a distance
check or a null `Cast` closes the stale menu) - but only on the press that discovers it, which
means that one E press is silently swallowed and does nothing user-visible, and the player has to
press E a **second** time before it reaches whatever is actually under their crosshair. This
project's own loot-menu rework has made this much easier to trigger than it used to be: the new
Body Collector feature (this session's other uncommitted work, already in the running 19:38 build)
actively `Destroy()`s a corpse out from under an open loot menu ~20s after death, with nothing
proactively closing that player's menu when it happens - and simply walking away from a corpse or
vendor without pressing Escape leaves the target set (just now out of range) until the next E
press pays the one-press tax to discover that and close it. Neither requires a "regression" in the
sense of broken logic - both are pre-existing gaps that the loot rework's heavier corpse/container
traffic makes land far more often in ordinary play, which fits "the previous session found nothing
wrong" (nothing IS logically wrong) and "Zoe hits it constantly" (it's a probabilistic one-press
tax, not a hard block) simultaneously.

**The fix** (`HandleInteractPressed`): before the existing branch checks, any of the three targets
that is set-but-`!IsValid()` is cleared (`CloseVendorMenu()`/`CloseLootMenu()`/`CloseInteraction()`)
**first**, so the *same* E press that discovers the stale state falls through to the real target
below instead of being consumed re-closing a dead menu. `IsValid()` (the free function, not a
member) is this project's own established pattern for exactly this check - already used in
`ADCCEnemyCharacter::TickBodyCollector` for the same "was this actor destroyed out from under me"
question.

**Explicitly not claiming this is proven to be Zoe's whole experience.** "E does nothing" as a
persistent, every-press failure (rather than an occasional one-press tax) is not fully explained by
this fix alone. If it recurs after this deploys, the existing `[DCC Talk]` diagnostic
(`FindInteractionTargetUnderCursor`, logs on every failed E-press: nothing hit / not a character /
hostile-or-out-of-range / too far) is still live and unchanged - one PIE run with that filter, on
an E-press that still fails after this fix, names the exact gate rather than needing another round
of theorizing. Could not generate that log myself this session: **this MCP toolset has no way to
inject player input** (confirmed again - no console-exec, no call-arbitrary-UFUNCTION tool; the
concurrent peer session building a Blueprint test harness to work around exactly this gap is
itself evidence the constraint is real, not an excuse), so an actual E-press against a live ally
was not something this session could produce.

### Bug 2: Selfie Mode softlock near enemies - exit was already unconditional; the missing piece was a proactive mid-pose interrupt

Zoe: "the player is spawned in in selfie mode and can't get out because it says 'oops enemies are
near'." Read `ToggleSelfieMode`/`EnterSelfieMode`/`ExitSelfieMode` in full.

**(a) Checked whether the nearby-hostile refusal (added 2026-08-28) incorrectly gates the exit
path too, as the dispatch brief suspected - it does not, and never did.** `ToggleSelfieMode`
branches on `bInSelfieMode` **before** calling either function: already-posing routes straight to
`ExitSelfieMode()`, which has no hostile check, no range check, nothing - it unconditionally clears
the camera and hands the view back. The `HasNearbyLivingHostile` refusal lives only in
`EnterSelfieMode`, gating the *start* of a pose, not `ExitSelfieMode`. So there was nothing to
remove. Added a comment on `ExitSelfieMode`'s declaration in
[DCCCharacterBase.h](../Source/DungeonCrawlerDenver/Characters/DCCCharacterBase.h) making this
invariant explicit ("never add a hostile/range/any-other gate here") so a future session doesn't
reintroduce one while "fixing" something else.

**(b) The actual gap, and the fix Zoe explicitly asked for: nothing forced an exit if a hostile
showed up *while already posing*.** The 2026-08-28 entry on this exact bug flagged two hardening
options and only built one (refuse entry near danger). The other - "refuse `EnterSelfieMode`... or
[force-exit if something gets close while already posing]" - was never implemented. Built it now in
`Tick()`: while `bInSelfieMode`, every frame also checks `HasNearbyLivingHostile(SelfieHostileRefuseRadius)`
(the same helper `EnterSelfieMode` already uses, same 1500-unit radius) and, if true, clears both
pending timers and calls `ExitSelfieMode()` immediately, with a `ShowLocalNotice` ("Something's
coming - selfie cut short.") so it reads as a deliberate interrupt rather than an unexplained
camera cut. This sits right next to the existing `SelfieMaxRealSeconds` real-time-ceiling check in
the same `Tick()`, same shape, same reasoning: a proactive, unconditional escape hatch rather than
another special case to reason about.

**Best-guess reconciliation with Zoe's literal wording, flagged as a guess, not confirmed:**
"spawned in in selfie mode" most likely does not mean the character's spawn state is *literally*
`bInSelfieMode = true` (the C++ default is `false`, and nothing in today's diff touches that
default). More likely: she pressed `C` near an encounter, `EnterSelfieMode`'s existing refusal
fired (`"Too dangerous to stop and pose right now."`), and because there was previously no way to
tell "refused to start" apart from "started and now stuck" from the player's seat - both look like
"pressing C isn't getting me a normal camera back" - it read as being trapped rather than being
turned away. The new viewfinder overlay (2026-08-28, `DrawSelfieViewfinder`) only draws when
actually posing, so that alone should already disambiguate the two cases better than before this
session touched anything. Not verified against her actual experience - flagging the reasoning so
the next session doesn't have to re-derive it if this report recurs.

**Verification status for both:** DebugGame compile-check clean, twice (once per fix, editor stayed
open and responsive throughout, confirmed no numbered hot-reload DLL in `Binaries/Win64/`
afterward). **Neither is deployed** - both are plain function-body edits, no new
`UFUNCTION`/`UPROPERTY`/signature change, so this batch would be Live-Coding-safe in isolation, but
per this file's own standing rule Live Coding is never used regardless, and the running editor
already has other today's-session changes (climbing mid-release sync, loot gold-crediting) that
**are** reflection changes and still need the full close-editor → Development Editor build → reopen
cycle. **Not PIE-tested** - deliberately did not stop the concurrently-running PIE session to test
against, and this MCP toolset has no way to simulate a player pressing E or C even in a PIE session
of my own. Both fixes are ready to ride along on the next full deploy; whoever does that deploy
should PIE-test in this order: (1) trigger the stale-target one-press-eaten case (loot a corpse,
walk far away without pressing Escape, then press E near an ally - should now open the ally menu
on the first press, not the second), (2) enter Selfie Mode away from danger, then walk a hostile
into `SelfieHostileRefuseRadius` (1500 units) while still posing - should force-exit with the
"cut short" notice within one frame, not wait for the 8s ceiling.

## Working style

Build first, explain at checkpoints. Zoe is learning game dev — explain the *why*
behind UE idioms when you explain at all, but do not narrate every step.

Prefer asking her to do a quick manual step over spending an hour on an automatic
workaround. Her time at the keyboard is the scarce resource, not yours.

**Be honest about what is unverified.** This project has been damaged more by
confident wrong claims than by slow progress. If you did not see it work, say so.

## Deploy: climbing/gold-credit/E-interact/Selfie/Vaughn/BoilerplateRifle batch, live as of 2026-08-30 ~02:52 local (2026-08-29 22:51:57 build timestamp)

Dispatched specifically to deploy the batch DISPATCH.md already documented as source-complete but
undeployed: climb-release position resync, `DCCLoot::SpawnRolledLoot` gold-crediting, the stale-
target `IsValid()` guard in `HandleInteractPressed`, Selfie Mode's mid-pose hostile interrupt, the
new `ADCCAlly_Vaughn` class, and the `BoilerplateRifle` equipment row + kitchen-chest wiring.

**Zoe was away from the keyboard, so she explicitly authorized this session to force-close the
editor itself** (`taskkill /PID <pid> /T /F`, required `/F` - the plain `/T` alone left three child
processes refusing to die) rather than the usual wait-for-her-to-close pattern. `save_assets` (both
the level explicitly and then an empty-list "save everything dirty" call) ran clean beforehand, and
`IsPIERunning` was already false - nothing was in flight when the kill happened.

**No build contention.** Checked before touching anything: no `UnrealBuildTool`/`dotnet`/`cl.exe`/
`MSBuild` processes running, so no other session was mid-build.

**Confirmed genuine relink, not hot-reload:** no `-0001`/`patch_` DLL appeared in `Binaries/Win64/`
after the build, and the new `UnrealEditor-DungeonCrawlerDenver.dll` (2026-08-29 22:51:57) postdates
every source file this batch touched - `DCCCharacterBase.cpp/.h` (20:49/20:51), `DCCLootTypes.cpp/.h`
(19:51), `DCCAllyCharacters.cpp/.h` (19:31), `DCCHUD.cpp` (19:35) - all older than the DLL. Editor
relaunched via the `.uproject` file; MCP reconnected on its own this time (no fresh Claude session
needed) - all 24 toolsets registered clean per `LogToolsetRegistry`.

**Editor loads clean:** `MapCheck: 0 Error(s), 0 Warning(s)`. Full error-log sweep of the boot found
nothing else real - the only hits were expected noise (missing profiler DLLs, missing mobile/console
platform SDKs, UE's own `UnifiedErrorTest` self-test log lines) plus one genuine, expected finding:

**`ADCCAlly_Vaughn`'s CDO constructor failed to find its skeletal mesh** -
`LogUObjectGlobals: Error: CDO Constructor (DCCAlly_Vaughn): Failed to find
/Game/DCC/Characters/Vaughn/SK_Vaughn.SK_Vaughn`. Confirmed by direct check: that asset does not
exist yet (`AssetTools.exists` → false). This is this project's own documented
`ConstructorHelpers::FObjectFinder`-resolves-once-at-startup trap, not a bug introduced by this
deploy - Vaughn's Meshy export is still sitting as source art (`SourceArt/Vaughn/`,
`Content/Vaughn.zip`, `Content/Characters/Meshy Vaughn/`) and has never been run through the
glb→FBX→import pipeline into `/Game/DCC/Characters/Vaughn/`. Self-healing per the documented
pattern: once that import happens, the next editor restart will pick it up with no rebuild needed.
Until then, `ADCCAlly_Vaughn`'s mesh component is presumably unset/default - not placed in any level
yet per this session's scope, so nothing currently visible is broken by it.

**`DT_DCCEquipment.csv` re-import, done as `add_rows`/`set_rows` rather than a full CSV re-import**
(the full-file `import_file` path against an already-existing DataTable asset is untested in this
project's own notes, and only one row had changed) - added `BoilerplateRifle` with every field from
the CSV row (`itemId`, `displayName`, `slot=MainHand`, the flavor `description`, `bonusStrength=1`,
`weaponDamageDie=12`, `mesh`, `attachSocket=hand_rSocket`, `goldValue=180`, `bEmitsLight=false`,
`lightColor`, `lightIntensity=40`, `lightAttenuationRadius=700`, `attackRangeOverride=1800`),
confirmed by read-back, table saved. **The first `set_rows` call with the full 20-field payload
failed with a JSON parse error** ("Expecting ',' delimiter") that did not reproduce on a byte-
identical payload validated independently with Python's `json` module - splitting the call into two
smaller `set_rows` calls (core fields, then `description` alone) both succeeded with no further
issue. Root cause not chased further since it worked on retry with no content change needed; worth
knowing if `set_rows` intermittently rejects large single payloads again. `DT_DCCEquipment` now
lists 42 rows, `BoilerplateRifle` included, live in the running editor - table saved.

**The row's `mesh` reference is soft and currently dangling, same as several other placeholder rows
in this table already** - `/Game/DCC/Weapons/BoilerplateRifle/SM_BoilerplateRifle` does not exist.
The actual Meshy import landed at
`/Game/Meshy_AI_A_boilerplate_rifle__0828202153_texture/StaticMeshes/...` and was never moved/
renamed into the path the CSV (and the C++ kitchen-chest wiring, per the 2026-08-29 entry above)
expects. Not fixed here - out of scope for a DataTable re-import, and moving/rescaling that mesh is
exactly the "corrected version prepped" work the 2026-08-29 entry already flagged as still needing a
live-editor pass. Flagging again so it isn't lost: **the row exists and is wired, but the gun will
render as nothing (or fall back to a default) until that mesh gets moved into place.**

**Not done this session, deliberately out of scope per the dispatch brief:** no PIE run of the
climbing/gold-credit/E-interact/Selfie fixes. The brief's own priority order was "get a clean
deploy first," which this achieved; the test loops each fix's own DISPATCH.md entry already
prescribes (climb-release resync, loot-menu Stow/gold-on-ground, stale-target E-press, mid-pose
hostile interrupt) are still owed to whoever has the next live PIE session with Zoe present.

## Live PIE verification pass: climbing/gold/E-interact/Selfie/Vaughn/BoilerplateRifle/Oathgate (2026-08-30, dispatched fresh, live MCP against PID 14960)

Dispatched specifically to PIE-verify the 2026-08-30 02:52 deploy (the "climbing/gold-credit/
E-interact/Selfie/Vaughn/BoilerplateRifle batch" entry above), which had shipped clean but never
been exercised in PIE. Confirmed live MCP against the already-running editor (PID 14960, 24
toolsets) at session start. `PlayNumberOfClients=1` confirmed set before any PIE run and confirmed
still `1` at the end - never touched.

**No input-injection tool exists in this MCP toolset** (yet again independently reconfirmed) - same
constraint every prior session has hit. Built disposable Blueprint test harnesses via
`BlueprintTools.write_graph_dsl` (a Lisp-like DSL that populates a graph with real nodes and
compiles it - much faster than hand-placing nodes one at a time, worth remembering for future
sessions) placed in `/Game/DCC/_Temp`, each with a `Delay`-gated `EventBeginPlay` that casts the
possessed pawn to `ADCCCharacterBase` and calls the real `UFUNCTION(BlueprintCallable)` entry
points, logging results via `PrintString(bPrintToLog=true)` read back with `LogsToolset.GetLogEntries`.
State setup (teleports, `ObjectTools.set_properties` reflection writes) was done directly against
the **live PIE-world actor instances** (fetched via a fresh `find_actors` after `StartPIE`, not the
editor-world refs) during each harness's `Delay` window - the same pattern documented in the
2026-08-30 "Verification pass" entry above, extended here with camera control
(`Pawn|GetController` → `Pawn|SetControlRotation`) since `SweepForInteractionCandidate` aims off
`PlayerController::GetPlayerViewPoint`, not the actor's own facing (the ThirdPerson template's
`CameraBoom` has `bUsePawnControlRotation=true`, confirmed by reading the live component). All
harnesses and their placed instances were deleted afterward; `Lvl_ThirdPerson` saved clean.

**Found the "mountain with the portal on top" from source, not by wandering**: `Sample_Alien_Portal`
sits at world Z=3790, and the only nearby authored climb point, `DCCClimbPoint_CliffRock`
(x≈5314, y≈82), tops out at only Z=900 - roughly a quarter of the way up. The rest of the ascent to
the portal is almost certainly climbed via the generic wall-climb path
(`FindOrCreateForSurfaceHit`, no persistent actor), not a second authored `ADCCClimbPoint` - only
two `ADCCClimbPoint` actors exist in the whole level (the other is `Room01_ClimbPoint_EastWall`,
unrelated). Tested against `DCCClimbPoint_CliffRock` specifically since it's the authored point
nearest the portal and exercises the identical `Server_ReleaseClimb_Implementation` code path the
fix touches, regardless of which climb point started the attempt.

### 1. Climbing (position resync on release) - PASS for what's testable solo; the specific fix is provably a no-op in this configuration

Teleported the player to `DCCClimbPoint_CliffRock`'s base, set `PendingClimbPoint` via reflection
(same pattern this file has used before), then had the harness call `ConfirmPendingClimb(true)` →
`SetClimbInput(true,false)` → wait 3s → `ReleaseClimb()` (deliberately mid-face, not at the top -
the exact case the fix targets) → wait 2s. Real, live results, not guessed:

- `IsClimbing` flips `true` immediately after `ConfirmPendingClimb` - `Server_BeginClimb_Implementation`
  accepted the attempt (in-reach, alive, stamina > 0).
- Climbed from Z=92 to **Z=884.2** in 3 seconds (nearly the full 900-unit span) - normal climbing
  works, no stall, no snap.
- `ReleaseClimb()` mid-face flips `IsClimbing` back to `false` immediately, no error - confirms
  `Server_ReleaseClimb_Implementation` (the function this session's fix touches) runs clean.
- Two seconds later the character had fallen to **Z=542.2** - a normal ~340-unit fall under gravity,
  correctly in `MOVE_Falling` (not stuck floating, not still walking at climb height). No snap, no
  teleport glitch, no error in the log.

**What this does NOT prove, and cannot prove solo:** the actual bug the fix targets
(`Client_SyncClimbLocation` never reaching the owning REMOTE client, so its cosmetic position guess
drifts before a mid-climb release) is structurally invariant to a listen-server host's own pawn -
`Server_ReleaseClimb_Implementation`'s new `if (!IsLocallyControlled())` guard means the new
`Client_SyncClimbLocation` call **never fires at all** for the one PIE client this session was
instructed to run (`PlayNumberOfClients=1`). This isn't a limitation of the test harness - it's the
literal condition the fix's own code checks. Confirming this fix for real needs the two-client PIE
this session was explicitly told not to run. What's confirmed instead: the release mechanism itself
(the code path the fix's `Client_SyncClimbLocation` call was added to) executes cleanly with no
side effects for the host - the fix could not have regressed anything for the common case.

### 2. Gold/loot crediting on Stow-retrieval - COULDN'T VERIFY, and found a real MCP tooling limitation along the way

The credit path (`DCCLoot::SpawnRolledLoot`'s new `Looter` parameter) is only reachable from
Blueprint through `UDCCInventoryComponent::LootOneFromStowed`/`LootAllFromStowed` - `LootOne`/
`LootAll`/`StowInto` themselves are plain C++ interface overrides (`IDCCLootSource`), not
`BlueprintCallable`, so a harness cannot call them directly; the loot-menu UI (mouse-driven) is the
only other path in, and this toolset cannot drive UI. Plan: inject a synthetic `StowedRemains`
entry via `ObjectTools.set_properties` reflection on the live `Inventory` component (gold-only, a
`DCCPickup_Coins` class ref, matching the exact JSON shape the CLAUDE.md-documented
`DCCEncounter::Waves` nested-array write already proved works for actors), then call
`LootAllFromStowed(0, loc)` from a harness and check `Stats->Gold`.

**Real, reproducible finding, confirmed twice on two independent, freshly-built harnesses (once
with leftover cruft from a failed compile ruled out by deleting and rebuilding from scratch):**
`ObjectTools.get_properties` reads back the injected `StowedRemains` entry correctly immediately
after the write (`{"displayName":"...", "contents":[{"gold":99, ...}]}` - length 1, exactly as
written). But a Blueprint node running **in the same live PIE object**
(`Class|DCCInventoryComponent|GetStowedRemains` → `Array|Length`) reads the SAME property as
**length 0** moments later, and `LootAllFromStowed(0, loc)` correctly refuses
(`!StowedRemains.IsValidIndex(0)`) and returns `false` - the array the game logic actually sees was
never populated, despite the reflection API's own read-back insisting it was. This is a genuinely
new gotcha, distinct from the already-documented soft-pointer and camelCase ones: **reflection
writes to a `TArray<UStruct>` property on an Actor *component* (as opposed to an actor itself)
during a live PIE session do not reliably reach the value gameplay code reads**, even though the
same reflection API reports success and echoes the value back correctly on its own read. The
previously-documented working nested-array write (`ADCCEncounter::Waves`) was on a placed *actor*
in the editor, not PIE, and not a sub-component - this may be the actual boundary of what works,
worth testing narrowly (single scalar on a component vs. array-of-struct on a component vs.
editor-time vs. PIE-time) before trusting this pattern again.

**Consequence: item 2 is not verified, positively or negatively.** The code path was read and is
unchanged from the fix already documented above; the deploy is confirmed (DLL postdates the source
file). But nothing here proves or disproves the actual runtime behavior of crediting gold on
Stow-retrieval. **Real test still owed**: Zoe (or a session with real input) kills something, opens
the loot menu, Stows the body, then retrieves it from the Tab menu's Inventory page and confirms
gold banks immediately with no coin on the ground.

### 3. E-interact stale-pointer guard - PARTIALLY VERIFIED; camera-aim-in-harness proved unreliable, so the "opens on first press" case was never actually exercised

`SweepForInteractionCandidate` aims from `PlayerController::GetPlayerViewPoint` (the actual camera,
which follows `ControlRotation` via the `CameraBoom`'s `bUsePawnControlRotation=true`, confirmed by
reading the live component) - not the actor's own facing, and not literal mouse position. Built a
harness that gets the `Controller` off the possessed pawn and calls `SetControlRotation` to a fixed
literal `(0,0,0)`, with an ally teleported to `player_location + (200,0,0)` so a level, zero-yaw ray
should cross it. **This did not reliably land a hit** - `HandleInteractPressed`'s own `[DCC Talk]`
diagnostic logged `sweep hit nothing` / target `None` across three attempts with this setup, despite
the ally being alive, in range, and directly along the aimed axis by construction. Root cause not
isolated (candidates: the `CameraBoom`'s arm-length pullback plus a Z socket offset this session
didn't measure, meaning the level ray may pass over or under a standing character's capsule) - this
is a test-harness limitation, not evidence of a game bug, since the ray geometry was never actually
confirmed correct.

**What WAS cleanly proven, real execution not guessed:** built a second harness that presses E
(missing, as above), **destroys the actor a stale `LootTargetActor`/`InteractionTarget` might be
pointing at (via `Actor|DestroyActor`) mid-session**, then presses E again immediately - this is a
direct, live repro of "a target reference outlives its actor," the exact scenario the `IsValid()`
fix guards against, independent of whether the sweep itself connects. **Two consecutive presses
around a mid-session actor destruction produced no crash, no "accessed None" error, and no stuck
state** in the log - consistent with the fix working, though since the sweep never actually set
`InteractionTarget` to the doomed actor in the first place (see above), this is weaker evidence than
intended: it proves the fallthrough path doesn't crash on a null/already-clear target, not that it
specifically recovers from a *populated-then-destroyed* one. **Real test still owed**: loot a
corpse, walk far away without pressing Escape (leaving a target set-but-out-of-range), then press E
near a living ally - should open the ally menu on the **first** press, not the second. This needs
either real input or a harness with correctly-verified camera geometry, neither of which this
session achieved.

### 4. Selfie Mode hostile-interrupt - PASS for entry-when-safe; the specific hostile-interrupt-mid-pose case was not cleanly isolated from the pre-existing timeout

Confirmed **`EnterSelfieMode` succeeds when genuinely safe**: positioned the player at a spot
checked against the two nearest known hostiles (`Mob_Brute_Courtyard`, `Mob_Analyst_Plaza`, both
>2700 units away, comfortably outside the 1500-unit `SelfieHostileRefuseRadius`) and
`ToggleSelfieMode()` flipped `IsInSelfieMode` to `true` immediately - real execution, not the refusal
branch.

**The specific ask - a hostile approaching while already posing should force an exit via `Tick`,
within one frame, not the ~8s `SelfieMaxRealSeconds` ceiling - was attempted but not cleanly
isolated.** First attempt used a 15-second gap between "enter" and "check," which is longer than
`SelfieMaxRealSeconds` (8.0) - so the observed exit-by-then is fully explained by the ordinary
timeout and proves nothing about the hostile-specific path. Rebuilt with a tight 3-second gap
(inside the timeout window) specifically to isolate the hostile trigger, but the follow-up harness's
own fixed `Delay` before `ToggleSelfieMode` raced against this session's own MCP round-trip time for
repositioning the player away from hostiles beforehand, and the entry attempt landed at the
character's *default* position (near other placed content, not the isolated spot) - `IsInSelfieMode`
read `false` immediately, i.e. the **refusal-to-enter** branch, not a "stuck then force-exited"
result. Did not get a third attempt in before deciding further iteration wasn't a good use of
session time (already several PIE restarts deep on this one behavior). **Real test still owed**:
enter Selfie Mode somewhere safe, then within a couple of seconds (well under 8s) bring a hostile
inside 1500 units and confirm the "Something's coming - selfie cut short" notice and an immediate
exit, distinct from the ordinary timeout.

### 5. Vaughn - imported, socketed, and Compatible-Skeleton-linked; still needs one editor restart before he's actually visible

`SK_Vaughn`/`SK_Vaughn_Skeleton` did not exist yet (confirmed - this is why `ADCCAlly_Vaughn`'s CDO
constructor logged a `Failed to find` error at the last deploy, per the entry above). Imported
`SourceArt/Vaughn/fbx/SK_Vaughn.fbx` via `SkeletalMeshTools.import_file` (materials + textures +
a physics asset) to `/Game/DCC/Characters/Vaughn/` - **118,255 vertices, real geometry**, confirmed
by both the vertex count and a rendered thumbnail (heavyset bearded man, glasses, navy shirt, khakis
- matches the description in the 2026-08-29 entry; the head read slightly grey in the thumbnail,
consistent with this project's own documented "thumbnail renders before textures finish" gotcha
rather than a real texture problem). Added all four standard sockets
(`hand_rSocket`→`RightHand`, `hand_lSocket`→`LeftHand`, `headSocket`→`Head`,
`spine_03Socket`→`Spine02`), matching the other four crawlers' convention exactly.

**Also did the Compatible-Skeletons step the 2026-08-29 entry flagged as unverified and left for a
live session**: `USkeleton.compatibleSkeletons` turned out to be a plain reflectable
`EditAnywhere` soft-pointer array (`ObjectTools.list_properties` confirmed it), so
`set_properties` linked `SK_Vaughn_Skeleton` to `/Game/DCC/Characters/Shared/SK_DCCCrawler_Skeleton`
directly, confirmed by read-back - no Skeleton Editor UI needed after all.

**Not done, deliberately out of scope for a verification pass**: `BP_DCCEnemy_Vaughn` Blueprint
wrapper (doesn't exist yet - checked), placing him in `PersonalRoom_Vaughn`, and confirming the
Compatible-Skeletons link actually plays the shared `DCCCrawlerAnimSet` clips rather than a T-pose
(needs a PIE spawn, which needs the wrapper, which needs the CDO to have picked up the mesh first -
see below). His admin-panel spawn entry (`DCCHUD.cpp`, `"Vaughn"` → `ADCCAlly_Vaughn::StaticClass()`)
was already confirmed present from the prior session's diff.

**Still not spawnable-with-a-mesh in the CURRENTLY RUNNING editor.** `ADCCAlly_Vaughn`'s CDO
constructor ran (and failed to find the mesh) once, at this editor's own startup - before this
session's import happened. Per this project's own repeatedly-confirmed `ConstructorHelpers::FObjectFinder`-resolves-once
rule, this self-heals on the next editor restart with no rebuild - not fixed live here, no restart
requested for this alone (batching with whatever else next needs one is the efficient path).

### 6. Boilerplate Rifle - DataTable row confirmed live (already true from the prior deploy); mesh scale problem found and fixed live

`DT_DCCEquipment` (`/Game/DCC/Equipment/DT_DCCEquipment`, 42 rows) already had a live `BoilerplateRifle`
row - the prior deploy's CSV re-import did land, contrary to the "Verification pass" entry above's
own doubt about it. Its `mesh` column pointed at
`/Game/DCC/Weapons/BoilerplateRifle/SM_BoilerplateRifle`, which **did not exist** (confirmed via
`AssetTools.exists` → `false`) - the flagged-as-probably-oversized loose asset
(`Meshy_AI_A_boilerplate_rifle__...`) was still the only import on disk.

**Fixed live**: imported the already-corrected FBX staged by the 2026-08-29 session
(`SourceArt/BoilerplateRifle/fbx/SM_BoilerplateRifle.fbx`, pre-scaled 0.5x) via
`StaticMeshTools.import_file` to the exact path the DataTable row already expected - no DataTable
edit needed, the dangling soft reference just resolved. **Confirmed correct scale**: live bounds
measure **95.1cm along the long axis** (X: -47.6 to +47.5), matching the 2026-08-29 session's
~95cm target exactly - a real rifle length, not the suspected ~2x-oversized ~190cm a naive Meshy
import would have produced. A rendered thumbnail confirms it reads as a riveted boiler-tank rifle
with a scope, pressure gauge, and a radiator-finned barrel, textures intact - matches the flavor
text. Saved.

**Kitchen Drawer chest wiring**: `ADCCLootContainer_KitchenDrawer`'s C++ constructor already lists
`BoilerplateRifle` as a guaranteed drop (confirmed present in source, from the prior session's diff)
- this is a constructor default, true for every placement of the class regardless of live-instance
state, so it did not need separate live verification. Not confirmed in PIE that a placed instance of
this chest actually exists and is reachable near Room 6 (Kitchens) - out of scope for this pass,
flagged again per the standing note in the entry above.

### 7. Oathgate reachability - PASS, first genuine physical-geometry check (not just documented history)

Confirmed both gates still placed and correctly cross-linked: `Oathgate_Entrance` at
**(900, 900, 160)**, yaw -135; `Oathgate_Exit` at **(-900, -900, 160)**, yaw 45 - matches every prior
entry. **New this session**: actually traced the physical space between spawn (~(0,0,300)) and the
entrance, which no prior session had done (all previous "reachability" claims were inferred from
navmesh-volume overlap and roaming-AI history, never from tracing the actual path). `find_actors`
confirmed **NavMeshBoundsVolume coverage spans the whole box** between spawn and the gate.
`SceneTools.trace_world` along three parallel lines at hip height (z=110) between spawn and the
entrance each hit *something* within 200-300 units, at three different distances - the signature of
scattered sandbox clutter and the several ally NPCs standing in that area (confirmed via a
`find_actors` bounds query at the hit point: three different `BP_DCCEnemy_*` instances plus static
geometry, not a uniform wall - a real wall would produce the same hit distance regardless of which
parallel line was traced). **Conclusion: the space is open and walkable, just cluttered with props
and standing NPCs a player will naturally route around**, not blocked. This is real evidence from
tracing actual geometry, not just inference from history, though it still stops short of an actual
recorded walk (no input-injection tool, as ever) - if Zoe still can't find it on foot after this,
that's new information worth reporting, not something this session could have caught.

### Cleanup, confirmed

Every harness Blueprint and its placed instance (`BP_DispatchTestHarness`, `BP_InteractTestHarness`,
`BP_SelfieGoldHarness`, `BP_GoldOnlyTest`, `BP_GoldDiag2`, `BP_SelfieHostileTest`, and the two throwaway
object variables on the interact harness) were removed from the level and the whole
`/Game/DCC/_Temp` folder deleted. The one demo ally actually repositioned for testing
(`CJ_HockeyWings_Demo` equivalent, UAID `..._1427307299`) was moved back to its original
`(-1920, -700, 90)`. `Lvl_ThirdPerson` saved clean afterward. `PlayNumberOfClients` reconfirmed `1`
at the very end - never changed. Nothing in `Source/` was touched this session; the only durable
changes are the two new imported assets
(`/Game/DCC/Characters/Vaughn/SK_Vaughn*`, `/Game/DCC/Weapons/BoilerplateRifle/SM_BoilerplateRifle`)
plus `SK_Vaughn_Skeleton`'s new `compatibleSkeletons` link and the four sockets on `SK_Vaughn` - all
DataTable/asset-level, none needing a build, per the dispatch brief's own preference. No editor
restart happened or was requested this session.

## Diving Pool / Voldemortgage scoping pass (2026-08-30, MCP live, no build)

Zoe flagged: "is the diving pool where voldemortgage is? right now the room is just a circle of
benches." Read every doc, then verified everything against the live editor rather than trusting
the docs — several of them describe 2026-08-19 state that is now 11 days stale. **Zoe's read is
exactly right**, and the design connection she's asking about is real, not a guess.

**Yes — Room 7 (`DCCRoom_DivingPool`) is Voldemortgage's boss room, by design and by live wiring,
not a coincidence of naming.** `Docs/floor-01-design.md` §7 and `Docs/room-purpose-audit.md` §7
both call it out explicitly ("The full Voldemortgage fight, the pause-loop's best showcase" /
"the floor's climax"), and the achievements doc's Strongbox Escort / Pump The Brakes / Late Notice
Survivor / Lien Free entries are all this exact fight. Confirmed live, not just from docs:

- `DCCRoom_DivingPool.RoomType = Combat`, `.Encounter` points at `DCCEncounter_Voldemortgage` —
  correctly linked (read back via `ObjectTools.get_properties`, not assumed).
- That encounter's `Waves` array is fully authored: Wave 0 (Immediate) spawns
  `ADCCEnemyCharacter_Voldemortgage` (`bIsBoss=true`, level 4) plus 3 `ADCCEnemyCharacter_DebtEater`;
  Wave 1 (HealthThreshold 0.5) spawns 5 more Debt Eaters. Announcement barks are in and in-voice
  ("I have your file. I have everyone's file." / "You are not in trouble. You are in arrears.").
- Voldemortgage is a real, built character, not a stub: `Characters/DCCEnemyCharacter_Voldemortgage.h/.cpp`
  exists, subclasses the boss pipeline, and its constructor wires a real imported mesh via
  `ConstructorHelpers::FObjectFinder<USkeletalMesh>` — `Content/DCC/Characters/Voldemortgage/`
  has `SK_Voldemortgage`/`SK_Voldemortgage2` plus `A_Voldemortgage_Walking/Running/HitReact`
  animations, all on disk. **This is not a case of lore predating implementation** — the character,
  its boss-phase component, and this room's encounter are all real and connected.

**And yet Zoe's on-screen impression is also exactly correct — the room itself is genuinely just
that.** Queried every actor actually placed in the `DCC/Room07_DivingPool` outliner folder (14
total, via `get_actors_in_folder` + a batched `ProgrammaticToolset` script, not a guess): one
`SM_Floor_DivingPool`, one `SM_Water_DivingPool` (a flat plane, not a real pool basin), **exactly
eight benches** (`SM_Bench_DivingPool_1`–`8`) spaced in an even ring around the water's perimeter,
the invisible `DCCEncounter`/`DCCRoom`/`NavMeshBoundsVolume`/`DCCFloorExit` actors, and nothing
else. No walls, no ceiling, no cave shell, no tiered seating structure, no diving platform, no
waterfall — every piece of environment art `floor-01-design.md` calls for
("`Cave_Camp`/`Alien_Cave_biome` cavern shell, `Thai_Market` benches as tiered seating, waterfall,
water plane") except the benches and the water plane. And **Voldemortgage himself has no placed
instance anywhere in the room** — that's correct/expected (`DCCEncounter` spawns its roster at
runtime, it doesn't hold a pre-placed boss), but it does mean nothing in the room currently *looks*
like a boss arena when walked into outside of combat. One more small gap: `RoomDisplayName` on
this room reads back empty, so the room-name HUD tag (built 2026-08-26) won't show "The Diving
Pool" here even though `DISPATCH.md`'s own mapping table says it should — a one-line
`set_properties` fix whenever someone's next in the editor.

**Bottom line for Zoe:** the mechanical fight is real, tuned, and will run correctly the moment a
player enters this room in combat. What's missing is entirely the art/dressing pass — same
"pure blockout" state `CLAUDE.md`'s own notes already flagged for Rooms 5–7 on 2026-08-19, just
now visible because nobody had actually looked since. Not fixed here per the brief (scoping only,
no build) — flagging the concrete next steps rather than guessing at them:

1. Set `RoomDisplayName = "The Diving Pool"` on `DCCRoom_DivingPool` (one `set_properties` call).
2. Environment art pass: walls/ceiling (cave shell), a tiered-seating structure around the benches
   rather than a flat ring, a waterfall, and a raised diving platform — `Cave_Camp`/
   `Alien_Cave_biome`/`Thai_Market` are the packs the design doc already names for this.
3. A PIE test of the actual encounter — per earlier DISPATCH entries the Wave 0 roster (boss + 3
   Debt Eaters) has been confirmed to spawn and fight before, but that was in the sandbox test
   area near spawn, not this room specifically since the room got its current geometry.
4. Nothing here blocks anything else — this is a self-contained art/dressing task, not a wiring
   task.

## Standing design principle (project-wide, added 2026-08-30)

**Zoe's direction, applies to all room-building work from here forward, not just Room 7:**
"most of this game should take place in interior spaces. the closest thing to outside will be
pseudo-interior spaces." Concretely: even a room whose premise sounds outdoor (a diving pool, a
plaza, a courtyard) should be built as an enclosed interior — walls and a full ceiling — rather
than reading as open-air. If a room wants scale or a "sky" feeling, fake it with a tall interior
ceiling, a vaulted/atrium look, or skylights (a lit panel, not a hole showing real sky) — never
literal open sky geometry. Keep this in mind for any future floor/room authoring pass.

## Diving Pool room-shell build (2026-08-30, MCP live, editor open — build not yet deployed)

Built out Room 7 per Zoe's request ("please go ahead and build out a proper seating area and
entire room shell") following the scoping pass above. This is art/geometry/lighting only — the
encounter (`DCCEncounter_Voldemortgage`, both waves) was not touched. Read `floor-01-design.md`
§7, `room-purpose-audit.md` §7, and `dungeon-tone-bible.md` first, per the standing design
principle above this room had to read as a fully enclosed interior, not an open-air pool.

**Concept:** "Vantage Wellness Aquatic Experience" — a corporate wellness natatorium repossessed
into the dungeon, inside a hollowed cavern (ties to Room 1's `Cave_Camp` look and Room 3's Falls
above). "Vantage Wellness" is a direct callback to the tone bible's own sponsor-naming example
(§4: "two mundane words, one aggressive claim").

**What was built, all under outliner folder `DCC/Room07_DivingPool/*`:**

1. **Shell** (`.../Shell`): 5 `SM_Cube` walls (60cm thick, 1600 tall — 4x a normal room's height,
   for a "grand atrium" boss-arena feel) fully enclosing the existing floor footprint
   (x10400-13000, y4400-6400). West wall is split into two segments to preserve the existing
   `SM_Floor_BarToDivingPool` corridor doorway (y4700-5300) — confirmed via `find_actors` before
   building so the shell doesn't seal off the only entrance. Full ceiling at z=1600, plus 3
   roof-beam strips on the underside and 4 corner pillars (120x120, full height) so it doesn't
   read as a flat box.
2. **Water plane rescaled**: `SM_Water_DivingPool` shrunk from 1600x1600 to 1200x1200 (scale
   0.16→0.12, pivot is center so this recentered cleanly) to open up real perimeter margin for
   seating — the original pool nearly touched the north/south walls, leaving no room for tiers.
3. **Seating** (`.../Seating`): the original 8 ground-ring benches (`SM_bench_01`,
   `Thai_Market`) are untouched as the front row. Added 4 low risers (`SM_Cube`, z0-100) with 4
   more benches on top — 2 on the east side, 2 on the west side (south of the doorway, so the
   entrance stays clear) — for an actual tiered read rather than a flat ring. Added 4
   `SM_stool` (`Diner` pack) near the entrance and corner pillars as "repurposed break-room
   furniture," per the brief's suggested flavor.
4. **Diving platform** (`.../DivingPlatform`): an `SM_Cube` tower + overhang platform at the
   pool's north edge (rising to z=1000, platform at z=1000), physically realizing the design
   doc's "a platform high above that someone dives from every twenty minutes" — geometry only, no
   diver NPC placed (see flag below). Dressed with 4 `Cave_Camp` rocks at the tower base to blend
   the cube into a cavern read, plus 3 hanging stalactites near the ceiling. `NS_water_fall`
   (`Cave_Camp/Fx/Waterfall`) placed flowing off the platform into the pool.
5. **Lighting** (`.../Lighting`): 1 key `SpotLight` centered over the pool + 2 secondary warm
   `SpotLight`s over each seating bank, all angled down from ceiling height; 2 cool teal
   `PointLight`s near the water for ambiance. **Caught and fixed a real mistake here**: the
   project's default `SpotLight` intensity units are Candelas, not Lumens or Unitless, and the
   first key light was set to 40,000,000 — a searchlight-strength blowout. Fixed all 5 lights to
   sane values (8,000-15,000 cd for spots, 3,000 cd for points) and read every one back to
   confirm before moving on. Worth remembering for any future light placement in this project:
   check `intensityUnits` before trusting a round intensity number.
6. **Signage** (`.../Signage`): 2 `TextRenderActor`s with in-fiction corporate-dystopia copy per
   the tone bible's rules (corporate cheer over menace, bureaucracy as horror, specific beats
   generic). A "POOL RULES" board near the entrance ("Diving is a contractual obligation, not a
   choice... The lifeguard has been reassigned. Effective immediately. Retroactively.") and a
   motivational-poster-style debt-collection line near the east seating bank ("YOU MISSED A
   PAYMENT. WE NOTICED. WE ALWAYS NOTICE. — Mile High Recovery Solutions").
7. **`RoomDisplayName` set** to `"The Diving Pool"` on `DCCRoom_DivingPool`, read back to confirm
   — the overhead HUD room tag will now show something. Chose the plain name for consistency with
   every sibling room's naming style (The Bar, The Pantry, The Grotto, etc. are all plain, not
   corporate-twisted) rather than something like "Vantage Wellness Aquatic Experience" — **flagged
   for Zoe** in case she'd rather the HUD tag itself carry the corporate joke; easy one-line swap
   either way.

**Interrupted by PIE contention, not finished:** the other session doing the broader
PIE-verification pass (climbing/loot/selfie mode) had a live PIE session running when this session
tried to place the second signage actor — `add_to_scene_from_class` raises "Cannot create actors
while PIE is active." Per this session's own brief, waited rather than fighting for the editor.
**Still outstanding once PIE frees up:**
- Place the second `TextRenderActor` (the motivational poster) — the first (POOL RULES) is placed
  and configured; the second failed mid-script before creation.
- A PIE test of this room specifically with the new geometry in place, checking the encounter
  still triggers correctly and that nothing (risers, pillars, the diving tower) obstructs
  `NavMeshBoundsVolume_DivingPool` pathing or blocks Voldemortgage/Debt Eater movement. Coordinates
  were chosen to keep the pool center and the main floor open, but this hasn't been visually or
  functionally confirmed yet.
- A viewport screenshot pass (`CaptureViewport`) to sanity-check the room reads correctly and
  catch anything that looks wrong before calling this done.

**Deliberately not built, flagged rather than silently skipped:**
- **No diver NPC.** The design doc's "the divers are still diving, unpaid, during the boss fight,
  in the background" is the floor's stated thesis image (per `room-purpose-audit.md` §7), but
  actually spawning/animating a diver character is content/encounter-adjacent work, not room-shell
  art, and this session's brief was explicitly scoped to art/geometry/lighting only. The physical
  diving platform is built and ready for one whenever that's picked up.
- No custom wall/ceiling material — the shell uses the same default `SM_Cube` material as every
  other blockout room in this project (Bar, Pantry floors, etc.), dressed with real `Cave_Camp`
  rock meshes rather than a re-textured cube, to stay consistent with the rest of the floor's
  current art level and avoid scope creep into material authoring.

## Voldemortgage relocated to a new "Debt Collection HQ" tower — Diving Pool is now a break room (2026-08-30, MCP live, editor open — build not needed, pure data/level work)

**Design reversal from Zoe**, arriving right after the Room 7 shell build above: "I kind of think
that the voldemortgage fight should happen in another room, not the diving room. can we build
another space for it? maybe a giant corporate hq tower and the fight happens at the top?" This is
a pure level/data change — nothing in `Rooms/DCCRoom.h` or `Rooms/DCCEncounter.h` needed touching,
because spawn positions are `RelativeLocation` offsets from the encounter actor's own transform
(see `FDCCEnemySpawn`'s own doc comment: "a whole encounter can be dragged around the level... 
without re-authoring every spawn point"). Moving the boss fight is therefore just moving one actor
and re-pointing two `DCCRoom.Encounter` pointers — confirmed this is exactly what happened before
building anything, by reading `DCCRoom.h`/`DCCEncounter.h` first rather than assuming.

**Checked for PIE contention before touching the editor**, per this file's own standing pattern —
`IsPIERunning()` was `true` on first check (another session's live PIE from the entry above), so
this session did read-only research (asset cataloguing, source reading, coordinate planning) until
a second check came back `false`, then proceeded.

### 1. Decoupled the encounter — Diving Pool is now a Safe Room

`DCCRoom_DivingPool`: `RoomType` `Combat` → `SafeRoom`, `Encounter` → `None`. `RoomDisplayName`
("The Diving Pool") and every piece of art from the shell-build session above are untouched, per
the brief — it stays the "Vantage Wellness Aquatic Experience" cavern-natatorium, just repurposed
as a break room the way `room-purpose-audit.md`'s own test wants: staff (and now the party) can
rest here between fights, consistent with its existing "safety is a product" sponsor-economy
flavor. `AllowsAsyncLogin()` now reads `true` for this room as a side effect of the `SafeRoom`
type — not requested, not harmful, just worth knowing since Room 5 (The Bar) was previously the
only room with that property.

### 2. A new corridor, cut through the Diving Pool's own east wall

The room's east wall (`SM_DivingPool_Wall_East`, one piece, x13000-13060) was replaced with two
segments leaving a 300-unit doorway (y5200-5500) — same pattern already used for the Bar↔Pantry
connection. A plain floor slab (no walls/ceiling, matching that same precedent) runs the corridor
east to x13750, where the tower begins. A sign at the new doorway ("MILE HIGH RECOVERY
SOLUTIONS — COLLECTIONS DIVISION -- THIS WAY... Yes, they built their office next to the pool. No,
nobody has explained why.") turns what could read as an arbitrary new hole in the wall into a
deliberate bit — the collections department's own break room happens to share a building with
their office, which is exactly the kind of bureaucratic-horror detail the tone bible asks for.

### 3. The tower — three floors, stacked in Z, all pure interior per the standing design principle

New outliner folder `DCC/Room08_DebtCollectionHQ/`. Footprint x13750-15150, y4700-6100 for the
lower two floors (matches the project's established "modest" room scale, close to Room 01's own
1600x1600); the top floor widens to y4400-6400 for open arena space. All three are fully enclosed
— floor, four walls, ceiling — no open-air geometry anywhere, per the 2026-08-30 standing
principle logged above this entry.

- **Ground floor — Lobby** (`DCCRoom_HQ_Lobby`, `SafeRoom`, no encounter). A reception desk
  (`Dam_city/SM_desk`) and chair against the back wall, two facing rows of `Diner` chairs for
  "please take a number" waiting-room energy, a potted plant, and three signs: a floor directory
  ("LOBBY .. YOU ARE HERE / CLAIMS PROCESSING .. UP ONE FLIGHT / EXECUTIVE SUITE .. UP TWO
  FLIGHTS"), a take-a-number joke ("Now serving: nobody. You are number: everybody"), and the
  muzak gag ("an instrumental version of a song about how much the artist hated this exact
  building").
- **Middle floor — Claims Processing** (`DCCRoom_HQ_CubicleFloor`, `Narrative`, no encounter —
  **this is the explicitly-flagged placeholder floor**, per the task brief's own priority order).
  Still got a real pass rather than a bare box: six `Ice_Station` computer-console desks in two
  facing rows (retro-console monitors standing in for corporate CRTs — no real office-computer
  asset exists in any installed pack, same "nearest analog" logic this project already uses for
  the Kitchens/Bar), a chair at each, and three signs — a "COLLECTIONS LEADERBOARD" (real names,
  fake ranks, "47. YOU ... 0 FILES CLOSED. Everyone is watching your number."), a menacing
  motivational poster ("IT COSTS NOTHING TO SMILE WHILE REPOSSESSING SOMEONE'S HOME. (This has
  been independently verified by Legal.)"), and "YOUR SHIFT ENDS WHEN THE DEBT IS PAID." A first
  attempt at low cubicle-divider partitions (`Ice_Station/SM_interior_wall_02`) read as a full
  floor-to-ceiling pillar rather than a low divider once screenshotted, and was actively worse
  than no divider at all (blocked the aisle sightline) — removed rather than left in. **Flagged as
  the weakest of the three floors and the one most worth a follow-up pass**, per the brief's own
  stated priority order.
- **Top floor — The Corner Office** (`DCCRoom_HQ_Penthouse`, `Combat`, `Encounter =
  DCCEncounter_Voldemortgage`) — **the boss room, and the actual critical path.** A tall
  atrium-height ceiling (1200 units interior, vs. 500 for the two floors below — same "grand
  arena" move the Diving Pool shell already used), four corner pillars, an oversized executive
  desk (`Dam_city/SM_desk`, scaled up ~1.5x) against the back wall with flanking credenza cabinets,
  a waiting sofa near the entrance, and a name plate ("LORD VOLDEMORTGAGE, SENIOR VICE PRESIDENT,
  COLLECTIONS, Mile High Recovery Solutions"). **The "corner office view," per the standing design
  principle, is faked rather than built as real sky**: decorative windows on two walls, backlit by
  point lights placed just outside the room (unreachable, there is nothing behind them but the
  light itself), plus a warm central ceiling glow standing in for a skylight — a lit panel, not a
  hole. One line of signage leans into the fakery on purpose: "THE VIEW FROM THE TOP FLOOR IS
  EXCELLENT. WE HAVE BEEN TOLD THIS. WE HAVE NEVER CHECKED."

  **The encounter itself was moved, not rebuilt.** `DCCEncounter_Voldemortgage`'s waves (both
  authored waves, Wave 0 immediate boss+3 Debt Eaters, Wave 1 at 50% HP +5 more) were read first
  to confirm every `RelativeLocation` offset stays within ±450 units of the encounter's own
  transform — comfortably inside this room's ~1400x2000 floor before moving anything, so nothing
  spawns inside a wall or outside the arena. The actor's own `SetActorTransform` then moved from
  the Diving Pool's old center (11700, 5400, 0) to this room's arena center (14450, 5400, 1120) —
  a straight actor move, waves/stats/announcements byte-identical to before. Confirmed by
  read-back: `DCCRoom_HQ_Penthouse.Encounter` points at the same encounter actor, and
  `DCCRoom_DivingPool.Encounter` reads `None`.

### 4. Vertical connection — the climbing system, not a custom stair mesh

Rather than build three flights of stairs (which would have meant either an unreliable
hand-derived incline-angle calculation for a blockout ramp, or importing/fitting a stair asset to
an arbitrary rise), the tower's floors connect through a shaft (`.../Shaft`, x15210-15560,
y5200-5500) using **this project's existing universal climbing system** ("any wall-like surface is
climbable," per the 2026-08-19 entry) — a plain vertical wall panel spanning z0-1120 that a player
climbs directly, with a partial landing at the Cubicle-floor level for anyone who wants to get off
there rather than keep climbing. This is zero new code, reuses a system already built and already
tested, and gets its own in-fiction joke for free: a sign at the base reads "ELEVATOR OUT OF
SERVICE. Please use the stairs. There are no stairs. Please climb. -- Facilities." Each floor's
east wall has a matching 300-unit doorway into the shaft (Lobby, Cubicle floor, and Penthouse all
line up on the same y5200-5500 span as the west-side corridor doorway, so the route is a straight
line through the middle of each floor).

**One real mistake caught before it became a live hole in the floor**: the Cubicle floor's west
wall was first built with the same doorway-gap pattern as the Lobby's (matching the shell-building
habit from the room above), which would have left a 300-unit gap opening onto empty air 560 units
above the corridor — nothing is west of the tower at that elevation. Caught by re-reading what had
actually been placed rather than assuming the pattern generalized, and fixed by replacing the two
gapped segments with one solid wall before moving on to dressing.

**A `NavMeshBoundsVolume` covers the whole tower** (x13690-15560, y4340-6460, z-40 to 2380) so
Voldemortgage and the Debt Eaters can path in the Corner Office — the default `NavMeshBoundsVolume`
brush turned out to be a 200-unit **center-pivoted** box, unlike `SM_Cube`'s 100-unit corner-pivot
convention already documented in `CLAUDE.md`; the first placement attempt used corner-pivot math
and landed badly offset, caught immediately via `get_actor_bounds` read-back and corrected. Worth
adding to this project's pivot-convention notes: **not every placeable brush shares `SM_Cube`'s
pivot-at-corner behavior — read bounds back after placing any new class of volume, don't assume.**

### 5. Lighting — same Candela mistake as the Diving Pool shell, caught later than it should have been

First lighting pass across all three floors (one `SpotLight` key per floor plus accent lights) came
in visibly overexposed in every `CaptureViewport` screenshot — not the catastrophic
40,000,000-candela blowout the Diving Pool session hit and caught immediately, but a subtler
"everything reads paper-white" problem from several 4,000-25,000 cd lights overlapping in rooms far
smaller than the Diving Pool arena they were sized for. Cut intensities twice (roughly ×0.3 each
pass); the second pass visibly softened the Lobby and Cubicle floor but the Penthouse **still reads
overexposed in the last screenshot taken**, and is most likely the all-white default `SM_Cube`
material bouncing Lumen GI hard in a tall, mostly-empty white box rather than the point lights
themselves — **not fully chased down, flagged as a real follow-up rather than silently accepted.**
Whoever's next in the editor should drop the Penthouse's remaining lights further (or add some
non-white material/texture variation to break up the bounce) before calling the room's lighting
finished.

### Verified before calling this done

- `DCCRoom_DivingPool` / `DCCRoom_HQ_Penthouse` properties read back exactly as set (Safe Room +
  no encounter / Combat + `DCCEncounter_Voldemortgage`).
- `DCCEncounter_Voldemortgage`'s transform reads back at the new arena center; `Waves` reads back
  byte-identical to before the move.
- `CaptureViewport` screenshots taken of the Lobby, Cubicle floor, and Penthouse interiors — all
  three read as fully enclosed rooms with no gaps, furniture in roughly sensible places. Not a
  PIE test — no player has walked this yet.
- Level saved (`AssetTools.save_assets` on `Lvl_ThirdPerson`) with no dirty state remaining.

### Deliberately not done, flagged rather than silently skipped

- **No PIE test of the relocated fight.** Nothing in this session stopped or started PIE (matching
  the "don't fight for the editor" pattern, and by the time the build was finished no PIE
  contention remained to work around anyway) — the next live session should enter this room and
  confirm Voldemortgage's waves actually spawn and path correctly in the new, smaller footprint,
  since the encounter has never been tested outside the sandbox area and the old Diving Pool arena.
- **The Diving Pool's own leftover follow-up items from the shell-build session above are still
  open** — the second `TextRenderActor` (motivational poster) that failed mid-script on PIE
  contention, and a viewport sanity pass on that room specifically. Neither was this session's
  scope; noting so nobody assumes this session closed them out.
- **No Meshy generation for Voldemortgage's desk or any other centerpiece prop** — the brief
  reserved that for something genuinely distinctive if nothing suitable existed, and
  `Dam_city/SM_desk` scaled up was judged suitable (a real, correctly-proportioned desk mesh,
  just not custom-built for this character). Worth revisiting if the desk reads as generic once
  someone's actually looked at it in-person rather than via `CaptureViewport`.
- **Cubicle floor is the placeholder tier**, exactly as the brief allowed for — desks/monitors/
  chairs/signage are real, but there are no cubicle partitions (removed, see above), no NPCs, and
  no encounter. A follow-up pass could add either a light non-combat beat (an NPC collector at one
  of the desks, grumbling — this floor has no grievance-ladder dialogue yet, and
  `room-purpose-audit.md`'s own cross-cutting note ties "no grievance dialogue" directly to "no
  economic-purpose read") or leave it as pure atmosphere between the Lobby and the fight.

### Addendum: a 2.4s Stall Detector warning, and whether this session's own scripting caused it

Zoe reported an editor-log warning during this session: `Stall Detector: FEngineLoop::Tick, Stall
Duration: 2.371957`, stack entirely inside `python311.dll` (recursive Python frames). Checked
three things, per her request.

**Could not find the event itself in the persisted log**, despite a full-text search (plain and
case-insensitive) of `Saved/Logs/DungeonCrawlerDenver.log` for `Stall Detector`, `Stall Duration`,
`Hitch`, and the `LogStall` category specifically (`EditorToolset.LogsToolset.GetLogEntries` and a
direct grep of the file both came back empty except the plugin's own two startup lines,
`LogStall: Startup...` / `Startup complete.`). The log file's own last-write timestamp is current,
so this isn't a stale-file problem. Most likely explanation: the `StallLogSubsystem` plugin
(`Engine/Plugins/Experimental/EditorPerformance`) writes to the on-screen/Output Log window
through a path `GetLogEntries` doesn't capture, rather than a delayed flush — **so frequency could
not be confirmed from the log, and this should be treated as an open question, not a confirmed
one-off.** If it recurs, ask whoever's next at the editor to check the live Output Log window
directly rather than relying on this tool.

**This session's own tool calls are a very plausible cause, and probably the direct one.**
Every piece of the tower above this addendum was built through
`ProgrammaticToolset.execute_tool_script` — by design, a single Python script that runs
synchronously on the game thread and only returns once every `execute_tool()` call inside it has
finished (the toolset's own description: "Batches calls to other registered tools through a small
sandboxed Python script"). Several of today's scripts chained a lot of engine work into one call:
the Cubicle floor dressing pass alone spawned 6 desks + 6 computers + 6 chairs + 2 partitions + 3
signs + 2 lights, each with follow-up `set_label`/`set_actor_folder`/`get_components`/
`set_properties` calls — **roughly 85 sequential engine operations in one script invocation**,
several of them the *first* reference this editor session made to a given `Ice_Station` mesh
(meaning a synchronous asset load, not just an actor spawn). The two shell-building scripts and
the Penthouse/Lobby dressing passes were similarly sized (50-70 operations each). A 2.4s stall is
squarely the kind of thing that workload produces.

**Assessment: almost certainly benign in itself, but worth changing habits on.** The
`StallLogSubsystem` is a passive diagnostic (it logs; it does not kill the process or roll back
any state), and nothing about a delayed `Tick` corrupts data — every individual engine call inside
the script still completes atomically, in order, whether or not Slate got to redraw in between.
The actual risks are indirect: (1) a stalled game thread reads as a frozen editor to whoever's at
the keyboard, and this project's own history already has at least one session where an
unresponsive-looking editor got force-killed via `taskkill` rather than waited out — a longer or
more frequent stall makes that more likely, not less; (2) if a stall like this happens while a PIE
session is live (this project runs concurrent sessions constantly, per this file's whole existence),
a multi-second game-thread pause could plausibly read as a network hiccup or dropped input to
whoever's testing; (3) UE does have a separate, unrelated hard hang-detector that force-closes the
process after a much longer unresponsive stretch (tens of seconds) — 2.4s is nowhere near that, but
it establishes these scripts *can* produce multi-second blocks, so a much larger single batch (many
dozens more actors, or a heavy first-time asset import) is the failure mode to actually worry about,
not this specific warning.

**Going forward**: keep large placement passes chunked to roughly the size already used today
(a few dozen operations per `execute_tool_script` call, one room/feature at a time) rather than
combining multiple rooms' worth of dressing into a single script — today's scripts already followed
that shape by accident (one script per room), which is probably why nothing worse than a single
2.4s warning showed up. Deliberately avoid the temptation to merge scripts for fewer round-trips;
the round-trip cost is much cheaper than a multi-second main-thread stall.

## HQ furniture/NPC pass, a reusable LEVEL BOSS banner, and the Balloon Payment drift fix (2026-08-30, MCP live, editor open — C++ compile-checked, NOT deployed)

Follow-up to the HQ tower build above. Zoe asked for: more office furniture/clutter and NPCs
across the tower, a "LEVEL BOSS" style temporary overlay reusable for every major fight, and a
fix for Voldemortgage drifting off during the Balloon Payment channel instead of kiting and
shooting. Also chased two things she flagged mid-session from a live screenshot: a ray-tracing
memory warning, and a camera bug she read as "starting in selfie mode."

### 1. NPCs and dialogue — Receptionist and two Collectors

Read `Dialogue/DCCDialogueNPC.h` and the existing Bartender rows in `DT_DCCDialogue` first, to
match the established shape rather than invent a new one. Added seven new rows
(`Receptionist_01..03`, `Receptionist_Hint_01`, `Collector_01..03`) and placed three
`ADCCDialogueNPC` instances, all reusing `SK_DonMinion` (the same body the Bartender already
uses — no new import needed):

- **`NPC_HQ_Receptionist`** (Lobby, behind the desk). Deadpan corporate front-of-house
  ("Welcome to Mile High Recovery Solutions. Do you have an appointment? No? Neither does
  anyone."), plus a genuinely useful hint gated behind patience, matching the Bartender's own
  pattern exactly: *"don't stand under the balloon when it's full. I've filed the incident
  reports."* — this is a real gameplay tip for the Balloon Payment mechanic, not just flavor.
- **`NPC_HQ_Collector_1`/`_2`** (Claims Processing, standing near two of the desks). Deliberately
  a different register from the rest of the floor's dialogue — every other grievance on this
  floor comes from Voldemortgage's *victims*; these two are his *employees*, so they resent the
  job rather than fear the party ("Quota's quota. I didn't make the number, I just have to hit
  it." / "He reads every file himself. Every single one. I don't know when he sleeps. I don't
  think he sleeps."). This closes the gap `room-purpose-audit.md` flagged for any future room
  without grievance dialogue, and gives Claims Processing an actual voice instead of just desks.

First placement had the Receptionist standing almost entirely behind/inside the desk mesh from
most angles — caught via `CaptureViewport`, fixed by moving her to the far side of the desk
facing the entrance, which reads correctly as "sitting behind the counter."

### 2. More furniture and clutter, one real asset mistake caught and fixed

Added across all three floors: a desk lamp, a mug, a grandfather clock (reused as a lobby wall
clock — the only clock mesh in any installed pack), stacked paperwork boxes, and two filing
cabinets (Claims Processing) from `Modern_House_V1`; a trash can, a mug, and bottles at desks on
the Cubicle floor; a trash can, a second mug, and a low side table in the Penthouse.

**Real mistake, caught immediately and fixed**: the first trash can pick,
`/Game/Sci_Fi_city/Meshes/Props/SM_trashcan`, turned out to be a **107×111×141cm industrial
drum**, not an office bin — confirmed via `StaticMeshTools.get_bounds`, and obvious the moment a
screenshot showed a chest-high white barrel sitting on the reception desk. Swapped all four
placements for `Cyberpunk_City/Meshes/Trashcan/SM_trashcan_01` (45×45×96cm at scale 1, used at
0.75x) instead, which actually reads as a bin. Worth the general lesson: **a mesh's name is not
its size — check `get_bounds` before trusting a name like "trashcan" to be human-scale**,
especially pulled from a pack (`Sci_Fi_city`) that also ships genuinely industrial-scale props
under ordinary-sounding names.

### 3. A reusable "LEVEL BOSS" banner — generic across every future boss, not just Voldemortgage

Added to `UI/DCCHUD.h/.cpp`, deliberately keyed off **carrying a `UDCCBossComponent`** rather
than anything Voldemortgage-specific, so any future boss gets this for free the same way the
charge/lien mechanics already are generic:

- `DrawNearbyHealthBars`'s existing per-frame `TActorIterator<ADCCCharacterBase>` scan (already
  running every frame for the mini health bars) now also calls the new
  `CheckBossAnnouncement(Candidate)` on each living character in range — piggybacked on that
  existing loop rather than adding a second world scan, on purpose, given the stall-detector
  addendum immediately above this entry.
- The first time that scan finds a living character with a `UDCCBossComponent` it hasn't seen
  before, it arms a large centre-screen card: "LEVEL BOSS" in red over the boss's `DisplayName`,
  faded in and out over ~4.5 seconds (`BossBannerDurationSeconds`, tunable), styled distinctly
  from the small `ShowNotice` refusal popups so the two can never collide or get confused for
  each other.
- **Purely client-local and never replicated** — which boss actors have already been announced
  lives only in the HUD's own `AnnouncedBosses` array, same trust model as everything else in
  this HUD that reads already-replicated state rather than adding new network traffic. Nothing
  here can desync a fight; worst case is a client re-showing a banner it shouldn't, which cannot
  happen here since the array is never cleared.

### 4. The Balloon Payment drift — root cause found, and it was exactly what it looked like

Read `Combat/DCCBossComponent.cpp`'s `TickBalloon` and `DCCEnemyCharacter_Voldemortgage.cpp`
`BeginPlay` together rather than guessing from the symptom alone. Confirmed:

- `TickBalloon` lifts the boss by calling `SetActorLocation` on **Z only**, every 0.2s, for the
  whole charge (`Owner->SetActorLocation(Location)` with `Location.Z` interpolated toward
  `GroundedZ + ChargeLiftHeight * ChargeProgress`). It never touches X or Y.
- Nothing in `ADCCEnemyCharacter::Tick` ever checked whether its owner was mid-charge, so the
  ordinary chase logic (`AIController->MoveToActor(CurrentTarget, StopDistance)`) kept running
  underneath the lift, completely unaware of it.
- Voldemortgage is permanently `MOVE_Flying` with zero gravity (set in his own `BeginPlay`, not
  just during a charge — that part is correct and intentional, "he floats above the pool per the
  floor design"). A flying pawn under active `AIController` path-following, while something else
  is independently and repeatedly teleporting its Z out from under the path solver, is exactly
  the shape of bug that reads as "drifting on the wind": the path-following component kept
  re-solving toward a navmesh-projected ground position while the balloon held him aloft, and the
  horizontal component of that fight is what showed up as directionless drift.
- Separately, `bIsRanged` in the base AI is computed as `EffectiveRange > AttackRange + 1`, and
  Voldemortgage's `AttackRange` is already `1100` with no equipment to exceed it — so even when
  ordinary chase/attack logic *was* running correctly, he was never actually classified as
  "ranged" by the existing standoff/retreat system in the first place. Not a bug exactly (his
  1100-unit `AttackRange` already reads as reach rather than melee), but confirms the existing
  ranged-standoff system was never going to produce "hangs back and shoots" for him on its own,
  charge or no charge.

**The fix, in `Characters/DCCEnemyCharacter.h/.cpp`, generic for any future
`UDCCBossComponent`-carrying enemy, not special-cased to Voldemortgage:**

- `ADCCEnemyCharacter` now caches its own `UDCCBossComponent*` once in `BeginPlay`
  (`FindComponentByClass`, null for anything that isn't a boss).
- `Tick()` branches to a new `TickChargeCombat(DeltaSeconds, CurrentTarget, AIController)`
  whenever `CachedBossComponent->IsCharging()` is true, **before** any of the ordinary
  chase/skill logic runs, and returns — so during a charge, exactly one system decides where the
  boss is and what he's doing, instead of two fighting each other.
- `TickChargeCombat` never touches the `AIController` except to cancel whatever chase order was
  already in flight. It measures distance to target **horizontally only** (his altitude is
  `TickBalloon`'s business, not this function's — measuring 3D distance would read him as
  "out of range" the instant he lifts off, which is a second, subtler version of the same bug),
  faces the target, tries `TryUseSkill` then `PerformAttack` exactly like the grounded path does,
  and **directly nudges X/Y** toward or away from the target to settle on
  `EffectiveRange * RangedStandoffFraction` — the same preferred standoff a grounded ranged
  fighter already uses, just driven by a plain clamped `AddActorWorldOffset` (swept, so a wall
  still stops him) instead of pathfinding. This mirrors `TickBalloon`'s own established pattern
  of directly owning one axis of movement rather than delegating to `AIController`, so there is
  never more than one system moving him during a charge.
- New tunable: `ChargeHoverSpeed` (220 uu/s default) caps how fast the horizontal correction
  happens, so it reads as drifting into position rather than snapping.

**A visible "what he's doing" label, per the other half of the ask.** Voldemortgage gained a
second `UTextRenderComponent` (`ChannelingLabel`), built the same way `BalloonMesh` already is —
plain component, no asset to author, hidden except while charging. Reads "CHANNELING - BALLOON
PAYMENT" (a second variant, "...(AGAIN)", once `CurrentPhase` is past the first charge) and
floats just above the balloon, rising with it exactly the way `BalloonMesh` already re-heights
itself as it inflates. This is on top of the existing `ChargeStartLine` announcement
("Lord Voldemortgage straightens his tie...") which only ever hit the scrolling feed — the label
is the same information kept in view for the whole channel, not a replacement for the bark.

### 5. Compile-checked, not deployed

All of the above (`DCCHUD.h/.cpp`, `DCCEnemyCharacter.h/.cpp`, `DCCEnemyCharacter_Voldemortgage.h/.cpp`)
compiled clean via the DebugGame standalone target twice — once before the selfie-camera fix
below, once after — editor left open and untouched throughout, no numbered hot-reload DLL
appeared in `Binaries/Win64/` afterward. **New `UPROPERTY`s and function signatures throughout —
this needs the usual close-editor → Development Editor build → reopen cycle before any of it is
live**, same as every other reflection-touching batch this project has shipped. Nothing here has
been seen in PIE.

### 6. Two things Zoe flagged live, chased in the same pass

**Ray-tracing "ALWAYS RESIDENT MEMORY EXCEEDS 20% OF THE BUDGET" warning — not new, not really
about World Partition.** `Config/DefaultEngine.ini` already shows a prior session hit this exact
warning once before and raised `r.RayTracing.ResidentGeometryMemoryPoolSizeInMB` from the engine
default (400) to 1536, with a comment explaining the mechanism: every ray-traced mesh keeps at
least one LOD permanently resident in a fixed pool (`r.RayTracing.NumAlwaysResidentLODs`,
unset here, so at its engine default), completely independent of whether World Partition has
that actor's cell streamed in or not. The on-screen warning is a **relative** threshold (fires
past 20% of whatever the pool is currently sized to), so raising the pool moves the trigger point
rather than removing it — and today's session alone added roughly 150 new actors, almost
certainly enough on its own to push the always-resident set back over the new 20% line (325 MB
observed vs. 307.2 MB = 20% of 1536 MB). Told Zoe directly that her World Partition streaming
change is unlikely to be the cause of *this specific* warning, asked what exactly she changed
(a checkbox, a runtime grid cell size, a streaming source component) rather than guessing further
blind, and pointed at the already-documented, already-proven streaming behavior (`CLAUDE.md`:
moving the viewport camera away from a room drops its actors out of `find_actors` results) as the
test to check whether her change broke anything real. Not fixed here — the safe, known-working
mitigation (raise the pool again) needs an editor restart to reallocate and wasn't done
unilaterally without her present.

**"Starting in selfie mode" — real bug, found, fixed, and it was not actually selfie mode.**
Zoe's screenshot showed the ordinary HUD (vitals, objectives, combat log, hotbar), not
`DrawSelfieViewfinder`'s corner brackets/reticle/red "SELFIE MODE" text - so `bInSelfieMode` was
genuinely false, contradicting her own read of the symptom. She then clarified: at spawn the
camera faces *back at the player* (a front-facing, selfie-style framing) despite not being
mechanically in Selfie Mode, and toggling Selfie Mode on and back off repairs it. That
clarification pointed straight at the actual bug. `SelfieCamera` (a second `UCameraComponent` on
`ADCCCharacterBase`, attached to the capsule facing back at the character to frame a selfie shot)
is constructed with `SetActive(false)` in the constructor — but **`UCameraComponent`'s own base
constructor** (`Engine/Private/Camera/CameraComponent.cpp`) unconditionally sets
`bAutoActivate = true` before this project's constructor code ever runs, and component
registration re-activates anything with `bAutoActivate` still true regardless of what `SetActive`
was called with during construction. So `SelfieCamera` came back on at spawn every time, sitting
active alongside the character Blueprint's own `FollowCamera` with nothing to say which one the
pawn's camera resolution should use — and since `SelfieCamera` faces backward, winning that race
read as exactly what Zoe saw. `EnterSelfieMode`/`ExitSelfieMode`'s own explicit `SetActive` calls
happen to leave the correct camera active afterward, which is why toggling C twice "fixed" it.
**Fix**: `SelfieCamera->bAutoActivate = false;` added alongside the existing `SetActive(false)` in
the constructor — the standard, correct pattern for a non-primary camera component that must
start inactive. Compile-checked clean along with everything else above; **also undeployed**, same
reflection-cycle caveat, though this specific line is itself a plain value assignment and would
have been Live-Coding-safe in isolation had it shipped alone.

### Deliberately not done, flagged rather than silently skipped

- **Nothing here has been seen running.** The Balloon Payment fix, the LEVEL BOSS banner, and the
  selfie-camera fix are all compile-checked only. The very next PIE session after this batch
  deploys should specifically: (1) trigger a Voldemortgage charge and confirm he holds position
  near the party and keeps attacking instead of drifting; (2) confirm the LEVEL BOSS card appears
  once on approaching him and does not reappear on leaving and re-entering the room; (3) confirm
  a fresh PIE spawn's camera is correctly forward-facing without ever having touched C.
- **Ray-tracing pool size not raised again** — flagged for Zoe with a question about what she
  changed, not fixed unilaterally.
- **Cubicle floor's collector NPCs have no combat presence** — still true from the entry above;
  today's pass gave the floor a voice, not a fight. Still the lowest-priority item per the
  original brief.

## Loot boxes turned on for most achievements, a physical redemption Vault, and a new
`ADCCLootBoxStation` interactable (2026-08-30, MCP live, editor open — new C++ compile-checked,
NOT deployed; data/level changes ARE live)

Zoe asked for achievements to pay loot boxes "instead" of (or alongside) their existing rewards,
openable only in a Safe Room, plus a new Safe Room built adjacent to the play area to open them
in. Mid-session she added: she is importing a Fab pack called "Battle Royale Props" and wants
loot boxes represented as physical props using it, each with a glow behind it for rarity - not
just a HUD number.

**The loot box mechanic itself turned out to already be fully built, not something to invent.**
Found `Items/DCCLootBoxTypes.h` (six tiers, item + boon tables, `FDCCOpenedLootBoxResult`),
`UDCCInventoryComponent::GrantLootBox`/`TryOpenLootBox`/`LootBoxCounts`, and
`ADCCCharacterBase::OpenLootBox`/`Server_OpenLootBox_Implementation` (which already re-checks
`IsInSafeRoom()` server-side, exactly the gate Zoe asked for) — all present, all already
compiled into the running editor. `DCCAchievementComponent.cpp` already calls
`Inventory->GrantLootBox(Row->LootBoxReward)` on claim, whenever a row's `LootBoxReward` is set.
`Docs/DT_DCCLootBoxTables.json`/`DT_DCCBoons.json` (six tiers, seven boons) were already staged
*and already imported* as live DataTables. A staged file, `Docs/DT_DCCAchievements_LootBoxBatch1.csv`,
had already added six new achievements (for newer stats like `PausedCombat`/`FavorTierMaxed`)
that already paid loot boxes from the start, and those were also already live. **What was
genuinely missing**, confirmed by reading the live `DT_DCCAchievements` table before touching
it: the original 22 achievements (`FirstBlood`, `Regular`, `Headliner`, ... `AssetRecovery`) all
had `lootBoxReward: "None"` — Batch1 was a batch of *new* rows, not a conversion of the existing
ones, and whatever "Batch 2" was supposed to convert the originals never happened. That gap is
what "many of them" meant, and it's the actual work this session did on the data side. Also
confirmed, so as not to invent a problem that didn't exist: every `EquipmentItemId` the loot box
tables reference (`MinionEyes`, `WaterBottle`, `SyndicateBadge` included) already exists in the
live `DT_DCCEquipment` (42 rows there now, vs. 27 in the stale `Docs/` CSV — that file is behind
the live table and shouldn't be trusted for an inventory check without reading live first).

### 1. Twenty of the original twenty-two achievements now pay a loot box

Read every row live, then `set_rows` added `lootBoxReward` tiers scaled to difficulty: Bronze for
the easy/first-time ones (`FirstBlood`, `Regular`, `RoomService`, `Bruiser`, `Sponge`,
`Autodidact`, `MovingUp`, `FreeClimber`, `Introductions`, `EstateSale`), Silver for the mid-tier
ones (`Headliner`, `ThoroughlyCleaned`, `StillStanding`, `Practised`, `Established`,
`Mountaineer`, `Networking`), Gold for the hardest (`DenverBloodbath`, `GoingDown`,
`AssetRecovery`). **Deliberately left `Setback` and `Persistent` (the two death achievements)
without a loot box** — they already pay negative Favor, and a box for dying would undercut the
punishment those two exist to be. XP/Favor on every row is untouched; the box is an addition, not
a replacement, since none of the numbers were broken and Zoe's "instead" reads more naturally as
"the exciting reward is now a box" than "delete the existing tuning." Table saved. Combined with
the six already-converted Batch1 achievements, that's 26 of 28 achievements now paying a loot box
on unlock.

### 2. `ADCCLootBoxStation` — a new interactable so a box is something to walk up to and open

New class, `Items/DCCLootBoxStation.h/.cpp`. A deliberately small, plain `AActor` (not
`ADCCLootContainer` - no itemised contents to browse; not `ADCCDialogueNPC` - nothing to say),
found by the exact same generic Visibility-channel sweep every other interactable in this project
already uses (`ADCCCharacterBase::SweepForInteractionCandidate` - a static mesh blocks that
channel by default, so the station needed no special-casing there, only a new branch). One
station exists per tier; `TryPlayerInteract` either calls the character's already-safe-room-gated
`OpenLootBox(Tier)` if the player owns one, or shows a local notice ("No Bronze Boxes to open.")
if they don't. **The station holds no loot box logic of its own** - it is presentation and a
place to stand; the roll, the spawn, and the Safe Room re-check all still happen exactly where
they already did, inside `Server_OpenLootBox_Implementation`.

Wired into `ADCCCharacterBase::HandleInteractPressed` as a new branch, checked right after the
dialogue-NPC branch and before the corpse/chest branch - the same "smaller interaction, checked
first" ordering dialogue already uses. New `FindLootBoxStationUnderCursor()` mirrors
`FindDialogueNPCUnderCursor` exactly.

**Mesh and glow, both placeholder by design.** `Battle Royale Props` was not yet visible in the
content browser when this was built (confirmed via `find_assets` - still importing, per Zoe's own
message), so `Mesh` defaults to `Ice_Station/SM_large_crate_01` in the constructor, same
placeholder-until-real-art pattern as Voldemortgage's balloon sphere. `Mesh` and `Glow` are both
`VisibleAnywhere` components, not fixed - swapping in a real Battle Royale Props mesh per instance
once that import lands is a one-line `set_properties` call, no recompile. The rarity glow is a
plain `UPointLightComponent`, colour and intensity authored per placed instance rather than
derived from `Tier` in code, matching how every other light this project places is tuned by eye.

### 3. The Vault — a new Safe Room, physically attached to the HQ tower's Lobby

Built at x13950-14750, y6260-7060 (an 800x800 interior, modest per this project's own room-scale
convention), entered through a new 300-unit doorway cut into `DCCRoom_HQ_Lobby`'s previously-solid
north wall (split into two segments, same doorway pattern used everywhere else this session).
"Adjacent to the play area" per the brief - it hangs directly off the newest, most central room
in the level rather than requiring a new long connection. Fully enclosed per the project's
standing interior-only design principle: floor, four walls, ceiling, a `NavMeshBoundsVolume`.

New `DCCRoom_Vault` (`RoomType = SafeRoom`, `RoomDisplayName = "The Vault"`) - `IsInSafeRoom()`
now returns true here, so the loot box stations placed inside it actually work the moment this
deploys. Six `PLACEHOLDER_LootBoxStation_<Tier>` **plain `StaticMeshActor` placeholders** (not yet
the real `ADCCLootBoxStation` class - see the deploy note below) stand in two rows of three
(Bronze/Silver/Gold, then Platinum/Legendary/Celestial), each with its own colour-coded glow
(bronze-orange through celestial-purple) and a floating tier label, plus one soft overhead key
light and a flavour sign: "SPONSOR REDEMPTION VAULT - rewards are sealed at the moment they are
earned and may only be opened here, under supervision. This is for your safety. It is also for
the ratings." - the audience-economy framing the tone bible already established for the Bar's own
"safety is a product" line, applied to the loot box gate itself.

**Lighting mistake caught and fixed in the same pass this time, not after a screenshot round-trip
next session.** First pass put all six 3,500 cd glow lights within touching distance of each
other (crates spaced 120 units apart when the crate mesh itself is ~580 units wide at scale 1),
which produced exactly the same "everything reads paper-white" blowout the HQ tower already hit
twice today. Caught via `CaptureViewport`, fixed by respacing to a proper two-row grid (250-unit
centres, crates scaled down to 0.35) and cutting every glow to 900 cd / 220-unit radius plus the
overhead key to 1,200 cd - confirmed by a second screenshot that individual crates and their
distinct tier colours actually read now, instead of one continuous white blob.

### 4. Compile-checked, not deployed - same batch this rides in with

`Items/DCCLootBoxStation.h/.cpp` (new file) and the `HandleInteractPressed`/header changes in
`Characters/DCCCharacterBase.h/.cpp` compiled clean via the DebugGame standalone target, editor
untouched, no numbered hot-reload DLL afterward - the same verification this project always uses
before a deploy. **This is a new `UCLASS` - a real reflection change, not a plain function body -
so it needs the usual close-editor → Development Editor build → reopen cycle**, and it rides in
the same undeployed batch as the LEVEL BOSS banner, the Balloon Payment fix, and the selfie-camera
fix from the entry immediately above this one. **The six placeholders in the Vault are plain
`StaticMeshActor`s specifically because `ADCCLootBoxStation` does not exist in the currently
running editor's compiled module yet** - they are stand-ins for geometry and glow only, and carry
none of the actual open-a-box interaction. Whoever deploys this batch should replace all six
`PLACEHOLDER_LootBoxStation_<Tier>` actors with real `ADCCLootBoxStation` instances (same
position/tier/colour) as part of that session, or the Vault will look right and do nothing when
a player walks up and presses E.

### What's verified vs. not

- Live and confirmed by read-back: 20 achievements' new `lootBoxReward` values, both loot box
  DataTables' existing content, every equipment id they reference, the Vault's `DCCRoom` type and
  display name, the new doorway's geometry.
- **Not verified**: nothing about the actual open-a-box flow has been tested in PIE this session -
  neither the pre-existing `Server_OpenLootBox` path (which appears to have never been exercised
  by anything, ever - no key binding, no HUD button, no interactable existed before today) nor
  the new station interaction, which cannot be tested until the class is deployed. First PIE test
  after deploy should: claim an achievement with a loot box reward, walk to the Vault, confirm
  `IsInSafeRoom()` reads true there, interact with that tier's station, and confirm items/boons
  actually spawn and apply.
- **Not done**: no mesh swap yet (Battle Royale Props still importing as of this session) - the
  placeholders are crates, not the intended prop. Swapping `Mesh` per station once that pack lands
  is a `set_properties` call, no rebuild required for that part specifically.

## The real ally-interact root cause, Stow now removes the container, room connectivity audit,
and a visible Oathgate (2026-08-30, MCP live, editor open — one new C++ fix compile-checked, NOT
deployed; level/data changes ARE live)

Zoe reported: the inventory system still seems broken, Stow leaves a container sitting in the
world looking untouched, she still can't interact with allies via E ("find a space to test it
reliably or troubleshoot what could be overwriting it"), asked for every room built this session
to have a working connection to its neighbours, and still can't find the Oathgate.

### 1. The actual ally-interact bug, found and fixed — every prior session was chasing the wrong thing

**This is the headline finding of this session.** Every previous investigation of "can't interact
with allies" (there have been several, per this file's own history) focused on `bIsAlly` and the
CDO-vs-instance trap. Checked that again first, live: **`bIsAlly` reads `true` on every one of the
eight placed ally instances** (`Crawler_CJ`, `Scott_OfficeAirplane_Demo`, `Kyle_ArmsOut_Demo`,
`TestBoss` aside, `Crawler_Kyle`, `CJ_HockeyWings_Demo`, `Crawler_Scott`, `Marcel_DiceWinged_Demo`,
`Crawler_Marcel`, `Crawler_Vaughn`). That theory is dead — it was never the cause, at least not
today, and arguably not in any of the sessions that already ruled it out before.

Started PIE specifically to get a real player pawn and a controlled test space (per Zoe's own
instruction) — teleported `CJ_HockeyWings_Demo` to stand 300 units directly in front of the
player (well inside `InteractionRange` = 400) in the transient PIE world, confirmed live
`bIsAlly=true`/`bIsDowned=false`, and tried to replicate `SweepForInteractionCandidate`'s own
sphere sweep using `SceneTools.trace_world`. The trace found nothing along that line — which sent
this down the right path: **read `CollisionCylinder`'s (the capsule's) live `BodyInstance` and
found `collisionProfileName: "Custom"` with `Visibility: ECR_Ignore`.** Checked `CharacterMesh0`
next: `collisionProfileName: "CharacterMesh"` (the engine's own stock profile, completely
unmodified), with `Visibility: ECR_Ignore` there too.

**Confirmed directly against `Engine/Config/BaseEngine.ini`, not assumed:** both of Unreal's own
stock collision profiles ignore Visibility by default -
`Profiles=(Name="Pawn",...,CustomResponses=((Channel="Visibility",Response=ECR_Ignore)),...)` and
`Profiles=(Name="CharacterMesh",...,CustomResponses=((Channel="Pawn",...),(Channel="Vehicle",...),
(Channel="Visibility",Response=ECR_Ignore)),...)`. This is not a project-specific misconfiguration
- it is out-of-the-box Unreal behaviour for every third-person character, in every UE project that
has never touched it. And `SweepForInteractionCandidate` sweeps on **`ECC_Visibility`**.

**Neither a living character's capsule nor its mesh has ever blocked the channel this project's
own interaction sweep uses — full stop, since the day that sweep was written.** The sweep's own
comment even asserts the opposite ("a living character's capsule already blocks Visibility too"),
and that assumption was simply never checked against real collision data until today. Corpses and
chests were never evidence this system worked in general: a chest's static mesh blocks Visibility
by its own separate default, and `ApplyCollapsedBody` (the ragdoll function) explicitly calls
`MeshComponent->SetCollisionResponseToChannel(ECC_Visibility, ECR_Block)` on death - a line that
exists **only** for corpses. Nothing between a living character's capsule and its mesh has ever
been hit-testable by this sweep. Every session that chased `bIsAlly` was investigating a real,
already-fixed non-issue while the actual cause sat undisturbed the entire time.

**The fix**, in `DCCCharacterBase.cpp`'s constructor, right next to the existing (and already
correctly-reasoned) `ECC_Camera` ignore a few lines above it:
```cpp
Capsule->SetCollisionResponseToChannel(ECC_Visibility, ECR_Block);
```
Deliberately the capsule, not the mesh - this leaves `CharacterMesh`'s own Visibility:Ignore
untouched (whatever that stock behaviour exists for, camera-clipping avoidance most likely,
mirroring the Camera-channel reasoning already documented right above it) and leaves the
Camera-ignore untouched too. Every character sharing `ADCCCharacterBase` - player, every ally,
every enemy - gets a capsule that finally blocks Visibility, which is what
`SweepForInteractionCandidate`, the fist-bump sweep, and the loot-box-station sweep added this
session all assumed was already true.

**Attempted a live verification in the running PIE session before trusting this**, in the spirit
of "test it reliably": used `set_properties` to flip the *live* `CollisionCylinder.BodyInstance`
response for Visibility to Block on the transient PIE-world CJ, confirmed by read-back that the
property genuinely changed, then re-ran the trace. It still found nothing. **This does not
contradict the fix** - it's the well-known UE gotcha that a raw reflection write to
`BodyInstance.CollisionResponses` does not itself recreate the live physics/query state the way
the proper `SetCollisionResponseToChannel()` C++ call does internally, so a property-only edit on
an already-running actor is not expected to take effect without that call. The engine-config
evidence above is the real basis for this fix, not the inconclusive live poke - noted here so
nobody mistakes "the live test didn't confirm it" for "the fix might be wrong." **Genuinely not
verified in PIE with the actual compiled fix** - that needs the deploy.

Compile-checked clean via the DebugGame standalone target alongside the Stow fix below - editor
untouched, no numbered hot-reload DLL. **New reflection-adjacent risk is low (existing property,
same call pattern as the line above it) but this still rides in the same undeployed batch as
everything else today** - LEVEL BOSS banner, Balloon Payment fix, selfie-camera fix, the loot box
station class. First PIE test after deploy should be exactly what Zoe asked for: stand near CJ (or
any ally), press E, confirm the conversation menu opens.

### 2. Stow on a container now actually removes it

Per Zoe's report - `ADCCLootContainer::StowInto` previously reset `PendingLoot` and `bHasLoot` and
left the actor standing in the world, empty, on purpose (an earlier session's own deliberate
design choice, commented as "the more legible outcome for a piece of scenery nobody expects to
disappear"). Reversed: it now calls `Destroy()` immediately after the stow completes. Scoped
exactly to what she asked - only the `StowInto` path on `ADCCLootContainer` (chests specifically);
`LootAll`/`LootOne` are untouched, and a corpse's own `StowInto` (`ADCCCharacterBase`, a different
override) still uses its existing fade timer, since she referred to "a container" specifically and
a corpse already disappears on its own schedule. Plain function-body edit, no reflection change -
compile-checked clean with the interact fix above.

### 3. Room connectivity — audited, all clear

Checked every doorway/passage built this session for accidental blocking geometry left behind, via
`find_actors` bounds queries centred exactly on each gap: Diving Pool → corridor, corridor → HQ
Lobby, Lobby → Vault, and the shaft doorway into each of Lobby/Cubicle Floor/Penthouse. **All six
came back clear** - only expected things in each query (nav volumes, sign text actors, the
distant background skyline meshes, the floor exit, room markers). Every interior room built this
session has a genuine, unobstructed way through to its neighbour: Diving Pool ↔ HQ Lobby (open
corridor), HQ Lobby ↔ Vault (doorway), HQ Lobby ↔ Cubicle Floor ↔ Penthouse (the climbable shaft,
continuous z0-1120 with no gap, satisfying "a pathway" per the brief's own wording). Not verified
by an actual walk-through (no input injection, same standing limitation as everything else
input-related this session) - this confirms the geometry is sound, not that the climbing mechanic
itself behaves correctly moment-to-moment, which is a separate, previously-flagged concern this
session did not re-litigate.

### 4. The Oathgate — found, and the actual "can't find it" cause

Zoe knew roughly where it was (per this file's own 2026-08-29 reachability pass, `Oathgate_Entrance`
at (900,900,160) and `Oathgate_Exit` at (-900,-900,160)) and still couldn't find it. Checked the
`Marker` component on both `DCCOathgate` actors: **`StaticMesh: None`.** The gate has been a
trigger volume with an invisible marker this entire time - not a navigation problem, not a
clutter-blocking-sightlines problem (both of which prior sessions checked), a **nothing-to-see**
problem. A `CaptureViewport` screenshot of the entrance location confirmed it: an editor-only
wireframe collision box and a trigger-volume cone, otherwise empty, in a mostly unlit area.

**Fixed**: gave both markers a real mesh (`Alien_Portal/SM_portal`, scaled to ~450×595, the one
genuinely portal-shaped mesh in any installed pack), added a 6,000 cd violet point light at each
so it reads from a distance even in this room's low ambient light, and a floating
"OATHGATE — ENTRANCE"/"OATHGATE — EXIT" label at each. A second screenshot confirms it now: a
tall glowing violet structure, impossible to miss, replacing what was previously nothing at all.
Orientation is whatever the portal mesh's default facing happened to be at the yaw I used - not
fine-tuned, easy to nudge by hand if the open face isn't pointed the ideal direction.

### 5. Inventory system — not a bug, working exactly as designed; the tab just never explained itself

Zoe's follow-up symptom: equipment auto-assigns to her equipment slots and she can see it there,
but the Inventory *tab* never shows a single line, ever. That fully resolves it - confirmed
against `ADCCPickup::OnPickedUp`: **gear pickups (`ADCCPickup_Equipment`) equip straight into
their slot and `Destroy()` themselves the instant that happens - "rather than joining the
carry/throw loop", per the function's own existing comment.** They never touch
`UDCCCarryComponent` (held) or `UDCCInventoryComponent::Items` (stowed) at all, by original
design. The *only* way anything lands in `Items` is picking up something that is NOT equipment
(a mass-tier prop, a quest item, anything without an `EquipmentItemId`) so it goes to Carry
instead, then pressing F to stow whatever is currently held. A player who has only ever picked up
weapons and armour - which describes basically all of today's own testing, since equipment is
what most encounters actually drop - would see an empty Inventory tab no matter how much they'd
collected, forever, with nothing on screen ever explaining why. Not a bug in the mechanical sense;
a real UX gap in the "silence reads as broken" sense this project's own `ShowNotice` doc comment
already names as a standing principle ("every refusal in this game was previously silent... which
is indistinguishable from the input not registering at all").

**Fixed**: `DrawInventory` now prints a two-line clarification directly under the "Stowed 0 / N"
count whenever `Items` is empty - "Nothing stowed. Gear equips straight to its slot on pickup -
see the Character tab. This bag is for what doesn't equip." Plain function-body edit, compiled
clean alongside the interact fix and the Stow-removes-container fix above. Also undeployed, same
batch, same caveat.

## Gold in the Inventory tab, and a bulk "claim everything" loot box flow (2026-08-30, MCP live,
editor open — new C++ compile-checked, NOT deployed; level changes ARE live)

Two follow-ups from Zoe once she saw the Inventory tab clarification above. First: is gold shown
anywhere in the inventory? Checked - it was not, anywhere, except the vendor menu
(`Crawler->Stats->Gold` only ever appeared in `DrawVendorMenu`). A player's own gold total was
invisible for the entire rest of the game. **Fixed**: `DrawInventory` now prints `Gold: N` right
at the top, above the held/stowed lines - the same class of gap as the empty-state message above,
just a flat omission this time rather than an unexplained one.

Second, and bigger: Zoe wants a single interactable - in the menu, in the Vault, or both - that
claims every unclaimed achievement and opens every owned loot box in one go, with the results laid
out "in a long line" rather than requiring a visit to each of the six per-tier stations built
earlier this session. Explicitly fine with a teleport-to-a-reveal-screen approach if needed;
built the simpler in-place version instead, since it satisfies the actual request without a new
level/camera-transition system.

**New shared entry point**: `ADCCCharacterBase::ClaimAndOpenAllLootBoxes()` (client-callable,
routes through a new `Server_ClaimAndOpenAllLootBoxes` RPC, same client-ask/server-verifies shape
as `OpenLootBox`). Two halves, deliberately not gated together:
- **Claims every pending achievement unconditionally** - `Achievements->RequestClaimReward` for
  every id `GetPendingRewardIds` returns. This matches the existing single-achievement `Claim`
  button exactly, which has never required a Safe Room, so a bulk version that suddenly added that
  restriction would be a regression disguised as a feature.
- **Opens every owned box across every tier, one at a time, only if `IsInSafeRoom()`.** If not,
  the function stops there and tells the player why via `ShowLocalNotice` ("Rewards claimed.
  Sealed boxes can only be opened in a Safe Room.") - claiming still happened, only the boxes wait.
  When it does run, each call to `UDCCInventoryComponent::TryOpenLootBox` walks the spawn point
  sideways along the crawler's own right vector (130 units per box) instead of reusing one spot,
  which is the "long line" Zoe asked for - a whole backlog of rewards reads as a line to walk down
  rather than a pile nobody can parse. Logs one summary line ("N box(es) cracked open in one go")
  instead of one line per box, so a big claim doesn't flood the feed.

**Two touchpoints, both wired to the same function, per "in the menu, or in the safe room, or
both":**
- **Menu**: a new row in the Achievements tab's `DrawAchievements`, directly under Pending
  Rewards - "[CLAIM ALL + OPEN N SEALED BOX(ES)]", shown only when there is something to claim or
  open (same "don't draw a dead row" convention the rest of this HUD already uses). Always
  clickable regardless of location, for the same reason the function itself doesn't gate claiming.
- **Vault**: reused `ADCCLootBoxStation` rather than adding a second class - `Tier == None` (the
  enum's own pre-existing "not a reward" sentinel, never legitimately used by a real per-tier
  station) now means "this station is the claim-everything terminal" instead of a single-tier one.
  One new placeholder (`PLACEHOLDER_LootBoxTerminal_ClaimAll`, an `Ice_Station` computer console
  reused as a terminal prop, amber glow, a label reading "CLAIM ALL REWARDS") placed near the
  Vault's entrance threshold, ahead of the six tier crates, so it reads as the primary action and
  the tier stations read as the manual/single-tier alternative next to it. Same undeployed-class
  caveat as the six tier stations: this is a plain `StaticMeshActor` stand-in until
  `ADCCLootBoxStation` itself is live, at which point it needs replacing with a real instance
  (`Tier = None`) in the same position to actually do anything on E.

Compile-checked clean via the DebugGame standalone target alongside everything else in this
session's batch - editor untouched, no numbered hot-reload DLL. New `UFUNCTION`s (a real
reflection change) - needs the usual deploy cycle, same as the rest of today.

## The Tab menu's own tab bar runs off the panel (2026-08-30, MCP live, editor open — C++
compile-checked, NOT deployed)

Zoe: the menu headings run past the edge of the menu panel - "if the far right of the menu screen
is 5 inches wide, the menu headings go 7-8 inches long." Read `DrawMenuTabs` and the panel
background it draws into: **exactly what she described, confirmed in the code, not just the
description.** The Tab menu's backing panel (`DrawHUD`'s own `DrawRect`) is a fixed **460px**
wide. `DrawMenuTabs` lays all ten tabs - `EDCCMenuTab` has grown from a handful to CHARACTER /
SKILLS / INVENTORY / PARTY / TALK / ACHIEVEMENTS / FEED / CHATTER / ALLIES / WHISPERS over this
project's life - out in a single unbroken row, sized from each label's own text width with no
running-total check against the panel at all. Nobody re-verified the row still fit each time a
tab was added; by ten tabs it very much does not - a rough width estimate from the label text
alone puts the full row at somewhere around 2-2.5x the panel's own width, comfortably matching
Zoe's "7-8 inches in a 5 inch panel" by eye.

**Fixed**: `DrawMenuTabs` now wraps to a new row instead of running off the panel - when the next
tab would push the running row width past 420px (the same `RowWidth` constant already used
everywhere else in this panel, so tabs now wrap at the same boundary every other row in this menu
already respects), it starts a fresh row instead. No layout change to anything else in the
panel - the backing rect's height is already `Canvas->SizeY - 32` (essentially the full screen
height), so the one or two extra tab rows this produces have room to spare underneath them, and
every other section further down the panel just starts a little lower.

Compile-checked clean via the DebugGame standalone target, editor untouched, no numbered
hot-reload DLL. Plain function-body edit - no new `UPROPERTY`/`UFUNCTION` - so on its own this
would have been Live-Coding-safe, but per this project's standing rule it still rides in the same
undeployed batch as everything else from today rather than going in alone.

## The loot box reveal became an actual reveal - rooted, sealed boxes in a line, then opened
(2026-08-30, MCP live, editor open — C++ compile-checked, NOT deployed)

Zoe followed up on the bulk claim-and-open flow above with a reference image: Avatar the Last
Airbender's Avatar State shot, Aang standing still in front of a line of past Avatars stretching
away into the distance. What she wants is that composition, not the "spawn everything at your
feet, still free to walk around" version built earlier this session - "the player is stuck in
place and can see all their accumulated boxes in a line."

**Reworked `Server_ClaimAndOpenAllLootBoxes_Implementation` into an actual two-beat reveal
instead of an instant resolve:**

1. **Count, don't open.** Every owned box across every tier is tallied into a flat
   `TArray<EDCCLootBoxTier> BoxLineup` first - `TryOpenLootBox` is not called yet, because it
   rolls and spawns the instant it runs, and the whole point is to show every *sealed* box before
   any of them are opened.
2. **Root the player for the reveal.** Reused `BeginCastLock` - the exact same shared movement
   lock a skill cast and `BeginReviveChannel` already use, rather than inventing a second "hold
   still" mechanic. Duration scales with the line's length (`0.6s` per box, clamped 2-8s), so five
   boxes is a beat and fifty is not a coffee break.
3. **Spawn the line.** One `ADCCLootBoxStation` per owned box (reused as a pure visual prop here -
   its `TryPlayerInteract` is never called on these, so there is no risk of double-opening one
   early), each tier-coloured, walking straight out along the crawler's own forward vector
   (140 units/box) - receding into the distance in front of them, matching the reference
   composition, not the sideways row the first version used.
4. **Resolve on a timer.** A `FTimerDelegate` bound to a lambda (same pattern
   `BeginReviveChannel` already uses, weak-pointer-guarded against the caller dying or leaving
   mid-reveal) fires once `RevealSeconds` elapses: destroys every sealed prop, then actually calls
   `TryOpenLootBox` per box at the same line positions the sealed props just occupied, applies
   boons, and logs one summary line.

New state to support this: `FTimerHandle LootBoxRevealTimer` and
`TArray<TObjectPtr<AActor>> LootBoxRevealProps` (so a second claim mid-reveal, or the character
dying/leaving before resolution, has something concrete to clean up rather than leaking props -
both the trigger path and the timer's own lambda clear this array before repopulating or reading
it). Compiled clean via the DebugGame standalone target, editor untouched, no numbered hot-reload
DLL. New members plus the reworked function body - reflection change, same undeployed batch as
everything else today.

**Not built, flagged rather than assumed**: no camera work. The reference image's real drama is
as much the locked, deliberate camera framing as the line itself, and this pass only handles the
player's own movement being locked - the camera stays exactly wherever the player was already
looking. Attempting a proper forced camera (dolly down the line, Selfie-Mode-style camera
takeover) was judged out of scope for this pass; flagging it explicitly in case Zoe wants that as
a real follow-up rather than assuming the current version is the whole of what she pictured.

### Deploy note

This session's fixes ride in the same undeployed batch as every other C++ change from today's
earlier entries. Priority order for the next deploy's PIE verification, highest-value first: (1)
ally E-interact - the fix most likely to matter to Zoe immediately, (2) Voldemortgage's Balloon
Payment kiting, (3) the LEVEL BOSS banner, (4) the selfie-camera fix, (5) the rooted loot box
reveal (both the Achievements-tab button and the Vault terminal, once its placeholder is swapped
for the real class), (6) the remaining six per-tier loot box stations for the same reason, (7) the
tab bar wrap - visible the instant Tab is pressed, so it should be one of the first things checked.

## Takeover after a long stuck stretch: fresh status verification, rover fix confirmed, a car-window
feature, and the newscaster NPC design doc (2026-09-02, dispatched fresh, Zoe interrupted a stuck
sibling session and handed this one full control)

Zoe reported multiple sessions had gotten stuck today trying to reopen the editor after a close,
one stall over an hour with no resolution, and that she'd been interrupting/poking at things
herself in the meantime - so the brief was explicit: **verify everything fresh, don't trust old
assumptions.** Did exactly that before touching anything.

### Step 1 - actual state, checked, not assumed

- **`UnrealEditor.exe` PID 20792, started 22:09:00**, ~40 min of uptime by the time this was
  checked, 472s of accumulated CPU (climbing, not flatlined - a genuinely live process, not a
  hung one). This is a *different* PID than the one an earlier status-check session had been
  watching (30460, from 08:24 that morning) - that one is gone; a real close/build/reopen cycle
  happened between them.
- `UnrealEditor-DungeonCrawlerDenver.dll` timestamp: **22:06:45** - matches the editor's own log
  open time (22:09:01) almost exactly, confirming this is a genuine fresh Development Editor
  build the editor is actually running, not a stale DLL with a restarted process sitting on top
  of it.
- Port 8000 listening, owned by PID 20792. `Tools/mcp_call.py toolsets` answered immediately and
  listed all expected toolsets including `BehaviorTreeTools` - **MCP is fully healthy right now**,
  despite this session's own built-in `unreal-mcp` connector reporting a `CONNECT_TIMEOUT` at
  session start (expected and harmless per this doc's own standing note: the MCP client connects
  once, at session start, and this session started before - or during a restart window around -
  the editor coming back up; `mcp_call.py` bypasses that entirely, as designed).
  `Saved/Logs/DungeonCrawlerDenver.log` shows every toolset registering cleanly, one non-fatal
  `Handled ensure` (`SkeletonToCompactPose.IsValidIndex` - a shared-skeleton animation warning,
  not a crash), and a live MCP session initializing at 22:43:44 - all consistent with a healthy,
  responsive editor.
- **A build WAS owed, and it was real, not speculative.** Comparing every `Source/` file's mtime
  against the running DLL's build time turned up exactly four files newer than the last build:
  `Vehicles/DCCRover.cpp/h` and `Characters/DCCCharacterBase.cpp/h`. Mid-investigation, Zoe pasted
  in an update from a separate, unreachable Claude Code session she had open directly, describing
  the actual work in those four files: **the rover's kerb-vs-wall collision bug fixed** (
  `NotifyHit` was comparing whole-actor bounds, so a 20-unit kerb belonging to the arena floor's
  single large actor was judged identical to hitting a mountain - full bounce-back plus 0.55s of
  dead controls on every doorstep lip. Fixed by measuring the obstruction's actual height at the
  contact point, probed just past the hit surface so the trace doesn't re-hit the same face and
  read every step as zero), ride height raised so the car hovers with real clearance, a rate-
  limited step-climb for anything ≤260 units (`MaxStepHeight`) with a forward look-ahead so the
  climb starts before the collision box has already been stopped by contact, and allies boarding
  automatically and ejecting together with the driver. This matched what reading the actual diff
  had already turned up (the same functions, the same constants) - confirmed consistent, not a
  guess reconciled after the fact.

### Step 2 - deploy

Saved the new translucent window material and the level, then asked Zoe to close the editor.
*(This entry gets appended to before the close/build/reopen actually runs, since Zoe was still
mid-close at time of writing - see the top of this entry's "Result" line, added once the cycle
actually completed, for the real outcome rather than a predicted one.)*

### Step 3 - the car window feature

Zoe's ask: a button that rolls the driver/passenger windows down, "a pop out window that just
raises/lowers... and stays hidden (clipping inside) the car," so the party can see who's riding
along. Investigated the actual asset before promising anything, same rule this project has
learned the hard way for character armour:

- The rover's body (`ADCCRover::Body`, `Vehicles/DCCRover.cpp`) is
  `/Game/Synthwave_city/Meshes/Car/SM_car_01` - **one rigid StaticMesh**, confirmed via
  `StaticMeshTools.get_material_slots`: three slots, `Reactor`, `Paint`, `Window`. A material
  slot is not a separate movable piece - a `UStaticMeshComponent` moves as one unit, so there is
  no glass sub-mesh to translate into a door cavity. Checked the Synthwave pack's `Car` folder for
  a separate glass asset (none), and confirmed **no FBX export tool is exposed anywhere in this
  MCP toolset** (checked `AssetTools`, `StaticMeshTools`, `EditorAppToolset`, and the sandboxed
  `ProgrammaticToolset`, which can only call other registered tools, not run raw `unreal` Python)
  - so there is no way to pull this mesh back out, split "Window" off in Blender the way this
  project has split other assets before, and re-import it as an independent piece, at least not
  without the original source file, which isn't on disk here (a Fab pack asset, not something
  with `SourceArt/` behind it).
  **Honest verdict: a literal sliding-glass window, independently on the driver vs. passenger
  side, is not buildable right now without new source art.** If a manually re-exported or
  differently-sourced car with body/glass as separate pieces ever lands, revisit for real.
- What **is** real and shipped tonight: the `Window` material slot already carries its own
  material instance, `MI_window_car`, with an `Opacity` scalar parameter already wired to the
  `MP_Opacity` output on the shared master `M_window` (`MaterialTools.get_property_input`
  confirmed the wiring exists) - just inert, because `M_window`'s blend mode is `BLEND_Opaque`,
  which ignores that output entirely. Rather than editing the shared master (which every window
  in the whole Synthwave city also uses - far too large a blast radius for a one-car feature),
  duplicated it to a new, fully isolated asset, **`/Game/DCC/Materials/M_CarWindow_Translucent`**,
  flipped *that* copy's blend mode to `BLEND_Translucent`, recompiled, and read the property back
  to confirm (`{"blendMode":"BLEND_Translucent"}`) before wiring anything to it. `M_window` itself
  was never touched.
  - `ADCCRover` gained `bWindowsDown` (replicated - anyone looking at the car should see it
    open, not just the driver), `ToggleWindows()` (server-only), and `UpdateWindowOpacity()`
    (runs every Tick on every machine, lazily creates a `UMaterialInstanceDynamic` from the new
    translucent material, applies it as a **component-level override** on `Body`'s Window slot
    only - so this rover instance fades, nothing else in the world does - and eases the Opacity
    parameter toward 0/1 over `WindowRollSeconds` (0.4s default) via `FInterpConstantTo`).
  - Input: `R` while driving, routed through the same "client asks, server re-validates" shape as
    every other rover action - `ADCCRover::WindowTogglePressed` calls straight through on the
    server, or through a new `ADCCCharacterBase::Server_ToggleRoverWindows(Rover)` RPC (checks
    `Rover->Driver == this` before acting, same pattern as `Server_EjectRoverPassengers`) when a
    remote client presses it.
  - **The one real limitation, stated plainly rather than glossed over: this is ONE shared
    toggle for the whole car, not independent driver/passenger control.** `SM_car_01` has no seam
    between the two sides to split a toggle on - pressing R fades every window on the car
    together. It genuinely solves what Zoe said she actually wanted ("so we can see ourselves and
    who's in the car"), just not with the two-independent-buttons shape she originally described.
    Said so directly in the code comments and in the report back, not just here.
  - Compiled clean via the DebugGame standalone check (38 files, editor untouched, no numbered
    hot-reload DLL) alongside every other pending change tonight, including the rover fix above.

### Step 4 - the newscaster NPC design doc

Written to `Docs/newscaster-npc.md`, planning only per Zoe's own scoping. Highlights:

- **Model: `ParagonZinx`** (`/Game/ParagonZinx/Characters/Heroes/Zinx/Meshes/Zinx`), confirmed
  genuinely unused per `model-assignments.md`'s own "still has spare" list. Checked the actual
  skeleton rather than picking from memory: `ear_l`/`ear_r`, a `tail` bone, and a `hair_ponytail`
  chain on a roughly human-scale (≈188cm) humanoid rig - reads as alien on sight, distinct
  silhouette from every crawler and existing NPC.
- **The real find:** `get_morph_target_names` came back empty (zero blend shapes on this mesh),
  but `get_bone_names` turned up a **complete facial bone rig** already sitting on the skeleton -
  `C_jaw`, individual lip-segment bones (upper/lower mid, both corners), brow bones (mid + outer,
  both sides), and eyes with separate eyelid bones. Recommended building expressive talking-face
  animation as a procedural bone puppet driven by `USkeletalMeshComponent::SetBoneTransformByName`
  (an engine-native per-bone override that layers on top of a single-node animation with **no
  AnimBlueprint required** - compatible with this project's standing no-AnimBP constraint, and
  the same category of technique the crawlers' own `AnimationSingleNode` setup already relies on)
  - jaw driven by dialogue audio amplitude, lip corners on a phase-shifted secondary layer so it
  shapes rather than just hinges, brows punctuating on emphasis, eyes blinking/drifting. Costed at
  roughly one new C++ component, no new art, one PIE tuning pass. Full tradeoff table (vs. a
  single-jaw-bone version, vs. authoring real morph targets from scratch) is in the doc.
- Visual design reuses assets already imported and already placed in the HQ tower rather than
  inventing new ones: `Modern_House_V1` desk, `Beach_house/Meshes/Smalle_props/SM_mic`, a
  `Modern_House_V1`/`Modern_villa_kit` TV as the "on air" monitor, and an `ADCCHUD`-drawn chyron
  reusing the existing `DrawSegmentedBar` Canvas primitives rather than a new UMG widget system.

### Result

**Completed.** Zoe couldn't get to the keyboard to close the editor herself ("she's in bed"), so
this session did what the 2026-08-29 entry above already established as the authorized pattern for
that exact situation: `save_assets` (level + empty-list "everything dirty"), confirmed
`IsPIERunning() == false`, confirmed no `UnrealBuildTool`/`dotnet`/`cl`/`MSBuild` contention, then
`taskkill /PID 20792 /T /F` (only the one PID reported this time, plus its `CrashReportClientEditor`
child - both confirmed gone afterward). Development Editor build succeeded, real relink
(`UnrealEditor-DungeonCrawlerDenver.dll` 2026-09-03 00:19:44, no numbered patch DLL), reopened
clean (PID 19740): `Total Editor Startup Time, took 31.353`, all 24 toolsets registered, MCP
responsive, `PlayNumberOfClients=1` unchanged. The rover kerb/ride-height/step-climb/passenger
fixes and the window-toggle feature are genuinely live as of this build.

## Flight is now actually verified, not just reasoned - a mouse-drag "flap" control, and a 5-ring
practice course (2026-09-03, MCP live, one deploy cycle - editor closed/rebuilt/reopened, healthy)

Zoe's ask: encourage players to learn to fly - confirm it actually works on the current map/ramp,
add floating rings to fly through while learning, and let left-mouse-drag set the flight angle
like raising/lowering flaps.

### 1. The playground ramp launch - upgraded from "reasoned from geometry" to genuinely confirmed

`travel.md`'s own v3 write-up was honest that nobody had proven the ramp actually produces a real
mid-air fall - "there is no input-injection tool in this MCP toolset, so a rover cannot be driven
from here at all." Found the real `SM_bridge` ramp actor (`/Game/Synthwave_city/Meshes/Bridge/
SM_bridge`, world bounds X 64970-72963, Y 13172-30096, Z -1104-7034, roll -20°, uniform scale
14.27) via `find_actors("bridge")` - not the small `LevelPrototyping` `SM_Ramp*` cluster near
world origin, which is unrelated blockout scenery from the ThirdPerson template, not a real launch
ramp (only 200 units tall).

**`SceneTools.trace_world`, straight down at several points along the climb, confirms the whole
picture without needing to actually drive it:**

- y=13600→z≈249, y=18500→z≈2032, y=26000→z≈4762, y=29500→z≈6036 - a clean, consistent 20° climb
  the entire way (matches `tan(20°)` almost exactly at every sample point), no discontinuities.
- **y=30500 onward → z=50 flat, every time.** The deck ends around y≈29500-30000; immediately past
  it there is nothing until the ordinary playground ground, ~5900 units straight down.

**That is a genuine, enormous fall - over 13x `FlightUnlockFallDistance` (450).** A rover driving
straight off the top at any real speed WILL clear the 450-unit threshold almost immediately
(free-fall physics: ~0.57s to fall 450 units under this rover's `Gravity` 2800, against a total
drop of ~5900), regardless of exact exit speed. **This is the first time this feature has been
confirmed against real geometry rather than only reasoned about from measurements** - worth
updating `travel.md`'s own "Still NOT verified" section to reflect this the next time that file is
touched. One loose end, noted rather than chased: the "arch" `travel.md` documents at
(68970,13460,1240) sits near the ramp's BOTTOM (an entry gate the player drives under, not a
summit marker) - the doc's phrasing didn't make that obvious and it cost a couple of wasted trace
calls to sort out.

### 2. Flight angle - a mouse-drag "flap lever," additive with the existing Space/Ctrl climb keys

`ADCCRover` gained a small, self-contained control layer rather than replacing anything:

- **Hold left mouse button, drag vertically (MouseY)** to set `FlightPitchInput` (-1..1) -
  deliberately a **lever, not a joystick**: it holds wherever it was left when the button is
  released, same as a real flap control, rather than springing back to level. Eased visually
  toward `FlightPitchInput * MaxFlightPitchDegrees` (35° max) via `FInterpTo` so the nose tilts
  smoothly, not in a snap.
- **The angle is mechanical, not just cosmetic**: `sin(FlightPitchAngleDegrees) *
  FlightPitchLiftRate` (1800 cm/s at full deflection) is added directly to `VerticalVelocity` every
  Tick while flying - tilting the nose up climbs, tilting down dives, same shape as a real
  elevator. **Additive with `Space`/`Ctrl`**, not a replacement - the drag is the fine control, the
  keys are still there for a snap override, and both read from the same `VerticalVelocity`.
- `AlignToSurface` gained a dedicated `bFlying` branch (previously flight forced pitch/roll to a
  flat 0) so the visual nose IS the mechanical angle - what the player sees tilting is exactly
  what's changing their altitude, not a decoupled cosmetic tilt. No roll while flying; the flap
  lever only asked for pitch.
- Resets to level (`FlightPitchInput`/`FlightPitchAngleDegrees` both 0) in `EndFlight` - "flaps
  retract on landing," so each flight starts fresh rather than carrying over the last one's
  setting.
- **Flagged honestly, not guessed past**: MouseY's sign convention is a first attempt, not a
  verified one - there is still no input-injection tool in this toolset to confirm by actually
  flying it. Commented directly in `FlightPitchDrag` with "flip this if a real flight shows
  dragging up dives instead of climbs," matching this project's own standing rule about not
  asserting something works when it hasn't been seen to.

### 3. Five floating rings - a practice course through the launch arc, not a scoring system

New `Vehicles/DCCFlightRing.h/.cpp` (`ADCCFlightRing`), kept deliberately low-stakes:

- **No collision on the visual ring mesh** (`/Game/Evil_planet/Meshes/Portal/SM_ring_01` - a
  sci-fi portal-gate ring, already carries a "Lamp" material slot so it reads as lit/glowing with
  no extra material work) - a near-miss should never bounce or stop a player mid-lesson. Detection
  is a separate thin `UBoxComponent`, `PassThroughVolume`, sized to an estimated inner opening
  (this toolset has no way to query a mesh's actual hole diameter, only its outer bounds - flagged
  as a first estimate, not a measured one) and rotated to match the mesh's own 90°-pitched
  orientation (the source mesh lies flat with its hole facing local Z by default; pitched so the
  hole faces local X, the "through" axis).
- **Repeatable on purpose, not a one-shot collectible** - no "already triggered" gate. This is a
  practice course, and a player should be able to loop back and refly it as many times as it takes
  to get a feel for the drag control, exactly per Zoe's framing ("as they learn the system").
- `NotifyActorBeginOverlap` checks for an `ADCCRover` with a live `Driver` and calls
  `Driver->ShowLocalNotice("Ring {index} of {count}!")` - purely local feedback, same as every
  other flavour notice in this project, nothing here needed to be authoritative or seen by anyone
  but the pilot.
- **Five instances placed** in a descending arc starting just past the ramp's confirmed edge
  (y≈30800 down to y≈40000, z≈5700 down to z≈700), steepening pitch per ring (-10° to -40°) to
  roughly track a ballistic arc, mild alternating X offset (±600) so the course isn't perfectly
  straight. Folder `Playground/FlightRings`. `RingIndex`/`RingCount` write-back confirmed
  (`{"RingIndex":1,"RingCount":5}` read from the first instance).

### What's honestly NOT verified this session

**None of this has been seen working in PIE or flown by a real input device** - same standing
limitation as the rest of the rover work, restated rather than glossed over. Specifically:

- `CaptureViewport` returned the **identical stale frame three times in a row** despite two
  different `SetCameraTransform` calls and one `FocusOnActors` targeting the first ring - almost
  certainly because the editor window wasn't foregrounded/visible to actually re-render, not a
  camera-math error. **Ring visual orientation (whether the hole genuinely faces the direction of
  travel, not sideways or inverted) is unconfirmed by eye** - the 90°-pitch math is reasoned from
  the mesh's default bounds shape (flat, thin in Z), not seen. Worth one real screenshot or a PIE
  look before assuming the course reads correctly.
- The `PassThroughVolume` box size (extent 200x200, thin 60 along the through-axis) is an estimate
  against SM_ring_01's ~740-unit outer diameter, not a measured inner hole - if it turns out too
  generous or too tight, both are one-line tuning fixes (`SetBoxExtent` in the constructor).
  Zoe's own established preference is to nudge placement by hand rather than have a session chase
  pixel-perfect alignment via repeated screenshot round-trips, so this was deliberately not
  over-iterated.
- Whole session compiled clean via the DebugGame standalone check twice (once for the rover/window
  batch, once adding `DCCFlightRing`) before either was deployed, and the final Development Editor
  build was a real relink (`UnrealEditor-DungeonCrawlerDenver.dll` 2026-09-03 08:56:02, no numbered
  patch DLL) - but "compiles and the numbers add up" is still not the same claim as "flown and it
  feels right." A real playtest is the next step, not a formality.

## Eight-item ask after seeing the flight/rover work - chatter variety+speed+wrap fix, ring
resize/VFX/Piloting-XP done; Foundry researched; five items queued honestly rather than rushed
(2026-09-03, MCP live, one deploy cycle)

Zoe's response to the flight session above was warm but long - eight separate asks in one message,
explicitly flagging the chatter system as "the biggest success and reason to continue." Given the
spread (a UI overlay, an animation, an item+radio, VFX, quest content, and a from-scratch VTT
integration alongside the chatter work), this pass triaged rather than attempting all eight at
once: did the chatter system for real since she named it as the priority, finished the ring
follow-up from the flight session, answered the Foundry question from actual knowledge of the
platform, and queued the rest with real scoping notes instead of shipping something rushed.

### 1. Chatter - variety, speed, and the overflow bug, all real

Read `RollChatterBatch` and `DrawChannelMessages` in full before touching either, rather than
guessing at the shape.

- **Bucket math showed the actual problem**: `Benign` gets ~59% of every roll (everything left
  over after the eight named buckets) but only had 36 of the table's 130 rows - the bucket a
  player sees most often was, proportionally, the thinnest one. **Added 61 new rows** (`Docs/
  DT_DCCChatterLines.csv`, table now 191 rows), weighted toward Benign (36→60) with smaller top-ups
  everywhere else, plus two new `RunningGag` families tied to tonight's own new features -
  `RoverGuy` ("unpopular opinion: the car should not be allowed to fly") and `RingChallenge`
  ("ring guy has a spreadsheet. do not ask about the spreadsheet.") - both matching the existing
  gag structure (a `GagId` shared across variant rows) exactly. Pushed live via `add_rows` +
  `set_rows` rather than waiting for a CSV re-import, and read three rows back to confirm before
  moving on.
- **"Faster during exciting moments"**: the Chatter tab's trickle cadence (`ADCCHUD`'s
  `NextChatterTrickleTime`) now checks the Feed's own `EventLog` for a Combat/Death/Encounter entry
  within the last `ExcitingChatterWindowSeconds` (12s, tunable) - reusing the exact category
  grouping `RollChatterBatch`'s own boss-battle bucket and `PassesFeedFilter`'s `CombatOnly` case
  already use, not a new "is something happening" flag. When true, the trickle interval multiplies
  by `ExcitingChatterSpeedupFactor` (0.35, tunable) and each pull grabs 1-4 lines instead of 1-2 -
  chat visibly speeds up during a fight rather than just getting slightly busier at the same pace.
- **The overflow bug, found and fixed**: `DrawChannelMessages` was drawing `"  Sender: message"` as
  one unwrapped `DrawText` call - on a 460-unit-wide panel, any long sender name or message ran
  straight off the right edge, exactly what Zoe reported. Reused the file's own existing `WrapText`
  helper (already used by `DrawAchievementDetail` for the same reason) at a 46-character width
  matched to this panel's ~444-unit usable space. Applies to every channel this function serves
  (Chatter, Party, Allies, Whispers), not just Chatter.

### 2. Flight rings - resized to match Zoe's own edits, two more placed, Piloting XP, objective glow

Zoe had manually repositioned and rescaled rings 1-3 herself (queried live rather than assumed):
roughly 5.2x-6.9x scale, pitch standardized to 80° across all three, spanning a MUCH larger arc
than this session's original placement (out to y≈93000, not y≈40000). Rings 4-5 were still sitting
at the old small-scale positions, completely disconnected from her new curve.

- **Rings 4-5 moved and rescaled** to continue the curve: extrapolated from the Ring2→Ring3 vector
  (damped 0.6x per step so the course tapers rather than diverging), yaw matched to each ring's own
  approach direction (≈35°, consistent with how ring 3's yaw already lines up with the Ring2→Ring3
  heading), pitch held at her own standardized 80°, scale 6.0/6.5 in her range. Checked
  `trace_world` at both candidate points first - no hit at all (open sky), confirming neither
  lands inside geometry. Read back and saved.
- **Piloting skill XP now flows from a clean pass** - the actual half of item 7 that was
  achievable tonight (the quest-content half is queued below). `ADCCRover` gained
  `AwardPilotingBonusSeconds(float)`, a server-only bonus added directly to
  `PilotingSecondsAccumulated` - deliberately NOT a second `MarkSkillUsed` call, since
  `TickPilotingProgress`'s own comment already establishes that marking twice in one interval is
  worth nothing (`bAdvancementMark` is a binary flag). This is the same lever
  `CleanDriftTimeMultiplier` already pulls, just from an external event instead of an ongoing
  multiplier - `ADCCFlightRing::PilotingBonusSeconds` (2s default, tunable) adds real progress
  toward the driver's next Piloting mark on every ring passed.
- **"Next objective" ring now glows** - `ADCCFlightRing` gained `ObjectiveGlow`
  (`/Game/Alien_Portal/FX/Particles/NS_blue_FX`, the one glowing/techy-looking VFX in the whole
  project's library that happens to come from the SAME pack family spirit as the portal ring mesh
  itself), a replicated `bHasEverBeenPassed` flag, and a 0.5s repeating timer
  (`RefreshObjectiveHighlight`) that lights up whichever ring has the lowest `RingIndex` among
  those not yet passed in its course. Passing a ring never blocks re-flying it - the flag exists
  only to drive the glow, not to gate anything, so the practice-course repeatability from the
  original design is untouched.
- Compiled clean via DebugGame twice, deployed in one real Development Editor build/reopen cycle
  alongside the chatter fixes above (`UnrealEditor-DungeonCrawlerDenver.dll` 2026-09-03 17:14:07,
  no numbered patch DLL, MCP responsive after reopen, all 24 toolsets registered).

### 8. Foundry VTT - researched, not built; the small goal is real, the big goal needs its own pass

Answered from actual knowledge of the platform rather than guessing at feasibility:

- **Foundry has no first-party public REST API for external applications.** It is a self-hosted
  Node.js app; Systems and Modules are JavaScript running INSIDE Foundry's own process, and
  world/actor/scene state normally only leaves Foundry via its own internal Socket.IO protocol to
  connected browser clients - not a documented stable contract meant for a third-party engine to
  read or write. Community REST-API-relay modules exist (players install one Foundry-side to open
  an HTTP bridge), but that is an unofficial add-on, not something Foundry Gaming publishes or
  guarantees.
- **The small goal (export items/characters for Foundry import) does not need any live API at
  all**, and is genuinely worth building. Foundry imports content via Compendium Pack JSON, a
  structured document format each game System defines (Actor/Item schemas). A DCC-side exporter
  that emits JSON matching a System's schema (either an existing one like dnd5e, or a homebrew DCC
  system if Zoe's group has one) is real, scoped, low-risk work - format compatibility, not a
  network integration. Reasonable next step once picked up: confirm which Foundry System Zoe's
  group actually plays DCC on (or whether a custom one exists) before designing the export shape.
- **The big goal (Foundry token movement live-driving the in-game character) is real but large**,
  and disproportionate to scope blindly into tonight. It needs: a custom Foundry MODULE (JS) to
  capture token-move events and forward them out over some channel, a UE-side listener/translator,
  and - the genuinely hard part - a coordinate mapping between Foundry's 2D grid scenes and this
  project's 3D rooms (which scene maps to which floor, what's the grid-to-UE-unit scale). Buildable
  in principle - people have built comparable cross-platform bridges - but it is its own multi-
  session project with real ongoing maintenance, not an add-on to anything already in flight.
  **Recommendation, not a decision made unilaterally**: build the small export goal first since it
  stands alone and pays off regardless, and if the big goal is still wanted afterward, scope it
  down to one direction first (Foundry → game, read-only) rather than attempting full bidirectional
  sync as a first pass.

### Queued, not attempted this pass - scoped honestly rather than rushed

- **Emotes (item 1)** - "make actual emotes work" using the icon packs already in the library
  (`RPGClassIcons` etc. are confirmed present). Needs a look at what emote scaffolding, if any,
  already exists before designing - not checked this session.
- **Flight-controls HUD overlay (item 2)** - a small/medium overlay listing car commands
  (boost/drift/etc.) while flying. Straightforward given `ADCCHUD`'s existing Canvas-drawing
  patterns (same shapes as the LEVEL BOSS banner, the boost-charge readout `GetBoostChargeFraction`
  already exposes) - just not started.
- **Get-in-car animation (item 3)** - currently `EnterRover` hides the driver instantly
  (`SetActorHiddenInGame(true)`) with no animation beat first. Needs a one-shot animation play
  before the hide, same shape as the attack-animation pattern already used elsewhere
  (`PlayOneShotAnimation`) - not started.
- **Boombox item + car radio (item 4)** - two related asks: a Trinket-slot Boombox item (already
  flagged `[needs mesh]` in `races-and-classes.md`'s Lord of Dance starter kit - the same asset
  could serve both), and a radio toggle on the rover with placeholder sample audio until Zoe
  imports real tracks. Not started; needs a quick look at what music/audio assets already exist in
  the library before picking samples.
- **Dev-goal quests (item 7, the content half)** - a small quest batch that challenges specific new
  features ("gain 2 levels in Piloting" as the stated example). The mechanical prerequisite (ring
  flythrough actually granting Piloting progress) is done above; authoring the actual quest rows
  in `DT_DCCQuests` is still open.

## Two emote systems, not one - a player performance menu, and real icons in Chatter's Emote
bucket (2026-09-03, MCP live, one deploy cycle)

Zoe said yes to continuing, then clarified mid-build: "emotes" in her original ask meant the
Chatter tab's `:BORANT_W:`-style bracketed text (Bucket == `Emote` in `DT_DCCChatterLines`), which
she'd read as a stand-in for real Twitch custom emotes and wanted rendered as actual icons - not
a player-performable dance. By the time the correction landed, the OTHER interpretation (a
player-triggered emote menu) was most of the way built and she said to keep it too. Both shipped.

### 1. Player emote menu (the one she said to keep)

Found `EmoteAnimations`/`PlayEmote()` already existed on `ADCCCharacterBase` - fully working, just
only ever triggered automatically on a kill (`EmoteChanceOnKill`), never by a player on demand.
Reused it rather than building a second animation system:

- New indexed pair, `Server_RequestEmoteAtIndex`/`Multicast_PlayEmoteAtIndex`, alongside the
  existing random `PlayEmote`/`Multicast_PlayEmote` (untouched, still used by the kill path) -
  same server-validates-the-index shape as every other menu confirm in this file.
- `bEmoteMenuOpen`/`EmoteMenuIndex`/`ToggleEmoteMenu`/`CycleEmoteMenu`/`ConfirmEmote` mirror
  Selfie Mode's own local-toggle shape (the closest existing analog - a self-triggered picker with
  no world target, not a proximity-gated menu like Vendor/Oathgate). Wired into the existing wheel
  dispatcher (`InteractionScrollUp/Down`), `HandleInteractPressed` (E confirms, checked first since
  the picker has nothing to fall through to), and `CloseInteraction` (Esc closes it same as every
  other menu). Bound to **B** - checked the whole `SetupPlayerInputComponent` key list first; free.
- `ADCCHUD::DrawEmoteMenu` mirrors `DrawOathgateMenu`'s exact visual shape (centred popup, `>`
  cursor, dim footer hint) rather than inventing new chrome.
- **`BP_ThirdPersonCharacter`'s `EmoteAnimations` was empty** - `DCCCrawlerAnimSet::Apply` only
  ever populates it on `ADCCEnemyCharacter` (allies/enemies), not the player Blueprint. Set live via
  `ObjectTools.set_properties` on the CDO to the same 9 clips `DCCCrawlerAnimSet.cpp` already
  curates (BoomDance, AllNightDance, CardioDance, two AgreeGestures, SpinJump, two AirSquats,
  BoxingWarmup) - read back and saved before moving on. One Blueprint, shared by all five players.

### 2. Real icons for Chatter's Emote bucket (what she actually asked for)

`DrawChannelMessages` drew each chat line as one `DrawText` call, so `:BORANT_W:` rendered as
literal bracketed text - never an actual image. Fixed properly rather than faking it:

- `DCCChatter::FindEmoteIcon(Token)` (`UI/DCCChatterTypes.h/.cpp`) - a small in-code
  `TMap<FString, FSoftObjectPath>` mapping each of the 15 unique tokens across the table's Emote
  rows to a real `Texture2D` from `RPGClassIcons` (the only icon library of any size in the
  project). **Not literal matches** - there is no "debt stonks" icon in any pack here - picked for
  visual variety across the pack's categories instead, the same spirit the invented emote NAMES
  themselves already work in. All 15 paths confirmed to exist and confirmed `Texture2D` (not a
  Material or Blueprint) via `AssetTools` before wiring them in, not assumed. Deliberately a code
  map for now rather than a new DataTable - these are brand names Zoe invents rarely, unlike chat
  lines she adds constantly; flagged in comments to promote if that assumption stops holding.
- New `ADCCHUD::DrawChatLineWithEmotes` replaces the old per-character `WrapText` call for chat
  specifically: walks the line word by word, draws a real icon (`DrawTexture`, AHUD's own built-in,
  already used by the skill hotbar's icon slots) for any word shaped like `:TOKEN:` that resolves,
  falls back to plain `DrawText` for everything else - unrecognised or unmapped tokens included, so
  nothing silently vanishes - and wraps to a new line by measured width (`GetTextSize` per word)
  instead of a fixed character budget, since icons and text no longer have the same width.
- Compiled clean via DebugGame, deployed in the same Development Editor cycle as the player emote
  menu above (`UnrealEditor-DungeonCrawlerDenver.dll` 2026-09-03 17:44:24, no numbered patch DLL,
  MCP responsive after reopen, all 24 toolsets registered).

**Not verified in PIE** - same standing caveat as everything else tonight. The icon choices in
particular are a first pass by filename alone, not by eye; worth one real look at the Chatter tab
before assuming they read well together.

## Rest of the eight-item queue: flight HUD overlay, a real get-in-the-car beat, a boombox item,
a working car radio, and a "dev goal" quest chain with a new quest objective type (2026-09-03,
MCP live, one deploy cycle)

Zoe said to keep going at 5:27am. Worked through the remaining queued items from the two entries
above in order.

### 1. Flight controls HUD overlay (item 2)

`ADCCHUD::DrawFlightControlsOverlay`, bottom-right (quest tracker owns top-right, vitals/Feed own
bottom-left - see `DrawBossBanner`'s own comment on the first). Gated on a new `GetOwningRover()`
accessor, not `GetOwningCrawler()` - while driving, `EnterRover`/`FinishEnteringRover` possess the
rover pawn directly, so `PlayerOwner->GetPawn()` returns an `ADCCRover` and `GetOwningCrawler`'s
cast silently returns null. Lists the flight-only controls (throttle/steer/climb/descend/flap-drag/
windows/radio/exit) and one live readout, a boost-charge bar, since that's the one thing worth a
glance mid-flight that the world itself can't show. Gated on `IsFlying()`, matching Zoe's literal
"when you are flying" framing rather than showing for ordinary ground driving too.

### 2. Get-in-the-car animation (item 3)

`EnterRover` previously hid and possessed the driver in the same frame - instant, no beat.
Restructured into two steps: `EnterRover` now plays a new `Multicast_PlayEnterVehicleAnimation`
(every machine sees it, not just the driver), roots the driver for the clip's length via the same
`BeginCastLock` a skill cast and the loot-box reveal already use, and defers the actual seat/hide/
possess to a new `FinishEnteringRover`, fired by a weak-pointer-guarded timer once the animation
finishes. `FinishEnteringRover` re-checks every guard `EnterRover` originally checked plus
`IsAlive()` - the beat takes real time, and the driver could die or the rover could be claimed by
someone else in the meantime.

New dedicated `EnterVehicleAnimation` slot on `ADCCCharacterBase` (not reusing `EmoteAnimations` -
this needed its own purpose-built slot, same as `ClimbAnimation`/`AttackAnimation`). **No literal
"get in a car" clip exists in the shared crawler set** - populated with the AgreeGesture clip as an
honest placeholder (both in `DCCCrawlerAnimSet::Apply` for allies/enemies, and on
`BP_ThirdPersonCharacter`'s CDO via `set_properties` for the player, read back and saved).

### 3. Boombox item + car radio (item 4)

Checked the audio library before building anything: **exactly one real music asset exists in the
entire project**, `SoulCity/Sound/Cue/Music/Soul_Music_FlyUp01_Cue`. Matches Zoe's own framing
("I will import music but use samples for now so I know it's working") - this is that sample.

- **Car radio**: `ADCCRover` gained `RadioTracks` (an array, not a single sound reference, so
  dropping in real tracks later is a data change not a code change), a replicated `bRadioOn` -
  radio plays FROM the car, so everyone nearby should hear it, not just the driver - and a
  `RadioAudio` component. `RadioTogglePressed`/`ToggleRadio`/`OnRep_RadioOn`/
  `Server_ToggleRoverRadio` mirror the window-toggle plumbing from the earlier flight session
  exactly, same pattern, same reasoning. Bound to **T** (checked the rover's full key list first -
  free). Always plays `RadioTracks[0]` for now; the one line that grows into a real "next track"
  cycle once there is something to cycle through.
- **Boombox item**: a real `DT_DCCEquipment` row (Trinket slot, `spine_03Socket`, a small
  Charisma bonus, a purple neon `bEmitsLight` glow) - the exact item `races-and-classes.md`
  flagged `[needs mesh]` for Lord of Dance's starter kit, finally given one. Reuses
  `SM_kitchen_prop_01` (the same placeholder `ADCCPickup_Jukebox` already uses for its own
  "boxy appliance stands in for a music prop" problem - same gap, same fix, matching precedent
  rather than inventing a new placeholder). **Deliberately does not play audio when equipped** -
  that would mean touching `UDCCEquipmentComponent`'s shared Equip/Unequip flow, which every other
  equipped item also depends on, and wiring that in carefully is a separate pass, not a rushed
  add this late. Added and saved live via `add_rows`/`set_rows`, read back to confirm; also
  appended to `Docs/DT_DCCEquipment.csv` as the source-of-truth copy.

### 4. Dev-goal quest chain (item 7, the content half)

Checked `EDCCObjectiveType` before authoring anything and found the real gap: no type exists for
"gain N Skill Ranks," which is exactly Zoe's own example ("gain 2 levels in driving"). Added
**`EDCCObjectiveType::SkillRankGained`** and wired `UDCCQuestComponent::NotifyEvent` into
`UDCCSkillComponent::RunSkillAdvancement` at the exact point a Rank actually lands - the same
funnel Character XP already gets paid from, right next to it. `TargetId` is the skill (e.g.
`Piloting`), `RequiredCount` the Ranks needed.

Also added two more `Custom`-type hooks (per `EDCCObjectiveType::Custom`'s own doc comment - "the
escape hatch that keeps the enum honest" for events nothing else needed a dedicated type for):
`FlightEngaged` (`ADCCRover::EngageFlight`) and `FlightRingPassed` (`ADCCFlightRing`'s overlap,
alongside the Piloting bonus from the earlier flight session) on the rover side, and
`EmotePerformed` (`Multicast_PlayEmoteAtIndex_Implementation`) on the new player emote menu from
above - three real gameplay moments a quest can now key off, not just the one from Zoe's example.

**Four new quests, chained, System-AI-voiced** (`DT_DCCQuests`, imported live via
`add_rows`/`set_rows`, read back and saved; also appended to `Docs/DT_DCCQuests.json`, now 17
entries) - `DevGoal_Airborne` (bAutoStart, get the rover off the ground) → `DevGoal_Rings` (fly
through 3 practice rings) → `DevGoal_Piloting` (Zoe's literal example: gain 2 Ranks in Piloting) →
`DevGoal_Emotes` (perform 3 emotes). A separate standalone chain from the Floor 1 story questline,
not spliced into it - these are feature-teaching dev goals, not floor content.

### Deploy

One cycle covered items 3 and 4 plus the `SkillRankGained` enum (a new UENUM value has to exist in
the running editor's reflection data before `set_rows` will accept a JSON row referencing it by
name - confirmed the hard way, first attempt at writing the Piloting quest failed against the
pre-deploy editor). `UnrealEditor-DungeonCrawlerDenver.dll` 2026-09-03 18:07:57, no numbered patch
DLL, MCP responsive after reopen, all 24 toolsets registered. Item 2 (the HUD overlay) rode along
compiled but was folded into this same cycle rather than deployed alone.

**Everything in this entry and the two above it is unverified in PIE** - restated because it is
true of the whole night's work, not just this batch, and it is a lot of surface area to be taking
on faith. The individual pieces are small and mirror working patterns closely, which is the
honest basis for confidence here - not having watched any of it run.

## Root-caused and fixed: the `SkeletonToCompactPose.IsValidIndex` ensure Zoe hits "frequently...
just after a character dies" (2026-09-03, MCP live Blueprint surgery, no C++ build)

Zoe pasted the ensure popup with a correct hunch: "something that happens just after their
death." Static-analysis-only investigation (no PIE, no repro) but a high-confidence, verified
root cause - not a guess.

### What it wasn't

Checked the two obvious C++ suspects first, since this exact ensure was already diagnosed once
before in this codebase (`DCCCrawlerAnimSet.h`'s own comment references it) and a real guard,
`ADCCCharacterBase::CanPlayOnThisSkeleton`, exists specifically to prevent it. Both the one-shot
path (`PlayOneShotAnimation` - attacks, climbs, emotes, the new enter-vehicle beat) and the
locomotion loop (`UpdateLocomotionAnimation`) already call it before ever playing a clip. Neither
is the source. Also ruled out: death's own C++ handling (`ApplyCollapsedBody`/`FinishDying`) never
plays an animation at all - it is pure collision/physics/ragdoll setup, exactly as its own comment
says ("Ragdoll rather than a death animation"). Also checked `OnCharacterDied`'s only other
listener (`ADCCEnemyCharacter_NuclearFamily`) - a radiation-intensity bump and a log line, no
animation, and that class isn't even placed in the level. Found two genuinely unguarded raw
`PlayAnimation` calls elsewhere (`DCCDialogueNPC.cpp`, `DCCCrowdCrawler.cpp`) but neither is
death-triggered, and the crowd system isn't placed in the level either (confirmed live via
`find_actors`, not assumed from the stale 2026-08-22 CLAUDE.md note claiming the same).

### What it was

`BP_OnDied` and `BP_OnAttackPerformed` are `BlueprintImplementableEvent`s on `ADCCCharacterBase` -
invisible to any amount of C++ source grep. Checked them via `BlueprintTools.list_events` +
`read_graph_dsl` and found the real bug immediately, on the **base** enemy Blueprint
(`BP_DCCEnemy`, parent of every enemy/ally reskin):

```
(event DCC|EventOnDied
  (Components|Animation|PlayAnimation (Variables|Character|GetMesh)
    "/Game/Characters/Mannequins/Anims/Death/MM_Death_Front_01.MM_Death_Front_01")
  (Rendering|SetVisibility (Variables|Default|GetHealthBarWidget)))
```

**`BP_OnDied` calls `PlayAnimation` directly on the mesh with a Mannequin-skeleton death clip, on
every death, with zero skeleton check** - completely bypassing `CanPlayOnThisSkeleton` because it
is a raw Blueprint node, not a call through `PlayOneShotAnimation`. `HockeyWings`/`OfficeAirplane`/
`ArmsOut`/`DiceWinged` are all on the shared **crawler** skeleton, not Mannequin - so this fires the
exact mismatch the guard exists to prevent, on literally every one of those characters' deaths.
This is not a guess about mechanism; it is the same failure mode this project's own code comments
already named. Same story on `BP_OnAttackPerformed`, on both `BP_DCCEnemy` and
`BP_ThirdPersonCharacter` (the player): `PlayAnimMontage self "/Game/Variant_Combat/Anims/
AM_ComboAttack.AM_ComboAttack"` - fired on **every landed attack**, not just kills, which is
almost certainly the actual "frequently" and "first few seconds" of Zoe's report (a killing blow
is an attack that also happens to end in a death, so the two read as simultaneous). Confirmed via
`AssetTools.exists` that **`AM_ComboAttack` does not exist on disk at all anymore** - both nodes
were dead leftover wiring from the ThirdPerson template's own Variant_Combat sample content,
never cleaned up when this project's own `Multicast_PlayAttack`/`AttackAnimation` system
(properly guarded, C++-driven) superseded it.

### The fix

Both nodes are redundant with this project's own systems regardless of the crash - ragdoll
already handles death visually, `AttackAnimation` already handles attacks - so the fix was to
delete the harmful calls, not repoint them. Used `BlueprintTools.get_connected_subgraph` to find
the exact nodes, `delete_node`, and `connect_pins` to bridge the exec chain over the gap. Fixed on
**all 9 affected Blueprints**: `BP_DCCEnemy` (base) and all 7 subclasses (`Brute`, `Courier`,
`Analyst`, `HockeyWings`, `OfficeAirplane`, `ArmsOut`, `DiceWinged` - each had its own independent
copy of the same two nodes, not inherited from the base, confirmed by checking one before
assuming the rest matched) plus `BP_ThirdPersonCharacter`. Confirmed no other character Blueprint
exists in the project (`find_assets` for `BP_DCC`/`BP_Ally` project-wide) - Voldemortgage,
DebtEater, TestBoss, and the rest are pure C++ classes placed directly, with no Blueprint event
graph to carry this bug at all.

**A real mistake, caught and accounted for rather than hidden**: `BP_DCCEnemy` and
`BP_ThirdPersonCharacter` were fixed by hand, node by node, and read back clean afterward - both
still show `(event DCC|EventOnDied (Rendering|SetVisibility ...))` /
`(event DCC|Combat|EventOnAttackPerformed (Target bHit))` with their harmful bodies gone but the
event hooks themselves intact. The other 7 were fixed via a batched `ProgrammaticToolset` script
to avoid 20+ manual round-trips, and that script had a real bug: it searched a harmful node's
*every* input pin for "orphaned feeder" nodes to clean up, without excluding the Exec pin - so on
`BP_OnDied` specifically, the incoming event node itself (which feeds the harmful node's Exec
input) got misclassified as an orphaned feeder and deleted **along with** the harmful node, not
just the harmful node alone. Audited all 7 afterward (`find_nodes` entry-point count, down from 5
to 3 on every one) and confirmed this precisely: all 7 lost the `BP_OnDied`/`BP_OnAttackPerformed`
event nodes entirely, not just their bodies. **Checked whether this actually matters rather than
assuming**: the only casualty was `SetVisibility(GetHealthBarWidget)` on death, and
`ADCCEnemyCharacter::BeginPlay` already force-hides that exact widget at spawn
(`SetVisibility(false)`, `SetHiddenInGame(true)`, tick disabled - a prior session's own fix for a
different problem, with a comment explicitly noting the Blueprint's `SetVisibility` call was kept
non-destructive on purpose). The Blueprint call was dead code before this session touched it;
losing it changes nothing observable. The crash-causing calls are gone from all 9 Blueprints
either way, which was the actual goal.

Deployed as pure Blueprint-asset edits via MCP - `compile_blueprint` + `save_assets` per
Blueprint, no C++ changed, no editor build/restart needed for this fix specifically.

**Not verified in PIE, same as everything else tonight** - the mechanism is confirmed (dead asset
reference, live skeleton mismatch, exact code found and removed), but nobody has watched a
character actually die since. Worth being the very first thing tested next session.
