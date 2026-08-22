#pragma once

#include "CoreMinimal.h"
#include "Items/DCCPickup.h"
#include "DCCPickup_Equipment.generated.h"

class UDataTable;

/**
 * A piece of DT_DCCEquipment lying on the floor.
 *
 * Every other pickup in Items/DCCPickupVariants.h is one C++ class per mesh,
 * which works because each represents exactly one thing. Equipment already
 * has its own DataTable row per item - a second copy of that authoring, one
 * subclass per sword, would just be the same data typed twice. Instead this
 * is ONE class that resolves whichever row EquipmentItemId names at
 * BeginPlay and wears that row's mesh, so the whole equipment table becomes
 * placeable without any C++ growing alongside it.
 *
 * The actual "equip on pickup" behaviour lives on the base class
 * (ADCCPickup::EquipmentItemId / OnPickedUp) rather than here, because
 * nothing about consuming the pickup is specific to how it looks - this
 * class's only job is turning an item id into a mesh anyone can see in the
 * world before they have picked it up at all.
 */
UCLASS()
class DUNGEONCRAWLERDENVER_API ADCCPickup_Equipment : public ADCCPickup
{
	GENERATED_BODY()

public:
	ADCCPickup_Equipment();

protected:
	virtual void BeginPlay() override;

private:
	/** Cached in the constructor via ConstructorHelpers - BeginPlay cannot use FObjectFinder itself. */
	UPROPERTY()
	TObjectPtr<UDataTable> EquipmentTable;
};
