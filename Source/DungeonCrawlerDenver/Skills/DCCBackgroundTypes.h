#pragma once

#include "CoreMinimal.h"
#include "Engine/DataTable.h"
#include "DCCBackgroundTypes.generated.h"

// -----------------------------------------------------------------------------
// BACKGROUNDS - WHERE A CRAWLER'S FIRST SKILLS COME FROM
//
// Character creation in the published rules is "Step 1: Establish Your
// Background": you pick a background for each stage of your former life, and
// each one hands you two Skills at Rank 1. Skills are therefore earned from
// FICTION first and from grinding afterwards, which is the whole reason a
// crawler's sheet reads like a person rather than a build.
//
// The preview PDFs only contain the ANIMAL background tables (Tables 5-8). The
// human equivalents (Tables 1-4) are on pages Borant did not include, so the
// stage STRUCTURE below is taken from the rules and the human background CONTENT
// is ours - original material in the books' spirit, exactly as the floors and
// room clichés in Docs/dungeon-tone-bible.md are.
// -----------------------------------------------------------------------------

/**
 * The stages of a crawler's former life, in the order they are chosen.
 *
 * Ranks differ per stage in the published rules - adult careers stick harder
 * than childhood ones - so the stage is a mechanical fact, not just a label.
 */
UENUM(BlueprintType)
enum class EDCCBackgroundStage : uint8
{
	/** Circumstances of birth and childhood. Grants Rank 1 in the rules. */
	Youth    UMETA(DisplayName = "Youth"),

	/** Education, apprenticeship, whatever passed for training. Rank 1. */
	Training UMETA(DisplayName = "Training"),

	/** What you actually did for a living. Rank 3 - your career sticks hardest. */
	Adult    UMETA(DisplayName = "Adult"),

	/** The thing about you that is nobody's business. Rank 2. */
	Quirk    UMETA(DisplayName = "Quirk"),

	MAX      UMETA(Hidden)
};

/**
 * One selectable background: a bit of history, and the skills it plausibly left
 * you with.
 *
 * Authored as DataTable rows for the same reason skills are - Zoe can add a
 * background between sessions when a player turns up with a backstory nobody
 * anticipated, which at this table is the normal case rather than the exception.
 */
USTRUCT(BlueprintType)
struct FDCCBackgroundDef : public FTableRowBase
{
	GENERATED_BODY()

	/** Stable lookup key. Must be unique across all stages. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Background")
	FName BackgroundId;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Background")
	EDCCBackgroundStage Stage = EDCCBackgroundStage::Youth;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Background")
	FText DisplayName;

	/** The System AI's summary of your former life. Flavour only. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Background", meta = (MultiLine = "true"))
	FText Description;

	/**
	 * Row names in the skill table this background can grant.
	 *
	 * More options than the player may take, on purpose: the published rules
	 * offer three and let you keep two, so the same background produces
	 * different crawlers.
	 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Background")
	TArray<FName> SkillOptions;

	/** How many of SkillOptions the player keeps. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Background", meta = (ClampMin = "0"))
	int32 SkillsToChoose = 2;

	/**
	 * Rank the chosen skills are granted at.
	 *
	 * Authored per row rather than derived from Stage so a single standout
	 * background can break the pattern without needing a code change.
	 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Background", meta = (ClampMin = "0"))
	int32 GrantRank = 1;
};

/**
 * A skill the game suggests to a player, with the reason why.
 *
 * The reason is carried alongside the id because a recommendation the player
 * does not understand is just a default - and the whole point of suggesting by
 * stats or history is to teach a new player what their numbers mean.
 */
USTRUCT(BlueprintType)
struct FDCCSkillRecommendation
{
	GENERATED_BODY()

	UPROPERTY(BlueprintReadOnly, Category = "Recommendation")
	FName SkillId;

	UPROPERTY(BlueprintReadOnly, Category = "Recommendation")
	FText DisplayName;

	/** Higher is a better fit for this character. Used for ordering, not display. */
	UPROPERTY(BlueprintReadOnly, Category = "Recommendation")
	int32 Score = 0;

	/** Player-facing justification, e.g. "Your Strength is your best stat." */
	UPROPERTY(BlueprintReadOnly, Category = "Recommendation")
	FText Reason;

	/** True when the character could not currently afford to cast it. */
	UPROPERTY(BlueprintReadOnly, Category = "Recommendation")
	bool bUnaffordable = false;
};
