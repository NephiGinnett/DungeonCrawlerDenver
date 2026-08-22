#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "Items/DCCLootTypes.h"
#include "Items/DCCLootSource.h"
#include "DCCLootContainer.generated.h"

class UStaticMeshComponent;

/**
 * A chest: a static container the loot menu can open, exactly like a corpse.
 *
 * Deliberately NOT derived from ADCCCharacterBase, for the same reason
 * ADCCDialogueNPC is not - that class assumes combat all the way down, and a
 * chest fights nothing, has no stats, and is never a room occupant an
 * encounter can arm. The two classes share behaviour instead: both implement
 * IDCCLootSource, so DCCCharacterBase's loot menu code opens either without
 * caring which it is.
 *
 * Contents roll once, at BeginPlay, into PendingLoot - a chest has no "death"
 * to roll at, so BeginPlay is the corpse-equivalent moment. Chance = 1.0
 * entries in LootTable are effectively guaranteed contents, which is how the
 * curated variants in DCCLootContainerVariants.h place specific gear rather
 * than a random spread.
 */
UCLASS(Blueprintable)
class DUNGEONCRAWLERDENVER_API ADCCLootContainer : public AActor, public IDCCLootSource
{
	GENERATED_BODY()

public:
	ADCCLootContainer();

	virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;
	virtual void BeginPlay() override;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Loot")
	TObjectPtr<UStaticMeshComponent> Mesh;

	/** What the loot menu titles itself with. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Loot")
	FText DisplayName;

	/** Rolled once at BeginPlay. Author with Chance = 1.0 for guaranteed contents. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Loot")
	TArray<FDCCLootDrop> LootTable;

	/** Replicated so a client can grey out the menu without asking. */
	UPROPERTY(BlueprintReadOnly, ReplicatedUsing = OnRep_HasLoot, Category = "Loot")
	bool bHasLoot = false;

	// -- IDCCLootSource --------------------------------------------------------
	virtual FText GetLootSourceName() const override;
	virtual bool HasAnyLoot() const override;
	virtual const TArray<FDCCRolledLoot>& GetPendingLootItems() const override;
	virtual FVector GetLootOrigin() const override;
	virtual void LootOne(int32 Index, ADCCCharacterBase* Looter) override;
	virtual void LootAll(ADCCCharacterBase* Looter) override;
	virtual void StowInto(ADCCCharacterBase* Looter) override;

protected:
	/** Server only. Rolls LootTable into PendingLoot. */
	void RollLoot();

	UFUNCTION()
	void OnRep_HasLoot();

	/**
	 * Rolled contents. Replicated - unlike a corpse's own PendingLoot before
	 * this feature existed, the itemised "Check Inventory" submenu has to read
	 * real item data on whichever machine is drawing it, not just the server.
	 */
	UPROPERTY(Replicated, Transient)
	TArray<FDCCRolledLoot> PendingLoot;
};
