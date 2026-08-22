#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "DCCCrowdCrawler.generated.h"

class USkeletalMeshComponent;
class UTextRenderComponent;
class UAnimationAsset;

// ---------------------------------------------------------------------------
// TIER 2 - THE CROWD
//
// One of the other four million, standing there being alive at you.
//
// This is deliberately NOT a character. It is an AActor with a mesh, an idle
// clip and a name - no stats component, no skill component, no AI controller,
// no health bar, no combat, no interaction. It cannot be fought, talked to,
// looted or killed, and it will never appear in any of the systems that iterate
// ADCCCharacterBase, which is most of them.
//
// That restraint IS the feature. The population design in
// Docs/population-design.md sets one rule for this tier: the moment a crowd
// member needs a stat, promote it to a real character or cut it. A
// half-simulated crowd costs what a real one costs and delivers what neither
// does. Every field below exists to make a body look distinct from six metres
// away and nothing else.
//
// WHERE THE VARIETY COMES FROM. There are six skeletal meshes in this project
// and there will not soon be sixty, so identity is manufactured from cheap
// per-instance randomisation instead: mesh choice, height, build, skin tint,
// which idle they are playing and at what speed, and a generated name. Twenty
// of these in a bar read as twenty people. The trick is that no two share a
// silhouette AND a tint AND a pose phase.
//
// Cosmetic and client-safe. The seed is replicated so every machine generates
// the same crowd rather than five different ones.
// ---------------------------------------------------------------------------

UCLASS()
class DUNGEONCRAWLERDENVER_API ADCCCrowdCrawler : public AActor
{
	GENERATED_BODY()

public:
	ADCCCrowdCrawler();

	virtual void BeginPlay() override;
	virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Crowd")
	TObjectPtr<USkeletalMeshComponent> Mesh;

	/** Floating name, so a crowd reads as people rather than as props. */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Crowd")
	TObjectPtr<UTextRenderComponent> Nameplate;

	/**
	 * Everything this crowd member might be built from.
	 *
	 * Filled with the shared-skeleton crawler meshes by default. Deliberately a
	 * list rather than one mesh - a room of identical bodies reads worse than an
	 * empty room, because the eye notices repetition faster than absence.
	 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Crowd")
	TArray<TObjectPtr<USkeletalMesh>> MeshPool;

	/** Idle clips. One is chosen per crowd member and looped at a random phase. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Crowd")
	TArray<TObjectPtr<UAnimationAsset>> IdlePool;

	/** Height range. Real crowds vary more than game crowds usually do. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Crowd", meta = (ClampMin = "0.1"))
	float MinScale = 0.88f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Crowd", meta = (ClampMin = "0.1"))
	float MaxScale = 1.12f;

	/**
	 * How far the per-instance colour tint can stray from neutral.
	 *
	 * Applied to a dynamic material instance, so it costs one material per crowd
	 * member and nothing else. Small values only - this is meant to read as
	 * different clothing under the same bad lighting, not as a parade.
	 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Crowd", meta = (ClampMin = "0.0", ClampMax = "1.0"))
	float TintStrength = 0.22f;

	/**
	 * Seed for this crawler's appearance and name.
	 *
	 * Replicated, and everything cosmetic derives from it, so the host and every
	 * client build the same person. Set by the spawner; a hand-placed one gets a
	 * seed from its own location so it is stable across sessions rather than
	 * re-rolling every launch.
	 */
	UPROPERTY(ReplicatedUsing = OnRep_Seed, BlueprintReadOnly, Category = "Crowd")
	int32 Seed = 0;

	UFUNCTION(BlueprintCallable, Category = "Crowd")
	void SetSeed(int32 NewSeed);

protected:
	UFUNCTION()
	void OnRep_Seed();

	/** Builds the whole appearance from Seed. Safe to call on any machine. */
	void ApplyAppearance();

	/** Two fragments and an optional suffix, picked from the seed. */
	FText GenerateName(FRandomStream& Stream) const;
};
