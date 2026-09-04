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

	/**
	 * A running joke. Rows in this bucket carry a GagId; once one is rolled
	 * the gag goes "active" and gets a much higher chance of recurring for a
	 * while, then ages out. That recurrence is the whole point - real chat is
	 * mostly the same four jokes being reheated, and a pool of one-shot lines
	 * never reads as a crowd no matter how many of them there are.
	 */
	RunningGag    UMETA(DisplayName = "Running Gag"),

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

	/**
	 * Groups rows into one running joke. Every row sharing a GagId is a
	 * variation on the same bit, so the gag can recur without repeating the
	 * exact same words - which is what stops a "running joke" reading as a
	 * stuck record.
	 *
	 * Only meaningful on Bucket == RunningGag; None everywhere else.
	 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Chatter")
	FName GagId;
};

namespace DCCChatter
{
	/** {Name}/{Item}/{Event} substitution. Missing tokens are left untouched. */
	DUNGEONCRAWLERDENVER_API FText SubstituteTokens(const FText& Template,
		const FString& Name, const FString& Item, const FString& Event);

	/**
	 * Looks up the icon for a chat token like ":BORANT_W:" (the `Emote`
	 * bucket's whole reason to exist - "a string of invented, in-universe
	 * emote names," per that bucket's own doc comment above). Returns
	 * nullptr for anything not in the table, which the HUD falls back to
	 * drawing as plain text for - so an un-mapped or newly-authored token
	 * degrades gracefully instead of leaving a blank gap.
	 *
	 * A small in-code map rather than a DataTable on purpose, for now: these
	 * are brand names Zoe invents rarely, not chat lines she adds constantly
	 * the way DT_DCCChatterLines rows are - promote to a DataTable if that
	 * assumption stops holding and this needs to grow past a couple dozen.
	 */
	DUNGEONCRAWLERDENVER_API UTexture2D* FindEmoteIcon(const FString& Token);
}
