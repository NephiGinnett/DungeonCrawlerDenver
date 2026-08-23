#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "DCCOathgate.generated.h"

class UBoxComponent;
class UStaticMeshComponent;
class ADCCCharacterBase;
class ADCCEnemyCharacter;

// ---------------------------------------------------------------------------
// A DOORWAY, NOT A DUNGEON
//
// Rooms/DCCDungeonGenerator builds actual geometry - a grid of cells, corridors,
// room types - and stays deliberately dormant (see CLAUDE.md) because nothing
// asked for a second way to build floors. This is a different, much smaller
// idea: two portals, walk into one, arrive at the other. If the far side has an
// EnemyPool, something is waiting; if it doesn't (an exit gate, say), it's just
// a way back. No geometry generation, no room grid - it reuses ADCCEncounter
// for the fight itself rather than reinventing spawning.
//
// Built primarily as a DM's testing tool (per Zoe's own framing) - unlimited
// use, no cooldown beyond the short re-trigger debounce, so any enemy roster
// can be thrown at a party on demand without placing a real encounter by hand
// each time. Whether it becomes real in-fiction content later is an open
// question; nothing here assumes it will.
// ---------------------------------------------------------------------------

/**
 * One end of a portal pair. Walking through teleports to PairedGate's location,
 * gathering nearby allies along for the trip; if this gate carries an
 * EnemyPool, a fresh encounter is rolled and started at the far end first.
 */
UCLASS(Blueprintable)
class DUNGEONCRAWLERDENVER_API ADCCOathgate : public AActor
{
	GENERATED_BODY()

public:
	ADCCOathgate();

	virtual void BeginPlay() override;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Oathgate")
	TObjectPtr<UBoxComponent> Trigger;

	/** Optional visual marker so the portal is visible, not just discoverable. */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Oathgate")
	TObjectPtr<UStaticMeshComponent> Marker;

	/**
	 * The other end. Walking through THIS gate arrives at THAT one's location.
	 * Symmetric - set both gates' PairedGate to each other, once each.
	 */
	UPROPERTY(EditInstanceOnly, Category = "Oathgate")
	TObjectPtr<ADCCOathgate> PairedGate;

	/**
	 * Rolled fresh on every use. Empty means this side never spawns a fight -
	 * the natural shape for an exit/return gate that only teleports.
	 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Oathgate|Encounter")
	TArray<TSubclassOf<ADCCEnemyCharacter>> EnemyPool;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Oathgate|Encounter", meta = (ClampMin = "0"))
	int32 MinEnemies = 3;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Oathgate|Encounter", meta = (ClampMin = "0"))
	int32 MaxEnemies = 5;

	/** Radius the rolled roster scatters within, centred on PairedGate. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Oathgate|Encounter", meta = (ClampMin = "0.0"))
	float EncounterScatterRadius = 500.0f;

	/** Forwarded onto the spawned ADCCEncounter, for the clear-experience payout. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Oathgate|Encounter", meta = (ClampMin = "1"))
	int32 SpawnLevel = 2;

	/** How far to look for nearby friendly characters to bring along. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Oathgate", meta = (ClampMin = "0.0"))
	float PartyGatherRadius = 800.0f;

	/** Arrival spread at the far end, so a whole party does not land on one point. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Oathgate", meta = (ClampMin = "0.0"))
	float ArrivalSpreadRadius = 200.0f;

	/**
	 * Seconds before THIS gate can fire again.
	 *
	 * Not a real cooldown in the "come back later" sense - just long enough that
	 * a party mid-teleport, still standing in the trigger volume for a moment,
	 * cannot double-trigger it before they have physically left.
	 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Oathgate", meta = (ClampMin = "0.0"))
	float ReuseDelaySeconds = 3.0f;

	/**
	 * Quest tag fired (EDCCObjectiveType::ReachLocation) the first time any
	 * crawler steps through either gate of the pair. Both ends default to the
	 * same tag so either counts.
	 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Oathgate")
	FName QuestObjectiveTag = TEXT("Oathgate");

protected:
	UFUNCTION()
	void HandleTriggerOverlap(UPrimitiveComponent* OverlappedComponent, AActor* OtherActor,
		UPrimitiveComponent* OtherComp, int32 OtherBodyIndex, bool bFromSweep, const FHitResult& SweepResult);

	/** Server only. Gathers nearby allies, rolls a fight if this side has one, and teleports everyone. */
	void ActivateFor(ADCCCharacterBase* Requester);

	/** Server only. Rolls EnemyPool into a fresh ADCCEncounter near Origin and starts it. */
	void SpawnArenaEncounter(const FVector& Origin) const;

private:
	float NextUsableTime = 0.0f;
};
