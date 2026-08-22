#pragma once

#include "CoreMinimal.h"
#include "Items/DCCLootContainer.h"
#include "DCCLootContainerVariants.generated.h"

// ---------------------------------------------------------------------------
// FOUR CURATED CHESTS
//
// ADCCLootContainer's LootTable is instance-editable, so any chest can be
// authored by hand once MCP is available. These four exist so the floor has
// *something* to place today: each is a C++ subclass with its contents
// guaranteed (Chance = 1.0), matching the pattern DCCPickupVariants already
// uses for the same reason - a constructor is automatable and reviewable in a
// diff, where an MCP-authored Blueprint instance is neither until the editor
// is back.
//
// All four spawn ADCCPickup_Equipment for their gear entries, which resolves
// its own ground mesh from DT_DCCEquipment - no per-item C++ class needed.
// ---------------------------------------------------------------------------

/** Melee gear, kitchen theme: the mallet, the knife, the skillet. */
UCLASS()
class DUNGEONCRAWLERDENVER_API ADCCLootContainer_KitchenDrawer : public ADCCLootContainer
{
	GENERATED_BODY()

public:
	ADCCLootContainer_KitchenDrawer();
};

/** Melee gear, "management" theme: the fire axe, the wall sword, the shield. */
UCLASS()
class DUNGEONCRAWLERDENVER_API ADCCLootContainer_Armory : public ADCCLootContainer
{
	GENERATED_BODY()

public:
	ADCCLootContainer_Armory();
};

/**
 * Ranged gear - the crossbow, the nail gun, the flare gun.
 *
 * The headline chest: three items with real AttackRangeOverride values, so a
 * crawler who opens this one plays differently afterward rather than just
 * holding a reskinned melee weapon.
 */
UCLASS()
class DUNGEONCRAWLERDENVER_API ADCCLootContainer_Quartermaster : public ADCCLootContainer
{
	GENERATED_BODY()

public:
	ADCCLootContainer_Quartermaster();
};

/** Head/Body/Trinket gear plus a real gold stash - the payday chest. */
UCLASS()
class DUNGEONCRAWLERDENVER_API ADCCLootContainer_ManagementLockbox : public ADCCLootContainer
{
	GENERATED_BODY()

public:
	ADCCLootContainer_ManagementLockbox();
};
