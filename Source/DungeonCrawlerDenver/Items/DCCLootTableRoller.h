#pragma once

#include "CoreMinimal.h"

class UDataTable;
class UWorld;
class ADCCCharacterBase;

// ---------------------------------------------------------------------------
// Rolls a named FDCCLootTableRow and spawns the result, scattered around a
// world-space point rather than a corpse. Mirrors
// ADCCCharacterBase::RollLootOntoBody/LootRemains's odds and spawn shape
// exactly - same per-entry independent Chance roll, same MinGold/MaxGold
// range - so a Loot Vault reward and a mob's drop read the same way to a
// player even though they come from different code paths.
//
// A free function in a namespace rather than a UCLASS, matching
// DCCCrawlerAnimSet's precedent for a stateless helper nothing has an
// object-lifetime relationship with.
// ---------------------------------------------------------------------------

namespace DCCLootTableRoller
{
	/**
	 * Server only. Rolls every entry in the named row and spawns what hits,
	 * scattered around Origin by each entry's own ScatterRadius.
	 *
	 * @param LootTableAsset  The DataTable holding FDCCLootTableRow rows.
	 * @param RowName         Which row to roll. A missing row logs a warning
	 *                        and spawns nothing, the same "fail loud at the
	 *                        authoring mistake" choice UDCCSkillComponent::GrantSkill
	 *                        makes for a bad skill id.
	 * @param Origin          World-space centre to scatter drops around.
	 *
	 * @return How many pickups were actually spawned.
	 */
	int32 RollAndSpawn(UWorld* World, const UDataTable* LootTableAsset, FName RowName, const FVector& Origin);
}
