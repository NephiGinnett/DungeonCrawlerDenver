#include "UI/DCCEventPanelWidget.h"

#include "Brushes/SlateImageBrush.h"
#include "Dom/JsonObject.h"
#include "Framework/Application/SlateApplication.h"
#include "GameFramework/PlayerController.h"
#include "GameFramework/PlayerState.h"
#include "HAL/PlatformProcess.h"
#include "Misc/ConfigCacheIni.h"
#include "Misc/FileHelper.h"
#include "Misc/Paths.h"
#include "Rendering/DrawElements.h"
#include "Serialization/JsonReader.h"
#include "Serialization/JsonSerializer.h"
#include "Styling/CoreStyle.h"
#include "UI/DCCFxTextures.h"
#include "UI/DCCLook.h"
#include "UI/DCCMenuStyle.h"
#include "Widgets/Images/SImage.h"
#include "Widgets/Input/SButton.h"
#include "Widgets/Layout/SBorder.h"
#include "Widgets/Layout/SBox.h"
#include "Widgets/Layout/SSpacer.h"
#include "Widgets/SBoxPanel.h"
#include "Widgets/SLeafWidget.h"
#include "Widgets/SOverlay.h"
#include "Widgets/Text/STextBlock.h"

namespace
{
	// ---- the palette of the mock-ups --------------------------------------------------------------------------------
	const FLinearColor Deep(0.008f, 0.004f, 0.03f, 0.97f);
	const FLinearColor Deep2(0.018f, 0.008f, 0.06f, 1.0f);
	const FLinearColor Plum(0.06f, 0.02f, 0.16f, 1.0f);
	const FLinearColor Lilac(0.8f, 0.68f, 1.0f, 1.0f);
	const FLinearColor Gold(1.0f, 0.8f, 0.4f, 1.0f);
	const FLinearColor GoldDim(0.62f, 0.46f, 0.22f, 1.0f);
	const FLinearColor Pink(1.0f, 0.36f, 0.76f, 1.0f);
	const FLinearColor Violet(0.62f, 0.35f, 1.0f, 1.0f);
	const FLinearColor Sky(0.35f, 0.75f, 1.0f, 1.0f);

	FSlateFontInfo Display(float Size, int32 Spacing = 60) { return DCCLook::FromFile(TEXT("BebasNeue-Regular.ttf"), Size, Spacing); }
	FSlateFontInfo Text(float Size) { return DCCLook::FromFile(TEXT("BarlowCondensed-SemiBold.ttf"), Size); }

	const FSlateBrush* TexBrush(UTexture2D* Texture)
	{
		static TMap<UTexture2D*, TSharedPtr<FSlateImageBrush>> Cache;
		if (!Texture)
		{
			return FCoreStyle::Get().GetBrush("WhiteBrush");
		}
		if (TSharedPtr<FSlateImageBrush>* Found = Cache.Find(Texture))
		{
			return Found->Get();
		}
		return Cache.Add(Texture, MakeShared<FSlateImageBrush>(Texture, FVector2D(64.0f, 64.0f))).Get();
	}

	TSharedRef<SWidget> Label(const FString& S, const FSlateFontInfo& Font, const FLinearColor& Colour)
	{
		return SNew(STextBlock).Text(FText::FromString(S)).Font(Font).ColorAndOpacity(FSlateColor(Colour))
			.ShadowColorAndOpacity(FLinearColor(0.0f, 0.0f, 0.0f, 0.6f)).ShadowOffset(FVector2D(1.0f, 2.0f));
	}

	TSharedRef<SWidget> Gem(float Size, const FLinearColor& Colour)
	{
		return SNew(SBox).WidthOverride(Size).HeightOverride(Size)[ SNew(SImage).Image(TexBrush(DCCFx::Diamond())).ColorAndOpacity(Colour) ];
	}

	/** Twinkling stars and drifting motes, painted every frame (the mock-ups' starfield). */
	class SDCCStarfield : public SLeafWidget
	{
	public:
		SLATE_BEGIN_ARGS(SDCCStarfield) : _Count(70), _Tint(FLinearColor::White) {}
			SLATE_ARGUMENT(int32, Count)
			SLATE_ARGUMENT(FLinearColor, Tint)
		SLATE_END_ARGS()

		void Construct(const FArguments& Args)
		{
			Count = Args._Count;
			Tint = Args._Tint;
			SetVisibility(EVisibility::HitTestInvisible);
		}

		virtual FVector2D ComputeDesiredSize(float) const override { return FVector2D(8.0f, 8.0f); }

		virtual int32 OnPaint(const FPaintArgs&, const FGeometry& Geo, const FSlateRect&, FSlateWindowElementList& Out, int32 Layer,
			const FWidgetStyle&, bool) const override
		{
			const FVector2f Size = FVector2f(Geo.GetLocalSize());
			const float T = float(FSlateApplication::Get().GetCurrentTime());
			const FSlateBrush* Star = TexBrush(DCCFx::Sparkle());
			const FSlateBrush* Mote = TexBrush(DCCFx::Glow());
			auto Hash = [](float N) { return FMath::Frac(FMath::Sin(N * 12.9898f) * 43758.5453f); };
			for (int32 i = 0; i < Count; ++i)
			{
				const float X = Hash(i + 0.1f), Y0 = Hash(i + 7.3f), Big = Hash(i + 3.7f);
				const float Y = FMath::Frac(Y0 - T * (0.004f + 0.01f * Big)); // a slow upward drift
				const float Twinkle = 0.35f + 0.65f * FMath::Max(0.0f, FMath::Sin(T * (1.2f + 2.5f * Hash(i + 9.1f)) + i));
				const bool bStar = Big > 0.82f;
				const float S = bStar ? 10.0f + 16.0f * Big * Twinkle : 3.0f + 5.0f * Big;
				FLinearColor C = Tint * (bStar ? Twinkle : 0.55f * Twinkle);
				C.A = (bStar ? 0.95f : 0.6f) * Twinkle;
				Out.PushClip(FSlateClippingZone(Geo));
				FSlateDrawElement::MakeBox(Out, Layer, Geo.ToPaintGeometry(FVector2f(S, S), FSlateLayoutTransform(FVector2f(X * Size.X - S * 0.5f, Y * Size.Y - S * 0.5f))),
					bStar ? Star : Mote, ESlateDrawEffect::None, C);
				Out.PopClip();
			}
			return Layer + 1;
		}

	private:
		int32 Count = 70;
		FLinearColor Tint = FLinearColor::White;
	};

	/** A gold-trimmed frame: an outer gold line, the fill, an inner fine gold line. */
	TSharedRef<SWidget> Ornate(const TSharedRef<SWidget>& Content, float Radius, const FLinearColor& Fill, float Inset = 6.0f)
	{
		return SNew(SBorder).BorderImage(DCCLook::Framed(Radius, Gold, 2.0f)).BorderBackgroundColor(Fill).Padding(Inset)
			[
				SNew(SBorder).BorderImage(DCCLook::Framed(FMath::Max(0.0f, Radius - Inset), GoldDim, 1.0f)).BorderBackgroundColor(FLinearColor::Transparent).Padding(0.0f)
				[ Content ]
			];
	}

	/** The four corner gems on an ornate frame. */
	void AddCorners(SOverlay& Overlay, float Size)
	{
		const EHorizontalAlignment H[] = { HAlign_Left, HAlign_Right, HAlign_Left, HAlign_Right };
		const EVerticalAlignment V[] = { VAlign_Top, VAlign_Top, VAlign_Bottom, VAlign_Bottom };
		for (int32 i = 0; i < 4; ++i)
		{
			Overlay.AddSlot().HAlign(H[i]).VAlign(V[i])[ Gem(Size, Gold) ];
		}
	}

	struct FMilestone
	{
		int32 Lines;
		const TCHAR* Prize;
		const TCHAR* Extra;
		FLinearColor Colour;
	};

	const FMilestone Milestones[] = {
		{ 1, TEXT("BRONZE BOX"), TEXT(""), FLinearColor(0.95f, 0.55f, 0.28f) },
		{ 3, TEXT("SILVER BOX"), TEXT("+50 tickets"), FLinearColor(0.82f, 0.88f, 1.0f) },
		{ 6, TEXT("GOLD BOX"), TEXT("+ the OPEN MIC title"), FLinearColor(1.0f, 0.8f, 0.3f) },
	};
	constexpr int32 MaxLines = 6;

	const TCHAR* Months[] = { TEXT("JAN"), TEXT("FEB"), TEXT("MAR"), TEXT("APR"), TEXT("MAY"), TEXT("JUN"), TEXT("JUL"), TEXT("AUG"),
		TEXT("SEP"), TEXT("OCT"), TEXT("NOV"), TEXT("DEC") };
}

int32 UDCCEventPanelWidget::PreviewLines = -1;

void UDCCEventPanelWidget::NativeConstruct()
{
	Super::NativeConstruct();
	SetIsFocusable(true);
	Opened = 0.0f;
}

FReply UDCCEventPanelWidget::NativeOnKeyDown(const FGeometry& InGeometry, const FKeyEvent& InKeyEvent)
{
	if ((InKeyEvent.GetKey() == EKeys::Escape || InKeyEvent.GetKey() == EKeys::Tab) && OnCloseRequested)
	{
		OnCloseRequested();
		return FReply::Handled();
	}
	return Super::NativeOnKeyDown(InGeometry, InKeyEvent);
}

void UDCCEventPanelWidget::NativeTick(const FGeometry& MyGeometry, float InDeltaTime)
{
	Super::NativeTick(MyGeometry, InDeltaTime);
	Opened = FMath::Min(1.0f, Opened + InDeltaTime / 0.35f);
	const float E = 1.0f - FMath::Pow(1.0f - Opened, 3.0f);
	SetRenderOpacity(E);
	SetRenderScale(FVector2D(0.92f + 0.08f * E));
}

TSharedRef<SWidget> UDCCEventPanelWidget::RebuildWidget()
{
	// ---- this week's count ----
	const FDateTime Now = FDateTime::UtcNow();
	const FDateTime WeekStart = FDateTime(Now.GetYear(), Now.GetMonth(), Now.GetDay()) - FTimespan::FromDays((int32(Now.GetDayOfWeek()) + 7) % 7);
	const FDateTime WeekEnd = WeekStart + FTimespan::FromDays(7);
	Monday = WeekStart.ToString(TEXT("%Y-%m-%d"));
	Lines = 0;
	FString Me;
	if (APlayerController* PC = GetOwningPlayer())
	{
		Me = PC->PlayerState ? PC->PlayerState->GetPlayerName() : FString();
	}
	{
		FString Raw;
		TSharedPtr<FJsonObject> Root;
		const FString Path = FPaths::ProjectContentDir() / TEXT("DCC/VoiceCredits/credits.json");
		if (FFileHelper::LoadFileToString(Raw, *Path) && FJsonSerializer::Deserialize(TJsonReaderFactory<>::Create(Raw), Root) && Root.IsValid())
		{
			const TSharedPtr<FJsonObject>* Week = nullptr;
			const TSharedPtr<FJsonObject>* Counts = nullptr;
			if (Root->TryGetObjectField(TEXT("week"), Week) && (*Week)->GetStringField(TEXT("monday")) == Monday
				&& (*Week)->TryGetObjectField(TEXT("counts"), Counts))
			{
				(*Counts)->TryGetNumberField(Me.ToLower(), Lines);
			}
		}
	}
	if (PreviewLines >= 0)
	{
		Lines = PreviewLines; // DCC.EventPanel's preview
	}
	const FTimespan Left = WeekEnd - Now;
	const FString Ends = FString::Printf(TEXT("WEEK OF %s   ENDS IN %dD %dH"), *FString::Printf(TEXT("%s %d"), Months[WeekStart.GetMonth() - 1], WeekStart.GetDay()), Left.GetDays(), Left.GetHours());
	FString Url;
	if (GConfig)
	{
		GConfig->GetString(TEXT("DCCVoiceDrop"), TEXT("Url"), Url, GGameIni);
	}

	TWeakObjectPtr<UDCCEventPanelWidget> Self(this);
	const float BarW = 860.0f;
	auto Fill = [Self]()
	{
		if (!Self.IsValid())
		{
			return 0.0f;
		}
		const float E = FMath::Clamp((Self->Opened - 0.3f) / 0.7f, 0.0f, 1.0f);
		return FMath::Clamp(float(Self->Lines) / MaxLines, 0.0f, 1.0f) * (1.0f - FMath::Pow(1.0f - E, 2.0f));
	};
	auto At = [BarW](int32 L) { return BarW * (0.06f + 0.88f * float(L) / MaxLines); };

	// ---- the plaque ----
	TSharedRef<SWidget> Plaque = SNew(SVerticalBox)
		+ SVerticalBox::Slot().AutoHeight().HAlign(HAlign_Center).Padding(0, 0, 0, 4)
		[
			SNew(SHorizontalBox)
			+ SHorizontalBox::Slot().AutoWidth().VAlign(VAlign_Center)[ Gem(12.0f, Gold) ]
			+ SHorizontalBox::Slot().AutoWidth().VAlign(VAlign_Center).Padding(8, 0)[ Label(TEXT("SPECIAL EVENT"), Display(14, 300), Lilac) ]
			+ SHorizontalBox::Slot().AutoWidth().VAlign(VAlign_Center)[ Gem(12.0f, Gold) ]
		]
		+ SVerticalBox::Slot().AutoHeight().HAlign(HAlign_Center)
		[
			SNew(SHorizontalBox)
			+ SHorizontalBox::Slot().AutoWidth().VAlign(VAlign_Center).Padding(0, 0, -8, 0)[ Gem(30.0f, Gold) ]
			+ SHorizontalBox::Slot().AutoWidth()
			[
				SNew(SBox).MinDesiredWidth(520.0f)
				[
					Ornate(SNew(SBox).HAlign(HAlign_Center).Padding(FMargin(40.0f, 4.0f))[ Label(TEXT("OPEN MIC WEEK"), Display(42, 120), Gold) ], 4.0f, Plum, 5.0f)
				]
			]
			+ SHorizontalBox::Slot().AutoWidth().VAlign(VAlign_Center).Padding(-8, 0, 0, 0)[ Gem(30.0f, Gold) ]
		]
		+ SVerticalBox::Slot().AutoHeight().HAlign(HAlign_Center).Padding(0, 6, 0, 0)[ Label(Ends, Text(15), Lilac) ];

	// ---- Hiss's pitch ----
	TSharedRef<SWidget> Pitch = SNew(SVerticalBox)
		+ SVerticalBox::Slot().AutoHeight()[ Label(TEXT("HOSTED BY"), Display(16, 300), Pink) ]
		+ SVerticalBox::Slot().AutoHeight().Padding(0, -4, 0, 8)[ Label(TEXT("HISS CRANSON"), Display(54, 80), Gold) ]
		+ SVerticalBox::Slot().AutoHeight().Padding(0, 0, 30, 10)
		[
			SNew(STextBlock).AutoWrapText(true).Font(Text(18)).ColorAndOpacity(FSlateColor(FLinearColor(0.95f, 0.92f, 1.0f)))
			.Text(FText::FromString(TEXT("\"Record a line for my show. Every take we put on air this week moves your bar. "
				"And the voice I choose for myself walks away with my microphone. Professionally speaking, I am thrilled.\"")))
		]
		+ SVerticalBox::Slot().AutoHeight().Padding(0, 0, 30, 0)
		[
			SNew(STextBlock).AutoWrapText(true).Font(Text(15)).ColorAndOpacity(FSlateColor(Lilac))
			.Text(FText::FromString(TEXT("Lines are recorded on Voice Drop and paid out the next time you play.")))
		]
		+ SVerticalBox::Slot().AutoHeight().Padding(0, 18, 0, 0)
		[
			SNew(SHorizontalBox)
			+ SHorizontalBox::Slot().AutoWidth().VAlign(VAlign_Center)[ Label(FString::Printf(TEXT("%d"), Lines), Display(46, 0), Gold) ]
			+ SHorizontalBox::Slot().AutoWidth().VAlign(VAlign_Bottom).Padding(10, 0, 0, 12)
			[ Label(FString::Printf(TEXT("LINE%s ON AIR THIS WEEK"), Lines == 1 ? TEXT("") : TEXT("S")), Display(20, 160), Lilac) ]
		];

	// ---- the featured prize: framed like an SSR card ----
	const FSlateBrush* Mic = DCCMenu::IconBrush(TEXT("HotMic"));
	TSharedRef<SOverlay> Art = SNew(SOverlay)
		+ SOverlay::Slot()[ SNew(SBorder).BorderImage(DCCLook::RoundedWhite(6.0f)).BorderBackgroundColor(FLinearColor(0.03f, 0.008f, 0.09f, 1.0f)) ]
		+ SOverlay::Slot()[ SNew(SDCCStarfield).Count(55).Tint(FLinearColor(0.85f, 0.7f, 1.0f)) ]
		+ SOverlay::Slot().HAlign(HAlign_Center).VAlign(VAlign_Center)
		[
			SNew(SBox).WidthOverride(300.0f).HeightOverride(300.0f)
			[ SNew(SImage).Image(TexBrush(DCCFx::Glow())).ColorAndOpacity_Lambda([]() { const float T = float(FSlateApplication::Get().GetCurrentTime()); return FSlateColor(FLinearColor(0.55f, 0.25f, 1.0f, 0.75f + 0.25f * FMath::Sin(T * 2.0f))); }) ]
		]
		+ SOverlay::Slot().HAlign(HAlign_Center).VAlign(VAlign_Center)
		[
			SNew(SBox).WidthOverride(340.0f).HeightOverride(340.0f)
			[
				SNew(SImage).Image(TexBrush(DCCFx::Rays())).ColorAndOpacity(FLinearColor(0.7f, 0.5f, 1.0f, 0.35f))
				.RenderTransformPivot(FVector2D(0.5f, 0.5f))
				.RenderTransform_Lambda([]() { return FSlateRenderTransform(FQuat2D(FMath::DegreesToRadians(float(FSlateApplication::Get().GetCurrentTime()) * 9.0f))); })
			]
		]
		+ SOverlay::Slot().HAlign(HAlign_Center).VAlign(VAlign_Center)
		[
			SNew(SBox).WidthOverride(190.0f).HeightOverride(190.0f)
			[
				Mic ? StaticCastSharedRef<SWidget>(SNew(SImage).Image(Mic).RenderTransformPivot(FVector2D(0.5f, 0.5f))
					.RenderTransform_Lambda([]() { const float T = float(FSlateApplication::Get().GetCurrentTime()); return FSlateRenderTransform(FTransform2D(1.0f, FVector2D(0.0f, 6.0f * FMath::Sin(T * 1.6f)))); }))
				: StaticCastSharedRef<SWidget>(Gem(120.0f, Lilac))
			]
		];
	TSharedRef<SOverlay> Card = SNew(SOverlay)
		+ SOverlay::Slot()
		[
			Ornate(
				SNew(SVerticalBox)
				+ SVerticalBox::Slot().AutoHeight().HAlign(HAlign_Center).Padding(0, 10, 0, 8)
				[
					Ornate(SNew(SBox).Padding(FMargin(22.0f, 0.0f))[ Label(TEXT("EPIC"), Display(26, 200), Gold) ], 3.0f, Plum, 3.0f)
				]
				+ SVerticalBox::Slot().FillHeight(1.0f).Padding(12, 0)[ Ornate(Art, 6.0f, Deep2, 3.0f) ]
				+ SVerticalBox::Slot().AutoHeight().HAlign(HAlign_Center).Padding(0, 10, 0, 0)
				[
					SNew(SHorizontalBox)
					+ SHorizontalBox::Slot().AutoWidth().VAlign(VAlign_Center)[ Gem(14.0f, Gold) ]
					+ SHorizontalBox::Slot().AutoWidth().Padding(10, 0)[ Label(TEXT("THE HOT MIC"), Display(40, 100), Gold) ]
					+ SHorizontalBox::Slot().AutoWidth().VAlign(VAlign_Center)[ Gem(14.0f, Gold) ]
				]
				+ SVerticalBox::Slot().AutoHeight().HAlign(HAlign_Center).Padding(0, 0, 0, 12)
				[ Label(TEXT("FOR HISS CRANSON'S CHOSEN VOICE"), Display(15, 160), Lilac) ],
				10.0f, Deep2, 6.0f)
		];
	AddCorners(*Card, 22.0f);

	// ---- the progress bar ----
	TSharedRef<SHorizontalBox> Gradient = SNew(SHorizontalBox);
	for (int32 i = 0; i < 48; ++i)
	{
		const float U = i / 47.0f;
		const FLinearColor C = U < 0.5f ? FMath::Lerp(Pink, Violet, U * 2.0f) : FMath::Lerp(Violet, Sky, (U - 0.5f) * 2.0f);
		Gradient->AddSlot().FillWidth(1.0f)[ SNew(SImage).Image(FCoreStyle::Get().GetBrush("WhiteBrush")).ColorAndOpacity(C) ];
	}
	TSharedRef<SOverlay> Track = SNew(SOverlay)
		// The groove.
		+ SOverlay::Slot().VAlign(VAlign_Center)
		[
			SNew(SBox).HeightOverride(22.0f)
			[ SNew(SBorder).BorderImage(DCCLook::Framed(11.0f, GoldDim, 2.0f)).BorderBackgroundColor(FLinearColor(0.03f, 0.01f, 0.07f, 1.0f)) ]
		]
		// The fill, a pink to violet to sky gradient revealed from the left.
		+ SOverlay::Slot().VAlign(VAlign_Center).HAlign(HAlign_Left).Padding(4, 0)
		[
			SNew(SBox).HeightOverride(14.0f).WidthOverride_Lambda([Fill, BarW]() { return FOptionalSize(FMath::Max(0.0f, (BarW - 8.0f) * Fill())); })
			.Clipping(EWidgetClipping::ClipToBounds)
			[ SNew(SBox).WidthOverride(BarW - 8.0f)[ Gradient ] ]
		];
	// The milestone nodes on the bar, and their medallions above and labels below.
	TSharedRef<SOverlay> Prizes = SNew(SOverlay);
	TSharedRef<SOverlay> Captions = SNew(SOverlay);
	for (const FMilestone& M : Milestones)
	{
		const bool bReached = Lines >= M.Lines;
		const float X = At(M.Lines);
		const FLinearColor Edge = bReached ? Gold : GoldDim;
		Track->AddSlot().HAlign(HAlign_Left).VAlign(VAlign_Center).Padding(X - 20.0f, 0, 0, 0)
		[
			SNew(SBox).WidthOverride(40.0f).HeightOverride(40.0f)
			[
				SNew(SOverlay)
				+ SOverlay::Slot()[ SNew(SBorder).BorderImage(DCCLook::Framed(20.0f, Edge, 2.5f)).BorderBackgroundColor(bReached ? Plum : Deep2) ]
				+ SOverlay::Slot().HAlign(HAlign_Center).VAlign(VAlign_Center)[ Gem(18.0f, bReached ? M.Colour : M.Colour * 0.45f) ]
			]
		];
		// A medallion: a ring, the box's gem glowing in its colour, and a star when you have it.
		TSharedRef<SOverlay> Medal = SNew(SOverlay)
			+ SOverlay::Slot().HAlign(HAlign_Center).VAlign(VAlign_Center)
			[
				SNew(SBox).WidthOverride(150.0f).HeightOverride(150.0f)
				[ SNew(SImage).Image(TexBrush(DCCFx::Glow())).ColorAndOpacity(M.Colour * (bReached ? 0.9f : 0.35f)) ]
			]
			+ SOverlay::Slot().HAlign(HAlign_Center).VAlign(VAlign_Center)
			[
				SNew(SBox).WidthOverride(82.0f).HeightOverride(82.0f)
				[
					SNew(SOverlay)
					+ SOverlay::Slot()[ SNew(SBorder).BorderImage(DCCLook::Framed(41.0f, Edge, 3.0f)).BorderBackgroundColor(Deep2) ]
					+ SOverlay::Slot().HAlign(HAlign_Center).VAlign(VAlign_Center)[ Gem(50.0f, bReached ? M.Colour : M.Colour * 0.5f) ]
					+ SOverlay::Slot().HAlign(HAlign_Center).VAlign(VAlign_Center)[ Gem(22.0f, FLinearColor(1.0f, 1.0f, 1.0f, bReached ? 0.8f : 0.2f)) ]
				]
			];
		if (bReached)
		{
			Medal->AddSlot().HAlign(HAlign_Right).VAlign(VAlign_Top).Padding(0, 14, 18, 0)
			[
				Ornate(SNew(SBox).Padding(FMargin(6.0f, 0.0f))[ Label(TEXT("GOT IT"), Display(13, 100), Gold) ], 3.0f, Pink * 0.6f, 2.0f)
			];
		}
		Prizes->AddSlot().HAlign(HAlign_Left).VAlign(VAlign_Bottom).Padding(X - 75.0f, 0, 0, 0)[ SNew(SBox).WidthOverride(150.0f).HeightOverride(112.0f)[ Medal ] ];
		Captions->AddSlot().HAlign(HAlign_Left).VAlign(VAlign_Top).Padding(X - 110.0f, 0, 0, 0)
		[
			SNew(SBox).WidthOverride(220.0f)
			[
				SNew(SVerticalBox)
				+ SVerticalBox::Slot().AutoHeight().HAlign(HAlign_Center)
				[ Label(FString::Printf(TEXT("%d LINE%s"), M.Lines, M.Lines == 1 ? TEXT("") : TEXT("S")), Display(30, 60), bReached ? Gold : Lilac) ]
				+ SVerticalBox::Slot().AutoHeight().HAlign(HAlign_Center)[ Label(M.Prize, Display(18, 140), M.Colour) ]
				+ SVerticalBox::Slot().AutoHeight().HAlign(HAlign_Center)[ Label(M.Extra, Text(15), Lilac) ]
			]
		];
	}

	// ---- the buttons ----
	TSharedRef<SHorizontalBox> Buttons = SNew(SHorizontalBox);
	if (!Url.IsEmpty())
	{
		Buttons->AddSlot().AutoWidth().Padding(0, 0, 12, 0)
		[
			SNew(SButton).ButtonStyle(DCCMenu::ButtonStyle(Pink, true)).ContentPadding(FMargin(26.0f, 6.0f))
			.OnClicked_Lambda([Url]() { FPlatformProcess::LaunchURL(*Url, nullptr, nullptr); return FReply::Handled(); })
			[ Label(TEXT("RECORD A LINE"), Display(24, 140), FLinearColor(0.1f, 0.02f, 0.1f)) ]
		];
	}
	Buttons->AddSlot().AutoWidth()
	[
		SNew(SButton).ButtonStyle(DCCMenu::ButtonStyle(Gold, false)).ContentPadding(FMargin(26.0f, 6.0f))
		.OnClicked_Lambda([Self]() { if (Self.IsValid() && Self->OnCloseRequested) { Self->OnCloseRequested(); } return FReply::Handled(); })
		[ Label(TEXT("CLOSE"), Display(24, 140), Gold) ]
	];

	TSharedRef<SWidget> Progress = SNew(SVerticalBox)
		+ SVerticalBox::Slot().AutoHeight().Padding(0, 0, 0, 2)
		[
			SNew(SHorizontalBox)
			+ SHorizontalBox::Slot().AutoWidth().VAlign(VAlign_Center)
			[
			// The pink ribbon tag.
			SNew(SBorder).BorderImage(DCCLook::RoundedWhite(3.0f)).BorderBackgroundColor(FLinearColor(0.85f, 0.2f, 0.6f, 1.0f)).Padding(FMargin(12.0f, 2.0f, 18.0f, 2.0f))
			[
				SNew(SHorizontalBox)
				+ SHorizontalBox::Slot().AutoWidth().VAlign(VAlign_Center).Padding(0, 0, 8, 0)
				[ SNew(SBox).WidthOverride(18.0f).HeightOverride(18.0f)[ SNew(SImage).Image(TexBrush(DCCFx::Sparkle())) ] ]
				+ SHorizontalBox::Slot().AutoWidth()[ Label(TEXT("EVENT PROGRESS"), Display(22, 160), FLinearColor::White) ]
			]
			]
			+ SHorizontalBox::Slot().FillWidth(1.0f)[ SNew(SSpacer) ]
			+ SHorizontalBox::Slot().AutoWidth().VAlign(VAlign_Center)[ Buttons ]
		]
		+ SVerticalBox::Slot().AutoHeight().HAlign(HAlign_Center)[ SNew(SBox).WidthOverride(BarW).HeightOverride(112.0f)[ Prizes ] ]
		+ SVerticalBox::Slot().AutoHeight().HAlign(HAlign_Center)[ SNew(SBox).WidthOverride(BarW).HeightOverride(44.0f)[ Track ] ]
		+ SVerticalBox::Slot().AutoHeight().HAlign(HAlign_Center).Padding(0, 6, 0, 0)[ SNew(SBox).WidthOverride(BarW).HeightOverride(100.0f)[ Captions ] ];

	// ---- the window ----
	TSharedRef<SOverlay> Window = SNew(SOverlay)
		+ SOverlay::Slot()[ SNew(SBorder).BorderImage(DCCLook::Framed(14.0f, Gold, 2.5f)).BorderBackgroundColor(Deep) ]
		+ SOverlay::Slot().Padding(3.0f)[ SNew(SDCCStarfield).Count(110).Tint(FLinearColor(0.75f, 0.6f, 1.0f)) ]
		+ SOverlay::Slot().Padding(9.0f)[ SNew(SBorder).BorderImage(DCCLook::Framed(9.0f, GoldDim, 1.0f)).BorderBackgroundColor(FLinearColor::Transparent) ]
		+ SOverlay::Slot().Padding(FMargin(44.0f, 26.0f))
		[
			SNew(SVerticalBox)
			+ SVerticalBox::Slot().AutoHeight().Padding(0, 0, 0, 10)[ Plaque ]
			+ SVerticalBox::Slot().AutoHeight()
			[
				SNew(SHorizontalBox)
				+ SHorizontalBox::Slot().FillWidth(1.0f).VAlign(VAlign_Center)[ Pitch ]
				+ SHorizontalBox::Slot().AutoWidth()[ SNew(SBox).WidthOverride(320.0f).HeightOverride(370.0f)[ Card ] ]
			]
			+ SVerticalBox::Slot().AutoHeight().Padding(0, 10, 0, 0)[ Progress ]
		];
	AddCorners(*Window, 30.0f);
	Window->AddSlot().HAlign(HAlign_Center).VAlign(VAlign_Top).Padding(0, -14, 0, 0)[ Gem(34.0f, Gold) ];
	Window->AddSlot().HAlign(HAlign_Center).VAlign(VAlign_Bottom).Padding(0, 0, 0, -14)[ Gem(28.0f, Gold) ];

	return SNew(SOverlay)
		+ SOverlay::Slot()[ SNew(SImage).Image(FCoreStyle::Get().GetBrush("WhiteBrush")).ColorAndOpacity(FLinearColor(0.0f, 0.0f, 0.02f, 0.72f)) ]
		+ SOverlay::Slot().HAlign(HAlign_Center).VAlign(VAlign_Center)
		[
			SNew(SBox).WidthOverride(1180.0f)[ Window ]
		];
}
