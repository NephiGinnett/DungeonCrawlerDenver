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
 * near them. E is what changes: on a plain ADCCDialogueNPC it advances the
 * script, but here it opens the shop instead, so the two capabilities never
 * fight over the same button.
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
