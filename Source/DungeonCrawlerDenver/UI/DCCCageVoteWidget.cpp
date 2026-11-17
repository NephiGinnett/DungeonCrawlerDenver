#include "UI/DCCCageVoteWidget.h"

#include "Arcade/DCCArcadeHaunt.h"
#include "Arcade/DCCWalletComponent.h"
#include "Characters/DCCCharacterBase.h"
#include "GameFramework/PlayerController.h"
#include "Styling/CoreStyle.h"
#include "Widgets/Input/SButton.h"
#include "Widgets/Layout/SBorder.h"
#include "Widgets/Layout/SBox.h"
#include "Widgets/SBoxPanel.h"
#include "Widgets/Text/STextBlock.h"

TSharedRef<SWidget> UDCCCageVoteWidget::RebuildWidget()
{
	TWeakObjectPtr<UDCCCageVoteWidget> Self(this);
	auto Font = [](int32 Size, bool bBold) { return FCoreStyle::GetDefaultFontStyle(bBold ? "Bold" : "Regular", Size); };
	const FLinearColor Mint(0.4f, 1.0f, 0.6f);
	return
		SNew(SBox).HAlign(HAlign_Center).VAlign(VAlign_Bottom).Padding(FMargin(0.0f, 0.0f, 0.0f, 150.0f))
		[
			SNew(SBorder)
			.BorderImage(FCoreStyle::Get().GetBrush("WhiteBrush"))
			.BorderBackgroundColor(FLinearColor(0.02f, 0.04f, 0.03f, 0.9f))
			.Padding(FMargin(26.0f, 16.0f))
			[
				SNew(SVerticalBox)
				+ SVerticalBox::Slot().AutoHeight().HAlign(HAlign_Center)
				[
					SNew(STextBlock).Font(Font(18, true)).ColorAndOpacity(Mint)
					.Text(NSLOCTEXT("DCC", "CageAll", "EVERYONE'S IN THE CAGE"))
				]
				+ SVerticalBox::Slot().AutoHeight().HAlign(HAlign_Center).Padding(0.0f, 2.0f, 0.0f, 12.0f)
				[
					SNew(STextBlock).Font(Font(11, false)).ColorAndOpacity(FLinearColor(0.8f, 0.85f, 0.82f))
					.Text(NSLOCTEXT("DCC", "CageAllNote", "Go up now? Your tapes count as if the clock ran out. Step out to keep exploring."))
				]
				+ SVerticalBox::Slot().AutoHeight().HAlign(HAlign_Center)
				[
					SNew(SBox).WidthOverride(320.0f)
					[
						SNew(SButton)
						.IsFocusable(false)
						.HAlign(HAlign_Center)
						.ContentPadding(FMargin(14.0f, 12.0f))
						.ButtonColorAndOpacity_Lambda([Self, Mint]() { return Self.IsValid() && Self->bVoted ? Mint * 0.5f : Mint; })
						.OnClicked_Lambda([Self]()
						{
							if (!Self.IsValid()) { return FReply::Handled(); }
							Self->bVoted = !Self->bVoted;
							const APlayerController* PC = Self->GetOwningPlayer();
							if (const ADCCCharacterBase* Crawler = PC ? Cast<ADCCCharacterBase>(PC->GetPawn()) : nullptr)
							{
								if (Crawler->Wallet) { Crawler->Wallet->Server_CageVote(Self->bVoted); }
							}
							return FReply::Handled();
						})
						[
							SNew(STextBlock).Font(Font(16, true)).ColorAndOpacity(FLinearColor(0.02f, 0.05f, 0.03f))
							.Text_Lambda([Self]()
							{
								const ADCCArcadeHaunt* H = Self.IsValid() ? Self->Haunt.Get() : nullptr;
								const int32 Up = H ? H->CageVotesUp : 0, Of = H ? H->CageCrew : 0;
								return Self.IsValid() && Self->bVoted
									? FText::Format(NSLOCTEXT("DCC", "CageWaiting", "READY ({0} of {1})  -  click to wait"), FText::AsNumber(Up), FText::AsNumber(Of))
									: FText::Format(NSLOCTEXT("DCC", "CageSend", "SEND THE CAGE UP ({0} of {1})"), FText::AsNumber(Up), FText::AsNumber(Of));
							})
						]
					]
				]
			]
		];
}
