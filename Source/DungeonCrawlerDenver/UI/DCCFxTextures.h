#pragma once

#include "CoreMinimal.h"

class UTexture2D;

/**
 * Small effect pictures made in code once and kept (2026-11-23): a soft round glow, a starburst of rays, a four-point
 * sparkle, and a diamond. White, with the shape in both colour and alpha, so they tint to any colour and work drawn
 * additively on the Canvas or as Slate brushes. Used by the loot box rarities and the event panel.
 */
namespace DCCFx
{
	UTexture2D* Glow();
	UTexture2D* Rays();
	UTexture2D* Sparkle();
	UTexture2D* Diamond();
}
