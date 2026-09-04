#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "DCCPuzzleDoor.generated.h"

class UStaticMeshComponent;
class UPointLightComponent;
class ADCCCharacterBase;

// ---------------------------------------------------------------------------
// EVERY LOCK HAS A KEY AND A CROWBAR
//
// The shared base for everything in Puzzles/ that stands between the party and
// somewhere they want to be. Per Docs/next-evolution-plan.md, the rule this
// whole folder is built on:
//
//     Every lock has a key and a crowbar. The crowbar always works. The
//     crowbar is always more expensive.
//
// Nothing here may ever hard-block a five-player table. Four people watching a
// fifth fail a riddle is the fastest way to kill a session, so `ForceOpen` is
// on the BASE class rather than being something each puzzle opts into - a
// subclass has to actively set bAllowForcing = false to remove it, and should
// have a very good reason.
//
// It is also the thematically correct answer, which is the part worth
// protecting: the System AI does not care whether you comply or don't. It
// cares that it is watchable. A party that crowbars everything is running a
// different show, not failing this one.
// ---------------------------------------------------------------------------

/** How a door came open - forwarded to listeners so a forced entry can read differently. */
UENUM(BlueprintType)
enum class EDCCPuzzleSolveMethod : uint8
{
	/** The intended route: plates weighted, breakers balanced, stamps collected. */
	Solved  UMETA(DisplayName = "Solved"),

	/** The crowbar. Loud, on camera, and paid for in Favor. */
	Forced  UMETA(DisplayName = "Forced"),
};

DECLARE_DYNAMIC_MULTICAST_DELEGATE_TwoParams(FDCCPuzzleDoorOpened,
	ADCCPuzzleDoor*, Door, EDCCPuzzleSolveMethod, Method);

/**
 * A door that opens when its puzzle is satisfied, or when someone loses
 * patience and pays for it.
 *
 * Server-authoritative like everything else mechanical in this project:
 * `bOpen` replicates down, clients never set it. Placed directly as a class
 * instance (`add_to_scene_from_class`) the same way the Items/ pickups are -
 * no Blueprint wrapper needed.
 */
UCLASS(Blueprintable)
class DUNGEONCRAWLERDENVER_API ADCCPuzzleDoor : public AActor
{
	GENERATED_BODY()

public:
	ADCCPuzzleDoor();

	virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;
	virtual void BeginPlay() override;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Puzzle")
	TObjectPtr<UStaticMeshComponent> Mesh;

	/** Red while locked, green once open. Same at-a-glance convention as
	 *  ADCCPortalConsole's indicator and ADCCFloorExit's sign. */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Puzzle")
	TObjectPtr<UPointLightComponent> IndicatorLight;

	/** Shown on the HUD when a crawler looks at this while it is still locked. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Puzzle")
	FText LockedDescription;

	/** Announced (System AI voice) the moment it opens legitimately. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Puzzle")
	FText SolvedAnnouncement;

	/** Announced instead when it is forced. Should read as delighted - per the
	 *  tone bible, unprofessional conduct rates extremely well. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Puzzle")
	FText ForcedAnnouncement;

	/** The crowbar. Turn this off only where being stuck is genuinely the point. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Puzzle|Forcing")
	bool bAllowForcing = true;

	/** Favor cost of forcing. Negative because forcing SPENDS standing. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Puzzle|Forcing")
	int32 ForceFavorCost = -40;

	/** Minimum Strength SCORE (not modifier) to force it - the books use the
	 *  full word for the score, and lifting/breaking reads the score. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Puzzle|Forcing")
	int32 ForceStrengthRequired = 4;

	UPROPERTY(BlueprintAssignable, Category = "Puzzle")
	FDCCPuzzleDoorOpened OnDoorOpened;

	UFUNCTION(BlueprintPure, Category = "Puzzle")
	bool IsOpen() const { return bOpen; }

	/** Server only. The intended solution succeeded. Idempotent. */
	UFUNCTION(BlueprintCallable, Category = "Puzzle")
	void Unlock();

	/**
	 * Server only. The crowbar. Charges Favor, announces it, opens anyway.
	 * Returns false only if forcing is disabled or the crawler is too weak -
	 * never because the puzzle is unsolved, which is the entire point.
	 */
	UFUNCTION(BlueprintCallable, Category = "Puzzle")
	bool ForceOpen(ADCCCharacterBase* Forcer);

	/**
	 * Whether this door's own puzzle is currently satisfied. Base class has no
	 * puzzle of its own and reports false, so a plain ADCCPuzzleDoor is a door
	 * that can only ever be forced - which is a legitimate thing to place.
	 */
	UFUNCTION(BlueprintPure, Category = "Puzzle")
	virtual bool IsPuzzleSatisfied() const { return false; }

	/** Interaction entry point, same shape as ADCCPortalConsole::TryPlayerInteract. */
	virtual void TryPlayerInteract(ADCCCharacterBase* Interactor);

protected:
	UPROPERTY(ReplicatedUsing = OnRep_Open, BlueprintReadOnly, Category = "Puzzle")
	bool bOpen = false;

	UFUNCTION()
	void OnRep_Open();

	/** Runs on every machine - collision and visuals, never game logic. */
	void ApplyOpenState();

	/** Server-side shared tail of Unlock/ForceOpen. */
	void OpenInternal(EDCCPuzzleSolveMethod Method, ADCCCharacterBase* Cause);
};
