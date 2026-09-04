#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "Items/DCCLootTypes.h"
#include "Items/DCCConsumableTypes.h"
#include "Items/DCCLootBoxTypes.h"
#include "DCCInventoryComponent.generated.h"

class ADCCPickup;
class UDCCInventoryComponent;
class ADCCCharacterBase;

/**
 * One stowed item.
 *
 * Stores the pickup's CLASS and a snapshot of its display data rather than a
 * pointer to the actor, because a stowed item is destroyed in the world - it
 * exists only as a record until it is dropped back out and respawned. Keeping a
 * pointer would mean either leaving invisible actors lying around the level or
 * holding a dangling reference.
 */
USTRUCT(BlueprintType)
struct FDCCInventoryItem
{
	GENERATED_BODY()

	UPROPERTY(BlueprintReadOnly, Category = "Inventory")
	TSubclassOf<ADCCPickup> PickupClass;

	UPROPERTY(BlueprintReadOnly, Category = "Inventory")
	FText DisplayName;

	/** Kept so the HUD can show carried weight without loading every class. */
	UPROPERTY(BlueprintReadOnly, Category = "Inventory")
	float MassKilograms = 0.0f;

	/**
	 * Set only when this record is a piece of gear (a DT_DCCEquipment row id) -
	 * empty for anything else. Gear no longer equips itself on pickup (changed
	 * 2026-08-30, see DISPATCH.md) - it lands here like everything else, and
	 * UDCCInventoryComponent::EquipItem is the one place that actually equips
	 * it, on request from the Inventory page.
	 */
	UPROPERTY(BlueprintReadOnly, Category = "Inventory")
	FName EquipmentItemId;
};

/**
 * A whole corpse or chest, stowed rather than looted item-by-item.
 *
 * "Stow Body" - the loot menu's third option, on both corpses and chests -
 * hands everything the source still had over to this record and retires the
 * source, per IDCCLootSource::StowInto. Kept as a record rather than a live
 * actor for the same reason FDCCInventoryItem is: the world object goes away,
 * and only the data needed to draw and later act on it survives.
 */
USTRUCT(BlueprintType)
struct FDCCStowedRemains
{
	GENERATED_BODY()

	/** Whatever the source's loot menu was titled - "Mob_Brute_Courtyard", a chest's name. */
	UPROPERTY(BlueprintReadOnly, Category = "Inventory")
	FText DisplayName;

	/** Snapshot of whatever the source had not yet been looted of. */
	UPROPERTY(BlueprintReadOnly, Category = "Inventory")
	TArray<FDCCRolledLoot> Contents;
};

/**
 * A drink (or future food) stowed for later rather than drunk on the spot.
 *
 * A third, separate array rather than folded into Items - same reasoning as
 * StowedRemains above: a stack of identical bottles is a different shape
 * (ItemId + Quantity) than a one-class-per-prop pickup record
 * (PickupClass + mass), and mixing the two would mean every reader has to
 * check which kind of entry it is looking at.
 */
USTRUCT(BlueprintType)
struct FDCCStowedConsumable
{
	GENERATED_BODY()

	UPROPERTY(BlueprintReadOnly, Category = "Inventory")
	FName ItemId;

	UPROPERTY(BlueprintReadOnly, Category = "Inventory")
	FText DisplayName;

	/** Stacks rather than one row per bottle - buying three of the same round should not cost three slots. */
	UPROPERTY(BlueprintReadOnly, Category = "Inventory")
	int32 Quantity = 0;
};

DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FDCCInventoryChanged, UDCCInventoryComponent*, Inventory);

/**
 * What a character is carrying but not holding.
 *
 * Separate from UDCCCarryComponent on purpose: that models the one thing in your
 * hands, which is a physical object in the world with collision and a throw
 * arc. This models the rucksack - records, not actors.
 *
 * Server-authoritative like everything else. Clients see the list by replication
 * so the menu can be drawn without asking the server what it is allowed to show.
 */
UCLASS(ClassGroup = (DCC), meta = (BlueprintSpawnableComponent))
class DUNGEONCRAWLERDENVER_API UDCCInventoryComponent : public UActorComponent
{
	GENERATED_BODY()

public:
	UDCCInventoryComponent();

	virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;

	/**
	 * How many items fit, regardless of weight.
	 *
	 * A flat cap rather than a weight budget: Strength already governs what you
	 * can pick up in the first place, and stacking a second weight rule on top
	 * would make it unclear which one refused you.
	 */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Inventory", meta = (ClampMin = "0"))
	int32 Capacity = 12;

	UPROPERTY(BlueprintReadOnly, ReplicatedUsing = OnRep_Items, Category = "Inventory")
	TArray<FDCCInventoryItem> Items;

	UPROPERTY(BlueprintAssignable, Category = "Inventory")
	FDCCInventoryChanged OnInventoryChanged;

	UFUNCTION(BlueprintPure, Category = "Inventory")
	bool IsFull() const { return Items.Num() >= Capacity; }

	/** Total mass stowed. Reported for flavour; it does not gate anything yet. */
	UFUNCTION(BlueprintPure, Category = "Inventory")
	float GetTotalMass() const;

	/**
	 * Server only. Records the pickup and removes it from the world.
	 *
	 * Returns false when full, leaving the pickup untouched - so a failed stow
	 * never destroys the item.
	 */
	UFUNCTION(BlueprintCallable, Category = "Inventory")
	bool StowPickup(ADCCPickup* Pickup);

	/**
	 * Server only. Records an item directly, with no live pickup actor involved.
	 * StowPickup (above) is really "read a live pickup's data, then call this
	 * and destroy it" - this is the other caller, DCCLoot::SpawnRolledLoot,
	 * which already knows a looter is standing right here and would otherwise
	 * have to spawn a ground pickup for no reason other than to immediately
	 * read it back off the ground. Same capacity rule as StowPickup.
	 */
	UFUNCTION(BlueprintCallable, Category = "Inventory")
	bool AddItemRecord(TSubclassOf<ADCCPickup> PickupClass, const FText& DisplayName, float MassKilograms, FName EquipmentItemId);

	/**
	 * Server only. Equips Items[Index] if it is a piece of gear (EquipmentItemId
	 * set) and the slot will actually take it - a cursed item already holding
	 * that slot means UDCCEquipmentComponent::Equip refuses, in which case the
	 * record is left alone rather than lost. The Tab menu's Inventory page
	 * calls this on a clicked row, same click-to-act convention as
	 * StowedRemains/StowedConsumables.
	 */
	UFUNCTION(BlueprintCallable, Category = "Inventory")
	bool EquipItem(int32 Index);

	/**
	 * Server only. Respawns the item at a world location and removes the record.
	 *
	 * Returns the new actor, or null if the index was invalid or the spawn failed
	 * - in which case the record is kept rather than silently losing the item.
	 */
	UFUNCTION(BlueprintCallable, Category = "Inventory")
	ADCCPickup* DropItem(int32 Index, const FVector& WorldLocation);

	// -- Stowed remains --------------------------------------------------------
	// A corpse or chest carried away whole via "Stow Body" rather than looted
	// on the spot. Separate array from Items on purpose: a stowed body is not a
	// prop with a mass and a class, it is a bundle of still-unresolved loot -
	// mixing the two shapes into one list would mean every reader has to check
	// which kind of entry it is looking at.

	UPROPERTY(BlueprintReadOnly, ReplicatedUsing = OnRep_StowedRemains, Category = "Inventory")
	TArray<FDCCStowedRemains> StowedRemains;

	/** Server only. Always succeeds - a stowed body has no capacity limit of its own. */
	UFUNCTION(BlueprintCallable, Category = "Inventory")
	void StowRemains(const FText& SourceName, const TArray<FDCCRolledLoot>& Contents);

	/** Server only. Spawns Contents[ItemIndex] near WorldLocation and removes just that entry. */
	UFUNCTION(BlueprintCallable, Category = "Inventory")
	bool LootOneFromStowed(int32 RemainsIndex, int32 ItemIndex, const FVector& WorldLocation);

	/** Server only. Spawns everything left in Contents and forgets the record. */
	UFUNCTION(BlueprintCallable, Category = "Inventory")
	bool LootAllFromStowed(int32 RemainsIndex, const FVector& WorldLocation);

	// -- Consumables -------------------------------------------------------
	// Drinks bought and stowed for later rather than drunk on the spot - see
	// ADCCVendorNPC and ADCCCharacterBase::Server_PurchaseDrink.

	/** Defaulted via ConstructorHelpers to /Game/DCC/Items/DT_DCCConsumables, same pattern as every other content table in this project. */
	UPROPERTY(EditDefaultsOnly, Category = "Inventory")
	TObjectPtr<UDataTable> ConsumableTable;

	UFUNCTION(BlueprintPure, Category = "Inventory")
	bool FindConsumable(FName ItemId, FDCCConsumableDef& OutItem) const;

	UPROPERTY(BlueprintReadOnly, ReplicatedUsing = OnRep_StowedConsumables, Category = "Inventory")
	TArray<FDCCStowedConsumable> StowedConsumables;

	/** Server only. Stacks onto an existing entry for the same ItemId rather than adding a new row. */
	UFUNCTION(BlueprintCallable, Category = "Inventory")
	void AddConsumable(FName ItemId, int32 Quantity = 1);

	/**
	 * Server only. Resolves Index's ItemId to its full def (for the caller to
	 * apply Effects against the drinker's own Stats - this component only
	 * owns the record, not the drinking) and decrements Quantity, removing the
	 * row entirely once it hits zero. Returns false and leaves OutDef default
	 * if Index was invalid.
	 */
	UFUNCTION(BlueprintCallable, Category = "Inventory")
	bool ConsumeStowedConsumable(int32 Index, FDCCConsumableDef& OutDef);

	// -- Loot boxes --------------------------------------------------------
	// Dungeon Crawler Carl's actual mechanic: achievements pay a sealed box,
	// not items directly. See Items/DCCLootBoxTypes.h for the full design
	// note. Counts are fungible per tier - "own 2 Gold boxes" - rather than
	// per-achievement records, since a box's identity is its tier, not which
	// achievement paid it.

	/** Defaulted via ConstructorHelpers to /Game/DCC/Items/DT_DCCLootBoxTables, same pattern as every other content table in this project. */
	UPROPERTY(EditDefaultsOnly, Category = "Inventory|LootBox")
	TObjectPtr<UDataTable> LootBoxTable;

	/** Defaulted via ConstructorHelpers to /Game/DCC/Items/DT_DCCBoons. */
	UPROPERTY(EditDefaultsOnly, Category = "Inventory|LootBox")
	TObjectPtr<UDataTable> BoonTable;

	/** Indexed by EDCCLootBoxTier - LootBoxCounts[(uint8)EDCCLootBoxTier::Gold] is how many Gold boxes are owned. Index None is always 0 and unused. */
	UPROPERTY(BlueprintReadOnly, ReplicatedUsing = OnRep_LootBoxCounts, Category = "Inventory|LootBox")
	TArray<int32> LootBoxCounts;

	UFUNCTION(BlueprintPure, Category = "Inventory|LootBox")
	int32 GetLootBoxCount(EDCCLootBoxTier Tier) const;

	/** Server only. Always succeeds - owning a box has no capacity limit, same as StowedRemains. */
	UFUNCTION(BlueprintCallable, Category = "Inventory|LootBox")
	void GrantLootBox(EDCCLootBoxTier Tier, int32 Count = 1);

	/**
	 * Server only. Consumes one box of Tier if owned, rolls its table, and
	 * spawns the item half near WorldLocation via the same DCCLoot helpers
	 * every other loot source uses. Returns false (and leaves OutResult
	 * empty, nothing consumed) if none of that tier are owned or Tier has no
	 * row in LootBoxTable.
	 *
	 * Deliberately does not check what room the caller is standing in - that
	 * is ADCCCharacterBase::Server_OpenLootBox's job, the same split
	 * InteractionRange checks already follow: this component owns the box
	 * and the roll, not where a character is allowed to be when it asks.
	 */
	UFUNCTION(BlueprintCallable, Category = "Inventory|LootBox")
	bool TryOpenLootBox(EDCCLootBoxTier Tier, const FVector& WorldLocation, FDCCOpenedLootBoxResult& OutResult);

protected:
	UFUNCTION()
	void OnRep_Items();

	UFUNCTION()
	void OnRep_StowedRemains();

	UFUNCTION()
	void OnRep_StowedConsumables();

	UFUNCTION()
	void OnRep_LootBoxCounts();

	/** Shared spawn loop for TryOpenLootBox's item half - kept out of that function so its own logic reads as roll, then spawn, then boons, not interleaved. */
	void SpawnLootBoxItems(const TArray<FDCCRolledLoot>& RolledItems, const FVector& WorldLocation) const;
};
