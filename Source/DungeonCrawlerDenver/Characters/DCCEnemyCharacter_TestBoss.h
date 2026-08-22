#pragma once

#include "CoreMinimal.h"
#include "Characters/DCCEnemyCharacter.h"
#include "DCCEnemyCharacter_TestBoss.generated.h"

class UDCCBossComponent;

/**
 * A stand-in for Voldemortgage, so the boss-locked exit can be played through
 * end to end before his model exists.
 *
 * Everything the exit cares about - `ADCCFloorExit` watches for any living
 * actor carrying a `UDCCBossComponent` - is present here. Reskinned onto
 * `SK_HockeyWings` rather than left as the default capsule, so the fight is
 * legible while placed by hand rather than through an encounter wave.
 *
 * Voldemortgage now has a real placed encounter (see `ADCCEnemyCharacter_
 * Voldemortgage` and `DCCEncounter_Voldemortgage`), so this class is no longer
 * strictly needed for that original purpose - but `DCCHUD`'s admin/debug spawn
 * panel now references it directly for quick testing, so it is no longer safe
 * to delete without also updating that panel.
 */
UCLASS()
class DUNGEONCRAWLERDENVER_API ADCCEnemyCharacter_TestBoss : public ADCCEnemyCharacter
{
	GENERATED_BODY()

public:
	ADCCEnemyCharacter_TestBoss();

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "DCC")
	TObjectPtr<UDCCBossComponent> Boss;
};
