#include "Characters/DCCCharacterBase.h"

#include "Stats/DCCStatsComponent.h"
#include "Stats/DCCRuleset.h"
#include "Skills/DCCSkillComponent.h"
#include "Combat/DCCGameState.h"
#include "Combat/DCCBossComponent.h"
#include "Characters/DCCEnemyCharacter.h"
#include "Favor/DCCFavorComponent.h"
#include "Items/DCCCarryComponent.h"
#include "Items/DCCInventoryComponent.h"
#include "Items/DCCPickup.h"
#include "UI/DCCFloatingText.h"
#include "UI/DCCHUD.h"
#include "Rooms/DCCClimbPoint.h"
#include "Progression/DCCAchievementComponent.h"
#include "Equipment/DCCEquipmentComponent.h"
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
#include "Kismet/GameplayStatics.h"
#include "Camera/CameraComponent.h"
#include "Camera/PlayerCameraManager.h"
#include "UnrealClient.h"

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
	Equipment = CreateDefaultSubobject<UDCCEquipmentComponent>(TEXT("Equipment"));

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

	SelfieCamera = CreateDefaultSubobject<UCameraComponent>(TEXT("SelfieCamera"));
	SelfieCamera->SetupAttachment(RootComponent);
	SelfieCamera->SetRelativeLocation(SelfieCameraOffset);
	SelfieCamera->SetRelativeRotation(SelfieCameraRotation);

	// Off until Selfie Mode actually opens - see ToggleSelfieMode. An always-on
	// second camera would fight whatever the character Blueprint's own rig is
	// doing every frame for no reason.
	SelfieCamera->SetActive(false);
}

void ADCCCharacterBase::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);

	DOREPLIFETIME(ADCCCharacterBase, ActiveClimbPoint);
	DOREPLIFETIME(ADCCCharacterBase, bIsSprinting);
	DOREPLIFETIME(ADCCCharacterBase, bIsCasting);
	DOREPLIFETIME(ADCCCharacterBase, bBefriended);
	DOREPLIFETIME(ADCCCharacterBase, bIsDowned);
	DOREPLIFETIME(ADCCCharacterBase, DeathSaveSuccesses);
	DOREPLIFETIME(ADCCCharacterBase, DeathSaveFailures);
	DOREPLIFETIME(ADCCCharacterBase, bHasLoot);
	DOREPLIFETIME(ADCCCharacterBase, PendingLoot);
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

	// Captured before anything can ragdoll. Reviving reattaches the mesh to the
	// capsule and needs the offset it had at rest - read it back after a ragdoll
	// and you get wherever physics dumped the body, which stands the revived
	// crawler up embedded in the floor.
	if (const USkeletalMeshComponent* MeshComponent = GetMesh())
	{
		MeshRestTransform = MeshComponent->GetRelativeTransform();
	}

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

void ADCCCharacterBase::SetMovementSpeedOverride(float NewSpeed)
{
	const float Clamped = FMath::Max(0.0f, NewSpeed);
	if (FMath::IsNearlyEqual(MovementSpeedOverride, Clamped))
	{
		return;
	}

	MovementSpeedOverride = Clamped;

	// Re-derived immediately rather than waiting for the next Tick, so the change
	// is visible on the frame it was asked for.
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

	float Speed = BaseWalkSpeed * Ruleset->GetMoveSpeedMultiplier(Stats->GetEnhanced());

	if (bIsSprinting)
	{
		Speed *= Ruleset->GetSprintSpeedMultiplier(Stats->GetEnhanced());
	}

	// Applied before the casting clamp so a rooted caster is still rooted, and
	// after the sprint multiplier so an escorting ally cannot sprint by accident.
	if (MovementSpeedOverride > 0.0f)
	{
		Speed = FMath::Min(Speed, MovementSpeedOverride);
	}

	// Routed through the same funnel as sprint rather than poking MaxWalkSpeed
	// from the skill component, so the two can never disagree about who set the
	// speed last. A sprint multiplier applied to zero is still zero.
	if (bIsCasting)
	{
		Speed = 0.0f;
	}

	Movement->MaxWalkSpeed = Speed;
}

void ADCCCharacterBase::BeginCastLock(float Seconds)
{
	if (!HasAuthority() || Seconds <= 0.0f)
	{
		return;
	}

	bIsCasting = true;
	ApplyDerivedMovementSpeed();

	// Velocity is killed outright as well as capped: MaxWalkSpeed bounds new
	// input, but a character already sliding keeps its momentum otherwise, and a
	// caster drifting through their own cast reads as a bug.
	if (UCharacterMovementComponent* Movement = GetCharacterMovement())
	{
		Movement->StopMovementImmediately();
	}

	// Re-arming replaces the pending timer rather than stacking, so a second cast
	// mid-cast extends the root instead of ending it early.
	GetWorldTimerManager().ClearTimer(CastLockTimer);
	GetWorldTimerManager().SetTimer(CastLockTimer, this,
		&ADCCCharacterBase::EndCastLock, Seconds, false);
}

void ADCCCharacterBase::EndCastLock()
{
	if (!HasAuthority())
	{
		return;
	}

	bIsCasting = false;
	ApplyDerivedMovementSpeed();
}

void ADCCCharacterBase::OnRep_Casting()
{
	ApplyDerivedMovementSpeed();
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

void ADCCCharacterBase::DCCEmptyStowedRemains(int32 Index)
{
	if (HasAuthority())
	{
		Server_EmptyStowedRemains_Implementation(Index);
	}
	else
	{
		Server_EmptyStowedRemains(Index);
	}
}

void ADCCCharacterBase::Server_EmptyStowedRemains_Implementation(int32 Index)
{
	if (Inventory)
	{
		Inventory->LootAllFromStowed(Index, GetActorLocation());
	}
}

void ADCCCharacterBase::OnPrimaryPressed()
{
	// The character menu owns the mouse while it is open, so a click meant for an
	// achievement row does not also swing at whatever happens to be behind the
	// panel. Checked first: this outranks even climbing, because the cursor is
	// visibly over a UI panel and clicking UI should never move the character.
	if (const APlayerController* PC = Cast<APlayerController>(GetController()))
	{
		if (const ADCCHUD* DCCHUD = Cast<ADCCHUD>(PC->GetHUD()))
		{
			if (DCCHUD->IsMenuVisible())
			{
				return;
			}
		}
	}

	// Posing for a photo wins too - a stray click should not swing a weapon
	// while the character is holding still smiling at their own phone.
	if (bInSelfieMode)
	{
		return;
	}

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

	// With a skill armed from the menu, a climbable face under the cursor always
	// wins the click, whatever skill is armed - see TrySelectSkillTargetUnderCursor's
	// comment for why. Otherwise the click casts.
	if (!SelectedSkillId.IsNone())
	{
		if (TrySelectClimbTargetUnderCursor())
		{
			return;
		}

		TrySelectSkillTargetUnderCursor();
		return;
	}

	// A friendly under the crosshair turns the click into a greeting. Checked
	// before the attack because IsHostileTo already refuses to swing at an ally,
	// so this consumes a click that would otherwise have done nothing at all.
	if (TryFistBumpUnderCursor())
	{
		return;
	}

	// Hitting is the default action with nothing armed: a hostile in range
	// always wins the click over climbing, so an ordinary attack click near a
	// cave wall - which describes most of this game's combat rooms - is never
	// mistaken for a climb attempt. Climbing is only probed for when there is
	// nothing to fight, which is also the one situation where "the click did
	// nothing" would otherwise be true instead of a swing-and-miss.
	if (!FindNearestHostileInRange() && TrySelectClimbTargetUnderCursor())
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

bool ADCCCharacterBase::TrySelectClimbTargetUnderCursor()
{
	// Purely local: this decides what to *ask*, never what happens.
	APlayerController* PC = Cast<APlayerController>(GetController());
	UWorld* World = GetWorld();
	if (!PC || !World || IsClimbing())
	{
		return false;
	}

	FVector CameraLocation;
	FRotator CameraRotation;
	PC->GetPlayerViewPoint(CameraLocation, CameraRotation);

	const FVector TraceEnd = CameraLocation + CameraRotation.Vector() * 5000.0f;

	FCollisionQueryParams Params;
	Params.AddIgnoredActor(this);
	Params.bTraceComplex = true;

	FHitResult Hit;
	if (!World->LineTraceSingleByChannel(Hit, CameraLocation, TraceEnd, ECC_Visibility, Params))
	{
		return false;
	}

	// An authored climb point, exactly as before: raises the confirmation box.
	if (ADCCClimbPoint* Point = Cast<ADCCClimbPoint>(Hit.GetActor()))
	{
		if (Point->IsWithinReach(this))
		{
			PendingClimbPoint = Point;
			return true;
		}

		ShowLocalNotice(NSLOCTEXT("DCC", "ClimbTooFar", "Too far away to climb that."));
		return true;
	}

	// Not a climb point - a character under the cursor is a combat/social click,
	// never a climb attempt, whatever its collision does to Visibility traces.
	if (Cast<ADCCCharacterBase>(Hit.GetActor()))
	{
		return false;
	}

	// Wall-like means the hit face is closer to vertical than horizontal. A
	// normal near (0,0,1) is a floor, near (0,0,-1) a ceiling - neither is
	// something a player is trying to climb by clicking it.
	if (!Hit.bBlockingHit || FMath::Abs(Hit.Normal.Z) > 0.5f)
	{
		return false;
	}

	// Also excludes anything at foot level: cave floors are irregular rock, not
	// flat tile, so a pebble or a sloped seam right underfoot can present a
	// steep-enough local normal to pass the check above despite obviously being
	// ground, not a wall. Requiring the hit to sit meaningfully above the
	// character's own feet is what keeps that from being read as a climb
	// attempt during ordinary movement across rough terrain.
	const float FootZ = GetActorLocation().Z
		- (GetCapsuleComponent() ? GetCapsuleComponent()->GetScaledCapsuleHalfHeight() : 90.0f);

	if (Hit.Location.Z < FootZ + 30.0f)
	{
		return false;
	}

	// This is a genuine climb attempt at un-authored geometry now - report
	// failure rather than silently doing nothing if it is out of reach, same
	// as the authored case above.
	if (FVector::Dist(Hit.Location, GetActorLocation()) > GenericClimbReachDistance)
	{
		ShowLocalNotice(NSLOCTEXT("DCC", "ClimbTooFarGeneric", "Too far away to climb that."));
		return true;
	}

	// No confirmation for a generic surface - the click itself is the ask, and
	// the server is the one deciding whether a wall genuinely exists there.
	if (HasAuthority())
	{
		Server_RequestGenericClimb_Implementation(Hit.Location, Hit.Normal);
	}
	else
	{
		Server_RequestGenericClimb(Hit.Location, Hit.Normal);
	}

	return true;
}

AActor* ADCCCharacterBase::FindSkillTargetUnderCursor(FName SkillId) const
{
	if (!Skills || SkillId.IsNone())
	{
		return nullptr;
	}

	FDCCSkillDef Skill;
	if (!Skills->FindSkill(SkillId, Skill))
	{
		return nullptr;
	}

	// Self, Cone and AreaOfEffect all resolve from the caster's own position and
	// facing server-side (see UDCCSkillComponent::GatherTargets) - there is no
	// specific actor to click, only a direction, so the caster stands in as the
	// nominal target and the cast fires on confirm.
	if (Skill.Targeting != EDCCSkillTargeting::SingleTarget)
	{
		return const_cast<ADCCCharacterBase*>(this);
	}

	const APlayerController* PC = Cast<APlayerController>(GetController());
	const UWorld* World = GetWorld();
	if (!PC || !World)
	{
		return nullptr;
	}

	FVector CameraLocation;
	FRotator CameraRotation;
	PC->GetPlayerViewPoint(CameraLocation, CameraRotation);

	const float TraceDistance = FMath::Max(Skill.Range, 100.0f);
	const FVector TraceEnd = CameraLocation + CameraRotation.Vector() * TraceDistance;

	FCollisionQueryParams Params;
	Params.AddIgnoredActor(this);

	// A sphere sweep rather than a thin ray, matching the fist bump and the
	// interaction target sweep - a hair-thin raycast at close range (most
	// skill ranges are melee-ish) demands near-pixel-perfect aim to ever
	// register a hit, which is what "not sure it's locking onto a target" was.
	// Pawn channel rather than Visibility: a spell aimed at a person should not
	// be blocked from resolving by a lamp post the sweep happens to graze, and
	// it means a wall under the cursor correctly reads as "no target" rather
	// than "target: wall".
	FHitResult Hit;
	if (!World->SweepSingleByChannel(Hit, CameraLocation, TraceEnd, FQuat::Identity,
		ECC_Pawn, FCollisionShape::MakeSphere(40.0f), Params))
	{
		return nullptr;
	}

	ADCCCharacterBase* HitCharacter = Cast<ADCCCharacterBase>(Hit.GetActor());
	if (!HitCharacter || !HitCharacter->IsAlive() || !IsHostileTo(HitCharacter))
	{
		return nullptr;
	}

	return HitCharacter;
}

void ADCCCharacterBase::TrySelectSkillTargetUnderCursor()
{
	if (SelectedSkillId.IsNone())
	{
		return;
	}

	AActor* Target = FindSkillTargetUnderCursor(SelectedSkillId);
	if (!Target)
	{
		// Armed but nothing valid under the cursor yet - stay armed and wait for
		// a better click, the same way an unanswered climb prompt would.
		//
		// Now says so. This silently returning was the single biggest reason
		// skills felt broken: an armed skill, a click, and absolutely no
		// indication that anything had been considered at all.
		FDCCSkillDef Definition;
		const bool bFound = Skills && Skills->FindSkill(SelectedSkillId, Definition);

		ShowLocalNotice(DescribeSkillFailure(EDCCSkillFailure::NoTarget,
			bFound && !Definition.DisplayName.IsEmpty()
				? Definition.DisplayName
				: FText::FromName(SelectedSkillId)));
		return;
	}

	ResolveOrQueueSkill(SelectedSkillId, Target);
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

void ADCCCharacterBase::Server_RequestGenericClimb_Implementation(FVector HitLocation, FVector HitNormal)
{
	// Everything Server_BeginClimb already re-validates (alive, not already
	// climbing, stamina) still applies once a point exists - this only adds the
	// checks specific to trusting an un-authored location at all.
	if (!IsAlive() || IsClimbing())
	{
		return;
	}

	if (FVector::Dist(HitLocation, GetActorLocation()) > GenericClimbReachDistance + 50.0f)
	{
		Client_ShowClimbNotice(NSLOCTEXT("DCC", "ClimbTooFarServer", "Too far away to climb that."));
		return;
	}

	// The client's hit is a nomination, not a fact: confirm solid geometry is
	// genuinely near HitLocation before trusting it enough to spawn anything,
	// the same way a nominated skill target is re-checked rather than assumed.
	UWorld* World = GetWorld();
	if (!World)
	{
		return;
	}

	const FVector Normal = HitNormal.IsNearlyZero() ? FVector::ForwardVector : HitNormal.GetSafeNormal();
	const FVector ProbeStart = HitLocation + Normal * 50.0f;
	const FVector ProbeEnd = HitLocation - Normal * 50.0f;

	FCollisionQueryParams Params;
	Params.AddIgnoredActor(this);
	Params.bTraceComplex = true;

	FHitResult Confirm;
	if (!World->LineTraceSingleByChannel(Confirm, ProbeStart, ProbeEnd, ECC_Visibility, Params))
	{
		Client_ShowClimbNotice(NSLOCTEXT("DCC", "ClimbNothingThere", "There is nothing to climb there."));
		return;
	}

	ADCCClimbPoint* Point = ADCCClimbPoint::FindOrCreateForSurfaceHit(World, Confirm.Location, Confirm.Normal, this);
	if (!Point)
	{
		Client_ShowClimbNotice(NSLOCTEXT("DCC", "ClimbNothingThere", "There is nothing to climb there."));
		return;
	}

	Server_BeginClimb_Implementation(Point);
}

void ADCCCharacterBase::Client_ShowClimbNotice_Implementation(const FText& Message)
{
	ShowLocalNotice(Message);
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
			const float Speed = Ruleset->GetClimbSpeed(Stats->GetEnhanced(), ClimbRank);
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
				Ruleset->GetSprintStaminaCost(Stats->GetEnhanced()) * DeltaSeconds))
			{
				Server_SetSprinting_Implementation(false);
			}
		}
	}

	if (!bExerting)
	{
		Stats->RecoverStamina(Ruleset->GetStaminaRegenRate(Stats->GetEnhanced()) * DeltaSeconds);
	}
}

bool ADCCCharacterBase::IsHostileNearby() const
{
	UWorld* World = GetWorld();
	if (!World)
	{
		return false;
	}

	const float RadiusSquared = FMath::Square(CombatProximityRadius);

	for (TActorIterator<ADCCCharacterBase> It(World); It; ++It)
	{
		const ADCCCharacterBase* Other = *It;

		// Corpses and downed enemies do not count - a room full of things you
		// already killed is not a fight, and treating it as one would put the
		// confirmation step in front of every post-battle conversation.
		if (!Other || Other == this || !Other->IsAlive() || Other->IsDowned())
		{
			continue;
		}

		if (!IsHostileTo(Other))
		{
			continue;
		}

		if (FVector::DistSquared(GetActorLocation(), Other->GetActorLocation()) <= RadiusSquared)
		{
			return true;
		}
	}

	return false;
}

ADCCCharacterBase* ADCCCharacterBase::GetPendingTalkTarget() const
{
	const UWorld* World = GetWorld();
	if (!PendingTalkTarget || !World || World->GetTimeSeconds() > PendingTalkExpiry)
	{
		return nullptr;
	}

	return PendingTalkTarget;
}

AActor* ADCCCharacterBase::SweepForInteractionCandidate() const
{
	const APlayerController* PC = Cast<APlayerController>(GetController());
	UWorld* World = GetWorld();
	if (!PC || !World)
	{
		return nullptr;
	}

	FVector CameraLocation;
	FRotator CameraRotation;
	PC->GetPlayerViewPoint(CameraLocation, CameraRotation);

	FCollisionQueryParams Params;
	Params.AddIgnoredActor(this);

	// A generous sphere sweep, matching the fist bump. Starting a conversation
	// (or opening a loot menu) should not require better aim than swinging at
	// someone does.
	//
	// Visibility rather than Pawn: ApplyCollapsedBody explicitly sets a
	// ragdolled corpse's mesh to ECR_Ignore on ECC_Pawn (so a dead body's
	// flailing limbs cannot shove a living character around) and disables the
	// capsule outright. That is correct for movement collision and wrong for
	// this sweep - a Pawn-channel trace can never find a body to search, which
	// was the entire "can't loot corpses" bug. The corpse's mesh still blocks
	// Visibility (only Pawn was overridden to Ignore), and a living character's
	// capsule already blocks Visibility too, so this finds both without
	// changing what counts as a valid target. A chest's static mesh blocks
	// Visibility by default, so it needs no special casing here at all.
	FHitResult Hit;
	if (!World->SweepSingleByChannel(
		Hit, CameraLocation, CameraLocation + CameraRotation.Vector() * 2000.0f,
		FQuat::Identity, ECC_Visibility, FCollisionShape::MakeSphere(48.0f), Params))
	{
		return nullptr;
	}

	return Hit.GetActor();
}

ADCCCharacterBase* ADCCCharacterBase::FindInteractionTargetUnderCursor() const
{
	AActor* Hit = SweepForInteractionCandidate();
	ADCCCharacterBase* Target = Cast<ADCCCharacterBase>(Hit);

	// [DCC Talk] - diagnostic for the still-open "can't interact with allies"
	// report. Only fires on an E press that fails to open a conversation, which
	// is not every frame, so this is safe to leave live rather than pull once
	// the next PIE run has answered the question. Prints exactly which of the
	// four gates (nothing hit / not a character / hostile / out of range)
	// refused the attempt, so a repro no longer needs a theory - just a log
	// filtered on this tag.
	if (!Target || Target == this)
	{
		UE_LOG(LogTemp, Log, TEXT("[DCC Talk] %s: sweep hit %s - not a valid character."),
			*GetActorNameOrLabel(), Hit ? *Hit->GetActorNameOrLabel() : TEXT("nothing"));
		return nullptr;
	}

	// Two valid subjects, and the hostility rule only applies to the first: a
	// living friend to talk to, or a downed friend to pick up. A corpse is no
	// longer either - it opens the loot menu instead, via
	// FindLootTargetUnderCursor, so the conversation tree never has to know
	// what a body is.
	const bool bLivingFriend = Target->IsAlive() && !Target->IsDowned() && !Target->IsHostileTo(this);
	const bool bDownedFriend = Target->IsDowned() && !Target->IsHostileTo(this);

	if (!bLivingFriend && !bDownedFriend)
	{
		UE_LOG(LogTemp, Log,
			TEXT("[DCC Talk] %s: hit %s but refused - IsAlive=%d IsDowned=%d IsHostileTo=%d IsCrawler=%d (self IsCrawler=%d)."),
			*GetActorNameOrLabel(), *Target->GetActorNameOrLabel(),
			Target->IsAlive(), Target->IsDowned(), Target->IsHostileTo(this), Target->IsCrawler(), IsCrawler());
		return nullptr;
	}

	const float Distance = FVector::Dist(GetActorLocation(), Target->GetActorLocation());
	if (Distance > InteractionRange)
	{
		UE_LOG(LogTemp, Log, TEXT("[DCC Talk] %s: hit %s but too far - %.0f > InteractionRange %.0f."),
			*GetActorNameOrLabel(), *Target->GetActorNameOrLabel(), Distance, InteractionRange);
		return nullptr;
	}

	return Target;
}

AActor* ADCCCharacterBase::FindLootTargetUnderCursor() const
{
	AActor* Hit = SweepForInteractionCandidate();
	if (!Hit || Hit == this)
	{
		return nullptr;
	}

	// A person only counts once they are dead outright - a living or downed
	// ally is IDCCLootSource-capable in the C++ type system (every character
	// implements the interface, for the corpse case) but is not actually loot
	// while they are still standing or still savable.
	if (const ADCCCharacterBase* AsCharacter = Cast<ADCCCharacterBase>(Hit))
	{
		if (AsCharacter->IsAlive() || AsCharacter->IsDowned())
		{
			return nullptr;
		}
	}

	if (!Hit->Implements<UDCCLootSource>())
	{
		return nullptr;
	}

	if (FVector::Dist(GetActorLocation(), Hit->GetActorLocation()) > InteractionRange)
	{
		return nullptr;
	}

	return Hit;
}

void ADCCCharacterBase::BuildInteractionOptions()
{
	InteractionOptions.Reset();

	if (!InteractionTarget)
	{
		return;
	}

	const bool bFriendly = InteractionTarget->IsBefriended();
	const bool bDowned = InteractionTarget->IsDowned();

	auto Add = [this](EDCCInteractionOption Option, FText Label, bool bAvailable = true,
		FText Reason = FText::GetEmpty())
	{
		FDCCInteractionEntry Entry;
		Entry.Option = Option;
		Entry.Label = MoveTemp(Label);
		Entry.bAvailable = bAvailable;
		Entry.UnavailableReason = MoveTemp(Reason);
		InteractionOptions.Add(MoveTemp(Entry));
	};

	// A corpse never reaches this function any more - FindInteractionTargetUnderCursor
	// stops treating a body as a conversation target, and HandleInteractPressed
	// opens the separate loot menu on it instead. Only a living or downed
	// person gets this far.
	if (bDowned)
	{
		Add(EDCCInteractionOption::Revive,
			NSLOCTEXT("DCC", "InteractRevive", "Help them up"));

		Add(EDCCInteractionOption::Leave,
			NSLOCTEXT("DCC", "InteractLeaveDowned", "Leave them"));

		InteractionIndex = FMath::Clamp(InteractionIndex, 0, InteractionOptions.Num() - 1);
		return;
	}

	Add(EDCCInteractionOption::Chatter,
		NSLOCTEXT("DCC", "InteractChatter", "Talk"));

	Add(EDCCInteractionOption::AskAbout,
		NSLOCTEXT("DCC", "InteractAsk", "Ask about this place"));

	Add(EDCCInteractionOption::Persuade,
		NSLOCTEXT("DCC", "InteractPersuade", "Persuade"));

	Add(EDCCInteractionOption::Intimidate,
		NSLOCTEXT("DCC", "InteractIntimidate", "Intimidate"));

	// Recruiting is gated on already being on good terms. Shown greyed rather
	// than hidden, so the player learns that warming them up is the way in.
	Add(EDCCInteractionOption::Recruit,
		NSLOCTEXT("DCC", "InteractRecruit", "Ask them to join you"),
		bFriendly,
		bFriendly ? FText::GetEmpty()
			: NSLOCTEXT("DCC", "InteractRecruitLocked", "not friendly enough yet"));

	Add(EDCCInteractionOption::Inventory,
		NSLOCTEXT("DCC", "InteractInventory", "Look at their gear"));

	// Party commands only mean anything once they are actually following you.
	Add(EDCCInteractionOption::FollowMe,
		NSLOCTEXT("DCC", "InteractFollow", "Follow me"),
		bFriendly,
		bFriendly ? FText::GetEmpty()
			: NSLOCTEXT("DCC", "InteractFollowLocked", "not in your party"));

	Add(EDCCInteractionOption::HoldPosition,
		NSLOCTEXT("DCC", "InteractHold", "Hold position"),
		bFriendly,
		bFriendly ? FText::GetEmpty()
			: NSLOCTEXT("DCC", "InteractHoldLocked", "not in your party"));

	Add(EDCCInteractionOption::Leave,
		NSLOCTEXT("DCC", "InteractLeave", "Leave"));

	InteractionIndex = FMath::Clamp(InteractionIndex, 0, InteractionOptions.Num() - 1);
}

void ADCCCharacterBase::HandleInteractPressed()
{
	// The loot menu owns E first, if one is open - same reason the
	// conversation menu is checked before anything else below.
	if (LootTargetActor)
	{
		HandleLootMenuConfirm();
		return;
	}

	// Already talking: E confirms the highlighted row.
	if (InteractionTarget)
	{
		if (!InteractionOptions.IsValidIndex(InteractionIndex))
		{
			CloseInteraction();
			return;
		}

		const FDCCInteractionEntry& Entry = InteractionOptions[InteractionIndex];

		if (Entry.Option == EDCCInteractionOption::Leave)
		{
			CloseInteraction();
			return;
		}

		// Unavailable rows stay selectable so their reason can be read, but they
		// do nothing when confirmed.
		if (!Entry.bAvailable)
		{
			return;
		}

		// Walking away mid-sentence ends the conversation rather than silently
		// firing an option at someone out of earshot.
		if (FVector::Dist(GetActorLocation(), InteractionTarget->GetActorLocation()) > InteractionRange)
		{
			CloseInteraction();
			return;
		}

		if (HasAuthority())
		{
			Server_PerformInteraction_Implementation(Entry.Option, InteractionTarget);
		}
		else
		{
			Server_PerformInteraction(Entry.Option, InteractionTarget);
		}

		// Purely a local view - there is nothing for the server to do, and routing
		// it through an RPC would add a round trip before the panel appeared.
		if (Entry.Option == EDCCInteractionOption::Inventory)
		{
			// Toggles, so the same row that opened the panel closes it again.
			InspectedCharacter = (InspectedCharacter == InteractionTarget)
				? nullptr : InteractionTarget.Get();
			return;
		}

		// Options that finish a piece of business close the conversation behind
		// them. Recruiting somebody and then still standing in a menu is the
		// specific thing that felt like being stuck - the verb succeeded, so the
		// menu has done its job and should get out of the way.
		switch (Entry.Option)
		{
		case EDCCInteractionOption::Recruit:
		case EDCCInteractionOption::FollowMe:
		case EDCCInteractionOption::HoldPosition:
		case EDCCInteractionOption::Revive:
			CloseInteraction();
			return;

		default:
			break;
		}

		// The rest are conversational and can be repeated, so the menu stays -
		// rebuilt, because what just ran may have changed what is available.
		BuildInteractionOptions();
		return;
	}

	// Not talking yet: an ally under the crosshair starts a conversation.
	if (ADCCCharacterBase* Target = FindInteractionTargetUnderCursor())
	{
		// Chatting is still allowed mid-fight - sometimes ordering an ally to
		// hold position IS the tactical move - but it should not happen by
		// accident while reaching for the pickup key. With something hostile
		// nearby the first press only arms the prompt; the second opens it.
		//
		// Reviving is exempt: it is something you do *because* the fight is
		// going badly, and a confirmation step on picking a friend up off the
		// floor would be actively cruel.
		const bool bUrgent = Target->IsDowned();

		if (!bUrgent && IsHostileNearby())
		{
			const double Now = GetWorld() ? GetWorld()->GetTimeSeconds() : 0.0;

			if (PendingTalkTarget != Target || Now > PendingTalkExpiry)
			{
				PendingTalkTarget = Target;
				PendingTalkExpiry = Now + TalkConfirmSeconds;
				return;
			}
		}

		PendingTalkTarget = nullptr;

		InteractionTarget = Target;
		InteractionIndex = 0;
		BuildInteractionOptions();
		return;
	}

	// Still nobody to talk to: a corpse or chest under the crosshair opens the
	// loot menu instead. No hostile-nearby confirm step here - searching a body
	// or a crate is exactly the kind of thing you do mid-fight, the same reason
	// the old Loot option never gated on IsHostileNearby either.
	if (AActor* LootSource = FindLootTargetUnderCursor())
	{
		PendingTalkTarget = nullptr;

		LootTargetActor = LootSource;
		LootMenuIndex = 0;
		bLootShowingItems = false;
		return;
	}

	PendingTalkTarget = nullptr;

	// Nobody to talk to and nothing to loot, so E keeps its original job.
	AttemptPickupOrDrop();
}

void ADCCCharacterBase::HandleLootMenuConfirm()
{
	AActor* SourceActor = LootTargetActor;
	IDCCLootSource* Source = Cast<IDCCLootSource>(SourceActor);
	if (!SourceActor || !Source)
	{
		CloseLootMenu();
		return;
	}

	// Walking away closes the menu, same as the conversation one.
	if (FVector::Dist(GetActorLocation(), SourceActor->GetActorLocation()) > InteractionRange)
	{
		CloseLootMenu();
		return;
	}

	if (bLootShowingItems)
	{
		const TArray<FDCCRolledLoot>& Items = Source->GetPendingLootItems();

		// The row after the last item is "Back" - see DCCHUD::DrawLootMenu,
		// which draws it in exactly this position for exactly this reason.
		if (LootMenuIndex == Items.Num() || Items.IsEmpty())
		{
			bLootShowingItems = false;
			LootMenuIndex = 0;
			return;
		}

		if (Items.IsValidIndex(LootMenuIndex))
		{
			const int32 TakenIndex = LootMenuIndex;

			if (HasAuthority())
			{
				Server_LootOne_Implementation(SourceActor, TakenIndex);
			}
			else
			{
				Server_LootOne(SourceActor, TakenIndex);
			}

			// Locally re-clamped so the highlight does not point past the end
			// once the server's removal replicates back - it settles on
			// whatever slid up to fill the taken row's place.
			LootMenuIndex = FMath::Clamp(TakenIndex, 0, FMath::Max(0, Items.Num() - 2));
		}

		return;
	}

	// The top menu: Check Inventory / Loot All / Stow / Leave, in that order -
	// see DCCHUD::DrawLootMenu's TopLabels array, which must stay in sync.
	switch (LootMenuIndex)
	{
	case 0:
		if (Source->HasAnyLoot())
		{
			bLootShowingItems = true;
			LootMenuIndex = 0;
		}
		return;

	case 1:
		if (HasAuthority())
		{
			Server_LootAll_Implementation(SourceActor);
		}
		else
		{
			Server_LootAll(SourceActor);
		}
		CloseLootMenu();
		return;

	case 2:
		if (HasAuthority())
		{
			Server_StowLootSource_Implementation(SourceActor);
		}
		else
		{
			Server_StowLootSource(SourceActor);
		}
		CloseLootMenu();
		return;

	case 3:
	default:
		CloseLootMenu();
		return;
	}
}

void ADCCCharacterBase::CycleLootMenu(int32 Delta)
{
	IDCCLootSource* Source = Cast<IDCCLootSource>(LootTargetActor.Get());
	if (!Source)
	{
		return;
	}

	// +1 for the itemised page's trailing "Back" row.
	const int32 Count = bLootShowingItems ? Source->GetPendingLootItems().Num() + 1 : 4;
	if (Count <= 0)
	{
		return;
	}

	LootMenuIndex = ((LootMenuIndex + Delta) % Count + Count) % Count;
}

void ADCCCharacterBase::CloseLootMenu()
{
	LootTargetActor = nullptr;
	LootMenuIndex = 0;
	bLootShowingItems = false;
}

void ADCCCharacterBase::Server_LootOne_Implementation(AActor* Source, int32 Index)
{
	IDCCLootSource* LootSource = Cast<IDCCLootSource>(Source);
	if (!LootSource || !IsAlive() || IsDowned())
	{
		return;
	}

	if (FVector::Dist(GetActorLocation(), Source->GetActorLocation()) > InteractionRange)
	{
		return;
	}

	LootSource->LootOne(Index, this);
}

void ADCCCharacterBase::Server_LootAll_Implementation(AActor* Source)
{
	IDCCLootSource* LootSource = Cast<IDCCLootSource>(Source);
	if (!LootSource || !IsAlive() || IsDowned())
	{
		return;
	}

	if (FVector::Dist(GetActorLocation(), Source->GetActorLocation()) > InteractionRange)
	{
		return;
	}

	// Checked before LootAll empties it, and only counted if there was
	// genuinely something to take - an empty corpse or chest is not a
	// container "looted."
	const bool bHadLoot = LootSource->HasAnyLoot();

	LootSource->LootAll(this);

	if (bHadLoot)
	{
		UDCCAchievementComponent::RecordFor(this, EDCCAchievementStat::ContainersLooted);
	}

	ADCCGameState::Log(this, EDCCLogCategory::Encounter, FText::Format(
		NSLOCTEXT("DCC", "LogLootAll", "{0} empties {1} out."),
		FText::FromString(GetActorNameOrLabel()), LootSource->GetLootSourceName()));
}

void ADCCCharacterBase::Server_StowLootSource_Implementation(AActor* Source)
{
	IDCCLootSource* LootSource = Cast<IDCCLootSource>(Source);
	if (!LootSource || !IsAlive() || IsDowned())
	{
		return;
	}

	if (FVector::Dist(GetActorLocation(), Source->GetActorLocation()) > InteractionRange)
	{
		return;
	}

	const bool bHadLoot = LootSource->HasAnyLoot();

	ADCCGameState::Log(this, EDCCLogCategory::Encounter, FText::Format(
		NSLOCTEXT("DCC", "LogStow", "{0} pockets {1} whole. Grim, but efficient."),
		FText::FromString(GetActorNameOrLabel()), LootSource->GetLootSourceName()));

	LootSource->StowInto(this);

	if (bHadLoot)
	{
		UDCCAchievementComponent::RecordFor(this, EDCCAchievementStat::ContainersLooted);
	}
}

void ADCCCharacterBase::OpenChat(ADCCCharacterBase* Target)
{
	if (!Target || Target == this || !Target->IsAlive() || Target->IsHostileTo(this))
	{
		return;
	}

	PendingTalkTarget = nullptr;

	InteractionTarget = Target;
	InteractionIndex = 0;
	BuildInteractionOptions();
}

void ADCCCharacterBase::CycleInteractionOption(int32 Delta)
{
	if (!InteractionTarget || InteractionOptions.IsEmpty())
	{
		return;
	}

	const int32 Count = InteractionOptions.Num();

	// Wraps in both directions - scrolling past the end of a nine-item list to
	// reach "Leave" at the top is faster than scrolling back.
	InteractionIndex = ((InteractionIndex + Delta) % Count + Count) % Count;
}

void ADCCCharacterBase::CloseInteraction()
{
	InteractionTarget = nullptr;
	InspectedCharacter = nullptr;
	InteractionOptions.Reset();
	InteractionIndex = 0;

	// Esc is bound to this function alone, so it has to close whichever of the
	// two menus is actually open - harmless to call when the loot menu was
	// already closed, CloseLootMenu just resets fields that are reset already.
	CloseLootMenu();
}

void ADCCCharacterBase::ResolveSocialCheck(ADCCCharacterBase* Target, bool bIntimidate)
{
	UDCCRuleset* Ruleset = Stats ? Stats->GetRuleset() : nullptr;
	if (!Ruleset || !Target)
	{
		return;
	}

	const int32 Difficulty = bIntimidate ? IntimidateDifficulty : PersuadeDifficulty;

	// Charisma, per the published social skills. The roll is real and the result
	// is reported in full - the narrator layer describes outcomes, it never
	// decides them.
	const FDCCCheckResult Check = Ruleset->ResolveCheck(
		Stats->GetEnhanced(), EDCCAbility::Charisma, Difficulty);

	const FText SelfName = FText::FromString(GetActorNameOrLabel());
	const FText TargetName = Target->DisplayName.IsEmpty()
		? FText::FromString(Target->GetActorNameOrLabel())
		: Target->DisplayName;

	if (Check.bSuccess)
	{
		// Success on either verb warms them to you. That is what makes Recruit
		// reachable, and it is the only thing in this menu that changes state.
		Target->bBefriended = true;
		Target->OnRep_Befriended();

		ADCCGameState::Log(this, EDCCLogCategory::Skill, FText::Format(
			bIntimidate
				? NSLOCTEXT("DCC", "LogIntimidateWin", "{0} leans on {1}. ({2} vs {3}) They decide agreeing is cheaper.")
				: NSLOCTEXT("DCC", "LogPersuadeWin", "{0} talks {1} round. ({2} vs {3})"),
			SelfName, TargetName,
			FText::AsNumber(Check.Total), FText::AsNumber(Check.DifficultyClass)));
	}
	else
	{
		ADCCGameState::Log(this, EDCCLogCategory::Skill, FText::Format(
			bIntimidate
				? NSLOCTEXT("DCC", "LogIntimidateFail", "{0} tries to lean on {1} and it lands badly. ({2} vs {3})")
				: NSLOCTEXT("DCC", "LogPersuadeFail", "{0} fails to convince {1}. ({2} vs {3})"),
			SelfName, TargetName,
			FText::AsNumber(Check.Total), FText::AsNumber(Check.DifficultyClass)));
	}

	// Intimidation is content whichever way it goes, which is exactly the sort of
	// thing the producer rewards. Persuasion is merely competent, so it pays nothing.
	if (bIntimidate && Favor)
	{
		Favor->AwardFavor(Check.bSuccess ? 2 : 1, EDCCFavorEvent::DirectorCall,
			NSLOCTEXT("DCC", "FavorIntimidate", "A raised voice in a quiet room."));
	}

	// Marks the skill either way, matching the published rule that an attempt
	// marks a skill whether it succeeds or fails.
	if (Skills)
	{
		Skills->MarkSkillUsed(bIntimidate ? TEXT("FirstImpressions") : TEXT("QuickWit"));
	}
}

void ADCCCharacterBase::Server_PerformInteraction_Implementation(
	EDCCInteractionOption Option, ADCCCharacterBase* Target)
{
	// The menu is client-side, so none of its gating is trusted here. Reviving
	// acts on a character that is downed but not dead, so only "the caller must
	// be standing" applies up front - liveness of the target is checked per
	// option below. A corpse never reaches this RPC at all any more - see
	// Server_LootOne/LootAll/StowLootSource for that path.
	if (!Target || !IsAlive() || IsDowned())
	{
		return;
	}

	if (Target->IsHostileTo(this))
	{
		return;
	}

	if (FVector::Dist(GetActorLocation(), Target->GetActorLocation()) > InteractionRange)
	{
		return;
	}

	const FText TargetName = Target->DisplayName.IsEmpty()
		? FText::FromString(Target->GetActorNameOrLabel())
		: Target->DisplayName;

	switch (Option)
	{
	case EDCCInteractionOption::Chatter:
		ADCCGameState::Log(this, EDCCLogCategory::Announcement, FText::Format(
			NSLOCTEXT("DCC", "LogChatter", "{0} says something that will not be in the highlight reel."),
			TargetName));
		Multicast_PlayFistBump(Target);
		break;

	case EDCCInteractionOption::AskAbout:
		ADCCGameState::Log(this, EDCCLogCategory::Announcement, FText::Format(
			NSLOCTEXT("DCC", "LogAskAbout", "{0}: \"Management changed. The floor did not. Watch the water.\""),
			TargetName));
		break;

	case EDCCInteractionOption::Persuade:
		ResolveSocialCheck(Target, /*bIntimidate=*/false);
		break;

	case EDCCInteractionOption::Intimidate:
		ResolveSocialCheck(Target, /*bIntimidate=*/true);
		break;

	case EDCCInteractionOption::Recruit:
		// Re-checked server-side: the client greys this out below friendly, but
		// a client that lies about it still gets refused here.
		if (!Target->IsBefriended())
		{
			return;
		}

		Target->SetHoldingPosition(false);

		ADCCGameState::Log(this, EDCCLogCategory::Announcement, FText::Format(
			NSLOCTEXT("DCC", "LogRecruit", "{0} falls in behind you. The audience notes the roster change."),
			TargetName));

		if (Favor)
		{
			Favor->AwardFavor(3, EDCCFavorEvent::DirectorCall,
				NSLOCTEXT("DCC", "FavorRecruit", "A party is more watchable than a loner."));
		}
		break;

	case EDCCInteractionOption::FollowMe:
		Target->SetHoldingPosition(false);
		ADCCGameState::Log(this, EDCCLogCategory::Announcement, FText::Format(
			NSLOCTEXT("DCC", "LogFollowMe", "{0} moves to follow."), TargetName));
		break;

	case EDCCInteractionOption::HoldPosition:
		Target->SetHoldingPosition(true);
		ADCCGameState::Log(this, EDCCLogCategory::Announcement, FText::Format(
			NSLOCTEXT("DCC", "LogHold", "{0} holds position."), TargetName));
		break;

	case EDCCInteractionOption::Revive:
		BeginReviveChannel(Target);
		break;

	case EDCCInteractionOption::Inventory:
	case EDCCInteractionOption::Leave:
	default:
		// Inventory is a read-only view drawn by the HUD; nothing to do server-side.
		break;
	}
}

void ADCCCharacterBase::SetHoldingPosition(bool bHold)
{
	// Overridden by ADCCEnemyCharacter, which is the only thing with AI to stop.
}

void ADCCCharacterBase::DCCGrantSkill(const FString& SkillId, int32 Rank)
{
	const FName Id(*SkillId);

	if (HasAuthority())
	{
		Server_GrantSkill_Implementation(Id, Rank);
	}
	else
	{
		Server_GrantSkill(Id, Rank);
	}
}

void ADCCCharacterBase::Server_GrantSkill_Implementation(FName SkillId, int32 Rank)
{
	if (!Skills)
	{
		return;
	}

	// Refused rather than silently granted: a skill that is not in the table
	// would sit in KnownSkills forever, show as a blank hotbar slot, and fail
	// every activation with UnknownSkill.
	FDCCSkillDef Definition;
	if (!Skills->FindSkill(SkillId, Definition))
	{
		ADCCGameState::Log(this, EDCCLogCategory::Announcement, FText::Format(
			NSLOCTEXT("DCC", "CheatNoSkill", "No skill called '{0}' in the table."),
			FText::FromName(SkillId)));
		return;
	}

	const int32 ClampedRank = FMath::Clamp(Rank, 1, 15);

	// Re-ranking an already-known skill rather than adding a duplicate, which
	// would give the character two hotbar entries that share a cooldown.
	for (FDCCKnownSkill& Known : Skills->KnownSkills)
	{
		if (Known.SkillId == SkillId)
		{
			Known.Rank = ClampedRank;

			ADCCGameState::Log(this, EDCCLogCategory::Advancement, FText::Format(
				NSLOCTEXT("DCC", "CheatReRank", "{0} is now Rank {1}."),
				Definition.DisplayName.IsEmpty() ? FText::FromName(SkillId) : Definition.DisplayName,
				FText::AsNumber(ClampedRank)));
			return;
		}
	}

	FDCCKnownSkill Learned;
	Learned.SkillId = SkillId;
	Learned.Rank = ClampedRank;
	Skills->KnownSkills.Add(Learned);

	ADCCGameState::Log(this, EDCCLogCategory::Advancement, FText::Format(
		NSLOCTEXT("DCC", "CheatLearned", "{0} learns {1} at Rank {2}."),
		FText::FromString(GetActorNameOrLabel()),
		Definition.DisplayName.IsEmpty() ? FText::FromName(SkillId) : Definition.DisplayName,
		FText::AsNumber(ClampedRank)));
}

void ADCCCharacterBase::DCCEquip(const FString& ItemId)
{
	const FName Id(*ItemId);

	if (HasAuthority())
	{
		Server_EquipItem_Implementation(Id);
	}
	else
	{
		Server_EquipItem(Id);
	}
}

void ADCCCharacterBase::Server_EquipItem_Implementation(FName ItemId)
{
	if (!Equipment)
	{
		return;
	}

	// Equip already picks the right slot from the row and rebuilds the visuals
	// and the Enhanced stat layer, so there is nothing to do here but report.
	const bool bEquipped = Equipment->Equip(ItemId);

	ADCCGameState::Log(this, EDCCLogCategory::Announcement, bEquipped
		? FText::Format(NSLOCTEXT("DCC", "CheatEquipped", "Equipped '{0}'."), FText::FromName(ItemId))
		: FText::Format(NSLOCTEXT("DCC", "CheatEquipFail", "No item called '{0}' in the table."),
			FText::FromName(ItemId)));
}

void ADCCCharacterBase::DCCRefill()
{
	if (HasAuthority())
	{
		Server_Refill_Implementation();
	}
	else
	{
		Server_Refill();
	}
}

void ADCCCharacterBase::Server_Refill_Implementation()
{
	if (!Stats)
	{
		return;
	}

	// bRefillToFull re-derives the pools and tops all three up in one call, so
	// this also picks up any stat change made since the character spawned.
	Stats->RefreshDerivedStats(/*bRefillToFull=*/true);

	ADCCGameState::Log(this, EDCCLogCategory::Announcement,
		NSLOCTEXT("DCC", "CheatRefill", "Complimentary top-up applied. This is not a sponsorship."));
}

void ADCCCharacterBase::ToggleAdminPanel()
{
	const APlayerController* PC = Cast<APlayerController>(GetController());
	if (ADCCHUD* DCCHUD = PC ? Cast<ADCCHUD>(PC->GetHUD()) : nullptr)
	{
		DCCHUD->ToggleAdminPanel();
	}
}

void ADCCCharacterBase::Server_AdminSpawn_Implementation(TSubclassOf<AActor> ActorClass, bool bAsAlly)
{
	UWorld* World = GetWorld();
	if (!World || !ActorClass)
	{
		return;
	}

	// Three metres ahead and slightly up, so a spawned character lands in front
	// of the DM rather than inside them.
	const FVector SpawnLocation = GetActorLocation() + GetActorForwardVector() * 300.0f + FVector(0, 0, 60.0f);

	FActorSpawnParameters Params;
	Params.SpawnCollisionHandlingOverride =
		ESpawnActorCollisionHandlingMethod::AdjustIfPossibleButAlwaysSpawn;

	AActor* Spawned = World->SpawnActor<AActor>(
		ActorClass, SpawnLocation, GetActorRotation(), Params);

	if (ADCCEnemyCharacter* Enemy = Cast<ADCCEnemyCharacter>(Spawned))
	{
		// Set before BeginPlay would have mattered, so this also nudges the
		// toughness cushion into place for a freshly spawned friend.
		Enemy->bIsAlly = bAsAlly;
		if (bAsAlly && Enemy->Stats)
		{
			Enemy->Stats->SetIsAllyCharacter(true);
		}
	}

	ADCCGameState::Log(this, EDCCLogCategory::Announcement, FText::Format(
		NSLOCTEXT("DCC", "AdminSpawned", "Management has added {0} to the floor."),
		FText::FromString(GetNameSafe(Spawned))));
}

void ADCCCharacterBase::Server_AdminClearHostiles_Implementation()
{
	UWorld* World = GetWorld();
	if (!World)
	{
		return;
	}

	int32 Cleared = 0;

	for (TActorIterator<ADCCCharacterBase> It(World); It; ++It)
	{
		ADCCCharacterBase* Character = *It;

		// Hostile to the caller specifically, so this never kills the party's own
		// allies - which is the mistake a blunt "kill everything" would make.
		if (Character && Character->IsAlive() && IsHostileTo(Character) && Character->Stats)
		{
			Character->Stats->ApplyDamage(Character->Stats->MaxHitPoints * 10);
			++Cleared;
		}
	}

	ADCCGameState::Log(this, EDCCLogCategory::Announcement, FText::Format(
		NSLOCTEXT("DCC", "AdminCleared",
			"{0} contestants have been released from their contracts."),
		FText::AsNumber(Cleared)));
}

void ADCCCharacterBase::Server_AdminSetInvulnerable_Implementation(bool bInvulnerable)
{
	if (Stats)
	{
		Stats->SetInvulnerable(bInvulnerable);

		ADCCGameState::Log(this, EDCCLogCategory::Announcement, bInvulnerable
			? NSLOCTEXT("DCC", "AdminInvulnOn", "Producer protection enabled. This is not a sponsorship.")
			: NSLOCTEXT("DCC", "AdminInvulnOff", "Producer protection withdrawn. Good luck."));
	}
}

void ADCCCharacterBase::Server_AdminAddGold_Implementation(int32 Amount)
{
	if (Stats)
	{
		Stats->AddGold(Amount);
	}
}

void ADCCCharacterBase::Server_AdminLevelUp_Implementation()
{
	if (!Stats)
	{
		return;
	}

	++Stats->Level;

	// Refilled, because Health and Mana both derive from Level and arriving at a
	// new level already hurt would read as a bug.
	Stats->RefreshDerivedStats(/*bRefillToFull=*/true);

	ADCCGameState::Log(this, EDCCLogCategory::Experience, FText::Format(
		NSLOCTEXT("DCC", "AdminLevel", "{0} is now Level {1}. The audience assumes favouritism."),
		FText::FromString(GetActorNameOrLabel()), FText::AsNumber(Stats->Level)));
}

void ADCCCharacterBase::ToggleCharacterMenu()
{
	const APlayerController* PC = Cast<APlayerController>(GetController());
	if (ADCCHUD* DCCHUD = PC ? Cast<ADCCHUD>(PC->GetHUD()) : nullptr)
	{
		DCCHUD->ToggleMenu();
	}
}

void ADCCCharacterBase::ShowLocalNotice(const FText& Message) const
{
	const APlayerController* PC = Cast<APlayerController>(GetController());
	if (ADCCHUD* DCCHUD = PC ? Cast<ADCCHUD>(PC->GetHUD()) : nullptr)
	{
		DCCHUD->ShowNotice(Message);
	}
}

FText ADCCCharacterBase::DescribeSkillFailure(EDCCSkillFailure Failure, const FText& SkillName)
{
	switch (Failure)
	{
	case EDCCSkillFailure::OnCooldown:
		return FText::Format(NSLOCTEXT("DCC", "FailCooldown", "{0} is still on cooldown."), SkillName);

	case EDCCSkillFailure::NotEnoughMana:
		return FText::Format(NSLOCTEXT("DCC", "FailMana", "Not enough mana for {0}."), SkillName);

	case EDCCSkillFailure::NoTarget:
		return FText::Format(NSLOCTEXT("DCC", "FailTarget", "No valid target for {0}."), SkillName);

	case EDCCSkillFailure::OutOfRange:
		return FText::Format(NSLOCTEXT("DCC", "FailRange", "{0} cannot reach that."), SkillName);

	case EDCCSkillFailure::RequiresTraining:
		return FText::Format(
			NSLOCTEXT("DCC", "FailUntrained", "{0} cannot be attempted untrained."), SkillName);

	case EDCCSkillFailure::UnknownSkill:
		return FText::Format(NSLOCTEXT("DCC", "FailUnknown", "You do not know {0}."), SkillName);

	case EDCCSkillFailure::Dead:
		return NSLOCTEXT("DCC", "FailDead", "You are in no condition to do that.");

	default:
		return FText::Format(NSLOCTEXT("DCC", "FailGeneric", "{0} did not go off."), SkillName);
	}
}

void ADCCCharacterBase::CycleLogFilter()
{
	const APlayerController* PC = Cast<APlayerController>(GetController());
	if (ADCCHUD* DCCHUD = PC ? Cast<ADCCHUD>(PC->GetHUD()) : nullptr)
	{
		DCCHUD->CycleFeedFilter();
	}
}

void ADCCCharacterBase::CycleMenuTab(int32 Delta)
{
	const APlayerController* PC = Cast<APlayerController>(GetController());
	ADCCHUD* DCCHUD = PC ? Cast<ADCCHUD>(PC->GetHUD()) : nullptr;

	// Only while the menu is open - otherwise the bracket keys would silently
	// change a page nobody is looking at.
	if (DCCHUD && DCCHUD->IsMenuVisible())
	{
		DCCHUD->CycleMenuTab(Delta);
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

float ADCCCharacterBase::GetEffectiveAttackRange() const
{
	// Equipment wins when it says anything, so a bow genuinely changes how the
	// character fights rather than just how the swing looks.
	if (Equipment)
	{
		const float Override = Equipment->GetAttackRangeOverride();
		if (Override > 0.0f)
		{
			return Override;
		}
	}

	return AttackRange;
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

	return Distance <= (GetEffectiveAttackRange() + SelfRadius + TargetRadius);
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
		Stats->GetEnhanced(),
		AttackAbility,
		WeaponDamageDie,
		TargetStats->GetEnhanced(),
		TargetStats->Level);

	if (Result.bHit)
	{
		// A boss mid-Balloon-Payment soaks part of the hit. Applied here rather
		// than inside ApplyDamage so the stats layer stays ruleset-agnostic and
		// knows nothing about bosses - see the note on keeping Stats/ clean.
		int32 FinalDamage = Result.Damage;
		if (const UDCCBossComponent* TargetBoss = Target->FindComponentByClass<UDCCBossComponent>())
		{
			FinalDamage = FMath::Max(1,
				FMath::RoundToInt(FinalDamage * TargetBoss->GetIncomingDamageMultiplier()));
		}

		TargetStats->ApplyDamage(FinalDamage);
		Target->PlayCharacterSound(Target->HurtSound);

		// Staggered away from whoever swung, so the direction of the blow is
		// legible as well as the fact of it.
		Target->ReactToHitFrom(GetActorLocation());

		// Local to whoever was hit. Cast rather than assumed, because most things
		// taking damage in this game are AI and have no camera at all.
		if (Target->HurtCameraShake)
		{
			if (APlayerController* HurtPC = Cast<APlayerController>(Target->GetController()))
			{
				HurtPC->ClientStartCameraShake(Target->HurtCameraShake);
			}
		}

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
	// Contextual: an ally under the crosshair opens a conversation, otherwise this
	// falls through to the pickup/drop it has always been.
	PlayerInputComponent->BindKey(EKeys::E, IE_Pressed, this, &ADCCCharacterBase::HandleInteractPressed);

	// The wheel only does anything while a conversation is open, so it stays free
	// for the hotbar or camera zoom later.
	PlayerInputComponent->BindKey(EKeys::MouseScrollUp, IE_Pressed, this, &ADCCCharacterBase::InteractionScrollUp);
	PlayerInputComponent->BindKey(EKeys::MouseScrollDown, IE_Pressed, this, &ADCCCharacterBase::InteractionScrollDown);
	PlayerInputComponent->BindKey(EKeys::Escape, IE_Pressed, this, &ADCCCharacterBase::CloseInteraction);
	PlayerInputComponent->BindKey(EKeys::R, IE_Pressed, this, &ADCCCharacterBase::AttemptThrow);

	// F stows what is in hand; Tab opens the character menu. Tab is the near
	// universal convention for an inventory screen, so it costs no explaining.
	PlayerInputComponent->BindKey(EKeys::F, IE_Pressed, this, &ADCCCharacterBase::AttemptStow);
	PlayerInputComponent->BindKey(EKeys::C, IE_Pressed, this, &ADCCCharacterBase::ToggleSelfieMode);
	PlayerInputComponent->BindKey(EKeys::Tab, IE_Pressed, this, &ADCCCharacterBase::ToggleCharacterMenu);
	PlayerInputComponent->BindKey(EKeys::L, IE_Pressed, this, &ADCCCharacterBase::CycleLogFilter);

	// F1 for the DM panel. A function key rather than a letter because it is not
	// a game action - it should not compete with anything a player might press
	// mid-fight, and it stays out of the way of the movement hand entirely.
	PlayerInputComponent->BindKey(EKeys::F1, IE_Pressed, this, &ADCCCharacterBase::ToggleAdminPanel);

	// Menu pages. Bracket keys rather than Q/E, because E is the interact key and
	// Q is free but reads as a movement key to anyone who has played a shooter.
	PlayerInputComponent->BindKey(EKeys::LeftBracket, IE_Pressed, this, &ADCCCharacterBase::PreviousMenuTab);
	PlayerInputComponent->BindKey(EKeys::RightBracket, IE_Pressed, this, &ADCCCharacterBase::NextMenuTab);

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

void ADCCCharacterBase::ResolveOrQueueSkill(FName SkillId, AActor* Target)
{
	if (!HasAuthority())
	{
		Server_CastSkill(SkillId, Target);
		return;
	}

	const UWorld* World = GetWorld();
	if (ADCCGameState* GameState = World ? World->GetGameState<ADCCGameState>() : nullptr)
	{
		if (GameState->IsCombatPaused())
		{
			GameState->QueueAction(this, SkillId, Target);
			return;
		}
	}

	// TryActivateSkill already does its own client/server dispatch when called
	// off the server, but this path only ever runs once already-authoritative,
	// so it always takes the direct branch inside it.
	if (!Skills)
	{
		return;
	}

	FDCCSkillDef Definition;
	const bool bFound = Skills->FindSkill(SkillId, Definition);

	const FText SkillName = bFound && !Definition.DisplayName.IsEmpty()
		? Definition.DisplayName
		: FText::FromName(SkillId);

	// Asked BEFORE anything visible happens. Refusals used to be silent on every
	// path, which made a skill that could not be afforded indistinguishable from
	// a key that had not registered - the single biggest reason skills felt
	// broken. Checking first also stops a refused skill playing its wind-up.
	const EDCCSkillFailure Refusal = Skills->CanActivate(SkillId, Target);
	if (Refusal != EDCCSkillFailure::None)
	{
		// Reported into the replicated feed rather than a local notice, because
		// this runs on the server and has to reach the player who asked even when
		// they are a remote client.
		ADCCGameState::Log(this, EDCCLogCategory::Skill,
			DescribeSkillFailure(Refusal, SkillName));
		return;
	}

	// The wind-up should be visible even when the roll goes on to miss, exactly
	// as a swing is. ADCCEnemyCharacter does the same in its own cast path.
	if (bFound)
	{
		Multicast_PlayCast(Definition.CastTimeSeconds);
	}

	Skills->TryActivateSkill(SkillId, Target);
}

void ADCCCharacterBase::Server_CastSkill_Implementation(FName SkillId, AActor* Target)
{
	// Re-validated all the way down: ResolveOrQueueSkill queues or calls
	// TryActivateSkill, and either path re-checks CanActivate server-side before
	// anything happens. The client's target is a nomination, never a fact.
	ResolveOrQueueSkill(SkillId, Target);
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

bool ADCCCharacterBase::CanPlayOnThisSkeleton(const UAnimationAsset* Animation) const
{
	const USkeletalMeshComponent* MeshComponent = GetMesh();
	if (!Animation || !MeshComponent)
	{
		return false;
	}

	const USkeletalMesh* MeshAsset = MeshComponent->GetSkeletalMeshAsset();
	if (!MeshAsset)
	{
		return false;
	}

	const USkeleton* MeshSkeleton = MeshAsset->GetSkeleton();
	const USkeleton* AnimSkeleton = Animation->GetSkeleton();

	if (MeshSkeleton && AnimSkeleton && MeshSkeleton == AnimSkeleton)
	{
		return true;
	}

	// This is the `SkeletonToCompactPose.IsValidIndex` ensure, caught one frame
	// before the engine hits it. Playing a clip authored for one skeleton on a
	// mesh built from another does not fail cleanly - it produces a garbage pose,
	// which is what "the allies are standing on their heads" actually is.
	//
	// Warned once per character rather than every frame, and it names both assets,
	// because the useful question is never "did it break" but "which clip is on
	// the wrong rig".
	if (!bWarnedSkeletonMismatch)
	{
		bWarnedSkeletonMismatch = true;

		UE_LOG(LogTemp, Warning,
			TEXT("[DCC Anim] %s: refusing '%s' (skeleton '%s') on mesh '%s' (skeleton '%s'). Wrong rig."),
			*GetActorNameOrLabel(),
			*Animation->GetName(),
			AnimSkeleton ? *AnimSkeleton->GetName() : TEXT("none"),
			*MeshAsset->GetName(),
			MeshSkeleton ? *MeshSkeleton->GetName() : TEXT("none"));
	}

	return false;
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

	if (!CanPlayOnThisSkeleton(Animation))
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

	// Same guard as the one-shot path. A mismatched idle is the worst case of
	// all, because it is the pose the character holds while doing nothing -
	// which is precisely when it was seen standing on its head.
	if (!CanPlayOnThisSkeleton(Desired))
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

			// Forward must point AT the viewer, not away from them - a
			// TextRenderComponent draws on its front face only, so getting this
			// subtraction backwards shows the reverse of the quad, which reads as
			// mirrored text rather than as simply-not-facing-you.
			Nameplate->SetWorldRotation((ViewLocation - Nameplate->GetComponentLocation()).Rotation());
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
	PlayCharacterSound(AttackSound);

	BP_OnAttackPerformed(Target, bHit);
}

void ADCCCharacterBase::PlayCharacterSound(USoundBase* Sound) const
{
	// Located at the character rather than 2D, so a fight across the room sounds
	// like it is across the room - which is most of what audio buys a party that
	// cannot see each other constantly.
	if (Sound && GetWorld())
	{
		UGameplayStatics::PlaySoundAtLocation(this, Sound, GetActorLocation());
	}
}

void ADCCCharacterBase::Multicast_PlayCast_Implementation(float HoldSeconds)
{
	PlayOneShotAnimation(CastAnimation, HoldSeconds);
	PlayCharacterSound(CastSound);
}

void ADCCCharacterBase::Multicast_PlayHitReaction_Implementation()
{
	// Squeezed into HitReactionSeconds. PlayOneShotAnimation already stretches or
	// compresses a clip to fill a window, which is what makes a borrowed gesture
	// usable as a flinch until a real reaction clip exists.
	PlayOneShotAnimation(HitReactionAnimation, HitReactionSeconds);
}

void ADCCCharacterBase::ReactToHitFrom(const FVector& ImpactSource)
{
	UWorld* World = GetWorld();
	if (!HasAuthority() || !World || !IsAlive() || bIsDowned)
	{
		return;
	}

	const float Now = World->GetTimeSeconds();
	if (Now - LastHitReactionTime < HitReactionCooldown)
	{
		return;
	}
	LastHitReactionTime = Now;

	if (HitStaggerStrength > 0.0f)
	{
		FVector Away = GetActorLocation() - ImpactSource;
		Away.Z = 0.0f;

		// Falls back to straight backwards when the two are stacked exactly, which
		// happens more often than it sounds - melee closes to touching distance.
		if (!Away.Normalize())
		{
			Away = -GetActorForwardVector();
		}

		// A small upward component so the shove reads as a jolt rather than as
		// sliding. bXYOverride false so it adds to existing motion instead of
		// replacing it, which would stop a charging enemy dead.
		LaunchCharacter(Away * HitStaggerStrength + FVector(0.0f, 0.0f, 90.0f), false, false);
	}

	Multicast_PlayHitReaction();
}

void ADCCCharacterBase::ApplyCollapsedBody()
{
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

	// Collapsing mid-cast would otherwise leave the timer to fire on a body and
	// clear a flag nothing reads again.
	GetWorldTimerManager().ClearTimer(CastLockTimer);
	bIsCasting = false;

	// Ragdoll rather than a death animation. None of the imported characters ship
	// one, and physics gives a different fall every time - which reads better
	// than the same canned collapse on every mob in the room. Every skeletal mesh
	// already has a physics asset from import, so this costs nothing extra.
	if (USkeletalMeshComponent* MeshComponent = GetMesh())
	{
		MeshComponent->SetCollisionEnabled(ECollisionEnabled::QueryAndPhysics);
		MeshComponent->SetCollisionObjectType(ECC_PhysicsBody);
		MeshComponent->SetCollisionResponseToAllChannels(ECR_Block);
		MeshComponent->SetCollisionResponseToChannel(ECC_Pawn, ECR_Ignore);

		MeshComponent->SetAllBodiesSimulatePhysics(true);
		MeshComponent->SetSimulatePhysics(true);
		MeshComponent->WakeAllRigidBodies();
	}
}

void ADCCCharacterBase::RestoreStandingBody()
{
	if (USkeletalMeshComponent* MeshComponent = GetMesh())
	{
		// Stop the simulation before reattaching - a body still solving physics
		// fights the attachment and snaps back to wherever it had drifted.
		MeshComponent->SetSimulatePhysics(false);
		MeshComponent->SetAllBodiesSimulatePhysics(false);

		MeshComponent->AttachToComponent(
			GetCapsuleComponent(), FAttachmentTransformRules::KeepRelativeTransform);

		// Captured at BeginPlay, before anything could ragdoll. Without it the
		// mesh reattaches at wherever physics left it, which is usually the floor.
		MeshComponent->SetRelativeTransform(MeshRestTransform);

		MeshComponent->SetCollisionObjectType(ECC_Pawn);
		MeshComponent->SetCollisionEnabled(ECollisionEnabled::QueryOnly);
		MeshComponent->SetCollisionResponseToAllChannels(ECR_Ignore);
	}

	if (UCapsuleComponent* Capsule = GetCapsuleComponent())
	{
		Capsule->SetCollisionEnabled(ECollisionEnabled::QueryAndPhysics);
	}

	if (UCharacterMovementComponent* Movement = GetCharacterMovement())
	{
		Movement->SetMovementMode(MOVE_Walking);
	}

	if (Nameplate)
	{
		Nameplate->SetVisibility(true);
	}
}

void ADCCCharacterBase::HandleDied(UDCCStatsComponent* InStats)
{
	// OnDied can arrive more than once across authority and replication paths;
	// the visual teardown must only happen the first time.
	if (bHasDied || bIsDowned)
	{
		return;
	}

	// A crawler at zero goes DOWN, not out - the party gets a window to reach
	// them. Mobs skip straight to dead, because a Debt Eater that gets back up
	// is not a story beat.
	if (IsCrawler() && bCanBeDowned)
	{
		bIsDowned = true;
		DeathSaveSuccesses = 0;
		DeathSaveFailures = 0;
		ApplyCollapsedBody();
		PlayCharacterSound(DownedSound);

		if (HasAuthority())
		{
			GetWorldTimerManager().SetTimer(
				DeathSaveTimer, this, &ADCCCharacterBase::RollDeathSave, DeathSaveIntervalSeconds, /*bLoop=*/true);

			ADCCGameState::Log(this, EDCCLogCategory::Death, FText::Format(
				NSLOCTEXT("DCC", "LogCrawlerDowned",
					"{0} is down and rolling death saves. Somebody nearby could still change the outcome."),
				DisplayName.IsEmpty() ? FText::FromString(GetActorNameOrLabel()) : DisplayName));
		}

		BP_OnDied();
		return;
	}

	FinishDying();
}

void ADCCCharacterBase::RollDeathSave()
{
	if (!HasAuthority() || !bIsDowned || bHasDied)
	{
		return;
	}

	const int32 Roll = FMath::RandRange(1, 20);
	const bool bSuccess = Roll >= DeathSaveDC;
	const FText Name = DisplayName.IsEmpty() ? FText::FromString(GetActorNameOrLabel()) : DisplayName;

	if (bSuccess)
	{
		++DeathSaveSuccesses;

		ADCCGameState::Log(this, EDCCLogCategory::Death, FText::Format(
			NSLOCTEXT("DCC", "LogDeathSaveSuccess", "{0} rolls a {1} on the death save. Success ({2}/{3})."),
			Name, FText::AsNumber(Roll), FText::AsNumber(DeathSaveSuccesses), FText::AsNumber(DeathSavesToStabilize)));
	}
	else
	{
		++DeathSaveFailures;

		ADCCGameState::Log(this, EDCCLogCategory::Death, FText::Format(
			NSLOCTEXT("DCC", "LogDeathSaveFailure", "{0} rolls a {1} on the death save. Failure ({2}/{3})."),
			Name, FText::AsNumber(Roll), FText::AsNumber(DeathSaveFailures), FText::AsNumber(DeathSavesToExpire)));
	}

	// Checked in this order so a roll that happens to complete both counts in
	// one pass - impossible under normal play since one side is cleared
	// whenever bIsDowned flips false, but a designer-edited
	// DeathSavesToStabilize/ToExpire pair could overlap, and stabilising is the
	// friendlier outcome to prefer if it ever does.
	if (DeathSaveSuccesses >= DeathSavesToStabilize)
	{
		StabilizeAtOneHitPoint();
		return;
	}

	if (DeathSaveFailures >= DeathSavesToExpire)
	{
		GetWorldTimerManager().ClearTimer(DeathSaveTimer);
		bIsDowned = false;
		FinishDying();
	}
}

void ADCCCharacterBase::StabilizeAtOneHitPoint()
{
	if (!HasAuthority() || !bIsDowned || bHasDied || !Stats)
	{
		return;
	}

	GetWorldTimerManager().ClearTimer(DeathSaveTimer);

	bIsDowned = false;
	RestoreStandingBody();
	PlayCharacterSound(ReviveSound);

	// Exactly one point, always - the roll bought a second chance, not a good one.
	Stats->Heal(1);

	ADCCGameState::Log(this, EDCCLogCategory::Announcement, FText::Format(
		NSLOCTEXT("DCC", "LogStabilize",
			"{0} stabilises without any help at all. One hit point, and a story for later."),
		DisplayName.IsEmpty() ? FText::FromString(GetActorNameOrLabel()) : DisplayName));
}

void ADCCCharacterBase::FinishDying()
{
	if (bHasDied)
	{
		return;
	}
	bHasDied = true;

	GetWorldTimerManager().ClearTimer(DeathSaveTimer);

	// Safe to call twice - a bled-out crawler already collapsed when it went
	// down, and every step in here is idempotent.
	ApplyCollapsedBody();
	PlayCharacterSound(DeathSound);

	if (HasAuthority())
	{
		// Rolled here rather than spawned here. The result is fixed at the moment
		// of death so two players racing to the same corpse cannot re-roll it.
		RollLootOntoBody();

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

void ADCCCharacterBase::OnRep_Downed()
{
	// Clients run the same visual collapse the server did, so a downed crawler
	// looks downed on every machine rather than only where they fell.
	if (bIsDowned)
	{
		ApplyCollapsedBody();
	}
	else if (!bHasDied)
	{
		RestoreStandingBody();
	}
}

void ADCCCharacterBase::BeginReviveChannel(ADCCCharacterBase* Target)
{
	if (!HasAuthority() || !Target || !Target->IsDowned())
	{
		return;
	}

	// Rooted for the channel, then they come up. Reusing the cast lock means
	// a rescuer is interruptible exactly like a caster is - taking a hit
	// mid-revive should cost you the attempt. Shared by both a player's own
	// "Help Them Up" and an ally's first-aid AI, so the two mechanics can
	// never quietly drift apart.
	BeginCastLock(ReviveSeconds);

	TWeakObjectPtr<ADCCCharacterBase> WeakTarget(Target);
	TWeakObjectPtr<ADCCCharacterBase> WeakSelf(this);

	FTimerDelegate Finish;
	Finish.BindLambda([WeakSelf, WeakTarget]()
	{
		// Both ends re-checked on completion: the rescuer may have died
		// during the channel, and the target may have bled out or been
		// picked up by somebody faster.
		if (WeakSelf.IsValid() && WeakTarget.IsValid()
			&& WeakSelf->IsAlive() && WeakTarget->IsDowned())
		{
			WeakTarget->Revive(WeakSelf.Get());
		}
	});

	GetWorldTimerManager().SetTimer(ReviveTimer, Finish, ReviveSeconds, false);

	const FText TargetName = Target->DisplayName.IsEmpty()
		? FText::FromString(Target->GetActorNameOrLabel())
		: Target->DisplayName;

	ADCCGameState::Log(this, EDCCLogCategory::Announcement, FText::Format(
		NSLOCTEXT("DCC", "LogReviveStart", "{0} drops to help {1} up."),
		FText::FromString(GetActorNameOrLabel()), TargetName));
}

void ADCCCharacterBase::Revive(ADCCCharacterBase* Rescuer)
{
	if (!HasAuthority() || !bIsDowned || bHasDied || !Stats)
	{
		return;
	}

	GetWorldTimerManager().ClearTimer(DeathSaveTimer);
	DeathSaveSuccesses = 0;
	DeathSaveFailures = 0;

	bIsDowned = false;
	RestoreStandingBody();
	PlayCharacterSound(ReviveSound);

	const int32 Restored = FMath::Max(1,
		FMath::RoundToInt(Stats->MaxHitPoints * ReviveHealthFraction));
	Stats->Heal(Restored);

	ADCCGameState::Log(this, EDCCLogCategory::Announcement, FText::Format(
		NSLOCTEXT("DCC", "LogRevive", "{0} gets {1} back on their feet. The audience was hoping for the other thing."),
		Rescuer ? FText::FromString(Rescuer->GetActorNameOrLabel()) : FText::FromString(TEXT("Someone")),
		DisplayName.IsEmpty() ? FText::FromString(GetActorNameOrLabel()) : DisplayName));

	// Saving somebody is watchable. The producer pays for it.
	if (Rescuer && Rescuer->Favor)
	{
		Rescuer->Favor->AwardFavor(4, EDCCFavorEvent::DirectorCall,
			NSLOCTEXT("DCC", "FavorRevive", "A rescue, live on air."));
	}
}

float ADCCCharacterBase::GetTimeUntilNextDeathSave() const
{
	if (!bIsDowned)
	{
		return 0.0f;
	}

	// Only the server runs the timer, so a client asks its own world for the
	// value and gets 0 - the countdown is authoritative and shown to the host.
	// Clients still see the DOWN overlay and the replicated pips, just without
	// a ticking number for "when."
	return GetWorldTimerManager().GetTimerRemaining(DeathSaveTimer);
}

void ADCCCharacterBase::RollLootOntoBody()
{
	PendingLoot.Reset();
	bHasLoot = false;

	if (!HasAuthority() || LootTable.IsEmpty())
	{
		return;
	}

	// Crawlers do not carry a payday, and that covers allies too - IsCrawler()
	// returns bIsAlly on the enemy class, so CJ going down leaves a body rather
	// than a windfall. Looting your own downed friend is a different game.
	if (IsCrawler())
	{
		return;
	}

	DCCLoot::RollLootTable(LootTable, PendingLoot);
	bHasLoot = !PendingLoot.IsEmpty();
}

// -- IDCCLootSource --------------------------------------------------------
// A corpse's half of the interface. RollLootOntoBody above is corpse-specific
// (rolled at death, refuses crawlers) and stays a named function other death
// code calls directly; everything below is the generic contract the loot menu
// actually talks to, and ADCCLootContainer implements the same six calls for
// a chest using its own BeginPlay-rolled contents instead.

FText ADCCCharacterBase::GetLootSourceName() const
{
	return DisplayName.IsEmpty() ? FText::FromString(GetActorNameOrLabel()) : DisplayName;
}

bool ADCCCharacterBase::HasAnyLoot() const
{
	return bHasLoot;
}

const TArray<FDCCRolledLoot>& ADCCCharacterBase::GetPendingLootItems() const
{
	return PendingLoot;
}

FVector ADCCCharacterBase::GetLootOrigin() const
{
	return GetActorLocation();
}

void ADCCCharacterBase::LootOne(int32 Index, ADCCCharacterBase* Looter)
{
	if (!HasAuthority() || !PendingLoot.IsValidIndex(Index))
	{
		return;
	}

	DCCLoot::SpawnRolledLoot(GetWorld(), PendingLoot[Index], GetLootOrigin());
	PendingLoot.RemoveAt(Index);
	bHasLoot = !PendingLoot.IsEmpty();

	// Same cleanup as a full LootAll, once there is nothing left either way.
	if (!bHasLoot && LootedCorpseLifeSpan > 0.0f)
	{
		SetLifeSpan(LootedCorpseLifeSpan);
	}
}

void ADCCCharacterBase::LootAll(ADCCCharacterBase* Looter)
{
	if (!HasAuthority() || !bHasLoot)
	{
		return;
	}

	// Cleared first: two players confirming "Loot All" on the same frame would
	// otherwise both pass the check above and spawn everything twice.
	bHasLoot = false;

	const FVector Origin = GetLootOrigin();
	for (const FDCCRolledLoot& Rolled : PendingLoot)
	{
		DCCLoot::SpawnRolledLoot(GetWorld(), Rolled, Origin);
	}

	PendingLoot.Reset();

	// Now that it is empty, let it go. Bodies persist until looted precisely so
	// nothing is lost to a timer - but an emptied corpse is just an actor with a
	// physics asset costing frame time in a room the party has finished with.
	if (LootedCorpseLifeSpan > 0.0f)
	{
		SetLifeSpan(LootedCorpseLifeSpan);
	}
}

void ADCCCharacterBase::StowInto(ADCCCharacterBase* Looter)
{
	if (!HasAuthority())
	{
		return;
	}

	if (Looter && Looter->Inventory)
	{
		Looter->Inventory->StowRemains(GetLootSourceName(), PendingLoot);
	}

	PendingLoot.Reset();
	bHasLoot = false;

	if (LootedCorpseLifeSpan > 0.0f)
	{
		SetLifeSpan(LootedCorpseLifeSpan);
	}
}

void ADCCCharacterBase::ToggleSelfieMode()
{
	if (bInSelfieMode)
	{
		// Cancelled early - no shutter, no screenshot, just hand the camera back.
		GetWorldTimerManager().ClearTimer(SelfieShutterTimer);
		GetWorldTimerManager().ClearTimer(SelfieExitTimer);
		ExitSelfieMode();
		return;
	}

	EnterSelfieMode();
}

void ADCCCharacterBase::EnterSelfieMode()
{
	// Local only - every machine decides this for its own player, and there is
	// no shared state to agree on. Refused while down or dead for the same
	// reason a corpse cannot open the Tab menu: there is no camera worth
	// posing with left to hold.
	APlayerController* PC = Cast<APlayerController>(GetController());
	if (!PC || !SelfieCamera || bInSelfieMode || !IsAlive() || IsDowned())
	{
		return;
	}

	bInSelfieMode = true;

	// Whatever else was providing the view steps aside, so SelfieCamera wins
	// regardless of what the character Blueprint's own camera rig looks like -
	// no assumption about a spring arm or its current settings required.
	DeactivatedCamerasForSelfie.Reset();
	TArray<UCameraComponent*> Cameras;
	GetComponents<UCameraComponent>(Cameras);
	for (UCameraComponent* Camera : Cameras)
	{
		if (Camera != SelfieCamera && Camera->IsActive())
		{
			Camera->SetActive(false);
			DeactivatedCamerasForSelfie.Add(Camera);
		}
	}

	SelfieCamera->SetActive(true);
	PC->SetViewTargetWithBlend(this, 0.3f);

	// A beat to actually strike a pose before the shutter fires - the point is
	// a moment held for the camera, not an instant, unposed flash.
	GetWorldTimerManager().SetTimer(
		SelfieShutterTimer, this, &ADCCCharacterBase::TakeSelfieScreenshot, SelfiePoseSeconds, false);
}

void ADCCCharacterBase::TakeSelfieScreenshot()
{
	if (!bInSelfieMode)
	{
		return;
	}

	if (APlayerController* PC = Cast<APlayerController>(GetController()))
	{
		if (PC->PlayerCameraManager)
		{
			// A quick white flash reads as the shutter having actually fired,
			// the same reason a real phone's screen flashes on capture.
			PC->PlayerCameraManager->StartCameraFade(
				0.0f, 1.0f, 0.06f, FLinearColor::White, /*bShouldFadeAudio=*/false, /*bHoldWhenFinished=*/false);
		}
	}

	// Empty filename auto-generates one and lands it in Saved/Screenshots/ -
	// exactly where the console "shot" command's own output goes, so nothing
	// new to look for. bShowUI false: the point is the character, not the HUD.
	FScreenshotRequest::RequestScreenshot(FString(), /*bShowUI=*/false, /*bAddFilenameSuffix=*/true);

	ADCCGameState::Log(this, EDCCLogCategory::Announcement, FText::Format(
		NSLOCTEXT("DCC", "LogSelfie", "{0} takes a selfie. The System AI has already seen it."),
		DisplayName.IsEmpty() ? FText::FromString(GetActorNameOrLabel()) : DisplayName));

	// A short hold so the flash and the capture both land before the camera cuts back.
	GetWorldTimerManager().SetTimer(SelfieExitTimer, this, &ADCCCharacterBase::ExitSelfieMode, 0.4f, false);
}

void ADCCCharacterBase::ExitSelfieMode()
{
	if (!bInSelfieMode)
	{
		return;
	}

	bInSelfieMode = false;

	if (SelfieCamera)
	{
		SelfieCamera->SetActive(false);
	}

	for (const TWeakObjectPtr<UCameraComponent>& Camera : DeactivatedCamerasForSelfie)
	{
		if (Camera.IsValid())
		{
			Camera->SetActive(true);
		}
	}
	DeactivatedCamerasForSelfie.Reset();

	if (APlayerController* PC = Cast<APlayerController>(GetController()))
	{
		PC->SetViewTargetWithBlend(this, 0.3f);
	}
}
