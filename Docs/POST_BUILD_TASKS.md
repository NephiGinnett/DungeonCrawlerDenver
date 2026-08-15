# Post-build tasks

Editor-side wiring that the compiled C++ is waiting on. Nothing here needs another
build — every item is data set through the editor or over MCP.

## 0. The build itself — DONE (2026-08-14)

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

## 8. Verify

- `P` pauses; queued actions resolve in initiative order
- `E` picks up a crate, `R` throws it; a strong character throws further
- Walk into Room 02 → CJ, Scott and a mannequin spawn
- CJ casts Fire Fingers from range instead of closing to melee
- Clearing an encounter rolls Skill Advancement (watch the log for `advanced to Rank`)
- Hold Shift → sprint; the stamina bar appears and drains, and refuses at empty
- Tab → `[1]` arms Climbing → click a rock face → `Y` → hold LMB to rise, RMB to
  descend, Space to let go. Running the pool dry mid-face drops you.
