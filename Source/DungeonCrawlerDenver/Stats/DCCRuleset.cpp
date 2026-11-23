#include "Stats/DCCRuleset.h"

int32 UDCCRuleset::GetAbilityModifier(int32 Score) const
{
	// TABLE 1: STAT MODS, from Rules for Survival p.11. Banded and logarithmic
	// rather than linear, which is what lets scores run into the hundreds later
	// without the d20 becoming irrelevant.
	//
	// Note the modifier is never negative and never zero: even a Stat of 1 gives
	// +1. This is a real difference from d20 fantasy, where a dumped stat
	// actively penalises you. Here a bad stat simply fails to help.
	static const TArray<TPair<int32, int32>> ModifierBands =
	{
		{ 2,   1 },
		{ 5,   2 },
		{ 9,   3 },
		{ 19,  4 },
		{ 49,  5 },
		{ 99,  6 },
		{ 149, 7 },
		{ 199, 8 },
		{ 299, 9 },
	};

	const int32 ClampedScore = FMath::Max(1, Score);

	for (const TPair<int32, int32>& Band : ModifierBands)
	{
		if (ClampedScore <= Band.Key)
		{
			return Band.Value;
		}
	}

	// 300+
	return 10;
}

int32 UDCCRuleset::GetCheckDieSides() const
{
	return 20;
}

FDCCCheckResult UDCCRuleset::ResolveCheck(
	const FDCCAbilityScoreSet& AbilityScores,
	EDCCAbility Ability,
	int32 DifficultyClass,
	int32 SituationalBonus) const
{
	const int32 DieSides = FMath::Max(1, GetCheckDieSides());

	FDCCCheckResult Result;
	Result.Roll = FMath::RandRange(1, DieSides);
	Result.Modifier = GetAbilityModifier(AbilityScores.GetScore(Ability)) + SituationalBonus;
	Result.Total = Result.Roll + Result.Modifier;
	Result.DifficultyClass = DifficultyClass;
	Result.bSuccess = Result.Total >= DifficultyClass;
	Result.bCriticalSuccess = Result.Roll == DieSides;
	Result.bCriticalFailure = Result.Roll == 1;

	return Result;
}

float UDCCRuleset::GetAllyToughnessMultiplier() const
{
	return 1.75f;
}

int32 UDCCRuleset::GetMaxHitPoints(const FDCCAbilityScoreSet& AbilityScores, int32 Level) const
{
	// STILL AN INVENTION. Constitution determines Health per the rules, but the
	// actual formula is on Crawlers & Customization p.41, which is not in the
	// Sneak Preview excerpts we have. Everything else in this file now matches
	// the published rules; this does not, and is the last placeholder left.
	//
	// Tuned for the 2-6 score band rather than the old d20-fantasy one: a
	// Constitution 4 crawler starts at 18, a Constitution 6 one at 22.
	const int32 ConstitutionScore = FMath::Max(1, AbilityScores.GetScore(EDCCAbility::Constitution));
	const int32 EffectiveLevel = FMath::Max(1, Level);

	return FMath::Max(1, 10 + (ConstitutionScore * 2) + (EffectiveLevel - 1) * 2);
}

float UDCCRuleset::GetPlayerToughnessMultiplier() const
{
	// 2.5x. Enough that a player survives a fight they misjudged and learns from
	// it, not so much that the enemies stop being frightening - at 1.0 the
	// starting 18 HP is two solid hits from a Brute, which is not a fight, it is
	// a coin flip.
	return 2.5f;
}

int32 UDCCRuleset::GetMaxManaPoints(const FDCCAbilityScoreSet& AbilityScores, int32 Level) const
{
	// Straight Intelligence score, not the modifier - the real ruleset spends
	// Mana in double-digit amounts per spell, which only works off the score.
	return FMath::Max(0, AbilityScores.GetScore(EDCCAbility::Intelligence));
}

int32 UDCCRuleset::GetDefenceValue(const FDCCAbilityScoreSet& AbilityScores, int32 Level, int32 EvadeBonus) const
{
	// "Evade" is the published name for this. Dexterity enhances it, but the
	// exact formula is not in the preview excerpts, so the 10 + Dex shape is
	// still ours - it just now uses the correct banded modifier underneath.
	// EvadeBonus is equipment on top of that - see this function's own header
	// comment for why it is a separate parameter rather than folded into Dex.
	return 10 + GetAbilityModifier(AbilityScores.GetScore(EDCCAbility::Dexterity)) + EvadeBonus;
}

FDCCAttackResult UDCCRuleset::ResolveAttack(
	const FDCCAbilityScoreSet& AttackerScores,
	EDCCAbility AttackAbility,
	int32 WeaponDamageDie,
	const FDCCAbilityScoreSet& DefenderScores,
	int32 DefenderLevel,
	int32 SituationalBonus,
	int32 DefenderEvadeBonus) const
{
	const int32 DefenceValue = GetDefenceValue(DefenderScores, DefenderLevel, DefenderEvadeBonus);

	FDCCAttackResult Result;
	Result.Check = ResolveCheck(AttackerScores, AttackAbility, DefenceValue, SituationalBonus);

	// A critical always lands, a fumble never does, regardless of the arithmetic.
	Result.bHit = Result.Check.bCriticalSuccess || (Result.Check.bSuccess && !Result.Check.bCriticalFailure);

	if (Result.bHit)
	{
		const int32 DamageDie = FMath::Max(1, WeaponDamageDie);
		const int32 AbilityBonus = GetAbilityModifier(AttackerScores.GetScore(AttackAbility));

		int32 DamageRoll = FMath::RandRange(1, DamageDie);
		if (Result.Check.bCriticalSuccess)
		{
			// Crits double the die, not the modifier - doubling both makes high
			// stats scale the spike far too hard at low levels.
			DamageRoll += FMath::RandRange(1, DamageDie);
		}

		// A landed hit always does something; otherwise a bad modifier turns a
		// successful swing into a no-op, which reads as a bug to players.
		Result.Damage = FMath::Max(1, FMath::RoundToInt((DamageRoll + AbilityBonus) * GetScorePowerMultiplier(AttackerScores.GetScore(AttackAbility))));
	}

	return Result;
}

int32 UDCCRuleset::GetMaxSkillRank() const
{
	// Floors 1-5 are capped at 15 by the showrunners. Ranks above that exist in
	// the published rules but are gated behind content we do not have yet.
	return 15;
}

float UDCCRuleset::GetMaxStamina(const FDCCAbilityScoreSet& AbilityScores, int32 Level, int32 AthleticsRank) const
{
	// The SCORE, not the modifier - the banded modifier flattens 3, 4 and 5 all
	// to +2, which would give most of the party identical stamina and make the
	// stat invisible exactly where it should be felt.
	const int32 ConstitutionScore = FMath::Max(1, AbilityScores.GetScore(EDCCAbility::Constitution));

	// 40 base + 20 per point. On the 2-6 band that is 80 to 160, so Vaughn
	// (Con 6) climbs roughly twice as long as Nephi (Con 2). +2 per Athletics
	// Rank on top - modest on purpose, since this is a ceiling raised through
	// conditioning, not a second Constitution.
	return 40.0f + (ConstitutionScore * 20.0f) + (FMath::Max(0, AthleticsRank) * 2.0f);
}

float UDCCRuleset::GetClingStaminaCost(int32 ClimbSkillRank) const
{
	// Hanging still is cheap but never free, so a climber cannot park on a wall
	// indefinitely and wait out a fight below them.
	return FMath::Max(0.8f, 3.0f - (ClimbSkillRank * 0.15f));
}

float UDCCRuleset::GetClimbStaminaCost(int32 ClimbSkillRank) const
{
	// Rank makes climbing cheaper rather than making it succeed. That is the
	// whole point of dropping the pass/fail roll: a novice can attempt anything,
	// they just run out of arms sooner.
	// Tuned 2026-11-13 (Zoe: ledge to ledge should take a lot of stamina): a
	// Rank 1 climber spends about 70% of a Con 4 bar on a 6 m pitch; Rank 10
	// about a quarter; Rank 15 an eighth, since they are both cheaper AND faster.
	// Halved with the speed (2026-11-13, second pass): the climb is now twice as slow,
	// so per-second cost halves to keep a pitch costing about the same share of a bar.
	// Fourth pass (2026-11-13, Zoe: "one bar should last one or two ledges max"): a Rank 1 climber's
	// bar (Con 4, ~120) is gone in about one 8 m pitch; Rank 10 gets about two.
	return FMath::Max(3.5f, 7.8f - (ClimbSkillRank * 0.25f));
}

float UDCCRuleset::GetClimbSpeed(const FDCCAbilityScoreSet& AbilityScores, int32 ClimbSkillRank) const
{
	const int32 StrengthScore = FMath::Max(1, AbilityScores.GetScore(EDCCAbility::Strength));

	// Strength sets the baseline, Rank refines it - a strong novice hauls
	// themselves up, a practised climber moves efficiently.
	// Rank is the bigger term (2026-11-13): an experienced climber should
	// reach the summit clearly faster than a strong novice. Rank 1 with Str 4
	// is ~1.2 m/s; Rank 10 ~2.4 m/s; Rank 15 ~3.1 m/s.
	// Halved (2026-11-13, second pass: "players climb too quickly"). Rank 1 with
	// Str 4 is ~60 cm/s, which is the climb animation's natural pace at 1x
	// (ADCCCharacterBase::ClimbAnimNaturalSpeed); Rank 10 ~1.4 m/s, Rank 15 ~1.9.
	// Third pass (2026-11-13): the climb clip's natural pace is ~29 cm/s, so a
	// Rank 1, Str 4 climber (~46 cm/s) plays it at ~1.6x; Rank 10 ~91 cm/s, Rank 15 ~116.
	return 25.0f + (StrengthScore * 4.0f) + (ClimbSkillRank * 5.0f);
}

float UDCCRuleset::GetStaminaRegenRate(const FDCCAbilityScoreSet& AbilityScores, int32 AthleticsRank, float EquippedMassKg) const
{
	const int32 ConstitutionScore = FMath::Max(1, AbilityScores.GetScore(EDCCAbility::Constitution));

	const float BaseRate = 6.0f + (ConstitutionScore * 2.0f) + (FMath::Max(0, AthleticsRank) * 0.2f);

	// EquippedMassKg is already the excess over GetEncumbranceThresholdKg - the
	// caller subtracts the threshold (and its own Heavy Armor Training Rank)
	// before calling this, so this function only has to know about the
	// leftover weight, not re-derive the threshold itself. Never lets regen go
	// negative; a badly overloaded crawler stalls at zero recovery rather than
	// actively losing stamina just standing still.
	return FMath::Max(0.0f, BaseRate - (FMath::Max(0.0f, EquippedMassKg) * 0.5f));
}

float UDCCRuleset::GetSprintSpeedMultiplier(const FDCCAbilityScoreSet& AbilityScores) const
{
	const int32 DexterityScore = FMath::Max(1, AbilityScores.GetScore(EDCCAbility::Dexterity));

	// 1.5x at the middle of the starting band, rising to about 1.7x at Dex 6.
	// Kept modest because base walk speed is already Dexterity-scaled - stacking
	// a large multiplier on top would make a nimble crawler outrun the AI's
	// ability to path after them.
	return 1.5f + ((DexterityScore - 4) * 0.05f);
}

float UDCCRuleset::GetSprintStaminaCost(const FDCCAbilityScoreSet& AbilityScores, int32 AthleticsRank) const
{
	const int32 ConstitutionScore = FMath::Max(1, AbilityScores.GetScore(EDCCAbility::Constitution));

	// Tuned so a Constitution 4 crawler sprints roughly 12 seconds from full and
	// recovers it in about 10 - long enough to cross a room or escape a fight,
	// far too short to travel the map on. Athletics trims it further, the same
	// "cheaper, not automatic" shape as Climb Rank on climbing.
	return FMath::Max(2.0f, 12.0f - (ConstitutionScore * 1.0f) - (FMath::Max(0, AthleticsRank) * 0.3f));
}

float UDCCRuleset::GetEncumbranceThresholdKg(int32 HeavyArmorTrainingRank) const
{
	// 30kg is roughly "a loaded pack plus real armour" - enough that a
	// lightly-equipped crawler never notices this exists. +1kg per Rank in
	// Heavy Armor Training raises how much can be carried before it costs
	// stamina, rather than discounting the cost directly - "you can carry
	// more," not "the same load somehow weighs less."
	return 30.0f + FMath::Max(0, HeavyArmorTrainingRank);
}

FDCCCheckResult UDCCRuleset::ResolveSkillCheck(
	const FDCCAbilityScoreSet& AbilityScores,
	EDCCAbility Ability,
	int32 SkillRank,
	int32 DifficultyClass,
	bool bUntrained,
	int32 SituationalBonus,
	bool bAdvantage) const
{
	const int32 DieSides = FMath::Max(1, GetCheckDieSides());

	FDCCCheckResult Result;

	// Advantage and Disadvantage cancel out to a single ordinary roll, same as
	// they would on paper - an untrained crawler with an Advantage-granting
	// Class benefit is not stuck at Disadvantage forever, and isn't secretly
	// rolling twice-Advantage either.
	const bool bNetAdvantage = bAdvantage && !bUntrained;
	const bool bNetDisadvantage = bUntrained && !bAdvantage;

	if (bNetAdvantage || bNetDisadvantage)
	{
		// Two dice, keep the better (Advantage) or the worse (Disadvantage).
		// Rolled here rather than by shifting a bonus so the critical rules
		// below still see a real die face either way.
		const int32 FirstRoll = FMath::RandRange(1, DieSides);
		const int32 SecondRoll = FMath::RandRange(1, DieSides);
		Result.Roll = bNetAdvantage ? FMath::Max(FirstRoll, SecondRoll) : FMath::Min(FirstRoll, SecondRoll);
	}
	else
	{
		Result.Roll = FMath::RandRange(1, DieSides);
	}

	// Untrained attempts add the Stat Mod but no Rank, because there is no Rank
	// to add - that is the whole meaning of untrained.
	const int32 RankBonus = bUntrained ? 0 : FMath::Max(0, SkillRank);

	Result.Modifier = GetAbilityModifier(AbilityScores.GetScore(Ability)) + RankBonus + SituationalBonus;
	Result.Total = Result.Roll + Result.Modifier;
	Result.DifficultyClass = DifficultyClass;
	Result.bSuccess = Result.Total >= DifficultyClass;
	Result.bCriticalSuccess = Result.Roll == DieSides;
	Result.bCriticalFailure = Result.Roll == 1;

	return Result;
}

bool UDCCRuleset::ResolveAdvancementCheck(int32 CurrentRank) const
{
	if (CurrentRank >= GetMaxSkillRank())
	{
		return false;
	}

	const int32 DieSides = FMath::Max(1, GetCheckDieSides());

	// Meet-or-beat your own Rank. At Rank 1 this is a near certainty; at Rank 14
	// it is a 7-in-20 shot, which is the grind curve the books describe.
	return FMath::RandRange(1, DieSides) >= CurrentRank;
}

int32 UDCCRuleset::GetHealthSegmentCount() const
{
	return 10;
}

float UDCCRuleset::GetHealthSegmentSize(int32 MaxHitPoints) const
{
	const int32 Segments = FMath::Max(1, GetHealthSegmentCount());

	return FMath::Max(1.0f, static_cast<float>(FMath::Max(1, MaxHitPoints))) / Segments;
}

int32 UDCCRuleset::GetSegmentRegenCeiling(int32 CurrentHitPoints, int32 MaxHitPoints) const
{
	// The dead do not top up.
	if (CurrentHitPoints <= 0 || MaxHitPoints <= 0)
	{
		return FMath::Max(0, CurrentHitPoints);
	}

	const float SegmentSize = GetHealthSegmentSize(MaxHitPoints);

	// The epsilon matters. Sitting exactly on a boundary - 21 of 30, blocks of 3 -
	// divides to exactly 7.0, and float error nudging that to 7.0000001 would
	// round up to block 8 and hand out a free block every time.
	const float Blocks = FMath::CeilToFloat((CurrentHitPoints / SegmentSize) - KINDA_SMALL_NUMBER);

	const int32 Ceiling = FMath::FloorToInt32(Blocks * SegmentSize);

	return FMath::Clamp(Ceiling, CurrentHitPoints, MaxHitPoints);
}

float UDCCRuleset::GetHealthRegenRate(int32 MaxHitPoints) const
{
	// Five seconds to refill one block, for everybody. Expressed in blocks and
	// converted to hit points so that the bar animates at the same speed whether
	// the character has 30 hit points or 300 - the block is the unit players read,
	// so the block is the unit the timing should be authored in.
	constexpr float SecondsPerSegment = 5.0f;

	return GetHealthSegmentSize(MaxHitPoints) / SecondsPerSegment;
}

float UDCCRuleset::GetRegenDelaySeconds() const
{
	// Long enough that it never ticks mid-exchange, short enough that the party
	// gets their block back before the next room.
	return 4.0f;
}

float UDCCRuleset::GetManaRegenRate(int32 MaxManaPoints) const
{
	// A full pool in twenty-five seconds. Mana totals are small - Intelligence is
	// on the 2-6 band, so a caster holds single digits - which means a flat rate
	// would either be instant for a dull caster or useless for a bright one.
	constexpr float SecondsToFull = 25.0f;

	return FMath::Max(1, MaxManaPoints) / SecondsToFull;
}

int32 UDCCRuleset::GetExperienceToNextLevel(int32 CurrentLevel) const
{
	const int32 Level = FMath::Max(1, CurrentLevel);

	// Gently superlinear: 100, 250, 450, 700... Steep enough that a level feels
	// earned, shallow enough that it does not turn into a second job. The books
	// pace a level at roughly two hours of play, and this is tuned to land near
	// that for a party actually fighting things.
	return 100 * Level + 50 * Level * (Level - 1);
}

int32 UDCCRuleset::GetExperienceForKill(int32 VictimLevel, int32 KillerLevel) const
{
	const int32 Victim = FMath::Max(1, VictimLevel);
	const int32 Killer = FMath::Max(1, KillerLevel);

	const int32 Base = 25 * Victim;

	// Each level you outrank the victim by halves the payout, floored at a
	// tenth. Something far beneath you is still worth swinging at - it just
	// stops being worth seeking out.
	const int32 Gap = Killer - Victim;
	if (Gap <= 0)
	{
		return Base;
	}

	const float Scale = FMath::Max(0.1f, FMath::Pow(0.5f, static_cast<float>(Gap)));

	return FMath::Max(1, FMath::RoundToInt32(Base * Scale));
}

int32 UDCCRuleset::GetExperienceForEncounterClear(int32 EnemyCount, int32 AverageEnemyLevel) const
{
	const int32 Count = FMath::Max(1, EnemyCount);
	const int32 EnemyLevel = FMath::Max(1, AverageEnemyLevel);

	// Paid to every survivor, not split between them. Splitting would punish the
	// party for bringing everyone, which is the exact opposite of what a
	// five-player co-op game should be teaching.
	return 40 * Count * EnemyLevel;
}

int32 UDCCRuleset::GetExperienceForFloorClear(int32 FloorNumber, int32 EncounterCount) const
{
	const int32 Floor = FMath::Max(1, FloorNumber);
	const int32 Encounters = FMath::Max(1, EncounterCount);

	// Roughly a full level on floor 1, and it grows with depth. The floor is the
	// unit the books care about, so it is the grant that should feel like an
	// event rather than a trickle.
	return 250 * Floor * Encounters;
}

float UDCCRuleset::GetCarryMassLimit(const FDCCAbilityScoreSet& AbilityScores) const
{
	// The SCORE, not the modifier. The rules call this out by name as their
	// example of a score-derived value: "a crawler's Strength score is used to
	// determine what they can lift". Using the modifier here would flatten a
	// Strength 2 weakling and a Strength 6 bruiser into +1 versus +3, when the
	// whole point is that one of them can pick up the crate and the other cannot.
	const int32 StrengthScore = FMath::Max(1, AbilityScores.GetScore(EDCCAbility::Strength));

	// 15kg per point of Strength. On the starting 2-6 band that runs 30kg to
	// 90kg, so the Standard Array alone decides who moves the furniture.
	return FMath::Max(5.0f, StrengthScore * 15.0f);
}

float UDCCRuleset::GetThrowSpeed(const FDCCAbilityScoreSet& AbilityScores, float ObjectMass) const
{
	// Score again, for the same reason as lifting: how far you can hurl something
	// is physical capability, not a d20 roll.
	const int32 StrengthScore = FMath::Max(1, AbilityScores.GetScore(EDCCAbility::Strength));

	// Centred on the Standard Array's middle score of 4.
	const float BaseSpeed = 900.0f + ((StrengthScore - 4) * 160.0f);

	// Heavier than the reference mass throws proportionally slower; lighter than
	// it does NOT throw faster than the base. Without that ceiling the lightest
	// prop in the level becomes the best weapon in the game, which is funny once
	// and then ruins the encounter.
	const float SafeMass = FMath::Max(ObjectMass, 1.0f);
	const float MassFactor = FMath::Clamp(ReferenceObjectMass / SafeMass, 0.25f, 1.0f);

	return FMath::Max(250.0f, BaseSpeed * MassFactor);
}

int32 UDCCRuleset::GetThrowDamage(
	const FDCCAbilityScoreSet& AbilityScores,
	int32 ImpactDamageDie,
	float ImpactSpeed) const
{
	const int32 DamageDie = FMath::Max(1, ImpactDamageDie);
	const int32 StrModifier = GetAbilityModifier(AbilityScores.GetScore(EDCCAbility::Strength));

	// Below this the object is tumbling or rolling rather than flying, and should
	// not hurt anyone at all.
	constexpr float MinimumHarmfulSpeed = 300.0f;
	if (ImpactSpeed < MinimumHarmfulSpeed)
	{
		return 0;
	}

	// Full damage at roughly a thrown-at-reference-mass speed, scaling down for a
	// glancing late impact. Capped so a physics quirk that flings an object at an
	// absurd velocity cannot one-shot a boss.
	constexpr float FullDamageSpeed = 900.0f;
	const float SpeedFactor = FMath::Clamp(ImpactSpeed / FullDamageSpeed, 0.0f, 1.5f);

	const int32 Roll = FMath::RandRange(1, DamageDie);

	return FMath::Max(1, FMath::RoundToInt32((Roll + StrModifier) * SpeedFactor));
}

float UDCCRuleset::GetMoveSpeedMultiplier(const FDCCAbilityScoreSet& AbilityScores) const
{
	const int32 DexterityScore = FMath::Max(1, AbilityScores.GetScore(EDCCAbility::Dexterity));

	// Score rather than modifier, and measured against the Standard Array's
	// middle score of 4, so a Dexterity 2 crawler is genuinely slower than
	// average. Using the modifier would make everyone faster than baseline,
	// because this ruleset's modifiers are never negative.
	//
	// 4% per point: noticeable across a room, never so large that a nimble
	// crawler outruns the encounter's ability to path to them.
	return FMath::Clamp(1.0f + ((DexterityScore - 4) * 0.04f), 0.7f, 1.4f);
}
