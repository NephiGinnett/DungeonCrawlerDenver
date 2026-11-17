#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "DCCSprayTag.generated.h"

class UDecalComponent;
class UTexture2D;

/**
 * One spray-painted tag (2026-11-16, Zoe: "a jpg spray paint option where the player loads a jpg at purchase, and it
 * becomes pixilated but they can spray it wherever they want and it stays there"). A decal carrying the painter's
 * own picture, DCCSpray::Res pixels square. The server spawns it; the picture reaches every client by replication,
 * and each machine builds its own little texture from it. The host keeps them between sessions (DCCSpray::Save).
 */
UCLASS(NotPlaceable)
class DUNGEONCRAWLERDENVER_API ADCCSprayTag : public AActor
{
	GENERATED_BODY()

public:
	ADCCSprayTag();

	virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;
	virtual void BeginPlay() override;

	/** RGB, DCCSpray::Res * DCCSpray::Res * 3 bytes, row by row from the top. Set before FinishSpawning. */
	UPROPERTY(ReplicatedUsing = OnRep_Picture)
	TArray<uint8> Pixels;

	/** Who sprayed it (their profile name), for the save. */
	UPROPERTY(Replicated)
	FString Painter;

	/** When it went up (server, seconds since epoch), so the oldest goes first when there are too many. */
	double SprayedAt = 0.0;

private:
	UFUNCTION()
	void OnRep_Picture();
	void Paint();

	UPROPERTY(VisibleAnywhere)
	TObjectPtr<UDecalComponent> Decal;

	UPROPERTY(Transient)
	TObjectPtr<UTexture2D> Picture;
};
