#include "Characters/DCCCharacterBase.h"
#include "Interfaces/MovementBaseInterface.h"
#include "Items/DCCChestSound.h"
#include "Items/DCCLootContainer.h"
#include "Engine/OverlapResult.h"
#include "Arcade/DCCClimbItems.h"
#include "Arcade/DCCArcadeGames.h"
#include "Arcade/DCCCosmetics.h"
#include "Arcade/DCCWalletComponent.h"
#include "Components/SpotLightComponent.h"
#include "Items/DCCInventoryComponent.h"
#include "EngineUtils.h"
#include "Characters/DCCRetargetFix.h"
#include "Arcade/DCCWalletComponent.h"

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
#include "Rooms/DCCCliffLift.h"
#include "Rooms/DCCOathgate.h"
#include "Rooms/DCCExitPortal.h"
#include "Rooms/DCCPortalConsole.h"
#include "Puzzles/DCCPuzzleDoor.h"
#include "Puzzles/DCCComplianceDoor.h"
#include "Puzzles/DCCBreakerSwitch.h"
#include "Puzzles/DCCEscrowLock.h"
#include "Vehicles/DCCRover.h"
#include "Progression/DCCAchievementComponent.h"
#include "Equipment/DCCEquipmentComponent.h"
#include "Classes/DCCClassComponent.h"
#include "Onboarding/DCCOnboardingComponent.h"
#include "Rooms/DCCTaleFloor.h"
#include "Bench/DCCBenchData.h"
#include "Session/DCCSaveSystem.h"
#include "Items/DCCPickup_Equipment.h"
#include "Engine/PointLight.h"
#include "Components/PointLightComponent.h"
#include "Quests/DCCQuestComponent.h"
#include "Dialogue/DCCDialogueNPC.h"
#include "Dialogue/DCCDialogueNPC_LiftAttendant.h"
#include "Dialogue/DCCDialogueNPC_RaceBookie.h"
#include "Items/DCCLootBoxStation.h"
#include "Dialogue/DCCVendorNPC.h"
#include "NiagaraFunctionLibrary.h"
#include "NiagaraSystem.h"
#include "Components/TextRenderComponent.h"
#include "Animation/AnimSequenceBase.h"
#include "Net/UnrealNetwork.h"
#include "UI/DCCEventLogTypes.h"
#include "Components/CapsuleComponent.h"
#include "GameFramework/SpringArmComponent.h"
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

FDCCAttackAttempted ADCCCharacterBase::OnAnyAttackAttempted;
FDCCDamageLanded ADCCCharacterBase::OnAnyDamageLanded;

void ADCCCharacterBase::NoteDamageDealtTo(ADCCCharacterBase* Target, int32 Amount)
{
	if (HasAuthority() && Target && Amount > 0)
	{
		OnAnyDamageLanded.Broadcast(this, Target, Amount);
	}
}

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

	// The stock "Pawn" collision profile only overrides Visibility to Ignore
	// (checked directly against Engine/Config/BaseEngine.ini - nothing in this
	// project's own config touches it either) - every other channel, Camera
	// included, falls back to Pawn's default response, which is Block. The
	// third-person spring arm's collision probe traces on the Camera channel,
	// so without this, standing close to ANY other character (an ally, an
	// enemy, a cluster of tutorial dummies) pins the camera against - or
	// inside - whichever capsule the trace hits first. This is the actual
	// cause of "the camera is jammed against an NPC and won't pull back" -
	// not anything specific to one NPC's mesh being oversized. Every
	// character sharing this base class now ignores camera collision, which
	// is the standard fix in third-person UE games generally: the boom
	// should only ever collide with the environment, never with a pawn.
	if (UCapsuleComponent* Capsule = GetCapsuleComponent())
	{
		Capsule->SetCollisionResponseToChannel(ECC_Camera, ECR_Ignore);

		// The stock "Pawn" profile's own Visibility:Ignore (see the comment
		// above - this was already known and documented right here, just
		// never connected to this) is the actual, whole reason a living
		// character could never be interacted with. SweepForInteractionCandidate
		// sweeps on ECC_Visibility, matching the fist-bump and loot sweeps -
		// and CharacterMesh, the stock skeletal-mesh profile, ALSO ignores
		// Visibility by the same engine default (checked against
		// BaseEngine.ini directly: Pawn and CharacterMesh both list only
		// Visibility, plus Pawn/Vehicle for CharacterMesh, as overrides).
		// So neither the capsule nor the mesh of a living character has ever
		// blocked that channel - every "can't interact with allies" report
		// this project has chased with bIsAlly/CDO theories had nothing to do
		// with either of those; the sweep was never capable of hitting a
		// living character's collision at all, full stop. Corpses only ever
		// worked because ApplyCollapsedBody explicitly flips the ragdolled
		// mesh's Visibility response to Block (see that function) - nothing
		// upstream of a corpse's collision, in either direction, was ever
		// hit-testable. Blocking Visibility on the capsule here is the
		// minimal fix: it leaves CharacterMesh's own Visibility:Ignore alone
		// (whatever that was for), leaves Camera:Ignore above alone, and
		// gives every sweep that already assumed "a living character's
		// capsule blocks Visibility" - assumed in writing, per the comment
		// on SweepForInteractionCandidate, and never actually true until this
		// line - something to actually hit.
		Capsule->SetCollisionResponseToChannel(ECC_Visibility, ECR_Block);
	}

	Stats = CreateDefaultSubobject<UDCCStatsComponent>(TEXT("Stats"));
	Skills = CreateDefaultSubobject<UDCCSkillComponent>(TEXT("Skills"));
	Favor = CreateDefaultSubobject<UDCCFavorComponent>(TEXT("Favor"));
	Carry = CreateDefaultSubobject<UDCCCarryComponent>(TEXT("Carry"));
	Inventory = CreateDefaultSubobject<UDCCInventoryComponent>(TEXT("Inventory"));
	Achievements = CreateDefaultSubobject<UDCCAchievementComponent>(TEXT("Achievements"));
	Equipment = CreateDefaultSubobject<UDCCEquipmentComponent>(TEXT("Equipment"));
	Class = CreateDefaultSubobject<UDCCClassComponent>(TEXT("Class"));
	Onboarding = CreateDefaultSubobject<UDCCOnboardingComponent>(TEXT("Onboarding"));
	Wallet = CreateDefaultSubobject<UDCCWalletComponent>(TEXT("Wallet"));

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
	//
	// SetActive(false) alone does NOT stick here: UCameraComponent's own base
	// constructor (Engine/Private/Camera/CameraComponent.cpp) sets
	// bAutoActivate = true unconditionally, and that runs BEFORE this
	// constructor body. Component registration re-activates anything with
	// bAutoActivate still true regardless of what SetActive was called with
	// during construction, so without this line SelfieCamera came back on at
	// spawn - sitting active right alongside the character Blueprint's own
	// FollowCamera, with nothing to say which one the pawn's camera
	// resolution should actually use. Since SelfieCamera is attached directly
	// to the capsule facing back at the character, winning that race read as
	// spawning already in a "selfie" framing despite bInSelfieMode being
	// false - not stuck in Selfie Mode, just looking through the wrong
	// camera. EnterSelfieMode/ExitSelfieMode's own explicit SetActive calls
	// happen to fix it in passing, which is why toggling C twice "repairs" it.
	SelfieCamera->bAutoActivate = false;
	SelfieCamera->SetActive(false);
}

void ADCCCharacterBase::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);
	DOREPLIFETIME_CONDITION(ADCCCharacterBase, QuickSlots, COND_OwnerOnly);

	DOREPLIFETIME(ADCCCharacterBase, ActiveClimbPoint);
	DOREPLIFETIME(ADCCCharacterBase, bRecordingFootage);
	DOREPLIFETIME(ADCCCharacterBase, DisplayName);
	DOREPLIFETIME(ADCCCharacterBase, bIsSprinting);
	DOREPLIFETIME(ADCCCharacterBase, bIsMounted);
	DOREPLIFETIME(ADCCCharacterBase, bIsCasting);
	DOREPLIFETIME(ADCCCharacterBase, bBefriended);
	DOREPLIFETIME(ADCCCharacterBase, CurrentRoomDisplayName);
	DOREPLIFETIME(ADCCCharacterBase, bInSafeRoom);
	DOREPLIFETIME(ADCCCharacterBase, bIsDowned);
	DOREPLIFETIME(ADCCCharacterBase, bAwaitingOtherCrawlers);
	DOREPLIFETIME(ADCCCharacterBase, DeathSaveSuccesses);
	DOREPLIFETIME(ADCCCharacterBase, DeathSaveFailures);
	DOREPLIFETIME(ADCCCharacterBase, bHasLoot);
	DOREPLIFETIME(ADCCCharacterBase, PendingLoot);
	DOREPLIFETIME(ADCCCharacterBase, bHasDied);
	DOREPLIFETIME(ADCCCharacterBase, bHasSentFinalMessage);
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

		// dcc.AutoQuickPlay 1: skip the bedroom and drop straight onto floor 1.
		ADCCTaleFloor::ScheduleQuickPlay(this);
	}

	RefreshNameplate();
}

void ADCCCharacterBase::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	// Destroyed, travelling, or the game closing: keep this crawler's progress.
	// Only the pawn that loaded the save may write it (see DCCSave::Save).
	if (HasAuthority())
	{
		DCCSave::Save(this);
	}
	Super::EndPlay(EndPlayReason);
}

void ADCCCharacterBase::BeginPlay()
{
	Super::BeginPlay();

	// Characters spawned straight from a C++ class (the dungeon's and the gate's enemies) never had a Blueprint set
	// their body up, so they stood in a T-pose a metre off the floor (found 2026-11-15). Two fixes, only where nothing
	// else has decided: no anim Blueprint but clips assigned means play the clips (single-node mode, which
	// UpdateLocomotionAnimation drives), and a body left at the capsule's centre goes down to the capsule's feet.
	if (USkeletalMeshComponent* Body = GetMesh())
	{
		if (Body->GetSkeletalMeshAsset() && Body->GetAnimationMode() == EAnimationMode::AnimationBlueprint && !Body->GetAnimClass()
			&& (IdleAnimation || WalkAnimation || RunAnimation))
		{
			Body->SetAnimationMode(EAnimationMode::AnimationSingleNode);
		}
		if (Body->GetSkeletalMeshAsset() && Body->GetRelativeLocation().IsNearlyZero() && GetCapsuleComponent())
		{
			Body->SetRelativeLocation(FVector(0.0f, 0.0f, -GetCapsuleComponent()->GetUnscaledCapsuleHalfHeight()));
		}
	}

	// Shared clips must not stretch this body to another crawler's bone lengths.
	DCCRetarget::EnsureForMesh(GetMesh());

	// Gear bench bone nudges (F3) apply on top of every animation, every frame.
	DCCBench::BindBoneNudges(GetMesh());

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

void ADCCCharacterBase::SetClassMoveSpeedBonus(float NewBonus)
{
	ClassMoveSpeedBonus = NewBonus;
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

	// A flat Class-benefit add, not another multiplier - "+5ft Move" means
	// five feet, not five feet times whatever else is already in play.
	Speed += ClassMoveSpeedBonus;

	// Same shape as the always-on Class bonus above, but conditional on
	// actually being mounted - re-reads the Class each call rather than
	// caching, since toggling is rare and this keeps GrantsMount as the one
	// source of truth instead of a second field that could drift from it.
	if (bIsMounted && Class)
	{
		float MountBonus = 0.0f;
		FText MountLabel;
		if (Class->GetMountBenefit(MountBonus, MountLabel))
		{
			Speed += MountBonus;
		}
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

	if (Wallet && Wallet->WornIn(DCCCosmetics::Aura) == FName(TEXT("SponsorOrbs")))
	{
		Speed *= 1.10f;
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
	if (bHasDied)
	{
		return;
	}

	if (Carry)
	{
		Carry->TryThrow();
	}
}

void ADCCCharacterBase::AttemptStow()
{
	if (bHasDied)
	{
		return;
	}

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

void ADCCCharacterBase::DCCEquipItem(int32 Index)
{
	if (HasAuthority())
	{
		Server_EquipInventoryItem_Implementation(Index);
	}
	else
	{
		Server_EquipInventoryItem(Index);
	}
}

void ADCCCharacterBase::Server_EquipInventoryItem_Implementation(int32 Index)
{
	if (Inventory)
	{
		Inventory->EquipItem(Index);
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

	// A dead crawler keeps exactly one chat send and nothing else - see
	// Server_SendChatMessage. Every other action is refused client-side too,
	// not just server-side, so there's no dead-air click with no feedback.
	if (bHasDied)
	{
		return;
	}

	// Climbing wins: while on a wall the button is the climb, not a swing.
	if (IsClimbing())
	{
		bClimbHeld = true;
		SetClimbInput(/*bUp=*/!bDescendHeld, /*bDown=*/bDescendHeld);
		return;
	}

	// A pending question owns the click too, so the player cannot attack past a
	// prompt they have not answered.
	if (PendingClimbPoint)
	{
		return;
	}

	// Hold-to-climb: with Climbing equipped the button is "grab and climb", held.
	// TickHoldToClimb keeps trying to grab while it is held, so walking into a
	// wall with the button down grabs it too.
	if (IsClimbingEquipped())
	{
		bClimbHeld = true;
		ClimbGrabRetry = 0.0f;
		if (!TryGrabWallAhead())
		{
			ShowLocalNotice(NSLOCTEXT("DCC", "ClimbHoldHint", "Keep holding and walk into a wall to grab it."));
		}
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
	// Climbing needs the skill equipped now (2026-11-13); a wall under the cursor
	// with nothing to fight just says how.
	if (!FindNearestHostileInRange() && HintEquipClimbingIfWall())
	{
		return;
	}

	// A ranged weapon fires at whatever the camera is pointed at instead of
	// auto-targeting the nearest hostile in a radius - see AttemptRangedAttack's
	// own comment for why that split exists. GetAttackRangeOverride() > 0 is
	// the same "is this thing ranged" signal GetEffectiveAttackRange already
	// keys off - every melee row in DT_DCCEquipment leaves it at 0.
	if (Equipment && Equipment->GetAttackRangeOverride() > 0.0f)
	{
		AttemptRangedAttack();
		return;
	}

	AttemptAttackNearest();
}

void ADCCCharacterBase::OnPrimaryReleased()
{
	bClimbHeld = false;
	if (IsClimbing())
	{
		// Letting go of the button is letting go of the wall.
		ReleaseClimb();
	}
}

void ADCCCharacterBase::OnSecondaryPressed()
{
	if (IsClimbing())
	{
		bDescendHeld = true;
		SetClimbInput(/*bUp=*/false, /*bDown=*/true);
		return;
	}

	// Aiming down a ranged weapon. Melee/bare hands have nothing to zoom in
	// on, so right-click stays exactly the no-op it always was for them.
	if (Equipment && Equipment->GetAttackRangeOverride() > 0.0f)
	{
		BeginAimZoom();
	}
}

void ADCCCharacterBase::OnSecondaryReleased()
{
	bDescendHeld = false;
	if (IsClimbing())
	{
		SetClimbInput(/*bUp=*/bClimbHeld, /*bDown=*/false);
		return;
	}

	EndAimZoom();
}

void ADCCCharacterBase::BeginAimZoom()
{
	if (bIsAiming)
	{
		return;
	}

	APlayerController* PC = Cast<APlayerController>(GetController());
	if (!PC || !PC->PlayerCameraManager)
	{
		return;
	}

	bIsAiming = true;
	PreAimFOV = PC->PlayerCameraManager->GetFOVAngle();
	PC->PlayerCameraManager->SetFOV(PreAimFOV * AimZoomFactor);
}

void ADCCCharacterBase::EndAimZoom()
{
	if (!bIsAiming)
	{
		return;
	}

	bIsAiming = false;

	if (const APlayerController* PC = Cast<APlayerController>(GetController()))
	{
		if (PC->PlayerCameraManager)
		{
			PC->PlayerCameraManager->SetFOV(PreAimFOV);
		}
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

void ADCCCharacterBase::Server_SendChatMessage_Implementation(EDCCChatChannel Channel, const FString& WhisperTargetName, const FText& Text)
{
	if (!HasAuthority() || Text.IsEmptyOrWhitespace())
	{
		return;
	}

	// Re-validated server-side: a dead crawler gets exactly one more send, then
	// none, regardless of what the client's UI thinks it already used.
	if (!CanSendChatMessage())
	{
		return;
	}

	UWorld* World = GetWorld();
	ADCCGameState* GameState = World ? World->GetGameState<ADCCGameState>() : nullptr;
	if (!GameState)
	{
		return;
	}

	if (Channel == EDCCChatChannel::Chatter)
	{
		// Players don't post to Chatter - it's the simulated audience. A client
		// asking for this channel is either a stale UI state or a bad actor;
		// either way, silently refuse rather than let it through.
		return;
	}

	const FString SenderDisplayName = DisplayName.IsEmpty() ? GetActorNameOrLabel() : DisplayName.ToString();
	GameState->AddChatMessage(Channel, SenderDisplayName, this, WhisperTargetName, Text);

	if (bHasDied)
	{
		bHasSentFinalMessage = true;
	}
}

TArray<FDCCModifierSource> ADCCCharacterBase::GetAbilityScoreBreakdown(EDCCAbility Ability) const
{
	TArray<FDCCModifierSource> Sources;

	if (Stats)
	{
		FDCCModifierSource& Base = Sources.AddDefaulted_GetRef();
		Base.Label = NSLOCTEXT("DCC", "BreakdownBaseScore", "Starting Score");
		Base.Amount = Stats->AbilityScores.GetScore(Ability);
	}

	if (Equipment)
	{
		Sources.Append(Equipment->GetAbilityBonusSources(Ability));
	}

	if (Class)
	{
		Sources.Append(Class->GetAbilityBonusSources(Ability));
	}

	return Sources;
}

TArray<FDCCModifierSource> ADCCCharacterBase::GetSkillRankBreakdown(FName SkillId) const
{
	TArray<FDCCModifierSource> Sources;

	if (SkillId.IsNone())
	{
		return Sources;
	}

	if (Skills && Skills->IsSkillKnown(SkillId))
	{
		FDCCModifierSource& Trained = Sources.AddDefaulted_GetRef();
		Trained.Label = NSLOCTEXT("DCC", "BreakdownTrainedRank", "Trained Rank");
		Trained.Amount = Skills->GetSkillRank(SkillId);
	}

	// SelectClass grants Skill bonuses as a genuine Rank via GrantSkill (see
	// UDCCClassComponent::ApplyBenefits) rather than a separate overlay, so
	// this is informational, not additional - the Class's contribution is
	// already folded into "Trained Rank" above, not stacked on top of it. Zero
	// Amount here would still be worth showing "included in", but Amount is
	// kept for context (how much of that Rank the Class alone accounts for).
	if (Class)
	{
		for (FDCCModifierSource& ClassSource : Class->GetSkillBonusSources(SkillId))
		{
			ClassSource.Label = FText::Format(
				NSLOCTEXT("DCC", "BreakdownClassIncluded", "{0} (included above)"), ClassSource.Label);
			Sources.Add(ClassSource);
		}
	}

	return Sources;
}

void ADCCCharacterBase::Server_RequestChatterBatch_Implementation(int32 Count)
{
	if (!HasAuthority())
	{
		return;
	}

	// Clamped rather than trusted - the count arrives from a client, and the
	// rate limit inside TryRollChatterBatch only governs how OFTEN a request
	// is honoured, not how much one request may ask for.
	const int32 Requested = FMath::Clamp(Count, 1, 12);

	UWorld* World = GetWorld();
	if (ADCCGameState* GameState = World ? World->GetGameState<ADCCGameState>() : nullptr)
	{
		GameState->TryRollChatterBatch(Requested);
	}
}

void ADCCCharacterBase::SelectSkill(FName SkillId)
{
	if (bHasDied)
	{
		return;
	}

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
	OnRep_ActiveClimbPoint(nullptr);

	// Attempting marks the skill, and here there is no failure state - simply
	// trying to climb is what trains Climbing.
	if (Skills && Skills->IsSkillKnown(Point->ClimbSkillId))
	{
		Skills->MarkSkillUsed(Point->ClimbSkillId);
	}

	// No snap onto the wall here any more: TickStamina eases the climber onto it.

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

void ADCCCharacterBase::Server_RequestCliffLift_Implementation(ADCCCliffLift* Lift)
{
	if (!IsAlive() || !Lift)
	{
		return;
	}

	// Same reach the dialogue/talk interactions already use - a lift request comes
	// from standing at the NPC offering it, not from anywhere on the floor.
	if (FVector::Dist(Lift->GetActorLocation(), GetActorLocation()) > 500.0f)
	{
		return;
	}

	Lift->RequestRaise();
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

void ADCCCharacterBase::UseClimbTool()
{
	if (Wallet && !Wallet->ClimbTools.IsEmpty())
	{
		Wallet->Server_UseClimbTool();
	}
}

void ADCCCharacterBase::NextClimbTool()
{
	if (Wallet && Wallet->ClimbTools.Num() > 1)
	{
		Wallet->Server_NextClimbTool();
	}
}

void ADCCCharacterBase::SnapTo(const FVector& Location)
{
	SetActorLocation(Location);
	if (HasAuthority() && !IsLocallyControlled())
	{
		Client_SnapTo(Location);
	}
}

void ADCCCharacterBase::Client_SnapTo_Implementation(FVector Location)
{
	SetActorLocation(Location);
}

void ADCCCharacterBase::ForceReleaseClimb()
{
	if (HasAuthority() && IsClimbing())
	{
		Server_ReleaseClimb_Implementation();
	}
}

bool ADCCCharacterBase::TryGrabWallAhead()
{
	UWorld* World = GetWorld();
	if (!World || IsClimbing() || !IsAlive())
	{
		return false;
	}

	FCollisionQueryParams Params;
	Params.AddIgnoredActor(this);
	Params.bTraceComplex = true;

	// Straight ahead at chest height, then the way the camera faces, since the
	// body turns to its movement and the player may be looking past it.
	FVector Directions[2] = { GetActorForwardVector(), GetActorForwardVector() };
	if (const AController* C = GetController())
	{
		Directions[1] = FRotator(0.0f, C->GetControlRotation().Yaw, 0.0f).Vector();
	}
	for (const FVector& Direction : Directions)
	{
		const FVector Start = GetActorLocation() + FVector(0, 0, 40.0f);
		FHitResult Hit;
		if (!World->LineTraceSingleByChannel(Hit, Start, Start + Direction * GenericClimbReachDistance, ECC_Visibility, Params))
		{
			continue;
		}
		if (Cast<ADCCCharacterBase>(Hit.GetActor()) || FMath::Abs(Hit.Normal.Z) > 0.5f)
		{
			continue;
		}
		if (ADCCClimbPoint* Point = Cast<ADCCClimbPoint>(Hit.GetActor()))
		{
			if (HasAuthority()) { Server_BeginClimb_Implementation(Point); } else { Server_BeginClimb(Point); }
			return true;
		}
		if (HasAuthority()) { Server_RequestGenericClimb_Implementation(Hit.Location, Hit.Normal); }
		else { Server_RequestGenericClimb(Hit.Location, Hit.Normal); }
		return true;
	}
	return false;
}

bool ADCCCharacterBase::HintEquipClimbingIfWall()
{
	APlayerController* PC = Cast<APlayerController>(GetController());
	UWorld* World = GetWorld();
	if (!PC || !World)
	{
		return false;
	}
	FVector CameraLocation;
	FRotator CameraRotation;
	PC->GetPlayerViewPoint(CameraLocation, CameraRotation);
	FCollisionQueryParams Params;
	Params.AddIgnoredActor(this);
	FHitResult Hit;
	if (!World->LineTraceSingleByChannel(Hit, CameraLocation, CameraLocation + CameraRotation.Vector() * 5000.0f, ECC_Visibility, Params)
		|| Cast<ADCCCharacterBase>(Hit.GetActor()) || FMath::Abs(Hit.Normal.Z) > 0.5f
		|| FVector::Dist(Hit.Location, GetActorLocation()) > GenericClimbReachDistance)
	{
		return false;
	}
	const float FootZ = GetActorLocation().Z - (GetCapsuleComponent() ? GetCapsuleComponent()->GetScaledCapsuleHalfHeight() : 90.0f);
	if (Hit.Location.Z < FootZ + 30.0f)
	{
		return false;
	}
	ShowLocalNotice(NSLOCTEXT("DCC", "ClimbEquipHint", "Equip Climbing (hotbar) and hold the left mouse button to climb."));
	return true;
}

void ADCCCharacterBase::TickHoldToClimb(float DeltaSeconds)
{
	if (!IsLocallyControlled())
	{
		return;
	}
	// Unequipping Climbing mid-climb lets go, like opening your hand.
	if (IsClimbing() && !IsClimbingEquipped())
	{
		ReleaseClimb();
		return;
	}

	if (!bClimbHeld || !IsClimbingEquipped())
	{
		return;
	}
	if (IsClimbing())
	{
		// The grab landed while the button was held: start climbing at once.
		const bool bWantUp = !bDescendHeld;
		if (bClimbInputUp != bWantUp || bClimbInputDown != bDescendHeld)
		{
			SetClimbInput(bWantUp, bDescendHeld);
		}
		return;
	}
	ClimbGrabRetry -= DeltaSeconds;
	if (ClimbGrabRetry <= 0.0f)
	{
		ClimbGrabRetry = 0.15f;
		TryGrabWallAhead();
	}
}

void ADCCCharacterBase::TickClimbCamera(float DeltaSeconds)
{
	if (!IsLocallyControlled())
	{
		return;
	}
	USpringArmComponent* Boom = FindComponentByClass<USpringArmComponent>();
	if (!Boom)
	{
		return;
	}
	if (BaseArmLength < 0.0f)
	{
		BaseArmLength = Boom->TargetArmLength;
		BaseSocketOffset = Boom->SocketOffset;
		bBaseCollisionTest = Boom->bDoCollisionTest;
		bBaseCameraLag = Boom->bEnableCameraLag;
	}

	const bool bClimbing = IsClimbing();
	const float Target = bClimbing ? 1.0f : 0.0f;
	ClimbCamBlend = FMath::FInterpTo(ClimbCamBlend, Target, DeltaSeconds, bClimbing ? 3.0f : 2.0f);
	if (ClimbCamBlend < 0.001f && !bClimbing)
	{
		ClimbCamBlend = 0.0f;
		Boom->TargetArmLength = BaseArmLength;
		Boom->SocketOffset = BaseSocketOffset;
		Boom->bDoCollisionTest = bBaseCollisionTest;
		Boom->bEnableCameraLag = bBaseCameraLag;
		return;
	}

	Boom->TargetArmLength = FMath::Lerp(BaseArmLength, FMath::Max(BaseArmLength, 750.0f), ClimbCamBlend);
	Boom->SocketOffset = FMath::Lerp(BaseSocketOffset, BaseSocketOffset + FVector(0.0f, 0.0f, 120.0f), ClimbCamBlend);
	Boom->bDoCollisionTest = !bClimbing && bBaseCollisionTest;
	Boom->bEnableCameraLag = true;
	Boom->CameraLagSpeed = 8.0f;

	// Face the wall: the camera sits behind the climber, outside the rock, with
	// the climber and the wall above in view. Pitch stays the player's, within reason.
	AController* C = GetController();
	if (bClimbing && C && ActiveClimbPoint)
	{
		const FVector Wall = ActiveClimbPoint->IsWallPlane()
			? -ActiveClimbPoint->WallNormal
			: (ActiveClimbPoint->GetActorLocation() - GetActorLocation()).GetSafeNormal2D();
		FRotator Want = C->GetControlRotation();
		Want.Yaw = FMath::FixedTurn(Want.Yaw, Wall.Rotation().Yaw, 220.0f * DeltaSeconds);
		Want.Pitch = FMath::ClampAngle(Want.Pitch, -45.0f, 5.0f);
		C->SetControlRotation(Want);
	}
}

int32 ADCCCharacterBase::ModifyIncomingDamage(int32 Damage, ADCCCharacterBase* Attacker)
{
	if (Damage <= 0 || !Wallet)
	{
		return Damage;
	}
	const FName Plate = Wallet->WornIn(DCCCosmetics::Shoulders);
	float Keep = 1.0f;
	if (Plate == FName(TEXT("CardboardPlate"))) { Keep = 0.85f; }
	else if (Plate == FName(TEXT("SpikedPauldrons"))) { Keep = 0.70f; }
	else if (Plate == FName(TEXT("PitBossPlate"))) { Keep = 0.55f; }
	(void)Attacker;
	return FMath::Max(1, FMath::RoundToInt(Damage * Keep));
}

void ADCCCharacterBase::Landed(const FHitResult& Hit)
{
	Super::Landed(Hit);
	if (!HasAuthority() || !Stats || !IsAlive())
	{
		return;
	}
	float Metres = (FallPeakZ - GetActorLocation().Z) / 100.0f;
	if (Wallet && Wallet->WornIn(DCCCosmetics::Back) == FName(TEXT("Cape")))
	{
		Metres *= 0.55f;
	}
	FallPeakZ = GetActorLocation().Z;
	const UWorld* World = GetWorld();
	if (Metres <= SafeFallMetres || (World && World->GetTimeSeconds() < FallSafeUntil))
	{
		return;
	}
	// Grows from nothing at SafeFallMetres to everything at FatalFallMetres.
	const float Fraction = FMath::Clamp((Metres - SafeFallMetres) / FMath::Max(1.0f, FatalFallMetres - SafeFallMetres), 0.0f, 1.0f);
	const int32 Damage = FMath::Max(1, FMath::CeilToInt(Stats->MaxHitPoints * Fraction));
	Stats->ApplyDamage(Damage);
	Client_ShowClimbNotice(FText::Format(NSLOCTEXT("DCC", "FallDamage", "You fell {0} m. That's going to leave a mark (-{1})."),
		FText::AsNumber(FMath::RoundToInt(Metres)), FText::AsNumber(Damage)));
	ADCCGameState::Log(this, EDCCLogCategory::Combat, FText::Format(
		NSLOCTEXT("DCC", "LogFallDamage", "{0} falls {1} m and takes {2}."),
		DisplayName.IsEmpty() ? FText::FromString(GetName()) : DisplayName, FText::AsNumber(FMath::RoundToInt(Metres)), FText::AsNumber(Damage)));
}

void ADCCCharacterBase::Server_ReleaseClimb_Implementation()
{
	bCrawling = false;
	if (!ActiveClimbPoint)
	{
		return;
	}

	ADCCClimbPoint* Previous = ActiveClimbPoint;
	ActiveClimbPoint = nullptr;
	OnRep_ActiveClimbPoint(Previous);

	bClimbInputUp = false;
	bClimbInputDown = false;

	// Same replication gap as climb-start/climb-top (see Client_SyncClimbLocation's
	// comment): a mid-climb release - letting go on purpose, or the grip failing
	// when stamina hits zero - is another raw server-side position that never
	// reaches the owning client on its own. Without this, that client keeps falling
	// from wherever its own local TickClimbMovementCosmetic guess had drifted to,
	// which is normally a few centimetres and invisible, but grows with how long the
	// climb has been running - exactly why this only reads as "randomly drops" on
	// long/tall climbs and not short ones. Found 2026-08-29 while investigating a
	// report that a specific tall climb (with no reachable top before this) still
	// dropped players despite the 2026-08-28 climbing fix, which only closed the
	// start-of-climb and reached-the-top gaps, not this one.
	if (!IsLocallyControlled())
	{
		Client_SyncClimbLocation(GetActorLocation(), false);
	}
}

void ADCCCharacterBase::OnRep_ActiveClimbPoint(ADCCClimbPoint* PreviousActiveClimbPoint)
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
		Movement->bJustTeleported = true;

		// The server already snapped itself onto the wall in
		// Server_BeginClimb_Implementation, immediately before this call - this
		// is what gets every OTHER machine there too, including (critically)
		// the climbing player's own client, which never receives an ordinary
		// ReplicatedMovement update for its own pawn. Without it, that client
		// just kept standing wherever it clicked from until a movement-error
		// correction eventually yanked it onto the wall - the "pushed back
		// 10-15 feet" symptom. See Client_SyncClimbLocation's comment for the
		// full explanation and the matching fix at the other end of a climb.
		if (!HasAuthority())
		{
			SetActorLocation(ActiveClimbPoint->GetAttachLocationFor(this, GetActorLocation().Z));
		}
	}
	else
	{
		// Falling, not walking - letting go halfway up a rock face should drop
		// the climber, and MOVE_Walking at that height would leave them standing
		// on nothing until something nudged them. A completed climb (top
		// reached) overrides this to Walking right afterward, server-side
		// immediately and on the owning client via Client_SyncClimbLocation.
		Movement->SetMovementMode(MOVE_Falling);
	}
}

void ADCCCharacterBase::TickClimbMovementCosmetic(float DeltaSeconds)
{
	if (!ActiveClimbPoint || !Stats)
	{
		return;
	}

	UDCCRuleset* Ruleset = Stats->GetRuleset();
	if (!Ruleset)
	{
		return;
	}

	if (bCrawling && TickLedgeCrawl(DeltaSeconds))
	{
		return; // the server ends the crawl and puts us on top
	}

	const bool bMoving = bClimbInputUp != bClimbInputDown;
	if (!bMoving)
	{
		const FVector Hold = ActiveClimbPoint->GetAttachLocationFor(this, GetActorLocation().Z);
		if (FVector::DistSquared2D(Hold, GetActorLocation()) > 1.0f)
		{
			SetActorLocation(ClimbStepToward(Hold, DeltaSeconds));
		}
		return;
	}

	const int32 ClimbRank = Skills ? Skills->GetSkillRank(ActiveClimbPoint->ClimbSkillId) : 0;
	const float Speed = Ruleset->GetClimbSpeed(Stats->GetEnhanced(), ClimbRank);
	const float Direction = bClimbInputUp ? 1.0f : -1.0f;

	// Deliberately no stamina, achievement, quest or "reached the top" logic
	// here - all of that is server-authoritative and arrives back through
	// ActiveClimbPoint/Client_SyncClimbLocation. Clamping to GetTopZ() just
	// stops this local guess from overshooting past the ledge while it waits
	// for the real dismount to land.
	float NewZ = GetActorLocation().Z + (Speed * Direction * DeltaSeconds);
	NewZ = FMath::Clamp(NewZ, ActiveClimbPoint->GetBottomZ(), ActiveClimbPoint->GetTopZ());

	SetActorLocation(ClimbStepToward(ActiveClimbPoint->GetAttachLocationFor(this, NewZ), DeltaSeconds));
}

bool ADCCCharacterBase::BeginLedgeCrawl(const FHitResult& Underside)
{
	UWorld* World = GetWorld();
	UCapsuleComponent* Capsule = GetCapsuleComponent();
	if (!World || !Capsule || !ActiveClimbPoint)
	{
		return false;
	}
	const float R = Capsule->GetScaledCapsuleRadius();
	const float H = Capsule->GetScaledCapsuleHalfHeight();
	const FVector Here = GetActorLocation();
	FCollisionQueryParams Params(SCENE_QUERY_STAT(DCCLedgeCrawl), false, this);

	// Outward, away from the wall.
	FVector Out = ActiveClimbPoint->IsWallPlane()
		? ActiveClimbPoint->WallNormal
		: (Here - ActiveClimbPoint->GetActorLocation()).GetSafeNormal2D();
	Out.Z = 0.0f;
	Out = Out.GetSafeNormal();
	if (Out.IsNearlyZero())
	{
		return false;
	}

	// The ledge's top, straight above us.
	FHitResult Top;
	// 0.85, not 0.6: a 45-degree overhang wedge is not a top you can crawl onto (2026-11-13).
	// From just above what we bumped into (a ledge slab is 2 m thick), not 4 m over our head: a start
	// point inside the next rock up reported a fake, perfectly flat "top" (2026-11-13). A hit that
	// starts inside something is never a top.
	const float TopStart = FMath::Max(Here.Z + H, Underside.ImpactPoint.Z) + 280.0f;
	if (!World->LineTraceSingleByChannel(Top, FVector(Here.X, Here.Y, TopStart), Here, ECC_WorldStatic, Params)
		|| Top.bStartPenetrating || Top.ImpactNormal.Z < 0.85f)
	{
		UE_LOG(LogTemp, Verbose, TEXT("[DCC Crawl] no flat top above %s (hit %d, %s, normal %s)"), *Here.ToCompactString(),
			Top.bBlockingHit, Top.GetActor() ? *Top.GetActor()->GetName() : TEXT("-"), *Top.ImpactNormal.ToCompactString());
		return false;
	}
	const float TopZ = Top.ImpactPoint.Z;

	// How far it sticks out: step outward along its top until there is no top.
	float Edge = 0.0f;
	for (float D = 40.0f; D <= 700.0f; D += 40.0f)
	{
		const FVector Probe = Here + Out * D;
		FHitResult Hit;
		if (!World->LineTraceSingleByChannel(Hit, FVector(Probe.X, Probe.Y, TopZ + 60.0f), FVector(Probe.X, Probe.Y, TopZ - 40.0f), ECC_WorldStatic, Params))
		{
			break;
		}
		Edge = D;
	}
	// Then to 5 cm, so the way round never grazes the lip (DCC.ClimbTest caught that).
	for (float D = Edge + 5.0f; D < Edge + 40.0f; D += 5.0f)
	{
		const FVector Probe = Here + Out * D;
		FHitResult Hit;
		if (!World->LineTraceSingleByChannel(Hit, FVector(Probe.X, Probe.Y, TopZ + 60.0f), FVector(Probe.X, Probe.Y, TopZ - 40.0f), ECC_WorldStatic, Params))
		{
			break;
		}
		Edge = D;
	}
	const float Clear = Edge + R + 30.0f;

	// Where to end up: on top, near the wall (or mid-ledge if that is blocked).
	FVector Stand = FVector::ZeroVector;
	for (const float D : { R + 70.0f, Edge * 0.5f, Edge - R })
	{
		const FVector Candidate(Here.X + Out.X * D, Here.Y + Out.Y * D, TopZ + H + 4.0f);
		// Blocking only: an overlap volume (a supply chest's pickup sphere on the ledge) is not "in the way".
		if (!World->OverlapBlockingTestByChannel(Candidate, FQuat::Identity, ECC_Pawn, FCollisionShape::MakeCapsule(R, H - 2.0f), Params))
		{
			Stand = Candidate;
			break;
		}
	}
	if (Stand.IsZero())
	{
		{
			TArray<FOverlapResult> Blockers;
			const FVector Probe(Here.X + Out.X * (R + 70.0f), Here.Y + Out.Y * (R + 70.0f), TopZ + H + 4.0f);
			World->OverlapMultiByChannel(Blockers, Probe, FQuat::Identity, ECC_Pawn, FCollisionShape::MakeCapsule(R, H - 2.0f), Params);
			FString Names;
			for (const FOverlapResult& O : Blockers) { if (O.bBlockingHit && O.GetActor()) { Names += O.GetActor()->GetName() + TEXT(" "); } }
			UE_LOG(LogTemp, Verbose, TEXT("[DCC Crawl] nowhere to stand on top at %s (edge %.0f, top %.0f from %s): blocked by %s"),
				*Here.ToCompactString(), Edge, TopZ, Top.GetActor() ? *Top.GetActor()->GetName() : TEXT("-"), *Names);
		}
		return false;
	}

	// Out along the underside, up around the lip, in over the top - with the
	// corners rounded, so it reads as crawling over rock, not sliding on rails.
	const FVector Under = Here + Out * Clear;
	const FVector Over(Under.X, Under.Y, TopZ + H + 6.0f); // just clears the lip: no floating above the top
	TArray<FVector> Path;
	Path.Add(Here);
	Path.Add(Here + Out * (Clear * 0.55f) - FVector(0, 0, 15.0f));
	Path.Add(Under - Out * 20.0f - FVector(0, 0, 15.0f));
	Path.Add(Under + FVector(0, 0, (Over.Z - Under.Z) * 0.35f));
	Path.Add(Under + FVector(0, 0, (Over.Z - Under.Z) * 0.75f) - Out * 10.0f);
	Path.Add(Over - Out * 45.0f);
	Path.Add(FMath::Lerp(Over, Stand, 0.55f) + FVector(0, 0, 2.0f));
	Path.Add(Stand);

	CrawlPath = Path;
	CrawlDistance = 0.0f;
	bCrawling = true;
	if (!IsLocallyControlled())
	{
		Client_BeginLedgeCrawl(Path);
	}
	return true;
}

void ADCCCharacterBase::Client_BeginLedgeCrawl_Implementation(const TArray<FVector>& Path)
{
	CrawlPath = Path;
	CrawlDistance = 0.0f;
	bCrawling = true;
}

bool ADCCCharacterBase::TickLedgeCrawl(float DeltaSeconds)
{
	if (!bCrawling || CrawlPath.Num() < 2)
	{
		bCrawling = false;
		return false;
	}
	// Along the path at a little over twice the climber's wall speed (it is a short
	// scramble, not a pitch): a novice takes a few seconds over an outcrop, an expert ~1.
	const float Speed = FMath::Clamp(GetClimbSpeedNow() * 2.2f, 120.0f, 420.0f);
	CrawlDistance += Speed * DeltaSeconds;

	float Remaining = CrawlDistance;
	for (int32 i = 1; i < CrawlPath.Num(); ++i)
	{
		const float Segment = FVector::Dist(CrawlPath[i - 1], CrawlPath[i]);
		if (Remaining <= Segment)
		{
			SetActorLocation(FMath::Lerp(CrawlPath[i - 1], CrawlPath[i], Segment > 0.0f ? Remaining / Segment : 1.0f));
			// The last stretch is standing up, so it eases the tilt off rather than leaning into it.
			CrawlDirection = i == CrawlPath.Num() - 1 ? FVector::ZeroVector : (CrawlPath[i] - CrawlPath[i - 1]).GetSafeNormal();
			return true;
		}
		Remaining -= Segment;
	}

	// Over. The server stands the climber on top and tells their machine.
	bCrawling = false;
	const FVector Stand = CrawlPath.Last();
	SetActorLocation(Stand);
	if (HasAuthority())
	{
		Server_ReleaseClimb_Implementation();
		SetActorLocation(Stand);
		if (UCharacterMovementComponent* Movement = GetCharacterMovement())
		{
			Movement->SetMovementMode(MOVE_Walking);
			Movement->bJustTeleported = true;
		}
		FallPeakZ = Stand.Z;
		if (!IsLocallyControlled())
		{
			Client_SyncClimbLocation(Stand, true);
		}
	}
	return false;
}

FVector ADCCCharacterBase::ClimbStepToward(const FVector& Target, float DeltaSeconds) const
{
	const FVector Here = GetActorLocation();
	FVector Flat(Target.X - Here.X, Target.Y - Here.Y, 0.0f);
	const float MaxStep = 700.0f * DeltaSeconds;
	if (Flat.SizeSquared() > MaxStep * MaxStep)
	{
		Flat = Flat.GetSafeNormal() * MaxStep;
	}
	return FVector(Here.X + Flat.X, Here.Y + Flat.Y, Target.Z);
}

void ADCCCharacterBase::Client_SyncClimbLocation_Implementation(FVector NewLocation, bool bLandOnGround)
{
	SetActorLocation(NewLocation);

	if (UCharacterMovementComponent* Movement = GetCharacterMovement())
	{
		Movement->SetMovementMode(bLandOnGround ? MOVE_Walking : MOVE_Falling);
		Movement->bJustTeleported = true;
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

void ADCCCharacterBase::ToggleMount()
{
	if (HasAuthority())
	{
		Server_ToggleMount_Implementation();
		return;
	}

	Server_ToggleMount();
}

void ADCCCharacterBase::Server_ToggleMount_Implementation()
{
	// Silently refused for a Class that never granted GrantsMount, same as
	// pressing 1-5 with no Skill in that slot does nothing rather than erroring.
	float MoveSpeedBonus = 0.0f;
	FText Label;
	if (!Class || !Class->GetMountBenefit(MoveSpeedBonus, Label))
	{
		return;
	}

	bIsMounted = !bIsMounted;
	ApplyDerivedMovementSpeed();
}

void ADCCCharacterBase::OnRep_Mounted()
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

	const int32 AthleticsRank = Skills ? Skills->GetSkillRank(TEXT("Athletics")) : 0;

	// Excess over the threshold, not raw mass - GetStaminaRegenRate treats
	// this as "how overloaded", already net of what Heavy Armor Training buys
	// back, so it never has to re-derive the threshold itself.
	const int32 HeavyArmorTrainingRank = Skills ? Skills->GetSkillRank(TEXT("HeavyArmorTraining")) : 0;
	const float EquippedMassKg = Equipment ? Equipment->GetTotalEquippedMass() : 0.0f;
	const float ExcessMassKg = FMath::Max(0.0f, EquippedMassKg - Ruleset->GetEncumbranceThresholdKg(HeavyArmorTrainingRank));

	bool bExerting = false;

	if (ActiveClimbPoint)
	{
		bExerting = true;
		StaminaGroundedFor = 0.0f;

		// Crawling over a ledge: follow the path; it costs climbing stamina as usual.
		if (bCrawling)
		{
			const float Fair = ClimbStaminaReferenceMax > 0.0f && Stats->MaxStamina > 0.0f ? Stats->MaxStamina / ClimbStaminaReferenceMax : 1.0f;
			const float Pack = (Wallet && Wallet->WornIn(DCCCosmetics::Back) == FName(TEXT("TrailPack"))) ? 0.80f : 1.0f;
			Stats->ConsumeStamina(Ruleset->GetClimbStaminaCost(ClimbRank) * FMath::Max(0.0f, ClimbStaminaMultiplier) * Pack * Fair * DeltaSeconds);
			TickLedgeCrawl(DeltaSeconds);
			return;
		}

		// Off the side of the rock (2026-11-13: Zoe could climb sideways past the
		// mountain's corner and hang on air). Nothing solid behind us at chest or
		// waist height means there is no wall here any more: let go and fall.
		if (HasAuthority() && ActiveClimbPoint->IsWallPlane())
		{
			FCollisionQueryParams WallQuery(SCENE_QUERY_STAT(DCCClimbEdge), false, this);
			const float Reach = (GetCapsuleComponent() ? GetCapsuleComponent()->GetScaledCapsuleRadius() : 35.0f) + 90.0f;
			const FVector At = GetActorLocation();
			FHitResult WallHit;
			const bool bChest = GetWorld()->LineTraceSingleByChannel(WallHit, At + FVector(0, 0, 40), At + FVector(0, 0, 40) - ActiveClimbPoint->WallNormal * Reach, ECC_WorldStatic, WallQuery);
			const bool bWaist = GetWorld()->LineTraceSingleByChannel(WallHit, At - FVector(0, 0, 30), At - FVector(0, 0, 30) - ActiveClimbPoint->WallNormal * Reach, ECC_WorldStatic, WallQuery);
			if (!bChest && !bWaist)
			{
				Server_ReleaseClimb_Implementation();
				return;
			}
		}

		// Ease onto the wall (see ClimbStepToward) even while hanging still.
		{
			const FVector Hold = ActiveClimbPoint->GetAttachLocationFor(this, GetActorLocation().Z);
			if (FVector::DistSquared2D(Hold, GetActorLocation()) > 1.0f)
			{
				SetActorLocation(ClimbStepToward(Hold, DeltaSeconds));
			}
		}

		// Sideways counts as climbing too (2026-11-14): traversing a face is work, not a free float.
		const bool bTraversing = GetVelocity().Size2D() > 15.0f;
		const bool bMoving = bClimbInputUp != bClimbInputDown || bTraversing;
		const float Pack = (Wallet && Wallet->WornIn(DCCCosmetics::Back) == FName(TEXT("TrailPack"))) ? 0.80f : 1.0f;
		float Cost = (bMoving
			? Ruleset->GetClimbStaminaCost(ClimbRank)
			: Ruleset->GetClingStaminaCost(ClimbRank)) * FMath::Max(0.0f, ClimbStaminaMultiplier) * Pack;
		if (ClimbStaminaReferenceMax > 0.0f && Stats->MaxStamina > 0.0f)
		{
			// The same share of anyone's bar per metre: a big bar costs proportionally more, and a faster
			// climber (Strength, Rank) pays per metre rather than finishing a pitch on fewer seconds of cost.
			// 0.70 makes a Rank 1 novice's bar last about 1.3 eight-metre pitches.
			const float Fair = Stats->MaxStamina / ClimbStaminaReferenceMax;
			Cost *= Fair * (bMoving ? FMath::Max(0.5f, GetClimbSpeedNow() / 46.0f) * 0.70f : 1.0f);
		}

		if (bMoving)
		{
			const float Speed = Ruleset->GetClimbSpeed(Stats->GetEnhanced(), ClimbRank);
			const float Direction = bClimbInputUp ? 1.0f : -1.0f;

			const float PreviousZ = GetActorLocation().Z;

			float NewZ = PreviousZ + (Speed * Direction * DeltaSeconds);
			NewZ = FMath::Clamp(NewZ, ActiveClimbPoint->GetBottomZ(), ActiveClimbPoint->GetTopZ());

			// Swept, not teleported (2026-11-13): climbing used to pass straight
			// through ledges and anything else on the face, and the movement
			// component then shoved the capsule back out - the jitter.
			const FVector Target = ClimbStepToward(ActiveClimbPoint->GetAttachLocationFor(this, NewZ), DeltaSeconds);
			if (UCapsuleComponent* Capsule = GetCapsuleComponent())
			{
				FCollisionQueryParams Sweep(SCENE_QUERY_STAT(DCCClimbSweep), false, this);
				FHitResult Blocked;
				if (GetWorld()->SweepSingleByChannel(Blocked, GetActorLocation(), Target, FQuat::Identity, ECC_Pawn,
					FCollisionShape::MakeCapsule(Capsule->GetScaledCapsuleRadius() - 2.0f, Capsule->GetScaledCapsuleHalfHeight() - 2.0f), Sweep))
				{
					// Climbing up into the underside of a ledge: crawl out, around and over it.
					// Also when the hit is the ledge's side corner rather than its flat
					// underside (climbing up beside an outcrop): anything struck above our
					// middle counts. DCC.ClimbTest stalled 9 m up a face on exactly that.
					const bool bFromBelow = Blocked.ImpactNormal.Z < -0.3f
						|| Blocked.ImpactPoint.Z > GetActorLocation().Z + Capsule->GetScaledCapsuleHalfHeight() * 0.5f;
					if (Direction > 0.0f && bFromBelow && BeginLedgeCrawl(Blocked))
					{
						return;
					}
					// Anything else in the way (a ledge below, an overhang with no room): hold still.
					NewZ = PreviousZ;
				}
				else
				{
					SetActorLocation(Target);
				}
			}

			// Climbing down to the ground steps off the wall.
			if (Direction < 0.0f && NewZ <= ActiveClimbPoint->GetBottomZ() + 100.0f)
			{
				Server_ReleaseClimb_Implementation();
				return;
			}

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
					Movement->bJustTeleported = true;
				}

				// Dismount is a raw SetActorLocation, same as the climb-start
				// attach - which, same as there, never reaches the climbing
				// player's OWN client via ordinary replication. Left alone,
				// that client would sit near the ledge (wherever
				// TickClimbMovementCosmetic last put it) until a movement-error
				// correction eventually dragged it to the real dismount spot -
				// which is what "top doesn't match the floor" / "drops off the
				// side" looked like from the climber's own screen. See
				// Client_SyncClimbLocation's header comment for the full story.
				if (!IsLocallyControlled())
				{
					Client_SyncClimbLocation(Dismount, true);
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

			if (Skills)
			{
				Skills->MarkSkillUsed(TEXT("Athletics"));
			}

			if (!Stats->ConsumeStamina(
				Ruleset->GetSprintStaminaCost(Stats->GetEnhanced(), AthleticsRank) * DeltaSeconds))
			{
				Server_SetSprinting_Implementation(false);
			}
		}
	}

	// Recovery waits until the feet are actually down, and stays off through a short stall tap.
	// Letting go against the rock used to refill the bar in the air, so a climber could
	// grab, stall, and grab again all the way up the face.
	UCharacterMovementComponent* Move = GetCharacterMovement();
	const bool bFeetDown = Move && Move->IsMovingOnGround() && !Move->IsFalling();
	if (!bFeetDown)
	{
		StaminaGroundedFor = 0.0f;
	}
	else if (!bExerting)
	{
		StaminaGroundedFor += DeltaSeconds;
	}

	if (!bExerting && bFeetDown && StaminaGroundedFor >= 0.85f)
	{
		const float Fair = ClimbStaminaReferenceMax > 0.0f && Stats->MaxStamina > 0.0f ? Stats->MaxStamina / ClimbStaminaReferenceMax : 1.0f;
		const float Halo = (Wallet && Wallet->WornIn(DCCCosmetics::Aura) == FName(TEXT("UsedHalo"))) ? 1.25f : 1.0f;
		// On the mountain, a ledge you reached still refills, just not in one breath.
		// A camp (the arena sets the multiplier to 2) stays the fast rest.
		const float Ledge = (ClimbStaminaReferenceMax > 0.0f && StaminaRegenMultiplier < 1.5f) ? 0.55f : 1.0f;
		Stats->RecoverStamina(
			Ruleset->GetStaminaRegenRate(Stats->GetEnhanced(), AthleticsRank, ExcessMassKg) * FMath::Max(0.0f, StaminaRegenMultiplier) * Halo * Fair * Ledge * DeltaSeconds);
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

ADCCDialogueNPC* ADCCCharacterBase::FindDialogueNPCUnderCursor() const
{
	AActor* Hit = SweepForInteractionCandidate();
	ADCCDialogueNPC* Speaker = Cast<ADCCDialogueNPC>(Hit);
	if (!Speaker)
	{
		return nullptr;
	}

	if (FVector::Dist(GetActorLocation(), Speaker->GetActorLocation()) > InteractionRange)
	{
		return nullptr;
	}

	return Speaker;
}

ADCCLootBoxStation* ADCCCharacterBase::FindLootBoxStationUnderCursor() const
{
	AActor* Hit = SweepForInteractionCandidate();
	ADCCLootBoxStation* Station = Cast<ADCCLootBoxStation>(Hit);
	if (!Station)
	{
		return nullptr;
	}

	if (FVector::Dist(GetActorLocation(), Station->GetActorLocation()) > InteractionRange)
	{
		return nullptr;
	}

	return Station;
}

ADCCPortalConsole* ADCCCharacterBase::FindPortalConsoleUnderCursor() const
{
	AActor* Hit = SweepForInteractionCandidate();
	ADCCPortalConsole* Console = Cast<ADCCPortalConsole>(Hit);
	if (!Console)
	{
		return nullptr;
	}

	if (FVector::Dist(GetActorLocation(), Console->GetActorLocation()) > InteractionRange)
	{
		return nullptr;
	}

	return Console;
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
	// A System window on screen owns E until it is read. Checked before death,
	// menus and everything else, because the window is drawn over all of them.
	if (IsLocallyControlled() && Onboarding && Onboarding->DismissActiveWindow())
	{
		return;
	}

	if (bHasDied)
	{
		return;
	}

	// A vendor/loot/conversation target destroyed out from under an open menu
	// - a corpse swept up by the body collector, a loot container removed, an
	// ally whose corpse finally expired mid-conversation - leaves these set
	// but stale. TObjectPtr's bool check only tests for null, not validity,
	// so left unguarded the branches below would still treat a dangling
	// pointer as "menu open," silently re-closing a dead menu on this press
	// instead of reaching whatever is actually under the crosshair right now.
	// Found 2026-08-29 chasing a "E does nothing" report - clearing here lets
	// THIS press fall through to the real target below rather than eating it.
	if (VendorTargetActor && !IsValid(VendorTargetActor))
	{
		CloseVendorMenu();
	}
	if (LootTargetActor && !IsValid(LootTargetActor))
	{
		CloseLootMenu();
	}
	if (InteractionTarget && !IsValid(InteractionTarget))
	{
		CloseInteraction();
	}
	if (OathgateTargetActor && !IsValid(OathgateTargetActor))
	{
		CloseOathgatePrompt();
	}
	if (NPCInteractionTarget && !IsValid(NPCInteractionTarget))
	{
		CloseNPCInteraction();
	}

	// The emote picker owns E first if it's open - it has no world target to
	// fall back to, so nothing below it would ever get a chance otherwise.
	if (bEmoteMenuOpen)
	{
		ConfirmEmote();
		return;
	}

	// The vendor and loot menus own E first, if one is open - same reason the
	// conversation menu is checked before anything else below.
	if (VendorTargetActor)
	{
		HandleVendorMenuConfirm();
		return;
	}

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

	// The Oathgate confirm prompt owns E next, same reason as vendor/loot above -
	// it is server-armed (Client_ShowOathgatePrompt), not opened by this press,
	// so it has to be checked before anything below tries to open something else.
	if (OathgateTargetActor)
	{
		HandleOathgateMenuConfirm();
		return;
	}

	// The NPC menu (Talk/Shop/Leave) owns E next, same reason as everything
	// above - it is already open, so this press is a choice within it, not a
	// new interaction.
	if (NPCInteractionTarget)
	{
		HandleNPCInteractionConfirm();
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

	// Still nobody to talk to as an ally: a non-combat speaker (the bartender
	// and its kind) under the crosshair opens its own small menu - Talk, and
	// Shop if this is an ADCCVendorNPC - rather than the full ally menu above,
	// which ADCCDialogueNPC deliberately cannot open (it is not an
	// ADCCCharacterBase). No hostile-nearby confirm step, matching loot below -
	// talking to the bartender mid-fight is odd, not dangerous.
	if (ADCCDialogueNPC* Speaker = FindDialogueNPCUnderCursor())
	{
		PendingTalkTarget = nullptr;
		NPCInteractionTarget = Speaker;
		NPCInteractionIndex = 0;
		BuildNPCInteractionOptions();
		return;
	}

	// Still nobody to talk to: a loot box station under the crosshair opens
	// one owned box of its tier (or explains there is nothing to open).
	// Checked before the corpse/chest branch below for the same reason
	// dialogue is - it is an even smaller interaction than either.
	if (ADCCLootBoxStation* Station = FindLootBoxStationUnderCursor())
	{
		PendingTalkTarget = nullptr;
		Station->TryPlayerInteract(this);
		return;
	}

	// Still nobody to talk to: the science lab's portal switch, same tiny
	// "press it" interaction as a loot box station.
	if (ADCCPortalConsole* Console = FindPortalConsoleUnderCursor())
	{
		PendingTalkTarget = nullptr;
		Console->TryPlayerInteract(this);
		return;
	}

	// Puzzles/ interactables, same one-shot "press it" shape. Both resolve
	// through the shared Visibility sweep, so nothing new is being traced here.
	{
		AActor* PuzzleHit = SweepForInteractionCandidate();
		const bool bInRange = PuzzleHit
			&& FVector::Dist(GetActorLocation(), PuzzleHit->GetActorLocation()) <= InteractionRange;

		if (bInRange)
		{
			if (ADCCBreakerSwitch* Switch = Cast<ADCCBreakerSwitch>(PuzzleHit))
			{
				PendingTalkTarget = nullptr;
				Switch->TryPlayerInteract(this);
				return;
			}

			// Checked after the switch because a door is the broader type -
			// ADCCComplianceDoor is an ADCCPuzzleDoor and must not be caught
			// by an earlier, narrower branch.
			if (ADCCPuzzleDoor* Door = Cast<ADCCPuzzleDoor>(PuzzleHit))
			{
				PendingTalkTarget = nullptr;
				Door->TryPlayerInteract(this);
				return;
			}

			// A rover is bulkier than anything else here, so the sweep can find
			// it from further out - hence the doubled range check server-side.
			if (ADCCRover* Rover = Cast<ADCCRover>(PuzzleHit))
			{
				PendingTalkTarget = nullptr;
				Rover->TryPlayerInteract(this);
				return;
			}
		}
	}

	// Still nobody to talk to: a corpse or chest under the crosshair opens the
	// loot menu instead. No hostile-nearby confirm step here - searching a body
	// or a crate is exactly the kind of thing you do mid-fight, the same reason
	// the old Loot option never gated on IsHostileNearby either.
	if (AActor* LootSource = FindLootTargetUnderCursor())
	{
		PendingTalkTarget = nullptr;

		// A chest (not a body) creaks open as the menu comes up.
		if (LootSource->IsA<ADCCLootContainer>() && LootTargetActor != LootSource)
		{
			DCCChestSound::PlayOpen(this, LootSource->GetActorLocation());
		}
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

void ADCCCharacterBase::CycleVendorMenu(int32 Delta)
{
	// The Drink Now/Stow sub-prompt owns the wheel while it is open - just two
	// rows, no Leave (Esc still cancels the whole menu, same as everywhere else).
	if (bVendorConfirmingDrink)
	{
		VendorMenuIndex = ((VendorMenuIndex + Delta) % 2 + 2) % 2;
		return;
	}

	const ADCCVendorNPC* Vendor = Cast<ADCCVendorNPC>(VendorTargetActor.Get());
	if (!Vendor)
	{
		return;
	}

	// +1 for the trailing "Leave" row, same convention as CycleLootMenu.
	const int32 Count = Vendor->SellableEquipmentIds.Num() + Vendor->SellableConsumableIds.Num() + 1;
	if (Count <= 0)
	{
		return;
	}

	VendorMenuIndex = ((VendorMenuIndex + Delta) % Count + Count) % Count;
}

void ADCCCharacterBase::CloseVendorMenu()
{
	VendorTargetActor = nullptr;
	VendorMenuIndex = 0;
	bVendorConfirmingDrink = false;
	VendorPendingDrinkId = NAME_None;
}

void ADCCCharacterBase::CycleOathgateMenu(int32 Delta)
{
	if (!OathgateTargetActor)
	{
		return;
	}

	// Two rows: Enter, Turn Away.
	OathgateMenuIndex = ((OathgateMenuIndex + Delta) % 2 + 2) % 2;
}

void ADCCCharacterBase::CloseOathgatePrompt()
{
	OathgateTargetActor = nullptr;
	OathgateMenuIndex = 0;
}

void ADCCCharacterBase::HandleOathgateMenuConfirm()
{
	ADCCOathgate* Gate = OathgateTargetActor;

	// Row 0 = Enter, row 1 = Turn Away - only Enter does anything.
	if (Gate && OathgateMenuIndex == 0)
	{
		if (HasAuthority())
		{
			Server_ActivateOathgate_Implementation(Gate);
		}
		else
		{
			Server_ActivateOathgate(Gate);
		}
	}

	CloseOathgatePrompt();
}

void ADCCCharacterBase::Client_ShowOathgatePrompt_Implementation(ADCCOathgate* Gate)
{
	// Gate null is how ADCCOathgate::HandleTriggerEndOverlap tells this client
	// to close the prompt again - walking back out without deciding reads as
	// "turn away" on its own, same as picking that row explicitly would.
	if (!Gate)
	{
		CloseOathgatePrompt();
		return;
	}

	OathgateTargetActor = Gate;
	OathgateMenuIndex = 0;
}

void ADCCCharacterBase::Server_ActivateOathgate_Implementation(ADCCOathgate* Gate)
{
	// Re-validated here rather than trusted from the client's confirm - the
	// usual "client picks, server re-validates" shape everything else in this
	// file follows. ADCCOathgate::RequestActivation does its own eligibility
	// check too; this is just the character-side half (alive, actually a
	// crawler, not still on the other side of the map somehow).
	if (!Gate || !IsAlive() || IsDowned() || !IsCrawler())
	{
		return;
	}

	Gate->RequestActivation(this);
}

void ADCCCharacterBase::Server_ActivatePortalConsole_Implementation(ADCCPortalConsole* Console)
{
	if (!Console || !Console->Portal)
	{
		return;
	}

	if (FVector::Dist(GetActorLocation(), Console->GetActorLocation()) > InteractionRange)
	{
		return;
	}

	Console->Portal->Activate();
}

void ADCCCharacterBase::Server_ForcePuzzleDoor_Implementation(ADCCPuzzleDoor* Door)
{
	if (!Door)
	{
		return;
	}

	if (FVector::Dist(GetActorLocation(), Door->GetActorLocation()) > InteractionRange)
	{
		return;
	}

	// The door owns the rules - Favor cost, Strength gate, whether forcing is
	// allowed at all. Re-checking any of that here would just be a second
	// place to keep in sync.
	Door->ForceOpen(this);
}

void ADCCCharacterBase::Server_EnterRover_Implementation(ADCCRover* Rover)
{
	if (!Rover || Rover->IsOccupied())
	{
		return;
	}

	if (FVector::Dist(GetActorLocation(), Rover->GetActorLocation()) > InteractionRange * 2.0f)
	{
		return;
	}

	Rover->EnterRover(this);
}

void ADCCCharacterBase::Server_EjectRoverPassengers_Implementation(ADCCRover* Rover)
{
	if (Rover)
	{
		Rover->EjectPassengers();
	}
}

void ADCCCharacterBase::Server_ExitRover_Implementation(ADCCRover* Rover)
{
	if (Rover)
	{
		Rover->ExitRover();
	}
}

void ADCCCharacterBase::Server_ToggleRoverWindows_Implementation(ADCCRover* Rover)
{
	if (Rover && Rover->Driver == this)
	{
		Rover->ToggleWindows();
	}
}

void ADCCCharacterBase::Server_ToggleRoverRadio_Implementation(ADCCRover* Rover)
{
	if (Rover && Rover->Driver == this)
	{
		Rover->ToggleRadio();
	}
}

void ADCCCharacterBase::Server_CycleRoverRadio_Implementation(ADCCRover* Rover)
{
	if (Rover && Rover->Driver == this)
	{
		Rover->CycleRadio();
	}
}

void ADCCCharacterBase::Server_DepositToEscrow_Implementation(ADCCEscrowLock* Lock)
{
	if (!Lock)
	{
		return;
	}

	if (FVector::Dist(GetActorLocation(), Lock->GetActorLocation()) > InteractionRange)
	{
		return;
	}

	// Routed back through the lock's own interaction so the deposit-then-crowbar
	// ordering lives in exactly one place.
	Lock->TryPlayerInteract(this);
}

void ADCCCharacterBase::Server_FlipBreakerSwitch_Implementation(ADCCBreakerSwitch* Switch)
{
	if (!Switch)
	{
		return;
	}

	if (FVector::Dist(GetActorLocation(), Switch->GetActorLocation()) > InteractionRange)
	{
		return;
	}

	Switch->Flip();
}

void ADCCCharacterBase::EnterAwaitingOtherCrawlers()
{
	if (!HasAuthority() || bAwaitingOtherCrawlers)
	{
		return;
	}

	bAwaitingOtherCrawlers = true;
}

void ADCCCharacterBase::OnRep_AwaitingOtherCrawlers()
{
	if (!bAwaitingOtherCrawlers)
	{
		return;
	}

	// The black screen owns the view now - nothing underneath it should still
	// be open, same reasoning as every other terminal-state transition in
	// this class closing whatever menu happened to be up.
	CloseInteraction();
	CloseLootMenu();
	CloseVendorMenu();
	CloseOathgatePrompt();
	CloseNPCInteraction();
}

void ADCCCharacterBase::BuildNPCInteractionOptions()
{
	NPCInteractionOptions.Reset();

	if (!NPCInteractionTarget)
	{
		return;
	}

	NPCInteractionOptions.Add(EDCCNPCInteractionOption::Talk);

	// Only a shopkeeper gets the Shop row - a plain ADCCDialogueNPC has
	// nothing to sell, and an empty/disabled row would just be confusing.
	if (Cast<ADCCVendorNPC>(NPCInteractionTarget.Get()))
	{
		NPCInteractionOptions.Add(EDCCNPCInteractionOption::Shop);
	}

	TArray<FName> Offerable;
	GetOfferableQuestIds(Offerable);
	if (!Offerable.IsEmpty())
	{
		NPCInteractionOptions.Add(EDCCNPCInteractionOption::Quests);
	}

	if (const ADCCDialogueNPC_LiftAttendant* LiftAttendant = Cast<ADCCDialogueNPC_LiftAttendant>(NPCInteractionTarget.Get()))
	{
		if (LiftAttendant->LinkedLift)
		{
			NPCInteractionOptions.Add(EDCCNPCInteractionOption::RideLift);
		}
	}

	if (const ADCCDialogueNPC_RaceBookie* Bookie = Cast<ADCCDialogueNPC_RaceBookie>(NPCInteractionTarget.Get()))
	{
		// Same "only when it would actually do something" gate as RideLift
		// above - a bookie with no circuit wired up has nothing to sell.
		if (Bookie->LinkedTrack)
		{
			NPCInteractionOptions.Add(EDCCNPCInteractionOption::RaceBet);
		}
	}

	NPCInteractionOptions.Add(EDCCNPCInteractionOption::Leave);

	NPCInteractionIndex = FMath::Clamp(NPCInteractionIndex, 0, NPCInteractionOptions.Num() - 1);
}

void ADCCCharacterBase::GetOfferableQuestIds(TArray<FName>& OutIds) const
{
	OutIds.Reset();

	if (!NPCInteractionTarget)
	{
		return;
	}

	const UDCCQuestComponent* QuestComp = UDCCQuestComponent::Get(this);

	for (const FName QuestId : NPCInteractionTarget->OfferedQuestIds)
	{
		// Already active or already finished is not something to offer again -
		// StartQuest would refuse it anyway (see its own comment on why), this
		// just keeps it off the list instead of showing a row that does nothing.
		if (QuestComp && (QuestComp->IsQuestActive(QuestId) || QuestComp->IsQuestComplete(QuestId)))
		{
			continue;
		}

		OutIds.Add(QuestId);
	}
}

void ADCCCharacterBase::CycleNPCInteractionMenu(int32 Delta)
{
	int32 Count;
	if (bNPCShowingQuests)
	{
		// +1 for the trailing "Back" row, same convention as the loot menu's
		// itemised page.
		TArray<FName> Offerable;
		GetOfferableQuestIds(Offerable);
		Count = Offerable.Num() + 1;
	}
	else
	{
		Count = NPCInteractionOptions.Num();
	}

	if (Count <= 0)
	{
		return;
	}

	NPCInteractionIndex = ((NPCInteractionIndex + Delta) % Count + Count) % Count;
}

void ADCCCharacterBase::CloseNPCInteraction()
{
	NPCInteractionTarget = nullptr;
	NPCInteractionOptions.Reset();
	NPCInteractionIndex = 0;
	bNPCShowingQuests = false;
}

void ADCCCharacterBase::HandleNPCInteractionConfirm()
{
	ADCCDialogueNPC* Speaker = NPCInteractionTarget;
	if (!Speaker)
	{
		CloseNPCInteraction();
		return;
	}

	// Walking away closes the menu, same as every other menu in this class.
	if (FVector::Dist(GetActorLocation(), Speaker->GetActorLocation()) > InteractionRange)
	{
		CloseNPCInteraction();
		return;
	}

	if (bNPCShowingQuests)
	{
		TArray<FName> Offerable;
		GetOfferableQuestIds(Offerable);

		// The row after the last quest is Back, same layout as the loot menu's
		// itemised page.
		if (Offerable.IsValidIndex(NPCInteractionIndex))
		{
			const FName QuestId = Offerable[NPCInteractionIndex];
			if (HasAuthority())
			{
				Server_AcceptQuestFromNPC_Implementation(Speaker, QuestId);
			}
			else
			{
				Server_AcceptQuestFromNPC(Speaker, QuestId);
			}
		}

		// Back to the top menu either way - rebuilt, since accepting a quest
		// changes what GetOfferableQuestIds returns next time it is checked.
		bNPCShowingQuests = false;
		NPCInteractionIndex = 0;
		BuildNPCInteractionOptions();
		return;
	}

	if (!NPCInteractionOptions.IsValidIndex(NPCInteractionIndex))
	{
		CloseNPCInteraction();
		return;
	}

	const EDCCNPCInteractionOption Option = NPCInteractionOptions[NPCInteractionIndex];

	switch (Option)
	{
	case EDCCNPCInteractionOption::Talk:
		// Called directly rather than through Speaker->TryPlayerInteract -
		// that virtual is what makes E open the Shop menu instead on an
		// ADCCVendorNPC, which is now its own separate row below rather than
		// something Talk should also trigger.
		Speaker->SayNextLine();

		// Stays open, rebuilt - repeated Talk presses step through the
		// script one line at a time, same as it always has.
		BuildNPCInteractionOptions();
		return;

	case EDCCNPCInteractionOption::Shop:
		if (ADCCVendorNPC* Vendor = Cast<ADCCVendorNPC>(Speaker))
		{
			OpenVendorMenu(Vendor);
		}
		CloseNPCInteraction();
		return;

	case EDCCNPCInteractionOption::Quests:
		bNPCShowingQuests = true;
		NPCInteractionIndex = 0;
		return;

	case EDCCNPCInteractionOption::RideLift:
		if (ADCCDialogueNPC_LiftAttendant* LiftAttendant = Cast<ADCCDialogueNPC_LiftAttendant>(Speaker))
		{
			LiftAttendant->RequestLift(this);
		}
		CloseNPCInteraction();
		return;

	case EDCCNPCInteractionOption::RaceBet:
		if (ADCCDialogueNPC_RaceBookie* Bookie = Cast<ADCCDialogueNPC_RaceBookie>(Speaker))
		{
			// Server-side money and spawning, so this has to route through the
			// server on a client. Same nominate-don't-dictate shape as
			// Server_AcceptQuestFromNPC just below - the client names the NPC,
			// the server re-checks range and does the work.
			if (HasAuthority())
			{
				Server_RequestRaceBet_Implementation(Bookie);
			}
			else
			{
				Server_RequestRaceBet(Bookie);
			}
		}
		CloseNPCInteraction();
		return;

	case EDCCNPCInteractionOption::Leave:
	default:
		CloseNPCInteraction();
		return;
	}
}

void ADCCCharacterBase::Server_RequestRaceBet_Implementation(ADCCDialogueNPC_RaceBookie* Bookie)
{
	if (!Bookie)
	{
		return;
	}

	if (FVector::Dist(GetActorLocation(), Bookie->GetActorLocation()) > InteractionRange)
	{
		return;
	}

	Bookie->PlaceBetAndStartRace(this);
}

void ADCCCharacterBase::Server_AcceptQuestFromNPC_Implementation(ADCCDialogueNPC* NPC, FName QuestId)
{
	if (!NPC || !NPC->OfferedQuestIds.Contains(QuestId))
	{
		return;
	}

	if (FVector::Dist(GetActorLocation(), NPC->GetActorLocation()) > InteractionRange)
	{
		return;
	}

	if (UDCCQuestComponent* QuestComp = UDCCQuestComponent::Get(this))
	{
		QuestComp->StartQuest(QuestId);
	}
}

void ADCCCharacterBase::HandleVendorMenuConfirm()
{
	ADCCVendorNPC* Vendor = Cast<ADCCVendorNPC>(VendorTargetActor.Get());
	if (!Vendor)
	{
		CloseVendorMenu();
		return;
	}

	// Walking away closes the menu, same as the conversation and loot ones.
	if (FVector::Dist(GetActorLocation(), Vendor->GetActorLocation()) > InteractionRange)
	{
		CloseVendorMenu();
		return;
	}

	// The sub-prompt confirms first if it is open - same "innermost menu owns
	// E first" rule HandleInteractPressed already applies at the top level.
	if (bVendorConfirmingDrink)
	{
		const bool bDrinkNow = (VendorMenuIndex == 0);
		const FName ConsumableItemId = VendorPendingDrinkId;

		bVendorConfirmingDrink = false;
		VendorPendingDrinkId = NAME_None;
		VendorMenuIndex = 0;

		if (HasAuthority())
		{
			Server_PurchaseDrink_Implementation(Vendor, ConsumableItemId, bDrinkNow);
		}
		else
		{
			Server_PurchaseDrink(Vendor, ConsumableItemId, bDrinkNow);
		}
		return;
	}

	const int32 EquipCount = Vendor->SellableEquipmentIds.Num();
	const int32 DrinkCount = Vendor->SellableConsumableIds.Num();
	const int32 LeaveIndex = EquipCount + DrinkCount;

	if (VendorMenuIndex < 0 || VendorMenuIndex >= LeaveIndex)
	{
		CloseVendorMenu();
		return;
	}

	// Equipment buys immediately and the menu stays open, like the loot
	// menu's "take one" - buying a round is one trip to the bar, not one item.
	if (VendorMenuIndex < EquipCount)
	{
		const FName ItemId = Vendor->SellableEquipmentIds[VendorMenuIndex];

		if (HasAuthority())
		{
			Server_PurchaseEquipment_Implementation(Vendor, ItemId);
		}
		else
		{
			Server_PurchaseEquipment(Vendor, ItemId);
		}
		return;
	}

	// A drink is a choice, not a one-step buy - arm the sub-prompt instead of
	// spending gold yet. Nothing is bought until that confirms.
	VendorPendingDrinkId = Vendor->SellableConsumableIds[VendorMenuIndex - EquipCount];
	bVendorConfirmingDrink = true;
	VendorMenuIndex = 0;
}

// ---------------------------------------------------------------------------
// The shop screen (2026-11-13)
// ---------------------------------------------------------------------------

int32 ADCCCharacterBase::GetShopSellPrice(int32 BagIndex) const
{
	if (!Inventory || !Inventory->Items.IsValidIndex(BagIndex))
	{
		return 0;
	}
	const FDCCInventoryItem& Item = Inventory->Items[BagIndex];
	int32 Value = 0;
	FDCCEquipmentDef Def;
	if (!Item.EquipmentItemId.IsNone() && Equipment && Equipment->FindItem(Item.EquipmentItemId, Def))
	{
		Value = Def.GoldValue;
	}
	else if (const ADCCPickup* Defaults = Item.PickupClass ? Item.PickupClass->GetDefaultObject<ADCCPickup>() : nullptr)
	{
		Value = Defaults->GoldValue;
	}
	return FMath::Max(1, Value / 2);
}

void ADCCCharacterBase::ShopBuy(AActor* VendorActor, FName ItemId, bool bConsumable)
{
	if (HasAuthority())
	{
		Server_ShopBuy_Implementation(VendorActor, ItemId, bConsumable);
	}
	else
	{
		Server_ShopBuy(VendorActor, ItemId, bConsumable);
	}
}

void ADCCCharacterBase::ShopSell(AActor* VendorActor, int32 BagIndex)
{
	if (HasAuthority())
	{
		Server_ShopSell_Implementation(VendorActor, BagIndex);
	}
	else
	{
		Server_ShopSell(VendorActor, BagIndex);
	}
}

void ADCCCharacterBase::Server_ShopBuy_Implementation(AActor* VendorActor, FName ItemId, bool bConsumable)
{
	const ADCCVendorNPC* Vendor = Cast<ADCCVendorNPC>(VendorActor);
	if (!Vendor || !Stats || !Inventory || !IsAlive() || IsDowned()
		|| FVector::Dist(GetActorLocation(), Vendor->GetActorLocation()) > InteractionRange * 1.5f)
	{
		return;
	}

	FText Name;
	if (bConsumable)
	{
		FDCCConsumableDef Def;
		if (!Vendor->SellableConsumableIds.Contains(ItemId) || !Inventory->FindConsumable(ItemId, Def) || !Stats->SpendGold(Def.GoldValue))
		{
			return;
		}
		Inventory->AddConsumable(ItemId, 1);
		Name = Def.DisplayName;
	}
	else
	{
		FDCCEquipmentDef Def;
		if (!Vendor->SellableEquipmentIds.Contains(ItemId) || !Equipment || !Equipment->FindItem(ItemId, Def))
		{
			return;
		}
		if (Inventory->IsFull())
		{
			ShowLocalNotice(NSLOCTEXT("DCC", "ShopBagFull", "Your bag is full. Sell something first."));
			return;
		}
		if (!Stats->SpendGold(Def.GoldValue))
		{
			return;
		}
		if (!Inventory->AddItemRecord(ADCCPickup_Equipment::StaticClass(), Def.DisplayName, Def.MassKilograms, ItemId))
		{
			Stats->AddGold(Def.GoldValue); // refund: the bag said no after all
			return;
		}
		Name = Def.DisplayName;
	}

	ADCCGameState::Log(this, EDCCLogCategory::Announcement, FText::Format(
		NSLOCTEXT("DCC", "LogShopBuy", "{0} buys {1} from {2}. No refunds. There were never refunds."),
		FText::FromString(GetActorNameOrLabel()), Name, Vendor->DisplayName));
	DCCSave::Save(this);
}

void ADCCCharacterBase::Server_ShopSell_Implementation(AActor* VendorActor, int32 BagIndex)
{
	const ADCCVendorNPC* Vendor = Cast<ADCCVendorNPC>(VendorActor);
	if (!Vendor || !Stats || !Inventory || !Inventory->Items.IsValidIndex(BagIndex) || !IsAlive() || IsDowned()
		|| FVector::Dist(GetActorLocation(), Vendor->GetActorLocation()) > InteractionRange * 1.5f)
	{
		return;
	}

	const int32 Price = GetShopSellPrice(BagIndex);
	const FText Name = Inventory->Items[BagIndex].DisplayName;
	Inventory->Items.RemoveAt(BagIndex);
	Inventory->OnInventoryChanged.Broadcast(Inventory);
	Stats->AddGold(Price);

	ADCCGameState::Log(this, EDCCLogCategory::Announcement, FText::Format(
		NSLOCTEXT("DCC", "LogShopSell", "{0} sells {1} to {2} for {3} gold. It will be on the shelf again by morning, at three times the price."),
		FText::FromString(GetActorNameOrLabel()), Name, Vendor->DisplayName, FText::AsNumber(Price)));
	DCCSave::Save(this);
}

void ADCCCharacterBase::Server_PurchaseEquipment_Implementation(AActor* VendorActor, FName ItemId)
{
	const ADCCVendorNPC* Vendor = Cast<ADCCVendorNPC>(VendorActor);
	if (!Vendor || !Vendor->SellableEquipmentIds.Contains(ItemId) || !IsAlive() || IsDowned())
	{
		return;
	}

	if (FVector::Dist(GetActorLocation(), Vendor->GetActorLocation()) > InteractionRange)
	{
		return;
	}

	if (!Equipment || !Stats)
	{
		return;
	}

	FDCCEquipmentDef Def;
	if (!Equipment->FindItem(ItemId, Def))
	{
		return;
	}

	if (!Stats->SpendGold(Def.GoldValue))
	{
		ADCCGameState::Log(this, EDCCLogCategory::Announcement, FText::Format(
			NSLOCTEXT("DCC", "LogPurchaseFailed", "{0} can't afford {1} ({2}g)."),
			FText::FromString(GetActorNameOrLabel()), Def.DisplayName, FText::AsNumber(Def.GoldValue)));
		return;
	}

	// Same grant path a picked-up ground item already uses (see
	// ADCCPickup::OnPickedUp) - buying is just skipping the trip to the floor.
	Equipment->Equip(ItemId);

	ADCCGameState::Log(this, EDCCLogCategory::Announcement, FText::Format(
		NSLOCTEXT("DCC", "LogPurchase", "{0} buys {1} from {2} for {3}g."),
		FText::FromString(GetActorNameOrLabel()), Def.DisplayName, Vendor->DisplayName, FText::AsNumber(Def.GoldValue)));
}

void ADCCCharacterBase::ApplyConsumableEffects(const TArray<FDCCConsumableEffect>& Effects)
{
	if (!Stats)
	{
		return;
	}

	// This is the whole translation layer between "a drink" (a DCC/Items
	// concept) and the Stats component's ruleset-agnostic primitives - see
	// FDCCConsumableEffect's own comment on why Stats itself never hears the
	// word "consumable".
	for (const FDCCConsumableEffect& Effect : Effects)
	{
		if (Effect.InstantHealthChange > 0)
		{
			Stats->Heal(Effect.InstantHealthChange);
		}
		else if (Effect.InstantHealthChange < 0)
		{
			Stats->ApplyDamage(-Effect.InstantHealthChange);
		}

		if (Effect.ModifierAmount != 0)
		{
			Stats->ApplyTimedAbilityModifier(Effect.ModifiedAbility, Effect.ModifierAmount, Effect.ModifierDurationSeconds);
		}
	}
}

void ADCCCharacterBase::Server_PurchaseDrink_Implementation(AActor* VendorActor, FName ConsumableItemId, bool bDrinkNow)
{
	const ADCCVendorNPC* Vendor = Cast<ADCCVendorNPC>(VendorActor);
	if (!Vendor || !Vendor->SellableConsumableIds.Contains(ConsumableItemId) || !IsAlive() || IsDowned() || !Stats || !Inventory)
	{
		return;
	}

	if (FVector::Dist(GetActorLocation(), Vendor->GetActorLocation()) > InteractionRange)
	{
		return;
	}

	FDCCConsumableDef Def;
	if (!Inventory->FindConsumable(ConsumableItemId, Def))
	{
		return;
	}

	if (!Stats->SpendGold(Def.GoldValue))
	{
		ADCCGameState::Log(this, EDCCLogCategory::Announcement, FText::Format(
			NSLOCTEXT("DCC", "LogDrinkFailed", "{0} can't afford {1} ({2}g)."),
			FText::FromString(GetActorNameOrLabel()), Def.DisplayName, FText::AsNumber(Def.GoldValue)));
		return;
	}

	// "House Pour" - a real purchase happened, regardless of whether it's drunk
	// now or stowed for later. Recorded here rather than in each branch below
	// so both count exactly once, not zero or twice.
	if (Achievements)
	{
		Achievements->Record(EDCCAchievementStat::FirstDrinkBought);
	}

	if (bDrinkNow)
	{
		ApplyConsumableEffects(Def.Effects);

		ADCCGameState::Log(this, EDCCLogCategory::Announcement, FText::Format(
			NSLOCTEXT("DCC", "LogDrinkBought", "{0} orders a {1} from {2} and drinks it right there. {3}g well spent."),
			FText::FromString(GetActorNameOrLabel()), Def.DisplayName, Vendor->DisplayName, FText::AsNumber(Def.GoldValue)));
	}
	else
	{
		Inventory->AddConsumable(ConsumableItemId, 1);

		ADCCGameState::Log(this, EDCCLogCategory::Announcement, FText::Format(
			NSLOCTEXT("DCC", "LogDrinkStowed", "{0} buys a {1} from {2} and pockets it for later. {3}g well spent."),
			FText::FromString(GetActorNameOrLabel()), Def.DisplayName, Vendor->DisplayName, FText::AsNumber(Def.GoldValue)));
	}
}

void ADCCCharacterBase::DCCConsumeStowedDrink(int32 Index)
{
	if (HasAuthority())
	{
		Server_ConsumeStowedDrink_Implementation(Index);
	}
	else
	{
		Server_ConsumeStowedDrink(Index);
	}
}

void ADCCCharacterBase::Server_ConsumeStowedDrink_Implementation(int32 Index)
{
	if (!Inventory || !IsAlive() || IsDowned())
	{
		return;
	}

	FDCCConsumableDef Def;
	if (!Inventory->ConsumeStowedConsumable(Index, Def))
	{
		return;
	}

	ApplyConsumableEffects(Def.Effects);

	ADCCGameState::Log(this, EDCCLogCategory::Announcement, FText::Format(
		NSLOCTEXT("DCC", "LogDrinkFromPack", "{0} finally gets around to that {1}."),
		FText::FromString(GetActorNameOrLabel()), Def.DisplayName));

	// A skilled item (a med kit and First Aid): an Int skill check; a success
	// adds HealPerSkillRank per Rank on top of the item's own effects.
	if (!Def.LinkedSkillId.IsNone() && Skills && Stats)
	{
		const int32 Rank = Skills->GetSkillRank(Def.LinkedSkillId);
		const int32 Total = RollD20ForSkill(Rank <= 0) + Rank + Stats->GetAbilityModifier(EDCCAbility::Intelligence);
		const int32 Bonus = Total >= 10 ? Rank * Def.HealPerSkillRank : 0;
		if (Bonus > 0)
		{
			Stats->Heal(Bonus);
		}
		NoteSkillUse(Def.LinkedSkillId, Rank);
		ADCCGameState::Log(this, EDCCLogCategory::Announcement, Bonus > 0
			? FText::Format(NSLOCTEXT("DCC", "LogSkilledHeal", "{0} knows what they are doing with it: +{1} more from {2}."),
				FText::FromString(GetActorNameOrLabel()), FText::AsNumber(Bonus), FText::FromName(Def.LinkedSkillId))
			: FText::Format(NSLOCTEXT("DCC", "LogUnskilledHeal", "{0} applies it with more confidence than technique. {1} would have helped."),
				FText::FromString(GetActorNameOrLabel()), FText::FromName(Def.LinkedSkillId)));
	}
}

void ADCCCharacterBase::OpenLootBox(EDCCLootBoxTier Tier)
{
	if (HasAuthority())
	{
		Server_OpenLootBox_Implementation(Tier);
	}
	else
	{
		Server_OpenLootBox(Tier);
	}
}

void ADCCCharacterBase::Server_OpenLootBox_Implementation(EDCCLootBoxTier Tier)
{
	// Re-verified here, not trusted from the client's own HUD state - the
	// exact reason every other server entry point in this project re-checks
	// its own gate rather than assuming a client only ever asks when allowed.
	// "Boxes only open in a Safe Room" is a rule about the character's real,
	// server-known location, so this is the one place it can actually be
	// enforced.
	if (!Inventory || !IsAlive() || IsDowned() || !IsInSafeRoom())
	{
		return;
	}

	FDCCOpenedLootBoxResult Result;
	if (!Inventory->TryOpenLootBox(Tier, GetActorLocation(), Result))
	{
		return;
	}

	// Boons apply the exact same way a drink's Effects do - see this
	// function's own translation layer just above. A box grants a blessing,
	// not a bottle, but Stats never needed to know the difference.
	for (const FDCCBoonDef& Boon : Result.Boons)
	{
		ApplyConsumableEffects(Boon.Effects);
	}

	const FText TierName = StaticEnum<EDCCLootBoxTier>()->GetDisplayNameTextByValue((int64)Tier);

	FText BoonSummary = FText::GetEmpty();
	if (!Result.Boons.IsEmpty())
	{
		TArray<FString> BoonNames;
		for (const FDCCBoonDef& Boon : Result.Boons)
		{
			BoonNames.Add(Boon.DisplayName.ToString());
		}
		BoonSummary = FText::Format(
			NSLOCTEXT("DCC", "LogLootBoxBoonSuffix", " Blessed with: {0}."),
			FText::FromString(FString::Join(BoonNames, TEXT(", "))));
	}

	ADCCGameState::Log(this, EDCCLogCategory::Achievement, FText::Format(
		NSLOCTEXT("DCC", "LogLootBoxOpened", "{0} cracks open a {1} box - {2} item(s) hit the floor.{3}"),
		GetLootSourceName(), TierName, FText::AsNumber(Result.Items.Num()), BoonSummary));
}

void ADCCCharacterBase::ClaimAndOpenAllLootBoxes()
{
	if (HasAuthority())
	{
		Server_ClaimAndOpenAllLootBoxes_Implementation();
	}
	else
	{
		Server_ClaimAndOpenAllLootBoxes();
	}
}

void ADCCCharacterBase::Server_ClaimAndOpenAllLootBoxes_Implementation()
{
	if (!IsAlive() || IsDowned())
	{
		return;
	}

	// Claiming pays XP/Favor and is unconditional, matching the existing
	// per-achievement Claim button exactly - only the box-opening half below
	// is a Safe Room privilege, so a call made in the field still banks
	// everything except the boxes themselves.
	if (Achievements)
	{
		TArray<FName> Pending;
		Achievements->GetPendingRewardIds(Pending);
		for (FName Id : Pending)
		{
			Achievements->RequestClaimReward(Id);
		}
	}

	if (!Inventory)
	{
		return;
	}

	if (!IsInSafeRoom())
	{
		ShowLocalNotice(NSLOCTEXT("DCC", "LootBoxesAwaitSafeRoom",
			"Rewards claimed. Sealed boxes can only be opened in a Safe Room."));
		return;
	}

	// Counted, not opened yet - TryOpenLootBox rolls and spawns the instant
	// it is called, and the whole point of this reveal (per Zoe's own
	// reference - the Avatar State's line of past lives, stretching away
	// from the one standing still in front of it) is to show every sealed
	// box at once, in a line, before anything is actually opened.
	TArray<EDCCLootBoxTier> BoxLineup;
	for (uint8 TierIndex = 1; TierIndex < (uint8)EDCCLootBoxTier::MAX; ++TierIndex)
	{
		const EDCCLootBoxTier Tier = (EDCCLootBoxTier)TierIndex;
		const int32 Count = Inventory->GetLootBoxCount(Tier);
		for (int32 Index = 0; Index < Count; ++Index)
		{
			BoxLineup.Add(Tier);
		}
	}

	if (BoxLineup.IsEmpty())
	{
		ShowLocalNotice(NSLOCTEXT("DCC", "NoLootBoxesToOpen", "Nothing sealed to open."));
		return;
	}

	// Rooted for the whole reveal, same shared lock BeginReviveChannel uses -
	// "stuck in place" is the point, not a side effect. Scales with how many
	// boxes there are to look at, clamped so five boxes is not a blink and
	// fifty is not a coffee break.
	const float RevealSeconds = FMath::Clamp(0.6f * BoxLineup.Num(), 2.0f, 8.0f);
	BeginCastLock(RevealSeconds);

	// Receding directly away from the crawler, not sideways - the reference
	// image's whole composition is a line running INTO the distance in front
	// of whoever is standing still for it, not a row beside them.
	const FVector Forward = GetActorForwardVector();
	const FVector LineStart = GetActorLocation() + Forward * 200.0f;
	constexpr float StepDistance = 140.0f;

	for (AActor* OldProp : LootBoxRevealProps)
	{
		if (IsValid(OldProp))
		{
			OldProp->Destroy();
		}
	}
	LootBoxRevealProps.Reset();

	FActorSpawnParameters SpawnParams;
	SpawnParams.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;

	for (int32 Index = 0; Index < BoxLineup.Num(); ++Index)
	{
		const FVector SpawnLocation = LineStart + Forward * (StepDistance * Index);

		if (ADCCLootBoxStation* Prop = GetWorld()->SpawnActor<ADCCLootBoxStation>(
			ADCCLootBoxStation::StaticClass(), SpawnLocation, GetActorRotation(), SpawnParams))
		{
			// A sealed prop, not a real station - it never registers a real
			// tier count and TryPlayerInteract is never called on it, so
			// there is nothing here for a passing ally to accidentally
			// "open" early. Just the right colour standing in the line.
			Prop->Tier = BoxLineup[Index];
			LootBoxRevealProps.Add(Prop);
		}
	}

	TWeakObjectPtr<ADCCCharacterBase> WeakSelf(this);
	const int32 BoxCount = BoxLineup.Num();

	FTimerDelegate Finish;
	Finish.BindLambda([WeakSelf, BoxLineup = MoveTemp(BoxLineup), LineStart, Forward, BoxCount]()
	{
		if (!WeakSelf.IsValid())
		{
			return;
		}

		ADCCCharacterBase* Self = WeakSelf.Get();

		for (AActor* Prop : Self->LootBoxRevealProps)
		{
			if (IsValid(Prop))
			{
				Prop->Destroy();
			}
		}
		Self->LootBoxRevealProps.Reset();

		if (!Self->Inventory)
		{
			return;
		}

		int32 Opened = 0;
		for (int32 Index = 0; Index < BoxLineup.Num(); ++Index)
		{
			const FVector SpawnLocation = LineStart + Forward * (140.0f * Index);

			FDCCOpenedLootBoxResult Result;
			if (Self->Inventory->TryOpenLootBox(BoxLineup[Index], SpawnLocation, Result))
			{
				for (const FDCCBoonDef& Boon : Result.Boons)
				{
					Self->ApplyConsumableEffects(Boon.Effects);
				}
				++Opened;
			}
		}

		if (Opened > 0)
		{
			ADCCGameState::Log(Self, EDCCLogCategory::Achievement, FText::Format(
				NSLOCTEXT("DCC", "LogLootBoxOpenedAll", "{0} claims everything owed - {1} box(es) cracked open in one go."),
				Self->GetLootSourceName(), FText::AsNumber(Opened)));
		}
	});

	GetWorldTimerManager().SetTimer(LootBoxRevealTimer, Finish, RevealSeconds, false);

	ADCCGameState::Log(this, EDCCLogCategory::Announcement, FText::Format(
		NSLOCTEXT("DCC", "LogLootBoxRevealStart", "{0} lines up {1} sealed box(es). The line stretches out ahead of them."),
		GetLootSourceName(), FText::AsNumber(BoxCount)));
}

// ---------------------------------------------------------------------------
// Weapon skills and explosives (2026-11-13), per the DCC books
// ---------------------------------------------------------------------------

namespace
{
	/** d20, or the lower of two when untrained (the books' Disadvantage). */
	int32 RollD20(bool bDisadvantage)
	{
		const int32 First = FMath::RandRange(1, 20);
		return bDisadvantage ? FMath::Min(First, FMath::RandRange(1, 20)) : First;
	}

	FText ItemName(const FDCCEquipmentDef& Def)
	{
		return Def.DisplayName.IsEmpty() ? FText::FromName(Def.ItemId) : Def.DisplayName;
	}
}

bool ADCCCharacterBase::GetMainHandDef(FDCCEquipmentDef& Out) const
{
	if (!Equipment)
	{
		return false;
	}
	const FName Held = Equipment->GetEquippedId(EDCCEquipmentSlot::MainHand);
	return !Held.IsNone() && Equipment->FindItem(Held, Out);
}

void ADCCCharacterBase::NoteSkillUse(FName SkillId, int32 Rank)
{
	if (SkillId.IsNone() || !Skills)
	{
		return;
	}
	// Known skills get their advancement mark; unknown ones count towards
	// learning them, which is how the tutorial floors hand out new skills.
	if (Rank > 0)
	{
		Skills->MarkSkillUsed(SkillId);
	}
	else
	{
		Skills->RecordUntrainedAttempt(SkillId);
	}
}

FDCCAttackResult ADCCCharacterBase::ThrowExplosive(ADCCCharacterBase* Target, const FDCCEquipmentDef& Def)
{
	FDCCAttackResult Result;
	UDCCRuleset* Ruleset = Stats ? Stats->GetRuleset() : nullptr;
	if (!Ruleset || !Target || !Target->Stats)
	{
		return Result;
	}

	const FText Who = FText::FromString(GetActorNameOrLabel());
	const FText What = ItemName(Def);

	// 1. Handling. Before it leaves your hand, it has to not go off in it.
	const int32 HandleRank = (!Def.HandlingSkillId.IsNone() && Skills) ? Skills->GetSkillRank(Def.HandlingSkillId) : 0;
	const bool bUntrainedHandling = !Def.HandlingSkillId.IsNone() && HandleRank <= 0;
	const int32 Natural = RollD20(bUntrainedHandling);
	int32 FumbleAt = Def.FumbleOnNatural;
	if (Def.HandlingSkillId == TEXT("GoblinExplosives") && HandleRank >= 5)
	{
		FumbleAt = FMath::Min(FumbleAt, 3);
	}
	NoteSkillUse(Def.HandlingSkillId, HandleRank);

	const int32 Die = FMath::Max(4, Def.WeaponDamageDie);
	const int32 BaseDamage = FMath::RandRange(1, Die) + FMath::RandRange(1, Die) + HandleRank / 2;

	if (Natural <= FumbleAt)
	{
		Result.Check.Roll = Natural;
		Result.Check.bCriticalFailure = true;
		ADCCGameState::Log(this, EDCCLogCategory::Combat, FText::Format(
			NSLOCTEXT("DCC", "LogExplosiveFumble", "{0} fumbles the {1} (natural {2}). It goes off exactly where they are standing. Chat has clipped it already."),
			Who, What, FText::AsNumber(Natural)));
		Detonate(GetActorLocation(), Def, BaseDamage);
		Multicast_PlayAttack(Target, false);
		return Result;
	}

	const int32 HandleTotal = Natural + HandleRank + Stats->GetAbilityModifier(EDCCAbility::Intelligence);
	if (HandleTotal < 8)
	{
		Result.Check.Roll = Natural;
		ADCCGameState::Log(this, EDCCLogCategory::Combat, FText::Format(
			NSLOCTEXT("DCC", "LogExplosiveDud", "{0}'s {1} is a dud. It lands with a sad little clunk. The audience boos the fuse."),
			Who, What));
		Multicast_PlayAttack(Target, false);
		return Result;
	}

	// 2. The throw: an Attack Skill Check with Throwing (Str) against Evade.
	const FName ThrowSkill = Def.WeaponSkillId.IsNone() ? FName(TEXT("Throwing")) : Def.WeaponSkillId;
	const int32 ThrowRank = Skills ? Skills->GetSkillRank(ThrowSkill) : 0;
	Result = Ruleset->ResolveAttack(Stats->GetEnhanced(), EDCCAbility::Strength, Die,
		Target->Stats->GetEnhanced(), Target->Stats->Level, ThrowRank,
		Target->Equipment ? Target->Equipment->GetEvadeBonus() : 0);
	if (ThrowRank <= 0)
	{
		const FDCCAttackResult Second = Ruleset->ResolveAttack(Stats->GetEnhanced(), EDCCAbility::Strength, Die,
			Target->Stats->GetEnhanced(), Target->Stats->Level, 0,
			Target->Equipment ? Target->Equipment->GetEvadeBonus() : 0);
		if (Second.Check.Total < Result.Check.Total)
		{
			Result = Second;
		}
	}
	NoteSkillUse(ThrowSkill, ThrowRank);

	// 3. Where it lands. A miss lands as many feet off as it missed by, in a
	// random direction (the books' scatter rule), so near misses still hurt.
	FVector Landing = Target->GetActorLocation();
	int32 MissedByFeet = 0;
	if (!Result.bHit)
	{
		MissedByFeet = FMath::Max(1, Result.Check.DifficultyClass - Result.Check.Total);
		const float Angle = FMath::FRandRange(0.0f, 2.0f * PI);
		Landing += FVector(FMath::Cos(Angle), FMath::Sin(Angle), 0.0f) * (MissedByFeet * 30.48f);
	}

	ADCCGameState::Log(this, EDCCLogCategory::Combat, Result.bHit
		? FText::Format(NSLOCTEXT("DCC", "LogExplosiveHit", "{0} lobs the {1} right onto {2}."), Who, What, FText::FromString(Target->GetActorNameOrLabel()))
		: FText::Format(NSLOCTEXT("DCC", "LogExplosiveMiss", "{0} lobs the {1}. It lands {2} feet wide."), Who, What, FText::AsNumber(MissedByFeet)));

	Result.Damage = BaseDamage;
	Detonate(Landing, Def, BaseDamage);
	Multicast_PlayAttack(Target, Result.bHit);
	return Result;
}

void ADCCCharacterBase::Detonate(const FVector& Where, const FDCCEquipmentDef& Def, int32 Damage)
{
	UWorld* World = GetWorld();
	if (!World || !HasAuthority())
	{
		return;
	}

	const float Radius = FMath::Max(100.0f, Def.SplashRadius);
	TArray<ADCCCharacterBase*> Victims;
	for (TActorIterator<ADCCCharacterBase> It(World); It; ++It)
	{
		ADCCCharacterBase* Victim = *It;
		if (Victim && Victim->IsAlive() && Victim->Stats && FVector::Dist(Victim->GetActorLocation(), Where) <= Radius)
		{
			Victims.Add(Victim);
		}
	}

	// Everyone in the blast, friends and the thrower included. Explosives are
	// not a precision instrument and the audience would not have it otherwise.
	int32 Total = 0;
	for (ADCCCharacterBase* Victim : Victims)
	{
		const float Falloff = 1.0f - 0.5f * FMath::Clamp(FVector::Dist(Victim->GetActorLocation(), Where) / Radius, 0.0f, 1.0f);
		const int32 Dealt = FMath::Max(1, FMath::RoundToInt(Damage * Falloff));
		Victim->Stats->ApplyDamage(Dealt, Def.WeaponDamageType);
		NoteDamageDealtTo(Victim, Dealt);
		Victim->ReactToHitFrom(Where);
		Total += Dealt;
		if (!Victim->Stats->IsAlive() && Victim != this && IsHostileTo(Victim))
		{
			AwardKillExperience(Victim);
		}
	}
	UDCCAchievementComponent::RecordFor(this, EDCCAchievementStat::DamageDealt, Total);

	Multicast_Explosion(Where, Radius, Def.WeaponDamageType == EDCCDamageType::Fire);
}

void ADCCCharacterBase::Multicast_Explosion_Implementation(FVector Where, float Radius, bool bFire)
{
	UWorld* World = GetWorld();
	if (!World || GetNetMode() == NM_DedicatedServer)
	{
		return;
	}

	static const TCHAR* Sparks = TEXT("/Game/Legendary_Forge/Fx/Sparks/NS_sparks.NS_sparks");
	static const TCHAR* Smoke = TEXT("/Game/Legendary_Forge/Fx/Smoke/NS_smoke.NS_smoke");
	static const TCHAR* Fire = TEXT("/Game/Legendary_Forge/Fx/Fire/NS_fire.NS_fire");
	const float Scale = FMath::Clamp(Radius / 250.0f, 0.6f, 3.0f);
	for (const TCHAR* Path : { Sparks, Smoke, bFire ? Fire : nullptr })
	{
		if (!Path)
		{
			continue;
		}
		if (UNiagaraSystem* System = LoadObject<UNiagaraSystem>(nullptr, Path))
		{
			UNiagaraFunctionLibrary::SpawnSystemAtLocation(World, System, Where, FRotator::ZeroRotator, FVector(Scale));
		}
	}

	// A hard, short flash: reads as a bang even where the particles are thin.
	FActorSpawnParameters Params;
	Params.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
	if (APointLight* Flash = World->SpawnActor<APointLight>(Where + FVector(0.0f, 0.0f, 80.0f), FRotator::ZeroRotator, Params))
	{
		Flash->SetReplicates(false);
		if (UPointLightComponent* Light = Cast<UPointLightComponent>(Flash->GetLightComponent()))
		{
			Light->SetMobility(EComponentMobility::Movable);
			Light->SetIntensity(60000.0f);
			Light->SetAttenuationRadius(Radius * 3.0f);
			Light->SetLightColor(bFire ? FLinearColor(1.0f, 0.45f, 0.15f) : FLinearColor(1.0f, 0.8f, 0.55f));
			Light->SetCastShadows(false);
		}
		Flash->SetLifeSpan(0.18f);
	}

	// Shake whoever is close enough to feel it.
	if (APlayerController* PC = World->GetFirstPlayerController())
	{
		const APawn* Viewer = PC->GetPawn();
		if (Viewer && FVector::Dist(Viewer->GetActorLocation(), Where) < Radius * 4.0f && HurtCameraShake)
		{
			PC->ClientStartCameraShake(HurtCameraShake, 1.5f);
		}
	}
}

// ---------------------------------------------------------------------------
// Loot box screen (2026-11-12): open one box or a batch, reveal the contents
// ---------------------------------------------------------------------------

bool ADCCCharacterBase::CanOpenLootBoxesHere(FText& OutWhyNot) const
{
	if (!IsAlive() || IsDowned())
	{
		OutWhyNot = NSLOCTEXT("DCC", "LootBoxDown", "Sponsors do not deliver to the deceased.");
		return false;
	}
	if (IsInSafeRoom())
	{
		return true;
	}

	// The books keep boxes to Safe Rooms. The World Dungeon has none, so a
	// cleared room counts: nothing hostile within shouting distance.
	constexpr float ThreatRadius = 2500.0f;
	for (TActorIterator<ADCCCharacterBase> It(GetWorld()); It; ++It)
	{
		const ADCCCharacterBase* Other = *It;
		if (Other && Other != this && Other->IsAlive() && IsHostileTo(Other)
			&& FVector::DistSquared(Other->GetActorLocation(), GetActorLocation()) < FMath::Square(ThreatRadius))
		{
			OutWhyNot = NSLOCTEXT("DCC", "LootBoxHot",
				"Boxes do not open while something nearby wants you dead. Clear the room, or find a Safe Room.");
			return false;
		}
	}
	return true;
}

void ADCCCharacterBase::RequestOpenLootBoxes(EDCCLootBoxTier Tier, int32 Count)
{
	if (HasAuthority())
	{
		Server_OpenLootBoxBatch_Implementation(Tier, Count);
	}
	else
	{
		Server_OpenLootBoxBatch(Tier, Count);
	}
}

void ADCCCharacterBase::Server_OpenLootBoxBatch_Implementation(EDCCLootBoxTier Tier, int32 Count)
{
	FDCCLootBoxReveal Reveal;
	Reveal.Tier = Tier;

	FText WhyNot;
	if (!Inventory || !CanOpenLootBoxesHere(WhyNot))
	{
		Reveal.Message = WhyNot;
		Client_LootBoxReveal(Reveal);
		return;
	}

	// Which boxes, in order. None means every box owned, best last so the
	// batch ends on its biggest moment.
	TArray<EDCCLootBoxTier> Boxes;
	if (Tier == EDCCLootBoxTier::None)
	{
		for (uint8 TierIndex = 1; TierIndex < (uint8)EDCCLootBoxTier::MAX; ++TierIndex)
		{
			for (int32 i = 0; i < Inventory->GetLootBoxCount((EDCCLootBoxTier)TierIndex); ++i)
			{
				Boxes.Add((EDCCLootBoxTier)TierIndex);
			}
		}
	}
	else
	{
		for (int32 i = 0; i < FMath::Min(Count, Inventory->GetLootBoxCount(Tier)); ++i)
		{
			Boxes.Add(Tier);
		}
	}

	// A sanity cap on one request, not a gameplay rule.
	if (Boxes.Num() > 60)
	{
		Boxes.SetNum(60);
	}

	for (const EDCCLootBoxTier BoxTier : Boxes)
	{
		FDCCOpenedLootBoxResult Result;
		if (!Inventory->TryOpenLootBox(BoxTier, GetActorLocation(), Result, this))
		{
			continue;
		}

		++Reveal.BoxCount;
		Reveal.Tier = FMath::Max(Reveal.Tier, BoxTier);

		for (const FDCCRolledLoot& Rolled : Result.Items)
		{
			FDCCLootRevealEntry Entry;
			if (!Rolled.EquipmentItemId.IsNone())
			{
				Entry.Kind = EDCCLootRevealKind::Item;
				Entry.ItemId = Rolled.EquipmentItemId;
			}
			else if (Rolled.Gold > 0)
			{
				Entry.Kind = EDCCLootRevealKind::Gold;
				Entry.Amount = Rolled.Gold;
			}
			else
			{
				Entry.Kind = EDCCLootRevealKind::Item;
				const ADCCPickup* Defaults = Rolled.PickupClass ? Rolled.PickupClass.GetDefaultObject() : nullptr;
				Entry.Name = Defaults && !Defaults->DisplayName.IsEmpty()
					? Defaults->DisplayName
					: FText::FromString(GetNameSafe(Rolled.PickupClass.Get()));
				Entry.Detail = NSLOCTEXT("DCC", "LootRevealOnFloor", "Too big for the bag. It is at your feet.");
			}
			Reveal.Entries.Add(Entry);
		}

		for (const FDCCBoonDef& Boon : Result.Boons)
		{
			ApplyConsumableEffects(Boon.Effects);

			FDCCLootRevealEntry Entry;
			Entry.Kind = EDCCLootRevealKind::Boon;
			Entry.Name = Boon.DisplayName;
			Entry.Detail = Boon.Description;
			Reveal.Entries.Add(Entry);
		}
	}

	if (Reveal.BoxCount == 0)
	{
		Reveal.Message = NSLOCTEXT("DCC", "LootBoxNone", "Nothing sealed to open.");
	}
	else
	{
		ADCCGameState::Log(this, EDCCLogCategory::Achievement, FText::Format(
			NSLOCTEXT("DCC", "LogLootBoxBatch", "{0} opens {1} {1}|plural(one=box,other=boxes) on camera. {2} {2}|plural(one=thing,other=things) inside."),
			GetLootSourceName(), FText::AsNumber(Reveal.BoxCount), FText::AsNumber(Reveal.Entries.Num())));
	}

	Client_LootBoxReveal(Reveal);
}

void ADCCCharacterBase::Client_LootBoxReveal_Implementation(const FDCCLootBoxReveal& Reveal)
{
	const APlayerController* PC = Cast<APlayerController>(GetController());
	if (ADCCHUD* DCCHUD = PC ? Cast<ADCCHUD>(PC->GetHUD()) : nullptr)
	{
		DCCHUD->ReceiveLootBoxReveal(Reveal);
	}
}

void ADCCCharacterBase::ToggleRecording()
{
	if (HasAuthority())
	{
		Server_SetRecording_Implementation(!bRecordingFootage);
	}
	else
	{
		Server_SetRecording(!bRecordingFootage);
	}
}

void ADCCCharacterBase::Server_SetRecording_Implementation(bool bRecording)
{
	bRecordingFootage = bRecording && IsAlive() && !IsDowned();
}

void ADCCCharacterBase::ToggleGearBench()
{
	const APlayerController* PC = Cast<APlayerController>(GetController());
	if (ADCCHUD* DCCHUD = PC ? Cast<ADCCHUD>(PC->GetHUD()) : nullptr)
	{
		DCCHUD->ToggleGearBench();
	}
}

void ADCCCharacterBase::ToggleLootBoxScreen()
{
	const APlayerController* PC = Cast<APlayerController>(GetController());
	if (ADCCHUD* DCCHUD = PC ? Cast<ADCCHUD>(PC->GetHUD()) : nullptr)
	{
		DCCHUD->ToggleLootBoxScreen();
	}
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
	bEmoteMenuOpen = false;

	// Esc is bound to this function alone, so it has to close whichever of the
	// menus is actually open - harmless to call when a menu was already
	// closed, CloseLootMenu/CloseVendorMenu just reset fields that are reset
	// already.
	CloseLootMenu();
	CloseVendorMenu();
	CloseOathgatePrompt();
	CloseNPCInteraction();
}

void ADCCCharacterBase::ResolveSocialCheck(ADCCCharacterBase* Target, bool bIntimidate)
{
	UDCCRuleset* Ruleset = Stats ? Stats->GetRuleset() : nullptr;
	if (!Ruleset || !Target)
	{
		return;
	}

	const int32 Difficulty = bIntimidate ? IntimidateDifficulty : PersuadeDifficulty;

	// Routed through the Skill system (2026-08-23) rather than a flat CHA
	// check, so Rank - and any Race/Class benefit granting Advantage - actually
	// do something. FirstImpressions/QuickWit are this project's own existing
	// ids for Intimidate/Persuade (below still marks whichever one fired, so
	// there is exactly one mark call for this function, not two).
	static const FName IntimidateSkillId(TEXT("FirstImpressions"));
	static const FName PersuadeSkillId(TEXT("QuickWit"));
	const FName SkillId = bIntimidate ? IntimidateSkillId : PersuadeSkillId;

	const bool bKnown = Skills && Skills->IsSkillKnown(SkillId);
	const int32 SkillRank = Skills ? Skills->GetSkillRank(SkillId) : 0;

	// Charisma, per the published social skills. The roll is real and the result
	// is reported in full - the narrator layer describes outcomes, it never
	// decides them.
	const FDCCCheckResult Check = Ruleset->ResolveSkillCheck(
		Stats->GetEnhanced(), EDCCAbility::Charisma, SkillRank, Difficulty, /*bUntrained=*/!bKnown);

	if (Skills)
	{
		if (bKnown)
		{
			Skills->MarkSkillUsed(SkillId);
		}
		else
		{
			Skills->RecordUntrainedAttempt(SkillId);
		}
	}

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

		ADCCGameState::LogRoll(this, EDCCLogCategory::Skill, FText::Format(
			bIntimidate
				? NSLOCTEXT("DCC", "LogIntimidateWin", "{0} leans on {1}. ({2} vs {3}) They decide agreeing is cheaper.")
				: NSLOCTEXT("DCC", "LogPersuadeWin", "{0} talks {1} round. ({2} vs {3})"),
			SelfName, TargetName,
			FText::AsNumber(Check.Total), FText::AsNumber(Check.DifficultyClass)),
			this, Target, Check.bCriticalSuccess, Check.bCriticalFailure, EDCCAbility::Charisma, SkillId);
	}
	else
	{
		ADCCGameState::LogRoll(this, EDCCLogCategory::Skill, FText::Format(
			bIntimidate
				? NSLOCTEXT("DCC", "LogIntimidateFail", "{0} tries to lean on {1} and it lands badly. ({2} vs {3})")
				: NSLOCTEXT("DCC", "LogPersuadeFail", "{0} fails to convince {1}. ({2} vs {3})"),
			SelfName, TargetName,
			FText::AsNumber(Check.Total), FText::AsNumber(Check.DifficultyClass)),
			this, Target, Check.bCriticalSuccess, Check.bCriticalFailure, EDCCAbility::Charisma, SkillId);
	}

	// Intimidation is content whichever way it goes, which is exactly the sort of
	// thing the producer rewards. Persuasion is merely competent, so it pays nothing.
	if (bIntimidate && Favor)
	{
		Favor->AwardFavor(Check.bSuccess ? 2 : 1, EDCCFavorEvent::DirectorCall,
			NSLOCTEXT("DCC", "FavorIntimidate", "A raised voice in a quiet room."));
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

void ADCCCharacterBase::ToggleSessionMenu()
{
	const APlayerController* PC = Cast<APlayerController>(GetController());
	if (ADCCHUD* DCCHUD = PC ? Cast<ADCCHUD>(PC->GetHUD()) : nullptr)
	{
		DCCHUD->ToggleSessionMenu();
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

void ADCCCharacterBase::Server_AdminGauntlet_Implementation(bool bReset)
{
	ADCCGauntletEncounter* Gauntlet = ADCCGauntletEncounter::FindNearest(this, GetActorLocation());
	if (!Gauntlet)
	{
		ShowLocalNotice(NSLOCTEXT("DCC", "AdmNoGauntlet", "No gauntlet placed in this level."));
		return;
	}

	if (bReset)
	{
		Gauntlet->ResetGauntlet();
	}
	else
	{
		Gauntlet->StartGauntlet();
	}
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

void ADCCCharacterBase::CycleFeedVisibility()
{
	const APlayerController* PC = Cast<APlayerController>(GetController());
	if (ADCCHUD* DCCHUD = PC ? Cast<ADCCHUD>(PC->GetHUD()) : nullptr)
	{
		DCCHUD->CycleFeedVisibility();
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

	// A slow item (an explosive) can ask for a longer breather than the wielder's own.
	float Cooldown = AttackCooldownSeconds;
	FDCCEquipmentDef Weapon;
	if (GetMainHandDef(Weapon))
	{
		Cooldown = FMath::Max(Cooldown, Weapon.AttackCooldownOverride);
	}
	return (World->GetTimeSeconds() - LastAttackTime) >= Cooldown;
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

int32 ADCCCharacterBase::GetEffectiveWeaponDamageDie() const
{
	// Equipment wins when it says anything, same rule as GetEffectiveAttackRange -
	// see that function's own comment.
	if (Equipment)
	{
		const int32 Override = Equipment->GetWeaponDamageDie();
		if (Override > 0)
		{
			return Override;
		}
	}

	return WeaponDamageDie;
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

	// A real swing is happening, hit or miss - broadcast before resolving it
	// so a passive guard (ADCCEnemyCharacter::bPassiveUntilProvoked) can
	// notice violence the same frame it starts, not one frame late.
	OnAnyAttackAttempted.Broadcast(this, Target);

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

	// Explosives take their own path: handle, throw, scatter, blast.
	FDCCEquipmentDef Weapon;
	const bool bHasWeapon = GetMainHandDef(Weapon);
	if (bHasWeapon && Weapon.SplashRadius > 0.0f)
	{
		return ThrowExplosive(Target, Weapon);
	}

	// The weapon names its Attack Skill (2026-11-13). Rank adds to the roll;
	// untrained is Disadvantage (roll twice, keep the worse) and counts
	// towards learning it.
	const FName WeaponSkill = bHasWeapon ? Weapon.WeaponSkillId : NAME_None;
	const int32 WeaponRank = (!WeaponSkill.IsNone() && Skills) ? Skills->GetSkillRank(WeaponSkill) : 0;

	Result = Ruleset->ResolveAttack(
		Stats->GetEnhanced(),
		AttackAbility,
		GetEffectiveWeaponDamageDie(),
		TargetStats->GetEnhanced(),
		TargetStats->Level,
		/*SituationalBonus=*/WeaponRank,
		// A shield (or anything else with EvadeBonus) makes the TARGET harder
		// to hit - this was previously summed by GetEvadeBonus() and never
		// read anywhere, so equipping one did nothing to combat outcomes.
		Target->Equipment ? Target->Equipment->GetEvadeBonus() : 0);

	if (!WeaponSkill.IsNone())
	{
		if (WeaponRank <= 0)
		{
			const FDCCAttackResult Second = Ruleset->ResolveAttack(Stats->GetEnhanced(), AttackAbility,
				GetEffectiveWeaponDamageDie(), TargetStats->GetEnhanced(), TargetStats->Level, 0,
				Target->Equipment ? Target->Equipment->GetEvadeBonus() : 0);
			if (Second.Check.Total < Result.Check.Total)
			{
				Result = Second;
			}
		}
		// Rank 5 Upgrade on every weapon skill in the books: +1 base damage die.
		if (Result.bHit && WeaponRank >= 5 && Weapon.WeaponDamageDie > 0)
		{
			Result.Damage += FMath::RandRange(1, Weapon.WeaponDamageDie);
		}
		NoteSkillUse(WeaponSkill, WeaponRank);
	}

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

		// Physical (the default) for an unarmed swing; whatever the equipped
		// weapon authors otherwise. Read fresh here rather than cached
		// alongside WeaponDamageDie, since a damage type changes only as
		// often as gear does and isn't worth a second member field to track.
		const EDCCDamageType AttackDamageType = Equipment ? Equipment->GetWeaponDamageType() : EDCCDamageType::Physical;

		// The target's own last word on the blow. Anything that wants to be
		// unhittable-but-not-invulnerable overrides this - see
		// ADCCCharacterBase::ModifyIncomingDamage.
		FinalDamage = Target->ModifyIncomingDamage(FinalDamage, this);
		if (FinalDamage <= 0)
		{
			// Still a hit - it connected, it just did nothing. Reporting zero
			// damage rather than a miss keeps the result honest for anything
			// reading it (the Feed, achievements, the skill-mark system), and
			// the stagger is what tells the player the swing landed at all.
			Result.Damage = 0;
			Target->ReactToHitFrom(GetActorLocation());
			return Result;
		}

		TargetStats->ApplyDamage(FinalDamage, AttackDamageType);
		NoteDamageDealtTo(Target, FinalDamage);
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

		ADCCGameState::LogRoll(this, EDCCLogCategory::Combat, Line, this, Target,
			Result.Check.bCriticalSuccess, Result.Check.bCriticalFailure, AttackAbility);
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
	PlayerInputComponent->BindKey(EKeys::Q, IE_Pressed, this, &ADCCCharacterBase::UseClimbTool);
	PlayerInputComponent->BindKey(EKeys::T, IE_Pressed, this, &ADCCCharacterBase::NextClimbTool);
	PlayerInputComponent->BindKey(EKeys::C, IE_Pressed, this, &ADCCCharacterBase::ToggleSelfieMode);
	PlayerInputComponent->BindKey(EKeys::Tab, IE_Pressed, this, &ADCCCharacterBase::ToggleCharacterMenu);
	PlayerInputComponent->BindKey(EKeys::L, IE_Pressed, this, &ADCCCharacterBase::CycleLogFilter);
	PlayerInputComponent->BindKey(EKeys::K, IE_Pressed, this, &ADCCCharacterBase::CycleFeedVisibility);
	PlayerInputComponent->BindKey(EKeys::M, IE_Pressed, this, &ADCCCharacterBase::ToggleMount);

	// B for the emote picker - free, and reads fine either as "bust a move" or
	// as the game's own stand-in for whatever mnemonic sticks.
	PlayerInputComponent->BindKey(EKeys::B, IE_Pressed, this, &ADCCCharacterBase::ToggleEmoteMenu);

	// F1 for the DM panel. A function key rather than a letter because it is not
	// a game action - it should not compete with anything a player might press
	// mid-fight, and it stays out of the way of the movement hand entirely.
	PlayerInputComponent->BindKey(EKeys::F1, IE_Pressed, this, &ADCCCharacterBase::ToggleAdminPanel);

	// F2 for the party menu: host, invite, join, who's here.
	PlayerInputComponent->BindKey(EKeys::F2, IE_Pressed, this, &ADCCCharacterBase::ToggleSessionMenu);

	// O for the loot box screen: every sealed box in a row, hold to open.
	PlayerInputComponent->BindKey(EKeys::O, IE_Pressed, this, &ADCCCharacterBase::ToggleLootBoxScreen);

	// F3 for the gear bench: nudge grips, sockets and bones while an animation plays.
	PlayerInputComponent->BindKey(EKeys::F3, IE_Pressed, this, &ADCCCharacterBase::ToggleGearBench);

	// V for the body cam (Dead Air): start or stop recording.
	PlayerInputComponent->BindKey(EKeys::V, IE_Pressed, this, &ADCCCharacterBase::ToggleRecording);

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

void ADCCCharacterBase::AttemptRangedAttack()
{
	// Traced locally before the RPC, not after - this client's camera is the
	// only one that can answer "what am I looking at", same reasoning as
	// ResolveOrQueueSkill's target parameter.
	ADCCCharacterBase* NominatedTarget = FindHostileUnderCrosshair(GetEffectiveAttackRange());

	if (HasAuthority())
	{
		ResolveOrQueueRangedAttack(NominatedTarget);
		return;
	}

	Server_RangedAttack(NominatedTarget);
}

ADCCCharacterBase* ADCCCharacterBase::FindHostileUnderCrosshair(float MaxRange) const
{
	const APlayerController* PC = Cast<APlayerController>(GetController());
	UWorld* World = GetWorld();
	if (!PC || !World || MaxRange <= 0.0f)
	{
		return nullptr;
	}

	FVector CameraLocation;
	FRotator CameraRotation;
	PC->GetPlayerViewPoint(CameraLocation, CameraRotation);

	FCollisionQueryParams Params;
	Params.AddIgnoredActor(this);

	// A narrow sweep rather than a bare line trace - forgiving enough that
	// "roughly on target" still counts, same spirit as the interaction sweep's
	// generous sphere, just tighter since this is aimed fire, not a fist bump.
	FHitResult Hit;
	if (!World->SweepSingleByChannel(
		Hit, CameraLocation, CameraLocation + CameraRotation.Vector() * MaxRange,
		FQuat::Identity, ECC_Visibility, FCollisionShape::MakeSphere(24.0f), Params))
	{
		return nullptr;
	}

	ADCCCharacterBase* Target = Cast<ADCCCharacterBase>(Hit.GetActor());
	if (!Target || !Target->IsAlive() || !IsHostileTo(Target))
	{
		return nullptr;
	}

	return Target;
}

void ADCCCharacterBase::Server_RangedAttack_Implementation(ADCCCharacterBase* NominatedTarget)
{
	ResolveOrQueueRangedAttack(NominatedTarget);
}

void ADCCCharacterBase::ResolveOrQueueRangedAttack(ADCCCharacterBase* NominatedTarget)
{
	// The nomination is a claim, not a fact - re-checked here exactly like
	// ResolveOrQueueSkill re-checks CanActivate before trusting its own Target
	// parameter. A target that fails this still reaches PerformAttack below as
	// nullptr rather than being treated as an error - a shot fired at nothing
	// is not a refused action, it is a miss, same as swinging into empty air.
	ADCCCharacterBase* Target =
		(NominatedTarget && NominatedTarget->IsAlive() && IsHostileTo(NominatedTarget) && IsInAttackRange(NominatedTarget))
		? NominatedTarget : nullptr;

	const UWorld* World = GetWorld();
	if (ADCCGameState* GameState = World ? World->GetGameState<ADCCGameState>() : nullptr)
	{
		if (GameState->IsCombatPaused())
		{
			GameState->QueueAction(this, NAME_None, Target);
			return;
		}
	}

	PerformAttack(Target);
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

	// A character can stand on ITS OWN skeleton (e.g. `SK_ArmsOut_Fixed_Skeleton`,
	// bone-fitted to that body's real proportions - see DISPATCH.md's rig-fit
	// entries) and still play the shared `DCCCrawlerAnimSet` clips authored
	// against `SK_DCCCrawler_Skeleton`, PROVIDED the two skeletons have been
	// marked Compatible with each other (Skeleton asset's `CompatibleSkeletons`
	// array - writable straight through `ObjectTools.set_properties`, no
	// Skeleton Editor click-through required).
	//
	// Checked directly against the raw `CompatibleSkeletons` array rather than
	// `USkeleton::IsCompatibleForEditor` - that helper is `WITH_EDITORONLY_DATA`-
	// gated (confirmed by reading Skeleton.cpp) and would silently stop compiling
	// the moment this project builds a real dedicated server target. The array
	// itself carries no such guard, so this works identically in the editor, in
	// PIE, and in a future server build. Checked both directions since either
	// skeleton might be the one authored with the entry.
	if (MeshSkeleton && AnimSkeleton)
	{
		const FSoftObjectPath AnimSkeletonPath(AnimSkeleton);
		const FSoftObjectPath MeshSkeletonPath(MeshSkeleton);

		for (const TSoftObjectPtr<USkeleton>& Compatible : MeshSkeleton->GetCompatibleSkeletons())
		{
			if (Compatible.ToSoftObjectPath() == AnimSkeletonPath)
			{
				return true;
			}
		}
		for (const TSoftObjectPtr<USkeleton>& Compatible : AnimSkeleton->GetCompatibleSkeletons())
		{
			if (Compatible.ToSoftObjectPath() == MeshSkeletonPath)
			{
				return true;
			}
		}
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

void ADCCCharacterBase::ToggleEmoteMenu()
{
	if (bEmoteMenuOpen)
	{
		bEmoteMenuOpen = false;
		return;
	}

	if (EmoteAnimations.IsEmpty())
	{
		ShowLocalNotice(NSLOCTEXT("DCC", "NoEmotes", "Nothing to perform yet."));
		return;
	}

	bEmoteMenuOpen = true;
	EmoteMenuIndex = 0;
}

void ADCCCharacterBase::CycleEmoteMenu(int32 Delta)
{
	if (EmoteAnimations.IsEmpty())
	{
		return;
	}

	const int32 Count = EmoteAnimations.Num();
	EmoteMenuIndex = ((EmoteMenuIndex + Delta) % Count + Count) % Count;
}

void ADCCCharacterBase::ConfirmEmote()
{
	if (!EmoteAnimations.IsValidIndex(EmoteMenuIndex))
	{
		bEmoteMenuOpen = false;
		return;
	}

	Server_RequestEmoteAtIndex(EmoteMenuIndex);
	bEmoteMenuOpen = false;
}

void ADCCCharacterBase::Server_RequestEmoteAtIndex_Implementation(int32 Index)
{
	if (bHasDied || !EmoteAnimations.IsValidIndex(Index))
	{
		return;
	}

	Multicast_PlayEmoteAtIndex(Index);
}

void ADCCCharacterBase::Multicast_PlayEmoteAtIndex_Implementation(int32 Index)
{
	if (!EmoteAnimations.IsValidIndex(Index))
	{
		return;
	}

	PlayOneShotAnimation(EmoteAnimations[Index]);

	// "Custom" per DCCQuestTypes.h's own convention - a dev-goal quest
	// ("perform for the crowd a few times") is exactly the kind of thing
	// that escape hatch exists for. Every machine runs this Multicast, but
	// NotifyEvent's own quest lookup no-ops without authority, so nothing
	// double-counts.
	UDCCQuestComponent::NotifyEvent(this, EDCCObjectiveType::Custom, TEXT("EmotePerformed"), 1);

	// Same Feed line the random kill-celebration path already uses - a
	// player choosing to dance for the audience is exactly as showrunner-
	// coded as one landing by accident.
	ADCCGameState::Log(this, EDCCLogCategory::Combat, FText::Format(
		NSLOCTEXT("DCC", "LogEmote", "{0} plays to the cameras. Chat approves."),
		FText::FromString(GetName())));
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

	// The gear bench is driving the animation by hand.
	if (bBenchPosing)
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

float ADCCCharacterBase::GetClimbSpeedNow() const
{
	const UDCCRuleset* Rules = Stats ? Stats->GetRuleset() : nullptr;
	if (!Rules)
	{
		return ClimbAnimNaturalSpeed;
	}
	const int32 Rank = Skills ? Skills->GetSkillRank(ActiveClimbPoint ? ActiveClimbPoint->ClimbSkillId : FName(TEXT("Climbing"))) : 0;
	return Rules->GetClimbSpeed(Stats->GetEnhanced(), Rank);
}

void ADCCCharacterBase::TickClimbAnimRate(float DeltaSeconds)
{
	USkeletalMeshComponent* Body = GetMesh();
	const float Z = GetActorLocation().Z;
	const bool bOnWall = IsClimbing() && ClimbAnimation && ActiveLoopAnimation == ClimbAnimation && Body
		&& Body->GetAnimationMode() == EAnimationMode::AnimationSingleNode && !bBenchPosing
		&& GetWorld() && GetWorld()->GetTimeSeconds() >= OneShotAnimationUntil;
	if (!bOnWall || DeltaSeconds <= 0.0f)
	{
		SmoothedClimbSpeed = 0.0f;
		LastClimbAnimZ = Z;
		return;
	}
	float Rate;
	if (bCrawling)
	{
		Rate = 1.6f; // scrambling over the lip: brisk, whatever the path's direction
	}
	else
	{
		const float Vz = (Z - LastClimbAnimZ) / DeltaSeconds;
		// Smoothed over ~0.2 s: network updates arrive in steps on other machines.
		SmoothedClimbSpeed = FMath::FInterpTo(SmoothedClimbSpeed, FMath::Clamp(Vz, -600.0f, 600.0f), DeltaSeconds, 10.0f);
		Rate = FMath::Abs(SmoothedClimbSpeed) < 4.0f ? 0.0f : SmoothedClimbSpeed / ClimbAnimNaturalSpeed;
	}
	LastClimbAnimZ = Z;
	Body->SetPlayRate(FMath::Clamp(Rate, -3.0f, 3.0f));
}

void ADCCCharacterBase::OfferQuickItem(FName ItemId)
{
	if (!HasAuthority() || ItemId.IsNone())
	{
		return;
	}
	if (QuickSlots.Num() < 2)
	{
		QuickSlots.SetNum(2);
	}
	if (QuickSlots.Contains(ItemId))
	{
		return;
	}
	const int32 Free = QuickSlots.IndexOfByKey(NAME_None);
	if (Free != INDEX_NONE)
	{
		QuickSlots[Free] = ItemId;
	}
}

int32 ADCCCharacterBase::QuickItemCount(FName ItemId) const
{
	if (ItemId.IsNone())
	{
		return 0;
	}
	if (DCCClimbItems::Find(ItemId))
	{
		int32 Count = 0;
		if (Wallet)
		{
			for (const FName& Tool : Wallet->ClimbTools)
			{
				Count += Tool == ItemId ? 1 : 0;
			}
		}
		return Count;
	}
	if (Inventory)
	{
		for (const FDCCStowedConsumable& Stowed : Inventory->StowedConsumables)
		{
			if (Stowed.ItemId == ItemId)
			{
				return Stowed.Quantity;
			}
		}
	}
	return 0;
}

void ADCCCharacterBase::UseQuickSlot(int32 Index)
{
	if (HasAuthority())
	{
		Server_UseQuickSlot_Implementation(Index);
	}
	else
	{
		Server_UseQuickSlot(Index);
	}
}

void ADCCCharacterBase::Server_UseQuickSlot_Implementation(int32 Index)
{
	if (!QuickSlots.IsValidIndex(Index) || QuickSlots[Index].IsNone() || !IsAlive())
	{
		return;
	}
	const FName ItemId = QuickSlots[Index];
	if (DCCClimbItems::Find(ItemId))
	{
		for (TActorIterator<ADCCArcadeClimb> It(GetWorld()); It; ++It)
		{
			It->UseToolId(this, ItemId);
			break;
		}
	}
	else if (Inventory)
	{
		for (int32 i = 0; i < Inventory->StowedConsumables.Num(); ++i)
		{
			if (Inventory->StowedConsumables[i].ItemId == ItemId)
			{
				Server_ConsumeStowedDrink_Implementation(i);
				break;
			}
		}
	}
	if (QuickItemCount(ItemId) <= 0)
	{
		QuickSlots[Index] = NAME_None;
	}
}

void ADCCCharacterBase::AddScreenShake(float Strength, float Seconds)
{
	const double Now = GetWorld() ? GetWorld()->GetTimeSeconds() : 0.0;
	if (Now >= ShakeUntil || Strength >= ShakeStrength)
	{
		ShakeStarted = Now;
		ShakeStrength = Strength;
	}
	ShakeUntil = FMath::Max(ShakeUntil, Now + Seconds);
}

void ADCCCharacterBase::TickScreenShake()
{
	AController* PC = IsLocallyControlled() ? GetController() : nullptr;
	if (!PC || (ShakeApplied.IsNearlyZero() && GetWorld()->GetTimeSeconds() >= ShakeUntil))
	{
		return;
	}
	const double Now = GetWorld()->GetTimeSeconds();
	FRotator Want = FRotator::ZeroRotator;
	if (Now < ShakeUntil)
	{
		const float Fade = static_cast<float>((ShakeUntil - Now) / FMath::Max(0.05, ShakeUntil - ShakeStarted));
		const float T = static_cast<float>(Now);
		Want.Pitch = ShakeStrength * Fade * (FMath::Sin(T * 37.0f) + 0.5f * FMath::Sin(T * 71.0f));
		Want.Yaw = ShakeStrength * Fade * 0.7f * (FMath::Sin(T * 29.0f + 1.7f) + 0.5f * FMath::Sin(T * 83.0f));
	}
	// Apply only the change since last frame, so the wobble never drifts the view.
	PC->SetControlRotation(PC->GetControlRotation() + (Want - ShakeApplied));
	ShakeApplied = Want;
}

void ADCCCharacterBase::TickClimbTilt(float DeltaSeconds)
{
	USkeletalMeshComponent* Body = GetMesh();
	if (!Body)
	{
		return;
	}
	if (!bBaseMeshRelativeSet)
	{
		BaseMeshRelative = Body->GetRelativeTransform();
		bBaseMeshRelativeSet = true;
	}

	// Crawling over a ledge: the body's head-to-toe axis follows the path, belly to
	// the rock - out along the underside, round the lip, flat over the top. Capped
	// at 80 degrees so it never quite lies flat on its back.
	FQuat Target = FQuat::Identity;
	if (bCrawling && !CrawlDirection.IsNearlyZero())
	{
		const FVector Up(0, 0, 1);
		const float Angle = FMath::Min(FMath::Acos(FMath::Clamp(FVector::DotProduct(Up, CrawlDirection), -1.0f, 1.0f)), FMath::DegreesToRadians(80.0f));
		const FVector Axis = FVector::CrossProduct(Up, CrawlDirection).GetSafeNormal();
		if (!Axis.IsNearlyZero())
		{
			Target = FQuat(Axis, Angle);
		}
	}
	else if (!bCrawling)
	{
		CrawlDirection = FVector::ZeroVector;
	}
	ClimbTilt = FQuat::Slerp(ClimbTilt, Target, FMath::Clamp(DeltaSeconds * 9.0f, 0.0f, 1.0f));

	if (ClimbTilt.Equals(FQuat::Identity, 0.0005f) && Target.Equals(FQuat::Identity))
	{
		if (!Body->GetRelativeTransform().Equals(BaseMeshRelative, 0.01f))
		{
			Body->SetRelativeLocationAndRotation(BaseMeshRelative.GetLocation(), BaseMeshRelative.GetRotation());
		}
		return;
	}
	// The tilt is in world space, about the capsule's centre; turn it into the
	// actor's own frame and apply it to the mesh's resting offset.
	const FQuat Actor = GetActorQuat();
	const FQuat Local = Actor.Inverse() * ClimbTilt * Actor;
	Body->SetRelativeLocationAndRotation(Local.RotateVector(BaseMeshRelative.GetLocation()), Local * BaseMeshRelative.GetRotation());
}

void ADCCCharacterBase::Tick(float DeltaSeconds)
{
	Super::Tick(DeltaSeconds);

	TickHoldToClimb(DeltaSeconds);
	TickClimbCamera(DeltaSeconds);
	TickClimbTilt(DeltaSeconds);
	TickClimbAnimRate(DeltaSeconds);

	// On the wall the climber is in flying mode, whose default top speed (6 m/s) made A/D a free float
	// sideways (Zoe, 2026-11-14). Sideways is ~70% of the climb speed, and stops when you let go.
	if (UCharacterMovementComponent* Move = GetCharacterMovement())
	{
		if (IsClimbing() && !bCrawling)
		{
			Move->MaxFlySpeed = FMath::Max(25.0f, GetClimbSpeedNow() * 0.7f);
			Move->BrakingDecelerationFlying = 2400.0f;
			bClimbFlyTuned = true;
		}
		else if (bClimbFlyTuned)
		{
			const UCharacterMovementComponent* Defaults = GetDefault<UCharacterMovementComponent>();
			Move->MaxFlySpeed = Defaults->MaxFlySpeed;
			Move->BrakingDecelerationFlying = Defaults->BrakingDecelerationFlying;
			bClimbFlyTuned = false;
		}
	}
	TickScreenShake();

	// Fall damage: the highest point of the current fall, server-side. Anything
	// but falling (walking, climbing, swimming, riding) resets it to where we are.
	if (HasAuthority())
	{
		const UCharacterMovementComponent* Movement = GetCharacterMovement();
		const float Z = GetActorLocation().Z;
		FallPeakZ = Movement && Movement->IsFalling() ? FMath::Max(FallPeakZ, Z) : Z;
	}

	// Worn ticket-booth gear that has to do something every frame: the hobby jetpack,
	// and the miner's lamp. The plates, the cape, the pack and the auras are read
	// where the hit, the fall, the climb and the walk speed already live.
	if (Wallet)
	{
		struct FWornTick
		{
			float Fuel = 1.8f;
			bool bWasDown = false;
			TWeakObjectPtr<USpotLightComponent> Lamp;
		};
		static TMap<TWeakObjectPtr<ADCCCharacterBase>, FWornTick> Worn;
		FWornTick& State = Worn.FindOrAdd(this);

		const bool bLamp = Wallet->WornIn(DCCCosmetics::Head) == FName(TEXT("MinersHelmet"));
		if (bLamp && !State.Lamp.IsValid())
		{
			USpotLightComponent* Light = NewObject<USpotLightComponent>(this);
			Light->SetupAttachment(GetRootComponent());
			Light->SetRelativeLocation(FVector(28.0f, 0.0f, 72.0f));
			Light->SetRelativeRotation(FRotator(-8.0f, 0.0f, 0.0f));
			Light->SetIntensity(14000.0f);
			Light->SetAttenuationRadius(2400.0f);
			Light->SetInnerConeAngle(16.0f);
			Light->SetOuterConeAngle(36.0f);
			Light->SetLightColor(FLinearColor(1.0f, 0.93f, 0.78f));
			Light->SetCastShadows(false);
			Light->RegisterComponent();
			State.Lamp = Light;
		}
		else if (!bLamp && State.Lamp.IsValid())
		{
			State.Lamp->DestroyComponent();
			State.Lamp = nullptr;
		}

		const bool bJet = Wallet->WornIn(DCCCosmetics::Back) == FName(TEXT("HobbyJetpack"));
		UCharacterMovementComponent* Move = GetCharacterMovement();
		const bool bDrive = bJet && Move && !IsClimbing() && (HasAuthority() || IsLocallyControlled());
		const APlayerController* PC = (bDrive && IsLocallyControlled()) ? Cast<APlayerController>(GetController()) : nullptr;
		// bPressedJump is what the server actually has for a remote jumper. The key is what the owner has.
		const bool bDown = bPressedJump || (PC && PC->IsInputKeyDown(EKeys::SpaceBar));
		if (bDrive && Move->IsMovingOnGround())
		{
			State.Fuel = FMath::Min(1.8f, State.Fuel + DeltaSeconds * 0.45f);
		}
		if (bDrive && bDown && State.Fuel > 0.0f)
		{
			State.Fuel = FMath::Max(0.0f, State.Fuel - DeltaSeconds);
			if (Move->IsMovingOnGround())
			{
				if (!State.bWasDown)
				{
					LaunchCharacter(FVector(0.0f, 0.0f, 520.0f), false, true);
				}
			}
			else
			{
				Move->Velocity.Z = FMath::Max(Move->Velocity.Z, 360.0f);
			}
		}
		State.bWasDown = bDown;
	}

	// Safety net for Selfie Mode's own exit chain - see
	// SelfieModeEnteredRealTimeSeconds's comment for why this is necessary and
	// why it deliberately uses real time rather than another SetTimer call.
	if (bInSelfieMode && SelfieModeEnteredRealTimeSeconds >= 0.0 && GetWorld()
		&& GetWorld()->GetRealTimeSeconds() - SelfieModeEnteredRealTimeSeconds > SelfieMaxRealSeconds)
	{
		UE_LOG(LogTemp, Warning, TEXT("%s: Selfie Mode force-released after exceeding its %.1fs real-time ceiling - its normal exit timers likely stalled (e.g. combat pause slowing world time)."),
			*GetName(), SelfieMaxRealSeconds);

		GetWorldTimerManager().ClearTimer(SelfieShutterTimer);
		GetWorldTimerManager().ClearTimer(SelfieExitTimer);
		ExitSelfieMode();
	}

	// EnterSelfieMode already refuses to START a pose near a hostile, but that
	// only ever covered the moment of pressing C - it does nothing about a
	// hostile that closes in (or spawns) WHILE already posing, or a character
	// that somehow ends up in Selfie Mode with one already nearby. Zoe's own
	// report (2026-08-29): "spawned in in selfie mode and can't get out" with
	// enemies nearby - this is the other of the two hardening options this
	// project's own 2026-08-28 entry flagged and never built: proactively
	// throw the camera back rather than only bounding how long a stuck one
	// can last. ExitSelfieMode itself has no hostile gate and never has -
	// exiting is and must stay unconditional - this just calls it proactively
	// instead of waiting for the player to notice and press C.
	if (bInSelfieMode && HasNearbyLivingHostile(SelfieHostileRefuseRadius))
	{
		ShowLocalNotice(NSLOCTEXT("DCC", "SelfieInterrupted", "Something's coming - selfie cut short."));

		GetWorldTimerManager().ClearTimer(SelfieShutterTimer);
		GetWorldTimerManager().ClearTimer(SelfieExitTimer);
		ExitSelfieMode();
	}

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
	else if (IsClimbing() && IsLocallyControlled())
	{
		// Cosmetic-only mirror of TickStamina's climbing math, for the one
		// machine that is neither the server nor a simulated proxy: the
		// climbing player's own client. See TickClimbMovementCosmetic's
		// comment for why this is necessary at all.
		TickClimbMovementCosmetic(DeltaSeconds);
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

void ADCCCharacterBase::Multicast_PlayEnterVehicleAnimation_Implementation()
{
	PlayOneShotAnimation(EnterVehicleAnimation);
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
	if (bUseRagdollOnDeath)
	{
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
	else if (USkeletalMeshComponent* MeshComponent = GetMesh())
	{
		// No physics at all, rather than a tamed-down version of it - a bad
		// Physics Asset does not get better by simulating it gently. The mesh
		// freezes exactly where it stood, still blocking Visibility (so
		// FindLootTargetUnderCursor still finds it) but never ECC_PhysicsBody,
		// so nothing here can ever fight a constraint again.
		MeshComponent->SetCollisionEnabled(ECollisionEnabled::QueryOnly);
		MeshComponent->SetCollisionObjectType(ECC_WorldStatic);
		MeshComponent->SetCollisionResponseToAllChannels(ECR_Ignore);
		MeshComponent->SetCollisionResponseToChannel(ECC_Visibility, ECR_Block);

		if (UNiagaraSystem* DissolveSystem = DeathDissolveEffect.LoadSynchronous())
		{
			UNiagaraFunctionLibrary::SpawnSystemAtLocation(
				GetWorld(), DissolveSystem, GetActorLocation(), GetActorRotation());
		}
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
		// On the tale floor the System rewrites the ending (a narrator save) or,
		// once those are spent, sends the crawler home. Nobody dies for good there.
		if (ADCCTaleFloor::InterceptDeath(this))
		{
			return;
		}

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

		if (BodyCollectorClass)
		{
			GetWorldTimerManager().SetTimer(BodyCollectorTimer, this, &ADCCCharacterBase::CheckForBodyCollector, BodyCollectorDelaySeconds, false);
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

void ADCCCharacterBase::ReviveQuietly(float HealthFraction)
{
	if (!HasAuthority() || !bIsDowned || bHasDied || !Stats)
	{
		return;
	}

	GetWorldTimerManager().ClearTimer(DeathSaveTimer);
	GetWorldTimerManager().ClearTimer(ReviveTimer);
	DeathSaveSuccesses = 0;
	DeathSaveFailures = 0;

	bIsDowned = false;
	RestoreStandingBody();
	PlayCharacterSound(ReviveSound);

	Stats->Heal(FMath::Max(1, FMath::RoundToInt(Stats->MaxHitPoints * FMath::Clamp(HealthFraction, 0.0f, 1.0f))));
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

	bLootHandled = true;

	DCCLoot::SpawnRolledLoot(GetWorld(), PendingLoot[Index], GetLootOrigin(), Looter);
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

	bLootHandled = true;

	// Cleared first: two players confirming "Loot All" on the same frame would
	// otherwise both pass the check above and spawn everything twice.
	bHasLoot = false;

	const FVector Origin = GetLootOrigin();
	for (const FDCCRolledLoot& Rolled : PendingLoot)
	{
		DCCLoot::SpawnRolledLoot(GetWorld(), Rolled, Origin, Looter);
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

	bLootHandled = true;

	if (Looter && Looter->Inventory)
	{
		Looter->Inventory->StowRemains(GetLootSourceName(), PendingLoot);
	}

	PendingLoot.Reset();
	bHasLoot = false;

	// Stow retires the source outright, same as ADCCLootContainer::StowInto -
	// the whole point of "Stow" over "Loot All" is that the body/chest is gone
	// afterward, not left standing empty for LootedCorpseLifeSpan to catch up
	// to later. LootAll/LootOne still use the timed fade, since those leave the
	// choice of when to stop looting to the player.
	Destroy();
}

namespace
{
	// Resolved lazily, on first use, rather than as a constructor-time
	// ConstructorHelpers::FClassFinder default. That original version hung
	// the editor on every launch (see DISPATCH.md, 2026-08-28 incident):
	// ADCCCharacterBase's constructor runs for every character CDO,
	// including ADCCEnemyCharacter's own, and BP_DCCEnemy is itself a
	// Blueprint parented to ADCCEnemyCharacter - so loading it from inside
	// that hierarchy's own constructor made CDO construction depend on
	// itself. Resolving here, well after all CDOs exist, breaks the cycle.
	// Do not move this back into the constructor.
	TSubclassOf<ADCCEnemyCharacter> ResolveDefaultBodyCollectorClass()
	{
		static const TSubclassOf<ADCCEnemyCharacter> DefaultClass = []() -> TSubclassOf<ADCCEnemyCharacter>
		{
			const FSoftClassPath Path(TEXT("/Game/DCC/Characters/BP_DCCEnemy.BP_DCCEnemy_C"));
			return Path.TryLoadClass<ADCCEnemyCharacter>();
		}();
		return DefaultClass;
	}
}

void ADCCCharacterBase::CheckForBodyCollector()
{
	if (!HasAuthority() || bLootHandled || !bHasDied)
	{
		return;
	}

	// BodyCollectorClass is authorable per-Blueprint (EditDefaultsOnly); an
	// unset one falls back to the bare BP_DCCEnemy, resolved lazily - see
	// ResolveDefaultBodyCollectorClass above for why this can't happen in
	// the constructor.
	const TSubclassOf<ADCCEnemyCharacter> CollectorClass = BodyCollectorClass ? TSubclassOf<ADCCEnemyCharacter>(BodyCollectorClass) : ResolveDefaultBodyCollectorClass();
	if (!CollectorClass)
	{
		return;
	}

	UWorld* World = GetWorld();
	if (!World)
	{
		return;
	}

	FVector2D RandomDirection = FVector2D(FMath::FRandRange(-1.0f, 1.0f), FMath::FRandRange(-1.0f, 1.0f)).GetSafeNormal();
	if (RandomDirection.IsNearlyZero())
	{
		RandomDirection = FVector2D(1.0f, 0.0f);
	}

	const FVector SpawnLocation = GetActorLocation() + FVector(RandomDirection * BodyCollectorSpawnRadius, 0.0f);
	const FTransform SpawnTransform(FRotator::ZeroRotator, SpawnLocation);

	// Deferred, not SpawnActor - a plain SpawnActor runs BeginPlay before this
	// function returns, which is the exact "DungeonGenerator spawned rooms
	// with a plain SpawnActor and RoomType wasn't set yet" trap this project
	// has already been bitten by once. BeginBodyCollection has to land before
	// Tick's first run sees bIsBodyCollector.
	ADCCEnemyCharacter* Collector = World->SpawnActorDeferred<ADCCEnemyCharacter>(CollectorClass, SpawnTransform);
	if (Collector)
	{
		Collector->BeginBodyCollection(this);
		Collector->FinishSpawning(SpawnTransform);
	}
}

void ADCCCharacterBase::NotifyCombatStarted()
{
	if (!HasAuthority() || !Equipment)
	{
		return;
	}

	FDCCEquipmentDef CursedItem;
	if (!Equipment->FindCursedCombatStartEffect(CursedItem))
	{
		return;
	}

	// LaunchCharacter, not a teleport - matches this project's own existing
	// hit-stagger convention (see OnAnyAttackAttempted's ImpactSource shove)
	// and, because it is resolved through the normal movement component
	// simulation rather than an instant relocation, it cannot clip a
	// character through solid geometry the way a raw SetActorLocation could.
	// It CAN still carry someone off a ledge or into an open hazard (the
	// Diving Pool, a trap trigger) since nothing about a launch stops at a
	// trigger volume the way it stops at a wall - flagged, not solved, here.
	LaunchCharacter(
		GetActorForwardVector() * CursedItem.ForcedHopForwardSpeed + FVector(0.0f, 0.0f, CursedItem.ForcedHopUpwardSpeed),
		false, false);

	if (UAnimationAsset* TauntAnimation = CursedItem.ForcedActionAnimation.LoadSynchronous())
	{
		PlayOneShotAnimation(TauntAnimation);
	}

	ADCCGameState::Log(this, EDCCLogCategory::Announcement, !CursedItem.ForcedActionFlavorText.IsEmpty()
		? FText::Format(NSLOCTEXT("DCC", "LogCursedCombatStart", "{0} {1}"), GetLootSourceName(), CursedItem.ForcedActionFlavorText)
		: FText::Format(NSLOCTEXT("DCC", "LogCursedCombatStartDefault", "{0}'s cursed gear forces them into motion."), GetLootSourceName()));
}

void ADCCCharacterBase::RecordRoomVisited(FName RoomId)
{
	if (!HasAuthority() || RoomId.IsNone() || VisitedRoomIds.Contains(RoomId))
	{
		return;
	}

	// "Still In Your Pajamas" - this crawler's very first room entry, full
	// stop, checked before adding RoomId below. In this floor's own layout
	// the first room anyone ever enters is the Queue, so this reads as "reached
	// the Queue" without hardcoding that RoomId specifically - it would still
	// fire correctly if the starting room ever changed.
	if (VisitedRoomIds.IsEmpty() && Equipment && Achievements
		&& Equipment->GetEquippedId(EDCCEquipmentSlot::Body).IsNone())
	{
		Achievements->Record(EDCCAchievementStat::ArrivedBodySlotEmpty);
	}

	VisitedRoomIds.Add(RoomId);

	if (Achievements)
	{
		Achievements->Record(EDCCAchievementStat::UniqueRoomVisited);
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

	// Added after Zoe got caught posing mid-fight (2026-08-28). The real-time
	// ceiling below bounds how LONG a stuck camera can last, but does nothing
	// about the far more common case of choosing to pose while something is
	// actively trying to kill you. Refusing outright here is simpler and safer
	// than trying to auto-cancel mid-pose the moment a fight starts.
	if (HasNearbyLivingHostile(SelfieHostileRefuseRadius))
	{
		ShowLocalNotice(NSLOCTEXT("DCC", "SelfieTooDangerous", "Too dangerous to stop and pose right now."));
		return;
	}

	bInSelfieMode = true;
	SelfieModeEnteredRealTimeSeconds = GetWorld() ? GetWorld()->GetRealTimeSeconds() : -1.0;

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
	SelfieModeEnteredRealTimeSeconds = -1.0;
	CurrentSelfiePoseIndex = 0;

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

void ADCCCharacterBase::CycleSelfiePose(int32 Delta)
{
	if (!bInSelfieMode || SelfiePoseAnimations.Num() == 0)
	{
		return;
	}

	const int32 Count = SelfiePoseAnimations.Num();
	CurrentSelfiePoseIndex = ((CurrentSelfiePoseIndex + Delta) % Count + Count) % Count;

	if (UAnimationAsset* Pose = SelfiePoseAnimations[CurrentSelfiePoseIndex])
	{
		PlayOneShotAnimation(Pose);
	}

	// Picking a pose restarts the hold, so the shutter captures the pose just
	// chosen rather than firing mid-swap because the clock was already most of
	// the way through whatever pose came before.
	GetWorldTimerManager().SetTimer(
		SelfieShutterTimer, this, &ADCCCharacterBase::TakeSelfieScreenshot, SelfiePoseSeconds, false);
}

float ADCCCharacterBase::GetSelfiePoseProgress() const
{
	if (!bInSelfieMode || SelfiePoseSeconds <= 0.0f)
	{
		return 0.0f;
	}

	return FMath::Clamp(GetWorldTimerManager().GetTimerElapsed(SelfieShutterTimer) / SelfiePoseSeconds, 0.0f, 1.0f);
}

bool ADCCCharacterBase::HasNearbyLivingHostile(float Range) const
{
	UWorld* World = GetWorld();
	if (!World)
	{
		return false;
	}

	const float RangeSquared = FMath::Square(Range);

	for (TActorIterator<ADCCCharacterBase> It(World); It; ++It)
	{
		const ADCCCharacterBase* Candidate = *It;
		if (!Candidate || Candidate == this || !Candidate->IsAlive() || !IsHostileTo(Candidate))
		{
			continue;
		}

		if (FVector::DistSquared(GetActorLocation(), Candidate->GetActorLocation()) <= RangeSquared)
		{
			return true;
		}
	}

	return false;
}

namespace
{
	/**
	 * True for world pieces each machine builds for itself (the arcade games' floors, rocks and props: Block, Shape,
	 * Prop, HISM holders). They are not replicated and have no stable network name, so the other machine cannot
	 * find them. Standing on one used to make a client send "my position relative to <a thing you do not have>":
	 * the host could not resolve it and threw the moves away, so on the host that player stood still where they
	 * first stepped on a prop while they walked on in their own game (found 2026-11-16, Fourteener with Anke).
	 */
	bool IsLocalOnlyBase(const UObject* Base)
	{
		const UActorComponent* Component = Cast<UActorComponent>(Base);
		const AActor* Owner = Component ? Component->GetOwner() : nullptr;
		const UWorld* World = Owner ? Owner->GetWorld() : nullptr;
		return Owner && World && World->GetNetMode() != NM_Standalone
			&& !Owner->GetIsReplicated() && !Owner->IsNameStableForNetworking();
	}
}

void ADCCCharacterBase::SetBase(UPrimitiveComponent* NewBase, const FName BoneName, bool bNotifyActor)
{
	Super::SetBase(IsLocalOnlyBase(NewBase) ? nullptr : NewBase, BoneName, bNotifyActor);
}

void ADCCCharacterBase::SetBase(FMovementBaseInterfaceData* MovementBaseInterfaceData, const FName BoneName, bool bNotifyActor)
{
	if (MovementBaseInterfaceData && IsLocalOnlyBase(MovementBaseInterfaceData->GetMovementBaseObject()))
	{
		Super::SetBase(static_cast<UPrimitiveComponent*>(nullptr), BoneName, bNotifyActor);
		return;
	}
	Super::SetBase(MovementBaseInterfaceData, BoneName, bNotifyActor);
}
