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

	DCCLoot::SpawnRolledLoot(GetWorld(), PendingLoot[Index], GetLootOrigin(), Looter);
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
		DCCLoot::SpawnRolledLoot(GetWorld(), Rolled, GetLootOrigin(), Looter);
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

	PendingLoot.Reset();
	bHasLoot = false;

	// Reversed 2026-08-30, per Zoe: an emptied chest left standing in the
	// world read as Stow doing nothing at all - there was no visible change
	// between "stowed" and "never touched". Removed outright rather than
	// given a corpse-style fade timer: a chest is scenery, not a body with a
	// "someone's about to collect this" beat to play out, so there is no
	// reason to delay it once its contents have already left for the
	// looter's Stowed Remains record.
	Destroy();
}

void ADCCLootContainer::DepositLoot(const TArray<FDCCRolledLoot>& Items)
{
	if (!HasAuthority() || Items.IsEmpty())
	{
		return;
	}

	PendingLoot.Append(Items);
	bHasLoot = true;
}

void ADCCLootContainer::OnRep_HasLoot()
{
	// Nothing to do - DCCHUD::DrawLootMenu reads bHasLoot live every frame, so
	// there is no cached UI state this needs to push into. The function exists
	// only because ReplicatedUsing requires one.
}
