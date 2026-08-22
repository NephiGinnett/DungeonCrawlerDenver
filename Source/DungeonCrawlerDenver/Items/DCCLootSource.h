#pragma once

#include "CoreMinimal.h"
#include "UObject/Interface.h"
#include "DCCLootSource.generated.h"

struct FDCCRolledLoot;
class ADCCCharacterBase;

// ---------------------------------------------------------------------------
// SOMETHING YOU CAN OPEN THE LOOT MENU ON
//
// A corpse (ADCCCharacterBase, once dead) and a chest (ADCCLootContainer) are
// otherwise unrelated actors - one carries stats, skills and an AI, the other
// is a static prop with a mesh. They share exactly one thing: a body of
// rolled loot that can be checked, taken all at once, or stowed away. This
// interface is that one thing, factored out so DCCCharacterBase's interaction
// code can open the same menu on either without knowing which it is.
//
// Deliberately plain C++ virtuals rather than BlueprintNativeEvents - nothing
// here needs to be overridden from Blueprint, and a UFUNCTION would only add
// the reflection overhead of a dispatch this project never uses.
// ---------------------------------------------------------------------------

UINTERFACE(MinimalAPI)
class UDCCLootSource : public UInterface
{
	GENERATED_BODY()
};

class DUNGEONCRAWLERDENVER_API IDCCLootSource
{
	GENERATED_BODY()

public:
	/** Shown as the loot menu's title. */
	virtual FText GetLootSourceName() const = 0;

	/** Whether "Check Inventory" and "Loot All" have anything to do. */
	virtual bool HasAnyLoot() const = 0;

	/** The itemised list the "Check Inventory" submenu draws. */
	virtual const TArray<FDCCRolledLoot>& GetPendingLootItems() const = 0;

	/** Where spawned pickups scatter from. */
	virtual FVector GetLootOrigin() const = 0;

	/** Server only. Spawns Index's item and removes it from the list. */
	virtual void LootOne(int32 Index, ADCCCharacterBase* Looter) = 0;

	/** Server only. Spawns everything remaining and empties the list. */
	virtual void LootAll(ADCCCharacterBase* Looter) = 0;

	/**
	 * Server only. Hands everything remaining to Looter's inventory as one
	 * stowed record, then retires the source - however that source defines
	 * "retired" (a corpse fades on a timer, a chest just sits there empty).
	 */
	virtual void StowInto(ADCCCharacterBase* Looter) = 0;
};
