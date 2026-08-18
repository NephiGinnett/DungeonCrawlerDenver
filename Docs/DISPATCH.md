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

**Diagnosed in a PIE run (2026-08-18).** Both open gameplay bugs now have answers, and
neither is quite what it looked like from outside.

**`[DCC Cast]` — not missing rows. Two different bugs wearing one symptom.**

The raw log settles one thing immediately: every actor, *including both failures*,
prints `table=DT_DCCSkills`. The table is assigned. An earlier draft of this note
blamed missing or misnamed rows — that is wrong, because the working actors resolve
their rows off the same table.

Verified against the CDOs and the live instances over MCP, 2026-08-18:

| Actor | Blueprint | `StartingSkillIds` | Verdict |
|---|---|---|---|
| `Mob_Brute_Courtyard` | `BP_DCCEnemy_Brute` | **empty on CDO *and* placed instance** | Never authored |
| `TestBoss_InternManager` | `BP_DCCEnemy_ArmsOut` | `ParkingLotSpecial`, `QuickWit` @ rank 2 | Works, matches CDO exactly |
| `Scott_OfficePlaza_Demo` | — | `Critic`, `NumberCruncher` | Works |
| `Crawler_Kyle` | `BP_DCCEnemy_ArmsOut` | see below | **Still open** |

**The Brute needs no theory at all.** `StartingSkillIds` is empty on the class default
*and* on the placed actor, so `GrantStartingSkills` has nothing to grant and correctly
grants nothing. `POST_BUILD_TASKS.md` §3 lists an intended loadout (`Headbutt`,
`KickItOver`) that was planned and never applied. Fill it in and the Brute is done.

**Kyle is genuinely unresolved — do not record it as fixed.** `Crawler_Kyle` is an
`ArmsOut` actor, and the ArmsOut loadout demonstrably works, because
`TestBoss_InternManager` runs it. But in the PIE run that produced this log Kyle knew
nothing, while in a later run the one `ArmsOut` actor matching "Kyle" had `KnownSkills`
populated correctly at rank 2. Either there are two Kyle actors and only one was
inspected, or something changed between the two runs. Identify the exact actor that
logged `Crawler_Kyle` and read `StartingSkillIds` off *that* instance.

**Actor naming is its own problem, and it is blocking diagnosis.** The `[DCC Cast]` and
`[DCC Anim]` logs share **no actor names whatsoever** — `Scott_OfficePlaza_Demo` versus
`Scott_OfficeAirplane_Demo`, `Crawler_Kyle` versus `Kyle_ArmsOut_Demo`. Either the two
diagnostics print different name fields, or the level holds near-duplicate demo actors.
Worth settling before chasing either bug further, because right now every
cross-reference between the two logs is ambiguous.

**`[DCC Anim]` — three Blueprints were never repointed after the skeleton merge.**
Alert clips are built against `SK_DCCCrawler_Skeleton` and silently refuse with
"Wrong rig" on exactly three characters:

| Actor | Refused clip | Mesh in use | That mesh's skeleton |
|---|---|---|---|
| `Scott_OfficeAirplane_Demo` | `A_DCC_QA_Alert` | `SK_OfficeAirplane` | `SK_OfficeAirplane_Skeleton` |
| `Kyle_ArmsOut_Demo` | `A_DCC_OA_Alert` | `SK_ArmsOut` | `SK_ArmsOut_Skeleton` |
| `Marcel_DiceWinged_Demo` | `A_DCC_OA_Alert` | `SK_DiceWinged` | `SK_DiceWinged_Skeleton` |

**This needs no retargeting and no design decision — the work is already done.** The
2026-08-15 consolidation (CLAUDE.md, *"All four crawlers now share one skeleton"*)
imported `SK_OfficeAirplane_Shared`, `SK_ArmsOut_Shared` and `SK_DiceWinged_Shared`
already bound to `SK_DCCCrawler_Skeleton`, and noted the originals could be deleted
*"once the Blueprints are repointed"*. They never were. All three assets are still on
disk under `Content/DCC/Characters/`, and these three actors are still using the
pre-merge originals.

The corroborating detail is who is **absent** from the failure list: CJ. `SK_HockeyWings`
was already on the shared skeleton and needed no re-import, so his alert plays fine.
That is the same bug seen from the other side.

Fix is to point three Blueprints at the `_Shared` meshes. Verify the sockets came across
— sockets are mesh-level, not skeleton-level, so each `_Shared` mesh needed its own
`add_socket` pass and it is worth confirming that happened before deleting the originals.

**Waiting on MCP:** per-asset texture caps, reparent `BP_ThirdPersonGameMode` to
`ADCCGameMode`, import Voldemortgage from `SourceArt/Voldemortgage/`, four pickup
collisions, re-import `DT_DCCEquipment.csv`, build rooms 5-7, and the two fixes the
PIE run above diagnosed:

- **Repoint three Blueprints** to `SK_OfficeAirplane_Shared`, `SK_ArmsOut_Shared` and
  `SK_DiceWinged_Shared`. Cheapest of the outstanding items and it unblocks every
  shared animation on those characters, not just the alert.
- **Author `BP_DCCEnemy_Brute`'s `StartingSkillIds`** (`Headbutt`, `KickItOver` per
  `POST_BUILD_TASKS.md` §3). Verified empty; this is the whole Brute fix.
- **Identify the actor logging as `Crawler_Kyle`** and read `StartingSkillIds` off that
  instance. Not diagnosed yet — see the naming caveat above.

---

## Working style

Build first, explain at checkpoints. Zoe is learning game dev — explain the *why*
behind UE idioms when you explain at all, but do not narrate every step.

Prefer asking her to do a quick manual step over spending an hour on an automatic
workaround. Her time at the keyboard is the scarce resource, not yours.

**Be honest about what is unverified.** This project has been damaged more by
confident wrong claims than by slow progress. If you did not see it work, say so.
