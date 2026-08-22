#include "Items/DCCInventoryComponent.h"

#include "Items/DCCPickup.h"
#include "Engine/World.h"
#include "Net/UnrealNetwork.h"

UDCCInventoryComponent::UDCCInventoryComponent()
{
	PrimaryComponentTick.bCanEverTick = false;
	SetIsReplicatedByDefault(true);
}

void UDCCInventoryComponent::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);

	DOREPLIFETIME(UDCCInventoryComponent, Items);
	DOREPLIFETIME(UDCCInventoryComponent, StowedRemains);
}

float UDCCInventoryComponent::GetTotalMass() const
{
	float Total = 0.0f;
	for (const FDCCInventoryItem& Item : Items)
	{
		Total += Item.MassKilograms;
	}
	return Total;
}

bool UDCCInventoryComponent::StowPickup(ADCCPickup* Pickup)
{
	if (GetOwnerRole() != ROLE_Authority)
	{
		UE_LOG(LogTemp, Warning, TEXT("DCCInventoryComponent::StowPickup called on a client; ignoring."));
		return false;
	}

	if (!Pickup || IsFull())
	{
		return false;
	}

	FDCCInventoryItem Item;
	Item.PickupClass = Pickup->GetClass();
	Item.MassKilograms = Pickup->MassKilograms;
	Item.DisplayName = Pickup->DisplayName.IsEmpty()
		? FText::FromString(Pickup->GetClass()->GetName())
		: Pickup->DisplayName;

	Items.Add(Item);

	// Recorded before destroying, so a failure above cannot lose the item.
	Pickup->Destroy();

	OnInventoryChanged.Broadcast(this);

	return true;
}

ADCCPickup* UDCCInventoryComponent::DropItem(int32 Index, const FVector& WorldLocation)
{
	if (GetOwnerRole() != ROLE_Authority)
	{
		UE_LOG(LogTemp, Warning, TEXT("DCCInventoryComponent::DropItem called on a client; ignoring."));
		return nullptr;
	}

	if (!Items.IsValidIndex(Index) || !Items[Index].PickupClass)
	{
		return nullptr;
	}

	UWorld* World = GetWorld();
	if (!World)
	{
		return nullptr;
	}

	FActorSpawnParameters SpawnParams;
	SpawnParams.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AdjustIfPossibleButAlwaysSpawn;

	ADCCPickup* Spawned = World->SpawnActor<ADCCPickup>(
		Items[Index].PickupClass, WorldLocation, FRotator::ZeroRotator, SpawnParams);

	// Only forget the record once the item exists again in the world. Removing it
	// first would delete the item outright if the spawn failed.
	if (!Spawned)
	{
		return nullptr;
	}

	Items.RemoveAt(Index);
	OnInventoryChanged.Broadcast(this);

	return Spawned;
}

void UDCCInventoryComponent::OnRep_Items()
{
	OnInventoryChanged.Broadcast(this);
}

void UDCCInventoryComponent::StowRemains(const FText& SourceName, const TArray<FDCCRolledLoot>& Contents)
{
	if (GetOwnerRole() != ROLE_Authority)
	{
		UE_LOG(LogTemp, Warning, TEXT("DCCInventoryComponent::StowRemains called on a client; ignoring."));
		return;
	}

	// No capacity check, deliberately - a stowed corpse is not a physical prop
	// competing for a slot with the sword in your pocket. Refusing "Stow Body"
	// because your Items list happens to be full would be a rule nobody could
	// see coming.
	FDCCStowedRemains Remains;
	Remains.DisplayName = SourceName;
	Remains.Contents = Contents;

	StowedRemains.Add(MoveTemp(Remains));

	OnInventoryChanged.Broadcast(this);
}

bool UDCCInventoryComponent::LootOneFromStowed(int32 RemainsIndex, int32 ItemIndex, const FVector& WorldLocation)
{
	if (GetOwnerRole() != ROLE_Authority || !StowedRemains.IsValidIndex(RemainsIndex))
	{
		return false;
	}

	FDCCStowedRemains& Remains = StowedRemains[RemainsIndex];
	if (!Remains.Contents.IsValidIndex(ItemIndex))
	{
		return false;
	}

	UWorld* World = GetWorld();
	if (!DCCLoot::SpawnRolledLoot(World, Remains.Contents[ItemIndex], WorldLocation))
	{
		return false;
	}

	Remains.Contents.RemoveAt(ItemIndex);

	// An emptied record is clutter, not a memento - drop it the moment the last
	// item leaves, same as a corpse fading once it has nothing left to give.
	if (Remains.Contents.IsEmpty())
	{
		StowedRemains.RemoveAt(RemainsIndex);
	}

	OnInventoryChanged.Broadcast(this);
	return true;
}

bool UDCCInventoryComponent::LootAllFromStowed(int32 RemainsIndex, const FVector& WorldLocation)
{
	if (GetOwnerRole() != ROLE_Authority || !StowedRemains.IsValidIndex(RemainsIndex))
	{
		return false;
	}

	UWorld* World = GetWorld();
	for (const FDCCRolledLoot& Rolled : StowedRemains[RemainsIndex].Contents)
	{
		DCCLoot::SpawnRolledLoot(World, Rolled, WorldLocation);
	}

	StowedRemains.RemoveAt(RemainsIndex);

	OnInventoryChanged.Broadcast(this);
	return true;
}

void UDCCInventoryComponent::OnRep_StowedRemains()
{
	OnInventoryChanged.Broadcast(this);
}
