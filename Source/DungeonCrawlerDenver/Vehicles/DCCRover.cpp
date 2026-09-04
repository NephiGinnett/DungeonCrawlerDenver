#include "Vehicles/DCCRover.h"

#include "Characters/DCCCharacterBase.h"
#include "Components/AudioComponent.h"
#include "Components/BoxComponent.h"
#include "Components/StaticMeshComponent.h"
#include "GameFramework/SpringArmComponent.h"
#include "Camera/CameraComponent.h"
#include "GameFramework/FloatingPawnMovement.h"
#include "GameFramework/PlayerController.h"
#include "Engine/World.h"
#include "EngineUtils.h"
#include "Net/UnrealNetwork.h"
#include "Quests/DCCQuestComponent.h"
#include "Materials/MaterialInstanceDynamic.h"
#include "NiagaraComponent.h"
#include "NiagaraSystem.h"
#include "Skills/DCCSkillComponent.h"
#include "Stats/DCCStatsComponent.h"
#include "UObject/ConstructorHelpers.h"

ADCCRover::ADCCRover()
{
	PrimaryActorTick.bCanEverTick = true;
	bReplicates = true;
	SetReplicateMovement(true);

	Collision = CreateDefaultSubobject<UBoxComponent>(TEXT("Collision"));
	SetRootComponent(Collision);
	// The Synthwave car meshes are ~353 wide x 628 long x 162 tall, oriented
	// down Y, so the box is sized to that and the body is rotated to face +X.
	Collision->SetBoxExtent(FVector(320.0f, 180.0f, 90.0f));
	// Blocks world geometry rather than overlapping it. Previously this
	// responded Overlap to everything, which is why the rover drove straight
	// through walls - the movement sweep had nothing to stop against.
	Collision->SetCollisionEnabled(ECollisionEnabled::QueryOnly);
	Collision->SetCollisionResponseToAllChannels(ECR_Block);
	Collision->SetCollisionResponseToChannel(ECC_Pawn, ECR_Overlap);
	Collision->SetCollisionObjectType(ECC_Pawn);

	Body = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("Body"));
	Body->SetupAttachment(Collision);
	static ConstructorHelpers::FObjectFinder<UStaticMesh> CarMesh(
		TEXT("/Game/Synthwave_city/Meshes/Car/SM_car_01.SM_car_01"));
	if (CarMesh.Succeeded())
	{
		Body->SetStaticMesh(CarMesh.Object);
	}
	// The mesh's length runs down its local Y; yaw it so "forward" is +X, which
	// is what every movement call below assumes.
	Body->SetRelativeRotation(FRotator(0.0f, -90.0f, 0.0f));
	Body->SetRelativeLocation(FVector(0.0f, 0.0f, -90.0f));
	Body->SetCollisionEnabled(ECollisionEnabled::NoCollision);

	CameraBoom = CreateDefaultSubobject<USpringArmComponent>(TEXT("CameraBoom"));
	CameraBoom->SetupAttachment(Collision);
	CameraBoom->TargetArmLength = 900.0f;
	CameraBoom->SetRelativeRotation(FRotator(-18.0f, 0.0f, 0.0f));
	CameraBoom->bDoCollisionTest = false;
	CameraBoom->bInheritPitch = false;
	CameraBoom->bInheritRoll = false;

	ChaseCamera = CreateDefaultSubobject<UCameraComponent>(TEXT("ChaseCamera"));
	ChaseCamera->SetupAttachment(CameraBoom);

	// Thruster trail.
	//
	// WAS NS_Flying_car - wrong effect, found the hard way (Zoe, 2026-09-03:
	// "it causes a trail of what looks like a random gray vehicle body that
	// just keeps spawning behind me"). Checked its actual dependencies
	// rather than guessing further: it hard-references
	// /Game/Cyberpunk_City/Meshes/Flying_car_01/SM_flying_car_01 and its
	// material - it is a MESH-RENDERER "afterimage" effect built to ghost
	// THAT specific hovercar's own body, not a generic light/energy trail.
	// Reused verbatim, it does exactly what it was built to do: spawn copies
	// of a car body that isn't this one, textured wrong outside its own
	// scene.
	//
	// NS_blue_FX replaces it - the same Alien_Portal glow already used for
	// the flight rings' objective marker. Confirmed via the same dependency
	// check to be purely sprite-based (only /Game/Alien_Portal/Material/
	// MI_sprite, no mesh reference at all), and its blue glow reads as
	// engine wash against this car's own Synthwave neon look better than
	// Legendary_Forge's NS_sparks would. Starts inactive; UpdateTrail
	// switches it with boost and flight.
	BoostTrail = CreateDefaultSubobject<UNiagaraComponent>(TEXT("BoostTrail"));
	BoostTrail->SetupAttachment(Collision);
	BoostTrail->SetRelativeLocation(FVector(-330.0f, 0.0f, -40.0f));
	BoostTrail->SetRelativeRotation(FRotator(0.0f, 180.0f, 0.0f));
	BoostTrail->bAutoActivate = false;
	static ConstructorHelpers::FObjectFinder<UNiagaraSystem> TrailFX(
		TEXT("/Game/Alien_Portal/FX/Particles/NS_blue_FX.NS_blue_FX"));
	if (TrailFX.Succeeded())
	{
		BoostTrail->SetAsset(TrailFX.Object);
	}

	RadioAudio = CreateDefaultSubobject<UAudioComponent>(TEXT("RadioAudio"));
	RadioAudio->SetupAttachment(Body);
	RadioAudio->bAutoActivate = false;
	RadioAudio->bIsUISound = false; // Spatialized - it plays FROM the car, not straight to the listener's ear.

	// Zoe's own track, added 2026-09-03 (SourceArt/Freaks.mp3) - ahead of the
	// generic sample per her ask ("play it before other songs for now").
	// ConstructorHelpers resolves live at editor startup, not baked into the
	// DLL - if this import hasn't landed at /Game/DCC/Audio/Freaks yet, this
	// entry is silently skipped and self-heals on the next restart once it
	// has, same as every other asset added mid-session in this project.
	static ConstructorHelpers::FObjectFinder<USoundBase> FreaksTrack(
		TEXT("/Game/DCC/Audio/Freaks.Freaks"));
	if (FreaksTrack.Succeeded())
	{
		RadioTracks.Add(FreaksTrack.Object);
	}

	static ConstructorHelpers::FObjectFinder<USoundBase> SampleTrack(
		TEXT("/Game/SoulCity/Sound/Cue/Music/Soul_Music_FlyUp01_Cue.Soul_Music_FlyUp01_Cue"));
	if (SampleTrack.Succeeded())
	{
		RadioTracks.Add(SampleTrack.Object);
	}

	Movement = CreateDefaultSubobject<UFloatingPawnMovement>(TEXT("Movement"));
	Movement->UpdatedComponent = Collision;
	Movement->MaxSpeed = MaxCruiseSpeed;

	// Both dropped from 2200/2600 for weight (Zoe, 2026-09-03 - see
	// SecondsToReachCruise's own comment for the other half of this).
	// Acceleration alone controls how fast actual velocity climbs toward
	// whatever SpeedRamp's CURRENT cap is - with the old 2200 it reached
	// that cap almost instantly regardless of the ramp, so the ramp was
	// doing all the "building up" work by itself. Lowering this too
	// compounds a heavier launch on top of it. Deceleration is the literal
	// "drags a little": releasing the throttle now coasts down over a real
	// couple of seconds instead of snapping to a stop, which is what a
	// heavy vehicle's own momentum should feel like.
	Movement->Acceleration = 1500.0f;
	Movement->Deceleration = 900.0f;
}

void ADCCRover::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);
	DOREPLIFETIME(ADCCRover, Driver);
	DOREPLIFETIME(ADCCRover, Passengers);
	DOREPLIFETIME(ADCCRover, bWindowsDown);
	DOREPLIFETIME(ADCCRover, bRadioOn);
}

void ADCCRover::SetupPlayerInputComponent(UInputComponent* PlayerInputComponent)
{
	Super::SetupPlayerInputComponent(PlayerInputComponent);
	if (!PlayerInputComponent)
	{
		return;
	}

	// Raw key bindings rather than named axes, deliberately.
	//
	// This project has NO AxisMappings in DefaultInput.ini - the player pawn
	// drives off Enhanced Input mapping contexts owned by the character - so a
	// BindAxis("MoveForward") here would silently never fire and the rover
	// would look drivable and do nothing. Binding keys directly keeps the rover
	// independent of both input systems, which is also what makes it work the
	// moment it is possessed without an IMC being pushed for it.
	PlayerInputComponent->BindKey(EKeys::W, IE_Pressed,  this, &ADCCRover::ThrottleForwardPressed);
	PlayerInputComponent->BindKey(EKeys::W, IE_Released, this, &ADCCRover::ThrottleForwardReleased);
	PlayerInputComponent->BindKey(EKeys::S, IE_Pressed,  this, &ADCCRover::ThrottleReversePressed);
	PlayerInputComponent->BindKey(EKeys::S, IE_Released, this, &ADCCRover::ThrottleReverseReleased);
	PlayerInputComponent->BindKey(EKeys::A, IE_Pressed,  this, &ADCCRover::SteerLeftPressed);
	PlayerInputComponent->BindKey(EKeys::A, IE_Released, this, &ADCCRover::SteerLeftReleased);
	PlayerInputComponent->BindKey(EKeys::D, IE_Pressed,  this, &ADCCRover::SteerRightPressed);
	PlayerInputComponent->BindKey(EKeys::D, IE_Released, this, &ADCCRover::SteerRightReleased);
	PlayerInputComponent->BindKey(EKeys::LeftShift, IE_Pressed,  this, &ADCCRover::DriftPressed);
	PlayerInputComponent->BindKey(EKeys::LeftShift, IE_Released, this, &ADCCRover::DriftReleased);
	PlayerInputComponent->BindKey(EKeys::SpaceBar,  IE_Pressed,  this, &ADCCRover::BoostPressed);
	PlayerInputComponent->BindKey(EKeys::SpaceBar,  IE_Pressed,  this, &ADCCRover::ClimbPressed);
	PlayerInputComponent->BindKey(EKeys::SpaceBar,  IE_Released, this, &ADCCRover::ClimbReleased);
	PlayerInputComponent->BindKey(EKeys::LeftControl, IE_Pressed,  this, &ADCCRover::DescendPressed);
	PlayerInputComponent->BindKey(EKeys::LeftControl, IE_Released, this, &ADCCRover::DescendReleased);
	PlayerInputComponent->BindKey(EKeys::X, IE_Pressed,  this, &ADCCRover::EjectPressed);
	PlayerInputComponent->BindKey(EKeys::E, IE_Pressed,  this, &ADCCRover::InputExit);
	PlayerInputComponent->BindKey(EKeys::R, IE_Pressed,  this, &ADCCRover::WindowTogglePressed);
	PlayerInputComponent->BindKey(EKeys::T, IE_Pressed,  this, &ADCCRover::RadioTogglePressed);
	PlayerInputComponent->BindKey(EKeys::LeftMouseButton, IE_Pressed,  this, &ADCCRover::FlightPitchDragPressed);
	PlayerInputComponent->BindKey(EKeys::LeftMouseButton, IE_Released, this, &ADCCRover::FlightPitchDragReleased);
	PlayerInputComponent->BindAxisKey(EKeys::MouseY, this, &ADCCRover::FlightPitchDrag);

	// Mouse X swings the chase camera rather than steering, so a driver can
	// look into a corner without changing where the car is pointed.
	PlayerInputComponent->BindAxisKey(EKeys::MouseX, this, &ADCCRover::CameraLook);
}

void ADCCRover::DriftPressed()  { bDrifting = true; }
void ADCCRover::DriftReleased() { bDrifting = false; }

void ADCCRover::BoostPressed()
{
	// Space does two jobs, chosen by context: on the ground it boosts, and in
	// the air - once the rover has fallen far enough to have committed to the
	// jump - the same key engages flight. One button, because "boost again at
	// the top of the arc" is how a player will reach for it anyway.
	if (CanEngageFlight())
	{
		EngageFlight();
		return;
	}

	if (bFlying)
	{
		// Already flying: Space is the climb key, handled as an axis below.
		return;
	}

	if (bBoosting || BoostCooldownRemaining > 0.0f || CrashRecoveryRemaining > 0.0f)
	{
		return;
	}
	bBoosting = true;
	BoostSecondsRemaining = BoostDurationSeconds;
}

// Space climbs and Left Ctrl descends while flying. Additive for the same
// reason the throttle keys are: holding both cancels cleanly.
void ADCCRover::ClimbPressed()    { VerticalInput += 1.0f; }
void ADCCRover::ClimbReleased()   { VerticalInput -= 1.0f; }
void ADCCRover::DescendPressed()  { VerticalInput -= 1.0f; }
void ADCCRover::DescendReleased() { VerticalInput += 1.0f; }

void ADCCRover::FlightPitchDragPressed()  { bFlightPitchDragHeld = true; }
void ADCCRover::FlightPitchDragReleased() { bFlightPitchDragHeld = false; }

void ADCCRover::FlightPitchDrag(float Value)
{
	// A flap lever, not a joystick: dragging changes the setting, releasing
	// the button leaves it exactly where it was rather than springing back
	// to level. Sign is a first guess, not a verified one - there is no
	// input-injection tool in this project's MCP toolset to confirm MouseY's
	// polarity by actually flying it, so flip this if a real flight shows
	// dragging up dives instead of climbs.
	if (bFlightPitchDragHeld && !FMath::IsNearlyZero(Value))
	{
		FlightPitchInput = FMath::Clamp(FlightPitchInput + Value * FlightPitchDragSensitivity, -1.0f, 1.0f);
	}
}

void ADCCRover::CameraLook(float Value)
{
	// Accumulates an offset the camera then eases back out of, so letting go of
	// the mouse drifts the view back behind the car instead of snapping.
	CameraYawTarget = FMath::Clamp(CameraYawTarget + Value * 2.5f, -120.0f, 120.0f);
}

float ADCCRover::GetBoostChargeFraction() const
{
	if (bBoosting)
	{
		return 0.0f;
	}
	return BoostCooldownSeconds > 0.0f
		? 1.0f - FMath::Clamp(BoostCooldownRemaining / BoostCooldownSeconds, 0.0f, 1.0f)
		: 1.0f;
}

// Additive rather than assigning, so holding W and S together cancels and
// releasing one key does not wipe the other's contribution.
void ADCCRover::ThrottleForwardPressed()  { ThrottleInput += 1.0f; }
void ADCCRover::ThrottleForwardReleased() { ThrottleInput -= 1.0f; }
void ADCCRover::ThrottleReversePressed()  { ThrottleInput -= 1.0f; }
void ADCCRover::ThrottleReverseReleased() { ThrottleInput += 1.0f; }
void ADCCRover::SteerLeftPressed()        { SteerInput -= 1.0f; }
void ADCCRover::SteerLeftReleased()       { SteerInput += 1.0f; }
void ADCCRover::SteerRightPressed()       { SteerInput += 1.0f; }
void ADCCRover::SteerRightReleased()      { SteerInput -= 1.0f; }

void ADCCRover::InputExit()
{
	if (HasAuthority())
	{
		ExitRover();
	}
	else if (Driver)
	{
		Driver->Server_ExitRover(this);
	}
}

void ADCCRover::WindowTogglePressed()
{
	if (HasAuthority())
	{
		ToggleWindows();
	}
	else if (Driver)
	{
		Driver->Server_ToggleRoverWindows(this);
	}
}

void ADCCRover::ToggleWindows()
{
	bWindowsDown = !bWindowsDown;
	// The server doesn't get its own OnRep call - drive the visual directly
	// here too, same as every other ReplicatedUsing property in this project.
	OnRep_WindowsDown();
}

void ADCCRover::OnRep_WindowsDown()
{
	// Nothing to do beyond the replication itself - UpdateWindowOpacity reads
	// bWindowsDown every Tick and eases toward it, so there is no snap state
	// to set here. Exists as a hook in case a "windows moving" sound cue
	// wants one later.
}

void ADCCRover::UpdateWindowOpacity(float DeltaSeconds)
{
	if (!WindowMID)
	{
		if (!Body)
		{
			return;
		}
		UMaterialInterface* Translucent = LoadObject<UMaterialInterface>(nullptr,
			TEXT("/Game/DCC/Materials/M_CarWindow_Translucent.M_CarWindow_Translucent"));
		if (!Translucent)
		{
			return;
		}
		const int32 WindowSlot = Body->GetMaterialIndex(TEXT("Window"));
		if (WindowSlot == INDEX_NONE)
		{
			return;
		}
		WindowMID = UMaterialInstanceDynamic::Create(Translucent, this);
		Body->SetMaterial(WindowSlot, WindowMID);
	}

	const float TargetOpacity = bWindowsDown ? 0.0f : 1.0f;
	const float InterpSpeed = WindowRollSeconds > 0.0f ? 1.0f / WindowRollSeconds : 100.0f;
	WindowOpacityCurrent = FMath::FInterpConstantTo(WindowOpacityCurrent, TargetOpacity, DeltaSeconds, InterpSpeed);
	WindowMID->SetScalarParameterValue(TEXT("Opacity"), WindowOpacityCurrent);
}

void ADCCRover::RadioTogglePressed()
{
	if (HasAuthority())
	{
		ToggleRadio();
	}
	else if (Driver)
	{
		Driver->Server_ToggleRoverRadio(this);
	}
}

void ADCCRover::ToggleRadio()
{
	bRadioOn = !bRadioOn;
	// The server doesn't get its own OnRep call - drive the audio directly
	// here too, same as every other ReplicatedUsing property in this project.
	ApplyRadioState();
}

void ADCCRover::OnRep_RadioOn()
{
	ApplyRadioState();
}

void ADCCRover::ApplyRadioState()
{
	if (!RadioAudio)
	{
		return;
	}

	if (bRadioOn && !RadioTracks.IsEmpty() && RadioTracks[0])
	{
		// Always track 0 for now - there's exactly one sample track. Once
		// real music lands, this is the one line that grows into a "next
		// track" cycle rather than a rewrite.
		RadioAudio->SetSound(RadioTracks[0]);
		RadioAudio->Play();
	}
	else
	{
		RadioAudio->Stop();
	}
}

void ADCCRover::Tick(float DeltaSeconds)
{
	Super::Tick(DeltaSeconds);

	// Runs on every machine rendering this rover, driver or not - it is a
	// pure visual ease toward the replicated bWindowsDown, not gameplay.
	UpdateWindowOpacity(DeltaSeconds);

	// --- timers ------------------------------------------------------------
	if (bBoosting)
	{
		BoostSecondsRemaining -= DeltaSeconds;
		if (BoostSecondsRemaining <= 0.0f)
		{
			bBoosting = false;
			BoostCooldownRemaining = BoostCooldownSeconds;
		}
	}
	else if (BoostCooldownRemaining > 0.0f)
	{
		BoostCooldownRemaining = FMath::Max(0.0f, BoostCooldownRemaining - DeltaSeconds);
	}

	if (CrashRecoveryRemaining > 0.0f)
	{
		CrashRecoveryRemaining = FMath::Max(0.0f, CrashRecoveryRemaining - DeltaSeconds);
	}

	const bool bControllable = Driver != nullptr && CrashRecoveryRemaining <= 0.0f;

	if (bControllable)
	{
		// --- speed ramp ----------------------------------------------------
		// Throttle winds SpeedRamp up over SecondsToReachCruise rather than
		// jumping straight to top speed, so the rover accelerates instead of
		// simply being fast the instant a key goes down.
		const float RampRate = SecondsToReachCruise > 0.0f ? 1.0f / SecondsToReachCruise : 10.0f;
		if (!FMath::IsNearlyZero(ThrottleInput))
		{
			SpeedRamp = FMath::Clamp(SpeedRamp + RampRate * DeltaSeconds, 0.0f, 1.0f);
		}
		else
		{
			SpeedRamp = FMath::Clamp(SpeedRamp - RampRate * 1.6f * DeltaSeconds, 0.0f, 1.0f);
		}

		if (Movement)
		{
			// Boost overrides the ramp outright - that is what makes it read as
			// a shove rather than a slightly steeper acceleration curve.
			Movement->MaxSpeed = bBoosting
				? BoostSpeed
				: FMath::Max(600.0f, MaxCruiseSpeed * FMath::Max(SpeedRamp, 0.15f));
		}

		// --- steering ------------------------------------------------------
		const float Speed = GetVelocity().Size();
		const float SteerScale = FMath::Clamp(Speed / FMath::Max(1.0f, MaxCruiseSpeed * 0.30f), 0.0f, 1.0f);
		const float TurnRate = TurnRateDegreesPerSecond * (bDrifting ? DriftTurnMultiplier : 1.0f);

		if (!FMath::IsNearlyZero(SteerInput))
		{
			AddActorWorldRotation(FRotator(0.0f,
				SteerInput * TurnRate * SteerScale * DeltaSeconds, 0.0f));
		}

		// --- throttle ------------------------------------------------------
		if (!FMath::IsNearlyZero(ThrottleInput) || bBoosting)
		{
			const float Drive = bBoosting ? 1.0f : ThrottleInput;
			FVector Direction = GetActorForwardVector();

			// Drifting keeps part of the OLD velocity direction, so the car
			// slides wide through a turn instead of tracking its nose exactly.
			if (bDrifting && Speed > 200.0f)
			{
				const FVector Heading = GetVelocity().GetSafeNormal();
				Direction = FMath::Lerp(Direction, Heading, DriftSlide).GetSafeNormal();
			}

			AddMovementInput(Direction, Drive);
		}
	}
	else
	{
		SpeedRamp = FMath::Max(0.0f, SpeedRamp - DeltaSeconds);
	}

	// --- camera ------------------------------------------------------------
	if (CameraBoom)
	{
		// Eases back to centre once the mouse stops, so the view trails the car
		// rather than staying wherever it was last flicked.
		CameraYawTarget = FMath::FInterpTo(CameraYawTarget, 0.0f, DeltaSeconds, 1.4f);
		CameraYawOffset = FMath::FInterpTo(CameraYawOffset, CameraYawTarget, DeltaSeconds, 6.0f);
		CameraBoom->SetRelativeRotation(FRotator(-18.0f, CameraYawOffset, 0.0f));
	}

	// --- drift lean -----------------------------------------------------------
	if (Body)
	{
		const float TargetRoll = (bDrifting && !FMath::IsNearlyZero(SteerInput))
			? SteerInput * DriftBodyRollDegrees : 0.0f;
		const float NewRoll = FMath::FInterpTo(Body->GetRelativeRotation().Roll,
			TargetRoll, DeltaSeconds, 5.0f);
		Body->SetRelativeRotation(FRotator(0.0f, -90.0f, NewRoll));
	}

	// --- idle hover bob ---------------------------------------------------
	// "I do want it to slowly float" (Zoe, 2026-09-03) - a pure visual
	// offset on Body's own relative Z, not on the actor's world position, so
	// it never touches SnapToGround's ground-detection math at all. Safe
	// under Body's fixed -90 yaw: a component with no pitch/roll of its own
	// has local Z == world Z regardless of yaw, so this is exactly the same
	// as bobbing the mesh straight up and down.
	//
	// Fades out with speed rather than being constant - it should read as
	// "floating at rest", not as suspension wobble while actually driving,
	// which would fight the speed-tilt pitch above for attention.
	if (Body)
	{
		const UWorld* World = GetWorld();
		const float SpeedFraction = FMath::Clamp(GetVelocity().Size() / FMath::Max(1.0f, MaxCruiseSpeed), 0.0f, 1.0f);
		const float BobPeriod = FMath::Max(0.1f, HoverBobPeriodSeconds);
		const float Bob = World
			? FMath::Sin(World->GetTimeSeconds() * (2.0f * PI / BobPeriod)) * HoverBobAmplitude * (1.0f - SpeedFraction)
			: 0.0f;
		Body->SetRelativeLocation(FVector(0.0f, 0.0f, -90.0f + Bob));
	}

	// Flight has its own throttle - the ground ramp does not apply, and the
	// movement component's cap has to be raised or flying is capped at cruise.
	if (bFlying && Movement)
	{
		Movement->MaxSpeed = FlightSpeed;
		if (!FMath::IsNearlyZero(ThrottleInput))
		{
			AddMovementInput(GetActorForwardVector(), ThrottleInput);
		}
	}

	UpdateTrail();
	TickPilotingProgress(DeltaSeconds);
	SnapToGround(DeltaSeconds);
}

void ADCCRover::TickPilotingProgress(float DeltaSeconds)
{
	if (!HasAuthority() || !Driver)
	{
		return;
	}

	// Only time spent actually moving counts. Parking on the throttle against a
	// wall is not piloting practice.
	if (GetVelocity().Size() < PilotingMinSpeed)
	{
		return;
	}

	// Drifting is better practice than holding a straight line, so it earns the
	// interval faster. A crash clears the flag (see NotifyHit), which is what
	// makes "drift without crashing" the thing actually being rewarded.
	if (bDrifting)
	{
		bDriftedCleanlyThisInterval = true;
	}

	PilotingSecondsAccumulated += DeltaSeconds
		* (bDrifting ? CleanDriftTimeMultiplier : 1.0f);
	if (PilotingSecondsAccumulated < PilotingTickSeconds)
	{
		return;
	}
	PilotingSecondsAccumulated = 0.0f;

	// Marking is how every other skill in this project advances - the
	// mark-and-advance system in Skills/, where a mark makes the skill eligible
	// for its next 1d20-vs-Rank advancement roll. NOT an XP pool. The character
	// XP below is a separate, smaller nod so driving still pays in the moment.
	if (UDCCSkillComponent* PilotSkills = Driver->FindComponentByClass<UDCCSkillComponent>())
	{
		if (!PilotingSkillId.IsNone())
		{
			// A crawler who has never driven does not know Piloting, and marking
			// a skill you do not have does nothing. RecordUntrainedAttempt is the
			// component's own path for that case: it makes the skill a candidate
			// to be LEARNED outright at the next advancement roll. So the first
			// few drives teach you to drive, and every drive after that improves
			// it - which is exactly how the books describe picking a skill up.
			if (PilotSkills->GetSkillRank(PilotingSkillId) > 0)
			{
				PilotSkills->MarkSkillUsed(PilotingSkillId);
			}
			else
			{
				PilotSkills->RecordUntrainedAttempt(PilotingSkillId);
			}
		}
	}

	// No character XP here on purpose. Driving earns SKILL progress - a mark -
	// and the XP is paid once, when Piloting actually gains a Rank, centrally in
	// UDCCSkillComponent::RunSkillAdvancement. Paying per interval as well would
	// make circling a field an XP faucet.
	//
	// The clean-drift reward is handled by CleanDriftTimeMultiplier at the top of
	// this function, NOT by marking a second time: bAdvancementMark is a binary
	// flag, not a counter, so a second mark in the same interval is worth exactly
	// nothing. Earning the mark faster is the only honest lever the rules give.
	bDriftedCleanlyThisInterval = false;
}

void ADCCRover::AwardPilotingBonusSeconds(float BonusSeconds)
{
	if (!HasAuthority() || BonusSeconds <= 0.0f)
	{
		return;
	}
	// TickPilotingProgress consumes this the next time it runs at speed -
	// no separate mark call needed here, and none wanted: marking twice in
	// one interval is worth nothing (see that function's own comment), so
	// adding seconds is the only lever that actually pays out faster.
	PilotingSecondsAccumulated += BonusSeconds;
}

bool ADCCRover::IsLargerThanRover(const AActor* Other) const
{
	if (!Other)
	{
		return false;
	}

	FVector OtherOrigin, OtherExtent;
	Other->GetActorBounds(true, OtherOrigin, OtherExtent);

	// Compared on the horizontal footprint only. A long low wall should stop the
	// rover; a tall thin lamp post should not, and height alone is a poor proxy
	// for "is this something I can shove out of the way".
	const float OtherFootprint = FMath::Max(OtherExtent.X, OtherExtent.Y);
	const float SelfFootprint = Collision
		? FMath::Max(Collision->GetScaledBoxExtent().X, Collision->GetScaledBoxExtent().Y)
		: 320.0f;

	return OtherFootprint >= SelfFootprint * 0.9f;
}

void ADCCRover::NotifyHit(UPrimitiveComponent* MyComp, AActor* Other, UPrimitiveComponent* OtherComp,
	bool bSelfMoved, FVector HitLocation, FVector HitNormal, FVector NormalImpulse, const FHitResult& Hit)
{
	Super::NotifyHit(MyComp, Other, OtherComp, bSelfMoved, HitLocation, HitNormal,
		NormalImpulse, Hit);

	if (!Driver || !Movement)
	{
		return;
	}

	if (GetVelocity().Size() < 250.0f)
	{
		return;
	}

	// How high is the thing actually in the way, measured AT the contact point?
	//
	// This used to ask IsLargerThanRover(Other), which compared the whole
	// ACTOR's bounds - so clipping a 20-unit kerb belonging to a large floor
	// actor was judged identical to hitting a mountain, and the rover got a full
	// bounce plus half a second of dead controls. That is why it could not leave
	// the starting arena: every edge in the room read as a wall.
	const float ObstacleTop = MeasureObstacleTop(Hit);
	const float FloorZ = GetActorLocation().Z - RideHeight;
	const float ObstacleHeight = ObstacleTop - FloorZ;

	if (ObstacleHeight <= MaxStepHeight)
	{
		// A lip, a kerb, a bump. A hovering vehicle should ride up over this, so
		// it is handed to the step-over logic rather than treated as an impact.
		// Speed is barely scrubbed - being slowed to a crawl by a doorstep is
		// exactly the feel being removed here.
		PendingStepUpZ = FMath::Max(PendingStepUpZ, ObstacleTop + RideHeight);
		return;
	}

	// A real wall. Stop, shove gently back off it, and take the controls away
	// for a moment so it reads as an impact rather than as the car sticking to
	// the surface with the throttle still buried.
	Movement->Velocity = HitNormal * BounceBackSpeed;
	SpeedRamp = 0.0f;
	CrashRecoveryRemaining = CrashRecoverySeconds;
	bBoosting = false;
	BoostSecondsRemaining = 0.0f;

	// A crash spoils the clean-drift bonus. That is the entire risk side of
	// drifting for skill progress.
	bDriftedCleanlyThisInterval = false;
}

float ADCCRover::MeasureObstacleTop(const FHitResult& Hit) const
{
	UWorld* World = GetWorld();
	if (!World)
	{
		return Hit.ImpactPoint.Z;
	}

	// Trace down just BEYOND the surface that was hit, so the probe lands on top
	// of the obstruction rather than back on its face. Without the nudge past
	// the contact point the trace re-hits the same wall and every step reads as
	// zero height.
	const FVector Probe = Hit.ImpactPoint - Hit.ImpactNormal * 40.0f;
	const FVector Start(Probe.X, Probe.Y, GetActorLocation().Z + MaxStepHeight + 200.0f);
	const FVector End(Probe.X, Probe.Y, GetActorLocation().Z - RideHeight - 200.0f);

	FHitResult TopHit;
	FCollisionQueryParams Params(SCENE_QUERY_STAT(RoverStep), false, this);
	if (Driver)
	{
		Params.AddIgnoredActor(Driver);
	}

	if (World->LineTraceSingleByChannel(TopHit, Start, End, ECC_Visibility, Params))
	{
		return TopHit.ImpactPoint.Z;
	}

	// Nothing on top of it within reach: treat it as tall, i.e. a wall.
	return Start.Z;
}

void ADCCRover::SnapToGround(float DeltaSeconds)
{
	// Kept as a thin forwarder so nothing outside has to care that the ground
	// handling grew into slopes, air time and flight.
	UpdateGroundAndAir(DeltaSeconds);
}

void ADCCRover::UpdateGroundAndAir(float DeltaSeconds)
{
	UWorld* World = GetWorld();
	if (!World)
	{
		return;
	}

	const FVector Origin = GetActorLocation();

	FHitResult Hit;
	FCollisionQueryParams Params(SCENE_QUERY_STAT(RoverGround), false, this);
	if (Driver)
	{
		Params.AddIgnoredActor(Driver);
	}

	// Traces from above the body so a rover that has clipped slightly into a
	// rising surface still finds that surface rather than the far side of it.
	const FVector Start = Origin + FVector(0.0f, 0.0f, 300.0f);
	const FVector End = Origin - FVector(0.0f, 0.0f, 4000.0f);
	const bool bFoundGround = World->LineTraceSingleByChannel(Hit, Start, End, ECC_Visibility, Params);

	// --- flying: no ground following at all --------------------------------
	if (bFlying)
	{
		if (FlightDurationSeconds > 0.0f)
		{
			FlightSecondsRemaining -= DeltaSeconds;
			if (FlightSecondsRemaining <= 0.0f)
			{
				EndFlight();
			}
		}

		// Ease the visual/mechanical nose angle toward wherever the flap lever
		// is set, rather than snapping - a sudden pitch flip reads as a glitch.
		const float TargetPitch = FlightPitchInput * MaxFlightPitchDegrees;
		FlightPitchAngleDegrees = FMath::FInterpTo(FlightPitchAngleDegrees, TargetPitch, DeltaSeconds, FlightPitchAlignSpeed);

		// sin() so a shallow drag barely changes altitude and full deflection
		// really matters, same shape as a real elevator. Additive with
		// Space/Ctrl rather than replacing them - the flap is the fine
		// control, the keys are still there for a snap override.
		const float PitchClimb = FMath::Sin(FMath::DegreesToRadians(FlightPitchAngleDegrees)) * FlightPitchLiftRate;
		VerticalVelocity = VerticalInput * FlightClimbRate + PitchClimb;
		SetActorLocation(Origin + FVector(0.0f, 0.0f, VerticalVelocity * DeltaSeconds), true);

		// Touching down ends it, so flight resolves by landing rather than by
		// the player having to remember a second key.
		if (bFoundGround && (Origin.Z - Hit.Location.Z) <= RideHeight + GroundedTolerance
			&& VerticalInput <= 0.0f)
		{
			EndFlight();
		}

		AlignToSurface(DeltaSeconds);
		return;
	}

	if (!bFoundGround)
	{
		// Nothing beneath at all - fall, and keep tracking the drop so flight
		// can still be earned over a chasm.
		bAirborne = true;
		VerticalVelocity -= Gravity * DeltaSeconds;
		ApexZ = FMath::Max(ApexZ, Origin.Z);
		FallenDistance = FMath::Max(0.0f, ApexZ - Origin.Z);
		SetActorLocation(Origin + FVector(0.0f, 0.0f, VerticalVelocity * DeltaSeconds), true);
		AlignToSurface(DeltaSeconds);
		return;
	}

	LastGroundNormal = Hit.ImpactNormal;

	const float TargetZ = Hit.Location.Z + RideHeight;
	const float HeightAboveTarget = Origin.Z - TargetZ;

	if (HeightAboveTarget > GroundedTolerance)
	{
		// --- airborne ------------------------------------------------------
		bAirborne = true;
		VerticalVelocity -= Gravity * DeltaSeconds;

		float NewZ = Origin.Z + VerticalVelocity * DeltaSeconds;
		if (NewZ <= TargetZ)
		{
			// Landed.
			NewZ = TargetZ;
			VerticalVelocity = 0.0f;
			bAirborne = false;
			ApexZ = NewZ;
			FallenDistance = 0.0f;
		}
		else
		{
			ApexZ = FMath::Max(ApexZ, Origin.Z);
			FallenDistance = FMath::Max(0.0f, ApexZ - Origin.Z);
		}

		SetActorLocation(FVector(Origin.X, Origin.Y, NewZ), true);
	}
	else
	{
		// --- on the ground -------------------------------------------------
		bAirborne = false;
		VerticalVelocity = 0.0f;
		ApexZ = Origin.Z;
		FallenDistance = 0.0f;

		// Look ahead so a kerb or step is climbed BEFORE the box reaches it.
		// Reacting only on contact means the sweep has already been stopped by
		// the time anything lifts, which is how a hover car ends up pinned
		// against a doorstep.
		float EffectiveTarget = TargetZ;
		const float AheadZ = ProbeGroundAhead();
		if (AheadZ > TargetZ && (AheadZ - TargetZ) <= MaxStepHeight)
		{
			EffectiveTarget = AheadZ + RideHeight;
		}

		// A contact-time step request (NotifyHit) always wins, since that is a
		// surface actually in the way right now.
		if (PendingStepUpZ > EffectiveTarget)
		{
			EffectiveTarget = PendingStepUpZ;
		}
		PendingStepUpZ = 0.0f;

		// THIS is what makes ramps work. Rising ground is followed IMMEDIATELY,
		// because easing upward lets the nose sink into the slope until the
		// collision box catches it and the rover simply stops dead at the foot
		// of the ramp. Falling ground is still eased, so cresting a rise does
		// not drop the car like a stone.
		float NewZ;
		if (EffectiveTarget > Origin.Z)
		{
			// Climbing a step is deliberately RATE-LIMITED rather than snapped:
			// hopping instantly onto a ledge looks like teleporting, and Zoe
			// asked for it to crawl over things.
			const float MaxRise = StepClimbSpeed * DeltaSeconds;
			NewZ = FMath::Min(EffectiveTarget, Origin.Z + FMath::Max(MaxRise, 0.0f));
			bClimbingStep = (EffectiveTarget - Origin.Z) > 12.0f;
		}
		else
		{
			NewZ = FMath::FInterpTo(Origin.Z, EffectiveTarget, DeltaSeconds, 10.0f);
			bClimbingStep = false;
		}

		SetActorLocation(FVector(Origin.X, Origin.Y, NewZ), true);
	}

	AlignToSurface(DeltaSeconds);
}

float ADCCRover::ProbeGroundAhead() const
{
	UWorld* World = GetWorld();
	if (!World)
	{
		return -BIG_NUMBER;
	}

	const FVector Ahead = GetActorLocation()
		+ GetActorForwardVector() * ForwardProbeDistance;

	const FVector Start(Ahead.X, Ahead.Y, GetActorLocation().Z + MaxStepHeight + 200.0f);
	const FVector End(Ahead.X, Ahead.Y, GetActorLocation().Z - RideHeight - 400.0f);

	FHitResult Hit;
	FCollisionQueryParams Params(SCENE_QUERY_STAT(RoverAhead), false, this);
	if (Driver)
	{
		Params.AddIgnoredActor(Driver);
	}

	if (World->LineTraceSingleByChannel(Hit, Start, End, ECC_Visibility, Params))
	{
		return Hit.ImpactPoint.Z;
	}
	return -BIG_NUMBER;
}

void ADCCRover::AlignToSurface(float DeltaSeconds)
{
	// Airborne or on a surface too steep to be a road, level out instead of
	// matching it - otherwise the body flips to vertical against a wall.
	const float SlopeDegrees = FMath::RadiansToDegrees(
		FMath::Acos(FMath::Clamp(LastGroundNormal.Z, -1.0f, 1.0f)));

	FRotator Desired = GetActorRotation();
	if (bFlying)
	{
		// The eased flap angle IS the visual nose pitch here, not a separate
		// cosmetic tilt - what you see is exactly what SnapToGround is using
		// to climb or dive. No roll while flying; the flap lever only asked
		// for pitch.
		Desired.Pitch = FlightPitchAngleDegrees;
		Desired.Roll = 0.0f;
	}
	else if (bClimbingStep)
	{
		// Nose up while crawling over something, so it reads as heaving itself
		// over the lip rather than levitating flatly across it.
		Desired.Pitch = StepClimbPitchDegrees;
		Desired.Roll = 0.0f;
	}
	else if (!bAirborne && SlopeDegrees <= MaxClimbAngleDegrees)
	{
		// Pitch/roll from the surface, yaw kept from the actor so aligning never
		// steers the car.
		const FRotator SurfaceRot = FRotationMatrix::MakeFromZX(
			LastGroundNormal, GetActorForwardVector()).Rotator();

		// "Shift up and down in angle with speed" (Zoe, 2026-09-03) - nose
		// lifts as speed climbs, on top of whatever the surface itself
		// already asks for. Positive Pitch reads as nose-up in this
		// codebase's own convention (see StepClimbPitchDegrees above), so
		// this is additive, not a replacement - a rover already pitched by
		// a slope keeps that pitch and lifts further from it.
		const float SpeedFraction = FMath::Clamp(GetVelocity().Size() / FMath::Max(1.0f, MaxCruiseSpeed), 0.0f, 1.0f);
		Desired.Pitch = SurfaceRot.Pitch + SpeedFraction * MaxSpeedTiltDegrees;
		Desired.Roll = SurfaceRot.Roll;
	}
	else
	{
		Desired.Pitch = 0.0f;
		Desired.Roll = 0.0f;
	}

	const FRotator Current = GetActorRotation();
	const FRotator Smoothed(
		FMath::FInterpTo(Current.Pitch, Desired.Pitch, DeltaSeconds, SurfaceAlignSpeed),
		Current.Yaw,
		FMath::FInterpTo(Current.Roll, Desired.Roll, DeltaSeconds, SurfaceAlignSpeed));
	SetActorRotation(Smoothed);
}

bool ADCCRover::CanEngageFlight() const
{
	// Airborne, already committed to a real drop, and with boost available -
	// all three, so flight is a reward for a good launch rather than a hover
	// button.
	return !bFlying
		&& bAirborne
		&& FallenDistance >= FlightUnlockFallDistance
		&& (bBoosting || BoostCooldownRemaining <= 0.0f);
}

void ADCCRover::EngageFlight()
{
	bFlying = true;
	bAirborne = false;
	VerticalVelocity = 0.0f;
	FlightSecondsRemaining = FlightDurationSeconds;

	// Flight consumes the boost outright, so it cannot be chained straight into
	// another one on landing.
	bBoosting = false;
	BoostSecondsRemaining = 0.0f;
	BoostCooldownRemaining = BoostCooldownSeconds;

	// "Custom" per DCCQuestTypes.h's own convention for an event nothing
	// else needed a dedicated ObjectiveType for - a dev-goal quest ("get
	// airborne for the first time") is exactly that.
	if (Driver)
	{
		UDCCQuestComponent::NotifyEvent(Driver, EDCCObjectiveType::Custom, TEXT("FlightEngaged"), 1);
	}
}

void ADCCRover::EndFlight()
{
	bFlying = false;
	FlightSecondsRemaining = 0.0f;
	VerticalVelocity = 0.0f;
	ApexZ = GetActorLocation().Z;
	FallenDistance = 0.0f;

	// Flaps retract on landing - the next flight starts level rather than
	// carrying over whatever angle the last one ended on.
	FlightPitchInput = 0.0f;
	FlightPitchAngleDegrees = 0.0f;
}

void ADCCRover::BoardNearbyAllies()
{
	UWorld* World = GetWorld();
	if (!HasAuthority() || !World || !Driver)
	{
		return;
	}

	for (TActorIterator<ADCCCharacterBase> It(World); It; ++It)
	{
		if (Passengers.Num() >= MaxPassengers)
		{
			break;
		}

		ADCCCharacterBase* Candidate = *It;
		if (!Candidate || Candidate == Driver || !Candidate->IsAlive())
		{
			continue;
		}

		// Allies only. IsCrawler() is what distinguishes a friendly from a mob -
		// it derives from bIsAlly, so this cannot accidentally load the car with
		// Debt Eaters.
		if (!Candidate->IsCrawler() || Candidate->IsPlayerControlled())
		{
			continue;
		}

		if (Passengers.Contains(Candidate))
		{
			continue;
		}

		if (FVector::Dist(GetActorLocation(), Candidate->GetActorLocation()) > PassengerBoardRadius)
		{
			continue;
		}

		Passengers.Add(Candidate);
		Candidate->SetActorEnableCollision(false);
		Candidate->SetActorHiddenInGame(true);
		Candidate->AttachToComponent(Collision,
			FAttachmentTransformRules::SnapToTargetNotIncludingScale);
	}

	ApplyPassengerState();
}

void ADCCRover::EjectPassengers()
{
	if (!HasAuthority() || Passengers.IsEmpty())
	{
		return;
	}

	// Fanned out to one side rather than dropped on one point, or four allies
	// materialise inside each other and get shoved somewhere unpredictable.
	int32 Index = 0;
	for (ADCCCharacterBase* Passenger : Passengers)
	{
		if (!Passenger)
		{
			continue;
		}

		Passenger->DetachFromActor(FDetachmentTransformRules::KeepWorldTransform);
		Passenger->SetActorHiddenInGame(false);
		Passenger->SetActorEnableCollision(true);

		const FVector Offset = GetActorRightVector() * (-360.0f - Index * 200.0f)
			+ FVector(0.0f, 0.0f, 40.0f);
		Passenger->SetActorLocation(GetActorLocation() + Offset, false, nullptr,
			ETeleportType::TeleportPhysics);
		++Index;
	}

	Passengers.Reset();
}

void ADCCRover::ApplyPassengerState()
{
	// Runs on every machine - a remote client has to see them disappear into the
	// car too, not just the server.
	for (ADCCCharacterBase* Passenger : Passengers)
	{
		if (Passenger)
		{
			Passenger->SetActorHiddenInGame(true);
		}
	}
}

void ADCCRover::EjectPressed()
{
	if (HasAuthority())
	{
		EjectPassengers();
	}
	else if (Driver)
	{
		Driver->Server_EjectRoverPassengers(this);
	}
}

void ADCCRover::UpdateTrail()
{
	if (!BoostTrail)
	{
		return;
	}

	const bool bWanted = (bBoosting || bFlying) && Driver != nullptr;
	if (bWanted != BoostTrail->IsActive())
	{
		if (bWanted) { BoostTrail->Activate(true); }
		else         { BoostTrail->Deactivate(); }
	}

	// NS_Flying_car was authored for a different vehicle's own Blueprint,
	// which very likely fed it a Speed/Velocity User Parameter every tick to
	// drive spawn rate - a common pattern for exhaust/wake trails, and the
	// likely reason Activate() alone reported "not working": the system can
	// be genuinely active and still emit nothing if the parameter it scales
	// off is never set. Fed defensively under both common naming
	// conventions; SetVariableFloat/Vec3 on a name the system does not
	// expose is a safe no-op, not an error, so this costs nothing if the
	// actual bug turns out to be something else.
	if (bWanted)
	{
		const float Speed = GetVelocity().Size();
		BoostTrail->SetVariableFloat(TEXT("Speed"), Speed);
		BoostTrail->SetVariableVec3(TEXT("Velocity"), GetVelocity());
	}
}

void ADCCRover::EnterRover(ADCCCharacterBase* NewDriver)
{
	if (!HasAuthority() || Driver || !NewDriver)
	{
		return;
	}

	APlayerController* PC = Cast<APlayerController>(NewDriver->GetController());
	if (!PC)
	{
		return;
	}

	// The "getting in" beat: a quick animation while still standing beside
	// the rover, THEN disappear - rather than an instant teleport-and-hide.
	// Multicast so every machine sees it, not just the driver's own. Rooted
	// for the duration via the same shared movement lock a skill cast and
	// the loot box reveal already use, so nobody walks halfway through
	// their own entrance.
	//
	// CanPlayOnThisSkeleton is checked HERE, before committing to a delay -
	// not left to PlayOneShotAnimation's own silent no-op. A driver on a
	// skeleton the clip doesn't match (the default player mesh is the raw
	// Mannequin, not any crawler skeleton - confirmed live, not assumed)
	// would otherwise still get locked for the clip's full length while
	// nothing plays: "stuck for several seconds, then pops in," Zoe's exact
	// report, and a real bug in this function, not a cosmetic one.
	float Delay = 0.0f;
	if (NewDriver->CanPlayOnThisSkeleton(NewDriver->EnterVehicleAnimation))
	{
		if (const UAnimSequenceBase* Sequence = Cast<UAnimSequenceBase>(NewDriver->EnterVehicleAnimation))
		{
			Delay = FMath::Max(0.0f, Sequence->GetPlayLength());
		}
	}

	if (Delay <= 0.0f)
	{
		FinishEnteringRover(NewDriver);
		return;
	}

	NewDriver->Multicast_PlayEnterVehicleAnimation();

	NewDriver->BeginCastLock(Delay);

	TWeakObjectPtr<ADCCRover> WeakThis(this);
	TWeakObjectPtr<ADCCCharacterBase> WeakDriver(NewDriver);
	FTimerHandle EnterRoverTimer;
	GetWorldTimerManager().SetTimer(EnterRoverTimer, FTimerDelegate::CreateLambda([WeakThis, WeakDriver]()
	{
		if (ADCCRover* StrongThis = WeakThis.Get())
		{
			if (ADCCCharacterBase* StrongDriver = WeakDriver.Get())
			{
				StrongThis->FinishEnteringRover(StrongDriver);
			}
		}
	}), Delay, false);
}

void ADCCRover::FinishEnteringRover(ADCCCharacterBase* NewDriver)
{
	// Re-checked, not assumed - the beat above takes real time, and the
	// driver could have died or the rover could have been claimed by
	// someone else in the meantime.
	if (!HasAuthority() || Driver || !NewDriver || !NewDriver->IsAlive())
	{
		return;
	}

	APlayerController* PC = Cast<APlayerController>(NewDriver->GetController());
	if (!PC)
	{
		return;
	}

	Driver = NewDriver;

	// The crawler rides along attached and hidden rather than being destroyed
	// or teleported away, so everything hanging off them - inventory, stats,
	// quest state, the carried Strongbox - simply continues to exist.
	Driver->SetActorEnableCollision(false);
	Driver->SetActorHiddenInGame(true);
	Driver->AttachToComponent(Collision, FAttachmentTransformRules::SnapToTargetNotIncludingScale);

	PC->Possess(this);
	ApplyDriverState();

	// Allies climb in with the driver rather than being left standing in
	// the road watching the car leave.
	BoardNearbyAllies();
}

void ADCCRover::ExitRover()
{
	if (!HasAuthority() || !Driver)
	{
		return;
	}

	ADCCCharacterBase* Leaving = Driver;
	Driver = nullptr;

	APlayerController* PC = Cast<APlayerController>(GetController());

	Leaving->DetachFromActor(FDetachmentTransformRules::KeepWorldTransform);
	Leaving->SetActorHiddenInGame(false);
	Leaving->SetActorEnableCollision(true);

	// Put them down beside the rover rather than inside it, or they spawn
	// intersecting the body and get shoved somewhere unpredictable.
	const FVector DropAt = GetActorLocation() + GetActorRightVector() * 320.0f
		+ FVector(0.0f, 0.0f, 40.0f);
	Leaving->SetActorLocation(DropAt, false, nullptr, ETeleportType::TeleportPhysics);

	if (PC)
	{
		PC->Possess(Leaving);
	}

	// Everyone gets out together - a rover parked with four invisible allies
	// still attached to it is a very hard bug to notice.
	EjectPassengers();

	ThrottleInput = 0.0f;
	SteerInput = 0.0f;
	ApplyDriverState();
}

void ADCCRover::OnRep_Driver()
{
	ApplyDriverState();
}

void ADCCRover::ApplyDriverState()
{
	// Runs on every machine: a remote client has to see the driver vanish into
	// the rover too, not just the server.
	if (Driver)
	{
		Driver->SetActorHiddenInGame(true);
	}
}

void ADCCRover::TryPlayerInteract(ADCCCharacterBase* Interactor)
{
	if (!Interactor)
	{
		return;
	}

	if (IsOccupied())
	{
		Interactor->ShowLocalNotice(NSLOCTEXT("DCC", "RoverOccupied",
			"Somebody is already in it."));
		return;
	}

	if (HasAuthority())
	{
		EnterRover(Interactor);
	}
	else
	{
		Interactor->Server_EnterRover(this);
	}
}
