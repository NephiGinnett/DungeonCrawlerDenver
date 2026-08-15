#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "Rooms/DCCRoomTypes.h"
#include "Stats/DCCAbilityTypes.h"
#include "DCCEncounter.generated.h"

class ADCCCharacterBase;
class ADCCEnemyCharacter;
class UDCCStatsComponent;

/**
 * One enemy to place when the encounter starts.
 *
 * Positions are relative to the encounter actor so a whole encounter can be
 * dragged around the level, or reused on another floor, without re-authoring
 * every spawn point.
 */
USTRUCT(BlueprintType)
struct FDCCEnemySpawn
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Encounter")
	TSubclassOf<ADCCEnemyCharacter> EnemyClass;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Encounter")
	FVector RelativeLocation = FVector::ZeroVector;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Encounter", meta = (ClampMin = "1"))
	int32 Level = 1;

	/** Optional per-spawn stat override. Leave at defaults to use the class's. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Encounter")
	bool bOverrideAbilityScores = false;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Encounter", meta = (EditCondition = "bOverrideAbilityScores"))
	FDCCAbilityScoreSet AbilityScores;
};

DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FDCCEncounterStateChanged, EDCCEncounterState, NewState);

/**
 * A single fight: a set of enemies, spawned on demand, tracked until none are
 * left standing.
 *
 * Server-authoritative in the strong sense - enemies only ever exist on the
 * server and arrive on clients by replication, so a client cannot spawn, see
 * early, or resolve anything about the encounter. Clients learn the state
 * through the replicated State value.
 */
UCLASS()
class DUNGEONCRAWLERDENVER_API ADCCEncounter : public AActor
{
	GENERATED_BODY()

public:
	ADCCEncounter();

	virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Encounter")
	TArray<FDCCEnemySpawn> Enemies;

	/** Fires on server and on every client that receives the change. */
	UPROPERTY(BlueprintAssignable, Category = "Encounter")
	FDCCEncounterStateChanged OnEncounterStateChanged;

	UFUNCTION(BlueprintPure, Category = "Encounter")
	EDCCEncounterState GetState() const { return State; }

	UFUNCTION(BlueprintPure, Category = "Encounter")
	int32 GetLivingEnemyCount() const;

	/** Server only. Spawns the enemy set. Does nothing if already started. */
	UFUNCTION(BlueprintCallable, Category = "Encounter")
	void StartEncounter();

	/** Server only. Removes every spawned enemy and returns to Dormant. Used by the DM client. */
	UFUNCTION(BlueprintCallable, Category = "Encounter")
	void ResetEncounter();

protected:
	UPROPERTY(ReplicatedUsing = OnRep_State, BlueprintReadOnly, Category = "Encounter")
	EDCCEncounterState State = EDCCEncounterState::Dormant;

	UFUNCTION()
	void OnRep_State();

	UFUNCTION()
	void HandleEnemyDied(ADCCCharacterBase* Character);

	UFUNCTION(BlueprintImplementableEvent, Category = "Encounter", meta = (DisplayName = "On Encounter Cleared"))
	void BP_OnEncounterCleared();

private:
	void SetState(EDCCEncounterState NewState);

	/**
	 * Server only. Rolls Skill Advancement for every surviving crawler.
	 *
	 * Runs when the encounter is cleared, which is this game's translation of the
	 * published rule's "end of each floor" advancement interval.
	 */
	void AwardSkillAdvancement();

	/**
	 * Pays every surviving crawler for the clear, then checks whether that was the
	 * last encounter on the floor and pays the floor bonus too.
	 */
	void AwardClearExperience();

	/** True when no encounter anywhere in the level is still unfinished. */
	bool IsFloorCleared() const;

public:
	/**
	 * Which floor this encounter sits on. Drives the floor-clear bonus.
	 *
	 * Authored rather than derived because floors are not modelled as objects yet
	 * - there is one level containing everything. When they are, this reads from
	 * the floor instead and nothing else changes.
	 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Encounter", meta = (ClampMin = "1"))
	int32 FloorNumber = 1;

	/** Average level of what this encounter spawned. Used for the clear payout. */
	UPROPERTY(BlueprintReadOnly, Category = "Encounter")
	int32 LastSpawnedAverageLevel = 1;

	UPROPERTY(BlueprintReadOnly, Category = "Encounter")
	int32 LastSpawnedCount = 0;

protected:

	/** Server-side only. Clients never need the roster, just the state. */
	UPROPERTY(Transient)
	TArray<TObjectPtr<ADCCEnemyCharacter>> SpawnedEnemies;
};
