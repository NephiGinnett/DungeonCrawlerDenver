#pragma once

#include "CoreMinimal.h"
#include "Fonts/SlateFontInfo.h"
#include "Styling/SlateBrush.h"
#include "Widgets/SLeafWidget.h"

class UTexture;

/**
 * The HUD, drawn through Slate (2026-11-23, Zoe: "can you make the rest of the menu and the HUD look similar?").
 *
 * ADCCHUD draws its ~9000 lines of HUD with AHUD's DrawRect / DrawText / DrawLine / DrawTexture. ADCCHUD now hides those
 * four with its own versions, which record each call here instead of drawing it on the Canvas. SDCCHudPaint, a
 * full-screen, click-through widget, replays the list every frame in Slate, which is what lets the same code get:
 *   - the look's fonts (UI/DCCLook.h) instead of the engine's bitmap Roboto, measured the same way for layout;
 *   - rounded panels in the look's colours, with a fine edge, instead of flat rectangles;
 *   - a soft shadow under text.
 * Click boxes are unaffected: they are AHUD hit boxes in the same pixel coordinates.
 *
 * Canvas-only parts (triangles, additive pictures, the speed gauge, the minimap) still draw on the Canvas, which is
 * under every Slate widget, so they are drawn whole inside a FCanvasOnly scope rather than half and half.
 */
struct FDCCHudCmd
{
	enum EKind : uint8 { KBox, KText, KLine, KImage };
	EKind Kind = KBox;
	FVector2f Pos = FVector2f::ZeroVector;
	FVector2f Size = FVector2f::ZeroVector;   // Box/Image size; Line end point
	FLinearColor Colour = FLinearColor::White;
	float Radius = 0.0f;                      // Box rounding; Line thickness
	FLinearColor Edge = FLinearColor::Transparent;
	FString Text;
	FSlateFontInfo Font;                      // Size in Slate points at scale 1 (one Canvas pixel = one unit)
	FSlateBrush Brush;                        // Image
	bool bShadow = false;
};

class SDCCHudPaint : public SLeafWidget
{
public:
	SLATE_BEGIN_ARGS(SDCCHudPaint) {}
	SLATE_END_ARGS()

	void Construct(const FArguments& InArgs);

	/** The list to draw, swapped in by the HUD once a frame. */
	void Publish(TArray<FDCCHudCmd>&& Commands);

	virtual int32 OnPaint(const FPaintArgs& Args, const FGeometry& AllottedGeometry, const FSlateRect& MyCullingRect,
		FSlateWindowElementList& OutDrawElements, int32 LayerId, const FWidgetStyle& InWidgetStyle, bool bParentEnabled) const override;
	virtual FVector2D ComputeDesiredSize(float) const override { return FVector2D(8.0f, 8.0f); }

private:
	TArray<FDCCHudCmd> Painted;
	uint64 PublishedFrame = 0;
};

namespace DCCHudPaint
{
	/** The Slate font that stands in for a Canvas UFont at a scale, in the current look. */
	FSlateFontInfo FontFor(class UFont* Font, float Scale);
	/** Its size in pixels, the way GetTextSize reports it. */
	FVector2D Measure(const FString& Text, const FSlateFontInfo& Font);
	/** Is this rect a panel (big and dark) that should get the look's rounded window treatment? */
	bool IsPanel(const FLinearColor& Colour, float W, float H);
}
