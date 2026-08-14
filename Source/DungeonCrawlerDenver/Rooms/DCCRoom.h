#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "Rooms/DCCRoomTypes.h"
#include "DCCRoom.generated.h"

class UBoxComponent;
class ADCCEncounter;
class ADCCCharacterBase;
class ADCCRoom;

DECLARE_DYNAMIC_MULTICAST_DELEGATE_TwoParams(FDCCRoomOccupancyChanged, ADCCRoom*, Room, int32, OccupantCount);

/**
 * One node of a floor.
 *
 * A room owns its bounds, knows what type it is, tracks who is inside it, and
 * optionally owns an encounter that arms the first time a player walks in.
 *
 * Occupancy is tracked server-side and deliberately exposed, because it is the
 * hook two later systems both need: the instanced-pocket merge (clients get
 * merged into a shared scene when two characters occupy the same node) and
 * session gating (only Safe Rooms permit async login). Neither is implemented
 * here - this is the data they will read.
 */
UCLASS()
class DUNGEONCRAWLERDENVER_API ADCCRoom : public AActor
{
	GENERATED_BODY()

public:
	ADCCRoom();

	virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;

	/** Stable identifier for this node, used by floor layout and save data. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Room")
	FName RoomId;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Room")
	EDCCRoomType RoomType = EDCCRoomType::Combat;

	/** The fight this room arms on entry. Optional; non-combat rooms leave it unset. */
	UPROPERTY(EditInstanceOnly, BlueprintReadOnly, Category = "Room")
	TObjectPtr<ADCCEncounter> Encounter;

	/** If false, the encounter only starts when something calls TriggerEncounter. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Room")
	bool bStartEncounterOnEntry = true;

	UPROPERTY(BlueprintAssignable, Category = "Room")
	FDCCRoomOccupancyChanged OnOccupancyChanged;

	/**
	 * Whether this node permits solo async login outside a scheduled session.
	 *
	 * Derived from room type rather than authored, because the design doc makes
	 * this a server-enforced rule and not a per-room decision someone can get
	 * wrong while laying out a floor.
	 */
	UFUNCTION(BlueprintPure, Category = "Room")
	bool AllowsAsyncLogin() const { return RoomType == EDCCRoomType::SafeRoom; }

	UFUNCTION(BlueprintPure, Category = "Room")
	int32 GetOccupantCount() const { return Occupants.Num(); }

	UFUNCTION(BlueprintPure, Category = "Room")
	TArray<ADCCCharacterBase*> GetOccupants() const;

	UFUNCTION(BlueprintPure, Category = "Room")
	bool IsCleared() const { return bCleared; }

	/** Server only. Arms the encounter if there is one and it has not run yet. */
	UFUNCTION(BlueprintCallable, Category = "Room")
	void TriggerEncounter();

protected:
	virtual void BeginPlay() override;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Room")
	TObjectPtr<UBoxComponent> Bounds;

	/** True once the room's encounter is cleared, or immediately if it has none. */
	UPROPERTY(ReplicatedUsing = OnRep_Cleared, BlueprintReadOnly, Category = "Room")
	bool bCleared = false;

	UFUNCTION()
	void OnRep_Cleared();

	UFUNCTION()
	void HandleBeginOverlap(
		UPrimitiveComponent* OverlappedComponent,
		AActor* OtherActor,
		UPrimitiveComponent* OtherComp,
		int32 OtherBodyIndex,
		bool bFromSweep,
		const FHitResult& SweepResult);

	UFUNCTION()
	void HandleEndOverlap(
		UPrimitiveComponent* OverlappedComponent,
		AActor* OtherActor,
		UPrimitiveComponent* OtherComp,
		int32 OtherBodyIndex);

	UFUNCTION()
	void HandleEncounterStateChanged(EDCCEncounterState NewState);

	/** Shared entry/exit logic, used by both the overlap events and the reconcile. */
	void AddOccupant(ADCCCharacterBase* Character);
	void RemoveOccupant(ADCCCharacterBase* Character);

	/**
	 * Re-derives occupancy from the box's actual overlap set.
	 *
	 * Overlap events alone are NOT sufficient, which cost a full debugging pass
	 * to learn: under World Partition this room streams in *after* the player
	 * pawn already exists, so a player standing inside generates no begin-overlap
	 * the room is around to hear, and a one-shot sweep in BeginPlay runs before
	 * the box's overlap set is populated. Teleports and level transitions have
	 * the same shape.
	 *
	 * Occupancy gates encounter triggering and safe-room session gating, so it is
	 * reconciled from ground truth on an interval rather than trusted to arrive
	 * as a well-timed event. The events are kept purely for instant response.
	 */
	void ReconcileOccupancy();

	UPROPERTY(EditDefaultsOnly, Category = "Room", meta = (ClampMin = "0.05"))
	float OccupancyReconcileInterval = 0.25f;

	UFUNCTION(BlueprintImplementableEvent, Category = "Room", meta = (DisplayName = "On Room Cleared"))
	void BP_OnRoomCleared();

	UFUNCTION(BlueprintImplementableEvent, Category = "Room", meta = (DisplayName = "On Player Entered"))
	void BP_OnPlayerEntered(ADCCCharacterBase* Character);

	/** Server-side truth. Clients only ever need the count and cleared flag. */
	UPROPERTY(Transient, BlueprintReadOnly, Category = "Room")
	TArray<TObjectPtr<ADCCCharacterBase>> Occupants;

	UPROPERTY(Transient, BlueprintReadOnly, Category = "Room")
	bool bEncounterTriggered = false;

private:
	FTimerHandle OccupancyTimerHandle;
};
