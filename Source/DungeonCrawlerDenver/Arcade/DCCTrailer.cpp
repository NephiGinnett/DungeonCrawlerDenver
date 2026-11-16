#include "Arcade/DCCTrailer.h"

#include "Arcade/DCCArcadeGames.h"
#include "Arcade/DCCWalletComponent.h"
#include "Characters/DCCCharacterBase.h"
#include "Onboarding/DCCOnboardingComponent.h"
#include "Camera/CameraActor.h"
#include "Camera/CameraComponent.h"
#include "Components/CapsuleComponent.h"
#include "Components/TextRenderComponent.h"
#include "Engine/World.h"
#include "EngineUtils.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "GameFramework/GameModeBase.h"
#include "GameFramework/HUD.h"
#include "GameFramework/PlayerController.h"
#include "HAL/IConsoleManager.h"

namespace
{
	float Ease(float A) { return FMath::SmoothStep(0.0f, 1.0f, FMath::Clamp(A, 0.0f, 1.0f)); }

	/** Where a shot is up to, 0..1, given its start and end in trailer seconds. */
	float Phase(float T, float Start, float End) { return FMath::Clamp((T - Start) / (End - Start), 0.0f, 1.0f); }

	// The mountain (see ADCCArcadeClimb): wide block to 2400, narrow block to 4800.
	const float Wide = 1200.0f, Narrow = 700.0f, TopZ = 16000.0f; // ADCCArcadeClimb WideHalf, NarrowHalf, TopZ
}

ADCCTrailerDirector::ADCCTrailerDirector()
{
	PrimaryActorTick.bCanEverTick = true;
	PrimaryActorTick.TickGroup = TG_PostPhysics;
	SetReplicates(false);
}

ADCCCharacterBase* ADCCTrailerDirector::SpawnCrawler(FName Who, const FVector& At, float Yaw, FName Cosmetic, bool bClimbing)
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
		Crawler->Nameplate->SetVisibility(false); // a cleaner frame for the ad
	}
	if (UCapsuleComponent* Capsule = Crawler->GetCapsuleComponent())
	{
		Capsule->SetCollisionEnabled(ECollisionEnabled::NoCollision); // posed, never pushed
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
	if (bClimbing)
	{
		Crawler->PlayClimbAnimation(1000.0f);
	}
	Crew.Add(Crawler);
	return Crawler;
}

void ADCCTrailerDirector::Setup()
{
	UWorld* World = GetWorld();
	for (TActorIterator<ADCCArcadeClimb> It(World); It; ++It)
	{
		Origin = It->GetActorLocation();
		break;
	}
	APlayerController* PC = World->GetFirstPlayerController();
	if (PC)
	{
		// A clean frame: no HUD, no engine messages, no player body in shot.
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
			if (ADCCCharacterBase* Mine = Cast<ADCCCharacterBase>(Pawn))
			{
				if (Mine->Wallet) { Mine->Wallet->ApplyOutfit({}); }
			}
		}
	}
	// Steady streaming: the ad should not open on blurry textures.
	if (IConsoleVariable* Pool = IConsoleManager::Get().FindConsoleVariable(TEXT("r.Streaming.PoolSize")))
	{
		Pool->Set(4000);
	}

	const float Stand = 96.0f;
	// Scott: north face (+Y), between ledges, struggling.
	Scott = SpawnCrawler(TEXT("Scott"), Origin + FVector(380, Wide + 40, 850), -90.0f, TEXT("TrailPack"), true);
	// Marcel and Vaughn: resting on the west face's 18 m rock (ledge centre x -900, y -220), looking out.
	Marcel = SpawnCrawler(TEXT("Marcel"), Origin + FVector(-Wide - 130, 150, 1600 + Stand), 180.0f, TEXT("TenGallon"), false);
	Vaughn = SpawnCrawler(TEXT("Vaughn"), Origin + FVector(-Wide - 150, -150, 1600 + Stand), 200.0f, TEXT("MinersHelmet"), false);
	// CJ: the express face (+X), upper half, heading for the top.
	CJ = SpawnCrawler(TEXT("CJ"), Origin + FVector(Narrow + 40, -480, TopZ - 1300), 180.0f, TEXT("HobbyJetpack"), true);
	// Kyle and Zoe: already at the summit, looking toward the fort.
	Kyle = SpawnCrawler(TEXT("Kyle"), Origin + FVector(160, 140, TopZ + Stand), 0.0f, TEXT("Cape"), false);
	Zoe = SpawnCrawler(TEXT("Zoe"), Origin + FVector(-60, 260, TopZ + Stand), 20.0f, TEXT("FloorBossCrown"), false);

	FActorSpawnParameters Params;
	Params.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
	Camera = World->SpawnActor<ACameraActor>(Origin, FRotator::ZeroRotator, Params);
	if (Camera)
	{
		Camera->GetCameraComponent()->SetFieldOfView(55.0f);
		Camera->GetCameraComponent()->bConstrainAspectRatio = false;
		Camera->GetCameraComponent()->PostProcessSettings.bOverride_MotionBlurAmount = true;
		Camera->GetCameraComponent()->PostProcessSettings.MotionBlurAmount = 0.0f; // crisp frames for the encode
		if (PC)
		{
			PC->SetViewTarget(Camera);
		}
	}
	bSetUp = true;
}

void ADCCTrailerDirector::Pose(float T)
{
	// Scott struggles: gains height, stalls, slips a little, grabs on again.
	if (ADCCCharacterBase* S = Scott.Get())
	{
		const float Local = FMath::Clamp(T - 9.0f, 0.0f, 9.0f);
		const float Z = 850.0f + 20.0f * Local + 30.0f * FMath::Sin(Local * 2.1f) - 14.0f * FMath::Max(0.0f, FMath::Sin(Local * 4.7f));
		S->SetActorLocation(Origin + FVector(380.0f + 6.0f * FMath::Sin(Local * 3.3f), Wide + 40.0f, Z));
	}
	// CJ climbs steadily up the express face during the crane, then is on top for the summit.
	if (ADCCCharacterBase* C = CJ.Get())
	{
		if (T < 37.6f)
		{
			const float A = Phase(T, 27.0f, 37.6f);
			C->SetActorLocation(Origin + FVector(Narrow + 40.0f, -480.0f, FMath::Lerp(TopZ - 1300.0f, TopZ - 80.0f, A)));
		}
		else if (C->GetActorLocation().Z < TopZ)
		{
			C->SetActorLocationAndRotation(Origin + FVector(120.0f, -160.0f, TopZ + 96.0f), FRotator(0, -10.0f, 0));
			C->PlayClimbAnimation(0.01f); // let go of the climb pose
		}
	}
}

void ADCCTrailerDirector::Shoot(float T)
{
	if (!Camera)
	{
		return;
	}
	FVector From, LookAt;
	if (T < 9.0f)
	{
		// Establishing: in over the fort to the peak.
		const float A = Ease(Phase(T, 0.0f, 9.0f));
		From = FMath::Lerp(FVector(11000, -3800, 500), FVector(8800, -2600, 900), A);
		LookAt = FMath::Lerp(FVector(2500, 0, 2500), FVector(0, 0, 5200), A);
	}
	else if (T < 18.0f)
	{
		// The struggle: close and a little below, drifting along the face.
		const float A = Ease(Phase(T, 9.0f, 18.0f));
		const FVector S = Scott.IsValid() ? Scott->GetActorLocation() - Origin : FVector(380, 790, 950);
		From = S + FVector(FMath::Lerp(-420.0f, -180.0f, A), 620.0f, 40.0f);
		LookAt = S + FVector(0, 0, 60);
	}
	else if (T < 27.0f)
	{
		// The rest: a slow orbit out in front of the ledge.
		const float A = Ease(Phase(T, 18.0f, 27.0f));
		const FVector Ledge(-Wide - 150, 0, 1700); // the 16 m ledge on the west face (ledges every 8 m)
		const float Angle = FMath::DegreesToRadians(FMath::Lerp(150.0f, 205.0f, A));
		From = Ledge + FVector(FMath::Cos(Angle) * 700.0f, FMath::Sin(Angle) * 700.0f, 160.0f);
		LookAt = Ledge + FVector(0, 0, 40);
	}
	else if (T < 38.0f)
	{
		// The crane: rise up the express face past CJ to the summit.
		const float A = Ease(Phase(T, 27.0f, 38.0f));
		From = FVector(Narrow + 1300.0f, -900.0f, FMath::Lerp(TopZ - 1800.0f, TopZ + 650.0f, A));
		const FVector C = CJ.IsValid() ? CJ->GetActorLocation() - Origin : FVector(Narrow + 40, -480, TopZ - 1000);
		LookAt = FMath::Lerp(C + FVector(0, 0, 60), FVector(0, 0, TopZ + 150), Ease(Phase(T, 31.0f, 38.0f)));
	}
	else
	{
		// The summit, the fort and the range beyond.
		const float A = Ease(Phase(T, 38.0f, Length));
		From = FMath::Lerp(FVector(-1500, -1250, TopZ + 650), FVector(-1250, -1000, TopZ + 550), A);
		LookAt = FMath::Lerp(FVector(100, 100, TopZ + 120), FVector(400, 120, TopZ + 60), A);
	}
	From += Origin;
	LookAt += Origin;
	Camera->SetActorLocationAndRotation(From, (LookAt - From).Rotation());
}

void ADCCTrailerDirector::Tick(float DeltaSeconds)
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
	if (PC && Camera && PC->GetViewTarget() != Camera)
	{
		PC->SetViewTarget(Camera);
	}
	if (bCapture && Elapsed >= PreRoll && PC)
	{
		PC->ConsoleCommand(FString::Printf(TEXT("HighResShot 1920x1080 filename=%s/frame_%05d.png"), *CaptureDir, Frame++));
	}
	if (T >= Length)
	{
		bDone = true;
		UE_LOG(LogTemp, Display, TEXT("[DCC Trailer] done, %d frames"), Frame);
		if (bCapture && PC)
		{
			PC->ConsoleCommand(TEXT("quit"));
		}
	}
}

static void DCCTrailerCommand(const TArray<FString>& Args, UWorld* World)
{
	if (!World)
	{
		return;
	}
	FActorSpawnParameters Params;
	Params.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
	ADCCTrailerDirector* Director = World->SpawnActor<ADCCTrailerDirector>(FVector::ZeroVector, FRotator::ZeroRotator, Params);
	if (Director && Args.Num() > 0 && Args[0].Equals(TEXT("capture"), ESearchCase::IgnoreCase))
	{
		Director->bCapture = true;
		Director->CaptureDir = Args.Num() > 1 ? Args[1] : FString(TEXT("D:/Builds/TrailerFrames"));
	}
	UE_LOG(LogTemp, Display, TEXT("[DCC Trailer] rolling%s"), Director && Director->bCapture ? TEXT(", capturing frames") : TEXT(""));
}

static FAutoConsoleCommandWithWorldAndArgs GTrailerCommand(
	TEXT("DCC.Trailer"),
	TEXT("DCC.Trailer [capture <dir>]: play the 45 s Fourteener ad camera cut (in a Fourteener round). Tools/make_trailer.py renders it with the song."),
	FConsoleCommandWithWorldAndArgsDelegate::CreateStatic(&DCCTrailerCommand));
