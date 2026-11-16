#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "DCCFireworks.generated.h"

class UInstancedStaticMeshComponent;
class UPointLightComponent;
class UCanvas;

/**
 * A summit celebration (2026-11-13): a fanfare, a banner, and a few seconds of
 * fireworks over the peak. Spawned by the server when someone tops out; every
 * machine runs the show itself from the replicated seed, so it costs nothing
 * on the network after the spawn.
 *
 * No pack ships firework effects or sounds, so the sparks are instanced spheres
 * lit by a flash light, and the fanfare and pops are synthesized at runtime
 * into a USoundWaveProcedural (the same trick DCCVoiceLines uses for WAVs).
 */
UCLASS()
class DUNGEONCRAWLERDENVER_API ADCCFireworks : public AActor
{
	GENERATED_BODY()

public:
	ADCCFireworks();

	virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;
	virtual void BeginPlay() override;
	virtual void Tick(float DeltaSeconds) override;

	UPROPERTY(Replicated)
	int32 Seed = 0;

	/** Who made it ("CJ"), for the banner. */
	UPROPERTY(Replicated)
	FString Climber;

	/** Big centred "CJ REACHED THE SUMMIT" while any show is young. Called by the HUD. */
	static void DrawBanners(UWorld* World, UCanvas* Canvas);

private:
	struct FBurst
	{
		float LaunchAt = 0.0f;
		FVector Center = FVector::ZeroVector;
		FLinearColor Colour = FLinearColor::White;
		TArray<FVector> Velocities;
		bool bPopped = false;
		UInstancedStaticMeshComponent* Sparks = nullptr;
		UInstancedStaticMeshComponent* Rocket = nullptr;
		UPointLightComponent* Flash = nullptr;
	};
	TArray<FBurst> Bursts;
	float Age = 0.0f;

	void Pop(FBurst& Burst);
};
