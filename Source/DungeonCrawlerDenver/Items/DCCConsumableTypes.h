#pragma once

#include "CoreMinimal.h"
#include "Engine/DataTable.h"
#include "GameplayTagContainer.h"
#include "Stats/DCCAbilityTypes.h"
#include "DCCConsumableTypes.generated.h"

class UStaticMesh;

/**
 * One effect a consumable applies when drunk. Deliberately generic rather
 * than one struct per effect "kind" - a single row covers "heal", "hurt",
 * "temporary ability nudge", or (with all three fields set) all of them at
 * once, e.g. a rotgut that hurts a little AND leaves you Woozy.
 *
 * Translated into calls on the DRINKER's UDCCStatsComponent
 * (Heal/ApplyDamage/ApplyTimedAbilityModifier) by
 * ADCCCharacterBase::ApplyConsumableEffects - the Stats layer itself stays
 * ruleset-agnostic and has no idea a "consumable" exists, per Stats/'s own
 * architecture rule.
 */
USTRUCT(BlueprintType)
struct FDCCConsumableEffect
{
	GENERATED_BODY()

	/** Positive heals, negative hurts, zero does nothing to health. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Consumable")
	int32 InstantHealthChange = 0;

	/** Which score ModifierAmount nudges. Ignored if ModifierAmount is zero. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Consumable")
	EDCCAbility ModifiedAbility = EDCCAbility::Strength;

	/**
	 * Positive or negative - a Charisma boost and a "Woozy" Dexterity hit are
	 * both just this field with a different sign. Zero means this effect does
	 * not touch an ability score at all.
	 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Consumable")
	int32 ModifierAmount = 0;

	/** How long ModifierAmount lasts. Ignored (and wasted) if ModifierAmount is zero. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Consumable", meta = (ClampMin = "0.0"))
	float ModifierDurationSeconds = 0.0f;
};

/**
 * A drinkable (or, later, eatable) item - the bartender's actual stock, per
 * Zoe. Its own DataTable row rather than folded into FDCCEquipmentDef: a
 * drink is never worn in a slot, it is consumed and gone (or stowed and
 * consumed later), which is a different enough lifecycle that sharing one
 * struct would mean most of its columns are meaningless for either use.
 */
USTRUCT(BlueprintType)
struct FDCCConsumableDef : public FTableRowBase
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Consumable")
	FName ItemId;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Consumable")
	FText DisplayName;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Consumable")
	FText Description;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Consumable", meta = (ClampMin = "0"))
	int32 GoldValue = 0;

	/**
	 * Free-form classification - Liquid, Alcohol, Flammable, Organic,
	 * Craftable, whatever fits (Zoe's own examples). Not read by any code yet;
	 * this is the hook for crafting/interaction systems that don't exist yet
	 * rather than a currently-consumed input, so an empty container is not a
	 * bug - just nothing downstream asks yet.
	 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Consumable")
	FGameplayTagContainer Tags;

	/** What actually happens when this is drunk. Empty is legal - a purely flavour drink. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Consumable")
	TArray<FDCCConsumableEffect> Effects;

	/**
	 * A skill that makes this work better (First Aid, for a med kit). Using it is
	 * a skill check (Int): a success adds HealPerSkillRank per Rank on top of the
	 * Effects; a fail gives the Effects alone. Untrained use counts towards
	 * learning the skill. None means a plain drink, as before.
	 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Consumable|Skills")
	FName LinkedSkillId;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Consumable|Skills", meta = (ClampMin = "0"))
	int32 HealPerSkillRank = 0;

	/** Optional - shown held/stowed once a visual pass exists. Not required for Effects to work. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Consumable")
	TSoftObjectPtr<UStaticMesh> Mesh;
};
