#pragma once

#include "CoreMinimal.h"
#include "DCCEventLogTypes.generated.h"

class ADCCCharacterBase;

// -----------------------------------------------------------------------------
// THE EVENT FEED
//
// A running, human-readable account of what the game just decided: attacks,
// skills, rank-ups, deaths, cleared rooms.
//
// It exists because the mechanics are currently invisible. Fire Fingers hitting
// for 6 is a line in the output log and nothing else - the table cannot see the
// dice doing their work, which is most of the fun of a TTRPG adaptation.
//
// Shared rather than per-player, and stored on the GameState, because five
// people around one table want the same feed and because GameState is already
// the home for state every client must agree on. It also survives a pawn dying,
// which a component on the character would not.
//
// Voice follows Docs/dungeon-tone-bible.md: the System AI is a producer with a
// quota, never a referee. Per that document's rule 1, entries only ever restate
// an outcome the deterministic game already computed - they never invent one.
// -----------------------------------------------------------------------------

/**
 * What kind of event a line describes.
 *
 * Carried so the HUD can colour or filter the feed, and so the eventual AI
 * narrator can decide what is worth speaking aloud rather than reading out
 * every point of damage.
 */
UENUM(BlueprintType)
enum class EDCCLogCategory : uint8
{
	/** Attacks and damage. The bulk of the feed. */
	Combat      UMETA(DisplayName = "Combat"),

	/** Skill and spell activations. */
	Skill       UMETA(DisplayName = "Skill"),

	/** Skill Ranks gained. Rare and worth celebrating. */
	Advancement UMETA(DisplayName = "Advancement"),

	/** Someone went down. */
	Death       UMETA(DisplayName = "Death"),

	/** Room cleared, encounter armed, floor progress. */
	Encounter   UMETA(DisplayName = "Encounter"),

	/** AI Favor moved. */
	Favor       UMETA(DisplayName = "Favor"),

	/** An achievement fired. Its own category so the HUD can make a fuss of it. */
	Achievement UMETA(DisplayName = "Achievement"),

	/** XP and levels. */
	Experience  UMETA(DisplayName = "Experience"),

	/** The System AI talking to the room. */
	Announcement UMETA(DisplayName = "Announcement"),

	MAX         UMETA(Hidden)
};

/**
 * One line in the feed.
 *
 * Deliberately the same shape as FDCCFavorChange in Favor/DCCFavorTypes.h -
 * category, text, timestamp - rather than a second, differently-shaped logging
 * concept. The favor ledger already proved this is the right amount of data.
 */
USTRUCT(BlueprintType)
struct FDCCEventLogEntry
{
	GENERATED_BODY()

	UPROPERTY(BlueprintReadOnly, Category = "Log")
	EDCCLogCategory Category = EDCCLogCategory::Combat;

	/** The line as it appears on screen. Already formatted and localisable. */
	UPROPERTY(BlueprintReadOnly, Category = "Log")
	FText Text;

	/** Server world-seconds, so the HUD can age or order entries. */
	UPROPERTY(BlueprintReadOnly, Category = "Log")
	float TimeSeconds = 0.0f;

	/**
	 * Who rolled, and against whom - null on entries that were never a roll
	 * (an achievement firing, a favor change). Added for the "My Rolls" feed
	 * filter and for colour-coding; existing call sites that only pass
	 * Category/Text/context are unaffected, since these default to null/false.
	 */
	UPROPERTY(BlueprintReadOnly, Category = "Log")
	TObjectPtr<ADCCCharacterBase> Instigator = nullptr;

	UPROPERTY(BlueprintReadOnly, Category = "Log")
	TObjectPtr<ADCCCharacterBase> RollTarget = nullptr;

	/** Natural 20. Feed colours this green. */
	UPROPERTY(BlueprintReadOnly, Category = "Log")
	bool bCriticalSuccess = false;

	/** Natural 1. Feed colours this red. */
	UPROPERTY(BlueprintReadOnly, Category = "Log")
	bool bCriticalFailure = false;
};
