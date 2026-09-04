#include "Items/DCCLootContainerVariants.h"

#include "Items/DCCPickupVariants.h"
#include "Items/DCCPickup_Equipment.h"
#include "Components/StaticMeshComponent.h"
#include "Engine/StaticMesh.h"
#include "UObject/ConstructorHelpers.h"

namespace
{
	void SetChestMesh(UStaticMeshComponent* Mesh, const TCHAR* MeshPath)
	{
		if (!Mesh)
		{
			return;
		}

		ConstructorHelpers::FObjectFinder<UStaticMesh> MeshAsset(MeshPath);
		if (MeshAsset.Succeeded())
		{
			Mesh->SetStaticMesh(MeshAsset.Object);
		}
	}

	/** One guaranteed-drop row. Chance 1.0 makes "curated contents" rather than "random chest". */
	FDCCLootDrop MakeGearDrop(FName ItemId)
	{
		FDCCLootDrop Drop;
		Drop.PickupClass = ADCCPickup_Equipment::StaticClass();
		Drop.Chance = 1.0f;
		Drop.EquipmentItemId = ItemId;
		return Drop;
	}

	/** A guaranteed gold entry, using the same coin pickup corpses drop. */
	FDCCLootDrop MakeGoldDrop(int32 MinGold, int32 MaxGold)
	{
		FDCCLootDrop Drop;
		Drop.PickupClass = ADCCPickup_Coins::StaticClass();
		Drop.Chance = 1.0f;
		Drop.MinGold = MinGold;
		Drop.MaxGold = MaxGold;
		return Drop;
	}
}

ADCCLootContainer_KitchenDrawer::ADCCLootContainer_KitchenDrawer()
{
	// The same wooden crate the Pantry's mid-tier props use - "kitchen storage"
	// is exactly what it already looks like.
	SetChestMesh(Mesh, TEXT("/Game/Cave_Camp/Meshes/Props_02/SM_crate_01.SM_crate_01"));

	DisplayName = NSLOCTEXT("DCC", "ChestKitchen", "Kitchen Drawer, Jammed Shut");

	LootTable.Add(MakeGearDrop(TEXT("TenderiserMallet")));
	LootTable.Add(MakeGearDrop(TEXT("ChefsKnife")));
	LootTable.Add(MakeGearDrop(TEXT("CastIronSkillet")));
	// The Boilerplate Rifle, added 2026-08-29 - Zoe's explicit ask was to confirm
	// a copy sits in "the loot chest under the kitchen"; a prior session may have
	// intended to place it here but no C++ or documented live-table evidence of
	// that was found. Added as a fourth guaranteed drop (constructor default, so
	// it survives regardless of what any one placed instance's live state was)
	// rather than trusting an unverifiable claim. Off-theme for a drawer of
	// kitchen tools on purpose - the joke is that it's there at all.
	LootTable.Add(MakeGearDrop(TEXT("BoilerplateRifle")));
	LootTable.Add(MakeGoldDrop(10, 25));
}

ADCCLootContainer_Armory::ADCCLootContainer_Armory()
{
	// The strongbox mesh, repurposed - a locked case reads as an armory rack
	// just as well as it reads as a strongbox.
	SetChestMesh(Mesh, TEXT("/Game/Cave_Camp/Meshes/Props_02/SM_crate_02.SM_crate_02"));

	DisplayName = NSLOCTEXT("DCC", "ChestArmory", "Emergency Equipment Locker");

	LootTable.Add(MakeGearDrop(TEXT("FireAxe")));
	LootTable.Add(MakeGearDrop(TEXT("WallSword")));
	LootTable.Add(MakeGearDrop(TEXT("FeatureWallShield")));
	LootTable.Add(MakeGoldDrop(20, 40));
}

ADCCLootContainer_Quartermaster::ADCCLootContainer_Quartermaster()
{
	SetChestMesh(Mesh, TEXT("/Game/Cave_Camp/Meshes/Props_02/SM_barrel_01.SM_barrel_01"));

	DisplayName = NSLOCTEXT("DCC", "ChestQuartermaster", "Quartermaster's Supply Crate");

	LootTable.Add(MakeGearDrop(TEXT("ConfettiCrossbow")));
	LootTable.Add(MakeGearDrop(TEXT("NailGunPro")));
	LootTable.Add(MakeGearDrop(TEXT("FlareGunEmergency")));
	LootTable.Add(MakeGoldDrop(15, 30));
}

ADCCLootContainer_ManagementLockbox::ADCCLootContainer_ManagementLockbox()
{
	SetChestMesh(Mesh, TEXT("/Game/Cave_Camp/Meshes/Props_02/SM_barrel_03.SM_barrel_03"));

	DisplayName = NSLOCTEXT("DCC", "ChestManagement", "Assistant Manager's Lockbox");

	LootTable.Add(MakeGearDrop(TEXT("PatioSunHat")));
	LootTable.Add(MakeGearDrop(TEXT("ManagersBlazer")));
	LootTable.Add(MakeGearDrop(TEXT("NameTagDave")));
	LootTable.Add(MakeGoldDrop(60, 120));
}

ADCCLootContainer_LostAndFound::ADCCLootContainer_LostAndFound()
{
	// Reusing the crate mesh rather than guessing at an unverified one - CLAUDE.md
	// flags more than one prior session losing real time to Fab packs that turned
	// out to be station-scale rather than human-scale, and this is a constructor
	// default that every future placement of this class inherits, not a one-off
	// a scale mistake here is cheap to fix in one place.
	SetChestMesh(Mesh, TEXT("/Game/Cave_Camp/Meshes/Props_02/SM_crate_01.SM_crate_01"));

	DisplayName = NSLOCTEXT("DCC", "ChestLostAndFound", "Lost and Found, Never Claimed");

	LootTable.Add(MakeGearDrop(TEXT("SeveranceCheckUncashed")));
	LootTable.Add(MakeGearDrop(TEXT("NameTagDave")));
	LootTable.Add(MakeGearDrop(TEXT("PatioSunHat")));
	LootTable.Add(MakeGoldDrop(5, 15));
}

ADCCLootContainer_SafetyLocker::ADCCLootContainer_SafetyLocker()
{
	SetChestMesh(Mesh, TEXT("/Game/Cave_Camp/Meshes/Props_02/SM_barrel_01.SM_barrel_01"));

	DisplayName = NSLOCTEXT("DCC", "ChestSafetyLocker", "Safety Locker, Inspection Overdue");

	LootTable.Add(MakeGearDrop(TEXT("HighVisibilitySafetyVest")));
	LootTable.Add(MakeGearDrop(TEXT("ComplaintFolderOverstuffed")));
	LootTable.Add(MakeGearDrop(TEXT("GreaseApron")));
	LootTable.Add(MakeGoldDrop(10, 25));
}
