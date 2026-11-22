#pragma once

#include "CoreMinimal.h"
#include "GameFramework/HUD.h"
#include "UI/DCCChatTypes.h"
#include "Stats/DCCAbilityTypes.h"
#include "Rooms/DCCGauntletEncounter.h"
#include "Onboarding/DCCOnboardingTypes.h"
#include "Items/DCCLootBoxTypes.h"
#include "DCCHUD.generated.h"

class ADCCCharacterBase;
class ADCCIntroDirector;
class UDCCStatsComponent;
class UDCCChatInputWidget;
class UDCCSessionMenuWidget;
class UDCCCharacterSelectWidget;
class UDCCCosmeticShopWidget;
class UDCCRoundOverWidget;
class UDCCLootScreenInput;
class UDCCGearBenchWidget;
class UDCCShopWidget;
class UDCCBossComponent;
enum class EDCCLogCategory : uint8;
struct FDCCEventLogEntry;

/**
 * One item pop-up card (2026-11-12, after Elyra's Tales' notices.gd and its rarity
 * colours). Plain C++, client-side only: the HUD builds these itself by watching
 * the owning crawler's replicated inventory, equipment, loot boxes and gold.
 */
struct FDCCItemToastCard
{
	FString Header;
	FString Name;
	FString Subtitle;
	TArray<FString> Lines;
	FString Compare;
	int32 CompareSign = 0;
	FLinearColor Colour = FLinearColor::White;
	uint8 Icon = 0;
	int32 GoldAmount = 0;
	double ShownAt = -1.0;
};

/** Pages of the Tab menu. Clickable tabs, or Q/E-style arrow keys. */
enum class EDCCMenuTab : uint8
{
	Character,
	Skills,
	Inventory,
	Party,

	/** Renamed from Chat 2026-08-23: this is the proximity NPC-talk picker
	 *  (opens DrawInteractionMenu), not the new player chat channels below.
	 *  Keeping "Chat" for that would collide with Chatter/Allies/Whispers in
	 *  a player's head even though the enum values themselves don't clash. */
	Talk,

	Achievements,

	/** Bigger scrollback of the always-on Feed box, for catching up on lines
	 *  that scrolled past - the always-on box itself is unaffected. */
	Feed,

	/** Simulated audience - see UI/DCCChatterTypes.h. Read-only, no send box. */
	Chatter,

	/** Anyone fist-bumped who's still alive - see ADCCCharacterBase::IsBefriended. */
	Allies,

	/** Private 1:1 - type a name to open a channel with them. */
	Whispers,

	/** Keys and reactions (2026-11-16): remap the new keys, choose the bubble and move for each moment. */
	Settings,

	/** Dead Air posts, the channel, and the old party / allies / whisper threads. */
	Social,

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

	/** Only entries where the local crawler is Instigator or RollTarget - the
	 *  "cut the noise with five NPCs swinging at once" filter. */
	MyRolls,

	MAX
};

/** How much of the always-on Feed box is showing. Re-pinning happens from the
 *  Feed tab of the Tab menu; minimizing/hiding happens live via the same key
 *  that opens this cycle (K by default). */
enum class EDCCFeedVisibility : uint8
{
	/** Full box, exactly like today. */
	Pinned,

	/** Just the "Log: ... [N new]" header line. */
	Minimized,

	/** Nothing drawn at all. */
	Hidden,

	MAX
};

/** What ChatInputWidget is currently collecting text for. */
enum class EDCCChatInputMode : uint8
{
	None,

	/** Free text, sent to ActiveChatChannel on submit. */
	Message,

	/** A crawler's name; submitting sets WhisperTargetName and reopens in
	 *  Message mode automatically. */
	WhisperTarget,
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

	/** Onboarding: has this crawler been issued this part of the HUD yet? Always true with no onboarding in the level. */
	bool IsHudFeatureUnlocked(EDCCHudFeature Feature) const;
	bool IsMenuTabUnlocked(EDCCMenuTab Tab) const;

	UFUNCTION(BlueprintCallable, Category = "DCC|UI")
	void ToggleMenu();

	/**
	 * Closes one open panel that is not the Tab sheet: loot boxes, a shop, the party menu,
	 * the pose bench, the DM panel, or a F.I.A.T. tube. True when something closed.
	 * Esc and Tab call this before they open or stand up.
	 */
	bool DismissOverlay();
	/** Opens the Tab menu on a given page (dev command DCC.MenuTab). */
	void OpenMenuTab(EDCCMenuTab Tab) { MenuTab = Tab; if (!bShowMenu) { ToggleMenu(); } }
	/** Opens SOCIAL on one friend's whisper thread. */
	void OpenSocialFriend(const FString& Name);
	/** The starter-kit choice (UDCCWalletComponent::Client_OfferStarterKit). */
	void OpenStarterKit();

	/**
	 * Canvas hit boxes are how an immediate-mode HUD gets clickable regions - the
	 * boxes are registered fresh each frame during DrawHUD and this fires when one
	 * is clicked. Used for the achievement list; the box name is the achievement id.
	 */
	virtual void NotifyHitBoxClick(FName BoxName) override;

	UFUNCTION(BlueprintPure, Category = "DCC|UI")
	bool IsMenuVisible() const { return bShowMenu; }

	// --- The summoning tubes' panel (2026-11-17, Arcade/DCCSummons.h) ---
	/** E at a podium: open its panel, or close it. */
	void ToggleSummonPanel(int32 Tube);
	bool IsSummonPanelOpen() const { return SummonTube >= 0; }
	/** A Mother Lode scratchcard is on screen (2026-11-21). The mouse scratches; it does not swing. */
	bool IsScratchCardOpen() const { return bScratchOpen; }
	/** The reveal banner after a pull. */
	void ShowSummonResult(FName Friend, uint8 Tier, uint8 Variant, const FString& WeaponName);
	/** The reveal banner alone (also drawn while a trailer is rolling). */
	void DrawSummonBanner();

	/** How many feed lines to show under the menu. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadWrite, Category = "DCC|UI", meta = (ClampMin = "0"))
	int32 FeedLineCount = 8;

	/** Advances the feed to its next filter preset. Local and cosmetic. */
	UFUNCTION(BlueprintCallable, Category = "DCC|UI")
	void CycleFeedFilter();

	/** Moves between menu pages. Wraps. */
	UFUNCTION(BlueprintCallable, Category = "DCC|UI")
	void CycleMenuTab(int32 Delta);

	/** Pinned -> Minimized -> Hidden -> Pinned. The Feed tab's "Re-pin" row
	 *  jumps straight back to Pinned instead of cycling through all three. */
	UFUNCTION(BlueprintCallable, Category = "DCC|UI")
	void CycleFeedVisibility();

	// Plain C++, not UFUNCTION: EDCCFeedVisibility is a plain enum like
	// EDCCMenuTab/EDCCFeedFilter, not a UENUM, matching this class's existing
	// convention that Canvas-HUD state enums stay internal rather than being
	// exposed to Blueprint.
	EDCCFeedVisibility GetFeedVisibility() const { return FeedVisibility; }
	void SetFeedVisibility(EDCCFeedVisibility NewVisibility) { FeedVisibility = NewVisibility; }

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

	/** Slides an achievement card in at the top. Several queue so they never overlap. */
	void PushAchievement(const FString& Title, const FString& Reward);

	/** Opens or closes the DM panel, and takes the cursor with it. */
	UFUNCTION(BlueprintCallable, Category = "DCC|Admin")
	void ToggleAdminPanel();

	/** The Den shop (prize counter): spend arcade tickets on cosmetics. */
	/** Tab: 0 prizes, 1 trophies, 2 chop shop. */
	void OpenCosmeticShop(int32 Tab = 0);

	/** Guides that show once per player, ever (DCCSession::HasSeenGuide), cached so the HUD can ask every frame. */
	bool IsGuideSeen(FName Guide);
	void MarkGuideSeen(FName Guide);
	TMap<FName, bool> GuideCache;
	FString GuideCacheKey;
	float RoverGuideSeconds = 0.0f;
	void CloseCosmeticShop();
	bool IsCosmeticShopOpen() const;

	/** The party menu (F2): host, invite, join, who's here, start the run. */
	void ToggleSessionMenu();
	bool IsSessionMenuOpen() const;

	// --- Loot box screen (O) - UI/DCCHUD_LootBoxScreen.cpp -----------------
	// Every sealed box in a row. Hold Space (or hold the mouse on a box) to
	// open one, and keep holding to open the next. Hold R to open them all.
	void ToggleLootBoxScreen();
	/** Dev (DCC.LootOpen): open the screen if needed and open one of the best boxes you have. */
	void DevOpenBestLootBox();
	bool IsLootBoxScreenOpen() const { return bLootScreenOpen; }
	void ReceiveLootBoxReveal(const FDCCLootBoxReveal& Reveal);
	/** From UDCCLootScreenInput. True if the screen used the key. */
	bool HandleLootScreenKey(const FKey& Key, bool bPressed);

	/** The gear bench (F3): nudge grips, sockets and bones. See Bench/DCCGearBenchWidget.h. */
	void ToggleGearBench();
	bool IsGearBenchOpen() const;

	/** The shop screen. Opens itself when the crawler picks Shop on a vendor. */
	bool IsShopOpen() const;
	void CloseShop();

	virtual void BeginPlay() override;
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;

	UFUNCTION(BlueprintPure, Category = "DCC|Admin")
	bool IsAdminPanelVisible() const { return bShowAdminPanel; }

protected:
	/** Returns the pawn this HUD describes, or null when not possessing one. */
	ADCCCharacterBase* GetOwningCrawler() const;

	/**
	 * Returns the rover this HUD's owner is currently driving, or null.
	 * Needed as its own accessor rather than reusing GetOwningCrawler:
	 * driving possesses the rover pawn directly (ADCCRover::EnterRover calls
	 * PC->Possess(this)), so GetPawn() returns an ADCCRover while driving,
	 * not an ADCCCharacterBase - GetOwningCrawler's cast fails and returns
	 * null for exactly the case this needs to detect.
	 */
	class ADCCRover* GetOwningRover() const;

	/** Full-screen overlay shown once the local crawler is dead for good. */
	void DrawDeathScreen();

	/** Full black takeover shown once the local crawler has walked into an active ADCCExitPortal. */
	void DrawAwaitingOtherCrawlersScreen();

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
	 * Small always-on tag near the top of the screen naming the room the local
	 * crawler currently physically occupies (ADCCCharacterBase::CurrentRoomDisplayName,
	 * itself mirrored from ADCCRoom's existing occupancy tracking - no separate
	 * room-tracking system here). Draws nothing while the value is empty, i.e.
	 * whenever the crawler is not inside any authored ADCCRoom volume, rather
	 * than showing a stale or placeholder label.
	 */
	void DrawRoomTag(ADCCCharacterBase* Crawler);

	/**
	 * Camera-viewfinder framing shown only while ADCCCharacterBase::IsInSelfieMode
	 * is true (2026-08-28) - corner brackets, a centre reticle, a pose-cycle hint,
	 * and a progress bar toward the shutter actually firing. Purely cosmetic; the
	 * mechanics (posing, the shutter, the screenshot) all live on the character,
	 * this just makes the moment read as "taking a photo" instead of a camera cut
	 * with no feedback.
	 */
	void DrawSelfieViewfinder(ADCCCharacterBase* Crawler);

	/**
	 * A small three-dot "busy" wave, shown for as long as Crawler->IsCasting()
	 * is true - a skill cast, a revive channel, a loot box reveal, or getting
	 * into a rover all root the player the same way and this covers all of
	 * them for free. Added specifically as a diagnostic (Zoe's ask,
	 * 2026-09-03): if a one-shot animation was SUPPOSED to be playing during
	 * the lock and silently isn't (a skeleton mismatch, an unset clip), this
	 * still shows something is happening and for how long - which is what
	 * makes "stuck for several seconds, nothing visible" diagnosable instead
	 * of looking identical to a hang.
	 */
	void DrawCastingIndicator(ADCCCharacterBase* Crawler);

	/**
	 * A small gapped-cross reticle at screen centre, shown whenever a ranged
	 * weapon is equipped (Zoe, 2026-09-04: "make a small crosshair appear...
	 * where if anywhere the bullet lands"). Re-fires the exact same trace
	 * FindHostileUnderCrosshair uses to actually resolve a shot, so "is the
	 * reticle lit up" always agrees with "would this shot land" - screen
	 * centre is where that trace's hit point projects to by construction,
	 * since the trace runs along the camera's own forward vector, so no
	 * world-to-screen projection is needed at all.
	 */
	void DrawRangedCrosshair(ADCCCharacterBase* Crawler);

	/**
	 * Big centre-screen "LEVEL BOSS" card, shown once per boss actor the first
	 * time the local crawler comes within health-bar range of it alive.
	 *
	 * Deliberately keyed off UDCCBossComponent rather than anything
	 * Voldemortgage-specific, so every future boss gets this for free just by
	 * carrying that component - the same reason the charge/lien mechanics on
	 * that component are generic. Client-local and purely cosmetic: which
	 * bosses have already been announced this session lives only in
	 * AnnouncedBosses below, never replicated, so nothing here can desync a
	 * fight.
	 */
	void DrawBossBanner();

	/**
	 * Small/medium command reminder for the whole rover, driving or flying.
	 * Originally flight-only (Zoe, 2026-09-03: "when you are flying... a
	 * list of car related commands"), widened to IsOccupied() - Zoe,
	 * 2026-09-04: "I also think that you should have the visible commands
	 * (boost on space bar etc.)." Same row list either way; Climb/Descend
	 * simply do nothing while grounded rather than needing to be hidden.
	 * The title line still says FLIGHT CONTROLS while actually airborne.
	 */
	void DrawRoverControlsOverlay();

	/**
	 * Speed readout + bar, shown any time the local player is driving the
	 * rover - grounded or flying, same gauge both times (Zoe's ask,
	 * 2026-09-03: "please speed gauge display while driving, and make the
	 * flying one similar" - one shared gauge is the literal way to keep
	 * them similar, rather than building two that could drift apart).
	 */
	void DrawSpeedGauge();

	/**
	 * The skill practice bar (2026-11-14): pops up for a few seconds whenever one of your skills gains practice,
	 * with its Rank and how full the bar to the next Rank is, and flashes on a Rank up. UI/DCCHUD_RallyMenus.cpp.
	 */
	void DrawSkillProgress();
	TWeakObjectPtr<const class ADCCCharacterBase> SkillBarCrawler;
	TMap<FName, float> SkillBarSeen;
	TMap<FName, int32> SkillBarRanks;
	FName SkillBarId;
	double SkillBarUntil = -100.0;
	double SkillBarRankUpAt = -100.0;
	float SkillBarShown = 0.0f;

	/**
	 * Checked once per nearby living character in DrawNearbyHealthBars' own
	 * scan - piggybacked there rather than a second world iteration, since
	 * that loop already visits everyone in range every frame. Arms the banner
	 * the first time it finds an unannounced UDCCBossComponent.
	 */
	void CheckBossAnnouncement(ADCCCharacterBase* Candidate);

	/**
	 * "Waiting for other players (n/m ready)" - shown while the local crawler
	 * has reached the Collapse's exit trigger but the rest of the party has
	 * not. Reads ADCCIntroDirector::ReadyForCollapse directly (a replicated
	 * array every client already has) rather than an RPC-driven flag, so
	 * there is nothing here to keep in sync by hand.
	 */
	void DrawWaitingForPartyBanner(ADCCCharacterBase* Crawler);

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
	 * The OBJECTIVES card. Starter lines come from DCCArcade::BoardSteps.
	 * Floor quests sit under the same header in the Den, the hub, and a live
	 * dungeon run. Repo Rally draws the card mid-left, off THE BOARD and CREW.
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
	 * The NPC menu (Talk/Shop/Leave), drawn when the local crawler has one
	 * open - same panel styling as DrawInteractionMenu, kept as its own
	 * function for the same reason the two backing option lists are kept
	 * separate on ADCCCharacterBase (see NPCInteractionTarget's comment).
	 */
	void DrawNPCInteractionMenu(ADCCCharacterBase* Crawler);

	/** Casino Cascade: the hand you are sitting, and the dots over everyone who is. */
	void DrawCasino(ADCCCharacterBase* Crawler);

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
	 * The vendor menu: opened on a shopkeeper NPC (ADCCVendorNPC) instead of a
	 * corpse or a chest. One flat list - equipment rows, then drinks, then
	 * Leave - no itemised sub-page, unlike DrawLootMenu, because there is
	 * nothing here to take some-and-leave-some of.
	 */
	void DrawVendorMenu(ADCCCharacterBase* Crawler);

	/**
	 * The Oathgate confirm prompt: "Enter the gate?" with Enter/Turn Away rows.
	 * Same wheel-to-choose, E-to-confirm shape as the loot and vendor menus,
	 * just two fixed rows and no itemised sub-page.
	 */
	void DrawOathgateMenu(ADCCCharacterBase* Crawler);

	/** The B-key emote picker: wheel to choose a performance from
	 *  EmoteAnimations, E to actually play it. */
	void DrawEmoteMenu(ADCCCharacterBase* Crawler);
	/** Reaction bubbles over heads (Characters/DCCReactions). */
	void DrawReactions();
	/** The Den: a slim strip of game logos with the players on each pad, in place of the long status list. */
	void DrawDenStrip(const class ADCCArcadeLobby* Lobby);
	/** How to play the game on screen, once per game per session (Arena->GetTip). */
	void DrawArcadeTip(const class ADCCArcadeArena* Arena);
	TMap<FName, double> TipShownAt;
	/** The health bar shows only when below full or recently hit. */
	int32 LastSeenHitPoints = 0;
	double HealthShownUntil = 0.0;
	bool bHealthBarUp = false;
	/** SETTINGS: show the quick-reaction key rows that have no key yet. */
	bool bShowAllQuickKeys = false;
	/** The podium prompt, the summon panel and the reveal banner. */
	void DrawSummon(const class ADCCArcadeLobby* Lobby);
	int32 SummonTube = -1;

	// The scratchcard (2026-11-21). The server dealt the card; these are the coating patches this screen has rubbed off.
	void DrawScratchCard(class ADCCCasinoCascade* Casino, ADCCCharacterBase* Crawler);
	void SetScratchOpen(bool bOpen);
	void ClaimScratchCard();
	bool bScratchOpen = false;
	uint8 ScratchSerialSeen = 0;
	uint16 ScratchMask[9] = {};
	bool bScratchClaimSent = false;
	/** Pulls waiting for (or showing) their banner, each from when its friend lands. Pulls in quick succession each get
	 *  their turn rather than resetting one banner's timer (the gacha ad's button-mashing showed none until the last). */
	struct FSummonBanner { FName Friend; uint8 Tier = 0; uint8 Variant = 0; FString Weapon; double At = 0.0; };
	TArray<FSummonBanner> SummonBanners;
	/** How far down the Den strip is: 0 tucked up off the top, 1 all the way down. */
	float DenStripShown = 0.0f;
	/** The SETTINGS tab: keys (UI/DCCControls) and each moment's bubble and move (DCCReactions). */
	float DrawSettingsTab(float X, float Y, ADCCCharacterBase* Crawler);
	void HandleSettingsClick(const FString& Command);
	/** Reads the remappable keys every frame (and a key being set, while one is). */
	void PollRemappableKeys();
	/** The action waiting for a key on the SETTINGS tab, or -1. */
	int32 CapturingAction = -1;
	TArray<uint8> SettingsChoices;

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

	/** Party page: who is with you, their health, whether they are holding, and
	 *  (2026-08-23) the Party chat log/input below the roster. */
	float DrawParty(float X, float Y, ADCCCharacterBase* Crawler);

	/**
	 * Talk page (renamed from Chat 2026-08-23 - see EDCCMenuTab::Talk): nearby
	 * friendly or neutral faces, pick one to start talking.
	 *
	 * Deliberately just a picker for the conversation menu that already exists
	 * (DrawInteractionMenu, always drawn regardless of this tab) rather than a
	 * second dialogue system - AskAbout/Persuade/Intimidate/Chatter(the NPC
	 * verb) are the whole of "talk" here. There is no shop option anywhere in
	 * that menu, and this page must never grow one; picking a name opens a
	 * conversation, not a storefront.
	 */
	float DrawTalk(float X, float Y, ADCCCharacterBase* Crawler);

	/** Equipment page content, folded into the Character tab. */
	float DrawEquipment(float X, float Y, ADCCCharacterBase* Crawler);

	/** Panel sections. Each returns the Y it finished at, so they stack. */
	float DrawStats(float X, float Y, ADCCCharacterBase* Crawler);
	float DrawSkills(float X, float Y, ADCCCharacterBase* Crawler);
	float DrawInventory(float X, float Y, ADCCCharacterBase* Crawler);
	float DrawAchievements(float X, float Y, ADCCCharacterBase* Crawler);
	float DrawFeed(float X, float Y);

	// -------------------------------------------------------------------------
	// CHAT TABS (2026-08-23) - Feed/Chatter/Party/Allies/Whispers, per Zoe's
	// naming. Feed and Chatter here are the *tab* versions; the always-on Feed
	// box above is unaffected by any of this.
	// -------------------------------------------------------------------------

	/** Feed tab: a bigger scrollback of the same EventLog the always-on box
	 *  shows, for catching up on lines that scrolled past. */
	float DrawFeedTab(float X, float Y);

	/** Chatter tab: simulated audience. Requests a fresh batch the moment this
	 *  tab is opened (see DrawHUD's tab-transition check), not on a timer. */
	float DrawChatterTab(float X, float Y);

	/** Allies tab: roster of fist-bumped-and-still-alive crawlers, then the
	 *  Allies channel log/input below it - same shape as DrawParty. */
	float DrawAlliesTab(float X, float Y, ADCCCharacterBase* Crawler);

	/** Whispers tab: a name field (WhisperTargetName) gating a 1:1 log/input. */
	float DrawWhispersTab(float X, float Y, ADCCCharacterBase* Crawler);

	/** Social page: uploads, channel numbers, friends, and the party / allies / whisper threads. */
	float DrawSocial(float X, float Y, ADCCCharacterBase* Crawler);

	/** 0 home, 1 party, 2 allies, 3 whisper, 4 your post, 5 a friend's posts, 6 one of theirs. */
	int32 SocialView = 0;
	int32 SocialPost = -1;
	FString SocialFriend;

	/**
	 * Shared message-list renderer for Party/Allies/Whispers/Chatter.
	 *
	 * For Whispers, TargetFilter must be non-empty and the list is further
	 * narrowed to messages where the local crawler is sender or recipient of
	 * that specific target - everyone else's whispers never reach this HUD's
	 * filter step at all, which is what makes it "private" on a shared array
	 * (see AddChatMessage's own comment on why that's an acceptable model for
	 * a trusted five-person table). A dead sender's messages, past or future,
	 * get a "This message sent/received from a dead crawler" header - checked
	 * live via SenderCharacter->IsDead(), not stamped at send time.
	 */
	float DrawChannelMessages(float X, float Y, EDCCChatChannel Channel, ADCCCharacterBase* Crawler,
		int32 LineCount, const FString& TargetFilter = FString());

	/**
	 * Word-by-word line renderer used by DrawChannelMessages: any word shaped
	 * like ":TOKEN:" that DCCChatter::FindEmoteIcon recognises draws as a
	 * real icon instead of the literal bracketed text - what the Chatter
	 * tab's `Emote` bucket rows (":BORANT_W: :BORANT_W:" and friends) were
	 * always meant to read as. Unrecognised words, including unmapped
	 * tokens, still draw as plain text, so nothing silently disappears.
	 * Wraps to a new line at MaxX the same way WrapText does for plain text,
	 * just measured per-word instead of per-character since icons and text
	 * have different widths.
	 */
	float DrawChatLineWithEmotes(float X, float Y, float MaxX, const FString& Line, const FLinearColor& Colour);

	/** The "type a message..." row under a channel. Clicking it opens
	 *  ChatInputWidget in Message mode for that channel. */
	float DrawChatSendRow(float X, float Y, EDCCChatChannel Channel, ADCCCharacterBase* Crawler,
		const FString& HitBoxSuffix);

	/** Creates ChatInputWidget on first use and positions/shows/focuses it. */
	void OpenChatInputForChannel(EDCCChatChannel Channel, EDCCChatInputMode Mode, float X, float Y);

	void CloseChatInput();

	UFUNCTION()
	void HandleChatInputSubmitted(const FString& SubmittedText);

	UFUNCTION()
	void HandleChatInputCancelled();

private:
	bool bShowMenu = false;

	/** Real time until which a second SAFE ROOM click leaves the game. 0 means it is not armed. */
	double BailArmedUntil = 0.0;

	struct FAchieveToast
	{
		FString Title;
		FString Reward;
		double StartedAt = -1.0;
	};
	TArray<FAchieveToast> AchieveToasts;
	void DrawAchievementPopups();

	/** Which achievement row is expanded. None means no card is showing. */
	FName SelectedAchievementId;

	/**
	 * The Talk tab's last-drawn list, indexed by the hit boxes DrawTalk just
	 * registered - "Talk_2" means ChatCandidates[2]. Rebuilt fresh every frame
	 * DrawTalk runs, same lifetime as any other Canvas hit box.
	 */
	UPROPERTY(Transient)
	TArray<TWeakObjectPtr<ADCCCharacterBase>> ChatCandidates;

	EDCCFeedFilter FeedFilter = EDCCFeedFilter::All;

	EDCCMenuTab MenuTab = EDCCMenuTab::Character;

	/** So DrawHUD can tell "just switched to Chatter" from "still on Chatter"
	 *  and only request a batch on the transition. */
	EDCCMenuTab PreviousDrawnMenuTab = EDCCMenuTab::Character;

	/** Next world-time the open Chatter tab should pull another comment or two.
	 *  Local to this client's HUD; the server rate-limits what it honours. */
	float NextChatterTrickleTime = 0.0f;

	/** Base gap between trickle pulls, jittered per pull so the channel doesn't
	 *  tick like a clock. */
	UPROPERTY(EditDefaultsOnly, Category = "HUD")
	float ChatterTrickleIntervalSeconds = 2.5f;

	/** How recent the last Combat/Death/Encounter Feed entry has to be for chat
	 *  to count as reacting to something happening right now, rather than to
	 *  the fight that ended two minutes ago. */
	UPROPERTY(EditDefaultsOnly, Category = "HUD")
	float ExcitingChatterWindowSeconds = 12.0f;

	/** Trickle interval is multiplied by this while a moment is exciting - well
	 *  under 1 so chat visibly speeds up instead of just adding a bit more
	 *  volume at the same pace. */
	UPROPERTY(EditDefaultsOnly, Category = "HUD")
	float ExcitingChatterSpeedupFactor = 0.35f;

	// Minimized by default since 2026-11-13 (Zoe: the full box was distracting). K cycles it as before.
	EDCCFeedVisibility FeedVisibility = EDCCFeedVisibility::Minimized;

	/** Lazily created C++-only Slate text box - see DCCChatInputWidget.h for
	 *  why this exists instead of a WBP. */
	UPROPERTY(Transient)
	TObjectPtr<UDCCChatInputWidget> ChatInputWidget;

	/** Created on first open, kept for the life of the HUD. See UI/DCCSessionMenuWidget.h. */
	UPROPERTY(Transient)
	TObjectPtr<UDCCSessionMenuWidget> SessionMenu;

	UPROPERTY(Transient)
	TObjectPtr<UDCCCosmeticShopWidget> CosmeticShop;

	/** PLAY AGAIN / BACK TO THE DEN, while an arcade arena's round is over. */
	UPROPERTY(Transient)
	TObjectPtr<UDCCRoundOverWidget> RoundOver;
	UPROPERTY(Transient)
	TObjectPtr<class UDCCCageVoteWidget> CageVote;
	UPROPERTY(Transient)
	TObjectPtr<class UDCCStarterKitWidget> StarterKit;
	/** The new-player guide (2026-11-16): 0 open the menu, 1 open loot boxes, 2 equip something, 3 done. Per crawler, on this PC. */
	int32 GuideStep = -1;
	FString GuideKey;
	FString GuideGear;
	void DrawNewPlayerGuide(class ADCCCharacterBase* Crawler);
	void UpdateRoundOver();

	/** Running as the standalone pose bench (DCCArcade::IsBenchTool). */
	bool bBenchTool = false;
	FTimerHandle BenchToolTimer;

	/** The front door's character select (UI/DCCCharacterSelectWidget.h). Only exists there. */
	UPROPERTY(Transient)
	TObjectPtr<UDCCCharacterSelectWidget> CharacterSelect;
	void OpenCharacterSelect();

	/** Cursor and input mode for whatever is open right now (Tab menu, DM panel, party menu). */
	void ApplyCursorForOpenPanels();
	/** True while the downed screen is showing a clickable bleed-out button. */
	bool bCursorForDowned = false;

	UPROPERTY(Transient)
	TObjectPtr<UDCCLootScreenInput> LootScreenInput;

	UPROPERTY(Transient)
	TObjectPtr<UDCCGearBenchWidget> GearBench;

	UPROPERTY(Transient)
	TObjectPtr<UDCCShopWidget> ShopWidget;

	/** Opens or closes the shop to match the crawler's vendor target. */
	void SyncShop(ADCCCharacterBase* Crawler);

	bool bLootScreenOpen = false;
	int32 LootSelected = 0;
	float LootScroll = 0.0f;
	double LootHoldStart = -1.0;
	double LootHoldAllStart = -1.0;
	bool bLootHoldOne = false;
	bool bLootHoldAll = false;
	/** Earliest time the next box may start charging (a reveal is playing). */
	double LootNextChargeAt = 0.0;
	/** Requested, not yet back from the server, per tier. */
	TArray<int32> LootInFlight;
	TArray<int32> LootSeenCounts;
	double LootRequestSentAt = -1.0;
	FDCCLootBoxReveal LootReveal;
	double LootRevealStart = -1.0;
	FString LootNotice;
	double LootNoticeUntil = 0.0;
	/** Screen rects of the boxes drawn last frame, for mouse picking. Index matches the row. */
	TArray<FBox2D> LootBoxRects;

	void DrawLootBoxScreen();
	void BuildLootRow(TArray<EDCCLootBoxTier>& OutRow) const;
	void SendLootOpen(EDCCLootBoxTier Tier, int32 Count, bool bAll);
	void DrawGiftBox(float CentreX, float BaseY, float Size, const FLinearColor& Colour, float Alpha, float Glow);
	/** A real chest (UI/DCCLootStage), standing on BaseY; falls back to DrawGiftBox if the stage is missing. */
	void DrawChest(float CentreX, float BaseY, float Size, const FLinearColor& Colour, float Alpha, float Glow);
	/** Text cut into lines that fit MaxWidth (at most MaxLines; the last one ends in "..." if it had to be cut). */
	TArray<FString> WrapToWidth(const FString& Text, class UFont* Font, float MaxWidth, int32 MaxLines);
	/** The loot screen's photo studio, and the picture of each item in the current reveal. */
	UPROPERTY(Transient)
	TObjectPtr<class ADCCLootStage> LootStage;
	TArray<TWeakObjectPtr<class UTexture>> RevealPictures;
	class ADCCLootStage* GetLootStage();
	void DrawLootReveal(double Now);

	EDCCChatInputMode ChatInputMode = EDCCChatInputMode::None;

	/** Which channel ChatInputWidget's next submit goes to, when in Message mode. */
	EDCCChatChannel ActiveChatChannel = EDCCChatChannel::Party;

	/** Set by submitting the Whispers tab's target field; empty means no
	 *  Whispers conversation is open yet. */
	FString WhisperTargetName;

	/** Canvas->SizeY as of the last DrawHUD, so NotifyHitBoxClick (which can
	 *  fire outside of DrawHUD, when Canvas is not guaranteed valid) has
	 *  something safe to position the chat input widget against. */
	float CachedViewportHeight = 720.0f;

	bool bShowAdminPanel = false;
	EDCCAdminTab AdminTab = EDCCAdminTab::Self;

	/** Paging for the skill list, which is far longer than one screen. */
	int32 AdminSkillPage = 0;

	FText NoticeText;
	float NoticeExpiryTime = 0.0f;

	/** Draws the transient centre-screen notice, if one is live. */
	void DrawNotice();

	/** Everything DrawHUD used to do. Split out so the System window can always draw last, past every early return. */
	void DrawHUDContents();

	/** The isekai pop-up. See UDCCOnboardingComponent for the queue behind it. */
	void DrawSystemWindow(ADCCCharacterBase* Crawler);

	/** Last tab a first-visit tutorial was requested for, so it is asked once per change, not every frame. */
	uint8 LastTutorialTab = 0xFF;

	/** How long the LEVEL BOSS card stays up, including its fade in/out. */
	UPROPERTY(EditDefaultsOnly, Category = "DCC|UI", meta = (ClampMin = "1.0"))
	float BossBannerDurationSeconds = 4.5f;

	FText BossBannerName;
	float BossBannerExpiryTime = 0.0f;

	/** Boss actors already announced this session - client-local, never
	 *  replicated, so each boss only gets the banner once per HUD lifetime. */
	UPROPERTY(Transient)
	TArray<TWeakObjectPtr<AActor>> AnnouncedBosses;

	/**
	 * A "QUEST COMPLETE" card, same poll-and-compare shape as the boss
	 * banner above: every frame, compare UDCCQuestComponent's own
	 * LastCompletedQuestId (replicated) against LastShownQuestCompleteId
	 * (client-local); a mismatch means a quest finished since the last
	 * check, so show the card and update the cache. Added 2026-09-03 so
	 * quest completion is undeniable on screen instead of a Feed line easy
	 * to miss - see UDCCQuestComponent::LastCompletedQuestId's own comment
	 * for why this exists.
	 */
	void DrawQuestCompleteBanner();

	/** The arcade's top-centre panel: whichever game's arena is loaded (Arcade/DCCArcadeArena). */
	void DrawArcadeStatus();
	/** A slim XP bar along the bottom of every game (2026-11-22): level, progress, a +XP flash, unspent stat points. */
	void DrawXpStrip(class ADCCCharacterBase* Crawler);
	int32 XpStripLastLevel = -1;
	int32 XpStripLastXp = -1;
	int32 XpStripGain = 0;
	double XpStripGainAt = -100.0;
	/** The gold QUEST line. Same banner in every game, including the dungeon. */
	void DrawSeriesQuestBanner(FName GameId);

	/** Dead Air's body-cam overlay while the local crawler is recording (Arcade/DCCArcadeHaunt). */
	void DrawBodyCam();
	/** Dead Air: the note you are reading off a body, on a scrap of paper (2026-11-15). */
	void DrawMineNote();
	/** Dead Air's end of round: data extraction bars over the heads, then the highlight reel (2026-11-15). */
	void DrawMineReel();
public:
	/** A trailer is rolling (DCC.HauntTrailer): draw only the extraction and the feed. */
	static inline bool bTrailerMinimal = false;
private:

	// --- Item pop-ups (UI/DCCHUD_ItemToasts.cpp) ---------------------------
	// Watches the owning crawler and queues a card for anything new: an item in
	// the bag, a potion, a loot box, a newly equipped piece, gold. Diffing the
	// replicated state rather than adding an RPC means every source (pickups,
	// chests, corpses, payouts, achievements) gets a card with no new plumbing.
	void WatchForNewItems(ADCCCharacterBase* Crawler);
	void PushItemToast(FDCCItemToastCard&& Card);
	void DrawItemToasts();

	/** The arcade game's arena, if this level is one (the HUD trims itself there). */
	const class ADCCArcadeArena* GetArcadeArena() const;

	/** Fourteener: boosts and carried artifacts, bottom right. Tools are on the belt. */
	void DrawClimbKit(class ADCCCharacterBase* Crawler);

	/** Repo Rally: tools ([Q]/[F]) and the shift readout. Read off the car, which is the possessed pawn. */
	void DrawRallyKit(const class ADCCRover* Rover);

	// Repo Rally's screens: work order, board, crew, truck, shift report (UI/DCCHUD_RallyMenus.cpp, 2026-11-14).
	void DrawRallyScreens(const class ADCCArcadeKart* Kart);
	void DrawRallyBriefing(const class ADCCArcadeKart* Kart);
	void DrawRallyClock(const class ADCCArcadeKart* Kart);
	void DrawRallyBoard(const class ADCCArcadeKart* Kart);
	void DrawRallyCrew(const class ADCCArcadeKart* Kart);
	void DrawRallyTruck(const class ADCCArcadeKart* Kart, const class ADCCRover* Rover);
	void DrawRallyReport(const class ADCCArcadeKart* Kart);
	void RallyPanel(float X, float Y, float W, float H, const FLinearColor& Accent);
	/** A key cap; returns its width. */
	float RallyKey(float X, float Y, const FString& Key, float S);
	void RallyBar(float X, float Y, float W, float H, float Fraction, const FLinearColor& Colour);
	/** A coloured label in a tinted box; returns its width (2026-11-14). */
	float RallyPill(float X, float Y, const FString& Text, const FLinearColor& Colour, float S);
	/** A small arrow pointing at Angle (degrees, 0 = up the screen). */
	void RallyArrow(float CX, float CY, float Size, float Angle, const FLinearColor& Colour);
	/** Tags over angry owners, and the red edge when your truck takes a hit. */
	void DrawRallyTrouble(const class ADCCArcadeKart* Kart);
	/**
	 * Repo Rally text in crisp Slate fonts (Roboto) instead of the engine's bitmap fonts scaled up (2026-11-14).
	 * Same arguments as DrawText / GetTextSize, so the size of a line matches what the bitmap font gave.
	 */
	void RallyText(const FString& Text, const FLinearColor& Colour, float X, float Y, class UFont* Font, float Scale = 1.0f, bool bShadow = true);
	void RallyTextSize(const FString& Text, float& OutW, float& OutH, class UFont* Font, float Scale = 1.0f);
	float RallyLastHull = -1.0f;
	double RallyHurtAt = -100.0;

	/** Repo Rally: the city, bottom left. Marks, crews, traffic, gas. */
	void DrawRepoMinimap(const class ADCCRover* Self);

	/** Repo Rally: the centre reticle while the hook camera is up. */
	void DrawHookReticle(const class ADCCRover* Rover);

	/** What the nearest talking NPC is saying, as a subtitle above the hotbar (2026-11-13). */
	void DrawNPCSubtitle();
	void DrawItemIcon(uint8 Icon, float X, float Y, float Size, const FLinearColor& Colour, float Alpha);

	TArray<FDCCItemToastCard> ItemToastQueue;
	TArray<FDCCItemToastCard> ItemToastsShowing;
	TWeakObjectPtr<ADCCCharacterBase> ItemToastWatched;
	int32 SeenItemCount = 0;
	TMap<FName, int32> SeenConsumables;
	TArray<int32> SeenLootBoxes;
	TMap<uint8, FName> SeenEquipped;
	int32 SeenGold = 0;
	/** False until the gear this pawn arrived with has been noted, so a travel cannot replay those cards. */
	bool bItemToastSettled = false;
	bool bItemToastSawCarryIn = false;
	double ItemToastWatchStart = 0.0;
	double ItemToastStableSince = 0.0;
	uint32 ItemToastSignature = 0;

	UPROPERTY(EditDefaultsOnly, Category = "DCC|UI", meta = (ClampMin = "1.0"))
	float QuestCompleteBannerDurationSeconds = 4.0f;

	FText QuestCompleteTitle;
	float QuestCompleteBannerExpiryTime = 0.0f;
	FName LastShownQuestCompleteId;

	/**
	 * "GAUNTLET n / N" while a gauntlet is Active, plus a fading "GAUNTLET
	 * CLEARED" card (same fade shape as DrawQuestCompleteBanner) on the
	 * Dormant/Active -> Victory transition. CachedGauntlet is refreshed by
	 * a cheap periodic actor search rather than every frame, the same
	 * reasoning as DrawQuestCompleteBanner's GameState component lookup:
	 * there is normally exactly one ADCCGauntletEncounter in the level, so
	 * finding it once every couple of seconds is enough.
	 */
	void DrawGauntletProgress();

	UPROPERTY(Transient)
	TWeakObjectPtr<ADCCGauntletEncounter> CachedGauntlet;

	float GauntletCacheRefreshTime = 0.0f;
	EDCCGauntletState LastObservedGauntletState = EDCCGauntletState::Dormant;
	float GauntletVictoryBannerExpiryTime = 0.0f;

	/** Whether this entry should draw under the current preset. Takes the whole
	 *  entry rather than just its Category because MyRolls needs to compare
	 *  Instigator/RollTarget against the local crawler. */
	bool PassesFeedFilter(const FDCCEventLogEntry& Entry, const ADCCCharacterBase* LocalCrawler) const;

	/** Name shown next to the feed so the active preset is never a guess. */
	FString GetFeedFilterLabel() const;

	/**
	 * Registers a clickable hit box over a Feed line whose roll belongs to the
	 * local crawler (CheckAbility set, Instigator is them), then draws the
	 * line - shared by the always-on box and the Feed tab so both behave
	 * identically. HitBoxIndex just needs to be unique within one frame's draw
	 * pass; the entry's own Ability/SkillId ride along in the hit box name.
	 */
	float DrawFeedLine(const FDCCEventLogEntry& Entry, float X, float Y, const FLinearColor& Colour,
		const ADCCCharacterBase* LocalCrawler, int32 HitBoxIndex);

	/** The floating panel shown after clicking a roll - GetAbilityScoreBreakdown,
	 *  and GetSkillRankBreakdown when the roll had a Skill. Drawn last so it
	 *  sits over everything else. */
	void DrawRollBreakdown(ADCCCharacterBase* LocalCrawler);

	/** Which roll's breakdown is currently pinned open. MAX means none. */
	EDCCAbility BreakdownAbility = EDCCAbility::MAX;
	FName BreakdownSkillId;

	/** Lazily resolved and cached - there is exactly one per level, so this
	 *  finds it once rather than searching every frame. */
	TWeakObjectPtr<ADCCIntroDirector> CachedIntroDirector;

	/** Finds (and caches) the level's ADCCIntroDirector, or null if none is placed. */
	ADCCIntroDirector* GetIntroDirector();
};
