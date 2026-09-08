#pragma once

#include "CoreMinimal.h"
#include "GameFramework/HUD.h"
#include "UI/DCCChatTypes.h"
#include "Stats/DCCAbilityTypes.h"
#include "Rooms/DCCGauntletEncounter.h"
#include "DCCHUD.generated.h"

class ADCCCharacterBase;
class ADCCIntroDirector;
class UDCCStatsComponent;
class UDCCChatInputWidget;
class UDCCBossComponent;
enum class EDCCLogCategory : uint8;
struct FDCCEventLogEntry;

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

	/** Opens or closes the DM panel, and takes the cursor with it. */
	UFUNCTION(BlueprintCallable, Category = "DCC|Admin")
	void ToggleAdminPanel();

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
	 * The NPC menu (Talk/Shop/Leave), drawn when the local crawler has one
	 * open - same panel styling as DrawInteractionMenu, kept as its own
	 * function for the same reason the two backing option lists are kept
	 * separate on ADCCCharacterBase (see NPCInteractionTarget's comment).
	 */
	void DrawNPCInteractionMenu(ADCCCharacterBase* Crawler);

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

	EDCCFeedVisibility FeedVisibility = EDCCFeedVisibility::Pinned;

	/** Lazily created C++-only Slate text box - see DCCChatInputWidget.h for
	 *  why this exists instead of a WBP. */
	UPROPERTY(Transient)
	TObjectPtr<UDCCChatInputWidget> ChatInputWidget;

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
