#pragma once

#include "CoreMinimal.h"
#include "Engine/DataTable.h"
#include "Items/DCCLootBoxTypes.h"
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

	/**
	 * Corpses and chests emptied out - Loot All or Stow, not a per-item Check
	 * Inventory take (that path is left uncounted on purpose, so a player
	 * cannot inflate this by clicking through one item at a time).
	 */
	ContainersLooted    UMETA(DisplayName = "Containers Looted"),

	/**
	 * First time this crawler paused combat. A one-off flag rather than a
	 * running total - Record fires once, at Threshold 1, same shape as every
	 * counter above, just with a ceiling of one.
	 */
	PausedCombat        UMETA(DisplayName = "Paused Combat"),

	/** Reached the top AI Favor tier (Headliner) at least once. */
	FavorTierMaxed      UMETA(DisplayName = "Favor Tier Maxed"),

	/** First drink bought from a vendor - "House Pour". One-off, same shape as PausedCombat. */
	FirstDrinkBought    UMETA(DisplayName = "First Drink Bought"),

	/** Succeeded an Untrained skill check (no Rank, disadvantage) against DC 15+. */
	UntrainedHighDCSuccess UMETA(DisplayName = "Untrained High-DC Success"),

	/**
	 * Recorded once per DISTINCT room this crawler has ever physically entered
	 * - "The Full Tour" is just this stat with Threshold set to however many
	 * rooms a floor has, so the achievement system's own threshold logic does
	 * the counting rather than any bespoke "visited all N" check in C++.
	 */
	UniqueRoomVisited   UMETA(DisplayName = "Unique Room Visited"),

	/** This crawler's first room entry ever happened with the Body equipment slot still empty. */
	ArrivedBodySlotEmpty UMETA(DisplayName = "Arrived Body Slot Empty"),

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

	/**
	 * A sealed loot box paid on unlock, on top of (or instead of) XP/Favor.
	 * None is the default - most achievements still just pay XP/Favor exactly
	 * as before this existed. Tier should track how hard the achievement is:
	 * an easy/common one pays Bronze at best, a rare one can pay Celestial.
	 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Achievement")
	EDCCLootBoxTier LootBoxReward = EDCCLootBoxTier::None;

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
