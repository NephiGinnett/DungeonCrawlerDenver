#pragma once

#include "CoreMinimal.h"
#include "Dialogue/DCCVendorNPC.h"
#include "DCCHubVendors.generated.h"

/**
 * The two shopkeepers in Concourse Zero (Rooms/DCCHub). Stock is set in the
 * constructor rather than on a placed instance: the hub is built at runtime,
 * and a spawned actor's per-instance arrays never reach clients, while a class
 * default is the same on every machine with nothing to replicate.
 *
 * Item ids match Tools/gen_loot_content.py (SKILL_GEAR, CONSUMABLES). An id the
 * tables do not have yet is simply not shown, so this is safe before the import.
 */

/** Duty Free (Duty Mandatory): weapons for every Attack Skill, and armour. */
UCLASS()
class DUNGEONCRAWLERDENVER_API ADCCVendor_DutyFree : public ADCCVendorNPC
{
	GENERATED_BODY()
public:
	ADCCVendor_DutyFree();
};

/** Pharmacy & Pyrotechnics: explosives for the three handling skills, and First Aid. */
UCLASS()
class DUNGEONCRAWLERDENVER_API ADCCVendor_Pyrotechnics : public ADCCVendorNPC
{
	GENERATED_BODY()
public:
	ADCCVendor_Pyrotechnics();
};
