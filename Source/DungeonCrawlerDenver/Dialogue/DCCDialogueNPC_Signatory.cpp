#include "Dialogue/DCCDialogueNPC_Signatory.h"

#include "Puzzles/DCCComplianceDoor.h"
#include "Characters/DCCCharacterBase.h"

ADCCDialogueNPC_Signatory::ADCCDialogueNPC_Signatory()
{
}

void ADCCDialogueNPC_Signatory::TryPlayerInteract(ADCCCharacterBase* Interactor)
{
	// Say the line first. The signature is the consequence of the conversation,
	// not a replacement for it.
	Super::TryPlayerInteract(Interactor);

	if (!HasAuthority() || !ComplianceDoor)
	{
		return;
	}

	const FName Id = SignatoryId.IsNone() ? SpeakerId : SignatoryId;
	if (Id.IsNone() || ComplianceDoor->HasSignature(Id))
	{
		return;
	}

	ComplianceDoor->GrantSignature(Id);

	if (Interactor)
	{
		Interactor->ShowLocalNotice(FText::Format(
			NSLOCTEXT("DCC", "SignatoryGranted", "{0} signs. {1}"),
			DisplayName, ComplianceDoor->GetProgressText()));
	}
}
