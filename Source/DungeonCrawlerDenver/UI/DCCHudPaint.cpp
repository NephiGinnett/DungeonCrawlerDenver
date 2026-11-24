#include "UI/DCCHudPaint.h"

#include "Engine/Engine.h"
#include "Engine/Font.h"
#include "Fonts/FontMeasure.h"
#include "Framework/Application/SlateApplication.h"
#include "Rendering/DrawElements.h"
#include "Rendering/SlateRenderer.h"
#include "Styling/CoreStyle.h"
#include "UI/DCCLook.h"

void SDCCHudPaint::Construct(const FArguments& InArgs)
{
	SetVisibility(EVisibility::HitTestInvisible);
}

void SDCCHudPaint::Publish(TArray<FDCCHudCmd>&& Commands)
{
	Painted = MoveTemp(Commands);
	PublishedFrame = GFrameCounter;
}

int32 SDCCHudPaint::OnPaint(const FPaintArgs& Args, const FGeometry& AllottedGeometry, const FSlateRect& MyCullingRect,
	FSlateWindowElementList& OutDrawElements, int32 LayerId, const FWidgetStyle& InWidgetStyle, bool bParentEnabled) const
{
	// A HUD that stopped drawing (travel, a hidden HUD) leaves nothing behind.
	if (Painted.IsEmpty() || GFrameCounter > PublishedFrame + 3)
	{
		return LayerId;
	}
	// Commands are in Canvas pixels; this widget's local units are pixels over the DPI scale.
	const float S = FMath::Max(0.01f, AllottedGeometry.Scale);
	const float Inv = 1.0f / S;
	const TSharedRef<FSlateFontMeasure> Measure = FSlateApplication::Get().GetRenderer()->GetFontMeasureService();

	int32 Layer = LayerId;
	FDCCHudCmd::EKind PreviousKind = FDCCHudCmd::KText;
	bool bPreviousWasPanel = false;
	for (const FDCCHudCmd& C : Painted)
	{
		// Slate batches by layer, so a new layer whenever the order matters: a box after text or a picture, and
		// around every panel (a panel must cover what came before it and sit under what comes after).
		const bool bPanel = C.Kind == FDCCHudCmd::KBox && C.Edge.A > 0.0f;
		if (C.Kind != PreviousKind || bPanel || bPreviousWasPanel)
		{
			++Layer;
		}
		PreviousKind = C.Kind;
		bPreviousWasPanel = bPanel;

		switch (C.Kind)
		{
		case FDCCHudCmd::KBox:
		{
			const FVector2f Size = C.Size * Inv;
			const FSlateLayoutTransform At(C.Pos * Inv);
			const float R = FMath::Min(C.Radius, FMath::Min(Size.X, Size.Y) * 0.5f);
			// MakeBox's tint is the fill; a rounded brush's own outline is drawn in its own colour. So a panel is one box
			// with an outlined brush, tinted with the fill.
			const FSlateBrush* Brush = bPanel ? DCCLook::Outline(FMath::Max(R, 0.0f), C.Edge, 1.0f)
				: R >= 1.0f ? DCCLook::RoundedWhite(R) : FCoreStyle::Get().GetBrush("WhiteBrush");
			FSlateDrawElement::MakeBox(OutDrawElements, Layer, AllottedGeometry.ToPaintGeometry(Size, At), Brush, ESlateDrawEffect::None, C.Colour);
			break;
		}
		case FDCCHudCmd::KText:
		{
			FSlateFontInfo Font = C.Font;
			Font.Size = C.Font.Size * Inv;
			const float LineH = Measure->GetMaxCharacterHeight(C.Font);
			TArray<FString> Lines;
			C.Text.ParseIntoArray(Lines, TEXT("\n"), false);
			for (int32 i = 0; i < Lines.Num(); ++i)
			{
				if (Lines[i].IsEmpty())
				{
					continue;
				}
				const FVector2f P = (C.Pos + FVector2f(0.0f, LineH * i)) * Inv;
				const FVector2f Box(4096.0f * Inv, LineH * Inv + 4.0f);
				if (C.bShadow)
				{
					FSlateDrawElement::MakeText(OutDrawElements, Layer, AllottedGeometry.ToPaintGeometry(Box, FSlateLayoutTransform(P + FVector2f(1.0f, 1.0f) * Inv)),
						Lines[i], Font, ESlateDrawEffect::None, FLinearColor(0.0f, 0.0f, 0.0f, 0.45f * C.Colour.A));
				}
				FSlateDrawElement::MakeText(OutDrawElements, Layer, AllottedGeometry.ToPaintGeometry(Box, FSlateLayoutTransform(P)),
					Lines[i], Font, ESlateDrawEffect::None, C.Colour);
			}
			break;
		}
		case FDCCHudCmd::KLine:
		{
			TArray<FVector2f> Points;
			Points.Add(C.Pos * Inv);
			Points.Add(C.Size * Inv);
			FSlateDrawElement::MakeLines(OutDrawElements, Layer, AllottedGeometry.ToPaintGeometry(), Points, ESlateDrawEffect::None,
				C.Colour, true, FMath::Max(1.0f, C.Radius) * Inv);
			break;
		}
		case FDCCHudCmd::KImage:
		{
			FSlateDrawElement::MakeBox(OutDrawElements, Layer, AllottedGeometry.ToPaintGeometry(C.Size * Inv, FSlateLayoutTransform(C.Pos * Inv)),
				&C.Brush, ESlateDrawEffect::None, C.Colour);
			break;
		}
		}
	}
	return Layer;
}

namespace DCCHudPaint
{
	FSlateFontInfo FontFor(UFont* Font, float Scale)
	{
		// Point sizes that match the engine bitmap fonts' line heights at scale 1 (the same table Repo Rally's crisp
		// text uses), so every hand-placed line in the HUD still lands where it did.
		const bool bLarge = GEngine && Font == GEngine->GetLargeFont();
		const bool bMedium = !Font || (GEngine && Font == GEngine->GetMediumFont());
		const bool bTiny = GEngine && Font == GEngine->GetTinyFont();
		float Base = bLarge ? 11.0f : bMedium ? 10.0f : bTiny ? 6.5f : 7.6f;
		if (Font && !bLarge && !bMedium && !bTiny && GEngine && Font != GEngine->GetSmallFont() && Font->GetLegacySlateFontInfo().Size > 0)
		{
			Base = Font->GetLegacySlateFontInfo().Size * 0.75f;
		}
		const float Size = FMath::Max(5.0f, Base * Scale);
		return bLarge || Size >= 16.0f ? DCCLook::Head(Size, 0) : DCCLook::Body(Size, 0);
	}

	FVector2D Measure(const FString& Text, const FSlateFontInfo& Font)
	{
		if (!FSlateApplication::IsInitialized())
		{
			return FVector2D(Text.Len() * Font.Size * 0.6f, Font.Size * 1.3f);
		}
		return FSlateApplication::Get().GetRenderer()->GetFontMeasureService()->Measure(Text, Font);
	}

	bool IsPanel(const FLinearColor& Colour, float W, float H)
	{
		return W >= 80.0f && H >= 28.0f && Colour.A >= 0.35f && FMath::Max3(Colour.R, Colour.G, Colour.B) < 0.2f;
	}
}
