#pragma once

#include "CoreMinimal.h"
#include "GameFramework/HUD.h"
#include "DCCHUD.generated.h"

class ADCCCharacterBase;
class UDCCStatsComponent;
enum class EDCCLogCategory : uint8;

/** Pages of the Tab menu. Clickable tabs, or Q/E-style arrow keys. */
enum class EDCCMenuTab : uint8
{
	Character,
	Skills,
	Inventory,
	Party,
	Chat,
	Achievements,

	MAX
};

/** Pages of the DM panel. */
enum class EDCCAdminTab : uint8
{
	Self,
	Skills,
	Spawn,

	MAX
};

/** Coarse presets over EDCCLogCategory - one key to cycle rather than nine to remember. */
enum class EDCCFeedFilter : uint8
{
	All,
	CombatOnly,
	SkillsOnly,
	Milestones,

	MAX
};

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
	void ToggleMenu();

	/**
	 * Canvas hit boxes are how an immediate-mode HUD gets clickable regions - the
	 * boxes are registered fresh each frame during DrawHUD and this fires when one
	 * is clicked. Used for the achievement list; the box name is the achievement id.
	 */
	virtual void NotifyHitBoxClick(FName BoxName) override;

	UFUNCTION(BlueprintPure, Category = "DCC|UI")
	bool IsMenuVisible() const { return bShowMenu; }

	/** How many feed lines to show under the menu. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadWrite, Category = "DCC|UI", meta = (ClampMin = "0"))
	int32 FeedLineCount = 8;

	/** Advances the feed to its next filter preset. Local and cosmetic. */
	UFUNCTION(BlueprintCallable, Category = "DCC|UI")
	void CycleFeedFilter();

	/** Moves between menu pages. Wraps. */
	UFUNCTION(BlueprintCallable, Category = "DCC|UI")
	void CycleMenuTab(int32 Delta);

	/**
	 * Flashes a short message in the middle of the screen.
	 *
	 * For things the player did that did NOT happen. Every refusal in this game
	 * was previously silent - a click that found no target, a skill that could
	 * not be afforded - which is indistinguishable from the input not registering
	 * at all. Silence is the worst possible feedback for a failed action.
	 */
	UFUNCTION(BlueprintCallable, Category = "DCC|UI")
	void ShowNotice(const FText& Message, float Seconds = 2.5f);

	/** Opens or closes the DM panel, and takes the cursor with it. */
	UFUNCTION(BlueprintCallable, Category = "DCC|Admin")
	void ToggleAdminPanel();

	UFUNCTION(BlueprintPure, Category = "DCC|Admin")
	bool IsAdminPanelVisible() const { return bShowAdminPanel; }

protected:
	/** Returns the pawn this HUD describes, or null when not possessing one. */
	ADCCCharacterBase* GetOwningCrawler() const;

	/** Full-screen overlay shown once the local crawler is dead for good. */
	void DrawDeathScreen();

	/**
	 * The recoverable version, shown while downed and bleeding out.
	 *
	 * Separate from the death screen because the two states must never be
	 * confused: one is a countdown you can be saved from, the other is the end of
	 * your evening.
	 */
	void DrawDownedScreen(ADCCCharacterBase* Crawler);

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
	 * The countable-block health bar, shared by the player's own vitals and the
	 * mini bars drawn above nearby characters.
	 *
	 * FillColour is the colour of every FULL block. The player's own bar passes
	 * a fixed green - segments simply go missing as they are lost, which is the
	 * whole mechanic. World-space bars pass GetAllegianceColour() instead, so a
	 * block reads ally or hostile at a glance rather than reddening with damage,
	 * which would be indistinguishable from "hostile" at low health.
	 */
	void DrawSegmentedBar(UDCCStatsComponent* Stats, float CenterX, float TopY,
		float TotalWidth, float SegmentHeight, const FLinearColor& FillColour,
		bool bShowRegenGhost);

	/**
	 * Mini segmented bars above every living character within range, coloured by
	 * allegiance and gated by distance - replacing the old WBP_EnemyHealthBar
	 * Screen Space widget, which had no distance cull and cost a full compositing
	 * pass per enemy every frame regardless of range. This is one Canvas pass,
	 * bounded to what is actually near the player.
	 */
	void DrawNearbyHealthBars(ADCCCharacterBase* Crawler);

	/**
	 * A reticle for whatever skill is currently armed, plus a bracket around
	 * whatever it would actually hit.
	 *
	 * Calls the exact same ADCCCharacterBase::FindSkillTargetUnderCursor the
	 * click handler resolves against, so the overlay is never lying about what
	 * a click would do - there is one function deciding the target, not a
	 * second copy here that might drift out of sync with it.
	 */
	void DrawSkillTargetingOverlay(ADCCCharacterBase* Crawler);

	/**
	 * Always-on objective tracker, top right.
	 *
	 * Deliberately opposite the feed rather than inside the Tab menu: a goal you
	 * have to open a menu to remember is a goal the party forgets. It draws over
	 * the world because that is the price of it being useful.
	 */
	void DrawQuestTracker();

	/**
	 * The expanded card for whichever achievement was clicked: what it is
	 * counting, exact progress, what it pays, and the System AI's line.
	 *
	 * Drawn beside the menu rather than inside it because the list is already
	 * near the bottom of a full-height panel, and pushing eight rows down to
	 * make room would move the thing the player just clicked out from under
	 * their cursor.
	 */
	void DrawAchievementDetail(ADCCCharacterBase* Crawler);

	/**
	 * The armed-skill bar along the bottom.
	 *
	 * Slots map to keys 1-5, which have always worked - there was simply no way
	 * to see what they were bound to, so arming a skill was done blind. Shows
	 * mana cost and live cooldown, because those are the two things that decide
	 * whether pressing the key will do anything.
	 */
	void DrawHotbar(ADCCCharacterBase* Crawler);

	/**
	 * The conversation menu, drawn when the local crawler is talking to someone.
	 *
	 * Unavailable options are drawn greyed with their reason rather than hidden -
	 * "Ask them to join you — not friendly enough yet" teaches the player how to
	 * unlock it, which a missing row cannot.
	 */
	void DrawInteractionMenu(ADCCCharacterBase* Crawler);

	/**
	 * Somebody else's gear, opened from the conversation menu.
	 *
	 * Read-only and local. Trading is not built, so this is deliberately a
	 * *look*, not a transfer - which is still worth having, because deciding
	 * whether an ally should be carrying the Fire Axe is a conversation the party
	 * cannot have without seeing who has it.
	 */
	void DrawInspectedGear(ADCCCharacterBase* Crawler);

	/**
	 * The loot menu: opened on a corpse or a chest instead of a person.
	 *
	 * A separate panel from DrawInteractionMenu rather than a mode of it - see
	 * ADCCCharacterBase's own comment on LootTargetActor for why the two were
	 * kept apart. Draws whichever of the two pages is open: the top menu
	 * (Check Inventory / Loot All / Stow / Leave) or the itemised submenu.
	 */
	void DrawLootMenu(ADCCCharacterBase* Crawler);

	/**
	 * The DM panel: buttons for everything the exec commands do.
	 *
	 * Drawn with the same Canvas hit boxes as the achievement rows, so it needs
	 * no UMG asset and works the moment it compiles - which is the same reason
	 * the rest of this HUD is Canvas.
	 */
	void DrawAdminPanel(ADCCCharacterBase* Crawler);

	/**
	 * One button. Returns the Y below it so callers can stack without arithmetic.
	 *
	 * `Action` becomes the hit box name, prefixed so NotifyHitBoxClick can tell
	 * panel buttons from achievement rows and menu tabs.
	 */
	float DrawAdminButton(float X, float Y, float Width, const FString& Label,
		const FString& Action, bool bHighlighted = false);

	/** Routes a clicked `Adm|` hit box to the matching action. */
	void HandleAdminAction(const FString& Action, ADCCCharacterBase* Crawler);

	/** The row of page tabs at the top of the character menu. */
	float DrawMenuTabs(float X, float Y);

	/** Party page: who is with you, their health, and whether they are holding. */
	float DrawParty(float X, float Y, ADCCCharacterBase* Crawler);

	/**
	 * Chat page: nearby friendly or neutral faces, pick one to start talking.
	 *
	 * Deliberately just a picker for the conversation menu that already exists
	 * (DrawInteractionMenu, always drawn regardless of this tab) rather than a
	 * second dialogue system - Chatter/AskAbout/Persuade/Intimidate are the
	 * whole of "chat" here. There is no shop option anywhere in that menu, and
	 * this page must never grow one; picking a name opens a conversation, not
	 * a storefront.
	 */
	float DrawChat(float X, float Y, ADCCCharacterBase* Crawler);

	/** Equipment page content, folded into the Character tab. */
	float DrawEquipment(float X, float Y, ADCCCharacterBase* Crawler);

	/** Panel sections. Each returns the Y it finished at, so they stack. */
	float DrawStats(float X, float Y, ADCCCharacterBase* Crawler);
	float DrawSkills(float X, float Y, ADCCCharacterBase* Crawler);
	float DrawInventory(float X, float Y, ADCCCharacterBase* Crawler);
	float DrawAchievements(float X, float Y, ADCCCharacterBase* Crawler);
	float DrawFeed(float X, float Y);

private:
	bool bShowMenu = false;

	/** Which achievement row is expanded. None means no card is showing. */
	FName SelectedAchievementId;

	/**
	 * The Chat tab's last-drawn list, indexed by the hit boxes DrawChat just
	 * registered - "Chat_2" means ChatCandidates[2]. Rebuilt fresh every frame
	 * DrawChat runs, same lifetime as any other Canvas hit box.
	 */
	UPROPERTY(Transient)
	TArray<TWeakObjectPtr<ADCCCharacterBase>> ChatCandidates;

	EDCCFeedFilter FeedFilter = EDCCFeedFilter::All;

	EDCCMenuTab MenuTab = EDCCMenuTab::Character;

	bool bShowAdminPanel = false;
	EDCCAdminTab AdminTab = EDCCAdminTab::Self;

	/** Paging for the skill list, which is far longer than one screen. */
	int32 AdminSkillPage = 0;

	FText NoticeText;
	float NoticeExpiryTime = 0.0f;

	/** Draws the transient centre-screen notice, if one is live. */
	void DrawNotice();

	/** Whether a line of this category should draw under the current preset. */
	bool PassesFeedFilter(EDCCLogCategory Category) const;

	/** Name shown next to the feed so the active preset is never a guess. */
	FString GetFeedFilterLabel() const;
};
