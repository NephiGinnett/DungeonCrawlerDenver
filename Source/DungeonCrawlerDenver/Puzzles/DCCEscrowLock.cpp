#include "Puzzles/DCCEscrowLock.h"

#include "Characters/DCCCharacterBase.h"
#include "Items/DCCCarryComponent.h"
#include "Items/DCCPickup.h"
#include "Rooms/DCCRoom.h"
#include "Rooms/DCCEncounter.h"
#include "Engine/World.h"
#include "Net/UnrealNetwork.h"

ADCCEscrowLock::ADCCEscrowLock()
{
	LockedDescription = NSLOCTEXT("DCC", "EscrowLocked",
		"A collection slot, and a notice: ITEMS OF VALUE HELD IN ESCROW PENDING COMPLETION. "
		"Deposit something you are carrying to pass.");

	SolvedAnnouncement = NSLOCTEXT("DCC", "EscrowSolved",
		"Deposit received and held in good faith. Please note that good faith is a legal term "
		"and not a description of anyone's intentions.");

	ForcedAnnouncement = NSLOCTEXT("DCC", "EscrowForced",
		"The escrow slot has been forced open without a deposit. The account remains in arrears "
		"and the arrears have been noted against you personally.");
}

void ADCCEscrowLock::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);
	DOREPLIFETIME(ADCCEscrowLock, HeldItem);
}

void ADCCEscrowLock::BeginPlay()
{
	Super::BeginPlay();

	if (!HasAuthority())
	{
		return;
	}

	// The room itself has no native cleared delegate (only a Blueprint event),
	// so the encounter it owns is what actually reports the change.
	if (RedeemOnRoomCleared && RedeemOnRoomCleared->Encounter)
	{
		RedeemOnRoomCleared->Encounter->OnEncounterStateChanged.AddDynamic(
			this, &ADCCEscrowLock::HandleEncounterStateChanged);
	}
}

void ADCCEscrowLock::HandleEncounterStateChanged(EDCCEncounterState NewState)
{
	if (!HasAuthority() || NewState != EDCCEncounterState::Cleared)
	{
		return;
	}

	ReleaseDeposit();
}

bool ADCCEscrowLock::AcceptDeposit(ADCCCharacterBase* Depositor)
{
	if (!HasAuthority() || !Depositor || HeldItem)
	{
		return false;
	}

	UDCCCarryComponent* Carry = Depositor->FindComponentByClass<UDCCCarryComponent>();
	if (!Carry || !Carry->IsCarrying())
	{
		Depositor->ShowLocalNotice(NSLOCTEXT("DCC", "EscrowNothingCarried",
			"The slot wants something of value. Your hands are empty."));
		return false;
	}

	if (!RequiredItemId.IsNone() && Carry->CarriedPickup
		&& Carry->CarriedPickup->EquipmentItemId != RequiredItemId)
	{
		Depositor->ShowLocalNotice(NSLOCTEXT("DCC", "EscrowWrongItem",
			"The slot rejects it. It is holding out for something specific."));
		return false;
	}

	// ReleaseCarried rather than a normal drop: the prop is about to be hidden
	// rather than left lying on the floor, so waking its physics first would be
	// wasted work and a visible frame of falling.
	ADCCPickup* Deposited = Carry->ReleaseCarried();
	if (!Deposited)
	{
		return false;
	}

	HeldItem = Deposited;
	HeldItem->SetActorHiddenInGame(true);
	HeldItem->SetActorEnableCollision(false);

	return true;
}

void ADCCEscrowLock::ReleaseDeposit()
{
	if (!HasAuthority() || !HeldItem)
	{
		return;
	}

	// Put it back beside the lock rather than destroying it. A party that
	// failed and never came back has still lost it in every way that matters,
	// and one that did come back should not find a hole where their crate was.
	const FVector ReturnLocation = GetActorLocation() + GetActorForwardVector() * -160.0f
		+ FVector(0.0f, 0.0f, 60.0f);

	HeldItem->SetActorLocation(ReturnLocation);
	HeldItem->SetActorHiddenInGame(false);
	HeldItem->SetActorEnableCollision(true);
	HeldItem = nullptr;
}

void ADCCEscrowLock::TryPlayerInteract(ADCCCharacterBase* Interactor)
{
	if (!Interactor || IsOpen())
	{
		return;
	}

	// Try the deposit first. Only fall through to the base class's crowbar if
	// there is nothing to deposit - otherwise walking up holding a crate would
	// quietly charge Favor instead of taking the crate.
	if (HasAuthority())
	{
		if (AcceptDeposit(Interactor))
		{
			Unlock();
			return;
		}
	}
	else
	{
		UDCCCarryComponent* Carry = Interactor->FindComponentByClass<UDCCCarryComponent>();
		if (Carry && Carry->IsCarrying())
		{
			// Server re-validates; the client only nominates, same posture as
			// Server_ForcePuzzleDoor.
			Interactor->Server_DepositToEscrow(this);
			return;
		}
	}

	Super::TryPlayerInteract(Interactor);
}
