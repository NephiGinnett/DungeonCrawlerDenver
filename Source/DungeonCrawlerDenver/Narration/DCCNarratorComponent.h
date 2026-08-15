#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "UI/DCCEventLogTypes.h"
#include "DCCNarratorComponent.generated.h"

/**
 * Turns the event feed into commentary from a local language model.
 *
 * Sits on the GameState next to the feed it reads. The feed was built to make the
 * dice visible to the table - resolved outcomes, in plain language, with no
 * mechanics attached - which turns out to be exactly what a narrator wants, so the
 * integration needed no new data path.
 *
 * SERVER ONLY. Five clients each running their own model would produce five
 * different narrations of the same event, four of them describing a machine that
 * is not theirs. The output goes back into the replicated feed, so everyone reads
 * the same line.
 *
 * THE NARRATOR CANNOT AFFECT STATE. Per the design doc it describes what the
 * ruleset already decided and never decides anything itself. That constraint is
 * what makes the whole thing tractable: it is allowed to be slow, to fail, to
 * return garbage, and to be switched off, and none of those break the game.
 * Silence is always an acceptable output.
 *
 * See Docs/ai-narrator-integration.md.
 */
UCLASS(ClassGroup = (DCC), meta = (BlueprintSpawnableComponent))
class DUNGEONCRAWLERDENVER_API UDCCNarratorComponent : public UActorComponent
{
	GENERATED_BODY()

public:
	UDCCNarratorComponent();

	virtual void BeginPlay() override;
	virtual void TickComponent(float DeltaSeconds, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction) override;

	/**
	 * Offers an event for comment. Returns immediately, always.
	 *
	 * Most events are dropped on the floor - see ShouldNarrate. Ordinary hits and
	 * misses are never sent: they happen several times a second, they already have
	 * authored lines, and a model remarking on each would be both expensive and
	 * unbearable.
	 */
	UFUNCTION(BlueprintCallable, Category = "Narrator")
	void Consider(const FDCCEventLogEntry& Entry);

	/** True while a request is outstanding. Exactly one is allowed at a time. */
	UFUNCTION(BlueprintPure, Category = "Narrator")
	bool IsBusy() const { return bRequestInFlight; }

	/** Diagnostics for the DM client: how many replies were thrown away and why. */
	UPROPERTY(BlueprintReadOnly, Category = "Narrator")
	int32 RepliesRejected = 0;

	UPROPERTY(BlueprintReadOnly, Category = "Narrator")
	int32 LinesNarrated = 0;

protected:
	/** Whether this event is worth a model round trip at all. */
	bool ShouldNarrate(const FDCCEventLogEntry& Entry) const;

	/** Builds the prompt and posts it. Fire and forget. */
	void SendRequest(const FString& EventContext);

	/**
	 * Truncates, strips, and rejects output that invents numbers.
	 *
	 * Returns false when the reply should be discarded. Model output reaches five
	 * people through a replicated FText, so it is checked rather than trusted.
	 */
	bool SanitiseReply(const FString& Raw, const FString& SourceContext, FString& OutClean) const;

	/** The last N feed lines, newline separated, as model context. */
	FString BuildEventContext() const;

private:
	bool bRequestInFlight = false;

	/** World seconds of the last line actually shown. Drives the throttle. */
	float LastNarrationTime = 0.0f;

	/** World seconds of the last event of any kind. Drives idle colour. */
	float LastEventTime = 0.0f;

	/**
	 * Endpoint failures back off rather than retrying every beat.
	 *
	 * Without this, a model that is simply not running would produce a failed HTTP
	 * request for every kill in the game and fill the log with noise.
	 */
	float SuppressedUntilTime = 0.0f;
};
