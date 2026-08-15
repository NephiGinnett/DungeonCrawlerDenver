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

int32 UDCCRuleset::GetDefenceValue(const FDCCAbilityScoreSet& AbilityScores, int32 Level) const
{
	// "Evade" is the published name for this. Dexterity enhances it, but the
	// exact formula is not in the preview excerpts, so the 10 + Dex shape is
	// still ours - it just now uses the correct banded modifier underneath.
	return 10 + GetAbilityModifier(AbilityScores.GetScore(EDCCAbility::Dexterity));
}

FDCCAttackResult UDCCRuleset::ResolveAttack(
	const FDCCAbilityScoreSet& AttackerScores,
	EDCCAbility AttackAbility,
	int32 WeaponDamageDie,
	const FDCCAbilityScoreSet& DefenderScores,
	int32 DefenderLevel,
	int32 SituationalBonus) const
{
	const int32 DefenceValue = GetDefenceValue(DefenderScores, DefenderLevel);

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
		Result.Damage = FMath::Max(1, DamageRoll + AbilityBonus);
	}

	return Result;
}

int32 UDCCRuleset::GetMaxSkillRank() const
{
	// Floors 1-5 are capped at 15 by the showrunners. Ranks above that exist in
	// the published rules but are gated behind content we do not have yet.
	return 15;
}

float UDCCRuleset::GetMaxStamina(const FDCCAbilityScoreSet& AbilityScores, int32 Level) const
{
	// The SCORE, not the modifier - the banded modifier flattens 3, 4 and 5 all
	// to +2, which would give most of the party identical stamina and make the
	// stat invisible exactly where it should be felt.
	const int32 ConstitutionScore = FMath::Max(1, AbilityScores.GetScore(EDCCAbility::Constitution));

	// 40 base + 20 per point. On the 2-6 band that is 80 to 160, so Vaughn
	// (Con 6) climbs roughly twice as long as Nephi (Con 2).
	return 40.0f + (ConstitutionScore * 20.0f);
}

float UDCCRuleset::GetClingStaminaCost(int32 ClimbSkillRank) const
{
	// Hanging still is cheap but never free, so a climber cannot park on a wall
	// indefinitely and wait out a fight below them.
	return FMath::Max(0.5f, 3.0f - (ClimbSkillRank * 0.15f));
}

float UDCCRuleset::GetClimbStaminaCost(int32 ClimbSkillRank) const
{
	// Rank makes climbing cheaper rather than making it succeed. That is the
	// whole point of dropping the pass/fail roll: a novice can attempt anything,
	// they just run out of arms sooner.
	return FMath::Max(3.0f, 12.0f - (ClimbSkillRank * 0.6f));
}

float UDCCRuleset::GetClimbSpeed(const FDCCAbilityScoreSet& AbilityScores, int32 ClimbSkillRank) const
{
	const int32 StrengthScore = FMath::Max(1, AbilityScores.GetScore(EDCCAbility::Strength));

	// Strength sets the baseline, Rank refines it - a strong novice hauls
	// themselves up, a practised climber moves efficiently.
	return 90.0f + (StrengthScore * 12.0f) + (ClimbSkillRank * 6.0f);
}

float UDCCRuleset::GetStaminaRegenRate(const FDCCAbilityScoreSet& AbilityScores) const
{
	const int32 ConstitutionScore = FMath::Max(1, AbilityScores.GetScore(EDCCAbility::Constitution));

	return 6.0f + (ConstitutionScore * 2.0f);
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

float UDCCRuleset::GetSprintStaminaCost(const FDCCAbilityScoreSet& AbilityScores) const
{
	const int32 ConstitutionScore = FMath::Max(1, AbilityScores.GetScore(EDCCAbility::Constitution));

	// Tuned so a Constitution 4 crawler sprints roughly 12 seconds from full and
	// recovers it in about 10 - long enough to cross a room or escape a fight,
	// far too short to travel the map on.
	return FMath::Max(4.0f, 12.0f - (ConstitutionScore * 1.0f));
}

FDCCCheckResult UDCCRuleset::ResolveSkillCheck(
	const FDCCAbilityScoreSet& AbilityScores,
	EDCCAbility Ability,
	int32 SkillRank,
	int32 DifficultyClass,
	bool bUntrained,
	int32 SituationalBonus) const
{
	const int32 DieSides = FMath::Max(1, GetCheckDieSides());

	FDCCCheckResult Result;

	if (bUntrained)
	{
		// Disadvantage: two dice, keep the worse. Rolled here rather than by
		// halving a bonus so that the critical rules below still see a real die
		// face - an untrained fumble should be able to happen.
		const int32 FirstRoll = FMath::RandRange(1, DieSides);
		const int32 SecondRoll = FMath::RandRange(1, DieSides);
		Result.Roll = FMath::Min(FirstRoll, SecondRoll);
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
