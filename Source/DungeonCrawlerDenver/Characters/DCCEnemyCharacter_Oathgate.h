#pragma once

#include "CoreMinimal.h"
#include "Characters/DCCEnemyCharacter.h"
#include "DCCEnemyCharacter_Oathgate.generated.h"

// ---------------------------------------------------------------------------
// THE OATHGATE'S OWN ROSTER
//
// The gate is a magically-binding arbitration clause dressed up as a doorway -
// per the tone bible's own logic ("the dungeon repossessed these places, then
// made them legally distinct enough to avoid a lawsuit"), stepping through one
// counts as agreeing to terms nobody read. What comes out the other side is
// staffed accordingly: paperwork given legs.
//
// Each now wears its own model (changed 2026-08-30, see DISPATCH.md) - Serath
// (ParagonSerath, its own skeleton) for the Arbiter, Slime (MonsterForSurvivalGame,
// its own skeleton) for FinePrint. Both ship complete Walk/Run clips for their
// own skeleton, so this is a direct WalkAnimation/RunAnimation assignment,
// same pattern as DebtEater - NOT DCCCrawlerAnimSet::Apply, which is only
// valid on the shared crawler skeleton family and asserts
// (SkeletonToCompactPose.IsValidIndex) on anything else. Previously both
// reskinned onto shared crawler meshes (ArmsOut/DiceWinged) as a no-new-art
// placeholder for a testing-arena roster.
// ---------------------------------------------------------------------------

/**
 * Fast, weak, numerous. The swarm half of the gate's pool.
 *
 * "Two hits and gone" the same way a Debt Eater is, but without the gold-steal
 * mechanic - FinePrint's whole job is raw numbers arriving quickly, not a
 * secondary threat layered on top. A pool that is only tough single targets
 * reads as one fight repeated; this is what makes MinEnemies/MaxEnemies
 * actually feel like a crowd.
 */
UCLASS()
class DUNGEONCRAWLERDENVER_API ADCCEnemyCharacter_FinePrint : public ADCCEnemyCharacter
{
	GENERATED_BODY()

public:
	ADCCEnemyCharacter_FinePrint();
};

/**
 * The gate's tougher pull - not a boss, but the one that makes a party stop
 * and focus fire rather than just holding the swing button.
 *
 * Carries HouseRules, the same Intelligence debuff Voldemortgage uses ("opens
 * a crawler's file and makes them take more damage") - reused rather than
 * authoring a new skill, since an arbitrator finding against you is exactly
 * what that skill already models, and giving the gate's own mini-boss a
 * flavor of Voldemortgage's own authority is the point.
 */
UCLASS()
class DUNGEONCRAWLERDENVER_API ADCCEnemyCharacter_Arbiter : public ADCCEnemyCharacter
{
	GENERATED_BODY()

public:
	ADCCEnemyCharacter_Arbiter();
};
