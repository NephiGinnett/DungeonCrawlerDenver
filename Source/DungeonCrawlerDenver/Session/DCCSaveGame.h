#pragma once

#include "CoreMinimal.h"
#include "GameFramework/SaveGame.h"
#include "Subsystems/WorldSubsystem.h"
#include "DCCSaveGame.generated.h"

class ADCCCharacterBase;

/**
 * One crawler's progress, as saved on the host's machine
 * (Saved/SaveGames/DCC_<Crawler>.sav).
 *
 * Values maps "Component.Property" to that property's exported text, so the
 * save follows the components rather than a hand-written list of fields: a
 * field added to FDCCKnownSkill next month is saved without touching this.
 * Unknown keys are skipped on load and missing ones keep the pawn's default,
 * so old saves keep loading as the game grows. See Session/DCCSaveSystem.cpp
 * for exactly which properties are kept.
 */
UCLASS()
class DUNGEONCRAWLERDENVER_API UDCCCrawlerSave : public USaveGame
{
	GENERATED_BODY()

public:
	UPROPERTY()
	int32 Version = 1;

	UPROPERTY()
	FName CrawlerId;

	UPROPERTY()
	FDateTime SavedAt;

	UPROPERTY()
	TMap<FString, FString> Values;
};

/**
 * Saves every crawler on a timer (every 60 s) and when the world shuts down.
 * Server only; does nothing on clients.
 */
UCLASS()
class DUNGEONCRAWLERDENVER_API UDCCSaveSubsystem : public UWorldSubsystem
{
	GENERATED_BODY()

public:
	virtual bool ShouldCreateSubsystem(UObject* Outer) const override;
	virtual void OnWorldBeginPlay(UWorld& InWorld) override;
	virtual void Deinitialize() override;

private:
	FTimerHandle AutosaveTimer;
};
