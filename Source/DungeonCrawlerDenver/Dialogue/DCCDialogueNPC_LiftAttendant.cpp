#include "Dialogue/DCCDialogueNPC_LiftAttendant.h"

#include "Dialogue/DCCDialogueTypes.h"
#include "Rooms/DCCCliffLift.h"
#include "Characters/DCCCharacterBase.h"

ADCCDialogueNPC_LiftAttendant::ADCCDialogueNPC_LiftAttendant()
{
	RequestLine = NSLOCTEXT("DCC", "LiftRequestLine", "Hang on, I'll take you up.");
	AlreadyGoingLine = NSLOCTEXT("DCC", "LiftAlreadyGoingLine", "Already on its way. Hop on.");
}

void ADCCDialogueNPC_LiftAttendant::RequestLift(ADCCCharacterBase* Interactor)
{
	if (!Interactor || !LinkedLift)
	{
		return;
	}

	FDCCDialogueLine Line;
	Line.SpeakerId = SpeakerId;
	Line.Text = (LinkedLift->IsRising() || LinkedLift->IsAtTop()) ? AlreadyGoingLine : RequestLine;
	ShowLine(Line);

	Interactor->Server_RequestCliffLift(LinkedLift);
}

void ADCCDialogueNPC_LiftAttendant::TryPlayerInteract(ADCCCharacterBase* Interactor)
{
	// Does NOT call Super, for the same reason ADCCVendorNPC doesn't: a deliberate E
	// press here means "send me up", not "say the next scripted line". No longer
	// what a player's E press reaches, though (changed 2026-08-30 alongside the
	// Talk/Shop/Quests/Leave menu, see DISPATCH.md) - that calls RequestLift
	// directly. Kept for whatever else still calls TryPlayerInteract programmatically.
	RequestLift(Interactor);
}
