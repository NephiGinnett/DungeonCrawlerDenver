#pragma once

#include "CoreMinimal.h"
#include "DCCAbilityTypes.generated.h"

// -----------------------------------------------------------------------------
// RULESET-AGNOSTIC LAYER
//
// Everything in Source/DungeonCrawlerDenver/Stats/ is deliberately free of any
// DCC-specific concept (sponsors, mutations, audience approval). The design doc
// calls for the real DCC TTRPG ruleset to be swappable in later without
// restructuring character data, so this layer knows only about abstract ability
// scores and checks. DCC flavour lives elsewhere and talks to this via UDCCRuleset.
// -----------------------------------------------------------------------------

/**
 * The five core stats. There is no Wisdom - the published ruleset folds wit and
 * wisdom together under Intelligence.
 *
 * Order matches the books (Str, Int, Con, Dex, Cha) and the party's own sheet,
 * so a stat block reads the same on screen as it does on paper.
 */
UENUM(BlueprintType)
enum class EDCCAbility : uint8
{
	Strength     UMETA(DisplayName = "Strength"),
	Intelligence UMETA(DisplayName = "Intelligence"),
	Constitution UMETA(DisplayName = "Constitution"),
	Dexterity    UMETA(DisplayName = "Dexterity"),
	Charisma     UMETA(DisplayName = "Charisma"),

	MAX          UMETA(Hidden)
};

/**
 * The five stat scores for one character.
 *
 * SCALE MATTERS HERE, and it is not the d20-fantasy one. A typical human crawler
 * starts with scores between 3 and 5; the book's Standard Array assigns exactly
 * 2, 3, 4, 5 and 6, one to each stat. Scores climb far higher than 20 later on,
 * which is why the modifier table is banded rather than linear.
 *
 * Scores and modifiers are NOT interchangeable. The rules are explicit: the full
 * name (Strength) means the score and is used for derived values like how much
 * you can lift; the abbreviation (Str) means the modifier and is what gets added
 * to d20 rolls. Mixing them up is the single easiest way to get this ruleset
 * subtly wrong.
 *
 * Stored as an array rather than named fields so iterating, replicating and
 * serialising stay uniform.
 */
USTRUCT(BlueprintType)
struct FDCCAbilityScoreSet
{
	GENERATED_BODY()

	FDCCAbilityScoreSet()
	{
		Scores.Init(DefaultScore, static_cast<int32>(EDCCAbility::MAX));
	}

	/** Middle of the typical human starting band (3-5). */
	static constexpr int32 DefaultScore = 4;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Abilities")
	TArray<int32> Scores;

	/**
	 * The book's Standard Array: 2, 3, 4, 5, 6 assigned one per stat, in the
	 * order the stats are listed. Callers reorder to taste.
	 */
	void ApplyStandardArray()
	{
		Scores = { 2, 3, 4, 5, 6 };
		Scores.SetNum(static_cast<int32>(EDCCAbility::MAX));
	}

	/** Total of all scores. The Standard Array totals 20. */
	int32 GetTotalPoints() const
	{
		int32 Total = 0;
		for (const int32 Score : Scores)
		{
			Total += Score;
		}
		return Total;
	}

	int32 GetScore(EDCCAbility Ability) const
	{
		const int32 Index = static_cast<int32>(Ability);
		return Scores.IsValidIndex(Index) ? Scores[Index] : DefaultScore;
	}

	void SetScore(EDCCAbility Ability, int32 NewScore)
	{
		const int32 Index = static_cast<int32>(Ability);
		if (Scores.IsValidIndex(Index))
		{
			Scores[Index] = NewScore;
		}
	}
};

/**
 * The outcome of a single resolved check. Returned by value so callers (and the
 * eventual phone HUD / DM client) can display the full arithmetic, not just a
 * pass/fail - showing players the roll is most of the fun.
 */
USTRUCT(BlueprintType)
struct FDCCCheckResult
{
	GENERATED_BODY()

	/** The raw die result before modifiers. */
	UPROPERTY(BlueprintReadOnly, Category = "Check")
	int32 Roll = 0;

	/** Ability modifier plus any situational bonus that was passed in. */
	UPROPERTY(BlueprintReadOnly, Category = "Check")
	int32 Modifier = 0;

	/** Roll + Modifier. */
	UPROPERTY(BlueprintReadOnly, Category = "Check")
	int32 Total = 0;

	/** The difficulty this was measured against. */
	UPROPERTY(BlueprintReadOnly, Category = "Check")
	int32 DifficultyClass = 0;

	UPROPERTY(BlueprintReadOnly, Category = "Check")
	bool bSuccess = false;

	/** Max roll on the die. Kept separate from bSuccess so sponsor beats can key off spectacle, not just outcome. */
	UPROPERTY(BlueprintReadOnly, Category = "Check")
	bool bCriticalSuccess = false;

	/** Roll of 1. Same reasoning as above, in the other direction. */
	UPROPERTY(BlueprintReadOnly, Category = "Check")
	bool bCriticalFailure = false;
};

/**
 * One line in a roll's modifier breakdown - the Feed's click-to-inspect UI
 * (ADCCCharacterBase::GetAbilityScoreBreakdown/GetSkillRankBreakdown). Lives
 * here rather than in Equipment or Classes because both of those need to
 * produce it and neither should depend on the other to do so.
 */
USTRUCT(BlueprintType)
struct FDCCModifierSource
{
	GENERATED_BODY()

	UPROPERTY(BlueprintReadOnly, Category = "Breakdown")
	FText Label;

	UPROPERTY(BlueprintReadOnly, Category = "Breakdown")
	int32 Amount = 0;
};

/**
 * The outcome of one attack: the to-hit check plus what it actually cost the
 * target. Damage is reported alongside the roll for the same reason the check
 * shows its arithmetic - the DM client and phone HUD both want to narrate the
 * swing, not just the result.
 */
USTRUCT(BlueprintType)
struct FDCCAttackResult
{
	GENERATED_BODY()

	/** The to-hit check. Its DifficultyClass is the defender's defence value. */
	UPROPERTY(BlueprintReadOnly, Category = "Attack")
	FDCCCheckResult Check;

	UPROPERTY(BlueprintReadOnly, Category = "Attack")
	bool bHit = false;

	/** Damage actually dealt. Zero on a miss. */
	UPROPERTY(BlueprintReadOnly, Category = "Attack")
	int32 Damage = 0;
};
