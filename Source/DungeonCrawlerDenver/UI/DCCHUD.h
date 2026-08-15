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

	/** Full-screen overlay shown once the local crawler is down. */
	void DrawDeathScreen();

	/** Climb confirmation, climb controls, or targeting hint - whichever applies. */
	void DrawInteractionPrompt(ADCCCharacterBase* Crawler);

	/** Stamina meter, shown only while it matters. */
	void DrawStaminaBar(ADCCCharacterBase* Crawler);

	/**
	 * Always-on health, mana and XP bars.
	 *
	 * Drawn here rather than in WBP_PlayerHUD because a Canvas bar is recomputed
	 * from the replicated value every frame, so it cannot get stuck showing a
	 * stale fraction - which is exactly what the widget version did at zero HP.
	 */
	void DrawVitals(ADCCCharacterBase* Crawler);

	/** Shared bar drawing, so every meter on screen lines up and behaves alike. */
	void DrawMeter(float X, float Y, float Width, float Height, float Fraction,
		const FLinearColor& Fill, const FString& Label);

	/**
	 * Always-on objective tracker, top right.
	 *
	 * Deliberately opposite the feed rather than inside the Tab menu: a goal you
	 * have to open a menu to remember is a goal the party forgets. It draws over
	 * the world because that is the price of it being useful.
	 */
	void DrawQuestTracker();

	/** Panel sections. Each returns the Y it finished at, so they stack. */
	float DrawStats(float X, float Y, ADCCCharacterBase* Crawler);
	float DrawSkills(float X, float Y, ADCCCharacterBase* Crawler);
	float DrawInventory(float X, float Y, ADCCCharacterBase* Crawler);
	float DrawAchievements(float X, float Y, ADCCCharacterBase* Crawler);
	float DrawFeed(float X, float Y);

private:
	bool bShowMenu = false;
};
