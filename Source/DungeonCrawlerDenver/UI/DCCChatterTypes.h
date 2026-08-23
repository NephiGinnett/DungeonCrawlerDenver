#pragma once

#include "CoreMinimal.h"
#include "Engine/DataTable.h"
#include "DCCChatterTypes.generated.h"

// -----------------------------------------------------------------------------
// THE SIMULATED AUDIENCE
//
// Chatter lines are flavour text standing in for the quintillions of viewers
// Docs/story-premise.md describes - a DataTable rather than code for the same
// reason Skills/Achievements/Quests/Dialogue are: Zoe should be able to add a
// line mid-session without a recompile. This is a starter pool
// (Docs/DT_DCCChatterLines.csv), not a claim of completeness.
//
// A batch is rolled on demand when a player opens the Chatter tab
// (ADCCGameState::RequestChatterBatch), not on a background timer - per Zoe's
// own framing, it's a burst of "comments coming in" when you check the
// channel, not a constant trickle nobody asked for.
// -----------------------------------------------------------------------------

/** What a Chatter line is reacting to. Percentages are the per-comment odds
 *  Zoe specified; whatever's left over after all the named buckets is Benign. */
UENUM(BlueprintType)
enum class EDCCChatterBucket : uint8
{
	/** ~5%: positive about a crawler with Favor >= 60%, or negative about one
	 *  at <= 40%. {Name} is filled from a crawler picked to match. */
	Sentiment     UMETA(DisplayName = "Sentiment"),

	/** ~5%: about a crawler's current gear. {Name}/{Item}. */
	Equipment     UMETA(DisplayName = "Equipment"),

	/** ~5%: references a recent Combat/Death/Encounter Feed entry. {Event}. */
	BossBattle    UMETA(DisplayName = "Boss Battle"),

	/** ~5%: generic appearance flavour, no data dependency. {Name}. */
	Appearance    UMETA(DisplayName = "Appearance"),

	/** ~5%: a string of invented, in-universe emote names - not real Twitch
	 *  emotes, for the same IP-safety reason Voldemortgage isn't the real
	 *  DCC villain. */
	Emote         UMETA(DisplayName = "Emote"),

	/** ~5%: untranslated alien-language gibberish. */
	AlienLanguage UMETA(DisplayName = "Alien Language"),

	/** A small chance any of the above is swapped for a moderation notice
	 *  instead of real content. */
	Moderated     UMETA(DisplayName = "Moderated"),

	/** Everything else: ordinary Twitch-chat-style filler. The majority of
	 *  lines land here, per Zoe's "most are benign" framing. */
	Benign        UMETA(DisplayName = "Benign"),

	MAX           UMETA(Hidden)
};

/** One line template, as a DataTable row. */
USTRUCT(BlueprintType)
struct FDCCChatterLineDef : public FTableRowBase
{
	GENERATED_BODY()

	/** Stable identifier. Must match the row name. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Chatter")
	FName LineId;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Chatter")
	EDCCChatterBucket Bucket = EDCCChatterBucket::Benign;

	/** May contain {Name}, {Item} or {Event} tokens - substituted by whichever
	 *  of those the bucket actually has data for. A token with nothing to fill
	 *  it is left as-is rather than crashing, so a mismatched row just reads
	 *  oddly instead of breaking the batch. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Chatter")
	FText Template;

	/** A fake viewer handle to attribute the line to. Picked at random from the
	 *  table rather than generated, so Zoe can keep the tone consistent
	 *  (in-universe handles, not real usernames). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Chatter")
	FString SenderHandle;
};

namespace DCCChatter
{
	/** {Name}/{Item}/{Event} substitution. Missing tokens are left untouched. */
	DUNGEONCRAWLERDENVER_API FText SubstituteTokens(const FText& Template,
		const FString& Name, const FString& Item, const FString& Event);
}
