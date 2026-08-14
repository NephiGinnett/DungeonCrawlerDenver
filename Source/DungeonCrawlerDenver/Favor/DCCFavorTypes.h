#pragma once

#include "CoreMinimal.h"
#include "DCCFavorTypes.generated.h"

// -----------------------------------------------------------------------------
// AI FAVOR
//
// The design doc calls this the "sponsor system" / audience-approval meter. The
// published DCC ruleset calls it AI Favor, so that is the name used here - when
// the real rules land, the vocabulary already matches.
//
// It rises on entertaining choices (risk, showmanship, crowd-pleasing kills,
// style, deliberate handicaps) and falls on boring, safe play. Crossing a tier
// unlocks sponsor buffs, item drops and DM interventions.
//
// v1 adjudication is DM-driven: Zoe calls the swings live. Automated detection is
// a later upgrade and explicitly not a launch blocker, so the automatic hooks
// below are a small, honest subset rather than a guess at judging "style".
// -----------------------------------------------------------------------------

/**
 * Why favor moved. Recorded with every change so the DM client, the phone HUD
 * and the AI narrator can all say *why* the crowd reacted, not just that it did.
 */
UENUM(BlueprintType)
enum class EDCCFavorEvent : uint8
{
	/** Zoe called it live. The primary path in v1. */
	DirectorCall      UMETA(DisplayName = "Director Call"),

	/** A maximum roll landing a kill. Spectacle, not just damage. */
	CriticalKill      UMETA(DisplayName = "Critical Kill"),

	/** Won a fight at very low health. The crowd loves a near miss. */
	NarrowSurvival    UMETA(DisplayName = "Narrow Survival"),

	/**
	 * Beat an encounter using a deliberately underpowered option. The ruleset
	 * preview is explicit that wimpy weapons earn favor when used for the
	 * majority of a real fight - the audience rewards showing off.
	 */
	StylishHandicap   UMETA(DisplayName = "Stylish Handicap"),

	/** Cleared a room without taking a swing. Efficient, but dull television. */
	BoringVictory     UMETA(DisplayName = "Boring Victory"),

	/** Hid, stalled, or refused to engage. */
	Cowardice         UMETA(DisplayName = "Cowardice"),

	/** Downed. Sympathy only goes so far. */
	Defeated          UMETA(DisplayName = "Defeated"),

	/** Sponsor or story beat scripted into a floor. */
	ScriptedBeat      UMETA(DisplayName = "Scripted Beat"),

	MAX               UMETA(Hidden)
};

/**
 * How interested the audience is. Thresholds are authored on the component
 * rather than hardcoded, because tuning this is a live, feel-based decision Zoe
 * will want to make between sessions.
 */
UENUM(BlueprintType)
enum class EDCCFavorTier : uint8
{
	/** Nobody is watching. No sponsors, no gifts, no help. */
	Unwatched     UMETA(DisplayName = "Unwatched"),

	/** A few viewers have you on a side window. */
	Noticed       UMETA(DisplayName = "Noticed"),

	/** Clipped and shared. Small sponsor offers start arriving. */
	Trending      UMETA(DisplayName = "Trending"),

	/** A real following. Gifts, and the crowd starts making demands. */
	FanFavorite   UMETA(DisplayName = "Fan Favorite"),

	/** Appointment viewing. The dungeon starts arranging things for you. */
	Headliner     UMETA(DisplayName = "Headliner"),

	MAX           UMETA(Hidden)
};

/** One entry in the favor ledger. */
USTRUCT(BlueprintType)
struct FDCCFavorChange
{
	GENERATED_BODY()

	UPROPERTY(BlueprintReadOnly, Category = "Favor")
	EDCCFavorEvent Reason = EDCCFavorEvent::DirectorCall;

	UPROPERTY(BlueprintReadOnly, Category = "Favor")
	int32 Delta = 0;

	UPROPERTY(BlueprintReadOnly, Category = "Favor")
	int32 NewTotal = 0;

	/** Free text for the DM client and the AI narrator to quote. */
	UPROPERTY(BlueprintReadOnly, Category = "Favor")
	FText Note;

	/** Server world-seconds, so the DM client can order the ledger. */
	UPROPERTY(BlueprintReadOnly, Category = "Favor")
	float TimeSeconds = 0.0f;
};
