# Post-build tasks

Everything queued behind one editor close. Written down because the C++ below is
**written but never compiled** — UnrealBuildTool refuses to build at all while the
editor holds the Live Coding lock, so none of it has been verified.

## 0. The build itself

Close the editor completely, then:

```bash
"D:/UE/UE_5.8/Engine/Build/BatchFiles/Build.bat" DungeonCrawlerDenverEditor Win64 Development -Project="C:/Users/kmett/Documents/Unreal Projects/DungeonCrawlerDenver/DungeonCrawlerDenver.uproject" -WaitMutex
```

Expect compile errors on the first attempt — this is a large unverified batch touching
reflection, a new module dependency (`Niagara`), and a breaking change to `EDCCAbility`.

**Do not use Live Coding for any of it.** These are new `UCLASS`/`UPROPERTY`/`UFUNCTION`
declarations and a changed enum; Live Coding reports success and then crashes the editor
on PIE (see CLAUDE.md).

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

## 7. Verify

- `P` pauses; queued actions resolve in initiative order
- `E` picks up a crate, `R` throws it; a strong character throws further
- Walk into Room 02 → CJ, Scott and a mannequin spawn
- CJ casts Fire Fingers from range instead of closing to melee
- Clearing an encounter rolls Skill Advancement (watch the log for `advanced to Rank`)
