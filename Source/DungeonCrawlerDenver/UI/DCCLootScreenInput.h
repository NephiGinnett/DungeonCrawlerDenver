#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "DCCLootScreenInput.generated.h"

class ADCCHUD;

/**
 * An invisible full-screen widget that owns input while the loot box screen is
 * open. The screen itself is drawn on the HUD's Canvas (animation is easier
 * there); this just takes keyboard focus and mouse clicks so holding Space to
 * open a box does not also jump, and forwards them to the HUD.
 */
UCLASS()
class DUNGEONCRAWLERDENVER_API UDCCLootScreenInput : public UUserWidget
{
	GENERATED_BODY()

public:
	TWeakObjectPtr<ADCCHUD> Hud;

protected:
	virtual void NativeOnInitialized() override;
	virtual TSharedRef<SWidget> RebuildWidget() override;
	virtual FReply NativeOnKeyDown(const FGeometry& InGeometry, const FKeyEvent& InKeyEvent) override;
	virtual FReply NativeOnKeyUp(const FGeometry& InGeometry, const FKeyEvent& InKeyEvent) override;
	virtual FReply NativeOnMouseButtonDown(const FGeometry& InGeometry, const FPointerEvent& InMouseEvent) override;
	virtual FReply NativeOnMouseButtonUp(const FGeometry& InGeometry, const FPointerEvent& InMouseEvent) override;
	virtual FReply NativeOnMouseWheel(const FGeometry& InGeometry, const FPointerEvent& InMouseEvent) override;
};
