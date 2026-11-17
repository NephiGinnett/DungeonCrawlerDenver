#include "UI/DCCHUD.h"
#include "UObject/StrongObjectPtr.h"
#include "Arcade/DCCChopShop.h"
#include "Engine/TextureRenderTarget2D.h"
#include "CanvasItem.h"
#include "Arcade/DCCRallyThug.h"
#include "GlobalRenderResources.h"
#include "CanvasItem.h"
#include "Onboarding/DCCOnboardingComponent.h"

#include "Characters/DCCCharacterBase.h"
#include "Stats/DCCStatsComponent.h"
#include "Stats/DCCRuleset.h"
#include "Skills/DCCSkillComponent.h"
#include "Items/DCCInventoryComponent.h"
#include "Items/DCCCarryComponent.h"
#include "Items/DCCPickup.h"
#include "Items/DCCLootSource.h"
#include "Items/DCCLootTypes.h"
#include "Progression/DCCAchievementComponent.h"
#include "Equipment/DCCEquipmentComponent.h"
#include "Dialogue/DCCVendorNPC.h"
#include "Engine/DataTable.h"
#include "Progression/DCCCrowdCrawler.h"
#include "Characters/DCCAllyCharacters.h"
#include "Characters/DCCEnemyCharacter_NuclearFamily.h"
#include "Characters/DCCEnemyCharacter_TestBoss.h"
#include "Characters/DCCEnemyCharacter_Voldemortgage.h"
#include "Characters/DCCEnemyCharacter_Wildlife.h"
#include "Characters/DCCEnemyCharacter_Mine.h"
#include "Characters/DCCEnemyCharacter_SurveyDrone.h"
#include "Characters/DCCEnemyCharacter_Corporate.h"
#include "Characters/DCCEnemyCharacter_DebtEater.h"
#include "Quests/DCCQuestComponent.h"
#include "Quests/DCCQuestMarker.h"
#include "Combat/DCCGameState.h"
#include "Combat/DCCBossComponent.h"
#include "Intro/DCCIntroDirector.h"
#include "UI/DCCEventLogTypes.h"
#include "UI/DCCChatterTypes.h"
#include "UI/DCCChatInputWidget.h"
#include "UI/DCCSessionMenuWidget.h"
#include "UI/DCCCharacterSelectWidget.h"
#include "Dialogue/DCCDialogueNPC.h"
#include "UI/DCCCosmeticShopWidget.h"
#include "UI/DCCRoundOverWidget.h"
#include "UI/DCCCageVoteWidget.h"
#include "UI/DCCStarterKitWidget.h"
#include "Equipment/DCCEquipmentTypes.h"
#include "Arcade/DCCArcadeArena.h"
#include "Arcade/DCCWalletComponent.h"
#include "Arcade/DCCClimbItems.h"
#include "Arcade/DCCRallyItems.h"
#include "Arcade/DCCArcadeGames.h"
#include "Engine/GameViewportClient.h"
#include "GameFramework/GameStateBase.h"
#include "UI/DCCLootScreenInput.h"
#include "Bench/DCCGearBenchWidget.h"
#include "UI/DCCShopWidget.h"
#include "Arcade/DCCArcadeArena.h"
#include "Arcade/DCCArcadeHaunt.h"
#include "GameFramework/PlayerState.h"
#include "Arcade/DCCArcade.h"
#include "EngineUtils.h"
#include "Session/DCCSession.h"
#include "Blueprint/WidgetBlueprintLibrary.h"
#include "TimerManager.h"
#include "HAL/IConsoleManager.h"
#include "Blueprint/UserWidget.h"
#include "Rooms/DCCClimbPoint.h"
#include "Vehicles/DCCRover.h"
#include "Engine/Canvas.h"
#include "CanvasItem.h"
#include "Engine/Font.h"
#include "Engine/Engine.h"
#include "Engine/Texture2D.h"
#include "Engine/Engine.h"
#include "EngineUtils.h"
#include "Components/CapsuleComponent.h"
#include "Arcade/DCCFireworks.h"
#include "GameFramework/PlayerController.h"
#include "Arcade/DCCSpray.h"
#include "Characters/DCCReactions.h"
#include "UI/DCCControls.h"
#include "UI/DCCGameLogos.h"

namespace
{
	constexpr float LineHeight = 20.0f;
	constexpr float SectionGap = 14.0f;

	const FLinearColor HeadingColour(1.0f, 0.82f, 0.35f);
	const FLinearColor BodyColour(0.92f, 0.94f, 1.0f);
	const FLinearColor DimColour(0.62f, 0.66f, 0.78f);

	/**
	 * One line describing a rolled loot entry for the itemised submenu.
	 *
	 * FDCCRolledLoot itself carries no display name - just a class, a gold
	 * amount and an equipment id - so this resolves one: the equipment table's
	 * row name when it is gear, "N Gold" when it is a bank-on-pickup coin drop,
	 * and the pickup class's own DisplayName default as the fallback for
	 * anything else (a thrown-prop-flavoured drop, say).
	 */
	FString DescribeRolledLoot(const FDCCRolledLoot& Rolled, const ADCCCharacterBase* Crawler)
	{
		if (!Rolled.EquipmentItemId.IsNone())
		{
			if (const UDCCEquipmentComponent* Gear = Crawler ? Crawler->Equipment : nullptr)
			{
				FDCCEquipmentDef Def;
				if (Gear->FindItem(Rolled.EquipmentItemId, Def) && !Def.DisplayName.IsEmpty())
				{
					return Def.DisplayName.ToString();
				}
			}

			return Rolled.EquipmentItemId.ToString();
		}

		if (Rolled.Gold > 0)
		{
			return FString::Printf(TEXT("%d Gold"), Rolled.Gold);
		}

		if (Rolled.PickupClass)
		{
			const ADCCPickup* Default = GetDefault<ADCCPickup>(Rolled.PickupClass);
			if (Default && !Default->DisplayName.IsEmpty())
			{
				return Default->DisplayName.ToString();
			}

			return Rolled.PickupClass->GetName();
		}

		return TEXT("Something");
	}
}

ADCCCharacterBase* ADCCHUD::GetOwningCrawler() const
{
	return PlayerOwner ? Cast<ADCCCharacterBase>(PlayerOwner->GetPawn()) : nullptr;
}

ADCCRover* ADCCHUD::GetOwningRover() const
{
	return PlayerOwner ? Cast<ADCCRover>(PlayerOwner->GetPawn()) : nullptr;
}

void ADCCHUD::ToggleMenu()
{
	if (!bShowMenu && !IsHudFeatureUnlocked(EDCCHudFeature::MenuCharacter))
	{
		ShowNotice(NSLOCTEXT("DCC", "MenuNotIssued", "You have no interface. You are, as far as anyone can tell, just a person."));
		return;
	}

	bShowMenu = !bShowMenu;

	// Closing clears the expanded card. Reopening to whatever was last clicked
	// half an hour ago is worse than reopening clean.
	if (!bShowMenu)
	{
		SelectedAchievementId = NAME_None;
	}

	if (!PlayerOwner)
	{
		return;
	}

	// A Canvas HUD has no cursor and no click routing of its own - both have to
	// be switched on for AddHitBox regions to receive anything at all.
	// Shared with the DM panel and the party menu, so closing one never takes
	// the cursor away from another that is still open.
	ApplyCursorForOpenPanels();
}

void ADCCHUD::NotifyHitBoxClick(FName BoxName)
{
	Super::NotifyHitBoxClick(BoxName);

	const FString AsString = BoxName.ToString();

	if (AsString.StartsWith(TEXT("Set|")))
	{
		HandleSettingsClick(AsString.RightChop(4));
		return;
	}

	// The Tab menu's buttons for what used to have keys (2026-11-16).
	if (AsString.StartsWith(TEXT("Quick|")))
	{
		const FString What = AsString.RightChop(6);
		if (ADCCCharacterBase* Me = GetOwningCrawler())
		{
			if (What == TEXT("Loot") || What == TEXT("Party") || What == TEXT("Bench"))
			{
				Me->ToggleCharacterMenu(); // close the menu first: each of these opens its own screen
			}
			if (What == TEXT("Loot")) { Me->ToggleLootBoxScreen(); }
			else if (What == TEXT("Party")) { Me->ToggleSessionMenu(); }
			else if (What == TEXT("Bench")) { Me->ToggleGearBench(); }
			else if (What == TEXT("Feed")) { Me->CycleFeedVisibility(); }
			else if (What == TEXT("Log")) { Me->CycleLogFilter(); }
		}
		return;
	}

	// DM panel buttons are prefixed so they can never collide with achievement
	// ids, which are raw row names.
	if (AsString.StartsWith(TEXT("Adm|")))
	{
		HandleAdminAction(AsString.RightChop(4), GetOwningCrawler());
		return;
	}

	// Tabs are named Tab_<index> so they can be told apart from achievement rows,
	// which use the achievement id directly.
	if (AsString.StartsWith(TEXT("Tab_")))
	{
		const int32 Index = FCString::Atoi(*AsString.RightChop(4));
		if (Index >= 0 && Index < static_cast<int32>(EDCCMenuTab::MAX))
		{
			MenuTab = static_cast<EDCCMenuTab>(Index);
			SelectedAchievementId = NAME_None;
		}
		return;
	}

	// Talk rows are named Talk_<index> into ChatCandidates, the same shape as
	// tabs - an actor pointer cannot be embedded in a hit box name, but an
	// index into the list DrawTalk just built can.
	if (AsString.StartsWith(TEXT("Talk_")))
	{
		const int32 Index = FCString::Atoi(*AsString.RightChop(5));
		if (ChatCandidates.IsValidIndex(Index) && ChatCandidates[Index].IsValid())
		{
			if (ADCCCharacterBase* Crawler = GetOwningCrawler())
			{
				Crawler->OpenChat(ChatCandidates[Index].Get());
			}
		}
		return;
	}

	// "ChatOpen|<channel>" opens the Slate input box for Party/Allies, or the
	// Whispers target field. "ChatOpen|WhisperMsg" opens it for the message
	// itself once a target is already set.
	if (AsString.StartsWith(TEXT("ChatOpen|")))
	{
		const FString Sub = AsString.RightChop(9);
		const float InputX = 32.0f;
		const float InputY = CachedViewportHeight - 90.0f;

		if (Sub == TEXT("Party"))
		{
			OpenChatInputForChannel(EDCCChatChannel::Party, EDCCChatInputMode::Message, InputX, InputY);
		}
		else if (Sub == TEXT("Allies"))
		{
			OpenChatInputForChannel(EDCCChatChannel::Allies, EDCCChatInputMode::Message, InputX, InputY);
		}
		else if (Sub == TEXT("WhisperTarget"))
		{
			OpenChatInputForChannel(EDCCChatChannel::Whispers, EDCCChatInputMode::WhisperTarget, InputX, InputY);
		}
		else if (Sub == TEXT("WhisperMsg") && !WhisperTargetName.IsEmpty())
		{
			OpenChatInputForChannel(EDCCChatChannel::Whispers, EDCCChatInputMode::Message, InputX, InputY);
		}
		return;
	}

	// The Feed tab's "Re-pin" row - jumps straight to Pinned rather than
	// cycling, since the player clicked it specifically to get the box back.
	if (AsString == TEXT("FeedRePin"))
	{
		FeedVisibility = EDCCFeedVisibility::Pinned;
		return;
	}

	// "RollBreak|<uniqueIndex>|<abilityIndex>|<skillId>" - clicking the same
	// roll again closes the panel, same toggle pattern as achievement rows.
	if (AsString.StartsWith(TEXT("RollBreak|")))
	{
		TArray<FString> Parts;
		AsString.ParseIntoArray(Parts, TEXT("|"));
		if (Parts.Num() >= 4)
		{
			const EDCCAbility ClickedAbility = static_cast<EDCCAbility>(FCString::Atoi(*Parts[2]));
			const FName ClickedSkillId = Parts[3] == TEXT("None") ? NAME_None : FName(*Parts[3]);

			if (BreakdownAbility == ClickedAbility && BreakdownSkillId == ClickedSkillId)
			{
				BreakdownAbility = EDCCAbility::MAX;
				BreakdownSkillId = NAME_None;
			}
			else
			{
				BreakdownAbility = ClickedAbility;
				BreakdownSkillId = ClickedSkillId;
			}
		}
		return;
	}

	// Stowed-remains rows: emptying one at the player's feet. Server-authoritative
	// like everything else that spawns loot - this call just asks.
	if (AsString.StartsWith(TEXT("Stow|")))
	{
		const int32 Index = FCString::Atoi(*AsString.RightChop(5));
		if (ADCCCharacterBase* Crawler = GetOwningCrawler())
		{
			Crawler->DCCEmptyStowedRemains(Index);
		}
		return;
	}

	// Gear rows in the plain Items list: equipping one from the pack. Same
	// server-authoritative "this call just asks" shape as Stow| above - gear
	// no longer equips itself on pickup (see ADCCPickup::OnPickedUp), this is
	// the one place it actually happens now.
	if (AsString.StartsWith(TEXT("Equip|")))
	{
		const int32 Index = FCString::Atoi(*AsString.RightChop(6));
		if (ADCCCharacterBase* Crawler = GetOwningCrawler())
		{
			Crawler->DCCEquipItem(Index);
		}
		return;
	}

	// Stowed-drink rows: drinking one from the pack. Same server-authoritative
	// "this call just asks" shape as Stow| above.
	if (AsString.StartsWith(TEXT("Drink|")))
	{
		const int32 Index = FCString::Atoi(*AsString.RightChop(6));
		if (ADCCCharacterBase* Crawler = GetOwningCrawler())
		{
			Crawler->DCCConsumeStowedDrink(Index);
		}
		return;
	}

	// The bulk-claim row - see DrawAchievements for what it does and why it
	// is safe to call unconditionally from here.
	if (AsString == TEXT("ClaimAllLootBoxes"))
	{
		if (ADCCCharacterBase* Crawler = GetOwningCrawler())
		{
			Crawler->ClaimAndOpenAllLootBoxes();
		}
		return;
	}

	// Reward rows are prefixed for the same reason DM panel buttons are: an
	// achievement id must never be mistaken for a request to open its reward.
	if (AsString.StartsWith(TEXT("Claim|")))
	{
		if (ADCCCharacterBase* Crawler = GetOwningCrawler())
		{
			if (UDCCAchievementComponent* Achievements = Crawler->Achievements)
			{
				Achievements->RequestClaimReward(FName(*AsString.RightChop(6)));
			}
		}
		return;
	}

	// Clicking the open row closes it, so the same click both opens and dismisses.
	SelectedAchievementId = (SelectedAchievementId == BoxName) ? NAME_None : BoxName;
}

namespace
{
	/** DT_DCCSystemWindows row explaining a menu page on its first visit. */
	FName TutorialRowForTab(EDCCMenuTab Tab)
	{
		switch (Tab)
		{
		case EDCCMenuTab::Character:    return TEXT("Tab_Character");
		case EDCCMenuTab::Skills:       return TEXT("Tab_Skills");
		case EDCCMenuTab::Inventory:    return TEXT("Tab_Inventory");
		case EDCCMenuTab::Party:        return TEXT("Tab_Party");
		case EDCCMenuTab::Achievements: return TEXT("Tab_Achievements");
		case EDCCMenuTab::Chatter:      return TEXT("Tab_Chatter");
		default:                        return NAME_None;
		}
	}
}

void ADCCHUD::DrawHUD()
{
	if (IsGearBenchOpen() && GetWorld())
	{
		GearBench->TickBench(GetWorld()->DeltaRealTimeSeconds);
	}
	UpdateRoundOver();
	// The remappable keys (spray, grapple, reactions), read here because the HUD is there whatever you are driving or
	// standing in.
	if (!CharacterSelect && !bBenchTool)
	{
		PollRemappableKeys();
	}

	// The front door is a menu over a stage, and the bench tool is only the
	// bench: no vitals, feed or prompts on either.
	if (CharacterSelect || bBenchTool)
	{
		Super::DrawHUD();
		return;
	}
	if (bTrailerMinimal)
	{
		// A trailer is rolling: only the feed and the extraction, nothing of the hidden player's own HUD.
		Super::DrawHUD();
		DrawMineReel();
		return;
	}
	SyncShop(GetOwningCrawler());

	DrawHUDContents();

	if (Canvas)
	{
		DrawSystemWindow(GetOwningCrawler());
		// Last, so no panel covers a bubble (your own head sits right under the status panel).
		DrawReactions();
	}
}

void ADCCHUD::DrawHUDContents()
{
	Super::DrawHUD();

	if (!Canvas)
	{
		return;
	}

	CachedViewportHeight = Canvas->SizeY;

	ADCCCharacterBase* Crawler = GetOwningCrawler();
	WatchForNewItems(Crawler);

	// A true takeover, unlike the death screen - nothing plays on underneath
	// it, because there is nowhere further for this crawler to go right now.
	// See ADCCCharacterBase's own comment on why this is a terminal state
	// rather than a banner.
	if (Crawler && Crawler->IsAwaitingOtherCrawlers())
	{
		DrawAwaitingOtherCrawlersScreen();
		return;
	}

	// On by default: it is the only way to see what the dice did, and hiding it
	// behind a menu would defeat the point. Minimized/Hidden (2026-08-23, [K]
	// to cycle) are an explicit opt-out for exactly that reason - Pinned stays
	// the default and DrawFeed itself is unchanged.
	// The minigames show only what players need (2026-11-13): no log, no mana or
	// level, no quests, no character-menu hint; the hotbar and stamina only where
	// there is climbing.
	const ADCCArcadeArena* Arena = GetArcadeArena();

	if (!IsHudFeatureUnlocked(EDCCHudFeature::EventFeed) || Arena)
	{
		// Not issued yet - nothing at all, not even the minimized stub.
	}
	else if (FeedVisibility == EDCCFeedVisibility::Pinned)
	{
		DrawFeed(24.0f, Canvas->SizeY - (FeedLineCount + 1) * LineHeight - 24.0f);
	}
	else if (FeedVisibility == EDCCFeedVisibility::Minimized)
	{
		DrawText(FString::Printf(TEXT("Log: %s (minimized)  [K] restore"), *GetFeedFilterLabel()),
			DimColour, 24.0f, Canvas->SizeY - LineHeight - 24.0f);
	}

	// Drawn before the death check, so the bars are still on screen underneath the
	// GAME OVER overlay showing exactly zero. Watching it sit empty is the point -
	// a bar that vanishes at the moment it matters teaches the player nothing.
	if (IsHudFeatureUnlocked(EDCCHudFeature::Vitals) && (!Arena || Arena->ShowsHealth()))
	{
		DrawVitals(Crawler);
		DrawNearbyHealthBars(Crawler);
	}
	DrawSkillTargetingOverlay(Crawler);
	DrawRoomTag(Crawler);
	DrawWaitingForPartyBanner(Crawler);
	DrawSelfieViewfinder(Crawler);
	DrawBossBanner();
	DrawQuestCompleteBanner();
	DrawItemToasts();
	DrawNPCSubtitle();
	DrawArcadeStatus();
	ADCCFireworks::DrawBanners(GetWorld(), Canvas);
	DrawBodyCam();
	DrawMineNote();
	DrawMineReel();
	DrawLootBoxScreen();
	DrawGauntletProgress();
	DrawRoverControlsOverlay();
	DrawSpeedGauge();
	DrawSkillProgress();
	DrawCastingIndicator(Crawler);
	DrawRangedCrosshair(Crawler);

	// Being down is not being dead, and the overlay must not say otherwise - a
	// crawler on the floor with forty seconds left is still in the game, and
	// telling them GAME OVER would have them put the controller down.
	if (Crawler && Crawler->IsDowned())
	{
		DrawDownedScreen(Crawler);
		return;
	}

	// Death takes over the screen. Checked on the stats rather than on a null
	// pawn, because the corpse is still possessed for a moment after dying.
	if (Crawler && Crawler->Stats && !Crawler->Stats->IsAlive())
	{
		DrawDeathScreen();

		// A dead crawler still gets exactly one final chat send and can still
		// read every channel (see ADCCCharacterBase::Server_SendChatMessage) -
		// so if the Tab menu was already open, it stays reachable instead of
		// being cut off the instant death lands. Every actual gameplay action
		// below this point is separately guarded on IsDead() in
		// DCCCharacterBase, so nothing here needs to re-check that.
		if (!bShowMenu)
		{
			return;
		}
	}

	if (IsHudFeatureUnlocked(EDCCHudFeature::QuestTracker) && !Arena)
	{
		DrawQuestTracker();
	}
	DrawInteractionPrompt(Crawler);
	DrawNewPlayerGuide(Crawler);
	if (IsHudFeatureUnlocked(EDCCHudFeature::Vitals) && (!Arena || Arena->ShowsClimbHud()))
	{
		DrawStaminaBar(Crawler);
	}
	if (Arena && Arena->ShowsClimbHud())
	{
		DrawClimbKit(Crawler);
	}
	if (Arena && Arena->GetGameId() == TEXT("Kart"))
	{
		DrawRallyKit(GetOwningRover());
		if (!Arena->bRoundOver) { DrawRepoMinimap(GetOwningRover()); } // the shift report has the screen
		DrawHookReticle(GetOwningRover());
	}

	// Always on, both of them: the hotbar is useless if it only appears in a menu,
	// and a conversation has to be readable while the world is still visible.
	// The belt is drawn wherever it has something on it (2026-11-16): GetBelt leaves skills off in the arcade games
	// that do not use them, so an empty belt draws nothing.
	if (IsHudFeatureUnlocked(EDCCHudFeature::Hotbar))
	{
		DrawHotbar(Crawler);
	}
	DrawInteractionMenu(Crawler);
	DrawNPCInteractionMenu(Crawler);
	DrawLootMenu(Crawler);
	DrawVendorMenu(Crawler);
	DrawOathgateMenu(Crawler);
	DrawEmoteMenu(Crawler);
	DrawInspectedGear(Crawler);
	DrawAdminPanel(Crawler);
	DrawNotice();
	DrawRollBreakdown(Crawler);

	if (!bShowMenu || !Crawler)
	{
		if (!bShowMenu && IsHudFeatureUnlocked(EDCCHudFeature::MenuCharacter) && !Arena)
		{
			DrawText(TEXT("[Tab] Character"), DimColour, 24.0f, 24.0f);
		}
		LastTutorialTab = 0xFF;
		return;
	}

	// A tab that has not been issued yet is skipped over rather than shown
	// empty. Falls back to the first unlocked page; if there is none the menu
	// should not be open at all (ToggleMenu refuses), so just close it.
	if (!IsMenuTabUnlocked(MenuTab))
	{
		CycleMenuTab(1);
		if (!IsMenuTabUnlocked(MenuTab))
		{
			ToggleMenu();
			return;
		}
	}

	// First visit to each page: its explainer window, once per session.
	if (LastTutorialTab != static_cast<uint8>(MenuTab))
	{
		LastTutorialTab = static_cast<uint8>(MenuTab);
		if (Crawler->Onboarding)
		{
			Crawler->Onboarding->ShowLocalTutorialOnce(TutorialRowForTab(MenuTab));
		}
	}

	// A dark backing panel, otherwise light text vanishes against the sky.
	DrawRect(FLinearColor(0.02f, 0.03f, 0.06f, 0.78f), 16.0f, 16.0f, 460.0f, Canvas->SizeY - 32.0f);

	float Y = 28.0f;
	const float X = 32.0f;

	Y = DrawMenuTabs(X, Y);
	Y += SectionGap;

	// Chatter: an opening burst on the transition INTO the tab, then a slow
	// trickle for as long as it stays open, then nothing at all once it closes.
	//
	// That shape is deliberate and is the cheap way to sell "this channel has
	// been running the whole time": the burst backfills a plausible history the
	// instant you look, and the trickle keeps it alive while you watch. Nothing
	// rolls while the tab is shut, so a channel that appears to run constantly
	// costs exactly nothing whenever nobody is reading it.
	//
	// Compared against the tab drawn last frame rather than a "have I ever
	// opened this" flag, so leaving and coming back re-bursts.
	if (Crawler && MenuTab == EDCCMenuTab::Chatter)
	{
		const float Now = GetWorld() ? GetWorld()->GetTimeSeconds() : 0.0f;

		// Chat should visibly speed up when something is actually happening -
		// a boss just got hit, someone just died - rather than trickling at
		// the same flat pace through a fight as through an empty hallway.
		// Reuses the Feed's own timestamps (same Combat/Death/Encounter
		// grouping RollChatterBatch and PassesFeedFilter's CombatOnly case
		// already use) instead of a separate "is something exciting" flag.
		bool bExcitingMoment = false;
		if (const ADCCGameState* GameState = GetWorld() ? GetWorld()->GetGameState<ADCCGameState>() : nullptr)
		{
			const TArray<FDCCEventLogEntry>& Log = GameState->GetEventLog();
			for (int32 Index = Log.Num() - 1; Index >= 0; --Index)
			{
				const EDCCLogCategory Category = Log[Index].Category;
				if (Category == EDCCLogCategory::Combat || Category == EDCCLogCategory::Death
					|| Category == EDCCLogCategory::Encounter)
				{
					bExcitingMoment = (Now - Log[Index].TimeSeconds) <= ExcitingChatterWindowSeconds;
					break;
				}
			}
		}

		const float TrickleInterval = bExcitingMoment
			? ChatterTrickleIntervalSeconds * ExcitingChatterSpeedupFactor
			: ChatterTrickleIntervalSeconds;

		if (PreviousDrawnMenuTab != EDCCMenuTab::Chatter)
		{
			Crawler->Server_RequestChatterBatch(8);
			NextChatterTrickleTime = Now + TrickleInterval;
		}
		else if (Now >= NextChatterTrickleTime)
		{
			// One or two at a time normally, up to four in an exciting moment -
			// at an uneven cadence either way, since a fixed interval reads as
			// a metronome and real chat arrives in clumps.
			Crawler->Server_RequestChatterBatch(FMath::RandRange(1, bExcitingMoment ? 4 : 2));
			NextChatterTrickleTime = Now + TrickleInterval
				* FMath::FRandRange(0.6f, 1.6f);
		}
	}
	PreviousDrawnMenuTab = MenuTab;

	// One page at a time. The old panel stacked all four sections into a single
	// column that ran off the bottom of the screen on anything but a tall
	// display - and the sections nobody was looking at pushed the ones they were
	// out of view.
	switch (MenuTab)
	{
	case EDCCMenuTab::Character:
		Y = DrawStats(X, Y, Crawler);
		Y += SectionGap;
		Y = DrawEquipment(X, Y, Crawler);
		break;

	case EDCCMenuTab::Skills:
		Y = DrawSkills(X, Y, Crawler);
		break;

	case EDCCMenuTab::Inventory:
		Y = DrawInventory(X, Y, Crawler);
		break;

	case EDCCMenuTab::Party:
		Y = DrawParty(X, Y, Crawler);
		break;

	case EDCCMenuTab::Talk:
		Y = DrawTalk(X, Y, Crawler);
		break;

	case EDCCMenuTab::Achievements:
		Y = DrawAchievements(X, Y, Crawler);

		// Last, so the card draws over the menu panel rather than under it.
		DrawAchievementDetail(Crawler);
		break;

	case EDCCMenuTab::Feed:
		Y = DrawFeedTab(X, Y);
		break;

	case EDCCMenuTab::Chatter:
		Y = DrawChatterTab(X, Y);
		break;

	case EDCCMenuTab::Allies:
		Y = DrawAlliesTab(X, Y, Crawler);
		break;

	case EDCCMenuTab::Whispers:
		Y = DrawWhispersTab(X, Y, Crawler);
		break;

	case EDCCMenuTab::Settings:
		Y = DrawSettingsTab(X, Y, Crawler);
		break;

	default:
		break;
	}
}

void ADCCHUD::ShowNotice(const FText& Message, float Seconds)
{
	NoticeText = Message;
	NoticeExpiryTime = GetWorld() ? GetWorld()->GetTimeSeconds() + Seconds : 0.0f;
}

void ADCCHUD::DrawRoomTag(ADCCCharacterBase* Crawler)
{
	if (!Canvas || !Crawler || Crawler->CurrentRoomDisplayName.IsEmpty())
	{
		return;
	}

	const FString Line = Crawler->CurrentRoomDisplayName.ToString();

	float W = 0.0f, H = 0.0f;
	GetTextSize(Line, W, H, GEngine->GetMediumFont(), 1.0f);

	const float X = (Canvas->SizeX - W) * 0.5f;
	const float Y = 20.0f;

	DrawRect(FLinearColor(0.02f, 0.03f, 0.06f, 0.65f), X - 14.0f, Y - 5.0f, W + 28.0f, H + 10.0f);
	DrawText(Line, DimColour, X, Y, GEngine->GetMediumFont(), 1.0f);
}

void ADCCHUD::DrawCastingIndicator(ADCCCharacterBase* Crawler)
{
	if (!Canvas || !Crawler || !Crawler->IsCasting())
	{
		return;
	}

	const UWorld* World = GetWorld();
	const float Time = World ? World->GetTimeSeconds() : 0.0f;

	constexpr int32 DotCount = 3;
	constexpr float DotSpacing = 22.0f;
	constexpr float DotRadius = 5.0f;
	constexpr float BobHeight = 10.0f;
	constexpr float WaveSpeed = 6.0f; // radians/sec - one dot peaks roughly every 1s.

	const float TotalWidth = (DotCount - 1) * DotSpacing;
	const float CenterX = Canvas->SizeX * 0.5f;
	// A little above the vitals bar, not on top of it - this fires alongside
	// combat/downed states too (skill casts), not just the rover.
	const float BaseY = Canvas->SizeY - 140.0f;

	for (int32 Index = 0; Index < DotCount; ++Index)
	{
		// Phase-shifted per dot so they rise and fall in sequence rather than
		// bouncing in unison - the actual "ellipsis" read she asked for.
		const float Phase = Index * (PI / 3.0f);
		const float Bob = (FMath::Sin(Time * WaveSpeed - Phase) * 0.5f + 0.5f) * BobHeight;

		const float X = CenterX - TotalWidth * 0.5f + Index * DotSpacing;
		const float Y = BaseY - Bob;

		DrawRect(FLinearColor(1.0f, 0.92f, 0.70f, 0.9f),
			X - DotRadius, Y - DotRadius, DotRadius * 2.0f, DotRadius * 2.0f);
	}
}

void ADCCHUD::DrawRangedCrosshair(ADCCCharacterBase* Crawler)
{
	if (!Canvas || !Crawler || !Crawler->Equipment || Crawler->Equipment->GetAttackRangeOverride() <= 0.0f)
	{
		return;
	}

	// Same trace AttemptRangedAttack itself fires with (via
	// FindHostileUnderCrosshair), so "is the reticle lit up" always agrees
	// with "would this shot actually land" - a crosshair that lies about
	// the reticle is worse than no crosshair at all.
	const bool bOnTarget = Crawler->FindHostileUnderCrosshair(Crawler->GetEffectiveAttackRange()) != nullptr;

	const float CenterX = Canvas->SizeX * 0.5f;
	const float CenterY = Canvas->SizeY * 0.5f;

	const FLinearColor Colour = bOnTarget
		? FLinearColor(1.0f, 0.2f, 0.15f) : FLinearColor(1.0f, 1.0f, 1.0f, 0.8f);
	constexpr float GapRadius = 6.0f;
	constexpr float ArmLength = 8.0f;
	constexpr float Thickness = 2.0f;

	// Four short arms with a gap at the centre, rather than a solid plus -
	// reads as a reticle instead of a blob, and the gap keeps it from
	// obscuring exactly the point it exists to mark.
	DrawLine(CenterX - GapRadius - ArmLength, CenterY, CenterX - GapRadius, CenterY, Colour, Thickness);
	DrawLine(CenterX + GapRadius, CenterY, CenterX + GapRadius + ArmLength, CenterY, Colour, Thickness);
	DrawLine(CenterX, CenterY - GapRadius - ArmLength, CenterX, CenterY - GapRadius, Colour, Thickness);
	DrawLine(CenterX, CenterY + GapRadius, CenterX, CenterY + GapRadius + ArmLength, Colour, Thickness);
}

void ADCCHUD::DrawSelfieViewfinder(ADCCCharacterBase* Crawler)
{
	if (!Canvas || !Crawler || !Crawler->IsInSelfieMode())
	{
		return;
	}

	const float Margin = 48.0f;
	const float BracketLength = 36.0f;
	const float Thickness = 3.0f;
	const FLinearColor FrameColour(1.0f, 1.0f, 1.0f, 0.85f);

	const float Left = Margin;
	const float Right = Canvas->SizeX - Margin;
	const float Top = Margin;
	const float Bottom = Canvas->SizeY - Margin;

	// Four L-shaped corner brackets - the standard camera-viewfinder frame.
	DrawLine(Left, Top, Left + BracketLength, Top, FrameColour, Thickness);
	DrawLine(Left, Top, Left, Top + BracketLength, FrameColour, Thickness);

	DrawLine(Right, Top, Right - BracketLength, Top, FrameColour, Thickness);
	DrawLine(Right, Top, Right, Top + BracketLength, FrameColour, Thickness);

	DrawLine(Left, Bottom, Left + BracketLength, Bottom, FrameColour, Thickness);
	DrawLine(Left, Bottom, Left, Bottom - BracketLength, FrameColour, Thickness);

	DrawLine(Right, Bottom, Right - BracketLength, Bottom, FrameColour, Thickness);
	DrawLine(Right, Bottom, Right, Bottom - BracketLength, FrameColour, Thickness);

	// Small centre reticle - purely decorative, sells the "framing a shot" read.
	const float CentreX = Canvas->SizeX * 0.5f;
	const float CentreY = Canvas->SizeY * 0.5f;
	DrawLine(CentreX - 10.0f, CentreY, CentreX + 10.0f, CentreY, FrameColour, 1.5f);
	DrawLine(CentreX, CentreY - 10.0f, CentreX, CentreY + 10.0f, FrameColour, 1.5f);

	DrawText(TEXT("SELFIE MODE"), FLinearColor(1.0f, 0.35f, 0.35f), Left, Top - 26.0f, GEngine->GetSmallFont(), 1.0f);

	const FString Hint = Crawler->SelfiePoseAnimations.Num() > 0
		? TEXT("[Scroll] Change pose    [Esc] Cancel")
		: TEXT("[Esc] Cancel");

	float HintW = 0.0f, HintH = 0.0f;
	GetTextSize(Hint, HintW, HintH, GEngine->GetSmallFont(), 1.0f);
	DrawText(Hint, DimColour, CentreX - HintW * 0.5f, Bottom + 12.0f, GEngine->GetSmallFont(), 1.0f);

	// Progress toward the shutter actually firing.
	const float BarW = 200.0f;
	const float BarX = CentreX - BarW * 0.5f;
	const float BarY = Bottom - 20.0f;
	DrawRect(FLinearColor(0.0f, 0.0f, 0.0f, 0.5f), BarX, BarY, BarW, 4.0f);
	DrawRect(FLinearColor(1.0f, 1.0f, 1.0f, 0.9f), BarX, BarY, BarW * Crawler->GetSelfiePoseProgress(), 4.0f);
}

ADCCIntroDirector* ADCCHUD::GetIntroDirector()
{
	if (CachedIntroDirector.IsValid())
	{
		return CachedIntroDirector.Get();
	}

	if (UWorld* World = GetWorld())
	{
		for (TActorIterator<ADCCIntroDirector> It(World); It; ++It)
		{
			CachedIntroDirector = *It;
			return *It;
		}
	}

	return nullptr;
}

void ADCCHUD::DrawWaitingForPartyBanner(ADCCCharacterBase* Crawler)
{
	ADCCIntroDirector* Director = GetIntroDirector();
	if (!Canvas || !Crawler || !Director || !Director->IsReadyForCollapse(Crawler))
	{
		return;
	}

	const FString Line = FString::Printf(TEXT("Waiting for other players... (%d/%d ready)"),
		Director->GetReadyCount(), Director->GetExpectedPartySize());

	float W = 0.0f, H = 0.0f;
	GetTextSize(Line, W, H, GEngine->GetMediumFont(), 1.0f);

	const float X = (Canvas->SizeX - W) * 0.5f;
	const float Y = Canvas->SizeY * 0.15f;

	DrawRect(FLinearColor(0.02f, 0.03f, 0.06f, 0.75f), X - 16.0f, Y - 8.0f, W + 32.0f, H + 16.0f);
	DrawText(Line, HeadingColour, X, Y, GEngine->GetMediumFont(), 1.0f);
}

void ADCCHUD::DrawNotice()
{
	if (NoticeText.IsEmpty() || !Canvas || !GetWorld()
		|| GetWorld()->GetTimeSeconds() > NoticeExpiryTime)
	{
		return;
	}

	const FString Line = NoticeText.ToString();

	// 2026-11-14 polish: a card with a shadow and an amber edge, in crisp text. In Repo Rally it sits under the
	// shift clock instead of over the road ahead.
	const float US = FMath::Clamp(Canvas->SizeY / 1080.0f, 0.6f, 2.0f);
	UFont* Font = GEngine->GetSmallFont();
	float W = 0.0f, H = 0.0f;
	RallyTextSize(Line, W, H, Font, 1.3f * US);

	const float X = (Canvas->SizeX - W) * 0.5f;
	const ADCCArcadeArena* NoticeArena = GetArcadeArena();
	const bool bRally = NoticeArena && NoticeArena->GetGameId() == TEXT("Kart");
	// Above centre, clear of the crosshair and the hotbar.
	const float Y = bRally ? 172.0f * US : Canvas->SizeY * 0.38f;
	const float PX = 18.0f * US, PY = 9.0f * US;
	const float Fade = FMath::Clamp(float(NoticeExpiryTime - GetWorld()->GetTimeSeconds()) / 0.4f, 0.0f, 1.0f);
	DrawRect(FLinearColor(0.0f, 0.0f, 0.0f, 0.3f * Fade), X - PX + 4.0f, Y - PY + 5.0f, W + PX * 2.0f, H + PY * 2.0f);
	DrawRect(FLinearColor(0.035f, 0.018f, 0.065f, 0.9f * Fade), X - PX, Y - PY, W + PX * 2.0f, H + PY * 2.0f);
	DrawRect(FLinearColor(1.0f, 0.76f, 0.32f, Fade), X - PX, Y - PY, 4.0f * US, H + PY * 2.0f);
	RallyText(Line, FLinearColor(1.0f, 0.9f, 0.7f, Fade), X, Y, Font, 1.3f * US);
}


void ADCCHUD::ToggleAdminPanel()
{
	bShowAdminPanel = !bShowAdminPanel;

	if (!PlayerOwner)
	{
		return;
	}

	// Same cursor handling as the character menu. Both can be open at once, so
	// the condition is "is anything that needs clicking open" rather than the
	// state of this one panel.
	ApplyCursorForOpenPanels();
}

float ADCCHUD::DrawAdminButton(float X, float Y, float Width, const FString& Label,
	const FString& Action, bool bHighlighted)
{
	constexpr float ButtonHeight = 26.0f;

	AddHitBox(FVector2D(X, Y), FVector2D(Width, ButtonHeight),
		FName(*(FString(TEXT("Adm|")) + Action)), /*bConsumesInput=*/true, /*Priority=*/3);

	// Border then fill, so a button reads as raised rather than as coloured text.
	DrawRect(bHighlighted ? FLinearColor(0.85f, 0.70f, 0.20f, 0.95f)
						  : FLinearColor(0.42f, 0.44f, 0.52f, 0.6f),
		X - 1.0f, Y - 1.0f, Width + 2.0f, ButtonHeight + 2.0f);

	DrawRect(bHighlighted ? FLinearColor(0.22f, 0.18f, 0.06f, 0.95f)
						  : FLinearColor(0.08f, 0.10f, 0.14f, 0.95f),
		X, Y, Width, ButtonHeight);

	DrawText(Label, bHighlighted ? HeadingColour : BodyColour, X + 10.0f, Y + 5.0f);

	return Y + ButtonHeight + 4.0f;
}

void ADCCHUD::DrawAdminPanel(ADCCCharacterBase* Crawler)
{
	if (!bShowAdminPanel || !Crawler || !Canvas)
	{
		return;
	}

	constexpr float PanelW = 300.0f;
	constexpr float ButtonW = PanelW - 32.0f;

	const float X = Canvas->SizeX - PanelW - 24.0f;
	float Y = 90.0f;

	DrawRect(FLinearColor(0.42f, 0.44f, 0.52f, 0.5f),
		X - 14.0f, Y - 14.0f, PanelW + 28.0f, Canvas->SizeY - 200.0f);
	DrawRect(FLinearColor(0.02f, 0.03f, 0.06f, 0.92f),
		X - 12.0f, Y - 12.0f, PanelW + 24.0f, Canvas->SizeY - 204.0f);

	DrawText(TEXT("DM PANEL"), HeadingColour, X, Y);
	Y += LineHeight + 6.0f;

	{
		static const TCHAR* TabNames[] = { TEXT("Self"), TEXT("Skills"), TEXT("Spawn") };
		float TabX = X;

		for (int32 Index = 0; Index < static_cast<int32>(EDCCAdminTab::MAX); ++Index)
		{
			const bool bActive = static_cast<int32>(AdminTab) == Index;

			AddHitBox(FVector2D(TabX, Y - 2.0f), FVector2D(86.0f, LineHeight + 4.0f),
				FName(*FString::Printf(TEXT("Adm|tab%d"), Index)), true, 3);

			if (bActive)
			{
				DrawRect(FLinearColor(0.16f, 0.18f, 0.26f, 0.9f),
					TabX, Y - 2.0f, 86.0f, LineHeight + 4.0f);
			}

			DrawText(TabNames[Index], bActive ? HeadingColour : DimColour, TabX + 8.0f, Y);
			TabX += 90.0f;
		}

		Y += LineHeight + 10.0f;
	}

	switch (AdminTab)
	{
	case EDCCAdminTab::Self:
	{
		const bool bInvuln = Crawler->Stats && Crawler->Stats->bInvulnerable;

		Y = DrawAdminButton(X, Y, ButtonW, TEXT("Refill health / mana / stamina"), TEXT("refill"));
		Y = DrawAdminButton(X, Y, ButtonW,
			bInvuln ? TEXT("Invulnerable: ON") : TEXT("Invulnerable: off"),
			TEXT("invuln"), bInvuln);
		Y = DrawAdminButton(X, Y, ButtonW, TEXT("Level up"), TEXT("levelup"));
		Y = DrawAdminButton(X, Y, ButtonW, TEXT("+100 gold"), TEXT("gold100"));
		Y = DrawAdminButton(X, Y, ButtonW, TEXT("Clear all hostiles"), TEXT("clear"));

		Y += 8.0f;
		DrawText(TEXT("Equip"), HeadingColour, X, Y);
		Y += LineHeight;

		// The handful worth reaching for mid-test, not the whole table.
		static const TCHAR* QuickItems[] = {
			TEXT("WallSword"), TEXT("FireAxe"), TEXT("SodaGun"),
			TEXT("ServiceKnifeSet"), TEXT("Torch")
		};

		for (const TCHAR* Item : QuickItems)
		{
			Y = DrawAdminButton(X, Y, ButtonW, Item, FString(TEXT("equip:")) + Item);
		}
		break;
	}

	case EDCCAdminTab::Skills:
	{
		UDCCSkillComponent* SkillComp = Crawler->Skills;
		if (!SkillComp || !SkillComp->SkillTable)
		{
			DrawText(TEXT("  (no skill table)"), DimColour, X, Y);
			break;
		}

		// Paged, because the table is forty rows and the panel is not.
		TArray<FName> Rows = SkillComp->SkillTable->GetRowNames();
		Rows.Sort([](const FName& A, const FName& B)
		{
			return A.ToString() < B.ToString();
		});

		constexpr int32 PerPage = 12;
		const int32 PageCount = FMath::Max(1, FMath::DivideAndRoundUp(Rows.Num(), PerPage));
		AdminSkillPage = FMath::Clamp(AdminSkillPage, 0, PageCount - 1);

		const int32 First = AdminSkillPage * PerPage;
		const int32 Last = FMath::Min(First + PerPage, Rows.Num());

		for (int32 Index = First; Index < Last; ++Index)
		{
			const FName RowName = Rows[Index];

			// Known skills highlight, so the panel doubles as a readout of what
			// this character already has.
			const bool bKnown = SkillComp->IsSkillKnown(RowName);

			Y = DrawAdminButton(X, Y, ButtonW, RowName.ToString(),
				FString(TEXT("skill:")) + RowName.ToString(), bKnown);
		}

		Y += 6.0f;
		DrawText(FString::Printf(TEXT("Page %d / %d"), AdminSkillPage + 1, PageCount),
			DimColour, X, Y);
		Y += LineHeight;

		DrawAdminButton(X, Y, 120.0f, TEXT("< Prev"), TEXT("skillprev"));
		DrawAdminButton(X + 128.0f, Y, 120.0f, TEXT("Next >"), TEXT("skillnext"));
		break;
	}

	case EDCCAdminTab::Spawn:
	{
		DrawText(TEXT("Spawns 3m ahead of you"), DimColour, X, Y);
		Y += LineHeight + 4.0f;

		Y = DrawAdminButton(X, Y, ButtonW, TEXT("Debt Eater"), TEXT("spawn:DebtEater"));
		Y = DrawAdminButton(X, Y, ButtonW, TEXT("Voldemortgage (boss)"), TEXT("spawn:Voldemortgage"));
		Y = DrawAdminButton(X, Y, ButtonW, TEXT("Test Boss"), TEXT("spawn:TestBoss"));

		Y += 8.0f;
		DrawText(TEXT("Wildlife"), HeadingColour, X, Y);
		Y += LineHeight;

		Y = DrawAdminButton(X, Y, ButtonW, TEXT("The Auditor"), TEXT("spawn:Beholder"));
		Y = DrawAdminButton(X, Y, ButtonW, TEXT("Thornback"), TEXT("spawn:Thornback"));
		Y = DrawAdminButton(X, Y, ButtonW, TEXT("Unclaimed Property"), TEXT("spawn:UnclaimedProperty"));
		Y = DrawAdminButton(X, Y, ButtonW, TEXT("Break Room Mold"), TEXT("spawn:BreakRoomMold"));
		Y = DrawAdminButton(X, Y, ButtonW, TEXT("Shell Company"), TEXT("spawn:ShellCompany"));

		Y += 8.0f;
		DrawText(TEXT("The Mine"), HeadingColour, X, Y);
		Y += LineHeight;

		Y = DrawAdminButton(X, Y, ButtonW, TEXT("Legacy System"), TEXT("spawn:LegacySystem"));
		Y = DrawAdminButton(X, Y, ButtonW, TEXT("Reconciliation (pair)"), TEXT("spawn:Reconciliation"));
		Y = DrawAdminButton(X, Y, ButtonW, TEXT("Survey Drone"), TEXT("spawn:SurveyDrone"));

		Y += 8.0f;
		DrawText(TEXT("Corporate"), HeadingColour, X, Y);
		Y += LineHeight;

		Y = DrawAdminButton(X, Y, ButtonW, TEXT("Intake Manager"), TEXT("spawn:InternManager"));
		Y = DrawAdminButton(X, Y, ButtonW, TEXT("The Notary"), TEXT("spawn:Notary"));
		Y = DrawAdminButton(X, Y, ButtonW, TEXT("Escalation Specialist"), TEXT("spawn:Escalation"));

		Y += 8.0f;
		DrawText(TEXT("The Nuclear Family"), HeadingColour, X, Y);
		Y += LineHeight;

		Y = DrawAdminButton(X, Y, ButtonW, TEXT("Dad"), TEXT("spawn:NuclearDad"));
		Y = DrawAdminButton(X, Y, ButtonW, TEXT("Mom"), TEXT("spawn:NuclearMom"));
		Y = DrawAdminButton(X, Y, ButtonW, TEXT("The Kid"), TEXT("spawn:NuclearKid"));

		Y += 8.0f;
		DrawText(TEXT("Friendly"), HeadingColour, X, Y);
		Y += LineHeight;

		Y = DrawAdminButton(X, Y, ButtonW, TEXT("Ally crawler"), TEXT("spawnally:HockeyWings"));
		Y = DrawAdminButton(X, Y, ButtonW, TEXT("Bystander (crowd)"), TEXT("spawn:Crowd"));
		break;
	}

	default:
		break;
	}
}

void ADCCHUD::HandleAdminAction(const FString& Action, ADCCCharacterBase* Crawler)
{
	if (!Crawler)
	{
		return;
	}

	if (Action.StartsWith(TEXT("tab")))
	{
		const int32 Index = FCString::Atoi(*Action.RightChop(3));
		if (Index >= 0 && Index < static_cast<int32>(EDCCAdminTab::MAX))
		{
			AdminTab = static_cast<EDCCAdminTab>(Index);
		}
		return;
	}

	if (Action == TEXT("skillprev"))
	{
		AdminSkillPage = FMath::Max(0, AdminSkillPage - 1);
		return;
	}

	if (Action == TEXT("skillnext"))
	{
		++AdminSkillPage;
		return;
	}

	if (Action == TEXT("refill"))
	{
		Crawler->DCCRefill();
		ShowNotice(NSLOCTEXT("DCC", "AdmRefill", "Topped up."));
		return;
	}

	if (Action == TEXT("levelup"))
	{
		Crawler->Server_AdminLevelUp();
		return;
	}

	if (Action == TEXT("gold100"))
	{
		Crawler->Server_AdminAddGold(100);
		ShowNotice(NSLOCTEXT("DCC", "AdmGold", "+100 gold."));
		return;
	}

	if (Action == TEXT("clear"))
	{
		Crawler->Server_AdminClearHostiles();
		return;
	}

	if (Action == TEXT("gauntletstart") || Action == TEXT("gauntletreset"))
	{
		// Nearest to the admin, not "the only one" - lets a second gauntlet
		// exist later (a harder one, say) without this action becoming
		// ambiguous. Runs through the same Server_ RPC pattern as every
		// other admin action here rather than calling StartGauntlet/
		// ResetGauntlet directly, since HandleAdminAction executes on
		// whichever client pressed the button and gauntlets are server-
		// authoritative like every other encounter.
		Crawler->Server_AdminGauntlet(Action == TEXT("gauntletreset"));
		return;
	}

	if (Action == TEXT("invuln"))
	{
		const bool bNow = Crawler->Stats && Crawler->Stats->bInvulnerable;
		Crawler->Server_AdminSetInvulnerable(!bNow);
		return;
	}

	if (Action.StartsWith(TEXT("skill:")))
	{
		// Rank 3 rather than 1, so a granted skill is immediately usable instead
		// of failing its checks and looking broken.
		Crawler->DCCGrantSkill(Action.RightChop(6), 3);
		return;
	}

	if (Action.StartsWith(TEXT("equip:")))
	{
		Crawler->DCCEquip(Action.RightChop(6));
		return;
	}

	if (Action.StartsWith(TEXT("spawn:")) || Action.StartsWith(TEXT("spawnally:")))
	{
		const bool bAsAlly = Action.StartsWith(TEXT("spawnally:"));
		const FString Which = bAsAlly ? Action.RightChop(10) : Action.RightChop(6);

		UClass* Resolved = nullptr;

		if (Which == TEXT("DebtEater"))          { Resolved = ADCCEnemyCharacter_DebtEater::StaticClass(); }
		else if (Which == TEXT("Voldemortgage")) { Resolved = ADCCEnemyCharacter_Voldemortgage::StaticClass(); }
		else if (Which == TEXT("TestBoss"))      { Resolved = ADCCEnemyCharacter_TestBoss::StaticClass(); }
		else if (Which == TEXT("NuclearDad"))    { Resolved = ADCCEnemy_NuclearDad::StaticClass(); }
		else if (Which == TEXT("NuclearMom"))    { Resolved = ADCCEnemy_NuclearMom::StaticClass(); }
		else if (Which == TEXT("NuclearKid"))    { Resolved = ADCCEnemy_NuclearKid::StaticClass(); }
		else if (Which == TEXT("Crowd"))         { Resolved = ADCCCrowdCrawler::StaticClass(); }
		else if (Which == TEXT("HockeyWings"))   { Resolved = ADCCAlly_HockeyWings::StaticClass(); }
		else if (Which == TEXT("Vaughn"))        { Resolved = ADCCAlly_Vaughn::StaticClass(); }
		else if (Which == TEXT("Beholder"))      { Resolved = ADCCEnemyCharacter_Beholder::StaticClass(); }
		else if (Which == TEXT("Thornback"))     { Resolved = ADCCEnemyCharacter_Thornback::StaticClass(); }
		else if (Which == TEXT("UnclaimedProperty")) { Resolved = ADCCEnemyCharacter_UnclaimedProperty::StaticClass(); }
		else if (Which == TEXT("BreakRoomMold")) { Resolved = ADCCEnemyCharacter_BreakRoomMold::StaticClass(); }
		else if (Which == TEXT("ShellCompany"))  { Resolved = ADCCEnemyCharacter_ShellCompany::StaticClass(); }
		else if (Which == TEXT("LegacySystem"))  { Resolved = ADCCEnemyCharacter_LegacySystem::StaticClass(); }
		else if (Which == TEXT("Reconciliation")) { Resolved = ADCCEnemyCharacter_Reconciliation::StaticClass(); }
		else if (Which == TEXT("SurveyDrone"))   { Resolved = ADCCEnemyCharacter_SurveyDrone::StaticClass(); }
		else if (Which == TEXT("InternManager")) { Resolved = ADCCEnemyCharacter_InternManager::StaticClass(); }
		else if (Which == TEXT("Notary"))        { Resolved = ADCCEnemyCharacter_Notary::StaticClass(); }
		else if (Which == TEXT("Escalation"))    { Resolved = ADCCEnemyCharacter_EscalationSpecialist::StaticClass(); }

		// Says so rather than silently doing nothing, which is the failure mode
		// this whole panel exists to get away from.
		if (!Resolved)
		{
			ShowNotice(FText::Format(
				NSLOCTEXT("DCC", "AdmNoClass", "No spawnable class called '{0}'."),
				FText::FromString(Which)));
			return;
		}

		Crawler->Server_AdminSpawn(Resolved, bAsAlly);
		return;
	}
}

void ADCCHUD::CycleMenuTab(int32 Delta)
{
	const int32 Count = static_cast<int32>(EDCCMenuTab::MAX);
	const int32 Step = Delta >= 0 ? 1 : -1;
	int32 Next = static_cast<int32>(MenuTab);

	// Walk past pages that have not been issued yet. Bounded by Count so a
	// crawler with nothing unlocked cannot spin here forever.
	for (int32 Tries = 0; Tries < Count; ++Tries)
	{
		Next = ((Next + (Tries == 0 ? Delta : Step)) % Count + Count) % Count;
		if (IsMenuTabUnlocked(static_cast<EDCCMenuTab>(Next)))
		{
			break;
		}
	}
	MenuTab = static_cast<EDCCMenuTab>(Next);

	// Selection belongs to the Achievements page; carrying it to another tab
	// would leave a card open over content it does not describe.
	SelectedAchievementId = NAME_None;
}

namespace
{
	const TCHAR* MenuTabName(EDCCMenuTab Tab)
	{
		switch (Tab)
		{
		case EDCCMenuTab::Character:    return TEXT("CHARACTER");
		case EDCCMenuTab::Skills:       return TEXT("SKILLS");
		case EDCCMenuTab::Inventory:    return TEXT("INVENTORY");
		case EDCCMenuTab::Party:        return TEXT("PARTY");
		case EDCCMenuTab::Talk:         return TEXT("TALK");
		case EDCCMenuTab::Achievements: return TEXT("ACHIEVEMENTS");
		case EDCCMenuTab::Feed:         return TEXT("FEED");
		case EDCCMenuTab::Chatter:      return TEXT("CHATTER");
		case EDCCMenuTab::Allies:       return TEXT("ALLIES");
		case EDCCMenuTab::Whispers:     return TEXT("WHISPERS");
		case EDCCMenuTab::Settings:     return TEXT("SETTINGS");
		default:                        return TEXT("");
		}
	}
}

float ADCCHUD::DrawMenuTabs(float X, float Y)
{
	float TabX = X;

	// The panel itself is a fixed 460px wide (see DrawHUD's own backing
	// DrawRect) - this project grew from four tabs to ten without anyone
	// re-checking whether a single row still fit, and for a while now it
	// has not: the row ran well past the panel's own right edge with
	// nothing to stop it. 420 matches the RowWidth constant used everywhere
	// else in this panel (DrawAchievements, DrawParty, etc.), so tabs wrap
	// at the same boundary every other row in this menu already respects.
	constexpr float MaxRowWidth = 420.0f;
	const float RowStartX = X;

	for (int32 Index = 0; Index < static_cast<int32>(EDCCMenuTab::MAX); ++Index)
	{
		const EDCCMenuTab Tab = static_cast<EDCCMenuTab>(Index);
		const FString Label = MenuTabName(Tab);
		const bool bActive = Tab == MenuTab;

		// Width from the label rather than a fixed step, so renaming a tab cannot
		// silently overlap its neighbour.
		float TextW = 0.0f;
		float TextH = 0.0f;
		GetTextSize(Label, TextW, TextH);

		const float BoxW = TextW + 18.0f;

		// Wrap to a new row rather than letting the tab bar run off the
		// panel - the whole point of a tab bar is being visible at a glance.
		if (TabX != RowStartX && (TabX - RowStartX) + BoxW > MaxRowWidth)
		{
			TabX = RowStartX;
			Y += LineHeight + 4.0f;
		}

		// Registered every frame, like the achievement rows. The name is the tab
		// index so NotifyHitBoxClick can route it without a second lookup table.
		AddHitBox(FVector2D(TabX, Y - 2.0f), FVector2D(BoxW, LineHeight + 4.0f),
			FName(*FString::Printf(TEXT("Tab_%d"), Index)), /*bConsumesInput=*/true, /*Priority=*/2);

		if (bActive)
		{
			DrawRect(FLinearColor(0.16f, 0.18f, 0.26f, 0.9f), TabX, Y - 2.0f, BoxW, LineHeight + 4.0f);
		}

		DrawText(Label, bActive ? HeadingColour : DimColour, TabX + 9.0f, Y);
		if (GuideStep == 2 && Tab == EDCCMenuTab::Inventory && GetWorld())
		{
			const float Pulse = 0.5f + 0.5f * FMath::Sin(GetWorld()->GetRealTimeSeconds() * 6.0f);
			DrawRect(FLinearColor(1.0f, 0.15f, 0.1f, 0.6f + 0.4f * Pulse), TabX + BoxW - 7.0f, Y - 6.0f, 10.0f, 10.0f);
		}
		TabX += BoxW + 4.0f;
	}

	Y += LineHeight + 6.0f;

	// A rule under the tabs, so the page below reads as belonging to one of them.
	DrawRect(FLinearColor(0.42f, 0.44f, 0.52f, 0.55f), X, Y, 420.0f, 1.0f);
	Y += 6.0f;

	// The rare things that used to have keys of their own (2026-11-16: O, F2, F3, K, L), as buttons.
	{
		const FString FeedLabel = FString::Printf(TEXT("FEED: %s"), FeedVisibility == EDCCFeedVisibility::Pinned ? TEXT("SHOWN")
			: FeedVisibility == EDCCFeedVisibility::Minimized ? TEXT("SMALL") : TEXT("HIDDEN"));
		const FString LogLabel = FString::Printf(TEXT("LOG: %s"), *GetFeedFilterLabel().ToUpper());
		const TPair<FString, FString> Buttons[] = {
			{ TEXT("LOOT BOXES"), TEXT("Loot") }, { TEXT("PARTY"), TEXT("Party") }, { TEXT("POSE BENCH"), TEXT("Bench") },
			{ FeedLabel, TEXT("Feed") }, { LogLabel, TEXT("Log") } };
		float BX = X;
		for (const TPair<FString, FString>& B : Buttons)
		{
			float TW = 0.0f, TH = 0.0f;
			GetTextSize(B.Key, TW, TH);
			const float BW = TW + 14.0f;
			if (BX != X && (BX - X) + BW > MaxRowWidth)
			{
				BX = X;
				Y += LineHeight + 6.0f;
			}
			DrawRect(FLinearColor(0.30f, 0.32f, 0.42f, 0.7f), BX, Y - 1.0f, BW, LineHeight + 2.0f);
			DrawRect(FLinearColor(0.05f, 0.06f, 0.10f, 0.95f), BX + 1.0f, Y, BW - 2.0f, LineHeight);
			DrawText(B.Key, FLinearColor(0.85f, 0.88f, 1.0f), BX + 7.0f, Y);
			AddHitBox(FVector2D(BX, Y - 1.0f), FVector2D(BW, LineHeight + 2.0f), FName(*(FString(TEXT("Quick|")) + B.Value)), true, 2);
			BX += BW + 5.0f;
		}
		Y += LineHeight + 8.0f;
	}

	return Y + 6.0f;
}

float ADCCHUD::DrawEquipment(float X, float Y, ADCCCharacterBase* Crawler)
{
	UDCCEquipmentComponent* Equipment = Crawler->Equipment;

	DrawText(TEXT("Equipment"), HeadingColour, X, Y);
	Y += LineHeight;

	if (!Equipment)
	{
		DrawText(TEXT("  (none)"), DimColour, X, Y);
		return Y + LineHeight;
	}

	static const TCHAR* SlotNames[] = { TEXT("Main Hand"), TEXT("Off Hand"), TEXT("Head"), TEXT("Body"), TEXT("Trinket") };

	for (int32 Index = 0; Index < static_cast<int32>(EDCCEquipmentSlot::MAX); ++Index)
	{
		const EDCCEquipmentSlot Slot = static_cast<EDCCEquipmentSlot>(Index);

		const FName ItemId = Equipment->GetEquippedId(Slot);

		FDCCEquipmentDef Def;
		const bool bHas = !ItemId.IsNone() && Equipment->FindItem(ItemId, Def);

		DrawText(FString::Printf(TEXT("  %-11s %s"),
			SlotNames[Index],
			bHas ? *Def.DisplayName.ToString() : TEXT("-")),
			bHas ? BodyColour : DimColour, X, Y);
		Y += LineHeight;
	}

	return Y;
}

float ADCCHUD::DrawParty(float X, float Y, ADCCCharacterBase* Crawler)
{
	DrawText(TEXT("Party"), HeadingColour, X, Y);
	Y += LineHeight;

	UWorld* World = GetWorld();
	if (!World)
	{
		return Y;
	}

	int32 Count = 0;

	for (TActorIterator<ADCCCharacterBase> It(World); It; ++It)
	{
		ADCCCharacterBase* Other = *It;

		// Everyone on your side except you. Befriended NPCs and other players
		// both qualify - from the party panel's point of view there is no
		// difference between a recruited ally and a second crawler.
		if (!Other || Other == Crawler || !Other->IsCrawler())
		{
			continue;
		}

		const FString Name = Other->DisplayName.IsEmpty()
			? Other->GetActorNameOrLabel()
			: Other->DisplayName.ToString();

		FString State;
		if (!Other->IsAlive())
		{
			State = TEXT("DOWN");
		}
		else if (Other->IsBefriended())
		{
			State = TEXT("with you");
		}
		else
		{
			State = TEXT("nearby");
		}

		const UDCCStatsComponent* OtherStats = Other->Stats;
		const FString Health = OtherStats
			? FString::Printf(TEXT("%d%%"), FMath::RoundToInt(OtherStats->GetHealthFraction() * 100.0f))
			: FString(TEXT("?"));

		const float Distance = FVector::Dist(Crawler->GetActorLocation(), Other->GetActorLocation());

		DrawText(FString::Printf(TEXT("  %-12s %-9s %-8s %.0fm"),
			*Name, *State, *Health, Distance / 100.0f),
			Other->IsAlive() ? BodyColour : DimColour, X, Y);
		Y += LineHeight;
		++Count;
	}

	if (Count == 0)
	{
		DrawText(TEXT("  Nobody. The audience finds this relatable."), DimColour, X, Y);
		Y += LineHeight;
	}

	Y += SectionGap;
	Y = DrawChannelMessages(X, Y, EDCCChatChannel::Party, Crawler, 6);
	Y = DrawChatSendRow(X, Y, EDCCChatChannel::Party, Crawler, TEXT("Party"));

	return Y;
}

float ADCCHUD::DrawTalk(float X, float Y, ADCCCharacterBase* Crawler)
{
	DrawText(TEXT("Talk  -  pick a name to talk"), HeadingColour, X, Y);
	Y += LineHeight;

	ChatCandidates.Reset();

	UWorld* World = GetWorld();
	if (!Crawler || !World)
	{
		return Y;
	}

	TArray<ADCCCharacterBase*> Nearby;

	for (TActorIterator<ADCCCharacterBase> It(World); It; ++It)
	{
		ADCCCharacterBase* Candidate = *It;

		// Same gate Server_PerformInteraction itself enforces - not hostile,
		// alive, in range - so nothing listed here is a guaranteed no-op once
		// clicked. Corpses and downed allies belong to the Party/Loot flows,
		// not a conversation, so IsAlive() is checked here rather than left to
		// the server.
		if (!Candidate || Candidate == Crawler || !Candidate->IsAlive() || Candidate->IsHostileTo(Crawler))
		{
			continue;
		}

		if (FVector::Dist(Crawler->GetActorLocation(), Candidate->GetActorLocation()) > Crawler->InteractionRange)
		{
			continue;
		}

		Nearby.Add(Candidate);
	}

	if (Nearby.IsEmpty())
	{
		DrawText(TEXT("  (nobody friendly close enough to talk to)"), DimColour, X, Y);
		return Y + LineHeight;
	}

	Nearby.Sort([Crawler](const ADCCCharacterBase& A, const ADCCCharacterBase& B)
	{
		return FVector::DistSquared(Crawler->GetActorLocation(), A.GetActorLocation())
			< FVector::DistSquared(Crawler->GetActorLocation(), B.GetActorLocation());
	});

	constexpr float RowWidth = 420.0f;

	for (int32 Index = 0; Index < Nearby.Num(); ++Index)
	{
		ADCCCharacterBase* Candidate = Nearby[Index];
		ChatCandidates.Add(Candidate);

		const bool bTalkingToThem = Crawler->GetInteractionTarget() == Candidate;

		AddHitBox(FVector2D(X, Y), FVector2D(RowWidth, LineHeight),
			FName(*FString::Printf(TEXT("Talk_%d"), Index)), /*bConsumesInput=*/true, /*Priority=*/1);

		if (bTalkingToThem)
		{
			DrawRect(FLinearColor(0.16f, 0.18f, 0.26f, 0.85f), X - 4.0f, Y - 1.0f, RowWidth, LineHeight);
		}

		const FString Name = Candidate->DisplayName.IsEmpty()
			? Candidate->GetActorNameOrLabel()
			: Candidate->DisplayName.ToString();

		DrawText(FString::Printf(TEXT("  %s"), *Name),
			bTalkingToThem ? HeadingColour : BodyColour, X, Y);
		Y += LineHeight;
	}

	// The conversation itself is DrawInteractionMenu, already drawn every frame
	// regardless of which Tab page is open - clicking a row above just sets
	// InteractionTarget, the same thing aiming at someone and pressing E does.
	return Y;
}

void ADCCHUD::DrawQuestTracker()
{
	const UDCCQuestComponent* QuestLog = UDCCQuestComponent::Get(this);
	if (!QuestLog)
	{
		return;
	}

	TArray<FDCCQuestDef> Active;
	QuestLog->GetActiveQuests(Active);

	if (Active.IsEmpty())
	{
		return;
	}

	constexpr float PanelW = 340.0f;

	const float X = Canvas->SizeX - PanelW - 24.0f;
	float Y = 28.0f;

	// Measured before drawing so the backing panel is exactly as tall as the
	// content. A fixed-height panel either clips objectives or leaves a slab of
	// dead black over the play area.
	int32 Lines = 0;
	for (const FDCCQuestDef& Quest : Active)
	{
		++Lines;
		for (const FDCCQuestObjective& Objective : Quest.Objectives)
		{
			Lines += Objective.bHidden ? 0 : 1;
		}
	}

	DrawRect(FLinearColor(0.02f, 0.03f, 0.06f, 0.72f),
		X - 12.0f, Y - 10.0f, PanelW + 24.0f, (Lines + 1) * LineHeight + 24.0f);

	DrawText(TEXT("OBJECTIVES"), HeadingColour, X, Y);
	Y += LineHeight + 4.0f;

	for (const FDCCQuestDef& Quest : Active)
	{
		DrawText(Quest.Title.ToString(), HeadingColour, X, Y);
		Y += LineHeight;

		const FDCCQuestState* State = QuestLog->FindState(Quest.QuestId);

		for (int32 Index = 0; Index < Quest.Objectives.Num(); ++Index)
		{
			const FDCCQuestObjective& Objective = Quest.Objectives[Index];
			if (Objective.bHidden)
			{
				continue;
			}

			const bool bComplete = State && State->Objectives.IsValidIndex(Index)
				&& State->Objectives[Index].bComplete;

			const int32 Count = State && State->Objectives.IsValidIndex(Index)
				? State->Objectives[Index].Count : 0;

			// The count is only shown when there is more than one to do. "Reach
			// the ledge  0/1" is noise; "Kill interns  3/5" is the whole point.
			const FString Line = Objective.RequiredCount > 1
				? FString::Printf(TEXT("  %s %s  %d/%d"),
					bComplete ? TEXT("[X]") : TEXT("[ ]"),
					*Objective.Description.ToString(),
					FMath::Min(Count, Objective.RequiredCount), Objective.RequiredCount)
				: FString::Printf(TEXT("  %s %s"),
					bComplete ? TEXT("[X]") : TEXT("[ ]"),
					*Objective.Description.ToString());

			DrawText(Line, bComplete ? DimColour : BodyColour, X, Y);
			Y += LineHeight;
		}
	}

	// Distance to the nearest live marker, so "go there" has a direction even
	// before the beacon is on screen.
	const ADCCCharacterBase* Crawler = GetOwningCrawler();
	if (!Crawler || !GetWorld())
	{
		return;
	}

	float NearestDistance = TNumericLimits<float>::Max();
	FText NearestLabel;

	for (TActorIterator<ADCCQuestMarker> It(GetWorld()); It; ++It)
	{
		const ADCCQuestMarker* Marker = *It;
		if (!Marker || !Marker->IsMarkerActive())
		{
			continue;
		}

		const float Distance = FVector::Dist(Crawler->GetActorLocation(), Marker->GetGoalLocation());
		if (Distance < NearestDistance)
		{
			NearestDistance = Distance;
			NearestLabel = Marker->MarkerLabel;
		}
	}

	if (NearestDistance < TNumericLimits<float>::Max())
	{
		Y += 4.0f;

		// Metres, not centimetres. Nobody has ever wanted to read "4820 uu".
		DrawText(FString::Printf(TEXT("  -> %s  (%.0f m)"),
			*NearestLabel.ToString(), NearestDistance / 100.0f), HeadingColour, X, Y);
	}
}

void ADCCHUD::DrawMeter(float X, float Y, float Width, float Height, float Fraction,
	const FLinearColor& Fill, const FString& Label)
{
	const float Clamped = FMath::Clamp(Fraction, 0.0f, 1.0f);

	// The empty track is always drawn at full width, so an empty bar still reads
	// as "this meter exists and it is empty" rather than as nothing at all.
	DrawRect(FLinearColor(0.03f, 0.04f, 0.07f, 0.80f), X - 2.0f, Y - 2.0f, Width + 4.0f, Height + 4.0f);
	DrawRect(FLinearColor(0.12f, 0.13f, 0.18f, 0.90f), X, Y, Width, Height);

	if (Clamped > 0.0f)
	{
		DrawRect(Fill, X, Y, Width * Clamped, Height);
	}

	DrawText(Label, BodyColour, X + Width + 10.0f, Y - 2.0f);
}

void ADCCHUD::DrawVitals(ADCCCharacterBase* Crawler)
{
	UDCCStatsComponent* Stats = Crawler ? Crawler->Stats : nullptr;
	if (!Stats)
	{
		return;
	}

	// ---------------------------------------------------------------------
	// The segmented health bar, across the top.
	//
	// Ten blocks, whatever the hit point total is. Given the top of the screen
	// and real width because it is the single most important thing on screen -
	// and because blocks are countable at a glance from across a room, which a
	// percentage bar never is. That is the entire point of the mechanic.
	// ---------------------------------------------------------------------
	{
		const float TotalW = FMath::Min(Canvas->SizeX * 0.62f, 760.0f);
		constexpr float SegH = 26.0f;
		constexpr float TopY = 22.0f;

		// Reddens as the row empties, so "hurt" and "nearly dead" differ without
		// anyone reading a number. This is the player's own bar specifically -
		// world-space bars over other characters pass a fixed allegiance colour
		// instead, since a colour that shifts with health would be impossible to
		// tell apart from "this is hostile" at a glance mid-fight.
		const int32 SegmentCountForLerp = Stats->GetRuleset() ? Stats->GetRuleset()->GetHealthSegmentCount() : 10;
		const int32 FullSegments = Stats->GetFullHealthSegments();
		const float HealthFraction = SegmentCountForLerp > 0
			? static_cast<float>(FullSegments) / SegmentCountForLerp : 0.0f;
		const FLinearColor FillColour = FMath::Lerp(
			FLinearColor(0.88f, 0.16f, 0.12f), FLinearColor(0.28f, 0.86f, 0.36f), HealthFraction);

		DrawSegmentedBar(Stats, Canvas->SizeX * 0.5f, TopY, TotalW, SegH, FillColour, /*bShowRegenGhost=*/true);

		const FString HealthLabel = FString::Printf(TEXT("%d / %d"),
			FMath::Max(0, Stats->CurrentHitPoints), Stats->MaxHitPoints);

		float LabelW = 0.0f, LabelH = 0.0f;
		GetTextSize(HealthLabel, LabelW, LabelH, GEngine->GetMediumFont(), 1.0f);

		DrawText(HealthLabel, BodyColour,
			Canvas->SizeX * 0.5f - LabelW * 0.5f, TopY + SegH + 6.0f, GEngine->GetMediumFont(), 1.0f);
	}

	constexpr float BarW = 240.0f;
	constexpr float BarH = 16.0f;

	const float X = 24.0f;
	if (GetArcadeArena())
	{
		return; // the minigames show health only: no mana, no level
	}
	float Y = Canvas->SizeY - 132.0f;

	if (Stats->MaxManaPoints > 0)
	{
		DrawMeter(X, Y, BarW, BarH, Stats->GetManaFraction(), FLinearColor(0.30f, 0.55f, 0.95f),
			FString::Printf(TEXT("MP  %d / %d"), Stats->CurrentManaPoints, Stats->MaxManaPoints));
		Y += BarH + 8.0f;
	}

	// At the tutorial cap the XP numbers stop meaning anything, so say so
	// rather than showing a full bar that never moves and letting the player
	// wonder whether progression is broken.
	const FString ExperienceLabel = Stats->IsAtMaxLevel()
		? FString::Printf(TEXT("LVL %d   MAX (tutorial cap)"), Stats->Level)
		: FString::Printf(TEXT("LVL %d   %d / %d XP"),
			Stats->Level, Stats->ExperiencePoints, Stats->ExperienceToNextLevel);

	DrawMeter(X, Y, BarW, BarH * 0.6f, Stats->GetExperienceFraction(),
		Stats->IsAtMaxLevel() ? FLinearColor(0.55f, 0.55f, 0.55f) : FLinearColor(0.85f, 0.70f, 0.25f),
		ExperienceLabel);
}

void ADCCHUD::DrawSegmentedBar(UDCCStatsComponent* Stats, float CenterX, float TopY,
	float TotalWidth, float SegmentHeight, const FLinearColor& FillColour, bool bShowRegenGhost)
{
	if (!Stats)
	{
		return;
	}

	UDCCRuleset* Ruleset = Stats->GetRuleset();
	const int32 SegmentCount = Ruleset ? Ruleset->GetHealthSegmentCount() : 10;
	if (SegmentCount <= 0)
	{
		return;
	}

	constexpr float Gap = 5.0f;
	const float SegW = (TotalWidth - (Gap * (SegmentCount - 1))) / SegmentCount;
	const float StartX = CenterX - TotalWidth * 0.5f;

	const int32 FullSegments = Stats->GetFullHealthSegments();
	const float PartialFraction = Stats->GetPartialHealthSegmentFraction();

	// How far the partial block will refill on its own. Drawn as a dim ghost so
	// the player can see what is coming back versus what is genuinely gone -
	// which is the difference the mechanic is built on. Skipped for the mini
	// world-space bars: the detail is real but illegible at that size, and
	// costs a DrawRect per nearby character for something nobody could read.
	const float RegenFraction = Stats->IsRegenSuppressed() ? PartialFraction : 1.0f;

	DrawRect(FLinearColor(0.02f, 0.03f, 0.06f, 0.70f),
		StartX - 8.0f, TopY - 8.0f, TotalWidth + 16.0f, SegmentHeight + 16.0f);

	for (int32 Index = 0; Index < SegmentCount; ++Index)
	{
		const float SegX = StartX + Index * (SegW + Gap);

		// Empty socket first, so a lost block still reads as a block rather
		// than as absence.
		DrawRect(FLinearColor(0.10f, 0.11f, 0.15f, 0.95f), SegX, TopY, SegW, SegmentHeight);

		if (Index < FullSegments)
		{
			DrawRect(FillColour, SegX, TopY, SegW, SegmentHeight);
		}
		else if (Index == FullSegments && PartialFraction > 0.0f)
		{
			if (bShowRegenGhost && RegenFraction > PartialFraction)
			{
				DrawRect(FLinearColor(0.30f, 0.45f, 0.35f, 0.55f),
					SegX, TopY, SegW * RegenFraction, SegmentHeight);
			}

			DrawRect(FLinearColor(0.85f, 0.70f, 0.20f), SegX, TopY, SegW * PartialFraction, SegmentHeight);
		}
	}
}

void ADCCHUD::DrawNearbyHealthBars(ADCCCharacterBase* Crawler)
{
	UWorld* World = GetWorld();
	if (!Crawler || !World || !PlayerOwner)
	{
		return;
	}

	// Bounded to what could plausibly be on screen and worth showing. This is
	// also what makes "only bars near me" true by construction, rather than a
	// widget that has to be told to stop rendering something far away - there is
	// nothing to cull because nothing outside this radius is ever considered.
	constexpr float MaxShowDistanceSquared = 2500.0f * 2500.0f;
	constexpr float MiniBarWidth = 70.0f;
	constexpr float MiniBarHeight = 6.0f;
	constexpr float HeadClearance = 30.0f;

	for (TActorIterator<ADCCCharacterBase> It(World); It; ++It)
	{
		ADCCCharacterBase* Candidate = *It;
		if (!Candidate || Candidate == Crawler || !Candidate->Stats || !Candidate->IsAlive())
		{
			continue;
		}

		if (FVector::DistSquared(Crawler->GetActorLocation(), Candidate->GetActorLocation())
			> MaxShowDistanceSquared)
		{
			continue;
		}

		// Same range gate as the health bar itself: close enough to see a
		// mini bar over someone's head is close enough to count as "the fight
		// has started" for the purposes of the LEVEL BOSS card.
		CheckBossAnnouncement(Candidate);

		const float HalfHeight = Candidate->GetCapsuleComponent()
			? Candidate->GetCapsuleComponent()->GetScaledCapsuleHalfHeight() : 90.0f;
		const FVector HeadLocation = Candidate->GetActorLocation()
			+ FVector(0.0f, 0.0f, HalfHeight + HeadClearance);

		FVector2D ScreenPos;
		if (!PlayerOwner->ProjectWorldLocationToScreen(HeadLocation, ScreenPos))
		{
			// Behind the camera or otherwise unprojectable - not merely off to
			// the side, which ProjectWorldLocationToScreen still resolves.
			continue;
		}

		// A flat allegiance tint, not the player-bar's health-lerp: a bar that
		// reddens as it empties would be indistinguishable from "this is
		// hostile" at a glance, which is exactly the reading a health bar must
		// never give.
		const FLinearColor Tint = Candidate->GetAllegianceColour();

		DrawSegmentedBar(Candidate->Stats, ScreenPos.X, ScreenPos.Y,
			MiniBarWidth, MiniBarHeight, Tint, /*bShowRegenGhost=*/false);
	}
}

void ADCCHUD::CheckBossAnnouncement(ADCCCharacterBase* Candidate)
{
	if (!Candidate || !Candidate->IsAlive())
	{
		return;
	}

	UDCCBossComponent* Boss = Candidate->FindComponentByClass<UDCCBossComponent>();
	if (!Boss)
	{
		return;
	}

	if (AnnouncedBosses.ContainsByPredicate([Candidate](const TWeakObjectPtr<AActor>& Announced)
		{
			return Announced.Get() == Candidate;
		}))
	{
		return;
	}

	AnnouncedBosses.Add(Candidate);

	const UWorld* World = GetWorld();
	if (!World)
	{
		return;
	}

	// Falls back to the actor's own name rather than going blank - every boss
	// placed so far sets DisplayName, but nothing enforces that on whatever
	// gets built next.
	BossBannerName = Candidate->DisplayName.IsEmpty()
		? FText::FromString(Candidate->GetActorNameOrLabel())
		: Candidate->DisplayName;
	BossBannerExpiryTime = World->GetTimeSeconds() + BossBannerDurationSeconds;
}

void ADCCHUD::DrawBossBanner()
{
	const UWorld* World = GetWorld();
	if (!Canvas || !World || BossBannerName.IsEmpty()
		|| World->GetTimeSeconds() >= BossBannerExpiryTime)
	{
		return;
	}

	const float TimeRemaining = BossBannerExpiryTime - World->GetTimeSeconds();
	const float Elapsed = BossBannerDurationSeconds - TimeRemaining;

	// Fades at both ends of the window rather than cutting hard, so the card
	// never simply vanishes mid-read the way a hard timer cutoff would.
	constexpr float FadeSeconds = 0.6f;
	const float Alpha = FMath::Min(
		FMath::Clamp(Elapsed / FadeSeconds, 0.0f, 1.0f),
		FMath::Clamp(TimeRemaining / FadeSeconds, 0.0f, 1.0f));

	const FString TitleLine = TEXT("LEVEL BOSS");
	const FString NameLine = BossBannerName.ToString();

	float TitleW = 0.0f, TitleH = 0.0f;
	GetTextSize(TitleLine, TitleW, TitleH, GEngine->GetLargeFont(), 1.0f);
	float NameW = 0.0f, NameH = 0.0f;
	GetTextSize(NameLine, NameW, NameH, GEngine->GetMediumFont(), 1.0f);

	const float CardW = FMath::Max(TitleW, NameW) + 64.0f;
	const float CardH = TitleH + NameH + 34.0f;
	const float X = (Canvas->SizeX - CardW) * 0.5f;

	// A quarter of the way down: high enough to read as an announcement
	// rather than a targeting prompt, low enough to stay clear of the quest
	// tracker in the top right.
	const float Y = Canvas->SizeY * 0.16f;

	DrawRect(FLinearColor(0.04f, 0.02f, 0.02f, 0.80f * Alpha), X, Y, CardW, CardH);
	DrawRect(FLinearColor(0.55f, 0.12f, 0.10f, 0.9f * Alpha), X, Y, CardW, 3.0f);
	DrawRect(FLinearColor(0.55f, 0.12f, 0.10f, 0.9f * Alpha), X, Y + CardH - 3.0f, CardW, 3.0f);

	DrawText(TitleLine, FLinearColor(0.95f, 0.25f, 0.20f, Alpha),
		(Canvas->SizeX - TitleW) * 0.5f, Y + 12.0f, GEngine->GetLargeFont(), 1.0f);
	DrawText(NameLine, FLinearColor(1.0f, 1.0f, 1.0f, Alpha),
		(Canvas->SizeX - NameW) * 0.5f, Y + TitleH + 20.0f, GEngine->GetMediumFont(), 1.0f);
}

void ADCCHUD::DrawRoverControlsOverlay()
{
	const ADCCRover* Rover = GetOwningRover();
	if (!Canvas || !Rover || !Rover->IsOccupied())
	{
		return;
	}
	// Repo Rally draws its own kit in this corner, and the status panel already says how to drive.
	if (const ADCCArcadeArena* Arena = GetArcadeArena())
	{
		if (Arena->GetGameId() == TEXT("Kart"))
		{
			return;
		}
	}

	// A guide, so it shows once per player (2026-11-14): through their first 25 seconds of driving, then never.
	if (IsGuideSeen(TEXT("RoverControls")))
	{
		return;
	}
	RoverGuideSeconds += GetWorld() ? GetWorld()->GetDeltaSeconds() : 0.0f;
	if (RoverGuideSeconds > 25.0f)
	{
		MarkGuideSeen(TEXT("RoverControls"));
	}

	static const TCHAR* Rows[] = {
		TEXT("W / S       Throttle"),
		TEXT("A / D       Steer"),
		TEXT("Space       Climb"),
		TEXT("Left Ctrl   Descend"),
		TEXT("Left Click  Boost"),
		TEXT("R           Windows"),
		TEXT("T           Radio"),
		TEXT("C           Next song"),
		TEXT("E           Exit"),
	};
	constexpr int32 RowCount = UE_ARRAY_COUNT(Rows);

	constexpr float PanelW = 300.0f;
	const float PanelH = (RowCount + 2) * LineHeight + 16.0f;

	// Bottom-right: the quest tracker already owns the top-right (see
	// DrawBossBanner's own comment on that), and the vitals/Feed cluster
	// owns the bottom-left.
	const float X = Canvas->SizeX - PanelW - 32.0f;
	float Y = Canvas->SizeY - PanelH - 32.0f;

	DrawRect(FLinearColor(0.42f, 0.44f, 0.52f, 0.45f), X - 10.0f, Y - 10.0f, PanelW + 20.0f, PanelH + 20.0f);
	DrawRect(FLinearColor(0.02f, 0.03f, 0.06f, 0.72f), X - 8.0f, Y - 8.0f, PanelW + 16.0f, PanelH + 16.0f);

	DrawText(Rover->IsFlying() ? TEXT("FLIGHT CONTROLS") : TEXT("ROVER CONTROLS"), HeadingColour, X, Y);
	Y += LineHeight + 4.0f;

	for (const TCHAR* Row : Rows)
	{
		DrawText(Row, BodyColour, X, Y);
		Y += LineHeight;
	}

	// The one live readout worth showing here, rather than a bank of
	// numbers - whether boost is up is the one flight-relevant thing a
	// glance at this panel should answer that the world itself can't show.
	Y += 4.0f;
	const float BoostFraction = Rover->GetBoostChargeFraction();
	DrawRect(FLinearColor(0.15f, 0.15f, 0.18f, 0.8f), X, Y, PanelW - 16.0f, 8.0f);
	DrawRect(BoostFraction >= 1.0f ? FLinearColor(0.3f, 0.85f, 0.4f) : FLinearColor(0.6f, 0.5f, 0.2f),
		X, Y, (PanelW - 16.0f) * BoostFraction, 8.0f);
}

// DrawSpeedGauge (the speedometer and altimeter) lives in UI/DCCHUD_VehicleGauges.cpp.

void ADCCHUD::DrawQuestCompleteBanner()
{
	const UWorld* World = GetWorld();
	const UDCCQuestComponent* QuestComponent = World && World->GetGameState()
		? World->GetGameState()->FindComponentByClass<UDCCQuestComponent>() : nullptr;

	// New completion since the last check - arm the card. Checked ahead of
	// the "is one currently showing" logic below so a second quest
	// completing while the first card is still fading still gets its own
	// card rather than being swallowed.
	if (QuestComponent && !QuestComponent->LastCompletedQuestId.IsNone()
		&& QuestComponent->LastCompletedQuestId != LastShownQuestCompleteId)
	{
		LastShownQuestCompleteId = QuestComponent->LastCompletedQuestId;

		FDCCQuestDef Definition;
		QuestComponent->FindQuest(LastShownQuestCompleteId, Definition);
		QuestCompleteTitle = Definition.Title.IsEmpty()
			? FText::FromName(LastShownQuestCompleteId) : Definition.Title;
		QuestCompleteBannerExpiryTime = World->GetTimeSeconds() + QuestCompleteBannerDurationSeconds;
	}

	if (!Canvas || !World || QuestCompleteTitle.IsEmpty()
		|| World->GetTimeSeconds() >= QuestCompleteBannerExpiryTime)
	{
		return;
	}

	const float TimeRemaining = QuestCompleteBannerExpiryTime - World->GetTimeSeconds();
	const float Elapsed = QuestCompleteBannerDurationSeconds - TimeRemaining;
	constexpr float FadeSeconds = 0.5f;
	const float Alpha = FMath::Min(
		FMath::Clamp(Elapsed / FadeSeconds, 0.0f, 1.0f),
		FMath::Clamp(TimeRemaining / FadeSeconds, 0.0f, 1.0f));

	const FString TitleLine = TEXT("QUEST COMPLETE");
	const FString NameLine = QuestCompleteTitle.ToString();

	float TitleW = 0.0f, TitleH = 0.0f;
	GetTextSize(TitleLine, TitleW, TitleH, GEngine->GetLargeFont(), 1.0f);
	float NameW = 0.0f, NameH = 0.0f;
	GetTextSize(NameLine, NameW, NameH, GEngine->GetMediumFont(), 1.0f);

	const float CardW = FMath::Max(TitleW, NameW) + 64.0f;
	const float CardH = TitleH + NameH + 34.0f;
	const float X = (Canvas->SizeX - CardW) * 0.5f;

	// A little lower than the boss banner's own 0.16, so the two can never
	// land on top of each other if a kill and a quest complete together.
	const float Y = Canvas->SizeY * 0.24f;

	DrawRect(FLinearColor(0.02f, 0.04f, 0.02f, 0.80f * Alpha), X, Y, CardW, CardH);
	DrawRect(FLinearColor(0.20f, 0.55f, 0.20f, 0.9f * Alpha), X, Y, CardW, 3.0f);
	DrawRect(FLinearColor(0.20f, 0.55f, 0.20f, 0.9f * Alpha), X, Y + CardH - 3.0f, CardW, 3.0f);

	DrawText(TitleLine, FLinearColor(0.35f, 0.95f, 0.35f, Alpha),
		(Canvas->SizeX - TitleW) * 0.5f, Y + 12.0f, GEngine->GetLargeFont(), 1.0f);
	DrawText(NameLine, FLinearColor(1.0f, 1.0f, 1.0f, Alpha),
		(Canvas->SizeX - NameW) * 0.5f, Y + TitleH + 20.0f, GEngine->GetMediumFont(), 1.0f);
}

void ADCCHUD::DrawGauntletProgress()
{
	UWorld* World = GetWorld();
	if (!Canvas || !World)
	{
		return;
	}

	const float Now = World->GetTimeSeconds();

	// Refreshed every couple of seconds via a world search rather than every
	// frame - there is normally exactly one gauntlet in the level, so this is
	// the same cost/benefit call DrawQuestCompleteBanner makes for its own
	// GameState component lookup, just with an actor iterator instead.
	if (!CachedGauntlet.IsValid() && Now >= GauntletCacheRefreshTime)
	{
		GauntletCacheRefreshTime = Now + 2.0f;
		for (TActorIterator<ADCCGauntletEncounter> It(World); It; ++It)
		{
			CachedGauntlet = *It;
			break;
		}
	}

	ADCCGauntletEncounter* Gauntlet = CachedGauntlet.Get();
	if (!Gauntlet)
	{
		return;
	}

	const EDCCGauntletState CurrentState = Gauntlet->GetState();

	if (CurrentState == EDCCGauntletState::Active)
	{
		const FString Line = FString::Printf(TEXT("GAUNTLET  %d / %d"),
			Gauntlet->GetKillCount(), Gauntlet->TargetKillCount);

		float LineW = 0.0f, LineH = 0.0f;
		GetTextSize(Line, LineW, LineH, GEngine->GetMediumFont(), 1.0f);

		const float X = (Canvas->SizeX - LineW) * 0.5f;
		const float Y = 36.0f;

		DrawRect(FLinearColor(0.0f, 0.0f, 0.0f, 0.55f), X - 14.0f, Y - 6.0f, LineW + 28.0f, LineH + 12.0f);
		DrawText(Line, FLinearColor(0.95f, 0.75f, 0.15f, 1.0f), X, Y, GEngine->GetMediumFont(), 1.0f);
	}

	// Same poll-and-compare-a-replicated-value shape as
	// DrawQuestCompleteBanner, just diffing the state enum itself instead of
	// a dedicated id - a gauntlet only ever makes this transition once
	// between resets, so there is nothing else worth keying off.
	if (CurrentState == EDCCGauntletState::Victory && LastObservedGauntletState != EDCCGauntletState::Victory)
	{
		GauntletVictoryBannerExpiryTime = Now + 5.0f;
	}
	LastObservedGauntletState = CurrentState;

	if (Now >= GauntletVictoryBannerExpiryTime)
	{
		return;
	}

	constexpr float BannerDurationSeconds = 5.0f;
	const float TimeRemaining = GauntletVictoryBannerExpiryTime - Now;
	const float Elapsed = BannerDurationSeconds - TimeRemaining;
	constexpr float FadeSeconds = 0.6f;
	const float Alpha = FMath::Min(
		FMath::Clamp(Elapsed / FadeSeconds, 0.0f, 1.0f),
		FMath::Clamp(TimeRemaining / FadeSeconds, 0.0f, 1.0f));

	const FString TitleLine = TEXT("GAUNTLET CLEARED");
	const FString NameLine = FString::Printf(TEXT("%d down"), Gauntlet->TargetKillCount);

	float TitleW = 0.0f, TitleH = 0.0f;
	GetTextSize(TitleLine, TitleW, TitleH, GEngine->GetLargeFont(), 1.0f);
	float NameW = 0.0f, NameH = 0.0f;
	GetTextSize(NameLine, NameW, NameH, GEngine->GetMediumFont(), 1.0f);

	const float CardW = FMath::Max(TitleW, NameW) + 64.0f;
	const float CardH = TitleH + NameH + 34.0f;
	const float X = (Canvas->SizeX - CardW) * 0.5f;

	// A third of the way down: clear of the boss banner (0.16) and the quest
	// banner (0.24) so all three can never overlap if they ever coincide.
	const float Y = Canvas->SizeY * 0.32f;

	DrawRect(FLinearColor(0.05f, 0.04f, 0.0f, 0.80f * Alpha), X, Y, CardW, CardH);
	DrawRect(FLinearColor(0.75f, 0.55f, 0.10f, 0.9f * Alpha), X, Y, CardW, 3.0f);
	DrawRect(FLinearColor(0.75f, 0.55f, 0.10f, 0.9f * Alpha), X, Y + CardH - 3.0f, CardW, 3.0f);

	DrawText(TitleLine, FLinearColor(1.0f, 0.85f, 0.25f, Alpha),
		(Canvas->SizeX - TitleW) * 0.5f, Y + 12.0f, GEngine->GetLargeFont(), 1.0f);
	DrawText(NameLine, FLinearColor(1.0f, 1.0f, 1.0f, Alpha),
		(Canvas->SizeX - NameW) * 0.5f, Y + TitleH + 20.0f, GEngine->GetMediumFont(), 1.0f);
}

void ADCCHUD::DrawSkillTargetingOverlay(ADCCCharacterBase* Crawler)
{
	if (!Crawler || !Canvas)
	{
		return;
	}

	const FName SkillId = Crawler->GetSelectedSkillId();
	if (SkillId.IsNone())
	{
		return;
	}

	const float CenterX = Canvas->SizeX * 0.5f;
	const float CenterY = Canvas->SizeY * 0.5f;

	AActor* Target = Crawler->FindSkillTargetUnderCursor(SkillId);
	const bool bSelfCast = (Target == Crawler);
	const bool bValidTarget = (Target != nullptr) && !bSelfCast;

	// Amber "armed, aim by facing" for Self/Cone/AoE, green for a hostile
	// actually under the cursor, dim red for armed-but-nothing-there. Three
	// states because those are the three things a click could currently do.
	const FLinearColor ReticleColour = bSelfCast
		? FLinearColor(0.85f, 0.70f, 0.20f)
		: bValidTarget
			? FLinearColor(0.30f, 0.90f, 0.40f)
			: FLinearColor(0.75f, 0.25f, 0.22f, 0.85f);

	// A crosshair rather than a dot: legible at a glance and consistent with
	// the third-person camera already centring aim on screen for melee and
	// climbing, so a skill reticle in the same spot reads as "the same kind of
	// aiming", not a new mechanic.
	constexpr float ArmLength = 14.0f;
	constexpr float Thickness = 3.0f;
	constexpr float Gap = 7.0f;

	DrawRect(ReticleColour, CenterX + Gap, CenterY - Thickness * 0.5f, ArmLength, Thickness);
	DrawRect(ReticleColour, CenterX - Gap - ArmLength, CenterY - Thickness * 0.5f, ArmLength, Thickness);
	DrawRect(ReticleColour, CenterX - Thickness * 0.5f, CenterY - Gap - ArmLength, Thickness, ArmLength);
	DrawRect(ReticleColour, CenterX - Thickness * 0.5f, CenterY + Gap, Thickness, ArmLength);

	// A valid hostile target additionally gets corner brackets at its own
	// screen position - the reticle says "you are aiming", the brackets say
	// "at this specific thing", which matters once more than one candidate is
	// on screen at once.
	if (bValidTarget && PlayerOwner)
	{
		const float HalfHeight = Target->GetSimpleCollisionHalfHeight();
		const FVector TargetCenter = Target->GetActorLocation() + FVector(0.0f, 0.0f, HalfHeight);

		FVector2D ScreenPos;
		if (PlayerOwner->ProjectWorldLocationToScreen(TargetCenter, ScreenPos))
		{
			constexpr float BoxHalf = 26.0f;
			constexpr float BracketLen = 10.0f;
			constexpr float BracketThick = 2.5f;

			const float Left = ScreenPos.X - BoxHalf;
			const float Right = ScreenPos.X + BoxHalf;
			const float Top = ScreenPos.Y - BoxHalf;
			const float Bottom = ScreenPos.Y + BoxHalf;

			// Four corners, each an L of two thin rects, rather than a full
			// outline - reads as a targeting reticle rather than a selection box.
			DrawRect(ReticleColour, Left, Top, BracketLen, BracketThick);
			DrawRect(ReticleColour, Left, Top, BracketThick, BracketLen);

			DrawRect(ReticleColour, Right - BracketLen, Top, BracketLen, BracketThick);
			DrawRect(ReticleColour, Right - BracketThick, Top, BracketThick, BracketLen);

			DrawRect(ReticleColour, Left, Bottom - BracketThick, BracketLen, BracketThick);
			DrawRect(ReticleColour, Left, Bottom - BracketLen, BracketThick, BracketLen);

			DrawRect(ReticleColour, Right - BracketLen, Bottom - BracketThick, BracketLen, BracketThick);
			DrawRect(ReticleColour, Right - BracketThick, Bottom - BracketLen, BracketThick, BracketLen);
		}
	}

	// Named beneath the reticle so an armed skill never reads as an unlabelled
	// crosshair - the player should be able to glance down and confirm which
	// of their five slots they are about to spend.
	FDCCSkillDef Skill;
	if (Crawler->Skills && Crawler->Skills->FindSkill(SkillId, Skill))
	{
		const FText Label = Skill.DisplayName.IsEmpty() ? FText::FromName(SkillId) : Skill.DisplayName;
		float LabelW = 0.0f, LabelH = 0.0f;
		GetTextSize(Label.ToString(), LabelW, LabelH, GEngine->GetSmallFont(), 1.0f);
		DrawText(Label.ToString(), ReticleColour, CenterX - LabelW * 0.5f, CenterY + Gap + ArmLength + 6.0f,
			GEngine->GetSmallFont(), 1.0f);
	}
}

float ADCCHUD::DrawAchievements(float X, float Y, ADCCCharacterBase* Crawler)
{
	UDCCAchievementComponent* Achievements = Crawler->Achievements;
	if (!Achievements)
	{
		return Y;
	}

	TArray<FDCCAchievementDef> Visible;
	Achievements->GetVisibleAchievements(Visible);

	// Pending rewards first and separate from the list below - these are
	// things to DO, not things to browse, and burying them at position eleven
	// of a sorted list is how a reward sits unopened for three floors.
	TArray<FName> Pending;
	Achievements->GetPendingRewardIds(Pending);

	if (!Pending.IsEmpty())
	{
		DrawText(FString::Printf(TEXT("Pending Rewards  (%d)   click to open"), Pending.Num()),
			HeadingColour, X, Y);
		Y += LineHeight;

		constexpr float RowWidth = 420.0f;

		for (FName Id : Pending)
		{
			const FDCCAchievementDef* Def = Visible.FindByPredicate(
				[Id](const FDCCAchievementDef& Candidate) { return Candidate.AchievementId == Id; });

			if (!Def)
			{
				continue;
			}

			AddHitBox(FVector2D(X, Y), FVector2D(RowWidth, LineHeight),
				FName(*(TEXT("Claim|") + Id.ToString())), /*bConsumesInput=*/true, /*Priority=*/2);

			DrawText(FString::Printf(TEXT("  [OPEN] %s"), *Def->DisplayName.ToString()),
				FLinearColor(1.0f, 0.82f, 0.45f), X, Y);
			Y += LineHeight;
		}

		Y += SectionGap;
	}

	// Bulk claim: one row that does everything Pending Rewards above would
	// take several clicks to do, plus every sealed box already sitting in
	// the pack - shown only when there is actually something for it to do,
	// same "don't draw a row that would do nothing" convention as the rest
	// of this HUD. Always clickable regardless of location: claiming pays
	// out anywhere, same as the individual Claim rows above; only the
	// box-opening half needs a Safe Room, and ClaimAndOpenAllLootBoxes says
	// so itself via a local notice if that part gets skipped.
	{
		int32 OwnedBoxes = 0;
		if (UDCCInventoryComponent* PlayerInventory = Crawler->Inventory)
		{
			for (uint8 TierIndex = 1; TierIndex < (uint8)EDCCLootBoxTier::MAX; ++TierIndex)
			{
				OwnedBoxes += PlayerInventory->GetLootBoxCount((EDCCLootBoxTier)TierIndex);
			}
		}

		if (!Pending.IsEmpty() || OwnedBoxes > 0)
		{
			constexpr float RowWidth = 420.0f;
			AddHitBox(FVector2D(X, Y), FVector2D(RowWidth, LineHeight),
				FName("ClaimAllLootBoxes"), /*bConsumesInput=*/true, /*Priority=*/2);

			DrawText(FString::Printf(TEXT("  [CLAIM ALL + OPEN %d SEALED BOX(ES)]"), OwnedBoxes),
				FLinearColor(1.0f, 0.82f, 0.45f), X, Y);
			Y += LineHeight;
			Y += SectionGap;
		}
	}

	DrawText(FString::Printf(TEXT("Achievements  (%d)   click one for detail"),
		Achievements->GetUnlockedCount()), HeadingColour, X, Y);
	Y += LineHeight;

	if (Visible.IsEmpty())
	{
		DrawText(TEXT("  (no achievement table assigned)"), DimColour, X, Y);
		return Y + LineHeight;
	}

	// Unlocked first, then the closest to unlocking. A flat list would bury the
	// one the player is two kills away from under twenty they will never see.
	Visible.Sort([Achievements](const FDCCAchievementDef& A, const FDCCAchievementDef& B)
	{
		const bool bAUnlocked = Achievements->IsUnlocked(A.AchievementId);
		const bool bBUnlocked = Achievements->IsUnlocked(B.AchievementId);

		if (bAUnlocked != bBUnlocked)
		{
			return bAUnlocked;
		}

		const float AProgress = A.Threshold > 0
			? static_cast<float>(Achievements->GetStatTotal(A.Stat)) / A.Threshold : 0.0f;
		const float BProgress = B.Threshold > 0
			? static_cast<float>(Achievements->GetStatTotal(B.Stat)) / B.Threshold : 0.0f;

		return AProgress > BProgress;
	});

	int32 Shown = 0;
	for (const FDCCAchievementDef& Definition : Visible)
	{
		if (Shown++ >= 8)
		{
			DrawText(FString::Printf(TEXT("  ... and %d more"), Visible.Num() - 8), DimColour, X, Y);
			Y += LineHeight;
			break;
		}

		const bool bUnlocked = Achievements->IsUnlocked(Definition.AchievementId);
		const int32 Total = Achievements->GetStatTotal(Definition.Stat);
		const bool bSelected = SelectedAchievementId == Definition.AchievementId;

		// Registered fresh every frame - hit boxes are cleared between frames, so
		// this is a per-frame declaration rather than a one-off registration.
		constexpr float RowWidth = 420.0f;
		AddHitBox(FVector2D(X, Y), FVector2D(RowWidth, LineHeight),
			Definition.AchievementId, /*bConsumesInput=*/true, /*Priority=*/1);

		if (bSelected)
		{
			DrawRect(FLinearColor(0.16f, 0.18f, 0.26f, 0.85f), X - 4.0f, Y - 1.0f, RowWidth, LineHeight);
		}

		const FString Line = bUnlocked
			? FString::Printf(TEXT("  [X] %s"), *Definition.DisplayName.ToString())
			: FString::Printf(TEXT("  [ ] %-26s %d / %d"),
				*Definition.DisplayName.ToString(), FMath::Min(Total, Definition.Threshold), Definition.Threshold);

		DrawText(Line, bUnlocked ? HeadingColour : (bSelected ? BodyColour : DimColour), X, Y);
		Y += LineHeight;
	}

	return Y;
}

namespace
{
	/**
	 * Canvas DrawText does not wrap, so a two-sentence announcement would run off
	 * the panel and off the screen. Greedy word wrap at a character count rather
	 * than a measured width - the HUD font is fixed and this is a caption, not
	 * a layout engine.
	 */
	void WrapText(const FString& Source, int32 CharactersPerLine, TArray<FString>& OutLines)
	{
		OutLines.Reset();

		TArray<FString> Words;
		Source.ParseIntoArray(Words, TEXT(" "), /*CullEmpty=*/true);

		FString Current;
		for (const FString& Word : Words)
		{
			if (!Current.IsEmpty() && Current.Len() + 1 + Word.Len() > CharactersPerLine)
			{
				OutLines.Add(Current);
				Current.Reset();
			}

			Current += Current.IsEmpty() ? Word : TEXT(" ") + Word;
		}

		if (!Current.IsEmpty())
		{
			OutLines.Add(Current);
		}
	}

	FString DescribeAchievementStat(EDCCAchievementStat Stat)
	{
		switch (Stat)
		{
		case EDCCAchievementStat::Kills:             return TEXT("characters put down");
		case EDCCAchievementStat::EncountersCleared: return TEXT("encounters cleared");
		case EDCCAchievementStat::FloorsCleared:     return TEXT("floors cleared");
		case EDCCAchievementStat::DamageDealt:       return TEXT("damage dealt");
		case EDCCAchievementStat::DamageTaken:       return TEXT("damage taken");
		case EDCCAchievementStat::SkillRanksGained:  return TEXT("skill ranks gained");
		case EDCCAchievementStat::LevelsGained:      return TEXT("levels gained");
		case EDCCAchievementStat::MetresClimbed:     return TEXT("metres climbed");
		case EDCCAchievementStat::FistBumps:         return TEXT("fist bumps");
		case EDCCAchievementStat::Deaths:            return TEXT("deaths");
		default:                                     return TEXT("unknown");
		}
	}
}

void ADCCHUD::DrawAchievementDetail(ADCCCharacterBase* Crawler)
{
	if (SelectedAchievementId.IsNone() || !Crawler || !Canvas)
	{
		return;
	}

	UDCCAchievementComponent* Achievements = Crawler->Achievements;
	if (!Achievements)
	{
		return;
	}

	TArray<FDCCAchievementDef> Visible;
	Achievements->GetVisibleAchievements(Visible);

	const FDCCAchievementDef* Found = Visible.FindByPredicate(
		[this](const FDCCAchievementDef& Candidate)
		{
			return Candidate.AchievementId == SelectedAchievementId;
		});

	// A selected achievement can legitimately vanish from the visible list - a
	// secret one unlocks and re-sorts, or the crawler is replaced on respawn.
	// Silently dropping the card is better than drawing a stale one.
	if (!Found)
	{
		SelectedAchievementId = NAME_None;
		return;
	}

	const bool bUnlocked = Achievements->IsUnlocked(Found->AchievementId);
	const int32 Total = Achievements->GetStatTotal(Found->Stat);

	constexpr float PanelW = 400.0f;
	constexpr int32 WrapChars = 44;

	const float X = 500.0f;
	float Y = 28.0f;

	TArray<FString> AnnouncementLines;
	WrapText(Found->Announcement.ToString(), WrapChars, AnnouncementLines);

	// Measured before drawing, same as the quest tracker, so the card is exactly
	// as tall as its contents rather than a fixed slab.
	int32 Lines = 4 + AnnouncementLines.Num();
	if (Found->ExperienceReward > 0) { ++Lines; }
	if (Found->FavorReward != 0)     { ++Lines; }
	if (Found->bSecret)              { ++Lines; }

	DrawRect(FLinearColor(0.42f, 0.44f, 0.52f, 0.5f), X - 14.0f, Y - 12.0f, PanelW + 28.0f, (Lines + 1) * LineHeight + 28.0f);
	DrawRect(FLinearColor(0.02f, 0.03f, 0.06f, 0.88f), X - 12.0f, Y - 10.0f, PanelW + 24.0f, (Lines + 1) * LineHeight + 24.0f);

	DrawText(Found->DisplayName.ToString(), HeadingColour, X, Y);
	Y += LineHeight + 4.0f;

	DrawText(bUnlocked ? TEXT("UNLOCKED") : TEXT("Locked"),
		bUnlocked ? FLinearColor(0.45f, 0.85f, 0.45f) : DimColour, X, Y);
	Y += LineHeight;

	DrawText(FString::Printf(TEXT("%d / %d %s"),
		FMath::Min(Total, Found->Threshold), Found->Threshold,
		*DescribeAchievementStat(Found->Stat)), BodyColour, X, Y);
	Y += LineHeight;

	// The bar reuses DrawMeter so it matches every other meter on screen.
	const float Fraction = Found->Threshold > 0
		? static_cast<float>(Total) / Found->Threshold : 0.0f;
	DrawMeter(X, Y, PanelW - 8.0f, 10.0f, Fraction,
		bUnlocked ? FLinearColor(0.45f, 0.85f, 0.45f) : FLinearColor(0.35f, 0.55f, 0.85f), FString());
	Y += LineHeight;

	if (Found->ExperienceReward > 0)
	{
		DrawText(FString::Printf(TEXT("Pays %d XP"), Found->ExperienceReward), BodyColour, X, Y);
		Y += LineHeight;
	}

	if (Found->FavorReward != 0)
	{
		// Negative favor is the interesting case and should read as a penalty,
		// not as a smaller reward - hence the explicit sign and the red.
		DrawText(FString::Printf(TEXT("AI Favor %+d"), Found->FavorReward),
			Found->FavorReward < 0 ? FLinearColor(0.90f, 0.45f, 0.40f) : BodyColour, X, Y);
		Y += LineHeight;
	}

	if (Found->bSecret)
	{
		DrawText(TEXT("Secret - hidden until earned"), DimColour, X, Y);
		Y += LineHeight;
	}

	Y += 4.0f;
	for (const FString& Line : AnnouncementLines)
	{
		// The System AI's line, dimmed - it is flavour, not a stat.
		DrawText(Line, DimColour, X, Y);
		Y += LineHeight;
	}
}

void ADCCHUD::DrawDownedScreen(ADCCCharacterBase* Crawler)
{
	// Amber rather than the death screen's red. The colour is doing real work
	// here: this state is recoverable and the palette should say so before any
	// of the text is read.
	DrawRect(FLinearColor(0.35f, 0.20f, 0.02f, 0.40f), 0.0f, 0.0f, Canvas->SizeX, Canvas->SizeY);

	const float CentreX = Canvas->SizeX * 0.5f;
	const float CentreY = Canvas->SizeY * 0.5f;

	const FString Headline = TEXT("DOWN");

	float HeadW = 0.0f, HeadH = 0.0f;
	GetTextSize(Headline, HeadW, HeadH, GEngine->GetLargeFont(), 2.0f);
	DrawText(Headline, FLinearColor(1.0f, 0.90f, 0.70f),
		CentreX - HeadW * 0.5f, CentreY - 60.0f, GEngine->GetLargeFont(), 2.0f);

	// Pips rather than a countdown - death saves are a beat, not a timer, and
	// the tabletop rhythm is the point: three of one side ends the sequence,
	// however long that actually takes.
	auto BuildPips = [](int32 Count, int32 Needed) -> FString
	{
		FString Result;
		for (int32 Index = 0; Index < Needed; ++Index)
		{
			Result += Index < Count ? TEXT("● ") : TEXT("○ ");
		}
		return Result;
	};

	const int32 Successes = Crawler ? Crawler->GetDeathSaveSuccesses() : 0;
	const int32 Failures = Crawler ? Crawler->GetDeathSaveFailures() : 0;

	const FString SuccessLine = FString::Printf(TEXT("Successes   %s"),
		*BuildPips(Successes, Crawler ? Crawler->DeathSavesToStabilize : 3));
	const FString FailureLine = FString::Printf(TEXT("Failures    %s"),
		*BuildPips(Failures, Crawler ? Crawler->DeathSavesToExpire : 3));

	float SuccessW = 0.0f, SuccessH = 0.0f;
	GetTextSize(SuccessLine, SuccessW, SuccessH, GEngine->GetMediumFont(), 1.2f);
	DrawText(SuccessLine, FLinearColor(0.45f, 0.90f, 0.55f),
		CentreX - SuccessW * 0.5f, CentreY + 6.0f, GEngine->GetMediumFont(), 1.2f);

	float FailureW = 0.0f, FailureH = 0.0f;
	GetTextSize(FailureLine, FailureW, FailureH, GEngine->GetMediumFont(), 1.2f);
	DrawText(FailureLine, FLinearColor(0.90f, 0.40f, 0.35f),
		CentreX - FailureW * 0.5f, CentreY + 6.0f + SuccessH + 4.0f, GEngine->GetMediumFont(), 1.2f);

	const FString Footer = TEXT("Any crawler may reach you and press E - or the dice might do it for you.");
	float FootW = 0.0f, FootH = 0.0f;
	GetTextSize(Footer, FootW, FootH, GEngine->GetMediumFont(), 1.0f);
	DrawText(Footer, FLinearColor(0.82f, 0.75f, 0.65f),
		CentreX - FootW * 0.5f, CentreY + 6.0f + SuccessH + FailureH + 24.0f, GEngine->GetMediumFont(), 1.0f);
}

void ADCCHUD::DrawDeathScreen()
{
	// Dim the whole screen rather than clear it: the fight carrying on behind the
	// overlay is the point, since the rest of the party is still in it.
	DrawRect(FLinearColor(0.35f, 0.02f, 0.02f, 0.45f), 0.0f, 0.0f, Canvas->SizeX, Canvas->SizeY);

	const float CentreX = Canvas->SizeX * 0.5f;
	const float CentreY = Canvas->SizeY * 0.5f;

	// Per the tone bible the System AI never breaks to sincerity - when something
	// genuinely bad happens it gets quieter and more corporate, and that contrast
	// does the emotional work.
	const FString Headline = TEXT("GAME OVER");
	const FString Subtitle = TEXT("Your viewership has ended. Thank you for participating.");
	const FString Footer   = TEXT("A complimentary difficulty adjustment has been scheduled.");

	float HeadW = 0.0f, HeadH = 0.0f;
	GetTextSize(Headline, HeadW, HeadH, GEngine->GetLargeFont(), 2.0f);

	DrawText(Headline, FLinearColor(1.0f, 0.92f, 0.92f),
		CentreX - HeadW * 0.5f, CentreY - 60.0f, GEngine->GetLargeFont(), 2.0f);

	float SubW = 0.0f, SubH = 0.0f;
	GetTextSize(Subtitle, SubW, SubH, GEngine->GetMediumFont(), 1.0f);
	DrawText(Subtitle, FLinearColor(0.95f, 0.85f, 0.85f),
		CentreX - SubW * 0.5f, CentreY + 10.0f, GEngine->GetMediumFont(), 1.0f);

	float FootW = 0.0f, FootH = 0.0f;
	GetTextSize(Footer, FootW, FootH, GEngine->GetMediumFont(), 1.0f);
	DrawText(Footer, FLinearColor(0.8f, 0.7f, 0.7f),
		CentreX - FootW * 0.5f, CentreY + 40.0f, GEngine->GetMediumFont(), 1.0f);
}

void ADCCHUD::DrawAwaitingOtherCrawlersScreen()
{
	// Solid, not dimmed - unlike DrawDeathScreen this is not "the fight
	// carries on behind you", there is genuinely nothing further to show.
	DrawRect(FLinearColor(0.0f, 0.0f, 0.0f, 1.0f), 0.0f, 0.0f, Canvas->SizeX, Canvas->SizeY);

	const float CentreX = Canvas->SizeX * 0.5f;
	const float CentreY = Canvas->SizeY * 0.5f;

	const FString Readout = TEXT("AWAITING OTHER CRAWLERS");

	float ReadoutW = 0.0f, ReadoutH = 0.0f;
	GetTextSize(Readout, ReadoutW, ReadoutH, GEngine->GetLargeFont(), 1.5f);

	DrawText(Readout, FLinearColor(0.85f, 0.85f, 0.90f),
		CentreX - ReadoutW * 0.5f, CentreY - ReadoutH * 0.5f, GEngine->GetLargeFont(), 1.5f);
}

void ADCCHUD::DrawInteractionPrompt(ADCCCharacterBase* Crawler)
{
	if (!Crawler || !GetWorld())
	{
		return;
	}

	// Three mutually exclusive states, checked most-committed first: already on a
	// wall, being asked about one, or looking for one.
	FString Prompt;
	FLinearColor Colour = HeadingColour;

	if (Crawler->IsClimbing())
	{
		Prompt = TEXT("Keep holding [LMB] to climb.  Let go to drop.  [RMB] climbs down.");
		Colour = BodyColour;
	}
	else if (Crawler->IsClimbingEquipped())
	{
		Prompt = TEXT("Climbing ready: hold [LMB] and walk into a wall.");
		Colour = DimColour;
	}
	else if (const ADCCClimbPoint* Pending = Crawler->GetPendingClimbPoint())
	{
		// The question itself is authored on the climb point, so a particular
		// cliff can ask in its own voice.
		Prompt = FString::Printf(TEXT("%s   Click: yes   Right click: no"), *Pending->PromptText.ToString());
	}
	else if (Crawler->HasBeltArmed())
	{
		Prompt = FString::Printf(TEXT("%s ready: click to use it.  Right click puts it away."), *Crawler->GetArmedBeltName());
		Colour = FLinearColor(1.0f, 0.86f, 0.55f);
	}
	else if (!Crawler->GetSelectedSkillId().IsNone())
	{
		Prompt = TEXT("Click a surface to use the armed skill.");
		Colour = DimColour;
	}
	else
	{
		return;
	}

	float W = 0.0f, H = 0.0f;
	GetTextSize(Prompt, W, H, GEngine->GetMediumFont(), 1.0f);

	const float BoxX = Canvas->SizeX * 0.5f - W * 0.5f - 20.0f;
	const float BoxY = Canvas->SizeY * 0.62f;

	DrawRect(FLinearColor(0.02f, 0.03f, 0.06f, 0.78f), BoxX, BoxY - 8.0f, W + 40.0f, H + 20.0f);
	DrawText(Prompt, Colour, BoxX + 20.0f, BoxY, GEngine->GetMediumFont(), 1.0f);
}

void ADCCHUD::DrawStaminaBar(ADCCCharacterBase* Crawler)
{
	UDCCStatsComponent* Stats = Crawler ? Crawler->Stats : nullptr;
	if (!Stats || Stats->MaxStamina <= 0.0f)
	{
		return;
	}

	const float Fraction = Stats->GetStaminaFraction();
	// Fourteener's hunger (2026-11-15, Zoe: "have the hunger bar just steadily growing on the same bar as the stamina.
	// It just eats into the stamina"): hunger caps max stamina, so it is drawn as a dark band eating in from the right
	// end of this bar instead of a bar of its own. Cap is the share of the bar stamina can still fill.
	const UDCCWalletComponent* Wallet = Crawler->Wallet.Get();
	const bool bHunger = Wallet && Wallet->ClimbHunger >= 0.0f;
	const float Fed = bHunger ? FMath::Clamp(Wallet->ClimbHunger / 100.0f, 0.0f, 1.0f) : 1.0f;
	const float Cap = bHunger ? 0.35f + 0.65f * Fed : 1.0f;

	// Hidden at full stamina unless it is actively being spent. A bar that is
	// always there while nothing is draining it just teaches players to ignore it.
	// (Once hunger has bitten off a visible piece, it stays up, so you can see it coming.)
	if (Fraction >= 0.999f && !Crawler->IsSprinting() && !Crawler->IsClimbing() && Cap > 0.97f)
	{
		return;
	}

	constexpr float BarW = 260.0f;
	constexpr float BarH = 12.0f;

	const float BarX = Canvas->SizeX * 0.5f - BarW * 0.5f;
	const float BarY = Canvas->SizeY * 0.86f;

	DrawRect(FLinearColor(0.02f, 0.03f, 0.06f, 0.72f), BarX - 2.0f, BarY - 2.0f, BarW + 4.0f, BarH + 4.0f);

	// Reddens as it empties, so the "you are about to fall off this cliff"
	// warning is peripheral and does not need reading.
	const FLinearColor Fill = FMath::Lerp(
		FLinearColor(0.85f, 0.25f, 0.15f),
		FLinearColor(0.35f, 0.80f, 0.95f),
		FMath::Clamp(Fraction, 0.0f, 1.0f));

	DrawRect(Fill, BarX, BarY, BarW * FMath::Clamp(Fraction, 0.0f, 1.0f), BarH);
	if (bHunger && Cap < 0.999f)
	{
		// The bitten-off end: dark amber, with a bright edge where hunger meets what you can still use, and notches
		// so it reads as eaten rather than empty.
		const float HX = BarX + BarW * Cap;
		const float HW = BarW * (1.0f - Cap);
		const FLinearColor Hungry = Fed > 0.1f ? FLinearColor(0.55f, 0.32f, 0.08f, 0.95f) : FLinearColor(0.55f, 0.1f, 0.06f, 0.95f);
		DrawRect(Hungry, HX, BarY, HW, BarH);
		for (float N = HX + 6.0f; N < BarX + BarW - 2.0f; N += 9.0f)
		{
			DrawRect(FLinearColor(0.0f, 0.0f, 0.0f, 0.35f), N, BarY, 3.0f, BarH);
		}
		DrawRect(Fed > 0.1f ? FLinearColor(1.0f, 0.7f, 0.25f) : FLinearColor(1.0f, 0.3f, 0.2f), HX - 1.0f, BarY - 2.0f, 2.0f, BarH + 4.0f);
		const FString Note = Fed <= 0.1f ? TEXT("STARVING - eat something") : Fed < 0.45f ? TEXT("hungry") : TEXT("peckish");
		float NW = 0.0f, NH = 0.0f;
		GetTextSize(Note, NW, NH, GEngine->GetSmallFont(), 1.0f);
		DrawText(Note, Fed > 0.1f ? FLinearColor(1.0f, 0.72f, 0.3f) : FLinearColor(1.0f, 0.35f, 0.25f), BarX + BarW - NW, BarY - LineHeight);
	}
	DrawText(TEXT("Stamina"), DimColour, BarX, BarY - LineHeight);
}

float ADCCHUD::DrawStats(float X, float Y, ADCCCharacterBase* Crawler)
{
	UDCCStatsComponent* Stats = Crawler->Stats;
	if (!Stats)
	{
		return Y;
	}

	DrawText(TEXT("Stats"), HeadingColour, X, Y);
	Y += LineHeight;

	UDCCRuleset* Ruleset = Stats->GetRuleset();
	const UEnum* AbilityEnum = StaticEnum<EDCCAbility>();

	for (int32 Index = 0; Index < static_cast<int32>(EDCCAbility::MAX); ++Index)
	{
		const EDCCAbility Ability = static_cast<EDCCAbility>(Index);
		const int32 Score = Stats->GetEnhanced().GetScore(Ability);
		const int32 Modifier = Stats->GetAbilityModifier(Ability);

		// Both numbers, because the ruleset uses them for different things - the
		// score decides what you can lift, the modifier is what you add to a roll.
		const FString Line = FString::Printf(TEXT("  %-14s %2d   (+%d)"),
			AbilityEnum ? *AbilityEnum->GetDisplayNameTextByValue(Index).ToString() : TEXT("?"),
			Score, Modifier);

		DrawText(Line, BodyColour, X, Y);
		Y += LineHeight;
	}

	Y += 4.0f;
	DrawText(FString::Printf(TEXT("  Health   %d / %d"), Stats->CurrentHitPoints, Stats->MaxHitPoints), BodyColour, X, Y);
	Y += LineHeight;
	DrawText(FString::Printf(TEXT("  Mana     %d / %d"), Stats->CurrentManaPoints, Stats->MaxManaPoints), BodyColour, X, Y);
	Y += LineHeight;
	DrawText(FString::Printf(TEXT("  Stamina  %.0f / %.0f"), Stats->CurrentStamina, Stats->MaxStamina), BodyColour, X, Y);
	Y += LineHeight;

	if (Ruleset)
	{
		// Surfaced because it is the number that makes Strength legible: it says
		// exactly which props this character can pick up.
		DrawText(FString::Printf(TEXT("  Can lift %.0f kg"),
			Ruleset->GetCarryMassLimit(Stats->GetEnhanced())), DimColour, X, Y);
		Y += LineHeight;

		// The number a shield actually moves - surfaced so equipping one reads
		// as something changing rather than a silent stat nobody can see.
		const int32 EquipmentEvadeBonus = Crawler->Equipment ? Crawler->Equipment->GetEvadeBonus() : 0;
		DrawText(FString::Printf(TEXT("  Evade    %d"),
			Ruleset->GetDefenceValue(Stats->GetEnhanced(), Stats->Level, EquipmentEvadeBonus)), DimColour, X, Y);
		Y += LineHeight;
	}

	return Y;
}

float ADCCHUD::DrawSkills(float X, float Y, ADCCCharacterBase* Crawler)
{
	UDCCSkillComponent* Skills = Crawler->Skills;
	if (!Skills)
	{
		return Y;
	}

	DrawText(TEXT("Skills"), HeadingColour, X, Y);
	Y += LineHeight;

	if (Skills->KnownSkills.IsEmpty())
	{
		DrawText(TEXT("  (none yet - try something untrained)"), DimColour, X, Y);
		return Y + LineHeight;
	}

	for (int32 Index = 0; Index < Skills->KnownSkills.Num(); ++Index)
	{
		const FDCCKnownSkill& Known = Skills->KnownSkills[Index];

		FDCCSkillDef Definition;
		const bool bResolved = Skills->FindSkill(Known.SkillId, Definition);

		const FString Name = bResolved && !Definition.DisplayName.IsEmpty()
			? Definition.DisplayName.ToString()
			: Known.SkillId.ToString();

		// The number is the key that arms this skill, so the list doubles as the
		// keybind reference rather than needing a second one somewhere else.
		const FString Slot = Index < 9
			? FString::Printf(TEXT("[%d]"), Index + 1)
			: TEXT("   ");

		// The advancement mark is shown because it is otherwise invisible state
		// that decides whether this skill can rank up at the next interval.
		const FString Line = FString::Printf(TEXT("  %s %-22s Rank %-2d %s"),
			*Slot, *Name, Known.Rank, Known.bAdvancementMark ? TEXT("*") : TEXT(""));

		const bool bArmed = Crawler->GetSelectedSkillId() == Known.SkillId;

		DrawText(Line, bArmed ? HeadingColour : BodyColour, X, Y);
		Y += LineHeight;
	}

	DrawText(TEXT("  * used since last advancement"), DimColour, X, Y);
	Y += LineHeight;

	if (!Crawler->GetSelectedSkillId().IsNone())
	{
		DrawText(TEXT("  Armed - click a target in the world"), HeadingColour, X, Y);
		Y += LineHeight;
	}

	return Y;
}

float ADCCHUD::DrawInventory(float X, float Y, ADCCCharacterBase* Crawler)
{
	DrawText(TEXT("Inventory"), HeadingColour, X, Y);
	Y += LineHeight;

	// Held and stowed are genuinely different states - one is a physical object
	// in your hands that can be thrown, the other is a record in a bag - so the
	// menu shows them separately rather than pretending they are one list.
	if (UDCCCarryComponent* Carry = Crawler->Carry)
	{
		if (ADCCPickup* Held = Carry->CarriedPickup)
		{
			const FString Name = Held->DisplayName.IsEmpty()
				? Held->GetClass()->GetName()
				: Held->DisplayName.ToString();

			DrawText(FString::Printf(TEXT("  Holding: %s (%.0f kg)"), *Name, Held->MassKilograms),
				BodyColour, X, Y);
		}
		else
		{
			DrawText(TEXT("  Holding: nothing"), DimColour, X, Y);
		}
		Y += LineHeight;
	}

	// Previously shown nowhere except the vendor menu, which meant a player's
	// own gold total was invisible for the entire rest of the game - the
	// single most basic "what do I have" question this tab exists to answer.
	if (Crawler->Stats)
	{
		DrawText(FString::Printf(TEXT("  Gold: %d"), Crawler->Stats->Gold), BodyColour, X, Y);
		Y += LineHeight;
	}

	UDCCInventoryComponent* Inventory = Crawler->FindComponentByClass<UDCCInventoryComponent>();
	if (!Inventory)
	{
		return Y;
	}

	DrawText(FString::Printf(TEXT("  Stowed %d / %d  (%.0f kg)"),
		Inventory->Items.Num(), Inventory->Capacity, Inventory->GetTotalMass()), DimColour, X, Y);
	Y += LineHeight;

	for (int32 Index = 0; Index < Inventory->Items.Num(); ++Index)
	{
		const FDCCInventoryItem& Item = Inventory->Items[Index];
		const bool bIsGear = !Item.EquipmentItemId.IsNone();

		if (bIsGear)
		{
			constexpr float RowWidth = 380.0f;
			AddHitBox(FVector2D(X, Y), FVector2D(RowWidth, LineHeight),
				FName(*FString::Printf(TEXT("Equip|%d"), Index)), /*bConsumesInput=*/true, /*Priority=*/1);
		}

		DrawText(FString::Printf(TEXT("    %d. %s (%.0f kg)%s"),
			Index + 1, *Item.DisplayName.ToString(), Item.MassKilograms,
			bIsGear ? TEXT("  [click to equip]") : TEXT("")), BodyColour, X, Y);
		Y += LineHeight;
	}

	// An empty bag with no explanation reads as broken rather than empty - see
	// ShowNotice's own doc comment on why this project treats silence as the
	// worst possible feedback.
	if (Inventory->Items.IsEmpty())
	{
		DrawText(TEXT("    Nothing stowed yet."), DimColour, X, Y);
		Y += LineHeight;
	}

	// Whole corpses or chests carried away via "Stow Body" rather than looted on
	// the spot. Click a row to empty it at your feet - the same "Loot All" verb
	// the loot menu itself offers, just reached from the pack instead of from
	// standing over the thing.
	if (!Inventory->StowedRemains.IsEmpty())
	{
		Y += 4.0f;
		DrawText(TEXT("  Stowed remains (click to empty)"), HeadingColour, X, Y);
		Y += LineHeight;

		for (int32 Index = 0; Index < Inventory->StowedRemains.Num(); ++Index)
		{
			const FDCCStowedRemains& Remains = Inventory->StowedRemains[Index];

			constexpr float RowWidth = 380.0f;
			AddHitBox(FVector2D(X, Y), FVector2D(RowWidth, LineHeight),
				FName(*FString::Printf(TEXT("Stow|%d"), Index)), /*bConsumesInput=*/true, /*Priority=*/1);

			const int32 ItemCount = Remains.Contents.Num();
			DrawText(FString::Printf(TEXT("    %s (%d item%s)"),
				*Remains.DisplayName.ToString(), ItemCount, ItemCount == 1 ? TEXT("") : TEXT("s")),
				BodyColour, X, Y);
			Y += LineHeight;
		}
	}

	// Drinks bought and stowed for later rather than drunk at the bar. Click a
	// row to drink one now - same click-to-act convention as Stowed Remains
	// above, just a different verb.
	if (!Inventory->StowedConsumables.IsEmpty())
	{
		Y += 4.0f;
		DrawText(TEXT("  Stowed drinks (click to drink)"), HeadingColour, X, Y);
		Y += LineHeight;

		for (int32 Index = 0; Index < Inventory->StowedConsumables.Num(); ++Index)
		{
			const FDCCStowedConsumable& Stowed = Inventory->StowedConsumables[Index];

			constexpr float RowWidth = 380.0f;
			AddHitBox(FVector2D(X, Y), FVector2D(RowWidth, LineHeight),
				FName(*FString::Printf(TEXT("Drink|%d"), Index)), /*bConsumesInput=*/true, /*Priority=*/1);

			DrawText(FString::Printf(TEXT("    %s x%d"), *Stowed.DisplayName.ToString(), Stowed.Quantity),
				BodyColour, X, Y);
			Y += LineHeight;
		}
	}

	return Y;
}

namespace
{
	/**
	 * A little pixel mouse with its middle button (the wheel) lit, the button-prompt symbol for "middle click"
	 * (2026-11-17, Zoe: "instead of mmb can you just show a diagram type symbol of the mmb like other games might").
	 * Drawn in code, 9 x 12 pixels; draw it at a whole multiple so it stays crisp.
	 */
	UTexture2D* MiddleMouseIcon()
	{
		static TStrongObjectPtr<UTexture2D> Cached;
		if (Cached.IsValid())
		{
			return Cached.Get();
		}
		static const TCHAR* Rows[] = {
			TEXT("..#####.."),
			TEXT(".#..W..#."),
			TEXT("#...W...#"),
			TEXT("#...W...#"),
			TEXT("#########"),
			TEXT("#.......#"),
			TEXT("#.......#"),
			TEXT("#.......#"),
			TEXT("#.......#"),
			TEXT("#.......#"),
			TEXT(".#.....#."),
			TEXT("..#####.."),
		};
		constexpr int32 W = 9, H = 12;
		UTexture2D* Texture = UTexture2D::CreateTransient(W, H, PF_B8G8R8A8);
		if (!Texture || !Texture->GetPlatformData() || Texture->GetPlatformData()->Mips.IsEmpty())
		{
			return nullptr;
		}
		Texture->Filter = TF_Nearest;
		Texture->SRGB = true;
		const FColor Body(215, 220, 235, 255), Fill(30, 34, 48, 230), Wheel(255, 205, 90, 255);
		TArray<FColor> Pixels;
		Pixels.Init(FColor(0, 0, 0, 0), W * H);
		for (int32 Y = 0; Y < H; ++Y)
		{
			// Inside the outline is a dark fill, so the shape reads on any background.
			int32 First = -1, Last = -1;
			for (int32 X = 0; X < W; ++X)
			{
				if (Rows[Y][X] != TEXT('.')) { First = First < 0 ? X : First; Last = X; }
			}
			for (int32 X = 0; X < W; ++X)
			{
				const TCHAR C = Rows[Y][X];
				Pixels[Y * W + X] = C == TEXT('#') ? Body : C == TEXT('W') ? Wheel : (X > First && X < Last ? Fill : FColor(0, 0, 0, 0));
			}
		}
		FTexture2DMipMap& Mip = Texture->GetPlatformData()->Mips[0];
		FMemory::Memcpy(Mip.BulkData.Lock(LOCK_READ_WRITE), Pixels.GetData(), W * H * sizeof(FColor));
		Mip.BulkData.Unlock();
		Texture->UpdateResource();
		Cached = TStrongObjectPtr<UTexture2D>(Texture);
		return Texture;
	}
}

void ADCCHUD::DrawHotbar(ADCCCharacterBase* Crawler)
{
	if (!Crawler || !Canvas)
	{
		return;
	}

	// The belt (2026-11-16): every slot the number keys reach, built from what this crawler has right now.
	TArray<FDCCBeltEntry> Belt;
	Crawler->GetBelt(Belt);
	if (Belt.IsEmpty())
	{
		return;
	}
	const int32 SlotCount = Belt.Num();
	const float Gap = 6.0f;
	const float SlotW = FMath::Min(108.0f, (Canvas->SizeX - 40.0f - (SlotCount - 1) * Gap) / SlotCount);
	constexpr float SlotH = 46.0f;
	const float TotalW = SlotCount * SlotW + (SlotCount - 1) * Gap;
	const float StartX = (Canvas->SizeX - TotalW) * 0.5f;
	const float Y = Canvas->SizeY - SlotH - 22.0f;
	const int32 Chars = FMath::Max(6, FMath::FloorToInt((SlotW - 24.0f) / 7.5f));
	UDCCSkillComponent* SkillComp = Crawler->Skills;
	const FName ArmedSkill = Crawler->GetSelectedSkillId();

	for (int32 Index = 0; Index < SlotCount; ++Index)
	{
		const FDCCBeltEntry& E = Belt[Index];
		const float SlotX = StartX + Index * (SlotW + Gap);
		const bool bArmed = E.Kind == EDCCBelt::Skill ? (!ArmedSkill.IsNone() && ArmedSkill == E.ItemId) : Crawler->IsBeltArmed(E);
		const bool bReady = E.Cooldown <= 0.0f && E.Count != 0;

		// Border: gold when armed, green when a switch is on, grey otherwise. Then the dark fill.
		const FLinearColor Border = bArmed ? FLinearColor(0.85f, 0.70f, 0.20f, 0.95f)
			: E.bOn ? FLinearColor(0.35f, 0.80f, 0.50f, 0.85f)
			: FLinearColor(0.42f, 0.44f, 0.52f, 0.55f);
		DrawRect(Border, SlotX - 2.0f, Y - 2.0f, SlotW + 4.0f, SlotH + 4.0f);
		DrawRect(FLinearColor(0.02f, 0.03f, 0.06f, 0.85f), SlotX, Y, SlotW, SlotH);

		// A cooldown drains from the bottom up.
		float CooldownMax = 0.0f;
		if (E.Kind == EDCCBelt::Skill && SkillComp)
		{
			FDCCSkillDef Def;
			CooldownMax = SkillComp->FindSkill(E.ItemId, Def) ? Def.CooldownSeconds : 0.0f;
		}
		else if (E.Kind == EDCCBelt::Grapple && Crawler->Wallet)
		{
			CooldownMax = DCCChopShop::GrappleCooldown(Crawler->Wallet->ChopLevel(DCCChopShop::Grapple));
		}
		if (E.Cooldown > 0.0f && CooldownMax > 0.0f)
		{
			const float Fraction = FMath::Clamp(E.Cooldown / CooldownMax, 0.0f, 1.0f);
			DrawRect(FLinearColor(0.35f, 0.10f, 0.10f, 0.55f), SlotX, Y + SlotH * (1.0f - Fraction), SlotW, SlotH * Fraction);
		}

		DrawText(FString::Printf(TEXT("%d"), Index + 1), HeadingColour, SlotX + 5.0f, Y + 3.0f);

		// The skill's icon when its row has one; otherwise the name.
		UTexture2D* IconTexture = nullptr;
		if (E.Kind == EDCCBelt::Skill && SkillComp)
		{
			FDCCSkillDef Def;
			IconTexture = SkillComp->FindSkill(E.ItemId, Def) ? Def.Icon.LoadSynchronous() : nullptr;
		}
		if (IconTexture)
		{
			DrawTexture(IconTexture, SlotX + 19.0f, Y + 2.0f, 28.0f, 28.0f, 0.0f, 0.0f, 1.0f, 1.0f, bReady ? FLinearColor::White : FLinearColor(0.5f, 0.5f, 0.5f, 1.0f));
		}
		else
		{
			const FString Name = E.Name.Len() > Chars ? E.Name.Left(Chars - 1).TrimEnd() + TEXT(".") : E.Name;
			DrawText(Name, bReady ? BodyColour : DimColour, SlotX + 19.0f, Y + 3.0f);
		}

		// The second line: how long, how many, on or off, or how it is used.
		FString Detail;
		FLinearColor DetailColour = DimColour;
		bool bMouseHint = false;
		if (E.Cooldown > 0.0f)
		{
			Detail = FString::Printf(TEXT("%.0fs"), E.Cooldown);
			DetailColour = FLinearColor(0.90f, 0.55f, 0.45f);
		}
		else if (E.Kind == EDCCBelt::Lamp || E.Kind == EDCCBelt::BodyCam)
		{
			Detail = E.bOn ? (E.Kind == EDCCBelt::BodyCam ? TEXT("REC") : TEXT("on")) : TEXT("off");
			DetailColour = E.bOn ? (E.Kind == EDCCBelt::BodyCam ? FLinearColor(1.0f, 0.35f, 0.3f) : FLinearColor(1.0f, 0.85f, 0.55f)) : DimColour;
		}
		else if (E.Choices > 1)
		{
			// The food slot with more than one kind: the middle mouse button picks.
			Detail = FString::Printf(TEXT("x%d   %d/%d"), E.Count, FMath::Max(1, Crawler->GetFoodIndex() + 1), E.Choices);
			bMouseHint = true;
		}
		else if (E.Count >= 0)
		{
			Detail = FString::Printf(TEXT("x%d"), E.Count);
		}
		else if (E.Kind == EDCCBelt::Skill && SkillComp)
		{
			const FDCCKnownSkill* Known = SkillComp->KnownSkills.FindByPredicate([&E](const FDCCKnownSkill& K) { return K.SkillId == E.ItemId; });
			Detail = Known ? FString::Printf(TEXT("R%d"), Known->Rank) : FString();
		}
		else if (E.bAimed)
		{
			Detail = bArmed ? TEXT("click!") : TEXT("aim");
			DetailColour = bArmed ? FLinearColor(1.0f, 0.86f, 0.55f) : DimColour;
		}
		else if (E.Kind == EDCCBelt::Grapple)
		{
			Detail = TEXT("ready");
			DetailColour = FLinearColor(0.75f, 0.9f, 1.0f);
		}
		DrawText(Detail, DetailColour, SlotX + 6.0f, Y + SlotH - 18.0f);
		// The food slot: a little mouse with the middle button lit, in the bottom right corner (middle click: next food).
		if (bMouseHint)
		{
			if (UTexture2D* Mouse = MiddleMouseIcon())
			{
				DrawTexture(Mouse, SlotX + SlotW - 22.0f, Y + SlotH - 27.0f, 18.0f, 24.0f, 0.0f, 0.0f, 1.0f, 1.0f, FLinearColor::White, BLEND_Translucent);
			}
		}
	}
}

void ADCCHUD::DrawInspectedGear(ADCCCharacterBase* Crawler)
{
	if (!Crawler || !Canvas)
	{
		return;
	}

	ADCCCharacterBase* Subject = Crawler->GetInspectedCharacter();
	if (!Subject)
	{
		return;
	}

	// Left of the conversation menu, which sits on the right - the two are read
	// together, so they must not overlap.
	constexpr float PanelW = 330.0f;
	const float X = Canvas->SizeX - 380.0f - PanelW - 40.0f;
	float Y = Canvas->SizeY * 0.5f - 120.0f;

	static const TCHAR* SlotNames[] = { TEXT("Main"), TEXT("Off"), TEXT("Head"), TEXT("Body"), TEXT("Trinket") };

	UDCCEquipmentComponent* Gear = Subject->Equipment;
	UDCCInventoryComponent* Pack = Subject->Inventory;

	int32 Lines = 2 + static_cast<int32>(EDCCEquipmentSlot::MAX);
	Lines += Pack ? FMath::Max(1, Pack->Items.Num()) + 1 : 0;

	const float PanelH = Lines * LineHeight + 24.0f;

	DrawRect(FLinearColor(0.42f, 0.44f, 0.52f, 0.5f), X - 14.0f, Y - 14.0f, PanelW + 28.0f, PanelH + 12.0f);
	DrawRect(FLinearColor(0.02f, 0.03f, 0.06f, 0.90f), X - 12.0f, Y - 12.0f, PanelW + 24.0f, PanelH + 8.0f);

	const FString Name = Subject->DisplayName.IsEmpty()
		? Subject->GetActorNameOrLabel()
		: Subject->DisplayName.ToString();

	DrawText(FString::Printf(TEXT("%s - carrying"), *Name), HeadingColour, X, Y);
	Y += LineHeight + 4.0f;

	for (int32 Index = 0; Index < static_cast<int32>(EDCCEquipmentSlot::MAX); ++Index)
	{
		const EDCCEquipmentSlot Slot = static_cast<EDCCEquipmentSlot>(Index);

		FString Line = FString::Printf(TEXT("  %-8s -"), SlotNames[Index]);

		if (Gear)
		{
			const FName ItemId = Gear->GetEquippedId(Slot);

			FDCCEquipmentDef Def;
			if (!ItemId.IsNone() && Gear->FindItem(ItemId, Def))
			{
				// The die is the number that decides whether swapping is worth
				// arguing about, so it goes on the same line as the name.
				Line = Def.WeaponDamageDie > 0
					? FString::Printf(TEXT("  %-8s %s (d%d)"),
						SlotNames[Index], *Def.DisplayName.ToString(), Def.WeaponDamageDie)
					: FString::Printf(TEXT("  %-8s %s"),
						SlotNames[Index], *Def.DisplayName.ToString());
			}
		}

		DrawText(Line, BodyColour, X, Y);
		Y += LineHeight;
	}

	if (!Pack)
	{
		return;
	}

	Y += 4.0f;
	DrawText(TEXT("  Pack"), HeadingColour, X, Y);
	Y += LineHeight;

	if (Pack->Items.IsEmpty())
	{
		DrawText(TEXT("    (empty)"), DimColour, X, Y);
		return;
	}

	for (const FDCCInventoryItem& Item : Pack->Items)
	{
		DrawText(FString::Printf(TEXT("    %s (%.0f kg)"),
			*Item.DisplayName.ToString(), Item.MassKilograms), DimColour, X, Y);
		Y += LineHeight;
	}
}

void ADCCHUD::DrawInteractionMenu(ADCCCharacterBase* Crawler)
{
	if (!Crawler || !Canvas)
	{
		return;
	}

	ADCCCharacterBase* Target = Crawler->GetInteractionTarget();

	// The armed half of the mid-combat two-step. Without this the first E press
	// would appear to do nothing at all, which is worse than the problem the
	// confirmation was added to solve.
	if (!Target)
	{
		if (const ADCCCharacterBase* Pending = Crawler->GetPendingTalkTarget())
		{
			const FString Name = Pending->DisplayName.IsEmpty()
				? Pending->GetActorNameOrLabel()
				: Pending->DisplayName.ToString();

			const FString Prompt = FString::Printf(TEXT("Press E again to talk to %s"), *Name);

			float PromptW = 0.0f, PromptH = 0.0f;
			GetTextSize(Prompt, PromptW, PromptH, GEngine->GetMediumFont(), 1.0f);

			const float PromptX = (Canvas->SizeX - PromptW) * 0.5f;
			const float PromptY = Canvas->SizeY * 0.62f;

			DrawRect(FLinearColor(0.02f, 0.03f, 0.06f, 0.75f),
				PromptX - 14.0f, PromptY - 6.0f, PromptW + 28.0f, PromptH + 12.0f);

			DrawText(Prompt, FLinearColor(1.0f, 0.88f, 0.60f),
				PromptX, PromptY, GEngine->GetMediumFont(), 1.0f);
		}

		return;
	}

	const TArray<FDCCInteractionEntry>& Options = Crawler->GetInteractionOptions();
	if (Options.IsEmpty())
	{
		return;
	}

	const int32 Selected = Crawler->GetInteractionIndex();

	constexpr float PanelW = 380.0f;
	const float X = Canvas->SizeX - PanelW - 40.0f;
	float Y = Canvas->SizeY * 0.5f - (Options.Num() * LineHeight) * 0.5f;

	const float PanelH = (Options.Num() + 3) * LineHeight + 16.0f;

	DrawRect(FLinearColor(0.42f, 0.44f, 0.52f, 0.55f), X - 14.0f, Y - 34.0f, PanelW + 28.0f, PanelH + 12.0f);
	DrawRect(FLinearColor(0.02f, 0.03f, 0.06f, 0.90f), X - 12.0f, Y - 32.0f, PanelW + 24.0f, PanelH + 8.0f);

	const FString Name = Target->DisplayName.IsEmpty()
		? Target->GetActorNameOrLabel()
		: Target->DisplayName.ToString();

	DrawText(Name, HeadingColour, X, Y - 26.0f);
	Y += 4.0f;

	for (int32 Index = 0; Index < Options.Num(); ++Index)
	{
		const FDCCInteractionEntry& Entry = Options[Index];
		const bool bSelected = Index == Selected;

		if (bSelected)
		{
			DrawRect(FLinearColor(0.16f, 0.18f, 0.26f, 0.9f), X - 6.0f, Y - 1.0f, PanelW + 4.0f, LineHeight);
		}

		FLinearColor Colour = DimColour;
		if (Entry.bAvailable)
		{
			Colour = bSelected ? FLinearColor(1.0f, 0.92f, 0.70f) : BodyColour;
		}

		DrawText(FString::Printf(TEXT("%s %s"),
			bSelected ? TEXT(">") : TEXT(" "), *Entry.Label.ToString()), Colour, X, Y);

		// The reason sits on the same row as the option it blocks, so the two are
		// never read apart.
		if (!Entry.bAvailable && !Entry.UnavailableReason.IsEmpty())
		{
			DrawText(Entry.UnavailableReason.ToString(),
				FLinearColor(0.55f, 0.45f, 0.40f), X + 210.0f, Y);
		}

		Y += LineHeight;
	}

	Y += 6.0f;
	DrawText(TEXT("Wheel to choose  ·  E to confirm  ·  Esc to leave"), DimColour, X, Y);
}

void ADCCHUD::DrawNPCInteractionMenu(ADCCCharacterBase* Crawler)
{
	if (!Crawler || !Canvas)
	{
		return;
	}

	ADCCDialogueNPC* Target = Crawler->GetNPCInteractionTarget();
	if (!Target)
	{
		return;
	}

	const bool bShowingQuests = Crawler->IsNPCShowingQuests();
	const int32 Selected = Crawler->GetNPCInteractionIndex();

	TArray<FName> OfferableQuests;
	if (bShowingQuests)
	{
		Crawler->GetOfferableQuestIds(OfferableQuests);
	}

	const TArray<EDCCNPCInteractionOption>& Options = Crawler->GetNPCInteractionOptions();
	if (!bShowingQuests && Options.IsEmpty())
	{
		return;
	}

	// +1 for the itemised page's trailing "Back" row - same count CycleNPCInteractionMenu uses.
	const int32 RowCount = bShowingQuests ? FMath::Max(1, OfferableQuests.Num() + 1) : Options.Num();

	constexpr float PanelW = 380.0f;
	const float X = Canvas->SizeX - PanelW - 40.0f;
	float Y = Canvas->SizeY * 0.5f - (RowCount * LineHeight) * 0.5f;

	const float PanelH = (RowCount + 3) * LineHeight + 16.0f;

	DrawRect(FLinearColor(0.42f, 0.44f, 0.52f, 0.55f), X - 14.0f, Y - 34.0f, PanelW + 28.0f, PanelH + 12.0f);
	DrawRect(FLinearColor(0.02f, 0.03f, 0.06f, 0.90f), X - 12.0f, Y - 32.0f, PanelW + 24.0f, PanelH + 8.0f);

	const FString Name = Target->DisplayName.IsEmpty()
		? Target->GetActorNameOrLabel()
		: Target->DisplayName.ToString();

	DrawText(Name, HeadingColour, X, Y - 26.0f);
	Y += 4.0f;

	if (bShowingQuests)
	{
		const UDCCQuestComponent* QuestComp = UDCCQuestComponent::Get(Crawler);

		if (OfferableQuests.IsEmpty())
		{
			DrawText(TEXT("  (nothing to offer right now)"), DimColour, X, Y);
			Y += LineHeight;
		}

		for (int32 Index = 0; Index < OfferableQuests.Num(); ++Index)
		{
			const bool bSelected = Index == Selected;
			if (bSelected)
			{
				DrawRect(FLinearColor(0.16f, 0.18f, 0.26f, 0.9f), X - 6.0f, Y - 1.0f, PanelW + 4.0f, LineHeight);
			}

			FDCCQuestDef Def;
			const bool bFound = QuestComp && QuestComp->FindQuest(OfferableQuests[Index], Def);
			const FString Label = bFound ? Def.Title.ToString() : OfferableQuests[Index].ToString();

			DrawText(FString::Printf(TEXT("%s %s"), bSelected ? TEXT(">") : TEXT(" "), *Label),
				bSelected ? FLinearColor(1.0f, 0.92f, 0.70f) : BodyColour, X, Y);
			Y += LineHeight;
		}

		const bool bBackSelected = Selected == OfferableQuests.Num();
		if (bBackSelected)
		{
			DrawRect(FLinearColor(0.16f, 0.18f, 0.26f, 0.9f), X - 6.0f, Y - 1.0f, PanelW + 4.0f, LineHeight);
		}
		DrawText(FString::Printf(TEXT("%s Back"), bBackSelected ? TEXT(">") : TEXT(" ")),
			bBackSelected ? FLinearColor(1.0f, 0.92f, 0.70f) : BodyColour, X, Y);
		Y += LineHeight;
	}
	else
	{
		for (int32 Index = 0; Index < Options.Num(); ++Index)
		{
			const bool bSelected = Index == Selected;
			if (bSelected)
			{
				DrawRect(FLinearColor(0.16f, 0.18f, 0.26f, 0.9f), X - 6.0f, Y - 1.0f, PanelW + 4.0f, LineHeight);
			}

			const TCHAR* Label = TEXT("Leave");
			switch (Options[Index])
			{
			case EDCCNPCInteractionOption::Talk: Label = TEXT("Talk"); break;
			case EDCCNPCInteractionOption::Shop: Label = TEXT("Shop"); break;
			case EDCCNPCInteractionOption::Quests: Label = TEXT("Quests"); break;
			case EDCCNPCInteractionOption::RideLift: Label = TEXT("Ride the lift"); break;
			case EDCCNPCInteractionOption::RaceBet: Label = TEXT("Bet and start the race"); break;
			default: break;
			}

			DrawText(FString::Printf(TEXT("%s %s"), bSelected ? TEXT(">") : TEXT(" "), Label),
				bSelected ? FLinearColor(1.0f, 0.92f, 0.70f) : BodyColour, X, Y);
			Y += LineHeight;
		}
	}

	Y += 6.0f;
	DrawText(TEXT("Wheel to choose  ·  E to confirm  ·  Esc to leave"), DimColour, X, Y);
}

void ADCCHUD::DrawLootMenu(ADCCCharacterBase* Crawler)
{
	if (!Crawler || !Canvas)
	{
		return;
	}

	AActor* SourceActor = Crawler->GetLootTarget();
	IDCCLootSource* Source = Cast<IDCCLootSource>(SourceActor);
	if (!SourceActor || !Source)
	{
		return;
	}

	const bool bShowingItems = Crawler->IsShowingLootItems();
	const int32 Selected = Crawler->GetLootMenuIndex();
	const TArray<FDCCRolledLoot>& Items = Source->GetPendingLootItems();

	// +1 for the itemised page's trailing "Back" row - same count CycleLootMenu uses.
	const int32 RowCount = bShowingItems ? FMath::Max(1, Items.Num() + 1) : 4;

	constexpr float PanelW = 380.0f;
	const float X = Canvas->SizeX - PanelW - 40.0f;
	float Y = Canvas->SizeY * 0.5f - (RowCount * LineHeight) * 0.5f;

	const float PanelH = (RowCount + 3) * LineHeight + 16.0f;

	DrawRect(FLinearColor(0.42f, 0.44f, 0.52f, 0.55f), X - 14.0f, Y - 34.0f, PanelW + 28.0f, PanelH + 12.0f);
	DrawRect(FLinearColor(0.02f, 0.03f, 0.06f, 0.90f), X - 12.0f, Y - 32.0f, PanelW + 24.0f, PanelH + 8.0f);

	DrawText(Source->GetLootSourceName().ToString(), HeadingColour, X, Y - 26.0f);
	Y += 4.0f;

	if (bShowingItems)
	{
		if (Items.IsEmpty())
		{
			DrawText(TEXT("  (nothing left)"), DimColour, X, Y);
			Y += LineHeight;
		}

		for (int32 Index = 0; Index < Items.Num(); ++Index)
		{
			const bool bSelected = Index == Selected;
			if (bSelected)
			{
				DrawRect(FLinearColor(0.16f, 0.18f, 0.26f, 0.9f), X - 6.0f, Y - 1.0f, PanelW + 4.0f, LineHeight);
			}

			DrawText(FString::Printf(TEXT("%s %s"),
				bSelected ? TEXT(">") : TEXT(" "), *DescribeRolledLoot(Items[Index], Crawler)),
				bSelected ? FLinearColor(1.0f, 0.92f, 0.70f) : BodyColour, X, Y);
			Y += LineHeight;
		}

		const bool bBackSelected = Selected == Items.Num();
		if (bBackSelected)
		{
			DrawRect(FLinearColor(0.16f, 0.18f, 0.26f, 0.9f), X - 6.0f, Y - 1.0f, PanelW + 4.0f, LineHeight);
		}
		DrawText(FString::Printf(TEXT("%s Back"), bBackSelected ? TEXT(">") : TEXT(" ")),
			bBackSelected ? FLinearColor(1.0f, 0.92f, 0.70f) : BodyColour, X, Y);
		Y += LineHeight;
	}
	else
	{
		// Must stay in this order - HandleLootMenuConfirm's top-menu switch
		// indexes into this same layout by position.
		static const TCHAR* TopLabels[] = { TEXT("Check Inventory"), TEXT("Loot All"), TEXT("Stow"), TEXT("Leave") };
		const bool bHasLoot = Source->HasAnyLoot();

		for (int32 Index = 0; Index < 4; ++Index)
		{
			const bool bSelected = Index == Selected;

			// Stow (2) and Leave (3) always work; the other two need something
			// inside to be worth pressing.
			const bool bAvailable = (Index == 2 || Index == 3) || bHasLoot;

			FLinearColor Colour = DimColour;
			if (bAvailable)
			{
				Colour = bSelected ? FLinearColor(1.0f, 0.92f, 0.70f) : BodyColour;
			}

			if (bSelected)
			{
				DrawRect(FLinearColor(0.16f, 0.18f, 0.26f, 0.9f), X - 6.0f, Y - 1.0f, PanelW + 4.0f, LineHeight);
			}

			DrawText(FString::Printf(TEXT("%s %s"), bSelected ? TEXT(">") : TEXT(" "), TopLabels[Index]),
				Colour, X, Y);
			Y += LineHeight;
		}
	}

	Y += 6.0f;
	DrawText(TEXT("Wheel to choose  ·  E to confirm  ·  Esc to leave"), DimColour, X, Y);
}

void ADCCHUD::DrawReactions()
{
	UWorld* World = GetWorld();
	if (!World || !Canvas)
	{
		return;
	}
	const float Now = World->GetTimeSeconds();
	const FVector Eye = PlayerOwner && PlayerOwner->PlayerCameraManager ? PlayerOwner->PlayerCameraManager->GetCameraLocation() : FVector::ZeroVector;
	for (TActorIterator<ADCCCharacterBase> It(World); It; ++It)
	{
		const ADCCCharacterBase* Who = *It;
		const float Age = Now - Who->GetReactionStart();
		if (Who->GetReaction() == DCCReactions::None || Age < 0.0f || Age > DCCReactions::Seconds || Who->IsHidden())
		{
			continue;
		}
		UTexture2D* Icon = DCCReactions::Icon(Who->GetReaction());
		const float HalfHeight = Who->GetCapsuleComponent() ? Who->GetCapsuleComponent()->GetScaledCapsuleHalfHeight() : 90.0f;
		const FVector Head = Who->GetActorLocation() + FVector(0.0f, 0.0f, HalfHeight + 12.0f + Age * 10.0f); // just over the head, rising a little
		const FVector Screen = Project(Head);
		if (!Icon || Screen.Z <= 0.0f)
		{
			continue; // behind the camera
		}
		// Pops in big, settles, and fades out at the end; smaller with distance, never too small to read.
		const float Distance = FVector::Dist(Eye, Head);
		const float Base = FMath::Clamp(70.0f * 1100.0f / FMath::Max(Distance, 1.0f), 30.0f, 78.0f);
		const float Pop = Age < 0.12f ? FMath::Lerp(0.3f, 1.2f, Age / 0.12f) : Age < 0.24f ? FMath::Lerp(1.2f, 1.0f, (Age - 0.12f) / 0.12f) : 1.0f;
		const float Alpha = FMath::Clamp((DCCReactions::Seconds - Age) / 0.4f, 0.0f, 1.0f);
		const float Size = Base * Pop;
		DrawTexture(Icon, Screen.X - Size * 0.5f, Screen.Y - Size, Size, Size, 0.0f, 0.0f, 1.0f, 1.0f, FLinearColor(1.0f, 1.0f, 1.0f, Alpha), BLEND_Translucent);
	}
}

void ADCCHUD::DrawEmoteMenu(ADCCCharacterBase* Crawler)
{
	if (!Crawler || !Canvas || !Crawler->IsEmoteMenuOpen())
	{
		return;
	}

	// Reactions first (a bubble, sometimes a move), then the moves themselves (2026-11-16).
	const TArray<TObjectPtr<UAnimationAsset>>& Emotes = Crawler->EmoteAnimations;
	const int32 ReactionRows = DCCReactions::Count - 1;
	const int32 Selected = Crawler->GetEmoteMenuIndex();
	const int32 CameraRows = ADCCCharacterBase::EmoteCameraRows;
	const int32 RowCount = ReactionRows + CameraRows + Emotes.Num();
	// "A_DCC_HW_BoomDance" reads as "Boom Dance".
	auto MoveName = [](const UAnimationAsset* Asset) -> FString
	{
		if (!Asset)
		{
			return TEXT("(missing)");
		}
		FString Name = Asset->GetName();
		TArray<FString> Parts;
		Name.ParseIntoArray(Parts, TEXT("_"));
		Name = Parts.Num() > 0 ? Parts.Last() : Name;
		FString Spaced;
		for (int32 i = 0; i < Name.Len(); ++i)
		{
			if (i > 0 && FChar::IsUpper(Name[i]) && !FChar::IsUpper(Name[i - 1]))
			{
				Spaced.AppendChar(TEXT(' '));
			}
			Spaced.AppendChar(Name[i]);
		}
		return Spaced;
	};

	constexpr float PanelW = 340.0f;
	const float X = (Canvas->SizeX - PanelW) * 0.5f;
	float Y = Canvas->SizeY * 0.5f - (RowCount * LineHeight) * 0.5f;

	const float PanelH = (RowCount + 3) * LineHeight + 16.0f;

	DrawRect(FLinearColor(0.42f, 0.44f, 0.52f, 0.55f), X - 14.0f, Y - 34.0f, PanelW + 28.0f, PanelH + 12.0f);
	DrawRect(FLinearColor(0.02f, 0.03f, 0.06f, 0.90f), X - 12.0f, Y - 32.0f, PanelW + 24.0f, PanelH + 8.0f);

	DrawText(TEXT("Play to the crowd"), HeadingColour, X, Y - 26.0f);
	Y += 4.0f;

	for (int32 Index = 0; Index < RowCount; ++Index)
	{
		const bool bSelected = Index == Selected;
		if (bSelected)
		{
			DrawRect(FLinearColor(0.16f, 0.18f, 0.26f, 0.9f), X - 6.0f, Y - 1.0f, PanelW + 4.0f, LineHeight);
		}

		if (Index < ReactionRows)
		{
			// A reaction: its bubble, then its name.
			if (UTexture2D* Icon = DCCReactions::Icon(uint8(Index + 1)))
			{
				DrawTexture(Icon, X + 14.0f, Y - 3.0f, LineHeight + 4.0f, LineHeight + 4.0f, 0.0f, 0.0f, 1.0f, 1.0f, FLinearColor::White, BLEND_Translucent);
			}
			DrawText(FString::Printf(TEXT("%s"), bSelected ? TEXT(">") : TEXT(" ")), FLinearColor(1.0f, 0.92f, 0.70f), X, Y);
			DrawText(DCCReactions::Info(uint8(Index + 1)).Name, bSelected ? FLinearColor(1.0f, 0.92f, 0.70f) : BodyColour, X + 44.0f, Y);
		}
		else if (Index < ReactionRows + CameraRows)
		{
			// The cameras (they had their own keys, C and X, until 2026-11-16).
			const FString Label = Index == ReactionRows ? FString(TEXT("Photo pose (take a selfie)"))
				: FString::Printf(TEXT("Selfie camera: %s"), Crawler->IsFacingCamera() ? TEXT("on") : TEXT("off"));
			DrawText(FString::Printf(TEXT("%s %s"), bSelected ? TEXT(">") : TEXT(" "), *Label),
				bSelected ? FLinearColor(1.0f, 0.92f, 0.70f) : FLinearColor(0.75f, 0.9f, 1.0f), X + 44.0f, Y);
		}
		else
		{
			DrawText(FString::Printf(TEXT("%s %s"), bSelected ? TEXT(">") : TEXT(" "), *MoveName(Emotes[Index - ReactionRows - CameraRows])),
				bSelected ? FLinearColor(1.0f, 0.92f, 0.70f) : BodyColour, X + 44.0f, Y);
		}
		Y += LineHeight;
	}

	Y += 6.0f;
	DrawText(TEXT("Wheel to choose  ·  E to perform  ·  Esc / B to leave"), DimColour, X, Y);
}

void ADCCHUD::DrawOathgateMenu(ADCCCharacterBase* Crawler)
{
	if (!Crawler || !Canvas)
	{
		return;
	}

	if (!Crawler->GetOathgateTarget())
	{
		return;
	}

	const int32 Selected = Crawler->GetOathgateMenuIndex();
	static const TCHAR* Rows[] = { TEXT("Enter the gate"), TEXT("Turn away") };
	constexpr int32 RowCount = 2;

	constexpr float PanelW = 340.0f;
	const float X = (Canvas->SizeX - PanelW) * 0.5f;
	float Y = Canvas->SizeY * 0.5f - (RowCount * LineHeight) * 0.5f;

	const float PanelH = (RowCount + 3) * LineHeight + 16.0f;

	DrawRect(FLinearColor(0.42f, 0.44f, 0.52f, 0.55f), X - 14.0f, Y - 34.0f, PanelW + 28.0f, PanelH + 12.0f);
	DrawRect(FLinearColor(0.02f, 0.03f, 0.06f, 0.90f), X - 12.0f, Y - 32.0f, PanelW + 24.0f, PanelH + 8.0f);

	DrawText(TEXT("Enter the gate?"), HeadingColour, X, Y - 26.0f);
	Y += 4.0f;

	for (int32 Index = 0; Index < RowCount; ++Index)
	{
		const bool bSelected = Index == Selected;
		if (bSelected)
		{
			DrawRect(FLinearColor(0.16f, 0.18f, 0.26f, 0.9f), X - 6.0f, Y - 1.0f, PanelW + 4.0f, LineHeight);
		}

		DrawText(FString::Printf(TEXT("%s %s"), bSelected ? TEXT(">") : TEXT(" "), Rows[Index]),
			bSelected ? FLinearColor(1.0f, 0.92f, 0.70f) : BodyColour, X, Y);
		Y += LineHeight;
	}

	Y += 6.0f;
	DrawText(TEXT("Wheel to choose  ·  E to confirm  ·  Esc / walk away to turn back"), DimColour, X, Y);
}

void ADCCHUD::DrawVendorMenu(ADCCCharacterBase* Crawler)
{
	// The shop screen (UDCCShopWidget) replaced this list on 2026-11-13.
	if (IsShopOpen())
	{
		return;
	}

	if (!Crawler || !Canvas)
	{
		return;
	}

	const ADCCVendorNPC* Vendor = Cast<ADCCVendorNPC>(Crawler->GetVendorTarget());
	if (!Vendor)
	{
		return;
	}

	const int32 Gold = Crawler->Stats ? Crawler->Stats->Gold : 0;
	const int32 Selected = Crawler->GetVendorMenuIndex();

	constexpr float PanelW = 380.0f;
	const float X = Canvas->SizeX - PanelW - 40.0f;

	// The Drink Now/Stow sub-prompt replaces the whole list while it is open -
	// there is nothing useful to show behind it, and it is only ever two rows.
	if (Crawler->IsConfirmingVendorDrink())
	{
		FDCCConsumableDef Def;
		const bool bResolved = Crawler->Inventory && Crawler->Inventory->FindConsumable(Crawler->GetVendorPendingDrinkId(), Def);

		constexpr int32 RowCount = 2;
		float Y = Canvas->SizeY * 0.5f - (RowCount * LineHeight) * 0.5f;
		const float PanelH = (RowCount + 3) * LineHeight + 16.0f;

		DrawRect(FLinearColor(0.42f, 0.44f, 0.52f, 0.55f), X - 14.0f, Y - 34.0f, PanelW + 28.0f, PanelH + 12.0f);
		DrawRect(FLinearColor(0.02f, 0.03f, 0.06f, 0.90f), X - 12.0f, Y - 32.0f, PanelW + 24.0f, PanelH + 8.0f);

		DrawText(bResolved ? *Def.DisplayName.ToString() : TEXT("Drink"), HeadingColour, X, Y - 26.0f);
		Y += 4.0f;

		static const TCHAR* SubLabels[] = { TEXT("Drink it now"), TEXT("Stow it for later") };
		for (int32 Index = 0; Index < 2; ++Index)
		{
			const bool bSelected = Index == Selected;
			if (bSelected)
			{
				DrawRect(FLinearColor(0.16f, 0.18f, 0.26f, 0.9f), X - 6.0f, Y - 1.0f, PanelW + 4.0f, LineHeight);
			}
			DrawText(FString::Printf(TEXT("%s %s"), bSelected ? TEXT(">") : TEXT(" "), SubLabels[Index]),
				bSelected ? FLinearColor(1.0f, 0.92f, 0.70f) : BodyColour, X, Y);
			Y += LineHeight;
		}

		Y += 6.0f;
		DrawText(TEXT("Wheel to choose  ·  E to confirm  ·  Esc to leave"), DimColour, X, Y);
		return;
	}

	const int32 EquipCount = Vendor->SellableEquipmentIds.Num();
	const int32 DrinkCount = Vendor->SellableConsumableIds.Num();

	// +1 for the trailing Leave row - no itemised sub-page here, unlike the
	// loot menu, so the row count is just everything this vendor sells.
	const int32 RowCount = EquipCount + DrinkCount + 1;

	float Y = Canvas->SizeY * 0.5f - (RowCount * LineHeight) * 0.5f;
	const float PanelH = (RowCount + 3) * LineHeight + 16.0f;

	DrawRect(FLinearColor(0.42f, 0.44f, 0.52f, 0.55f), X - 14.0f, Y - 34.0f, PanelW + 28.0f, PanelH + 12.0f);
	DrawRect(FLinearColor(0.02f, 0.03f, 0.06f, 0.90f), X - 12.0f, Y - 32.0f, PanelW + 24.0f, PanelH + 8.0f);

	DrawText(FString::Printf(TEXT("%s (%dg)"), *Vendor->DisplayName.ToString(), Gold), HeadingColour, X, Y - 26.0f);
	Y += 4.0f;

	int32 RowIndex = 0;

	for (const FName& ItemId : Vendor->SellableEquipmentIds)
	{
		FDCCEquipmentDef Def;
		const bool bResolved = Crawler->Equipment && Crawler->Equipment->FindItem(ItemId, Def);
		const bool bAffordable = bResolved && Gold >= Def.GoldValue;
		const bool bSelected = RowIndex == Selected;

		if (bSelected)
		{
			DrawRect(FLinearColor(0.16f, 0.18f, 0.26f, 0.9f), X - 6.0f, Y - 1.0f, PanelW + 4.0f, LineHeight);
		}

		const FLinearColor Colour = !bAffordable ? DimColour
			: bSelected ? FLinearColor(1.0f, 0.92f, 0.70f) : BodyColour;

		DrawText(FString::Printf(TEXT("%s %s - %dg"), bSelected ? TEXT(">") : TEXT(" "),
			bResolved ? *Def.DisplayName.ToString() : *ItemId.ToString(),
			bResolved ? Def.GoldValue : 0),
			Colour, X, Y);
		Y += LineHeight;
		++RowIndex;
	}

	for (const FName& ItemId : Vendor->SellableConsumableIds)
	{
		FDCCConsumableDef Def;
		const bool bResolved = Crawler->Inventory && Crawler->Inventory->FindConsumable(ItemId, Def);
		const bool bAffordable = bResolved && Gold >= Def.GoldValue;
		const bool bSelected = RowIndex == Selected;

		if (bSelected)
		{
			DrawRect(FLinearColor(0.16f, 0.18f, 0.26f, 0.9f), X - 6.0f, Y - 1.0f, PanelW + 4.0f, LineHeight);
		}

		const FLinearColor Colour = !bAffordable ? DimColour
			: bSelected ? FLinearColor(1.0f, 0.92f, 0.70f) : BodyColour;

		DrawText(FString::Printf(TEXT("%s %s - %dg"), bSelected ? TEXT(">") : TEXT(" "),
			bResolved ? *Def.DisplayName.ToString() : *ItemId.ToString(),
			bResolved ? Def.GoldValue : 0),
			Colour, X, Y);
		Y += LineHeight;
		++RowIndex;
	}

	const bool bLeaveSelected = RowIndex == Selected;
	if (bLeaveSelected)
	{
		DrawRect(FLinearColor(0.16f, 0.18f, 0.26f, 0.9f), X - 6.0f, Y - 1.0f, PanelW + 4.0f, LineHeight);
	}
	DrawText(FString::Printf(TEXT("%s Leave"), bLeaveSelected ? TEXT(">") : TEXT(" ")),
		bLeaveSelected ? FLinearColor(1.0f, 0.92f, 0.70f) : BodyColour, X, Y);
	Y += LineHeight;

	Y += 6.0f;
	DrawText(TEXT("Wheel to choose  ·  E to buy  ·  Esc to leave"), DimColour, X, Y);
}

bool ADCCHUD::PassesFeedFilter(const FDCCEventLogEntry& Entry, const ADCCCharacterBase* LocalCrawler) const
{
	switch (FeedFilter)
	{
	case EDCCFeedFilter::CombatOnly:
		return Entry.Category == EDCCLogCategory::Combat || Entry.Category == EDCCLogCategory::Death
			|| Entry.Category == EDCCLogCategory::Encounter;

	case EDCCFeedFilter::SkillsOnly:
		return Entry.Category == EDCCLogCategory::Skill || Entry.Category == EDCCLogCategory::Advancement;

	case EDCCFeedFilter::Milestones:
		return Entry.Category == EDCCLogCategory::Achievement || Entry.Category == EDCCLogCategory::Experience
			|| Entry.Category == EDCCLogCategory::Favor || Entry.Category == EDCCLogCategory::Announcement;

	case EDCCFeedFilter::MyRolls:
		return LocalCrawler && (Entry.Instigator == LocalCrawler || Entry.RollTarget == LocalCrawler);

	case EDCCFeedFilter::All:
	default:
		return true;
	}
}

FString ADCCHUD::GetFeedFilterLabel() const
{
	switch (FeedFilter)
	{
	case EDCCFeedFilter::CombatOnly: return TEXT("Combat");
	case EDCCFeedFilter::SkillsOnly: return TEXT("Skills");
	case EDCCFeedFilter::Milestones: return TEXT("Milestones");
	case EDCCFeedFilter::MyRolls:    return TEXT("My Rolls");
	case EDCCFeedFilter::All:
	default:                         return TEXT("All");
	}
}

void ADCCHUD::CycleFeedFilter()
{
	FeedFilter = static_cast<EDCCFeedFilter>(
		(static_cast<uint8>(FeedFilter) + 1) % static_cast<uint8>(EDCCFeedFilter::MAX));
}

void ADCCHUD::CycleFeedVisibility()
{
	FeedVisibility = static_cast<EDCCFeedVisibility>(
		(static_cast<uint8>(FeedVisibility) + 1) % static_cast<uint8>(EDCCFeedVisibility::MAX));
}

namespace
{
	/** Nat 20 green, nat 1 red, everything else the usual age-lerp - shared by
	 *  the always-on box and the Feed tab so the two never disagree. */
	FLinearColor FeedEntryColour(const FDCCEventLogEntry& Entry, float Age, const FLinearColor& InBodyColour, const FLinearColor& InDimColour)
	{
		if (Entry.bCriticalSuccess)
		{
			return FLinearColor(0.35f, 0.95f, 0.45f);
		}
		if (Entry.bCriticalFailure)
		{
			return FLinearColor(0.95f, 0.30f, 0.30f);
		}
		return FMath::Lerp(InBodyColour, InDimColour, Age);
	}
}

float ADCCHUD::DrawFeed(float X, float Y)
{
	const ADCCGameState* GameState = GetWorld() ? GetWorld()->GetGameState<ADCCGameState>() : nullptr;
	if (!GameState)
	{
		return Y;
	}

	const ADCCCharacterBase* LocalCrawler = GetOwningCrawler();
	const TArray<FDCCEventLogEntry>& Log = GameState->GetEventLog();

	// Walk backward from newest, keeping only lines the active preset allows,
	// until the box is full or the log runs out.
	TArray<const FDCCEventLogEntry*> Shown;
	Shown.Reserve(FeedLineCount);
	for (int32 Index = Log.Num() - 1; Index >= 0 && Shown.Num() < FeedLineCount; --Index)
	{
		if (PassesFeedFilter(Log[Index], LocalCrawler))
		{
			Shown.Add(&Log[Index]);
		}
	}

	// A fixed-height panel so the box does not hop around as lines fill in, with
	// a lighter border rect behind the dark fill for the outline the flat
	// background elsewhere in this file doesn't have.
	const float PanelWidth = 460.0f;
	const float PanelHeight = (FeedLineCount + 1) * LineHeight + 6.0f;
	DrawRect(FLinearColor(0.42f, 0.44f, 0.52f, 0.5f), X - 10.0f, Y - 10.0f, PanelWidth + 20.0f, PanelHeight + 20.0f);
	DrawRect(FLinearColor(0.02f, 0.03f, 0.06f, 0.78f), X - 8.0f, Y - 8.0f, PanelWidth + 16.0f, PanelHeight + 16.0f);

	DrawText(FString::Printf(TEXT("Log: %s   [L] cycle   [K] minimize"), *GetFeedFilterLabel()), DimColour, X, Y);
	Y += LineHeight;

	// Drawn oldest-to-newest top-to-bottom, dimming toward the top, so the eye
	// lands on what just happened at the bottom of the box.
	for (int32 ShownIndex = Shown.Num() - 1; ShownIndex >= 0; --ShownIndex)
	{
		const float Age = static_cast<float>(ShownIndex) / FMath::Max(1, Shown.Num() - 1);

		Y = DrawFeedLine(*Shown[ShownIndex], X, Y,
			FeedEntryColour(*Shown[ShownIndex], Age, BodyColour, DimColour), LocalCrawler, ShownIndex);
	}

	return Y;
}

float ADCCHUD::DrawFeedLine(const FDCCEventLogEntry& Entry, float X, float Y, const FLinearColor& Colour,
	const ADCCCharacterBase* LocalCrawler, int32 HitBoxIndex)
{
	// Only the local crawler's own rolls are inspectable - Zoe's own wording,
	// "only for their own rolls" - so a click on someone else's crit does
	// nothing rather than exposing a stranger's stat sheet.
	const bool bClickable = LocalCrawler && Entry.Instigator == LocalCrawler && Entry.CheckAbility != EDCCAbility::MAX;

	if (bClickable)
	{
		AddHitBox(FVector2D(X, Y), FVector2D(440.0f, LineHeight),
			FName(*FString::Printf(TEXT("RollBreak|%d|%d|%s"), HitBoxIndex,
				static_cast<int32>(Entry.CheckAbility), *Entry.CheckSkillId.ToString())),
			/*bConsumesInput=*/true, /*Priority=*/1);

		// A small marker so a clickable roll is discoverable without being
		// told - otherwise nothing on screen hints that this line is special.
		DrawText(TEXT("*"), HeadingColour, X - 12.0f, Y);
	}

	DrawText(Entry.Text.ToString(), Colour, X, Y);

	return Y + LineHeight;
}

void ADCCHUD::DrawRollBreakdown(ADCCCharacterBase* LocalCrawler)
{
	if (BreakdownAbility == EDCCAbility::MAX || !LocalCrawler || !Canvas || !LocalCrawler->Stats)
	{
		return;
	}

	UDCCRuleset* Ruleset = LocalCrawler->Stats->GetRuleset();
	if (!Ruleset)
	{
		return;
	}

	static const TCHAR* AbilityNames[] = { TEXT("Strength"), TEXT("Intelligence"), TEXT("Constitution"), TEXT("Dexterity"), TEXT("Charisma") };
	const int32 AbilityIndex = FMath::Clamp(static_cast<int32>(BreakdownAbility), 0, UE_ARRAY_COUNT(AbilityNames) - 1);

	const TArray<FDCCModifierSource> AbilitySources = LocalCrawler->GetAbilityScoreBreakdown(BreakdownAbility);
	const TArray<FDCCModifierSource> SkillSources = LocalCrawler->GetSkillRankBreakdown(BreakdownSkillId);

	int32 ScoreTotal = 0;
	for (const FDCCModifierSource& Source : AbilitySources)
	{
		ScoreTotal += Source.Amount;
	}

	const float PanelW = 380.0f;
	const int32 ExtraLines = SkillSources.Num() > 0 ? SkillSources.Num() + 2 : 0;
	const float PanelH = (AbilitySources.Num() + ExtraLines + 3) * LineHeight + 20.0f;
	const float PanelX = Canvas->SizeX - PanelW - 40.0f;
	const float PanelY = 120.0f;

	DrawRect(FLinearColor(0.42f, 0.44f, 0.52f, 0.5f), PanelX - 10.0f, PanelY - 10.0f, PanelW + 20.0f, PanelH + 20.0f);
	DrawRect(FLinearColor(0.02f, 0.03f, 0.06f, 0.92f), PanelX - 8.0f, PanelY - 8.0f, PanelW + 16.0f, PanelH + 16.0f);

	float Y = PanelY;
	DrawText(FString::Printf(TEXT("%s  -  click the roll again to close"), AbilityNames[AbilityIndex]), HeadingColour, PanelX, Y);
	Y += LineHeight + 4.0f;

	for (const FDCCModifierSource& Source : AbilitySources)
	{
		DrawText(FString::Printf(TEXT("  %+d   %s"), Source.Amount, *Source.Label.ToString()), BodyColour, PanelX, Y);
		Y += LineHeight;
	}

	const int32 Modifier = Ruleset->GetAbilityModifier(ScoreTotal);
	DrawText(FString::Printf(TEXT("  = Score %d  ->  %+d to the roll"), ScoreTotal, Modifier), HeadingColour, PanelX, Y);
	Y += LineHeight;

	if (!SkillSources.IsEmpty())
	{
		Y += 4.0f;
		DrawText(FString::Printf(TEXT("Skill: %s"), *BreakdownSkillId.ToString()), HeadingColour, PanelX, Y);
		Y += LineHeight;

		for (const FDCCModifierSource& Source : SkillSources)
		{
			DrawText(FString::Printf(TEXT("  %+d   %s"), Source.Amount, *Source.Label.ToString()), BodyColour, PanelX, Y);
			Y += LineHeight;
		}
	}
}

float ADCCHUD::DrawFeedTab(float X, float Y)
{
	DrawText(TEXT("Feed  -  [L] cycle filter"), HeadingColour, X, Y);
	Y += LineHeight;

	if (FeedVisibility != EDCCFeedVisibility::Pinned)
	{
		AddHitBox(FVector2D(X, Y), FVector2D(200.0f, LineHeight), TEXT("FeedRePin"),
			/*bConsumesInput=*/true, /*Priority=*/1);
		DrawText(TEXT("  [click to re-pin the on-screen log]"), HeadingColour, X, Y);
		Y += LineHeight + 4.0f;
	}

	const ADCCGameState* GameState = GetWorld() ? GetWorld()->GetGameState<ADCCGameState>() : nullptr;
	if (!GameState)
	{
		return Y;
	}

	const ADCCCharacterBase* LocalCrawler = GetOwningCrawler();
	const TArray<FDCCEventLogEntry>& Log = GameState->GetEventLog();

	constexpr int32 TabFeedLineCount = 20;
	TArray<const FDCCEventLogEntry*> Shown;
	Shown.Reserve(TabFeedLineCount);
	for (int32 Index = Log.Num() - 1; Index >= 0 && Shown.Num() < TabFeedLineCount; --Index)
	{
		if (PassesFeedFilter(Log[Index], LocalCrawler))
		{
			Shown.Add(&Log[Index]);
		}
	}

	if (Shown.IsEmpty())
	{
		DrawText(TEXT("  (nothing yet)"), DimColour, X, Y);
		return Y + LineHeight;
	}

	for (int32 ShownIndex = Shown.Num() - 1; ShownIndex >= 0; --ShownIndex)
	{
		const float Age = static_cast<float>(ShownIndex) / FMath::Max(1, Shown.Num() - 1);
		// +10000 keeps this tab's hit box names from colliding with the
		// always-on box's, which can both be on screen in the same frame.
		Y = DrawFeedLine(*Shown[ShownIndex], X, Y,
			FeedEntryColour(*Shown[ShownIndex], Age, BodyColour, DimColour), LocalCrawler, 10000 + ShownIndex);
	}

	return Y;
}

float ADCCHUD::DrawChannelMessages(float X, float Y, EDCCChatChannel Channel, ADCCCharacterBase* Crawler,
	int32 LineCount, const FString& TargetFilter)
{
	// Same character-count approximation DrawAchievementDetail uses for its
	// own 400-wide card (44 chars), scaled to this panel's ~444 usable width
	// (the 460-wide tab panel, minus its X=32 left margin against a 16-unit
	// panel edge).
	constexpr int32 ChatWrapCharacters = 46;

	const ADCCGameState* GameState = GetWorld() ? GetWorld()->GetGameState<ADCCGameState>() : nullptr;
	if (!GameState)
	{
		return Y;
	}

	const FString LocalName = Crawler
		? (Crawler->DisplayName.IsEmpty() ? Crawler->GetActorNameOrLabel() : Crawler->DisplayName.ToString())
		: FString();

	const TArray<FDCCChatMessage>& AllMessages = GameState->GetChatMessages();

	TArray<const FDCCChatMessage*> Shown;
	Shown.Reserve(LineCount);
	for (int32 Index = AllMessages.Num() - 1; Index >= 0 && Shown.Num() < LineCount; --Index)
	{
		const FDCCChatMessage& Message = AllMessages[Index];
		if (Message.Channel != Channel)
		{
			continue;
		}

		// Whispers only ever show a conversation with one specific other
		// person - everyone else's whispers are filtered out here, which is
		// the entire privacy model (see AddChatMessage's own comment).
		if (Channel == EDCCChatChannel::Whispers)
		{
			if (TargetFilter.IsEmpty())
			{
				continue;
			}
			const bool bImSender = Message.SenderName == LocalName && Message.WhisperRecipientName == TargetFilter;
			const bool bImRecipient = Message.WhisperRecipientName == LocalName && Message.SenderName == TargetFilter;
			if (!bImSender && !bImRecipient)
			{
				continue;
			}
		}

		Shown.Add(&Message);
	}

	if (Shown.IsEmpty())
	{
		DrawText(TEXT("  (nothing yet)"), DimColour, X, Y);
		return Y + LineHeight;
	}

	FString LastHeaderedSender;
	for (int32 ShownIndex = Shown.Num() - 1; ShownIndex >= 0; --ShownIndex)
	{
		const FDCCChatMessage& Message = *Shown[ShownIndex];
		const bool bSenderDead = Message.SenderCharacter && Message.SenderCharacter->IsDead();

		// One header per run of consecutive messages from the same dead
		// sender, not one per line - it's a status marker, not noise.
		if (bSenderDead && LastHeaderedSender != Message.SenderName)
		{
			DrawText(TEXT("  -- message sent/received from a dead crawler --"),
				FLinearColor(0.6f, 0.35f, 0.35f), X, Y);
			Y += LineHeight;
			LastHeaderedSender = Message.SenderName;
		}
		else if (!bSenderDead)
		{
			LastHeaderedSender.Empty();
		}

		// Word-by-word, not one unbroken DrawText call - the panel is 460
		// units wide and an unwrapped "Sender: message" ran straight off its
		// right edge the instant either got long (same problem
		// DrawAchievementDetail's own WrapText exists to avoid), and this is
		// also what lets ":BORANT_W:"-style Emote-bucket tokens render as
		// real icons instead of literal bracketed text.
		const FString Line = FString::Printf(TEXT("  %s: %s"), *Message.SenderName, *Message.Text.ToString());
		Y = DrawChatLineWithEmotes(X, Y, X + ChatWrapCharacters * 9.1f, Line,
			Message.bModerated ? DimColour : BodyColour);
	}

	return Y;
}

float ADCCHUD::DrawChatLineWithEmotes(float X, float Y, float MaxX, const FString& Line, const FLinearColor& Colour)
{
	constexpr float IconSize = 20.0f;
	constexpr float WordGap = 6.0f;

	TArray<FString> Words;
	Line.ParseIntoArray(Words, TEXT(" "), /*CullEmpty=*/true);

	float CursorX = X;
	for (const FString& Word : Words)
	{
		UTexture2D* Icon = DCCChatter::FindEmoteIcon(Word);

		float AdvanceWidth;
		if (Icon)
		{
			AdvanceWidth = IconSize;
		}
		else
		{
			float TextW = 0.0f, TextH = 0.0f;
			GetTextSize(Word, TextW, TextH, GEngine->GetMediumFont(), 1.0f);
			AdvanceWidth = TextW;
		}

		// Wrap before drawing if this word would overrun the panel - but
		// never on the very first word of a line, or a single long word
		// would wrap forever against itself.
		if (CursorX > X && CursorX + AdvanceWidth > MaxX)
		{
			CursorX = X;
			Y += LineHeight;
		}

		if (Icon)
		{
			DrawTexture(Icon, CursorX, Y, IconSize, IconSize, 0.0f, 0.0f, 1.0f, 1.0f, FLinearColor::White);
		}
		else
		{
			DrawText(Word, Colour, CursorX, Y);
		}

		CursorX += AdvanceWidth + WordGap;
	}

	return Y + LineHeight;
}

float ADCCHUD::DrawChatSendRow(float X, float Y, EDCCChatChannel Channel, ADCCCharacterBase* Crawler,
	const FString& HitBoxSuffix)
{
	const bool bCanSend = Crawler && Crawler->CanSendChatMessage();

	AddHitBox(FVector2D(X, Y), FVector2D(300.0f, LineHeight),
		FName(*FString::Printf(TEXT("ChatOpen|%s"), *HitBoxSuffix)), /*bConsumesInput=*/bCanSend, /*Priority=*/1);

	if (!bCanSend)
	{
		DrawText(TEXT("  (that final message has already gone out)"), DimColour, X, Y);
	}
	else
	{
		DrawText(TEXT("  [click to type a message]"), DimColour, X, Y);
	}

	return Y + LineHeight;
}

float ADCCHUD::DrawChatterTab(float X, float Y)
{
	DrawText(TEXT("Chatter  -  the audience at home"), HeadingColour, X, Y);
	Y += LineHeight;

	return DrawChannelMessages(X, Y, EDCCChatChannel::Chatter, nullptr, 20);
}

float ADCCHUD::DrawAlliesTab(float X, float Y, ADCCCharacterBase* Crawler)
{
	DrawText(TEXT("Allies  -  everyone you've fist-bumped, still standing"), HeadingColour, X, Y);
	Y += LineHeight;

	UWorld* World = GetWorld();
	int32 Count = 0;
	if (World && Crawler)
	{
		for (TActorIterator<ADCCCharacterBase> It(World); It; ++It)
		{
			ADCCCharacterBase* Other = *It;
			if (!Other || Other == Crawler || !Other->IsBefriended() || !Other->IsAlive())
			{
				continue;
			}

			const FString Name = Other->DisplayName.IsEmpty()
				? Other->GetActorNameOrLabel()
				: Other->DisplayName.ToString();

			DrawText(FString::Printf(TEXT("  %s"), *Name), BodyColour, X, Y);
			Y += LineHeight;
			++Count;
		}
	}

	if (Count == 0)
	{
		DrawText(TEXT("  Nobody yet - go bump some fists."), DimColour, X, Y);
		Y += LineHeight;
	}

	Y += SectionGap;
	Y = DrawChannelMessages(X, Y, EDCCChatChannel::Allies, Crawler, 6);
	Y = DrawChatSendRow(X, Y, EDCCChatChannel::Allies, Crawler, TEXT("Allies"));

	return Y;
}

float ADCCHUD::DrawWhispersTab(float X, float Y, ADCCCharacterBase* Crawler)
{
	DrawText(TEXT("Whispers  -  private, one name at a time"), HeadingColour, X, Y);
	Y += LineHeight;

	AddHitBox(FVector2D(X, Y), FVector2D(300.0f, LineHeight), TEXT("ChatOpen|WhisperTarget"),
		/*bConsumesInput=*/true, /*Priority=*/1);
	DrawText(FString::Printf(TEXT("  [click to set name]  Talking to: %s"),
		WhisperTargetName.IsEmpty() ? TEXT("(nobody yet)") : *WhisperTargetName),
		HeadingColour, X, Y);
	Y += LineHeight + SectionGap;

	if (WhisperTargetName.IsEmpty())
	{
		DrawText(TEXT("  Set a name above to open a conversation."), DimColour, X, Y);
		return Y + LineHeight;
	}

	Y = DrawChannelMessages(X, Y, EDCCChatChannel::Whispers, Crawler, 8, WhisperTargetName);
	Y = DrawChatSendRow(X, Y, EDCCChatChannel::Whispers, Crawler, TEXT("WhisperMsg"));

	return Y;
}

void ADCCHUD::OpenChatInputForChannel(EDCCChatChannel Channel, EDCCChatInputMode Mode, float X, float Y)
{
	if (!PlayerOwner)
	{
		return;
	}

	if (!ChatInputWidget)
	{
		ChatInputWidget = CreateWidget<UDCCChatInputWidget>(PlayerOwner, UDCCChatInputWidget::StaticClass());
		if (ChatInputWidget)
		{
			ChatInputWidget->OnSubmitted.AddDynamic(this, &ADCCHUD::HandleChatInputSubmitted);
			ChatInputWidget->OnCancelled.AddDynamic(this, &ADCCHUD::HandleChatInputCancelled);
		}
	}

	if (!ChatInputWidget)
	{
		return;
	}

	ActiveChatChannel = Channel;
	ChatInputMode = Mode;

	const FText Hint = Mode == EDCCChatInputMode::WhisperTarget
		? NSLOCTEXT("DCC", "WhisperTargetHint", "Type a crawler's name...")
		: NSLOCTEXT("DCC", "ChatMessageHint", "Type a message...");
	ChatInputWidget->SetHintText(Hint);

	if (!ChatInputWidget->IsInViewport())
	{
		ChatInputWidget->AddToViewport(100);
	}

	ChatInputWidget->SetPositionInViewport(FVector2D(X, Y), false);
	ChatInputWidget->SetDesiredSizeInViewport(FVector2D(300.0f, LineHeight + 8.0f));
	ChatInputWidget->FocusInput();
}

void ADCCHUD::CloseChatInput()
{
	if (ChatInputWidget && ChatInputWidget->IsInViewport())
	{
		ChatInputWidget->RemoveFromParent();
	}
	ChatInputMode = EDCCChatInputMode::None;
}

void ADCCHUD::HandleChatInputSubmitted(const FString& SubmittedText)
{
	ADCCCharacterBase* Crawler = GetOwningCrawler();

	if (ChatInputMode == EDCCChatInputMode::WhisperTarget)
	{
		WhisperTargetName = SubmittedText;
		CloseChatInput();

		// Straight into typing the actual message - a name on its own isn't
		// useful, so there's no reason to make that two separate clicks.
		OpenChatInputForChannel(EDCCChatChannel::Whispers, EDCCChatInputMode::Message, 32.0f,
			CachedViewportHeight - 90.0f);
		return;
	}

	if (Crawler)
	{
		const FString WhisperTarget = ActiveChatChannel == EDCCChatChannel::Whispers ? WhisperTargetName : FString();
		Crawler->Server_SendChatMessage(ActiveChatChannel, WhisperTarget, FText::FromString(SubmittedText));
	}

	CloseChatInput();
}

void ADCCHUD::HandleChatInputCancelled()
{
	CloseChatInput();
}

// ---------------------------------------------------------------------------
// Onboarding: HUD gating and the System window (2026-11-05)
// ---------------------------------------------------------------------------

bool ADCCHUD::IsHudFeatureUnlocked(EDCCHudFeature Feature) const
{
	const ADCCCharacterBase* Crawler = GetOwningCrawler();
	const UDCCOnboardingComponent* Onboarding = Crawler ? Crawler->Onboarding.Get() : nullptr;

	// No pawn (spectating, a rover) or no component: nothing is withheld.
	return !Onboarding || Onboarding->IsFeatureUnlocked(Feature);
}

bool ADCCHUD::IsMenuTabUnlocked(EDCCMenuTab Tab) const
{
	switch (Tab)
	{
	case EDCCMenuTab::Character:    return IsHudFeatureUnlocked(EDCCHudFeature::MenuCharacter);
	case EDCCMenuTab::Skills:       return IsHudFeatureUnlocked(EDCCHudFeature::MenuSkills);
	case EDCCMenuTab::Inventory:    return IsHudFeatureUnlocked(EDCCHudFeature::MenuInventory);
	case EDCCMenuTab::Feed:         return IsHudFeatureUnlocked(EDCCHudFeature::EventFeed);
	case EDCCMenuTab::Settings:     return true; // your own keys are always yours to change
	default:                        return IsHudFeatureUnlocked(EDCCHudFeature::MenuSocial);
	}
}

namespace
{
	FLinearColor SystemAccent(EDCCSystemWindowStyle Style)
	{
		switch (Style)
		{
		case EDCCSystemWindowStyle::Reward:  return FLinearColor(1.0f, 0.78f, 0.22f);
		case EDCCSystemWindowStyle::Warning: return FLinearColor(1.0f, 0.28f, 0.25f);
		default:                             return FLinearColor(0.30f, 0.86f, 1.0f);
		}
	}
}

void ADCCHUD::DrawSystemWindow(ADCCCharacterBase* Crawler)
{
	UDCCOnboardingComponent* Onboarding = Crawler ? Crawler->Onboarding.Get() : nullptr;
	const FDCCSystemWindowPayload* Window = Onboarding ? Onboarding->GetActiveWindow() : nullptr;
	if (!Window || !Canvas || !GEngine)
	{
		return;
	}

	// The look is the bright, glassy "status window" of the isekai shows the
	// party already knows: a dark pane, a hard glowing frame, everything in
	// caps, and it unrolls instead of fading. Built from rects and text on
	// Canvas for the same reason the rest of this HUD is (see the header).
	const float Age = Onboarding->GetActiveWindowAge();
	const float Unroll = FMath::InterpEaseOut(0.0f, 1.0f, FMath::Clamp(Age / 0.28f, 0.0f, 1.0f), 3.0f);
	const float TextAlpha = FMath::Clamp((Age - 0.2f) / 0.25f, 0.0f, 1.0f);
	const FLinearColor Accent = SystemAccent(Window->Style);

	UFont* TitleFont = GEngine->GetLargeFont();
	UFont* BodyFont = GEngine->GetMediumFont();

	const float PanelW = FMath::Min(780.0f, Canvas->SizeX - 80.0f);
	const float Pad = 34.0f;

	// Body text, wrapped by measured width. '|' forces a paragraph break.
	TArray<FString> Lines;
	{
		TArray<FString> Paragraphs;
		Window->Body.ToString().ParseIntoArray(Paragraphs, TEXT("|"), /*CullEmpty=*/false);
		for (int32 P = 0; P < Paragraphs.Num(); ++P)
		{
			TArray<FString> Words;
			Paragraphs[P].TrimStartAndEnd().ParseIntoArray(Words, TEXT(" "), true);
			FString Current;
			for (const FString& Word : Words)
			{
				const FString Candidate = Current.IsEmpty() ? Word : Current + TEXT(" ") + Word;
				float W = 0.0f, H = 0.0f;
				GetTextSize(Candidate, W, H, BodyFont, 1.0f);
				if (W > PanelW - Pad * 2.0f && !Current.IsEmpty())
				{
					Lines.Add(Current);
					Current = Word;
				}
				else
				{
					Current = Candidate;
				}
			}
			Lines.Add(Current);
			if (P < Paragraphs.Num() - 1)
			{
				Lines.Add(FString());
			}
		}
	}

	const float BodyLineH = 24.0f;
	const float FullH = Pad + 22.0f + 46.0f + 18.0f + Lines.Num() * BodyLineH + 22.0f + 30.0f + Pad * 0.5f;
	const float PanelH = FMath::Max(6.0f, FullH * Unroll);
	const float X = (Canvas->SizeX - PanelW) * 0.5f;
	const float CentreY = Canvas->SizeY * 0.42f;
	const float Y = CentreY - PanelH * 0.5f;

	// Dim the world a little: the window is the only thing that matters right now.
	DrawRect(FLinearColor(0.0f, 0.0f, 0.02f, 0.35f * Unroll), 0.0f, 0.0f, Canvas->SizeX, Canvas->SizeY);

	// Outer glow: a few widening, fading frames.
	for (int32 Ring = 4; Ring >= 1; --Ring)
	{
		const float Grow = Ring * 3.0f;
		FLinearColor Glow = Accent;
		Glow.A = 0.10f * Unroll;
		DrawRect(Glow, X - Grow, Y - Grow, PanelW + Grow * 2.0f, PanelH + Grow * 2.0f);
	}

	DrawRect(FLinearColor(0.01f, 0.03f, 0.09f, 0.94f), X, Y, PanelW, PanelH);

	// Scanlines, faint.
	for (float LineY = Y + 3.0f; LineY < Y + PanelH; LineY += 5.0f)
	{
		DrawRect(FLinearColor(Accent.R, Accent.G, Accent.B, 0.035f), X, LineY, PanelW, 1.0f);
	}

	// Hard frame plus corner brackets.
	const float T = 2.0f;
	DrawRect(Accent, X, Y, PanelW, T);
	DrawRect(Accent, X, Y + PanelH - T, PanelW, T);
	DrawRect(Accent, X, Y, T, PanelH);
	DrawRect(Accent, X + PanelW - T, Y, T, PanelH);
	const float Bracket = 22.0f;
	const float BT = 5.0f;
	if (PanelH > Bracket * 2.0f)
	{
		DrawRect(Accent, X - 4.0f, Y - 4.0f, Bracket, BT);
		DrawRect(Accent, X - 4.0f, Y - 4.0f, BT, Bracket);
		DrawRect(Accent, X + PanelW + 4.0f - Bracket, Y - 4.0f, Bracket, BT);
		DrawRect(Accent, X + PanelW + 4.0f - BT, Y - 4.0f, BT, Bracket);
		DrawRect(Accent, X - 4.0f, Y + PanelH + 4.0f - BT, Bracket, BT);
		DrawRect(Accent, X - 4.0f, Y + PanelH + 4.0f - Bracket, BT, Bracket);
		DrawRect(Accent, X + PanelW + 4.0f - Bracket, Y + PanelH + 4.0f - BT, Bracket, BT);
		DrawRect(Accent, X + PanelW + 4.0f - BT, Y + PanelH + 4.0f - Bracket, BT, Bracket);
	}

	if (TextAlpha <= 0.0f)
	{
		return;
	}

	float CursorY = Y + Pad * 0.6f;

	// Header: [ NOTIFICATION ]
	{
		const FString Header = FString::Printf(TEXT("[ %s ]"),
			*(Window->Header.IsEmpty() ? FString(TEXT("NOTIFICATION")) : Window->Header.ToString().ToUpper()));
		float W = 0.0f, H = 0.0f;
		GetTextSize(Header, W, H, BodyFont, 1.0f);
		FLinearColor C = Accent;
		C.A = TextAlpha;
		DrawText(Header, C, X + (PanelW - W) * 0.5f, CursorY, BodyFont, 1.0f);
		CursorY += 30.0f;
	}

	// Title, large and centred.
	{
		const FString Title = Window->Title.ToString().ToUpper();
		const float Scale = 1.5f;
		float W = 0.0f, H = 0.0f;
		GetTextSize(Title, W, H, TitleFont, Scale);
		DrawText(Title, FLinearColor(1.0f, 1.0f, 1.0f, TextAlpha), X + (PanelW - W) * 0.5f, CursorY, TitleFont, Scale);
		CursorY += 46.0f;
	}

	// Divider.
	DrawRect(FLinearColor(Accent.R, Accent.G, Accent.B, 0.55f * TextAlpha), X + Pad, CursorY, PanelW - Pad * 2.0f, 1.0f);
	CursorY += 16.0f;

	for (const FString& Line : Lines)
	{
		DrawText(Line, FLinearColor(0.88f, 0.94f, 1.0f, TextAlpha), X + Pad, CursorY, BodyFont, 1.0f);
		CursorY += BodyLineH;
	}

	// Footer: the prompt pulses so it reads as live, plus how many are waiting.
	const float Pulse = 0.55f + 0.45f * FMath::Sin(GetWorld()->GetRealTimeSeconds() * 4.0f);
	FString Footer = TEXT("[E]  ACKNOWLEDGE");
	if (Onboarding->GetQueuedWindowCount() > 1)
	{
		Footer = FString::Printf(TEXT("%d MORE  -  %s"), Onboarding->GetQueuedWindowCount() - 1, *Footer);
	}
	float FW = 0.0f, FH = 0.0f;
	GetTextSize(Footer, FW, FH, BodyFont, 1.0f);
	FLinearColor FooterColour = Accent;
	FooterColour.A = TextAlpha * Pulse;
	DrawText(Footer, FooterColour, X + PanelW - Pad - FW, Y + PanelH - Pad * 0.5f - FH - 6.0f, BodyFont, 1.0f);
}
// ---------------------------------------------------------------------------
// Party menu (2026-11-12)
// ---------------------------------------------------------------------------

void ADCCHUD::BeginPlay()
{
	Super::BeginPlay();

	DCCSession::EnsureFailureHandlers();

	// The pose bench as its own program: open the bench as soon as there is a
	// body on the stage, and keep it open (closing it quits the program).
	bBenchTool = DCCArcade::IsBenchTool(GetWorld());
	if (bBenchTool)
	{
		GetWorldTimerManager().SetTimer(BenchToolTimer, FTimerDelegate::CreateWeakLambda(this, [this]()
		{
			if (IsGearBenchOpen())
			{
				GetWorldTimerManager().ClearTimer(BenchToolTimer);
			}
			else if (GetOwningCrawler())
			{
				ToggleGearBench();
			}
		}), 0.25f, true);
		return;
	}

	// A launched game starts at the front door (character select), then the Den.
	if (DCCArcade::IsFrontDoor(GetWorld()))
	{
		FTimerHandle Handle;
		GetWorldTimerManager().SetTimer(Handle, FTimerDelegate::CreateWeakLambda(this, [this]()
		{
			OpenCharacterSelect();
		}), 0.1f, false);
		return;
	}

	// PLAY travels with seamless travel. That swaps in a new controller and
	// destroys this HUD's predecessor only after the local player has already
	// moved, so the old EndPlay's SetInputMode never reaches the viewport.
	// The front door's menu leaves that viewport on SetIgnoreInput(true)
	// (UI-only). The Den then draws and ticks, and every key is dropped.
	if (PlayerOwner)
	{
		PlayerOwner->SetInputMode(FInputModeGameOnly());
		PlayerOwner->bShowMouseCursor = false;
	}
	if (UGameViewportClient* Viewport = GetWorld() ? GetWorld()->GetGameViewport() : nullptr)
	{
		Viewport->SetIgnoreInput(false);
	}

	DCCArcade::MaybeStartInLobby(GetWorld());

	// The party menu opens on its own only when there is something to read in
	// it: why a join failed, or (once per hosting) the invite for a host who just
	// clicked HOST A PARTY and has nobody yet. It used to open on every level
	// load of a launched game, which meant at the start of every minigame; the
	// main menu (front door) now does the "what do you want to do" job.
	const UWorld* World = GetWorld();
	static bool bShownInviteForThisHosting = false;
	const bool bHosting = World && World->GetNetMode() == NM_ListenServer;
	const bool bLonelyHost = bHosting && !bShownInviteForThisHosting && World->GetGameState()
		&& World->GetGameState()->PlayerArray.Num() <= 1;
	if (!bHosting)
	{
		bShownInviteForThisHosting = false; // the next HOST A PARTY shows it again
	}
	if (bLonelyHost)
	{
		bShownInviteForThisHosting = true;
	}
	if (bLonelyHost || DCCSession::HasPendingError())
	{
		FTimerHandle Handle;
		GetWorldTimerManager().SetTimer(Handle, FTimerDelegate::CreateWeakLambda(this, [this]()
		{
			if (!IsSessionMenuOpen())
			{
				ToggleSessionMenu();
			}
		}), 0.75f, false);
	}
}

bool ADCCHUD::IsSessionMenuOpen() const
{
	return SessionMenu && SessionMenu->IsInViewport();
}

void ADCCHUD::ToggleSessionMenu()
{
	if (!PlayerOwner)
	{
		return;
	}

	if (IsSessionMenuOpen())
	{
		SessionMenu->RemoveFromParent();
		ApplyCursorForOpenPanels();
		return;
	}

	if (!SessionMenu)
	{
		SessionMenu = CreateWidget<UDCCSessionMenuWidget>(PlayerOwner, UDCCSessionMenuWidget::StaticClass());
		if (!SessionMenu)
		{
			return;
		}
		TWeakObjectPtr<ADCCHUD> WeakHud(this);
		SessionMenu->OnCloseRequested = [WeakHud]()
		{
			if (WeakHud.IsValid() && WeakHud->IsSessionMenuOpen())
			{
				WeakHud->ToggleSessionMenu();
			}
		};
	}

	SessionMenu->AddToViewport(50);
	SessionMenu->Refresh();
	ApplyCursorForOpenPanels();
}

bool ADCCHUD::IsCosmeticShopOpen() const
{
	return CosmeticShop && CosmeticShop->IsInViewport();
}

void ADCCHUD::OpenCosmeticShop(int32 Tab)
{
	if (!PlayerOwner || IsCosmeticShopOpen())
	{
		return;
	}
	if (!CosmeticShop)
	{
		CosmeticShop = CreateWidget<UDCCCosmeticShopWidget>(PlayerOwner, UDCCCosmeticShopWidget::StaticClass());
		if (!CosmeticShop)
		{
			return;
		}
		TWeakObjectPtr<ADCCHUD> WeakHud(this);
		CosmeticShop->OnCloseRequested = [WeakHud]()
		{
			if (WeakHud.IsValid())
			{
				WeakHud->CloseCosmeticShop();
			}
		};
	}
	CosmeticShop->Tab = FMath::Clamp(Tab, 0, 5); // 5 is BOUNTIES
	CosmeticShop->AddToViewport(62);
	ApplyCursorForOpenPanels();
}

void ADCCHUD::CloseCosmeticShop()
{
	if (IsCosmeticShopOpen())
	{
		CosmeticShop->RemoveFromParent();
		ApplyCursorForOpenPanels();
	}
}

void ADCCHUD::UpdateRoundOver()
{
	const ADCCArcadeArena* Arena = nullptr;
	if (UWorld* World = GetWorld())
	{
		for (TActorIterator<ADCCArcadeArena> It(World); It; ++It)
		{
			Arena = *It;
			break;
		}
	}
	const bool bWant = Arena && Arena->bRoundOver && PlayerOwner;
	const bool bShowing = RoundOver && RoundOver->IsInViewport();
	if (bWant && !bShowing)
	{
		if (!RoundOver)
		{
			RoundOver = CreateWidget<UDCCRoundOverWidget>(PlayerOwner, UDCCRoundOverWidget::StaticClass());
		}
		if (RoundOver)
		{
			RoundOver->Arena = const_cast<ADCCArcadeArena*>(Arena);
			RoundOver->AddToViewport(55);
			ApplyCursorForOpenPanels();
		}
	}
	else if (!bWant && bShowing)
	{
		RoundOver->RemoveFromParent();
		ApplyCursorForOpenPanels();
	}

	// Dead Air's early ride up: shown while everyone standing is in the cage, to those standing in it.
	const ADCCArcadeHaunt* Haunt = Cast<ADCCArcadeHaunt>(Arena);
	const APawn* Me = PlayerOwner ? PlayerOwner->GetPawn() : nullptr;
	const bool bCage = Haunt && PlayerOwner && Haunt->bAllInCage && Haunt->ReelPhase == 0 && !Haunt->bRoundOver
		&& Me && Haunt->IsInCage(Me->GetActorLocation());
	const bool bCageShowing = CageVote && CageVote->IsInViewport();
	if (bCage && !bCageShowing)
	{
		if (!CageVote)
		{
			CageVote = CreateWidget<UDCCCageVoteWidget>(PlayerOwner, UDCCCageVoteWidget::StaticClass());
		}
		if (CageVote)
		{
			CageVote->Haunt = const_cast<ADCCArcadeHaunt*>(Haunt);
			CageVote->bVoted = false;
			CageVote->AddToViewport(54);
			ApplyCursorForOpenPanels();
		}
	}
	else if (!bCage && bCageShowing)
	{
		CageVote->RemoveFromParent();
		ApplyCursorForOpenPanels();
	}
}

void ADCCHUD::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	// Like the select screen, these belong to this level, not the next one.
	if (CosmeticShop)
	{
		CosmeticShop->RemoveFromParent();
	}
	if (RoundOver)
	{
		RoundOver->RemoveFromParent();
	}
	if (CageVote)
	{
		CageVote->RemoveFromParent();
	}
	if (StarterKit)
	{
		StarterKit->RemoveFromParent();
	}

	// The select screen must not follow into the Den. SetInputMode here only
	// works when this controller still owns the local player. Seamless travel
	// moves the player first and then destroys us, so the call no-ops and the
	// viewport stays on "ignore input" until the new HUD's BeginPlay.
	if (CharacterSelect)
	{
		CharacterSelect->RemoveFromParent();
		CharacterSelect = nullptr;
		if (PlayerOwner && PlayerOwner->Player)
		{
			PlayerOwner->SetInputMode(FInputModeGameOnly());
			PlayerOwner->bShowMouseCursor = false;
		}
	}
	Super::EndPlay(EndPlayReason);
}

void ADCCHUD::OpenCharacterSelect()
{
	if (!PlayerOwner || CharacterSelect)
	{
		return;
	}
	CharacterSelect = CreateWidget<UDCCCharacterSelectWidget>(PlayerOwner, UDCCCharacterSelectWidget::StaticClass());
	if (!CharacterSelect)
	{
		return;
	}
	CharacterSelect->AddToViewport(60);
	CharacterSelect->Refresh();
	ApplyCursorForOpenPanels();
}

void ADCCHUD::ApplyCursorForOpenPanels()
{
	if (!PlayerOwner)
	{
		return;
	}

	// The shop is modal: clicks buy things.
	if (IsCosmeticShopOpen())
	{
		PlayerOwner->bShowMouseCursor = true;
		PlayerOwner->bEnableClickEvents = true;
		FInputModeUIOnly InputMode;
		InputMode.SetWidgetToFocus(CosmeticShop->TakeWidget());
		InputMode.SetLockMouseToViewportBehavior(EMouseLockMode::DoNotLock);
		PlayerOwner->SetInputMode(InputMode);
		if (CharacterSelect)
		{
			CharacterSelect->SetVisibility(ESlateVisibility::Collapsed);
		}
		return;
	}

	// The main menu owns everything until it travels to the Den, except the
	// pose bench opened from it, which hides the menu while it is up.
	if (CharacterSelect)
	{
		CharacterSelect->SetVisibility(IsGearBenchOpen() ? ESlateVisibility::Collapsed : ESlateVisibility::Visible);
	}
	if (CharacterSelect && CharacterSelect->IsInViewport() && !IsGearBenchOpen())
	{
		PlayerOwner->bShowMouseCursor = true;
		PlayerOwner->bEnableClickEvents = true;
		FInputModeUIOnly InputMode;
		InputMode.SetWidgetToFocus(CharacterSelect->TakeWidget());
		InputMode.SetLockMouseToViewportBehavior(EMouseLockMode::DoNotLock);
		PlayerOwner->SetInputMode(InputMode);
		return;
	}

	const bool bParty = IsSessionMenuOpen();
	const bool bVoting = (RoundOver && RoundOver->IsInViewport()) || (CageVote && CageVote->IsInViewport()) || (StarterKit && StarterKit->IsInViewport());
	const bool bWantsCursor = bParty || bShowMenu || bShowAdminPanel || bLootScreenOpen || bVoting;

	PlayerOwner->bShowMouseCursor = bWantsCursor;
	PlayerOwner->bEnableClickEvents = bWantsCursor;
	PlayerOwner->bEnableMouseOverEvents = bWantsCursor;

	// The shop is modal: clicks buy things, they do not swing a sword.
	if (IsShopOpen())
	{
		PlayerOwner->bShowMouseCursor = true;
		FInputModeUIOnly InputMode;
		InputMode.SetWidgetToFocus(ShopWidget->TakeWidget());
		InputMode.SetLockMouseToViewportBehavior(EMouseLockMode::DoNotLock);
		PlayerOwner->SetInputMode(InputMode);
		return;
	}

	// The gear bench owns the mouse (orbit) and the keyboard (play, step).
	if (IsGearBenchOpen())
	{
		PlayerOwner->bShowMouseCursor = true;
		FInputModeUIOnly InputMode;
		InputMode.SetWidgetToFocus(GearBench->TakeWidget());
		InputMode.SetLockMouseToViewportBehavior(EMouseLockMode::DoNotLock);
		PlayerOwner->SetInputMode(InputMode);
		return;
	}

	// The loot box screen is modal: Space opens boxes rather than jumping.
	if (bLootScreenOpen && LootScreenInput)
	{
		FInputModeUIOnly InputMode;
		InputMode.SetWidgetToFocus(LootScreenInput->TakeWidget());
		InputMode.SetLockMouseToViewportBehavior(EMouseLockMode::DoNotLock);
		PlayerOwner->SetInputMode(InputMode);
		return;
	}

	if (bWantsCursor)
	{
		// GameAndUI, as the other panels do: F2 still reaches the game to close it.
		FInputModeGameAndUI InputMode;
		InputMode.SetLockMouseToViewportBehavior(EMouseLockMode::DoNotLock);
		InputMode.SetHideCursorDuringCapture(false);
		if (bParty)
		{
			InputMode.SetWidgetToFocus(SessionMenu->TakeWidget());
		}
		PlayerOwner->SetInputMode(InputMode);
	}
	else
	{
		PlayerOwner->SetInputMode(FInputModeGameOnly());
	}
}

static void DCCPartyCommand(UWorld* World)
{
	if (!World)
	{
		return;
	}
	for (FConstPlayerControllerIterator It = World->GetPlayerControllerIterator(); It; ++It)
	{
		APlayerController* PC = It->Get();
		if (PC && PC->IsLocalController())
		{
			if (ADCCHUD* Hud = Cast<ADCCHUD>(PC->GetHUD()))
			{
				Hud->ToggleSessionMenu();
			}
			return;
		}
	}
}

static FAutoConsoleCommandWithWorld GPartyCmd(
	TEXT("DCC.Party"),
	TEXT("Open or close the party menu (host, invite, join). Same as F2."),
	FConsoleCommandWithWorldDelegate::CreateStatic(&DCCPartyCommand));

// ---------------------------------------------------------------------------
// Gear bench (2026-11-13)
// ---------------------------------------------------------------------------

bool ADCCHUD::IsGearBenchOpen() const
{
	return GearBench && GearBench->IsInViewport();
}

void ADCCHUD::ToggleGearBench()
{
	if (!PlayerOwner)
	{
		return;
	}

	if (IsGearBenchOpen())
	{
		GearBench->End();
		GearBench->RemoveFromParent();
		ApplyCursorForOpenPanels();
		return;
	}

	ADCCCharacterBase* Crawler = GetOwningCrawler();
	if (!Crawler)
	{
		return;
	}
	if (!GearBench)
	{
		GearBench = CreateWidget<UDCCGearBenchWidget>(PlayerOwner, UDCCGearBenchWidget::StaticClass());
		if (!GearBench)
		{
			return;
		}
		GearBench->Hud = this;
	}

	GearBench->AddToViewport(58);
	GearBench->Begin(Crawler);
	ApplyCursorForOpenPanels();
}

static void DCCBenchCommand(UWorld* World)
{
	if (!World)
	{
		return;
	}
	for (FConstPlayerControllerIterator It = World->GetPlayerControllerIterator(); It; ++It)
	{
		APlayerController* PC = It->Get();
		if (PC && PC->IsLocalController())
		{
			if (ADCCHUD* Hud = Cast<ADCCHUD>(PC->GetHUD()))
			{
				Hud->ToggleGearBench();
			}
			return;
		}
	}
}

static FAutoConsoleCommandWithWorld GBenchCmd(
	TEXT("DCC.Bench"),
	TEXT("Open or close the gear bench: nudge item grips, sockets and bones, and step through animations. Same as F3."),
	FConsoleCommandWithWorldDelegate::CreateStatic(&DCCBenchCommand));

// ---------------------------------------------------------------------------
// Shop screen (2026-11-13)
// ---------------------------------------------------------------------------

bool ADCCHUD::IsShopOpen() const
{
	return ShopWidget && ShopWidget->IsInViewport();
}

void ADCCHUD::CloseShop()
{
	if (IsShopOpen())
	{
		ShopWidget->RemoveFromParent();
		ApplyCursorForOpenPanels();
	}
}

void ADCCHUD::SyncShop(ADCCCharacterBase* Crawler)
{
	ADCCVendorNPC* Vendor = Crawler ? Cast<ADCCVendorNPC>(Crawler->GetVendorTarget()) : nullptr;
	if (!Vendor)
	{
		CloseShop();
		return;
	}
	if (IsShopOpen())
	{
		ShopWidget->TickShop();
		return;
	}
	if (!PlayerOwner)
	{
		return;
	}
	if (!ShopWidget)
	{
		ShopWidget = CreateWidget<UDCCShopWidget>(PlayerOwner, UDCCShopWidget::StaticClass());
		if (!ShopWidget)
		{
			return;
		}
		ShopWidget->Hud = this;
	}
	ShopWidget->AddToViewport(56);
	ShopWidget->Open(Vendor);
	ApplyCursorForOpenPanels();
}

// ---------------------------------------------------------------------------
// Arcade status (2026-11-13)
// ---------------------------------------------------------------------------

const ADCCArcadeArena* ADCCHUD::GetArcadeArena() const
{
	if (UWorld* World = GetWorld())
	{
		for (TActorIterator<ADCCArcadeArena> It(World); It; ++It)
		{
			return *It;
		}
	}
	return nullptr;
}

void ADCCHUD::DrawClimbKit(ADCCCharacterBase* Crawler)
{
	const UDCCWalletComponent* Wallet = Crawler ? Crawler->Wallet.Get() : nullptr;
	// Hunger lives on the stamina bar now (DrawStaminaBar), eating into it from the right (2026-11-15).
	if (!Canvas || !Wallet)
	{
		return;
	}
	UFont* Font = GEngine->GetSmallFont();
	TArray<TPair<FString, FLinearColor>> Lines;
	// The grappling hook and the tools are slots on the belt now (2026-11-16, DrawHotbar); this panel keeps the rest.
	if (!Wallet->ClimbBoosts.IsEmpty())
	{
		Lines.Add({ Wallet->ClimbBoosts, FLinearColor(0.6f, 0.9f, 1.0f) });
	}
	for (const FName& Id : Wallet->CarriedArtifacts)
	{
		const DCCClimbItems::FItem* Artifact = DCCClimbItems::Find(Id);
		Lines.Add({ FString::Printf(TEXT("Carrying: %s"), Artifact ? Artifact->Name : TEXT("?")), FLinearColor(1.0f, 0.78f, 0.3f) });
	}

	float W = 0.0f, H = 0.0f;
	for (const TPair<FString, FLinearColor>& Line : Lines)
	{
		float LW = 0.0f;
		GetTextSize(Line.Key, LW, H, Font, 1.0f);
		W = FMath::Max(W, LW);
	}
	const float PanelW = W + 24.0f, PanelH = Lines.Num() * (H + 3.0f) + 14.0f;
	const float X = Canvas->SizeX - PanelW - 24.0f, Y = Canvas->SizeY - PanelH - 120.0f;
	DrawRect(FLinearColor(0.02f, 0.015f, 0.04f, 0.7f), X, Y, PanelW, PanelH);
	float LY = Y + 7.0f;
	for (const TPair<FString, FLinearColor>& Line : Lines)
	{
		DrawText(Line.Key, Line.Value, X + 12.0f, LY, Font, 1.0f);
		LY += H + 3.0f;
	}
}

void ADCCHUD::DrawRallyKit(const ADCCRover* Rover)
{
	const ADCCArcadeKart* Kart = Cast<ADCCArcadeKart>(GetArcadeArena());
	const float Scale = Canvas ? Canvas->ClipY / 1080.0f : 1.0f;
	UFont* Medium = GEngine ? GEngine->GetMediumFont() : nullptr;
	const float T = GetWorld() ? float(GetWorld()->GetTimeSeconds()) : 0.0f;
	// Marks in the world (2026-11-14 polish): each one carries the same number and colour as its row on THE BOARD.
	// On screen: a pulsing diamond with a numbered tag. Off screen: a solid chevron on the edge with the number and
	// distance only, so several off-screen marks no longer pile their names on top of each other.
	const float US = Canvas ? FMath::Clamp(Canvas->SizeY / 1080.0f, 0.6f, 2.0f) : 1.0f;
	UFont* SmallF = GEngine ? GEngine->GetSmallFont() : nullptr;
	auto Chevron = [this](const FVector2D& At, const FVector2D& Dir, float Size, const FLinearColor& C)
	{
		const FVector2D Side(-Dir.Y, Dir.X);
		const FVector2D Tip = At + Dir * Size, L = At - Dir * Size * 0.4f + Side * Size * 0.8f, R = At - Dir * Size * 0.4f - Side * Size * 0.8f;
		FCanvasTriangleItem Tri(Tip, L, R, GWhiteTexture);
		Tri.SetColor(C);
		Tri.BlendMode = SE_BLEND_Translucent;
		Canvas->DrawItem(Tri);
	};
	auto NumberTile = [this, SmallF, US](float X, float Y, int32 Number, const FLinearColor& C)
	{
		const float T = 22.0f * US;
		DrawRect(FLinearColor(0.03f, 0.015f, 0.05f, 0.85f), X, Y, T, T);
		DrawRect(FLinearColor(C.R, C.G, C.B, 0.35f), X, Y, T, T);
		DrawRect(C, X, Y + T - 2.0f * US, T, 2.0f * US);
		const FString N = Number > 0 ? FString::FromInt(Number) : FString(TEXT("?"));
		float W = 0.0f, H = 0.0f;
		RallyTextSize(N, W, H, SmallF, 1.3f * US);
		RallyText(N, FLinearColor::White, X + (T - W) * 0.5f, Y + (T - H) * 0.5f, SmallF, 1.3f * US);
		return T;
	};
	auto Place = [this](const FVector& World, FVector2D& OutP, FVector2D& OutDir)
	{
		const FVector Screen = Canvas->Project(World);
		const float CX = Canvas->ClipX * 0.5f, CY = Canvas->ClipY * 0.5f;
		const float M = 70.0f;
		const bool bOn = Screen.Z > 0.0f && Screen.X > M && Screen.X < Canvas->ClipX - M && Screen.Y > M && Screen.Y < Canvas->ClipY - M;
		OutP = FVector2D(Screen.X, Screen.Y);
		if (bOn)
		{
			return true;
		}
		FVector2D D(Screen.X - CX, Screen.Y - CY);
		if (Screen.Z <= 0.0f) { D = -D; }
		D.Normalize();
		const float K = FMath::Min((CX - M) / FMath::Max(0.001f, FMath::Abs(D.X)), (CY - M) / FMath::Max(0.001f, FMath::Abs(D.Y)));
		OutP = FVector2D(CX, CY) + D * K;
		OutDir = D;
		return false;
	};
	(void)Scale;
	(void)Medium;

	if (Canvas && Kart && Rover && Kart->IsShiftOn() && !Kart->bRoundOver)
	{
		TArray<FBox2D> PlacedTags;
		for (TActorIterator<ADCCRover> It(GetWorld()); It; ++It)
		{
			if (It->RallyDuty != 1)
			{
				continue;
			}
			int32 Number = 0;
			for (int32 b = 0; b < Kart->Board.Num(); ++b)
			{
				if (Kart->Board[b].Plate == It->RallyPlate) { Number = b + 1; break; }
			}
			if (Number > 0 && Kart->Board[Number - 1].State == 6)
			{
				continue; // already on somebody's bumper: no marker over the tow
			}
			FLinearColor C = ADCCArcadeKart::MarkColour(It->RallyPlate);
			const float Metres = FVector::Dist(It->GetActorLocation(), Rover->GetActorLocation()) / 100.0f;
			FVector2D P, Dir;
			if (Place(It->GetActorLocation() + FVector(0, 0, 220), P, Dir))
			{
				const float Pulse = 0.5f + 0.5f * FMath::Sin(T * 5.0f);
				const float R = (14.0f + 5.0f * Pulse) * US;
				const FLinearColor Ring(C.R, C.G, C.B, 0.95f);
				DrawLine(P.X, P.Y - R, P.X + R, P.Y, Ring, 2.5f * US);
				DrawLine(P.X + R, P.Y, P.X, P.Y + R, Ring, 2.5f * US);
				DrawLine(P.X, P.Y + R, P.X - R, P.Y, Ring, 2.5f * US);
				DrawLine(P.X - R, P.Y, P.X, P.Y - R, Ring, 2.5f * US);
				const float TX = P.X + 26.0f * US;
				float TY = P.Y - 11.0f * US;
				// Two marks near each other on screen (often, at the horizon): push the later tag down until it is clear.
				const float TagW = 240.0f * US, TagH = 25.0f * US;
				for (int32 Guard = 0; Guard < 6; ++Guard)
				{
					const FBox2D Mine(FVector2D(TX, TY), FVector2D(TX + TagW, TY + TagH));
					const FBox2D* Hit = PlacedTags.FindByPredicate([&Mine](const FBox2D& B) { return B.Intersect(Mine); });
					if (!Hit) { break; }
					TY = Hit->Max.Y + 2.0f * US;
				}
				PlacedTags.Add(FBox2D(FVector2D(TX, TY), FVector2D(TX + TagW, TY + TagH)));
				const float TileW = NumberTile(TX, TY, Number, C);
				RallyPill(TX + TileW + 3.0f * US, TY + 1.0f * US, FString::Printf(TEXT("%s  %.0f m"), *It->RallyPlate, Metres), C, US);
			}
			else
			{
				Chevron(P, Dir, 16.0f * US, FLinearColor(C.R, C.G, C.B, 0.95f));
				const FVector2D Back = P - Dir * 34.0f * US;
				const float Tile = NumberTile(Back.X - 11.0f * US, Back.Y - 11.0f * US, Number, C);
				const FString D = FString::Printf(TEXT("%.0fm"), Metres);
				float DW = 0.0f, DH = 0.0f;
				RallyTextSize(D, DW, DH, SmallF, 1.0f * US);
				RallyText(D, FLinearColor(1, 1, 1, 0.9f), Back.X - DW * 0.5f, Back.Y + Tile * 0.5f + 2.0f * US, SmallF, 1.0f * US);
			}
		}
	}

	// Low fuel: a green chevron to the nearest station (the stations also light up, FDCCRallyAmbience).
	if (Canvas && Kart && Rover && Rover->RallyFuel >= 0.0f && Rover->RallyFuel < 35.0f)
	{
		FVector Best = FVector::ZeroVector;
		float BestD = TNumericLimits<float>::Max();
		for (int32 g = 0; g < 2; ++g)
		{
			const FVector2D Off = ADCCArcadeKart::GasOffset(g);
			const FVector At = Kart->GetActorLocation() + FVector(Off.X, Off.Y, 300.0f);
			const float D = FVector::Dist(At, Rover->GetActorLocation());
			if (D < BestD) { BestD = D; Best = At; }
		}
		const FLinearColor Green(0.3f, 1.0f, 0.45f, 0.55f + 0.45f * FMath::Abs(FMath::Sin(T * 4.0f)));
		FVector2D P, Dir;
		const bool bOn = Place(Best, P, Dir);
		if (!bOn)
		{
			Chevron(P, Dir, 16.0f * US, Green);
			P -= Dir * 40.0f * US;
		}
		const FString Label = FString::Printf(TEXT("GAS  %.0f m"), BestD / 100.0f);
		float LW = 0.0f, LH = 0.0f;
		RallyTextSize(Label, LW, LH, SmallF, 1.05f * US);
		RallyPill(P.X - LW * 0.5f - 7.0f * US, P.Y - LH * 0.5f, Label, Green, US);
	}

	// While a car is on the line: one clear card, the key and what it does (2026-11-13, restyled 2026-11-14).
	if (Canvas && Rover && Rover->RallyTow)
	{
		const bool bPays = Rover->bRallyTowDocked && Rover->bRallyInYard;
		const FString Main = bPays ? TEXT("DROP IT") : Rover->bRallyTowDocked ? TEXT("BRING IT TO THE YARD") : TEXT("MAGNET LOCKED");
		const FString Sub = bPays ? TEXT("In the yard - it pays.") : TEXT("Q cuts it loose. No pay outside the yard.");
		const FLinearColor Accent = bPays ? FLinearColor(0.45f, 1.0f, 0.6f) : FLinearColor(1.0f, 0.78f, 0.32f);
		UFont* Big = GEngine->GetLargeFont();
		float MW = 0.0f, MH = 0.0f, SW = 0.0f, SH = 0.0f;
		RallyTextSize(Main, MW, MH, Big, 1.7f * US);
		RallyTextSize(Sub, SW, SH, SmallF, 1.1f * US);
		const float KeyW = bPays ? 40.0f * US : 0.0f;
		const float W = FMath::Max(MW + KeyW, SW) + 40.0f * US;
		const float H = MH + SH + 24.0f * US;
		const float X = (Canvas->ClipX - W) * 0.5f, Y = Canvas->ClipY * 0.64f;
		RallyPanel(X, Y, W, H, Accent);
		float MX = X + (W - MW - KeyW) * 0.5f;
		if (bPays)
		{
			MX += RallyKey(MX, Y + 10.0f * US + (MH - 26.0f * US) * 0.5f, TEXT("Q"), US) + 10.0f * US;
		}
		RallyText(Main, Accent, MX, Y + 8.0f * US, Big, 1.7f * US);
		RallyText(Sub, FLinearColor(0.85f, 0.83f, 0.92f), X + (W - SW) * 0.5f, Y + 12.0f * US + MH, SmallF, 1.1f * US);
	}
	// The kit text that used to sit here is the YOUR TRUCK panel now (DrawRallyTruck, 2026-11-14).
	if (Canvas)
	{
		return;
	}
	UFont* Font = GEngine->GetSmallFont();
	TArray<TPair<FString, FLinearColor>> Lines;
	if (const FString Radio = Rover->GetRadioLine(); !Radio.IsEmpty())
	{
		Lines.Add({ Radio, FLinearColor(1.0f, 0.55f, 0.85f) });
	}
	if (!Rover->RallyTools.IsEmpty())
	{
		const int32 i = FMath::Clamp(Rover->RallyToolIndex, 0, Rover->RallyTools.Num() - 1);
		const DCCRallyItems::FItem* Tool = DCCRallyItems::Find(Rover->RallyTools[i]);
		Lines.Add({ FString::Printf(TEXT("[Q] %s"), Tool ? Tool->Name : TEXT("?")), FLinearColor(1.0f, 0.86f, 0.55f) });
		if (Rover->RallyTools.Num() > 1)
		{
			TArray<FString> Others;
			for (int32 k = 1; k < Rover->RallyTools.Num(); ++k)
			{
				const DCCRallyItems::FItem* Other = DCCRallyItems::Find(Rover->RallyTools[(i + k) % Rover->RallyTools.Num()]);
				Others.Add(Other ? Other->Name : TEXT("?"));
			}
			Lines.Add({ FString::Printf(TEXT("[F] next: %s"), *FString::Join(Others, TEXT(", "))), DimColour });
		}
	}
	if (!Rover->RallyStatus.IsEmpty())
	{
		Lines.Add({ Rover->RallyStatus, FLinearColor(0.55f, 0.85f, 1.0f) });
	}
	for (const FName& Id : Rover->RallyCargo)
	{
		const DCCRallyItems::FItem* Seized = DCCRallyItems::Find(Id);
		Lines.Add({ FString::Printf(TEXT("Carrying: %s"), Seized ? Seized->Name : TEXT("?")), FLinearColor(1.0f, 0.78f, 0.3f) });
	}

	float W = 0.0f, H = 0.0f;
	for (const TPair<FString, FLinearColor>& Line : Lines)
	{
		float LW = 0.0f;
		GetTextSize(Line.Key, LW, H, Font, 1.0f);
		W = FMath::Max(W, LW);
	}
	const float PanelW = W + 24.0f, PanelH = Lines.Num() * (H + 3.0f) + 14.0f;
	const float X = Canvas->SizeX - PanelW - 24.0f, Y = Canvas->SizeY - PanelH - 120.0f;
	DrawRect(FLinearColor(0.04f, 0.02f, 0.015f, 0.72f), X, Y, PanelW, PanelH);
	float LY = Y + 7.0f;
	for (const TPair<FString, FLinearColor>& Line : Lines)
	{
		DrawText(Line.Key, Line.Value, X + 12.0f, LY, Font, 1.0f);
		LY += H + 3.0f;
	}
}

void ADCCHUD::DrawRepoMinimap(const ADCCRover* Self)
{
	// The city map (2026-11-14 polish): dark city blocks between neon streets, the yard in green, gas stations,
	// the marks as diamonds in their board colour and number, rival crews in red, angry owners as red dots,
	// and you as an arrow that points the way the truck faces. A legend inside the panel.
	const ADCCArcadeKart* Kart = Cast<ADCCArcadeKart>(GetArcadeArena());
	if (!Canvas || !Kart || !GetWorld())
	{
		return;
	}
	const float US = FMath::Clamp(Canvas->SizeY / 1080.0f, 0.6f, 2.0f);
	UFont* Small = GEngine->GetSmallFont();
	const FLinearColor Pink(1.0f, 0.32f, 0.82f), Cyan(0.3f, 0.9f, 1.0f), Green(0.4f, 1.0f, 0.55f), Red(1.0f, 0.3f, 0.3f);
	const FLinearColor Dim(0.66f, 0.62f, 0.76f);

	const float MS = 236.0f * US;                // the square map
	const float Head = 38.0f * US, Foot = 30.0f * US, Pad = 10.0f * US;
	const float W = MS + Pad * 2.0f, H = Head + MS + Foot;
	const float X = 24.0f, Y = Canvas->SizeY - H - 24.0f;
	RallyPanel(X, Y, W, H, Pink);
	RallyText(TEXT("CITY"), Pink, X + 14.0f * US, Y + 8.0f * US, GEngine->GetLargeFont(), 1.5f * US);
	const float MX = X + Pad, MY = Y + Head;
	DrawRect(FLinearColor(0.012f, 0.006f, 0.025f, 0.9f), MX, MY, MS, MS);

	const FVector O = Kart->GetActorLocation();
	const float Half = ADCCArcadeKart::MapHalf;
	auto ToMap = [&](float Wx, float Wy)
	{
		const float U = (Wx - O.X) / (Half * 2.0f) + 0.5f;
		const float V = (Wy - O.Y) / (Half * 2.0f) + 0.5f;
		return FVector2D(MX + FMath::Clamp(U, 0.0f, 1.0f) * MS, MY + (1.0f - FMath::Clamp(V, 0.0f, 1.0f)) * MS);
	};
	const float Sp = ADCCArcadeKart::StreetSpacing;
	const int32 R = ADCCArcadeKart::StreetRadius;

	// Blocks, then the streets between them.
	const float Inset = 2600.0f;
	for (int32 Ix = -R; Ix < R; ++Ix)
	{
		for (int32 Iy = -R; Iy < R; ++Iy)
		{
			const bool bYard = Ix == -R && Iy == -R;
			const FVector2D A = ToMap(O.X + Ix * Sp + Inset * 0.5f, O.Y + (Iy + 1) * Sp - Inset * 0.5f);
			const FVector2D B = ToMap(O.X + (Ix + 1) * Sp - Inset * 0.5f, O.Y + Iy * Sp + Inset * 0.5f);
			DrawRect(bYard ? FLinearColor(0.1f, 0.35f, 0.18f, 0.75f) : FLinearColor(0.16f, 0.08f, 0.24f, 0.9f), A.X, A.Y, B.X - A.X, B.Y - A.Y);
			if (bYard)
			{
				float TW = 0.0f, TH = 0.0f;
				RallyTextSize(TEXT("YARD"), TW, TH, Small, 0.9f * US);
				RallyText(TEXT("YARD"), Green, (A.X + B.X - TW) * 0.5f, (A.Y + B.Y - TH) * 0.5f, Small, 0.9f * US, false);
			}
		}
	}
	const float Span = R * Sp;
	for (int32 i = -R; i <= R; ++i)
	{
		const FLinearColor Line = (i % 2 == 0) ? FLinearColor(Pink.R, Pink.G, Pink.B, 0.45f) : FLinearColor(Cyan.R, Cyan.G, Cyan.B, 0.45f);
		const FVector2D A = ToMap(O.X - Span, O.Y + i * Sp), B = ToMap(O.X + Span, O.Y + i * Sp);
		DrawRect(Line, FMath::Min(A.X, B.X), A.Y - 0.5f * US, FMath::Abs(B.X - A.X), 1.0f * US);
		const FVector2D C = ToMap(O.X + i * Sp, O.Y - Span), D = ToMap(O.X + i * Sp, O.Y + Span);
		DrawRect(Line, C.X - 0.5f * US, FMath::Min(C.Y, D.Y), 1.0f * US, FMath::Abs(D.Y - C.Y));
	}

	auto Square = [&](const FVector2D& P, float Size, const FLinearColor& C)
	{
		DrawRect(C, P.X - Size * 0.5f, P.Y - Size * 0.5f, Size, Size);
	};
	auto Diamond = [&](const FVector2D& P, float Size, const FLinearColor& C)
	{
		for (float k = 0.0f; k <= Size; k += 1.0f)
		{
			const float Wd = (Size - k) * 2.0f;
			DrawRect(C, P.X - Wd * 0.5f, P.Y - k, Wd, 1.0f);
			DrawRect(C, P.X - Wd * 0.5f, P.Y + k, Wd, 1.0f);
		}
	};

	// Gas stations.
	for (int32 g = 0; g < 2; ++g)
	{
		const FVector2D Off = ADCCArcadeKart::GasOffset(g);
		const FVector2D P = ToMap(O.X + Off.X, O.Y + Off.Y);
		Square(P, 13.0f * US, FLinearColor(0.05f, 0.25f, 0.1f, 1.0f));
		Square(P, 11.0f * US, Green * 0.8f);
		float TW = 0.0f, TH = 0.0f;
		RallyTextSize(TEXT("G"), TW, TH, Small, 0.85f * US);
		RallyText(TEXT("G"), FLinearColor(0.02f, 0.1f, 0.04f), P.X - TW * 0.5f, P.Y - TH * 0.5f, Small, 0.85f * US, false);
	}

	// Traffic first (under everything), then crews, marks, owners and other players.
	for (TActorIterator<ADCCRover> It(GetWorld()); It; ++It)
	{
		const FVector2D P = ToMap(It->GetActorLocation().X, It->GetActorLocation().Y);
		if (It->RallyDuty == 2 || It->RallyDuty == 4)
		{
			Square(P, 2.5f * US, FLinearColor(0.6f, 0.55f, 0.7f, 0.45f));
		}
	}
	for (TActorIterator<ADCCRover> It(GetWorld()); It; ++It)
	{
		const FVector2D P = ToMap(It->GetActorLocation().X, It->GetActorLocation().Y);
		if (It->RallyDuty == 3)
		{
			Square(P, 7.0f * US, FLinearColor(0.2f, 0.0f, 0.0f, 1.0f));
			Square(P, 5.0f * US, Red);
		}
		else if (It->RallyDuty == 1)
		{
			int32 Number = 0;
			for (int32 b = 0; b < Kart->Board.Num(); ++b)
			{
				if (Kart->Board[b].Plate == It->RallyPlate) { Number = b + 1; break; }
			}
			const FLinearColor C = ADCCArcadeKart::MarkColour(It->RallyPlate);
			Diamond(P, 6.0f * US, FLinearColor(0.0f, 0.0f, 0.0f, 0.8f));
			Diamond(P, 5.0f * US, C);
			if (Number > 0)
			{
				RallyText(FString::FromInt(Number), FLinearColor::White, P.X + 6.0f * US, P.Y - 9.0f * US, Small, 0.85f * US);
			}
		}
		else if (It->RallyDuty == 0 && *It != Self && It->IsOccupied())
		{
			Square(P, 7.0f * US, Cyan);
		}
	}
	for (TActorIterator<ADCCRallyThug> It(GetWorld()); It; ++It)
	{
		if (It->Mood != ADCCRallyThug::Down)
		{
			Square(ToMap(It->GetActorLocation().X, It->GetActorLocation().Y), 4.0f * US, Red);
		}
	}
	if (Self)
	{
		const FVector2D P = ToMap(Self->GetActorLocation().X, Self->GetActorLocation().Y);
		const float Yaw = Self->GetActorRotation().Yaw;
		Square(P, 12.0f * US, FLinearColor(1.0f, 1.0f, 1.0f, 0.12f));
		RallyArrow(P.X, P.Y, 6.5f * US, 90.0f - Yaw, FLinearColor::White);
	}

	// Legend, in one row along the bottom of the panel.
	float LX = X + 12.0f * US;
	const float LY = Y + Head + MS + 8.0f * US;
	auto Key = [&](const FLinearColor& C, const TCHAR* Label, bool bDiamond)
	{
		const FVector2D P(LX + 5.0f * US, LY + 7.0f * US);
		if (bDiamond) { Diamond(P, 5.0f * US, C); } else { Square(P, 8.0f * US, C); }
		float TW = 0.0f, TH = 0.0f;
		RallyTextSize(Label, TW, TH, Small, 0.95f * US);
		RallyText(Label, Dim, LX + 14.0f * US, LY, Small, 0.95f * US, false);
		LX += 14.0f * US + TW + 12.0f * US;
	};
	Key(FLinearColor(1.0f, 0.75f, 0.3f), TEXT("mark"), true);
	Key(Red, TEXT("danger"), false);
	Key(Green, TEXT("gas"), false);
	Key(Cyan, TEXT("crew"), false);
}

void ADCCHUD::DrawHookReticle(const ADCCRover* Rover)
{
	if (!Canvas || !Rover || !Rover->bHookAim)
	{
		return;
	}
	const float CX = Canvas->SizeX * 0.5f;
	const float CY = Canvas->SizeY * 0.5f;
	const float T = GetWorld() ? static_cast<float>(GetWorld()->GetTimeSeconds()) : 0.0f;
	auto Ring = [this](float X, float Y, float Radius, const FLinearColor& C, float Thick)
	{
		const int32 Segments = 32;
		for (int32 i = 0; i < Segments; ++i)
		{
			const float A0 = 2.0f * PI * i / Segments, A1 = 2.0f * PI * (i + 1) / Segments;
			DrawLine(X + FMath::Cos(A0) * Radius, Y + FMath::Sin(A0) * Radius, X + FMath::Cos(A1) * Radius, Y + FMath::Sin(A1) * Radius, C, Thick);
		}
	};

	// The lock (2026-11-13: "a pulsing target indicator"): a marked car within a few degrees of the aim
	// and in range gets a pulsing ring, its plate and distance; Q now fires straight at it.
	FVector LockDir;
	if (const ADCCRover* Locked = Rover->FindHookLock(LockDir))
	{
		const FVector Screen = Canvas->Project(Locked->GetActorLocation() + FVector(0, 0, 60));
		if (Screen.Z > 0.0f)
		{
			const float Pulse = 0.5f + 0.5f * FMath::Sin(T * 7.0f);
			FLinearColor Lock = ADCCArcadeKart::MarkColour(Locked->RallyPlate);
			Lock.A = 0.95f;
			Ring(Screen.X, Screen.Y, 38.0f + 10.0f * Pulse, Lock, 3.0f);
			Ring(Screen.X, Screen.Y, 22.0f, FLinearColor(0.35f, 1.0f, 0.55f, 0.35f + 0.5f * Pulse), 2.0f);
			for (int32 k = 0; k < 4; ++k) // corner ticks closing in
			{
				const float A = PI * 0.25f + k * PI * 0.5f;
				const float R0 = 54.0f + 8.0f * (1.0f - Pulse), R1 = R0 + 14.0f;
				DrawLine(Screen.X + FMath::Cos(A) * R0, Screen.Y + FMath::Sin(A) * R0, Screen.X + FMath::Cos(A) * R1, Screen.Y + FMath::Sin(A) * R1, Lock, 3.0f);
			}
			const float Metres = FVector::Dist(Locked->GetActorLocation(), Rover->GetActorLocation()) / 100.0f;
			RallyPill(Screen.X + 66.0f, Screen.Y - 12.0f, FString::Printf(TEXT("LOCKED  %s  %.0f m   -   Q  HOOK"), *Locked->RallyPlate, Metres), Lock,
				FMath::Clamp(Canvas->SizeY / 1080.0f, 0.6f, 2.0f) * 1.15f);
			DrawLine(CX, CY, Screen.X, Screen.Y, FLinearColor(0.35f, 1.0f, 0.55f, 0.25f), 1.0f);
		}
	}
	const FLinearColor Colour(1.0f, 0.78f, 0.38f, 0.95f);
	Ring(CX, CY, 26.0f, FLinearColor(1.0f, 0.78f, 0.38f, 0.35f), 1.5f);
	DrawRect(Colour, CX - 16.0f, CY - 1.0f, 8.0f, 2.0f);
	DrawRect(Colour, CX + 8.0f, CY - 1.0f, 8.0f, 2.0f);
	DrawRect(Colour, CX - 1.0f, CY - 16.0f, 2.0f, 8.0f);
	DrawRect(Colour, CX - 1.0f, CY + 8.0f, 2.0f, 8.0f);
	DrawRect(Colour, CX - 1.5f, CY - 1.5f, 3.0f, 3.0f);
	DrawText(TEXT("HOOK"), Colour, CX + 18.0f, CY - 20.0f, GEngine->GetSmallFont(), 1.0f);
}

void ADCCHUD::DrawNPCSubtitle()
{
	const APawn* Pawn = PlayerOwner ? PlayerOwner->GetPawn() : nullptr;
	if (!Canvas || !Pawn || !GetWorld())
	{
		return;
	}
	const ADCCDialogueNPC* Speaker = nullptr;
	float Best = 1500.0f * 1500.0f;
	for (TActorIterator<ADCCDialogueNPC> It(GetWorld()); It; ++It)
	{
		const float D = FVector::DistSquared(It->GetActorLocation(), Pawn->GetActorLocation());
		if (!It->GetCurrentLine().IsEmpty() && D < Best)
		{
			Best = D;
			Speaker = *It;
		}
	}
	if (!Speaker)
	{
		return;
	}

	// "Name: line", wrapped to fit, in a dark band just above the hotbar.
	UFont* Font = GEngine->GetMediumFont();
	const FString Name = Speaker->DisplayName.IsEmpty() ? FString() : Speaker->DisplayName.ToString() + TEXT(":  ");
	TArray<FString> Words;
	Speaker->GetCurrentLine().ToString().ParseIntoArrayWS(Words);
	TArray<FString> Rows;
	FString Row = Name;
	const float MaxW = FMath::Min(900.0f, Canvas->SizeX * 0.6f);
	for (const FString& Word : Words)
	{
		const FString Try = Row.IsEmpty() ? Word : Row + TEXT(" ") + Word;
		float W = 0.0f, H = 0.0f;
		GetTextSize(Try, W, H, Font, 1.0f);
		if (W > MaxW && !Row.IsEmpty())
		{
			Rows.Add(Row);
			Row = Word;
		}
		else
		{
			Row = Try;
		}
	}
	Rows.Add(Row);

	float LineW = 0.0f, LineH = 0.0f;
	for (const FString& R : Rows)
	{
		float W = 0.0f;
		GetTextSize(R, W, LineH, Font, 1.0f);
		LineW = FMath::Max(LineW, W);
	}
	const float PanelH = Rows.Num() * (LineH + 2.0f) + 16.0f;
	const float X = (Canvas->SizeX - LineW - 32.0f) * 0.5f;
	const float Y = Canvas->SizeY - 150.0f - PanelH;
	DrawRect(FLinearColor(0.0f, 0.0f, 0.0f, 0.6f), X, Y, LineW + 32.0f, PanelH);
	float TY = Y + 8.0f;
	for (int32 i = 0; i < Rows.Num(); ++i)
	{
		float W = 0.0f, H = 0.0f;
		GetTextSize(Rows[i], W, H, Font, 1.0f);
		DrawText(Rows[i], i == 0 ? FLinearColor(1.0f, 0.86f, 0.6f) : FLinearColor(0.96f, 0.94f, 0.9f), (Canvas->SizeX - W) * 0.5f, TY, Font, 1.0f);
		TY += LineH + 2.0f;
	}
}

void ADCCHUD::DrawArcadeStatus()
{
	if (!Canvas || !GetWorld())
	{
		return;
	}
	const ADCCArcadeArena* Arena = nullptr;
	for (TActorIterator<ADCCArcadeArena> It(GetWorld()); It; ++It)
	{
		Arena = *It;
		break;
	}
	if (!Arena || Arena->Title.IsEmpty())
	{
		return;
	}
	// The boss bar: a long segmented bar low in the middle of the screen, the boss's name over it.
	if (!Arena->BossName.IsEmpty() && Arena->BossHealth >= 0.0f)
	{
		const float BarW = FMath::Min(720.0f, Canvas->SizeX * 0.5f), BarH = 16.0f;
		const float BX = (Canvas->SizeX - BarW) * 0.5f, BY = Canvas->SizeY * 0.86f - 78.0f;
		const float Fill = FMath::Clamp(Arena->BossHealth, 0.0f, 1.0f);
		DrawRect(FLinearColor(0.0f, 0.0f, 0.0f, 0.7f), BX - 3.0f, BY - 3.0f, BarW + 6.0f, BarH + 6.0f);
		DrawRect(FLinearColor(0.25f, 0.08f, 0.35f, 0.9f), BX, BY, BarW, BarH);
		DrawRect(FMath::Lerp(FLinearColor(1.0f, 0.25f, 0.35f), FLinearColor(0.7f, 0.35f, 1.0f), Fill), BX, BY, BarW * Fill, BarH);
		for (const float Mark : { 0.33f, 0.66f })
		{
			DrawRect(FLinearColor(1.0f, 1.0f, 1.0f, 0.55f), BX + BarW * Mark - 1.0f, BY - 3.0f, 2.0f, BarH + 6.0f);
		}
		float NW = 0.0f, NH = 0.0f;
		UFont* Medium = GEngine->GetMediumFont();
		GetTextSize(Arena->BossName, NW, NH, Medium, 1.0f);
		DrawText(Arena->BossName, FLinearColor(0.92f, 0.82f, 1.0f), (Canvas->SizeX - NW) * 0.5f, BY - NH - 6.0f, Medium, 1.0f);
	}
	if (const ADCCArcadeKart* Kart = Cast<ADCCArcadeKart>(Arena))
	{
		DrawRallyScreens(Kart); // its own work order, board, crew, truck and report (DCCHUD_RallyMenus.cpp)
		return;
	}
	if (const ADCCArcadeLobby* Lobby = Cast<ADCCArcadeLobby>(Arena))
	{
		DrawDenStrip(Lobby);
		return;
	}

	UFont* Large = GEngine->GetLargeFont();
	UFont* Small = GEngine->GetSmallFont();

	// The full box for the first minute of a game (2026-11-13: it was distracting
	// to keep). After that only alert lines - ones the arena starts with "!", like
	// rockfall and gust warnings - show, in a slim banner while they last.
	const bool bFull = Arena->KeepsStatusPanel() || GetWorld()->GetTimeSeconds() < 60.0f;
	TArray<FString> Lines;
	TArray<bool> IsAlert;
	for (const FString& Line : Arena->StatusLines)
	{
		const bool bAlert = Line.StartsWith(TEXT("!"));
		if (bFull || bAlert)
		{
			Lines.Add(bAlert ? Line.RightChop(1).TrimStart() : Line);
			IsAlert.Add(bAlert);
		}
	}
	if (!bFull)
	{
		if (Lines.IsEmpty())
		{
			return;
		}
		UFont* Medium = GEngine->GetMediumFont();
		float BW = 0.0f, BH = 0.0f;
		for (const FString& Line : Lines)
		{
			float LW = 0.0f;
			GetTextSize(Line, LW, BH, Medium, 1.0f);
			BW = FMath::Max(BW, LW);
		}
		const float Pulse = 0.75f + 0.25f * FMath::Sin(GetWorld()->GetRealTimeSeconds() * 5.0f);
		const float BX = (Canvas->SizeX - BW - 36.0f) * 0.5f, BY = 84.0f; // just under the health bar (2026-11-15)
		DrawRect(FLinearColor(0.02f, 0.015f, 0.04f, 0.78f), BX, BY, BW + 36.0f, Lines.Num() * (BH + 4.0f) + 14.0f);
		DrawRect(Arena->Accent * Pulse, BX, BY, BW + 36.0f, 3.0f);
		float LY = BY + 8.0f;
		for (const FString& Line : Lines)
		{
			float LW = 0.0f, LH = 0.0f;
			GetTextSize(Line, LW, LH, Medium, 1.0f);
			DrawText(Line, FLinearColor(1.0f, 0.85f, 0.55f), (Canvas->SizeX - LW) * 0.5f, LY, Medium, 1.0f);
			LY += LH + 4.0f;
		}
		return;
	}

	float W = 0.0f, H = 0.0f;
	GetTextSize(Arena->Title, W, H, Large, 1.0f);
	float PanelW = W;
	float LineH = 0.0f;
	for (const FString& Line : Lines)
	{
		float LW = 0.0f;
		GetTextSize(Line, LW, LineH, Small, 1.0f);
		PanelW = FMath::Max(PanelW, LW);
	}
	PanelW += 40.0f;
	const float PanelH = H + 16.0f + Lines.Num() * (LineH + 3.0f) + 12.0f;
	const float X = (Canvas->SizeX - PanelW) * 0.5f;
	const float Y = 84.0f; // just under the health bar and its number, not over them (2026-11-15)

	DrawRect(FLinearColor(0.02f, 0.015f, 0.04f, 0.82f), X, Y, PanelW, PanelH);
	DrawRect(Arena->Accent, X, Y, PanelW, 3.0f);
	DrawText(Arena->Title, Arena->Accent, (Canvas->SizeX - W) * 0.5f, Y + 8.0f, Large, 1.0f);
	float TY = Y + H + 14.0f;
	for (int32 i = 0; i < Lines.Num(); ++i)
	{
		float LW = 0.0f, LH = 0.0f;
		GetTextSize(Lines[i], LW, LH, Small, 1.0f);
		DrawText(Lines[i], IsAlert[i] ? FLinearColor(1.0f, 0.8f, 0.45f) : FLinearColor(0.93f, 0.9f, 0.86f), (Canvas->SizeX - LW) * 0.5f, TY, Small, 1.0f);
		TY += LH + 3.0f;
	}
}

void ADCCHUD::DrawMineReel()
{
	if (!Canvas || !GetWorld())
	{
		return;
	}
	const ADCCArcadeHaunt* Haunt = nullptr;
	for (TActorIterator<ADCCArcadeHaunt> It(GetWorld()); It; ++It)
	{
		Haunt = *It;
		break;
	}
	if (!Haunt || Haunt->ReelPhase == 0 || Haunt->ReelPhase == 3)
	{
		return;
	}
	UFont* Big = GEngine->GetMediumFont();
	UFont* Small = GEngine->GetSmallFont();
	const float W = Canvas->SizeX, H = Canvas->SizeY;
	const double Now = Haunt->NowServer();

	if (Haunt->ReelPhase == 1)
	{
		// DATA EXTRACTION: a bar over each head in the cage; TAPE LOST over anyone who did not make it.
		const float P = FMath::Clamp(float((Now - Haunt->ExtractStart) / ADCCArcadeHaunt::ExtractSeconds), 0.0f, 1.0f);
		float TW = 0, TH = 0;
		const FString Banner = TEXT("DATA EXTRACTION");
		GetTextSize(Banner, TW, TH, Big, 1.4f);
		DrawText(Banner, FLinearColor(0.4f, 1.0f, 0.55f), (W - TW) * 0.5f, H * 0.16f, Big, 1.4f);
		for (TActorIterator<ADCCCharacterBase> It(GetWorld()); It; ++It)
		{
			const ADCCCharacterBase* C = *It;
			const APlayerState* State = C->GetPlayerState();
			const FString Who = State ? State->GetPlayerName() : (bTrailerMinimal ? C->DisplayName.ToString() : FString());
			if (Who.IsEmpty() || C->IsA<ADCCEnemyCharacter>() || C->IsHidden())
			{
				continue;
			}
			const FVector Screen = Project(C->GetActorLocation() + FVector(0, 0, 150));
			if (Screen.Z <= 0.0f)
			{
				continue;
			}
			const bool bUp = Haunt->Uploaders.Contains(Who);
			const float BW = 150.0f, BX = Screen.X - BW * 0.5f, BY = Screen.Y - 30.0f;
			if (bUp)
			{
				DrawRect(FLinearColor(0, 0, 0, 0.7f), BX - 2, BY - 2, BW + 4, 14);
				DrawRect(FLinearColor(0.35f, 1.0f, 0.5f), BX, BY, BW * P, 10);
				DrawText(P >= 1.0f ? TEXT("UPLOADED") : FString::Printf(TEXT("EXTRACTING  %d%%"), FMath::RoundToInt(P * 100.0f)),
					FLinearColor(0.8f, 1.0f, 0.85f), BX, BY - 20.0f, Small, 1.0f);
			}
			else
			{
				DrawText(TEXT("TAPE LOST"), FLinearColor(1.0f, 0.3f, 0.25f), BX + 30.0f, BY - 10.0f, Small, 1.0f);
			}
		}
		return;
	}

	// THE SCROLLER: a phone in the middle of the screen, the post's video filling it, the feed's furniture over it.
	int32 Index = 0;
	double PlayTime = 0.0, Into = 0.0;
	DrawRect(FLinearColor(0.02f, 0.02f, 0.03f, 0.92f), 0, 0, W, H);
	if (!Haunt->CurrentClip(Index, PlayTime, Into))
	{
		return;
	}
	const FDCCReelClip& Post = Haunt->Reel[Index];
	const float PH = H * 0.9f, PW = PH * 9.0f / 16.0f;
	const float PX = (W - PW) * 0.5f, PY = (H - PH) * 0.5f;
	// The phone: a bezel, then the video.
	DrawRect(FLinearColor(0.08f, 0.08f, 0.1f), PX - 12, PY - 12, PW + 24, PH + 24);
	if (Haunt->ReelTargets.Num() > 0 && Haunt->ReelTargets[0] && Haunt->ReelTargets[0]->GetResource())
	{
		FCanvasTileItem Tile(FVector2D(PX, PY), Haunt->ReelTargets[0]->GetResource(), FVector2D(PW, PH), FLinearColor::White);
		Tile.BlendMode = SE_BLEND_Opaque;
		Canvas->DrawItem(Tile);
	}
	// Top: the app, the tabs.
	DrawRect(FLinearColor(0, 0, 0, 0.35f), PX, PY, PW, 54);
	DrawText(TEXT("Scroller"), FLinearColor(1, 1, 1), PX + 16, PY + 14, Big, 1.0f);
	DrawText(TEXT("Following   |   For You"), FLinearColor(1, 1, 1, 0.8f), PX + PW - 190, PY + 20, Small, 1.0f);
	// Trending badge.
	const FLinearColor RankColour = Post.Rank == 1 ? FLinearColor(1.0f, 0.8f, 0.2f) : Post.Rank == 2 ? FLinearColor(0.8f, 0.85f, 0.9f) : Post.Rank == 3 ? FLinearColor(0.85f, 0.55f, 0.3f) : FLinearColor(0.6f, 0.6f, 0.7f);
	DrawRect(RankColour * 0.9f, PX + 16, PY + 66, 150, 26);
	DrawText(Post.Rank == 1 ? TEXT("#1  TRENDING") : FString::Printf(TEXT("#%d  ON THE FEED"), Post.Rank), FLinearColor(0.05f, 0.05f, 0.08f), PX + 24, PY + 70, Small, 1.0f);
	// Right rail: the counts, ticking up while you watch.
	const float Grow = FMath::Clamp(float(Into / 3.0), 0.25f, 1.0f);
	auto Count = [&](const TCHAR* Label, int32 Value, float Y)
	{
		DrawRect(FLinearColor(0, 0, 0, 0.35f), PX + PW - 82, Y, 70, 58);
		DrawText(ADCCArcadeHaunt::CompactCount(FMath::RoundToInt(Value * Grow)), FLinearColor(1, 1, 1), PX + PW - 74, Y + 6, Big, 0.8f);
		DrawText(Label, FLinearColor(1, 1, 1, 0.7f), PX + PW - 74, Y + 36, Small, 0.9f);
	};
	Count(TEXT("likes"), Post.Likes, PY + PH * 0.42f);
	Count(TEXT("comments"), Post.Comments, PY + PH * 0.42f + 68);
	Count(TEXT("shares"), Post.Shares, PY + PH * 0.42f + 136);
	// A comment drifting up, now and then.
	static const TCHAR* Chatter[] = {
		TEXT("the way the lamp went BLUE"), TEXT("Mount Fourteener is NOT normal"), TEXT("ok who else heard three knocks"),
		TEXT("he's standing RIGHT THERE"), TEXT("the planks. the PLANKS"), TEXT("this is the most haunted hole in Colorado"),
		TEXT("Pip is my emotional support canary now"), TEXT("why is nobody talking about the cart"), TEXT("ratio'd by a ghost miner"),
		TEXT("not the Spookseekers flashback"), TEXT("someone get this crew better tanks"), TEXT("I paused at 0:03 and I'm not ok"),
	};
	// Comments load in one after another and stack up, newest at the bottom, older ones fading as they climb.
	{
		const float OnPost = float(FMath::Max(0.0, Now - Haunt->FeedSince));
		const float Every = 1.1f;
		const int32 Loaded = 1 + FMath::FloorToInt(OnPost / Every);
		const float Slide = FMath::Clamp(FMath::Fmod(OnPost, Every) / 0.25f, 0.0f, 1.0f);
		const int32 Shown = FMath::Min(Loaded, 5);
		for (int32 k = 0; k < Shown; ++k)
		{
			const int32 Nth = Loaded - 1 - k; // k = 0 is the newest
			const int32 Pick = (Index * 7 + Nth * 5) % UE_ARRAY_COUNT(Chatter);
			const float Y = PY + PH * 0.70f - k * 34.0f + (k == 0 ? (1.0f - Slide) * 24.0f : 0.0f);
			const float Alpha = (k == 0 ? Slide : 1.0f) * (1.0f - k * 0.17f);
			const FString Line = FString::Printf(TEXT("@viewer%d  %s"), 100 + (Pick * 37 + Nth * 11) % 900, Chatter[Pick]);
			float LW = 0, LH = 0;
			GetTextSize(Line, LW, LH, Small, 1.0f);
			DrawRect(FLinearColor(0, 0, 0, 0.45f * Alpha), PX + 12, Y - 3, FMath::Min(LW + 12, PW - 110), LH + 6);
			DrawText(Line, FLinearColor(1, 1, 1, 0.95f * Alpha), PX + 18, Y, Small, 1.0f);
		}
	}
	// Bottom: who posted it, the caption, the tags, and what the rank paid.
	DrawRect(FLinearColor(0, 0, 0, 0.45f), PX, PY + PH - 150, PW, 150);
	DrawText(FString::Printf(TEXT("@%s"), *Post.Cams[0]), FLinearColor(1, 1, 1), PX + 16, PY + PH - 140, Big, 1.0f);
	DrawText(Post.Title, FLinearColor(1.0f, 0.95f, 0.8f), PX + 16, PY + PH - 106, Small, 1.1f);
	DrawText(TEXT("#deadair  #mountfourteener  #luckytuesday  #found"), FLinearColor(0.6f, 0.85f, 1.0f), PX + 16, PY + PH - 80, Small, 1.0f);
	DrawText(FString::Printf(TEXT("+%d tickets   +%d credits for #%d"), Post.Tickets, Post.Credits, Post.Rank), FLinearColor(0.5f, 1.0f, 0.6f), PX + 16, PY + PH - 50, Small, 1.0f);
	// The progress of this post, like the thin bar on a phone video.
	const float Len = FMath::Max(0.5f, float(Post.To - Post.From));
	DrawRect(FLinearColor(1, 1, 1, 0.25f), PX, PY + PH - 4, PW, 4);
	DrawRect(FLinearColor(1, 1, 1, 0.9f), PX, PY + PH - 4, PW * FMath::Fmod(float(Into), Len) / Len, 4);
	// Beside the phone: where you are in the feed, and how to flick.
	DrawText(FString::Printf(TEXT("%d / %d"), Index + 1, Haunt->Reel.Num()), FLinearColor(1, 1, 1, 0.8f), PX + PW + 40, PY + PH * 0.5f - 20, Big, 1.0f);
	DrawText(TEXT("scroll or arrows to flick"), FLinearColor(1, 1, 1, 0.55f), PX + PW + 40, PY + PH * 0.5f + 16, Small, 1.0f);
}

void ADCCHUD::DrawMineNote()
{
	if (!Canvas || !GetWorld() || !GetWorld()->GetGameState())
	{
		return;
	}
	const ADCCArcadeHaunt* Haunt = nullptr;
	for (TActorIterator<ADCCArcadeHaunt> It(GetWorld()); It; ++It)
	{
		Haunt = *It;
		break;
	}
	const APlayerState* Me = PlayerOwner ? PlayerOwner->PlayerState : nullptr;
	const FDCCFootageEntry* Entry = Haunt && Me ? Haunt->FindEntry(Me->GetPlayerName()) : nullptr;
	const double Now = GetWorld()->GetGameState()->GetServerWorldTimeSeconds();

	// How filming works (2026-11-16, Zoe: "player's need an explanation of the camera system"): a card for the first
	// minute of a round, until you start recording.
	if (Haunt && Me && Haunt->ReelPhase == 0)
	{
		const ADCCCharacterBase* Self = PlayerOwner ? Cast<ADCCCharacterBase>(PlayerOwner->GetPawn()) : nullptr;
		const float Elapsed = ADCCArcadeHaunt::RoundSeconds - Haunt->TimeLeft;
		if (Self && !Self->bRecordingFootage && Elapsed < 60.0f && Haunt->TimeLeft > 0.0f)
		{
			const TCHAR* Lines[] = {
				TEXT("YOUR BODY CAM"),
				TEXT("Your belt (the number keys): BODY CAM starts and stops recording. A red light over your head means you are filming."),
				TEXT("What you film is what rates: ghosts, monsters, traps going off, and each other's suffering."),
				TEXT("Tape is limited, so film the good parts. Watch how much you have left."),
				TEXT("Also on the belt: your LAMP, and LIGHTSTICKS (press the number, then click to throw)."),
				TEXT("B: reactions, a photo pose, and the selfie camera that turns round to film your face."),
				TEXT("Get back inside the cage before time runs out, or your tape stays down here. Nothing hurts you in the cage."),
			};
			UFont* Small = GEngine->GetSmallFont();
			const float W = 640.0f, LH = 20.0f, X = (Canvas->SizeX - W) * 0.5f, Y = Canvas->SizeY * 0.58f;
			DrawRect(FLinearColor(0.0f, 0.0f, 0.0f, 0.62f), X - 14.0f, Y - 10.0f, W + 28.0f, LH * UE_ARRAY_COUNT(Lines) + 22.0f);
			for (int32 i = 0; i < UE_ARRAY_COUNT(Lines); ++i)
			{
				DrawText(Lines[i], i == 0 ? FLinearColor(1.0f, 0.35f, 0.3f) : FLinearColor(0.92f, 0.92f, 0.9f), X, Y + i * LH, Small, i == 0 ? 1.2f : 1.0f);
			}
		}
	}

	// Tape full: a banner that stays until the round ends, so nobody films on into nothing.
	if (Haunt && Entry && Haunt->ReelPhase == 0 && Entry->Tape <= 0.0f && Haunt->TimeLeft > 0.0f)
	{
		UFont* Medium = GEngine->GetMediumFont();
		const FString Banner = TEXT("TAPE FULL  -  get it back to the cage");
		float BW = 0.0f, BH = 0.0f;
		GetTextSize(Banner, BW, BH, Medium, 1.0f);
		const float BX = (Canvas->SizeX - BW) * 0.5f, BY = Canvas->SizeY * 0.12f + 90.0f;
		DrawRect(FLinearColor(0.35f, 0.02f, 0.02f, 0.75f), BX - 16.0f, BY - 6.0f, BW + 32.0f, BH + 12.0f);
		DrawText(Banner, FLinearColor(1.0f, 0.85f, 0.8f), BX, BY, Medium, 1.0f);
	}

	// The mine's small HUD (2026-11-15): the lamp, the canary, bad air, and the knockers' ledge.
	if (Haunt && Me)
	{
		const APawn* Pawn = PlayerOwner ? PlayerOwner->GetPawn() : nullptr;
		UFont* Tiny = GEngine->GetSmallFont();
		const bool bLampOff = Haunt->IsLampOff(Me->GetPlayerName());
		const float BX = 40.0f, BY = Canvas->SizeY - 170.0f;
		DrawText(bLampOff ? TEXT("LAMP OFF") : TEXT("LAMP ON"), bLampOff ? FLinearColor(0.6f, 0.6f, 0.65f) : FLinearColor(1.0f, 0.85f, 0.55f), BX, BY, Tiny, 1.0f);
		if (Pawn && Haunt->IsInFiredamp(Pawn->GetActorLocation()) && !bLampOff)
		{
			DrawText(TEXT("Your lamp is burning BLUE. Firedamp. Lamp off (on your belt), or get through fast."), FLinearColor(0.5f, 0.7f, 1.0f), BX, BY - 22.0f, Tiny, 1.0f);
		}
		// Depth, air and the kit you carry.
		const FDCCFootageEntry* Kit = Haunt->FindEntry(Me->GetPlayerName());
		if (Kit && Pawn)
		{
			const float Depth = Haunt->DepthAt(Pawn->GetActorLocation());
			const float Reach = DCCChopShop::AirReach(Kit->KitAir);
			const bool bDeep = Depth > Reach;
			DrawText(FString::Printf(TEXT("DEPTH %d%%   your tanks reach %d%%"), FMath::RoundToInt(Depth * 100.0f), FMath::Min(100, FMath::RoundToInt(Reach * 100.0f))),
				bDeep ? FLinearColor(1.0f, 0.55f, 0.4f) : FLinearColor(0.75f, 0.85f, 0.95f), BX, BY - 92.0f, Tiny, 1.0f);
			// The air gauge.
			const float GW = 180.0f;
			DrawRect(FLinearColor(0, 0, 0, 0.5f), BX, BY - 70.0f, GW, 10.0f);
			DrawRect(Kit->Air > 0.3f ? FLinearColor(0.4f, 0.8f, 1.0f) : FLinearColor(1.0f, 0.35f, 0.25f), BX, BY - 70.0f, GW * Kit->Air, 10.0f);
			DrawText(TEXT("AIR"), FLinearColor(0.8f, 0.9f, 1.0f), BX + GW + 8.0f, BY - 74.0f, Tiny, 1.0f);
			DrawText(FString::Printf(TEXT("LIGHTSTICKS %d"), Kit->Sticks), FLinearColor(0.5f, 1.0f, 0.55f), BX + 160.0f, BY, Tiny, 1.0f);
			if (Kit->KitGas >= 1)
			{
				const float Gas = Haunt->NearestGas(Pawn->GetActorLocation());
				DrawText(Gas <= 0.0f ? TEXT("GAS METER: FIREDAMP HERE") : Gas < 1200.0f ? FString::Printf(TEXT("GAS METER: firedamp %d m"), FMath::RoundToInt(Gas / 100.0f)) : FString(TEXT("GAS METER: clear")),
					Gas < 1200.0f ? FLinearColor(0.5f, 0.7f, 1.0f) : FLinearColor(0.6f, 0.7f, 0.8f), BX + 160.0f, BY + 20.0f, Tiny, 1.0f);
			}
		}
		const float Bad = Haunt->LocalBadAirSeconds;
		DrawText(Bad > 0.0f ? (Bad > 8.0f ? TEXT("CANARY: silent. Bad air is hurting you. Get out.") : TEXT("CANARY: gone quiet. Bad air."))
			: TEXT("CANARY: singing"), Bad > 0.0f ? FLinearColor(1.0f, 0.5f, 0.35f) : FLinearColor(1.0f, 0.9f, 0.4f), BX, BY + 20.0f, Tiny, 1.0f);
		if (Bad > 0.0f)
		{
			// The edges of the world going dark.
			const float A = FMath::Clamp(Bad / 10.0f, 0.15f, 0.55f);
			const float W = Canvas->SizeX, H = Canvas->SizeY;
			DrawRect(FLinearColor(0, 0, 0, A), 0, 0, W, H * 0.12f);
			DrawRect(FLinearColor(0, 0, 0, A), 0, H * 0.88f, W, H * 0.12f);
			DrawRect(FLinearColor(0, 0, 0, A), 0, 0, W * 0.1f, H);
			DrawRect(FLinearColor(0, 0, 0, A), W * 0.9f, 0, W * 0.1f, H);
		}
		if (Pawn && !Haunt->bCrustLeft && !Haunt->LedgeAt.IsZero() && FVector::Dist2D(Pawn->GetActorLocation(), Haunt->LedgeAt) < 260.0f)
		{
			DrawText(TEXT("A tin plate on a ledge, \"for the knockers\". Stand here a moment to leave something."), FLinearColor(0.9f, 0.85f, 0.7f), BX, BY - 44.0f, Tiny, 1.0f);
		}
	}

	if (!Entry || Entry->NoteText.IsEmpty() || Now > Entry->NoteUntil)
	{
		return;
	}
	// Fades in over the first half second and out over the last.
	const float Left = static_cast<float>(Entry->NoteUntil - Now);
	const float Alpha = FMath::Clamp(FMath::Min(Left, (10.0f - Left)) * 2.0f, 0.0f, 1.0f);

	UFont* Title = GEngine->GetMediumFont();
	UFont* Body = GEngine->GetSmallFont();
	const float W = FMath::Min(620.0f, Canvas->SizeX - 80.0f);
	const float Pad = 22.0f;

	// Word-wrap the note to the card.
	TArray<FString> Lines;
	{
		TArray<FString> Words;
		Entry->NoteText.ParseIntoArray(Words, TEXT(" "), true);
		FString Line;
		for (const FString& Word : Words)
		{
			const FString Try = Line.IsEmpty() ? Word : Line + TEXT(" ") + Word;
			float TW = 0, TH = 0;
			GetTextSize(Try, TW, TH, Body, 1.15f);
			if (TW > W - Pad * 2.0f && !Line.IsEmpty())
			{
				Lines.Add(Line);
				Line = Word;
			}
			else
			{
				Line = Try;
			}
		}
		if (!Line.IsEmpty())
		{
			Lines.Add(Line);
		}
	}
	const float LineH = 22.0f;
	const float H = Pad * 2.0f + 30.0f + Lines.Num() * LineH + 18.0f;
	const float X = (Canvas->SizeX - W) * 0.5f;
	const float Y = Canvas->SizeY - H - 150.0f;

	// Old paper: a warm card with a darker edge, slightly uneven.
	DrawRect(FLinearColor(0.12f, 0.09f, 0.05f, 0.85f * Alpha), X - 4, Y - 4, W + 8, H + 8);
	DrawRect(FLinearColor(0.78f, 0.7f, 0.52f, 0.96f * Alpha), X, Y, W, H);
	DrawRect(FLinearColor(0.68f, 0.6f, 0.43f, 0.6f * Alpha), X, Y + H - 10, W, 10);
	DrawText(Entry->NoteTitle.ToUpper(), FLinearColor(0.35f, 0.18f, 0.08f, Alpha), X + Pad, Y + Pad - 4, Title, 0.9f);
	for (int32 i = 0; i < Lines.Num(); ++i)
	{
		DrawText(Lines[i], FLinearColor(0.16f, 0.11f, 0.06f, Alpha), X + Pad, Y + Pad + 30.0f + i * LineH, Body, 1.15f);
	}
	DrawText(Entry->bRecording ? TEXT("read on camera  +footage") : TEXT("(not recording: V to film the next one)"),
		FLinearColor(0.35f, 0.25f, 0.15f, 0.8f * Alpha), X + Pad, Y + H - Pad - 4, Body, 0.9f);
}

void ADCCHUD::DrawBodyCam()
{
	const ADCCCharacterBase* Crawler = GetOwningCrawler();
	if (!Canvas || !GetWorld() || !Crawler || !Crawler->bRecordingFootage)
	{
		return;
	}
	const ADCCArcadeHaunt* Haunt = nullptr;
	for (TActorIterator<ADCCArcadeHaunt> It(GetWorld()); It; ++It)
	{
		Haunt = *It;
		break;
	}
	if (!Haunt)
	{
		return;
	}
	const APlayerState* Me = PlayerOwner ? PlayerOwner->PlayerState : nullptr;
	const FDCCFootageEntry* Entry = Me ? Haunt->FindEntry(Me->GetPlayerName()) : nullptr;

	const float SW = Canvas->SizeX, SH = Canvas->SizeY;
	const float Inset = 60.0f, Arm = 70.0f, Thick = 4.0f;
	const FLinearColor White(1.0f, 1.0f, 1.0f, 0.85f);

	// A faint green cast and the viewfinder corners.
	DrawRect(FLinearColor(0.1f, 0.3f, 0.12f, 0.08f), 0, 0, SW, SH);
	for (const FVector2D& Corner : { FVector2D(Inset, Inset), FVector2D(SW - Inset, Inset), FVector2D(Inset, SH - Inset), FVector2D(SW - Inset, SH - Inset) })
	{
		const float DX = Corner.X < SW * 0.5f ? 1.0f : -1.0f;
		const float DY = Corner.Y < SH * 0.5f ? 1.0f : -1.0f;
		DrawRect(White, DX > 0 ? Corner.X : Corner.X - Arm, Corner.Y - (DY > 0 ? 0 : Thick), Arm, Thick);
		DrawRect(White, Corner.X - (DX > 0 ? 0 : Thick), DY > 0 ? Corner.Y : Corner.Y - Arm, Thick, Arm);
	}
	// Centre cross.
	DrawRect(White, SW * 0.5f - 12, SH * 0.5f - 1, 24, 2);
	DrawRect(White, SW * 0.5f - 1, SH * 0.5f - 12, 2, 24);

	UFont* Medium = GEngine->GetMediumFont();
	UFont* Small = GEngine->GetSmallFont();
	const bool bBlink = FMath::Fmod(GetWorld()->GetRealTimeSeconds(), 1.0) < 0.6;
	if (bBlink)
	{
		DrawRect(FLinearColor(1.0f, 0.1f, 0.1f), Inset + 20, Inset + 22, 16, 16);
	}
	DrawText(TEXT("REC"), FLinearColor(1.0f, 0.2f, 0.2f), Inset + 44, Inset + 18, Medium, 1.0f);

	if (Entry)
	{
		const int32 Tape = FMath::CeilToInt(Entry->Tape);
		// Low tape turns the counter red and blinks it.
		const bool bLow = Entry->Tape <= 30.0f;
		if (!bLow || bBlink)
		{
			DrawText(FString::Printf(TEXT("TAPE %d:%02d"), Tape / 60, Tape % 60), bLow ? FLinearColor(1.0f, 0.3f, 0.25f) : White, SW - Inset - 150, Inset + 18, Medium, 1.0f);
		}
		if (bLow)
		{
			DrawText(TEXT("TAPE LOW"), FLinearColor(1.0f, 0.3f, 0.25f), SW - Inset - 150, Inset + 46, Small, 1.0f);
		}

		// How good this shot is, right now.
		const float Quality = FMath::Clamp(Entry->LiveShot / 40.0f, 0.0f, 1.0f);
		const float BarW = 260.0f, BarX = (SW - BarW) * 0.5f, BarY = SH - Inset - 40;
		DrawText(Quality > 0.6f ? TEXT("INCREDIBLE FOOTAGE") : Quality > 0.25f ? TEXT("GOOD SHOT") : Quality > 0.02f ? TEXT("something there...") : TEXT("nothing in frame"),
			White, BarX, BarY - 22, Small, 1.0f);
		DrawRect(FLinearColor(1, 1, 1, 0.15f), BarX, BarY, BarW, 10);
		DrawRect(FLinearColor::LerpUsingHSV(FLinearColor(1.0f, 0.8f, 0.2f), FLinearColor(0.3f, 1.0f, 0.4f), Quality), BarX, BarY, BarW * Quality, 10);
		DrawText(FString::Printf(TEXT("%s views on tape"), *FText::AsNumber(FMath::RoundToInt(Entry->Footage * ADCCArcadeHaunt::ViewsPerPoint)).ToString()),
			White, Inset + 20, SH - Inset - 40, Small, 1.0f);
	}
}

void ADCCHUD::OpenStarterKit()
{
	if (!PlayerOwner || (StarterKit && StarterKit->IsInViewport()))
	{
		return;
	}
	if (!StarterKit)
	{
		StarterKit = CreateWidget<UDCCStarterKitWidget>(PlayerOwner, UDCCStarterKitWidget::StaticClass());
	}
	if (StarterKit)
	{
		TWeakObjectPtr<ADCCHUD> Self(this);
		StarterKit->OnClosed = [Self]()
		{
			if (Self.IsValid() && Self->StarterKit)
			{
				Self->StarterKit->RemoveFromParent();
				Self->ApplyCursorForOpenPanels();
			}
		};
		StarterKit->AddToViewport(60);
		ApplyCursorForOpenPanels();
	}
}

void ADCCHUD::DrawNewPlayerGuide(ADCCCharacterBase* Crawler)
{
	// New players (2026-11-16, Zoe: "have them open their menu, loot boxes, and then equipment like with guided menu
	// flashing or some other method. A red dot over the menu button"): one step at a time, a pulsing red dot and a
	// card, until each is done. In the Den and the dungeon, never over a minigame.
	if (!Canvas || !Crawler || !GetWorld() || CharacterSelect)
	{
		return;
	}
	const ADCCArcadeArena* Arena = GetArcadeArena();
	if (Arena && !Arena->IsA<ADCCArcadeLobby>())
	{
		return;
	}
	const FString Key = Crawler->GetPlayerState() ? Crawler->GetPlayerState()->GetPlayerName() : FString();
	if (Key.IsEmpty())
	{
		return;
	}
	if (Key != GuideKey)
	{
		GuideKey = Key;
		GuideStep = 0;
		GConfig->GetInt(TEXT("DCCGuide"), *Key, GuideStep, GGameUserSettingsIni);
		GuideGear.Reset();
	}
	if (GuideStep >= 3)
	{
		return;
	}
	auto Advance = [this, &Key]()
	{
		++GuideStep;
		GConfig->SetInt(TEXT("DCCGuide"), *Key, GuideStep, GGameUserSettingsIni);
		GConfig->Flush(false, GGameUserSettingsIni);
	};
	// What is worn right now, as one string: the equipment step is done when it changes.
	FString Gear;
	if (Crawler->Equipment)
	{
		for (const EDCCEquipmentSlot Slot : { EDCCEquipmentSlot::MainHand, EDCCEquipmentSlot::OffHand, EDCCEquipmentSlot::Head, EDCCEquipmentSlot::Body, EDCCEquipmentSlot::Trinket })
		{
			Gear += Crawler->Equipment->GetEquippedId(Slot).ToString() + TEXT("|");
		}
	}
	if (GuideStep == 0 && bShowMenu) { Advance(); }
	else if (GuideStep == 1 && bLootScreenOpen) { Advance(); GuideGear = Gear; }
	else if (GuideStep == 2)
	{
		if (GuideGear.IsEmpty()) { GuideGear = Gear; }
		else if (Gear != GuideGear) { Advance(); }
	}
	if (GuideStep >= 3 || bLootScreenOpen || (bShowMenu && GuideStep != 2))
	{
		return;
	}

	static const TCHAR* Titles[] = { TEXT("Open your menu"), TEXT("Open your loot boxes"), TEXT("Equip something") };
	static const TCHAR* Lines[] = {
		TEXT("Press Tab. Everything about your crawler lives there."),
		TEXT("Press Tab, then click LOOT BOXES. Every box you have earned is waiting."),
		TEXT("Tab, then INVENTORY: click gear to wear it."),
	};
	UFont* Medium = GEngine->GetMediumFont();
	UFont* Small = GEngine->GetSmallFont();
	const float Pulse = 0.5f + 0.5f * FMath::Sin(GetWorld()->GetRealTimeSeconds() * 5.0f);
	const float W = 340.0f, H = 78.0f;
	const float X = Canvas->SizeX - W - 24.0f, Y = Canvas->SizeY * 0.34f;
	DrawRect(FLinearColor(0.02f, 0.02f, 0.05f, 0.82f), X, Y, W, H);
	DrawRect(FLinearColor(1.0f, 0.25f, 0.2f, 0.35f + 0.5f * Pulse), X, Y, 4.0f, H);
	DrawRect(FLinearColor(1.0f, 0.15f, 0.1f, 0.6f + 0.4f * Pulse), X + W - 22.0f, Y + 10.0f, 12.0f, 12.0f);
	DrawText(FString::Printf(TEXT("NEW  %d/3"), GuideStep + 1), FLinearColor(1.0f, 0.4f, 0.35f), X + 14.0f, Y + 8.0f, Small, 1.0f);
	DrawText(Titles[GuideStep], FLinearColor(1.0f, 0.95f, 0.85f), X + 14.0f, Y + 24.0f, Medium, 1.0f);
	DrawText(Lines[GuideStep], FLinearColor(0.8f, 0.8f, 0.85f), X + 14.0f, Y + 52.0f, Small, 1.0f);
}

// ---- SETTINGS tab (2026-11-16) ----------------------------------------------------------------------------------

void ADCCHUD::PollRemappableKeys()
{
	if (!PlayerOwner)
	{
		return;
	}
	// Setting a key: the next key pressed (not a mouse click, which is how you got here) becomes the binding.
	if (CapturingAction >= 0)
	{
		static TArray<FKey> AllKeys;
		if (AllKeys.IsEmpty())
		{
			EKeys::GetAllKeys(AllKeys);
			AllKeys.RemoveAll([](const FKey& K)
			{
				return !K.IsValid() || K.IsMouseButton() || K.IsAxis1D() || K.IsAxis2D() || K.IsTouch() || K.IsGamepadKey()
					|| K.IsModifierKey() || K == EKeys::AnyKey;
			});
		}
		for (const FKey& Key : AllKeys)
		{
			if (!PlayerOwner->WasInputKeyJustPressed(Key))
			{
				continue;
			}
			const uint8 Action = uint8(CapturingAction);
			CapturingAction = -1;
			if (Key == EKeys::Escape)
			{
				return;
			}
			const FString Taken = DCCControls::TakenBy(Key);
			const uint8 Other = DCCControls::BoundTo(Key, Action);
			if (!Taken.IsEmpty())
			{
				ShowNotice(FText::FromString(FString::Printf(TEXT("%s is already %s. Pick another key."), *Key.GetDisplayName().ToString(), *Taken)), 3.0f);
			}
			else if (Other != DCCControls::Count)
			{
				ShowNotice(FText::FromString(FString::Printf(TEXT("%s is already set to %s. Clear that one first."), *Key.GetDisplayName().ToString(), DCCControls::Name(Other))), 3.0f);
			}
			else
			{
				DCCControls::Set(Action, Key);
			}
			return;
		}
		return; // nothing else reads keys while one is being set
	}

	auto Pressed = [this](uint8 Action)
	{
		const FKey Key = DCCControls::Get(Action);
		return Key.IsValid() && PlayerOwner->WasInputKeyJustPressed(Key);
	};
	// On foot the spray can is a belt slot (2026-11-16); this key is for spraying from a truck.
	ADCCCharacterBase* Crawler = Cast<ADCCCharacterBase>(PlayerOwner->GetPawn());
	if (Pressed(DCCControls::Spray) && !Crawler)
	{
		DCCSpray::TryLocalSpray(PlayerOwner);
	}
	if (!Crawler)
	{
		return;
	}
	if (Pressed(DCCControls::Reactions))
	{
		Crawler->ToggleEmoteMenu();
	}
	for (uint8 n = 0; n < 8; ++n)
	{
		if (Pressed(uint8(DCCControls::QuickReact1 + n)))
		{
			Crawler->Server_RequestReaction(uint8(n + 1));
		}
	}
}

float ADCCHUD::DrawSettingsTab(float X, float Y, ADCCCharacterBase* Crawler)
{
	if (SettingsChoices.Num() != DCCReactions::MomentCount * 2)
	{
		SettingsChoices = DCCReactions::LoadChoices();
	}
	const float Row = 24.0f;
	// A raised button that is also a click target.
	auto Button = [this, Row](float BX, float BY, float W, const FString& Label, const FString& Command, bool bHot)
	{
		AddHitBox(FVector2D(BX, BY), FVector2D(W, Row - 4.0f), FName(*(FString(TEXT("Set|")) + Command)), true, 3);
		DrawRect(bHot ? FLinearColor(0.85f, 0.70f, 0.20f, 0.95f) : FLinearColor(0.42f, 0.44f, 0.52f, 0.6f), BX - 1.0f, BY - 1.0f, W + 2.0f, Row - 2.0f);
		DrawRect(bHot ? FLinearColor(0.22f, 0.18f, 0.06f, 0.95f) : FLinearColor(0.08f, 0.10f, 0.14f, 0.95f), BX, BY, W, Row - 4.0f);
		DrawText(Label, bHot ? FLinearColor(1.0f, 0.92f, 0.70f) : BodyColour, BX + 6.0f, BY + 1.0f);
	};

	DrawText(TEXT("KEYS"), HeadingColour, X, Y);
	Y += LineHeight;
	DrawText(TEXT("Click a key, then press the new one (Esc cancels)."), DimColour, X, Y);
	Y += LineHeight + 2.0f;
	for (uint8 a = 0; a < DCCControls::Count; ++a)
	{
		if (!DCCControls::IsLive(a))
		{
			continue;
		}
		const bool bSetting = CapturingAction == a;
		const FKey Key = DCCControls::Get(a);
		DrawText(DCCControls::Name(a), BodyColour, X, Y + 1.0f);
		Button(X + 300.0f, Y, 92.0f, bSetting ? FString(TEXT("press a key")) : Key.IsValid() ? Key.GetDisplayName().ToString() : FString(TEXT("-")),
			FString::Printf(TEXT("Key|%d"), a), bSetting);
		if (Key.IsValid() && a >= DCCControls::QuickReact1)
		{
			Button(X + 400.0f, Y, 24.0f, TEXT("x"), FString::Printf(TEXT("Clear|%d"), a), false);
		}
		Y += Row;
	}

	Y += SectionGap;
	DrawText(TEXT("REACTIONS"), HeadingColour, X, Y);
	Y += LineHeight;
	DrawText(TEXT("What pops over your head, and what you do, when..."), DimColour, X, Y);
	Y += LineHeight + 2.0f;
	const TArray<DCCReactions::FMove>& Moves = DCCReactions::Moves();
	for (uint8 m = 0; m < DCCReactions::MomentCount; ++m)
	{
		const uint8 Bubble = SettingsChoices[m * 2];
		const uint8 Move = SettingsChoices[m * 2 + 1];
		DrawText(DCCReactions::MomentName(m), BodyColour, X, Y + 1.0f);
		Button(X + 140.0f, Y, 120.0f, Bubble == DCCReactions::None ? FString(TEXT("no bubble")) : FString(TEXT("      ")) + DCCReactions::Info(Bubble).Name,
			FString::Printf(TEXT("Bub|%d"), m), false);
		if (UTexture2D* Icon = DCCReactions::Icon(Bubble))
		{
			DrawTexture(Icon, X + 143.0f, Y - 2.0f, 22.0f, 22.0f, 0.0f, 0.0f, 1.0f, 1.0f, FLinearColor::White, BLEND_Translucent);
		}
		Button(X + 268.0f, Y, 156.0f, Moves.IsValidIndex(Move) ? FString(Moves[Move].Name) : FString(TEXT("?")), FString::Printf(TEXT("Move|%d"), m), false);
		Y += Row;
	}
	Y += 6.0f;
	Button(X, Y, 150.0f, TEXT("Reset to defaults"), TEXT("Reset|0"), false);
	Y += Row;
	return Y;
}

void ADCCHUD::HandleSettingsClick(const FString& Command)
{
	FString Kind, Arg;
	if (!Command.Split(TEXT("|"), &Kind, &Arg))
	{
		Kind = Command;
	}
	const int32 Index = FCString::Atoi(*Arg);
	ADCCCharacterBase* Crawler = GetOwningCrawler();
	auto SendChoices = [this, Crawler]()
	{
		DCCReactions::SaveChoices(SettingsChoices);
		if (Crawler)
		{
			Crawler->Server_SetReactionChoices(SettingsChoices);
		}
	};
	if (Kind == TEXT("Key") && Index >= 0 && Index < DCCControls::Count)
	{
		CapturingAction = CapturingAction == Index ? -1 : Index;
	}
	else if (Kind == TEXT("Clear") && Index >= 0 && Index < DCCControls::Count)
	{
		DCCControls::Set(uint8(Index), FKey());
	}
	else if ((Kind == TEXT("Bub") || Kind == TEXT("Move")) && Index >= 0 && Index < DCCReactions::MomentCount
		&& SettingsChoices.Num() == DCCReactions::MomentCount * 2)
	{
		// Each click moves on to the next choice, round to "none" and back.
		const bool bBubble = Kind == TEXT("Bub");
		uint8& Value = SettingsChoices[Index * 2 + (bBubble ? 0 : 1)];
		const int32 Options = bBubble ? int32(DCCReactions::Count) : DCCReactions::Moves().Num();
		Value = uint8((Value + 1) % Options);
		SendChoices();
		// A preview of the bubble you just picked.
		if (Crawler && bBubble && Value != DCCReactions::None)
		{
			Crawler->Server_RequestReaction(Value);
		}
	}
	else if (Kind == TEXT("Reset"))
	{
		for (uint8 a = 0; a < DCCControls::Count; ++a)
		{
			DCCControls::Set(a, DCCControls::Default(a));
		}
		SettingsChoices.Reset();
		for (uint8 m = 0; m < DCCReactions::MomentCount; ++m)
		{
			const DCCReactions::FChoice Default = DCCReactions::DefaultFor(m);
			SettingsChoices.Add(Default.Bubble);
			SettingsChoices.Add(Default.Move);
		}
		SendChoices();
		ShowNotice(NSLOCTEXT("DCC", "SettingsReset", "Keys and reactions are back to how they started."));
	}
}

// ---- The Den's vote strip (2026-11-16) ---------------------------------------------------------------------------

void ADCCHUD::DrawDenStrip(const ADCCArcadeLobby* Lobby)
{
	if (!Lobby || !Canvas)
	{
		return;
	}
	const TArray<DCCArcade::FGame>& Games = DCCArcade::Games();
	const int32 N = Games.Num();
	const float Tile = 52.0f, Gap = 18.0f;
	const float Total = N * Tile + (N - 1) * Gap;
	const float X0 = (Canvas->SizeX - Total) * 0.5f;
	UFont* Small = GEngine->GetSmallFont();
	UFont* Medium = GEngine->GetMediumFont();
	const bool bCountdown = !Lobby->CountdownLine.IsEmpty();
	const float PanelH = Tile + 52.0f + (bCountdown ? 22.0f : 0.0f);
	// It comes down from off the top while anyone is on a pad (or a game is about to start) and rolls back up when
	// they all step off (2026-11-16).
	const bool bWanted = Lobby->IsSomeoneOnAPad() || bCountdown;
	DenStripShown = FMath::FInterpConstantTo(DenStripShown, bWanted ? 1.0f : 0.0f, GetWorld() ? GetWorld()->DeltaRealTimeSeconds : 0.016f, 3.0f);
	if (DenStripShown <= 0.0f)
	{
		return;
	}
	const float Eased = FMath::SmoothStep(0.0f, 1.0f, DenStripShown);
	const float Y0 = 112.0f - (112.0f + PanelH) * (1.0f - Eased); // 112: its resting place, under the health bar
	DrawRect(FLinearColor(0.02f, 0.015f, 0.04f, 0.62f), X0 - 18.0f, Y0 - 28.0f, Total + 36.0f, PanelH);
	DrawRect(Lobby->Accent, X0 - 18.0f, Y0 - 28.0f, Total + 36.0f, 2.0f);
	{
		const FString Head = TEXT("PICK THE NEXT GAME  -  stand on its pad");
		float W = 0.0f, H = 0.0f;
		GetTextSize(Head, W, H, Small, 1.0f);
		DrawText(Head, DimColour, (Canvas->SizeX - W) * 0.5f, Y0 - 22.0f, Small, 1.0f);
	}
	const float Pulse = 0.65f + 0.35f * FMath::Sin(GetWorld() ? GetWorld()->GetRealTimeSeconds() * 4.0f : 0.0f);
	for (int32 i = 0; i < N; ++i)
	{
		const DCCArcade::FGame& Game = Games[i];
		const float X = X0 + i * (Tile + Gap);
		const int32 Players = Lobby->PadVotes.IsValidIndex(i) ? Lobby->PadVotes[i] : 0;
		// The leading game glows.
		if (i == Lobby->LeadingPad)
		{
			DrawRect(Game.Colour * Pulse, X - 3.0f, Y0 - 3.0f, Tile + 6.0f, Tile + 6.0f);
		}
		if (UTexture2D* Logo = DCCGameLogos::Logo(Game.Id, Game.Colour))
		{
			DrawTexture(Logo, X, Y0, Tile, Tile, 0.0f, 0.0f, 1.0f, 1.0f, FLinearColor(1.0f, 1.0f, 1.0f, Players > 0 || Lobby->LeadingPad < 0 ? 1.0f : 0.55f), BLEND_Translucent);
		}
		// The players standing on its pad, right under it.
		const FString Count = FString::FromInt(Players);
		float W = 0.0f, H = 0.0f;
		GetTextSize(Count, W, H, Medium, 1.0f);
		DrawText(Count, Players > 0 ? FLinearColor(1.0f, 0.86f, 0.45f) : DimColour, X + (Tile - W) * 0.5f, Y0 + Tile + 3.0f, Medium, 1.0f);
	}
	if (bCountdown)
	{
		float W = 0.0f, H = 0.0f;
		GetTextSize(Lobby->CountdownLine, W, H, Small, 1.0f);
		DrawText(Lobby->CountdownLine, FLinearColor(1.0f, 0.86f, 0.45f), (Canvas->SizeX - W) * 0.5f, Y0 + Tile + 26.0f, Small, 1.0f);
	}
}
