#include "Characters/DCCEnemyCharacter_Wildlife.h"

#include "Stats/DCCStatsComponent.h"
#include "Components/SkeletalMeshComponent.h"
#include "Engine/SkeletalMesh.h"
#include "Animation/AnimSequence.h"
#include "UObject/ConstructorHelpers.h"

ADCCEnemyCharacter_Beholder::ADCCEnemyCharacter_Beholder()
{
	DisplayName = NSLOCTEXT("DCC", "BeholderName", "The Auditor");

	static ConstructorHelpers::FObjectFinder<USkeletalMesh> BeholderMesh(
		TEXT("/Game/MonsterForSurvivalGame/Mesh/PBR/Beholder_SK.Beholder_SK"));
	if (USkeletalMeshComponent* MeshComponent = GetMesh())
	{
		if (BeholderMesh.Succeeded())
		{
			MeshComponent->SetSkeletalMesh(BeholderMesh.Object);
		}
	}

	static ConstructorHelpers::FObjectFinder<UAnimSequence> WalkAnim(
		TEXT("/Game/MonsterForSurvivalGame/Animation/PBR/Beholder/Beholder_WalkFWD_ANIM.Beholder_WalkFWD_ANIM"));
	static ConstructorHelpers::FObjectFinder<UAnimSequence> RunAnim(
		TEXT("/Game/MonsterForSurvivalGame/Animation/PBR/Beholder/Beholder_Run_ANIM.Beholder_Run_ANIM"));

	if (WalkAnim.Succeeded()) { WalkAnimation = WalkAnim.Object; }
	if (RunAnim.Succeeded()) { RunAnimation = RunAnim.Object; }

	if (Stats)
	{
		Stats->Level = 2;
		Stats->AbilityScores.SetScore(EDCCAbility::Strength, 2);
		Stats->AbilityScores.SetScore(EDCCAbility::Intelligence, 6);
		Stats->AbilityScores.SetScore(EDCCAbility::Constitution, 3);
		Stats->AbilityScores.SetScore(EDCCAbility::Dexterity, 4);
		Stats->AbilityScores.SetScore(EDCCAbility::Charisma, 3);
	}

	// The roster's ranged threat - stands off and picks, rather than closing
	// like everything else. AttackRange plays the part a weapon's
	// AttackRangeOverride does for a player; wildlife has no equipment to
	// carry that number instead.
	WeaponDamageDie = 6;
	AttackCooldownSeconds = 1.8f;
	AttackRange = 900.0f;
	AggroRadius = 2000.0f;
}

ADCCEnemyCharacter_Thornback::ADCCEnemyCharacter_Thornback()
{
	DisplayName = NSLOCTEXT("DCC", "ThornbackName", "Thornback");

	static ConstructorHelpers::FObjectFinder<USkeletalMesh> CactusMesh(
		TEXT("/Game/MonsterForSurvivalGame/Mesh/PBR/Cactus_SK.Cactus_SK"));
	if (USkeletalMeshComponent* MeshComponent = GetMesh())
	{
		if (CactusMesh.Succeeded())
		{
			MeshComponent->SetSkeletalMesh(CactusMesh.Object);
		}
	}

	static ConstructorHelpers::FObjectFinder<UAnimSequence> WalkAnim(
		TEXT("/Game/MonsterForSurvivalGame/Animation/PBR/Cactus/Cactus_WalkFWD_ANIM.Cactus_WalkFWD_ANIM"));
	static ConstructorHelpers::FObjectFinder<UAnimSequence> RunAnim(
		TEXT("/Game/MonsterForSurvivalGame/Animation/PBR/Cactus/Cactus_RunFWD_ANIM.Cactus_RunFWD_ANIM"));

	if (WalkAnim.Succeeded()) { WalkAnimation = WalkAnim.Object; }
	if (RunAnim.Succeeded()) { RunAnimation = RunAnim.Object; }

	if (Stats)
	{
		Stats->Level = 2;
		Stats->AbilityScores.SetScore(EDCCAbility::Strength, 4);
		Stats->AbilityScores.SetScore(EDCCAbility::Intelligence, 2);
		Stats->AbilityScores.SetScore(EDCCAbility::Constitution, 6);
		Stats->AbilityScores.SetScore(EDCCAbility::Dexterity, 2);
		Stats->AbilityScores.SetScore(EDCCAbility::Charisma, 2);
	}

	// Slow and defensive: high Constitution carries this one, not speed.
	WeaponDamageDie = 8;
	AttackCooldownSeconds = 1.6f;
	AttackRange = 180.0f;
	AggroRadius = 1400.0f;
}

ADCCEnemyCharacter_UnclaimedProperty::ADCCEnemyCharacter_UnclaimedProperty()
{
	DisplayName = NSLOCTEXT("DCC", "UnclaimedPropertyName", "Unclaimed Property");

	static ConstructorHelpers::FObjectFinder<USkeletalMesh> ChestMonsterMesh(
		TEXT("/Game/MonsterForSurvivalGame/Mesh/PBR/ChestMonster_SK.ChestMonster_SK"));
	if (USkeletalMeshComponent* MeshComponent = GetMesh())
	{
		if (ChestMonsterMesh.Succeeded())
		{
			MeshComponent->SetSkeletalMesh(ChestMonsterMesh.Object);
		}
	}

	static ConstructorHelpers::FObjectFinder<UAnimSequence> WalkAnim(
		TEXT("/Game/MonsterForSurvivalGame/Animation/PBR/ChestMonster/ChestMonster_WalkFWD_ANIM.ChestMonster_WalkFWD_ANIM"));
	static ConstructorHelpers::FObjectFinder<UAnimSequence> RunAnim(
		TEXT("/Game/MonsterForSurvivalGame/Animation/PBR/ChestMonster/ChestMonster_Run_ANIM.ChestMonster_Run_ANIM"));

	if (WalkAnim.Succeeded()) { WalkAnimation = WalkAnim.Object; }
	if (RunAnim.Succeeded()) { RunAnimation = RunAnim.Object; }

	if (Stats)
	{
		Stats->Level = 3;
		Stats->AbilityScores.SetScore(EDCCAbility::Strength, 6);
		Stats->AbilityScores.SetScore(EDCCAbility::Intelligence, 3);
		Stats->AbilityScores.SetScore(EDCCAbility::Constitution, 4);
		Stats->AbilityScores.SetScore(EDCCAbility::Dexterity, 3);
		Stats->AbilityScores.SetScore(EDCCAbility::Charisma, 2);
	}

	// Waits to be approached (short AggroRadius, like a real chest doesn't
	// chase) then hits harder per swing than anything else on this roster -
	// the ambush is the whole point.
	WeaponDamageDie = 12;
	AttackCooldownSeconds = 2.4f;
	AttackRange = 200.0f;
	AggroRadius = 1000.0f;
}

ADCCEnemyCharacter_BreakRoomMold::ADCCEnemyCharacter_BreakRoomMold()
{
	DisplayName = NSLOCTEXT("DCC", "BreakRoomMoldName", "Break Room Mold");

	static ConstructorHelpers::FObjectFinder<USkeletalMesh> MushroomMesh(
		TEXT("/Game/MonsterForSurvivalGame/Mesh/PBR/Mushroom_SK.Mushroom_SK"));
	if (USkeletalMeshComponent* MeshComponent = GetMesh())
	{
		if (MushroomMesh.Succeeded())
		{
			MeshComponent->SetSkeletalMesh(MushroomMesh.Object);
		}
	}

	// AngryVersion specifically - the pack also ships a SmileVersion of every
	// clip, which is not what a hostile spawn should be playing.
	static ConstructorHelpers::FObjectFinder<UAnimSequence> WalkAnim(
		TEXT("/Game/MonsterForSurvivalGame/Animation/PBR/Mushroom/AngryVersion/Mushroom_walkFWDAngry_ANIM.Mushroom_walkFWDAngry_ANIM"));
	static ConstructorHelpers::FObjectFinder<UAnimSequence> RunAnim(
		TEXT("/Game/MonsterForSurvivalGame/Animation/PBR/Mushroom/AngryVersion/Mushroom_runFWDAngry_ANIM.Mushroom_runFWDAngry_ANIM"));

	if (WalkAnim.Succeeded()) { WalkAnimation = WalkAnim.Object; }
	if (RunAnim.Succeeded()) { RunAnimation = RunAnim.Object; }

	if (Stats)
	{
		Stats->Level = 1;
		Stats->AbilityScores.SetScore(EDCCAbility::Strength, 2);
		Stats->AbilityScores.SetScore(EDCCAbility::Intelligence, 2);
		Stats->AbilityScores.SetScore(EDCCAbility::Constitution, 2);
		Stats->AbilityScores.SetScore(EDCCAbility::Dexterity, 2);
		Stats->AbilityScores.SetScore(EDCCAbility::Charisma, 2);
	}

	// Same "arrives in numbers, not a threat alone" role FinePrint plays at
	// the Oathgate - weak everywhere, fast cooldown, cheap to place several of.
	WeaponDamageDie = 4;
	AttackCooldownSeconds = 1.2f;
	AttackRange = 150.0f;
	AggroRadius = 1200.0f;
}

ADCCEnemyCharacter_ShellCompany::ADCCEnemyCharacter_ShellCompany()
{
	DisplayName = NSLOCTEXT("DCC", "ShellCompanyName", "Shell Company");

	static ConstructorHelpers::FObjectFinder<USkeletalMesh> TurtleShellMesh(
		TEXT("/Game/MonsterForSurvivalGame/Mesh/PBR/TurtleShell_SK.TurtleShell_SK"));
	if (USkeletalMeshComponent* MeshComponent = GetMesh())
	{
		if (TurtleShellMesh.Succeeded())
		{
			MeshComponent->SetSkeletalMesh(TurtleShellMesh.Object);
		}
	}

	static ConstructorHelpers::FObjectFinder<UAnimSequence> WalkAnim(
		TEXT("/Game/MonsterForSurvivalGame/Animation/PBR/TurtleShell/TurtleShell_Walk_ANIM.TurtleShell_Walk_ANIM"));
	static ConstructorHelpers::FObjectFinder<UAnimSequence> RunAnim(
		TEXT("/Game/MonsterForSurvivalGame/Animation/PBR/TurtleShell/TurtleShell_Run_ANIM.TurtleShell_Run_ANIM"));

	if (WalkAnim.Succeeded()) { WalkAnimation = WalkAnim.Object; }
	if (RunAnim.Succeeded()) { RunAnimation = RunAnim.Object; }

	if (Stats)
	{
		Stats->Level = 3;
		Stats->AbilityScores.SetScore(EDCCAbility::Strength, 4);
		Stats->AbilityScores.SetScore(EDCCAbility::Intelligence, 2);
		Stats->AbilityScores.SetScore(EDCCAbility::Constitution, 6);
		Stats->AbilityScores.SetScore(EDCCAbility::Dexterity, 2);
		Stats->AbilityScores.SetScore(EDCCAbility::Charisma, 3);
	}

	// The roster's heaviest tank - built to be committed to, not burst down.
	WeaponDamageDie = 8;
	AttackCooldownSeconds = 2.2f;
	AttackRange = 200.0f;
	AggroRadius = 1200.0f;
}
