#include "Rooms/DCCClimbPoint.h"

#include "Characters/DCCCharacterBase.h"
#include "Stats/DCCStatsComponent.h"
#include "Stats/DCCRuleset.h"
#include "Skills/DCCSkillComponent.h"
#include "Combat/DCCGameState.h"
#include "UI/DCCEventLogTypes.h"
#include "Components/BoxComponent.h"
#include "Components/StaticMeshComponent.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "Components/CapsuleComponent.h"
#include "Engine/World.h"

ADCCClimbPoint::ADCCClimbPoint()
{
	PrimaryActorTick.bCanEverTick = true;

	// Replicated so clients can see who is climbing; the decision stays on the
	// server.
	bReplicates = true;

	Trigger = CreateDefaultSubobject<UBoxComponent>(TEXT("Trigger"));
	SetRootComponent(Trigger);
	Trigger->SetBoxExtent(FVector(120.0f, 200.0f, 120.0f));
	Trigger->SetCollisionEnabled(ECollisionEnabled::QueryOnly);
	Trigger->SetCollisionResponseToAllChannels(ECR_Ignore);
	Trigger->SetCollisionResponseToChannel(ECC_Pawn, ECR_Overlap);

	Marker = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("Marker"));
	Marker->SetupAttachment(Trigger);
	Marker->SetCollisionEnabled(ECollisionEnabled::NoCollision);

	PromptText = NSLOCTEXT("DCC", "ClimbPromptDefault",
		"The rock face is climbable. [G] to attempt it.");
}

void ADCCClimbPoint::BeginPlay()
{
	Super::BeginPlay();
}

bool ADCCClimbPoint::IsCharacterInRange(const ADCCCharacterBase* Character) const
{
	if (!Character || !Trigger)
	{
		return false;
	}

	// A distance test against the box extent rather than overlap events, so this
	// is valid the instant it is asked - the HUD polls it every frame to decide
	// whether to draw the prompt, and overlap state can lag a teleport or spawn.
	const FVector Local = GetActorTransform().InverseTransformPosition(Character->GetActorLocation());
	const FVector Extent = Trigger->GetUnscaledBoxExtent();

	return FMath::Abs(Local.X) <= Extent.X
		&& FMath::Abs(Local.Y) <= Extent.Y
		&& FMath::Abs(Local.Z) <= Extent.Z + 100.0f;
}

bool ADCCClimbPoint::TryClimb(ADCCCharacterBase* Character)
{
	if (!HasAuthority() || !Character || !Character->IsAlive())
	{
		return false;
	}

	if (!IsCharacterInRange(Character))
	{
		return false;
	}

	if (!bAllowRetry && Succeeded.Contains(Character))
	{
		return false;
	}

	// Already on the wall.
	for (const FActiveClimb& Climb : ActiveClimbs)
	{
		if (Climb.Character.Get() == Character)
		{
			return false;
		}
	}

	UDCCStatsComponent* Stats = Character->Stats;
	UDCCRuleset* Ruleset = Stats ? Stats->GetRuleset() : nullptr;
	if (!Stats || !Ruleset)
	{
		return false;
	}

	// Rank counts if they have the skill, and the attempt is untrained otherwise -
	// which is exactly the published rule for Utility skills, Disadvantage and all.
	int32 Rank = 0;
	bool bUntrained = true;
	if (UDCCSkillComponent* Skills = Character->Skills)
	{
		if (Skills->IsSkillKnown(ClimbSkillId))
		{
			Rank = Skills->GetSkillRank(ClimbSkillId);
			bUntrained = false;

			// Marks the skill whether this succeeds or fails, so climbing badly is
			// still how you get better at climbing.
			Skills->MarkSkillUsed(ClimbSkillId);
		}
	}

	const FDCCCheckResult Result = Ruleset->ResolveSkillCheck(
		Stats->AbilityScores, ClimbAbility, Rank, DifficultyClass, bUntrained);

	const FText Name = FText::FromString(Character->GetName());

	if (!Result.bSuccess)
	{
		ADCCGameState::Log(this, EDCCLogCategory::Encounter, FText::Format(
			NSLOCTEXT("DCC", "ClimbFail", "{0} slips off the rock. Rolled {1} against {2}."),
			Name, FText::AsNumber(Result.Total), FText::AsNumber(Result.DifficultyClass)));
		return false;
	}

	FActiveClimb Climb;
	Climb.Character = Character;
	Climb.Start = Character->GetActorLocation();
	Climb.End = GetActorLocation() + GetActorRotation().RotateVector(ClimbEndOffset);
	ActiveClimbs.Add(Climb);

	Succeeded.AddUnique(Character);

	// Movement off for the ascent: the character is being carried along a path,
	// and leaving gravity and input active would fight that every frame.
	if (UCharacterMovementComponent* Movement = Character->GetCharacterMovement())
	{
		Movement->StopMovementImmediately();
		Movement->SetMovementMode(MOVE_Flying);
	}

	// Collision off so the climber does not catch on the very surface they are
	// climbing. Restored at the top.
	if (UCapsuleComponent* Capsule = Character->GetCapsuleComponent())
	{
		Capsule->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	}

	Character->PlayClimbAnimation(ClimbDurationSeconds);

	ADCCGameState::Log(this, EDCCLogCategory::Encounter, FText::Format(
		NSLOCTEXT("DCC", "ClimbStart", "{0} starts climbing. Rolled {1} against {2}."),
		Name, FText::AsNumber(Result.Total), FText::AsNumber(Result.DifficultyClass)));

	return true;
}

void ADCCClimbPoint::Tick(float DeltaSeconds)
{
	Super::Tick(DeltaSeconds);

	if (!HasAuthority() || ActiveClimbs.IsEmpty())
	{
		return;
	}

	for (int32 Index = ActiveClimbs.Num() - 1; Index >= 0; --Index)
	{
		FActiveClimb& Climb = ActiveClimbs[Index];
		ADCCCharacterBase* Character = Climb.Character.Get();

		// Died or was destroyed mid-climb: drop the entry rather than dragging a
		// corpse up the wall.
		if (!Character || !Character->IsAlive())
		{
			ActiveClimbs.RemoveAt(Index);
			continue;
		}

		Climb.Elapsed += DeltaSeconds;
		const float Alpha = FMath::Clamp(Climb.Elapsed / FMath::Max(0.1f, ClimbDurationSeconds), 0.0f, 1.0f);

		// Ease the vertical so the ascent starts and finishes gently rather than
		// snapping, which sells it as climbing rather than teleporting.
		const float Eased = FMath::InterpEaseInOut(0.0f, 1.0f, Alpha, 2.0f);
		Character->SetActorLocation(FMath::Lerp(Climb.Start, Climb.End, Eased));

		if (Alpha >= 1.0f)
		{
			if (UCharacterMovementComponent* Movement = Character->GetCharacterMovement())
			{
				Movement->SetMovementMode(MOVE_Walking);
			}

			if (UCapsuleComponent* Capsule = Character->GetCapsuleComponent())
			{
				Capsule->SetCollisionEnabled(ECollisionEnabled::QueryAndPhysics);
			}

			ADCCGameState::Log(this, EDCCLogCategory::Encounter, FText::Format(
				NSLOCTEXT("DCC", "ClimbDone", "{0} is over the top. The crowd expected worse."),
				FText::FromString(Character->GetName())));

			ActiveClimbs.RemoveAt(Index);
		}
	}
}
