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
