#pragma once

#include "CoreMinimal.h"
#include "Puzzles/DCCPuzzleDoor.h"
#include "Rooms/DCCRoomTypes.h"
#include "DCCEscrowLock.generated.h"

class ADCCPickup;
class ADCCRoom;

/**
 * A door that takes collateral.
 *
 * Surrender something you are carrying and it opens. Clear the room it guards
 * and you get it back; leave without clearing, or die trying, and you do not.
 *
 * The fourth of the Puzzles/ primitives from Docs/next-evolution-plan.md, and
 * the cheapest of them to build because the loot and carry plumbing already
 * existed - what it adds is not a mechanic so much as a *sentence*: the dungeon
 * takes a deposit. On a floor whose whole theme is repossession, that is the
 * theme stated as a rule rather than as set dressing.
 *
 * The deposited prop is not destroyed. It is hidden and held, then either put
 * back where the depositor stood or - on failure - left inside the lock for
 * whoever comes next, which is a quieter and better outcome than deleting a
 * player's belongings.
 */
UCLASS(Blueprintable)
class DUNGEONCRAWLERDENVER_API ADCCEscrowLock : public ADCCPuzzleDoor
{
	GENERATED_BODY()

public:
	ADCCEscrowLock();

	virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;

	/**
	 * Optional. Empty means any carried prop is acceptable collateral; set it
	 * and only that pickup's EquipmentItemId will do.
	 *
	 * Leaving it empty is the more interesting authoring in almost every case,
	 * because then the party chooses what they can afford to lose - which is
	 * the actual decision this door exists to pose.
	 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Escrow")
	FName RequiredItemId;

	/**
	 * The room whose clearance redeems the deposit. When it reports cleared,
	 * the held item is returned. Unset means the deposit is never returned
	 * automatically and only ReleaseDeposit() will do it - valid, and worth
	 * saying out loud in the door's own LockedDescription if so.
	 */
	UPROPERTY(EditInstanceOnly, Category = "Escrow")
	TObjectPtr<ADCCRoom> RedeemOnRoomCleared;

	/** What is currently being held, if anything. */
	UPROPERTY(Replicated, BlueprintReadOnly, Category = "Escrow")
	TObjectPtr<ADCCPickup> HeldItem;

	UFUNCTION(BlueprintPure, Category = "Escrow")
	bool IsHoldingDeposit() const { return HeldItem != nullptr; }

	/** Server only. Puts the deposit back into the world beside this lock. */
	UFUNCTION(BlueprintCallable, Category = "Escrow")
	void ReleaseDeposit();

	virtual bool IsPuzzleSatisfied() const override { return HeldItem != nullptr; }

	virtual void TryPlayerInteract(ADCCCharacterBase* Interactor) override;

protected:
	virtual void BeginPlay() override;

	/** Bound to the redeeming room's own encounter - ADCCRoom has no native
	 *  cleared delegate, only a BlueprintImplementableEvent, so the encounter's
	 *  OnEncounterStateChanged is the real signal. */
	UFUNCTION()
	void HandleEncounterStateChanged(EDCCEncounterState NewState);

	/** Server only. Takes the prop out of the depositor's hands. */
	bool AcceptDeposit(ADCCCharacterBase* Depositor);
};
