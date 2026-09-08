#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "DCCGauntletEncounter.generated.h"

class ADCCEnemyCharacter;
class ADCCCharacterBase;
class UBoxComponent;

UENUM(BlueprintType)
enum class EDCCGauntletState : uint8
{
	Dormant  UMETA(DisplayName = "Dormant"),
	Active   UMETA(DisplayName = "Active"),
	Victory  UMETA(DisplayName = "Victory")
};

DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FDCCGauntletStateChanged, EDCCGauntletState, NewState);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FDCCGauntletKillCountChanged, int32, NewKillCount);

/**
 * A Smash-Bros-style "100 man melee": a trickle of enemies drawn from EnemyPool
 * keeps MaxConcurrentEnemies alive on the field at all times, each replaced the
 * moment it dies, until KillCount reaches TargetKillCount.
 *
 * Deliberately NOT an ADCCEncounter subclass. Waves/EDCCWaveTrigger model a
 * fixed, staged fight - spawn a group, wait for it to clear, spawn the next -
 * which has no notion of "keep exactly N alive, replacing each one as it falls,
 * until a running total is reached." Expressing that on Waves would mean one
 * wave per kill, which is not what the wave system is for. Everything else
 * about it follows the same conventions as ADCCEncounter on purpose: server-
 * authoritative spawning, a replicated state enum clients read rather than
 * infer, and the same SpawnCollisionHandlingOverride so a hand-placed spawn
 * point that clips geometry nudges the enemy out instead of silently eating it.
 */
UCLASS()
class DUNGEONCRAWLERDENVER_API ADCCGauntletEncounter : public AActor
{
	GENERATED_BODY()

public:
	ADCCGauntletEncounter();

	virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;
	virtual void BeginPlay() override;
	virtual void Tick(float DeltaSeconds) override;

	/** Any crawler entering while Dormant starts the gauntlet. */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Gauntlet")
	TObjectPtr<UBoxComponent> StartVolume;

	/** Enemy classes to draw from. Picked at random per spawn, so the field stays mixed. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Gauntlet")
	TArray<TSubclassOf<ADCCEnemyCharacter>> EnemyPool;

	/**
	 * Where new enemies appear, relative to this actor. Picked at random per
	 * spawn. Empty is valid - everything spawns at the actor's own location,
	 * nudged apart by SpawnCollisionHandlingOverride.
	 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Gauntlet")
	TArray<FVector> SpawnPoints;

	/** How many kills clears the gauntlet. 100 for the "100 man melee." */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Gauntlet", meta = (ClampMin = "1"))
	int32 TargetKillCount = 100;

	/** How many enemies are kept alive on the field at once. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Gauntlet", meta = (ClampMin = "1"))
	int32 MaxConcurrentEnemies = 3;

	/** Level given to every spawned enemy. Flat on purpose - see the .cpp on why this never scales with kill count. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Gauntlet", meta = (ClampMin = "1"))
	int32 SpawnLevel = 1;

	/** Minimum gap between two spawns, so a fast multi-kill doesn't dump several enemies on the same frame. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Gauntlet", meta = (ClampMin = "0.0"))
	float MinSpawnGapSeconds = 0.75f;

	/** Flat gold paid to every living crawler on Victory. See AwardVictory for why this pays gold, not XP. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Gauntlet", meta = (ClampMin = "0"))
	int32 VictoryGoldReward = 500;

	UPROPERTY(BlueprintAssignable, Category = "Gauntlet")
	FDCCGauntletStateChanged OnGauntletStateChanged;

	/** Fires on every kill, server and every client alike (mirrored the same way OnRep_State is) - the hook ADCCGauntletWeaponStash listens on to decide when to upgrade. */
	UPROPERTY(BlueprintAssignable, Category = "Gauntlet")
	FDCCGauntletKillCountChanged OnKillCountChanged;

	UFUNCTION(BlueprintPure, Category = "Gauntlet")
	EDCCGauntletState GetState() const { return State; }

	UFUNCTION(BlueprintPure, Category = "Gauntlet")
	int32 GetKillCount() const { return KillCount; }

	/** Server only. Begins the trickle-spawn. Does nothing once already started. */
	UFUNCTION(BlueprintCallable, Category = "Gauntlet")
	void StartGauntlet();

	/** Server only. Removes every living spawn, zeroes KillCount, returns to Dormant. */
	UFUNCTION(BlueprintCallable, Category = "Gauntlet")
	void ResetGauntlet();

	/**
	 * The gauntlet nearest to Location, of any state. Used by the admin panel
	 * so a DM can force-start or reset one without having to physically be
	 * standing in its trigger volume.
	 */
	UFUNCTION(BlueprintCallable, Category = "Gauntlet", meta = (WorldContext = "WorldContext"))
	static ADCCGauntletEncounter* FindNearest(const UObject* WorldContext, FVector Location);

protected:
	UFUNCTION()
	void HandleStartVolumeOverlap(UPrimitiveComponent* OverlappedComponent, AActor* OtherActor,
		UPrimitiveComponent* OtherComp, int32 OtherBodyIndex, bool bFromSweep, const FHitResult& SweepResult);

	UFUNCTION()
	void HandleEnemyDied(ADCCCharacterBase* Character);

	UPROPERTY(ReplicatedUsing = OnRep_State, BlueprintReadOnly, Category = "Gauntlet")
	EDCCGauntletState State = EDCCGauntletState::Dormant;

	UPROPERTY(ReplicatedUsing = OnRep_KillCount, BlueprintReadOnly, Category = "Gauntlet")
	int32 KillCount = 0;

	UFUNCTION()
	void OnRep_State();

	UFUNCTION()
	void OnRep_KillCount();

private:
	void SpawnOne();
	void SetState(EDCCGauntletState NewState);

	/** Pays out and announces on the kill that reaches TargetKillCount. */
	void AwardVictory();

	/** Server-side only. Clients never need the roster, just KillCount/State. */
	UPROPERTY(Transient)
	TArray<TObjectPtr<ADCCEnemyCharacter>> LivingSpawns;

	float LastSpawnTime = 0.0f;
};
