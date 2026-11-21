#include "UI/DCCSessionMenuWidget.h"
#include "InputCoreTypes.h"
#include "Session/DCCUPnP.h"

#include "Characters/DCCCharacterBase.h"
#include "Onboarding/DCCOnboardingComponent.h"
#include "Rooms/DCCTaleFloor.h"
#include "Arcade/DCCArcade.h"
#include "Arcade/DCCWalletComponent.h"
#include "Session/DCCProfile.h"
#include "Session/DCCSession.h"
#include "Engine/World.h"
#include "GameFramework/GameStateBase.h"
#include "GameFramework/PlayerController.h"
#include "GameFramework/PlayerState.h"
#include "HAL/PlatformApplicationMisc.h"
#include "Styling/CoreStyle.h"
#include "Widgets/Images/SImage.h"
#include "Widgets/Input/SButton.h"
#include "Widgets/Input/SEditableTextBox.h"
#include "Widgets/Layout/SBorder.h"
#include "Widgets/Layout/SBox.h"
#include "Widgets/Layout/SScrollBox.h"
#include "Widgets/Layout/SSpacer.h"
#include "Widgets/Layout/SWrapBox.h"
#include "Widgets/SBoxPanel.h"
#include "Widgets/Text/STextBlock.h"
#include "UI/DCCMenuStyle.h"

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

	// The System's window style (UI/DCCMenuStyle, 2026-11-14): spaced capitals for headings, rounded glass buttons.
	TSharedRef<STextBlock> Heading(const FText& Text)
	{
		return SNew(STextBlock).Text(Text).Font(DCCMenu::Caps(9, 240)).ColorAndOpacity(DCCMenu::Cyan);
	}

	TSharedRef<SWidget> MenuButton(const FText& Label, FOnClicked OnClicked)
	{
		return DCCMenu::Button(Label, DCCMenu::Cyan, OnClicked, false, true, 10, FMargin(14.0f, 7.0f));
	}
}

ENetMode UDCCSessionMenuWidget::GetNetModeSafe() const
{
	const UWorld* World = GetWorld();
	return World ? World->GetNetMode() : NM_Standalone;
}

void UDCCSessionMenuWidget::Refresh()
{
	DCCSession::EnsureFailureHandlers();
	// Hosting: ask the router for an internet invite (once; the menu redraws when it answers).
	if (GetNetModeSafe() == NM_ListenServer)
	{
		DCCUPnP::Start(DCCSession::DefaultPort);
	}
	SeenUPnPVersion = DCCUPnP::GetVersion();
	Addresses = DCCSession::GetHostAddresses();
	RebuildInviteList();

	if (DCCSession::HasPendingError())
	{
		Message = DCCSession::TakeLastError();
		bMessageIsError = true;
	}
}

TSharedRef<SWidget> UDCCSessionMenuWidget::RebuildWidget()
{
	TWeakObjectPtr<UDCCSessionMenuWidget> Self(this);

	// The crawler picker that used to be here is gone (2026-11-14). A client could never switch mid-party anyway,
	// and with passwords a switch needs the main menu's password box: one place to choose who you are.

	auto Visible = [Self](TFunction<bool(const UDCCSessionMenuWidget&)> Test)
	{
		return TAttribute<EVisibility>::CreateLambda([Self, Test]()
		{
			return Self.IsValid() && Test(*Self) ? EVisibility::Visible : EVisibility::Collapsed;
		});
	};

	SAssignNew(JoinBox, SEditableTextBox)
		.Style(DCCMenu::TextBoxStyle())
		.Font(Font(13))
		.HintText(NSLOCTEXT("DCC", "PartyJoinHint", "Invite code (K7M2Q-9TB4X) or address"))
		.Text(FText::FromString(DCCSession::GetLastJoinTarget()))
		.OnTextCommitted_Lambda([Self](const FText&, ETextCommit::Type Commit)
		{
			if (Self.IsValid() && Commit == ETextCommit::OnEnter)
			{
				Self->JoinFromBox();
			}
		});

	SAssignNew(InviteList, SVerticalBox);

	TSharedRef<SWidget> Root =
		SNew(SBox)
		.HAlign(HAlign_Center)
		.VAlign(VAlign_Center)
		[
			SNew(SBox)
			.WidthOverride(640.0f)
			[
				DCCMenu::Window(DCCMenu::Cyan, SNew(SBox)
				[
					SNew(SVerticalBox)

					// Title and status
					+ SVerticalBox::Slot().AutoHeight()
					[
						DCCMenu::Header(NSLOCTEXT("DCC", "PartyKicker", "SYSTEM  /  PARTY"), NSLOCTEXT("DCC", "PartyTitle", "PARTY"), FSlateColor(DCCMenu::Ink))
					]
					+ SVerticalBox::Slot().AutoHeight().Padding(0, 2, 0, 14)
					[
						SNew(STextBlock)
						.Text_Lambda([Self]() { return Self.IsValid() ? Self->GetStatusText() : FText::GetEmpty(); })
						.Font(Font(12))
						.ColorAndOpacity(Ink)
						.AutoWrapText(true)
					]

					// Who you are, and your password. Changing crawler is on the main menu.
					+ SVerticalBox::Slot().AutoHeight()
					[
						SNew(STextBlock)
						.Text_Lambda([]() { return FText::Format(NSLOCTEXT("DCC", "PartyYouAre", "YOU ARE {0}"), FText::FromString(DCCSession::GetPreferredCrawler().ToString().ToUpper())); })
						.Font(Font(11, true))
						.ColorAndOpacity(Dim)
					]
					+ SVerticalBox::Slot().AutoHeight().Padding(0, 6, 0, 2)
					[
						SNew(SHorizontalBox)
						+ SHorizontalBox::Slot().FillWidth(1.0f).Padding(0, 0, 8, 0)
						[
							SAssignNew(OldPasswordBox, SEditableTextBox).Style(DCCMenu::TextBoxStyle()).Font(Font(12)).IsPassword(true)
							.HintText(NSLOCTEXT("DCC", "PartyPwOld", "current password"))
						]
						+ SHorizontalBox::Slot().FillWidth(1.0f).Padding(0, 0, 8, 0)
						[
							SAssignNew(NewPasswordBox, SEditableTextBox).Style(DCCMenu::TextBoxStyle()).Font(Font(12)).IsPassword(true)
							.HintText(NSLOCTEXT("DCC", "PartyPwNew", "new password"))
						]
						+ SHorizontalBox::Slot().AutoWidth()
						[
							MenuButton(NSLOCTEXT("DCC", "PartyPwChange", "Change password"), FOnClicked::CreateLambda([Self]()
							{
								if (Self.IsValid()) { Self->ChangePassword(); }
								return FReply::Handled();
							}))
						]
					]
					+ SVerticalBox::Slot().AutoHeight().Padding(0, 0, 0, 14)
					[
						SNew(STextBlock)
						.Text_Lambda([Self]() { return Self.IsValid() ? Self->GetPasswordNote() : FText::GetEmpty(); })
						.Font(Font(10))
						.ColorAndOpacity(Dim)
						.AutoWrapText(true)
					]

					// Host (solo only)
					+ SVerticalBox::Slot().AutoHeight()
					[
						SNew(SVerticalBox)
						.Visibility(Visible([](const UDCCSessionMenuWidget& W) { return W.IsSolo(); }))
						+ SVerticalBox::Slot().AutoHeight()[ Heading(NSLOCTEXT("DCC", "PartyHostHeading", "HOST")) ]
						+ SVerticalBox::Slot().AutoHeight().Padding(0, 6, 0, 2)
						[
							MenuButton(NSLOCTEXT("DCC", "PartyHost", "Host this game"), FOnClicked::CreateLambda([Self]()
							{
								if (Self.IsValid()) { Self->Host(); }
								return FReply::Handled();
							}))
						]
						+ SVerticalBox::Slot().AutoHeight().Padding(0, 0, 0, 14)
						[
							SNew(STextBlock)
							.Text(NSLOCTEXT("DCC", "PartyHostNote", "Opens this game to your friends without restarting it. You get invite codes to send them."))
							.Font(Font(10))
							.ColorAndOpacity(Dim)
							.AutoWrapText(true)
						]
					]

					// Invite (host only)
					+ SVerticalBox::Slot().AutoHeight()
					[
						SNew(SVerticalBox)
						.Visibility(Visible([](const UDCCSessionMenuWidget& W) { return W.IsHost(); }))
						+ SVerticalBox::Slot().AutoHeight()[ Heading(NSLOCTEXT("DCC", "PartyInviteHeading", "INVITE")) ]
						+ SVerticalBox::Slot().AutoHeight().Padding(0, 6, 0, 2)[ InviteList.ToSharedRef() ]
						+ SVerticalBox::Slot().AutoHeight().Padding(0, 2, 0, 14)
						[
							SNew(STextBlock)
							.Text(NSLOCTEXT("DCC", "PartyInviteNote",
								"Send it to your friends. They copy your message and click JOIN A PARTY. Not on your Wi-Fi? Everyone installs Tailscale and you send the Tailscale invite. (Or forward UDP 7777 on your router.)"))
							.Font(Font(10))
							.ColorAndOpacity(Dim)
							.AutoWrapText(true)
						]
					]

					// Join (solo only)
					+ SVerticalBox::Slot().AutoHeight()
					[
						SNew(SVerticalBox)
						.Visibility(Visible([](const UDCCSessionMenuWidget& W) { return W.IsSolo(); }))
						+ SVerticalBox::Slot().AutoHeight()[ Heading(NSLOCTEXT("DCC", "PartyJoinHeading", "JOIN A FRIEND")) ]
						+ SVerticalBox::Slot().AutoHeight().Padding(0, 6, 0, 6)
						[
							MenuButton(NSLOCTEXT("DCC", "PartyPasteJoin", "Join the invite I copied"), FOnClicked::CreateLambda([Self]()
							{
								if (Self.IsValid()) { Self->JoinFromClipboard(); }
								return FReply::Handled();
							}))
						]
						+ SVerticalBox::Slot().AutoHeight().Padding(0, 0, 0, 14)
						[
							SNew(SHorizontalBox)
							+ SHorizontalBox::Slot().FillWidth(1.0f).Padding(0, 0, 8, 0)[ JoinBox.ToSharedRef() ]
							+ SHorizontalBox::Slot().AutoWidth()
							[
								MenuButton(NSLOCTEXT("DCC", "PartyJoin", "Join"), FOnClicked::CreateLambda([Self]()
								{
									if (Self.IsValid()) { Self->JoinFromBox(); }
									return FReply::Handled();
								}))
							]
						]
					]

					// Who's here (whenever networked)
					+ SVerticalBox::Slot().AutoHeight()
					[
						SNew(SVerticalBox)
						.Visibility(Visible([](const UDCCSessionMenuWidget& W) { return !W.IsSolo(); }))
						+ SVerticalBox::Slot().AutoHeight()[ Heading(NSLOCTEXT("DCC", "PartyWho", "WHO'S HERE")) ]
						+ SVerticalBox::Slot().AutoHeight().Padding(0, 6, 0, 14)
						[
							SNew(STextBlock)
							.Text_Lambda([Self]() { return Self.IsValid() ? Self->GetPartyText() : FText::GetEmpty(); })
							.Font(Font(12))
							.ColorAndOpacity(Ink)
						]
					]

					// Message line (errors, "copied")
					+ SVerticalBox::Slot().AutoHeight().Padding(0, 0, 0, 10)
					[
						SNew(STextBlock)
						.Text_Lambda([Self]() { return Self.IsValid() ? FText::FromString(Self->Message) : FText::GetEmpty(); })
						.ColorAndOpacity_Lambda([Self]() { return FSlateColor(Self.IsValid() && Self->bMessageIsError ? Bad : Good); })
						.Font(Font(11, true))
						.AutoWrapText(true)
					]

					// Actions
					+ SVerticalBox::Slot().AutoHeight()
					[
						SNew(SHorizontalBox)
						+ SHorizontalBox::Slot().AutoWidth().Padding(0, 0, 8, 0)
						[
							SNew(SBox)
							.Visibility(Visible([](const UDCCSessionMenuWidget& W) { return !W.IsClient(); }))
							[
								SNew(SButton)
								.ContentPadding(FMargin(14.0f, 6.0f))
								.ButtonColorAndOpacity(FLinearColor(0.35f, 0.75f, 0.35f))
								.OnClicked_Lambda([Self]()
								{
									if (Self.IsValid()) { Self->StartRun(); }
									return FReply::Handled();
								})
								[
									SNew(STextBlock)
									.Text_Lambda([Self]() { return Self.IsValid() ? Self->GetStartRunLabel() : FText::GetEmpty(); })
									.Font(Font(12, true))
								]
							]
						]
						+ SHorizontalBox::Slot().AutoWidth().Padding(0, 0, 8, 0)
						[
							SNew(SBox)
							.Visibility(Visible([](const UDCCSessionMenuWidget& W) { return W.CanBail(); }))
							[
								MenuButton(NSLOCTEXT("DCC", "PartyBail", "Safe Room"), FOnClicked::CreateLambda([Self]()
								{
									if (Self.IsValid()) { Self->BailToLobby(); }
									return FReply::Handled();
								}))
							]
						]
						+ SHorizontalBox::Slot().AutoWidth().Padding(0, 0, 8, 0)
						[
							SNew(SBox)
							.Visibility(Visible([](const UDCCSessionMenuWidget& W) { return W.IsSolo(); }))
							[
								MenuButton(NSLOCTEXT("DCC", "PartyMainMenu", "Main menu"), FOnClicked::CreateLambda([Self]()
								{
									if (Self.IsValid())
									{
										DCCArcade::Travel(Self->GetWorld(), FName(DCCArcade::FrontDoorId));
									}
									return FReply::Handled();
								}))
							]
						]
						+ SHorizontalBox::Slot().AutoWidth().Padding(0, 0, 8, 0)
						[
							SNew(SBox)
							.Visibility(Visible([](const UDCCSessionMenuWidget& W) { return W.IsClient(); }))
							[
								MenuButton(NSLOCTEXT("DCC", "PartyLeave", "Leave game"), FOnClicked::CreateLambda([Self]()
								{
									if (Self.IsValid()) { Self->Leave(); }
									return FReply::Handled();
								}))
							]
						]
						+ SHorizontalBox::Slot().FillWidth(1.0f)[ SNew(SSpacer) ]
						+ SHorizontalBox::Slot().AutoWidth()
						[
							MenuButton(NSLOCTEXT("DCC", "PartyClose", "Close  (Esc)"), FOnClicked::CreateLambda([Self]()
							{
								if (Self.IsValid() && Self->OnCloseRequested) { Self->OnCloseRequested(); }
								return FReply::Handled();
							}))
						]
					]
				])
			]
		];

	Refresh();
	return Root;
}

FReply UDCCSessionMenuWidget::NativeOnKeyDown(const FGeometry& InGeometry, const FKeyEvent& InKeyEvent)
{
	const FKey Key = InKeyEvent.GetKey();
	if ((Key == EKeys::Escape || Key == EKeys::Tab) && OnCloseRequested)
	{
		OnCloseRequested();
		return FReply::Handled();
	}
	return Super::NativeOnKeyDown(InGeometry, InKeyEvent);
}

void UDCCSessionMenuWidget::ReleaseSlateResources(bool bReleaseChildren)
{
	Super::ReleaseSlateResources(bReleaseChildren);
	JoinBox.Reset();
	InviteList.Reset();
	OldPasswordBox.Reset();
	NewPasswordBox.Reset();
}

void UDCCSessionMenuWidget::RebuildInviteList()
{
	if (!InviteList.IsValid())
	{
		return;
	}

	InviteList->ClearChildren();
	TWeakObjectPtr<UDCCSessionMenuWidget> Self(this);

	if (Addresses.IsEmpty())
	{
		InviteList->AddSlot().AutoHeight()
		[
			SNew(STextBlock)
			.Text(NSLOCTEXT("DCC", "PartyNoAddress", "No network address found. Is this PC online?"))
			.Font(Font(11))
			.ColorAndOpacity(Bad)
		];
		return;
	}

	// What the router said about an internet invite, while hosting.
	if (GetNetModeSafe() == NM_ListenServer && DCCUPnP::GetState() != DCCUPnP::EState::Idle)
	{
		InviteList->AddSlot().AutoHeight().Padding(0, 0, 0, 4)
		[
			SNew(STextBlock)
			.Text(FText::FromString(DCCUPnP::Describe()))
			.Font(Font(10))
			.AutoWrapText(true)
			.ColorAndOpacity(DCCUPnP::GetState() == DCCUPnP::EState::Failed ? Dim : Gold)
		];
	}
	for (const DCCSession::FHostAddress& Address : Addresses)
	{
		InviteList->AddSlot().AutoHeight().Padding(0, 2)
		[
			SNew(SHorizontalBox)
			+ SHorizontalBox::Slot().AutoWidth().VAlign(VAlign_Center)
			[
				SNew(SBox).WidthOverride(110.0f)
				[
					SNew(STextBlock).Text(FText::FromString(Address.Label)).Font(Font(11)).ColorAndOpacity(Dim)
				]
			]
			+ SHorizontalBox::Slot().AutoWidth().VAlign(VAlign_Center)
			[
				SNew(SBox).WidthOverride(170.0f)
				[
					SNew(STextBlock).Text(FText::FromString(Address.InviteCode)).Font(Font(16, true)).ColorAndOpacity(Gold)
				]
			]
			+ SHorizontalBox::Slot().AutoWidth().VAlign(VAlign_Center).Padding(0, 0, 10, 0)
			[
				SNew(SButton)
				.ContentPadding(FMargin(10.0f, 3.0f))
				.OnClicked_Lambda([Self, Address]()
				{
					if (Self.IsValid()) { Self->CopyInvite(Address); }
					return FReply::Handled();
				})
				[
					SNew(STextBlock).Text(NSLOCTEXT("DCC", "PartyCopy", "Copy invite")).Font(Font(11, true))
				]
			]
			+ SHorizontalBox::Slot().AutoWidth().VAlign(VAlign_Center)
			[
				SNew(STextBlock).Text(FText::FromString(Address.Address)).Font(Font(10)).ColorAndOpacity(Dim)
			]
		];
	}
}

FText UDCCSessionMenuWidget::GetStatusText() const
{
	const UWorld* World = GetWorld();
	const AGameStateBase* GameState = World ? World->GetGameState() : nullptr;
	const int32 Count = GameState ? GameState->PlayerArray.Num() : 1;

	switch (GetNetModeSafe())
	{
	case NM_ListenServer:
		return FText::Format(NSLOCTEXT("DCC", "PartyStatusHost",
			"You are hosting. {0} {0}|plural(one=crawler,other=crawlers) here. Send a friend an invite code below."), FText::AsNumber(Count));
	case NM_Client:
		return FText::Format(NSLOCTEXT("DCC", "PartyStatusClient",
			"Connected to the host's game. {0} {0}|plural(one=crawler,other=crawlers) here."), FText::AsNumber(Count));
	default:
		return NSLOCTEXT("DCC", "PartyStatusSolo",
			"Playing solo. Host to bring friends into this game, or join theirs with an invite code.");
	}
}

FText UDCCSessionMenuWidget::GetPartyText() const
{
	const UWorld* World = GetWorld();
	const AGameStateBase* GameState = World ? World->GetGameState() : nullptr;
	if (!GameState)
	{
		return FText::GetEmpty();
	}

	const APlayerController* Local = GetOwningPlayer();
	FString Lines;
	for (const APlayerState* PlayerState : GameState->PlayerArray)
	{
		if (!PlayerState)
		{
			continue;
		}

		FString Crawler = TEXT("arriving...");
		if (const ADCCCharacterBase* Pawn = Cast<ADCCCharacterBase>(PlayerState->GetPawn()))
		{
			if (Pawn->Onboarding && !Pawn->Onboarding->CrawlerId.IsNone())
			{
				Crawler = Pawn->Onboarding->CrawlerId.ToString();
			}
			else
			{
				Crawler = TEXT("crawler");
			}
		}

		const bool bYou = Local && Local->PlayerState == PlayerState;
		Lines += FString::Printf(TEXT("%s  -  %s%s   %d ms\n"), *Crawler, *PlayerState->GetPlayerName(),
			bYou ? TEXT(" (you)") : TEXT(""), FMath::RoundToInt(PlayerState->GetPingInMilliseconds()));
	}
	return FText::FromString(Lines.TrimEnd());
}

FText UDCCSessionMenuWidget::GetStartRunLabel() const
{
	return ADCCTaleFloor::IsRunLive(GetWorld())
		? NSLOCTEXT("DCC", "PartyBringIn", "Bring everyone into the run")
		: IsHost()
			? NSLOCTEXT("DCC", "PartyStartAll", "Start the run (everyone)")
			: NSLOCTEXT("DCC", "PartyStartSolo", "Start a run");
}

void UDCCSessionMenuWidget::ChangePassword()
{
	const FName Crawler = DCCSession::GetPreferredCrawler();
	const FString Old = OldPasswordBox.IsValid() ? OldPasswordBox->GetText().ToString() : FString();
	const FString New = NewPasswordBox.IsValid() ? NewPasswordBox->GetText().ToString() : FString();
	const APlayerController* PC = GetOwningPlayer();
	const ADCCCharacterBase* Me = PC ? Cast<ADCCCharacterBase>(PC->GetPawn()) : nullptr;
	if (New.Len() < 3 || Old.IsEmpty())
	{
		Message = TEXT("Type your current password and a new one (3 characters or more).");
		bMessageIsError = true;
		return;
	}
	if (!Me || !Me->Wallet)
	{
		Message = TEXT("Get out of the car first, then change it.");
		bMessageIsError = true;
		return;
	}
	// Hashes only. The host checks the old one against the save and answers through the wallet.
	Me->Wallet->PasswordMessage.Reset();
	Me->Wallet->Server_ChangePassword(DCCProfile::HashPassword(Crawler.ToString(), Old), DCCProfile::HashPassword(Crawler.ToString(), New));
	OldPasswordBox->SetText(FText::GetEmpty());
	NewPasswordBox->SetText(FText::GetEmpty());
	Message.Reset();
}

FText UDCCSessionMenuWidget::GetPasswordNote() const
{
	const APlayerController* PC = GetOwningPlayer();
	const ADCCCharacterBase* Me = PC ? Cast<ADCCCharacterBase>(PC->GetPawn()) : nullptr;
	if (Me && Me->Wallet && !Me->Wallet->PasswordMessage.IsEmpty())
	{
		return FText::FromString(Me->Wallet->PasswordMessage);
	}
	return NSLOCTEXT("DCC", "PartyPwNote", "Your password keeps friends from playing as you. To be someone else, go to the main menu.");
}

void UDCCSessionMenuWidget::PickCrawler(FName CrawlerId)
{
	// The host (or a solo player) owns the server, so the switch can happen now.
	// A client's choice is sent with its next join; the server decides who it is.
	if (DCCSession::SwitchCrawler(GetOwningPlayer(), CrawlerId))
	{
		Message = FString::Printf(TEXT("You are %s."), *CrawlerId.ToString());
	}
	else
	{
		Message = FString::Printf(TEXT("%s saved. It applies the next time you join."), *CrawlerId.ToString());
	}
	bMessageIsError = false;
}

void UDCCSessionMenuWidget::Host()
{
	FString Error;
	if (DCCSession::StartHosting(GetWorld(), GetOwningPlayer(), Error))
	{
		Refresh();
		Message = TEXT("Hosting. Copy an invite and send it to the party.");
		bMessageIsError = false;
	}
	else
	{
		Message = Error;
		bMessageIsError = true;
	}
}

void UDCCSessionMenuWidget::JoinFromBox()
{
	const FString Typed = JoinBox.IsValid() ? JoinBox->GetText().ToString() : FString();
	if (Typed.TrimStartAndEnd().IsEmpty())
	{
		Message = TEXT("Paste the invite code your host sent you.");
		bMessageIsError = true;
		return;
	}

	Message = FString::Printf(TEXT("Connecting to %s..."), *DCCSession::ResolveJoinTarget(Typed));
	bMessageIsError = false;
	DCCSession::Join(GetOwningPlayer(), Typed);
}

void UDCCSessionMenuWidget::JoinFromClipboard()
{
	FString Target;
	if (DCCSession::JoinFromClipboard(GetOwningPlayer(), Target))
	{
		Message = FString::Printf(TEXT("Connecting to %s..."), *Target);
		bMessageIsError = false;
	}
	else
	{
		Message = TEXT("No invite found. Copy the whole message your host sent (Ctrl+C), then click again.");
		bMessageIsError = true;
	}
}

void UDCCSessionMenuWidget::StartRun()
{
	UWorld* World = GetWorld();
	if (!World || IsClient())
	{
		return;
	}

	ADCCTaleFloor::EnterEveryone(World);
	if (OnCloseRequested)
	{
		OnCloseRequested();
	}
}

void UDCCSessionMenuWidget::Leave()
{
	DCCSession::Leave(GetOwningPlayer());
}

bool UDCCSessionMenuWidget::CanBail() const
{
	return DCCArcade::CanLeaveForLobby(GetWorld());
}

void UDCCSessionMenuWidget::BailToLobby()
{
	const UWorld* World = GetWorld();
	const double Now = World ? World->GetRealTimeSeconds() : 0.0;
	if (Now > BailArmedUntil)
	{
		BailArmedUntil = Now + 6.0;
		Message = TEXT("Click Safe Room again. Everyone goes back.");
		bMessageIsError = false;
		return;
	}
	BailArmedUntil = 0.0;
	if (APawn* Pawn = GetOwningPlayerPawn())
	{
		if (ADCCCharacterBase* Crawler = Cast<ADCCCharacterBase>(Pawn))
		{
			Crawler->Server_BailToLobby();
			Message = TEXT("Heading back to the Safe Room.");
			bMessageIsError = false;
		}
	}
}

void UDCCSessionMenuWidget::CopyInvite(const DCCSession::FHostAddress& Address)
{
	FPlatformApplicationMisc::ClipboardCopy(*DCCSession::InviteMessage(Address.InviteCode));
	Message = FString::Printf(TEXT("Copied the %s invite (%s). Paste it into your group chat."), *Address.Label, *Address.InviteCode);
	bMessageIsError = false;
}

void UDCCSessionMenuWidget::NativeTick(const FGeometry& MyGeometry, float InDeltaTime)
{
	Super::NativeTick(MyGeometry, InDeltaTime);
	// The router answers on a background thread; redraw the invites when it does.
	if (DCCUPnP::GetVersion() != SeenUPnPVersion)
	{
		Refresh();
	}
}
