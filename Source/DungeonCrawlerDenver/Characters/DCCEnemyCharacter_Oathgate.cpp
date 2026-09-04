#include "Characters/DCCEnemyCharacter_Oathgate.h"

#include "Skills/DCCSkillComponent.h"
#include "Stats/DCCStatsComponent.h"
#include "Components/SkeletalMeshComponent.h"
#include "Engine/SkeletalMesh.h"
#include "Animation/AnimSequence.h"
#include "UObject/ConstructorHelpers.h"

ADCCEnemyCharacter_FinePrint::ADCCEnemyCharacter_FinePrint()
{
	DisplayName = NSLOCTEXT("DCC", "FinePrintName", "Fine Print");

	// Its own model and skeleton now - small and numerous reads correctly on
	// a genuine slime rather than a scaled-down ally. See this class's own
	// header comment for why WalkAnimation/RunAnimation replace
	// DCCCrawlerAnimSet::Apply below.
	static ConstructorHelpers::FObjectFinder<USkeletalMesh> FinePrintMesh(
		TEXT("/Game/MonsterForSurvivalGame/Mesh/PBR/Slime_SK.Slime_SK"));
	if (USkeletalMeshComponent* MeshComponent = GetMesh())
	{
		if (FinePrintMesh.Succeeded())
		{
			MeshComponent->SetSkeletalMesh(FinePrintMesh.Object);
		}
	}

	static ConstructorHelpers::FObjectFinder<UAnimSequence> FinePrintWalkAnim(
		TEXT("/Game/MonsterForSurvivalGame/Animation/PBR/Slime/Slime_Walk_ANIM.Slime_Walk_ANIM"));
	static ConstructorHelpers::FObjectFinder<UAnimSequence> FinePrintRunAnim(
		TEXT("/Game/MonsterForSurvivalGame/Animation/PBR/Slime/Slime_Run_ANIM.Slime_Run_ANIM"));

	if (FinePrintWalkAnim.Succeeded())
	{
		WalkAnimation = FinePrintWalkAnim.Object;
	}

	if (FinePrintRunAnim.Succeeded())
	{
		RunAnimation = FinePrintRunAnim.Object;
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

	// No emote-on-kill clip - Slime's pack has no equivalent gesture, an
	// accepted cosmetic gap the same way DebtEater has no idle/attack/hit
	// clip. EmoteChanceOnKill stays at its zero default.
}

ADCCEnemyCharacter_Arbiter::ADCCEnemyCharacter_Arbiter()
{
	DisplayName = NSLOCTEXT("DCC", "ArbiterName", "The Arbiter");

	static ConstructorHelpers::FObjectFinder<USkeletalMesh> ArbiterMesh(
		TEXT("/Game/ParagonSerath/Characters/Heroes/Serath/Meshes/Serath.Serath"));
	if (USkeletalMeshComponent* MeshComponent = GetMesh())
	{
		if (ArbiterMesh.Succeeded())
		{
			MeshComponent->SetSkeletalMesh(ArbiterMesh.Object);
		}
	}

	static ConstructorHelpers::FObjectFinder<UAnimSequence> ArbiterWalkAnim(
		TEXT("/Game/ParagonSerath/Characters/Heroes/Serath/Animations/Jog_Fwd.Jog_Fwd"));
	static ConstructorHelpers::FObjectFinder<UAnimSequence> ArbiterRunAnim(
		TEXT("/Game/ParagonSerath/Characters/Heroes/Serath/Animations/Sprint_Fwd.Sprint_Fwd"));

	if (ArbiterWalkAnim.Succeeded())
	{
		WalkAnimation = ArbiterWalkAnim.Object;
	}

	if (ArbiterRunAnim.Succeeded())
	{
		RunAnimation = ArbiterRunAnim.Object;
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

	// No emote-on-kill clip - same accepted gap as FinePrint above.
}
