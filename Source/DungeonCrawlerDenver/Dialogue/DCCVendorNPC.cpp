#include "Dialogue/DCCVendorNPC.h"

#include "Characters/DCCCharacterBase.h"

void ADCCVendorNPC::TryPlayerInteract(ADCCCharacterBase* Interactor)
{
	// Does NOT call Super - opens the shop instead of advancing the dialogue
	// script. See this header's own comment: a player's E press no longer
	// reaches this function at all (it opens the Talk/Shop/Leave menu, whose
	// Shop row calls OpenVendorMenu directly) - this override remains for
	// whatever else still calls TryPlayerInteract programmatically.
	if (Interactor)
	{
		Interactor->OpenVendorMenu(this);
	}
}
