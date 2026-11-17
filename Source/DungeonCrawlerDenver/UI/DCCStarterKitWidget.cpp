#include "UI/DCCStarterKitWidget.h"

#include "Arcade/DCCWalletComponent.h"
#include "Characters/DCCCharacterBase.h"
#include "GameFramework/PlayerController.h"
#include "Session/DCCStarterKits.h"
#include "Styling/CoreStyle.h"
#include "Widgets/Input/SButton.h"
#include "Widgets/Layout/SBorder.h"
#include "Widgets/Layout/SBox.h"
#include "Widgets/SBoxPanel.h"
#include "Widgets/Text/STextBlock.h"

TSharedRef<SWidget> UDCCStarterKitWidget::RebuildWidget()
{
	TWeakObjectPtr<UDCCStarterKitWidget> Self(this);
	auto Font = [](int32 Size, bool bBold) { return FCoreStyle::GetDefaultFontStyle(bBold ? "Bold" : "Regular", Size); };

	TSharedRef<SHorizontalBox> Cards = SNew(SHorizontalBox);
	const TArray<DCCStarterKits::FKit>& Kits = DCCStarterKits::All();
	for (int32 i = 0; i < Kits.Num(); ++i)
	{
		const DCCStarterKits::FKit& Kit = Kits[i];
		Cards->AddSlot().AutoWidth().Padding(8.0f, 0.0f)
		[
			SNew(SBox).WidthOverride(200.0f).HeightOverride(230.0f)
			[
				SNew(SButton)
				.IsFocusable(false)
				.ButtonColorAndOpacity(Kit.Colour * 0.35f)
				.ContentPadding(FMargin(14.0f))
				.OnClicked_Lambda([Self, i]()
				{
					if (Self.IsValid())
					{
						const APlayerController* PC = Self->GetOwningPlayer();
						if (const ADCCCharacterBase* Crawler = PC ? Cast<ADCCCharacterBase>(PC->GetPawn()) : nullptr)
						{
							if (Crawler->Wallet) { Crawler->Wallet->Server_ChooseStarterKit(i); }
						}
						if (Self->OnClosed) { Self->OnClosed(); }
					}
					return FReply::Handled();
				})
				[
					SNew(SVerticalBox)
					+ SVerticalBox::Slot().AutoHeight()
					[
						SNew(STextBlock).Font(Font(18, true)).ColorAndOpacity(Kit.Colour).Text(FText::FromString(Kit.Name))
					]
					+ SVerticalBox::Slot().AutoHeight().Padding(0.0f, 8.0f, 0.0f, 0.0f)
					[
						SNew(STextBlock).Font(Font(11, false)).ColorAndOpacity(FLinearColor(0.92f, 0.92f, 0.95f)).AutoWrapText(true).Text(FText::FromString(Kit.Blurb))
					]
					+ SVerticalBox::Slot().FillHeight(1.0f)[ SNew(SBox) ]
					+ SVerticalBox::Slot().AutoHeight()
					[
						SNew(STextBlock).Font(Font(10, true)).ColorAndOpacity(FLinearColor(1.0f, 0.86f, 0.5f))
						.Text(FText::FromString(FString::Printf(TEXT("+ %d tickets"), Kit.Tickets)))
					]
				]
			]
		];
	}

	return
		SNew(SBox).HAlign(HAlign_Center).VAlign(VAlign_Center)
		[
			SNew(SBorder)
			.BorderImage(FCoreStyle::Get().GetBrush("WhiteBrush"))
			.BorderBackgroundColor(FLinearColor(0.02f, 0.02f, 0.05f, 0.94f))
			.Padding(FMargin(28.0f, 22.0f))
			[
				SNew(SVerticalBox)
				+ SVerticalBox::Slot().AutoHeight().HAlign(HAlign_Center)
				[
					SNew(STextBlock).Font(Font(24, true)).ColorAndOpacity(FLinearColor(1.0f, 0.83f, 0.42f))
					.Text(NSLOCTEXT("DCC", "KitTitle", "WELCOME TO THE WORLD DUNGEON"))
				]
				+ SVerticalBox::Slot().AutoHeight().HAlign(HAlign_Center).Padding(0.0f, 4.0f, 0.0f, 18.0f)
				[
					SNew(STextBlock).Font(Font(12, false)).ColorAndOpacity(FLinearColor(0.8f, 0.8f, 0.85f))
					.Text(NSLOCTEXT("DCC", "KitPick", "Pick a starter kit. It is equipped now, and it is yours to keep."))
				]
				+ SVerticalBox::Slot().AutoHeight()[ Cards ]
			]
		];
}
