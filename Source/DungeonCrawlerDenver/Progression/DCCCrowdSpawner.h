#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "DCCCrowdSpawner.generated.h"

class ADCCCrowdCrawler;
class UBoxComponent;

/**
 * Fills a volume with crowd members so a room can be populated by placing one
 * actor instead of twenty.
 *
 * Server-only spawning; the crawlers replicate down with their seeds, so every
 * machine sees the same crowd standing in the same places rather than five
 * independently generated ones.
 *
 * Placement is rejection-sampled against a minimum spacing rather than pure
 * random, because uniform random points cluster - and a crowd with three people
 * standing inside each other and a bare patch beside them reads as broken, not
 * as busy.
 */
UCLASS()
class DUNGEONCRAWLERDENVER_API ADCCCrowdSpawner : public AActor
{
	GENERATED_BODY()

public:
	ADCCCrowdSpawner();

	virtual void BeginPlay() override;

	/** The area to fill. Scale the box in the level to size the crowd's footprint. */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Crowd")
	TObjectPtr<UBoxComponent> Bounds;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Crowd")
	TSubclassOf<ADCCCrowdCrawler> CrowdClass;

	/**
	 * How many to place. Twenty reads as a busy bar; sixty is a concourse.
	 *
	 * They cost close to nothing each - no tick, no AI, no stats - but they are
	 * still skeletal meshes, so this is the number to turn down first if a room
	 * ever gets heavy.
	 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Crowd", meta = (ClampMin = "0"))
	int32 CrowdSize = 20;

	/** Minimum gap between two crowd members, in centimetres. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Crowd", meta = (ClampMin = "0.0"))
	float MinSpacing = 130.0f;

	/**
	 * Fixed seed for the whole crowd, so a room's regulars are the same people
	 * every session. Zero derives one from the spawner's position instead.
	 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Crowd")
	int32 CrowdSeed = 0;

	/**
	 * Drops each crawler onto whatever is beneath it.
	 *
	 * On by default: a box placed roughly in a room will otherwise leave people
	 * hovering, and getting every spawner's height exactly right by hand is the
	 * sort of tedium this actor exists to avoid.
	 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Crowd")
	bool bSnapToGround = true;

protected:
	/** Server only. Places CrowdSize crawlers, or as many as will fit. */
	void PopulateCrowd();
};
