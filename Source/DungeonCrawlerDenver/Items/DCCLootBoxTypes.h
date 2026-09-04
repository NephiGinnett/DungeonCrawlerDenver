#pragma once

#include "CoreMinimal.h"
#include "Engine/DataTable.h"
#include "Items/DCCLootTypes.h"
#include "Items/DCCConsumableTypes.h"
#include "DCCLootBoxTypes.generated.h"

// ---------------------------------------------------------------------------
// LOOT BOXES
//
// Dungeon Crawler Carl's actual mechanic: achievements pay out a sealed box
// rather than items directly, and the box is only opened later, in a Safe
// Room, all at once. Six tiers, ascending rarity - Bronze is mostly junk,
// Celestial is exceptionally rare and can carry build-altering gear.
//
// Deliberately built as a thin layer over what already exists rather than a
// parallel loot system:
//   - Item contents reuse FDCCLootDrop/DCCLoot::RollLootTable/SpawnRolledLoot
//     verbatim - a box's item pool is just another named loot table, the same
//     shape FDCCLootTableRow already gives room generation and chests.
//   - Boon contents reuse FDCCConsumableEffect verbatim - a boon IS a
//     consumable's "Drink Now" effect with no drink attached, applied through
//     the exact same ADCCCharacterBase::ApplyConsumableEffects a bought round
//     already goes through. No second buff engine.
// ---------------------------------------------------------------------------

UENUM(BlueprintType)
enum class EDCCLootBoxTier : uint8
{
	/** Not a reward - the zero/default state, same as ExperienceReward=0 meaning "no XP". */
	None      UMETA(DisplayName = "None"),

	Bronze    UMETA(DisplayName = "Bronze"),
	Silver    UMETA(DisplayName = "Silver"),
	Gold      UMETA(DisplayName = "Gold"),
	Platinum  UMETA(DisplayName = "Platinum"),
	Legendary UMETA(DisplayName = "Legendary"),
	Celestial UMETA(DisplayName = "Celestial"),

	MAX       UMETA(Hidden)
};

/**
 * A blessing a loot box can grant instead of an item.
 *
 * Its own DataTable so the same boon can appear in more than one tier's table
 * (a Bronze box and a Gold box can both list "Minor Fortitude" at different
 * odds) without copying the effect definition. Mechanically identical to a
 * consumable - see Effects - because DCC's own boons and drinks are the same
 * kind of thing: a temporary blessing, just with no bottle involved.
 */
USTRUCT(BlueprintType)
struct FDCCBoonDef : public FTableRowBase
{
	GENERATED_BODY()

	/** Stable identifier. Must match the row name. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Boon")
	FName BoonId;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Boon")
	FText DisplayName;

	/** Flavour, read out when the box grants it. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Boon")
	FText Description;

	/** What it actually does - applied via ADCCCharacterBase::ApplyConsumableEffects, same as a drink. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Boon")
	TArray<FDCCConsumableEffect> Effects;
};

/**
 * One boon a box's table can roll, by reference rather than by value.
 *
 * Mirrors FDCCLootDrop's own shape (a reference plus an independent roll
 * chance) so a box's item and boon lists read the same way at a glance.
 */
USTRUCT(BlueprintType)
struct FDCCBoonEntry
{
	GENERATED_BODY()

	/** A DT_DCCBoons row id. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Boon")
	FName BoonId;

	/** Probability this entry grants at all, 0-1. Rolled independently per entry, same as FDCCLootDrop::Chance. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Boon", meta = (ClampMin = "0.0", ClampMax = "1.0"))
	float Chance = 0.5f;
};

/**
 * One tier's contents, as a DataTable row - e.g. the "Bronze" row of
 * DT_DCCLootBoxTables.
 *
 * Two independent roll passes rather than one merged list: items and boons
 * are different enough downstream (one spawns a pickup to walk over to, the
 * other applies straight to Stats) that a caller needs to know which is
 * which, and rolling them separately means a tier's "how many boons" and
 * "how many items" are each tuned on their own terms rather than competing
 * for the same slots.
 */
USTRUCT(BlueprintType)
struct FDCCLootBoxTableRow : public FTableRowBase
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "LootBox")
	FText DisplayName;

	/** Same struct, same independent-roll-per-entry semantics as any other loot table in this project. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "LootBox")
	TArray<FDCCLootDrop> ItemEntries;

	/** Rolled independently from ItemEntries - see this struct's own comment. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "LootBox")
	TArray<FDCCBoonEntry> BoonEntries;
};

/**
 * What actually came out of a box, for the caller to announce and apply.
 *
 * UDCCInventoryComponent::TryOpenLootBox fills this in but does not act on
 * it - same split as ConsumeStowedConsumable, which resolves a drink's def
 * and leaves drinking it (touching Stats) to ADCCCharacterBase. A box is no
 * different: Inventory owns the roll, the character applies the result.
 */
USTRUCT(BlueprintType)
struct FDCCOpenedLootBoxResult
{
	GENERATED_BODY()

	UPROPERTY(BlueprintReadOnly, Category = "LootBox")
	TArray<FDCCRolledLoot> Items;

	UPROPERTY(BlueprintReadOnly, Category = "LootBox")
	TArray<FDCCBoonDef> Boons;
};
