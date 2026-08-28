#pragma once

#include "CoreMinimal.h"
#include "Characters/DCCEnemyCharacter.h"
#include "DCCEnemyCharacter_Guard.generated.h"

/**
 * A passive security guard - the Bar's "buff minion guard" Zoe described:
 * there for appearances, patrols, and only fights if a fight actually breaks
 * out near it. Not an ally (bIsAlly stays false - it does not escort or
 * follow anyone) and not a normal mob either (bPassiveUntilProvoked is true,
 * so it never auto-aggroes on sight) - see ADCCEnemyCharacter's own comment
 * on that flag for the whole mechanism.
 *
 * PatrolPoints is empty by default and has to be authored per placed
 * instance (world-space points, walked in a loop) - there is no patrol-path
 * generator here, the same reason encounter waves are hand-placed rather
 * than procedural.
 *
 * SK_BuffMinion has no imported animations yet (its walk/run clips are
 * converted and staged in SourceArt/BuffMinion/Anims/, waiting on an MCP
 * import pass - see Docs/DISPATCH.md) - accepted as a temporary cosmetic gap,
 * same as ADCCEnemyCharacter_DebtEater shipped mesh-only before its own
 * animations existed.
 */
UCLASS()
class DUNGEONCRAWLERDENVER_API ADCCEnemyCharacter_Guard : public ADCCEnemyCharacter
{
	GENERATED_BODY()

public:
	ADCCEnemyCharacter_Guard();
};
