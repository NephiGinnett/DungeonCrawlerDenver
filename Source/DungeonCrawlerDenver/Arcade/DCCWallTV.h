#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "DCCWallTV.generated.h"

class UStaticMeshComponent;
class UTextRenderComponent;
class UNiagaraSystem;

/** A bezel, a dark screen, and a caption. No video until a media texture exists. */
UCLASS()
class DUNGEONCRAWLERDENVER_API ADCCWallTV : public AActor
{
	GENERATED_BODY()

public:
	ADCCWallTV();
	virtual void BeginPlay() override;
	virtual void Tick(float DeltaSeconds) override;

private:
	void LoadLines();
	void ApplyColour(UStaticMeshComponent* Mesh, const FLinearColor& Colour);

	UPROPERTY()
	TObjectPtr<UStaticMeshComponent> Bezel;
	UPROPERTY()
	TObjectPtr<UStaticMeshComponent> Screen;
	UPROPERTY()
	TObjectPtr<UTextRenderComponent> Caption;

	TArray<FString> Lines;
	int32 Index = 0;
	float NextSwap = 0.0f;
	bool bPushedNews = false;
};

/** Breaking news for the Safe Room sets. The file is the record. The arena replicates the latest lines. */
namespace DCCNews
{
	void Post(UWorld* World, const FString& Line);
}

/** Arena announcer. One line per moment, from Content/DCC/Announcers/announcers.json, with a short fallback. */
namespace DCCAnnouncer
{
	/** Server. Plays on every machine through the caller's multicast. Returns the sound path, or empty. */
	FString PathFor(const FString& Moment);
	void PlayLocal(const UObject* WorldContext, const FString& Moment);
}
