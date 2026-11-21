#pragma once

#include "CoreMinimal.h"

enum class EDCCLootBoxTier : uint8;
enum class EDCCEquipmentSlot : uint8;

/**
 * Shared look for items: rarity colours (after Elyra's Tales' enchanting.gd)
 * and the drawn icon set. Used by the item pop-ups and the loot box screen,
 * so an item reads the same everywhere. Defined in DCCHUD_ItemToasts.cpp.
 */
class ADCCCharacterBase;

namespace DCCItemToast
{
	/**
	 * The save (or a fresh start) has been applied to this pawn. Item cards wait
	 * for this on the host, then treat whatever is already carried as old news.
	 * A client never runs the save; it waits until the replicated bag sits still.
	 */
	void NoteCarryIn(const ADCCCharacterBase* Crawler);
	bool HasCarryIn(const ADCCCharacterBase* Crawler);

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
