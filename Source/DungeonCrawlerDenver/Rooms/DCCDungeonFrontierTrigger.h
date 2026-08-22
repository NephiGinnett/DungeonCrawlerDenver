#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "Rooms/DCCRoomTypes.h"
#include "DCCDungeonFrontierTrigger.generated.h"

class UBoxComponent;
class ADCCDungeonGenerator;

/**
 * An unresolved doorway. Everything beyond it does not exist yet.
 *
 * This is what makes generation happen "as they come up" rather than all at
 * once at floor load: a room is only ever a set of open connectors until a
 * player actually walks toward one. The trigger asks its generator to build
 * whatever is on the other side, then removes itself - a resolved connector
 * has no further use for a trigger, and leaving one behind would just be a
 * second way to accidentally re-trigger generation for a cell that already
 * has a room.
 */
UCLASS()
class DUNGEONCRAWLERDENVER_API ADCCDungeonFrontierTrigger : public AActor
{
	GENERATED_BODY()

public:
	ADCCDungeonFrontierTrigger();

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Dungeon")
	TObjectPtr<UBoxComponent> Trigger;

	/** Which generator to ask, which cell to build, and which side it is entered from. */
	UPROPERTY(BlueprintReadOnly, Category = "Dungeon")
	TWeakObjectPtr<ADCCDungeonGenerator> Generator;

	UPROPERTY(BlueprintReadOnly, Category = "Dungeon")
	FIntPoint TargetCell = FIntPoint::ZeroValue;

	UPROPERTY(BlueprintReadOnly, Category = "Dungeon")
	EDCCCardinalDirection EntryDirection = EDCCCardinalDirection::North;

protected:
	virtual void BeginPlay() override;

	UFUNCTION()
	void HandleBeginOverlap(
		UPrimitiveComponent* OverlappedComponent,
		AActor* OtherActor,
		UPrimitiveComponent* OtherComp,
		int32 OtherBodyIndex,
		bool bFromSweep,
		const FHitResult& SweepResult);
};
