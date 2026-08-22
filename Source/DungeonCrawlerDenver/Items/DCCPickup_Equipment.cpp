#include "Items/DCCPickup_Equipment.h"

#include "Equipment/DCCEquipmentTypes.h"
#include "Components/StaticMeshComponent.h"
#include "Engine/DataTable.h"
#include "Engine/StaticMesh.h"
#include "UObject/ConstructorHelpers.h"

ADCCPickup_Equipment::ADCCPickup_Equipment()
{
	// Light enough that reaching a piece of gear is never gated by Strength on
	// top of whatever else guarded the room it was in - the mass tiers in
	// DCCPickupVariants.h are about the Pantry's Strength lesson specifically,
	// which this is not.
	MassKilograms = 5.0f;
	ImpactDamageDie = 4;

	static ConstructorHelpers::FObjectFinder<UDataTable> Table(
		TEXT("/Game/DCC/Equipment/DT_DCCEquipment.DT_DCCEquipment"));
	if (Table.Succeeded())
	{
		EquipmentTable = Table.Object;
	}
}

void ADCCPickup_Equipment::BeginPlay()
{
	Super::BeginPlay();

	if (EquipmentItemId.IsNone() || !EquipmentTable)
	{
		return;
	}

	static const FString Context(TEXT("ADCCPickup_Equipment::BeginPlay"));
	const FDCCEquipmentDef* Row = EquipmentTable->FindRow<FDCCEquipmentDef>(EquipmentItemId, Context);
	if (!Row)
	{
		UE_LOG(LogTemp, Warning, TEXT("ADCCPickup_Equipment: '%s' is not a row in DT_DCCEquipment."),
			*EquipmentItemId.ToString());
		return;
	}

	DisplayName = Row->DisplayName;

	// Soft on the DataTable row so a floor's worth of equipment definitions
	// does not drag every mesh into memory at load - resolved here, the
	// moment one is actually placed and needs to be seen.
	if (UStaticMesh* ResolvedMesh = Row->Mesh.LoadSynchronous())
	{
		if (Mesh)
		{
			Mesh->SetStaticMesh(ResolvedMesh);
		}
	}
}
