#include "Items/DCCLootTypes.h"

#include "Items/DCCPickup.h"
#include "Characters/DCCCharacterBase.h"
#include "Stats/DCCStatsComponent.h"
#include "Equipment/DCCEquipmentComponent.h"
#include "Items/DCCInventoryComponent.h"
#include "Combat/DCCGameState.h"
#include "UI/DCCEventLogTypes.h"
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

	ADCCPickup* SpawnRolledLoot(UWorld* World, const FDCCRolledLoot& Rolled, const FVector& Origin, ADCCCharacterBase* Looter)
	{
		if (!World || !Rolled.PickupClass)
		{
			return nullptr;
		}

		// A known looter and ordinary pocket change: credit it straight to their
		// purse rather than leaving a coin on the ground that has to be separately
		// walked over. See this function's header comment for why this does not
		// apply to an unclaimed floor drop (Looter is null there) or to a class
		// that deliberately must be carried (bBanksOnPickup false).
		bool bGoldCredited = false;
		if (Looter && Looter->Stats && Rolled.Gold > 0)
		{
			const ADCCPickup* DefaultPickup = Rolled.PickupClass.GetDefaultObject();
			if (DefaultPickup && DefaultPickup->bBanksOnPickup)
			{
				Looter->Stats->AddGold(Rolled.Gold);
				bGoldCredited = true;

				ADCCGameState::Log(Looter, EDCCLogCategory::Experience, FText::Format(
					NSLOCTEXT("DCC", "LogLootGoldClaimed", "{0} pockets {1} gold."),
					FText::FromString(GetNameSafe(Looter)), FText::AsNumber(Rolled.Gold)));
			}
		}

		// A pure coin whose gold was just paid out above has nothing left worth
		// representing on the floor.
		if (bGoldCredited && Rolled.EquipmentItemId.IsNone())
		{
			return nullptr;
		}

		// A known looter and a piece of gear: record it straight into their pack
		// rather than spawning it on the ground - same reasoning as the gold
		// credit above, just for gear instead of coin. Without this, "Check
		// Inventory" on a corpse/chest and emptying a Stowed Remains record
		// both used to drop the item on the ground at the looter's own feet
		// and rely on them noticing it and walking over it a second time -
		// easy to miss among other clutter, and reads as "picked it up but it
		// went nowhere." Gear does NOT auto-equip here (or anywhere else as of
		// 2026-08-30) - it lands in Items like anything else, and the
		// Inventory page's EquipItem is what actually equips it, on request.
		// DisplayName has to come from the equipment row itself, not the
		// pickup class's CDO - ADCCPickup_Equipment only resolves its own
		// DisplayName in BeginPlay, which a record with no live actor never
		// runs. Falls through to the ground-pickup path below only if the
		// pack is full or the row cannot be found, so nothing is ever lost.
		if (Looter && Looter->Inventory && Looter->Equipment && !Rolled.EquipmentItemId.IsNone())
		{
			FDCCEquipmentDef Def;
			if (Looter->Equipment->FindItem(Rolled.EquipmentItemId, Def))
			{
				const ADCCPickup* DefaultPickup = Rolled.PickupClass.GetDefaultObject();
				const float Mass = DefaultPickup ? DefaultPickup->MassKilograms : 0.0f;

				if (Looter->Inventory->AddItemRecord(Rolled.PickupClass, Def.DisplayName, Mass, Rolled.EquipmentItemId))
				{
					return nullptr;
				}
			}
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

		if (Rolled.Gold > 0 && !bGoldCredited)
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
