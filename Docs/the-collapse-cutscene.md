# The Collapse

Book-accurate DCC beat: before Carl ever sets foot in the dungeon, every interior
space in the world is smashed flat in a single instant. This doc covers the
version built into `ADCCIntroDirector` (2026-08-27) — the party-ready gate and
the collapse cutscene itself — and what is still queued for the next MCP
session.

## Where this sits

`ADCCIntroDirector` (`Source/DungeonCrawlerDenver/Intro/DCCIntroDirector.h/.cpp`)
already modeled almost this exact beat: wake up, get up, explore the room,
walk out the door, watch the world end, arrive at the Queue. This work extends
that existing state machine rather than building a second one next to it —
`Exploring → Street → Collapsing` was already there; what was missing was (a)
gating the door on the whole party rather than the first crawler through it,
and (b) an actual visible collapse instead of a first-person camera shake
held to black.

```
Waking → Rising → Exploring → [gate: wait for every crawler] → Street → Collapsing → (teleport to the Queue)
```

Everything after "the Queue" is unchanged — `TeleportAllCrawlersToDungeon`
still lands the party at `DungeonArrivalLocation`, and the System AI's first
line (`Good morning. Your building has been repossessed...`) still fires
immediately after, ahead of the already-existing `Orientation` quest
(`bAutoStart: true` in `Docs/DT_DCCQuests.json`). Nothing about the quest
chain needed to change; the collapse just fills the seam that was already
there between "explored the room" and "arrived at the dungeon."

## The party-ready gate

**Before:** `HandleDoorOverlap` fired on the *first* crawler through
`DoorTrigger` and immediately multicast the whole party into `Street`. In a
five-player session that meant whoever wandered out first dragged everyone
else along mid-exploration.

**Now:** each crawler who reaches the door is added to a replicated
`TArray<ADCCCharacterBase*> ReadyForCollapse` and, if the party isn't
complete yet, simply... keeps playing. There is no cinematic lock, no freeze,
no teleport-back for an early arrival. Reasoning: the Collapsing phase already
forces every client's camera onto `CollapseCamera` regardless of where their
own pawn is standing, so a waiting player's physical position doesn't matter
for the shared cutscene — nothing is gained by trapping them at the door, and
`ApplyPhase`'s own existing comment on `Street` ("taking the camera away...
would read as a punishment for doing the thing they were asked to do") argues
directly against it.

`GetExpectedPartySize()` reads `GameState->PlayerArray.Num()` rather than a
hardcoded five — the same reasoning `ADCCGameMode::PostLogin`'s own
player-count line already uses. A two-player PIE test gates on two, not five.

`ADCCHUD::DrawWaitingForPartyBanner` reads `ReadyForCollapse` directly (no RPC
needed — the array is already replicated to every client) and shows
`"Waiting for other players... (n/m ready)"` centered near the top of the
screen for anyone who's already through the door.

## The cutscene itself — cheap procedural, not Chaos destruction

**This was a design decision Zoe made in advance, not one this session
revisited:** background buildings squash toward the ground on a timeline;
one hero building gets a small number of pre-separated chunks that tumble;
sound, camera shake and dust VFX carry the moment. No per-mesh Chaos
fracturing, anywhere.

### Background skyline — `SkylineBuildings`

Each entry is a placed static mesh actor (`FDCCCollapsingBuilding` struct,
`BuildingActor` + optional `ExtraDelaySeconds`). At collapse start its scale,
location and rotation are cached; every tick after its stagger delay elapses,
its Z-scale lerps from 1.0 to 0.04 with an ease-in curve — a squash toward the
ground, assuming the mesh's pivot sits at its base (true of every building-kit
mesh checked for this — see "Assets" below). No location compensation is
applied for a centered pivot; if a future mesh needs one, `AnimateBuilding` in
`DCCIntroDirector.cpp` is the place to add it.

**Stagger is a pure function of the building's own placement**, not a
replicated random value: `GetBuildingStaggerDelay` computes
`distance-from-the-director × CollapseStaggerSecondsPerMeter`, so the collapse
reads as spreading outward from the hero building rather than every mesh
dropping in unison, and every machine — including a hypothetical dedicated
server that never renders any of this — computes the identical schedule with
nothing to keep in sync.

### The hero building — `HeroBuildingChunks`

The building the party was just inside. Each entry is `bIsHeroChunk = true`,
and instead of squashing it tumbles: a fall offset along -Z
(`HeroChunkFallDistance`) and a rotation about a per-chunk deterministic axis
(`HeroChunkTumbleDegrees`), both driven by the same ease-in curve as the
skyline. The tumble axis comes from `FRandomStream` seeded by
`GetTypeHash(the chunk's own starting location)` — deterministic, not
`FMath::Rand()`, for the same cross-machine-consistency reason as the stagger.

**Why chunks instead of one mesh:** this project's MCP toolset has no
mesh-fracturing tool (confirmed live — `list_toolsets` shows nothing
resembling Chaos Fracture or a modeling-mode boolean/split tool). The
practical, already-available substitute is exactly what Zoe asked for: 4-8
separate static mesh actors — modular building-kit pieces stacked to form one
building in the first place — each independently animatable. No fracturing
step is needed because the "pieces" already exist as separate assets before
the building is ever assembled.

### Camera, shake, sound, dust

- **`CollapseCamera`** is a second `UCameraComponent` on the director,
  inactive until Collapsing starts, at which point `IntroCamera` deactivates
  and it takes over (`AActor::bFindCameraComponentWhenViewTarget`'s normal
  "first active camera component wins" behavior — no extra plumbing needed).
  This is the pulled-back establishing shot; `IntroCamera` stays exactly what
  it was, a first-person eye-height camera for Waking/Rising/Exploring/Street.
- **Shake** reuses the exact hand-rolled sine-wave shake `Collapsing` already
  had, just retargeted from `IntroCamera` to `CollapseCamera` and offset from
  a cached base transform instead of a hardcoded `StandingCameraOffset`.
- **Sound** — `CollapseCrashSounds` (an array, played 2D and staggered by
  `CrashSoundStaggerSeconds` for the "layered per building" texture the brief
  asked for). **Left unassigned by default: no crash/impact sound effect
  exists anywhere in this project.** This is a real content gap, not a bug —
  same category as the missing cast animation and missing spell VFX already
  flagged elsewhere in `CLAUDE.md`. The hook is fully wired and silent-safe
  (`LoadSynchronous()` on an unset `TSoftObjectPtr` just returns null); it
  needs an actual `.wav`/`SoundCue` dropped in and assigned once one exists.
- **Dust** — `DustEffect`, defaulted via constructor to
  `/Game/Dam_city/FX/NS_Smoke` (the same pack as the default skyline meshes,
  so the look is already coherent without any wiring). Spawns once per
  building the instant its stagger delay elapses.

## Assets

**`Dam_city`** is the recommended pack for both the skyline and the hero
building — plain rectangular building meshes (`SM_building_01..06`,
`SM_square_building_module_02..07`, `SM_building_thin`), the least
sci-fi-flavored of the four city kits already installed
(`Sci_Fi_city`/`Synthwave_city`/`Megastructure_Scifi_World` all lean overtly
sci-fi/neon, which fights a contemporary-Denver-apartment collapse). It also
ships its own smoke FX (`NS_Smoke`/`NS_Smoke1`/`NS_Smoke2`), already wired as
the default dust effect above. This continues the plan already recorded in
`Docs/DISPATCH.md` (`Distant-skyline packs already installed and unused`) and
`Docs/asset-needs-meshy-pipeline.md` §3 — pulling one pack's building
dressing in wholesale rather than authoring or generating unique buildings.

Composing the hero building from `SM_square_building_module_*` pieces (they
are already separate, stackable modular meshes) gets the "4-8 independently
animatable chunks" for free, per the reasoning above.

## Deliberate deviation: no Sequencer

The brief asked for this to be built in Sequencer. **It is not, on purpose,
matching a constraint this exact class's own header comment already
documented before this session touched it:** there is no Sequencer-editing
toolset among the ~28 `*Toolset` plugins this project's MCP setup exposes
(`SequencerAnimMixerToolset` exists and is a different, narrower thing —
retargeting/mixing existing anim clips, not authoring shots or tracks). A
`LevelSequence` asset built by hand in the editor could not be read, edited,
or fixed from a Claude session afterward, the same reasoning that made the
original bedroom intro a code-driven director instead of a `.uasset` sequence.
This is not a new judgment call — it is following the precedent already set
in this file's own class comment — but it's flagged here explicitly since
Zoe's brief asked for Sequencer by name. If a Sequencer toolset ever gets
added, this whole system ports reasonably cleanly: the timing/curves here are
already isolated in `BeginCollapseSequence`/`TickCollapsingBuildings` rather
than scattered through gameplay code.

## What exists now vs. what's still queued

**Built and compiled 2026-08-27** (Development Editor target, editor closed,
real `Link [x64] UnrealEditor-DungeonCrawlerDenver.dll`, no numbered
hot-reload artifacts):

- The party-ready gate (`ReadyForCollapse`, `GetExpectedPartySize`,
  `IsReadyForCollapse`) and the HUD's waiting banner.
- `CollapseCamera`, `SkylineBuildings`, `HeroBuildingChunks` and all the
  timing/FX properties described above, with sensible defaults.
- The full Tick-driven animation (squash, tumble, stagger, dust, shake,
  staggered sound playback).

**Not done — needs the editor and MCP, next session:**

1. **No `ADCCIntroDirector` instance is placed in the level at all yet** —
   confirmed live via `find_actors` before this session's changes. Placing
   one, and wiring `DoorTrigger`'s position to wherever the party's shared
   exit actually ends up, is unstarted.
2. **The actual skyline and hero-building actors are not placed.** This doc
   describes the system they plug into; no `Dam_city` meshes have been added
   to the level yet, and `SkylineBuildings`/`HeroBuildingChunks` are empty
   arrays until someone (Zoe or a future MCP session) places them and wires
   the references via `set_properties`.
3. **`CollapseCamera`'s default transform is a placeholder** (pulled back and
   slightly elevated, aimed roughly forward) — repositioning it once the
   skyline actually exists is expected, same as every other placed-then-nudged
   actor in this project.
4. **No crash sound exists in the project** — see "Camera, shake, sound,
   dust" above. `CollapseCrashSounds` needs a real asset before the collapse
   has any impact sound at all; shake and dust currently carry the whole
   moment on their own.
5. **A genuine layout question, not a technique question:** the level
   currently has an undocumented, in-progress `PersonalRooms/` folder — five
   small walled rooms (`PersonalRoom_CJ/Kyle/Marcel/Scott/Vaughn`, each with a
   doorway gap in its west wall and a placeholder body standing in it) that
   don't appear in this file's own "Current state" section or in any other
   doc. This looks like exactly the "explore their room, then a small demo
   space" setup the brief describes, mid-construction, and is almost
   certainly Zoe's own recent manual level work (per this file's existing
   note on her editing habit). **Nothing about where the shared exit door, the
   demo space, or "outside" physically go has been decided by this session** —
   that's a real layout call for Zoe, not a technique fork, so it wasn't
   guessed at. The gate and cutscene system above works with a single
   `DoorTrigger` wherever it ends up; it does not need to know about the five
   personal rooms specifically.

## Session notes

- A live PIE session was running in the editor at the start of this work
  (confirmed via `IsPIERunning` + actor paths under `UEDPIE_0_Lvl_ThirdPerson`)
  and was stopped via `StopPIE` before any level inspection, since edits made
  during PIE only touch the transient play-world copy and vanish the moment
  PIE ends. Nothing besides stopping playback was done to that session.
- The editor's MCP connection dropped partway through this session (most
  likely Zoe closing the editor). That happened to be exactly the
  precondition the Development Editor build needs, so the build was run
  immediately rather than left waiting — see the git log / this session's
  DISPATCH.md entry for the build result.
