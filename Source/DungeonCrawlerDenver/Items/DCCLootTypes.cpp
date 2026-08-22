#include "Items/DCCLootTypes.h"

#include "Items/DCCPickup.h"
#include "Engine/World.h"

namespace DCCLoot
{
	void RollLootTable(const TArray<FDCCLootDrop>& Table, TArray<FDCCRolledLoot>& OutRolled)
	{
		OutRolled.Reset();

		for (const FDCCLootDrop& Drop : Table)
		{
			if (!Drop.PickupClass || FMath::FRand() > Drop.Chance)
			{
				continue;
			}

			FDCCRolledLoot Rolled;
			Rolled.PickupClass = Drop.PickupClass;
			Rolled.EquipmentItemId = Drop.EquipmentItemId;

			if (Drop.MaxGold > 0)
			{
				const int32 Low = FMath::Min(Drop.MinGold, Drop.MaxGold);
				Rolled.Gold = FMath::RandRange(Low, Drop.MaxGold);
			}

			OutRolled.Add(Rolled);
		}
	}

	ADCCPickup* SpawnRolledLoot(UWorld* World, const FDCCRolledLoot& Rolled, const FVector& Origin)
	{
		if (!World || !Rolled.PickupClass)
		{
			return nullptr;
		}

		// Tight scatter rather than the old wide spread - these are being emptied
		// at the looter's feet, not flung from a point of death.
		const FVector2D Offset = FMath::RandPointInCircle(60.0f);
		const FVector SpawnLocation = Origin + FVector(Offset.X, Offset.Y, 40.0f);

		FActorSpawnParameters Params;
		Params.SpawnCollisionHandlingOverride =
			ESpawnActorCollisionHandlingMethod::AdjustIfPossibleButAlwaysSpawn;

		ADCCPickup* Spawned = World->SpawnActor<ADCCPickup>(
			Rolled.PickupClass, SpawnLocation, FRotator::ZeroRotator, Params);

		if (!Spawned)
		{
			return nullptr;
		}

		if (Rolled.Gold > 0)
		{
			Spawned->GoldValue = Rolled.Gold;
		}

		if (!Rolled.EquipmentItemId.IsNone())
		{
			Spawned->EquipmentItemId = Rolled.EquipmentItemId;
		}

		return Spawned;
	}
}
