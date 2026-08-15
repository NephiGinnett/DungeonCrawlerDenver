# Post-build tasks

Editor-side wiring that the compiled C++ is waiting on. Nothing here needs another
build — every item is data set through the editor or over MCP.

## 0a. ONE BUILD IS OUTSTANDING (2026-08-15)

The **quest system** is written and committed but **never compiled** — the editor
was open and holds the Live Coding lock, which blocks every target, not just the
editor one. Close the editor and run the command in §0 below. Do not use Live
Coding for it: it adds new `UCLASS`/`USTRUCT`/`UENUM` types.

Everything before the quest system *is* compiled and working.

## 0b. MCP needs re-authorising

The `unreal-mcp` connection expired mid-session (`token expired`), which is why
the asset-gallery level and the wiring in §8 are not done — none of it can be
driven without the editor connection. Re-auth in an interactive session, then
the remaining work is quick.

## 0. The build itself — DONE for everything except the quest system

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

## 6. Pickups

`ADCCPickup` exists but no Blueprint or placed instance does. Make `BP_DCCPickup_Crate`
from `ADCCPickup`, mesh `/Game/Ice_Station/Meshes/Crates/SM_large_crate_01`,
`MassKilograms` 60, then scatter a few in Room 01 and Room 02.

60kg matters: on the 2–6 Strength scale, carry limit is `Strength × 15kg`. So **STR 4
(60kg) can just lift it and STR 3 cannot** — the stat is immediately legible at the table.
Add a lighter one (~30kg) so weaker crawlers still have something to throw.

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

### 8a. Import the two tables

| File | Import as | Row struct |
|---|---|---|
| `Docs/DT_DCCAchievements.csv` | `/Game/DCC/Progression/DT_DCCAchievements` | `DCCAchievementDef` |
| `Docs/DT_DCCQuests.json` | `/Game/DCC/Quests/DT_DCCQuests` | `DCCQuestDef` |

Quests are **JSON, not CSV**, on purpose — a quest's `Objectives` is an array of
structs and the CSV row format cannot express one.

Then assign them:
- `DT_DCCAchievements` → the `Achievements` component on each crawler Blueprint
- `DT_DCCQuests` → the `Quests` component on `ADCCGameState`

Without those two assignments both systems are inert. They fail quiet, not loud:
no table means no rows means nothing ever fires.

### 8b. Place the quest markers

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

### 8c. Remaining wiring

- Clear `HUDWidgetClass` on the player Blueprint, or the old broken UMG HP bar
  draws on top of the working Canvas one.
- `FistBumpAnimation` on each character BP — the `AgreeGesture` clips are closest.
- `ClimbAnimation` — only `A_DW_ClimbStairs` exists, DiceWinged skeleton only.

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
