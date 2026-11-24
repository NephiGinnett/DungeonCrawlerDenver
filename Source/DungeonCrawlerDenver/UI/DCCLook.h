#pragma once

#include "CoreMinimal.h"
#include "Fonts/SlateFontInfo.h"

struct FSlateBrush;

/**
 * The game's UI look (2026-11-23, Zoe: "can you make the rest of the menu and the HUD look similar?").
 *
 * One place for the three looks the CHARACTER sheet introduced, so the sheet, the Canvas HUD (drawn through Slate by
 * UI/DCCHudPaint) and the subtitles all agree. The look is the SETTINGS > STYLE choice, saved as DCCUi.Style in
 * GameUserSettings: 0 SYSTEM GLASS, 1 NEON ARCADE, 2 BROADCAST.
 *
 * Fonts are the OFL files in Content/DCC/UI/Fonts, loaded from disk (staged by DefaultGame.ini).
 */
namespace DCCLook
{
	struct FLook
	{
		const TCHAR* Name;
		const TCHAR* HeadFile;   // display face (null: Roboto Bold)
		const TCHAR* BodyFile;   // body face (null: Roboto)
		float HeadScale;         // display faces differ in size at the same point size
		FLinearColor Fill;       // a window
		FLinearColor Panel;      // a section
		FLinearColor Card;       // a card
		FLinearColor Accent;
		FLinearColor Accent2;
		FLinearColor Ink;
		FLinearColor Dim;
		float Radius;            // corner rounding
		bool bSlantBars;         // TV-style hard bars rather than rounded glass
	};

	/** 0 SYSTEM GLASS, 1 NEON ARCADE, 2 BROADCAST. */
	DUNGEONCRAWLERDENVER_API int32 Index();
	DUNGEONCRAWLERDENVER_API void Set(int32 Look);
	DUNGEONCRAWLERDENVER_API const FLook& Get();
	DUNGEONCRAWLERDENVER_API const TCHAR* Name(int32 Look);

	/** Display face (headings, big numbers). Size in Slate points. */
	DUNGEONCRAWLERDENVER_API FSlateFontInfo Head(float Size, int32 Spacing = 40);
	/** Body face. */
	DUNGEONCRAWLERDENVER_API FSlateFontInfo Body(float Size, int32 Spacing = 0);
	/** A face from Content/DCC/UI/Fonts by file name (falls back to Roboto Bold). */
	DUNGEONCRAWLERDENVER_API FSlateFontInfo FromFile(const TCHAR* File, float Size, int32 Spacing = 0);
	/** Small tracked capitals for labels. */
	DUNGEONCRAWLERDENVER_API FSlateFontInfo Caps(float Size);

	/** A white rounded box (tint it when drawing). */
	DUNGEONCRAWLERDENVER_API const FSlateBrush* RoundedWhite(float Radius);
	/** An outline only, in a colour (cached; keep the colour set small). */
	DUNGEONCRAWLERDENVER_API const FSlateBrush* Outline(float Radius, const FLinearColor& Colour, float Width = 1.0f);

	/** A white-filled rounded box with an outline, for SBorder (its BorderBackgroundColor becomes the fill). */
	DUNGEONCRAWLERDENVER_API const FSlateBrush* Framed(float Radius, const FLinearColor& Colour, float Width = 1.0f);

	/** A speaker's own colours for subtitles: who is talking decides the box. */
	struct FVoice
	{
		FLinearColor Box;
		FLinearColor Edge;
		FLinearColor Name;
		FLinearColor Text;
		const TCHAR* Tag;        // a small label under the name ("ON AIR", "SYSTEM"), may be empty
		bool bLowerThird;        // a TV lower-third bar rather than a box
	};
	DUNGEONCRAWLERDENVER_API FVoice VoiceFor(const FString& Speaker, const FLinearColor& Fallback);
}
