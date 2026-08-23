#include "UI/DCCHUD.h"

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
#include "Engine/DataTable.h"
#include "Progression/DCCCrowdCrawler.h"
#include "Characters/DCCAllyCharacters.h"
#include "Characters/DCCEnemyCharacter_NuclearFamily.h"
#include "Characters/DCCEnemyCharacter_TestBoss.h"
#include "Characters/DCCEnemyCharacter_Voldemortgage.h"
#include "Characters/DCCEnemyCharacter_DebtEater.h"
#include "Quests/DCCQuestComponent.h"
#include "Quests/DCCQuestMarker.h"
#include "Combat/DCCGameState.h"
#include "UI/DCCEventLogTypes.h"
#include "UI/DCCChatInputWidget.h"
#include "Blueprint/UserWidget.h"
#include "Rooms/DCCClimbPoint.h"
#include "Engine/Canvas.h"
#include "Engine/Engine.h"
#include "EngineUtils.h"
#include "Components/CapsuleComponent.h"
#include "GameFramework/PlayerController.h"

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

void ADCCHUD::ToggleMenu()
{
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
	PlayerOwner->bShowMouseCursor = bShowMenu;
	PlayerOwner->bEnableClickEvents = bShowMenu;
	PlayerOwner->bEnableMouseOverEvents = bShowMenu;

	if (bShowMenu)
	{
		// GameAndUI rather than plain "show the cursor": without it the mouse
		// still drives the camera, so reaching for an achievement row would spin
		// the character on the spot. DoNotLock keeps the cursor free of the
		// viewport edge, and movement keys keep working, which is deliberate -
		// this is an overlay, not a modal screen that pauses the dungeon.
		FInputModeGameAndUI InputMode;
		InputMode.SetLockMouseToViewportBehavior(EMouseLockMode::DoNotLock);
		InputMode.SetHideCursorDuringCapture(false);
		PlayerOwner->SetInputMode(InputMode);
	}
	else
	{
		PlayerOwner->SetInputMode(FInputModeGameOnly());
	}
}

void ADCCHUD::NotifyHitBoxClick(FName BoxName)
{
	Super::NotifyHitBoxClick(BoxName);

	const FString AsString = BoxName.ToString();

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

void ADCCHUD::DrawHUD()
{
	Super::DrawHUD();

	if (!Canvas)
	{
		return;
	}

	CachedViewportHeight = Canvas->SizeY;

	ADCCCharacterBase* Crawler = GetOwningCrawler();

	// On by default: it is the only way to see what the dice did, and hiding it
	// behind a menu would defeat the point. Minimized/Hidden (2026-08-23, [K]
	// to cycle) are an explicit opt-out for exactly that reason - Pinned stays
	// the default and DrawFeed itself is unchanged.
	if (FeedVisibility == EDCCFeedVisibility::Pinned)
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
	DrawVitals(Crawler);
	DrawNearbyHealthBars(Crawler);
	DrawSkillTargetingOverlay(Crawler);

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

	DrawQuestTracker();
	DrawInteractionPrompt(Crawler);
	DrawStaminaBar(Crawler);

	// Always on, both of them: the hotbar is useless if it only appears in a menu,
	// and a conversation has to be readable while the world is still visible.
	DrawHotbar(Crawler);
	DrawInteractionMenu(Crawler);
	DrawLootMenu(Crawler);
	DrawInspectedGear(Crawler);
	DrawAdminPanel(Crawler);
	DrawNotice();

	if (!bShowMenu || !Crawler)
	{
		if (!bShowMenu)
		{
			DrawText(TEXT("[Tab] Character"), DimColour, 24.0f, 24.0f);
		}
		return;
	}

	// A dark backing panel, otherwise light text vanishes against the sky.
	DrawRect(FLinearColor(0.02f, 0.03f, 0.06f, 0.78f), 16.0f, 16.0f, 460.0f, Canvas->SizeY - 32.0f);

	float Y = 28.0f;
	const float X = 32.0f;

	Y = DrawMenuTabs(X, Y);
	Y += SectionGap;

	// Chatter is rolled fresh on the transition into the tab, not on a timer -
	// see ADCCGameState::RollChatterBatch. Compared against the tab drawn last
	// frame rather than a "have I ever opened this" flag, so leaving and
	// coming back also refreshes it.
	if (MenuTab == EDCCMenuTab::Chatter && PreviousDrawnMenuTab != EDCCMenuTab::Chatter && Crawler)
	{
		Crawler->Server_RequestChatterBatch();
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

	default:
		break;
	}
}

void ADCCHUD::ShowNotice(const FText& Message, float Seconds)
{
	NoticeText = Message;
	NoticeExpiryTime = GetWorld() ? GetWorld()->GetTimeSeconds() + Seconds : 0.0f;
}

void ADCCHUD::DrawNotice()
{
	if (NoticeText.IsEmpty() || !Canvas || !GetWorld()
		|| GetWorld()->GetTimeSeconds() > NoticeExpiryTime)
	{
		return;
	}

	const FString Line = NoticeText.ToString();

	float W = 0.0f, H = 0.0f;
	GetTextSize(Line, W, H, GEngine->GetMediumFont(), 1.0f);

	const float X = (Canvas->SizeX - W) * 0.5f;

	// Above centre, clear of the crosshair and the hotbar. Low enough to be in
	// the same glance as whatever the player was aiming at.
	const float Y = Canvas->SizeY * 0.38f;

	DrawRect(FLinearColor(0.02f, 0.03f, 0.06f, 0.78f), X - 16.0f, Y - 7.0f, W + 32.0f, H + 14.0f);
	DrawText(Line, FLinearColor(1.0f, 0.82f, 0.45f), X, Y, GEngine->GetMediumFont(), 1.0f);
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
	const bool bWantsCursor = bShowAdminPanel || bShowMenu;

	PlayerOwner->bShowMouseCursor = bWantsCursor;
	PlayerOwner->bEnableClickEvents = bWantsCursor;
	PlayerOwner->bEnableMouseOverEvents = bWantsCursor;

	if (bWantsCursor)
	{
		FInputModeGameAndUI InputMode;
		InputMode.SetLockMouseToViewportBehavior(EMouseLockMode::DoNotLock);
		InputMode.SetHideCursorDuringCapture(false);
		PlayerOwner->SetInputMode(InputMode);
	}
	else
	{
		PlayerOwner->SetInputMode(FInputModeGameOnly());
	}
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
	const int32 Next = ((static_cast<int32>(MenuTab) + Delta) % Count + Count) % Count;
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
		default:                        return TEXT("");
		}
	}
}

float ADCCHUD::DrawMenuTabs(float X, float Y)
{
	float TabX = X;

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

		// Registered every frame, like the achievement rows. The name is the tab
		// index so NotifyHitBoxClick can route it without a second lookup table.
		AddHitBox(FVector2D(TabX, Y - 2.0f), FVector2D(BoxW, LineHeight + 4.0f),
			FName(*FString::Printf(TEXT("Tab_%d"), Index)), /*bConsumesInput=*/true, /*Priority=*/2);

		if (bActive)
		{
			DrawRect(FLinearColor(0.16f, 0.18f, 0.26f, 0.9f), TabX, Y - 2.0f, BoxW, LineHeight + 4.0f);
		}

		DrawText(Label, bActive ? HeadingColour : DimColour, TabX + 9.0f, Y);
		TabX += BoxW + 4.0f;
	}

	Y += LineHeight + 6.0f;

	// A rule under the tabs, so the page below reads as belonging to one of them.
	DrawRect(FLinearColor(0.42f, 0.44f, 0.52f, 0.55f), X, Y, 420.0f, 1.0f);

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
	float Y = Canvas->SizeY - 132.0f;

	if (Stats->MaxManaPoints > 0)
	{
		DrawMeter(X, Y, BarW, BarH, Stats->GetManaFraction(), FLinearColor(0.30f, 0.55f, 0.95f),
			FString::Printf(TEXT("MP  %d / %d"), Stats->CurrentManaPoints, Stats->MaxManaPoints));
		Y += BarH + 8.0f;
	}

	DrawMeter(X, Y, BarW, BarH * 0.6f, Stats->GetExperienceFraction(), FLinearColor(0.85f, 0.70f, 0.25f),
		FString::Printf(TEXT("LVL %d   %d / %d XP"),
			Stats->Level, Stats->ExperiencePoints, Stats->ExperienceToNextLevel));
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
		Prompt = TEXT("Hold [LMB] to climb up, [RMB] to descend.  [Space] to let go.");
		Colour = BodyColour;
	}
	else if (const ADCCClimbPoint* Pending = Crawler->GetPendingClimbPoint())
	{
		// The question itself is authored on the climb point, so a particular
		// cliff can ask in its own voice.
		Prompt = FString::Printf(TEXT("%s   [Y] yes   [N] no"), *Pending->PromptText.ToString());
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

	// Hidden at full stamina unless it is actively being spent. A bar that is
	// always there while nothing is draining it just teaches players to ignore it.
	if (Fraction >= 0.999f && !Crawler->IsSprinting() && !Crawler->IsClimbing())
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
		DrawText(FString::Printf(TEXT("    %d. %s (%.0f kg)"),
			Index + 1, *Item.DisplayName.ToString(), Item.MassKilograms), BodyColour, X, Y);
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

	return Y;
}

void ADCCHUD::DrawHotbar(ADCCCharacterBase* Crawler)
{
	if (!Crawler || !Canvas)
	{
		return;
	}

	UDCCSkillComponent* SkillComp = Crawler->Skills;
	if (!SkillComp)
	{
		return;
	}

	const TArray<FDCCKnownSkill>& Known = SkillComp->KnownSkills;
	if (Known.IsEmpty())
	{
		return;
	}

	// Five, because that is how many keys are bound. Showing a sixth skill in a
	// bar whose key does not exist would be a lie.
	const int32 SlotCount = FMath::Min(Known.Num(), 5);

	constexpr float SlotW = 108.0f;
	constexpr float SlotH = 46.0f;
	constexpr float Gap = 6.0f;

	const float TotalW = SlotCount * SlotW + (SlotCount - 1) * Gap;
	const float StartX = (Canvas->SizeX - TotalW) * 0.5f;
	const float Y = Canvas->SizeY - SlotH - 22.0f;

	const FName Armed = Crawler->GetSelectedSkillId();

	for (int32 Index = 0; Index < SlotCount; ++Index)
	{
		const FDCCKnownSkill& Slot = Known[Index];
		const float SlotX = StartX + Index * (SlotW + Gap);

		FDCCSkillDef Def;
		const bool bFound = SkillComp->FindSkill(Slot.SkillId, Def);

		const bool bArmed = !Armed.IsNone() && Armed == Slot.SkillId;
		const float Cooldown = SkillComp->GetRemainingCooldown(Slot.SkillId);
		const bool bReady = Cooldown <= 0.0f;

		// Border first, then fill, so the armed slot reads as raised rather than
		// merely differently coloured.
		DrawRect(bArmed ? FLinearColor(0.85f, 0.70f, 0.20f, 0.95f)
						: FLinearColor(0.42f, 0.44f, 0.52f, 0.55f),
			SlotX - 2.0f, Y - 2.0f, SlotW + 4.0f, SlotH + 4.0f);

		DrawRect(FLinearColor(0.02f, 0.03f, 0.06f, 0.85f), SlotX, Y, SlotW, SlotH);

		// Cooldown drains from the bottom up, so a glance at the bar height says
		// how long is left without reading the number.
		if (!bReady && Def.CooldownSeconds > 0.0f)
		{
			const float Fraction = FMath::Clamp(Cooldown / Def.CooldownSeconds, 0.0f, 1.0f);
			DrawRect(FLinearColor(0.35f, 0.10f, 0.10f, 0.55f),
				SlotX, Y + SlotH * (1.0f - Fraction), SlotW, SlotH * Fraction);
		}

		DrawText(FString::Printf(TEXT("%d"), Index + 1), HeadingColour, SlotX + 5.0f, Y + 3.0f);

		const FString Name = bFound ? Def.DisplayName.ToString() : Slot.SkillId.ToString();
		DrawText(Name.Left(13), bReady ? BodyColour : DimColour, SlotX + 19.0f, Y + 3.0f);

		// Rank always; mana only when it costs any. A row of "0 mana" on melee
		// skills is noise that makes the real costs harder to spot.
		FString Detail = FString::Printf(TEXT("R%d"), Slot.Rank);
		if (bFound && Def.ManaCost > 0)
		{
			Detail += FString::Printf(TEXT("  %d mana"), Def.ManaCost);
		}
		if (!bReady)
		{
			Detail = FString::Printf(TEXT("%.0fs"), Cooldown);
		}

		DrawText(Detail, bReady ? DimColour : FLinearColor(0.90f, 0.55f, 0.45f), SlotX + 6.0f, Y + SlotH - 18.0f);
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

		DrawText(Shown[ShownIndex]->Text.ToString(),
			FeedEntryColour(*Shown[ShownIndex], Age, BodyColour, DimColour), X, Y);
		Y += LineHeight;
	}

	return Y;
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
		DrawText(Shown[ShownIndex]->Text.ToString(),
			FeedEntryColour(*Shown[ShownIndex], Age, BodyColour, DimColour), X, Y);
		Y += LineHeight;
	}

	return Y;
}

float ADCCHUD::DrawChannelMessages(float X, float Y, EDCCChatChannel Channel, ADCCCharacterBase* Crawler,
	int32 LineCount, const FString& TargetFilter)
{
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

		const FString Line = FString::Printf(TEXT("  %s: %s"), *Message.SenderName, *Message.Text.ToString());
		DrawText(Line, Message.bModerated ? DimColour : BodyColour, X, Y);
		Y += LineHeight;
	}

	return Y;
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
