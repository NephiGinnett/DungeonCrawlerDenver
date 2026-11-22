#include "Items/DCCGearRoller.h"

#include "Engine/DataTable.h"
#include "Equipment/DCCEquipmentTypes.h"
#include "Items/DCCLootTypes.h"
#include "Items/DCCPickup_Equipment.h"
#include "Items/DCCPickupVariants.h"

namespace DCCGearRoller
{
	namespace
	{
		FString GLastBest;

		const TCHAR* TierName(int32 Tier)
		{
			static const TCHAR* Names[] = { TEXT("Bronze"), TEXT("Bronze"), TEXT("Silver"), TEXT("Gold"), TEXT("Platinum"), TEXT("Legendary"), TEXT("Celestial") };
			return Names[FMath::Clamp(Tier, 0, 6)];
		}
	}

	FString LastBestRarity()
	{
		return GLastBest;
	}

	TArray<FDCCLootDrop> Roll(int32 Rank, int32 Bonus)
	{
		TArray<FDCCLootDrop> Out;
		GLastBest.Reset();
		const UDataTable* Table = LoadObject<UDataTable>(nullptr, TEXT("/Game/DCC/Equipment/DT_DCCEquipment.DT_DCCEquipment"));
		if (!Table)
		{
			UE_LOG(LogTemp, Warning, TEXT("[DCC Loot] No equipment table; the chest gets gold only."));
		}

		// Rows grouped by rarity (None counts as Bronze: the plain kit nobody graded).
		TArray<FName> ByTier[7];
		if (Table)
		{
			Table->ForeachRow<FDCCEquipmentDef>(TEXT("DCCGearRoller"), [&ByTier](const FName& Name, const FDCCEquipmentDef& Def)
			{
				if (Def.Mesh.IsNull() && Def.GroundMesh.IsNull())
				{
					return; // nothing to see on the floor
				}
				const int32 Tier = FMath::Clamp(static_cast<int32>(Def.Rarity), 1, 6);
				ByTier[Tier].Add(Name);
			});
		}

		// Weights per rarity. A rank E gate is mostly bronze and silver with the odd gold; an S gate leans gold and
		// platinum, and is the only place a celestial turns up. A better chest (Bonus) shifts it all up a step.
		const int32 Lift = FMath::Clamp(Rank, 0, 5) + 2 * FMath::Clamp(Bonus, 0, 3);
		const float Weights[7] = {
			0.0f,
			FMath::Max(4.0f, 60.0f - 9.0f * Lift),       // Bronze
			30.0f,                                       // Silver
			6.0f + 4.0f * Lift,                          // Gold
			Lift >= 2 ? 1.0f + 2.5f * (Lift - 1) : 0.0f, // Platinum
			Lift >= 4 ? 0.5f + 1.2f * (Lift - 3) : 0.0f, // Legendary
			Lift >= 7 ? 0.3f * (Lift - 6) : 0.0f,        // Celestial
		};
		const int32 Count = FMath::Clamp(1 + (Rank >= 2 ? 1 : 0) + FMath::RandRange(0, 1) + (Bonus >= 2 ? 1 : 0), 1, 4);
		TSet<FName> Taken;
		int32 Best = 0;
		for (int32 i = 0; i < Count; ++i)
		{
			float Total = 0.0f;
			for (int32 t = 1; t < 7; ++t) { Total += ByTier[t].Num() > 0 ? Weights[t] : 0.0f; }
			if (Total <= 0.0f)
			{
				break;
			}
			float Pick = FMath::FRandRange(0.0f, Total);
			int32 Tier = 1;
			for (int32 t = 1; t < 7; ++t)
			{
				if (ByTier[t].Num() == 0) { continue; }
				Pick -= Weights[t];
				if (Pick <= 0.0f) { Tier = t; break; }
			}
			// A different item each time, within the chest.
			TArray<FName>& Pool = ByTier[Tier];
			for (int32 Try = 0; Try < 6 && Pool.Num() > 0; ++Try)
			{
				const FName Item = Pool[FMath::RandRange(0, Pool.Num() - 1)];
				if (Taken.Contains(Item)) { continue; }
				Taken.Add(Item);
				FDCCLootDrop Drop;
				Drop.PickupClass = ADCCPickup_Equipment::StaticClass();
				Drop.Chance = 1.0f;
				Drop.EquipmentItemId = Item;
				Out.Add(Drop);
				Best = FMath::Max(Best, Tier);
				break;
			}
		}

		FDCCLootDrop Gold;
		Gold.PickupClass = ADCCPickup_Coins::StaticClass();
		Gold.Chance = 1.0f;
		Gold.MinGold = 8 + 8 * Rank + 10 * Bonus;
		Gold.MaxGold = 20 + 15 * Rank + 20 * Bonus;
		Out.Add(Gold);

		GLastBest = Best > 0 ? TierName(Best) : FString();
		FString Names;
		for (const FName& Item : Taken) { Names += Item.ToString() + TEXT(" "); }
		UE_LOG(LogTemp, Log, TEXT("[DCC Loot] chest rank %d bonus %d: %s(best %s)"), Rank, Bonus, *Names, *GLastBest);
		return Out;
	}
}
