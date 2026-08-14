#include "Characters/DCCCharacterBase.h"

#include "Stats/DCCStatsComponent.h"
#include "Stats/DCCRuleset.h"
#include "Skills/DCCSkillComponent.h"
#include "Combat/DCCGameState.h"
#include "Favor/DCCFavorComponent.h"
#include "Items/DCCCarryComponent.h"
#include "Components/CapsuleComponent.h"
#include "Components/InputComponent.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "GameFramework/Controller.h"
#include "GameFramework/PlayerController.h"
#include "Blueprint/UserWidget.h"
#include "EngineUtils.h"

ADCCCharacterBase::ADCCCharacterBase()
{
	PrimaryActorTick.bCanEverTick = false;

	// Everything mechanical about a character has to exist on the server and
	// reach clients by replication, so the actor itself must replicate.
	bReplicates = true;
	SetReplicateMovement(true);

	Stats = CreateDefaultSubobject<UDCCStatsComponent>(TEXT("Stats"));
	Skills = CreateDefaultSubobject<UDCCSkillComponent>(TEXT("Skills"));
	Favor = CreateDefaultSubobject<UDCCFavorComponent>(TEXT("Favor"));
	Carry = CreateDefaultSubobject<UDCCCarryComponent>(TEXT("Carry"));
}

void ADCCCharacterBase::BeginPlay()
{
	Super::BeginPlay();

	if (Stats)
	{
		// Bound on every machine: the server reaches this through ApplyDamage,
		// clients through OnRep. Both sides want the cosmetic death.
		Stats->OnDied.AddDynamic(this, &ADCCCharacterBase::HandleDied);
	}

	// Applied on every machine rather than replicated: MaxWalkSpeed is an input to
	// client-side movement prediction, so each machine deriving the same value
	// from the already-replicated ability scores keeps prediction in agreement.
	ApplyDerivedMovementSpeed();
}

void ADCCCharacterBase::ApplyDerivedMovementSpeed()
{
	UCharacterMovementComponent* Movement = GetCharacterMovement();
	if (!Movement || !Stats)
	{
		return;
	}

	UDCCRuleset* Ruleset = Stats->GetRuleset();
	if (!Ruleset)
	{
		return;
	}

	Movement->MaxWalkSpeed = BaseWalkSpeed * Ruleset->GetMoveSpeedMultiplier(Stats->AbilityScores);
}

void ADCCCharacterBase::AttemptPickupOrDrop()
{
	if (Carry)
	{
		Carry->TryPickupOrDrop();
	}
}

void ADCCCharacterBase::AttemptThrow()
{
	if (Carry)
	{
		Carry->TryThrow();
	}
}

bool ADCCCharacterBase::IsAlive() const
{
	return Stats && Stats->IsAlive();
}

bool ADCCCharacterBase::IsAttackReady() const
{
	const UWorld* World = GetWorld();
	if (!World)
	{
		return false;
	}

	return (World->GetTimeSeconds() - LastAttackTime) >= AttackCooldownSeconds;
}

bool ADCCCharacterBase::IsInAttackRange(const ADCCCharacterBase* Target) const
{
	if (!Target)
	{
		return false;
	}

	// Measured between capsule surfaces rather than origins so that fat enemies
	// do not need a hand-tuned range to be able to reach anything.
	const float SelfRadius = GetCapsuleComponent() ? GetCapsuleComponent()->GetScaledCapsuleRadius() : 0.0f;
	const float TargetRadius = Target->GetCapsuleComponent() ? Target->GetCapsuleComponent()->GetScaledCapsuleRadius() : 0.0f;

	const float Distance = FVector::Dist(GetActorLocation(), Target->GetActorLocation());

	return Distance <= (AttackRange + SelfRadius + TargetRadius);
}

FDCCAttackResult ADCCCharacterBase::PerformAttack(ADCCCharacterBase* Target, bool bIgnoreCooldown)
{
	FDCCAttackResult Result;

	if (!HasAuthority())
	{
		UE_LOG(LogTemp, Warning, TEXT("DCCCharacterBase::PerformAttack called on a client; ignoring. Attacks are server-authoritative."));
		return Result;
	}

	if (!Target || Target == this || !IsAlive() || !Target->IsAlive())
	{
		return Result;
	}

	if ((!bIgnoreCooldown && !IsAttackReady()) || !IsInAttackRange(Target))
	{
		return Result;
	}

	UDCCStatsComponent* TargetStats = Target->Stats;
	if (!Stats || !TargetStats)
	{
		return Result;
	}

	UDCCRuleset* Ruleset = Stats->GetRuleset();
	if (!Ruleset)
	{
		return Result;
	}

	if (const UWorld* World = GetWorld())
	{
		LastAttackTime = World->GetTimeSeconds();
	}

	Result = Ruleset->ResolveAttack(
		Stats->AbilityScores,
		AttackAbility,
		WeaponDamageDie,
		TargetStats->AbilityScores,
		TargetStats->Level);

	if (Result.bHit)
	{
		TargetStats->ApplyDamage(Result.Damage);
	}

	Multicast_PlayAttack(Target, Result.bHit);

	return Result;
}

void ADCCCharacterBase::PawnClientRestart()
{
	Super::PawnClientRestart();

	if (!HUDWidgetClass || HUDWidget || !IsLocallyControlled())
	{
		return;
	}

	APlayerController* PC = Cast<APlayerController>(GetController());
	if (!PC)
	{
		return;
	}

	HUDWidget = CreateWidget<UUserWidget>(PC, HUDWidgetClass);
	if (HUDWidget)
	{
		HUDWidget->AddToViewport();
	}
}

void ADCCCharacterBase::SetupPlayerInputComponent(UInputComponent* PlayerInputComponent)
{
	Super::SetupPlayerInputComponent(PlayerInputComponent);

	if (!PlayerInputComponent)
	{
		return;
	}

	// Bound to a raw key rather than an Enhanced Input action on purpose: it
	// makes the first encounter playable without authoring input assets, and
	// binds alongside the template's Enhanced Input rather than replacing it.
	// Replace with a proper InputAction when the pause/queue loop lands.
	PlayerInputComponent->BindKey(EKeys::LeftMouseButton, IE_Pressed, this, &ADCCCharacterBase::AttemptAttackNearest);

	// Space is the template's jump; P keeps the pause off anything the third
	// person template already claims.
	PlayerInputComponent->BindKey(EKeys::P, IE_Pressed, this, &ADCCCharacterBase::RequestTogglePause);

	// E lifts and drops, R throws. Two keys rather than one because "put this
	// down carefully" and "hurl this at the thing trying to eat me" are different
	// intentions, and a single key would guess wrong at the worst moment.
	PlayerInputComponent->BindKey(EKeys::E, IE_Pressed, this, &ADCCCharacterBase::AttemptPickupOrDrop);
	PlayerInputComponent->BindKey(EKeys::R, IE_Pressed, this, &ADCCCharacterBase::AttemptThrow);
}

void ADCCCharacterBase::AttemptAttackNearest()
{
	if (HasAuthority())
	{
		ResolveOrQueueAttack();
		return;
	}

	Server_AttackNearest();
}

void ADCCCharacterBase::Server_AttackNearest_Implementation()
{
	// Deliberately re-runs target selection rather than accepting a target from
	// the client, so a modified client cannot nominate an out-of-range victim.
	ResolveOrQueueAttack();
}

void ADCCCharacterBase::ResolveOrQueueAttack()
{
	ADCCCharacterBase* Target = FindNearestHostileInRange();

	const UWorld* World = GetWorld();
	if (ADCCGameState* GameState = World ? World->GetGameState<ADCCGameState>() : nullptr)
	{
		if (GameState->IsCombatPaused())
		{
			// Same input, different meaning while frozen: plan rather than swing.
			GameState->QueueAction(this, NAME_None, Target);
			return;
		}
	}

	PerformAttack(Target);
}

void ADCCCharacterBase::RequestTogglePause()
{
	if (HasAuthority())
	{
		Server_TogglePause_Implementation();
		return;
	}

	Server_TogglePause();
}

void ADCCCharacterBase::Server_TogglePause_Implementation()
{
	const UWorld* World = GetWorld();
	if (ADCCGameState* GameState = World ? World->GetGameState<ADCCGameState>() : nullptr)
	{
		GameState->SetCombatPaused(!GameState->IsCombatPaused());
	}
}

bool ADCCCharacterBase::IsHostileTo(const ADCCCharacterBase* Other) const
{
	if (!Other || Other == this)
	{
		return false;
	}

	return IsCrawler() != Other->IsCrawler();
}

ADCCCharacterBase* ADCCCharacterBase::FindNearestHostileInRange() const
{
	UWorld* World = GetWorld();
	if (!World)
	{
		return nullptr;
	}

	ADCCCharacterBase* Best = nullptr;
	float BestDistanceSquared = TNumericLimits<float>::Max();

	for (TActorIterator<ADCCCharacterBase> It(World); It; ++It)
	{
		ADCCCharacterBase* Candidate = *It;
		if (!Candidate || !Candidate->IsAlive() || !IsHostileTo(Candidate) || !IsInAttackRange(Candidate))
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

	return Best;
}

void ADCCCharacterBase::Multicast_PlayAttack_Implementation(ADCCCharacterBase* Target, bool bHit)
{
	BP_OnAttackPerformed(Target, bHit);
}

void ADCCCharacterBase::HandleDied(UDCCStatsComponent* InStats)
{
	// OnDied can arrive more than once across authority and replication paths;
	// the visual teardown must only happen the first time.
	if (bHasDied)
	{
		return;
	}
	bHasDied = true;

	if (UCapsuleComponent* Capsule = GetCapsuleComponent())
	{
		Capsule->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	}

	if (UCharacterMovementComponent* Movement = GetCharacterMovement())
	{
		Movement->StopMovementImmediately();
		Movement->DisableMovement();
	}

	// Detaching the controller stops AI from continuing to issue orders to a
	// corpse, and stops a dead player from steering it.
	if (HasAuthority())
	{
		DetachFromControllerPendingDestroy();

		if (CorpseLifeSpan > 0.0f)
		{
			SetLifeSpan(CorpseLifeSpan);
		}
	}

	BP_OnDied();

	OnCharacterDied.Broadcast(this);
}
