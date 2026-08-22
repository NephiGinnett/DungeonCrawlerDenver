#include "Items/DCCPickupVariants.h"

#include "Components/StaticMeshComponent.h"
#include "Engine/StaticMesh.h"
#include "UObject/ConstructorHelpers.h"

namespace
{
	/**
	 * Shared setup. Mesh already exists as the root - ADCCPickup's constructor
	 * creates it - so this only ever fills in the authored half.
	 *
	 * Mass is authored rather than derived from the mesh for the reason given on
	 * ADCCPickup::MassKilograms: physical mass follows bounds and density, which
	 * would make the big light-looking crate outweigh the barrel and quietly
	 * wreck the Strength tiers.
	 */
	void ConfigurePickup(UStaticMeshComponent* Mesh, const TCHAR* MeshPath,
		float& OutMass, int32& OutDie, float Mass, int32 Die)
	{
		OutMass = Mass;
		OutDie = Die;

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
}

ADCCPickup_Coins::ADCCPickup_Coins()
{
	// Reusing the bottle mesh as a stand-in: no pack in the project ships a coin
	// or a pouch. Scaled down so it reads as "small thing on the floor" rather
	// than "someone left a drink here". Swap the mesh when loot art exists.
	ConfigurePickup(Mesh, TEXT("/Game/Cave_Camp/Meshes/SM_bottle.SM_bottle"),
		MassKilograms, ImpactDamageDie, 1.0f, 1);

	if (Mesh)
	{
		Mesh->SetRelativeScale3D(FVector(0.5f, 0.5f, 0.35f));
	}

	DisplayName = NSLOCTEXT("DCC", "PickupCoins", "Loose Gold");

	GoldValue = 5;
	bBanksOnPickup = true;
}

ADCCPickup_Bottle::ADCCPickup_Bottle()
{
	ConfigurePickup(Mesh, TEXT("/Game/Cave_Camp/Meshes/SM_bottle.SM_bottle"),
		MassKilograms, ImpactDamageDie, 3.0f, 4);

	DisplayName = NSLOCTEXT("DCC", "PickupBottle", "House Tequila, Unopened");
}

ADCCPickup_EggBox::ADCCPickup_EggBox()
{
	ConfigurePickup(Mesh, TEXT("/Game/Diner/Meshes/SM_egg_box.SM_egg_box"),
		MassKilograms, ImpactDamageDie, 8.0f, 4);

	DisplayName = NSLOCTEXT("DCC", "PickupEggBox", "Eggs, Allegedly Fresh");
}

ADCCPickup_Crate::ADCCPickup_Crate()
{
	ConfigurePickup(Mesh, TEXT("/Game/Cave_Camp/Meshes/Props_02/SM_crate_01.SM_crate_01"),
		MassKilograms, ImpactDamageDie, 40.0f, 6);

	DisplayName = NSLOCTEXT("DCC", "PickupCrate", "Crate of Unclaimed Property");
}

ADCCPickup_GreaseBarrel::ADCCPickup_GreaseBarrel()
{
	ConfigurePickup(Mesh, TEXT("/Game/Cave_Camp/Meshes/Props_02/SM_barrel_01.SM_barrel_01"),
		MassKilograms, ImpactDamageDie, 35.0f, 6);

	DisplayName = NSLOCTEXT("DCC", "PickupGreaseBarrel", "Barrel of Fryer Grease");
}

ADCCPickup_Keg::ADCCPickup_Keg()
{
	ConfigurePickup(Mesh, TEXT("/Game/Cave_Camp/Meshes/Props_02/SM_barrel_03.SM_barrel_03"),
		MassKilograms, ImpactDamageDie, 70.0f, 8);

	DisplayName = NSLOCTEXT("DCC", "PickupKeg", "Keg, Contents Disputed");
}

ADCCPickup_StrongboxCrate::ADCCPickup_StrongboxCrate()
{
	ConfigurePickup(Mesh, TEXT("/Game/Cave_Camp/Meshes/Props_02/SM_crate_02.SM_crate_02"),
		MassKilograms, ImpactDamageDie, 75.0f, 8);

	DisplayName = NSLOCTEXT("DCC", "PickupStrongbox", "The Manager's Strongbox");

	// The floor's payday, and deliberately NOT banked on pickup. It is only worth
	// anything if somebody carries it all the way out through the exit - which
	// means committing a crawler to holding it through the boss fight. That trade
	// is the entire point of the Pantry.
	GoldValue = 250;
	bBanksOnPickup = false;
}

ADCCPickup_Jukebox::ADCCPickup_Jukebox()
{
	// No jukebox mesh exists in any installed pack - a boxy kitchen appliance
	// stands in for now, which happens to fit "unclaimed property" perfectly.
	ConfigurePickup(Mesh, TEXT("/Game/Diner/Meshes/Blocking/SM_kitchen_prop_01.SM_kitchen_prop_01"),
		MassKilograms, ImpactDamageDie, 50.0f, 8);

	DisplayName = NSLOCTEXT("DCC", "PickupJukebox", "Jukebox, Unplugged");

	// Curiosity value rather than scrap value - somebody on the surface will
	// pay more for this than the crate it is sitting next to.
	GoldValue = 60;
}
