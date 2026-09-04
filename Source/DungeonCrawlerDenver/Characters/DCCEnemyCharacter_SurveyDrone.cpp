#include "Characters/DCCEnemyCharacter_SurveyDrone.h"

#include "Stats/DCCStatsComponent.h"
#include "Components/SkeletalMeshComponent.h"
#include "Components/CapsuleComponent.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "Engine/SkeletalMesh.h"
#include "Animation/AnimSequence.h"
#include "UObject/ConstructorHelpers.h"

ADCCEnemyCharacter_SurveyDrone::ADCCEnemyCharacter_SurveyDrone()
{
	DisplayName = NSLOCTEXT("DCC", "SurveyDroneName", "Survey Drone");

	// Swarm08 - a small hovering thing with a complete animation set
	// (idle/move/attack/hit/die) that has been sitting unused since the pack
	// was installed. Reads as machine-adjacent at this scale and, unlike the
	// SoulCave "Guide Robot Scanner", is an actual mesh rather than a flat
	// decal plane.
	static ConstructorHelpers::FObjectFinder<USkeletalMesh> DroneMesh(
		TEXT("/Game/MonsterForSurvivalGame/Mesh/PBR/Swarm08_SK.Swarm08_SK"));
	if (USkeletalMeshComponent* MeshComponent = GetMesh())
	{
		if (DroneMesh.Succeeded())
		{
			MeshComponent->SetSkeletalMesh(DroneMesh.Object);
		}
		MeshComponent->SetRelativeScale3D(FVector(0.7f, 0.7f, 0.7f));
	}

	static ConstructorHelpers::FObjectFinder<UAnimSequence> IdleAnim(
		TEXT("/Game/MonsterForSurvivalGame/Animation/PBR/Swarm08/Swarm08_Idle_Anim.Swarm08_Idle_Anim"));
	static ConstructorHelpers::FObjectFinder<UAnimSequence> MoveAnim(
		TEXT("/Game/MonsterForSurvivalGame/Animation/PBR/Swarm08/Swarm08_MoveFWD_Anim.Swarm08_MoveFWD_Anim"));

	// Walk and run both read the same clip: a drone has no gait to change.
	if (MoveAnim.Succeeded())
	{
		WalkAnimation = MoveAnim.Object;
		RunAnimation = MoveAnim.Object;
	}
	if (IdleAnim.Succeeded())
	{
		IdleAnimation = IdleAnim.Object;
	}

	if (Stats)
	{
		Stats->Level = 2;
		Stats->AbilityScores.SetScore(EDCCAbility::Strength, 2);
		Stats->AbilityScores.SetScore(EDCCAbility::Intelligence, 4);
		Stats->AbilityScores.SetScore(EDCCAbility::Constitution, 2);
		Stats->AbilityScores.SetScore(EDCCAbility::Dexterity, 5);
		Stats->AbilityScores.SetScore(EDCCAbility::Charisma, 1);
	}

	// It only ever fights back, so the numbers are for a nuisance rather than
	// a threat - a party that decides to swat one should win, and should then
	// have to think about what it just did on camera.
	WeaponDamageDie = 4;
	AttackCooldownSeconds = 1.6f;
	AttackRange = 260.0f;

	// THE WHOLE POINT. Never auto-acquires; the only route to a target is
	// HandleNearbyAttack. AggroRadius is left at its default but is not read
	// at all while passive.
	bPassiveUntilProvoked = true;
	ViolenceWatchRadius = 1100.0f;
	PatrolWaitSeconds = 2.0f;
	PatrolPointTolerance = 120.0f;

	// Hovering, roughly head height. Gravity off so it holds altitude over the
	// uneven cave floor instead of trudging along it; flying movement also
	// makes the circling patrol read as flight rather than as a walk.
	if (UCharacterMovementComponent* Movement = GetCharacterMovement())
	{
		Movement->GravityScale = 0.0f;
		Movement->DefaultLandMovementMode = MOVE_Flying;
		Movement->MaxFlySpeed = 260.0f;
		Movement->MaxWalkSpeed = 260.0f;
		Movement->BrakingDecelerationFlying = 800.0f;
	}
}
