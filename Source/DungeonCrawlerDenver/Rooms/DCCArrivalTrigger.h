#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "DCCArrivalTrigger.generated.h"

class UBoxComponent;
class ADCCCharacterBase;

/**
 * "They were not here when you went in."
 *
 * A volume that hides a set of actors until a crawler walks through it, then
 * reveals them for good. Built for the Front Range shaft head, where the
 * corporations and their barricade have to be absent on the way IN and present
 * on the way OUT - the timing is the entire joke, since it means they turned up
 * the instant something was found, which means they were watching the hole,
 * which means the drones inside were theirs.
 *
 * WHY A TRIGGER RATHER THAN SPAWNING
 * Revealing pre-placed actors keeps the whole confrontation authorable in the
 * editor - meshes, dialogue tables, signatory wiring, exact positions - instead
 * of being assembled at runtime by code nobody can see the results of until
 * PIE. It is the same reasoning ADCCEncounter uses hand-placed waves rather
 * than procedural ones. It also means an EditInstanceOnly pointer like
 * ADCCDialogueNPC_Signatory::ComplianceDoor still works, which it could not on
 * a spawned actor.
 *
 * Deliberately one-way and permanent: there is no "and then they leave again".
 */
UCLASS(Blueprintable)
class DUNGEONCRAWLERDENVER_API ADCCArrivalTrigger : public AActor
{
	GENERATED_BODY()

public:
	ADCCArrivalTrigger();

	virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;
	virtual void BeginPlay() override;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Arrival")
	TObjectPtr<UBoxComponent> Trigger;

	/** Hidden and non-colliding until this fires. Authored per placement. */
	UPROPERTY(EditInstanceOnly, Category = "Arrival")
	TArray<TObjectPtr<AActor>> ArrivingActors;

	/**
	 * Optional gate: nothing arrives until this OTHER trigger has already
	 * fired. Set the shaft-head trigger's Prerequisite to a trigger placed
	 * deep inside the mine, and the corporations can only show up once the
	 * party has actually been in - not on the way past.
	 */
	UPROPERTY(EditInstanceOnly, Category = "Arrival")
	TObjectPtr<ADCCArrivalTrigger> Prerequisite;

	/** Announced (System AI voice) when they turn up. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Arrival")
	FText ArrivalAnnouncement;

	UFUNCTION(BlueprintPure, Category = "Arrival")
	bool HasFired() const { return bFired; }

	/** Server only. Idempotent. */
	UFUNCTION(BlueprintCallable, Category = "Arrival")
	void Fire();

protected:
	UPROPERTY(ReplicatedUsing = OnRep_Fired, BlueprintReadOnly, Category = "Arrival")
	bool bFired = false;

	UFUNCTION()
	void OnRep_Fired();

	/** Runs on every machine - visibility and collision only. */
	void ApplyArrivalState();

	UFUNCTION()
	void HandleOverlapBegin(UPrimitiveComponent* OverlappedComponent, AActor* OtherActor,
		UPrimitiveComponent* OtherComp, int32 OtherBodyIndex, bool bFromSweep,
		const FHitResult& SweepResult);
};
