#include "Items/DCCLootTableRoller.h"

#include "Items/DCCLootTypes.h"
#include "Items/DCCPickup.h"
#include "Engine/DataTable.h"
#include "Engine/World.h"

namespace DCCLootTableRoller
{
	int32 RollAndSpawn(UWorld* World, const UDataTable* LootTableAsset, FName RowName, const FVector& Origin)
	{
		if (!World || !LootTableAsset)
		{
			return 0;
		}

		static const FString ContextString(TEXT("DCCLootTableRoller::RollAndSpawn"));
		const FDCCLootTableRow* Row = LootTableAsset->FindRow<FDCCLootTableRow>(RowName, ContextString);
		if (!Row)
		{
			UE_LOG(LogTemp, Warning, TEXT("DCCLootTableRoller: '%s' is not a row in %s."),
				*RowName.ToString(), *LootTableAsset->GetName());
			return 0;
		}

		int32 SpawnedCount = 0;

		for (const FDCCLootDrop& Drop : Row->Entries)
		{
			if (!Drop.PickupClass || FMath::FRand() > Drop.Chance)
			{
				continue;
			}

			const FVector2D Offset = FMath::RandPointInCircle(Drop.ScatterRadius);
			const FVector SpawnLocation = Origin + FVector(Offset.X, Offset.Y, 40.0f);

			FActorSpawnParameters Params;
			Params.SpawnCollisionHandlingOverride =
				ESpawnActorCollisionHandlingMethod::AdjustIfPossibleButAlwaysSpawn;

			ADCCPickup* Pickup = World->SpawnActor<ADCCPickup>(
				Drop.PickupClass, SpawnLocation, FRotator::ZeroRotator, Params);

			if (!Pickup)
			{
				continue;
			}

			if (Drop.MaxGold > 0)
			{
				const int32 Low = FMath::Min(Drop.MinGold, Drop.MaxGold);
				Pickup->GoldValue = FMath::RandRange(Low, Drop.MaxGold);
			}

			if (!Drop.EquipmentItemId.IsNone())
			{
				Pickup->EquipmentItemId = Drop.EquipmentItemId;
			}

			++SpawnedCount;
		}

		return SpawnedCount;
	}
}
