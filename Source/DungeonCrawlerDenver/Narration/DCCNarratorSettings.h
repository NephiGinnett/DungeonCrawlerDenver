#pragma once

#include "CoreMinimal.h"
#include "Engine/DeveloperSettings.h"
#include "DCCNarratorSettings.generated.h"

/** Which local runner is on the other end. They speak different JSON. */
UENUM(BlueprintType)
enum class EDCCNarratorApi : uint8
{
	/** Ollama's native endpoint: POST /api/generate, { "response": "..." }. */
	Ollama       UMETA(DisplayName = "Ollama (/api/generate)"),

	/** OpenAI-compatible: POST /v1/chat/completions. LM Studio, llama.cpp server. */
	OpenAIChat   UMETA(DisplayName = "OpenAI-compatible (/v1/chat/completions)")
};

/**
 * Project Settings → Game → DCC Narrator.
 *
 * Config-driven rather than hardcoded because switching between Ollama and LM
 * Studio, or swapping models, is something Zoe will do repeatedly and it should
 * never need a rebuild.
 *
 * Off by default. The game shipped without a narrator and plays identically
 * without one, so the failure mode of "not configured" has to be silence rather
 * than errors.
 */
UCLASS(config = Game, defaultconfig, meta = (DisplayName = "DCC Narrator"))
class DUNGEONCRAWLERDENVER_API UDCCNarratorSettings : public UDeveloperSettings
{
	GENERATED_BODY()

public:
	virtual FName GetCategoryName() const override { return TEXT("Game"); }

	/** Master switch. Off means the component never ticks and costs nothing. */
	UPROPERTY(config, EditAnywhere, BlueprintReadOnly, Category = "Connection")
	bool bEnabled = false;

	UPROPERTY(config, EditAnywhere, BlueprintReadOnly, Category = "Connection")
	EDCCNarratorApi Api = EDCCNarratorApi::Ollama;

	UPROPERTY(config, EditAnywhere, BlueprintReadOnly, Category = "Connection")
	FString Endpoint = TEXT("http://127.0.0.1:11434/api/generate");

	UPROPERTY(config, EditAnywhere, BlueprintReadOnly, Category = "Connection")
	FString Model = TEXT("llama3.2");

	/**
	 * Abandoned after this long.
	 *
	 * Short on purpose: a narrator commenting on a fight that ended forty seconds
	 * ago is worse than silence, so a slow reply is not worth waiting for.
	 */
	UPROPERTY(config, EditAnywhere, BlueprintReadOnly, Category = "Connection", meta = (ClampMin = "1.0"))
	float TimeoutSeconds = 8.0f;

	/** Hard cap on generated tokens. Two sentences is the target. */
	UPROPERTY(config, EditAnywhere, BlueprintReadOnly, Category = "Generation", meta = (ClampMin = "8"))
	int32 MaxTokens = 60;

	UPROPERTY(config, EditAnywhere, BlueprintReadOnly, Category = "Generation", meta = (ClampMin = "0.0", ClampMax = "2.0"))
	float Temperature = 0.85f;

	/** Output longer than this is truncated before it reaches the feed. */
	UPROPERTY(config, EditAnywhere, BlueprintReadOnly, Category = "Generation", meta = (ClampMin = "40"))
	int32 MaxCharacters = 240;

	/**
	 * Discard any reply containing a digit that was not in the source events.
	 *
	 * The cheapest possible enforcement of the design doc's hard line that the AI
	 * never invents a mechanical outcome. Strongly recommended: it is the
	 * difference between a narrator that is charming and one that tells the party
	 * they took 14 damage when they took 6.
	 */
	UPROPERTY(config, EditAnywhere, BlueprintReadOnly, Category = "Generation")
	bool bRejectInventedNumbers = true;

	/** Minimum gap between narrations, whatever happens. */
	UPROPERTY(config, EditAnywhere, BlueprintReadOnly, Category = "Pacing", meta = (ClampMin = "0.0"))
	float MinSecondsBetweenLines = 12.0f;

	/** Silence this long triggers an idle remark. Zero disables idle colour. */
	UPROPERTY(config, EditAnywhere, BlueprintReadOnly, Category = "Pacing", meta = (ClampMin = "0.0"))
	float IdleCommentAfterSeconds = 90.0f;

	/** How many recent feed lines are sent as context. */
	UPROPERTY(config, EditAnywhere, BlueprintReadOnly, Category = "Pacing", meta = (ClampMin = "1"))
	int32 ContextLineCount = 8;

	/**
	 * The voice.
	 *
	 * Compressed from Docs/dungeon-tone-bible.md §1 and §7. The "never invent"
	 * rule is stated twice, top and bottom, because small models drop the middle
	 * of a long system prompt.
	 */
	UPROPERTY(config, EditAnywhere, BlueprintReadOnly, Category = "Generation", meta = (MultiLine = "true"))
	FString SystemPrompt = TEXT(
		"You are the System AI of a broadcast dungeon: a television producer with a quota, "
		"narrating live to an audience. You are smug, corporate, and bored by violence you have seen a thousand times.\n\n"
		"RULE ONE: never invent a mechanical outcome. You describe only what the events below already say happened. "
		"If you are unsure whether something happened, do not mention it. Never state a number that is not in the events.\n\n"
		"Style: one or two sentences, maximum. Refer to the players as crawlers, never as 'you the player'. "
		"Never invent a character name. Never use markdown, quotation marks, or a speaker prefix. "
		"When something genuinely terrible happens, get quieter and more corporate rather than louder.\n\n"
		"RULE ONE AGAIN: describe only what the events say. Invent no numbers, no names, and no outcomes.");
};
