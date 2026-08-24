#include "Equipment/DCCEquipmentComponent.h"

#include "Characters/DCCCharacterBase.h"
#include "Items/DCCPickup.h"
#include "Stats/DCCStatsComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Components/SkeletalMeshComponent.h"
#include "Components/PointLightComponent.h"
#include "Engine/DataTable.h"
#include "Engine/StaticMesh.h"
#include "Engine/World.h"
#include "Net/UnrealNetwork.h"
#include "UObject/ConstructorHelpers.h"

UDCCEquipmentComponent::UDCCEquipmentComponent()
{
	PrimaryComponentTick.bCanEverTick = false;
	SetIsReplicatedByDefault(true);

	// Defaulted for the same reason as the other tables: an unassigned table
	// fails silently, and silence is the worst failure mode this project has.
	static ConstructorHelpers::FObjectFinder<UDataTable> DefaultTable(
		TEXT("/Game/DCC/Equipment/DT_DCCEquipment.DT_DCCEquipment"));
	if (DefaultTable.Succeeded())
	{
		EquipmentTable = DefaultTable.Object;
	}
}

void UDCCEquipmentComponent::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);

	DOREPLIFETIME(UDCCEquipmentComponent, Equipped);
}

void UDCCEquipmentComponent::BeginPlay()
{
	Super::BeginPlay();

	// Anything authored onto the actor before play starts still needs its meshes
	// built and its stats folded in.
	RebuildVisuals();

	if (GetOwner() && GetOwner()->HasAuthority())
	{
		// True here only: InitializeComponent already refilled Current to Max
		// against the UNENHANCED scores (equipment cannot be read that early),
		// so a character with stat-boosting starting gear would otherwise spawn
		// permanently pinned below the true, gear-inclusive max the moment this
		// recalculates it - clamped down next time, never refilled up. Every
		// other caller of NotifyStatsDirty leaves this false.
		NotifyStatsDirty(/*bRefillToFull=*/true);
	}
}

UDCCEquipmentComponent* UDCCEquipmentComponent::Find(AActor* Actor)
{
	return Actor ? Actor->FindComponentByClass<UDCCEquipmentComponent>() : nullptr;
}

ADCCCharacterBase* UDCCEquipmentComponent::GetOwningCharacter() const
{
	return Cast<ADCCCharacterBase>(GetOwner());
}

bool UDCCEquipmentComponent::FindItem(FName ItemId, FDCCEquipmentDef& OutItem) const
{
	if (!EquipmentTable || ItemId.IsNone())
	{
		return false;
	}

	static const FString Context(TEXT("DCCEquipment"));
	if (const FDCCEquipmentDef* Row = EquipmentTable->FindRow<FDCCEquipmentDef>(ItemId, Context, false))
	{
		OutItem = *Row;
		return true;
	}

	return false;
}

FName UDCCEquipmentComponent::GetEquippedId(EDCCEquipmentSlot Slot) const
{
	for (const FDCCEquippedItem& Item : Equipped)
	{
		if (Item.Slot == Slot)
		{
			return Item.ItemId;
		}
	}

	return NAME_None;
}

bool UDCCEquipmentComponent::Equip(FName ItemId)
{
	AActor* Owner = GetOwner();
	if (!Owner || !Owner->HasAuthority())
	{
		return false;
	}

	FDCCEquipmentDef Item;
	if (!FindItem(ItemId, Item))
	{
		return false;
	}

	// Whatever the replaced item's container was holding must not simply
	// disappear with it - spilled into the world before the entry is removed.
	for (const FDCCEquippedItem& Existing : Equipped)
	{
		if (Existing.Slot == Item.Slot && !Existing.Contents.IsEmpty())
		{
			SpillContainerContents(Existing);
		}
	}

	// One item per slot: replacing is the normal case, so it is handled here
	// rather than making every caller unequip first.
	Equipped.RemoveAll([&Item](const FDCCEquippedItem& Existing)
	{
		return Existing.Slot == Item.Slot;
	});

	FDCCEquippedItem Entry;
	Entry.Slot = Item.Slot;
	Entry.ItemId = ItemId;
	Equipped.Add(Entry);

	// The server does not receive its own OnRep, so both halves are driven here
	// explicitly. Everything below this line is what OnRep_Equipped does too.
	RebuildVisuals();
	NotifyStatsDirty();
	OnEquipmentChanged.Broadcast(this);

	return true;
}

bool UDCCEquipmentComponent::Unequip(EDCCEquipmentSlot Slot)
{
	AActor* Owner = GetOwner();
	if (!Owner || !Owner->HasAuthority())
	{
		return false;
	}

	// Same reasoning as Equip: spill before removing, or a container's contents
	// are simply gone the moment the container itself is taken off.
	for (const FDCCEquippedItem& Existing : Equipped)
	{
		if (Existing.Slot == Slot && !Existing.Contents.IsEmpty())
		{
			SpillContainerContents(Existing);
		}
	}

	const int32 Removed = Equipped.RemoveAll([Slot](const FDCCEquippedItem& Existing)
	{
		return Existing.Slot == Slot;
	});

	if (Removed <= 0)
	{
		return false;
	}

	RebuildVisuals();
	NotifyStatsDirty();
	OnEquipmentChanged.Broadcast(this);

	return true;
}

void UDCCEquipmentComponent::OnRep_Equipped()
{
	RebuildVisuals();
	OnEquipmentChanged.Broadcast(this);
}

void UDCCEquipmentComponent::AccumulateAbilityBonuses(TArray<int32>& OutBonuses) const
{
	OutBonuses.SetNumZeroed(static_cast<int32>(EDCCAbility::MAX));

	for (const FDCCEquippedItem& Entry : Equipped)
	{
		FDCCEquipmentDef Item;
		if (!FindItem(Entry.ItemId, Item))
		{
			continue;
		}

		// Indexed in the enum's declared order - Str, Int, Con, Dex, Cha - which
		// is the order the books list them in and the order the party's own
		// character sheet uses.
		const int32 Bonuses[] = {
			Item.BonusStrength,
			Item.BonusIntelligence,
			Item.BonusConstitution,
			Item.BonusDexterity,
			Item.BonusCharisma,
		};

		const int32 Count = FMath::Min(
			static_cast<int32>(UE_ARRAY_COUNT(Bonuses)), OutBonuses.Num());

		for (int32 Index = 0; Index < Count; ++Index)
		{
			OutBonuses[Index] += Bonuses[Index];
		}
	}
}

TArray<FDCCModifierSource> UDCCEquipmentComponent::GetAbilityBonusSources(EDCCAbility Ability) const
{
	TArray<FDCCModifierSource> Sources;

	for (const FDCCEquippedItem& Entry : Equipped)
	{
		FDCCEquipmentDef Item;
		if (!FindItem(Entry.ItemId, Item))
		{
			continue;
		}

		const int32 Bonuses[] = {
			Item.BonusStrength,
			Item.BonusIntelligence,
			Item.BonusConstitution,
			Item.BonusDexterity,
			Item.BonusCharisma,
		};

		const int32 Index = static_cast<int32>(Ability);
		const bool bValidIndex = Index >= 0 && Index < static_cast<int32>(UE_ARRAY_COUNT(Bonuses));
		if (bValidIndex && Bonuses[Index] != 0)
		{
			FDCCModifierSource& Source = Sources.AddDefaulted_GetRef();
			Source.Label = Item.DisplayName;
			Source.Amount = Bonuses[Index];
		}
	}

	return Sources;
}

float UDCCEquipmentComponent::GetTotalEquippedMass() const
{
	float Total = 0.0f;

	for (const FDCCEquippedItem& Entry : Equipped)
	{
		FDCCEquipmentDef Item;
		if (FindItem(Entry.ItemId, Item))
		{
			Total += Item.MassKilograms;
		}
	}

	return Total;
}

float UDCCEquipmentComponent::GetAttackRangeOverride() const
{
	float Best = 0.0f;

	// Longest wins, matching GetWeaponDamageDie's "best equipped" rule. Holding a
	// bow and a knife should let you shoot; being forced to the shorter of the
	// two would make picking up a sidearm a downgrade.
	for (const FDCCEquippedItem& Entry : Equipped)
	{
		FDCCEquipmentDef Item;
		if (FindItem(Entry.ItemId, Item))
		{
			Best = FMath::Max(Best, Item.AttackRangeOverride);
		}
	}

	return Best;
}

int32 UDCCEquipmentComponent::GetWeaponDamageDie() const
{
	int32 Best = 0;

	for (const FDCCEquippedItem& Entry : Equipped)
	{
		FDCCEquipmentDef Item;
		if (FindItem(Entry.ItemId, Item))
		{
			Best = FMath::Max(Best, Item.WeaponDamageDie);
		}
	}

	return Best;
}

EDCCDamageType UDCCEquipmentComponent::GetWeaponDamageType() const
{
	int32 Best = 0;
	EDCCDamageType BestType = EDCCDamageType::Physical;

	// Tracks the die and its type together, so "best equipped" never reports
	// one weapon's die with a different weapon's type.
	for (const FDCCEquippedItem& Entry : Equipped)
	{
		FDCCEquipmentDef Item;
		if (FindItem(Entry.ItemId, Item) && Item.WeaponDamageDie > Best)
		{
			Best = Item.WeaponDamageDie;
			BestType = Item.WeaponDamageType;
		}
	}

	return BestType;
}

void UDCCEquipmentComponent::AccumulateDamageResistances(TArray<FDCCResistanceSource>& OutSources) const
{
	for (const FDCCEquippedItem& Entry : Equipped)
	{
		FDCCEquipmentDef Item;
		if (FindItem(Entry.ItemId, Item) && Item.ResistanceLevel != EDCCResistanceLevel::Normal)
		{
			FDCCResistanceSource& Source = OutSources.AddDefaulted_GetRef();
			Source.Label = Item.DisplayName;
			Source.Type = Item.ResistanceDamageType;
			Source.Level = Item.ResistanceLevel;
		}
	}
}

int32 UDCCEquipmentComponent::GetEvadeBonus() const
{
	int32 Total = 0;

	for (const FDCCEquippedItem& Entry : Equipped)
	{
		FDCCEquipmentDef Item;
		if (FindItem(Entry.ItemId, Item))
		{
			Total += Item.EvadeBonus;
		}
	}

	return Total;
}

bool UDCCEquipmentComponent::CanStoreInContainer(EDCCEquipmentSlot Slot, const ADCCPickup* Item) const
{
	if (!Item)
	{
		return false;
	}

	const FDCCEquippedItem* Entry = Equipped.FindByPredicate(
		[Slot](const FDCCEquippedItem& Existing) { return Existing.Slot == Slot; });
	if (!Entry)
	{
		return false;
	}

	FDCCEquipmentDef Def;
	if (!FindItem(Entry->ItemId, Def) || !Def.bIsContainer)
	{
		return false;
	}

	if (Entry->Contents.Num() >= Def.ContainerSlots)
	{
		return false;
	}

	const bool bLiquidOk = Def.bContainerAcceptsLiquids && Item->ItemTags.Contains(TEXT("Liquid"));
	const bool bSizeOk = Item->MassKilograms <= Def.ContainerMaxItemMassKg;

	return bLiquidOk || bSizeOk;
}

bool UDCCEquipmentComponent::StoreInContainer(EDCCEquipmentSlot Slot, ADCCPickup* Item)
{
	AActor* Owner = GetOwner();
	if (!Owner || !Owner->HasAuthority() || !CanStoreInContainer(Slot, Item))
	{
		return false;
	}

	FDCCEquippedItem* Entry = Equipped.FindByPredicate(
		[Slot](const FDCCEquippedItem& Existing) { return Existing.Slot == Slot; });
	if (!Entry)
	{
		return false;
	}

	FDCCContainedItem Stored;
	Stored.PickupClass = Item->GetClass();
	Stored.DisplayName = Item->DisplayName;
	Stored.Gold = Item->GoldValue;
	Stored.EquipmentItemId = Item->EquipmentItemId;
	Stored.MassKilograms = Item->MassKilograms;
	Stored.ItemTags = Item->ItemTags;
	Entry->Contents.Add(Stored);

	// Consumed the same way a picked-up sword is: the world actor's job ends
	// the moment its identity is safely recorded somewhere else.
	Item->Destroy();

	OnEquipmentChanged.Broadcast(this);

	return true;
}

bool UDCCEquipmentComponent::TryStoreInAnyContainer(ADCCPickup* Item)
{
	AActor* Owner = GetOwner();
	if (!Owner || !Owner->HasAuthority() || !Item)
	{
		return false;
	}

	for (const FDCCEquippedItem& Entry : Equipped)
	{
		if (StoreInContainer(Entry.Slot, Item))
		{
			return true;
		}
	}

	return false;
}

ADCCPickup* UDCCEquipmentComponent::RetrieveFromContainer(EDCCEquipmentSlot Slot, int32 Index)
{
	AActor* Owner = GetOwner();
	ADCCCharacterBase* Character = GetOwningCharacter();
	UWorld* World = GetWorld();
	if (!Owner || !Owner->HasAuthority() || !Character || !World)
	{
		return nullptr;
	}

	FDCCEquippedItem* Entry = Equipped.FindByPredicate(
		[Slot](const FDCCEquippedItem& Existing) { return Existing.Slot == Slot; });
	if (!Entry || !Entry->Contents.IsValidIndex(Index))
	{
		return nullptr;
	}

	const FDCCContainedItem Stored = Entry->Contents[Index];
	Entry->Contents.RemoveAt(Index);
	OnEquipmentChanged.Broadcast(this);

	if (!Stored.PickupClass)
	{
		return nullptr;
	}

	// A step in front of the character rather than back in their hand - a
	// retrieved item re-enters the normal carry/throw loop through the
	// world, the same way anything else on the ground does.
	const FVector SpawnLocation = Character->GetActorLocation() + Character->GetActorForwardVector() * 80.0f;

	FActorSpawnParameters SpawnParams;
	SpawnParams.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AdjustIfPossibleButAlwaysSpawn;

	ADCCPickup* Spawned = World->SpawnActor<ADCCPickup>(Stored.PickupClass, SpawnLocation, FRotator::ZeroRotator, SpawnParams);
	if (Spawned)
	{
		Spawned->DisplayName = Stored.DisplayName;
		Spawned->GoldValue = Stored.Gold;
		Spawned->EquipmentItemId = Stored.EquipmentItemId;

		// Restored so the round trip is lossless - CanStoreInContainer's own
		// size/liquid check depends on both, and without this the item could
		// come back at the class default mass with no tags at all.
		if (Stored.MassKilograms > 0.0f)
		{
			Spawned->MassKilograms = Stored.MassKilograms;
		}
		Spawned->ItemTags = Stored.ItemTags;
	}

	return Spawned;
}

void UDCCEquipmentComponent::SpillContainerContents(const FDCCEquippedItem& Entry) const
{
	UWorld* World = GetWorld();
	const ADCCCharacterBase* Character = GetOwningCharacter();
	if (!World || !Character || Entry.Contents.IsEmpty())
	{
		return;
	}

	const FVector SpawnOrigin = Character->GetActorLocation() + Character->GetActorForwardVector() * 80.0f;

	for (const FDCCContainedItem& Stored : Entry.Contents)
	{
		if (!Stored.PickupClass)
		{
			continue;
		}

		// Lightly scattered rather than stacked on one point - the same
		// tight-circle shape DCCLoot::SpawnRolledLoot uses for the same reason.
		const FVector2D Offset = FMath::RandPointInCircle(60.0f);
		const FVector SpawnLocation = SpawnOrigin + FVector(Offset.X, Offset.Y, 0.0f);

		FActorSpawnParameters SpawnParams;
		SpawnParams.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AdjustIfPossibleButAlwaysSpawn;

		ADCCPickup* Spawned = World->SpawnActor<ADCCPickup>(
			Stored.PickupClass, SpawnLocation, FRotator::ZeroRotator, SpawnParams);

		if (!Spawned)
		{
			continue;
		}

		Spawned->DisplayName = Stored.DisplayName;
		Spawned->GoldValue = Stored.Gold;
		Spawned->EquipmentItemId = Stored.EquipmentItemId;

		if (Stored.MassKilograms > 0.0f)
		{
			Spawned->MassKilograms = Stored.MassKilograms;
		}
		Spawned->ItemTags = Stored.ItemTags;
	}
}

void UDCCEquipmentComponent::NotifyStatsDirty(bool bRefillToFull)
{
	if (const ADCCCharacterBase* Character = GetOwningCharacter())
	{
		if (UDCCStatsComponent* Stats = Character->Stats)
		{
			Stats->RecalculateEnhancedScores(bRefillToFull);
		}
	}
}

void UDCCEquipmentComponent::RebuildVisuals()
{
	const ADCCCharacterBase* Character = GetOwningCharacter();
	USkeletalMeshComponent* CharacterMesh = Character ? Character->GetMesh() : nullptr;
	if (!CharacterMesh)
	{
		return;
	}

	// Tear down first. Rebuilding wholesale rather than diffing keeps this
	// correct for the replicated case, where an array arrives changed without
	// any record of what it used to be.
	for (const TPair<EDCCEquipmentSlot, TObjectPtr<UStaticMeshComponent>>& Pair : SlotMeshes)
	{
		if (Pair.Value)
		{
			Pair.Value->DestroyComponent();
		}
	}
	SlotMeshes.Reset();

	for (const TPair<EDCCEquipmentSlot, TObjectPtr<UPointLightComponent>>& Pair : SlotLights)
	{
		if (Pair.Value)
		{
			Pair.Value->DestroyComponent();
		}
	}
	SlotLights.Reset();

	for (const FDCCEquippedItem& Entry : Equipped)
	{
		FDCCEquipmentDef Item;
		if (!FindItem(Entry.ItemId, Item))
		{
			continue;
		}

		// Fall back to the mesh root when the socket is missing rather than
		// dropping the item, so a skeleton that has not had its sockets authored
		// yet shows the gear in the wrong place instead of not at all - a visible
		// wrong answer gets fixed, an invisible one does not.
		const FName Socket = CharacterMesh->DoesSocketExist(Item.AttachSocket)
			? Item.AttachSocket
			: NAME_None;

		if (!Item.Mesh.IsNull())
		{
			// Synchronous because equipping is a discrete, rare, player-initiated
			// event, and a sword that appears a frame late reads as a bug.
			if (UStaticMesh* LoadedMesh = Item.Mesh.LoadSynchronous())
			{
				if (UStaticMeshComponent* MeshComponent = NewObject<UStaticMeshComponent>(GetOwner()))
				{
					MeshComponent->SetStaticMesh(LoadedMesh);

					// Worn gear is scenery: it must never block movement, catch a
					// trace, or take part in a physics query. The character's own
					// capsule does all of that already.
					MeshComponent->SetCollisionEnabled(ECollisionEnabled::NoCollision);
					MeshComponent->SetGenerateOverlapEvents(false);

					MeshComponent->RegisterComponent();
					MeshComponent->AttachToComponent(CharacterMesh,
						FAttachmentTransformRules::SnapToTargetNotIncludingScale, Socket);

					SlotMeshes.Add(Entry.Slot, MeshComponent);
				}
			}
		}

		// Independent of whether a mesh exists: a light-only item is a valid
		// shape even though nothing in the table currently uses it that way.
		if (Item.bEmitsLight)
		{
			if (UPointLightComponent* LightComponent = NewObject<UPointLightComponent>(GetOwner()))
			{
				LightComponent->SetLightColor(Item.LightColor);
				LightComponent->SetIntensity(Item.LightIntensity);
				LightComponent->SetIntensityUnits(ELightUnits::Candelas);
				LightComponent->SetAttenuationRadius(Item.LightAttenuationRadius);
				LightComponent->SetCastShadows(false);

				LightComponent->RegisterComponent();
				LightComponent->AttachToComponent(CharacterMesh,
					FAttachmentTransformRules::SnapToTargetNotIncludingScale, Socket);

				SlotLights.Add(Entry.Slot, LightComponent);
			}
		}
	}
}
