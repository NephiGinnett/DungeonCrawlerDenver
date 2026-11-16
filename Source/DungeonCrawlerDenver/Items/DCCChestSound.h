#pragma once

#include "CoreMinimal.h"

/**
 * The sound of a chest opening (2026-11-15, Zoe: chests "need to be updated to have a creek or latch sound when
 * they open"): a metal latch clicking, then a wooden hinge creaking. Synthesized at runtime like the arcade's other
 * effects (no pack ships one), played locally where the chest is, on the machine that opened it.
 * Used by the Fourteener supply chests (ADCCSupplyChest) and every loot chest (ADCCLootContainer, via the loot menu).
 */
namespace DCCChestSound
{
	DUNGEONCRAWLERDENVER_API void PlayOpen(const UObject* WorldContext, const FVector& At);
}
