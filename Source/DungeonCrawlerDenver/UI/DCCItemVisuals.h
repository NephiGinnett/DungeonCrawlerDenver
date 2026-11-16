#pragma once

#include "CoreMinimal.h"

enum class EDCCLootBoxTier : uint8;
enum class EDCCEquipmentSlot : uint8;

/**
 * Shared look for items: rarity colours (after Elyra's Tales' enchanting.gd)
 * and the drawn icon set. Used by the item pop-ups and the loot box screen,
 * so an item reads the same everywhere. Defined in DCCHUD_ItemToasts.cpp.
 */
namespace DCCItemToast
{
	enum EIcon : uint8
	{
		Bag = 0,
		Sword,
		Shield,
		Helmet,
		Tunic,
		Ring,
		Potion,
		LootBox,
		Coin
	};

	FLinearColor RarityColour(EDCCLootBoxTier Tier);
	FString RarityName(EDCCLootBoxTier Tier);
	uint8 IconForSlot(EDCCEquipmentSlot Slot);
}
