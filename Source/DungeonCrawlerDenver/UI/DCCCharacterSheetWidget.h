#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "DCCCharacterSheetWidget.generated.h"

class ADCCCharacterBase;
class SBox;

/**
 * The Tab menu's CHARACTER page, rebuilt in Slate (2026-11-23, Zoe: "I feel that the game is still quite far from a AAA
 * style game when it comes to HUDs ... port one screen and send me a list of style kits ... or just try them out and
 * show me screenshots").
 *
 * The first screen off the Canvas HUD. Same data and the same actions as the Canvas page (raise a stat, equip from the
 * bag, unequip), laid out properly: a header with name, class, level and the XP bar; vitals as bars; the five scores as
 * cards with their + buttons; what a point buys; the gear on you with icons; the bag. It slides in when it opens.
 *
 * Three looks to choose from (Look, set by DCC.SheetLook <0-2> for now): 0 SYSTEM GLASS (the prize counter's own style,
 * Roboto), 1 NEON ARCADE (Oxanium and Chakra Petch, pink and cyan), 2 BROADCAST (Bebas Neue and Barlow Condensed, gold
 * on black, a TV lower-third). Fonts are OFL files in Content/DCC/UI/Fonts, loaded straight from disk.
 */
UCLASS()
class DUNGEONCRAWLERDENVER_API UDCCCharacterSheetWidget : public UUserWidget
{
	GENERATED_BODY()

public:
	/** The HUD asks for a tab when one of the other tab buttons is clicked. */
	TFunction<void(int32 Tab)> OnTabRequested;
	TFunction<void()> OnCloseRequested;

	/** Rebuild now (the look changed). */
	void Refresh();

protected:
	virtual TSharedRef<SWidget> RebuildWidget() override;
	virtual void NativeTick(const FGeometry& MyGeometry, float InDeltaTime) override;
	virtual void NativeConstruct() override;

private:
	TSharedRef<SWidget> BuildContent();
	FString Signature() const;
	ADCCCharacterBase* Crawler() const;

	TSharedPtr<SBox> Root;
	FString LastSignature;
	int32 BuiltLook = -1;
	float Opened = 0.0f;
};
