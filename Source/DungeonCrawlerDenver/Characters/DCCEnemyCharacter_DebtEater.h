#pragma once

#include "CoreMinimal.h"
#include "Characters/DCCEnemyCharacter.h"
#include "DCCEnemyCharacter_DebtEater.generated.h"

/**
 * Voldemortgage's collectors, from `Docs/DT_DCCEnemies.json`.
 *
 * "Two hits and gone, on purpose. Their threat is arithmetic, not damage" - the
 * stat block is intentionally weak (STR 2, 4 HP-ish at Level 1). What makes them
 * dangerous is `GoldStolenPerHit`, already implemented on the base class:
 * `ADCCEnemyCharacter::OnDamageDealt` bites gold out of whoever they hit and
 * feeds it to whichever character is flagged `bIsBoss` in the same encounter,
 * resolved lazily on the first successful bite. That is the whole of
 * "Repossession Nip" - no separate skill needed, the base class already does it.
 *
 * Every one still standing when Balloon Payment completes adds to the charge
 * damage - see `UDCCBossComponent::GetLivingMinionCount`. That is what turns a
 * two-hit mob into the fight's actual clock.
 */
UCLASS()
class DUNGEONCRAWLERDENVER_API ADCCEnemyCharacter_DebtEater : public ADCCEnemyCharacter
{
	GENERATED_BODY()

public:
	ADCCEnemyCharacter_DebtEater();
};
