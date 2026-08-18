# Post-build tasks

Editor-side wiring that the compiled C++ is waiting on.

## ✅ 0. NOTHING IS OUTSTANDING (2026-08-16)

The Development Editor build landed clean (real `Link [x64] UnrealEditor-DungeonCrawlerDenver.dll`,
no numbered hot-reload artifacts), the editor was restarted, and the whole wiring pass ran through
MCP. Everything below this heading is history; **there is no queued work and no manual step left
for Zoe.**

Completed in the post-restart pass:

| Step | State |
|---|---|
| Four ally Blueprints reparented to `ADCCAlly_*` | ✅ done, verified by read-back |
| Every animation slot on those four (attack/cast/climb/fist-bump/idle/walk/run + 9 emotes) | ✅ inherited from C++, confirmed on the **placed** actors, not just the CDOs |
| Kyle's and Marcel's stale non-shared mesh overrides | ✅ repointed to `SK_ArmsOut_Shared` / `SK_DiceWinged_Shared` |
| CJ's curated skill loadout (`FireFingers`, `MediaAnalyst` @ Rank 2) | ✅ survived the reparent unchanged |
| `HUDWidgetClass` cleared on the player Blueprint | ✅ done via `set_properties` — **no longer a manual step**, see the corrected CLAUDE.md rule |
| Both quest markers configured (`ObjectiveTag`, `MarkerLabel`, beacon mesh + scale) | ✅ done in place; the `ADCCQuestMarker_*` C++ subclasses were not needed after all |
| `ADCCEnemyCharacter_TestBoss` placed in Room 02 (7600, 5000, 200) | ✅ hostile, animated |
| `Torch` row added to `DT_DCCEquipment` with its light columns | ✅ done |
| Nine new skills in `DT_DCCSkills` | ✅ done |

**The headline correction from this pass:** `ObjectTools.set_properties` was never broken. The
old calls were malformed — it wants a `values` parameter holding a **JSON string**. That single
mistake is what made quest markers, `HUDWidgetClass` and component assignment look permanently
manual for two sessions. Full detail in CLAUDE.md.

### What to actually do now: PIE

Nothing is gated. Play it and check:

- Allies **visibly swing** instead of sliding around silently — this was the real "allies won't
  fight" bug (`Multicast_PlayAttack` no-ops on a null `AttackAnimation`)
- CJ **gestures** when Fire Fingers goes off, and is rooted ~2s doing it
- Ally health bars read blue/green, not red, **on a second client** (the `bIsAlly` replication fix)
- Nameplates read forwards, not mirrored
- Both quest beacons are **visible in the world**, not just in the tracker
- Click an achievement row → its card opens beside the menu
- `L` cycles the log filter (All → Combat → Skills → Milestones) in its new bordered box
- Kill the Interim Manager in Room 02 → the floor exit flips from red to green ~0.5s later

---

## Historical record below

## -1. Latest batch, from live two-player playtest bug reports (2026-08-15, third pass)

Found by actually playing, not by re-reading code. All compiled via the DebugGame
check; **none deployed until the next Development Editor build + restart.**

| Fix | Where | Symptom it explains |
|---|---|---|
| `bIsAlly` was never replicated | `Characters/DCCEnemyCharacter.h/.cpp` | Allies fought correctly but showed the enemy red HP bar on remote clients |
| Nameplate/speech-bubble/quest-marker billboard math was backwards in 3 places | `DCCCharacterBase.cpp`, `DCCDialogueNPC.cpp`, `DCCQuestMarker.cpp` | Text rendered mirrored/backwards — same copy-pasted `(Self - Camera)` vs correct `(Camera - Self)` bug as the already-fixed `DCCFloatingText.cpp` |
| `SkillTable` had no C++ default (belt-and-braces only — see the correction below) | `Skills/DCCSkillComponent.cpp` | Defensive. Did **not** fix a live bug |
| **Every animation slot reads `None` on all four ally Blueprints** | `Characters/DCCCrawlerAnimSet.*` | The real "allies run at enemies but never fight" cause. `Multicast_PlayAttack` no-ops silently on a null `AttackAnimation`, so combat resolved mechanically with nothing on screen |
| `FireFingers.castTimeSeconds` bumped 0 → 2.0 (live table edit, no restart needed) | `/Game/DCC/Skills/DT_DCCSkills` | CJ had no cast lock because his one spell had no cast time authored |
| `ComplexAsSimple` collision on 4 Cave_Camp pickups replaced with real simple collision | content-only, `StaticMeshTools.generate_convex_collisions` | The physics-simulation warnings from the screenshot — **already live, no restart needed** |
| Event feed now has a background/border panel, plus an `L`-cycled filter (`All → Combat → Skills → Milestones`) using the existing `EDCCLogCategory` on each entry | `UI/DCCHUD.h/.cpp`, new `CycleLogFilter()` + `L` keybind on `Characters/DCCCharacterBase.*` | "outline around the text box" + "ability to filter through types of messages" |
| **Clickable achievements.** Canvas hit boxes per row + a detail card beside the menu (progress bar, what it counts, XP/Favor payout, secret flag, the System AI's line). Opening the menu now shows the cursor and switches to `GameAndUI`, and a click while the menu is open no longer also swings | `UI/DCCHUD.h/.cpp`, `Characters/DCCCharacterBase.cpp` | "make sure you can click on achievements to read info about them" |
| **Nine new skills**, covering all seven archetypes including `Debuff` and `Buff`, which had no rows at all | live `/Game/DCC/Skills/DT_DCCSkills` — **already saved, needs no build** | "build out more skills" |
| **Every animation slot filled** on the four allies *and* the three boss classes, which had none at all. Adds the three clips the consolidation unlocked but nobody ever wired: cast, climb, fist bump. Emote pool 6 → 9 | `Characters/DCCCrawlerAnimSet.*` (new), the four `ADCCAlly_*`, DebtEater/Voldemortgage/TestBoss | "build out more animations" |
| `Multicast_PlayCast` — the player's cast path never played an animation on any machine, unlike the NPC path | `Characters/DCCCharacterBase.*` | Casting looked like nothing happened |
| Voldemortgage now casts `HouseRules` + `ClosingTime`; Debt Eaters cast `LastCall`. These classes have no Blueprint, so a C++ loadout is genuinely authoritative | `Characters/DCCEnemyCharacter_Voldemortgage.cpp`, `_DebtEater.cpp` | Gives the boss fight a caster's rhythm |

### ⚠️ Correction to an earlier diagnosis in this file's history

An earlier pass here claimed **"`SkillTable` was never assigned, so nobody has ever
known a skill."** That was **wrong**, and it was checked properly afterwards by
reading the Blueprint CDOs over MCP:

| Character | `SkillTable` | `StartingSkillIds` |
|---|---|---|
| `BP_ThirdPersonCharacter` (player) | assigned | `Climbing`, `Vault`, `MartialArts` @ Rank 1 |
| `BP_DCCEnemy_HockeyWings` (CJ) | assigned | `FireFingers`, `MediaAnalyst` @ Rank 2 |
| `BP_DCCEnemy_ArmsOut` (Kyle) | assigned | `ParkingLotSpecial`, `QuickWit` |
| `BP_DCCEnemy_DiceWinged` (Marcel) | assigned | `MartialArts`, `FirstImpressions` |

The §3 skill-loadout wiring in this file **was already done**. The C++
`StartingSkillIds` grants written against that false diagnosis have been
**removed** from `DCCAllyCharacters.cpp` — a Blueprint's explicitly-authored value
wins over a C++ CDO default, so they could only ever have been inert or wrong.

The `SkillTable` constructor default was **kept**: harmless, and it means a future
character can't fail quiet. But it fixed nothing that was broken.

**The genuine bug in the same area is animation, not skills.** Every animation slot
(`AttackAnimation`, `CastAnimation`, `IdleAnimation`, `RunAnimation`) reads `None`
on all four ally Blueprints, and `EmoteChanceOnKill` is 0. Because `None` *is* the
default, those slots are the one case where a C++ default does reach the Blueprint —
which is exactly why `DCCCrawlerAnimSet` works where a skill grant would not.

**Once restarted, PIE and confirm:** allies show a blue/green bar not red on a
second client; nameplates read correctly; allies **visibly swing** rather than
sliding around silently; CJ visibly gestures when Fire Fingers goes off and takes
~2s over it; clicking an achievement row opens its card; `L` cycles the feed filter
label at the top of its new bordered box.

## 0. START HERE (2026-08-15, later same day — second restart pending)

**The first restart already happened this session** (build landed clean, 17:15, real
`Link [x64] UnrealEditor-DungeonCrawlerDenver.dll`, no numbered hot-reload artifacts).
Everything from that batch is live: the three DataTables, `CastTimeSeconds`, gold/loot,
the seven pickups (placed, in Room 01), `ADCCFloorExit` (placed, in Room 02, confirmed
genuinely locked with no boss in the level), the Unenhanced/Enhanced stat split, equipment,
skeleton consolidation, and the enemy-health-bar rewrite off UMG onto the HUD Canvas.

### ⚠️ A second, smaller batch is queued behind another restart

All new `UCLASS`es, all compiled and DebugGame-verified, none placeable yet:

| Change | Where |
|---|---|
| `ADCCEnemyCharacter_TestBoss` — placeholder boss for testing the exit lock today | `Characters/DCCEnemyCharacter_TestBoss.*` |
| `ADCCEnemyCharacter_DebtEater` — real stat block from the design docs | `Characters/DCCEnemyCharacter_DebtEater.*` |
| `ADCCEnemyCharacter_Voldemortgage` — real stat block + boss tuning + voice lines | `Characters/DCCEnemyCharacter_Voldemortgage.*` |
| Retired `HealthBarWidget` at runtime (`DestroyComponent` in `BeginPlay`) | `Characters/DCCEnemyCharacter.cpp` |
| `ADCCAlly_HockeyWings/OfficeAirplane/ArmsOut/DiceWinged` — mesh + uniform animation defaults for the four ally Blueprints | `Characters/DCCAllyCharacters.*` |
| `ADCCQuestMarker_Orientation` / `_EastLedge` — fixes markers that tracked correctly but were invisible (Beacon mesh was never set) | `Quests/DCCQuestMarkers_Floor01.*` |
| Player skill casting wired to input at all — was previously dead for every skill except Climbing | `Characters/DCCCharacterBase.*` |
| Skill targeting overlay on the HUD | `UI/DCCHUD.*` |
| Equipment light support (`bEmitsLight` etc.) + Torch item | `Equipment/DCCEquipmentTypes.h`, `DCCEquipmentComponent.*`, `Docs/DT_DCCEquipment.csv` |
| `ADCCPickup_Jukebox` | `Items/DCCPickupVariants.*` |

**Close the editor, run the build in §0b (`Development`, not DebugGame), reopen.**

### Then, in this order

1. **Reconnect MCP.** Drops on every restart — a session id the server forgets, not a
   credential.
2. **Reparent the four ally Blueprints** — this is now scriptable, not manual (see below).
   Do this before anything else in this list, because `BP_DCCEnemy_OfficeAirplane`'s mesh is
   currently blank (see the note under §16-equivalent below) until this step runs.
3. **Delete the two placed `ADCCQuestMarker` instances and replace them** with
   `ADCCQuestMarker_Orientation` and `ADCCQuestMarker_EastLedge` at the same coordinates
   (Orientation at 5400,0,0; EastLedge at 6700,0,400) — these carry their tag, label and
   Beacon mesh as C++ defaults, so nothing needs setting by hand. This is the fix for
   "the quest tracker shows a goal but nothing is visible in the world."
4. **Re-import `Docs/DT_DCCEquipment.csv`** into `/Game/DCC/Equipment/DT_DCCEquipment` —
   the live table's schema is still the pre-restart struct layout (confirmed via
   `get_schema`, no light columns yet), so the Torch item cannot be imported until the
   editor has loaded the new `FDCCEquipmentDef` fields.
5. **Place `ADCCEnemyCharacter_TestBoss`** in Room 02 and kill it (§8e2) — proves the whole
   exit-lock loop end to end before Voldemortgage's real art exists.
6. **Finish clearing `HUDWidgetClass`** on the player Blueprint (§8c) — genuinely still needs
   the Details panel. `reset_properties` was tried on it and confirmed **not** to work (see
   CLAUDE.md) — it silently left the old value in place despite returning success.
7. **PIE and actually look at it.** Segmented health, the quest tracker, mana regeneration,
   nameplates, the cast lock, the world-space allegiance-coloured health bars, and — new
   this pass — **skill casting itself**, which was dead for every skill except Climbing
   until this session. Arm a damage or heal skill (not slot 1/Climbing) and confirm the
   targeting reticle appears and a click actually resolves it.

### The ally repoint — now automated, not manual (2026-08-15)

`ObjectTools.set_properties` is still broken, but `ObjectTools.reset_properties` turned out to
genuinely clear a component subobject's overridden mesh reference — confirmed by reading the
value back, not by trusting the return code (see CLAUDE.md's warning box for the full finding,
including where `reset_properties` *also* silently failed). Combined with
`BlueprintTools.set_parent`, that's a complete substitute for hand-editing here.

Four new classes in `Characters/DCCAllyCharacters.h/.cpp` carry the correct mesh (three of
them) and a **uniform** animation set (all four) — `A_DCC_OA_Alert` idle, `A_DCC_HW_Walking`
walk, `A_DCC_HW_Running` run, `A_DCC_DW_ChargedSlash` attack, a six-clip emote pool,
`EmoteChanceOnKill` 0.6. Once the editor is back and these classes exist:

1. `BlueprintTools.set_parent` on each of `BP_DCCEnemy_HockeyWings`, `_OfficeAirplane`,
   `_ArmsOut`, `_DiceWinged`, pointed at the matching `ADCCAlly_*` class.
2. `ObjectTools.reset_properties` on the `Mesh` component's `SkeletalMeshAsset` for
   **OfficeAirplane, ArmsOut and DiceWinged only** — HockeyWings needs no mesh reset, its
   mesh was already correct. This clears the old baked-in override so the mesh inherits the
   new parent's default instead. **OfficeAirplane's was already cleared to `None` this
   session** while confirming `reset_properties` works at all — it will show no mesh at all
   until step 1 runs for it, which is expected, not a new bug.
3. `compile_blueprint` and save each of the four.

Deliberately **not** on the shared `ADCCEnemyCharacter` base — these clips are authored
against `SK_DCCCrawler_Skeleton`, and `ADCCEnemyCharacter` is also the parent of the
Brute/Analyst/Courier mobs, still on the Mannequin skeleton. Playing a crawler clip on a
Mannequin mesh is exactly the `SkeletonToCompactPose.IsValidIndex` ensure hit earlier this
session — hence four narrow classes instead of one shared change.

The demo loop that should exist after that pass: **fight → enemies drop gold → pick it up →
carry the Strongbox → find the exit → it is locked → kill the boss → leave with the money.**
Every piece of that is written and placed; only the boss half is untested.

### The DebugGame compile-check works — use it from now on

Confirmed 2026-08-15, three times including a reflection-touching batch. With Live Coding
**off** and the editor **open**, building the **`DungeonCrawlerDenver`** target in
**DebugGame** compiles and links without disturbing the editor or the MCP session.
Incremental builds are 40 seconds to 3.5 minutes. Full details in CLAUDE.md.

It verifies compilation only — it does **not** deploy into the running editor. That is why
the restart above is still owed.

## 0b. The build command

Both batches are compiled and committed: the pickups/skill-rank/five-stat/NPC-casting
batch, and the climbing/sprint batch on top of it. `Build.bat DungeonCrawlerDenverEditor`
succeeded clean.

For reference, the command is:

```bash
"D:/UE/UE_5.8/Engine/Build/BatchFiles/Build.bat" DungeonCrawlerDenverEditor Win64 Development -Project="C:/Users/kmett/Documents/Unreal Projects/DungeonCrawlerDenver/DungeonCrawlerDenver.uproject" -WaitMutex
```

**Do not use Live Coding for reflection changes.** It reports success and then crashes
the editor on PIE (see CLAUDE.md).

## 1. Expect Blueprint fixups after the enum change

`EDCCAbility` dropped Wisdom and reordered to Str/Int/Con/Dex/Cha. `FDCCAbilityScoreSet.Scores`
is an index-based array, so **every existing stat block is now misread**. Affected:

- `BP_DCCEnemy`, `BP_DCCEnemy_Brute`, `BP_DCCEnemy_Courier`, `BP_DCCEnemy_Analyst`
- `BP_DCCEnemy_HockeyWings`, `BP_DCCEnemy_OfficeAirplane`
- Any `bOverrideAbilityScores` entries on the two encounters (currently all false, so harmless)

They were deliberately left unset rather than written against the old six-slot order.

## 2. Stat blocks to apply (five values, Str/Int/Con/Dex/Cha)

| Blueprint | STR | INT | CON | DEX | CHA | Level |
|---|---|---|---|---|---|---|
| `BP_DCCEnemy_Brute` | 6 | 2 | 5 | 3 | 4 | 2 |
| `BP_DCCEnemy_Courier` | 3 | 4 | 2 | 6 | 5 | 1 |
| `BP_DCCEnemy_Analyst` | 2 | 6 | 4 | 5 | 3 | 2 |
| `BP_DCCEnemy_HockeyWings` (CJ) | 4 | 6 | 3 | 2 | 5 | 2 |
| `BP_DCCEnemy_OfficeAirplane` (Scott) | 4 | 3 | 5 | 3 | 4 | 2 |

CJ and Scott use the party sheet's own arrays. CJ's **Intelligence 6 gives him 6 Mana**,
which is what pays for Fire Fingers.

## 3. Skill loadouts

Set on each Blueprint's `Skills` component:

- `SkillTable` → `/Game/DCC/Skills/DT_DCCSkills`
- `StartingSkillIds` → per table below
- `StartingSkillRank` → 1 unless noted

| Blueprint | StartingSkillIds | Rank |
|---|---|---|
| `BP_DCCEnemy_HockeyWings` (CJ) | `FireFingers`, `MediaAnalyst` | 2 |
| `BP_DCCEnemy_OfficeAirplane` (Scott) | `Critic`, `NumberCruncher` | 1 |
| `BP_DCCEnemy_Brute` | `Headbutt`, `KickItOver` | 1 |
| `BP_DCCEnemy_Courier` | `ParkingLotSpecial`, `Vault` | 1 |
| `BP_DCCEnemy_Analyst` | `SummonInterns`, `FieldDressing` | 1 |

## 4. Mark the spells as un-attemptable untrained

`bCanAttemptUntrained` did not exist when the rows were authored. Set it **false** on
`FireFingers`, `SoulCollector`, `FieldDressing`, `HypeTheCrowd`, `SummonInterns` —
the published rules forbid casting a Spell Skill you have no Ranks in.

## 5. Fire Fingers VFX

On the `FireFingers` row of `DT_DCCSkills`:

- `CastEffect` → `/Game/Legendary_Forge/Fx/Fire/NS_fire`
- `ImpactEffect` → `/Game/Variant_Combat/VFX/NS_Damage`
- `CastSocketName` → leave empty for now

The socket is deliberately empty: the Meshy skeletons have **no hand sockets**, so
`PlaySkillEffects` falls back to chest height in front of the caster. Adding a
`hand_r` socket to `SK_HockeyWings_Skeleton` later and naming it here is a one-line
improvement.

Neither effect is a purpose-built spell VFX — `NS_fire` is a forge flame. It reads as
fire at the hands, which is enough to confirm the plumbing works before spending money
on a proper spell pack.

## 6. Pickups — ✅ WRITTEN 2026-08-15, placement pending restart

Six `ADCCPickup` subclasses now exist in `Items/DCCPickupVariants.h/.cpp`, compiled and
linked. **C++ subclasses rather than Blueprints on purpose** — `ObjectTools.set_properties`
returns false and writes nothing, so a `BP_Pickup_*` could not be configured through MCP at
all. A constructor is automatable and shows up in a diff.

| Class | Mesh | Mass | Die | Needs |
|---|---|---|---|---|
| `ADCCPickup_Bottle` | `Cave_Camp/SM_bottle` | 3kg | d4 | anyone |
| `ADCCPickup_EggBox` | `Diner/SM_egg_box` | 8kg | d4 | anyone |
| `ADCCPickup_GreaseBarrel` | `Cave_Camp/SM_barrel_01` | 35kg | d6 | STR 3+ |
| `ADCCPickup_Crate` | `Cave_Camp/SM_crate_01` | 40kg | d6 | STR 3+ |
| `ADCCPickup_Keg` | `Cave_Camp/SM_barrel_03` | 70kg | d8 | STR 5+ |
| `ADCCPickup_StrongboxCrate` | `Cave_Camp/SM_crate_02` | 75kg | d8 | STR 5+ |

Carry limit is `Strength score × 15kg`, so on the Standard Array: STR 2 → 30kg, STR 3 →
45kg, STR 4 → 60kg, STR 5 → 75kg, STR 6 → 90kg. That puts **Kyle (STR 2) on bottles and egg
boxes only**, and makes `StrongboxCrate` at 75kg the exact STR 5 boundary.

All meshes were bounds-checked as human-scale. The original suggestion here,
`Ice_Station/SM_large_crate_01`, is **582 × 384 × 277 cm — nearly six metres** — and was
rejected for exactly the reason the Ice_Station warning in CLAUDE.md gives.

**These are new `UCLASS`es, so they do not exist in a running editor** — they cannot be
placed until after the next close/build/reopen. Once restarted, scatter them across Room 01
as a throw sandbox and, later, on the Pantry shelving.

Not yet modelled: the floor design's "heaviest item is the most valuable" half. Weight
already carries the fight-vs-loot tension on its own, since carrying fills both hands;
a gold value would need a deposit mechanic to consume it, and unconsumed data is worse
than none.

## 7. Climbing and sprinting

Both are compiled. What is missing is content.

**A `Climbing` row in `DT_DCCSkills`.** Archetype `Utility - Movement`, governing
ability Strength, `bIsPassive` false, `bCanAttemptUntrained` true. It is not cast
through `ActivateSkill` — the climb path only reads its Rank and marks it — but it has
to exist in the table or it cannot appear in the menu to be armed.

Then add `Climbing` to every crawler's `StartingSkillIds` at Rank 1.

**`ADCCClimbPoint` actors on the faces worth climbing.** The `Surface` box is the
clickable face: size it to the rock, and set `ClimbTopZ`/`ClimbBottomZ` if the box does
not already match the climbable span. `ReachDistance` defaults to 220cm.

Rank matters here: `GetClimbSpeed` is `90 + Str×12 + Rank×6` cm/s and
`GetClimbStaminaCost` falls from 12/s at Rank 0 to 3/s at Rank 15, so a trained climber
covers roughly twice the height on the same pool.

**Animation slots.** `ClimbAnimation` on each character Blueprint — `A_DW_ClimbStairs`
is the only climb-ish clip in the set and only exists for the DiceWinged skeleton. The
template player character has none, so a player will climb in their idle pose until a
clip is imported. Everything still works; it just looks wrong.

## 8. Quests, achievements and the asset gallery (editor work, after the build)

### 8a. Import the tables — ✅ DONE 2026-08-15

All three are imported, saved, and **no longer need assigning**: each is loaded as a C++
constructor default (`ConstructorHelpers::FObjectFinder`) in `UDCCAchievementComponent`,
`UDCCQuestComponent` and `ADCCDialogueNPC`. That was forced by `ObjectTools.set_properties`
being broken, but it is the better answer anyway — the systems can no longer fail quiet.

Note for any future table: **`import_file` rejects `.json`** (it routes through
`CSVImportFactory`). Use `create` → `add_rows` → `set_rows` for JSON sources. Details in
CLAUDE.md under MCP gotchas.

The original instructions, kept for reference:



| File | Import as | Row struct |
|---|---|---|
| `Docs/DT_DCCAchievements.csv` | `/Game/DCC/Progression/DT_DCCAchievements` | `DCCAchievementDef` |
| `Docs/DT_DCCQuests.json` | `/Game/DCC/Quests/DT_DCCQuests` | `DCCQuestDef` |
| `Docs/DT_DCCDialogue.json` | `/Game/DCC/Dialogue/DT_DCCDialogue` | `DCCDialogueLine` |

`Docs/DT_DCCEnemies.json` is **reference, not an import** — it has no matching row
struct and is not meant to have one. Enemy stat blocks are set on the Blueprints;
that file is where the numbers and the boss phase structure are written down.

Quests are **JSON, not CSV**, on purpose — a quest's `Objectives` is an array of
structs and the CSV row format cannot express one.

Then assign them:
- `DT_DCCAchievements` → the `Achievements` component on each crawler Blueprint
- `DT_DCCQuests` → the `Quests` component on `ADCCGameState`

Without those two assignments both systems are inert. They fail quiet, not loud:
no table means no rows means nothing ever fires.

### 8b. Place the quest markers — ⚠️ PLACED, NOT CONFIGURED

Both actors exist in `Lvl_ThirdPerson` and are saved, at the coordinates below. **Their
`ObjectiveTag`, `MarkerLabel` and `Beacon` mesh are all still unset**, because
`ObjectTools.set_properties` returns `false` and writes nothing (see CLAUDE.md, MCP gotchas).
Until those four fields are filled in by hand they are invisible no-ops.

`ADCCQuestMarker` actors, one per `ReachLocation` objective. The starter
questline needs exactly two:

| `ObjectiveTag` | `MarkerLabel` | Where |
|---|---|---|
| `Orientation` | `ORIENTATION` | Room 01 floor, near the west doorway (~x 5400, y 0, z 0) |
| `EastLedge` | `THE LEDGE` | On the climb ledge (~x 6700, y 0, z 400) |

Give `Beacon` a mesh — `/Game/LevelPrototyping/Meshes/SM_Cube` scaled tall and
thin works, ideally with an emissive material. A marker with no beacon mesh still
functions; it is just invisible, which defeats the point.

Markers hide themselves whenever no active objective carries their tag, so the
level can be pre-populated with every marker a floor will ever need.

### 8b2. Two known bugs — ✅ BOTH RESOLVED 2026-08-15

**Achievement components are on every character, including mobs.** — *no change needed.*
`UDCCAchievementComponent::BeginPlay` already checks `IsCrawler()` and calls
`SetIsReplicated(false)` for mobs, leaving `Progress`/`StatTotals` empty rather than
zero-filled. The replicated-state waste this worried about does not actually occur. Moving the
component off `ADCCCharacterBase` has no clean target anyway — `IsCrawler()` is a runtime
check, not a subclass split.

**Nameplates and quest markers billboard to the wrong camera.** — *was already fixed* at both
named sites; `ADCCCharacterBase::Tick` and `ADCCQuestMarker::Tick` both use
`GEngine->GetFirstLocalPlayerController(GetWorld())` with comments explaining why.

**But a third site had it and was not listed:** `UI/DCCFloatingText.cpp` — fixed 2026-08-15.
That one mattered most: floating damage numbers spawn constantly in combat and draw on a flat
`TextRenderComponent` quad, so on a listen server every remote client's damage text would have
turned edge-on and vanished while the host's looked fine. It was missed because it lives in
`UI/`, not `Characters/` or `Quests/`.

**Lesson worth keeping:** grep for the *pattern* (`GetFirstPlayerController`) rather than
visiting the files a doc names. The doc named two sites; the codebase had three.

### 8c. Remaining wiring

- Clear `HUDWidgetClass` on the player Blueprint, or the old broken UMG HP bar
  draws on top of the working Canvas one.
- `FistBumpAnimation` on each character BP — the `AgreeGesture` clips are closest.
- `ClimbAnimation` — only `A_DW_ClimbStairs` exists, DiceWinged skeleton only.
- `EmoteChanceOnKill` on the four allies. Currently 0, so they never celebrate;
  0.6 or so was the intent.

### 8e2. The test exit, and what to do when Voldemortgage's model arrives

**`ADCCFloorExit` is written and compiled** but cannot be placed until after the restart —
it is a new `UCLASS`. Once the editor is back:

**Place the test exit in Room 02**, not the real arena. Room 7 (the Diving Pool, x 10400–13000)
does not exist as geometry yet, and the point of a test exit is to prove the lock works before
the arena is built. Room 02 already has a `DCCRoom`, a `DCCEncounter`, navmesh and working
enemies.

| Setting | Value |
|---|---|
| Location | ≈ x 8300, y 5000, z 0 — the east end of Room 02, opposite its west doorway |
| `bRequiresBossDefeat` | `true` (the default) |
| `FloorNumber` | `1` |

Then walk into it before placing anything else. It should refuse you, and the sign should
read `EXIT — CLOSED FOR MAINTENANCE` in red. That is the correct behaviour with **no boss in
the level at all** — the latch means an empty world reads as "boss hasn't arrived", not
"boss defeated".

**Then place `ADCCEnemyCharacter_TestBoss`** (also new this session, also gated on the same
restart) somewhere in Room 02, and kill it. The sign should flip to green `EXIT` about half a
second later — the exit polls every 0.5s, so that delay is expected, not a bug. This proves
the whole loop — lock, boss detection, unlock, banked gold on exit — end to end, today,
without waiting on real art.

**When the Voldemortgage model lands**, the C++ side is already done — see §8e. The only
steps left are art:

1. Convert and import — `Tools/glb_to_fbx.py`, then `Tools/split_glb_anims.py` for the
   animation set. The two expected import warnings are documented in CLAUDE.md.
2. One line in `ADCCEnemyCharacter_Voldemortgage`'s constructor: swap the
   `ConstructorHelpers::FObjectFinder<USkeletalMesh>` path from
   `SK_DiceWinged_Shared` to the real import.
3. Build the arena (Room 7) and place an `ADCCEncounter` there with the waves described
   in §8e.

**Nothing needs wiring between him and the exit.** The exit detects any living actor carrying
a `UDCCBossComponent`, so §8e's wave 0 alone locks it and killing him opens it. Deliberate — a
hand-wired reference is one more thing to forget on every future floor, and forgetting it
fails silently.

### 8e. Author the Voldemortgage encounter — ✅ CLASSES BUILT 2026-08-15, wave wiring still open

**Steps 1 and 2 below no longer need doing — both classes exist, tuned exactly
from `Docs/DT_DCCEnemies.json`, compiled and verified:**

- `ADCCEnemyCharacter_DebtEater` (`Characters/DCCEnemyCharacter_DebtEater.*`) —
  `GoldStolenPerHit` 2, `SummonerHealPerGold` 1.0 (that's the whole of
  "Repossession Nip" — the base class already bites gold on hit and feeds it to
  whichever spawn is flagged `bIsBoss`, no new mechanic needed), full stat
  block, reskinned onto `SK_ArmsOut_Shared` at 0.85x pending real art.
- `ADCCEnemyCharacter_Voldemortgage` (`Characters/DCCEnemyCharacter_Voldemortgage.*`)
  — carries a `UDCCBossComponent` tuned `ChargeHealthThreshold` 0.5,
  `ChargeDurationSeconds` 9, `ChargeBaseDamage` 6, `ChargeDamagePerMinion` 8,
  `VulnerableSecondsAfterCharge` 6, `LienGold` 25, full stat block, all four
  announcement lines written (one of his own barks reused for
  `ChargeStartLine`, since it already fit the Late Notice moment exactly),
  reskinned onto `SK_DiceWinged_Shared` at 1.5x.

Both are **new `UCLASS`es — cannot be placed until the next editor restart.**

**What's still a manual step, and why it can't be automated:** `ADCCEncounter`'s
`Waves` array is a nested struct property (`TArray<FDCCEncounterWave>` of
`TArray<FDCCEnemySpawn>`), and `ObjectTools.set_properties` cannot write it —
same broken path as everything else in this file's warning. Once Room 7 (the
Diving Pool arena) exists as geometry:

1. Place an `ADCCEncounter` in the arena.
2. **Wave 0**, trigger `Immediate`: one spawn of
   `ADCCEnemyCharacter_Voldemortgage` with **`bIsBoss` ticked** — the fight does
   not work at all without this flag, since it's what the threshold wave
   watches and what `GetLivingMinionCount` subtracts — plus three spawns of
   `ADCCEnemyCharacter_DebtEater`. Optionally put one of his barks in the
   wave's `Announcement` field: *"I have your file. I have everyone's file."*
3. **Wave 1**, trigger `HealthThreshold` at `0.5`: five more
   `ADCCEnemyCharacter_DebtEater` spawns. This is Late Notice.

The remaining two barks — *"Let's talk about your... payment history"* and
*"You are not in trouble. You are in arrears. There is a difference and it is
worse"* — are written down in the JSON for whenever idle chatter or the AI
narrator pass gets built. No such system exists yet; none was added for this.

**Until the arena exists**, `ADCCEnemyCharacter_TestBoss` (§8e2 below —
HockeyWings reskin, short-tuned charge, exists purely to give the exit lock a
real boss component to detect) is the way to test the exit lock end to end
today.

### 8d. The asset-gallery level (not started)

Blocked on the MCP re-auth, not on anything else. The plan:

1. New empty level at `/Game/DCC/Maps/Lvl_AssetGallery` — **not** a duplicate of
   a pack showcase map (that approach was rejected and should not be retried).
2. A flat prototyping floor, one `DirectionalLight`, one `SkyAtmosphere`, one
   `PlayerStart`.
3. Meshes laid out in labelled rows, one row per Fab pack, spaced by measured
   bounds rather than guessed ones — `Ice_Station`'s "floor module" is 55x58
   **metres** and `Evil_planet/SM_large_module_01` is 38,500 units wide.
4. A `UTextRenderComponent` sign per row naming the pack, so the row can be
   identified without hunting through the outliner.

Enumerate packs with `AssetTools.list_folders("/Game")` rather than trusting any
written list — packs get added between sessions.

## 9. Verify

- `P` pauses; queued actions resolve in initiative order
- `E` picks up a crate, `R` throws it; a strong character throws further
- Walk into Room 02 → CJ, Scott and a mannequin spawn
- CJ casts Fire Fingers from range instead of closing to melee
- Clearing an encounter rolls Skill Advancement (watch the log for `advanced to Rank`)
- Hold Shift → sprint; the stamina bar appears and drains, and refuses at empty
- Tab → `[1]` arms Climbing → click a rock face → `Y` → hold LMB to rise, RMB to
  descend, Space to let go. Running the pool dry mid-face drops you.
- Nameplates: red on mobs, pale blue `ALLY`, green `FRIEND` after a fist bump
- Click a living ally at close range → both turn and animate → they go green
- Take a killing blow → the HP bar sits visibly at empty under **GAME OVER**
- The objective tracker fills the top right and the first quest auto-starts;
  walking into the `Orientation` marker completes it and chains to the next
