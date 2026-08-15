#include "Characters/DCCEnemyCharacter.h"

#include "Stats/DCCStatsComponent.h"
#include "Skills/DCCSkillComponent.h"
#include "Combat/DCCGameState.h"
#include "AIController.h"
#include "Navigation/PathFollowingComponent.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "GameFramework/PlayerController.h"
#include "Engine/World.h"
#include "EngineUtils.h"

ADCCEnemyCharacter::ADCCEnemyCharacter()
{
	PrimaryActorTick.bCanEverTick = true;

	AIControllerClass = AAIController::StaticClass();
	AutoPossessAI = EAutoPossessAI::PlacedInWorldOrSpawned;

	// Enemies are spawned by an encounter, so they must be cleaned up rather
	// than left standing around a cleared room.
	CorpseLifeSpan = 8.0f;

	if (UCharacterMovementComponent* Movement = GetCharacterMovement())
	{
		Movement->bOrientRotationToMovement = true;
		Movement->RotationRate = FRotator(0.0f, 540.0f, 0.0f);
		Movement->MaxWalkSpeed = 400.0f;
	}

	bUseControllerRotationYaw = false;
}

void ADCCEnemyCharacter::BeginPlay()
{
	Super::BeginPlay();

	// Only the server thinks. Clients would otherwise run a second, divergent
	// copy of this AI against their own view of the world.
	if (!HasAuthority())
	{
		SetActorTickEnabled(false);
	}
}

ADCCCharacterBase* ADCCEnemyCharacter::FindNearestHostileTarget() const
{
	UWorld* World = GetWorld();
	if (!World)
	{
		return nullptr;
	}

	ADCCCharacterBase* Best = nullptr;
	float BestDistanceSquared = TNumericLimits<float>::Max();

	// Every combatant, filtered by faction. This replaced a player-controller
	// scan: that version could only ever hunt the human player, so a friendly NPC
	// had nothing to attack and mobs ignored allies standing next to them.
	for (TActorIterator<ADCCCharacterBase> It(World); It; ++It)
	{
		ADCCCharacterBase* Candidate = *It;
		if (!Candidate || !Candidate->IsAlive() || !IsHostileTo(Candidate))
		{
			continue;
		}

		const float DistanceSquared = FVector::DistSquared(GetActorLocation(), Candidate->GetActorLocation());
		if (DistanceSquared < BestDistanceSquared)
		{
			BestDistanceSquared = DistanceSquared;
			Best = Candidate;
		}
	}

	if (Best && BestDistanceSquared <= FMath::Square(AggroRadius))
	{
		return Best;
	}

	return nullptr;
}

bool ADCCEnemyCharacter::TryUseSkill(ADCCCharacterBase* Target)
{
	if (!bUseSkills || !Skills || !Target || !HasAuthority())
	{
		return false;
	}

	const UWorld* World = GetWorld();
	if (!World)
	{
		return false;
	}

	if ((World->GetTimeSeconds() - LastSkillAttemptTime) < SkillAttemptInterval)
	{
		return false;
	}

	const float DistanceToTarget = FVector::Dist(GetActorLocation(), Target->GetActorLocation());

	FName BestSkillId = NAME_None;
	float BestRange = -1.0f;

	for (const FDCCKnownSkill& Known : Skills->KnownSkills)
	{
		FDCCSkillDef Skill;
		if (!Skills->FindSkill(Known.SkillId, Skill) || Skill.bIsPassive)
		{
			continue;
		}

		// CanActivate only range-checks SingleTarget skills, so cones and areas
		// need checking here or an enemy would happily breathe fire from across
		// the room and hit nothing.
		if (Skill.Targeting != EDCCSkillTargeting::Self && DistanceToTarget > Skill.Range)
		{
			continue;
		}

		if (Skills->CanActivate(Known.SkillId, Target) != EDCCSkillFailure::None)
		{
			continue;
		}

		if (Skill.Range > BestRange)
		{
			BestRange = Skill.Range;
			BestSkillId = Known.SkillId;
		}
	}

	if (BestSkillId.IsNone())
	{
		return false;
	}

	// Face the target first: cast effects spawn along the caster's forward
	// vector, so a spell thrown by someone facing away looks like a bug.
	const FVector ToTarget = Target->GetActorLocation() - GetActorLocation();
	if (!ToTarget.IsNearlyZero())
	{
		SetActorRotation(FRotator(0.0f, ToTarget.Rotation().Yaw, 0.0f));
	}

	LastSkillAttemptTime = World->GetTimeSeconds();
	CastHoldUntilTime = World->GetTimeSeconds() + CastHoldSeconds;

	// Rooted for the cast: stop dead now, and Tick refuses to move or re-path
	// until the hold expires.
	if (UCharacterMovementComponent* Movement = GetCharacterMovement())
	{
		Movement->StopMovementImmediately();
	}

	if (AAIController* AIController = Cast<AAIController>(GetController()))
	{
		AIController->StopMovement();
	}
	bIsChasing = false;

	PlayOneShotAnimation(CastAnimation, CastHoldSeconds);

	Skills->TryActivateSkill(BestSkillId, Target);

	return true;
}

void ADCCEnemyCharacter::Tick(float DeltaSeconds)
{
	Super::Tick(DeltaSeconds);

	if (!HasAuthority() || !IsAlive())
	{
		return;
	}

	// The pause is a planning phase for the whole table, so the dungeon stops
	// thinking too. Without this, enemies would keep closing and swinging while
	// the players are frozen deciding what to do.
	if (const ADCCGameState* GameState = GetWorld() ? GetWorld()->GetGameState<ADCCGameState>() : nullptr)
	{
		if (GameState->IsCombatPaused())
		{
			return;
		}
	}

	AAIController* AIController = Cast<AAIController>(GetController());

	// Drop a target that died or wandered past the leash before looking for a new one.
	if (CurrentTarget)
	{
		const float DistanceSquared = FVector::DistSquared(GetActorLocation(), CurrentTarget->GetActorLocation());
		if (!CurrentTarget->IsAlive() || DistanceSquared > FMath::Square(LeashRadius))
		{
			CurrentTarget = nullptr;
			bIsChasing = false;

			if (AIController)
			{
				AIController->StopMovement();
			}
		}
	}

	if (!CurrentTarget)
	{
		CurrentTarget = FindNearestHostileTarget();
		bIsChasing = false;
	}

	if (!CurrentTarget)
	{
		return;
	}

	// Mid-cast: hold position. Facing still tracks the target so the spell does
	// not fire off into empty space, but no movement is issued.
	if (GetWorld()->GetTimeSeconds() < CastHoldUntilTime)
	{
		const FVector ToTargetHeld = CurrentTarget->GetActorLocation() - GetActorLocation();
		if (!ToTargetHeld.IsNearlyZero())
		{
			SetActorRotation(FRotator(0.0f, ToTargetHeld.Rotation().Yaw, 0.0f));
		}
		return;
	}

	// Skills come before melee and before movement. A caster whose spell outranges
	// its fists should stand and use it rather than closing first - and stopping
	// the chase here is what actually produces that behaviour.
	if (TryUseSkill(CurrentTarget))
	{
		if (bIsChasing && AIController)
		{
			AIController->StopMovement();
			bIsChasing = false;
		}

		return;
	}

	if (IsInAttackRange(CurrentTarget))
	{
		if (bIsChasing && AIController)
		{
			AIController->StopMovement();
			bIsChasing = false;
		}

		// Face the target so the swing reads correctly even though movement
		// rotation is no longer driving us.
		const FVector ToTarget = CurrentTarget->GetActorLocation() - GetActorLocation();
		if (!ToTarget.IsNearlyZero())
		{
			const FRotator Facing(0.0f, ToTarget.Rotation().Yaw, 0.0f);
			SetActorRotation(Facing);
		}

		// PerformAttack enforces its own cooldown, so calling every frame is safe.
		PerformAttack(CurrentTarget);
		return;
	}

	if (AIController && !bIsChasing)
	{
		// Requires a NavMeshBoundsVolume covering the room; without one this is
		// a no-op and the enemy will stand still.
		AIController->MoveToActor(CurrentTarget, AttackRange * 0.5f);
		bIsChasing = true;
	}
	else if (AIController && bIsChasing)
	{
		// Re-issue when the path finished but we are still out of range, which
		// happens whenever the target keeps moving.
		if (AIController->GetMoveStatus() == EPathFollowingStatus::Idle)
		{
			AIController->MoveToActor(CurrentTarget, AttackRange * 0.5f);
		}
	}
}
