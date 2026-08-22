#include "Items/DCCLootContainer.h"

#include "Items/DCCInventoryComponent.h"
#include "Characters/DCCCharacterBase.h"
#include "Components/StaticMeshComponent.h"
#include "Net/UnrealNetwork.h"

ADCCLootContainer::ADCCLootContainer()
{
	PrimaryActorTick.bCanEverTick = false;
	bReplicates = true;

	Mesh = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("Mesh"));
	SetRootComponent(Mesh);

	// Static scenery, not a physics prop - a chest that can be shoved around or
	// knocked over by a thrown crate would be a worse chest, not a better one.
	Mesh->SetSimulatePhysics(false);
	Mesh->SetCollisionEnabled(ECollisionEnabled::QueryOnly);
	Mesh->SetCollisionResponseToAllChannels(ECR_Block);
}

void ADCCLootContainer::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);

	DOREPLIFETIME(ADCCLootContainer, bHasLoot);
	DOREPLIFETIME(ADCCLootContainer, PendingLoot);
}

void ADCCLootContainer::BeginPlay()
{
	Super::BeginPlay();

	if (HasAuthority())
	{
		RollLoot();
	}
}

void ADCCLootContainer::RollLoot()
{
	DCCLoot::RollLootTable(LootTable, PendingLoot);
	bHasLoot = !PendingLoot.IsEmpty();
}

FText ADCCLootContainer::GetLootSourceName() const
{
	return DisplayName.IsEmpty() ? FText::FromString(GetActorNameOrLabel()) : DisplayName;
}

bool ADCCLootContainer::HasAnyLoot() const
{
	return bHasLoot;
}

const TArray<FDCCRolledLoot>& ADCCLootContainer::GetPendingLootItems() const
{
	return PendingLoot;
}

FVector ADCCLootContainer::GetLootOrigin() const
{
	return GetActorLocation();
}

void ADCCLootContainer::LootOne(int32 Index, ADCCCharacterBase* Looter)
{
	if (!HasAuthority() || !PendingLoot.IsValidIndex(Index))
	{
		return;
	}

	DCCLoot::SpawnRolledLoot(GetWorld(), PendingLoot[Index], GetLootOrigin());
	PendingLoot.RemoveAt(Index);
	bHasLoot = !PendingLoot.IsEmpty();
}

void ADCCLootContainer::LootAll(ADCCCharacterBase* Looter)
{
	if (!HasAuthority() || !bHasLoot)
	{
		return;
	}

	// Cleared first, same race guard as a corpse: two players confirming on the
	// same frame must not both pass and spawn everything twice.
	bHasLoot = false;

	for (const FDCCRolledLoot& Rolled : PendingLoot)
	{
		DCCLoot::SpawnRolledLoot(GetWorld(), Rolled, GetLootOrigin());
	}

	PendingLoot.Reset();
}

void ADCCLootContainer::StowInto(ADCCCharacterBase* Looter)
{
	if (!HasAuthority())
	{
		return;
	}

	if (Looter && Looter->Inventory)
	{
		Looter->Inventory->StowRemains(GetLootSourceName(), PendingLoot);
	}

	// A chest has no fade-away timer the way a corpse does - it stays in the
	// world, just empty, which is the more legible outcome for a piece of
	// scenery nobody expects to disappear.
	PendingLoot.Reset();
	bHasLoot = false;
}

void ADCCLootContainer::OnRep_HasLoot()
{
	// Nothing to do - DCCHUD::DrawLootMenu reads bHasLoot live every frame, so
	// there is no cached UI state this needs to push into. The function exists
	// only because ReplicatedUsing requires one.
}
