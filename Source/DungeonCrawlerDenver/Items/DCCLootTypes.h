#pragma once

#include "CoreMinimal.h"
#include "Templates/SubclassOf.h"
#include "Engine/DataTable.h"
#include "DCCLootTypes.generated.h"

class ADCCPickup;
class ADCCCharacterBase;

// ---------------------------------------------------------------------------
// WHAT A CORPSE LEAVES BEHIND
//
// Drops are spawned as real ADCCPickup actors on the floor rather than credited
// straight into the killer's purse. That costs a few actors per fight and buys
// three things worth more than the saving:
//
//   - Loot has to be *walked to*, which pulls the party out of position after a
//     fight and gives the next wave somewhere to arrive.
//   - It reuses the carry/throw loop that already exists instead of adding a
//     second, invisible currency path with its own rules.
//   - Gold on the floor is gold anybody can grab, so who picks it up is a table
//     conversation rather than an automatic credit to whoever landed the hit.
// ---------------------------------------------------------------------------

/**
 * A drop that has already been rolled and is waiting on a body.
 *
 * The roll happens at the moment of death so the result is fixed and
 * server-authoritative - looting the same corpse twice, or two players racing to
 * loot it, cannot re-roll for a better outcome. What is on the body was decided
 * when it hit the floor; looting only moves it.
 */
USTRUCT(BlueprintType)
struct FDCCRolledLoot
{
	GENERATED_BODY()

	UPROPERTY(BlueprintReadOnly, Category = "Loot")
	TSubclassOf<ADCCPickup> PickupClass;

	/** Resolved from the drop's gold range at death. 0 means the class default. */
	UPROPERTY(BlueprintReadOnly, Category = "Loot")
	int32 Gold = 0;

	/**
	 * Forwarded from the drop's own EquipmentItemId, so a rolled "bow" or "gun"
	 * still knows which DT_DCCEquipment row it is once the roll has happened.
	 *
	 * This was missing for a while - FDCCLootDrop::EquipmentItemId documented
	 * itself as forwarded onto the spawned pickup, but nothing between the roll
	 * and the spawn actually carried it, so an equipment drop on any loot table
	 * silently produced an unequippable prop. Fixed alongside the loot menu
	 * rework because chest gear is the first content that would have made the
	 * gap obvious.
	 */
	UPROPERTY(BlueprintReadOnly, Category = "Loot")
	FName EquipmentItemId;
};

/** One possible drop. A loot table is an array of these, each rolled separately. */
USTRUCT(BlueprintType)
struct FDCCLootDrop
{
	GENERATED_BODY()

	/**
	 * What to spawn. Usually a coin for gold, but any pickup works - a Debt Eater
	 * dropping the crate it was carrying is the same mechanism.
	 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Loot")
	TSubclassOf<ADCCPickup> PickupClass;

	/**
	 * Probability this entry drops at all, 0-1. Rolled independently per entry,
	 * so a table can hand out a guaranteed coin (1.0) and a rare item (0.05)
	 * without the two competing for the same roll.
	 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Loot", meta = (ClampMin = "0.0", ClampMax = "1.0"))
	float Chance = 0.5f;

	/**
	 * Gold written onto the spawned pickup, rolled inclusively between these.
	 *
	 * Overrides whatever the class defaults to, so one coin class covers every
	 * denomination on the floor and the enemy decides what it is worth. Leave
	 * both at zero to drop an item whose value is whatever its class says.
	 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Loot", meta = (ClampMin = "0"))
	int32 MinGold = 0;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Loot", meta = (ClampMin = "0"))
	int32 MaxGold = 0;

	/** How far from the corpse it can land, so a multi-drop does not stack in one spot. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Loot", meta = (ClampMin = "0.0"))
	float ScatterRadius = 90.0f;

	/**
	 * A DT_DCCEquipment row id, forwarded onto the spawned pickup's own
	 * EquipmentItemId. Only meaningful when PickupClass is (or derives from)
	 * ADCCPickup_Equipment - left None, an equipment pickup spawns with no
	 * mesh and does nothing on pickup, same as leaving any other field unset.
	 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Loot")
	FName EquipmentItemId;
};

// ---------------------------------------------------------------------------
// NAMED TABLES, FOR ANYTHING THAT ISN'T A CORPSE
//
// ADCCCharacterBase::LootTable is a per-character array, authored once per
// Blueprint - the right shape for "what does this specific mob drop." A
// procedurally generated Loot Vault room has no Blueprint to author one on,
// and shouldn't need a new C++ class every time someone wants a new tier of
// reward - it just needs to name a table and roll it. FDCCLootTableRow is
// that: the exact same FDCCLootDrop entries, addressable by row name from a
// DataTable so any system (room generation today, a treasure chest or a
// vendor tomorrow) can pull from a shared, editable pool instead of carrying
// its own copy.
// ---------------------------------------------------------------------------

USTRUCT(BlueprintType)
struct FDCCLootTableRow : public FTableRowBase
{
	GENERATED_BODY()

	/** Shown in editor tooling only - the row name is what code actually asks for. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Loot")
	FText DisplayName;

	/** Same struct, same independent-roll-per-entry semantics as a character's LootTable. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Loot")
	TArray<FDCCLootDrop> Entries;
};

// ---------------------------------------------------------------------------
// SHARED ROLLING AND SPAWNING
//
// Both a corpse (ADCCCharacterBase) and a chest (ADCCLootContainer) roll a
// TArray<FDCCLootDrop> into a TArray<FDCCRolledLoot> and, later, spawn each
// rolled entry as a real pickup near themselves. Free functions rather than a
// method on either class, so the one place this logic lives is not owned by
// whichever class happened to need it first - a vendor or a room-generator
// table can use the exact same two calls without inheriting from anything.
// ---------------------------------------------------------------------------

namespace DCCLoot
{
	/** Rolls Table into OutRolled, one independent roll per entry. Server-side only. */
	DUNGEONCRAWLERDENVER_API void RollLootTable(const TArray<FDCCLootDrop>& Table, TArray<FDCCRolledLoot>& OutRolled);

	/**
	 * Spawns Rolled as a real pickup, scattered lightly around Origin.
	 *
	 * Server-side only - returns null without authority. Applies the rolled gold
	 * and equipment id onto the spawned actor, since neither is guaranteed to
	 * match the pickup class's own defaults.
	 *
	 * Looter is optional and changes gold specifically, not items. A floor drop
	 * from a kill has no single claimant - the header comment above explains why
	 * that is deliberate - but every caller of this function that goes through
	 * the corpse/chest loot MENU (LootOne, LootAll, and a Stowed Remains record
	 * being emptied later) already knows exactly who chose to take it. When
	 * Looter is set and the rolled class is ordinary "pocket change"
	 * (PickupClass's own bBanksOnPickup - the same flag ADCCPickup::OnPickedUp
	 * already uses to decide instant-bank vs must-carry), the gold is credited
	 * straight to that looter instead of also being left on the ground to be
	 * separately walked over. That second walk-over step is exactly what read as
	 * "Stow doesn't add to inventory" / "gold pops out onto the ground instead of
	 * being added" - both were the same root cause: this function never had any
	 * path that credited gold directly, only ever spawning a coin. See
	 * DISPATCH.md, "Stow and gold loot" (2026-08-29). A rolled entry that also
	 * carries an EquipmentItemId still spawns a real pickup for the item half
	 * even when its gold half was credited directly.
	 */
	DUNGEONCRAWLERDENVER_API ADCCPickup* SpawnRolledLoot(UWorld* World, const FDCCRolledLoot& Rolled, const FVector& Origin, ADCCCharacterBase* Looter = nullptr);
}
