#include "Items/DCCInventoryComponent.h"

#include "Items/DCCPickup.h"
#include "Characters/DCCCharacterBase.h"
#include "Equipment/DCCEquipmentComponent.h"
#include "Engine/World.h"
#include "Engine/DataTable.h"
#include "Net/UnrealNetwork.h"
#include "UObject/ConstructorHelpers.h"

UDCCInventoryComponent::UDCCInventoryComponent()
{
	PrimaryComponentTick.bCanEverTick = false;
	SetIsReplicatedByDefault(true);

	static ConstructorHelpers::FObjectFinder<UDataTable> DefaultConsumableTable(
		TEXT("/Game/DCC/Items/DT_DCCConsumables.DT_DCCConsumables"));
	if (DefaultConsumableTable.Succeeded())
	{
		ConsumableTable = DefaultConsumableTable.Object;
	}

	static ConstructorHelpers::FObjectFinder<UDataTable> DefaultLootBoxTable(
		TEXT("/Game/DCC/Items/DT_DCCLootBoxTables.DT_DCCLootBoxTables"));
	if (DefaultLootBoxTable.Succeeded())
	{
		LootBoxTable = DefaultLootBoxTable.Object;
	}

	static ConstructorHelpers::FObjectFinder<UDataTable> DefaultBoonTable(
		TEXT("/Game/DCC/Items/DT_DCCBoons.DT_DCCBoons"));
	if (DefaultBoonTable.Succeeded())
	{
		BoonTable = DefaultBoonTable.Object;
	}

	LootBoxCounts.SetNumZeroed((int32)EDCCLootBoxTier::MAX);
}

void UDCCInventoryComponent::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);

	DOREPLIFETIME(UDCCInventoryComponent, Items);
	DOREPLIFETIME(UDCCInventoryComponent, StowedRemains);
	DOREPLIFETIME(UDCCInventoryComponent, StowedConsumables);
	DOREPLIFETIME(UDCCInventoryComponent, LootBoxCounts);
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

	if (!Pickup)
	{
		return false;
	}

	const FText DisplayName = Pickup->DisplayName.IsEmpty()
		? FText::FromString(Pickup->GetClass()->GetName())
		: Pickup->DisplayName;

	// Recorded before destroying, so a failed (full) record cannot lose the
	// item - AddItemRecord already refuses without touching Items in that case.
	if (!AddItemRecord(Pickup->GetClass(), DisplayName, Pickup->MassKilograms, Pickup->EquipmentItemId))
	{
		return false;
	}

	Pickup->Destroy();

	return true;
}

bool UDCCInventoryComponent::AddItemRecord(TSubclassOf<ADCCPickup> PickupClass, const FText& DisplayName, float MassKilograms, FName EquipmentItemId)
{
	if (GetOwnerRole() != ROLE_Authority)
	{
		UE_LOG(LogTemp, Warning, TEXT("DCCInventoryComponent::AddItemRecord called on a client; ignoring."));
		return false;
	}

	if (!PickupClass || IsFull())
	{
		return false;
	}

	FDCCInventoryItem Item;
	Item.PickupClass = PickupClass;
	Item.DisplayName = DisplayName;
	Item.MassKilograms = MassKilograms;
	Item.EquipmentItemId = EquipmentItemId;

	Items.Add(Item);
	OnInventoryChanged.Broadcast(this);

	return true;
}

bool UDCCInventoryComponent::EquipItem(int32 Index)
{
	if (GetOwnerRole() != ROLE_Authority || !Items.IsValidIndex(Index))
	{
		return false;
	}

	const FDCCInventoryItem Item = Items[Index];
	if (Item.EquipmentItemId.IsNone())
	{
		return false;
	}

	ADCCCharacterBase* CharacterOwner = Cast<ADCCCharacterBase>(GetOwner());

	// Equip refuses (returns false) when a cursed item already holds the slot -
	// the record is left in place rather than lost, same "nothing is ever lost
	// to a failed equip" rule DCCLoot::SpawnRolledLoot follows.
	if (!CharacterOwner || !CharacterOwner->Equipment || !CharacterOwner->Equipment->Equip(Item.EquipmentItemId))
	{
		return false;
	}

	Items.RemoveAt(Index);
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

	const FDCCInventoryItem Item = Items[Index];
	const FTransform SpawnTransform(FRotator::ZeroRotator, WorldLocation);

	ADCCPickup* Spawned = nullptr;
	if (Item.EquipmentItemId.IsNone())
	{
		FActorSpawnParameters SpawnParams;
		SpawnParams.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AdjustIfPossibleButAlwaysSpawn;

		Spawned = World->SpawnActor<ADCCPickup>(Item.PickupClass, WorldLocation, FRotator::ZeroRotator, SpawnParams);
	}
	else
	{
		// Deferred so EquipmentItemId is set before BeginPlay runs -
		// ADCCPickup_Equipment resolves its displayed mesh from it at BeginPlay,
		// which a non-deferred spawn would already have missed.
		if (ADCCPickup* Deferred = World->SpawnActorDeferred<ADCCPickup>(
			Item.PickupClass, SpawnTransform, nullptr, nullptr,
			ESpawnActorCollisionHandlingMethod::AdjustIfPossibleButAlwaysSpawn))
		{
			Deferred->EquipmentItemId = Item.EquipmentItemId;
			Deferred->FinishSpawning(SpawnTransform);
			Spawned = Deferred;
		}
	}

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

	const FDCCRolledLoot& Rolled = Remains.Contents[ItemIndex];
	UWorld* World = GetWorld();
	if (!World || !Rolled.PickupClass)
	{
		return false;
	}

	// A nullptr return from here no longer means failure by itself - since a
	// looter is known on this path, a pure-gold entry is credited directly and
	// intentionally spawns nothing (see SpawnRolledLoot's header comment). The
	// two real failure modes (no world, no PickupClass) are already checked above.
	DCCLoot::SpawnRolledLoot(World, Rolled, WorldLocation, Cast<ADCCCharacterBase>(GetOwner()));

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
	ADCCCharacterBase* Looter = Cast<ADCCCharacterBase>(GetOwner());
	for (const FDCCRolledLoot& Rolled : StowedRemains[RemainsIndex].Contents)
	{
		DCCLoot::SpawnRolledLoot(World, Rolled, WorldLocation, Looter);
	}

	StowedRemains.RemoveAt(RemainsIndex);

	OnInventoryChanged.Broadcast(this);
	return true;
}

void UDCCInventoryComponent::OnRep_StowedRemains()
{
	OnInventoryChanged.Broadcast(this);
}

bool UDCCInventoryComponent::FindConsumable(FName ItemId, FDCCConsumableDef& OutItem) const
{
	if (!ConsumableTable || ItemId.IsNone())
	{
		return false;
	}

	static const FString Context(TEXT("UDCCInventoryComponent::FindConsumable"));
	if (const FDCCConsumableDef* Row = ConsumableTable->FindRow<FDCCConsumableDef>(ItemId, Context))
	{
		OutItem = *Row;
		return true;
	}

	return false;
}

void UDCCInventoryComponent::AddConsumable(FName ItemId, int32 Quantity)
{
	if (GetOwnerRole() != ROLE_Authority || ItemId.IsNone() || Quantity <= 0)
	{
		return;
	}

	for (FDCCStowedConsumable& Existing : StowedConsumables)
	{
		if (Existing.ItemId == ItemId)
		{
			Existing.Quantity += Quantity;
			OnInventoryChanged.Broadcast(this);
			return;
		}
	}

	FDCCConsumableDef Def;
	FDCCStowedConsumable& NewEntry = StowedConsumables.AddDefaulted_GetRef();
	NewEntry.ItemId = ItemId;
	NewEntry.Quantity = Quantity;
	NewEntry.DisplayName = FindConsumable(ItemId, Def) ? Def.DisplayName : FText::FromName(ItemId);

	OnInventoryChanged.Broadcast(this);
}

bool UDCCInventoryComponent::ConsumeStowedConsumable(int32 Index, FDCCConsumableDef& OutDef)
{
	if (GetOwnerRole() != ROLE_Authority || !StowedConsumables.IsValidIndex(Index))
	{
		return false;
	}

	if (!FindConsumable(StowedConsumables[Index].ItemId, OutDef))
	{
		return false;
	}

	FDCCStowedConsumable& Entry = StowedConsumables[Index];
	--Entry.Quantity;

	// Empties the same way a corpse record does once its last item leaves -
	// no zero-quantity rows lingering in the list for the HUD to skip over.
	if (Entry.Quantity <= 0)
	{
		StowedConsumables.RemoveAt(Index);
	}

	OnInventoryChanged.Broadcast(this);
	return true;
}

void UDCCInventoryComponent::OnRep_StowedConsumables()
{
	OnInventoryChanged.Broadcast(this);
}

int32 UDCCInventoryComponent::GetLootBoxCount(EDCCLootBoxTier Tier) const
{
	const int32 Index = (int32)Tier;
	return LootBoxCounts.IsValidIndex(Index) ? LootBoxCounts[Index] : 0;
}

void UDCCInventoryComponent::GrantLootBox(EDCCLootBoxTier Tier, int32 Count)
{
	if (GetOwnerRole() != ROLE_Authority || Tier == EDCCLootBoxTier::None || Count <= 0)
	{
		return;
	}

	// Guards against LootBoxCounts arriving at a stale size - it is only ever
	// sized in the constructor, but a saved/replicated array from before a
	// tier was added would otherwise read as "index out of range" forever.
	const int32 Index = (int32)Tier;
	if (!LootBoxCounts.IsValidIndex(Index))
	{
		LootBoxCounts.SetNumZeroed((int32)EDCCLootBoxTier::MAX);
	}

	LootBoxCounts[Index] += Count;
	OnInventoryChanged.Broadcast(this);
}

bool UDCCInventoryComponent::TryOpenLootBox(EDCCLootBoxTier Tier, const FVector& WorldLocation, FDCCOpenedLootBoxResult& OutResult)
{
	OutResult = FDCCOpenedLootBoxResult();

	if (GetOwnerRole() != ROLE_Authority || Tier == EDCCLootBoxTier::None || !LootBoxTable)
	{
		return false;
	}

	const int32 Index = (int32)Tier;
	if (!LootBoxCounts.IsValidIndex(Index) || LootBoxCounts[Index] <= 0)
	{
		return false;
	}

	static const FString TableContext(TEXT("UDCCInventoryComponent::TryOpenLootBox"));
	const UEnum* TierEnum = StaticEnum<EDCCLootBoxTier>();
	const FName RowName(*TierEnum->GetNameStringByValue((int64)Tier));
	const FDCCLootBoxTableRow* Row = LootBoxTable->FindRow<FDCCLootBoxTableRow>(RowName, TableContext);
	if (!Row)
	{
		return false;
	}

	// Spent before rolling, same reason LootAll clears bHasLoot first: nothing
	// here can be re-entered mid-roll, but paying up front is the cheap way to
	// make that true rather than relying on it.
	--LootBoxCounts[Index];

	DCCLoot::RollLootTable(Row->ItemEntries, OutResult.Items);
	SpawnLootBoxItems(OutResult.Items, WorldLocation);

	if (BoonTable)
	{
		static const FString BoonContext(TEXT("UDCCInventoryComponent::TryOpenLootBox::Boon"));
		for (const FDCCBoonEntry& Entry : Row->BoonEntries)
		{
			if (FMath::FRand() > Entry.Chance)
			{
				continue;
			}

			if (const FDCCBoonDef* BoonRow = BoonTable->FindRow<FDCCBoonDef>(Entry.BoonId, BoonContext))
			{
				OutResult.Boons.Add(*BoonRow);
			}
		}
	}

	OnInventoryChanged.Broadcast(this);
	return true;
}

void UDCCInventoryComponent::SpawnLootBoxItems(const TArray<FDCCRolledLoot>& RolledItems, const FVector& WorldLocation) const
{
	UWorld* World = GetWorld();
	if (!World)
	{
		return;
	}

	for (const FDCCRolledLoot& Rolled : RolledItems)
	{
		DCCLoot::SpawnRolledLoot(World, Rolled, WorldLocation);
	}
}

void UDCCInventoryComponent::OnRep_LootBoxCounts()
{
	OnInventoryChanged.Broadcast(this);
}
