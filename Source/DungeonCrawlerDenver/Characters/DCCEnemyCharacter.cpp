#include "Characters/DCCEnemyCharacter.h"

#include "Stats/DCCStatsComponent.h"
#include "Combat/DCCGameState.h"
#include "AIController.h"
#include "Navigation/PathFollowingComponent.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "GameFramework/PlayerController.h"
#include "Engine/World.h"

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

ADCCCharacterBase* ADCCEnemyCharacter::FindNearestPlayerTarget() const
{
	const UWorld* World = GetWorld();
	if (!World)
	{
		return nullptr;
	}

	ADCCCharacterBase* Best = nullptr;
	float BestDistanceSquared = TNumericLimits<float>::Max();

	// Iterating player controllers rather than all pawns keeps enemies from
	// aggroing onto each other, which is the behaviour we want for a first
	// encounter and costs nothing to revisit later.
	for (FConstPlayerControllerIterator It = World->GetPlayerControllerIterator(); It; ++It)
	{
		const APlayerController* PC = It->Get();
		if (!PC)
		{
			continue;
		}

		ADCCCharacterBase* Candidate = Cast<ADCCCharacterBase>(PC->GetPawn());
		if (!Candidate || !Candidate->IsAlive())
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
		CurrentTarget = FindNearestPlayerTarget();
		bIsChasing = false;
	}

	if (!CurrentTarget)
	{
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
