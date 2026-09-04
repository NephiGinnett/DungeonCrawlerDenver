#pragma once

#include "CoreMinimal.h"
#include "Dialogue/DCCDialogueNPC.h"
#include "DCCVendorNPC.generated.h"

/**
 * A dialogue NPC who also sells things - the bartender's actual job per Zoe:
 * drinks and "appropriate items", not just flavour lines or quest hints.
 *
 * Kept as a subclass rather than folded into ADCCDialogueNPC itself: most of
 * the "wide array of non-aggressive NPCs" Zoe asked for are not shopkeepers,
 * and giving every one of them empty SellableEquipmentIds/Drinks arrays would
 * be clutter on every dialogue NPC for a capability only some of them need.
 *
 * Ambient talk-on-approach is untouched and still works exactly as it does on
 * the base class - a vendor still tells you about quests just by standing
 * near them.
 *
 * TryPlayerInteract itself is no longer what a player's E press reaches
 * (changed 2026-08-30, see DISPATCH.md) - E opens ADCCCharacterBase's own
 * Talk/Shop/Leave menu, and that menu's Shop row calls OpenVendorMenu
 * directly rather than through here, precisely so Talk (which calls
 * SayNextLine directly, not through this virtual) cannot end up here by
 * accident. This override is kept for whatever else still calls
 * TryPlayerInteract programmatically - see that function's own comment.
 */
UCLASS()
class DUNGEONCRAWLERDENVER_API ADCCVendorNPC : public ADCCDialogueNPC
{
	GENERATED_BODY()

public:
	/** Equipment rows from DT_DCCEquipment this vendor sells, at that row's own GoldValue. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Vendor")
	TArray<FName> SellableEquipmentIds;

	/**
	 * DT_DCCConsumables rows this vendor sells, at that row's own GoldValue.
	 * Resolved through the buyer's own UDCCInventoryComponent::FindConsumable
	 * (see ADCCCharacterBase::Server_PurchaseDrink) rather than this class
	 * holding its own table reference - one less thing to keep in sync.
	 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Vendor")
	TArray<FName> SellableConsumableIds;

	/** Opens the shop on Interactor instead of advancing the dialogue script. */
	virtual void TryPlayerInteract(ADCCCharacterBase* Interactor) override;
};
