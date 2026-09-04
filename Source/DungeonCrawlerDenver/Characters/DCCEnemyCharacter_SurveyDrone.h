#pragma once

#include "CoreMinimal.h"
#include "Characters/DCCEnemyCharacter.h"
#include "DCCEnemyCharacter_SurveyDrone.generated.h"

/**
 * A corporate survey drone, deep inside the Front Range workings.
 *
 * These are not guards and are not looking for a fight. They are doing a job -
 * mapping a deposit somebody else has a claim on - and they will keep doing it
 * with a party of armed crawlers standing right next to them, because nothing
 * in their tasking says otherwise.
 *
 * WHY THIS CLASS IS ALMOST EMPTY
 * Everything the behaviour needs already existed on ADCCEnemyCharacter:
 * `bPassiveUntilProvoked` (never auto-acquires a target - the only route to a
 * CurrentTarget is HandleNearbyAttack), `ViolenceWatchRadius`, and
 * `PatrolPoints`/`PatrolWaitSeconds` for the circling. So this is a mesh, a
 * stat block and three flags rather than a new AI - which is the correct
 * outcome, and worth stating so nobody later assumes a drone-specific system
 * exists and goes looking for it.
 *
 * PatrolPoints are deliberately NOT set here. They are world-space and belong
 * to a placement, not to a class; each drone gets its own circuit when placed.
 *
 * The drones matter narratively rather than mechanically: they are the answer
 * to the lights the Surveyor keeps seeing on the mountain at night, and they
 * are proof the corporations were already inside long before anyone argued
 * about who had the paperwork to be.
 */
UCLASS()
class DUNGEONCRAWLERDENVER_API ADCCEnemyCharacter_SurveyDrone : public ADCCEnemyCharacter
{
	GENERATED_BODY()

public:
	ADCCEnemyCharacter_SurveyDrone();
};
