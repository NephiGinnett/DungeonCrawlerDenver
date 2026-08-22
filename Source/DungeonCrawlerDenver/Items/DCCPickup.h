#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "DCCPickup.generated.h"

class ADCCCharacterBase;
class UStaticMeshComponent;

// -----------------------------------------------------------------------------
// PROPS YOU CAN PICK UP AND THROW
//
// This exists to make Strength legible. A number on a character sheet is an
// abstraction; a crawler who can lift the big crate when their friend cannot is
// the same information delivered as a moment at the table. Everything here is
// therefore deliberately physical and visible rather than statistical.
//
// The design doc's "anything is possible" promise starts with the mundane case:
// if a player can see a crate, they should be able to throw it at something.
// -----------------------------------------------------------------------------

/**
 * A physics prop a character can carry and throw.
 *
 * Server-authoritative in the same shape as the rest of the game: only the
 * server decides whether a pickup happened, what a throw is worth, and whether
 * an impact hurt anyone. Clients see the outcome through replicated movement.
 */
UCLASS(Blueprintable)
class DUNGEONCRAWLERDENVER_API ADCCPickup : public AActor
{
	GENERATED_BODY()

public:
	ADCCPickup();

	virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Pickup")
	TObjectPtr<UStaticMeshComponent> Mesh;

	/** What the HUD and the AI narrator call this. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Pickup")
	FText DisplayName;

	/**
	 * Mass in kilograms, checked against the lifter's Strength.
	 *
	 * Authored rather than read back from the physics body because the physical
	 * mass of a mesh is a function of its bounds and material density, which is
	 * a terrible way to decide game balance - the light-looking crate would
	 * outweigh the barrel purely because it is bigger.
	 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Pickup", meta = (ClampMin = "0.1"))
	float MassKilograms = 25.0f;

	/** Die rolled for impact damage when thrown. Scaled by impact speed and Strength. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Pickup", meta = (ClampMin = "1"))
	int32 ImpactDamageDie = 4;

	/** Set false for scenery that should stay put. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Pickup")
	bool bCanBePickedUp = true;

	/**
	 * What this is worth in gold. Zero means it is a prop, not loot.
	 *
	 * Kept on the pickup rather than in a separate loot asset because the whole
	 * point of the Pantry is that value and mass live on the same object - the
	 * argument about who carries the heavy thing is only interesting because the
	 * heavy thing is also the valuable one.
	 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Replicated, Category = "Pickup|Loot", meta = (ClampMin = "0"))
	int32 GoldValue = 0;

	/**
	 * True: banks its gold the moment it is lifted, then vanishes. Coins and
	 * small loot work this way, because making the party walk pocket change to
	 * the exit is tedium rather than tension.
	 *
	 * False: keeps its value until somebody carries it out through the floor
	 * exit. That is the Pantry's twist made mechanical - the strongbox is only
	 * worth anything if a crawler commits to carrying it, and carrying it is
	 * exactly what stops them being useful in the boss fight.
	 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Pickup|Loot")
	bool bBanksOnPickup = true;

	/** True once its gold has been paid out, so it can never be banked twice. */
	UPROPERTY(BlueprintReadOnly, Category = "Pickup|Loot")
	bool bValueClaimed = false;

	/**
	 * A DT_DCCEquipment row id. None (the default) means "not equipment, ignore
	 * this field entirely" - every mass-tier prop and coin leaves it unset and
	 * behaves exactly as before.
	 *
	 * Set, OnPickedUp equips it straight into the carrier's matching slot and
	 * the pickup consumes itself, the same "pick it up and it is just yours"
	 * shape bBanksOnPickup already gives coins - gear does not want the
	 * carry/throw loop, since nobody means to throw the sword they just found.
	 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Replicated, Category = "Pickup|Equipment")
	FName EquipmentItemId;

	/**
	 * Freeform category tags. "Liquid" is the one a container item currently
	 * checks for (see FDCCEquipmentDef::bContainerAcceptsLiquids) - kept as
	 * an open tag list rather than a single bool so a later container can
	 * filter on something else without another pass over every pickup.
	 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Pickup")
	TArray<FName> ItemTags;

	/**
	 * Server only. Pays GoldValue to Claimant and marks this claimed.
	 *
	 * Returns the amount actually paid, which is zero for a prop, for something
	 * already claimed, or for a claimant with no stats.
	 */
	int32 ClaimValue(ADCCCharacterBase* Claimant);

	/** True while a character is holding this. Replicated so clients suppress physics. */
	UPROPERTY(BlueprintReadOnly, ReplicatedUsing = OnRep_IsHeld, Category = "Pickup")
	bool bIsHeld = false;

	UFUNCTION(BlueprintPure, Category = "Pickup")
	bool IsHeld() const { return bIsHeld; }

	/** Server only. Attaches to the carrier and stops simulating. */
	void OnPickedUp(ADCCCharacterBase* Carrier);

	/** Server only. Detaches and resumes simulating, with no launch impulse. */
	void OnDropped();

	/**
	 * Server only. Detaches, resumes simulating and launches along Velocity.
	 *
	 * Thrower is remembered so an impact can be attributed and rolled against
	 * their Strength. Note the parameter is NOT named Instigator - AActor already
	 * declares a property by that name and UHT rejects the shadow.
	 */
	void OnThrown(const FVector& Velocity, ADCCCharacterBase* Thrower);

protected:
	virtual void BeginPlay() override;

	UFUNCTION()
	void OnRep_IsHeld();

	/** Applies physical state for the current bIsHeld value. Runs on every machine. */
	void ApplyHeldState();

	UFUNCTION()
	void HandleMeshHit(
		UPrimitiveComponent* HitComponent,
		AActor* OtherActor,
		UPrimitiveComponent* OtherComponent,
		FVector NormalImpulse,
		const FHitResult& Hit);

	/** Cosmetic hook for pickup/throw VFX and sound. */
	UFUNCTION(BlueprintImplementableEvent, Category = "Pickup", meta = (DisplayName = "On Carry State Changed"))
	void BP_OnCarryStateChanged(bool bNowHeld);

private:
	/**
	 * Who threw this, for damage attribution. Server-side only and cleared once
	 * the throw has resolved, so a crate that has come to rest cannot later be
	 * blamed on someone who has since left the encounter.
	 */
	UPROPERTY(Transient)
	TObjectPtr<ADCCCharacterBase> ThrownByCharacter;

	/**
	 * True between leaving the hand and the first meaningful impact.
	 *
	 * Without this a thrown crate would deal damage on every bounce as it rolled
	 * through a crowd, which turns one throw into a room clear.
	 */
	bool bThrowIsLive = false;
};
