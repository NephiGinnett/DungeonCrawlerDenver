#include "UI/DCCShopWidget.h"

#include "UI/DCCHUD.h"
#include "UI/DCCItemVisuals.h"
#include "Characters/DCCCharacterBase.h"
#include "Dialogue/DCCVendorNPC.h"
#include "Equipment/DCCEquipmentComponent.h"
#include "Equipment/DCCEquipmentTypes.h"
#include "Items/DCCConsumableTypes.h"
#include "Items/DCCInventoryComponent.h"
#include "Skills/DCCSkillComponent.h"
#include "Stats/DCCStatsComponent.h"
#include "GameFramework/PlayerController.h"
#include "Styling/CoreStyle.h"
#include "Widgets/Input/SButton.h"
#include "Widgets/Layout/SBorder.h"
#include "Widgets/Layout/SBox.h"
#include "Widgets/Layout/SScrollBox.h"
#include "Widgets/SBoxPanel.h"
#include "Widgets/Text/STextBlock.h"

namespace
{
	const FLinearColor Gold(1.0f, 0.83f, 0.42f);
	const FLinearColor Ink(0.94f, 0.91f, 0.86f);
	const FLinearColor Dim(0.62f, 0.60f, 0.66f);
	const FLinearColor Good(0.45f, 0.95f, 0.45f);
	const FLinearColor Bad(1.0f, 0.45f, 0.40f);

	FSlateFontInfo Font(int32 Size, bool bBold = false)
	{
		return FCoreStyle::GetDefaultFontStyle(bBold ? "Bold" : "Regular", Size);
	}

	FString Pretty(FName SkillId)
	{
		// "ExplosivesHandling" -> "Explosives Handling"
		return FName::NameToDisplayString(SkillId.ToString(), false);
	}

	int32 Score(const FDCCEquipmentDef& Def)
	{
		return Def.BonusStrength + Def.BonusIntelligence + Def.BonusConstitution + Def.BonusDexterity + Def.BonusCharisma
			+ Def.WeaponDamageDie / 2 + Def.EvadeBonus * 2;
	}

	FString StatsLine(const FDCCEquipmentDef& Def)
	{
		TArray<FString> Parts;
		if (Def.WeaponDamageDie > 0)
		{
			Parts.Add(FString::Printf(TEXT("1d%d %s"), Def.WeaponDamageDie, *UEnum::GetDisplayValueAsText(Def.WeaponDamageType).ToString()));
		}
		const TPair<int32, const TCHAR*> Bonuses[] = {
			{ Def.BonusStrength, TEXT("STR") }, { Def.BonusIntelligence, TEXT("INT") }, { Def.BonusConstitution, TEXT("CON") },
			{ Def.BonusDexterity, TEXT("DEX") }, { Def.BonusCharisma, TEXT("CHA") }
		};
		for (const TPair<int32, const TCHAR*>& B : Bonuses)
		{
			if (B.Key)
			{
				Parts.Add(FString::Printf(TEXT("%+d %s"), B.Key, B.Value));
			}
		}
		if (Def.EvadeBonus)
		{
			Parts.Add(FString::Printf(TEXT("%+d Evade"), Def.EvadeBonus));
		}
		if (Def.AttackRangeOverride > 0.0f && Def.SplashRadius <= 0.0f)
		{
			Parts.Add(FString::Printf(TEXT("reach %.0f m"), Def.AttackRangeOverride / 100.0f));
		}
		if (Def.bIsCursed)
		{
			Parts.Add(TEXT("CURSED"));
		}
		return FString::Join(Parts, TEXT("   "));
	}

	/** Which skill this uses, and what that means for the crawler holding the receipt. */
	FString SkillLine(const ADCCCharacterBase* Crawler, FName SkillId, const TCHAR* Role)
	{
		if (SkillId.IsNone())
		{
			return FString();
		}
		const int32 Rank = Crawler && Crawler->Skills ? Crawler->Skills->GetSkillRank(SkillId) : 0;
		return Rank > 0
			? FString::Printf(TEXT("%s: %s, your Rank %d%s"), Role, *Pretty(SkillId), Rank, Rank >= 5 ? TEXT(" (Rank 5 bonus die)") : TEXT(""))
			: FString::Printf(TEXT("%s: %s, untrained (Disadvantage; using it teaches it)"), Role, *Pretty(SkillId));
	}

	TSharedRef<SWidget> Card(const FLinearColor& Rarity, const FString& Name, const TArray<TPair<FString, FLinearColor>>& Lines,
		const FString& Price, const FText& ButtonText, TFunction<bool()> CanPress, TFunction<void()> OnPress)
	{
		TSharedRef<SVerticalBox> Text = SNew(SVerticalBox)
			+ SVerticalBox::Slot().AutoHeight()
			[
				SNew(STextBlock).Text(FText::FromString(Name)).Font(Font(13, true)).ColorAndOpacity(Rarity * 0.7f + FLinearColor::White * 0.3f)
			];
		for (const TPair<FString, FLinearColor>& Line : Lines)
		{
			if (!Line.Key.IsEmpty())
			{
				Text->AddSlot().AutoHeight().Padding(0, 1, 0, 0)
				[
					SNew(STextBlock).Text(FText::FromString(Line.Key)).Font(Font(10)).ColorAndOpacity(Line.Value).AutoWrapText(true)
				];
			}
		}

		return SNew(SBox).Padding(FMargin(0, 0, 0, 6))
			[
				SNew(SBorder)
				.BorderImage(FCoreStyle::Get().GetBrush("WhiteBrush"))
				.BorderBackgroundColor(FLinearColor(0.07f, 0.06f, 0.10f, 0.95f))
				.Padding(0)
				[
					SNew(SHorizontalBox)
					+ SHorizontalBox::Slot().AutoWidth()
					[
						SNew(SBox).WidthOverride(6.0f)
						[
							SNew(SBorder).BorderImage(FCoreStyle::Get().GetBrush("WhiteBrush")).BorderBackgroundColor(Rarity)
						]
					]
					+ SHorizontalBox::Slot().FillWidth(1.0f).Padding(FMargin(12, 8))[ Text ]
					+ SHorizontalBox::Slot().AutoWidth().VAlign(VAlign_Center).Padding(FMargin(8, 0))
					[
						SNew(STextBlock).Text(FText::FromString(Price)).Font(Font(13, true)).ColorAndOpacity(Gold)
					]
					+ SHorizontalBox::Slot().AutoWidth().VAlign(VAlign_Center).Padding(FMargin(0, 0, 12, 0))
					[
						SNew(SButton)
						.ContentPadding(FMargin(14, 5))
						.IsEnabled_Lambda([CanPress]() { return CanPress(); })
						.OnClicked_Lambda([OnPress]() { OnPress(); return FReply::Handled(); })
						[
							SNew(STextBlock).Text(ButtonText).Font(Font(11, true))
						]
					]
				]
			];
	}
}

void UDCCShopWidget::Open(ADCCVendorNPC* InVendor)
{
	Vendor = InVendor;
	bSellTab = false;
	Message.Reset();
	SeenBagCount = -1;
	SeenGold = -1;
	RebuildList();
}

void UDCCShopWidget::Close()
{
	if (APlayerController* PC = GetOwningPlayer())
	{
		if (ADCCCharacterBase* Crawler = Cast<ADCCCharacterBase>(PC->GetPawn()))
		{
			Crawler->CloseVendorMenu();
		}
	}
	if (Hud.IsValid())
	{
		Hud->CloseShop();
	}
}

void UDCCShopWidget::TickShop()
{
	const APlayerController* PC = GetOwningPlayer();
	const ADCCCharacterBase* Crawler = PC ? Cast<ADCCCharacterBase>(PC->GetPawn()) : nullptr;
	if (!Crawler || !Crawler->Inventory || !Crawler->Stats)
	{
		return;
	}
	// The bag and purse change by replication after a trade; follow them.
	const int32 Bag = Crawler->Inventory->Items.Num();
	const int32 Purse = Crawler->Stats->Gold;
	if (Bag != SeenBagCount || Purse != SeenGold)
	{
		SeenBagCount = Bag;
		SeenGold = Purse;
		RebuildList();
	}
}

void UDCCShopWidget::RebuildList()
{
	if (!List.IsValid())
	{
		return;
	}
	List->ClearChildren();

	APlayerController* PC = GetOwningPlayer();
	ADCCCharacterBase* Crawler = PC ? Cast<ADCCCharacterBase>(PC->GetPawn()) : nullptr;
	ADCCVendorNPC* Shop = Vendor.Get();
	if (!Crawler || !Shop || !Crawler->Equipment || !Crawler->Inventory || !Crawler->Stats)
	{
		return;
	}

	TWeakObjectPtr<UDCCShopWidget> Self(this);
	TWeakObjectPtr<ADCCCharacterBase> WeakCrawler(Crawler);
	TWeakObjectPtr<ADCCVendorNPC> WeakVendor(Shop);
	auto PurseGold = [WeakCrawler]() { return WeakCrawler.IsValid() && WeakCrawler->Stats ? WeakCrawler->Stats->Gold : 0; };

	if (!bSellTab)
	{
		for (const FName& Id : Shop->SellableEquipmentIds)
		{
			FDCCEquipmentDef Def;
			if (!Crawler->Equipment->FindItem(Id, Def))
			{
				continue; // not imported yet
			}
			TArray<TPair<FString, FLinearColor>> Lines;
			Lines.Add({ FString::Printf(TEXT("%s  -  %s"), *DCCItemToast::RarityName(Def.Rarity), *UEnum::GetDisplayValueAsText(Def.Slot).ToString()), Dim });
			Lines.Add({ StatsLine(Def), Ink });
			if (Def.SplashRadius > 0.0f)
			{
				Lines.Add({ FString::Printf(TEXT("Explosive: %.0f m blast, hits everyone in it (you too)."), Def.SplashRadius / 100.0f), Bad });
				Lines.Add({ SkillLine(Crawler, Def.HandlingSkillId, TEXT("Handling")) + (Def.FumbleOnNatural > 1 ? FString::Printf(TEXT(". Goes off in hand on 1-%d."), Def.FumbleOnNatural) : FString()), Gold });
			}
			Lines.Add({ SkillLine(Crawler, Def.WeaponSkillId, Def.SplashRadius > 0.0f ? TEXT("Thrown with") : TEXT("Attack Skill")), Gold });

			const FName Current = Crawler->Equipment->GetEquippedId(Def.Slot);
			FDCCEquipmentDef CurrentDef;
			if (Current.IsNone())
			{
				Lines.Add({ TEXT("Your slot is empty."), Good });
			}
			else if (Current != Id && Crawler->Equipment->FindItem(Current, CurrentDef))
			{
				const int32 Delta = Score(Def) - Score(CurrentDef);
				const FString Was = CurrentDef.DisplayName.ToString();
				Lines.Add(Delta > 0 ? TPair<FString, FLinearColor>(FString::Printf(TEXT("UPGRADE over %s"), *Was), Good)
					: Delta < 0 ? TPair<FString, FLinearColor>(FString::Printf(TEXT("Weaker than %s"), *Was), Bad)
					: TPair<FString, FLinearColor>(FString::Printf(TEXT("About the same as %s"), *Was), Dim));
			}

			const int32 Price = Def.GoldValue;
			const FText Display = Def.DisplayName;
			List->AddSlot().AutoHeight()
			[
				Card(DCCItemToast::RarityColour(Def.Rarity), Def.DisplayName.ToString(), Lines, FString::Printf(TEXT("%dg"), Price),
					NSLOCTEXT("DCC", "ShopBuy", "Buy"),
					[PurseGold, Price, WeakCrawler]() { return PurseGold() >= Price && WeakCrawler.IsValid() && !WeakCrawler->Inventory->IsFull(); },
					[Self, WeakCrawler, WeakVendor, Id, Display]()
					{
						if (WeakCrawler.IsValid())
						{
							WeakCrawler->ShopBuy(WeakVendor.Get(), Id, false);
						}
						if (Self.IsValid())
						{
							Self->Message = FString::Printf(TEXT("Bought %s. It is in your bag (Tab to equip)."), *Display.ToString());
						}
					})
			];
		}

		for (const FName& Id : Shop->SellableConsumableIds)
		{
			FDCCConsumableDef Def;
			if (!Crawler->Inventory->FindConsumable(Id, Def))
			{
				continue;
			}
			TArray<FString> Effects;
			for (const FDCCConsumableEffect& Effect : Def.Effects)
			{
				if (Effect.InstantHealthChange)
				{
					Effects.Add(FString::Printf(TEXT("%+d health"), Effect.InstantHealthChange));
				}
				if (Effect.ModifierAmount)
				{
					Effects.Add(FString::Printf(TEXT("%+d %s for %.0f s"), Effect.ModifierAmount,
						*UEnum::GetDisplayValueAsText(Effect.ModifiedAbility).ToString(), Effect.ModifierDurationSeconds));
				}
			}
			TArray<TPair<FString, FLinearColor>> Lines;
			Lines.Add({ TEXT("Consumable  -  goes in your pack"), Dim });
			Lines.Add({ FString::Join(Effects, TEXT("   ")), Ink });
			if (!Def.LinkedSkillId.IsNone())
			{
				Lines.Add({ SkillLine(Crawler, Def.LinkedSkillId, TEXT("Better with")) + FString::Printf(TEXT(": +%d health per Rank on a success."), Def.HealPerSkillRank), Gold });
			}
			const int32 Price = Def.GoldValue;
			const FText Display = Def.DisplayName;
			List->AddSlot().AutoHeight()
			[
				Card(FLinearColor(0.48f, 0.82f, 0.48f), Def.DisplayName.ToString(), Lines, FString::Printf(TEXT("%dg"), Price),
					NSLOCTEXT("DCC", "ShopBuy", "Buy"),
					[PurseGold, Price]() { return PurseGold() >= Price; },
					[Self, WeakCrawler, WeakVendor, Id, Display]()
					{
						if (WeakCrawler.IsValid())
						{
							WeakCrawler->ShopBuy(WeakVendor.Get(), Id, true);
						}
						if (Self.IsValid())
						{
							Self->Message = FString::Printf(TEXT("Bought %s. It is in your pack."), *Display.ToString());
						}
					})
			];
		}
		return;
	}

	// Sell: everything in the bag, at half value.
	const TArray<FDCCInventoryItem>& Items = Crawler->Inventory->Items;
	if (Items.IsEmpty())
	{
		List->AddSlot().AutoHeight().Padding(8)
		[
			SNew(STextBlock).Text(NSLOCTEXT("DCC", "ShopNothingToSell", "Your bag is empty. Gear you are wearing has to come off before it can be sold.")).Font(Font(11)).ColorAndOpacity(Dim)
		];
		return;
	}
	for (int32 Index = 0; Index < Items.Num(); ++Index)
	{
		const FDCCInventoryItem& Item = Items[Index];
		FDCCEquipmentDef Def;
		const bool bGear = !Item.EquipmentItemId.IsNone() && Crawler->Equipment->FindItem(Item.EquipmentItemId, Def);
		TArray<TPair<FString, FLinearColor>> Lines;
		Lines.Add({ bGear ? FString::Printf(TEXT("%s  -  %s"), *DCCItemToast::RarityName(Def.Rarity), *UEnum::GetDisplayValueAsText(Def.Slot).ToString()) : FString(TEXT("Item")), Dim });
		if (bGear)
		{
			Lines.Add({ StatsLine(Def), Ink });
		}
		const int32 Price = Crawler->GetShopSellPrice(Index);
		const FText Display = Item.DisplayName;
		List->AddSlot().AutoHeight()
		[
			Card(bGear ? DCCItemToast::RarityColour(Def.Rarity) : Dim, Item.DisplayName.ToString(), Lines, FString::Printf(TEXT("+%dg"), Price),
				NSLOCTEXT("DCC", "ShopSell", "Sell"),
				[]() { return true; },
				[Self, WeakCrawler, WeakVendor, Index, Display, Price]()
				{
					if (WeakCrawler.IsValid())
					{
						WeakCrawler->ShopSell(WeakVendor.Get(), Index);
					}
					if (Self.IsValid())
					{
						Self->Message = FString::Printf(TEXT("Sold %s for %d gold."), *Display.ToString(), Price);
					}
				})
		];
	}
}

TSharedRef<SWidget> UDCCShopWidget::RebuildWidget()
{
	TWeakObjectPtr<UDCCShopWidget> Self(this);
	auto Tab = [Self](const FText& Label, bool bSell)
	{
		return SNew(SButton)
			.ContentPadding(FMargin(16, 5))
			.ButtonColorAndOpacity_Lambda([Self, bSell]()
			{
				return Self.IsValid() && Self->bSellTab == bSell ? FLinearColor(1.0f, 0.75f, 0.25f) : FLinearColor(0.35f, 0.35f, 0.42f);
			})
			.OnClicked_Lambda([Self, bSell]()
			{
				if (Self.IsValid())
				{
					Self->bSellTab = bSell;
					Self->RebuildList();
				}
				return FReply::Handled();
			})
			[
				SNew(STextBlock).Text(Label).Font(Font(12, true))
			];
	};

	SAssignNew(List, SVerticalBox);

	TSharedRef<SWidget> Root =
		SNew(SBox).HAlign(HAlign_Center).VAlign(VAlign_Center)
		[
			SNew(SBox).WidthOverride(780.0f).HeightOverride(640.0f)
			[
				SNew(SBorder)
				.BorderImage(FCoreStyle::Get().GetBrush("WhiteBrush"))
				.BorderBackgroundColor(FLinearColor(0.035f, 0.03f, 0.06f, 0.96f))
				.Padding(FMargin(22, 18))
				[
					SNew(SVerticalBox)
					+ SVerticalBox::Slot().AutoHeight()
					[
						SNew(SHorizontalBox)
						+ SHorizontalBox::Slot().FillWidth(1.0f)
						[
							SNew(STextBlock)
							.Text_Lambda([Self]() { return Self.IsValid() && Self->Vendor.IsValid() ? Self->Vendor->DisplayName : FText::GetEmpty(); })
							.Font(Font(20, true)).ColorAndOpacity(Gold)
						]
						+ SHorizontalBox::Slot().AutoWidth().VAlign(VAlign_Center)
						[
							SNew(STextBlock)
							.Text_Lambda([Self]()
							{
								const APlayerController* PC = Self.IsValid() ? Self->GetOwningPlayer() : nullptr;
								const ADCCCharacterBase* C = PC ? Cast<ADCCCharacterBase>(PC->GetPawn()) : nullptr;
								return FText::FromString(C && C->Stats && C->Inventory
									? FString::Printf(TEXT("%d gold     bag %d/%d"), C->Stats->Gold, C->Inventory->Items.Num(), C->Inventory->Capacity)
									: FString());
							})
							.Font(Font(13, true)).ColorAndOpacity(Ink)
						]
					]
					+ SVerticalBox::Slot().AutoHeight().Padding(0, 10, 0, 10)
					[
						SNew(SHorizontalBox)
						+ SHorizontalBox::Slot().AutoWidth().Padding(0, 0, 6, 0)[ Tab(NSLOCTEXT("DCC", "ShopTabBuy", "Buy"), false) ]
						+ SHorizontalBox::Slot().AutoWidth()[ Tab(NSLOCTEXT("DCC", "ShopTabSell", "Sell"), true) ]
					]
					+ SVerticalBox::Slot().FillHeight(1.0f)
					[
						SNew(SScrollBox) + SScrollBox::Slot()[ List.ToSharedRef() ]
					]
					+ SVerticalBox::Slot().AutoHeight().Padding(0, 10, 0, 0)
					[
						SNew(SHorizontalBox)
						+ SHorizontalBox::Slot().FillWidth(1.0f).VAlign(VAlign_Center)
						[
							SNew(STextBlock)
							.Text_Lambda([Self]() { return Self.IsValid() ? FText::FromString(Self->Message) : FText::GetEmpty(); })
							.Font(Font(11, true)).ColorAndOpacity(Good)
						]
						+ SHorizontalBox::Slot().AutoWidth()
						[
							SNew(SButton)
							.ContentPadding(FMargin(14, 5))
							.OnClicked_Lambda([Self]() { if (Self.IsValid()) { Self->Close(); } return FReply::Handled(); })
							[
								SNew(STextBlock).Text(NSLOCTEXT("DCC", "ShopClose", "Leave  (Esc)")).Font(Font(11, true))
							]
						]
					]
				]
			]
		];

	RebuildList();
	return Root;
}

void UDCCShopWidget::ReleaseSlateResources(bool bReleaseChildren)
{
	Super::ReleaseSlateResources(bReleaseChildren);
	List.Reset();
}

FReply UDCCShopWidget::NativeOnKeyDown(const FGeometry& InGeometry, const FKeyEvent& InKeyEvent)
{
	if (InKeyEvent.GetKey() == EKeys::Escape || InKeyEvent.GetKey() == EKeys::E || InKeyEvent.GetKey() == EKeys::Tab)
	{
		Close();
		return FReply::Handled();
	}
	return Super::NativeOnKeyDown(InGeometry, InKeyEvent);
}
