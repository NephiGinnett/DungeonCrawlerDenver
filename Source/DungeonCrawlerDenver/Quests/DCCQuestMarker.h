#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "DCCQuestMarker.generated.h"

class USphereComponent;
class UStaticMeshComponent;
class UTextRenderComponent;

/**
 * A goal, made visible in the world.
 *
 * Three parts, because "where do I go" and "what do I do there" are different
 * questions: a beacon you can see across a room, floating words telling you what
 * it wants, and a trigger that reports arrival for ReachLocation objectives.
 *
 * It shows itself only while an objective with its tag is actually outstanding,
 * so a level can be pre-populated with every marker a floor will ever need and
 * they light up in sequence rather than all at once. That is the whole reason
 * the tag indirection exists instead of markers referencing quests directly.
 */
UCLASS()
class DUNGEONCRAWLERDENVER_API ADCCQuestMarker : public AActor
{
	GENERATED_BODY()

public:
	ADCCQuestMarker();

	virtual void Tick(float DeltaSeconds) override;
	virtual void BeginPlay() override;

	/** Matches FDCCQuestObjective::TargetId. Nothing happens without one. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Quest Marker")
	FName ObjectiveTag;

	/**
	 * The words shown floating at the marker.
	 *
	 * Separate from the objective's own description because the tracker and the
	 * world want different lengths - "Kill 5 interns" reads fine in a list and
	 * badly on a signpost, where "HOLDING PEN" is what you want.
	 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Quest Marker")
	FText MarkerLabel;

	/** Fires the ReachLocation objective when a crawler steps inside. */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Quest Marker")
	TObjectPtr<USphereComponent> Trigger;

	/** The visible pillar. Tall and thin so it clears furniture. */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Quest Marker")
	TObjectPtr<UStaticMeshComponent> Beacon;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Quest Marker")
	TObjectPtr<UTextRenderComponent> Label;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Quest Marker", meta = (ClampMin = "50.0"))
	float TriggerRadius = 250.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Quest Marker")
	FLinearColor MarkerColour = FLinearColor(1.0f, 0.78f, 0.25f);

	/**
	 * Whether this marker disappears once its objective is met.
	 *
	 * On by default. A marker that outlives its purpose is worse than no marker,
	 * because the party keeps walking back to it.
	 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Quest Marker")
	bool bHideWhenComplete = true;

	/** World location the HUD should point at for this marker. */
	UFUNCTION(BlueprintPure, Category = "Quest Marker")
	FVector GetGoalLocation() const;

	UFUNCTION(BlueprintPure, Category = "Quest Marker")
	bool IsMarkerActive() const { return bActive; }

protected:
	UFUNCTION()
	void HandleTriggerOverlap(UPrimitiveComponent* OverlappedComponent, AActor* OtherActor,
		UPrimitiveComponent* OtherComp, int32 OtherBodyIndex, bool bFromSweep, const FHitResult& Sweep);

	/** Re-reads the quest log and shows or hides accordingly. */
	void RefreshActiveState();

private:
	bool bActive = false;

	/**
	 * Seconds until the next quest-log check.
	 *
	 * Polled rather than subscribed because markers stream in and out with World
	 * Partition, and a subscription list on the quest component would need
	 * unwinding every time one unloads. A quarter-second poll on a handful of
	 * actors is cheaper than getting that lifetime management wrong.
	 */
	float TimeUntilRefresh = 0.0f;

	/** Accumulated bob phase, so the beacon reads as interactive rather than as scenery. */
	float BobPhase = 0.0f;

	float BeaconBaseZ = 0.0f;
};
