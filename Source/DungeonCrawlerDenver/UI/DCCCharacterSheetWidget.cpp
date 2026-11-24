#include "UI/DCCCharacterSheetWidget.h"

#include "Characters/DCCCharacterBase.h"
#include "Classes/DCCBasicClasses.h"
#include "Equipment/DCCEquipmentComponent.h"
#include "Equipment/DCCEquipmentTypes.h"
#include "Fonts/CompositeFont.h"
#include "GameFramework/PlayerController.h"
#include "GameFramework/PlayerState.h"
#include "Items/DCCInventoryComponent.h"
#include "Misc/Paths.h"
#include "Stats/DCCRuleset.h"
#include "Stats/DCCStatsComponent.h"
#include "UI/DCCLook.h"
#include "UI/DCCMenuStyle.h"
#include "Widgets/Images/SImage.h"
#include "Widgets/Input/SButton.h"
#include "Widgets/Layout/SBorder.h"
#include "Widgets/Layout/SBox.h"
#include "Widgets/Layout/SScrollBox.h"
#include "Widgets/Layout/SSpacer.h"
#include "Widgets/Layout/SUniformGridPanel.h"
#include "Widgets/Notifications/SProgressBar.h"
#include "Widgets/SBoxPanel.h"
#include "Widgets/SOverlay.h"
#include "Widgets/Text/STextBlock.h"


namespace
{
	using DCCLook::FLook;
	const FLook& L() { return DCCLook::Get(); }
	FSlateFontInfo Head(int32 Size, int32 Spacing = 40) { return DCCLook::Head(Size, Spacing); }
	FSlateFontInfo Body(int32 Size, int32 Spacing = 0) { return DCCLook::Body(Size, Spacing); }
	FSlateFontInfo Caps(int32 Size) { return DCCLook::Caps(Size); }

	const FSlateBrush* Box(const FLinearColor& Fill, const FLinearColor& Edge = FLinearColor::Transparent, float EdgeWidth = 0.0f)
	{
		return DCCMenu::Rounded(Fill, L().Radius, Edge, EdgeWidth);
	}

	TSharedRef<SWidget> Text(const TAttribute<FText>& T, const FSlateFontInfo& Font, const FLinearColor& Colour)
	{
		return SNew(STextBlock).Text(T).Font(Font).ColorAndOpacity(FSlateColor(Colour));
	}

	TSharedRef<SWidget> Label(const FString& S, const FLinearColor& Colour)
	{
		return Text(FText::FromString(S.ToUpper()), Caps(10), Colour);
	}

	/** A titled section: a caption in the accent, a hairline, then the content. */
	TSharedRef<SWidget> Section(const FString& Title, const TSharedRef<SWidget>& Content, const FLinearColor& Accent)
	{
		const FLook& K = L();
		return SNew(SBorder).BorderImage(Box(K.Panel)).Padding(FMargin(16.0f, 12.0f))
			[
				SNew(SVerticalBox)
				+ SVerticalBox::Slot().AutoHeight()
				[
					SNew(SHorizontalBox)
					+ SHorizontalBox::Slot().AutoWidth().VAlign(VAlign_Center).Padding(0, 0, 8, 0)
					[
						SNew(SBox).WidthOverride(K.bSlantBars ? 6.0f : 3.0f).HeightOverride(14.0f)
						[ SNew(SImage).Image(Box(Accent)) ]
					]
					+ SHorizontalBox::Slot().AutoWidth().VAlign(VAlign_Center)[ Label(Title, Accent) ]
				]
				+ SVerticalBox::Slot().AutoHeight().Padding(0, 8, 0, 10)
				[ SNew(SBox).HeightOverride(1.0f)[ SNew(SImage).Image(Box(DCCMenu::WithAlpha(K.Ink, 0.08f))) ] ]
				+ SVerticalBox::Slot().AutoHeight()[ Content ]
			];
	}

	/** A bar with its number on it: health, mana, stamina, XP. */
	TSharedRef<SWidget> Bar(const FString& Name, TAttribute<TOptional<float>> Fill, TAttribute<FText> Value, const FLinearColor& Colour, float Height = 14.0f)
	{
		const FLook& K = L();
		static FProgressBarStyle Styles[8];
		static int32 Next = 0;
		FProgressBarStyle& Style = Styles[Next++ & 7];
		Style = FProgressBarStyle()
			.SetBackgroundImage(*Box(FLinearColor(0.0f, 0.0f, 0.0f, 0.45f)))
			.SetFillImage(*Box(Colour))
			.SetMarqueeImage(*Box(Colour));
		return SNew(SVerticalBox)
			+ SVerticalBox::Slot().AutoHeight().Padding(0, 0, 0, 3)
			[
				SNew(SHorizontalBox)
				+ SHorizontalBox::Slot().FillWidth(1.0f)[ Label(Name, K.Dim) ]
				+ SHorizontalBox::Slot().AutoWidth()[ Text(Value, Body(11), K.Ink) ]
			]
			+ SVerticalBox::Slot().AutoHeight()
			[
				SNew(SBox).HeightOverride(Height)
				[ SNew(SProgressBar).Style(&Style).Percent(Fill).BarFillType(EProgressBarFillType::LeftToRight) ]
			];
	}

	/** A small button in the look's accent. */
	TSharedRef<SWidget> Action(const FString& S, const FLinearColor& Accent, FOnClicked OnClicked, bool bPrimary = false)
	{
		return SNew(SButton)
			.ButtonStyle(DCCMenu::ButtonStyle(Accent, bPrimary))
			.ContentPadding(FMargin(12.0f, 4.0f))
			.OnClicked(OnClicked)
			[ Text(FText::FromString(S.ToUpper()), Caps(9), bPrimary ? FLinearColor(0.05f, 0.03f, 0.08f) : Accent) ];
	}
}

void UDCCCharacterSheetWidget::NativeConstruct()
{
	Super::NativeConstruct();
	Opened = 0.0f;
	LastSignature.Reset();
}

ADCCCharacterBase* UDCCCharacterSheetWidget::Crawler() const
{
	const APlayerController* PC = GetOwningPlayer();
	return PC ? Cast<ADCCCharacterBase>(PC->GetPawn()) : nullptr;
}

TSharedRef<SWidget> UDCCCharacterSheetWidget::RebuildWidget()
{
	SAssignNew(Root, SBox);
	Refresh();
	return Root.ToSharedRef();
}

void UDCCCharacterSheetWidget::Refresh()
{
	if (Root.IsValid())
	{
		BuiltLook = DCCLook::Index();
		LastSignature = Signature();
		Root->SetContent(BuildContent());
	}
}

FString UDCCCharacterSheetWidget::Signature() const
{
	// What changes the layout (not the bars, which read live): scores, points, gear, the bag.
	const ADCCCharacterBase* C = Crawler();
	if (!C || !C->Stats)
	{
		return TEXT("-");
	}
	FString S = FString::Printf(TEXT("%d|%d|%d|"), C->Stats->Level, C->Stats->GetUnspentStatPoints(), C->Stats->Gold);
	for (int32 i = 0; i < int32(EDCCAbility::MAX); ++i)
	{
		S += FString::Printf(TEXT("%d,"), C->Stats->GetEnhanced().GetScore(EDCCAbility(i)));
	}
	if (C->Equipment)
	{
		for (int32 i = 0; i < int32(EDCCEquipmentSlot::MAX); ++i)
		{
			S += C->Equipment->GetEquippedId(EDCCEquipmentSlot(i)).ToString() + TEXT(",");
		}
	}
	if (C->Inventory)
	{
		S += FString::Printf(TEXT("|%d"), C->Inventory->Items.Num());
		for (const FDCCInventoryItem& Item : C->Inventory->Items)
		{
			S += Item.EquipmentItemId.ToString();
		}
	}
	return S;
}

void UDCCCharacterSheetWidget::NativeTick(const FGeometry& MyGeometry, float InDeltaTime)
{
	Super::NativeTick(MyGeometry, InDeltaTime);
	// It slides in and fades up over a fifth of a second.
	Opened = FMath::Min(1.0f, Opened + InDeltaTime / 0.2f);
	const float Ease = 1.0f - FMath::Square(1.0f - Opened);
	SetRenderOpacity(Ease);
	SetRenderTranslation(FVector2D(-30.0f * (1.0f - Ease), 0.0f));
	if (BuiltLook != DCCLook::Index() || Signature() != LastSignature)
	{
		Refresh();
	}
}

TSharedRef<SWidget> UDCCCharacterSheetWidget::BuildContent()
{
	const FLook& K = L();
	ADCCCharacterBase* C = Crawler();
	if (!C || !C->Stats)
	{
		return SNew(SSpacer);
	}
	TWeakObjectPtr<ADCCCharacterBase> Weak(C);
	TWeakObjectPtr<UDCCCharacterSheetWidget> Self(this);
	UDCCStatsComponent* Stats = C->Stats;
	const DCCBasicClasses::FClass* MyClass = DCCBasicClasses::Find(C);
	const FString Name = C->DisplayName.IsEmpty() ? (C->GetPlayerState() ? C->GetPlayerState()->GetPlayerName() : TEXT("Crawler")) : C->DisplayName.ToString();

	// ---- the tab row ----
	TSharedRef<SHorizontalBox> Tabs = SNew(SHorizontalBox);
	struct FTab { int32 Index; const TCHAR* Name; };
	static const FTab TabList[] = { { 0, TEXT("CHARACTER") }, { 1, TEXT("SKILLS") }, { 11, TEXT("SOCIAL") }, { 5, TEXT("ACHIEVEMENTS") }, { 10, TEXT("SETTINGS") } };
	for (const FTab& T : TabList)
	{
		const bool bOn = T.Index == 0;
		const int32 Index = T.Index;
		Tabs->AddSlot().AutoWidth().Padding(0, 0, 6, 0)
		[
			SNew(SButton)
			.ButtonStyle(DCCMenu::ButtonStyle(K.Accent, bOn))
			.ContentPadding(FMargin(14.0f, 6.0f))
			.OnClicked_Lambda([Self, Index]() { if (Self.IsValid() && Self->OnTabRequested) { Self->OnTabRequested(Index); } return FReply::Handled(); })
			[ Text(FText::FromString(T.Name), Caps(10), bOn ? FLinearColor(0.05f, 0.03f, 0.08f) : K.Ink) ]
		];
	}
	Tabs->AddSlot().FillWidth(1.0f)[ SNew(SSpacer) ];
	Tabs->AddSlot().AutoWidth()
	[
		Action(TEXT("Close  [Tab]"), K.Dim, FOnClicked::CreateLambda([Self]() { if (Self.IsValid() && Self->OnCloseRequested) { Self->OnCloseRequested(); } return FReply::Handled(); }))
	];

	// ---- the header: name, class, level and XP ----
	const FString ClassLine = MyClass ? FString::Printf(TEXT("%s   %s"), MyClass->Name, *DCCBasicClasses::BonusLine(C)) : FString(TEXT("No class yet"));
	TSharedRef<SWidget> Header = SNew(SHorizontalBox)
		+ SHorizontalBox::Slot().AutoWidth().VAlign(VAlign_Center).Padding(0, 0, 18, 0)
		[
			// The level, big, in a badge.
			SNew(SBox).WidthOverride(84.0f).HeightOverride(84.0f)
			[
				SNew(SBorder).BorderImage(Box(DCCMenu::WithAlpha(K.Accent, 0.16f), K.Accent, 2.0f)).HAlign(HAlign_Center).VAlign(VAlign_Center)
				[
					SNew(SVerticalBox)
					+ SVerticalBox::Slot().AutoHeight().HAlign(HAlign_Center)[ Label(TEXT("Level"), K.Accent) ]
					+ SVerticalBox::Slot().AutoHeight().HAlign(HAlign_Center)[ Text(FText::AsNumber(Stats->Level), Head(30, 0), K.Ink) ]
				]
			]
		]
		+ SHorizontalBox::Slot().FillWidth(1.0f).VAlign(VAlign_Center)
		[
			SNew(SVerticalBox)
			+ SVerticalBox::Slot().AutoHeight()[ Text(FText::FromString(Name.ToUpper()), Head(30, 60), K.Ink) ]
			+ SVerticalBox::Slot().AutoHeight().Padding(0, 2, 0, 10)[ Text(FText::FromString(ClassLine), Body(13), MyClass ? MyClass->Colour : K.Dim) ]
			+ SVerticalBox::Slot().AutoHeight()
			[
				Bar(TEXT("Experience"),
					TAttribute<TOptional<float>>::CreateLambda([Weak]() -> TOptional<float> { const ADCCCharacterBase* P = Weak.Get(); return P && P->Stats && P->Stats->ExperienceToNextLevel > 0 ? float(P->Stats->ExperiencePoints) / P->Stats->ExperienceToNextLevel : 1.0f; }),
					TAttribute<FText>::CreateLambda([Weak]() { const ADCCCharacterBase* P = Weak.Get(); return P && P->Stats ? (P->Stats->IsAtMaxLevel() ? FText::FromString(TEXT("MAX")) : FText::FromString(FString::Printf(TEXT("%d / %d XP"), P->Stats->ExperiencePoints, P->Stats->ExperienceToNextLevel))) : FText(); }),
					K.Accent2, 10.0f)
			]
		];

	// ---- vitals ----
	auto Vital = [Weak](int32 Which)
	{
		return TAttribute<TOptional<float>>::CreateLambda([Weak, Which]() -> TOptional<float>
		{
			const ADCCCharacterBase* P = Weak.Get();
			if (!P || !P->Stats) { return 0.0f; }
			const UDCCStatsComponent* S = P->Stats;
			return Which == 0 ? float(S->CurrentHitPoints) / FMath::Max(1, S->MaxHitPoints)
				: Which == 1 ? float(S->CurrentManaPoints) / FMath::Max(1, S->MaxManaPoints)
				: S->CurrentStamina / FMath::Max(1.0f, S->MaxStamina);
		});
	};
	auto VitalText = [Weak](int32 Which)
	{
		return TAttribute<FText>::CreateLambda([Weak, Which]()
		{
			const ADCCCharacterBase* P = Weak.Get();
			if (!P || !P->Stats) { return FText(); }
			const UDCCStatsComponent* S = P->Stats;
			return FText::FromString(Which == 0 ? FString::Printf(TEXT("%d / %d"), S->CurrentHitPoints, S->MaxHitPoints)
				: Which == 1 ? FString::Printf(TEXT("%d / %d"), S->CurrentManaPoints, S->MaxManaPoints)
				: FString::Printf(TEXT("%.0f / %.0f"), S->CurrentStamina, S->MaxStamina));
		});
	};
	UDCCRuleset* Rules = Stats->GetRuleset();
	const int32 Evade = Rules ? Rules->GetDefenceValue(Stats->GetEnhanced(), Stats->Level, C->Equipment ? C->Equipment->GetEvadeBonus() : 0) : 0;
	const float Lift = Rules ? Rules->GetCarryMassLimit(Stats->GetEnhanced()) : 0.0f;
	TSharedRef<SWidget> Vitals = SNew(SVerticalBox)
		+ SVerticalBox::Slot().AutoHeight().Padding(0, 0, 0, 10)[ Bar(TEXT("Health"), Vital(0), VitalText(0), FLinearColor(0.95f, 0.3f, 0.32f)) ]
		+ SVerticalBox::Slot().AutoHeight().Padding(0, 0, 0, 10)[ Bar(TEXT("Mana"), Vital(1), VitalText(1), FLinearColor(0.35f, 0.55f, 1.0f)) ]
		+ SVerticalBox::Slot().AutoHeight().Padding(0, 0, 0, 12)[ Bar(TEXT("Stamina"), Vital(2), VitalText(2), FLinearColor(0.4f, 0.95f, 0.55f)) ]
		+ SVerticalBox::Slot().AutoHeight()
		[
			SNew(SHorizontalBox)
			+ SHorizontalBox::Slot().FillWidth(1.0f)[ Text(FText::FromString(FString::Printf(TEXT("Evade %d"), Evade)), Body(13), K.Ink) ]
			+ SHorizontalBox::Slot().FillWidth(1.0f)[ Text(FText::FromString(FString::Printf(TEXT("Lifts %.0f kg"), Lift)), Body(13), K.Ink) ]
			+ SHorizontalBox::Slot().FillWidth(1.0f)[ Text(FText::FromString(FString::Printf(TEXT("%d gold"), Stats->Gold)), Body(13), K.Accent2) ]
		];

	// ---- the scores ----
	static const TCHAR* Short[] = { TEXT("STR"), TEXT("INT"), TEXT("CON"), TEXT("DEX"), TEXT("CHA") };
	static const TCHAR* Does[] = { TEXT("Melee hits"), TEXT("Mana, spells"), TEXT("Health"), TEXT("Dodge, speed"), TEXT("Charm, heals") };
	const int32 Unspent = Stats->GetUnspentStatPoints();
	TSharedRef<SUniformGridPanel> Scores = SNew(SUniformGridPanel).SlotPadding(FMargin(4.0f));
	for (int32 i = 0; i < 5; ++i)
	{
		const EDCCAbility Ability = EDCCAbility(i);
		const int32 Score = Stats->GetEnhanced().GetScore(Ability);
		const int32 Mod = Stats->GetAbilityModifier(Ability);
		TSharedRef<SVerticalBox> CardBody = SNew(SVerticalBox)
			+ SVerticalBox::Slot().AutoHeight().HAlign(HAlign_Center)[ Label(Short[i], K.Accent) ]
			+ SVerticalBox::Slot().AutoHeight().HAlign(HAlign_Center)[ Text(FText::AsNumber(Score), Head(28, 0), K.Ink) ]
			+ SVerticalBox::Slot().AutoHeight().HAlign(HAlign_Center).Padding(0, 0, 0, 4)[ Text(FText::FromString(FString::Printf(TEXT("+%d to hit"), Mod)), Body(11), K.Accent2) ]
			+ SVerticalBox::Slot().AutoHeight().HAlign(HAlign_Center)[ Text(FText::FromString(Does[i]), Body(10), K.Dim) ];
		if (Unspent > 0)
		{
			CardBody->AddSlot().AutoHeight().HAlign(HAlign_Center).Padding(0, 8, 0, 0)
			[
				Action(TEXT("+ Raise"), FLinearColor(0.4f, 1.0f, 0.55f), FOnClicked::CreateLambda([Weak, Ability]()
				{
					if (ADCCCharacterBase* P = Weak.Get()) { if (P->Stats) { P->Stats->Server_RaiseAbility(Ability); } }
					return FReply::Handled();
				}), true)
			];
		}
		Scores->AddSlot(i, 0)
		[
			SNew(SBorder).BorderImage(Box(K.Card, DCCMenu::WithAlpha(K.Accent, 0.25f), 1.0f)).Padding(FMargin(8.0f, 10.0f))[ CardBody ]
		];
	}
	TSharedRef<SVerticalBox> ScoreBlock = SNew(SVerticalBox);
	if (Unspent > 0)
	{
		ScoreBlock->AddSlot().AutoHeight().Padding(0, 0, 0, 8)
		[ Text(FText::FromString(FString::Printf(TEXT("%d stat point%s to spend"), Unspent, Unspent == 1 ? TEXT("") : TEXT("s"))), Body(13), FLinearColor(0.45f, 1.0f, 0.6f)) ];
	}
	ScoreBlock->AddSlot().AutoHeight()[ Scores ];

	// ---- equipped ----
	static const TCHAR* SlotNames[] = { TEXT("Main hand"), TEXT("Off hand"), TEXT("Head"), TEXT("Body"), TEXT("Trinket") };
	TSharedRef<SVerticalBox> Gear = SNew(SVerticalBox);
	for (int32 i = 0; i < int32(EDCCEquipmentSlot::MAX) && i < 5; ++i)
	{
		const FName Id = C->Equipment ? C->Equipment->GetEquippedId(EDCCEquipmentSlot(i)) : NAME_None;
		FDCCEquipmentDef Def;
		const bool bHas = !Id.IsNone() && C->Equipment->FindItem(Id, Def);
		TArray<FString> Bits;
		if (bHas)
		{
			auto Add = [&Bits](int32 V, const TCHAR* N) { if (V) { Bits.Add(FString::Printf(TEXT("%+d %s"), V, N)); } };
			Add(Def.BonusStrength, TEXT("Str")); Add(Def.BonusIntelligence, TEXT("Int")); Add(Def.BonusConstitution, TEXT("Con"));
			Add(Def.BonusDexterity, TEXT("Dex")); Add(Def.BonusCharisma, TEXT("Cha"));
			if (Def.WeaponDamageDie > 0) { Bits.Add(FString::Printf(TEXT("d%d"), Def.WeaponDamageDie)); }
			if (Def.EvadeBonus) { Bits.Add(FString::Printf(TEXT("%+d Evade"), Def.EvadeBonus)); }
		}
		const uint8 SlotIndex = uint8(i);
		TSharedRef<SHorizontalBox> Row = SNew(SHorizontalBox)
			+ SHorizontalBox::Slot().AutoWidth().VAlign(VAlign_Center).Padding(0, 0, 10, 0)[ DCCMenu::Icon(Id, 40.0f, bHas ? K.Accent : K.Dim) ]
			+ SHorizontalBox::Slot().FillWidth(1.0f).VAlign(VAlign_Center)
			[
				SNew(SVerticalBox)
				+ SVerticalBox::Slot().AutoHeight()[ Label(SlotNames[i], K.Dim) ]
				+ SVerticalBox::Slot().AutoHeight()[ Text(FText::FromString(bHas ? Def.DisplayName.ToString() : TEXT("empty")), Body(14), bHas ? K.Ink : K.Dim) ]
				+ SVerticalBox::Slot().AutoHeight()[ Text(FText::FromString(Bits.Num() ? FString::Join(Bits, TEXT("   ")) : FString()), Body(11), K.Accent2) ]
			];
		if (bHas)
		{
			Row->AddSlot().AutoWidth().VAlign(VAlign_Center)
			[
				Action(TEXT("Unequip"), K.Accent, FOnClicked::CreateLambda([Weak, SlotIndex]()
				{
					if (ADCCCharacterBase* P = Weak.Get()) { P->DCCUnequipSlot(SlotIndex); }
					return FReply::Handled();
				}))
			];
		}
		Gear->AddSlot().AutoHeight().Padding(0, 0, 0, 8)
		[ SNew(SBorder).BorderImage(Box(K.Card)).Padding(FMargin(10.0f, 6.0f))[ Row ] ];
	}

	// ---- the bag ----
	TSharedRef<SVerticalBox> Bag = SNew(SVerticalBox);
	if (UDCCInventoryComponent* Inv = C->Inventory)
	{
		Bag->AddSlot().AutoHeight().Padding(0, 0, 0, 8)
		[ Text(FText::FromString(FString::Printf(TEXT("%d / %d stowed   %.0f kg"), Inv->Items.Num(), Inv->Capacity, Inv->GetTotalMass())), Body(12), K.Dim) ];
		for (int32 i = 0; i < Inv->Items.Num(); ++i)
		{
			const FDCCInventoryItem& Item = Inv->Items[i];
			const bool bGear = !Item.EquipmentItemId.IsNone();
			const int32 Index = i;
			TSharedRef<SHorizontalBox> Row = SNew(SHorizontalBox)
				+ SHorizontalBox::Slot().AutoWidth().VAlign(VAlign_Center).Padding(0, 0, 8, 0)[ DCCMenu::Icon(Item.EquipmentItemId, 28.0f, bGear ? K.Accent2 : K.Dim) ]
				+ SHorizontalBox::Slot().FillWidth(1.0f).VAlign(VAlign_Center)[ Text(Item.DisplayName, Body(13), K.Ink) ]
				+ SHorizontalBox::Slot().AutoWidth().VAlign(VAlign_Center).Padding(8, 0)[ Text(FText::FromString(FString::Printf(TEXT("%.0f kg"), Item.MassKilograms)), Body(11), K.Dim) ];
			if (bGear)
			{
				Row->AddSlot().AutoWidth().VAlign(VAlign_Center)
				[
					Action(TEXT("Equip"), K.Accent2, FOnClicked::CreateLambda([Weak, Index]()
					{
						if (ADCCCharacterBase* P = Weak.Get()) { P->DCCEquipItem(Index); }
						return FReply::Handled();
					}))
				];
			}
			Bag->AddSlot().AutoHeight().Padding(0, 0, 0, 4)[ SNew(SBorder).BorderImage(Box(K.Card)).Padding(FMargin(8.0f, 4.0f))[ Row ] ];
		}
		if (Inv->Items.IsEmpty())
		{
			Bag->AddSlot().AutoHeight()[ Text(FText::FromString(TEXT("Nothing stowed. Loot a chest or a body, then Stow.")), Body(12), K.Dim) ];
		}
	}

	// ---- the window ----
	TSharedRef<SWidget> Window = SNew(SBorder)
		.BorderImage(Box(K.Fill, DCCMenu::WithAlpha(K.Accent, 0.55f), 1.5f))
		.Padding(FMargin(26.0f, 20.0f))
		[
			SNew(SVerticalBox)
			+ SVerticalBox::Slot().AutoHeight().Padding(0, 0, 0, 16)[ Tabs ]
			+ SVerticalBox::Slot().AutoHeight().Padding(0, 0, 0, 16)[ Header ]
			+ SVerticalBox::Slot().FillHeight(1.0f)
			[
				SNew(SHorizontalBox)
				+ SHorizontalBox::Slot().FillWidth(1.1f).Padding(0, 0, 14, 0)
				[
					SNew(SScrollBox)
					+ SScrollBox::Slot().Padding(0, 0, 0, 12)[ Section(TEXT("Vitals"), Vitals, K.Accent) ]
					+ SScrollBox::Slot()[ Section(TEXT("Scores"), ScoreBlock, K.Accent) ]
				]
				+ SHorizontalBox::Slot().FillWidth(1.0f)
				[
					SNew(SScrollBox)
					+ SScrollBox::Slot().Padding(0, 0, 0, 12)[ Section(TEXT("Equipped"), Gear, K.Accent2) ]
					+ SScrollBox::Slot()[ Section(TEXT("Bag"), Bag, K.Accent2) ]
				]
			]
			+ SVerticalBox::Slot().AutoHeight().HAlign(HAlign_Right).Padding(0, 10, 0, 0)
			[ Text(FText::FromString(FString::Printf(TEXT("LOOK: %s"), K.Name)), Caps(8), DCCMenu::WithAlpha(K.Dim, 0.6f)) ]
		];

	// Left side of the screen, most of its height, so the world still shows on the right.
	return SNew(SBox).Padding(FMargin(24.0f, 24.0f)).HAlign(HAlign_Left).VAlign(VAlign_Fill)
		[
			SNew(SBox).WidthOverride(1120.0f)[ Window ]
		];
}
