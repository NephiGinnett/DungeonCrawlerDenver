# DungeonCrawlerDenver

Dungeon Crawler Carl × BG3 — 5-player synchronous networked dungeon crawler, UE 5.8.
Design doc: `C:\Users\kmett\Downloads\dcc-bg3-design-doc.md` (read it before planning work).

Zoe (`Nephi` on this machine) is the developer and the DM. Players: Scott, CJ, Marcel, Vaughn, Kyle.

## Environment

- **Engine:** `D:\UE\UE_5.8` (Epic launcher binary install). This is NOT the project — don't run Claude Code from there.
- **Project:** this directory. Run Claude Code from here so `.mcp.json` is picked up.
- **Toolchain:** VS 2022 Community, MSVC 14.44.35207, Windows SDK 10.0.26100. The "Game development with C++" workload is NOT installed, but the compiler and SDK are, which is all UnrealBuildTool needs.
- **Engine association:** the `.uproject` uses `"EngineAssociation": "5.8"`. It previously held a
  GUID, which is the form used for *source-built* engines resolved via
  `HKCU\Software\Epic Games\Unreal Engine\Builds` — that key is empty on this machine, so the GUID
  resolved to nothing and double-clicking the `.uproject` raised a **"Convert Project"** dialog.
  If that dialog ever reappears: **Cancel**. Never "Open a copy" — it forks the project into a new
  folder and strands the built binaries and source. Nothing needs converting; the engine is right.

### When Zoe has to close the editor (and when she doesn't)

Most work needs **no** restart. Only ask when the change is one of the bottom two rows.

| Change | Editor restart? |
|---|---|
| Level edits, actor placement, properties, Blueprints, UMG | No — all live over MCP |
| C++ edit to an existing function *body* | No — Live Coding (`Ctrl+Alt+F11`, or the LiveCodingToolset MCP tool) |
| New `UCLASS`/`UFUNCTION`/`UPROPERTY`, changed signatures | **Yes** — reflection data can't hot-patch |
| Enabling a plugin / new toolset | **Yes** — toolsets only register at startup |

So batch reflection-level C++ changes into one pass rather than trickling them out; each one costs a
close/build/reopen cycle. **Always `save_assets` before asking her to close** — unsaved level work is lost.

> **Turning Live Coding OFF does not make building-with-the-editor-open safe.** Confirmed
> 2026-08-15. With Live Coding disabled, UBT no longer refuses — it compiles to a numbered
> `UnrealEditor-DungeonCrawlerDenver-0001.dll` and the editor picks it up via **legacy Hot
> Reload** (`LogHotReload: Starting Hot-Reload from IDE`), reporting success in under a second.
> That is the mechanism Live Coding was built to *replace*, and it is worse at exactly the same
> thing: new `UFUNCTION`/`UPROPERTY`/`UCLASS` reflection data. So the trap did not go away, it
> only changed hats, and `Result: Succeeded` still is not evidence the editor is running your
> code. **Check `Binaries/Win64/` for numbered DLLs after any build — if one appeared, the build
> hot-reloaded rather than relinked, and a clean build with the editor closed is still owed.**

> **Do not use Live Coding to dodge that last rule — it lies.** Running `CompileLiveCoding` after
> adding a `UFUNCTION` returned `Result: Success` with only a mild warning about packaging, then
> crashed the editor 12 seconds later on PIE start with
> `Cast of Object /Script/CoreUObject.Default__Object to Actor failed`. It patches the binary but
> leaves reflection data inconsistent. **A "Success" from Live Coding is not evidence the change was
> safe.** If the change touches reflection, close the editor and do a full build — no exceptions.
> Recovery: delete `Binaries/Win64/*patch_*` and rebuild normally; on-disk assets are unaffected.

**Level edits survive a crash only if saved, and the `.umap` timestamp is not the signal.** This
project uses World Partition, so actors are stored one-file-per-actor under `Content/__ExternalActors__/`.
Check for recently-modified files there, not on `Lvl_ThirdPerson.umap`.

**Building requires the editor to be fully closed** — UnrealBuildTool refuses with "Unable to build
while Live Coding is active". Ask Zoe to close it rather than trying to work around it:

```
"D:/UE/UE_5.8/Engine/Build/BatchFiles/Build.bat" DungeonCrawlerDenverEditor Win64 Development \
  -Project="C:/Users/kmett/Documents/Unreal Projects/DungeonCrawlerDenver/DungeonCrawlerDenver.uproject" -WaitMutex
```

## UE5 MCP

The editor hosts the MCP server via the built-in `ModelContextProtocol` plugin at
`http://127.0.0.1:8000/mcp`, consumed through `.mcp.json` in this directory.

- `bAutoStartServer=True` is saved in `Saved/Config/WindowsEditor/EditorPerProjectUserSettings.ini`,
  so it starts with the editor. **It only starts at editor startup** — toggling the setting on a
  running editor does nothing until restart.
- If tools are missing: check the editor is running, then check port 8000 is listening.
- Console fallback inside the editor: `ModelContextProtocol.StartServer`.

**The server is only the transport — the tools come from separate plugins.** `ModelContextProtocol`
being enabled and port 8000 answering proves nothing about what Claude can actually do. The tools
live in ~28 `*Toolset` plugins under `D:\UE\UE_5.8\Engine\Plugins\Experimental\Toolsets\`, all
disabled by default. Without `EditorToolset` there is no way to spawn an actor or touch a Blueprint,
and `list_toolsets` will return only `AgentSkillToolset` (which just reads/writes skill assets).

Enabled in the `.uproject`: `EditorToolset`, `GameplayTagsToolset`, `AIModuleToolset`, `UMGToolSet`,
`ConfigSettingsToolset`, `LiveCodingToolset`. All ship precompiled, so enabling them costs no build
time — but they only register at editor startup, so adding one means a restart.

Deliberately *not* using the `AllToolsets` aggregator: it pulls in 21 experimental plugins
(Niagara, PCG, Chaos Cloth) whose tools we don't need and whose load cost we do pay.

To confirm the setup is genuinely working, check the editor log for one line per toolset:
`LogToolsetRegistry: Display: Registering Toolset ...`

## Architecture decisions

**C++ hybrid.** Core systems in C++; Blueprints for animation, UI, and level scripting.
Chosen because the dedicated Linux server binary effectively requires it, replication is
far more reliable in C++, and C++ gives readable diffs (Blueprints are binary assets).

**Three build targets** already exist, including `DungeonCrawlerDenverServer.Target.cs`.
The server target exists from day one specifically so the project never quietly grows
client-only assumptions that are painful to unpick at deploy time.

**Server-authoritative everything mechanical.** Damage, healing, and dice rolls are refused
on clients and reach them by replication. Per the design doc, the AI/LLM layer narrates
outcomes but never decides them.

### `Source/DungeonCrawlerDenver/Stats/` — keep this ruleset-agnostic

The design doc requires the real DCC TTRPG ruleset to be swappable in later without
restructuring character data. So this folder must stay free of DCC-specific concepts
(sponsors, mutations, audience approval). Those live elsewhere and talk to this layer.

- `DCCAbilityTypes.h` — `EDCCAbility`, `FDCCAbilityScoreSet`, `FDCCCheckResult`
- `DCCRuleset.h/.cpp` — **the swap-in point.** All rules arithmetic routes through this
  UObject. The real ruleset becomes a subclass; callers don't change.
- `DCCStatsComponent.h/.cpp` — replicated per-character stats + HP. Goes on players and
  enemies alike, so the DM client can inspect both the same way.

Gotcha worth preserving: ability modifiers use `FMath::FloorToInt32((Score - 10) / 2.0f)`,
not integer division — C++ truncates toward zero, which gives the wrong answer for scores
below 10 (7 would give -1 instead of -2).

#### What the real DCC ruleset actually says (from the preview PDFs)

Source: `C:\Users\kmett\Downloads\{Rules for Survival, Crawlers Preview, Game Masters Toolkit}.pdf`.
These are **Sneak Preview excerpts** (7/11/5 real pages of selected spreads), not the full books —
the harness reports inflated page counts. Extract text with `pypdf` and grep; do not page through them.

Confirmed rules, and where our placeholder diverges:

- **Five core stats, not six: STR, INT, CON, DEX, CHA. There is no Wisdom.** **Swapped in
  2026-08-14** — `EDCCAbility` now has five, ordered Str/Int/Con/Dex/Cha to match the books and the
  party's own sheet.
- **The score scale is 2–6, not 3–18.** A typical human crawler starts with scores between 3 and 5.
  The **Standard Array** assigns exactly 2, 3, 4, 5, 6 — one per stat, totalling 20. The party's
  handwritten sheet uses a 20-point spread, confirming this. Scores climb into the hundreds later.
- **The modifier is a banded lookup, never negative** (Rules for Survival p.11, Table 1):

  | Score | Mod | Score | Mod |
  |---|---|---|---|
  | 1–2 | +1 | 20–49 | +5 |
  | 3–5 | +2 | 50–99 | +6 |
  | 6–9 | +3 | 100–149 | +7 |
  | 10–19 | +4 | 300+ | +10 |

  A dumped stat does not penalise you here; it just fails to help. The old
  `floor((Score − 10) / 2)` was d20-fantasy math and was **wrong** — it made the party's STR 6
  bruiser a −2. Fixed.
- **Scores and modifiers are not interchangeable, and the books signal which they mean by name.**
  The full word ("Strength") means the **score**, used for derived values — the rules' own example is
  *"a crawler's Strength score is used to determine what they can lift."* The abbreviation ("Str")
  means the **modifier**, added to d20 rolls. Carry limit, throw distance and move speed therefore
  read the **score**; checks and attack rolls read the **modifier**.
- **Skill Check = `1d20 + Skill Rank + Stat Mod`** vs Difficulty.
- Stats have two layers, **Unenhanced** (base + potions + level-ups) and **Enhanced** (plus gear,
  spells, buffs). Not yet modelled — we have a single layer.
- **Mana = Intelligence score at a 1:1 ratio** (the score itself, not the modifier; spells cost
  double-digit Mana). `GetMaxManaPoints` already implements this exactly.
- **Constitution determines Health.** Formula is on book p.41, which is *not* in the preview — our
  `10 + CONmod × Level` is still an invention.
- **Dexterity drives "Evade"**, which is DCC's name for the defence value an attack must beat.
- **"AI Favor"** is the real name for the design doc's audience-approval/sponsor meter. Build the
  sponsor system against that term.

##### Skill Ranks and advancement (Crawlers Preview p.42–44; Rules for Survival p.12)

Confirmed from the previews on 2026-08-14. Skills level **through use**, Runescape-style:

- **Skill Rank 0–20**, capped at **15** on Floors 1–5. The core check is
  `d20 + Stat Mod + Skill Rank` vs Difficulty — our `ResolveCheck` is missing the Rank term.
- **Using a skill marks it.** Attempting a Skill Check marks the skill whether it **succeeds or
  fails**. It is a *binary mark*, not a usage counter — twenty uses mark it once.
- **Skill Advancement Check:** roll `1d20` against **the skill's current Rank as the Difficulty**;
  meet or beat it to gain +1 Rank, max 15. Self-limiting by construction: trivial at low Rank,
  rare at high Rank. No XP table is needed or wanted.
- **Intervals:** Rank ≤4 → every 2 hours of play. Rank ≥5 → end of each floor. Then erase all marks.
- **Untrained use:** Attack and Utility skills may be attempted untrained at **Disadvantage**
  (2d20 take lower), Stat Mod but no Rank. On Tutorial Floors this is how new skills are *earned*
  (1 Rank in one skill tried untrained per 2-hour session; they cannot be banked).
  **Spell Skills cannot be attempted untrained.** Passive Skills never get marks.
- **No experience points anywhere.** Levels come from playtime (1 per 2 hours), quests, and boss
  tier (Neighborhood 1 → Floor Boss 6). Killing another crawler gives 1d6 modified by level gap.

Implication for `Skills/`: Rank is **per-character state, not library data**. `KnownSkills` holding
whole `FDCCSkillDef` copies is the wrong shape for it — the intended end state is
`{ SkillId, Rank, bMarked }` per character with definitions resolved from `SkillTable`.

**Swapped in on 2026-08-14**, once the party's own character sheet confirmed the five-stat, 20-point
layout and the previews yielded the modifier table. Only **Health remains an invention** — the real
formula is on Crawlers & Customization p.41, which is not in the excerpts. `GetMaxHitPoints` is
flagged in-code as the last placeholder. Evade's exact formula is likewise absent, so `10 + Dex`
remains ours, though it now uses the correct banded modifier underneath.

### `Source/DungeonCrawlerDenver/Rooms/` and `Characters/`

- `DCCRoomTypes.h` — `EDCCRoomType` (the design doc's five templates), `EDCCEncounterState`.
- `DCCRoom.h/.cpp` — one floor node: bounds volume, type, server-side occupancy list, optional
  encounter armed on player entry. **Occupancy is exposed on purpose** — it's the hook both the
  instanced-pocket merge and safe-room session gating will read. Neither is built yet.
  `AllowsAsyncLogin()` is derived from room type, not authored, so a floor layout can't get it wrong.
- `DCCEncounter.h/.cpp` — an enemy set spawned on demand, cleared when none are left standing.
  Enemies exist only on the server and arrive on clients by replication.
- `DCCCharacterBase.h/.cpp` — shared by players and enemies so encounters don't care which side
  they're looking at. `AttackCooldownSeconds` is the design doc's per-action initiative value;
  the pause/queue loop will consume it rather than inventing its own.
- `DCCEnemyCharacter.h/.cpp` — intentionally dumb AI (nearest player → close → swing on cooldown),
  no behaviour tree, because a tree needs asset authoring before the first fight is playable at all.
  Replace with StateTree when encounters need real tactics.

Build gotcha, hit once already: `Cond ? TSubclassOf<T> : T::StaticClass()` won't compile under
MSVC — the ternary is ambiguous between `TSubclassOf<T>` and `UClass*`. Spell both branches
as `TSubclassOf`.

Second gotcha: a `UFUNCTION` parameter may not be named `Instigator`. `AActor` already declares
that property and UHT rejects the shadow. Use `Character`/`Actor` instead.

### `Source/DungeonCrawlerDenver/Skills/`

The design doc's Skill Archetype System: player-invented skills are **data, not code**, so Zoe
can add one mid-session without a recompile — which is the whole point, since the tabletop game
invents skills at the table.

- `DCCSkillTypes.h` — `FDCCSkillDef` derives from `FTableRowBase`, so skills are DataTable rows.
  7 archetypes; parameters are mana cost, cooldown, range, cone angle, power die, DC, duration.
- `DCCSkillComponent.h/.cpp` — server-authoritative activation. Clients may *ask*
  (`TryActivateSkill`); cost, cooldown, targeting and the roll all resolve on the server.
  Mana is spent **before** the roll — a failed spell still burns it, which is what makes a bad
  roll hurt. Damage/Heal resolve in C++; the fuzzier archetypes fire `BP_OnSkillEffect` so the
  DM authors them in Blueprint. That split is deliberate: deterministic mechanics in C++,
  "anything is possible" in Zoe's hands.

### `Source/DungeonCrawlerDenver/Favor/`

The sponsor/audience meter, named **AI Favor** to match the published ruleset.

- `DCCFavorTypes.h` — `EDCCFavorEvent` (why favor moved), `EDCCFavorTier`, `FDCCFavorChange` ledger.
- `DCCFavorComponent.h/.cpp` — replicated meter with authored tier thresholds and a capped
  ledger. Every change records a reason and a note, so the AI narrator can say *why* the crowd
  turned rather than inventing it. v1 is DM-driven (`AwardFavor` with `DirectorCall`); automatic
  detection of "style" is deliberately not attempted yet.

### `Source/DungeonCrawlerDenver/Combat/`

- `DCCGameState.h/.cpp` — the real-time-with-pause loop. Any player may pause; while paused each
  character queues one action (re-queueing replaces it); on resume actions resolve in
  **initiative order** using the action's own cooldown, which is what stops unpausing from
  becoming a fastest-clicker contest. Lives on GameState because that is match-wide state every
  client must agree on.
  - `P` toggles pause. Left-click means "attack" live and "plan an attack" while paused.
  - Enemy AI early-outs while paused, so the dungeon stops thinking during the planning phase.
  - **Requires the GameMode's `GameStateClass` to be set to `ADCCGameState`** — without that
    wiring the pause silently does nothing.

## Importing character models (Meshy AI / glTF)

Meshy exports a `.zip` with two `.glb` files: the character, and a *merged animations* file.
Getting them in has two gotchas that cost a while to find:

1. **The MCP `SkeletalMeshTools.import_file` cannot read `.glb`.** It routes through `FbxFactory`,
   which accepts only `fbx` and `obj` — even though the editor's own drag-and-drop glTF importer
   would handle them. Convert first with Blender (installed at `D:\Blender\blender.exe`):
   `blender --background --python <script> -- <src.glb> <dst.fbx> <bake_anim 0|1>`.
2. **One animation per FBX.** Unreal imports a single take per file, so a merged-animations FBX
   yields exactly one AnimSequence no matter how many actions it contains. Export each Blender
   action to its own FBX.
3. **An armature-only FBX imports as nothing at all** — no error, just "produced no assets". The
   skeletal-mesh import path needs geometry present, so every per-animation FBX must include the
   mesh. Each import therefore also spits out a throwaway SkeletalMesh; delete it afterwards.
   Naming: importing as `TMP_X` produces `TMP_X` (junk mesh) and `TMP_X_Anim` (the animation).

Conversion scripts live in `Tools/` (`glb_to_fbx.py`, `split_glb_anims.py`); extracted
source art lives in `SourceArt/`.

**Two import warnings are expected and harmless:**
- *"Found texture name clash, `texture_0_001` was renamed…"* — every Meshy character ships a
  texture called `texture_0`, so the second and later imports get suffixed. Cosmetic; each
  character's material still points at its own texture. Verify by capturing the material
  thumbnail rather than the mesh thumbnail, which caches before textures finish importing.
- *"No smoothing group information was found"* — the per-animation FBX files carry a throwaway
  mesh whose normals are never used. `mesh_smooth_type='FACE'` is now set in both scripts to
  silence it, because the noise obscures real import errors.

**Grey character thumbnails are usually a stale cache, not missing textures.** The mesh thumbnail
renders before the material and textures finish importing. Check `CaptureAssetImage` on the
*material* — if the sphere is textured, the character is fine in-world.

**Verifying an import without opening the editor UI:** `EditorAppToolset.CaptureAssetImage` renders
an asset thumbnail and `CaptureViewport` grabs the level view. Both return base64 PNG that blows the
tool-output token limit, so decode the saved tool-result file to a `.png` and read that instead.
`CaptureViewport` requires `captureTransform` and `annotations` to be passed explicitly.

### Imported so far

`/Game/DCC/Characters/HockeyWings/` — CJ's crawler model (`SK_HockeyWings`), a bearded man in a
flaming hockey jersey. 170cm, correct scale against the mannequin, textures intact. Animations in
`Anims/`: `A_HW_Walking`, `A_HW_Running`, `A_HW_AgreeGesture`, `A_HW_BoomDance`.
**There is no attack or death animation in the set** — `BP_DCCEnemy_HockeyWings` currently runs a
single looping clip via `AnimationSingleNode` rather than an AnimBlueprint, so it has no
idle/walk blending and no attack montage. A proper ABP for this skeleton is the next step.

## MCP level-editing gotchas

- **`ActorTools.set_actor_transform` resets any field you omit.** Passing only `location` silently
  sets rotation to zero and **scale to 1**. A 20×20 scaled floor cube becomes a 1m cube and the room
  vanishes. The "unset means don't change" note in its docs applies to blueprint defaults, not to
  placed actors. **Always pass location, rotation and scale together.**
- **`SceneTools.save_actor` fails on World Partition actors** ("Asset does not exist" for the
  `__ExternalActors__` path). Save the level instead: `save_assets(["/Game/ThirdPerson/Lvl_ThirdPerson"])`.
- **`CaptureViewport` requires `captureTransform` and `annotations` explicitly** — both error if
  omitted rather than defaulting. Its PNG always blows the tool-output limit; decode the spilled
  tool-result file with `Tools/decode_tool_image.py`.
- **`snap_to_ground` snaps to whatever is under the point**, including the Cave_Camp rocks that
  surround Room 01 — it put a test actor at z=551 rather than on the floor. Prefer explicit Z.
- **Check for existing geometry before placing a room.** `find_actors` with a `bounds` box reports
  what already overlaps. Room 02 was first built at x≈7800,y=0 and landed inside cave rock; it now
  lives at **y≈5000**, which `find_actors` confirmed was empty apart from the sky sphere.
- **Ice_Station assets are station-scale, not room-scale.** `SM_large_floor_module` is 55×58 **metres**;
  `SM_pipe_group_01` is 20m wide. Measure with `StaticMeshTools.get_bounds` before placing. The
  crates (≈6m) and computers (≈3m) are the human-scale ones.

## Content / tone

`Docs/dungeon-tone-bible.md` — the System AI's voice, Denver-flavoured floor themes, room
clichés with twists, sponsor and AI Favor beats, loot/achievement naming. All original material
written in the books' spirit; deliberately no reuse of the books' actual floors or characters.
Read it before writing any player-facing text so announcements, loot and rooms sound like one
showrunner.

## Current state / next up

**Last updated 2026-08-15.** All C++ compiles and is cleanly linked — verified with the editor
closed, single DLL, no numbered hot-reload artifacts.

### What exists and works

The vertical slice is playable and has been played, including a **two-player PIE session**.
Room 01 (`DCC/Room01`, x 5200–6800) and Room 02 (`DCC/Room02`, y≈5000) both exist, with a
`DCCRoom`+`DCCEncounter`, navmesh, enemies that path and swing, ragdoll deaths, and a working
clear condition.

Systems built and compiled, roughly in the order they were added:

| System | Where | State |
|---|---|---|
| Stats, ruleset, five-stat DCC math | `Stats/` | Working |
| Rooms, encounters, **staged waves** | `Rooms/` | Compiled, waves untested |
| Skills, ranks, mark-and-advance | `Skills/` | Working |
| AI Favor | `Favor/` | Working, DM-driven only |
| Pickups, carry, throw, inventory | `Items/` | Working |
| Pause / queue / initiative | `Combat/DCCGameState` | Wired, lightly tested |
| **Boss phases** (charge, lien) | `Combat/DCCBossComponent` | Compiled, never run |
| Climbing + sprint on shared stamina | `Characters/` | Testable — see below |
| Nameplates, fist-bump friendship | `Characters/` | Working |
| XP, levels, gold | `Stats/` | Working |
| Achievements | `Progression/` | Compiled, **needs its table assigned** |
| Quests, markers, HUD tracker | `Quests/` | Compiled, **needs its table assigned** |
| Talking NPCs | `Dialogue/` | Compiled, never placed |
| AI narrator (local LLM) | `Narration/` | Compiled, **off by default** |
| Segmented health, mana regen | `Stats/`, `UI/DCCHUD` | Compiled, never seen |

### The immediate next step

**Nothing new should be written until the three DataTables are imported and assigned.** Quests,
achievements and dialogue are all inert without them and fail *quiet* — no table means no rows
means nothing ever fires, with no error. `Docs/POST_BUILD_TASKS.md` §8 is the checklist.

After that, in order:

1. **PIE and actually look at it.** Segmented health, the quest tracker, mana regen and the
   nameplates have all been compiled but never once seen on screen.
2. Place the two quest markers (`Orientation`, `EastLedge`) and one bartender NPC.
3. Clear `HUDWidgetClass` on the player Blueprint — the old UMG HP bar draws over the new
   Canvas one.
4. Author the Voldemortgage encounter from `Docs/floor-01-design.md` and `DT_DCCEnemies.json`.

### Design documents — read these before adding content

- `Docs/floor-01-design.md` — **Floor 1 is the Cliff Diver Cantina under new management.**
  Seven rooms with world coordinates, the Voldemortgage fight, what does not exist yet.
- `Docs/dungeon-tone-bible.md` — the System AI's voice. Read before writing player-facing text.
- `Docs/enemy-roster.md`, `Docs/DT_DCCEnemies.json` — stat blocks and boss phase structure.
- `Docs/ai-narrator-integration.md` — how the local LLM connects and what it may never do.
- `Docs/multiplayer-plan.md` — Enhanced Input migration, session model, predicted failures.
- `Docs/POST_BUILD_TASKS.md` — the live wiring checklist.

### Known-good test loop for climbing

Tab → `1` arms Climbing → look at Room 01's east wall → click → `Y` → hold LMB to rise, RMB to
descend, Space to let go. The player has `Climbing`, `Vault`, `MartialArts` at Rank 1 and there
is a climb point plus a landing ledge already placed. **There is no climb animation for the
player skeleton**, so it plays in an idle pose — that is cosmetic, not a bug.

### Gotcha that cost real time this session

**Blueprint defaults do not reach already-placed instances.** All four ally Blueprints had
`bIsAlly = true` on the CDO while the four actors in the level had `false` serialised on them,
so CJ, Scott, Kyle and Marcel spent a two-player playtest beating the player to death. Fixed and
saved. **When checking whether placed actors behave a certain way, read the instances, not the
class defaults** — and note that World Partition will not return unloaded actors from
`find_actors` at all. Moving the viewport camera (`EditorAppToolset.SetCameraTransform`) to the
region streams them in.

### Fab packs installed

**~30 packs, not the 6 originally listed.** Enumerate with `AssetTools.list_folders("/Game")`
rather than trusting any written list — Zoe adds packs between sessions.

Sci-fi options worth knowing: **`Ice_Station`** (the best interior kit — crates, computers,
generator, tanks, pipes, doors, walls, holograms), **`Modular_Sewer_Kit`** (a true modular dungeon
kit: tunnels, large rooms, arches, stairs), `Megastructure_Scifi_World` (grand hallways),
`Sci_Fi_city`, `Synthwave_city`, `Alien_Portal`, `Evil_planet`, `Abandonned_Brutalist`.

`Cave_Camp` **is already placed** in Room 01 — stalactites, tent, bedroll, table, campfire emitter,
crates and two point lights. Room 01 is committed to a cave look; sci-fi went into Room 02 rather
than re-theming over it.

VFX inventory is thin: the only fire is `Legendary_Forge/Fx/Fire/NS_fire`, plus `NS_sparks` there
and `Variant_Combat/VFX/NS_Damage`. Everything else is smoke, snow, leaves and waterfalls. **There
is no purpose-built spell VFX in any pack** — a spell-effects pack is the one asset purchase that
would actually pay for itself.

Note: Claude cannot browse Zoe's Fab *library* — that is account-side behind Epic auth. Only
packs already downloaded into `Content/` are visible. Ask her to download; don't attempt to fetch.

Not yet verified: the **server target has never been built.** Launcher installs don't ship
precompiled server binaries, so expect that to be its own piece of work. Per the design doc,
hosting is secondary and must not block playtesting. (Attempting it as a compile-check workaround
does **not** work — see below.)

### The Live Coding lock blocks *all* builds, not just the editor target

Confirmed 2026-08-14. With the editor open, UBT refuses immediately with *"Unable to build while
Live Coding is active"* and never reaches compilation. This applies to the **Server target too**,
so there is no way to compile-check C++ while the editor runs — not even against a different
binary. Batch C++ work and accept it is unverified until the next close.

### Build workflow, as it actually stands (2026-08-15)

There is **no uncompiled backlog** — everything in `Source/` is built and cleanly linked.

The workflow constraint is unchanged and has now been probed twice:

- Editor **open** → builds are unsafe. With Live Coding on, UBT refuses. With Live Coding off,
  it silently hot-reloads instead (see the warning near the top of this file). Neither is a way
  to verify C++.
- Editor **closed** → clean builds work, but MCP dies with it, so no editor automation.

The one idea not yet tested: with Live Coding **off** and the editor **open**, build the
`DebugGame` configuration. It links to `UnrealEditor-DungeonCrawlerDenver-Win64-DebugGame.dll`,
a filename the editor is not watching, so it should compile and link without triggering hot
reload. If that works it is a genuine compile-check with the editor running. **Worth ten minutes
next session** — it would remove the biggest friction in this project.

MCP itself needs reconnecting after every editor restart. That is a session id the server
forgets (`LogModelContextProtocol: Error: Unknown session id ...; client should reinitialize`),
not a credential — the plugin has no authentication code at all, so there is nothing to
configure and nothing to fix.

## Working style

Build first, teach at checkpoints — Zoe wants working results, then a walkthrough of what
was built at natural milestones, not step-by-step narration. She is actively learning game
dev; explain the *why* behind UE-specific idioms when you do explain.

Prefer asking her to do a quick manual step (restart the editor, flip a setting) over
spending minutes researching an automatic workaround.
