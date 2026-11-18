#include "UI/DCCCharacterSelectWidget.h"

#include "Arcade/DCCArcade.h"
#include "Session/DCCProfile.h"
#include "Session/DCCSession.h"
#include "UI/DCCHUD.h"
#include "Engine/World.h"
#include "GameFramework/PlayerController.h"
#include "InputCoreTypes.h"
#include "Styling/CoreStyle.h"
#include "Widgets/Input/SButton.h"
#include "Widgets/Input/SEditableTextBox.h"
#include "Arcade/DCCWalletComponent.h"
#include "Characters/DCCCharacterBase.h"
#include "Widgets/Layout/SBorder.h"
#include "Widgets/Layout/SBox.h"
#include "Widgets/Layout/SSpacer.h"
#include "Widgets/Layout/SUniformGridPanel.h"
#include "Widgets/SBoxPanel.h"
#include "Widgets/Text/STextBlock.h"
#include "UI/DCCMenuStyle.h"
#include "Session/DCCBuildStamp.h"

namespace
{
	const FLinearColor Gold(1.0f, 0.83f, 0.42f);
	const FLinearColor Pink(1.0f, 0.45f, 0.9f);
	const FLinearColor Ink(0.94f, 0.91f, 0.86f);
	const FLinearColor Dim(0.62f, 0.60f, 0.66f);
	const FLinearColor Good(0.45f, 0.95f, 0.45f);
	const FLinearColor Bad(1.0f, 0.45f, 0.40f);

	FSlateFontInfo Font(int32 Size, bool bBold = false)
	{
		return FCoreStyle::GetDefaultFontStyle(bBold ? "Bold" : "Regular", Size);
	}

	/**
	 * A big menu button: a coloured bar on the left, a title, and a line under
	 * it saying what happens. Not keyboard-focusable, so Enter always means Play
	 * rather than "press whatever was clicked last".
	 */
	TSharedRef<SWidget> BigButton(const FText& Title, const FText& Subtitle, const FLinearColor& Colour,
		int32 TitleSize, TAttribute<bool> Enabled, TFunction<void()> OnClick)
	{
		// A glass card (UI/DCCMenuStyle): a neon bar on the left, the title in light spaced type, and an edge that
		// lights up in the card's colour on hover.
		return SNew(SButton)
			.IsFocusable(false)
			.IsEnabled(Enabled)
			.ContentPadding(FMargin(0.0f))
			.ButtonStyle(DCCMenu::CardStyle(Colour, false))
			.OnClicked_Lambda([OnClick]() { OnClick(); return FReply::Handled(); })
			[
				SNew(SHorizontalBox)
				+ SHorizontalBox::Slot().AutoWidth().Padding(FMargin(10.0f, 12.0f, 0.0f, 12.0f))
				[
					SNew(SBox).WidthOverride(3.0f)[ SNew(SBorder).BorderImage(DCCMenu::Rounded(Colour, 1.5f)) ]
				]
				+ SHorizontalBox::Slot().FillWidth(1.0f).Padding(FMargin(16.0f, 12.0f, 18.0f, 12.0f))
				[
					SNew(SVerticalBox)
					+ SVerticalBox::Slot().AutoHeight()
					[
						SNew(STextBlock).Text(Title).Font(DCCMenu::Font(TitleSize, "Light", 60)).ColorAndOpacity(Colour)
					]
					+ SVerticalBox::Slot().AutoHeight().Padding(0.0f, 2.0f, 0.0f, 0.0f)
					[
						SNew(STextBlock).Text(Subtitle).Font(Font(10)).ColorAndOpacity(Dim).AutoWrapText(true)
					]
				]
			];
	}
}

void UDCCCharacterSelectWidget::NativeOnInitialized()
{
	Super::NativeOnInitialized();
	SetIsFocusable(true);
}

void UDCCCharacterSelectWidget::Refresh()
{
	if (DCCSession::HasPendingError())
	{
		SetMessage(DCCSession::TakeLastError(), true);
	}
	// Put the remembered crawler on the stage.
	DCCSession::SwitchCrawler(GetOwningPlayer(), DCCSession::GetPreferredCrawler());
}

void UDCCCharacterSelectWidget::SetMessage(const FString& Text, bool bError)
{
	Message = Text;
	bMessageIsError = bError;
}

FText UDCCCharacterSelectWidget::RecordsFor(FName CrawlerId) const
{
	TArray<FString> Parts;
	for (const DCCArcade::FGame& Game : DCCArcade::Games())
	{
		const float Value = DCCProfile::GetForKey(CrawlerId.ToString(), Game.Id, Game.RecordStat, -1.0f);
		if (Value > 0.0f)
		{
			Parts.Add(FString::Printf(TEXT("%s  %s"), Game.Title, *DCCArcade::FormatRecord(Game, Value)));
		}
	}
	return FText::FromString(Parts.IsEmpty()
		? FString(TEXT("No records yet. Go set some."))
		: FString::Join(Parts, TEXT("     ")));
}

TSharedRef<SWidget> UDCCCharacterSelectWidget::RebuildWidget()
{
	TWeakObjectPtr<UDCCCharacterSelectWidget> Self(this);
	const TAttribute<bool> NotLeaving = TAttribute<bool>::CreateLambda([Self]() { return Self.IsValid() && !Self->bLeaving; });

	// Crawlers: a 3 x 2 grid of name chips, the chosen one lit gold.
	TSharedRef<SUniformGridPanel> Crawlers = SNew(SUniformGridPanel).SlotPadding(FMargin(4.0f));
	const TArray<FName>& Roster = DCCSession::Roster();
	for (int32 i = 0; i < Roster.Num(); ++i)
	{
		const FName Id = Roster[i];
		Crawlers->AddSlot(i % 3, i / 3)
		[
			SNew(SButton)
			.IsFocusable(false)
			.ContentPadding(FMargin(0.0f))
			.ButtonStyle(&FCoreStyle::Get().GetWidgetStyle<FButtonStyle>("NoBorder"))
			.IsEnabled(NotLeaving)
			.OnClicked_Lambda([Self, Id]()
			{
				if (Self.IsValid())
				{
					Self->Pick(Id);
				}
				return FReply::Handled();
			})
			[
				// A flat fill rather than a button tint: the default button image is
				// dark grey, so tinting it gold came out a muddy brown.
				SNew(SBorder)
				.HAlign(HAlign_Center)
				.Padding(FMargin(8.0f, 10.0f))
				.BorderImage_Lambda([Id]()
				{
					return DCCSession::GetPreferredCrawler() == Id
						? DCCMenu::Rounded(DCCMenu::WithAlpha(DCCMenu::Gold, 0.92f), 6.0f, DCCMenu::Gold, 1.0f)
						: DCCMenu::Rounded(DCCMenu::Card, 6.0f, FLinearColor(1.0f, 1.0f, 1.0f, 0.1f), 1.0f);
				})
				[
					SNew(STextBlock).Text(FText::FromName(Id)).Font(DCCMenu::Font(15, "Bold", 40))
					.ColorAndOpacity_Lambda([Id]()
					{
						return DCCSession::GetPreferredCrawler() == Id ? FSlateColor(FLinearColor(0.08f, 0.05f, 0.02f)) : FSlateColor(Ink);
					})
				]
			]
		];
	}

	// "Rejoin K7M2Q-9TB4X": only when there is a last party to go back to.
	const FString LastJoin = DCCSession::GetLastJoinTarget();

	TSharedRef<SVerticalBox> Panel = SNew(SVerticalBox)

		// Title
		+ SVerticalBox::Slot().AutoHeight()
		[
			SNew(STextBlock).Text(NSLOCTEXT("DCC", "MenuKicker", "SYSTEM  /  DUNGEON CRAWLER DENVER")).Font(DCCMenu::Caps(9, 260)).ColorAndOpacity(DCCMenu::Cyan)
		]
		+ SVerticalBox::Slot().AutoHeight().Padding(0.0f, 0.0f, 0.0f, 22.0f)
		[
			SNew(STextBlock).Text(NSLOCTEXT("DCC", "MenuTitle", "THE ARCADE")).Font(DCCMenu::Font(40, "Light", 80)).ColorAndOpacity(DCCMenu::Ink)
		]

		// Who you are
		+ SVerticalBox::Slot().AutoHeight()
		[
			SNew(STextBlock).Text(NSLOCTEXT("DCC", "MenuYouAre", "YOU ARE")).Font(DCCMenu::Caps(9, 240)).ColorAndOpacity(DCCMenu::Cyan)
		]
		+ SVerticalBox::Slot().AutoHeight().Padding(-4.0f, 4.0f, -4.0f, 4.0f)
		[
			Crawlers
		]
		+ SVerticalBox::Slot().AutoHeight().Padding(0.0f, 0.0f, 0.0f, 10.0f)
		[
			SNew(STextBlock).Font(Font(10)).ColorAndOpacity(Dim).AutoWrapText(true)
			.Text_Lambda([Self]() { return Self.IsValid() ? Self->RecordsFor(DCCSession::GetPreferredCrawler()) : FText::GetEmpty(); })
		]

		// Password: one box, a lock state, and CHANGE. A PC that has typed it once remembers it.
		+ SVerticalBox::Slot().AutoHeight().Padding(0.0f, 0.0f, 0.0f, 6.0f)
		[
			SNew(SHorizontalBox)
			+ SHorizontalBox::Slot().AutoWidth().VAlign(VAlign_Center).Padding(0.0f, 0.0f, 10.0f, 0.0f)
			[
				SNew(STextBlock).Font(Font(11, true))
				.Text_Lambda([Self]() { return Self.IsValid() && Self->IsUnlocked() ? NSLOCTEXT("DCC", "MenuUnlocked", "UNLOCKED") : NSLOCTEXT("DCC", "MenuLocked", "LOCKED"); })
				.ColorAndOpacity_Lambda([Self]() { return FSlateColor(Self.IsValid() && Self->IsUnlocked() ? Good : Bad); })
			]
			+ SHorizontalBox::Slot().FillWidth(1.0f).VAlign(VAlign_Center)
			[
				SAssignNew(PasswordBox, SEditableTextBox)
				.Style(DCCMenu::TextBoxStyle())
				.Font(Font(12))
				.IsPassword(true)
				.IsEnabled(NotLeaving)
				.HintText_Lambda([]()
				{
					return DCCSession::GetSavedPasswordHash(DCCSession::GetPreferredCrawler()).IsEmpty()
						? FText::Format(NSLOCTEXT("DCC", "MenuPwHint", "{0}'s password"), FText::FromName(DCCSession::GetPreferredCrawler()))
						: NSLOCTEXT("DCC", "MenuPwSaved", "remembered on this PC");
				})
				.OnTextCommitted_Lambda([Self](const FText&, ETextCommit::Type Commit)
				{
					if (Self.IsValid() && Commit == ETextCommit::OnEnter) { Self->Play(); }
				})
			]
			+ SHorizontalBox::Slot().AutoWidth().VAlign(VAlign_Center).Padding(8.0f, 0.0f, 0.0f, 0.0f)
			[
				SNew(SButton)
				.IsFocusable(false)
				.ContentPadding(FMargin(10.0f, 4.0f))
				.IsEnabled(NotLeaving)
				.ButtonStyle(DCCMenu::ButtonStyle(DCCMenu::Cyan, false))
				.OnClicked_Lambda([Self]()
				{
					if (Self.IsValid() && Self->RequireUnlocked()) { Self->bChangingPassword = !Self->bChangingPassword; }
					return FReply::Handled();
				})
				[
					SNew(STextBlock).Text(NSLOCTEXT("DCC", "MenuPwChange", "CHANGE")).Font(DCCMenu::Caps(9, 160)).ColorAndOpacity(Ink)
				]
			]
		]
		+ SVerticalBox::Slot().AutoHeight().Padding(0.0f, 0.0f, 0.0f, 16.0f)
		[
			SNew(SHorizontalBox)
			.Visibility_Lambda([Self]() { return Self.IsValid() && Self->bChangingPassword ? EVisibility::Visible : EVisibility::Collapsed; })
			+ SHorizontalBox::Slot().FillWidth(1.0f).VAlign(VAlign_Center)
			[
				SAssignNew(NewPasswordBox, SEditableTextBox)
				.Style(DCCMenu::TextBoxStyle())
				.Font(Font(12))
				.IsPassword(true)
				.HintText(NSLOCTEXT("DCC", "MenuPwNew", "new password"))
				.OnTextCommitted_Lambda([Self](const FText&, ETextCommit::Type Commit)
				{
					if (Self.IsValid() && Commit == ETextCommit::OnEnter) { Self->SaveNewPassword(); }
				})
			]
			+ SHorizontalBox::Slot().AutoWidth().VAlign(VAlign_Center).Padding(8.0f, 0.0f, 0.0f, 0.0f)
			[
				SNew(SButton)
				.IsFocusable(false)
				.ContentPadding(FMargin(10.0f, 4.0f))
				.ButtonStyle(DCCMenu::ButtonStyle(DCCMenu::Mint, false))
				.OnClicked_Lambda([Self]() { if (Self.IsValid()) { Self->SaveNewPassword(); } return FReply::Handled(); })
				[
					SNew(STextBlock).Text(NSLOCTEXT("DCC", "MenuPwSave", "SAVE")).Font(DCCMenu::Caps(9, 160)).ColorAndOpacity(Ink)
				]
			]
		]

		// The big three
		+ SVerticalBox::Slot().AutoHeight().Padding(0.0f, 0.0f, 0.0f, 10.0f)
		[
			BigButton(NSLOCTEXT("DCC", "MenuPlay", "PLAY"),
				NSLOCTEXT("DCC", "MenuPlaySub", "Into the Safe Room. Stand on a game to vote for it."),
				Pink, 30, NotLeaving, [Self]() { if (Self.IsValid()) { Self->Play(); } })
		]
		+ SVerticalBox::Slot().AutoHeight().Padding(0.0f, 0.0f, 0.0f, 10.0f)
		[
			BigButton(NSLOCTEXT("DCC", "MenuHost", "HOST A PARTY"),
				NSLOCTEXT("DCC", "MenuHostSub", "Open the Safe Room to your friends. You get an invite to copy and send them."),
				Gold, 20, NotLeaving, [Self]() { if (Self.IsValid()) { Self->HostParty(); } })
		]
		+ SVerticalBox::Slot().AutoHeight().Padding(0.0f, 0.0f, 0.0f, 10.0f)
		[
			BigButton(NSLOCTEXT("DCC", "MenuJoin", "JOIN A PARTY"),
				NSLOCTEXT("DCC", "MenuJoinSub", "Copy the invite message your host sent you, then click here."),
				FLinearColor(0.45f, 0.85f, 1.0f), 20, NotLeaving, [Self]() { if (Self.IsValid()) { Self->JoinParty(); } })
		];

	if (!LastJoin.IsEmpty())
	{
		Panel->AddSlot().AutoHeight().Padding(0.0f, 0.0f, 0.0f, 10.0f)
		[
			BigButton(NSLOCTEXT("DCC", "MenuRejoin", "REJOIN LAST PARTY"),
				FText::FromString(LastJoin),
				FLinearColor(0.55f, 0.75f, 0.85f), 14, NotLeaving, [Self]() { if (Self.IsValid()) { Self->RejoinLast(); } })
		];
	}

	Panel->AddSlot().AutoHeight().Padding(0.0f, 4.0f, 0.0f, 8.0f)
	[
		SNew(STextBlock).Font(Font(12, true)).AutoWrapText(true)
		.Text_Lambda([Self]() { return Self.IsValid() ? FText::FromString(Self->Message) : FText::GetEmpty(); })
		.ColorAndOpacity_Lambda([Self]() { return FSlateColor(Self.IsValid() && Self->bMessageIsError ? Bad : Good); })
		.Visibility_Lambda([Self]() { return Self.IsValid() && !Self->Message.IsEmpty() ? EVisibility::Visible : EVisibility::Collapsed; })
	];

	Panel->AddSlot().FillHeight(1.0f)[ SNew(SSpacer) ];

	Panel->AddSlot().AutoHeight()
	[
		SNew(SHorizontalBox)
		+ SHorizontalBox::Slot().FillWidth(1.0f).VAlign(VAlign_Center)
		[
			// Which build this is. Everyone in a party needs the same one (Session/DCCBuildStamp).
			SNew(STextBlock).Font(DCCMenu::Caps(7, 160)).ColorAndOpacity(DCCMenu::Dim)
			.Text(FText::FromString(FString::Printf(TEXT("BUILD  %s"), *DCCBuildStamp::Get().ToUpper())))
		]
		+ SHorizontalBox::Slot().AutoWidth().Padding(0.0f, 0.0f, 8.0f, 0.0f)
		[
			SNew(SButton)
			.IsFocusable(false)
			.ContentPadding(FMargin(14.0f, 6.0f))
			.IsEnabled(NotLeaving)
			.ButtonStyle(DCCMenu::ButtonStyle(DCCMenu::Gold, false))
			.OnClicked_Lambda([Self]() { if (Self.IsValid()) { Self->OpenShop(); } return FReply::Handled(); })
			[
				SNew(STextBlock).Text(NSLOCTEXT("DCC", "MenuShop", "PRIZES")).Font(DCCMenu::Caps(9, 160)).ColorAndOpacity(Ink)
			]
		]
		+ SHorizontalBox::Slot().AutoWidth().Padding(0.0f, 0.0f, 8.0f, 0.0f)
		[
			SNew(SButton)
			.IsFocusable(false)
			.ContentPadding(FMargin(14.0f, 6.0f))
			.IsEnabled(NotLeaving)
			.ButtonStyle(DCCMenu::ButtonStyle(DCCMenu::Cyan, false))
			.OnClicked_Lambda([Self]() { if (Self.IsValid()) { Self->OpenPoseBench(); } return FReply::Handled(); })
			[
				SNew(STextBlock).Text(NSLOCTEXT("DCC", "MenuBench", "POSE BENCH")).Font(DCCMenu::Caps(9, 160)).ColorAndOpacity(Ink)
			]
		]
		+ SHorizontalBox::Slot().AutoWidth()
		[
			SNew(SButton)
			.IsFocusable(false)
			.ContentPadding(FMargin(18.0f, 6.0f))
			.ButtonStyle(DCCMenu::ButtonStyle(DCCMenu::Pink, false))
			.OnClicked_Lambda([Self]() { if (Self.IsValid()) { Self->Quit(); } return FReply::Handled(); })
			[
				SNew(STextBlock).Text(NSLOCTEXT("DCC", "MenuQuit", "QUIT")).Font(DCCMenu::Caps(9, 160)).ColorAndOpacity(Ink)
			]
		]
	];

	return
		SNew(SHorizontalBox)
		+ SHorizontalBox::Slot().AutoWidth()
		[
			SNew(SBox)
			.WidthOverride(500.0f)
			[
				SNew(SBorder)
				.BorderImage(DCCMenu::Rounded(FLinearColor(0.012f, 0.01f, 0.03f, 0.9f), 0.0f))
				.Padding(FMargin(40.0f, 44.0f, 40.0f, 28.0f))
				[
					Panel
				]
			]
		]
		// A neon edge down the panel's right side, in the System's colour.
		+ SHorizontalBox::Slot().AutoWidth()
		[
			SNew(SBox).WidthOverride(2.0f)[ SNew(SBorder).BorderImage(DCCMenu::Rounded(DCCMenu::WithAlpha(DCCMenu::Cyan, 0.55f), 0.0f)) ]
		]
		+ SHorizontalBox::Slot().FillWidth(1.0f)
		[
			SNew(SSpacer)
		];
}

FReply UDCCCharacterSelectWidget::NativeOnKeyDown(const FGeometry& InGeometry, const FKeyEvent& InKeyEvent)
{
	const FKey Key = InKeyEvent.GetKey();
	if (Key == EKeys::Up || Key == EKeys::Left || Key == EKeys::Gamepad_DPad_Up || Key == EKeys::Gamepad_DPad_Left)
	{
		Step(-1);
		return FReply::Handled();
	}
	if (Key == EKeys::Down || Key == EKeys::Right || Key == EKeys::Gamepad_DPad_Down || Key == EKeys::Gamepad_DPad_Right)
	{
		Step(1);
		return FReply::Handled();
	}
	if (Key == EKeys::Enter || Key == EKeys::Gamepad_FaceButton_Bottom)
	{
		if (!InKeyEvent.IsRepeat())
		{
			Play();
		}
		return FReply::Handled();
	}
	return Super::NativeOnKeyDown(InGeometry, InKeyEvent);
}

FReply UDCCCharacterSelectWidget::NativeOnMouseButtonDown(const FGeometry& InGeometry, const FPointerEvent& InMouseEvent)
{
	// A click on the stage side keeps keyboard focus here, so Enter still works.
	return FReply::Handled().SetUserFocus(TakeWidget());
}

void UDCCCharacterSelectWidget::Step(int32 Direction)
{
	const TArray<FName>& Roster = DCCSession::Roster();
	const int32 Current = FMath::Max(0, Roster.IndexOfByKey(DCCSession::GetPreferredCrawler()));
	Pick(Roster[(Current + Direction + Roster.Num()) % Roster.Num()]);
}

void UDCCCharacterSelectWidget::Pick(FName CrawlerId)
{
	if (!bLeaving)
	{
		if (CrawlerId != DCCSession::GetPreferredCrawler())
		{
			// A different crawler: whatever was typed was for the last one.
			if (PasswordBox.IsValid()) { PasswordBox->SetText(FText::GetEmpty()); }
			if (NewPasswordBox.IsValid()) { NewPasswordBox->SetText(FText::GetEmpty()); }
			bChangingPassword = false;
		}
		DCCSession::SwitchCrawler(GetOwningPlayer(), CrawlerId);
	}
}

void UDCCCharacterSelectWidget::ReleaseSlateResources(bool bReleaseChildren)
{
	Super::ReleaseSlateResources(bReleaseChildren);
	PasswordBox.Reset();
	NewPasswordBox.Reset();
}

FString UDCCCharacterSelectWidget::PasswordHash() const
{
	const FName Crawler = DCCSession::GetPreferredCrawler();
	const FString Typed = PasswordBox.IsValid() ? PasswordBox->GetText().ToString() : FString();
	return Typed.IsEmpty() ? DCCSession::GetSavedPasswordHash(Crawler) : DCCProfile::HashPassword(Crawler.ToString(), Typed);
}

bool UDCCCharacterSelectWidget::IsUnlocked() const
{
	return DCCProfile::CheckPassword(DCCSession::GetPreferredCrawler().ToString(), PasswordHash());
}

bool UDCCCharacterSelectWidget::RequireUnlocked()
{
	const FName Crawler = DCCSession::GetPreferredCrawler();
	if (!IsUnlocked())
	{
		SetMessage(FString::Printf(TEXT("%s is locked. Type %s's password in the box under the crawlers."), *Crawler.ToString(), *Crawler.ToString()), true);
		return false;
	}
	DCCSession::SavePasswordHash(Crawler, PasswordHash());
	return true;
}

void UDCCCharacterSelectWidget::SaveNewPassword()
{
	const FName Crawler = DCCSession::GetPreferredCrawler();
	const FString New = NewPasswordBox.IsValid() ? NewPasswordBox->GetText().ToString() : FString();
	if (New.Len() < 3)
	{
		SetMessage(TEXT("A new password needs at least 3 characters."), true);
		return;
	}
	if (!RequireUnlocked())
	{
		return;
	}
	const FString NewHash = DCCProfile::HashPassword(Crawler.ToString(), New);
	if (DCCProfile::ChangePassword(Crawler.ToString(), PasswordHash(), NewHash))
	{
		DCCSession::SavePasswordHash(Crawler, NewHash);
		if (PasswordBox.IsValid()) { PasswordBox->SetText(FText::GetEmpty()); }
		if (NewPasswordBox.IsValid()) { NewPasswordBox->SetText(FText::GetEmpty()); }
		bChangingPassword = false;
		SetMessage(FString::Printf(TEXT("%s's password is changed. This PC will remember it."), *Crawler.ToString()), false);
	}
	else
	{
		SetMessage(TEXT("Could not change the password."), true);
	}
}

void UDCCCharacterSelectWidget::Play()
{
	if (bLeaving || !RequireUnlocked())
	{
		return;
	}
	bLeaving = DCCArcade::Travel(GetWorld(), TEXT("Lobby"));
	SetMessage(bLeaving ? TEXT("Opening the Safe Room...") : TEXT("Could not open the Safe Room. Try again."), !bLeaving);
}

void UDCCCharacterSelectWidget::HostParty()
{
	if (bLeaving || !RequireUnlocked())
	{
		return;
	}
	// Open this world to the network, then travel: Travel adds ?listen for a
	// listen server, so the Den comes up hosted. The party menu opens there
	// with the invite ready to copy.
	FString Error;
	if (!DCCSession::StartHosting(GetWorld(), GetOwningPlayer(), Error))
	{
		SetMessage(Error, true);
		return;
	}
	bLeaving = DCCArcade::Travel(GetWorld(), TEXT("Lobby"));
	SetMessage(bLeaving ? TEXT("Hosting. Opening the Safe Room...") : TEXT("Could not open the Safe Room. Try again."), !bLeaving);
}

void UDCCCharacterSelectWidget::JoinParty()
{
	if (bLeaving)
	{
		return;
	}
	// The host has the saves, so the host decides whether the password is right. Send what was typed (or what
	// this PC remembers); a wrong one comes back here as an error.
	if (PasswordHash().IsEmpty())
	{
		SetMessage(FString::Printf(TEXT("Type %s's password in the box under the crawlers first."), *DCCSession::GetPreferredCrawler().ToString()), true);
		return;
	}
	DCCSession::SavePasswordHash(DCCSession::GetPreferredCrawler(), PasswordHash());
	FString Target;
	if (!DCCSession::JoinFromClipboard(GetOwningPlayer(), Target))
	{
		SetMessage(TEXT("No invite found. Copy the whole message your host sent (Ctrl+C), then click JOIN A PARTY again."), true);
		return;
	}
	bLeaving = true;
	SetMessage(FString::Printf(TEXT("Joining the party at %s..."), *Target), false);
}

void UDCCCharacterSelectWidget::RejoinLast()
{
	const FString LastJoin = DCCSession::GetLastJoinTarget();
	if (bLeaving || LastJoin.IsEmpty())
	{
		return;
	}
	if (PasswordHash().IsEmpty())
	{
		SetMessage(FString::Printf(TEXT("Type %s's password in the box under the crawlers first."), *DCCSession::GetPreferredCrawler().ToString()), true);
		return;
	}
	DCCSession::SavePasswordHash(DCCSession::GetPreferredCrawler(), PasswordHash());
	bLeaving = true;
	SetMessage(FString::Printf(TEXT("Rejoining %s..."), *LastJoin), false);
	DCCSession::Join(GetOwningPlayer(), LastJoin);
}

void UDCCCharacterSelectWidget::OpenPoseBench()
{
	APlayerController* PC = GetOwningPlayer();
	if (ADCCHUD* Hud = PC ? Cast<ADCCHUD>(PC->GetHUD()) : nullptr)
	{
		if (!Hud->IsGearBenchOpen())
		{
			Hud->ToggleGearBench(); // its own Close (or F3) brings the menu back
		}
	}
}

void UDCCCharacterSelectWidget::OpenShop()
{
	if (!RequireUnlocked())
	{
		return; // the counter spends this crawler's tickets
	}
	APlayerController* PC = GetOwningPlayer();
	if (ADCCHUD* Hud = PC ? Cast<ADCCHUD>(PC->GetHUD()) : nullptr)
	{
		Hud->OpenCosmeticShop(); // its DONE button brings the menu back
	}
}

void UDCCCharacterSelectWidget::Quit()
{
	if (APlayerController* PC = GetOwningPlayer())
	{
		PC->ConsoleCommand(TEXT("quit"));
	}
}
