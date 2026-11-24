#pragma once

class UTexture2D;

#include "CoreMinimal.h"
#include "Fonts/SlateFontInfo.h"
#include "Input/Reply.h"
#include "Layout/Margin.h"
#include "Misc/Attribute.h"
#include "Styling/SlateColor.h"

class SWidget;
struct FSlateBrush;
struct FButtonStyle;
struct FEditableTextBoxStyle;

/**
 * One look for every menu (2026-11-14, Zoe: the menus were "still kind of basic... blocky bright colors. Can you make
 * the menus more sleek and futuristic / isekai looking like the rest?").
 *
 * The System AI's windows: dark glass with a thin neon edge, corner brackets, a hairline of light along the top,
 * small spaced-out capitals for labels, and rounded buttons that light up on hover instead of flat blocks of colour.
 * The same palette as the in-game HUD panels (UI/DCCHUD_RallyMenus.cpp), so the menus and the HUD read as one system.
 *
 * Plain Slate, no assets except the item pictures (/Game/DCC/UI/Icons, Tools/render_item_icons.py).
 */
namespace DCCMenu
{
	// ---- palette ------------------------------------------------------------------------------------------
	extern const FLinearColor Glass;     // window fill
	extern const FLinearColor Well;      // recessed areas (lists, inputs)
	extern const FLinearColor Card;      // a card at rest
	extern const FLinearColor Cyan;      // the System's own colour: edges, focus
	extern const FLinearColor Pink;
	extern const FLinearColor Gold;
	extern const FLinearColor Mint;
	extern const FLinearColor Red;
	extern const FLinearColor Ink;       // body text
	extern const FLinearColor Dim;       // secondary text
	extern const FLinearColor Faint;     // hairlines

	FLinearColor WithAlpha(const FLinearColor& C, float A);

	// ---- type ---------------------------------------------------------------------------------------------
	/** Roboto. Weight "Regular", "Bold" or "Light". Spacing in 1/1000 em. */
	FSlateFontInfo Font(int32 Size, const char* Weight = "Regular", int32 Spacing = 0);
	/** Small spaced-out capitals: labels, kickers, button text. */
	FSlateFontInfo Caps(int32 Size, int32 Spacing = 160);

	// ---- brushes and styles (cached, stable for the life of the process) -----------------------------------
	const FSlateBrush* Rounded(const FLinearColor& Fill, float Radius = 6.0f, const FLinearColor& Outline = FLinearColor::Transparent, float OutlineWidth = 0.0f);
	const FSlateBrush* Solid();
	/** A rounded button: outlined glass that fills with Accent on hover. bPrimary starts filled. */
	const FButtonStyle* ButtonStyle(const FLinearColor& Accent, bool bPrimary);
	/** A card that is also a button: Card fill, a faint edge that turns Accent on hover. */
	const FButtonStyle* CardStyle(const FLinearColor& Accent, bool bSelected);
	const FEditableTextBoxStyle* TextBoxStyle();

	/** An item's picture (T_Icon_<Id>), or null if there is none. */
	/** The item's icon picture: T_Icon_<Id> if one was made by hand, else Content/DCC/UI/ItemIcons/<Id>.png. */
	UTexture2D* IconTexture(FName ItemId);
	const FSlateBrush* IconBrush(FName ItemId);

	// ---- pieces ---------------------------------------------------------------------------------------------
	/** The window frame: glass, a neon edge, corner brackets and a top hairline, around Content. */
	TSharedRef<SWidget> Window(const FLinearColor& Accent, const TSharedRef<SWidget>& Content, FMargin Padding = FMargin(30.0f, 24.0f));

	/** "SYSTEM  /  THE DEN" over a big title, with a hairline under it. */
	TSharedRef<SWidget> Header(const FText& Kicker, const TAttribute<FText>& Title, const TAttribute<FSlateColor>& TitleColour);

	/** A rounded button with spaced capitals. */
	TSharedRef<SWidget> Button(const TAttribute<FText>& Label, const FLinearColor& Accent, FOnClicked OnClicked,
		bool bPrimary = false, const TAttribute<bool>& Enabled = true, int32 Size = 11, FMargin Padding = FMargin(18.0f, 9.0f));

	/** A thin horizontal rule. */
	TSharedRef<SWidget> Rule(const FLinearColor& Colour = FLinearColor(1.0f, 1.0f, 1.0f, 0.08f));

	/** A balance readout: a number over a small caption, in a rounded chip. */
	TSharedRef<SWidget> Balance(const TAttribute<FText>& Number, const FText& Caption, const FLinearColor& Colour);

	/** A small rounded tag ("OWNED", "x2", "LV 2/3"). */
	TSharedRef<SWidget> Tag(const TAttribute<FText>& Text, const TAttribute<FSlateColor>& Colour);

	/** An item's picture on a soft round plate in Accent; the plate alone when there is no picture. */
	TSharedRef<SWidget> Icon(FName ItemId, float Size, const FLinearColor& Accent);
}
