#pragma once

#include "CoreMinimal.h"
#include "Engine/DataTable.h"
#include "DCCAchievementTypes.generated.h"

/**
 * What a given achievement is counting.
 *
 * Deliberately a small closed set rather than a general scripting hook. Every
 * value here is something the game already computes and already tells someone
 * about - adding a stat means adding a line to DCCAchievementComponent::Record,
 * which is the honest cost of tracking a new thing and keeps the list from
 * quietly filling with metrics nobody ever reads.
 */
UENUM(BlueprintType)
enum class EDCCAchievementStat : uint8
{
	/** Characters put down by this crawler. */
	Kills               UMETA(DisplayName = "Kills"),

	/** Encounters cleared while this crawler was alive and present. */
	EncountersCleared   UMETA(DisplayName = "Encounters Cleared"),

	/** Floors cleared outright. */
	FloorsCleared       UMETA(DisplayName = "Floors Cleared"),

	/** Damage dealt, cumulative. */
	DamageDealt         UMETA(DisplayName = "Damage Dealt"),

	/** Damage taken, cumulative. Surviving a lot of punishment is its own story. */
	DamageTaken         UMETA(DisplayName = "Damage Taken"),

	/** Skills successfully raised a Rank. */
	SkillRanksGained    UMETA(DisplayName = "Skill Ranks Gained"),

	/** Levels gained. */
	LevelsGained        UMETA(DisplayName = "Levels Gained"),

	/** Metres climbed, cumulative. */
	MetresClimbed       UMETA(DisplayName = "Metres Climbed"),

	/** Fist bumps offered and accepted. */
	FistBumps           UMETA(DisplayName = "Fist Bumps"),

	/** Times this crawler has died. Yes, this is an achievement. */
	Deaths              UMETA(DisplayName = "Deaths"),

	MAX                 UMETA(Hidden)
};

/**
 * One achievement, as a DataTable row.
 *
 * A row rather than code for the same reason skills are: the tabletop game
 * invents these at the table, and Zoe adding "Bumped every ally on the floor"
 * mid-session should cost a DataTable edit, not a recompile.
 */
USTRUCT(BlueprintType)
struct FDCCAchievementDef : public FTableRowBase
{
	GENERATED_BODY()

	/** Stable identifier. Must match the row name. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Achievement")
	FName AchievementId;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Achievement")
	FText DisplayName;

	/**
	 * The System AI's line when it unlocks.
	 *
	 * Per the tone bible this is a producer talking to an audience, never a
	 * referee congratulating a player - it should sound like the achievement
	 * happened *to* the crawler rather than being awarded to them.
	 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Achievement")
	FText Announcement;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Achievement")
	EDCCAchievementStat Stat = EDCCAchievementStat::Kills;

	/** Reached or exceeded, the achievement unlocks. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Achievement", meta = (ClampMin = "1"))
	int32 Threshold = 1;

	/** Paid once on unlock. Zero is fine - some achievements are only bragging. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Achievement", meta = (ClampMin = "0"))
	int32 ExperienceReward = 0;

	/**
	 * AI Favor paid on unlock.
	 *
	 * The interesting ones are negative. Dying a lot should absolutely move the
	 * meter, and not upward.
	 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Achievement")
	int32 FavorReward = 0;

	/** Hidden achievements are not listed until unlocked. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Achievement")
	bool bSecret = false;
};

/** One crawler's progress towards one achievement. Replicated to the owner. */
USTRUCT(BlueprintType)
struct FDCCAchievementProgress
{
	GENERATED_BODY()

	UPROPERTY(BlueprintReadOnly, Category = "Achievement")
	FName AchievementId;

	UPROPERTY(BlueprintReadOnly, Category = "Achievement")
	int32 Progress = 0;

	UPROPERTY(BlueprintReadOnly, Category = "Achievement")
	bool bUnlocked = false;

	/** World seconds at unlock, so the end-of-floor recap can order them. */
	UPROPERTY(BlueprintReadOnly, Category = "Achievement")
	float UnlockedAtSeconds = 0.0f;

	/**
	 * False from the moment of unlock until the player opens it from the
	 * Achievements tab. Unlocking and collecting are separate on purpose - see
	 * UDCCAchievementComponent::Unlock and ::Server_ClaimReward - so a whole
	 * session's worth of unlocks can pile up as a queue of things to open
	 * rather than vanishing into stat totals the instant they happen.
	 */
	UPROPERTY(BlueprintReadOnly, Category = "Achievement")
	bool bRewardClaimed = false;
};
