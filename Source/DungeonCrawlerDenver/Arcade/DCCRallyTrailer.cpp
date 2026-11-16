#include "Arcade/DCCRallyTrailer.h"

#include "Arcade/DCCArcadeGames.h"
#include "Arcade/DCCRallyThug.h"
#include "Engine/TextRenderActor.h"
#include "Arcade/DCCWalletComponent.h"
#include "Animation/AnimSequence.h"
#include "Camera/CameraActor.h"
#include "Camera/CameraComponent.h"
#include "Characters/DCCCharacterBase.h"
#include "Components/CapsuleComponent.h"
#include "Components/SkeletalMeshComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Components/TextRenderComponent.h"
#include "Engine/StaticMesh.h"
#include "Engine/StaticMeshActor.h"
#include "Engine/World.h"
#include "Materials/MaterialInterface.h"
#include "EngineUtils.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "GameFramework/FloatingPawnMovement.h"
#include "GameFramework/GameModeBase.h"
#include "GameFramework/HUD.h"
#include "GameFramework/PlayerController.h"
#include "HAL/IConsoleManager.h"
#include "Dialogue/DCCDialogueNPC.h"
#include "Onboarding/DCCOnboardingComponent.h"
#include "Vehicles/DCCRover.h"

namespace
{
	float Ease(float A) { return FMath::SmoothStep(0.0f, 1.0f, FMath::Clamp(A, 0.0f, 1.0f)); }
	float Phase(float T, float Start, float End) { return FMath::Clamp((T - Start) / (End - Start), 0.0f, 1.0f); }

	const TCHAR* AnimAxe = TEXT("/Game/DCC/Characters/Shared/AnimsFixed/A_DCC_OA_AxeStance.A_DCC_OA_AxeStance");
	const TCHAR* AnimBoom = TEXT("/Game/DCC/Characters/Shared/AnimsFixed/A_DCC_HW_BoomDance.A_DCC_HW_BoomDance");
	const TCHAR* AnimDance = TEXT("/Game/DCC/Characters/Shared/AnimsFixed/A_DCC_AO_AllNightDance.A_DCC_AO_AllNightDance");
	const TCHAR* AnimSquat = TEXT("/Game/DCC/Characters/Shared/AnimsFixed/A_DCC_AO_AirSquat.A_DCC_AO_AirSquat");
	const TCHAR* AnimBox = TEXT("/Game/DCC/Characters/Shared/AnimsFixed/A_DCC_OA_BoxingWarmup.A_DCC_OA_BoxingWarmup");
	const TCHAR* AnimCall = TEXT("/Game/DCC/Characters/Shared/AnimsFixed/A_DCC_DW_CallGesture.A_DCC_DW_CallGesture");
	const TCHAR* AnimAim = TEXT("/Game/DCC/Characters/Shared/AnimsFixed/A_DCC_AO_ArcheryAim.A_DCC_AO_ArcheryAim");
	const TCHAR* AnimShot = TEXT("/Game/DCC/Characters/Shared/AnimsFixed/A_DCC_AO_ArcheryShot.A_DCC_AO_ArcheryShot");
	const TCHAR* AnimAlert = TEXT("/Game/DCC/Characters/Shared/AnimsFixed/A_DCC_OA_Alert.A_DCC_OA_Alert");

	// The owner beat happens in the street where the hook beat left Kyle's truck.
	FVector StreetOf(const FVector& Yard) { return Yard + FVector(9400.0f, 500.0f, 0.0f); }

	// The hook beat. TruckX/Gap are yard-local. Strain is 1 while the mark is fighting the line.
	void HookWhere(float T, const FVector& Yard, float Ride, FVector& TruckAt, FVector& MarkAt, float& Strain)
	{
		float TruckX = 5600.0f;
		float Gap = 1900.0f;
		Strain = 0.0f;
		if (T >= 10.0f && T < 16.0f)
		{
			const float A = Ease(Phase(T, 10.0f, 16.0f));
			TruckX = FMath::Lerp(5600.0f, 6600.0f, A);
			Gap = FMath::Lerp(1900.0f, 2350.0f, A);
			Strain = 1.0f;
		}
		else if (T >= 16.0f && T < 24.0f)
		{
			const float A = Ease(Phase(T, 16.0f, 24.0f));
			// Out the gate and into the middle of the street, not on to the far curb: at 12000 the
			// wrecker's nose sat inside the corner tower across the road (2026-11-14).
			TruckX = FMath::Lerp(6600.0f, 9400.0f, A);
			Gap = FMath::Lerp(2350.0f, 780.0f, A);
			Strain = 1.0f - A;
		}
		else if (T >= 24.0f)
		{
			TruckX = 9400.0f;
			Gap = 780.0f;
		}
		TruckAt = Yard + FVector(TruckX, 500.0f, Ride);
		MarkAt = TruckAt - FVector(Gap, 0.0f, 0.0f);
	}

	// Matches BuildStreetFun's first skyway: along X, on the y=+1 street's north lane.
	const float SkyFixed = ADCCArcadeKart::StreetSpacing + 500.0f;
	const float SkyFrom = -14000.0f;
	const float RampLen = 6000.0f;
	const float Rise = 900.0f;

	FVector YardOf(const FVector& Origin)
	{
		const FVector2D Off = ADCCArcadeKart::YardOffset();
		return Origin + FVector(Off.X, Off.Y, 0.0f);
	}
}

ADCCRallyTrailerDirector::ADCCRallyTrailerDirector()
{
	PrimaryActorTick.bCanEverTick = true;
	PrimaryActorTick.TickGroup = TG_PostPhysics;
	SetReplicates(false);
}

ADCCCharacterBase* ADCCRallyTrailerDirector::SpawnCrawler(FName Who, const FVector& At, float Yaw, FName Cosmetic, const TCHAR* IdlePath)
{
	UWorld* World = GetWorld();
	const AGameModeBase* Mode = World ? World->GetAuthGameMode() : nullptr;
	UClass* PawnClass = Mode && Mode->DefaultPawnClass ? Mode->DefaultPawnClass.Get() : nullptr;
	if (!PawnClass || !PawnClass->IsChildOf(ADCCCharacterBase::StaticClass()))
	{
		return nullptr;
	}
	FActorSpawnParameters Params;
	Params.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
	ADCCCharacterBase* Crawler = World->SpawnActor<ADCCCharacterBase>(PawnClass, At, FRotator(0, Yaw, 0), Params);
	if (!Crawler)
	{
		return nullptr;
	}
	Crawler->SetReplicates(false);
	Crawler->DisplayName = FText::FromName(Who);
	if (Crawler->Nameplate)
	{
		Crawler->Nameplate->SetVisibility(false);
	}
	if (UCapsuleComponent* Capsule = Crawler->GetCapsuleComponent())
	{
		Capsule->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	}
	if (UCharacterMovementComponent* Movement = Crawler->GetCharacterMovement())
	{
		Movement->DisableMovement();
	}
	if (Crawler->Onboarding)
	{
		Crawler->Onboarding->CrawlerId = Who;
		Crawler->Onboarding->ApplyCrawlerLook();
	}
	if (Crawler->Wallet && !Cosmetic.IsNone())
	{
		Crawler->Wallet->ApplyOutfit({ Cosmetic });
	}
	// The player pawn's AnimBlueprint is for the mannequin. These bodies are the shared Meshy
	// skeleton, which plays clips directly. Without a loop they stand in the bind pose.
	if (USkeletalMeshComponent* Body = Crawler->GetMesh())
	{
		Body->SetAnimationMode(EAnimationMode::AnimationSingleNode);
	}
	if (UAnimSequence* Clip = LoadObject<UAnimSequence>(nullptr, IdlePath))
	{
		Crawler->IdleAnimation = Clip;
		Crawler->WalkAnimation = Clip;
		Crawler->RunAnimation = Clip;
	}
	Crawler->RestartLoopAnimation();
	Crew.Add(Crawler);
	return Crawler;
}

ADCCRover* ADCCRallyTrailerDirector::SpawnTruck(const FVector& At, float Yaw, int32 Body, const FString& Plate)
{
	UWorld* World = GetWorld();
	FActorSpawnParameters Params;
	Params.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
	ADCCRover* Car = World ? World->SpawnActor<ADCCRover>(ADCCRover::StaticClass(), FTransform(FRotator(0, Yaw, 0), At), Params) : nullptr;
	if (!Car)
	{
		return nullptr;
	}
	Car->SetReplicates(false);
	Car->SetActorEnableCollision(false);
	Car->bRallyHeld = true; // stay where we put it; the street trace would plant a skyway shot
	Car->bAutoPilotActive = false;
	Car->RallyPlate = Plate;
	Car->SetRallyBody(Body);
	if (Car->Movement)
	{
		Car->Movement->StopMovementImmediately();
		Car->Movement->Deactivate();
	}
	return Car;
}

void ADCCRallyTrailerDirector::Place(ADCCRover* Truck, const FVector& At, float Yaw)
{
	if (!Truck)
	{
		return;
	}
	Truck->SetActorLocationAndRotation(At, FRotator(0, Yaw, 0));
	if (Truck->Movement)
	{
		Truck->Movement->StopMovementImmediately();
	}
}

void ADCCRallyTrailerDirector::Setup()
{
	UWorld* World = GetWorld();
	ADCCArcadeKart* City = nullptr;
	for (TActorIterator<ADCCArcadeKart> It(World); It; ++It)
	{
		City = *It;
		break;
	}
	if (!City)
	{
		return; // the round has not built the city yet
	}
	Origin = City->GetActorLocation();

	APlayerController* PC = World->GetFirstPlayerController();
	if (PC)
	{
		if (PC->MyHUD)
		{
			PC->MyHUD->bShowHUD = false;
		}
		PC->ConsoleCommand(TEXT("DisableAllScreenMessages"));
		PC->SetIgnoreMoveInput(true);
		PC->SetIgnoreLookInput(true);
		if (APawn* Pawn = PC->GetPawn())
		{
			Pawn->SetActorHiddenInGame(true);
			Pawn->SetActorEnableCollision(false);
		}
	}
	for (TActorIterator<ADCCRover> It(World); It; ++It)
	{
		if (It->IsOccupied())
		{
			It->SetActorHiddenInGame(true);
			It->SetActorEnableCollision(false);
			if (It->Driver)
			{
				It->Driver->SetActorHiddenInGame(true);
			}
		}
	}
	if (IConsoleVariable* Pool = IConsoleManager::Get().FindConsoleVariable(TEXT("r.Streaming.PoolSize")))
	{
		Pool->Set(4000);
	}

	const FVector Yard = YardOf(Origin);
	const float Stand = Yard.Z + 100.0f;
	const float Ride = Yard.Z + 150.0f;
	const float BayY[] = { -5200.0f, -3600.0f, 3600.0f, 5200.0f };
	const int32 BayBody[] = { 0, 1, 2, 3 };

	BayScott = SpawnTruck(Yard + FVector(1400, BayY[0], Ride - Yard.Z), 0.0f, BayBody[0], TEXT("SCOTT"));
	BayCJ = SpawnTruck(Yard + FVector(1400, BayY[1], Ride - Yard.Z), 0.0f, BayBody[1], TEXT("CJ"));
	BayMarcel = SpawnTruck(Yard + FVector(1400, BayY[2], Ride - Yard.Z), 0.0f, BayBody[2], TEXT("MARCEL"));
	BayVaughn = SpawnTruck(Yard + FVector(1400, BayY[3], Ride - Yard.Z), 0.0f, BayBody[3], TEXT("VAUGHN"));
	for (const TWeakObjectPtr<ADCCRover>& Bay : { BayScott, BayCJ, BayMarcel, BayVaughn })
	{
		if (ADCCRover* Truck = Bay.Get()) { Truck->SetWrecker(true); }
	}
	Scott = SpawnCrawler(TEXT("Scott"), Yard + FVector(1000, BayY[0] + 340, Stand - Yard.Z), 20.0f, TEXT("TrailPack"), AnimAxe);
	CJ = SpawnCrawler(TEXT("CJ"), Yard + FVector(1000, BayY[1] + 340, Stand - Yard.Z), -15.0f, TEXT("HobbyJetpack"), AnimBoom);
	Marcel = SpawnCrawler(TEXT("Marcel"), Yard + FVector(1000, BayY[2] + 340, Stand - Yard.Z), 10.0f, TEXT("TenGallon"), AnimDance);
	Vaughn = SpawnCrawler(TEXT("Vaughn"), Yard + FVector(1000, BayY[3] + 340, Stand - Yard.Z), -10.0f, TEXT("MinersHelmet"), AnimSquat);

	// Kyle at the hitch, fists up at the mark. Pose() drives the pull.
	FVector HookAt, MarkAt;
	float Strain = 0.0f;
	HookWhere(0.0f, Yard, Ride - Yard.Z, HookAt, MarkAt, Strain);
	HookTruck = SpawnTruck(HookAt, 0.0f, 4, TEXT("KYLE"));
	if (ADCCRover* Truck = HookTruck.Get()) { Truck->SetWrecker(true); }
	HookMark = SpawnTruck(MarkAt, 0.0f, 2, TEXT("MARK"));
	Kyle = SpawnCrawler(TEXT("Kyle"), HookAt + FVector(-180, 340, Stand - Ride), -80.0f, TEXT("Cape"), AnimBox);

	// Zoe starts in the cab. The glass fades out over the preroll (body 0 is the mesh with a Window slot).
	const FVector SkyBase = Origin + FVector(SkyFrom + 800.0f, SkyFixed, 150.0f);
	SkyTruck = SpawnTruck(SkyBase, 0.0f, 0, TEXT("ZOE"));
	if (ADCCRover* Truck = SkyTruck.Get()) { Truck->SetWrecker(true); }
	if (ADCCRover* Truck = SkyTruck.Get())
	{
		Truck->bWindowsDown = true;
		Truck->WindowRollSeconds = 0.05f;
	}
	Zoe = SpawnCrawler(TEXT("Zoe"), SkyBase + FVector(50, -50, -15), -30.0f, TEXT("FloorBossCrown"), AnimCall);

	DropMark = SpawnTruck(Yard + FVector(1920, 4000, Ride - Yard.Z), 0.0f, 1, TEXT("MARK"));
	if (ADCCRover* Parked = DropMark.Get())
	{
		Parked->SetActorHiddenInGame(true);
	}

	FActorSpawnParameters Params;
	Params.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
	{
		// The mark's owner, out of a doorway and furious; his friend brought a rifle. The Plate picks the body and the yell.
		const FVector Street = StreetOf(Yard);
		auto Thug = [&](uint8 Kind, const FString& Plate, const FVector& At) -> ADCCRallyThug*
		{
			const FTransform Where(FRotator(0, 150, 0), At);
			ADCCRallyThug* T = World->SpawnActorDeferred<ADCCRallyThug>(ADCCRallyThug::StaticClass(), Where, nullptr, nullptr, ESpawnActorCollisionHandlingMethod::AlwaysSpawn);
			if (T)
			{
				T->Kind = Kind;
				T->Plate = Plate;
				T->FinishSpawning(Where);
				T->SetReplicates(false);
				T->SetActorEnableCollision(false);
				T->SetActorHiddenInGame(true);
			}
			return T;
		};
		Angry = Thug(ADCCRallyThug::Yeller, TEXT("MARK"), Street + FVector(1800, -1500, Stand - Yard.Z));
		Gunner = Thug(ADCCRallyThug::Shooter, TEXT("MARK-2"), Street + FVector(1700, -900, Stand - Yard.Z));
		ShoutCard = World->SpawnActor<ATextRenderActor>(Street, FRotator::ZeroRotator, Params);
		if (ATextRenderActor* Card = ShoutCard.Get())
		{
			Card->GetTextRender()->SetHorizontalAlignment(EHTA_Center);
			Card->GetTextRender()->SetWorldSize(26.0f);
			Card->GetTextRender()->SetTextRenderColor(FColor(255, 70, 60));
			Card->SetActorHiddenInGame(true);
		}
	}
	Cable = World->SpawnActor<AStaticMeshActor>(Origin, FRotator::ZeroRotator, Params);
	if (Cable)
	{
		if (UStaticMesh* Cube = LoadObject<UStaticMesh>(nullptr, TEXT("/Engine/BasicShapes/Cube.Cube")))
		{
			UStaticMeshComponent* Mesh = Cable->GetStaticMeshComponent();
			Mesh->SetStaticMesh(Cube);
			Mesh->SetMobility(EComponentMobility::Movable);
			Mesh->SetCollisionEnabled(ECollisionEnabled::NoCollision);
			Mesh->SetCastShadow(false);
			if (UMaterialInterface* Neon = LoadObject<UMaterialInterface>(nullptr, TEXT("/Game/Synthwave_city/Material/MI_neon_1.MI_neon_1")))
			{
				Mesh->SetMaterial(0, Neon);
			}
		}
		Cable->SetActorHiddenInGame(true);
	}

	UStaticMesh* Cube = LoadObject<UStaticMesh>(nullptr, TEXT("/Engine/BasicShapes/Cube.Cube"));
	UMaterialInterface* NeonPink = LoadObject<UMaterialInterface>(nullptr, TEXT("/Game/Synthwave_city/Material/MI_neon_1.MI_neon_1"));
	UMaterialInterface* NeonCyan = LoadObject<UMaterialInterface>(nullptr, TEXT("/Game/Synthwave_city/Material/MI_neon_4.MI_neon_4"));
	for (int32 i = 0; i < 14; ++i)
	{
		AStaticMeshActor* Line = World->SpawnActor<AStaticMeshActor>(Origin, FRotator::ZeroRotator, Params);
		if (!Line || !Cube)
		{
			continue;
		}
		UStaticMeshComponent* Mesh = Line->GetStaticMeshComponent();
		Mesh->SetStaticMesh(Cube);
		Mesh->SetMobility(EComponentMobility::Movable);
		Mesh->SetCollisionEnabled(ECollisionEnabled::NoCollision);
		Mesh->SetCastShadow(false);
		if (UMaterialInterface* Neon = (i % 2 == 0) ? NeonPink : NeonCyan)
		{
			Mesh->SetMaterial(0, Neon);
		}
		Line->SetActorHiddenInGame(true);
		Line->SetActorEnableCollision(false);
		Stress.Add(Line);
	}

	Camera = World->SpawnActor<ACameraActor>(Origin, FRotator::ZeroRotator, Params);
	if (Camera)
	{
		Camera->GetCameraComponent()->SetFieldOfView(50.0f);
		Camera->GetCameraComponent()->bConstrainAspectRatio = false;
		Camera->GetCameraComponent()->PostProcessSettings.bOverride_MotionBlurAmount = true;
		Camera->GetCameraComponent()->PostProcessSettings.MotionBlurAmount = 0.0f;
		if (PC)
		{
			PC->SetViewTarget(Camera);
		}
	}
	bSetUp = true;
	UE_LOG(LogTemp, Display, TEXT("[DCC RallyTrailer] city at %s"), *Origin.ToString());

}

void ADCCRallyTrailerDirector::Pose(float T)
{
	const FVector Yard = YardOf(Origin);
	const float Stand = 100.0f;
	const float Ride = 150.0f;

	// Pin the bay trucks. Scott's leaves its bay for the drop.
	const float BayY[] = { -5200.0f, -3600.0f, 3600.0f, 5200.0f };
	ADCCRover* Bays[] = { BayScott.Get(), BayCJ.Get(), BayMarcel.Get(), BayVaughn.Get() };
	for (int32 i = 0; i < 4; ++i)
	{
		if (i == 0 && T >= DropStart)
		{
			continue;
		}
		Place(Bays[i], Yard + FVector(1400.0f, BayY[i], Ride), 0.0f);
	}

	// The hook. 10-16 s the mark holds and the line stretches. 16-24 s it comes in and gets dragged out the gate.
	FVector HookAt, MarkAt;
	float Strain = 0.0f;
	HookWhere(T, Yard, Ride, HookAt, MarkAt, Strain);
	const bool bHook = T < 24.0f;
	if (ADCCRover* Truck = HookTruck.Get())
	{
		Truck->SetActorHiddenInGame(!bHook);
		if (bHook)
		{
			Place(Truck, HookAt, Strain * 4.0f * FMath::Sin(T * 11.0f));
		}
		// From the cut at 10 s the crane pays the magnet out onto the mark. The beam is the wrecker's own line.
		Truck->RallyTow = (bHook && T >= 10.0f) ? HookMark.Get() : nullptr;
	}
	if (ADCCRover* Mark = HookMark.Get())
	{
		Mark->SetActorHiddenInGame(!bHook);
		if (bHook)
		{
			Place(Mark, MarkAt, Strain * 10.0f * FMath::Sin(T * 14.0f));
		}
	}
	if (T < 24.0f)
	{
		if (ADCCCharacterBase* K = Kyle.Get())
		{
			// Far side of the hitch, so the camera (it sits on -Y) sees the cars first and him behind them.
			K->SetActorLocationAndRotation(HookAt + FVector(-160.0f, 360.0f, Stand - Ride), FRotator(0.0f, -70.0f, 0.0f));
		}
	}

	// Zoe rides in the wrecker cab. The cab is the nose, about 2 m ahead of the actor
	// origin, and the driver's side is -Y (the camera sits out there).
	if (ADCCRover* Truck = SkyTruck.Get())
	{
		const float Along = (T >= 24.0f && T < 34.0f)
			? FMath::Lerp(SkyFrom + 800.0f, 9000.0f, Ease(Phase(T, 24.0f, 34.0f)))
			: (T >= 34.0f ? 9000.0f : SkyFrom + 800.0f);
		const float Up = FMath::Clamp((Along - SkyFrom) / RampLen, 0.0f, 1.0f);
		const float Z = (Along < SkyFrom + RampLen ? Rise * Up : Rise) + Ride;
		const FVector At = Origin + FVector(Along, SkyFixed, Z);
		Place(Truck, At, 0.0f);
		if (ADCCCharacterBase* Who = Zoe.Get())
		{
			const FRotator Face(0.0f, -35.0f, 0.0f);
			Who->SetActorLocationAndRotation(At + FVector(220.0f, -45.0f, -10.0f), Face);
		}
	}

	OwnerBeat(T);

	// The drop: Scott's truck leaves its bay and the mark is on the bumper.
	if (T >= DropStart)
	{
		const FVector TruckAt = Yard + FVector(2700, 0, Ride);
		const FVector DropAt = TruckAt - FVector(780, 0, 0);
		Place(BayScott.Get(), TruckAt, 0.0f);
		if (ADCCRover* ScottTruck = BayScott.Get())
		{
			ScottTruck->RallyTow = DropMark.Get();
		}
		if (ADCCRover* Mark = DropMark.Get())
		{
			Mark->SetActorHiddenInGame(false);
			Place(Mark, DropAt, 0.0f);
		}
		const FVector Spots[] = {
			FVector(1500, 520, Stand), FVector(1700, -560, Stand), FVector(2100, 640, Stand), FVector(2300, -620, Stand),
		};
		ADCCCharacterBase* Who[] = { Scott.Get(), CJ.Get(), Marcel.Get(), Vaughn.Get() };
		const float Face[] = { 40.0f, -50.0f, 20.0f, -15.0f };
		for (int32 i = 0; i < 4; ++i)
		{
			if (Who[i])
			{
				Who[i]->SetActorLocationAndRotation(Yard + Spots[i], FRotator(0, Face[i], 0));
			}
		}
		if (ADCCCharacterBase* K = Kyle.Get())
		{
			// Fists up, toward the camera.
			K->SetActorLocationAndRotation(Yard + FVector(1900, -200, Stand), FRotator(0, -40.0f, 0));
		}
	}

	// The wrecker draws its own beam from the boom to the magnet. The old bumper cable would double it.
	if (Cable)
	{
		Cable->SetActorHiddenInGame(true);
	}

	// Stress lines fan off the mark's roof, where the magnet is stuck, only while the line is contested.
	const bool bShowLine = bHook && T >= 10.0f && HookTruck.IsValid() && HookMark.IsValid();
	const FVector Hitch = MarkAt + FVector(0.0f, 0.0f, 130.0f);
	for (int32 i = 0; i < Stress.Num(); ++i)
	{
		AStaticMeshActor* Line = Stress[i];
		if (!Line)
		{
			continue;
		}
		const bool bShow = bShowLine && Strain > 0.08f;
		Line->SetActorHiddenInGame(!bShow);
		if (!bShow)
		{
			continue;
		}
		const float Ang = (static_cast<float>(i) / static_cast<float>(Stress.Num())) * PI + T * 1.4f;
		const float Flicker = 0.65f + 0.35f * FMath::Sin(T * 22.0f + i * 1.7f);
		// Side camera sits on -Y, so the lines live in the XZ plane where that camera can see them.
		const float Len = (180.0f + 420.0f * Strain * Flicker);
		const FVector Unit = FVector(FMath::Sin(Ang), 0.12f * ((i % 2) ? 1.0f : -1.0f), FMath::Cos(Ang)).GetSafeNormal();
		Line->SetActorLocationAndRotation(Hitch + Unit * (40.0f + Len * 0.5f), Unit.Rotation());
		Line->SetActorScale3D(FVector(Len / 100.0f, 0.14f, 0.14f));
	}
}

void ADCCRallyTrailerDirector::Shoot(float T)
{
	if (!Camera)
	{
		return;
	}
	const FVector Yard = YardOf(Origin);
	const float Ride = 150.0f;
	FVector From, LookAt;
	float Fov = 50.0f;

	if (T < 3.0f)
	{
		// Short. On the sign, then a little way into the city. It does not linger up there.
		const float A = Ease(Phase(T, 0.0f, 3.0f));
		From = FMath::Lerp(Yard + FVector(2400, -1200, 380), Yard + FVector(6200, -2800, 1200), A);
		LookAt = FMath::Lerp(Yard + FVector(5200, 0, 760), Origin + FVector(4000, 5000, 900), A);
		Fov = FMath::Lerp(48.0f, 56.0f, A);
	}
	else if (T < 10.0f)
	{
		// Four hard cuts. A pan between the south bays and the north bays crosses an empty lot.
		const int32 I = FMath::Clamp(FMath::FloorToInt((T - 3.0f) / 1.75f), 0, 3);
		const float Stops[] = { -5200.0f, -3600.0f, 3600.0f, 5200.0f };
		const float Y = Stops[I];
		From = Yard + FVector(3100.0f, Y + 640.0f, 190.0f);
		LookAt = Yard + FVector(1150.0f, Y + 180.0f, 90.0f);
		Fov = 36.0f;
	}
	else if (T < 24.0f)
	{
		// Side-on, tracking the pair, so the line and the pull stay in frame.
		FVector TruckAt, MarkAt;
		float Strain = 0.0f;
		HookWhere(T, Yard, Ride, TruckAt, MarkAt, Strain);
		const FVector Mid = (TruckAt + MarkAt) * 0.5f;
		const bool bStruggle = T < 16.0f;
		const float Dist = bStruggle ? 3200.0f : 3900.0f;
		From = Mid + FVector(bStruggle ? -500.0f : -1100.0f, -Dist, bStruggle ? 340.0f : 480.0f);
		LookAt = Mid + FVector(180.0f, 0.0f, 30.0f);
		Fov = bStruggle ? 54.0f : 48.0f;
	}
	else if (T < 34.0f)
	{
		// Into the wrecker cab from the driver's side. She sits forward of the bed, in the nose.
		const FVector Truck = SkyTruck.IsValid() ? SkyTruck->GetActorLocation() : Origin + FVector(0, SkyFixed, Rise);
		From = Truck + FVector(40.0f, -480.0f, 120.0f);
		LookAt = Truck + FVector(210.0f, -20.0f, 30.0f);
		Fov = 34.0f;
	}
	else if (T < DropStart)
	{
		// The owner beat: wide on the street as he comes, then over Kyle's shoulder at the rifle, then back wide.
		const FVector Street = StreetOf(Yard);
		const FVector KyleAt = Street + FVector(260.0f, -360.0f, 100.0f);
		const FVector RifleAt = Street + FVector(1700.0f, -900.0f, 100.0f);
		const FVector Face = KyleAt + FVector(200.0f, -280.0f, 0.0f);
		if (T < 37.0f)
		{
			// Low and close beside the wrecker as the owner runs in at Kyle.
			const float A = Ease(Phase(T, 34.0f, 37.0f));
			From = KyleAt + FMath::Lerp(FVector(-520.0f, -760.0f, 170.0f), FVector(-380.0f, -620.0f, 150.0f), A);
			LookAt = FMath::Lerp(KyleAt + FVector(500.0f, -300.0f, 110.0f), (KyleAt + Face) * 0.5f + FVector(0, 0, 120.0f), A);
			Fov = 52.0f;
		}
		else if (T < 38.6f)
		{
			// Two-shot: the owner in Kyle's face, yelling; the rifle behind him.
			From = (KyleAt + Face) * 0.5f + FVector(-80.0f, -560.0f, 150.0f);
			LookAt = (KyleAt + Face) * 0.5f + FVector(60.0f, 0.0f, 130.0f);
			Fov = 46.0f;
		}
		else if (T < 40.3f)
		{
			// Over Kyle's shoulder at the rifle.
			const FVector Aim = (RifleAt - KyleAt).GetSafeNormal2D();
			const FVector Side(-Aim.Y, Aim.X, 0.0f);
			From = KyleAt - Aim * 230.0f - Side * 70.0f + FVector(0, 0, 170.0f);
			LookAt = RifleAt + FVector(0, 0, 110.0f);
			Fov = 36.0f;
		}
		else
		{
			// The rifle down, the owner running for it, Kyle and the truck in the back of the frame.
			From = RifleAt + FVector(-650.0f, -520.0f, 230.0f);
			LookAt = RifleAt + FVector(-150.0f, 0.0f, 60.0f);
			Fov = 50.0f;
		}
	}
	else
	{
		// The hitch, then back so the gate and the city sit behind the crew.
		const float A = Ease(Phase(T, DropStart, Length));
		From = FMath::Lerp(Yard + FVector(900, -1100, 220), Yard + FVector(-400, -1800, 700), A);
		LookAt = FMath::Lerp(Yard + FVector(2200, 0, 140), Yard + FVector(4000, 200, 400), A);
		Fov = FMath::Lerp(40.0f, 55.0f, A);
	}

	Camera->SetActorLocationAndRotation(From, (LookAt - From).Rotation());
	Camera->GetCameraComponent()->SetFieldOfView(Fov);
}

void ADCCRallyTrailerDirector::SetClip(ADCCCharacterBase* Crawler, const TCHAR* Path)
{
	UAnimSequence* Clip = Crawler ? LoadObject<UAnimSequence>(nullptr, Path) : nullptr;
	if (Clip && Crawler->IdleAnimation != Clip)
	{
		Crawler->IdleAnimation = Clip;
		Crawler->WalkAnimation = Clip;
		Crawler->RunAnimation = Clip;
		Crawler->RestartLoopAnimation();
	}
}

void ADCCRallyTrailerDirector::OwnerBeat(float T)
{
	const bool bOn = T >= 34.0f && T < DropStart;
	ADCCRallyThug* Yell = Angry.Get();
	ADCCRallyThug* Rifle = Gunner.Get();
	ATextRenderActor* Card = ShoutCard.Get();
	if (Yell) { Yell->SetActorHiddenInGame(!bOn); }
	if (Rifle) { Rifle->SetActorHiddenInGame(!bOn); }
	if (Card) { Card->SetActorHiddenInGame(true); }
	if (!bOn)
	{
		if (T >= DropStart) { SetClip(Kyle.Get(), AnimBox); }
		return;
	}
	const FVector Yard = YardOf(Origin);
	const FVector Street = StreetOf(Yard);
	const float Stand = Yard.Z + 100.0f;
	const float Ride = 150.0f;

	// Kyle's wrecker, the mark on the hook, stopped in the street.
	if (ADCCRover* Truck = HookTruck.Get())
	{
		Truck->SetActorHiddenInGame(false);
		Place(Truck, Street + FVector(0, 0, Ride), 0.0f);
		Truck->RallyTow = HookMark.Get();
	}
	if (ADCCRover* Mark = HookMark.Get())
	{
		Mark->SetActorHiddenInGame(false);
		Place(Mark, Street + FVector(-780.0f, 0, Ride), 0.0f);
	}
	// Kyle out of the cab on the near side. Squares up, then fires back.
	const FVector KyleAt = FVector(Street.X + 260.0f, Street.Y - 360.0f, Stand);
	const FVector RifleAt = FVector(Street.X + 1700.0f, Street.Y - 900.0f, Stand);
	if (ADCCCharacterBase* K = Kyle.Get())
	{
		const FVector Look = T < 37.0f ? FVector(Street.X + 1500.0f, Street.Y - 1200.0f, Stand) : RifleAt;
		K->SetActorLocationAndRotation(KyleAt, FRotator(0.0f, (Look - KyleAt).Rotation().Yaw, 0.0f));
		SetClip(K, T < 35.5f ? AnimAlert : T < 37.4f ? AnimBox : T < 38.6f ? AnimAim : AnimShot);
	}
	// The owner charges at Kyle, stops in his face and yells, then runs for it when his friend goes down.
	if (Yell)
	{
		const FVector From(Street.X + 1800.0f, Street.Y - 1500.0f, Stand);
		const FVector Face(KyleAt.X + 200.0f, KyleAt.Y - 280.0f, Stand);
		const FVector Away(Street.X + 2600.0f, Street.Y - 2400.0f, Stand);
		FVector At = FMath::Lerp(From, Face, Ease(Phase(T, 34.3f, 36.4f)));
		if (T >= 40.4f) { At = FMath::Lerp(Face, Away, Ease(Phase(T, 40.4f, DropStart))); }
		const FVector Look = T >= 40.4f ? Away : KyleAt;
		Yell->SetActorLocationAndRotation(At, FRotator(0.0f, (Look - At).Rotation().Yaw, 0.0f));
		Yell->SetMood(T < 36.4f || T >= 40.4f ? ADCCRallyThug::Run : ADCCRallyThug::Attack);
		if (T >= 36.2f) { Yell->TryShout(); }
		if (Card && T >= 36.2f && T < 40.6f)
		{
			Card->SetActorHiddenInGame(false);
			// The first sentence only: the whole yell is too long to read over a head.
			FString Line = Yell->ShoutText.IsEmpty() ? FString(TEXT("That's my car!")) : Yell->ShoutText;
			int32 Cut = INDEX_NONE;
			for (const TCHAR Stop : { TCHAR('.'), TCHAR('!'), TCHAR('?') })
			{
				int32 At2 = INDEX_NONE;
				if (Line.FindChar(Stop, At2) && At2 > 4 && (Cut == INDEX_NONE || At2 < Cut)) { Cut = At2; }
			}
			if (Cut != INDEX_NONE) { Line = Line.Left(Cut + 1); }
			if (Line.Len() > 34) { Line = Line.Left(32) + TEXT("..."); }
			Card->GetTextRender()->SetText(FText::FromString(Line.ToUpper()));
			Card->SetActorLocation(At + FVector(0, 0, 215));
			if (Camera)
			{
				const FVector ToCam = Camera->GetActorLocation() - Card->GetActorLocation();
				Card->SetActorRotation(FRotator(0.0f, ToCam.Rotation().Yaw, 0.0f));
			}
		}
	}
	// The friend with the rifle: two misses at the truck, then Kyle puts him down.
	if (Rifle)
	{
		Rifle->SetActorLocationAndRotation(RifleAt, FRotator(0.0f, (KyleAt - RifleAt).Rotation().Yaw, 0.0f));
		Rifle->SetMood(T >= 40.1f ? ADCCRallyThug::Down : T >= 37.0f ? ADCCRallyThug::Attack : ADCCRallyThug::Stand);
	}
	struct FShot { float At; bool bKyle; };
	const FShot Shots[] = { { 37.4f, false }, { 38.0f, false }, { 38.9f, true }, { 39.4f, true }, { 39.95f, true } };
	for (int32 i = 0; i < UE_ARRAY_COUNT(Shots); ++i)
	{
		if (T < Shots[i].At || ShotsFired.Contains(i))
		{
			continue;
		}
		ShotsFired.Add(i);
		if (Shots[i].bKyle)
		{
			ADCCRallyThug::PlayHit(GetWorld(), KyleAt + FVector(40, 0, 150), RifleAt + FVector(0, 0, 120), i == 4 ? 1 : 2);
		}
		else if (Rifle)
		{
			ADCCRallyThug::PlayHit(GetWorld(), Rifle->MuzzleLocation(), KyleAt + FVector(-200.0f + i * 160.0f, -60.0f, 170.0f), 2);
		}
	}
}

void ADCCRallyTrailerDirector::Tick(float DeltaSeconds)
{
	Super::Tick(DeltaSeconds);
	if (bDone)
	{
		return;
	}
	Elapsed += DeltaSeconds;
	if (!bSetUp)
	{
		if (Elapsed > 1.0f)
		{
			Setup();
		}
		return;
	}

	const float T = FMath::Max(0.0f, Elapsed - PreRoll);
	Pose(T);

	Shoot(T);

	APlayerController* PC = GetWorld()->GetFirstPlayerController();
	if (PC)
	{
		if (Camera && PC->GetViewTarget() != Camera)
		{
			PC->SetViewTarget(Camera);
		}
		if (APawn* Pawn = PC->GetPawn())
		{
			Pawn->SetActorHiddenInGame(true);
		}
	}
	for (TActorIterator<ADCCDialogueNPC> It(GetWorld()); It; ++It)
	{
		It->SetActorHiddenInGame(true);
	}
	for (TActorIterator<ADCCRover> It(GetWorld()); It; ++It)
	{
		const ADCCRover* Car = *It;
		const bool bOurs = Car == BayScott.Get() || Car == BayCJ.Get() || Car == BayMarcel.Get() || Car == BayVaughn.Get()
			|| Car == HookTruck.Get() || Car == HookMark.Get() || Car == SkyTruck.Get() || Car == DropMark.Get();
		if (!bOurs)
		{
			It->SetActorHiddenInGame(true);
			It->SetActorEnableCollision(false);
			if (It->Driver)
			{
				It->Driver->SetActorHiddenInGame(true);
			}
		}
	}
	if (bCapture && Elapsed >= PreRoll && PC)
	{
		PC->ConsoleCommand(FString::Printf(TEXT("HighResShot 1920x1080 filename=%s/frame_%05d.png"), *CaptureDir, Frame++));
	}
	if (T >= Length)
	{
		bDone = true;
		UE_LOG(LogTemp, Display, TEXT("[DCC RallyTrailer] done, %d frames"), Frame);
		if (bCapture && PC)
		{
			PC->ConsoleCommand(TEXT("quit"));
		}
	}
}

static void DCCRallyTrailerCommand(const TArray<FString>& Args, UWorld* World)
{
	if (!World)
	{
		return;
	}
	FActorSpawnParameters Params;
	Params.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
	ADCCRallyTrailerDirector* Director = World->SpawnActor<ADCCRallyTrailerDirector>(FVector::ZeroVector, FRotator::ZeroRotator, Params);
	if (Director && Args.Num() > 0 && Args[0].Equals(TEXT("capture"), ESearchCase::IgnoreCase))
	{
		Director->bCapture = true;
		Director->CaptureDir = Args.Num() > 1 ? Args[1] : FString(TEXT("D:/Builds/RallyTrailerFrames"));
	}
	UE_LOG(LogTemp, Display, TEXT("[DCC RallyTrailer] rolling%s"), Director && Director->bCapture ? TEXT(", capturing frames") : TEXT(""));
}

static FAutoConsoleCommandWithWorldAndArgs GRallyTrailerCommand(
	TEXT("DCC.RallyTrailer"),
	TEXT("DCC.RallyTrailer [capture <dir>]: play the 52 s Repo Rally ad (in a Repo Rally round). Tools/make_trailer.py --rally renders it."),
	FConsoleCommandWithWorldAndArgsDelegate::CreateStatic(&DCCRallyTrailerCommand));
