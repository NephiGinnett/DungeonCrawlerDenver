#pragma once

#include "CoreMinimal.h"
#include "GameFramework/HUD.h"
#include "DCCHUD.generated.h"

class ADCCCharacterBase;

/**
 * The character menu: stats, skills and inventory, toggled with Tab.
 *
 * Drawn with Canvas text rather than UMG, for the same reason the damage tickers
 * are: a widget needs a WBP asset authored through the UMG toolset, which cannot
 * be driven reliably over MCP, whereas this works the moment it compiles. It is
 * deliberately plain - the value here is that the numbers the ruleset computes
 * are finally visible to the player, not that the panel is pretty. Replacing it
 * with a UMG screen later touches only this class.
 *
 * Requires the GameMode's HUDClass to point at this class.
 */
UCLASS()
class DUNGEONCRAWLERDENVER_API ADCCHUD : public AHUD
{
	GENERATED_BODY()

public:
	virtual void DrawHUD() override;

	UFUNCTION(BlueprintCallable, Category = "DCC|UI")
	void ToggleMenu() { bShowMenu = !bShowMenu; }

	UFUNCTION(BlueprintPure, Category = "DCC|UI")
	bool IsMenuVisible() const { return bShowMenu; }

	/** How many feed lines to show under the menu. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadWrite, Category = "DCC|UI", meta = (ClampMin = "0"))
	int32 FeedLineCount = 8;

protected:
	/** Returns the pawn this HUD describes, or null when not possessing one. */
	ADCCCharacterBase* GetOwningCrawler() const;

	/** Panel sections. Each returns the Y it finished at, so they stack. */
	float DrawStats(float X, float Y, ADCCCharacterBase* Crawler);
	float DrawSkills(float X, float Y, ADCCCharacterBase* Crawler);
	float DrawInventory(float X, float Y, ADCCCharacterBase* Crawler);
	float DrawFeed(float X, float Y);

private:
	bool bShowMenu = false;
};
