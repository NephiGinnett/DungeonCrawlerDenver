#pragma once

#include "CoreMinimal.h"
#include "Characters/DCCEnemyCharacter.h"
#include "DCCEnemyCharacter_Wildlife.generated.h"

// ---------------------------------------------------------------------------
// THE MAP'S OWN WILDLIFE
//
// Five models from MonsterForSurvivalGame, sitting fully animated and
// completely unused until now - each with its own skeleton, so every one of
// these is a direct WalkAnimation/RunAnimation assignment rather than
// DCCCrawlerAnimSet::Apply, same reasoning as DebtEater and the Oathgate's
// own roster (see that class's header comment on the
// SkeletonToCompactPose.IsValidIndex ensure a shared-skeleton clip hits on a
// mismatched skeleton). No new skills - variety comes from stat shape,
// range, and cooldown, the same lever FinePrint/Arbiter already use against
// each other.
//
// Named in the same corporate-satire register as everything else on this
// floor rather than left as generic fantasy monster names - a mimic loot
// container reads as content here, not as a reused asset.
// ---------------------------------------------------------------------------

/**
 * The Auditor (Beholder model). Many eyes, one job: watching from range.
 * The roster's ranged threat - AttackRange stands in for reach the same way
 * a MainHand weapon's AttackRangeOverride does for a player, just authored
 * directly since wildlife carries no equipment.
 */
UCLASS()
class DUNGEONCRAWLERDENVER_API ADCCEnemyCharacter_Beholder : public ADCCEnemyCharacter
{
	GENERATED_BODY()

public:
	ADCCEnemyCharacter_Beholder();
};

/**
 * Thornback (Cactus model). Slow, defensive, hits like a filing cabinet
 * falling on you. High Constitution, low Dexterity - the "corner you don't
 * want to fight in" archetype.
 */
UCLASS()
class DUNGEONCRAWLERDENVER_API ADCCEnemyCharacter_Thornback : public ADCCEnemyCharacter
{
	GENERATED_BODY()

public:
	ADCCEnemyCharacter_Thornback();
};

/**
 * Unclaimed Property (ChestMonster model). A mimic, which this loot-table-
 * driven project can make into an actual joke rather than a generic trope -
 * the name matches HeavyLifting's own "Unclaimed Property" quest exactly on
 * purpose. Short aggro radius (it waits to be approached, the way a real
 * chest would) and the hardest single hit on the roster.
 */
UCLASS()
class DUNGEONCRAWLERDENVER_API ADCCEnemyCharacter_UnclaimedProperty : public ADCCEnemyCharacter
{
	GENERATED_BODY()

public:
	ADCCEnemyCharacter_UnclaimedProperty();
};

/**
 * Break Room Mold (Mushroom model, AngryVersion animset - the pack also
 * ships a friendlier SmileVersion, deliberately not used here). Weak and
 * numerous by design, same "arrives in volume, not as a threat on its own"
 * role FinePrint already plays at the Oathgate - this is that shape, loose
 * on the main map instead.
 */
UCLASS()
class DUNGEONCRAWLERDENVER_API ADCCEnemyCharacter_BreakRoomMold : public ADCCEnemyCharacter
{
	GENERATED_BODY()

public:
	ADCCEnemyCharacter_BreakRoomMold();
};

/**
 * Shell Company (TurtleShell model - the pun was sitting right there). The
 * roster's heaviest tank: high Constitution, slow cooldown, built to be the
 * one a party has to commit to rather than the one that ends a fight fast.
 */
UCLASS()
class DUNGEONCRAWLERDENVER_API ADCCEnemyCharacter_ShellCompany : public ADCCEnemyCharacter
{
	GENERATED_BODY()

public:
	ADCCEnemyCharacter_ShellCompany();
};
