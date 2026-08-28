#include "Dialogue/DCCVendorNPC.h"

#include "Characters/DCCCharacterBase.h"

void ADCCVendorNPC::TryPlayerInteract(ADCCCharacterBase* Interactor)
{
	// Does NOT call Super - a vendor's E press opens the shop instead of
	// advancing the dialogue script. Ambient talk-on-approach still plays the
	// script exactly as it does on any other ADCCDialogueNPC, so the
	// quest-hint half of "tell you about quests" is unaffected; only the
	// deliberate button press changes meaning.
	if (Interactor)
	{
		Interactor->OpenVendorMenu(this);
	}
}
