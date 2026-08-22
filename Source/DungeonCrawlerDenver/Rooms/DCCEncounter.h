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

	/**
	 * Marks this spawn as the encounter's boss.
	 *
	 * Two consequences: HealthThreshold waves watch its health, and the encounter
	 * will not clear while it lives even if every minion is dead.
	 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Encounter")
	bool bIsBoss = false;
};

/** What makes a wave arrive. */
UENUM(BlueprintType)
enum class EDCCWaveTrigger : uint8
{
	/** Spawns with the encounter. Wave 0 is almost always this. */
	Immediate       UMETA(DisplayName = "Immediate"),

	/** Spawns once everything already on the field is dead. */
	PreviousCleared UMETA(DisplayName = "Previous Wave Cleared"),

	/** Spawns a fixed time after the previous wave did. */
	Delay           UMETA(DisplayName = "Delay After Previous"),

	/**
	 * Spawns when a spawn flagged bIsBoss drops below a health fraction.
	 *
	 * This is Voldemortgage's Late Notice: five more Debt Eaters the moment he
	 * hits half, which is what turns the back half of the fight into a race.
	 */
	HealthThreshold UMETA(DisplayName = "Boss Health Threshold"),

	/** Never automatic. Blueprint or the DM client calls SpawnWave. */
	Manual          UMETA(DisplayName = "Manual")
};

/**
 * One group of enemies arriving together.
 *
 * Waves exist because a fight that spawns everything at once has no shape. The
 * Kitchens' ticket rail and the whole boss encounter are both built on the gap
 * between "these are here now" and "more are coming", and neither is expressible
 * with a single spawn list.
 */
USTRUCT(BlueprintType)
struct FDCCEncounterWave
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Wave")
	TArray<FDCCEnemySpawn> Enemies;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Wave")
	EDCCWaveTrigger Trigger = EDCCWaveTrigger::PreviousCleared;

	/** Used by Delay. Also a minimum gap for every other trigger. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Wave", meta = (ClampMin = "0.0"))
	float DelaySeconds = 0.0f;

	/** Used by HealthThreshold. 0.5 means "at half health". */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Wave", meta = (ClampMin = "0.0", ClampMax = "1.0"))
	float HealthThresholdFraction = 0.5f;

	/**
	 * Announced to the feed when the wave lands.
	 *
	 * Optional, but this is where a fight gets its voice - the ticket rail
	 * printing, or Voldemortgage straightening his tie.
	 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Wave")
	FText Announcement;
};

DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FDCCEncounterStateChanged, EDCCEncounterState, NewState);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FDCCWaveSpawned, int32, WaveIndex);

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

	/** Server only, and only while a fight is live. Drives wave triggers. */
	virtual void Tick(float DeltaSeconds) override;

	/**
	 * The single-wave spawn list.
	 *
	 * Kept for the encounters already placed in the level, and because most fights
	 * genuinely are one group. When Waves is empty this is treated as wave 0, so
	 * nothing authored before waves existed had to change.
	 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Encounter")
	TArray<FDCCEnemySpawn> Enemies;

	/** Staged waves. Overrides Enemies entirely when non-empty. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Encounter")
	TArray<FDCCEncounterWave> Waves;

	/** Fires on server and on every client that receives the change. */
	UPROPERTY(BlueprintAssignable, Category = "Encounter")
	FDCCEncounterStateChanged OnEncounterStateChanged;

	UPROPERTY(BlueprintAssignable, Category = "Encounter")
	FDCCWaveSpawned OnWaveSpawned;

	/** Server only. Spawns a specific wave now, regardless of its trigger. */
	UFUNCTION(BlueprintCallable, Category = "Encounter")
	void SpawnWave(int32 WaveIndex);

	UFUNCTION(BlueprintPure, Category = "Encounter")
	int32 GetWavesSpawned() const { return NextWaveIndex; }

	UFUNCTION(BlueprintPure, Category = "Encounter")
	int32 GetWaveCount() const { return Waves.Num() > 0 ? Waves.Num() : 1; }

	/** The living boss, if this encounter has one and it is still up. */
	UFUNCTION(BlueprintPure, Category = "Encounter")
	ADCCEnemyCharacter* GetBoss() const;

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

	/**
	 * Server only. Folds an enemy spawned OUTSIDE SpawnWave (a boss's charge
	 * minions, say) into this encounter's own roster - the death hookup and the
	 * SpawnedEnemies membership SpawnWave itself gives every wave-authored
	 * enemy for free.
	 *
	 * Exists because UDCCBossComponent::SpawnChargeMinion used to spawn
	 * Debt Eaters with no Owner and no registration at all: they were real,
	 * damage-dealing characters that this encounter's GetLivingEnemyCount()
	 * could never see, which silently broke both ChargeDamagePerMinion (counts
	 * living minions when the charge lands) and Repossession Nip's heal-the-boss
	 * lookup (Cast<ADCCEncounter>(GetOwner()), which needs Owner set).
	 */
	UFUNCTION(BlueprintCallable, Category = "Encounter")
	void RegisterSpawnedEnemy(ADCCEnemyCharacter* Enemy);

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

	/** The boss spawn, if one was flagged. Server-side. */
	UPROPERTY(Transient)
	TObjectPtr<ADCCEnemyCharacter> Boss;

private:
	/** Reads Waves, or synthesises one wave from Enemies when Waves is empty. */
	const TArray<FDCCEnemySpawn>& GetWaveSpawns(int32 WaveIndex, TArray<FDCCEnemySpawn>& Scratch) const;

	/** Server only, ticked. Decides whether the next wave's trigger is satisfied. */
	void EvaluateNextWave();

	/** How many waves have been spawned so far. */
	int32 NextWaveIndex = 0;

	/** World seconds the last wave landed, for Delay triggers. */
	float LastWaveTime = 0.0f;
};
