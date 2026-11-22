#pragma once

#include "CoreMinimal.h"

struct FDCCLootDrop;

/**
 * Random chest contents from the live equipment table (2026-11-22, Zoe: "make sure that loot is randomized from
 * appropriate tables. It seems like I've gotten the same chest on the first level of gate run several times").
 *
 * The curated chests (DCCLootContainerVariants) always hold the same three items; that is right for a hand-placed
 * chest and wrong for a run. This rolls 1 to 3 different items from DT_DCCEquipment, rarity weighted by how deep or
 * hard the place is, plus a gold stash. Rows with no model at all are skipped, so nothing invisible drops.
 */
namespace DCCGearRoller
{
	/**
	 * Server. Rank 0-5 (Gate Run's E to S), Bonus 0-3 for a better-than-usual chest (a solved vault, a champion).
	 * Returns guaranteed drops (Chance 1) ready for an ADCCLootContainer's LootTable.
	 */
	TArray<FDCCLootDrop> Roll(int32 Rank, int32 Bonus);

	/** "Gold", "Legendary"... of the best item in the last Roll, for naming the chest. */
	FString LastBestRarity();
}
