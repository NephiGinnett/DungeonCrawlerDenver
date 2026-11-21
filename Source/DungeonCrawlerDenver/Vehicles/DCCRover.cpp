#include "Vehicles/DCCRover.h"
#include "Arcade/DCCSpray.h"
#include "Components/PointLightComponent.h"
#include "Components/SpotLightComponent.h"

#include "Arcade/DCCArcadeArena.h"
#include "Dialogue/DCCVoiceLines.h"
#include "Arcade/DCCArcadeGames.h"
#include "Arcade/DCCChopShop.h"
#include "Arcade/DCCRallyItems.h"
#include "Arcade/DCCWalletComponent.h"
#include "Characters/DCCCharacterBase.h"
#include "UI/DCCHUD.h"
#include "Components/AudioComponent.h"
#include "Components/BoxComponent.h"
#include "Components/StaticMeshComponent.h"
#include "GameFramework/SpringArmComponent.h"
#include "Camera/CameraComponent.h"
#include "GameFramework/FloatingPawnMovement.h"
#include "GameFramework/GameStateBase.h"
#include "GameFramework/PlayerController.h"
#include "Engine/World.h"
#include "EngineUtils.h"
#include "Net/UnrealNetwork.h"
#include "Quests/DCCQuestComponent.h"
#include "Materials/MaterialInstanceDynamic.h"
#include "NiagaraComponent.h"
#include "NiagaraFunctionLibrary.h"
#include "NiagaraSystem.h"
#include "Skills/DCCSkillComponent.h"
#include "Stats/DCCStatsComponent.h"
#include "UObject/ConstructorHelpers.h"
#include "Math/RotationMatrix.h"
#include "CollisionQueryParams.h"

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

	TrafficShell = CreateDefaultSubobject<UBoxComponent>(TEXT("TrafficShell"));
	TrafficShell->SetupAttachment(Collision);
	TrafficShell->SetBoxExtent(FVector(310.0f, 170.0f, 85.0f));
	TrafficShell->SetCollisionObjectType(ECC_WorldDynamic);
	TrafficShell->SetCollisionResponseToAllChannels(ECR_Ignore);
	TrafficShell->SetCollisionResponseToChannel(ECC_Pawn, ECR_Block);
	TrafficShell->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	TrafficShell->SetGenerateOverlapEvents(false);

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

	// KMHR's playlist: every music track the project actually has. Repo Rally
	// is first so a rally tune-in (TuneIn(0)) lands on it. ConstructorHelpers
	// resolves at startup; a missing asset is skipped and self-heals next launch.
	{
		static ConstructorHelpers::FObjectFinder<USoundBase> RepoRally(
			TEXT("/Game/Songs/Repo_Rally.Repo_Rally"));
		if (RepoRally.Succeeded())
		{
			RadioTracks.Add(RepoRally.Object);
			RadioTrackNames.Add(TEXT("REPO RALLY"));
		}
	}
	{
		static ConstructorHelpers::FObjectFinder<USoundBase> Freaks(
			TEXT("/Game/DCC/Audio/Freaks.Freaks"));
		if (Freaks.Succeeded())
		{
			RadioTracks.Add(Freaks.Object);
			RadioTrackNames.Add(TEXT("FREAKS"));
		}
	}
	{
		static ConstructorHelpers::FObjectFinder<USoundBase> SweptAway(
			TEXT("/Game/Songs/Swept_Away.Swept_Away"));
		if (SweptAway.Succeeded())
		{
			RadioTracks.Add(SweptAway.Object);
			RadioTrackNames.Add(TEXT("SWEPT AWAY"));
		}
	}
	{
		static ConstructorHelpers::FObjectFinder<USoundBase> FlyUp(
			TEXT("/Game/SoulCity/Sound/Cue/Music/Soul_Music_FlyUp01_Cue.Soul_Music_FlyUp01_Cue"));
		if (FlyUp.Succeeded())
		{
			RadioTracks.Add(FlyUp.Object);
			RadioTrackNames.Add(TEXT("FLY UP"));
		}
	}
	{
		static ConstructorHelpers::FObjectFinder<USoundBase> Whoops(
			TEXT("/Game/Songs/Meet_the_Whoops.Meet_the_Whoops"));
		if (Whoops.Succeeded())
		{
			RadioTracks.Add(Whoops.Object);
			RadioTrackNames.Add(TEXT("MEET THE WHOOPS"));
		}
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
	// 2026-11-14: a little heavier, and the throttle has to catch the truck instead of
	// gliding it up to speed. Deceleration is the coast: shorter than the old 900,
	// still a real slide when you lift.
	Movement->Acceleration = 2100.0f;
	Movement->Deceleration = 1350.0f;
}

void ADCCRover::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);
	DOREPLIFETIME(ADCCRover, Driver);
	DOREPLIFETIME(ADCCRover, Passengers);
	DOREPLIFETIME(ADCCRover, bWindowsDown);
	DOREPLIFETIME(ADCCRover, bRadioOn);
	DOREPLIFETIME(ADCCRover, AutoPilotWaypointIndex);
	DOREPLIFETIME(ADCCRover, AutoPilotLapsCompleted);
	DOREPLIFETIME(ADCCRover, RaceSpeedScale);
	DOREPLIFETIME(ADCCRover, RaceAbsorb);
	DOREPLIFETIME_CONDITION(ADCCRover, RallyTools, COND_OwnerOnly);
	DOREPLIFETIME_CONDITION(ADCCRover, RallyToolIndex, COND_OwnerOnly);
	DOREPLIFETIME_CONDITION(ADCCRover, RallyStatus, COND_OwnerOnly);
	DOREPLIFETIME_CONDITION(ADCCRover, RallyCargo, COND_OwnerOnly);
	DOREPLIFETIME(ADCCRover, RallyEngine);
	DOREPLIFETIME(ADCCRover, RallyBoostExtra);
	DOREPLIFETIME(ADCCRover, RallyBody);
	DOREPLIFETIME(ADCCRover, RallyDuty);
	DOREPLIFETIME(ADCCRover, RallyPlate);
	DOREPLIFETIME(ADCCRover, RallyStrain);
	DOREPLIFETIME(ADCCRover, bRallyHeld);
	DOREPLIFETIME(ADCCRover, RallyTow);
	DOREPLIFETIME(ADCCRover, bRallyTowDocked);
	DOREPLIFETIME(ADCCRover, bRallyInYard);
	DOREPLIFETIME(ADCCRover, RallyTrick);
	DOREPLIFETIME(ADCCRover, RallyBalance);
	DOREPLIFETIME(ADCCRover, RallyMagnet);
	DOREPLIFETIME(ADCCRover, RallySpin);
	DOREPLIFETIME(ADCCRover, RallyTilt);
	DOREPLIFETIME(ADCCRover, RallyTrickScore);
	DOREPLIFETIME(ADCCRover, RallyTrickMul);
	DOREPLIFETIME(ADCCRover, RallyTrickChain);
	DOREPLIFETIME(ADCCRover, RallyTrickMove);
	DOREPLIFETIME(ADCCRover, RallyClampUntil);
	DOREPLIFETIME(ADCCRover, RallyHookRange);
	DOREPLIFETIME_CONDITION(ADCCRover, RallyFuel, COND_OwnerOnly);
	DOREPLIFETIME(ADCCRover, RadioTrackIndex);
	DOREPLIFETIME(ADCCRover, RadioStartOffset);
	DOREPLIFETIME(ADCCRover, RadioTuneSerial);
	DOREPLIFETIME(ADCCRover, bWrecker);
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
	// W/A/S/D/Space/Left Ctrl are NOT bound as Pressed/Released pairs here -
	// see the comment on the ThrottleInput/SteerInput/VerticalInput poll in
	// Tick() for why that shape was the actual bug behind "the car starts
	// moving backward uncontrollably or constantly spins until they get out."
	PlayerInputComponent->BindKey(EKeys::LeftShift, IE_Pressed,  this, &ADCCRover::DriftPressed);
	PlayerInputComponent->BindKey(EKeys::LeftShift, IE_Released, this, &ADCCRover::DriftReleased);
	// Boost lives on left click now, not Space - Zoe, 2026-09-04: Space was
	// doing double duty as both the continuous climb key and (after the
	// 2026-09-04 flying-boost fix) a boost trigger, which read as "getting
	// mixed up with the climb button." One press, one job: LMB is boost,
	// full stop, on the ground or in the air. Space keeps flight-engage
	// (a one-shot edge, still detected in Tick()) and climb (continuous).
	PlayerInputComponent->BindKey(EKeys::LeftMouseButton, IE_Pressed,  this, &ADCCRover::BoostPressed);
	PlayerInputComponent->BindKey(EKeys::X, IE_Pressed,  this, &ADCCRover::EjectPressed);
	PlayerInputComponent->BindKey(EKeys::E, IE_Pressed,  this, &ADCCRover::InputExit);
	PlayerInputComponent->BindKey(EKeys::R, IE_Pressed,  this, &ADCCRover::WindowTogglePressed);
	PlayerInputComponent->BindKey(EKeys::T, IE_Pressed,  this, &ADCCRover::RadioTogglePressed);
	// C changes the station while it is already on. On foot, C is still the selfie.
	PlayerInputComponent->BindKey(EKeys::C, IE_Pressed,  this, &ADCCRover::RadioChannelPressed);
	// Repo Rally's kit. [T] is already the radio, so [F] switches tools.
	PlayerInputComponent->BindKey(EKeys::Q, IE_Pressed, this, &ADCCRover::RallyToolPressed);
	PlayerInputComponent->BindKey(EKeys::F, IE_Pressed, this, &ADCCRover::RallyCyclePressed);
	// Repo Rally's hook camera. Right mouse is free on the rover (left mouse is boost).
	PlayerInputComponent->BindKey(EKeys::RightMouseButton, IE_Pressed, this, &ADCCRover::HookAimPressed);
	// The click-and-drag flap lever (LMB + MouseY) is gone - Zoe, 2026-09-04:
	// "let's remove the click and drag aspect of flying, I don't think it's
	// working great." Flying pitch is now just a fixed level attitude; climb
	// rate comes purely from Space/Ctrl (VerticalInput), no elevator lift on
	// top of it. LMB is free for boost as a result.

	// Mouse X swings the chase camera rather than steering, so a driver can
	// look into a corner without changing where the car is pointed.
	// Mouse Y only pitches while the hook camera is up. The chase view stays at -18.
	PlayerInputComponent->BindAxisKey(EKeys::MouseX, this, &ADCCRover::CameraLook);
	PlayerInputComponent->BindAxisKey(EKeys::MouseY, this, &ADCCRover::CameraPitchLook);
}

void ADCCRover::DriftPressed()
{
	// Overhead, Shift is a barrel roll. The tick poll sends it. Drift would roll the truck under the magnet.
	if (RallyTrick == 2)
	{
		return;
	}
	bDrifting = true;
}
void ADCCRover::DriftReleased() { bDrifting = false; }

void ADCCRover::BoostPressed()
{
	// Overhead, left click is a corkscrew. The tick poll sends it.
	if (RallyTrick == 2)
	{
		return;
	}
	// A dedicated trigger now - see the LMB binding comment in
	// SetupPlayerInputComponent for why this is no longer sharing Space with
	// climb/flight-engage. CanEngageFlight()/EngageFlight() moved to the
	// Space edge-detection in Tick(), since that's a one-shot action
	// unrelated to boosting.
	if (bBoosting || BoostCooldownRemaining > 0.0f || CrashRecoveryRemaining > 0.0f)
	{
		return;
	}
	bBoosting = true;
	BoostSecondsRemaining = BoostDurationSeconds + RallyBoostExtra;
}

void ADCCRover::GrantBoost()
{
	if (CrashRecoveryRemaining > 0.0f)
	{
		// Still reeling from a crash - a reward boost that shoved the
		// player straight into the wall they just hit would read as a
		// punishment, not a reward.
		return;
	}

	bBoosting = true;
	BoostSecondsRemaining = BoostDurationSeconds + RallyBoostExtra;
	BoostCooldownRemaining = 0.0f;
}

void ADCCRover::SetRallyBody(int32 Index)
{
	RallyBody = FMath::Clamp(Index, 0, 4);
	OnRep_RallyBody();
}

void ADCCRover::OnRep_RallyBody()
{
	// The wrecker mesh is not one of the five car bodies. A body swap would put the car back.
	if (bWrecker || RallyBody < 0 || !Body)
	{
		return;
	}
	const TCHAR* Meshes[] = {
		TEXT("/Game/Synthwave_city/Meshes/Car/SM_car_01.SM_car_01"),
		TEXT("/Game/Synthwave_city/Meshes/Car/SM_car_02.SM_car_02"),
		TEXT("/Game/Synthwave_city/Meshes/Car/SM_car_B.SM_car_B"),
		TEXT("/Game/Synthwave_city/Meshes/Car/SM_car_B1.SM_car_B1"),
		TEXT("/Game/Synthwave_city/Meshes/Car/SM_car_B2.SM_car_B2"),
	};
	if (UStaticMesh* Mesh = LoadObject<UStaticMesh>(nullptr, Meshes[FMath::Clamp(RallyBody, 0, 4)]))
	{
		Body->SetStaticMesh(Mesh);
		// The window fade was a material on the old mesh. The next opacity tick rebuilds it.
		WindowMID = nullptr;
	}
}

void ADCCRover::SetRacePace(float Scale)
{
	RaceSpeedScale = FMath::Clamp(Scale, 0.2f, 2.2f);
	// A hard snare (a net, a limp, a clip) cuts a boost. The city's lower gearing stays above this,
	// so a truck can still boost without the pace wipe cancelling it.
	if (RaceSpeedScale < 0.40f)
	{
		bBoosting = false;
		BoostSecondsRemaining = 0.0f;
	}
}

bool ADCCRover::TryAbsorb()
{
	if (RaceAbsorb <= 0)
	{
		return false;
	}
	if (HasAuthority())
	{
		--RaceAbsorb;
	}
	return true;
}

void ADCCRover::Stumble(float Seconds)
{
	if (!HasAuthority() || !Movement)
	{
		return;
	}
	Movement->Velocity *= 0.15f;
	SpeedRamp = 0.0f;
	CrashRecoveryRemaining = FMath::Max(CrashRecoveryRemaining, Seconds);
	bBoosting = false;
	BoostSecondsRemaining = 0.0f;
}

void ADCCRover::ForceBoost(float Seconds)
{
	if (!HasAuthority() || CrashRecoveryRemaining > 0.0f)
	{
		return;
	}
	bBoosting = true;
	BoostSecondsRemaining = FMath::Max(BoostSecondsRemaining, Seconds + RallyBoostExtra);
	BoostCooldownRemaining = 0.0f;
}

void ADCCRover::Client_RallyNote_Implementation(const FString& Message)
{
	if (APlayerController* PC = Cast<APlayerController>(GetController()))
	{
		if (ADCCHUD* HUD = Cast<ADCCHUD>(PC->GetHUD()))
		{
			HUD->ShowNotice(FText::FromString(Message), 4.0f);
		}
	}
}

void ADCCRover::Server_ArcadeVote_Implementation(bool bPlayAgain)
{
	APlayerController* PC = Cast<APlayerController>(GetController());
	for (TActorIterator<ADCCArcadeArena> It(GetWorld()); It; ++It)
	{
		if (PC)
		{
			It->CastVote(PC, bPlayAgain);
		}
		break;
	}
}

void ADCCRover::Client_RallyRadio_Implementation(FName Row)
{
	const FString Wav = DCCVoice::FindLine(TEXT("Dispatch"), Row);
	if (!Wav.IsEmpty())
	{
		DCCVoice::PlayAt(this, Wav, GetActorLocation(), 1.0f);
	}
}

ADCCRover* ADCCRover::FindHookLock(FVector& OutDirection) const
{
	if (!ChaseCamera || !GetWorld())
	{
		return nullptr;
	}
	const FVector Start = GetActorLocation() + FVector(0.0f, 0.0f, 20.0f);
	const FVector Aim = ChaseCamera->GetForwardVector().GetSafeNormal();
	const float Range = RallyHookRange > 0.0f ? RallyHookRange : 2600.0f;
	ADCCRover* Best = nullptr;
	float BestCos = FMath::Cos(FMath::DegreesToRadians(7.0f));
	for (TActorIterator<ADCCRover> It(GetWorld()); It; ++It)
	{
		ADCCRover* Car = *It;
		if (Car == this || Car->RallyDuty != 1)
		{
			continue;
		}
		const FVector To = Car->GetActorLocation() - Start;
		const float Dist = To.Size();
		if (Dist > Range || Dist < 1.0f)
		{
			continue;
		}
		const float Cos = FVector::DotProduct(To / Dist, Aim);
		if (Cos > BestCos)
		{
			BestCos = Cos;
			Best = Car;
			OutDirection = To / Dist;
		}
	}
	return Best;
}

void ADCCRover::RallyToolPressed()
{
	const int32 Index = RallyTools.Num() > 0 ? FMath::Clamp(RallyToolIndex, 0, RallyTools.Num() - 1) : 0;
	const bool bNet = RallyTools.IsValidIndex(Index) && RallyTools[Index] == DCCRallyItems::Net;
	// A net still throws on Q. The hook only fires from the aim camera, and the direction has to
	// ride in the RPC: the camera never leaves this machine.
	if (bHookAim && !bNet && ChaseCamera)
	{
		// A locked mark gets the shot aimed at its centre: what the ring shows is what you hit.
		FVector LockDir;
		Server_FireHook(FindHookLock(LockDir) ? LockDir : ChaseCamera->GetForwardVector().GetSafeNormal());
		// Back to the driving view the moment the hook is away (Zoe: the camera stayed in the aim view and
		// fought the driving). Right mouse aims again for the next shot.
		bHookAim = false;
		CameraYawTarget = 0.0f;
		return;
	}
	Server_UseRallyTool();
}

void ADCCRover::Server_UseRallyTool_Implementation()
{
	for (TActorIterator<ADCCArcadeKart> It(GetWorld()); It; ++It)
	{
		It->UseTool(this);
		break;
	}
}

void ADCCRover::Server_FireHook_Implementation(FVector Dir)
{
	for (TActorIterator<ADCCArcadeKart> It(GetWorld()); It; ++It)
	{
		It->FireHook(this, Dir);
		break;
	}
}

void ADCCRover::Server_CycleRallyTool_Implementation()
{
	for (TActorIterator<ADCCArcadeKart> It(GetWorld()); It; ++It)
	{
		It->CycleTool(this);
		break;
	}
}

void ADCCRover::CameraLook(float Value)
{
	// Accumulates an offset the camera then eases back out of, so letting go of
	// the mouse drifts the view back behind the car instead of snapping.
	// The hook camera does not ease back: the mouse owns the view until right mouse again.
	const float Limit = bHookAim ? 170.0f : 120.0f;
	const float Scale = bHookAim ? 1.6f : 2.5f;
	CameraYawTarget = FMath::Clamp(CameraYawTarget + Value * Scale, -Limit, Limit);
}

void ADCCRover::CameraPitchLook(float Value)
{
	if (!bHookAim)
	{
		return;
	}
	CameraPitch = FMath::Clamp(CameraPitch - Value * 1.5f, -42.0f, 32.0f);
}

void ADCCRover::HookAimPressed()
{
	bHookAim = !bHookAim;
	if (bHookAim)
	{
		CameraPitch = -8.0f;
	}
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
	if (bRadioOn)
	{
		bRadioOn = false;
		++RadioTuneSerial;
		ApplyRadioState();
	}
	else
	{
		// Turning the station on is a tune-in: a song, already in progress.
		TuneIn(-1);
	}
}

void ADCCRover::TuneIn(int32 Track)
{
	if (!HasAuthority())
	{
		return;
	}
	bRadioOn = true;
	if (!RadioTracks.IsEmpty())
	{
		RadioTrackIndex = RadioTracks.IsValidIndex(Track)
			? Track
			: FMath::RandRange(0, RadioTracks.Num() - 1);
		const USoundBase* Sound = RadioTracks[RadioTrackIndex];
		const float Reported = Sound ? Sound->GetDuration() : 0.0f;
		// The middle of the file, not the intro and not the last few seconds.
		RadioStartOffset = Reported > 8.0f
			? FMath::FRandRange(Reported * 0.22f, Reported * 0.68f)
			: 0.0f;
	}
	++RadioTuneSerial;
	ApplyRadioState();
}

FString ADCCRover::GetRadioLine() const
{
	if (!bRadioOn || !RadioTrackNames.IsValidIndex(RadioTrackIndex))
	{
		return FString();
	}
	return FString::Printf(TEXT("[C] next    %s"), *RadioTrackNames[RadioTrackIndex]);
}

void ADCCRover::RadioChannelPressed()
{
	if (!bRadioOn)
	{
		return;
	}
	if (HasAuthority())
	{
		CycleRadio();
	}
	else if (Driver)
	{
		Driver->Server_CycleRoverRadio(this);
	}
}

void ADCCRover::CycleRadio()
{
	if (!HasAuthority() || !bRadioOn || RadioTracks.IsEmpty())
	{
		return;
	}
	TuneIn((RadioTrackIndex + 1) % RadioTracks.Num());
}

void ADCCRover::OnRep_RadioOn()
{
	ApplyRadioState();
}

void ADCCRover::OnRep_RadioTune()
{
	ApplyRadioState();
}

void ADCCRover::ApplyRadioState()
{
	if (!RadioAudio)
	{
		return;
	}
	// Both rep notifies call this in one burst. The serial only moves when
	// the station actually changes, so the second call must not restart it.
	if (AppliedRadioSerial == RadioTuneSerial)
	{
		if (!bRadioOn)
		{
			RadioAudio->Stop();
		}
		return;
	}
	AppliedRadioSerial = RadioTuneSerial;

	USoundBase* Sound = (bRadioOn && RadioTracks.IsValidIndex(RadioTrackIndex))
		? RadioTracks[RadioTrackIndex].Get()
		: nullptr;
	if (!Sound)
	{
		RadioAudio->Stop();
		RadioTrackPlaySeconds = 0.0f;
		return;
	}

	const float Reported = Sound->GetDuration();
	const float Dur = Reported > 1.0f ? Reported : 180.0f;
	const float Offset = FMath::Clamp(RadioStartOffset, 0.0f, FMath::Max(0.0f, Dur - 1.0f));
	RadioTrackElapsed = 0.0f;
	RadioTrackPlaySeconds = FMath::Max(1.0f, Dur - Offset);
	RadioAudio->Stop();
	RadioAudio->SetSound(Sound);
	RadioAudio->FadeIn(0.45f, 0.72f, Offset);
}

void ADCCRover::AdvanceRadio(float DeltaSeconds)
{
	if (!HasAuthority() || !bRadioOn || RadioTracks.IsEmpty() || RadioTrackPlaySeconds <= 0.0f)
	{
		return;
	}
	RadioTrackElapsed += DeltaSeconds;
	if (RadioTrackElapsed < RadioTrackPlaySeconds - 0.05f)
	{
		return;
	}
	RadioTrackIndex = (RadioTrackIndex + 1) % RadioTracks.Num();
	RadioStartOffset = 0.0f;
	++RadioTuneSerial;
	ApplyRadioState();
}

void ADCCRover::SetWrecker(bool bOn)
{
	bWrecker = bOn;
	ApplyWreckerLook();
}

void ADCCRover::OnRep_Wrecker()
{
	ApplyWreckerLook();
}

void ADCCRover::ApplyWreckerLook()
{
	if (!bWrecker || !Body || !Collision)
	{
		return;
	}
	UStaticMesh* BodyMesh = LoadObject<UStaticMesh>(nullptr, TEXT("/Game/DCC/Vehicles/RepoTruck/Full/SM_RepoTruck_Body.SM_RepoTruck_Body"));
	UStaticMesh* BoomMesh = LoadObject<UStaticMesh>(nullptr, TEXT("/Game/DCC/Vehicles/RepoTruck/Full/SM_RepoTruck_Boom.SM_RepoTruck_Boom"));
	UStaticMesh* MagnetMesh = LoadObject<UStaticMesh>(nullptr, TEXT("/Game/DCC/Vehicles/RepoTruck/Full/SM_RepoTruck_Magnet.SM_RepoTruck_Magnet"));
	if (!BodyMesh)
	{
		return;
	}
	Body->SetStaticMesh(BodyMesh);
	// The mesh's nose is +Y, same as the synthwave cars, so the same yaw points it forward.
	// Origin is the tire contact, so it sits on the bottom of a taller box. The boom is not
	// part of the box: a swinging crane must not be a wall.
	Collision->SetBoxExtent(FVector(410.0f, 195.0f, 170.0f));
	Body->SetRelativeRotation(FRotator(0.0f, -90.0f, 0.0f));
	Body->SetRelativeLocation(FVector(0.0f, 0.0f, -170.0f));
	// Just clear of the 170 cm box, so the tires sit down instead of hovering the street.
	RideHeight = 186.0f;
	WindowMID = nullptr;
	// The truck starts in the yard with the dispatch building and crates behind it: a spring arm that
	// tests collision kept yanking the long chase camera in tight behind the magnet. No test for the wrecker.
	if (CameraBoom)
	{
		CameraBoom->bDoCollisionTest = false;
	}

	auto Piece = [this](TObjectPtr<UStaticMeshComponent>& Slot, const TCHAR* Name, UStaticMesh* Mesh) -> UStaticMeshComponent*
	{
		if (!Mesh)
		{
			return nullptr;
		}
		if (!Slot)
		{
			Slot = NewObject<UStaticMeshComponent>(this, Name);
			Slot->SetupAttachment(Body);
			Slot->SetCollisionEnabled(ECollisionEnabled::NoCollision);
			Slot->RegisterComponent();
		}
		Slot->SetStaticMesh(Mesh);
		return Slot;
	};
	if (UStaticMeshComponent* Boom = Piece(CraneBoom, TEXT("CraneBoom"), BoomMesh))
	{
		// Knuckle, in the body mesh's space (cm). Printed by Tools' export.
		Boom->SetRelativeLocation(FVector(9.8f, -186.8f, 345.2f));
		Boom->SetRelativeRotation(FRotator::ZeroRotator);
	}
	if (UStaticMeshComponent* Disc = Piece(Magnet, TEXT("Magnet"), MagnetMesh))
	{
		Disc->SetRelativeLocation(FVector(1.3f, -642.0f, 295.1f));
		Disc->SetRelativeRotation(FRotator::ZeroRotator);
	}
	// Neon underglow (2026-11-14, Zoe: "light neon near the bottom"): pink strips along the sides, cyan
	// front and back, and two soft lights spilling onto the street. The paint is the mesh's own material.
	if (!FindObject<UStaticMeshComponent>(this, TEXT("Underglow_L")))
	{
		const FBox Box = BodyMesh->GetBoundingBox(); // the body mesh's own space: nose +Y, wheels at Min.Z
		const FVector C = Box.GetCenter(), E = Box.GetExtent();
		UStaticMesh* Cube = LoadObject<UStaticMesh>(nullptr, TEXT("/Engine/BasicShapes/Cube.Cube"));
		UMaterialInterface* Pink = LoadObject<UMaterialInterface>(nullptr, TEXT("/Game/Synthwave_city/Material/MI_neon_1.MI_neon_1"));
		UMaterialInterface* Cyan = LoadObject<UMaterialInterface>(nullptr, TEXT("/Game/Synthwave_city/Material/MI_neon_4.MI_neon_4"));
		struct FStrip { const TCHAR* Name; FVector At; FVector Size; UMaterialInterface* Mat; };
		const float Z = Box.Min.Z + 28.0f;
		const FStrip Strips[] = {
			{ TEXT("Underglow_L"), FVector(C.X - E.X * 0.92f, C.Y, Z), FVector(8, E.Y * 1.7f, 6), Pink },
			{ TEXT("Underglow_R"), FVector(C.X + E.X * 0.92f, C.Y, Z), FVector(8, E.Y * 1.7f, 6), Pink },
			{ TEXT("Underglow_F"), FVector(C.X, C.Y + E.Y * 0.86f, Z), FVector(E.X * 1.6f, 8, 6), Cyan },
			{ TEXT("Underglow_B"), FVector(C.X, C.Y - E.Y * 0.86f, Z), FVector(E.X * 1.6f, 8, 6), Cyan },
		};
		for (const FStrip& Strip : Strips)
		{
			UStaticMeshComponent* Glow = NewObject<UStaticMeshComponent>(this, Strip.Name);
			Glow->SetupAttachment(Body);
			Glow->SetStaticMesh(Cube);
			Glow->SetCollisionEnabled(ECollisionEnabled::NoCollision);
			Glow->SetCastShadow(false);
			Glow->SetRelativeLocation(Strip.At);
			Glow->SetRelativeScale3D(Strip.Size / 100.0f);
			if (Strip.Mat) { Glow->SetMaterial(0, Strip.Mat); }
			Glow->RegisterComponent();
		}
		// Its own light (2026-11-14: the dark paint went black at night, "a weird dark blurry mesh"): a soft
		// key light over the cab and bed so the body reads, and two headlights throwing light up the road.
		{
			UPointLightComponent* Key = NewObject<UPointLightComponent>(this, TEXT("TruckKeyLight"));
			Key->SetupAttachment(Body);
			Key->SetRelativeLocation(FVector(C.X, C.Y + E.Y * 0.2f, Box.Max.Z + 260.0f));
			Key->SetLightColor(FLinearColor(1.0f, 0.88f, 0.85f));
			Key->SetIntensity(45000.0f);
			Key->SetAttenuationRadius(2600.0f);
			Key->SetCastShadows(false);
			Key->RegisterComponent();
			// Fill from behind and above, the side the chase camera sees.
			UPointLightComponent* Fill = NewObject<UPointLightComponent>(this, TEXT("TruckFillLight"));
			Fill->SetupAttachment(Body);
			Fill->SetRelativeLocation(FVector(C.X, Box.Min.Y - 450.0f, Box.Max.Z + 150.0f));
			Fill->SetLightColor(FLinearColor(0.85f, 0.9f, 1.0f));
			Fill->SetIntensity(35000.0f);
			Fill->SetAttenuationRadius(2600.0f);
			Fill->SetCastShadows(false);
			Fill->RegisterComponent();
			for (int32 Lamp = 0; Lamp < 2; ++Lamp)
			{
				USpotLightComponent* Head = NewObject<USpotLightComponent>(this, Lamp ? TEXT("HeadlightR") : TEXT("HeadlightL"));
				Head->SetupAttachment(Body);
				Head->SetRelativeLocation(FVector(C.X + (Lamp ? 1.0f : -1.0f) * E.X * 0.6f, Box.Max.Y - 10.0f, Box.Min.Z + E.Z * 0.45f));
				Head->SetRelativeRotation(FRotator(-6.0f, 90.0f, 0.0f)); // the body mesh's nose is +Y
				Head->SetLightColor(FLinearColor(0.85f, 0.92f, 1.0f));
				Head->SetIntensity(14000.0f);
				Head->SetAttenuationRadius(3200.0f);
				Head->SetInnerConeAngle(12.0f);
				Head->SetOuterConeAngle(28.0f);
				Head->SetCastShadows(false);
				Head->RegisterComponent();
			}
		}
		for (int32 Side = 0; Side < 2; ++Side)
		{
			UPointLightComponent* Spill = NewObject<UPointLightComponent>(this, Side ? TEXT("Underglow_LightB") : TEXT("Underglow_LightF"));
			Spill->SetupAttachment(Body);
			// Below the chassis, pointing at the road, small and soft: it should colour the street, not the paint.
			Spill->SetRelativeLocation(FVector(C.X, C.Y + (Side ? -E.Y : E.Y) * 0.45f, Box.Min.Z - 25.0f));
			Spill->SetLightColor(Side ? FLinearColor(0.25f, 0.85f, 1.0f) : FLinearColor(1.0f, 0.25f, 0.85f));
			Spill->SetIntensity(900.0f);
			Spill->SetAttenuationRadius(320.0f);
			Spill->SetCastShadows(false);
			Spill->RegisterComponent();
		}
	}

	if (!WinchLine)
	{
		WinchLine = NewObject<UStaticMeshComponent>(this, TEXT("WinchLine"));
		WinchLine->SetupAttachment(Body);
		WinchLine->SetCollisionEnabled(ECollisionEnabled::NoCollision);
		WinchLine->SetCastShadow(false);
		if (UStaticMesh* Cylinder = LoadObject<UStaticMesh>(nullptr, TEXT("/Engine/BasicShapes/Cylinder.Cylinder")))
		{
			WinchLine->SetStaticMesh(Cylinder);
		}
		if (UMaterialInterface* Neon = LoadObject<UMaterialInterface>(nullptr, TEXT("/Game/Synthwave_city/Material/MI_neon_1.MI_neon_1")))
		{
			WinchLine->SetMaterial(0, Neon);
		}
		WinchLine->RegisterComponent();
		WinchLine->SetVisibility(false);
	}
}

void ADCCRover::UpdateWreckerCrane(float DeltaSeconds)
{
	if (!bWrecker || !CraneBoom || !Body)
	{
		return;
	}

	// Rest is the sculpted pose: straight back over the bed, almost level.
	// Local -Y is the rear. The crane may yaw about 70 degrees off that and
	// pitch up, but it does not swing through the cab.
	FVector LocalDir(0.0f, -1.0f, 0.04f);
	const bool bTowing = RallyTow != nullptr;
	if (bTowing)
	{
		const FVector WorldDir = (RallyTow->GetActorLocation() - CraneBoom->GetComponentLocation()).GetSafeNormal();
		LocalDir = Body->GetComponentTransform().InverseTransformVectorNoScale(WorldDir);
	}
	else if (IsLocallyControlled() && bHookAim && ChaseCamera)
	{
		LocalDir = Body->GetComponentTransform().InverseTransformVectorNoScale(ChaseCamera->GetForwardVector());
	}
	if (LocalDir.IsNearlyZero())
	{
		LocalDir = FVector(0.0f, -1.0f, 0.04f);
	}

	const float Yaw = FMath::Clamp(FMath::RadiansToDegrees(FMath::Atan2(LocalDir.X, -LocalDir.Y)), -70.0f, 70.0f);
	const float Horiz = FMath::Max(1.0f, FVector(LocalDir.X, LocalDir.Y, 0.0f).Size());
	const float PitchUp = FMath::Clamp(FMath::RadiansToDegrees(FMath::Atan2(LocalDir.Z, Horiz)), -8.0f, 32.0f);
	// Roll around local X. Positive roll drops the rear of the arm, so raising it is negative.
	const FRotator Target(0.0f, Yaw, -PitchUp);
	CraneBoom->SetRelativeRotation(FMath::RInterpTo(CraneBoom->GetRelativeRotation(), Target, DeltaSeconds, bTowing ? 4.0f : 7.0f));

	if (!Magnet)
	{
		return;
	}
	// Seated on the boom, this is where the disc was sculpted.
	const FVector RestLocal(-8.5f, -455.2f, -50.1f);
	const FVector Rest = CraneBoom->GetComponentTransform().TransformPosition(RestLocal);
	FVector Goal = Rest;
	float Want = 0.0f;
	if (RallyTrick == 2)
	{
		// The disc rides under the car the driver is holding up, shifted by the magnet.
		Goal = GetActorLocation() + FVector::UpVector * 320.0f + GetActorRightVector() * RallyMagnet * 90.0f;
		Want = 1.0f;
	}
	else if (bTowing)
	{
		const float Roof = RallyTow->Collision ? RallyTow->Collision->GetScaledBoxExtent().Z + 30.0f : 140.0f;
		Goal = RallyTow->GetActorLocation() + FVector(0.0f, 0.0f, Roof);
		Want = 1.0f;
	}
	MagnetAlpha = FMath::FInterpTo(MagnetAlpha, Want, DeltaSeconds, 6.0f);
	const FVector At = FMath::Lerp(Rest, Goal, MagnetAlpha);
	Magnet->SetWorldLocation(At);
	Magnet->SetWorldRotation(MagnetAlpha < 0.2f ? CraneBoom->GetComponentRotation() : FRotator(0.0f, GetActorRotation().Yaw, 0.0f));

	if (!WinchLine)
	{
		return;
	}
	const FVector TipLocal(-17.3f, -417.0f, 15.2f);
	const FVector Tip = CraneBoom->GetComponentTransform().TransformPosition(TipLocal);
	const FVector Span = At - Tip;
	const float Len = Span.Size();
	const bool bShow = MagnetAlpha > 0.05f && Len > 30.0f;
	WinchLine->SetVisibility(bShow);
	if (bShow)
	{
		WinchLine->SetWorldLocation(Tip + Span * 0.5f);
		WinchLine->SetWorldRotation(FRotationMatrix::MakeFromZ(Span).Rotator());
		// Engine cylinder: 100 tall on Z, 100 across. A thin beam the length of the gap.
		WinchLine->SetWorldScale3D(FVector(0.08f, 0.08f, Len / 100.0f));
	}
}

void ADCCRover::ClearOverheadTrick()
{
	RallyTrickChain = 0;
	RallyTrickMove = 0;
	RallyTrickPhase = 1.0f;
	RallyCleanHold = 0.0f;
	bRallyTrickPop = false;
	RallyTrickQueued = 0;
	bRallyDevHold = false;
}

bool ADCCRover::ComputeYardDrive(float& OutThrottle, float& OutSteer, float& OutSpeedCap, float& OutDist, float& OutLane) const
{
	OutThrottle = 0.0f;
	OutSteer = 0.0f;
	OutSpeedCap = 0.0f;
	OutDist = 0.0f;
	OutLane = 0.0f;

	UWorld* World = GetWorld();
	if (!World)
	{
		return false;
	}
	const ADCCArcadeKart* Kart = nullptr;
	for (TActorIterator<ADCCArcadeKart> It(World); It; ++It)
	{
		Kart = *It;
		break;
	}
	if (!Kart)
	{
		return false;
	}

	const float S = ADCCArcadeKart::StreetSpacing;
	const int32 Radius = ADCCArcadeKart::StreetRadius;
	const float RoadHalf = ADCCArcadeKart::RoadHalf;
	const FVector Origin = Kart->GetActorLocation();
	const FVector2D Local(GetActorLocation().X - Origin.X, GetActorLocation().Y - Origin.Y);
	const FVector2D Yard = ADCCArcadeKart::YardOffset();
	const float DistYard = FVector2D::Distance(Local, Yard);
	OutDist = DistYard;

	const int32 NearIx = FMath::Clamp(FMath::RoundToInt(Local.X / S), -Radius, Radius);
	const int32 NearIy = FMath::Clamp(FMath::RoundToInt(Local.Y / S), -Radius, Radius);
	const float StreetX = static_cast<float>(NearIx) * S;
	const float StreetY = static_cast<float>(NearIy) * S;
	const float DistVert = FMath::Abs(Local.X - StreetX);
	const float DistHoriz = FMath::Abs(Local.Y - StreetY);
	const float Edge = -static_cast<float>(Radius) * S;

	// Kickers sit about 5 m off the centreline and are 8 m wide, so a centreline path drives up them.
	// The truck follows a ramp because its own ground trace does. These probes drop a line through
	// static geometry only, so a parked car or a ramp reads as raised and a moving car does not
	// change the lane the two machines agree on. Anything more than knee-high is in the way.
	const float StreetZ = Origin.Z + 16.0f;
	auto RiseAt = [&](const FVector2D& At) -> float
	{
		FCollisionQueryParams Params(TEXT("YardGround"), false, this);
		if (RallyTow)
		{
			Params.AddIgnoredActor(RallyTow);
		}
		if (Driver)
		{
			Params.AddIgnoredActor(Driver);
		}
		const FVector Start(Origin.X + At.X, Origin.Y + At.Y, StreetZ + 900.0f);
		const FVector End(Start.X, Start.Y, StreetZ - 200.0f);
		FHitResult Hit;
		if (!World->LineTraceSingleByChannel(Hit, Start, End, ECC_WorldStatic, Params))
		{
			return 5000.0f;
		}
		return Hit.ImpactPoint.Z - StreetZ;
	};
	auto PointBad = [&](int32 Axis, int32 Index, float Along, float Offset) -> bool
	{
		const float Street = static_cast<float>(Index) * S;
		const FVector2D Side = Axis == 0 ? FVector2D(0.0f, 1.0f) : FVector2D(1.0f, 0.0f);
		const FVector2D Center = Axis == 0 ? FVector2D(Along, Street + Offset) : FVector2D(Street + Offset, Along);
		const float Shoulders[] = { -200.0f, 0.0f, 200.0f };
		for (const float Shoulder : Shoulders)
		{
			const float Rise = RiseAt(Center + Side * Shoulder);
			if (Rise > 40.0f || Rise < -150.0f)
			{
				return true;
			}
		}
		return false;
	};
	// How many centimetres toward the yard (decreasing along) stay under the truck.
	// A parked box is only 620 long, so the step has to be shorter than that.
	auto OpenAhead = [&](int32 Axis, int32 Index, float FromAlong, float Dist, float Offset) -> float
	{
		if (Dist < 80.0f)
		{
			return Dist;
		}
		if (PointBad(Axis, Index, FromAlong, Offset) || PointBad(Axis, Index, FromAlong - 450.0f, Offset))
		{
			return 0.0f;
		}
		const int32 Steps = FMath::Clamp(FMath::CeilToInt(Dist / 320.0f), 1, 40);
		for (int32 Step = 1; Step <= Steps; ++Step)
		{
			const float Along = FromAlong - Dist * (static_cast<float>(Step) / static_cast<float>(Steps));
			if (PointBad(Axis, Index, Along, Offset))
			{
				const float Prev = FromAlong - Dist * (static_cast<float>(Step - 1) / static_cast<float>(Steps));
				return FMath::Max(0.0f, FromAlong - Prev);
			}
		}
		return Dist;
	};
	// The straight slide from one offset to another. The truck steers toward the end of this
	// line, so the body stays on the line's start side. Both ends are sampled.
	auto CorridorBad = [&](int32 Axis, int32 Index, float FromAlong, float ToAlong, float FromOff, float ToOff) -> bool
	{
		const float Span = FMath::Max(FMath::Abs(ToAlong - FromAlong), 1.0f);
		const int32 Steps = FMath::Clamp(FMath::CeilToInt(Span / 300.0f), 1, 30);
		for (int32 Step = 0; Step <= Steps; ++Step)
		{
			const float T = static_cast<float>(Step) / static_cast<float>(Steps);
			if (PointBad(Axis, Index, FMath::Lerp(FromAlong, ToAlong, T), FMath::Lerp(FromOff, ToOff, T)))
			{
				return true;
			}
		}
		return false;
	};

	// ClearSign is the side that leaves a skyway's ramp and pillars alone. The spans match
	// BuildStreetFun, padded so the truck is already in the lane before the ramp starts.
	auto SkywayClearSign = [](int32 Axis, int32 Index, float Along, float& ClearSign) -> bool
	{
		if (Axis == 0 && Index == 1 && Along >= -16000.0f && Along <= 34000.0f)
		{
			ClearSign = -1.0f;
			return true;
		}
		if (Axis == 0 && Index == -1 && Along >= -6000.0f && Along <= 54000.0f)
		{
			ClearSign = 1.0f;
			return true;
		}
		if (Axis == 1 && Index == 1 && Along >= -36000.0f && Along <= 16000.0f)
		{
			ClearSign = 1.0f;
			return true;
		}
		return false;
	};

	auto PreferSign = [&](int32 Axis, int32 Index, float Along) -> float
	{
		float Sky = 1.0f;
		if (SkywayClearSign(Axis, Index, Along, Sky))
		{
			return Sky;
		}
		const int32 Seg = FMath::Clamp(FMath::FloorToInt(Along / S), -Radius, Radius - 1);
		// Same toward-zero remainder the ramp builder uses. Do not wrap negatives to positive.
		const int32 Band = (Index + Seg) % 4;
		const float Kicker = Band < 2 ? -1.0f : 1.0f;
		return -Kicker;
	};

	const bool bOnVertRoad = DistVert <= RoadHalf + 250.0f;
	const bool bOnHorizRoad = DistHoriz <= RoadHalf + 250.0f;
	bool bFollowVert;
	if (bOnVertRoad && bOnHorizRoad)
	{
		// A crossing. Keep the street the nose is already on, so the lane change does not hop grids.
		const FVector Fwd = GetActorForwardVector();
		bFollowVert = FMath::Abs(Fwd.Y) >= FMath::Abs(Fwd.X);
	}
	else
	{
		bFollowVert = bOnVertRoad || (!bOnHorizRoad && DistVert <= DistHoriz);
	}
	// Reach the south rim, then the west rim, then the lot. Turning only happens at the rim.
	if (bFollowVert && NearIx > -Radius && (Local.Y - Edge) < 1400.0f && bOnVertRoad)
	{
		bFollowVert = false;
	}
	else if (!bFollowVert && NearIy > -Radius && (Local.X - Edge) < 1400.0f && bOnHorizRoad)
	{
		bFollowVert = true;
	}

	const int32 Axis = bFollowVert ? 1 : 0;
	int32 Index = bFollowVert ? NearIx : NearIy;
	if (bFollowVert && (Local.X - Edge) < 1400.0f)
	{
		Index = -Radius;
	}
	else if (!bFollowVert && (Local.Y - Edge) < 1400.0f)
	{
		Index = -Radius;
	}
	const float AlongNow = Axis == 0 ? Local.X : Local.Y;
	const float Street = static_cast<float>(Index) * S;
	const float AcrossNow = Axis == 0 ? (Local.Y - Street) : (Local.X - Street);
	const bool bOnThisRoad = FMath::Abs(AcrossNow) <= RoadHalf + 400.0f;

	FVector2D Way = Yard;
	bool bClear = true;
	bool bCutting = false;
	float LaneRoom = 11000.0f;
	const bool bNearLot = DistYard < 14000.0f && NearIx <= -2 && NearIy <= -2;
	if (bNearLot && RiseAt(Yard) < 40.0f)
	{
		bCutting = true;
		OutLane = 0.0f;
	}
	else
	{
		// Look far enough that a skyway ramp is in view before the truck has to leave its lane.
		// A lane change is a shallow slide along a probed line. It is not a diagonal across the
		// kicker and the parked cars. If that line is blocked, stay put or stop.
		const float Horizon = 11000.0f;
		const float Travel = bOnThisRoad && AlongNow > Edge + 800.0f ? -1.0f : 0.0f;
		const float RunTo = Travel < 0.0f ? FMath::Max(Edge, AlongNow - Horizon) : AlongNow;
		const float WantSign = PreferSign(Axis, Index, RunTo);
		const bool bInLane = FMath::Abs(AcrossNow) >= 1150.0f && FMath::Abs(AcrossNow) <= 1900.0f;
		const float Hold = bInLane
			? FMath::Sign(AcrossNow) * FMath::Clamp(FMath::Abs(AcrossNow), 1280.0f, 1840.0f)
			: (FMath::Abs(AcrossNow) < 450.0f ? 0.0f : FMath::Clamp(AcrossNow, -1840.0f, 1840.0f));
		float Offset = WantSign * 1620.0f;
		float TargetAlong = AlongNow - 200.0f;
		bClear = false;
		LaneRoom = 0.0f;

		if (Travel < 0.0f)
		{
			const float HoldOpen = OpenAhead(Axis, Index, AlongNow, Horizon, Hold);
			float Sky = 1.0f;
			const bool bSky = SkywayClearSign(Axis, Index, RunTo, Sky) || SkywayClearSign(Axis, Index, AlongNow, Sky);
			const float Mags[] = { 1620.0f, 1740.0f, 1500.0f, 1840.0f, 1360.0f };
			auto Allowed = [&](float Off) -> bool
			{
				if (!bSky)
				{
					return true;
				}
				// Pillars on the deck's side reach about 12 m off centre. Only the far lane is open there.
				return FMath::Sign(Off) == Sky || FMath::Abs(Off) >= 1580.0f;
			};
			auto Take = [&](float Off, float AimAlong, float Room)
			{
				Offset = Off;
				TargetAlong = AimAlong;
				LaneRoom = Room;
				bClear = true;
			};
			auto TrySide = [&](float Sign) -> bool
			{
				for (const float Mag : Mags)
				{
					const float Off = Sign * Mag;
					if (!Allowed(Off))
					{
						continue;
					}
					// Already in this lane. A real change is to the other side, or out of the centre.
					if (FMath::Abs(Off - Hold) < 280.0f)
					{
						continue;
					}
					// In a lane, the turn lingers there, so the slide has to finish before whatever
					// is ahead. Mid-crossing, the corridor itself is the check.
					const float Budget = bInLane ? HoldOpen : 8000.0f;
					if (Budget < 600.0f)
					{
						continue;
					}
					const float Delta = FMath::Abs(Off - Hold);
					float Forward = Delta / FMath::Tan(FMath::DegreesToRadians(22.0f));
					if (Forward > Budget - 400.0f)
					{
						Forward = Budget - 400.0f;
					}
					if (Forward < 450.0f)
					{
						continue;
					}
					if (FMath::RadiansToDegrees(FMath::Atan2(Delta, Forward)) > 38.0f)
					{
						continue;
					}
					const float To = FMath::Max(Edge, AlongNow - Forward);
					if (CorridorBad(Axis, Index, AlongNow, To, Hold, Off))
					{
						continue;
					}
					const float Need = FMath::Min(4200.0f, Forward);
					if (OpenAhead(Axis, Index, AlongNow, Need, Off) < Need - 100.0f)
					{
						continue;
					}
					Take(Off, To, bInLane ? FMath::Min(HoldOpen, 2800.0f) : 2800.0f);
					return true;
				}
				return false;
			};

			const bool bHoldGood = bInLane && HoldOpen >= Horizon - 50.0f && FMath::Sign(Hold) == WantSign;
			if (bHoldGood)
			{
				Take(Hold, RunTo, Horizon);
			}
			else if (!TrySide(WantSign) && !TrySide(-WantSign))
			{
				if (HoldOpen > 900.0f)
				{
					// No clear slide yet. Roll along the line already under the truck and look again.
					Take(Hold, AlongNow - FMath::Max(400.0f, HoldOpen - 700.0f), HoldOpen);
				}
				else
				{
					// Blocked. Point down the street and do not roll into it.
					Take(FMath::Abs(Hold) > 400.0f ? Hold : WantSign * 1620.0f, AlongNow - 200.0f, 0.0f);
					bClear = false;
					LaneRoom = 0.0f;
				}
			}
		}
		else
		{
			Offset = bInLane ? Hold : WantSign * 1620.0f;
			TargetAlong = AlongNow;
			LaneRoom = 2000.0f;
			bClear = true;
		}
		OutLane = Offset;
		Way = Axis == 0 ? FVector2D(TargetAlong, Street + Offset) : FVector2D(Street + Offset, TargetAlong);
	}

	const FVector2D To = Way - Local;
	const float Dist = To.Size();
	if (DistYard < 900.0f || (Dist < 80.0f && DistYard < 1600.0f))
	{
		return true;
	}

	const float DesiredYaw = FVector(To.X, To.Y, 0.0f).Rotation().Yaw;
	const float YawError = FMath::FindDeltaAngleDegrees(GetActorRotation().Yaw, DesiredYaw);
	OutSteer = FMath::Clamp(YawError / 28.0f, -1.0f, 1.0f);

	const float Turn = FMath::Abs(YawError);
	if (DistYard < 2800.0f)
	{
		OutSpeedCap = 480.0f;
		OutThrottle = 0.35f;
	}
	else if (DistYard < 8000.0f || Turn > 40.0f)
	{
		OutSpeedCap = Turn > 55.0f ? 1100.0f : 1600.0f;
		OutThrottle = 0.65f;
	}
	else
	{
		OutSpeedCap = 2800.0f;
		OutThrottle = 1.0f;
	}
	if (!bCutting)
	{
		const float LatErr = FMath::Abs(AcrossNow - OutLane);
		if (!bClear)
		{
			// A blocked stretch is a stop. Creeping is how the truck climbed a parked car.
			OutSteer = 0.0f;
			OutSpeedCap = 0.0f;
			OutThrottle = 0.0f;
		}
		else if (LatErr > 500.0f)
		{
			// Shallow slide into the next lane. Slow enough that the body stays on the probed line.
			OutSpeedCap = FMath::Min(OutSpeedCap, 1000.0f);
		}
		else if (LatErr > 350.0f || LaneRoom < 2200.0f)
		{
			OutSpeedCap = FMath::Min(OutSpeedCap, 800.0f);
		}
	}
	return true;
}

void ADCCRover::Tick(float DeltaSeconds)
{
	Super::Tick(DeltaSeconds);

	// The driver's machine reports the truck to the host (Server_DriveSync).
	if (!HasAuthority() && IsLocallyControlled() && Cast<APlayerController>(GetController()))
	{
		DriveSyncTimer -= DeltaSeconds;
		if (DriveSyncTimer <= 0.0f)
		{
			DriveSyncTimer = 0.05f;
			Server_DriveSync(GetActorLocation(), GetActorRotation(), Movement ? Movement->Velocity : FVector::ZeroVector, ThrottleInput, SteerInput);
		}
	}
	// The host moved a remotely driven truck itself (a teleport, not driving): tell the driver.
	else if (HasAuthority() && bHasDriveSync && GetController() && !IsLocallyControlled()
		&& FVector::Dist(GetActorLocation(), LastDriveSyncLocation) > 2500.0f)
	{
		IgnoreDriveSyncUntil = FPlatformTime::Seconds() + 0.6;
		LastDriveSyncLocation = GetActorLocation();
		Client_ForceTransform(GetActorLocation(), GetActorRotation());
	}

	// Runs on every machine rendering this rover, driver or not - it is a
	// pure visual ease toward the replicated bWindowsDown, not gameplay.
	UpdateWindowOpacity(DeltaSeconds);
	AdvanceRadio(DeltaSeconds);
	UpdateWreckerCrane(DeltaSeconds);

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

	const bool bControllable = (Driver != nullptr || bAutoPilotActive) && CrashRecoveryRemaining <= 0.0f;

	// Throttle/steer are read directly from live key state every tick,
	// rather than accumulated from Pressed/Released deltas the way they
	// used to be. The old +=/-= pairs broke whenever W/A/S/D was already
	// held at the moment of possession - walking up to the car holding W,
	// then pressing E to get in. A key already down when this pawn's
	// InputComponent is created never fires its Pressed edge (no state
	// transition to detect), but releasing it later still fires Released,
	// which left ThrottleInput or SteerInput stuck at +/-1 with nothing to
	// bring it back to 0 short of exiting. Zoe, 2026-09-03: "the car will
	// suddenly start moving backward uncontrollably or constantly spin
	// until they get out" - this is that bug. Polling ground truth every
	// tick makes a stuck accumulator structurally impossible.
	// VerticalInput (Space/Ctrl) rides along on the same poll, for the same
	// reason - it used to be a ClimbPressed/DescendPressed +=/-= pair with
	// the identical stuck-accumulator vulnerability. Space's flight-engage
	// is a genuine one-shot action though (CanEngageFlight() should only be
	// tried once per press, not every tick it's held), so that still needs
	// an edge - tracked here via bWasSpaceDownLastTick rather than a BindKey
	// Pressed callback, so it shares the same never-misses-a-transition
	// guarantee as the rest of this poll instead of reintroducing the bug
	// this whole block exists to avoid.
	// A player's car reads the keys. An AI-controlled car (Repo Rally's traffic, crews and marks have an
	// AIController so the floating-pawn movement applies their input at all) is also "locally controlled"
	// on the server, so it must fall through to the autopilot branch below, not stop here (2026-11-13).
	if (bControllable && IsLocallyControlled() && Cast<APlayerController>(GetController()))
	{
		if (const APlayerController* PC = Cast<APlayerController>(GetController()))
		{
			ThrottleInput = (PC->IsInputKeyDown(EKeys::W) ? 1.0f : 0.0f)
				- (PC->IsInputKeyDown(EKeys::S) ? 1.0f : 0.0f);
			SteerInput = (PC->IsInputKeyDown(EKeys::D) ? 1.0f : 0.0f)
				- (PC->IsInputKeyDown(EKeys::A) ? 1.0f : 0.0f);

			const bool bSpaceDown = PC->IsInputKeyDown(EKeys::SpaceBar);
			const bool bShiftDown = PC->IsInputKeyDown(EKeys::LeftShift);
			const bool bCtrlDown = PC->IsInputKeyDown(EKeys::LeftControl);
			const bool bMouseDown = PC->IsInputKeyDown(EKeys::LeftMouseButton);
			VerticalInput = (bSpaceDown ? 1.0f : 0.0f) - (bCtrlDown ? 1.0f : 0.0f);

			// Overhead, these four pick a trick. A and D stay on the magnet. Flight stays airborne-only.
			if (RallyTrick == 2)
			{
				if (bSpaceDown && !bWasSpaceDownLastTick) { Server_RallyPickTrick(1); }
				if (bShiftDown && !bWasShiftDownLastTick) { Server_RallyPickTrick(2); }
				if (bCtrlDown && !bWasCtrlDownLastTick) { Server_RallyPickTrick(3); }
				if (bMouseDown && !bWasMouseDownLastTick) { Server_RallyPickTrick(4); }
			}
			else if (bSpaceDown && !bWasSpaceDownLastTick && CanEngageFlight())
			{
				EngageFlight();
			}
			bWasSpaceDownLastTick = bSpaceDown;
			bWasShiftDownLastTick = bShiftDown;
			bWasCtrlDownLastTick = bCtrlDown;
			bWasMouseDownLastTick = bMouseDown;
		}
	}
	else if (bAutoPilotActive && HasAuthority())
	{
		// No Driver to poll input from - these are decoration/traffic cars
		// (Zoe, 2026-09-04: "I placed several cars on the road... animate
		// them to just travel forwards"). Server-driven like every other
		// autonomous actor in this project, rather than IsLocallyControlled
		// (there is no controller possessing this pawn at all to be local
		// on). Straight line at AutoPilotSteerInput, full throttle, and a
		// boost the instant one is off cooldown - "shoot off the end of the
		// ramp" wants it already at max speed well before it gets there, not
		// just at the last moment.
		ThrottleInput = 1.0f;
		SteerInput = AutoPilotSteerInput;

		// A route overrides the fixed steer value entirely - see
		// AutoPilotWaypoints' comment. Steering is proportional to the yaw
		// error toward the current waypoint and clamped to [-1,1], so it
		// reuses the exact same steering path a player's D/A produces rather
		// than teleporting the car around corners: a racer that cut a corner
		// by rotating in place would not read as driving.
		if (AutoPilotWaypoints.Num() > 0)
		{
			AutoPilotWaypointIndex = FMath::Clamp(AutoPilotWaypointIndex, 0, AutoPilotWaypoints.Num() - 1);
			const FVector Target = AutoPilotWaypoints[AutoPilotWaypointIndex];
			const FVector ToTarget = FVector(Target.X - GetActorLocation().X, Target.Y - GetActorLocation().Y, 0.0f);

			if (ToTarget.SizeSquared() > FMath::Square(AutoPilotAcceptanceRadius))
			{
				const float DesiredYaw = ToTarget.Rotation().Yaw;
				const float YawError = FMath::FindDeltaAngleDegrees(GetActorRotation().Yaw, DesiredYaw);
				// /45 rather than /180: full lock by 45 degrees off means it
				// commits to a corner instead of drifting lazily toward it.
				SteerInput = FMath::Clamp(YawError / 45.0f, -1.0f, 1.0f);
			}
			else
			{
				// Reached. Advance, and count a lap on the wrap.
				const int32 NextIndex = AutoPilotWaypointIndex + 1;
				if (NextIndex >= AutoPilotWaypoints.Num())
				{
					++AutoPilotLapsCompleted;
					if (bAutoPilotLoop)
					{
						AutoPilotWaypointIndex = 0;
					}
					else
					{
						// End of a one-shot route. Coast to a stop rather than
						// braking - same "existing momentum finishes the move"
						// idea as the ramp-launch shutoff.
						StopAutoPilot();
					}
				}
				else
				{
					AutoPilotWaypointIndex = NextIndex;
				}
			}
		}

		// Racers do not spam boost. bAutoPilotBoostOnLaunch is the traffic
		// behaviour (be at top speed by the time the ramp arrives); on a route
		// a permanent boost would erase AutoPilotSpeedScale, which is the only
		// thing separating the entrants from each other.
		if (bAutoPilotBoostOnLaunch && AutoPilotWaypoints.Num() == 0
			&& !bBoosting && BoostCooldownRemaining <= 0.0f)
		{
			BoostPressed();
		}
	}

	if (bControllable)
	{
		// Overhead balance takes A and D for the magnet, so SteerInput is not the wheel.
		// The truck drives itself to the yard on a separate throttle and steer. Struggle still coasts.
		float YardThrottle = 0.0f;
		float YardSteer = 0.0f;
		float YardCap = 0.0f;
		float YardDist = 0.0f;
		float YardLane = 0.0f;
		const bool bYardDrive = RallyTrick == 2 && Driver
			&& (IsLocallyControlled() || HasAuthority())
			&& ComputeYardDrive(YardThrottle, YardSteer, YardCap, YardDist, YardLane);
		if (bYardDrive)
		{
			bDrifting = false;
		}
		if (RallyTrick != 2)
		{
			bRallyYardNoted = false;
		}
		else if (bYardDrive && HasAuthority() && GetWorld())
		{
			const double Clock = GetWorld()->GetTimeSeconds();
			if (Clock >= RallyYardLogAt)
			{
				RallyYardLogAt = Clock + 1.0;
				UE_LOG(LogTemp, Log, TEXT("[DCC Drive] yard path dist %.0f cap %.0f lane %.0f at %s"),
					YardDist, YardCap, YardLane, *GetActorLocation().ToCompactString());
				if (!bRallyYardNoted && YardDist < 7200.0f)
				{
					bRallyYardNoted = true;
					UE_LOG(LogTemp, Log, TEXT("[DCC Drive] yard path in the lot, dist %.0f"), YardDist);
				}
			}
		}
		const float UsedThrottle = bYardDrive ? YardThrottle : ThrottleInput;

		// --- speed ramp ----------------------------------------------------
		// Throttle winds SpeedRamp up over SecondsToReachCruise rather than
		// jumping straight to top speed, so the rover accelerates instead of
		// simply being fast the instant a key goes down.
		// Player wreckers carry weight. Traffic and autopilot keep the old ramp so the city still flows.
		// Cruise seconds 5.8 -> 9.4. Coast multiplier 1.6 -> 0.48, so letting go keeps the line.
		const bool bWeight = bWrecker && Driver && !bAutoPilotActive;
		const float CruiseSeconds = bWeight ? 9.4f : SecondsToReachCruise;
		const float CoastMul = bWeight ? 0.48f : 1.6f;
		const float RampRate = CruiseSeconds > 0.0f ? 1.0f / CruiseSeconds : 10.0f;
		if (!FMath::IsNearlyZero(UsedThrottle))
		{
			SpeedRamp = FMath::Clamp(SpeedRamp + RampRate * DeltaSeconds, 0.0f, 1.0f);
		}
		else
		{
			SpeedRamp = FMath::Clamp(SpeedRamp - RampRate * CoastMul * DeltaSeconds, 0.0f, 1.0f);
		}
		if (bWeight)
		{
			static bool bLoggedFeel = false;
			if (!bLoggedFeel)
			{
				bLoggedFeel = true;
				UE_LOG(LogTemp, Log, TEXT("[DCC Drive] weight: seconds to cruise 5.8 -> 9.4, coast mul 1.6 -> 0.48, steer 1.25 at a stop -> 0.38 at cruise (was the other way, floor 0.3 and growing), drift slide 0.55 instant -> 0.78 with grip return 1.6/s, turn scrub 0.12 -> 0.04 unless drifting."));
			}
		}

		if (Movement)
		{
			// Boost overrides the ramp outright - that is what makes it read as
			// a shove rather than a slightly steeper acceleration curve.
			// AutoPilotSpeedScale is 1 for everything a player drives, so this
			// is a no-op outside a race - see its own comment.
			const float Pace = (bAutoPilotActive && AutoPilotWaypoints.Num() > 0)
				? FMath::Clamp(AutoPilotSpeedScale, 0.1f, 2.0f) : 1.0f;
			// RaceSpeedScale is 1 outside Repo Rally. In the race the server sets it
			// (a boot, a cable, a draft) and it replicates, so this machine agrees.
			const float SpeedScale = Pace * FMath::Clamp(RaceSpeedScale, 0.2f, 1.6f) * FMath::Clamp(RallyEngine, 1.0f, 1.25f);
			// The old 0.15 floor put the truck on a glide the instant the key went down.
			Movement->MaxSpeed = SpeedScale * (bBoosting
				? BoostSpeed
				: FMath::Max(320.0f, MaxCruiseSpeed * FMath::Max(SpeedRamp, 0.06f)));
			if (bYardDrive)
			{
				Movement->MaxSpeed = FMath::Max(0.0f, YardCap);
				if (YardCap <= 1.0f)
				{
					Movement->StopMovementImmediately();
				}
				else if (Movement->Velocity.Size() > YardCap)
				{
					Movement->Velocity = Movement->Velocity.GetClampedToMaxSize(YardCap);
				}
			}
		}

		// --- steering ------------------------------------------------------
		const float Speed = GetVelocity().Size();
		// MinSteerScale floors this rather than letting it reach 0 at a dead
		// stop - see the property's own comment for why.
		// A player truck turns tighter when it is slow and wider when it is fast.
		const float Speed01 = FMath::Clamp(Speed / FMath::Max(1.0f, MaxCruiseSpeed), 0.0f, 1.0f);
		const float SteerScale = bWeight
			? FMath::Lerp(1.25f, 0.38f, Speed01)
			: FMath::Clamp(FMath::Max(MinSteerScale, Speed / FMath::Max(1.0f, MaxCruiseSpeed * 0.30f)), 0.0f, 1.0f);
		if (bWeight)
		{
			const float GripTarget = bDrifting ? 0.22f : 1.0f;
			DriftGrip = FMath::FInterpConstantTo(DriftGrip, GripTarget, DeltaSeconds, bDrifting ? 0.55f : 1.6f);
		}
		else if (!bDrifting)
		{
			DriftGrip = 1.0f;
		}
		const float TurnRate = TurnRateDegreesPerSecond * (bDrifting ? DriftTurnMultiplier : 1.0f);

		// Struggle takes A and D, so the truck coasts. Overhead, the yard steer turns it
		// and SteerInput stays on the magnet.
		const float Wheel = bYardDrive ? YardSteer : SteerInput;
		if (!FMath::IsNearlyZero(Wheel) && (bYardDrive || RallyTrick == 0))
		{
			AddActorWorldRotation(FRotator(0.0f,
				Wheel * TurnRate * SteerScale * DeltaSeconds, 0.0f));
			// A hard corner scrubs a little speed. A weighted truck keeps more of it unless it is drifting.
			if (!bYardDrive && Speed > 800.0f)
			{
				const float Scrub = (bWeight && DriftGrip > 0.6f) ? 0.04f : 0.12f;
				SpeedRamp = FMath::Max(0.0f, SpeedRamp - FMath::Abs(Wheel) * Scrub * DeltaSeconds);
			}
		}

		// --- throttle ------------------------------------------------------
		if (!FMath::IsNearlyZero(UsedThrottle) || (bBoosting && !bYardDrive))
		{
			const float Drive = (!bYardDrive && bBoosting) ? 1.0f : UsedThrottle;
			FVector Direction = GetActorForwardVector();

			// Drifting keeps part of the OLD velocity direction, so the car
			// slides wide through a turn instead of tracking its nose exactly.
			// Grip returns on its own after Shift comes up, so a long drift is a line you hold.
			// The yard run tracks the street, so it does not slide.
			const float Slide = bYardDrive ? 0.0f
				: (bWeight ? FMath::Lerp(0.0f, 0.78f, 1.0f - DriftGrip) : (bDrifting ? DriftSlide : 0.0f));
			if (Slide > 0.04f && Speed > 200.0f)
			{
				const FVector Heading = GetVelocity().GetSafeNormal();
				Direction = FMath::Lerp(Direction, Heading, Slide).GetSafeNormal();
			}

			AddMovementInput(Direction, Drive);
		}
	}
	else
	{
		SpeedRamp = FMath::Max(0.0f, SpeedRamp - DeltaSeconds);
	}

	// --- traffic is solid to trucks, not to itself (every machine: sweeps run locally) ----
	{
		const bool bTraffic = RallyDuty == 2;
		if (TrafficShell && (TrafficShell->GetCollisionEnabled() != ECollisionEnabled::NoCollision) != bTraffic)
		{
			TrafficShell->SetCollisionEnabled(bTraffic ? ECollisionEnabled::QueryOnly : ECollisionEnabled::NoCollision);
			if (bTraffic && Collision)
			{
				Collision->SetCollisionEnabled(ECollisionEnabled::NoCollision);
			}
		}
	}

	// --- camera ------------------------------------------------------------
	if (CameraBoom)
	{
		// The hook camera is the driver's own view. Everyone else's copy of this pawn
		// keeps the chase camera, and losing the pawn (getting out) drops the aim.
		if (!IsLocallyControlled())
		{
			bHookAim = false;
		}
		if (bHookAim)
		{
			// Short, high, and held where the mouse left it. The reticle is the screen centre,
			// which is this boom's forward, and that forward is what Q sends.
			CameraYawOffset = FMath::FInterpTo(CameraYawOffset, CameraYawTarget, DeltaSeconds, 14.0f);
			CameraBoom->TargetArmLength = FMath::FInterpTo(CameraBoom->TargetArmLength, 260.0f, DeltaSeconds, 8.0f);
			CameraBoom->SetRelativeLocation(FMath::VInterpTo(CameraBoom->GetRelativeLocation(), FVector(-20.0f, 0.0f, 280.0f), DeltaSeconds, 8.0f));
			CameraBoom->SetRelativeRotation(FRotator(CameraPitch, CameraYawOffset, 0.0f));
		}
		else if (IsLocallyControlled() && GetWorld() && GetWorld()->GetGameState()
			&& GetWorld()->GetGameState()->GetServerWorldTimeSeconds() < RallyClampUntil && Magnet)
		{
			// The clamp, on this screen only. The shift keeps running for everyone else.
			const FVector From = GetActorLocation() + FVector(-120.0f, 80.0f, 260.0f);
			const FVector To = Magnet->GetComponentLocation() - From;
			CameraBoom->TargetArmLength = FMath::FInterpTo(CameraBoom->TargetArmLength, 420.0f, DeltaSeconds, 8.0f);
			CameraBoom->SetRelativeLocation(FMath::VInterpTo(CameraBoom->GetRelativeLocation(), FVector(-120.0f, 80.0f, 260.0f), DeltaSeconds, 8.0f));
			CameraBoom->SetRelativeRotation(FMath::RInterpTo(CameraBoom->GetRelativeRotation(), To.Rotation(), DeltaSeconds, 8.0f));
		}
		else if (RallyTow && IsLocallyControlled())
		{
			// Towing: the same chase view, pulled back and up with the line's length so the car on the hook
			// stays in shot - but never recentred, so it does not swing while you drive (2026-11-14).
			const float Span = FVector::Dist(GetActorLocation(), RallyTow->GetActorLocation());
			CameraYawTarget = FMath::FInterpTo(CameraYawTarget, 0.0f, DeltaSeconds, 1.4f);
			CameraYawOffset = FMath::FInterpTo(CameraYawOffset, CameraYawTarget, DeltaSeconds, 6.0f);
			CameraBoom->TargetArmLength = FMath::FInterpTo(CameraBoom->TargetArmLength, FMath::Clamp((bWrecker ? 1700.0f : 1100.0f) + Span * 0.35f, 1100.0f, 2800.0f), DeltaSeconds, 2.5f);
			CameraBoom->SetRelativeLocation(FMath::VInterpTo(CameraBoom->GetRelativeLocation(), FVector(0.0f, 0.0f, bWrecker ? 380.0f : 200.0f), DeltaSeconds, 3.0f));
			CameraBoom->SetRelativeRotation(FRotator(-24.0f, CameraYawOffset, 0.0f));
		}
		else
		{
			// Eases back to centre once the mouse stops, so the view trails the car
			// rather than staying wherever it was last flicked.
			CameraYawTarget = FMath::FInterpTo(CameraYawTarget, 0.0f, DeltaSeconds, 1.4f);
			CameraYawOffset = FMath::FInterpTo(CameraYawOffset, CameraYawTarget, DeltaSeconds, 6.0f);
			// The wrecker is twice a car's size: a longer, higher chase so the whole truck is in view.
			CameraBoom->TargetArmLength = FMath::FInterpTo(CameraBoom->TargetArmLength, bWrecker ? 1400.0f : 900.0f, DeltaSeconds, 6.0f);
			CameraBoom->SetRelativeLocation(FMath::VInterpTo(CameraBoom->GetRelativeLocation(), bWrecker ? FVector(0.0f, 0.0f, 400.0f) : FVector::ZeroVector, DeltaSeconds, 6.0f));
			CameraBoom->SetRelativeRotation(FRotator(-18.0f, CameraYawOffset, 0.0f));
		}
		// After the chain above, never inside it (2026-11-14: wedged between its branches, this block
		// swallowed the chase and tow cameras, so the view stayed in the aim pose after a shot).
		// Aiming zooms in; everything else is the wide chase view (90, the camera's own default).
		if (ChaseCamera)
		{
			ChaseCamera->SetFieldOfView(FMath::FInterpTo(ChaseCamera->FieldOfView, bHookAim ? 58.0f : 90.0f, DeltaSeconds, 9.0f));
		}
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
	// --- suspension spring --------------------------------------------------
	// Kicked by SuspensionVelocity -= ... on landing (see UpdateGroundAndAir's
	// landed branch); otherwise just a standard critically-damped-ish spring
	// relaxing back toward 0. Deliberately NOT faded by speed the way the
	// hover bob above is - a landing impact happens exactly while moving, so
	// fading it out at speed would hide the one moment it exists to be felt.
	// "The vehicle needs a little bit of give still" (Zoe, 2026-09-04).
	{
		const float SpringForce = -SuspensionStiffness * SuspensionOffset - SuspensionDamping * SuspensionVelocity;
		SuspensionVelocity += SpringForce * DeltaSeconds;
		SuspensionOffset = FMath::Clamp(SuspensionOffset + SuspensionVelocity * DeltaSeconds,
			-MaxSuspensionCompression, MaxSuspensionCompression);
	}

	if (Body)
	{
		const UWorld* World = GetWorld();
		const float SpeedFraction = FMath::Clamp(GetVelocity().Size() / FMath::Max(1.0f, MaxCruiseSpeed), 0.0f, 1.0f);
		const float BobPeriod = FMath::Max(0.1f, HoverBobPeriodSeconds);
		const float Bob = World
			? FMath::Sin(World->GetTimeSeconds() * (2.0f * PI / BobPeriod)) * HoverBobAmplitude * (1.0f - SpeedFraction)
			: 0.0f;
		// The wrecker's rest pose is the tire contact (-170). Overwriting it with the car's
		// -90 every tick is what left the truck floating above the asphalt.
		const float RestZ = bWrecker ? -170.0f : -90.0f;
		Body->SetRelativeLocation(FVector(0.0f, 0.0f, RestZ + Bob + SuspensionOffset));
	}

	// Flight has its own throttle - the ground ramp does not apply, and the
	// movement component's cap has to be raised or flying is capped at cruise.
	// bBoosting takes priority over the flat FlightSpeed cap, or a boost
	// triggered mid-air (see BoostPressed) would do nothing - this used to
	// unconditionally overwrite whatever the ground branch above had just
	// set, discarding boost outright the instant bFlying was true.
	if (bFlying && Movement)
	{
		const float Engine = (Driver && Driver->Wallet)
			? DCCChopShop::Pace(Driver->Wallet->ChopLevel(DCCChopShop::Engine)) : 1.0f;
		Movement->MaxSpeed = (bBoosting ? BoostSpeed : FlightSpeed) * Engine;
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

	// Studded tires (Repo Rally): one real hit, shrugged off. Checked before the
	// crash so the driver's own machine doesn't lock the controls and then get
	// corrected a moment later.
	if (TryAbsorb())
	{
		if (HasAuthority())
		{
			Client_RallyNote(TEXT("Studded tires. The wall didn't stop you."));
		}
		return;
	}

	// A glancing blow on a player wrecker keeps going along the wall. Head-on still stops.
	// Into is how square the velocity meets the surface. Under 0.62 is a scrape.
	const FVector Vel = GetVelocity();
	const float Into = FVector::DotProduct(Vel.GetSafeNormal(), -HitNormal);
	if (bWrecker && Driver && !bAutoPilotActive && Into < 0.62f)
	{
		const FVector Tangential = FVector::VectorPlaneProject(Vel, HitNormal);
		Movement->Velocity = Tangential * 0.82f + HitNormal * 80.0f;
		const float Side = FVector::CrossProduct(GetActorForwardVector(), HitNormal).Z;
		CameraYawTarget = FMath::Clamp(CameraYawTarget + FMath::Sign(Side) * 7.0f, -18.0f, 18.0f);
		static double LastSpark = 0.0;
		const double SparkNow = FPlatformTime::Seconds();
		if (SparkNow - LastSpark > 0.12)
		{
			LastSpark = SparkNow;
			static bool bTried = false;
			static UNiagaraSystem* Sparks = nullptr;
			if (!bTried)
			{
				bTried = true;
				Sparks = LoadObject<UNiagaraSystem>(nullptr, TEXT("/Game/Legendary_Forge/Fx/Sparks/NS_sparks.NS_sparks"));
			}
			if (Sparks)
			{
				UNiagaraFunctionLibrary::SpawnSystemAtLocation(this, Sparks, HitLocation, HitNormal.Rotation());
			}
		}
		static bool bLoggedScrape = false;
		if (!bLoggedScrape)
		{
			bLoggedScrape = true;
			UE_LOG(LogTemp, Log, TEXT("[DCC Drive] scrape: into %.2f kept %.0f cm/s (head-on still stops at 0.62)"),
				Into, Movement->Velocity.Size());
		}
		return;
	}

	// A real wall. Stop, shove gently back off it, and take the controls away
	// for a moment so it reads as an impact rather than as the car sticking to
	// the surface with the throttle still buried.
	const float ImpactSpeed = GetVelocity().Size();
	if (HasAuthority())
	{
		for (TActorIterator<ADCCArcadeKart> It(GetWorld()); It; ++It)
		{
			It->NoteImpact(this, ImpactSpeed);
		}
	}
	float Recovery = CrashRecoverySeconds;
	if (Driver && Driver->Wallet)
	{
		Recovery *= DCCChopShop::DamageTaken(Driver->Wallet->ChopLevel(DCCChopShop::Plating));
		Recovery = FMath::Max(0.12f, Recovery);
	}
	Movement->Velocity = HitNormal * BounceBackSpeed;
	SpeedRamp = 0.0f;
	CrashRecoveryRemaining = Recovery;
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

	// Repo Rally owns the height of a hovering mark, and of a mark on a hook. The street
	// trace below would plant either one every tick and fight the tow.
	if (bRallyHeld)
	{
		bAirborne = false;
		return;
	}

	const FVector Origin = GetActorLocation();
	const bool bWasAirborneBeforeThisTick = bAirborne;

	FHitResult Hit;
	FCollisionQueryParams Params(SCENE_QUERY_STAT(RoverGround), false, this);
	if (Driver)
	{
		Params.AddIgnoredActor(Driver);
	}
	// A car on the magnet sits above the bed. Tracing it as "the ground" makes the
	// truck climb onto the thing it is holding (seen: the wrecker rose to z 4332).
	if (RallyTow)
	{
		Params.AddIgnoredActor(RallyTow);
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
			const float Burn = (Driver && Driver->Wallet)
				? DCCChopShop::FuelBurn(Driver->Wallet->ChopLevel(DCCChopShop::Tank)) : 1.0f;
			FlightSecondsRemaining -= DeltaSeconds * Burn;
			if (FlightSecondsRemaining <= 0.0f)
			{
				EndFlight();
			}
		}

		// Climb rate is purely Space/Ctrl now - the mouse-drag flap lever that
		// used to add an elevator-style lift on top of this is gone (Zoe,
		// 2026-09-04: "let's remove the click and drag aspect of flying").
		VerticalVelocity = VerticalInput * FlightClimbRate;
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
		// can still be earned over a chasm. This is also the ramp-launch
		// case for an autopilot car - see the bAutoPilotActive check just
		// below.
		bAirborne = true;
		// AutoPilotWaypoints.Num() == 0 is what separates traffic from a
		// racer here - see that property's comment. A racer must keep its
		// route across every bump, kerb and crest on the track.
		if (bAutoPilotActive && AutoPilotWaypoints.Num() == 0 && !bWasAirborneBeforeThisTick)
		{
			// "Shoot off the end of the ramp... and then stop running after
			// that" (Zoe, 2026-09-04) - autopilot's job was getting the car
			// up to speed and pointed straight; the instant it leaves the
			// ground it already has everything it needs (existing
			// horizontal velocity plus gravity) to finish the jump on its
			// own. Stopping here rather than on landing means it never
			// tries to keep accelerating forward mid-air, where there is no
			// traction for that to mean anything.
			bAutoPilotActive = false;
		}
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
		if (bAutoPilotActive && AutoPilotWaypoints.Num() == 0 && !bWasAirborneBeforeThisTick)
		{
			// Same launch-detection as the !bFoundGround branch above - see
			// its comment. Reached when the ground trace still hits
			// something (a lower landing pad within trace range, say) but
			// the gap is already too big to be "still driving."
			bAutoPilotActive = false;
		}
		VerticalVelocity -= Gravity * DeltaSeconds;

		float NewZ = Origin.Z + VerticalVelocity * DeltaSeconds;
		if (NewZ <= TargetZ)
		{
			// Landed. Kicks the suspension spring (see the Tick() block that
			// relaxes SuspensionOffset back to 0) BEFORE VerticalVelocity is
			// zeroed, so a harder landing compresses harder - "the vehicle
			// needs a little bit of give still" (Zoe, 2026-09-04). Snapping
			// straight to rest read as too rigid; this is what actually
			// gives a landing weight.
			SuspensionVelocity -= FMath::Abs(VerticalVelocity) * SuspensionLandingKickScale;

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
		bool bTargetFromProbe = false;
		if (AheadZ > TargetZ && (AheadZ - TargetZ) <= MaxStepHeight)
		{
			EffectiveTarget = AheadZ + RideHeight;
			bTargetFromProbe = true;
		}

		// A contact-time step request (NotifyHit) always wins, since that is a
		// surface actually in the way right now.
		const bool bTargetFromContact = PendingStepUpZ > EffectiveTarget;
		if (bTargetFromContact)
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

			// Distinguish a genuine step/ledge from an ordinary ramp, so
			// AlignToSurface can match the ramp's real angle (its grounded
			// branch, keyed off LastGroundNormal) instead of forcing the
			// fixed StepClimbPitchDegrees the whole way up it. Zoe,
			// 2026-09-03: "when you go on the ramp, the car should conform
			// to the ramp's angle." The old test was a flat "rise > 12 units
			// this tick", which any ordinary ramp clears just as easily as a
			// real step at real driving speed - 4200 cm/s cruise alone covers
			// ~70 units a tick, so even a shallow ramp's rise blew past 12.
			// AheadZ was sampled a fixed ForwardProbeDistance ahead, so the
			// slope it implies (atan2(rise, that distance)) is a geometric
			// fact about what's ahead, independent of current speed or
			// DeltaSeconds - a real step reads far steeper than
			// MaxClimbAngleDegrees at that distance even at a crawl; an
			// ordinary ramp does not, at any speed.
			//
			// A contact-time bump (bTargetFromContact) still needs its OWN
			// size filter, not an unconditional true - a tiled floor's seams
			// register as real contacts (NotifyHit genuinely fires, however
			// small the lip), and forcing every one of them into the full
			// StepClimbPitchDegrees pose is what made the car "constantly
			// tilt back as if hitting something" the whole way down Zoe's
			// new road (2026-09-04). The pre-ramp-fix code applied this same
			// 12-unit-rise floor to every case uniformly; this restores it
			// for contact specifically, so a seam too small to be worth
			// noticing stays smooth while a real curb still gets the pose.
			if (bTargetFromContact)
			{
				bClimbingStep = (EffectiveTarget - Origin.Z) > 12.0f;
			}
			else if (bTargetFromProbe)
			{
				const float ImpliedSlopeDegrees = FMath::RadiansToDegrees(FMath::Atan2(
					AheadZ - TargetZ, FMath::Max(1.0f, ForwardProbeDistance)));
				bClimbingStep = ImpliedSlopeDegrees > MaxClimbAngleDegrees;
			}
			else
			{
				bClimbingStep = false;
			}
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
	if (RallyTow)
	{
		Params.AddIgnoredActor(RallyTow);
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
		// Level flight - no elevator-style pitch since the flap lever that
		// used to drive this is gone (see UpdateGroundAndAir's bFlying
		// branch). Climb/dive is still very much a thing, just felt through
		// altitude change and the hover bob/speed-tilt below rather than a
		// nose animation.
		Desired.Pitch = 0.0f;
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
		Candidate->AttachToComponent(Collision,
			FAttachmentTransformRules::SnapToTargetNotIncludingScale);
		// Seated and visible rather than hidden - ApplyPassengerState below
		// re-runs this on every machine once the attachment replicates.
		SeatOccupant(Candidate, PassengerSeatOffsets.IsEmpty()
			? FVector::ZeroVector
			: PassengerSeatOffsets[FMath::Min(Passengers.Num() - 1, PassengerSeatOffsets.Num() - 1)]);
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

void ADCCRover::SeatOccupant(ADCCCharacterBase* Occupant, const FVector& SeatOffset)
{
	if (!Occupant)
	{
		return;
	}

	// Occupants are visible in the car now rather than hidden inside it (Zoe,
	// 2026-09-05) - see the SEATING block in the header for why the offsets
	// look the way they do.
	Occupant->SetActorHiddenInGame(false);

	// Only positioned once the attachment has actually reached this machine.
	// Setting a relative location on an unattached actor would move them in
	// WORLD space instead, teleporting them to the map origin on any client
	// whose attachment replication has not landed yet.
	if (Occupant->GetAttachParentActor() == this)
	{
		Occupant->SetActorRelativeLocation(SeatOffset);
		Occupant->SetActorRelativeRotation(FRotator::ZeroRotator);
	}
}

void ADCCRover::ApplyPassengerState()
{
	// Runs on every machine - a remote client has to see them riding along
	// too, not just the server.
	for (int32 Index = 0; Index < Passengers.Num(); ++Index)
	{
		ADCCCharacterBase* Passenger = Passengers[Index];
		if (!Passenger)
		{
			continue;
		}

		// Past the end of the authored list, everyone shares the last seat -
		// better than dropping them at the car's origin, and MaxPassengers
		// keeps that from stacking more than a few deep anyway.
		const FVector Seat = PassengerSeatOffsets.IsEmpty()
			? FVector::ZeroVector
			: PassengerSeatOffsets[FMath::Min(Index, PassengerSeatOffsets.Num() - 1)];

		SeatOccupant(Passenger, Seat);
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
		if (bWanted)
		{
			// Scaled here rather than in the constructor so BoostTrailScale
			// stays a live Details-panel knob - see its own comment.
			BoostTrail->SetRelativeScale3D(FVector(FMath::Max(0.1f, BoostTrailScale)));
			BoostTrail->Activate(true);
		}
		else
		{
			BoostTrail->Deactivate();
		}
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
	// 2026-11-13 (Zoe: "when you enter a car it sometimes takes 3-4 full seconds where nothing is
	// happening"): the "getting in" clip is the agree gesture, a 3-4 s nod, and the driver stood rooted
	// for all of it. There is no real climb-in clip, so entering is immediate until there is one.
	float Delay = 0.0f;

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

	// A car mid-autopilot-launch is still fair game to hop into - turn the
	// autopilot off the moment someone actually does, or the server would
	// keep driving it straight via the bAutoPilotActive branch in Tick()
	// (which only yields to a Driver's own input on whichever machine is
	// IsLocallyControlled() for them - the server itself is not that
	// machine for a remote driver) fighting the new driver's real input.
	// StopAutoPilot rather than the bare flag, so hijacking a racer mid-race
	// also drops its route - otherwise re-arming autopilot later (a repeat
	// race) would resume it from wherever the player abandoned it.
	StopAutoPilot();

	// Which side they walked up on, measured BEFORE the attach moves them -
	// afterwards their location is the car's own and the answer would always
	// be the same. Zoe, 2026-09-05: "make sure when you exit the car, you
	// leave on the same side you got in in."
	const FVector ToDriver = Driver->GetActorLocation() - GetActorLocation();
	DriverEntrySide = FVector::DotProduct(ToDriver, GetActorRightVector()) >= 0.0f ? 1.0f : -1.0f;

	// The crawler rides along attached rather than being destroyed or
	// teleported away, so everything hanging off them - inventory, stats,
	// quest state, the carried Strongbox - simply continues to exist. Still
	// visible, just seated: see ApplyDriverState/SeatOccupant.
	Driver->SetActorEnableCollision(false);
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
	// intersecting the body and get shoved somewhere unpredictable. Which
	// side is whichever one they got in from (DriverEntrySide), not always
	// the right - walking around the car to get in and then being spat out
	// the far side reads as the car teleporting you.
	const FVector DropAt = GetActorLocation()
		+ GetActorRightVector() * ExitSideDistance * DriverEntrySide
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
	// Runs on every machine: a remote client has to see the driver sitting in
	// the rover too, not just the server.
	SeatOccupant(Driver, DriverSeatOffset);
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

// ---------------------------------------------------------------------------
// AUTOPILOT ROUTES
// ---------------------------------------------------------------------------

void ADCCRover::StartAutoPilotRoute(const TArray<FVector>& Route, bool bLoop)
{
	if (!HasAuthority())
	{
		return;
	}

	AutoPilotWaypoints = Route;
	bAutoPilotLoop = bLoop;
	AutoPilotLapsCompleted = 0;

	if (AutoPilotWaypoints.Num() == 0)
	{
		StopAutoPilot();
		return;
	}

	// Start at whichever waypoint is furthest AHEAD rather than the closest
	// one outright. A car sitting on the start line is within acceptance
	// range of waypoint 0 by construction, so aiming at the nearest point
	// would have it tick straight past the first corner and count a phantom
	// lap. Nearest-not-already-reached is the honest answer.
	int32 Best = 0;
	float BestDistSq = TNumericLimits<float>::Max();
	const FVector Here = GetActorLocation();
	for (int32 i = 0; i < AutoPilotWaypoints.Num(); ++i)
	{
		const FVector Delta = FVector(AutoPilotWaypoints[i].X - Here.X,
			AutoPilotWaypoints[i].Y - Here.Y, 0.0f);
		const float DistSq = Delta.SizeSquared();
		if (DistSq > FMath::Square(AutoPilotAcceptanceRadius) && DistSq < BestDistSq)
		{
			BestDistSq = DistSq;
			Best = i;
		}
	}
	AutoPilotWaypointIndex = Best;

	// Racers do not use the traffic boost - see the Tick() branch that reads
	// this alongside AutoPilotWaypoints.Num().
	bAutoPilotBoostOnLaunch = false;
	bAutoPilotActive = true;
}

void ADCCRover::StopAutoPilot()
{
	bAutoPilotActive = false;
	AutoPilotWaypoints.Reset();
	AutoPilotWaypointIndex = 0;
	AutoPilotSteerInput = 0.0f;
	// Deliberately NOT clearing AutoPilotLapsCompleted: ADCCRaceTrack reads it
	// after the finish to decide the placings, and a race is only over once
	// somebody crossed the line. StartAutoPilotRoute zeroes it for the next run.
}

void ADCCRover::Server_RallyPickTrick_Implementation(uint8 Move)
{
	if (RallyTrick != 2 || !Driver || Move < 1 || Move > 4)
	{
		return;
	}
	RallyTrickQueued = Move;
	bRallyTrickPop = true;
}

void ADCCRover::Server_DriveSync_Implementation(FVector_NetQuantize10 Location, FRotator Rotation, FVector_NetQuantize10 Velocity, float Throttle, float Steer)
{
	if (FPlatformTime::Seconds() < IgnoreDriveSyncUntil)
	{
		return; // the host just moved this truck; wait for the driver to catch up
	}
	SetActorLocationAndRotation(Location, Rotation, false, nullptr, ETeleportType::TeleportPhysics);
	if (Movement)
	{
		Movement->Velocity = Velocity;
	}
	ThrottleInput = FMath::Clamp(Throttle, -1.0f, 1.0f);
	SteerInput = FMath::Clamp(Steer, -1.0f, 1.0f);
	LastDriveSyncLocation = GetActorLocation();
	bHasDriveSync = true;
}

void ADCCRover::Client_ForceTransform_Implementation(FVector_NetQuantize10 Location, FRotator Rotation)
{
	SetActorLocationAndRotation(Location, Rotation, false, nullptr, ETeleportType::TeleportPhysics);
	if (Movement)
	{
		Movement->StopMovementImmediately();
	}
}

void ADCCRover::Server_Spray_Implementation(FVector At, FVector Normal, FVector ViewDir, const TArray<uint8>& RGB)
{
	DCCSpray::ServerSpray(this, GetPlayerState(), At, Normal, ViewDir, RGB);
}
