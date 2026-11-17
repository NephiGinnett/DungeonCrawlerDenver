#pragma once

#include "CoreMinimal.h"

class UTexture2D;

/**
 * A small pixel-art logo for each arcade game (2026-11-16, Zoe: "just add a small logo for each game and the number of
 * players underneath that"): a tile in the game's colour with its glyph. Built in code, like the reaction bubbles, so
 * they need no imported art. Used by the Den's vote strip.
 */
namespace DCCGameLogos
{
	/** 32x32, nearest-filtered, built once per machine. Null for an unknown game. */
	UTexture2D* Logo(FName GameId, const FLinearColor& Colour);
}
