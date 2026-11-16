#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "DCCShopWidget.generated.h"

class ADCCHUD;
class ADCCVendorNPC;
class SVerticalBox;

/**
 * The shop screen (2026-11-13). Opens when a crawler picks Shop on a vendor
 * (ADCCCharacterBase::OpenVendorMenu); the HUD notices and shows this instead
 * of the old scroll-wheel list.
 *
 *   Buy   every item the vendor stocks, as a card: rarity colour, slot, stats,
 *         how it compares with what you have on, and which skill it uses and
 *         your Rank in it. Gear goes to the bag; potions to the pack.
 *   Sell  everything in your bag, at half its value.
 *
 * Purchases and sales are server-checked (ADCCCharacterBase::Server_ShopBuy /
 * Server_ShopSell); this widget only asks. The HUD calls TickShop each frame so
 * the lists follow the replicated bag and purse.
 */
UCLASS()
class DUNGEONCRAWLERDENVER_API UDCCShopWidget : public UUserWidget
{
	GENERATED_BODY()

public:
	TWeakObjectPtr<ADCCHUD> Hud;
	TWeakObjectPtr<ADCCVendorNPC> Vendor;

	void Open(ADCCVendorNPC* InVendor);
	void TickShop();

protected:
	virtual TSharedRef<SWidget> RebuildWidget() override;
	virtual void ReleaseSlateResources(bool bReleaseChildren) override;
	virtual FReply NativeOnKeyDown(const FGeometry& InGeometry, const FKeyEvent& InKeyEvent) override;

private:
	void RebuildList();
	void Close();

	bool bSellTab = false;
	FString Message;
	int32 SeenBagCount = -1;
	int32 SeenGold = -1;
	TSharedPtr<SVerticalBox> List;
};
