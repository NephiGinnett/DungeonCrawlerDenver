#include "Characters/DCCCharacterBase.h"

#include "Stats/DCCStatsComponent.h"
#include "Stats/DCCRuleset.h"
#include "Skills/DCCSkillComponent.h"
#include "Combat/DCCGameState.h"
#include "Favor/DCCFavorComponent.h"
#include "Items/DCCCarryComponent.h"
#include "Items/DCCInventoryComponent.h"
#include "Items/DCCPickup.h"
#include "UI/DCCFloatingText.h"
#include "UI/DCCHUD.h"
#include "Rooms/DCCClimbPoint.h"
#include "Progression/DCCAchievementComponent.h"
#include "Quests/DCCQuestComponent.h"
#include "Components/TextRenderComponent.h"
#include "Animation/AnimSequenceBase.h"
#include "Net/UnrealNetwork.h"
#include "UI/DCCEventLogTypes.h"
#include "Components/CapsuleComponent.h"
#include "Components/InputComponent.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "GameFramework/Controller.h"
#include "GameFramework/PlayerController.h"
#include "Blueprint/UserWidget.h"
#include "EngineUtils.h"

ADCCCharacterBase::ADCCCharacterBase()
{
	// Ticks for locomotion animation selection. Cheap - a velocity check and a
	// comparison - and it has to run on clients too, so it cannot live in the
	// server-only enemy AI tick.
	PrimaryActorTick.bCanEverTick = true;

	// Everything mechanical about a character has to exist on the server and
	// reach clients by replication, so the actor itself must replicate.
	bReplicates = true;
	SetReplicateMovement(true);

	Stats = CreateDefaultSubobject<UDCCStatsComponent>(TEXT("Stats"));
	Skills = CreateDefaultSubobject<UDCCSkillComponent>(TEXT("Skills"));
	Favor = CreateDefaultSubobject<UDCCFavorComponent>(TEXT("Favor"));
	Carry = CreateDefaultSubobject<UDCCCarryComponent>(TEXT("Carry"));
	Inventory = CreateDefaultSubobject<UDCCInventoryComponent>(TEXT("Inventory"));
	Achievements = CreateDefaultSubobject<UDCCAchievementComponent>(TEXT("Achievements"));

	Nameplate = CreateDefaultSubobject<UTextRenderComponent>(TEXT("Nameplate"));
	Nameplate->SetupAttachment(RootComponent);
	Nameplate->SetRelativeLocation(FVector(0.0f, 0.0f, 110.0f));
	Nameplate->SetHorizontalAlignment(EHTA_Center);
	Nameplate->SetVerticalAlignment(EVRTA_TextCenter);
	Nameplate->SetWorldSize(22.0f);
	Nameplate->SetCollisionEnabled(ECollisionEnabled::NoCollision);

	// Movable for the same reason the damage tickers are: a Static text component
	// ignores runtime transform changes, so it would stay at the origin and never
	// follow the head.
	Nameplate->SetMobility(EComponentMobility::Movable);

	// Unlit, so an ally standing in a dark cave is still readable - the nameplate
	// is UI that happens to live in the world, and UI does not take shadows.
	Nameplate->SetTextRenderColor(FColor::White);
	Nameplate->bHiddenInGame = false;
}

void ADCCCharacterBase::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);

	DOREPLIFETIME(ADCCCharacterBase, ActiveClimbPoint);
	DOREPLIFETIME(ADCCCharacterBase, bIsSprinting);
	DOREPLIFETIME(ADCCCharacterBase, bBefriended);
}

void ADCCCharacterBase::PossessedBy(AController* NewController)
{
	Super::PossessedBy(NewController);

	// A person is behind this one now, so the ruleset's toughness cushion applies.
	// Checked against the controller rather than an authored flag because the same
	// Blueprint is an AI ally today and a player's crawler the moment they join.
	if (Stats && Cast<APlayerController>(NewController))
	{
		Stats->SetIsPlayerCharacter(true);
	}

	RefreshNameplate();
}

void ADCCCharacterBase::BeginPlay()
{
	Super::BeginPlay();

	if (Stats)
	{
		// Bound on every machine: the server reaches this through ApplyDamage,
		// clients through OnRep. Both sides want the cosmetic death.
		Stats->OnDied.AddDynamic(this, &ADCCCharacterBase::HandleDied);

		Stats->OnHealthChanged.AddDynamic(this, &ADCCCharacterBase::HandleHealthChangedForTicker);
	}

	// Applied on every machine rather than replicated: MaxWalkSpeed is an input to
	// client-side movement prediction, so each machine deriving the same value
	// from the already-replicated ability scores keeps prediction in agreement.
	ApplyDerivedMovementSpeed();

	RefreshNameplate();

	// The local player never needs to read their own nameplate, and it sits
	// directly in front of a third-person camera.
	if (Nameplate && IsLocallyControlled() && Cast<APlayerController>(GetController()))
	{
		Nameplate->SetVisibility(false);
	}
}

void ADCCCharacterBase::HandleHealthChangedForTicker(UDCCStatsComponent* InStats, int32 NewHitPoints, int32 Delta)
{
	if (Delta == 0)
	{
		return;
	}

	// Spawned at head height rather than the actor origin, which sits at the feet
	// on a Character - a number appearing at the ankles reads as belonging to the
	// floor rather than to the character.
	const float HalfHeight = GetCapsuleComponent() ? GetCapsuleComponent()->GetScaledCapsuleHalfHeight() : 90.0f;
	const FVector SpawnLocation = GetActorLocation() + FVector(0.0f, 0.0f, HalfHeight + 30.0f);

	ADCCFloatingText::Spawn(this, SpawnLocation, Delta, IsCrawler());
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

	float Speed = BaseWalkSpeed * Ruleset->GetMoveSpeedMultiplier(Stats->AbilityScores);

	if (bIsSprinting)
	{
		Speed *= Ruleset->GetSprintSpeedMultiplier(Stats->AbilityScores);
	}

	Movement->MaxWalkSpeed = Speed;
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

void ADCCCharacterBase::AttemptStow()
{
	if (HasAuthority())
	{
		Server_StowCarried_Implementation();
		return;
	}

	Server_StowCarried();
}

void ADCCCharacterBase::Server_StowCarried_Implementation()
{
	if (!Carry || !Inventory)
	{
		return;
	}

	// Clears the hand and detaches before the inventory destroys the actor, so
	// the carry component is never left pointing at a destroyed pickup.
	ADCCPickup* Held = Carry->ReleaseCarried();
	if (!Held)
	{
		return;
	}

	if (!Inventory->StowPickup(Held))
	{
		// Full. ReleaseCarried deliberately skipped the physics restore, so it has
		// to happen here or the item hangs in mid-air with no collision - visibly
		// broken, and worse than simply refusing the stow.
		Held->OnDropped();

		UE_LOG(LogTemp, Log, TEXT("DCCCharacterBase: inventory full, %s dropped instead."),
			*GetNameSafe(Held));
	}
}

void ADCCCharacterBase::OnPrimaryPressed()
{
	// Climbing wins: while on a wall the button is the climb, not a swing.
	if (IsClimbing())
	{
		SetClimbInput(/*bUp=*/true, /*bDown=*/bClimbInputDown);
		return;
	}

	// A pending question owns the click too, so the player cannot attack past a
	// prompt they have not answered.
	if (PendingClimbPoint)
	{
		return;
	}

	// With a skill armed from the menu, the click is aimed at the world.
	if (!SelectedSkillId.IsNone())
	{
		TrySelectClimbTargetUnderCursor();
		return;
	}

	// A friendly under the crosshair turns the click into a greeting. Checked
	// before the attack because IsHostileTo already refuses to swing at an ally,
	// so this consumes a click that would otherwise have done nothing at all.
	if (TryFistBumpUnderCursor())
	{
		return;
	}

	AttemptAttackNearest();
}

void ADCCCharacterBase::OnPrimaryReleased()
{
	if (IsClimbing())
	{
		SetClimbInput(/*bUp=*/false, /*bDown=*/bClimbInputDown);
	}
}

void ADCCCharacterBase::OnSecondaryPressed()
{
	if (IsClimbing())
	{
		SetClimbInput(/*bUp=*/bClimbInputUp, /*bDown=*/true);
	}
}

void ADCCCharacterBase::OnSecondaryReleased()
{
	if (IsClimbing())
	{
		SetClimbInput(/*bUp=*/bClimbInputUp, /*bDown=*/false);
	}
}

void ADCCCharacterBase::SelectSkillSlot(int32 Index)
{
	if (!Skills || !Skills->KnownSkills.IsValidIndex(Index))
	{
		return;
	}

	const FName SkillId = Skills->KnownSkills[Index].SkillId;

	// Pressing the same slot again disarms, so there is a way out of targeting
	// mode that does not involve clicking something.
	SelectSkill(SelectedSkillId == SkillId ? NAME_None : SkillId);
}

FLinearColor ADCCCharacterBase::GetAllegianceColour() const
{
	// Three states rather than two, because "on your side" and "you have actually
	// met" are different things and the fist bump has to visibly change something
	// or it is just an animation.
	if (!IsCrawler())
	{
		return FLinearColor(0.90f, 0.25f, 0.20f);
	}

	return bBefriended
		? FLinearColor(0.25f, 0.90f, 0.35f)
		: FLinearColor(0.80f, 0.84f, 0.95f);
}

FText ADCCCharacterBase::GetNameplateText() const
{
	const FText Name = DisplayName.IsEmpty()
		? FText::FromString(GetActorNameOrLabel())
		: DisplayName;

	if (!IsCrawler())
	{
		return Name;
	}

	// Only allies carry a tag. Labelling every mob "HOSTILE" would be noise - the
	// red is already saying it, and the nameplate is competing with the fight.
	return FText::Format(
		bBefriended
			? NSLOCTEXT("DCC", "NameplateFriend", "{0}\nFRIEND")
			: NSLOCTEXT("DCC", "NameplateAlly", "{0}\nALLY"),
		Name);
}

void ADCCCharacterBase::RefreshNameplate()
{
	if (!Nameplate)
	{
		return;
	}

	Nameplate->SetText(GetNameplateText());
	Nameplate->SetTextRenderColor(GetAllegianceColour().ToFColor(/*bSRGB=*/true));
}

void ADCCCharacterBase::OnRep_Befriended()
{
	RefreshNameplate();
}

bool ADCCCharacterBase::TryFistBumpUnderCursor()
{
	APlayerController* PC = Cast<APlayerController>(GetController());
	UWorld* World = GetWorld();
	if (!PC || !World || !IsAlive())
	{
		return false;
	}

	FVector CameraLocation;
	FRotator CameraRotation;
	PC->GetPlayerViewPoint(CameraLocation, CameraRotation);

	const FVector TraceEnd = CameraLocation + CameraRotation.Vector() * 2000.0f;

	FCollisionQueryParams Params;
	Params.AddIgnoredActor(this);

	// A sphere sweep rather than a line: a character is a moving target and
	// demanding pixel accuracy on a friendly greeting is a bad trade.
	FHitResult Hit;
	if (!World->SweepSingleByChannel(
		Hit, CameraLocation, TraceEnd, FQuat::Identity, ECC_Pawn,
		FCollisionShape::MakeSphere(40.0f), Params))
	{
		return false;
	}

	ADCCCharacterBase* Target = Cast<ADCCCharacterBase>(Hit.GetActor());

	// Only someone already on your side, already alive, and not already a friend.
	// You cannot befriend a mob; that is what makes the tag mean something.
	if (!Target || Target == this || !Target->IsAlive()
		|| Target->IsHostileTo(this) || Target->IsBefriended())
	{
		return false;
	}

	if (FVector::Dist(GetActorLocation(), Target->GetActorLocation()) > FistBumpRange)
	{
		return false;
	}

	if (HasAuthority())
	{
		Server_FistBump_Implementation(Target);
	}
	else
	{
		Server_FistBump(Target);
	}

	return true;
}

void ADCCCharacterBase::Server_FistBump_Implementation(ADCCCharacterBase* Target)
{
	// Re-validated server-side, exactly like the climb: the client's click asks,
	// the server decides.
	if (!Target || Target == this || !Target->IsAlive() || !IsAlive()
		|| Target->IsHostileTo(this) || Target->IsBefriended())
	{
		return;
	}

	if (FVector::Dist(GetActorLocation(), Target->GetActorLocation()) > FistBumpRange)
	{
		return;
	}

	Target->bBefriended = true;
	Target->OnRep_Befriended();

	UDCCAchievementComponent::RecordFor(this, EDCCAchievementStat::FistBumps);
	UDCCQuestComponent::NotifyEvent(this, EDCCObjectiveType::Befriend,
		Target->GetClass() ? Target->GetClass()->GetFName() : NAME_None);

	Multicast_PlayFistBump(Target);
	Target->Multicast_PlayFistBump(this);

	ADCCGameState::Log(this, EDCCLogCategory::Announcement, FText::Format(
		NSLOCTEXT("DCC", "LogFistBump", "{0} and {1} bump fists. The audience logs a new alliance."),
		FText::FromString(GetActorNameOrLabel()),
		FText::FromString(Target->GetActorNameOrLabel())));
}

void ADCCCharacterBase::Multicast_PlayFistBump_Implementation(ADCCCharacterBase* Partner)
{
	if (Partner)
	{
		// Turn to face each other first, or two characters bump fists while looking
		// in opposite directions, which reads as a bug rather than a greeting.
		FRotator Facing = (Partner->GetActorLocation() - GetActorLocation()).Rotation();
		Facing.Pitch = 0.0f;
		Facing.Roll = 0.0f;

		SetActorRotation(Facing);
	}

	// Falls back to an emote, since no pack ships a fist-bump clip.
	if (FistBumpAnimation)
	{
		PlayOneShotAnimation(FistBumpAnimation);
	}
	else
	{
		PlayEmote();
	}

	RefreshNameplate();
}

void ADCCCharacterBase::SelectSkill(FName SkillId)
{
	SelectedSkillId = SkillId;

	// Choosing a different skill abandons any question already on screen, so the
	// player cannot answer a prompt they have since changed their mind about.
	PendingClimbPoint = nullptr;
}

void ADCCCharacterBase::TrySelectClimbTargetUnderCursor()
{
	// Purely local: this decides what to *ask*, never what happens.
	APlayerController* PC = Cast<APlayerController>(GetController());
	UWorld* World = GetWorld();
	if (!PC || !World || IsClimbing())
	{
		return;
	}

	// Only meaningful once a skill is armed from the menu. Without this, every
	// left click in the game would start probing for walls.
	if (SelectedSkillId.IsNone())
	{
		return;
	}

	FVector CameraLocation;
	FRotator CameraRotation;
	PC->GetPlayerViewPoint(CameraLocation, CameraRotation);

	const FVector TraceEnd = CameraLocation + CameraRotation.Vector() * 5000.0f;

	FCollisionQueryParams Params;
	Params.AddIgnoredActor(this);

	FHitResult Hit;
	if (!World->LineTraceSingleByChannel(Hit, CameraLocation, TraceEnd, ECC_Visibility, Params))
	{
		return;
	}

	ADCCClimbPoint* Point = Cast<ADCCClimbPoint>(Hit.GetActor());
	if (!Point || !Point->IsWithinReach(this))
	{
		return;
	}

	PendingClimbPoint = Point;
}

void ADCCCharacterBase::ConfirmPendingClimb(bool bAccept)
{
	ADCCClimbPoint* Point = PendingClimbPoint;
	PendingClimbPoint = nullptr;

	if (!bAccept || !Point)
	{
		return;
	}

	if (HasAuthority())
	{
		Server_BeginClimb_Implementation(Point);
		return;
	}

	Server_BeginClimb(Point);
}

void ADCCCharacterBase::Server_BeginClimb_Implementation(ADCCClimbPoint* Point)
{
	// Re-validated server-side: the client's click is a request, never a
	// permission. A modified client can ask to climb a face across the map.
	if (!Point || IsClimbing() || !IsAlive() || !Point->IsWithinReach(this))
	{
		return;
	}

	if (!Stats || Stats->CurrentStamina <= 0.0f)
	{
		return;
	}

	ActiveClimbPoint = Point;
	OnRep_ActiveClimbPoint();

	// Attempting marks the skill, and here there is no failure state - simply
	// trying to climb is what trains Climbing.
	if (Skills && Skills->IsSkillKnown(Point->ClimbSkillId))
	{
		Skills->MarkSkillUsed(Point->ClimbSkillId);
	}

	SetActorLocation(Point->GetAttachLocationFor(this, GetActorLocation().Z));

	ADCCGameState::Log(this, EDCCLogCategory::Encounter, FText::Format(
		NSLOCTEXT("DCC", "ClimbBegin", "{0} takes hold of the rock."),
		FText::FromString(GetName())));
}

void ADCCCharacterBase::SetClimbInput(bool bUp, bool bDown)
{
	bClimbInputUp = bUp;
	bClimbInputDown = bDown;

	if (!HasAuthority())
	{
		Server_SetClimbInput(bUp, bDown);
	}
}

void ADCCCharacterBase::Server_SetClimbInput_Implementation(bool bUp, bool bDown)
{
	bClimbInputUp = bUp;
	bClimbInputDown = bDown;
}

void ADCCCharacterBase::ReleaseClimb()
{
	if (HasAuthority())
	{
		Server_ReleaseClimb_Implementation();
		return;
	}

	Server_ReleaseClimb();
}

void ADCCCharacterBase::Server_ReleaseClimb_Implementation()
{
	if (!ActiveClimbPoint)
	{
		return;
	}

	ActiveClimbPoint = nullptr;
	OnRep_ActiveClimbPoint();

	bClimbInputUp = false;
	bClimbInputDown = false;
}

void ADCCCharacterBase::OnRep_ActiveClimbPoint()
{
	UCharacterMovementComponent* Movement = GetCharacterMovement();
	if (!Movement)
	{
		return;
	}

	if (ActiveClimbPoint)
	{
		// Flying rather than a custom movement mode: gravity has to stop pulling,
		// but the capsule must still sweep so the climber cannot pass through the
		// cliff or the ground.
		Movement->StopMovementImmediately();
		Movement->SetMovementMode(MOVE_Flying);
	}
	else
	{
		// Falling, not walking - letting go halfway up a rock face should drop
		// the climber, and MOVE_Walking at that height would leave them standing
		// on nothing until something nudged them.
		Movement->SetMovementMode(MOVE_Falling);
	}
}

void ADCCCharacterBase::SetSprintInput(bool bWantsToSprint)
{
	if (HasAuthority())
	{
		Server_SetSprinting_Implementation(bWantsToSprint);
		return;
	}

	// Applied locally as well as sent, so the local player's movement responds on
	// the frame they pressed the key rather than after a round trip.
	bIsSprinting = bWantsToSprint;
	ApplyDerivedMovementSpeed();

	Server_SetSprinting(bWantsToSprint);
}

void ADCCCharacterBase::Server_SetSprinting_Implementation(bool bNewSprinting)
{
	if (bIsSprinting == bNewSprinting)
	{
		return;
	}

	bIsSprinting = bNewSprinting;
	ApplyDerivedMovementSpeed();
}

void ADCCCharacterBase::OnRep_Sprinting()
{
	ApplyDerivedMovementSpeed();
}

void ADCCCharacterBase::TickStamina(float DeltaSeconds)
{
	UDCCRuleset* Ruleset = Stats ? Stats->GetRuleset() : nullptr;
	if (!Stats || !Ruleset)
	{
		return;
	}

	const int32 ClimbRank = (Skills && ActiveClimbPoint)
		? Skills->GetSkillRank(ActiveClimbPoint->ClimbSkillId)
		: 0;

	bool bExerting = false;

	if (ActiveClimbPoint)
	{
		bExerting = true;

		const bool bMoving = bClimbInputUp != bClimbInputDown;
		const float Cost = bMoving
			? Ruleset->GetClimbStaminaCost(ClimbRank)
			: Ruleset->GetClingStaminaCost(ClimbRank);

		if (bMoving)
		{
			const float Speed = Ruleset->GetClimbSpeed(Stats->AbilityScores, ClimbRank);
			const float Direction = bClimbInputUp ? 1.0f : -1.0f;

			const float PreviousZ = GetActorLocation().Z;

			float NewZ = PreviousZ + (Speed * Direction * DeltaSeconds);
			NewZ = FMath::Clamp(NewZ, ActiveClimbPoint->GetBottomZ(), ActiveClimbPoint->GetTopZ());

			SetActorLocation(ActiveClimbPoint->GetAttachLocationFor(this, NewZ));

			// Accumulated in centimetres and banked a metre at a time, because
			// recording a fractional metre every frame would only ever add zero.
			ClimbDistanceAccumulator += FMath::Abs(NewZ - PreviousZ);
			if (ClimbDistanceAccumulator >= 100.0f)
			{
				const int32 WholeMetres = FMath::FloorToInt32(ClimbDistanceAccumulator / 100.0f);

				UDCCAchievementComponent::RecordFor(this, EDCCAchievementStat::MetresClimbed, WholeMetres);
				UDCCQuestComponent::NotifyEvent(this, EDCCObjectiveType::Climb, NAME_None, WholeMetres);

				ClimbDistanceAccumulator -= WholeMetres * 100.0f;
			}

			// Reaching the lip finishes the climb rather than leaving the player
			// stuck at the top holding a button with nothing left to climb.
			if (NewZ >= ActiveClimbPoint->GetTopZ() - 1.0f)
			{
				const FVector Dismount = ActiveClimbPoint->GetDismountLocation(this);

				ADCCGameState::Log(this, EDCCLogCategory::Encounter, FText::Format(
					NSLOCTEXT("DCC", "ClimbTop", "{0} hauls themselves over the top."),
					FText::FromString(GetName())));

				Server_ReleaseClimb_Implementation();
				SetActorLocation(Dismount);

				if (UCharacterMovementComponent* Movement = GetCharacterMovement())
				{
					Movement->SetMovementMode(MOVE_Walking);
				}
				return;
			}
		}

		if (!Stats->ConsumeStamina(Cost * DeltaSeconds))
		{
			// Arms gave out. Falling is the honest outcome, and the reason the
			// stamina bar is worth watching.
			ADCCGameState::Log(this, EDCCLogCategory::Encounter, FText::Format(
				NSLOCTEXT("DCC", "ClimbFall", "{0}'s grip fails. Gravity is a sponsor."),
				FText::FromString(GetName())));

			Server_ReleaseClimb_Implementation();
		}
	}
	else if (bIsSprinting)
	{
		// Only charge for sprinting that is actually happening: holding Shift
		// while standing still should not drain anything.
		if (GetVelocity().Size2D() > 10.0f)
		{
			bExerting = true;

			if (!Stats->ConsumeStamina(
				Ruleset->GetSprintStaminaCost(Stats->AbilityScores) * DeltaSeconds))
			{
				Server_SetSprinting_Implementation(false);
			}
		}
	}

	if (!bExerting)
	{
		Stats->RecoverStamina(Ruleset->GetStaminaRegenRate(Stats->AbilityScores) * DeltaSeconds);
	}
}

void ADCCCharacterBase::ToggleCharacterMenu()
{
	const APlayerController* PC = Cast<APlayerController>(GetController());
	if (ADCCHUD* DCCHUD = PC ? Cast<ADCCHUD>(PC->GetHUD()) : nullptr)
	{
		DCCHUD->ToggleMenu();
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

		UDCCAchievementComponent::RecordFor(this, EDCCAchievementStat::DamageDealt, Result.Damage);
		UDCCAchievementComponent::RecordFor(Target, EDCCAchievementStat::DamageTaken, Result.Damage);

		// The hook that lets a Debt Eater bite gold out of whoever it hit. Virtual
		// and empty here rather than a property, so nothing that is not an enemy
		// carries the concept at all.
		OnDamageDealt(Target, Result.Damage);

		// Everything below is credited immediately after the damage lands, which is
		// the only moment this character can be tied to the kill - ApplyDamage does
		// not report who dealt it, so attribution has to happen here.
		if (!TargetStats->IsAlive())
		{
			AwardKillExperience(Target);

			if (EmoteChanceOnKill > 0.0f && !EmoteAnimations.IsEmpty() && FMath::FRand() <= EmoteChanceOnKill)
			{
				Multicast_PlayEmote();
			}
		}
	}

	// Narrate what the dice already decided. Per the tone bible the System AI is a
	// commentator, never a referee - so this restates the resolved outcome and
	// never influences it.
	{
		const FText AttackerName = FText::FromString(GetName());
		const FText TargetName = FText::FromString(Target->GetName());

		FText Line;
		if (Result.Check.bCriticalSuccess)
		{
			Line = FText::Format(
				NSLOCTEXT("DCC", "LogCrit", "{0} crits {1} for {2}. Chat is going to replay that one."),
				AttackerName, TargetName, FText::AsNumber(Result.Damage));
		}
		else if (Result.bHit)
		{
			Line = FText::Format(
				NSLOCTEXT("DCC", "LogHit", "{0} hits {1} for {2}."),
				AttackerName, TargetName, FText::AsNumber(Result.Damage));
		}
		else
		{
			Line = FText::Format(
				NSLOCTEXT("DCC", "LogMiss", "{0} misses {1}. Rolled {2} against {3}."),
				AttackerName, TargetName,
				FText::AsNumber(Result.Check.Total), FText::AsNumber(Result.Check.DifficultyClass));
		}

		ADCCGameState::Log(this, EDCCLogCategory::Combat, Line);
	}

	Multicast_PlayAttack(Target, Result.bHit);

	return Result;
}

void ADCCCharacterBase::AwardKillExperience(ADCCCharacterBase* Victim)
{
	if (!HasAuthority() || !Victim || !Stats || !Victim->Stats)
	{
		return;
	}

	UDCCAchievementComponent::RecordFor(this, EDCCAchievementStat::Kills);

	// Reported by class name so a quest can ask for a specific kind of enemy -
	// "put down three Brutes" - while an objective with an empty TargetId still
	// counts anything at all.
	UDCCQuestComponent::NotifyEvent(this, EDCCObjectiveType::Kill,
		Victim->GetClass() ? Victim->GetClass()->GetFName() : NAME_None);

	UDCCRuleset* Ruleset = Stats->GetRuleset();
	if (!Ruleset)
	{
		return;
	}

	const int32 Gained = Ruleset->GetExperienceForKill(Victim->Stats->Level, Stats->Level);
	if (Gained <= 0)
	{
		return;
	}

	const int32 LevelBefore = Stats->Level;
	Stats->AddExperience(Gained);

	ADCCGameState::Log(this, EDCCLogCategory::Experience, FText::Format(
		NSLOCTEXT("DCC", "LogKillXP", "{0} banks {1} XP. The sponsors noticed."),
		FText::FromString(GetActorNameOrLabel()), FText::AsNumber(Gained)));

	if (Stats->Level > LevelBefore)
	{
		AnnounceLevelUp(LevelBefore);
	}
}

void ADCCCharacterBase::AnnounceLevelUp(int32 PreviousLevel)
{
	if (!Stats)
	{
		return;
	}

	UDCCAchievementComponent::RecordFor(this, EDCCAchievementStat::LevelsGained,
		Stats->Level - PreviousLevel);

	ADCCGameState::Log(this, EDCCLogCategory::Experience, FText::Format(
		NSLOCTEXT("DCC", "LogLevelUp", "{0} reaches level {1}. Contract terms improve accordingly."),
		FText::FromString(GetActorNameOrLabel()), FText::AsNumber(Stats->Level)));
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
	PlayerInputComponent->BindKey(EKeys::LeftMouseButton, IE_Pressed, this, &ADCCCharacterBase::OnPrimaryPressed);

	// Space is the template's jump; P keeps the pause off anything the third
	// person template already claims.
	PlayerInputComponent->BindKey(EKeys::P, IE_Pressed, this, &ADCCCharacterBase::RequestTogglePause);

	// E lifts and drops, R throws. Two keys rather than one because "put this
	// down carefully" and "hurl this at the thing trying to eat me" are different
	// intentions, and a single key would guess wrong at the worst moment.
	PlayerInputComponent->BindKey(EKeys::E, IE_Pressed, this, &ADCCCharacterBase::AttemptPickupOrDrop);
	PlayerInputComponent->BindKey(EKeys::R, IE_Pressed, this, &ADCCCharacterBase::AttemptThrow);

	// F stows what is in hand; Tab opens the character menu. Tab is the near
	// universal convention for an inventory screen, so it costs no explaining.
	PlayerInputComponent->BindKey(EKeys::F, IE_Pressed, this, &ADCCCharacterBase::AttemptStow);
	PlayerInputComponent->BindKey(EKeys::Tab, IE_Pressed, this, &ADCCCharacterBase::ToggleCharacterMenu);

	// Left click is context sensitive, which is why it routes through a handler
	// rather than binding straight to the attack: with a skill armed it picks a
	// target, while climbing it hauls upward, and otherwise it swings.
	PlayerInputComponent->BindKey(EKeys::LeftMouseButton, IE_Released, this, &ADCCCharacterBase::OnPrimaryReleased);
	PlayerInputComponent->BindKey(EKeys::RightMouseButton, IE_Pressed, this, &ADCCCharacterBase::OnSecondaryPressed);
	PlayerInputComponent->BindKey(EKeys::RightMouseButton, IE_Released, this, &ADCCCharacterBase::OnSecondaryReleased);

	// Answering the confirmation box.
	PlayerInputComponent->BindKey(EKeys::Y, IE_Pressed, this, &ADCCCharacterBase::AcceptPrompt);
	PlayerInputComponent->BindKey(EKeys::N, IE_Pressed, this, &ADCCCharacterBase::DeclinePrompt);

	// Space lets go of a wall. It is the template's jump key, which is harmless
	// here: while climbing the movement mode is flying, so the jump does nothing
	// and the key is free to mean the one thing a climber wants from it.
	PlayerInputComponent->BindKey(EKeys::SpaceBar, IE_Pressed, this, &ADCCCharacterBase::ReleaseClimbIfClimbing);

	// Sprint on Shift, held.
	PlayerInputComponent->BindKey(EKeys::LeftShift, IE_Pressed, this, &ADCCCharacterBase::StartSprint);
	PlayerInputComponent->BindKey(EKeys::LeftShift, IE_Released, this, &ADCCCharacterBase::StopSprint);

	// Number keys arm a skill from the menu list. Bound one at a time because
	// BindKey takes no payload, and a lambda per key would not survive a
	// UFUNCTION binding.
	PlayerInputComponent->BindKey(EKeys::One, IE_Pressed, this, &ADCCCharacterBase::SelectSkillSlot1);
	PlayerInputComponent->BindKey(EKeys::Two, IE_Pressed, this, &ADCCCharacterBase::SelectSkillSlot2);
	PlayerInputComponent->BindKey(EKeys::Three, IE_Pressed, this, &ADCCCharacterBase::SelectSkillSlot3);
	PlayerInputComponent->BindKey(EKeys::Four, IE_Pressed, this, &ADCCCharacterBase::SelectSkillSlot4);
	PlayerInputComponent->BindKey(EKeys::Five, IE_Pressed, this, &ADCCCharacterBase::SelectSkillSlot5);
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

void ADCCCharacterBase::PlayOneShotAnimation(UAnimationAsset* Animation, float OverrideDurationSeconds)
{
	USkeletalMeshComponent* MeshComponent = GetMesh();
	UWorld* World = GetWorld();
	if (!MeshComponent || !Animation || !World)
	{
		return;
	}

	// Only meaningful in single-node mode. With an AnimBlueprint driving the mesh
	// this call would be ignored anyway, so bail rather than fight it.
	if (MeshComponent->GetAnimationMode() != EAnimationMode::AnimationSingleNode)
	{
		return;
	}

	MeshComponent->PlayAnimation(Animation, false);

	float Duration = 1.0f;
	if (const UAnimSequenceBase* Sequence = Cast<UAnimSequenceBase>(Animation))
	{
		Duration = FMath::Max(0.05f, Sequence->GetPlayLength());
	}

	// Squeeze or stretch the clip to fill a required window - a two second climb
	// should not play a half second animation and then stand still.
	if (OverrideDurationSeconds > 0.0f)
	{
		MeshComponent->SetPlayRate(Duration / OverrideDurationSeconds);
		Duration = OverrideDurationSeconds;
	}
	else
	{
		MeshComponent->SetPlayRate(1.0f);
	}

	// Tick restores the loop once this deadline passes. Clearing the cached loop
	// forces it to re-apply rather than deciding nothing has changed.
	OneShotAnimationUntil = World->GetTimeSeconds() + Duration;
	ActiveLoopAnimation = nullptr;
}

void ADCCCharacterBase::PlayEmote()
{
	if (EmoteAnimations.IsEmpty())
	{
		return;
	}

	const int32 Index = FMath::RandRange(0, EmoteAnimations.Num() - 1);
	PlayOneShotAnimation(EmoteAnimations[Index]);
}

UAnimationAsset* ADCCCharacterBase::SelectLocomotionAnimation() const
{
	// Climbing overrides everything below it. It is picked from the replicated
	// climb point rather than from a one-shot, so it loops for as long as the
	// character is actually on the wall and every client agrees on it.
	if (IsClimbing() && ClimbAnimation)
	{
		return ClimbAnimation;
	}

	// Horizontal speed only: falling should not read as sprinting.
	const float Speed = GetVelocity().Size2D();

	if (Speed >= RunSpeedThreshold && RunAnimation)
	{
		return RunAnimation;
	}

	if (Speed >= WalkSpeedThreshold)
	{
		// Fall through to the run cycle when no walk clip exists, so a character
		// that only has a run still animates while moving.
		return WalkAnimation ? WalkAnimation.Get() : RunAnimation.Get();
	}

	// Standing still. Idle if authored, otherwise walk, otherwise run - something
	// looping is better than a frozen T-pose.
	if (IdleAnimation)
	{
		return IdleAnimation;
	}

	return WalkAnimation ? WalkAnimation.Get() : RunAnimation.Get();
}

void ADCCCharacterBase::UpdateLocomotionAnimation()
{
	USkeletalMeshComponent* MeshComponent = GetMesh();
	UWorld* World = GetWorld();
	if (!MeshComponent || !World)
	{
		return;
	}

	if (MeshComponent->GetAnimationMode() != EAnimationMode::AnimationSingleNode)
	{
		return;
	}

	// A one-shot owns the mesh until it finishes.
	if (World->GetTimeSeconds() < OneShotAnimationUntil)
	{
		return;
	}

	UAnimationAsset* Desired = SelectLocomotionAnimation();
	if (!Desired || Desired == ActiveLoopAnimation)
	{
		return;
	}

	MeshComponent->SetPlayRate(1.0f);
	MeshComponent->PlayAnimation(Desired, true);
	ActiveLoopAnimation = Desired;
}

void ADCCCharacterBase::Tick(float DeltaSeconds)
{
	Super::Tick(DeltaSeconds);

	// Runs on every machine: locomotion is cosmetic and each client should pick
	// its clip from the movement it can already see, rather than replicating a
	// state that velocity already implies.
	UpdateLocomotionAnimation();

	// Stamina is mechanical, so only the server spends it. Clients see the
	// result through the replicated value.
	if (HasAuthority())
	{
		TickStamina(DeltaSeconds);

		if (Stats)
		{
			Stats->TickRegeneration(DeltaSeconds);
		}
	}

	// The nameplate is billboarded here rather than through a rotating component,
	// because it only ever needs to face whoever is looking at this machine - the
	// server's idea of "towards the camera" is meaningless.
	//
	// GetFirstLocalPlayerController, not GetFirstPlayerController: the latter
	// returns the HOST on a listen server, so every remote client's nameplates
	// would turn to face the host's camera instead of their own.
	if (Nameplate && Nameplate->IsVisible())
	{
		if (const APlayerController* PC = GEngine ? GEngine->GetFirstLocalPlayerController(GetWorld()) : nullptr)
		{
			FVector ViewLocation;
			FRotator ViewRotation;
			PC->GetPlayerViewPoint(ViewLocation, ViewRotation);

			Nameplate->SetWorldRotation((Nameplate->GetComponentLocation() - ViewLocation).Rotation());
		}
	}
}

void ADCCCharacterBase::PlayClimbAnimation(float DurationSeconds)
{
	PlayOneShotAnimation(ClimbAnimation, DurationSeconds);
}

void ADCCCharacterBase::Multicast_PlayEmote_Implementation()
{
	PlayEmote();

	ADCCGameState::Log(this, EDCCLogCategory::Combat, FText::Format(
		NSLOCTEXT("DCC", "LogEmote", "{0} plays to the cameras. Chat approves."),
		FText::FromString(GetName())));
}

void ADCCCharacterBase::Multicast_PlayAttack_Implementation(ADCCCharacterBase* Target, bool bHit)
{
	// Runs on every machine, which is what finally makes swings visible: the
	// attack itself resolves server-side, but the clip has to play locally.
	PlayOneShotAnimation(AttackAnimation);

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

	// The nameplate outlives the character otherwise, hanging in the air over a
	// ragdoll and still cheerfully reading "ALLY".
	if (Nameplate)
	{
		Nameplate->SetVisibility(false);
	}

	if (UCapsuleComponent* Capsule = GetCapsuleComponent())
	{
		Capsule->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	}

	if (UCharacterMovementComponent* Movement = GetCharacterMovement())
	{
		Movement->StopMovementImmediately();
		Movement->DisableMovement();
	}

	// Ragdoll rather than a death animation. None of the imported characters ship
	// one, and physics gives a different fall every time - which reads better
	// than the same canned collapse on every mob in the room. Every skeletal mesh
	// already has a physics asset from import, so this costs nothing extra.
	if (USkeletalMeshComponent* MeshComponent = GetMesh())
	{
		// Detach from the capsule first: while attached, the capsule's transform
		// keeps overriding the simulation and the body twitches in place instead
		// of falling.
		MeshComponent->SetCollisionEnabled(ECollisionEnabled::QueryAndPhysics);
		MeshComponent->SetCollisionObjectType(ECC_PhysicsBody);
		MeshComponent->SetCollisionResponseToAllChannels(ECR_Block);
		MeshComponent->SetCollisionResponseToChannel(ECC_Pawn, ECR_Ignore);

		MeshComponent->SetAllBodiesSimulatePhysics(true);
		MeshComponent->SetSimulatePhysics(true);
		MeshComponent->WakeAllRigidBodies();
	}

	// Detaching the controller stops AI from continuing to issue orders to a
	// corpse, and stops a dead player from steering it.
	if (HasAuthority())
	{
		// A crawler going down is a bigger beat than a mob dying, and the tone
		// bible is explicit that the AI gets more corporate rather than warmer
		// when something actually matters.
		ADCCGameState::Log(this, EDCCLogCategory::Death, IsCrawler()
			? FText::Format(
				NSLOCTEXT("DCC", "LogCrawlerDown", "{0} is down. Our thoughts are with their sponsors."),
				FText::FromString(GetName()))
			: FText::Format(
				NSLOCTEXT("DCC", "LogMobDown", "{0} has been retired."),
				FText::FromString(GetName())));

		UDCCAchievementComponent::RecordFor(this, EDCCAchievementStat::Deaths);

		DetachFromControllerPendingDestroy();

		if (CorpseLifeSpan > 0.0f)
		{
			SetLifeSpan(CorpseLifeSpan);
		}
	}

	BP_OnDied();

	OnCharacterDied.Broadcast(this);
}
