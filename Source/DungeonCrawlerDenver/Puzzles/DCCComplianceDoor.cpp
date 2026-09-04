#include "Puzzles/DCCComplianceDoor.h"

#include "Characters/DCCCharacterBase.h"
#include "Net/UnrealNetwork.h"

ADCCComplianceDoor::ADCCComplianceDoor()
{
	// The three parties bickering over the Denver deposit. Ids match the
	// SpeakerIds in DT_DCCDialogue so a signatory NPC and its signature are
	// the same name in both places.
	RecognisedSignatories = { TEXT("ClaimsAdjuster"), TEXT("SiteForeman"), TEXT("UnionSteward") };

	LockedDescription = NSLOCTEXT("DCC", "ComplianceDoorLocked",
		"MINE ACCESS - RESTRICTED. Entry requires authorisation from two recognised parties. "
		"A laminated notice lists three. None of them agree on which two.");

	SolvedAnnouncement = NSLOCTEXT("DCC", "ComplianceDoorSolved",
		"Paperwork accepted. Mine access is now authorised. Please note that authorisation "
		"is not an assurance of survival, and does not constitute a claim on anything found below.");

	ForcedAnnouncement = NSLOCTEXT("DCC", "ComplianceDoorForced",
		"The door to the platinum claim has been opened without authorisation. Three separate "
		"legal departments have been notified and are, for the first time in weeks, in complete agreement.");

	// Forcing this one is expensive on purpose. Skipping the floor's central
	// questline should feel like a real decision rather than a shortcut.
	ForceFavorCost = -75;
	ForceStrengthRequired = 5;
}

void ADCCComplianceDoor::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);
	DOREPLIFETIME(ADCCComplianceDoor, CollectedSignatures);
}

void ADCCComplianceDoor::GrantSignature(FName SignatoryId)
{
	if (!HasAuthority() || SignatoryId.IsNone() || IsOpen())
	{
		return;
	}

	// Only parties the door actually recognises. A signature from someone with
	// no standing is exactly the kind of thing this door exists to reject.
	if (!RecognisedSignatories.Contains(SignatoryId))
	{
		return;
	}

	if (CollectedSignatures.Contains(SignatoryId))
	{
		return;
	}

	CollectedSignatures.Add(SignatoryId);

	if (IsPuzzleSatisfied())
	{
		Unlock();
	}
}

FText ADCCComplianceDoor::GetProgressText() const
{
	return FText::Format(
		NSLOCTEXT("DCC", "ComplianceDoorProgress", "Authorisations: {0} of {1}"),
		FText::AsNumber(CollectedSignatures.Num()),
		FText::AsNumber(RequiredSignatureCount));
}

void ADCCComplianceDoor::TryPlayerInteract(ADCCCharacterBase* Interactor)
{
	// Show how the form is going before falling through to the base class's
	// crowbar attempt - a party that has one of two signatures should be told
	// so, rather than being quietly offered a Favor bill instead.
	if (Interactor && !IsOpen())
	{
		Interactor->ShowLocalNotice(GetProgressText());
	}

	Super::TryPlayerInteract(Interactor);
}
