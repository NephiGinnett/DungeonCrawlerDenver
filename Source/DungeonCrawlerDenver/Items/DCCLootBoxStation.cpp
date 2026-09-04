#include "Items/DCCLootBoxStation.h"

#include "Characters/DCCCharacterBase.h"
#include "Items/DCCInventoryComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Components/PointLightComponent.h"
#include "UObject/ConstructorHelpers.h"

ADCCLootBoxStation::ADCCLootBoxStation()
{
	Mesh = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("Mesh"));
	SetRootComponent(Mesh);

	// A crate placeholder until Zoe's Battle Royale Props import lands - see
	// this class's own header comment on why the mesh is swappable per
	// instance rather than fixed here.
	static ConstructorHelpers::FObjectFinder<UStaticMesh> PlaceholderMesh(
		TEXT("/Game/Ice_Station/Meshes/Crates/SM_large_crate_01.SM_large_crate_01"));
	if (PlaceholderMesh.Succeeded())
	{
		Mesh->SetStaticMesh(PlaceholderMesh.Object);
	}

	// Default is Bronze-coloured; every other tier's station overrides this
	// per instance once placed, same as the mesh.
	Glow = CreateDefaultSubobject<UPointLightComponent>(TEXT("Glow"));
	Glow->SetupAttachment(Mesh);
	Glow->SetRelativeLocation(FVector(0.0f, 0.0f, 30.0f));
	Glow->IntensityUnits = ELightUnits::Candelas;
	Glow->SetIntensity(4000.0f);
	Glow->SetLightColor(FLinearColor(0.8f, 0.45f, 0.2f));
	Glow->SetAttenuationRadius(350.0f);
	Glow->SetCastShadows(false);
}

void ADCCLootBoxStation::TryPlayerInteract(ADCCCharacterBase* Interactor)
{
	if (!Interactor || !Interactor->Inventory)
	{
		return;
	}

	// The claim-everything terminal - see the header comment on Tier. Claims
	// every pending achievement and opens every owned box across every tier
	// in one press; ClaimAndOpenAllLootBoxes does the actual Safe Room
	// re-check and feedback, so there is nothing further to gate here.
	if (Tier == EDCCLootBoxTier::None)
	{
		Interactor->ClaimAndOpenAllLootBoxes();
		return;
	}

	if (Interactor->Inventory->GetLootBoxCount(Tier) <= 0)
	{
		Interactor->ShowLocalNotice(FText::Format(
			NSLOCTEXT("DCC", "LootBoxStationEmpty", "No {0} Boxes to open."),
			UEnum::GetDisplayValueAsText(Tier)));
		return;
	}

	// The actual roll, spawn, and Safe Room re-check all happen server-side
	// inside this call - see ADCCCharacterBase::Server_OpenLootBox_Implementation.
	// This station is a place to stand and a prompt, nothing more.
	Interactor->OpenLootBox(Tier);
}
