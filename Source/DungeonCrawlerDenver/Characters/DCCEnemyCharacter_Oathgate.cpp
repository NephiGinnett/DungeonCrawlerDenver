#include "Characters/DCCEnemyCharacter_Oathgate.h"

#include "Characters/DCCCrawlerAnimSet.h"
#include "Skills/DCCSkillComponent.h"
#include "Stats/DCCStatsComponent.h"
#include "Components/SkeletalMeshComponent.h"
#include "Engine/SkeletalMesh.h"
#include "UObject/ConstructorHelpers.h"

ADCCEnemyCharacter_FinePrint::ADCCEnemyCharacter_FinePrint()
{
	DisplayName = NSLOCTEXT("DCC", "FinePrintName", "Fine Print");

	// Shared-skeleton reskin, scaled down - a swarm should read as small next
	// to whatever else the gate pulls, the same visual shorthand the Nuclear
	// Family's Kid uses for "less of a threat, more of them."
	static ConstructorHelpers::FObjectFinder<USkeletalMesh> FinePrintMesh(
		TEXT("/Game/DCC/Characters/ArmsOut/SK_ArmsOut_Shared.SK_ArmsOut_Shared"));
	if (USkeletalMeshComponent* MeshComponent = GetMesh())
	{
		if (FinePrintMesh.Succeeded())
		{
			MeshComponent->SetSkeletalMesh(FinePrintMesh.Object);
		}

		MeshComponent->SetRelativeScale3D(FVector(0.72f, 0.72f, 0.72f));
	}

	if (Stats)
	{
		Stats->Level = 1;
		Stats->AbilityScores.SetScore(EDCCAbility::Strength, 2);
		Stats->AbilityScores.SetScore(EDCCAbility::Intelligence, 2);
		Stats->AbilityScores.SetScore(EDCCAbility::Constitution, 2);
		Stats->AbilityScores.SetScore(EDCCAbility::Dexterity, 6);
		Stats->AbilityScores.SetScore(EDCCAbility::Charisma, 3);
	}

	// Fast and weak: the whole point is arriving in numbers quickly rather
	// than posing a threat on its own. No skill, no gold-steal - just volume.
	WeaponDamageDie = 4;
	AttackCooldownSeconds = 0.9f;
	AttackRange = 170.0f;
	AggroRadius = 1600.0f;

	// Shared skeleton, so the consolidated animation set applies directly.
	DCCCrawlerAnimSet::Apply(*this, /*EmoteChanceOnKill=*/0.3f);
}

ADCCEnemyCharacter_Arbiter::ADCCEnemyCharacter_Arbiter()
{
	DisplayName = NSLOCTEXT("DCC", "ArbiterName", "The Arbiter");

	static ConstructorHelpers::FObjectFinder<USkeletalMesh> ArbiterMesh(
		TEXT("/Game/DCC/Characters/DiceWinged/SK_DiceWinged_Shared.SK_DiceWinged_Shared"));
	if (USkeletalMeshComponent* MeshComponent = GetMesh())
	{
		if (ArbiterMesh.Succeeded())
		{
			MeshComponent->SetSkeletalMesh(ArbiterMesh.Object);
		}

		// Reads as the one worth focusing, standing over the FinePrint swarm.
		MeshComponent->SetRelativeScale3D(FVector(1.15f, 1.15f, 1.15f));
	}

	if (Stats)
	{
		Stats->Level = 2;
		Stats->AbilityScores.SetScore(EDCCAbility::Strength, 4);
		Stats->AbilityScores.SetScore(EDCCAbility::Intelligence, 6);
		Stats->AbilityScores.SetScore(EDCCAbility::Constitution, 6);
		Stats->AbilityScores.SetScore(EDCCAbility::Dexterity, 3);
		Stats->AbilityScores.SetScore(EDCCAbility::Charisma, 5);
	}

	WeaponDamageDie = 8;
	AttackCooldownSeconds = 2.2f;
	AttackRange = 220.0f;
	AggroRadius = 1800.0f;

	// HouseRules rather than a new skill - Voldemortgage's own Intelligence
	// debuff already IS "an arbitrator finding against you," so the gate's
	// mini-boss borrowing his authority is the point, not a shortcut.
	if (Skills)
	{
		Skills->StartingSkillIds = { TEXT("HouseRules") };
		Skills->StartingSkillRank = 2;
	}

	DCCCrawlerAnimSet::Apply(*this, /*EmoteChanceOnKill=*/0.5f);
}
