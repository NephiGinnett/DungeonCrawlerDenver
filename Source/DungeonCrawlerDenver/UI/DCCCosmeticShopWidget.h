#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "DCCCosmeticShopWidget.generated.h"

class UDCCWalletComponent;

/**
 * The Den's counter (2026-11-13; one tabbed screen since 2026-11-14): PRIZES (cosmetics for tickets), TROPHIES
 * (what you brought home, some sell) and CHOP SHOP (truck upgrades for scrap). Opened by standing on the gold
 * Prize Counter pad or the cyan Chop Shop pad in the Den, or PRIZES on the main menu. Plain C++ Slate in the
 * System's window style (UI/DCCMenuStyle): tiles with pictures, and a panel showing whatever you hover.
 */
UCLASS()
class DUNGEONCRAWLERDENVER_API UDCCCosmeticShopWidget : public UUserWidget
{
	GENERATED_BODY()

public:
	TFunction<void()> OnCloseRequested;

	/** 0 PRIZES, 1 TROPHIES, 2 CHOP SHOP, 3 OUTFITTERS, 4 TRADING POST, 5 BOUNTIES. Set before it is shown; the tab buttons change it. */
	int32 Tab = 0;

	/** What the right-hand panel shows: the tile under the mouse, else the one last clicked on each tab. */
	FName HoverId;
	FName SelectedId[6];

	/** CLEAR ALL SPRAYS asks twice: the first click arms it for a few seconds. */
	double ClearArmedAt = 0.0;

protected:
	virtual void NativeOnInitialized() override;
	virtual TSharedRef<SWidget> RebuildWidget() override;
	virtual FReply NativeOnKeyDown(const FGeometry& InGeometry, const FKeyEvent& InKeyEvent) override;

private:
	UDCCWalletComponent* GetWallet() const;
};
