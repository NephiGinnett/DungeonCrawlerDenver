#include "UI/DCCMenuStyle.h"

#include "Brushes/SlateColorBrush.h"
#include "Brushes/SlateImageBrush.h"
#include "Brushes/SlateRoundedBoxBrush.h"
#include "Engine/Texture2D.h"
#include "Styling/CoreStyle.h"
#include "Styling/SlateTypes.h"
#include "Widgets/Images/SImage.h"
#include "Widgets/Input/SButton.h"
#include "Widgets/Layout/SBorder.h"
#include "Widgets/Layout/SBox.h"
#include "Widgets/SBoxPanel.h"
#include "Widgets/SOverlay.h"
#include "Widgets/Text/STextBlock.h"

namespace DCCMenu
{
	const FLinearColor Glass(0.018f, 0.016f, 0.04f, 0.95f);
	const FLinearColor Well(0.0f, 0.0f, 0.0f, 0.32f);
	const FLinearColor Card(0.06f, 0.055f, 0.11f, 0.92f);
	const FLinearColor Cyan(0.32f, 0.86f, 1.0f);
	const FLinearColor Pink(1.0f, 0.38f, 0.82f);
	const FLinearColor Gold(1.0f, 0.8f, 0.4f);
	const FLinearColor Mint(0.42f, 1.0f, 0.62f);
	const FLinearColor Red(1.0f, 0.38f, 0.38f);
	const FLinearColor Ink(0.92f, 0.92f, 0.98f);
	const FLinearColor Dim(0.58f, 0.6f, 0.72f);
	const FLinearColor Faint(1.0f, 1.0f, 1.0f, 0.08f);

	FLinearColor WithAlpha(const FLinearColor& C, float A)
	{
		return FLinearColor(C.R, C.G, C.B, A);
	}

	FSlateFontInfo Font(int32 Size, const char* Weight, int32 Spacing)
	{
		FSlateFontInfo Info = FCoreStyle::GetDefaultFontStyle(Weight, Size);
		Info.LetterSpacing = Spacing;
		return Info;
	}

	FSlateFontInfo Caps(int32 Size, int32 Spacing)
	{
		return Font(Size, "Bold", Spacing);
	}

	namespace
	{
		uint32 Key(const FLinearColor& A, const FLinearColor& B, float X, float Y, uint32 Salt = 0)
		{
			uint32 H = HashCombine(GetTypeHash(A), GetTypeHash(B));
			H = HashCombine(H, GetTypeHash(X));
			H = HashCombine(H, GetTypeHash(Y));
			return HashCombine(H, Salt);
		}
	}

	const FSlateBrush* Rounded(const FLinearColor& Fill, float Radius, const FLinearColor& Outline, float OutlineWidth)
	{
		static TMap<uint32, TUniquePtr<FSlateRoundedBoxBrush>> Cache;
		const uint32 K = Key(Fill, Outline, Radius, OutlineWidth);
		if (TUniquePtr<FSlateRoundedBoxBrush>* Found = Cache.Find(K))
		{
			return Found->Get();
		}
		return Cache.Add(K, MakeUnique<FSlateRoundedBoxBrush>(Fill, Radius, Outline, OutlineWidth)).Get();
	}

	const FSlateBrush* Solid()
	{
		return FCoreStyle::Get().GetBrush("WhiteBrush");
	}

	const FButtonStyle* ButtonStyle(const FLinearColor& Accent, bool bPrimary)
	{
		static TMap<uint32, TUniquePtr<FButtonStyle>> Cache;
		const uint32 K = Key(Accent, FLinearColor::Black, bPrimary ? 1.0f : 0.0f, 0.0f, 7);
		if (TUniquePtr<FButtonStyle>* Found = Cache.Find(K))
		{
			return Found->Get();
		}
		FButtonStyle Style;
		const float R = 5.0f;
		if (bPrimary)
		{
			Style.SetNormal(*Rounded(WithAlpha(Accent, 0.85f), R, WithAlpha(Accent, 1.0f), 1.0f));
			Style.SetHovered(*Rounded(WithAlpha(FMath::Lerp(Accent, FLinearColor::White, 0.25f), 1.0f), R, FLinearColor::White, 1.0f));
			Style.SetPressed(*Rounded(WithAlpha(Accent * 0.75f, 1.0f), R, WithAlpha(Accent, 1.0f), 1.0f));
		}
		else
		{
			Style.SetNormal(*Rounded(WithAlpha(Accent, 0.07f), R, WithAlpha(Accent, 0.5f), 1.0f));
			Style.SetHovered(*Rounded(WithAlpha(Accent, 0.22f), R, WithAlpha(Accent, 1.0f), 1.0f));
			Style.SetPressed(*Rounded(WithAlpha(Accent, 0.35f), R, WithAlpha(Accent, 1.0f), 1.0f));
		}
		Style.SetDisabled(*Rounded(FLinearColor(1.0f, 1.0f, 1.0f, 0.03f), R, FLinearColor(1.0f, 1.0f, 1.0f, 0.1f), 1.0f));
		Style.SetNormalPadding(FMargin(0.0f));
		Style.SetPressedPadding(FMargin(0.0f, 1.0f, 0.0f, -1.0f));
		return Cache.Add(K, MakeUnique<FButtonStyle>(Style)).Get();
	}

	const FButtonStyle* CardStyle(const FLinearColor& Accent, bool bSelected)
	{
		static TMap<uint32, TUniquePtr<FButtonStyle>> Cache;
		const uint32 K = Key(Accent, FLinearColor::Black, bSelected ? 1.0f : 0.0f, 0.0f, 11);
		if (TUniquePtr<FButtonStyle>* Found = Cache.Find(K))
		{
			return Found->Get();
		}
		FButtonStyle Style;
		const float R = 8.0f;
		const FLinearColor Rest = bSelected ? FMath::Lerp(Card, WithAlpha(Accent, 1.0f), 0.12f) : Card;
		Style.SetNormal(*Rounded(Rest, R, bSelected ? WithAlpha(Accent, 0.9f) : FLinearColor(1.0f, 1.0f, 1.0f, 0.07f), 1.0f));
		Style.SetHovered(*Rounded(FMath::Lerp(Card, WithAlpha(Accent, 1.0f), 0.16f), R, WithAlpha(Accent, 1.0f), 1.0f));
		Style.SetPressed(*Rounded(FMath::Lerp(Card, WithAlpha(Accent, 1.0f), 0.24f), R, WithAlpha(Accent, 1.0f), 1.0f));
		Style.SetDisabled(*Rounded(Card, R, FLinearColor(1.0f, 1.0f, 1.0f, 0.04f), 1.0f));
		Style.SetNormalPadding(FMargin(0.0f));
		Style.SetPressedPadding(FMargin(0.0f));
		return Cache.Add(K, MakeUnique<FButtonStyle>(Style)).Get();
	}

	const FEditableTextBoxStyle* TextBoxStyle()
	{
		static TUniquePtr<FEditableTextBoxStyle> Style;
		if (!Style)
		{
			Style = MakeUnique<FEditableTextBoxStyle>(FCoreStyle::Get().GetWidgetStyle<FEditableTextBoxStyle>("NormalEditableTextBox"));
			Style->SetBackgroundImageNormal(*Rounded(Well, 5.0f, FLinearColor(1.0f, 1.0f, 1.0f, 0.12f), 1.0f));
			Style->SetBackgroundImageHovered(*Rounded(Well, 5.0f, WithAlpha(Cyan, 0.6f), 1.0f));
			Style->SetBackgroundImageFocused(*Rounded(Well, 5.0f, Cyan, 1.0f));
			Style->SetBackgroundImageReadOnly(*Rounded(Well, 5.0f, Faint, 1.0f));
			Style->SetForegroundColor(FSlateColor(Ink));
			Style->SetPadding(FMargin(10.0f, 7.0f));
			Style->SetTextStyle(FTextBlockStyle(Style->TextStyle).SetFont(Font(12)).SetColorAndOpacity(FSlateColor(Ink)));
		}
		return Style.Get();
	}

	const FSlateBrush* IconBrush(FName ItemId)
	{
		static TMap<FName, TSharedPtr<FSlateBrush>> Cache;
		if (TSharedPtr<FSlateBrush>* Found = Cache.Find(ItemId))
		{
			return Found->Get();
		}
		const FString Name = FString::Printf(TEXT("T_Icon_%s"), *ItemId.ToString());
		UTexture2D* Texture = LoadObject<UTexture2D>(nullptr, *FString::Printf(TEXT("/Game/DCC/UI/Icons/%s.%s"), *Name, *Name));
		TSharedPtr<FSlateBrush> Brush;
		if (Texture)
		{
			Texture->AddToRoot(); // brushes do not hold their texture; the menus open and close all session
			Brush = MakeShared<FSlateImageBrush>(Texture, FVector2D(256.0f, 256.0f));
		}
		Cache.Add(ItemId, Brush);
		return Brush.Get();
	}

	TSharedRef<SWidget> Window(const FLinearColor& Accent, const TSharedRef<SWidget>& Content, FMargin Padding)
	{
		// Corner brackets: a short L at each corner, bright, just outside the edge.
		auto Bracket = [Accent](EHorizontalAlignment H, EVerticalAlignment V)
		{
			const float L = 18.0f, T = 2.0f;
			return SNew(SBox).WidthOverride(L).HeightOverride(L)
				[
					SNew(SOverlay)
					+ SOverlay::Slot().HAlign(HAlign_Fill).VAlign(V)
					[
						SNew(SBox).HeightOverride(T)[ SNew(SBorder).BorderImage(Solid()).BorderBackgroundColor(Accent) ]
					]
					+ SOverlay::Slot().HAlign(H).VAlign(VAlign_Fill)
					[
						SNew(SBox).WidthOverride(T)[ SNew(SBorder).BorderImage(Solid()).BorderBackgroundColor(Accent) ]
					]
				];
		};
		return SNew(SOverlay)
			// A soft outer glow: the edge colour, very faint, a few pixels out.
			+ SOverlay::Slot().Padding(-6.0f)
			[
				SNew(SBorder).BorderImage(Rounded(FLinearColor::Transparent, 12.0f, WithAlpha(Accent, 0.08f), 6.0f))
			]
			+ SOverlay::Slot()
			[
				SNew(SBorder).BorderImage(Rounded(Glass, 8.0f, WithAlpha(Accent, 0.35f), 1.0f)).Padding(Padding)
				[
					Content
				]
			]
			// The top hairline: bright in the middle third.
			+ SOverlay::Slot().VAlign(VAlign_Top).HAlign(HAlign_Center).Padding(0.0f, -1.0f, 0.0f, 0.0f)
			[
				SNew(SBox).WidthOverride(260.0f).HeightOverride(2.0f)[ SNew(SBorder).BorderImage(Solid()).BorderBackgroundColor(Accent) ]
			]
			+ SOverlay::Slot().HAlign(HAlign_Left).VAlign(VAlign_Top).Padding(-3.0f, -3.0f, 0.0f, 0.0f)[ Bracket(HAlign_Left, VAlign_Top) ]
			+ SOverlay::Slot().HAlign(HAlign_Right).VAlign(VAlign_Top).Padding(0.0f, -3.0f, -3.0f, 0.0f)[ Bracket(HAlign_Right, VAlign_Top) ]
			+ SOverlay::Slot().HAlign(HAlign_Left).VAlign(VAlign_Bottom).Padding(-3.0f, 0.0f, 0.0f, -3.0f)[ Bracket(HAlign_Left, VAlign_Bottom) ]
			+ SOverlay::Slot().HAlign(HAlign_Right).VAlign(VAlign_Bottom).Padding(0.0f, 0.0f, -3.0f, -3.0f)[ Bracket(HAlign_Right, VAlign_Bottom) ];
	}

	TSharedRef<SWidget> Header(const FText& Kicker, const TAttribute<FText>& Title, const TAttribute<FSlateColor>& TitleColour)
	{
		return SNew(SVerticalBox)
			+ SVerticalBox::Slot().AutoHeight()
			[
				SNew(STextBlock).Text(Kicker).Font(Caps(9, 260)).ColorAndOpacity(WithAlpha(Cyan, 0.9f))
			]
			+ SVerticalBox::Slot().AutoHeight().Padding(0.0f, 2.0f, 0.0f, 0.0f)
			[
				SNew(STextBlock).Text(Title).Font(Font(30, "Light", 40)).ColorAndOpacity(TitleColour)
			];
	}

	TSharedRef<SWidget> Button(const TAttribute<FText>& Label, const FLinearColor& Accent, FOnClicked OnClicked,
		bool bPrimary, const TAttribute<bool>& Enabled, int32 Size, FMargin Padding)
	{
		return SNew(SButton)
			.ButtonStyle(ButtonStyle(Accent, bPrimary))
			.IsFocusable(false)
			.IsEnabled(Enabled)
			.HAlign(HAlign_Center)
			.VAlign(VAlign_Center)
			.ContentPadding(Padding)
			.OnClicked(OnClicked)
			[
				SNew(STextBlock).Text(Label).Font(Caps(Size, 140))
				.ColorAndOpacity(bPrimary ? FSlateColor(FLinearColor(0.03f, 0.03f, 0.06f)) : FSlateColor(FMath::Lerp(Accent, FLinearColor::White, 0.35f)))
			];
	}

	TSharedRef<SWidget> Rule(const FLinearColor& Colour)
	{
		return SNew(SBox).HeightOverride(1.0f)[ SNew(SBorder).BorderImage(Solid()).BorderBackgroundColor(Colour) ];
	}

	TSharedRef<SWidget> Balance(const TAttribute<FText>& Number, const FText& Caption, const FLinearColor& Colour)
	{
		return SNew(SBorder).BorderImage(Rounded(WithAlpha(Colour, 0.08f), 6.0f, WithAlpha(Colour, 0.4f), 1.0f)).Padding(FMargin(16.0f, 6.0f))
			[
				SNew(SVerticalBox)
				+ SVerticalBox::Slot().AutoHeight().HAlign(HAlign_Right)
				[
					SNew(STextBlock).Text(Number).Font(Font(20, "Light", 20)).ColorAndOpacity(Colour)
				]
				+ SVerticalBox::Slot().AutoHeight().HAlign(HAlign_Right)
				[
					SNew(STextBlock).Text(Caption).Font(Caps(7, 220)).ColorAndOpacity(WithAlpha(Colour, 0.7f))
				]
			];
	}

	TSharedRef<SWidget> Tag(const TAttribute<FText>& Text, const TAttribute<FSlateColor>& Colour)
	{
		return SNew(SBorder).BorderImage(Rounded(FLinearColor(0.0f, 0.0f, 0.0f, 0.55f), 4.0f, FLinearColor(1.0f, 1.0f, 1.0f, 0.12f), 1.0f))
			.Padding(FMargin(7.0f, 2.0f))
			[
				SNew(STextBlock).Text(Text).Font(Caps(7, 160)).ColorAndOpacity(Colour)
			];
	}

	TSharedRef<SWidget> Icon(FName ItemId, float Size, const FLinearColor& Accent)
	{
		const FSlateBrush* Brush = IconBrush(ItemId);
		return SNew(SBox).WidthOverride(Size).HeightOverride(Size)
			[
				SNew(SOverlay)
				// A soft plate behind the picture, in the item's colour.
				+ SOverlay::Slot().Padding(Size * 0.06f)
				[
					SNew(SBorder).BorderImage(Rounded(WithAlpha(Accent, 0.1f), Size * 0.5f, WithAlpha(Accent, 0.28f), 1.0f))
				]
				+ SOverlay::Slot()
				[
					Brush ? StaticCastSharedRef<SWidget>(SNew(SImage).Image(Brush)) : StaticCastSharedRef<SWidget>(SNew(SBox))
				]
			];
	}
}
