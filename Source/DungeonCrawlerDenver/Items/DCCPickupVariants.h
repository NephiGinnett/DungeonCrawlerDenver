#pragma once

#include "CoreMinimal.h"
#include "Items/DCCPickup.h"
#include "DCCPickupVariants.generated.h"

// ---------------------------------------------------------------------------
// THE PANTRY'S PROP SET
//
// Six pickups spanning the three mass tiers, so Strength is legible without any
// UI explaining it. The tiers are chosen against the carry rule - limit is
// Strength SCORE x 15kg - so on the Standard Array (2,3,4,5,6):
//
//   STR 2 -> 30kg   STR 3 -> 45kg   STR 4 -> 60kg   STR 5 -> 75kg   STR 6 -> 90kg
//
// which makes the Heavy tier at 75kg exactly the STR 5+ line the floor design
// asks for, and leaves Kyle (STR 2) able to lift bottles and nothing else.
//
// These are C++ subclasses rather than Blueprints on purpose. Blueprint defaults
// cannot currently be set through MCP at all - ObjectTools.set_properties returns
// false and writes nothing - so a BP_Pickup_* would have to be hand-configured
// prop by prop in the editor. A constructor is both automatable and reviewable in
// a diff. Zoe can still subclass any of these in Blueprint to reskin one.
//
// Meshes are all measured human-scale (see the bounds check in CLAUDE.md's
// gotchas - Ice_Station's "large crate" is 5.8 METRES and was rejected for this).
// ---------------------------------------------------------------------------

/**
 * Loose gold, the default enemy drop.
 *
 * Weightless-ish and banks the instant it is touched, because the interesting
 * decision about pocket change is whether it is worth walking over for while
 * something is still shooting at you — not whether you can lift it.
 *
 * `GoldValue` is normally overwritten per-drop by the loot table's gold range,
 * so this one class covers every denomination on the floor.
 */
UCLASS()
class DUNGEONCRAWLERDENVER_API ADCCPickup_Coins : public ADCCPickup
{
	GENERATED_BODY()

public:
	ADCCPickup_Coins();
};

/** Light: 3kg. Anyone can lift it, including a Strength 2 crawler. */
UCLASS()
class DUNGEONCRAWLERDENVER_API ADCCPickup_Bottle : public ADCCPickup
{
	GENERATED_BODY()

public:
	ADCCPickup_Bottle();
};

/** Light: 8kg. The other end of the "anyone" band. */
UCLASS()
class DUNGEONCRAWLERDENVER_API ADCCPickup_EggBox : public ADCCPickup
{
	GENERATED_BODY()

public:
	ADCCPickup_EggBox();
};

/** Medium: 40kg. Needs Strength 3+. The first prop that excludes somebody. */
UCLASS()
class DUNGEONCRAWLERDENVER_API ADCCPickup_Crate : public ADCCPickup
{
	GENERATED_BODY()

public:
	ADCCPickup_Crate();
};

/** Medium: 35kg. Sits just under the crate so STR 3 gets a choice, not a rule. */
UCLASS()
class DUNGEONCRAWLERDENVER_API ADCCPickup_GreaseBarrel : public ADCCPickup
{
	GENERATED_BODY()

public:
	ADCCPickup_GreaseBarrel();
};

/** Heavy: 70kg. Strength 5+. */
UCLASS()
class DUNGEONCRAWLERDENVER_API ADCCPickup_Keg : public ADCCPickup
{
	GENERATED_BODY()

public:
	ADCCPickup_Keg();
};

/**
 * Heavy: 75kg. Strength 5+, and the exact boundary case - a STR 5 crawler can
 * lift this and nothing heavier, which is the tier line made physical.
 *
 * This is the floor design's "heaviest item is the most valuable and cannot be
 * carried into a fight" prop. The value half is not modelled yet; the weight
 * half already does the work, because carrying it means both hands are full.
 */
UCLASS()
class DUNGEONCRAWLERDENVER_API ADCCPickup_StrongboxCrate : public ADCCPickup
{
	GENERATED_BODY()

public:
	ADCCPickup_StrongboxCrate();
};

/**
 * Medium-Heavy: 50kg, Strength 4+. Not part of the mass-tier lesson - it exists
 * for the same reason the Meat Tenderiser and Name Tag Reading DAVE do: the
 * Pantry is funnier with a jukebox in it than without one.
 */
UCLASS()
class DUNGEONCRAWLERDENVER_API ADCCPickup_Jukebox : public ADCCPickup
{
	GENERATED_BODY()

public:
	ADCCPickup_Jukebox();
};

// Generic ground-representation equipment pickups already exist as
// ADCCPickup_Equipment in Items/DCCPickup_Equipment.h - resolves its mesh
// from DT_DCCEquipment in BeginPlay, exactly the shape a chest or a corpse
// drop needs. Use that class rather than adding a second one here.
