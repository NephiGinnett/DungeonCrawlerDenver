#include "Characters/DCCEnemyCharacter_DebtEater.h"

#include "Skills/DCCSkillComponent.h"
#include "Stats/DCCStatsComponent.h"
#include "Components/SkeletalMeshComponent.h"
#include "Engine/SkeletalMesh.h"
#include "Animation/AnimSequence.h"
#include "NiagaraSystem.h"
#include "UObject/ConstructorHelpers.h"

ADCCEnemyCharacter_DebtEater::ADCCEnemyCharacter_DebtEater()
{
	DisplayName = NSLOCTEXT("DCC", "DebtEaterName", "Debt Eater");

	// The real wraith model, landed 2026-08-21 (SK_DebtEater) - no longer the
	// ArmsOut stand-in. Its own skeleton, not the shared crawler one, so this
	// no longer goes through DCCCrawlerAnimSet - see the anim note below.
	static ConstructorHelpers::FObjectFinder<USkeletalMesh> DebtEaterMesh(
		TEXT("/Game/DCC/Characters/DebtEater/SK_DebtEater.SK_DebtEater"));
	if (USkeletalMeshComponent* MeshComponent = GetMesh())
	{
		if (DebtEaterMesh.Succeeded())
		{
			MeshComponent->SetSkeletalMesh(DebtEaterMesh.Object);
		}

		MeshComponent->SetRelativeScale3D(FVector(0.85f, 0.85f, 0.85f));
	}

	if (Stats)
	{
		Stats->Level = 1;
		Stats->AbilityScores.SetScore(EDCCAbility::Strength, 2);
		Stats->AbilityScores.SetScore(EDCCAbility::Intelligence, 3);
		Stats->AbilityScores.SetScore(EDCCAbility::Constitution, 2);
		Stats->AbilityScores.SetScore(EDCCAbility::Dexterity, 5);
		Stats->AbilityScores.SetScore(EDCCAbility::Charisma, 2);
	}

	WeaponDamageDie = 4;
	AttackCooldownSeconds = 1.2f;
	AttackRange = 180.0f;
	AggroRadius = 1400.0f;

	// SK_DebtEater's imported Physics Asset ragdolls into a grotesquely
	// stretched mess (constraints fighting each other) rather than a
	// believable fall - reported directly by Zoe, who suggested a pile of
	// ash as the fallback if it couldn't be tuned. Tuning it means the
	// Physics Asset Editor, which needs the running editor and isn't reached
	// by any tool this project's MCP toolset enables, so this takes the
	// suggested fallback: no ragdoll at all, just a puff of smoke as the
	// dissolve beat. See ADCCCharacterBase::bUseRagdollOnDeath.
	bUseRagdollOnDeath = false;
	DeathDissolveEffect = TSoftObjectPtr<UNiagaraSystem>(
		FSoftObjectPath(TEXT("/Game/Alien_Portal/FX/Smoke/NS_Smoke_01.NS_Smoke_01")));

	// The whole of "Repossession Nip". OnDamageDealt on the base class already
	// steals gold on a successful hit and feeds it to whichever character in the
	// same encounter is flagged bIsBoss - see the class comment.
	GoldStolenPerHit = 2;
	SummonerHealPerGold = 1.0f;

	// LootTable is left at the base class default (55% / 3-12 gold): nothing in
	// the design notes says a Debt Eater should drop differently from any other
	// mob, and giving some of the stolen gold back on the kill is consistent
	// with every other enemy on the floor.

	// Its own skeleton, not the shared crawler one, so this is a direct clip
	// assignment rather than DCCCrawlerAnimSet::Apply - see the class comment
	// on DCCEnemyCharacter_Voldemortgage.cpp for why a shared-skeleton clip on
	// a mismatched skeleton hits the SkeletonToCompactPose.IsValidIndex ensure.
	// Only Walk/Run came out of this Meshy set - no idle, attack or hit-react
	// clip - so those stay null: an accepted cosmetic gap (idle pose instead
	// of a whiff/flinch) rather than a new failure mode.
	static ConstructorHelpers::FObjectFinder<UAnimSequence> WalkAnim(
		TEXT("/Game/DCC/Characters/DebtEater/Anims/A_DebtEater_Walking.A_DebtEater_Walking"));
	static ConstructorHelpers::FObjectFinder<UAnimSequence> RunAnim(
		TEXT("/Game/DCC/Characters/DebtEater/Anims/A_DebtEater_Running.A_DebtEater_Running"));

	if (WalkAnim.Succeeded())
	{
		WalkAnimation = WalkAnim.Object;
	}

	if (RunAnim.Succeeded())
	{
		RunAnimation = RunAnim.Object;
	}

	// No Blueprint exists for this class, so a C++ loadout is the real one.
	// Last Call is a cheap Charisma cone that slows whoever it catches - it gives
	// a swarm of these something to do other than close and bite, and slowing the
	// party is exactly what a minion should contribute to the boss's charge.
	if (Skills)
	{
		Skills->StartingSkillIds = { TEXT("LastCall") };
		Skills->StartingSkillRank = 2;
	}
}
