#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "DCCFloorExit.generated.h"

class UBoxComponent;
class UStaticMeshComponent;
class UTextRenderComponent;
class ADCCCharacterBase;

// ---------------------------------------------------------------------------
// THE WAY OUT
//
// In DCC the real problem on a floor is not the monsters, it is finding the
// stairs. This is that, with the lock the design doc asks for: it does not open
// until the floor boss is dead.
//
// HOW IT KNOWS THE BOSS IS DEAD
//
// It watches for living actors carrying a UDCCBossComponent, rather than holding
// a reference to one. That is deliberate:
//
//   - The boss does not exist at BeginPlay. Voldemortgage arrives as an encounter
//     wave, so anything bound at startup would bind to nothing.
//   - A hand-wired reference is one more thing to forget when a floor is built,
//     and it fails silently when forgotten - the exit would simply never open and
//     nobody would know why.
//
// So it polls on the server: once a boss has been SEEN, and no living boss
// remains, the floor is clear. Locked before the boss spawns, locked while it
// lives, open once it does not.
// ---------------------------------------------------------------------------

UCLASS()
class DUNGEONCRAWLERDENVER_API ADCCFloorExit : public AActor
{
	GENERATED_BODY()

public:
	ADCCFloorExit();

	virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Exit")
	TObjectPtr<UStaticMeshComponent> Frame;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Exit")
	TObjectPtr<UBoxComponent> Trigger;

	/** Reads LOCKED / OPEN above the frame, so its state is legible across the room. */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Exit")
	TObjectPtr<UTextRenderComponent> Sign;

	/**
	 * Set false for a tutorial or test floor that should simply let people
	 * through. The test exit placed for the Voldemortgage fight leaves this true.
	 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Exit")
	bool bRequiresBossDefeat = true;

	/** Which floor this leads off. Announced, and used by the floor-clear award. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Exit", meta = (ClampMin = "1"))
	int32 FloorNumber = 1;

	/**
	 * Fraction of the surviving dungeon population that dies on this floor.
	 *
	 * Applied once, when the first crawler leaves. 18% of four million is roughly
	 * 750,000 people, which is the sort of number the System AI should be able to
	 * report without changing its tone at all.
	 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "DCC|Floor", meta = (ClampMin = "0.0", ClampMax = "0.99"))
	float FloorAttritionFraction = 0.18f;

	UPROPERTY(BlueprintReadOnly, ReplicatedUsing = OnRep_Locked, Category = "Exit")
	bool bLocked = true;

	UFUNCTION(BlueprintPure, Category = "Exit")
	bool IsLocked() const { return bLocked; }

protected:
	virtual void BeginPlay() override;

	UFUNCTION()
	void OnRep_Locked();

	UFUNCTION()
	void HandleTriggerBeginOverlap(
		UPrimitiveComponent* OverlappedComponent,
		AActor* OtherActor,
		UPrimitiveComponent* OtherComponent,
		int32 OtherBodyIndex,
		bool bFromSweep,
		const FHitResult& Sweep);

	/** Server only. Polls for a living boss and unlocks once none remain. */
	void CheckBossState();

	/** Applies sign text and colour for the current bLocked. Runs everywhere. */
	void ApplyLockedVisuals();

	/**
	 * Server only. Banks whatever the crawler is carrying that was never banked
	 * on pickup - the Strongbox and anything like it - then announces the exit.
	 */
	void UseExit(ADCCCharacterBase* Character);

	FTimerHandle BossWatchTimer;

	/**
	 * Latched so that "no boss in the world" cannot be mistaken for victory
	 * before the encounter has spawned one.
	 */
	bool bBossHasAppeared = false;

	/** Guards the announcement so five crawlers walking out do not print five times. */
	bool bFloorCompleted = false;
};
