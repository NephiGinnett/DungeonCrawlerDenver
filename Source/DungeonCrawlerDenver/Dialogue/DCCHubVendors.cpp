#include "Dialogue/DCCHubVendors.h"

#include "Animation/AnimationAsset.h"
#include "Components/SkeletalMeshComponent.h"
#include "Engine/SkeletalMesh.h"
#include "UObject/ConstructorHelpers.h"

namespace
{
	void Dress(ADCCVendorNPC& Vendor, USkeletalMesh* Body, UAnimationAsset* Idle)
	{
		if (Body && Vendor.Mesh)
		{
			Vendor.Mesh->SetSkeletalMeshAsset(Body);
		}
		if (Idle)
		{
			Vendor.IdleAnimation = Idle;
		}
	}
}

ADCCVendor_DutyFree::ADCCVendor_DutyFree()
{
	DisplayName = NSLOCTEXT("DCC", "VendorDutyFree", "Duty Free (Duty Mandatory)");
	static ConstructorHelpers::FObjectFinder<USkeletalMesh> Body(TEXT("/Game/Characters/Mannequins/Meshes/SKM_Quinn_Simple.SKM_Quinn_Simple"));
	static ConstructorHelpers::FObjectFinder<UAnimationAsset> Idle(TEXT("/Game/Characters/Mannequins/Anims/Unarmed/MM_Idle.MM_Idle"));
	Dress(*this, Body.Object, Idle.Object);

	SellableEquipmentIds = {
		// One for every Attack Skill in the books, cheapest first.
		TEXT("TableLegLiberated"), TEXT("RockiesGiveawayBat"), TEXT("CampingSectionHatchet"),
		TEXT("ConfiscatedSwitchblade"), TEXT("BootKnifeRegrettable"), TEXT("RenFaireLongsword"),
		TEXT("ContractorSledgehammer"), TEXT("IronicallyBeardedAxe"),
		// Existing gear, now tied to its skill.
		TEXT("ChefsKnife"), TEXT("FireAxe"), TEXT("CastIronSkillet"), TEXT("WallSword"), TEXT("SpearOfAnnualReview"),
		// Off hand and armour.
		TEXT("ServiceTray"), TEXT("GiftShopVikingShield"), TEXT("DecorativeHardHat"), TEXT("HistoricallyInaccurateHelm"),
		TEXT("HighVisibilitySafetyVest"), TEXT("BrandedFleeceVest"), TEXT("ManagersBlazer"),
	};
	SellableConsumableIds = { TEXT("TapWater"), TEXT("HouseAle") };
}

ADCCVendor_Pyrotechnics::ADCCVendor_Pyrotechnics()
{
	DisplayName = NSLOCTEXT("DCC", "VendorPyro", "Pharmacy & Pyrotechnics");
	static ConstructorHelpers::FObjectFinder<USkeletalMesh> Body(TEXT("/Game/Characters/Mannequins/Meshes/SKM_Manny_Simple.SKM_Manny_Simple"));
	static ConstructorHelpers::FObjectFinder<UAnimationAsset> Idle(TEXT("/Game/Characters/Mannequins/Anims/Unarmed/MM_Idle.MM_Idle"));
	Dress(*this, Body.Object, Idle.Object);

	SellableEquipmentIds = {
		// Throwing.
		TEXT("ThrownBottle"), TEXT("ServiceKnifeSet"),
		// Explosives Handling.
		TEXT("DynamiteBundledSingle"), TEXT("PipeBombHardwareSpecial"), TEXT("FireworksSurplus"), TEXT("DemolitionChargeUnion"),
		// Incendiary Device Handling.
		TEXT("MolotovWellSpirit"), TEXT("NapalmPotCeramic"),
		// Goblin Explosives. Cheaper, louder, and they know it.
		TEXT("GoblinBombProbablyStable"), TEXT("GoblinGrenadeExtraSpicy"),
	};
	SellableConsumableIds = {
		// First Aid.
		TEXT("BandageRollMostlySterile"), TEXT("FirstAidKitCompliant"), TEXT("TraumaKitParamedic"),
		TEXT("EnergyTonic"), TEXT("WellWhiskey"),
	};
}
