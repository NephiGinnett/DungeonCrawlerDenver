#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "DCCInventoryComponent.generated.h"

class ADCCPickup;
class UDCCInventoryComponent;

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
	 * Server only. Respawns the item at a world location and removes the record.
	 *
	 * Returns the new actor, or null if the index was invalid or the spawn failed
	 * - in which case the record is kept rather than silently losing the item.
	 */
	UFUNCTION(BlueprintCallable, Category = "Inventory")
	ADCCPickup* DropItem(int32 Index, const FVector& WorldLocation);

protected:
	UFUNCTION()
	void OnRep_Items();
};
