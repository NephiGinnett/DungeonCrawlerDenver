#include "UI/DCCRoundOverWidget.h"

#include "Arcade/DCCArcade.h"
#include "Arcade/DCCArcadeArena.h"
#include "Arcade/DCCWalletComponent.h"
#include "Characters/DCCCharacterBase.h"
#include "Vehicles/DCCRover.h"
#include "GameFramework/PlayerController.h"
#include "Styling/CoreStyle.h"
#include "Widgets/Input/SButton.h"
#include "Widgets/Layout/SBorder.h"
#include "Widgets/Layout/SBox.h"
#include "Widgets/SBoxPanel.h"
#include "Widgets/Text/STextBlock.h"

namespace
{
	FSlateFontInfo Font(int32 Size, bool bBold = true)
	{
		return FCoreStyle::GetDefaultFontStyle(bBold ? "Bold" : "Regular", Size);
	}
}

void UDCCRoundOverWidget::Vote(bool bPlayAgain)
{
	MyVote = bPlayAgain ? 1 : 2;
	const APlayerController* PC = GetOwningPlayer();
	if (const ADCCCharacterBase* Crawler = PC ? Cast<ADCCCharacterBase>(PC->GetPawn()) : nullptr)
	{
		if (Crawler->Wallet)
		{
			Crawler->Wallet->Server_ArcadeVote(bPlayAgain);
		}
	}
	else if (ADCCRover* Rover = PC ? Cast<ADCCRover>(PC->GetPawn()) : nullptr)
	{
		// Driving (Repo Rally, Neon Mile): the vote leaves from the car (2026-11-13: it was being dropped).
		Rover->Server_ArcadeVote(bPlayAgain);
	}
}

TSharedRef<SWidget> UDCCRoundOverWidget::RebuildWidget()
{
	TWeakObjectPtr<UDCCRoundOverWidget> Self(this);

	auto Big = [Self](bool bAgain)
	{
		const FLinearColor Colour = bAgain ? FLinearColor(1.0f, 0.55f, 0.2f) : FLinearColor(0.85f, 0.3f, 0.8f);
		return SNew(SBox)
			.WidthOverride(300.0f)
			[
				SNew(SButton)
				.IsFocusable(false)
				.HAlign(HAlign_Center)
				.ContentPadding(FMargin(16.0f, 14.0f))
				.ButtonColorAndOpacity_Lambda([Self, bAgain, Colour]()
				{
					const int32 Mine = Self.IsValid() ? Self->MyVote : 0;
					const bool bChosen = Mine == (bAgain ? 1 : 2);
					return Mine == 0 || bChosen ? Colour : Colour * 0.45f;
				})
				.OnClicked_Lambda([Self, bAgain]()
				{
					if (Self.IsValid()) { Self->Vote(bAgain); }
					return FReply::Handled();
				})
				[
					SNew(SVerticalBox)
					+ SVerticalBox::Slot().AutoHeight().HAlign(HAlign_Center)
					[
						SNew(STextBlock).Font(Font(20)).ColorAndOpacity(FLinearColor::White)
						.Text(bAgain ? NSLOCTEXT("DCC", "RoundAgain", "PLAY AGAIN") : NSLOCTEXT("DCC", "RoundDen", "BACK TO THE DEN"))
					]
					+ SVerticalBox::Slot().AutoHeight().HAlign(HAlign_Center)
					[
						SNew(STextBlock).Font(Font(11, false)).ColorAndOpacity(FLinearColor(1, 1, 1, 0.85f))
						.Text_Lambda([Self, bAgain]()
						{
							const ADCCArcadeArena* A = Self.IsValid() ? Self->Arena.Get() : nullptr;
							const int32 N = A ? (bAgain ? A->VotesAgain : A->VotesDen) : 0;
							return FText::Format(NSLOCTEXT("DCC", "RoundVotes", "{0} {0}|plural(one=vote,other=votes)"), FText::AsNumber(N));
						})
					]
				]
			];
	};

	return
		SNew(SBox)
		.HAlign(HAlign_Center)
		.VAlign(VAlign_Bottom)
		.Padding(FMargin(0.0f, 0.0f, 0.0f, 120.0f))
		[
			SNew(SBorder)
			.BorderImage(FCoreStyle::Get().GetBrush("WhiteBrush"))
			.BorderBackgroundColor(FLinearColor(0.03f, 0.02f, 0.05f, 0.9f))
			.Padding(FMargin(28.0f, 20.0f))
			[
				SNew(SVerticalBox)
				+ SVerticalBox::Slot().AutoHeight().HAlign(HAlign_Center)
				[
					SNew(STextBlock).Font(Font(24)).ColorAndOpacity(FLinearColor(1.0f, 0.83f, 0.42f))
					.Text(NSLOCTEXT("DCC", "RoundOver", "ROUND OVER"))
				]
				+ SVerticalBox::Slot().AutoHeight().HAlign(HAlign_Center).Padding(0.0f, 2.0f, 0.0f, 14.0f)
				[
					SNew(STextBlock).Font(Font(11, false)).ColorAndOpacity(FLinearColor(0.75f, 0.72f, 0.8f))
					.Text_Lambda([Self]()
					{
						const ADCCArcadeArena* A = Self.IsValid() ? Self->Arena.Get() : nullptr;
						if (!A) { return FText::GetEmpty(); }
						return FText::Format(NSLOCTEXT("DCC", "RoundDecide",
							"What next? Majority wins, ties go to the Den.   {0} of {1} voted   -   {2} s"),
							FText::AsNumber(A->VotesAgain + A->VotesDen), FText::AsNumber(A->VotersExpected), FText::AsNumber(A->SecondsToDecide));
					})
				]
				+ SVerticalBox::Slot().AutoHeight()
				[
					SNew(SHorizontalBox)
					+ SHorizontalBox::Slot().AutoWidth().Padding(0.0f, 0.0f, 16.0f, 0.0f)[ Big(true) ]
					+ SHorizontalBox::Slot().AutoWidth()[ Big(false) ]
				]
			]
		];
}
