#pragma once

#include "CoreMinimal.h"
#include "DCCDamageTypes.generated.h"

// -----------------------------------------------------------------------------
// DAMAGE TYPES
//
// Scoped deliberately narrow - see Docs/races-and-classes.md's engineering gap
// analysis. These eight are the only ones any authored Class benefit actually
// uses; add more only when a real class needs one, not as a taxonomy exercise.
// Physical is the default/untyped bucket every existing damage source already
// falls into, so nothing that predates this system needs to change to keep
// working exactly as it did.
// -----------------------------------------------------------------------------

UENUM(BlueprintType)
enum class EDCCDamageType : uint8
{
	/** The untyped default. Every damage source that doesn't author a type
	 *  deals this, and nothing is ever resistant/vulnerable/immune to it by
	 *  design - it is the "no interaction" bucket, not a ninth real type. */
	Physical    UMETA(DisplayName = "Physical"),

	Fire        UMETA(DisplayName = "Fire"),
	Cold        UMETA(DisplayName = "Cold"),
	Electric    UMETA(DisplayName = "Electric"),
	Poison      UMETA(DisplayName = "Poison"),
	Necrotic    UMETA(DisplayName = "Necrotic"),
	Psychic     UMETA(DisplayName = "Psychic"),
	Bludgeoning UMETA(DisplayName = "Bludgeoning"),
	Piercing    UMETA(DisplayName = "Piercing"),

	MAX         UMETA(Hidden)
};

/**
 * How a character's body reacts to one damage type.
 *
 * Resistant halves, Vulnerable doubles, Immune zeroes. When a character has
 * both Resistant and Vulnerable to the same type from different sources
 * (Equipment and Class disagreeing, say), they cancel to Normal rather than
 * stacking into a wash multiplier - matches the sourcebook's own rule that
 * two Resistances to the same type do not stack past half damage, extended
 * the same way to the opposite case.
 */
UENUM(BlueprintType)
enum class EDCCResistanceLevel : uint8
{
	Normal     UMETA(DisplayName = "Normal"),
	Resistant  UMETA(DisplayName = "Resistant"),
	Vulnerable UMETA(DisplayName = "Vulnerable"),
	Immune     UMETA(DisplayName = "Immune"),
};

/** One source's contribution to a character's resistance profile - an
 *  equipped item or a Class benefit granting Resistant/Vulnerable/Immune to
 *  one EDCCDamageType. UDCCStatsComponent::RecalculateDamageResistances
 *  merges a character's full list of these into the final per-type result. */
USTRUCT(BlueprintType)
struct FDCCResistanceSource
{
	GENERATED_BODY()

	UPROPERTY(BlueprintReadOnly, Category = "Resistance")
	FText Label;

	UPROPERTY(BlueprintReadOnly, Category = "Resistance")
	EDCCDamageType Type = EDCCDamageType::Physical;

	UPROPERTY(BlueprintReadOnly, Category = "Resistance")
	EDCCResistanceLevel Level = EDCCResistanceLevel::Normal;
};
