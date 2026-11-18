#include "UI/DCCCosmeticShopWidget.h"
#include "EngineUtils.h"
#include "Arcade/DCCArcadeGames.h"

#include "Arcade/DCCChopShop.h"
#include "Arcade/DCCCosmetics.h"
#include "Arcade/DCCClimbItems.h"
#include "Arcade/DCCRallyItems.h"
#include "Arcade/DCCSpray.h"
#include "Arcade/DCCBounties.h"
#include "Session/DCCProfile.h"
#include "Arcade/DCCWalletComponent.h"
#include "Characters/DCCCharacterBase.h"
#include "Equipment/DCCEquipmentComponent.h"
#include "Equipment/DCCEquipmentTypes.h"
#include "Items/DCCInventoryComponent.h"
#include "UI/DCCMenuStyle.h"
#include "GameFramework/PlayerController.h"
#include "InputCoreTypes.h"
#include "Widgets/Images/SImage.h"
#include "Widgets/Input/SButton.h"
#include "Widgets/Layout/SBorder.h"
#include "Widgets/Layout/SBox.h"
#include "Widgets/Layout/SScrollBox.h"
#include "Widgets/Layout/SSpacer.h"
#include "Widgets/Layout/SWidgetSwitcher.h"
#include "Widgets/Layout/SWrapBox.h"
#include "Widgets/SBoxPanel.h"
#include "Widgets/SOverlay.h"
#include "Widgets/Text/STextBlock.h"

// The Den's counter. Rebuilt 2026-11-14 in the System's window style (UI/DCCMenuStyle), after Zoe: the menus were
// "still kind of basic", and "can you have a small image of the item accompany the name to the side or when you
// scroll over it?".
//
//   left    a grid of item tiles: a picture, the name, the price or a state tag
//   right   whatever you hover (or last clicked) at full size: big picture, what it is, what it does, and the one
//           button for it
//   tabs    PRIZES (cosmetics and the booth's guns, for tickets), TROPHIES (sell what came home), CHOP SHOP
//           (truck upgrades, for scrap)
// Both balances are always in the header.

using namespace DCCMenu;

namespace
{
	enum class EKind : uint8 { Cosmetic, Gun, Trophy, Chop, Supply, Spray, Bounty };

	struct FEntry
	{
		EKind Kind;
		FName Id;
		FString Name;
		FString Blurb;
		FString Caption;   // "HEAD", "RANGED", "ARTIFACT", "TRUCK"
		FLinearColor Colour;
		int32 Price = 0;   // tickets, scrap, or (trophies) what it sells for
		FName Slot;        // cosmetics
		int32 MaxLevel = 0;
		FString PerLevel;
	};

	const TArray<FEntry>& Entries(int32 Tab)
	{
		static TArray<FEntry> Lists[6];
		if (Lists[0].IsEmpty())
		{
			// The spray can first: it is the one prize that is yours in a way nothing else is (2026-11-16).
			Lists[0].Add({ EKind::Spray, DCCSpray::CanId, TEXT("Spray Can"),
				TEXT("Pick any picture from your PC. It is cut square and turned into a pixel stencil: your tag. It goes on your belt: press its number, then click whatever you are looking at (Z from a truck). Everyone sees it, and it stays until the host clears the walls. You can change the picture any time for free."),
				TEXT("YOUR TAG"), Pink, DCCSpray::Price });
			for (const DCCCosmetics::FItem& Item : DCCCosmetics::Catalog())
			{
				FEntry E{ EKind::Cosmetic, Item.Id, Item.Name, Item.Blurb, DCCCosmetics::SlotLabel(Item.Slot).ToUpper(),
					Item.Parts.IsEmpty() ? Cyan : Item.Parts[0].Colour.GetClamped(), Item.Price, Item.Slot };
				Lists[0].Add(E);
			}
			for (const DCCCosmetics::FBoothWeapon& Gun : DCCCosmetics::BoothWeapons())
			{
				Lists[0].Add({ EKind::Gun, Gun.Id, Gun.Name, Gun.Blurb, TEXT("RANGED"), Gold, Gun.Price });
			}
			for (const DCCClimbItems::FItem& Item : DCCClimbItems::All())
			{
				if (Item.Kind == DCCClimbItems::EKind::Artifact)
				{
					Lists[1].Add({ EKind::Trophy, Item.Id, Item.Name, Item.Blurb, TEXT("FOURTEENER ARTIFACT"), Item.Colour, Item.SellValue });
				}
			}
			for (const DCCRallyItems::FItem& Item : DCCRallyItems::All())
			{
				if (Item.Kind == DCCRallyItems::EKind::Seized)
				{
					Lists[1].Add({ EKind::Trophy, Item.Id, Item.Name, Item.Blurb, TEXT("SEIZED GOODS"), Item.Colour, Item.SellValue });
				}
			}
			for (const DCCChopShop::FUpgrade& Up : DCCChopShop::All())
			{
				FEntry E{ EKind::Chop, Up.Id, Up.Name, Up.Blurb, Up.bTickets ? TEXT("FOURTEENER GEAR") : Up.bMine ? TEXT("MINE OUTFITTERS") : TEXT("TRUCK UPGRADE"), Up.Colour, Up.BasePrice };
				E.MaxLevel = Up.MaxLevel;
				E.PerLevel = Up.PerLevel;
				Lists[Up.bTickets ? 4 : Up.bMine ? 3 : 2].Add(E);
			}
			// Fort Tiner's Trading Post: Fourteener kit for this round, for tickets.
			for (const DCCClimbItems::FItem& Item : DCCClimbItems::All())
			{
				if (const int32 Price = DCCClimbItems::SupplyPrice(Item.Id))
				{
					Lists[4].Add({ EKind::Supply, Item.Id, Item.Name, Item.Blurb,
						Item.Food > 0.0f ? TEXT("FOOD") : Item.Kind == DCCClimbItems::EKind::Boost ? TEXT("BOOST") : TEXT("TOOL"), Item.Colour, Price });
				}
			}
		}
		if (Lists[5].IsEmpty())
		{
			// Bounties (DCCBounties): cross-game goals, each paid once.
			for (const DCCBounties::FBounty& B : DCCBounties::All())
			{
				Lists[5].Add({ EKind::Bounty, B.Id, B.Name, B.Blurb, B.Where, B.Colour, B.Reward });
			}
		}
		return Lists[FMath::Clamp(Tab, 0, 5)];
	}

	FLinearColor TabColour(int32 Tab)
	{
		return Tab == 0 ? Gold : Tab == 1 ? Pink : Tab == 2 ? Cyan : Tab == 3 ? Mint : Tab == 4 ? FLinearColor(1.0f, 0.62f, 0.25f) : FLinearColor(0.78f, 0.58f, 1.0f);
	}
}

void UDCCCosmeticShopWidget::NativeOnInitialized()
{
	Super::NativeOnInitialized();
	SetIsFocusable(true);
}

UDCCWalletComponent* UDCCCosmeticShopWidget::GetWallet() const
{
	const APlayerController* PC = GetOwningPlayer();
	const ADCCCharacterBase* Crawler = PC ? Cast<ADCCCharacterBase>(PC->GetPawn()) : nullptr;
	return Crawler ? Crawler->Wallet.Get() : nullptr;
}

FReply UDCCCosmeticShopWidget::NativeOnKeyDown(const FGeometry& InGeometry, const FKeyEvent& InKeyEvent)
{
	if (InKeyEvent.GetKey() == EKeys::Escape && OnCloseRequested)
	{
		OnCloseRequested();
		return FReply::Handled();
	}
	return Super::NativeOnKeyDown(InGeometry, InKeyEvent);
}

TSharedRef<SWidget> UDCCCosmeticShopWidget::RebuildWidget()
{
	TWeakObjectPtr<UDCCCosmeticShopWidget> Self(this);
	auto Wallet = [Self]() -> UDCCWalletComponent* { return Self.IsValid() ? Self->GetWallet() : nullptr; };

	// Owns a booth gun (equipped, or in the bag).
	auto HasGun = [Wallet](FName Id)
	{
		const UDCCWalletComponent* W = Wallet();
		const ADCCCharacterBase* Crawler = W ? Cast<ADCCCharacterBase>(W->GetOwner()) : nullptr;
		if (W && W->Owned.Contains(Id))
		{
			return true; // owned on the profile, so it is never lost by swapping guns
		}
		if (!Crawler)
		{
			return false;
		}
		if (Crawler->Equipment && Crawler->Equipment->GetEquippedId(EDCCEquipmentSlot::MainHand) == Id)
		{
			return true;
		}
		if (Crawler->Inventory)
		{
			for (const FDCCInventoryItem& Row : Crawler->Inventory->Items)
			{
				if (Row.EquipmentItemId == Id)
				{
					return true;
				}
			}
		}
		return false;
	};

	// ---- what an entry's one button says, whether it can be pressed, and what it does -------------------
	auto InHands = [Wallet](FName Id)
	{
		const UDCCWalletComponent* W = Wallet();
		const ADCCCharacterBase* Crawler = W ? Cast<ADCCCharacterBase>(W->GetOwner()) : nullptr;
		return Crawler && Crawler->Equipment && Crawler->Equipment->GetEquippedId(EDCCEquipmentSlot::MainHand) == Id;
	};
	auto ActionText = [Wallet, HasGun, InHands](const FEntry& E) -> FText
	{
		const UDCCWalletComponent* W = Wallet();
		switch (E.Kind)
		{
		case EKind::Cosmetic:
			if (W && W->WornIn(E.Slot) == E.Id) { return NSLOCTEXT("DCC", "ShopTakeOff", "TAKE OFF"); }
			if (W && W->Owned.Contains(E.Id)) { return NSLOCTEXT("DCC", "ShopWear", "WEAR"); }
			return FText::Format(NSLOCTEXT("DCC", "ShopBuy", "BUY  {0}"), FText::AsNumber(E.Price));
		case EKind::Gun:
			return InHands(E.Id) ? NSLOCTEXT("DCC", "ShopGunHeld", "IN YOUR HANDS")
				: HasGun(E.Id) ? NSLOCTEXT("DCC", "ShopGunEquip", "EQUIP") : FText::Format(NSLOCTEXT("DCC", "ShopBuyGun", "BUY  {0}"), FText::AsNumber(E.Price));
		case EKind::Trophy:
			return E.Price > 0 ? FText::Format(NSLOCTEXT("DCC", "ShopSell", "SELL  +{0}"), FText::AsNumber(E.Price)) : NSLOCTEXT("DCC", "ShopKeepsake", "KEEPSAKE");
		case EKind::Supply:
			return FText::Format(NSLOCTEXT("DCC", "ShopBuySupply", "BUY  {0}"), FText::AsNumber(E.Price));
		case EKind::Bounty:
			return W && W->BountyPaid(E.Id) ? NSLOCTEXT("DCC", "BountyPaid", "CLAIMED")
				: W && W->BountyDone(E.Id) ? FText::Format(NSLOCTEXT("DCC", "BountyClaim", "CLAIM  +{0}"), FText::AsNumber(E.Price))
				: NSLOCTEXT("DCC", "BountyNotYet", "NOT YET");
		case EKind::Spray:
			return W && W->Owns(E.Id) ? NSLOCTEXT("DCC", "ShopSprayChange", "CHANGE PICTURE")
				: FText::Format(NSLOCTEXT("DCC", "ShopSprayBuy", "BUY  {0}  &  PICK A PICTURE"), FText::AsNumber(E.Price));
		case EKind::Chop:
		default:
		{
			const int32 Level = W ? W->ChopLevel(E.Id) : 0;
			const DCCChopShop::FUpgrade* Up = DCCChopShop::Find(E.Id);
			return !Up || Level >= Up->MaxLevel ? NSLOCTEXT("DCC", "ChopMaxed", "MAXED")
				: FText::Format(NSLOCTEXT("DCC", "ChopBuy", "UPGRADE  {0}"), FText::AsNumber(DCCChopShop::PriceFor(*Up, Level)));
		}
		}
	};
	auto CanAct = [Wallet, HasGun, InHands](const FEntry& E)
	{
		const UDCCWalletComponent* W = Wallet();
		if (!W) { return false; }
		switch (E.Kind)
		{
		case EKind::Cosmetic: return W->Owned.Contains(E.Id) || W->Tickets >= E.Price;
		case EKind::Gun: return HasGun(E.Id) ? !InHands(E.Id) : W->Tickets >= E.Price;
		case EKind::Trophy: return E.Price > 0 && W->TrophyCount(E.Id) > 0;
		case EKind::Supply: return W->Tickets >= E.Price;
		case EKind::Spray: return W->Owns(E.Id) || W->Tickets >= E.Price;
		case EKind::Bounty: return W->BountyDone(E.Id) && !W->BountyPaid(E.Id);
		case EKind::Chop:
		default:
		{
			const DCCChopShop::FUpgrade* Up = DCCChopShop::Find(E.Id);
			const int32 Level = W->ChopLevel(E.Id);
			return Up && Level < Up->MaxLevel && (Up->bTickets ? W->Tickets : Up->bMine ? W->MineCredits : W->Scrap) >= DCCChopShop::PriceFor(*Up, Level);
		}
		}
	};
	auto Act = [Wallet, Self](const FEntry& E)
	{
		UDCCWalletComponent* W = Wallet();
		if (!W) { return; }
		switch (E.Kind)
		{
		case EKind::Cosmetic:
			if (W->WornIn(E.Slot) == E.Id) { W->Server_TakeOff(E.Slot); }
			else if (W->Owned.Contains(E.Id)) { W->Server_Wear(E.Id); }
			else { W->Server_Buy(E.Id); }
			break;
		case EKind::Gun: W->Server_Buy(E.Id); break;
		case EKind::Trophy: W->Server_SellTrophy(E.Id); break;
		case EKind::Chop: W->Server_BuyChop(E.Id); break;
		case EKind::Supply: W->Server_BuySupply(E.Id); break;
		case EKind::Bounty: W->Server_ClaimBounty(E.Id); break;
		case EKind::Spray:
		{
			// The picture is picked here, on this PC; only then is the can bought (a cancelled pick costs nothing).
			const APlayerController* PC = Self.IsValid() ? Self->GetOwningPlayer() : nullptr;
			FString Error;
			if (DCCSpray::PickPicture(DCCProfile::KeyFor(PC ? PC->PlayerState : nullptr), Error))
			{
				W->Server_Buy(E.Id);
			}
			else if (!Error.IsEmpty())
			{
				if (const ADCCCharacterBase* Crawler = Cast<ADCCCharacterBase>(W->GetOwner()))
				{
					Crawler->ShowLocalNotice(FText::FromString(Error));
				}
			}
			break;
		}
		}
	};
	// The small tag in a tile's corner: worn / owned / how many / level.
	auto StateTag = [Wallet, HasGun](const FEntry& E) -> FText
	{
		const UDCCWalletComponent* W = Wallet();
		if (!W) { return FText::GetEmpty(); }
		switch (E.Kind)
		{
		case EKind::Cosmetic:
			return W->WornIn(E.Slot) == E.Id ? NSLOCTEXT("DCC", "TagWorn", "WEARING") : W->Owned.Contains(E.Id) ? NSLOCTEXT("DCC", "TagOwned", "OWNED") : FText::GetEmpty();
		case EKind::Gun: return HasGun(E.Id) ? NSLOCTEXT("DCC", "TagOwned", "OWNED") : FText::GetEmpty(); // EQUIP swaps
		case EKind::Trophy: return FText::Format(NSLOCTEXT("DCC", "TagCount", "x{0}"), FText::AsNumber(W->TrophyCount(E.Id)));
		case EKind::Supply: return FText::GetEmpty();
		case EKind::Spray: return W->Owns(E.Id) ? NSLOCTEXT("DCC", "TagOwned", "OWNED") : FText::GetEmpty();
		case EKind::Bounty: return W->BountyPaid(E.Id) ? NSLOCTEXT("DCC", "TagPaid", "PAID") : W->BountyDone(E.Id) ? NSLOCTEXT("DCC", "TagDone", "DONE") : FText::GetEmpty();
		case EKind::Chop:
		default:
		{
			const DCCChopShop::FUpgrade* Up = DCCChopShop::Find(E.Id);
			return FText::Format(NSLOCTEXT("DCC", "TagLevel", "LV {0}/{1}"), FText::AsNumber(W->ChopLevel(E.Id)), FText::AsNumber(Up ? Up->MaxLevel : 0));
		}
		}
	};
	// The price line under a tile's name.
	auto PriceLine = [](const FEntry& E) -> FText
	{
		switch (E.Kind)
		{
		case EKind::Cosmetic:
		case EKind::Bounty: return FText::Format(NSLOCTEXT("DCC", "PriceBounty", "+{0} TICKETS"), FText::AsNumber(E.Price));
		case EKind::Gun:
		case EKind::Spray:
		case EKind::Supply: return FText::Format(NSLOCTEXT("DCC", "PriceTickets", "{0} TICKETS"), FText::AsNumber(E.Price));
		case EKind::Trophy: return E.Price > 0 ? FText::Format(NSLOCTEXT("DCC", "PriceSells", "SELLS {0}"), FText::AsNumber(E.Price)) : NSLOCTEXT("DCC", "PriceKeep", "KEEPSAKE");
		case EKind::Chop:
		default: return FText::FromString(E.PerLevel).ToUpper();
		}
	};

	// The entry the right panel shows: hovered, else clicked, else the first one that is visible.
	auto Focused = [Self, Wallet]() -> const FEntry*
	{
		if (!Self.IsValid()) { return nullptr; }
		const TArray<FEntry>& List = Entries(Self->Tab);
		const UDCCWalletComponent* W = Wallet();
		auto Visible = [W](const FEntry& E) { return E.Kind != EKind::Trophy || (W && W->TrophyCount(E.Id) > 0); };
		for (const FName& Want : { Self->HoverId, Self->SelectedId[FMath::Clamp(Self->Tab, 0, 5)] })
		{
			if (const FEntry* E = List.FindByPredicate([Want, &Visible](const FEntry& X) { return X.Id == Want && Visible(X); }))
			{
				return E;
			}
		}
		return List.FindByPredicate(Visible);
	};

	// ---- the tile grids, one per tab -------------------------------------------------------------------------
	auto BuildGrid = [&](int32 Tab) -> TSharedRef<SWidget>
	{
		TSharedRef<SWrapBox> Grid = SNew(SWrapBox).UseAllottedSize(true).InnerSlotPadding(FVector2D(10.0f, 10.0f));
		for (const FEntry& Entry : Entries(Tab))
		{
			const FEntry E = Entry;
			Grid->AddSlot()
			[
				SNew(SBox).WidthOverride(150.0f).HeightOverride(184.0f)
				.Visibility_Lambda([Wallet, E]()
				{
					if (E.Kind != EKind::Trophy) { return EVisibility::Visible; }
					const UDCCWalletComponent* W = Wallet();
					return W && W->TrophyCount(E.Id) > 0 ? EVisibility::Visible : EVisibility::Collapsed;
				})
				[
					SNew(SButton)
					.IsFocusable(false)
					.ContentPadding(FMargin(0.0f))
					.ButtonStyle(CardStyle(E.Colour, false))
					.OnHovered_Lambda([Self, E]() { if (Self.IsValid()) { Self->HoverId = E.Id; } })
					.OnUnhovered_Lambda([Self, E]() { if (Self.IsValid() && Self->HoverId == E.Id) { Self->HoverId = NAME_None; } })
					.OnClicked_Lambda([Self, E]() { if (Self.IsValid()) { Self->SelectedId[Self->Tab] = E.Id; } return FReply::Handled(); })
					[
						SNew(SOverlay)
						+ SOverlay::Slot().Padding(FMargin(10.0f, 10.0f, 10.0f, 8.0f))
						[
							SNew(SVerticalBox)
							+ SVerticalBox::Slot().AutoHeight().HAlign(HAlign_Center)
							[
								E.Kind == EKind::Spray
									// The spray can's tile is your own tag, once you have one.
									? StaticCastSharedRef<SWidget>(SNew(SBox).WidthOverride(96.0f).HeightOverride(96.0f)
									[
										SNew(SOverlay)
										+ SOverlay::Slot()[ Icon(E.Id, 96.0f, E.Colour) ]
										+ SOverlay::Slot().Padding(10.0f)
										[
											SNew(SImage).Image_Lambda([Self]() -> const FSlateBrush*
											{
												const APlayerController* PC = Self.IsValid() ? Self->GetOwningPlayer() : nullptr;
												return PC ? DCCSpray::PreviewBrush(DCCProfile::KeyFor(PC->PlayerState)) : nullptr;
											})
										]
									])
									: Icon(E.Id, 96.0f, E.Colour)
							]
							+ SVerticalBox::Slot().AutoHeight().Padding(0.0f, 8.0f, 0.0f, 0.0f)
							[
								SNew(STextBlock).Text(FText::FromString(E.Name)).Font(Font(11, "Bold")).ColorAndOpacity(Ink)
								.AutoWrapText(true).Justification(ETextJustify::Center)
							]
							+ SVerticalBox::Slot().FillHeight(1.0f)[ SNew(SSpacer) ]
							+ SVerticalBox::Slot().AutoHeight().HAlign(HAlign_Center)
							[
								SNew(STextBlock).Text(PriceLine(E)).Font(Caps(7, 160)).ColorAndOpacity(WithAlpha(TabColour(Tab), 0.85f))
							]
						]
						// The selected tile keeps a ring in its colour.
						+ SOverlay::Slot()
						[
							SNew(SBorder).BorderImage(Rounded(FLinearColor::Transparent, 8.0f, E.Colour.GetClamped(), 2.0f))
							.Visibility_Lambda([Self, E]() { return Self.IsValid() && Self->SelectedId[FMath::Clamp(Self->Tab, 0, 5)] == E.Id ? EVisibility::HitTestInvisible : EVisibility::Collapsed; })
						]
						+ SOverlay::Slot().HAlign(HAlign_Right).VAlign(VAlign_Top).Padding(6.0f)
						[
							SNew(SBox)
							.Visibility_Lambda([StateTag, E]() { return StateTag(E).IsEmpty() ? EVisibility::Collapsed : EVisibility::HitTestInvisible; })
							[
								Tag(TAttribute<FText>::CreateLambda([StateTag, E]() { return StateTag(E); }), FSlateColor(Mint))
							]
						]
					]
				]
			];
		}
		TSharedRef<SVerticalBox> Body = SNew(SVerticalBox);
		if (Tab == 1)
		{
			Body->AddSlot().AutoHeight().Padding(0.0f, 0.0f, 0.0f, 10.0f)
			[
				SNew(STextBlock)
				.Text(NSLOCTEXT("DCC", "ShopVaultEmpty", "Nothing here yet. Fourteener hides artifacts on the mountain. Repo Rally has seized goods: finish the shift still carrying them."))
				.Font(Font(11)).ColorAndOpacity(Dim).AutoWrapText(true)
				.Visibility_Lambda([Wallet]() { const UDCCWalletComponent* W = Wallet(); return W && !W->TrophyIds.IsEmpty() ? EVisibility::Collapsed : EVisibility::Visible; })
			];
		}
		Body->AddSlot().AutoHeight()[ Grid ];
		return SNew(SScrollBox) + SScrollBox::Slot().Padding(0.0f, 0.0f, 10.0f, 0.0f)[ Body ];
	};

	// ---- the one action button: filled in the tab's colour when it can be pressed, an outline when it cannot ---
	TSharedRef<SWidgetSwitcher> ActionSwitch = SNew(SWidgetSwitcher)
		.WidgetIndex_Lambda([Self, Focused, CanAct]()
		{
			const FEntry* E = Focused();
			return (Self.IsValid() ? FMath::Clamp(Self->Tab, 0, 5) : 0) * 2 + (E && CanAct(*E) ? 1 : 0);
		});
	for (int32 Variant = 0; Variant < 12; ++Variant)
	{
		const bool bReady = (Variant % 2) == 1;
		ActionSwitch->AddSlot()
		[
			SNew(SButton)
			.IsFocusable(false)
			.HAlign(HAlign_Center)
			.ContentPadding(FMargin(18.0f, 12.0f))
			.ButtonStyle(ButtonStyle(TabColour(Variant / 2), bReady))
			.IsEnabled_Lambda([Focused, CanAct, Wallet]()
			{
				const FEntry* E = Focused();
				const UDCCWalletComponent* W = Wallet();
				// Owned cosmetics can always be put on or taken off; the rest need the money or the item.
				return E && W && (CanAct(*E) || (E->Kind == EKind::Cosmetic && W->Owned.Contains(E->Id)));
			})
			.OnClicked_Lambda([Focused, Act]() { if (const FEntry* E = Focused()) { Act(*E); } return FReply::Handled(); })
			[
				SNew(STextBlock).Font(Caps(12, 160))
				.Text_Lambda([Focused, ActionText]() { const FEntry* E = Focused(); return E ? ActionText(*E) : FText::GetEmpty(); })
				.ColorAndOpacity(bReady ? FSlateColor(FLinearColor(0.03f, 0.03f, 0.06f)) : FSlateColor(Ink))
			]
		];
	}

	// ---- the detail panel ----------------------------------------------------------------------------------
	TSharedRef<SWidget> Detail =
		SNew(SBorder).BorderImage(Rounded(Well, 8.0f, Faint, 1.0f)).Padding(FMargin(22.0f, 20.0f))
		.Visibility_Lambda([Focused]() { return Focused() ? EVisibility::Visible : EVisibility::Hidden; })
		[
			SNew(SVerticalBox)
			// The big picture, swapped to whatever is focused.
			+ SVerticalBox::Slot().AutoHeight().HAlign(HAlign_Center)
			[
				SNew(SBox).WidthOverride(200.0f).HeightOverride(200.0f)
				[
					SNew(SOverlay)
					+ SOverlay::Slot().Padding(12.0f)
					[
						SNew(SBorder)
						.BorderImage_Lambda([Focused]() { const FEntry* E = Focused(); return Rounded(WithAlpha(E ? E->Colour : Cyan, 0.1f), 88.0f, WithAlpha(E ? E->Colour : Cyan, 0.35f), 1.0f); })
					]
					+ SOverlay::Slot()
					[
						SNew(SImage).Image_Lambda([Focused, Self]() -> const FSlateBrush*
						{
							const FEntry* E = Focused();
							if (E && E->Kind == EKind::Spray && Self.IsValid() && Self->GetOwningPlayer())
							{
								// Your own tag, once you have picked one.
								if (const FSlateBrush* Mine = DCCSpray::PreviewBrush(DCCProfile::KeyFor(Self->GetOwningPlayer()->PlayerState)))
								{
									return Mine;
								}
							}
							return E ? IconBrush(E->Id) : nullptr;
						})
					]
				]
			]
			+ SVerticalBox::Slot().AutoHeight().Padding(0.0f, 14.0f, 0.0f, 0.0f)
			[
				SNew(STextBlock).Font(Caps(8, 220))
				.Text_Lambda([Focused]() { const FEntry* E = Focused(); return FText::FromString(E ? E->Caption : FString()); })
				.ColorAndOpacity_Lambda([Focused]() { const FEntry* E = Focused(); return FSlateColor(WithAlpha(E ? E->Colour : Cyan, 0.9f)); })
			]
			+ SVerticalBox::Slot().AutoHeight().Padding(0.0f, 2.0f, 0.0f, 0.0f)
			[
				SNew(STextBlock).Font(Font(20, "Light", 10)).ColorAndOpacity(Ink).AutoWrapText(true)
				.Text_Lambda([Focused]() { const FEntry* E = Focused(); return FText::FromString(E ? E->Name : FString(TEXT("Nothing to show"))); })
			]
			+ SVerticalBox::Slot().AutoHeight().Padding(0.0f, 10.0f, 0.0f, 10.0f)[ Rule() ]
			+ SVerticalBox::Slot().AutoHeight()
			[
				SNew(STextBlock).Font(Font(11)).ColorAndOpacity(Dim).AutoWrapText(true).LineHeightPercentage(1.15f)
				.Text_Lambda([Focused]() { const FEntry* E = Focused(); return FText::FromString(E ? E->Blurb : FString()); })
			]
			// Chop Shop: the level pips and what each level does.
			+ SVerticalBox::Slot().AutoHeight().Padding(0.0f, 12.0f, 0.0f, 0.0f)
			[
				SNew(SVerticalBox)
				.Visibility_Lambda([Focused]() { const FEntry* E = Focused(); return E && E->Kind == EKind::Chop ? EVisibility::Visible : EVisibility::Collapsed; })
				+ SVerticalBox::Slot().AutoHeight()
				[
					SNew(STextBlock).Font(Caps(8, 160)).ColorAndOpacity(Cyan)
					.Text_Lambda([Focused]() { const FEntry* E = Focused(); return FText::FromString(E ? FString::Printf(TEXT("%s PER LEVEL"), *E->PerLevel.ToUpper()) : FString()); })
				]
				+ SVerticalBox::Slot().AutoHeight().Padding(0.0f, 6.0f, 0.0f, 0.0f)
				[
					SNew(SHorizontalBox)
					+ SHorizontalBox::Slot().AutoWidth().Padding(0, 0, 5, 0)[ SNew(SBox).WidthOverride(40.0f).HeightOverride(6.0f)[ SNew(SBorder).BorderImage_Lambda([Focused, Wallet]() { const FEntry* E = Focused(); const UDCCWalletComponent* W = Wallet(); return Rounded(E && W && W->ChopLevel(E->Id) > 0 ? E->Colour : FLinearColor(1, 1, 1, 0.1f), 3.0f); }) ] ]
					+ SHorizontalBox::Slot().AutoWidth().Padding(0, 0, 5, 0)[ SNew(SBox).WidthOverride(40.0f).HeightOverride(6.0f)[ SNew(SBorder).BorderImage_Lambda([Focused, Wallet]() { const FEntry* E = Focused(); const UDCCWalletComponent* W = Wallet(); return Rounded(E && W && W->ChopLevel(E->Id) > 1 ? E->Colour : FLinearColor(1, 1, 1, 0.1f), 3.0f); }) ] ]
					+ SHorizontalBox::Slot().AutoWidth().Padding(0, 0, 5, 0)
					[
						SNew(SBox).WidthOverride(40.0f).HeightOverride(6.0f)
						.Visibility_Lambda([Focused]() { const FEntry* E = Focused(); return E && E->MaxLevel >= 3 ? EVisibility::Visible : EVisibility::Collapsed; })
						[ SNew(SBorder).BorderImage_Lambda([Focused, Wallet]() { const FEntry* E = Focused(); const UDCCWalletComponent* W = Wallet(); return Rounded(E && W && W->ChopLevel(E->Id) > 2 ? E->Colour : FLinearColor(1, 1, 1, 0.1f), 3.0f); }) ]
					]
				]
			]
			+ SVerticalBox::Slot().FillHeight(1.0f)[ SNew(SSpacer) ]
			// State, then the one button.
			+ SVerticalBox::Slot().AutoHeight().Padding(0.0f, 0.0f, 0.0f, 8.0f)
			[
				SNew(STextBlock).Font(Caps(8, 160)).ColorAndOpacity(Mint)
				.Text_Lambda([Focused, StateTag]() { const FEntry* E = Focused(); return E ? StateTag(*E) : FText::GetEmpty(); })
			]
			+ SVerticalBox::Slot().AutoHeight()
			[
				ActionSwitch
			]
			// The host can wipe every wall (spray tags are kept on the host's PC). Two clicks, so it is never an accident.
			+ SVerticalBox::Slot().AutoHeight().Padding(0.0f, 8.0f, 0.0f, 0.0f)
			[
				SNew(SButton)
				.IsFocusable(false)
				.HAlign(HAlign_Center)
				.ContentPadding(FMargin(14.0f, 7.0f))
				.ButtonStyle(ButtonStyle(Pink, false))
				.Visibility_Lambda([Focused, Self]()
				{
					const FEntry* E = Focused();
					const APlayerController* PC = Self.IsValid() ? Self->GetOwningPlayer() : nullptr;
					const bool bHost = PC && PC->GetWorld() && PC->GetWorld()->GetNetMode() != NM_Client;
					return E && E->Kind == EKind::Spray && bHost ? EVisibility::Visible : EVisibility::Collapsed;
				})
				.OnClicked_Lambda([Self]()
				{
					if (!Self.IsValid()) { return FReply::Handled(); }
					const double Now = FPlatformTime::Seconds();
					if (Now - Self->ClearArmedAt < 5.0)
					{
						Self->ClearArmedAt = 0.0;
						DCCSpray::ClearAll(Self->GetWorld());
						if (const APlayerController* PC = Self->GetOwningPlayer())
						{
							if (const ADCCCharacterBase* Crawler = Cast<ADCCCharacterBase>(PC->GetPawn()))
							{
								Crawler->ShowLocalNotice(NSLOCTEXT("DCC", "SprayCleared", "Every spray tag is gone, in every game."));
							}
						}
					}
					else
					{
						Self->ClearArmedAt = Now;
					}
					return FReply::Handled();
				})
				[
					SNew(STextBlock).Font(Caps(9, 160)).ColorAndOpacity(FSlateColor(Pink))
					.Text_Lambda([Self]()
					{
						return Self.IsValid() && FPlatformTime::Seconds() - Self->ClearArmedAt < 5.0
							? NSLOCTEXT("DCC", "SprayClearSure", "CLICK AGAIN TO CLEAR EVERY TAG")
							: NSLOCTEXT("DCC", "SprayClear", "CLEAR ALL SPRAYS (HOST)");
					})
				]
			]
		];

	// ---- tabs ----------------------------------------------------------------------------------------------
	auto TabButton = [Self](int32 Index, const FText& Label) -> TSharedRef<SWidget>
	{
		auto Variant = [Self, Index, Label](bool bActive)
		{
			return SNew(SButton)
				.IsFocusable(false)
				.ContentPadding(FMargin(20.0f, 8.0f))
				.ButtonStyle(ButtonStyle(TabColour(Index), bActive))
				.OnClicked_Lambda([Self, Index]() { if (Self.IsValid()) { Self->Tab = Index; Self->HoverId = NAME_None; } return FReply::Handled(); })
				[
					SNew(STextBlock).Text(Label).Font(Caps(10, 200))
					.ColorAndOpacity(bActive ? FSlateColor(FLinearColor(0.03f, 0.03f, 0.06f)) : FSlateColor(FMath::Lerp(TabColour(Index), FLinearColor::White, 0.3f)))
				];
		};
		return SNew(SWidgetSwitcher)
			.WidgetIndex_Lambda([Self, Index]() { return Self.IsValid() && Self->Tab == Index ? 1 : 0; })
			+ SWidgetSwitcher::Slot()[ Variant(false) ]
			+ SWidgetSwitcher::Slot()[ Variant(true) ];
	};

	TSharedRef<SWidget> Content =
		SNew(SVerticalBox)
		+ SVerticalBox::Slot().AutoHeight()
		[
			SNew(SHorizontalBox)
			+ SHorizontalBox::Slot().FillWidth(1.0f).VAlign(VAlign_Center)
			[
				Header(NSLOCTEXT("DCC", "DenShopKicker", "SYSTEM  /  THE SAFE ROOM"),
					TAttribute<FText>::CreateLambda([Self]() { return Self.IsValid() && Self->Tab == 2 ? NSLOCTEXT("DCC", "ChopTitle", "THE CHOP SHOP") : Self.IsValid() && Self->Tab == 3 ? NSLOCTEXT("DCC", "MineTitle", "MINE OUTFITTERS") : Self.IsValid() && Self->Tab == 4 ? NSLOCTEXT("DCC", "TradingTitle", "TRADING POST") : Self.IsValid() && Self->Tab == 5 ? NSLOCTEXT("DCC", "BountyTitle", "BOUNTY BOARD") : NSLOCTEXT("DCC", "DenShopTitle", "PRIZE COUNTER"); }),
					TAttribute<FSlateColor>::CreateLambda([Self]() { return FSlateColor(Self.IsValid() ? TabColour(Self->Tab) : Gold); }))
			]
			+ SHorizontalBox::Slot().AutoWidth().VAlign(VAlign_Center).Padding(0.0f, 0.0f, 10.0f, 0.0f)
			[
				Balance(TAttribute<FText>::CreateLambda([Wallet]() { const UDCCWalletComponent* W = Wallet(); return FText::AsNumber(W ? W->Tickets : 0); }),
					NSLOCTEXT("DCC", "DenShopTickets", "TICKETS"), Gold)
			]
			+ SHorizontalBox::Slot().AutoWidth().VAlign(VAlign_Center)
			[
				Balance(TAttribute<FText>::CreateLambda([Wallet]() { const UDCCWalletComponent* W = Wallet(); return FText::AsNumber(W ? W->Scrap : 0); }),
					NSLOCTEXT("DCC", "DenShopScrap", "SCRAP"), Cyan)
			]
			+ SHorizontalBox::Slot().AutoWidth().VAlign(VAlign_Center).Padding(10.0f, 0.0f, 0.0f, 0.0f)
			[
				Balance(TAttribute<FText>::CreateLambda([Wallet]() { const UDCCWalletComponent* W = Wallet(); return FText::AsNumber(W ? W->MineCredits : 0); }),
					NSLOCTEXT("DCC", "DenShopCredits", "CREDITS"), Mint)
			]
		]
		+ SVerticalBox::Slot().AutoHeight().Padding(0.0f, 16.0f, 0.0f, 0.0f)
		[
			SNew(SHorizontalBox)
			+ SHorizontalBox::Slot().AutoWidth().Padding(0.0f, 0.0f, 8.0f, 0.0f)[ TabButton(0, NSLOCTEXT("DCC", "TabPrizes", "PRIZES")) ]
			+ SHorizontalBox::Slot().AutoWidth().Padding(0.0f, 0.0f, 8.0f, 0.0f)[ TabButton(1, NSLOCTEXT("DCC", "TabTrophies", "TROPHIES")) ]
			+ SHorizontalBox::Slot().AutoWidth().Padding(0.0f, 0.0f, 8.0f, 0.0f)[ TabButton(2, NSLOCTEXT("DCC", "TabChop", "CHOP SHOP")) ]
			+ SHorizontalBox::Slot().AutoWidth().Padding(0.0f, 0.0f, 8.0f, 0.0f)[ TabButton(3, NSLOCTEXT("DCC", "TabMine", "OUTFITTERS")) ]
			+ SHorizontalBox::Slot().AutoWidth().Padding(0.0f, 0.0f, 8.0f, 0.0f)[ TabButton(5, NSLOCTEXT("DCC", "TabBounties", "BOUNTIES")) ]
			// Only in a Fourteener round: it sells kit for that round.
			+ SHorizontalBox::Slot().AutoWidth()
			[
				SNew(SBox).Visibility_Lambda([Self]()
				{
					const UWorld* World = Self.IsValid() ? Self->GetWorld() : nullptr;
					if (World) { for (TActorIterator<ADCCArcadeClimb> It(World); It; ++It) { return EVisibility::Visible; } }
					return EVisibility::Collapsed;
				})
				[ TabButton(4, NSLOCTEXT("DCC", "TabTrading", "TRADING POST")) ]
			]
			+ SHorizontalBox::Slot().FillWidth(1.0f).VAlign(VAlign_Center).HAlign(HAlign_Right)
			[
				SNew(STextBlock).Font(Font(10)).ColorAndOpacity(Dim)
				.Text_Lambda([Self]()
				{
					return !Self.IsValid() || Self->Tab == 0 ? NSLOCTEXT("DCC", "TabPrizesNote", "Tickets from every game. Hover anything to see it.")
						: Self->Tab == 1 ? NSLOCTEXT("DCC", "TabTrophiesNote", "What you brought home. Sell what is worth tickets.")
						: Self->Tab == 2 ? NSLOCTEXT("DCC", "TabChopNote", "Scrap from Repo Rally and Neon Mile. Applies next shift.")
						: Self->Tab == 3 ? NSLOCTEXT("DCC", "TabMineNote", "Credits from Dead Air. Kit to go deeper into Mount Fourteener.")
						: Self->Tab == 5 ? NSLOCTEXT("DCC", "TabBountiesNote", "Goals across the games. Each one pays once.")
						: NSLOCTEXT("DCC", "TabTradingNote", "Fort Tiner. Tickets for kit you carry up this round.");
				})
			]
		]
		+ SVerticalBox::Slot().AutoHeight().Padding(0.0f, 14.0f, 0.0f, 14.0f)[ Rule(WithAlpha(Cyan, 0.18f)) ]
		+ SVerticalBox::Slot().FillHeight(1.0f)
		[
			SNew(SHorizontalBox)
			+ SHorizontalBox::Slot().FillWidth(1.0f)
			[
				SNew(SWidgetSwitcher)
				.WidgetIndex_Lambda([Self]() { return Self.IsValid() ? Self->Tab : 0; })
				+ SWidgetSwitcher::Slot()[ BuildGrid(0) ]
				+ SWidgetSwitcher::Slot()[ BuildGrid(1) ]
				+ SWidgetSwitcher::Slot()[ BuildGrid(2) ]
				+ SWidgetSwitcher::Slot()[ BuildGrid(3) ]
				+ SWidgetSwitcher::Slot()[ BuildGrid(4) ]
				+ SWidgetSwitcher::Slot()[ BuildGrid(5) ]
			]
			+ SHorizontalBox::Slot().AutoWidth().Padding(16.0f, 0.0f, 0.0f, 0.0f)
			[
				SNew(SBox).WidthOverride(330.0f)[ Detail ]
			]
		]
		+ SVerticalBox::Slot().AutoHeight().Padding(0.0f, 14.0f, 0.0f, 0.0f)
		[
			SNew(SHorizontalBox)
			+ SHorizontalBox::Slot().FillWidth(1.0f).VAlign(VAlign_Center)
			[
				SNew(STextBlock).Font(Font(11)).ColorAndOpacity(Mint).AutoWrapText(true)
				.Text_Lambda([Wallet]() { const UDCCWalletComponent* W = Wallet(); return FText::FromString(W ? W->ShopMessage : FString()); })
			]
			+ SHorizontalBox::Slot().AutoWidth().Padding(12.0f, 0.0f, 0.0f, 0.0f)
			[
				Button(NSLOCTEXT("DCC", "DenShopClose", "DONE"), Ink, FOnClicked::CreateLambda([Self]()
				{
					if (Self.IsValid() && Self->OnCloseRequested) { Self->OnCloseRequested(); }
					return FReply::Handled();
				}), false, true, 11, FMargin(28.0f, 9.0f))
			]
		];

	return
		SNew(SBox).HAlign(HAlign_Center).VAlign(VAlign_Center)
		[
			SNew(SBox).WidthOverride(1120.0f).HeightOverride(760.0f)
			[
				Window(Cyan, Content, FMargin(32.0f, 26.0f))
			]
		];
}
