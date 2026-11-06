#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "Equipment/DCCEquipmentTypes.h"
#include "DCCEquipmentComponent.generated.h"

class UDataTable;
class UStaticMeshComponent;
class UPointLightComponent;
class ADCCCharacterBase;
class ADCCPickup;

DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FDCCEquipmentChanged, UDCCEquipmentComponent*, Equipment);

/**
 * What a character is wearing, and what that does to their numbers.
 *
 * Server-authoritative in the same shape as skills and carrying: a client may
 * ask to equip, but the slot, the stat contribution and the resulting Enhanced
 * scores are all decided on the server and arrive by replication.
 *
 * The visual half is deliberately separate from the stat half. Meshes are
 * rebuilt from the replicated item list on every machine, so a client that has
 * never seen the equipment table still shows the right sword - it only needs the
 * row, which it has, not the rules.
 */
UCLASS(ClassGroup = (DCC), meta = (BlueprintSpawnableComponent))
class DUNGEONCRAWLERDENVER_API UDCCEquipmentComponent : public UActorComponent
{
	GENERATED_BODY()

public:
	UDCCEquipmentComponent();

	virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;
	virtual void BeginPlay() override;

	/** The item list. Without one this component does nothing at all. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Equipment")
	TObjectPtr<UDataTable> EquipmentTable;

	/** What is worn right now, one entry per filled slot. */
	UPROPERTY(BlueprintReadOnly, ReplicatedUsing = OnRep_Equipped, Category = "Equipment")
	TArray<FDCCEquippedItem> Equipped;

	UPROPERTY(BlueprintAssignable, Category = "Equipment")
	FDCCEquipmentChanged OnEquipmentChanged;

	/**
	 * Server only. Equips ItemId into its own slot, replacing whatever was there.
	 * Returns false for an unknown id or a missing table.
	 */
	UFUNCTION(BlueprintCallable, Category = "Equipment")
	bool Equip(FName ItemId);

	/** Server only. Empties a slot. Returns false if it was already empty. */
	UFUNCTION(BlueprintCallable, Category = "Equipment")
	bool Unequip(EDCCEquipmentSlot Slot);

	UFUNCTION(BlueprintPure, Category = "Equipment")
	FName GetEquippedId(EDCCEquipmentSlot Slot) const;

	/** Looks up a row. False when the table is missing or the id is unknown. */
	bool FindItem(FName ItemId, FDCCEquipmentDef& OutItem) const;

	/** First equipped item with bIsCursed && bForcesActionOnCombatStart, if any - see ADCCCharacterBase::NotifyCombatStarted. */
	bool FindCursedCombatStartEffect(FDCCEquipmentDef& OutItem) const;

	/**
	 * Total ability bonus across every equipped item. Read by the stats
	 * component when it rebuilds the Enhanced layer.
	 */
	void AccumulateAbilityBonuses(TArray<int32>& OutBonuses) const;

	/** One entry per equipped item that bonuses Ability - the Feed roll-
	 *  breakdown's "Equipment" row(s). */
	UFUNCTION(BlueprintPure, Category = "Equipment")
	TArray<FDCCModifierSource> GetAbilityBonusSources(EDCCAbility Ability) const;

	/** Summed MassKilograms across every equipped item - "wearing heavy
	 *  armour," read by the stamina regen formula. */
	UFUNCTION(BlueprintPure, Category = "Equipment")
	float GetTotalEquippedMass() const;

	/** The damage type of whichever equipped item is currently winning
	 *  GetWeaponDamageDie's "best equipped" comparison - the two always refer
	 *  to the same item, since a die and a type from different weapons would
	 *  mean nothing. */
	UFUNCTION(BlueprintPure, Category = "Equipment")
	EDCCDamageType GetWeaponDamageType() const;

	/** Every equipped item granting a real resistance (Level != Normal), one
	 *  entry per item - UDCCStatsComponent::RecalculateDamageResistances
	 *  merges this with UDCCClassComponent's own list. */
	void AccumulateDamageResistances(TArray<FDCCResistanceSource>& OutSources) const;

	/** Highest damage die among equipped items (rarity bonus included), or 0 if nothing overrides it. */
	UFUNCTION(BlueprintPure, Category = "Equipment")
	int32 GetWeaponDamageDie() const;

	/**
	 * A single item's real weapon die - its own WeaponDamageDie plus its
	 * rarity's bonus, or 0 outright for anything that isn't a weapon at all.
	 * Exposed (not just used internally by GetWeaponDamageDie) so anything
	 * comparing one item's damage against another - the ally auto-equip
	 * "is this an upgrade" heuristic, most notably - judges rarity-tiered
	 * gear correctly instead of only ever seeing the item's raw authored die.
	 */
	static int32 GetItemEffectiveDamageDie(const FDCCEquipmentDef& Item);

	/** Longest attack reach among equipped items, or 0 if none is ranged. */
	UFUNCTION(BlueprintPure, Category = "Equipment")
	float GetAttackRangeOverride() const;

	/** Summed Evade contribution of everything worn. */
	UFUNCTION(BlueprintPure, Category = "Equipment")
	int32 GetEvadeBonus() const;

	/** Convenience finder, mirroring UDCCAchievementComponent::Find. */
	static UDCCEquipmentComponent* Find(AActor* Actor);

	// -- Containers ----------------------------------------------------------
	// A container is just an equipped item whose row has bIsContainer set -
	// there is no separate "container" slot or component. See
	// FDCCEquipmentDef's Container fields and FDCCContainedItem.

	/** True if Slot holds a container with room for Item, per its size/liquid rule. */
	UFUNCTION(BlueprintPure, Category = "Equipment|Container")
	bool CanStoreInContainer(EDCCEquipmentSlot Slot, const ADCCPickup* Item) const;

	/**
	 * Server only. Records Item's identity into Slot's container and destroys
	 * the world pickup. False (and no side effect) if Slot cannot take it.
	 */
	UFUNCTION(BlueprintCallable, Category = "Equipment|Container")
	bool StoreInContainer(EDCCEquipmentSlot Slot, ADCCPickup* Item);

	/** Server only. Tries every equipped container slot in turn; stops at the first that accepts Item. */
	UFUNCTION(BlueprintCallable, Category = "Equipment|Container")
	bool TryStoreInAnyContainer(ADCCPickup* Item);

	/**
	 * Server only. Removes entry Index from Slot's container and spawns a
	 * fresh ADCCPickup near the owning character rebuilt from what was
	 * stored. Null if the slot, index or owning character is invalid.
	 */
	UFUNCTION(BlueprintCallable, Category = "Equipment|Container")
	ADCCPickup* RetrieveFromContainer(EDCCEquipmentSlot Slot, int32 Index);

	/** Re-hang equipped meshes after the owner's body changes (sockets live on the mesh, not the skeleton). */
	void RefreshAttachments() { RebuildVisuals(); }

protected:
	UFUNCTION()
	void OnRep_Equipped();

	ADCCCharacterBase* GetOwningCharacter() const;

	/**
	 * Server only. Spawns every item still stored in Entry's container back
	 * into the world near the owning character, so Equip/Unequip replacing or
	 * clearing a container slot never just deletes what was inside it.
	 *
	 * Previously neither call site did this at all - a canteen full of picked-up
	 * items simply vanished the moment it was unequipped or swapped for
	 * something else, unlike RetrieveFromContainer, which has always spawned a
	 * real pickup per removed entry.
	 */
	void SpillContainerContents(const FDCCEquippedItem& Entry) const;

	/**
	 * Rebuilds the attached meshes from Equipped. Runs on every machine, because
	 * a sword has to be visible on clients too.
	 */
	void RebuildVisuals();

	/**
	 * Tells the stats component its Enhanced layer is stale. Server only.
	 *
	 * bRefillToFull is passed straight through to
	 * UDCCStatsComponent::RecalculateEnhancedScores - false for every runtime
	 * Equip/Unequip (swapping gear must not rubber-band a damaged character
	 * back to full), true only from this component's own BeginPlay, where it
	 * closes the gap between InitializeComponent's unenhanced refill (which
	 * runs first and cannot see equipment yet) and whatever a character's
	 * starting gear actually grants.
	 */
	void NotifyStatsDirty(bool bRefillToFull = false);

	/**
	 * Spawned mesh components, one per visible equipped item, keyed by slot so a
	 * slot change only rebuilds its own.
	 *
	 * Tracked explicitly rather than found by iterating the actor's components,
	 * so this can never destroy a mesh that belongs to something else.
	 */
	UPROPERTY(Transient)
	TMap<EDCCEquipmentSlot, TObjectPtr<UStaticMeshComponent>> SlotMeshes;

	/** Same idea, for the light a torch-like item attaches alongside its mesh. */
	UPROPERTY(Transient)
	TMap<EDCCEquipmentSlot, TObjectPtr<UPointLightComponent>> SlotLights;
};
