# Kits and sets: the groundwork for building places (2026-11-25)

Zoe's call: Claude does the world-making. This file is the groundwork for it: what each place pack holds, how big
its pieces are, which finished sample maps can be dropped into a game whole, and how.

## Two ways to build a place

**1. Load a set** (`Arcade/DCCSets.h`). Most place packs ship a finished, dressed sample map. An ordinary (non World
Partition) map can be streamed into any game at runtime with one call:

```cpp
DCCSets::FOptions Opt;                       // strips the set's own sun, sky, fog, player starts and demo pawns
DCCSets::Load(GetWorld(), TEXT("OldMine"), At, Rot, TEXT("DeadAirMine"), Opt,
    [this](const FBox& Bounds) { /* place marks, spawns, triggers inside Bounds */ });
```

- Call it on **every machine** (BeginPlay or the arena's local build), with the **same instance name** everywhere.
- The set's actors are not replicated. Keep gameplay actors (marks, enemies, pickups) in our code, never in the set.
- The arena still owns the sky and the sun. Keep `bKeepLocalLights` on: the set's lamps are its look.
- **Before shipping a game that loads a set**, add that map to the cook (`+MapsToCook` in `DefaultGame.ini`'s
  packaging section). A launched build does not include sample maps on its own.

**2. Build from pieces** (`ADCCArcadeArena::Prop(path, at, yaw, fit)`), using the catalogue below when a set is
the wrong shape. Each pack has its own building grid; mix packs only where the grids agree.

**Preview tools** (dev, Claude's): `DCC.Set <alias|/Game/path> [sky] [delay]` loads a set 600 m above the player
and tours it with the camera (inside views facing four ways, then outside, then above). `DCC.SetView <0-6>` and
`DCC.SetWalk` move the camera or drop the player in for scale. For screenshots:

```
DCC_TEST_WAITFOR="[DCC Sets] tour start" python Tools/run_test.py DCCBenchGameMode "DisableAllScreenMessages, DCC.Set OldMine 3" 2.5 6 6 set_oldmine
```

Use the bench (`DCCBenchGameMode`), not the Den: the Den is heavy enough to put the game clock a minute behind.

**The raw catalogue:** `SourceArt/Kits/<Pack>.json` lists every static mesh (size in cm, triangles, Nanite, a guessed
role: floor, wall, pillar, door, prop, big) and every map, with whether it is World Partition. Regenerate with
`Tools/kit_catalogue.py` (a commandlet, editor closed). No mesh in any of these packs is over 200k triangles.

## The sets

Aliases are in `DCCSets.cpp`. "WP" maps cannot be loaded as sets (World Partition); use their pieces instead.

Toured in game on 2026-11-25 (`Saved/TestShots/set_<Alias>/sheet.png`). Sizes are the measured bounds.

| Alias | Map | What it really is | Size (m) | Use it for |
|---|---|---|---|---|
| `Studio` | `BroadcastStudio/Maps/Showcase` | **A finished three-floor round TV studio**: lounge sets, cameras, rigs | 13 x 32 x 20 | **PRIMETIME's studio.** Best set in the batch |
| `Metro` | `MetroMaintenanceStation/Maps/Metro1` | **A finished subway**: platform, tunnels, signals, tracks | 38 x 93 | Gate Run's "under Denver" theme; a tower defense road (tunnels are natural lanes) |
| `CommandCenter` | `StarterBundle/.../SciFi_COMM_EX1` | **A finished round sci-fi control room** | 22 x 28 | Gate Run's sci-fi boss room; a Primetime "away" set |
| `SciFiInterior1` | `StarterBundle/.../SciFi_INT_EX1` | Clean white sci-fi rooms and halls | 90 x 90 (padded) | Gate Run sci-fi rooms |
| `HallwayA` | `StarterBundle/.../SciFi_Example_A_Ext` | An H of white sci-fi corridors, two big doors | about 40 x 30 | Gate Run sci-fi corridors |
| `EngiRoom` | `ModSci_Engineer/Maps/Example_Dynamic` | Orange-lit engineering bay, machines, pipes | 116 x 116 (padded) | Tower defense crystal chamber; Gate Run machine room |
| `SciInterior` | `ModSciInteriors/Maps/Example_Dynamic` | Industrial engine room, fans, red lights | 29 x 24 | Gate Run machine room |
| `PowerPlant` | `AbandonedPowerPlant/Maps/PowerPlant` | **A huge dark factory hall** with tall windows | 57 x 25 x 12 | **Tower defense map**: one long hall, the wall at one end. Needs the arena's sun through the windows (keep `bKeepSun` or add light) |
| `Elderboom` | `ElderboomVillage/Maps/L_ElderboomVillage` | A whole fantasy village: cottages, graveyard, walls, trees | 152 x 152 | A Gate Run village theme; a Ring "outdoor" stage |
| `Inferno` | `Inferno_World_Free/Maps/Demonstration` | **A lava crater** ringed by rock with an island in the middle | about 400 across, landscape | **The Champions' Ring champion stage** (fight on the island); Gate Run's boss floor |
| `ForestVillage` | `Forest_village/Level/L_showcase_level` | Mountains and a forest clearing with huts (landscape) | about 840 across | Too big to drop in whole. Its huts and trees as pieces |
| `SpaceStation` | `SciFi_Space_Station/Level/L_SciFi_Space_Station` | The **outside** of a space station (hull modules) | 105 x 44 | A backdrop seen through windows; not walkable |
| `OldMine` | `Scene_OldMine/Maps/Industrial_OldMine` | **A prop showroom**, not a mine: one long hall with every mine prop in a row | 120 x 42 | Pieces only: timbers, rails, carts, lamps for Dead Air |
| `Saloon` | `Scene_Saloon/Maps/Historic_Saloon` | **The same kind of showroom**, saloon props | 117 x 42 | Pieces only. Its real rooms are WP maps (`PL_His_Sal_*`) |
| `Clinic` | `Clinic/Maps/Overview` | A prop showroom (holo screens, beds). Its `Clinic` map is a grey blockout | 93 x 69 | Pieces only |
| `Gothic`, `Banners` | their demo scenes | Prop showrooms | | Grok's fill |

**In use:** `PowerPlant` is the tower defense map (2026-11-25, `ADCCArcadeSiege::BuildLocal`). Its annex end is a solid
facade; the way in is a 2 m doorway in each long side of the annex, with closed doors removed on load
(at first by removing them; since the evening, they work).

**Doors work in every set** (`Arcade/DCCSetDoor`, `FOptions::bWorkingDoors`, on by default): found by mesh name and
size, they open when any character comes near and shut behind them. Narrow leaves swing, wide ones slide.

Things learned on the tour:
- **Some walls ignore the Visibility channel** (the Power Plant's end facade and its glass doors), so a sight trace
  walks through what a character cannot. The probes (`DCC.SetProbe`, `DCC.SetProbeWalls`) trace the Pawn channel.
  Still check a route by watching something walk it.
- **Interiors are lit by their own lamps** and look right with `bKeepSun`/`bKeepSky` off. Big windowed halls (the
  Power Plant) need light from outside.
- Some textures stream in over the first seconds (checkerboards on the village). Load a set behind a fade or
  before players arrive.
- The first load of a set compiles its shaders (minutes, once per machine). One first load of the Saloon crashed
  in the engine's own cache threads; the retry was fine.

## The piece kits and their grids

| Pack | Floor grid | Wall pieces (L x T x H cm) | Notes |
|---|---|---|---|
| `AbandonedPowerPlant` | 200, 300 | 300x20x400, 200x20x400, 600x20x400 | 1 m grid, 4 m walls. 7 doors. Cleanest modular kit here |
| `DerelictCorridor` | 510, 480 | 510x20x380 | 5 m grid. All its maps are WP: pieces only |
| `StarterBundle` (modular sci-fi) | 320x360 | 320x30x400 | 3.2 m grid. 289 meshes, 31 doors: the biggest sci-fi kit |
| `ModSci_Engineer` | 330, 360x480 | 360x40x480 | 3.6 m grid, 4.8 m walls |
| `ModSciInteriors` | 610x860 | 610x50x390 | Big room shells |
| `SciFi_Space_Station` | 300x480, 400x450, 200x400 | 50x400x250 | Mixed sizes: use its set rather than its pieces |
| `MetroMaintenanceStation` | 500 | 400x0x280, 1000x100x300 | Tunnels and platforms |
| `Clinic` | 420, 330 | 1040x20x400, 1540x20x450 | Whole-room floor plates (15 x 18 m) |
| `Scene_OldMine` | 250x150 | 1000x100x500 | Timbered tunnel sections |
| `Scene_Saloon` | 1000x200 | 400x10x240 | Western interior |
| `ElderboomVillage` | 200 | 200x20x200, 400x20x200 | Small timber houses |
| `Forest_village` | 350, 540x450 | 350x20x320 | Huts, 7 "big" pieces (cliffs, trees) |
| `Inferno_World_Free` | - | - | Rocks and 14 huge landscape pieces; set only |
| `BroadcastStudio` | 1010 (stage) | 200x30x400 | Studio walls, desks, screens |
| Factory (merged into `/Game/Meshes`) | 200, 400 | 20x500x500 | Shares `/Game/Meshes` with the Game Animation Sample |
| `StageProps` | 540x20 trusses | - | Truss and lighting rigs |
| `SuperGrid` | 200 | - | Prototype grid material and shapes, for blockouts |
| `StylizedVolClouds` | - | - | No meshes: six sky maps (AfterStorm, Cloudy, Foggy, LittleCloudy, S, Storm) to copy settings from |

Props packs (`Gothic_props_1`, `ModernStreetProps_01`, `Barrels_Boxes`, `Torn_Banner_Free`) are Grok's fill
(Part F of `Docs/grok-plan-2026-11-25-fab-packs.md`).
