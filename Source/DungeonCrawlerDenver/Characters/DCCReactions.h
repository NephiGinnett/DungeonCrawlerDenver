#pragma once

#include "CoreMinimal.h"

class UTexture2D;

/**
 * Reactions (2026-11-16, Zoe: "players could have more expressive forms maybe? It's a bit dull"): a pixel-art bubble
 * that pops over a crawler's head, often with a move to match. They fire on their own at the big moments (a kill,
 * the summit, a ghost, an enemy spotting you, going down, a hard fall, running out of breath) and from the B menu.
 *
 * The bubbles are drawn from small masks in code, so they need no imported art and stay crisp at any size.
 */
namespace DCCReactions
{
	enum EReaction : uint8
	{
		None = 0,
		Cheer,      // a star: a kill, the summit, a bounty
		Alert,      // "!": an enemy has seen you, a ghost
		Confused,   // "?"
		Love,       // a heart: picked up, revived
		Tired,      // a sweat drop: out of breath, a hard fall
		Down,       // a skull: down
		Party,      // a note: dancing, a tag sprayed
		Cold,       // a snowflake: high on the mountain
		Count
	};

	struct FInfo
	{
		const TCHAR* Name;      // in the B menu
		FLinearColor Colour;    // the glyph
		const TCHAR* Clip;      // the move that goes with it (shared crawler clip), or null
		float ClipSeconds;
	};

	const FInfo& Info(uint8 Reaction);

	/** The bubble for a reaction: 32x32, nearest-filtered, built once per machine. */
	UTexture2D* Icon(uint8 Reaction);

	/** How long a bubble stays up. */
	constexpr float Seconds = 2.2f;

	// ---- The moments, and each player's choice of bubble and move for them (2026-11-16, Zoe: "can you make it all
	// adjustable to ... different emotes showing up at those moments?") ----------------------------------------------
	enum EMoment : uint8
	{
		MomentKill = 0,  // you put something down
		MomentWin,       // the summit, an achievement, a finished bounty
		MomentSpotted,   // an enemy picks you; a ghost in Dead Air
		MomentRevived,   // back on your feet
		MomentHurt,      // out of breath, a hard fall
		MomentDowned,    // you go down
		MomentParty,     // a tag sprayed
		MomentCold,      // high on the mountain
		MomentCount
	};
	const TCHAR* MomentName(uint8 Moment);

	/** Moves a moment can play. Index 0 is "no move". */
	struct FMove { const TCHAR* Name; const TCHAR* Clip; };
	const TArray<FMove>& Moves();

	/** A bubble (EReaction, None for no bubble) and a move (index into Moves, 0 for none). */
	struct FChoice { uint8 Bubble; uint8 Move; };
	FChoice DefaultFor(uint8 Moment);
	/** The move that goes with a reaction picked from the B menu. */
	uint8 DefaultMoveFor(uint8 Reaction);

	/** This PC's choices, MomentCount pairs (bubble, move), from GameUserSettings.ini; defaults where unset. */
	TArray<uint8> LoadChoices();
	void SaveChoices(const TArray<uint8>& Choices);
}
