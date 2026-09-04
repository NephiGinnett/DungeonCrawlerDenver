#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "DCCPressurePlate.generated.h"

class UBoxComponent;
class UStaticMeshComponent;
class ADCCPuzzleDoor;

/**
 * A plate that cares how much is standing on it.
 *
 * Reads `ADCCPickup::MassKilograms` for props and a flat per-body figure for
 * characters, so the Pantry's already-authored mass tiers finally do something
 * load-bearing instead of being flavour on a carry limit.
 *
 * The interesting placement is a plate that needs more than one crawler's worth
 * of weight: a body on a plate is a body not in the fight, and the door only
 * stays open while the weight stays put. The best answer available on Floor 1
 * is the Strongbox Crate - which means setting down the floor's only 250-gold
 * payday and trusting it is still there afterwards. That tension already
 * exists (see CLAUDE.md on never duplicating the Strongbox); this is what
 * finally spends it.
 */
UCLASS(Blueprintable)
class DUNGEONCRAWLERDENVER_API ADCCPressurePlate : public AActor
{
	GENERATED_BODY()

public:
	ADCCPressurePlate();

	virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;
	virtual void BeginPlay() override;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Puzzle")
	TObjectPtr<UStaticMeshComponent> Mesh;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Puzzle")
	TObjectPtr<UBoxComponent> Trigger;

	/** Total kilograms needed before this plate reads as satisfied. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Puzzle")
	float RequiredMassKilograms = 80.0f;

	/**
	 * What one crawler counts for. Flat rather than derived from stats,
	 * because "how heavy is this person" is not something this project models
	 * anywhere and inventing it here would be a worse kind of precision.
	 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Puzzle")
	float CharacterMassKilograms = 70.0f;

	/** Optional. When every plate sharing this door is satisfied, it unlocks. */
	UPROPERTY(EditInstanceOnly, Category = "Puzzle")
	TObjectPtr<ADCCPuzzleDoor> LinkedDoor;

	UFUNCTION(BlueprintPure, Category = "Puzzle")
	bool IsSatisfied() const { return bSatisfied; }

	UFUNCTION(BlueprintPure, Category = "Puzzle")
	float GetCurrentMass() const { return CurrentMassKilograms; }

protected:
	UPROPERTY(ReplicatedUsing = OnRep_Satisfied, BlueprintReadOnly, Category = "Puzzle")
	bool bSatisfied = false;

	UPROPERTY(Replicated, BlueprintReadOnly, Category = "Puzzle")
	float CurrentMassKilograms = 0.0f;

	UFUNCTION()
	void OnRep_Satisfied();

	UFUNCTION()
	void HandleOverlapBegin(UPrimitiveComponent* OverlappedComponent, AActor* OtherActor,
		UPrimitiveComponent* OtherComp, int32 OtherBodyIndex, bool bFromSweep,
		const FHitResult& SweepResult);

	UFUNCTION()
	void HandleOverlapEnd(UPrimitiveComponent* OverlappedComponent, AActor* OtherActor,
		UPrimitiveComponent* OtherComp, int32 OtherBodyIndex);

	/** Server only. Re-totals from scratch rather than adjusting in place -
	 *  same reasoning as UDCCStatsComponent::RecalculateEnhancedScores, so a
	 *  missed overlap event can never leave a permanent residue. */
	void RecalculateMass();

	/** Visuals on every machine. */
	void ApplySatisfiedVisuals();

	/** Server only. Unlocks LinkedDoor once every plate feeding it is happy. */
	void EvaluateLinkedDoor();
};
