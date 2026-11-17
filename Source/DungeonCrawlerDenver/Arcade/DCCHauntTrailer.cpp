#include "Arcade/DCCHauntTrailer.h"

#include "Arcade/DCCArcadeHaunt.h"
#include "Arcade/DCCWalletComponent.h"
#include "Animation/AnimSequence.h"
#include "Camera/CameraActor.h"
#include "Camera/CameraComponent.h"
#include "Characters/DCCCharacterBase.h"
#include "Characters/DCCEnemyCharacter.h"
#include "Characters/DCCEnemyCharacter_Wildlife.h"
#include "Components/CapsuleComponent.h"
#include "Components/SkeletalMeshComponent.h"
#include "Components/TextRenderComponent.h"
#include "Engine/World.h"
#include "Engine/StaticMeshActor.h"
#include "Engine/StaticMesh.h"
#include "Engine/PointLight.h"
#include "Components/PointLightComponent.h"
#include "EngineUtils.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "GameFramework/GameModeBase.h"
#include "GameFramework/PlayerController.h"
#include "HAL/IConsoleManager.h"
#include "Onboarding/DCCOnboardingComponent.h"
#include "Rooms/DCCDungeonGrid.h"
#include "Stats/DCCStatsComponent.h"
#include "UI/DCCHUD.h"
#include "UnrealClient.h"

namespace
{
	float Ease(float A) { return FMath::SmoothStep(0.0f, 1.0f, FMath::Clamp(A, 0.0f, 1.0f)); }
	float Phase(float T, float Start, float End) { return FMath::Clamp((T - Start) / (End - Start), 0.0f, 1.0f); }

	const TCHAR* ClipWalk = TEXT("/Game/DCC/Characters/Shared/AnimsFixed/A_DCC_HW_Walking.A_DCC_HW_Walking");
	const TCHAR* ClipRun = TEXT("/Game/DCC/Characters/Shared/AnimsFixed/A_DCC_HW_Running.A_DCC_HW_Running");
	const TCHAR* ClipAlert = TEXT("/Game/DCC/Characters/Shared/AnimsFixed/A_DCC_OA_Alert.A_DCC_OA_Alert");
	const TCHAR* ClipPoint = TEXT("/Game/DCC/Characters/Shared/AnimsFixed/A_DCC_DW_CallGesture.A_DCC_DW_CallGesture");
	const TCHAR* ClipFight = TEXT("/Game/DCC/Characters/Shared/AnimsFixed/A_DCC_OA_BoxingWarmup.A_DCC_OA_BoxingWarmup");
	const TCHAR* ClipCheer = TEXT("/Game/DCC/Characters/Shared/AnimsFixed/A_DCC_HW_BoomDance.A_DCC_HW_BoomDance");
	const TCHAR* ClipSquat = TEXT("/Game/DCC/Characters/Shared/AnimsFixed/A_DCC_AO_AirSquat.A_DCC_AO_AirSquat");

	const FName Names[] = { TEXT("Zoe"), TEXT("Kyle"), TEXT("Marcel"), TEXT("Scott"), TEXT("CJ") };
	const FName Hats[] = { TEXT("MinersHelmet"), TEXT("Cape"), TEXT("TenGallon"), TEXT("TrailPack"), TEXT("HobbyJetpack") };
}

ADCCHauntTrailerDirector::ADCCHauntTrailerDirector()
{
	PrimaryActorTick.bCanEverTick = true;
	PrimaryActorTick.TickGroup = TG_PostPhysics; // after the arena's tick, so its ghost flicker can be overruled
	SetReplicates(false);
}

ADCCCharacterBase* ADCCHauntTrailerDirector::SpawnCrawler(FName Who, FName Cosmetic)
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
	ADCCCharacterBase* Crawler = World->SpawnActor<ADCCCharacterBase>(PawnClass, Cage + FVector(0, 0, 120), FRotator::ZeroRotator, Params);
	if (!Crawler)
	{
		return nullptr;
	}
	Crawler->SetReplicates(false);
	Crawler->DisplayName = FText::FromName(Who);
	if (Crawler->Nameplate) { Crawler->Nameplate->SetVisibility(false); }
	if (UCapsuleComponent* Capsule = Crawler->GetCapsuleComponent()) { Capsule->SetCollisionEnabled(ECollisionEnabled::NoCollision); }
	if (UCharacterMovementComponent* Movement = Crawler->GetCharacterMovement()) { Movement->DisableMovement(); }
	if (Crawler->Onboarding)
	{
		Crawler->Onboarding->CrawlerId = Who;
		Crawler->Onboarding->ApplyCrawlerLook();
	}
	if (Crawler->Wallet && !Cosmetic.IsNone())
	{
		Crawler->Wallet->ApplyOutfit({ Cosmetic });
	}
	if (Crawler->Stats)
	{
		// Traps go off around them; nobody dies in an ad.
		Crawler->Stats->MaxHitPoints = 100000;
		Crawler->Stats->CurrentHitPoints = 100000;
	}
	if (USkeletalMeshComponent* Body = Crawler->GetMesh())
	{
		Body->SetAnimationMode(EAnimationMode::AnimationSingleNode);
	}
	Crawler->bRecordingFootage = true; // red lights over everyone filming
	Crawler->SetActorTickEnabled(true);
	SetClip(Crawler, ClipAlert);
	Crew.Add(Crawler);
	return Crawler;
}

void ADCCHauntTrailerDirector::SetClip(ADCCCharacterBase* Crawler, const TCHAR* Path)
{
	if (!Crawler)
	{
		return;
	}
	UAnimSequence* Clip = LoadObject<UAnimSequence>(nullptr, Path);
	if (Clip && Crawler->IdleAnimation != Clip)
	{
		Crawler->IdleAnimation = Clip;
		Crawler->WalkAnimation = Clip;
		Crawler->RunAnimation = Clip;
		Crawler->RestartLoopAnimation();
	}
}

FVector ADCCHauntTrailerDirector::OpenDir(const FVector& From) const
{
	const ADCCArcadeHaunt* H = Haunt.Get();
	FVector Best(1, 0, 0);
	float BestClear = -1.0f;
	if (!H || !H->Grid)
	{
		return Best;
	}
	for (int32 a = 0; a < 8; ++a)
	{
		const float Ang = a * PI / 4.0f;
		const FVector D(FMath::Cos(Ang), FMath::Sin(Ang), 0.0f);
		float Clear = 0.0f;
		while (Clear < 2200.0f)
		{
			const FIntPoint Cell = H->Grid->CellOf(From + D * (Clear + 110.0f));
			if (!H->Grid->IsOpen(Cell.X, Cell.Y)) { break; }
			Clear += 110.0f;
		}
		if (Clear > BestClear) { BestClear = Clear; Best = D; }
	}
	return Best;
}

FVector ADCCHauntTrailerDirector::ClearShot(const FVector& LookAt, const FVector& From) const
{
	FCollisionQueryParams Params(TEXT("TrailerLens"), true);
	for (const TObjectPtr<ADCCCharacterBase>& C : Crew) { Params.AddIgnoredActor(C); }
	if (Brute) { Params.AddIgnoredActor(Brute); }
	if (Camera) { Params.AddIgnoredActor(Camera); }
	FHitResult Hit;
	if (GetWorld() && GetWorld()->LineTraceSingleByChannel(Hit, LookAt, From, ECC_Visibility, Params))
	{
		return Hit.Location + (LookAt - From).GetSafeNormal() * 60.0f;
	}
	return From;
}

void ADCCHauntTrailerDirector::Setup()
{
	UWorld* World = GetWorld();
	for (TActorIterator<ADCCArcadeHaunt> It(World); It; ++It)
	{
		Haunt = *It;
		break;
	}
	ADCCArcadeHaunt* H = Haunt.Get();
	if (!H || !H->bHouseBuilt || !H->Grid || H->Grid->GetRooms().IsEmpty())
	{
		return; // the mine is not built yet
	}

	// The round holds still for the camera: no clock, no random traps, no monsters.
	const double Now = H->ServerTime();
	H->EndsAt = Now + 3600.0;
	H->NextRockAt = H->NextCartAt = H->NextSnuffAt = H->NextLieAt = Now + 3600.0;
	for (const TWeakObjectPtr<ADCCEnemyCharacter>& Monster : H->Monsters)
	{
		if (ADCCEnemyCharacter* M = Monster.Get()) { M->Destroy(); }
	}
	for (auto& Pocket : H->Pockets) { Pocket.QuietUntil = Now + 3600.0; }

	// Where the beats are.
	Cage = H->VanBox.IsValid ? H->VanBox.GetCenter() : H->Grid->CellCenter(H->Grid->GetRooms()[0].Center());
	Cage.Z = H->GetActorLocation().Z;
	if (H->Carts.Num() > 0) { RunA = H->Carts[0].A; RunB = H->Carts[0].B; }
	else { RunA = Cage + FVector(1200, 0, 0); RunB = Cage + FVector(3200, 0, 0); }
	for (int32 i = 0; i < H->Anomalies.Num(); ++i)
	{
		if (H->Anomalies[i].bGhost) { GhostIndex = i; GhostAt = H->Anomalies[i].Location - FVector(0, 0, 110); break; }
	}
	CrateAt = H->Crates.Num() > 0 ? H->Crates[0].At : RunB;
	GasAt = H->Pockets.Num() > 0 ? H->Pockets[0].At : RunA;
	RockAt = FMath::Lerp(RunA, RunB, 0.25f);
	GhostDir = OpenDir(GhostAt);
	CrateDir = OpenDir(CrateAt);
	if (H->Anomalies.IsValidIndex(GhostIndex))
	{
		if (AActor* Ghost = H->Anomalies[GhostIndex].Visual.Get())
		{
			Ghost->SetActorRotation(FRotator(0.0f, GhostDir.Rotation().Yaw, 0.0f));
		}
		// A cold light on him, so he reads on camera.
		FActorSpawnParameters LightParams;
		LightParams.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
		if (APointLight* Cold = World->SpawnActor<APointLight>(GhostAt + GhostDir * 140.0f + FVector(0, 0, 230), FRotator::ZeroRotator, LightParams))
		{
			Cold->GetLightComponent()->SetMobility(EComponentMobility::Movable);
			Cold->GetLightComponent()->SetLightColor(FLinearColor(0.45f, 0.75f, 1.0f));
			Cold->GetLightComponent()->SetIntensity(9000.0f);
			Cold->PointLightComponent->SetAttenuationRadius(650.0f);
			Cold->GetLightComponent()->SetCastShadows(false);
		}
	}
	UE_LOG(LogTemp, Display, TEXT("[DCC HauntTrailer] seed %d: cart %s, ghost %s, crate %s, gas %s"), H->Seed,
		H->Carts.Num() ? TEXT("yes") : TEXT("NO"), GhostIndex >= 0 ? TEXT("yes") : TEXT("NO"), H->Crates.Num() ? TEXT("yes") : TEXT("NO"), H->Pockets.Num() ? TEXT("yes") : TEXT("NO"));

	APlayerController* PC = World->GetFirstPlayerController();
	if (PC)
	{
		PC->ConsoleCommand(TEXT("DisableAllScreenMessages"));
		PC->SetIgnoreMoveInput(true);
		PC->SetIgnoreLookInput(true);
		if (APawn* Pawn = PC->GetPawn())
		{
			Pawn->SetActorHiddenInGame(true);
			Pawn->SetActorEnableCollision(false);
			Pawn->SetActorLocation(H->GetActorLocation() + FVector(-20000, -20000, -5000)); // out of every shot
		}
	}
	ADCCHUD::bTrailerMinimal = true;

	for (int32 i = 0; i < 5; ++i)
	{
		SpawnCrawler(Names[i], Hats[i]);
	}
	// Something to fight beside the powder: a Thornback that stands its ground.
	{
		const FTransform At(FRotator(0, 200, 0), CrateAt + FVector(260, 160, 120));
		Brute = World->SpawnActorDeferred<ADCCEnemyCharacter>(ADCCEnemyCharacter_Thornback::StaticClass(), At, nullptr, nullptr,
			ESpawnActorCollisionHandlingMethod::AlwaysSpawn);
		if (Brute)
		{
			Brute->AutoPossessAI = EAutoPossessAI::Disabled;
			Brute->FinishSpawning(At);
			Brute->SetReplicates(false);
			if (UCharacterMovementComponent* Movement = Brute->GetCharacterMovement()) { Movement->DisableMovement(); }
			if (Brute->Stats) { Brute->Stats->MaxHitPoints = Brute->Stats->CurrentHitPoints = 100000; }
		}
	}

	FActorSpawnParameters Params;
	Params.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
	Camera = World->SpawnActor<ACameraActor>(Cage, FRotator::ZeroRotator, Params);
	if (Camera)
	{
		Camera->GetCameraComponent()->SetFieldOfView(55.0f);
		Camera->GetCameraComponent()->bConstrainAspectRatio = false;
		if (PC) { PC->SetViewTarget(Camera); }
	}
	bSetUp = true;
}

void ADCCHauntTrailerDirector::PlaceCrew(const FVector& Centre, const FVector& Facing, float Spread, const FVector& Look)
{
	// A loose huddle around Centre, everyone turned toward Look.
	for (int32 i = 0; i < Crew.Num(); ++i)
	{
		ADCCCharacterBase* C = Crew[i];
		if (!C) { continue; }
		const float A = (i - 2) * 0.55f;
		const FVector Side(-Facing.Y, Facing.X, 0.0f);
		const FVector At = Centre + Side * (A * Spread) - Facing * (FMath::Abs(i - 2) * Spread * 0.35f) + FVector(0, 0, 120);
		C->SetActorLocationAndRotation(At, FRotator(0.0f, (Look - At).Rotation().Yaw, 0.0f));
	}
}

void ADCCHauntTrailerDirector::Cue(float T)
{
	ADCCArcadeHaunt* H = Haunt.Get();
	if (!H)
	{
		return;
	}
	const double Now = H->ServerTime();
	auto Once = [&](FName Beat, float At) { if (T >= At && !Fired.Contains(Beat)) { Fired.Add(Beat); BeatTime.Add(Beat, Now); return true; } return false; };

	if (Once(TEXT("Ghost"), 14.0f)) {}
	if (Once(TEXT("Cart"), 20.0f) && H->Carts.Num() > 0)
	{
		H->Carts[0].bAtB = false;
		H->AddEvent(2, 0, RunA, FString(), false); // the rumble; it runs from 3.5 s later
	}
	if (Once(TEXT("Blast"), 30.0f) && H->Crates.Num() > 0)
	{
		H->Crates[0].bGone = true;
		H->AddEvent(3, 0, CrateAt);
	}
	if (Once(TEXT("Flash"), 37.0f))
	{
		H->AddEvent(4, 0, GasAt);
	}
	if (Once(TEXT("Rock"), 39.5f))
	{
		FDCCMineRock Rock;
		Rock.Id = H->NextRockId++;
		Rock.At = RockAt;
		Rock.FallsAt = Now + 2.6;
		H->Rocks.Add(Rock);
	}
	if (Once(TEXT("Extract"), 48.0f))
	{
		H->Uploaders.Reset();
		for (const FName& N : Names) { H->Uploaders.Add(N.ToString()); }
		H->ExtractStart = Now;
		H->ReelPhase = 1;
	}
	if (Once(TEXT("Feed"), 52.0f))
	{
		// One post each, from the moment they filmed. Comments pile up on each one.
		struct FPost { const TCHAR* Who; FName Beat; const TCHAR* Title; int32 Likes; };
		const FPost Posts[] = {
			{ TEXT("Zoe"), TEXT("Ghost"), TEXT("GHOST MINER"), 48200 },
			{ TEXT("Kyle"), TEXT("Cart"), TEXT("RUNAWAY CART"), 31700 },
			{ TEXT("Marcel"), TEXT("Blast"), TEXT("POWDER"), 22900 },
			{ TEXT("Scott"), TEXT("Flash"), TEXT("FIREDAMP"), 15400 },
		};
		H->Reel.Reset();
		for (int32 i = 0; i < 4; ++i)
		{
			FDCCReelClip Post;
			Post.Cams.Add(Posts[i].Who);
			const double At = BeatTime.FindRef(Posts[i].Beat) + (Posts[i].Beat == TEXT("Cart") ? 4.0 : 0.6);
			Post.From = At - 2.0;
			Post.To = At + 2.6;
			Post.Title = Posts[i].Title;
			Post.Likes = Posts[i].Likes;
			Post.Comments = Posts[i].Likes / 60;
			Post.Shares = Posts[i].Likes / 140;
			Post.Rank = i + 1;
			Post.Tickets = i == 0 ? 30 : i == 1 ? 20 : i == 2 ? 12 : 6;
			Post.Credits = i == 0 ? 15 : i == 1 ? 10 : i == 2 ? 6 : 3;
			H->Reel.Add(Post);
		}
		H->ReelStart = Now;
		H->ReelPhase = 2;
		H->FeedIndex = 0;
		H->FeedSince = Now;
		H->FeedAutoSeconds = 3.0;
	}
}

void ADCCHauntTrailerDirector::Pose(float T)
{
	ADCCArcadeHaunt* H = Haunt.Get();
	if (!H)
	{
		return;
	}
	const FVector Dir = (RunB - RunA).GetSafeNormal2D();
	const FVector Perp(-Dir.Y, Dir.X, 0.0f);

	if (T < 5.0f || (T >= 44.0f && T < 52.0f))
	{
		// The cage: in a row facing out, lamps on, red lights blinking. At 44 s they come running back in.
		const bool bBack = T >= 44.0f;
		const float In = bBack ? Ease(Phase(T, 44.0f, 46.5f)) : 1.0f;
		for (int32 i = 0; i < Crew.Num(); ++i)
		{
			if (ADCCCharacterBase* C = Crew[i])
			{
				const FVector Spot = Cage + FVector(-120.0f + (i % 2) * 140.0f, (i - 2) * 150.0f, 120.0f);
				// In through the cage's gate (2026-11-16), not through the bars.
				const FVector GateSide(-H->GateDir.Y, H->GateDir.X, 0.0f);
				const FVector From = Cage + H->GateDir * (900.0f + i * 80.0f) + GateSide * ((i - 2) * 110.0f) + FVector(0, 0, 120.0f);
				const float RunYaw = (Spot - From).Rotation().Yaw;
				C->SetActorLocationAndRotation(bBack ? FMath::Lerp(From, Spot, In) : Spot, FRotator(0.0f, bBack && In < 1.0f ? RunYaw : 180.0f + (i - 2) * 6.0f, 0.0f));
				SetClip(C, bBack && In < 1.0f ? ClipRun : (bBack ? ClipCheer : (i % 2 ? ClipSquat : ClipAlert)));
			}
		}
	}
	else if (T < 13.0f)
	{
		// Single file along the rails, lamps sweeping.
		const float P = Ease(Phase(T, 5.0f, 13.0f));
		const FVector Lead = FMath::Lerp(RunA - Dir * 300.0f, FMath::Lerp(RunA, RunB, 0.55f), P);
		for (int32 i = 0; i < Crew.Num(); ++i)
		{
			if (ADCCCharacterBase* C = Crew[i])
			{
				const FVector At = Lead - Dir * (i * 230.0f) + Perp * ((i % 2) ? 70.0f : -70.0f) + FVector(0, 0, 120);
				C->SetActorLocationAndRotation(At, FRotator(0.0f, Dir.Rotation().Yaw + FMath::Sin(T * 1.3f + i) * 12.0f, 0.0f));
				SetClip(C, ClipWalk);
			}
		}
	}
	else if (T < 19.0f)
	{
		// The ghost: from an open cell a few metres off, everyone turned to it. Zoe points; the others hold still.
		const FVector Side(-GhostDir.Y, GhostDir.X, 0.0f);
		const FVector Huddle = GhostAt + GhostDir * 520.0f + Side * 170.0f;
		PlaceCrew(Huddle, -GhostDir, 150.0f, GhostAt);
		// Puppets do not collide, so clear any cart or crate standing where they huddle, for this beat only.
		if (!bClearedHuddle)
		{
			bClearedHuddle = true;
			for (TActorIterator<AStaticMeshActor> It(GetWorld()); It; ++It)
			{
				const UStaticMeshComponent* Mesh = It->GetStaticMeshComponent();
				if (!Mesh || !Mesh->GetStaticMesh() || Mesh->GetStaticMesh()->GetName().Contains(TEXT("Cube")) || It->IsHidden())
				{
					continue; // walls, floors and roof are cubes
				}
				const FBox Box = It->GetComponentsBoundingBox();
				const float Tall = Box.GetSize().Z;
				if (Tall > 20.0f && Tall < 260.0f && FVector::Dist2D(Box.GetCenter(), Huddle) < 520.0f)
				{
					It->SetActorHiddenInGame(true);
					Cleared.Add(*It);
				}
			}
		}
		for (int32 i = 0; i < Crew.Num(); ++i) { SetClip(Crew[i], i == 0 ? ClipPoint : ClipAlert); }
		if (H->Anomalies.IsValidIndex(GhostIndex))
		{
			if (AActor* Ghost = H->Anomalies[GhostIndex].Visual.Get()) { Ghost->SetActorHiddenInGame(T > 17.6f && T < 18.0f); } // one flicker
		}
	}
	else if (T < 27.0f)
	{
		for (const TWeakObjectPtr<AActor>& Prop : Cleared) { if (AActor* A = Prop.Get()) { A->SetActorHiddenInGame(false); } }
		Cleared.Reset();
		// On the rails as the cart comes; at 24.6 s everyone dives clear, both ways.
		const FVector Mid = FMath::Lerp(RunA, RunB, 0.62f);
		const float Dive = Ease(Phase(T, 24.4f, 24.9f));
		for (int32 i = 0; i < Crew.Num(); ++i)
		{
			if (ADCCCharacterBase* C = Crew[i])
			{
				const float Side = (i % 2) ? 1.0f : -1.0f;
				const FVector At = Mid + Dir * ((i - 2) * 120.0f) + Perp * Side * (60.0f + 330.0f * Dive) + FVector(0, 0, 120);
				C->SetActorLocationAndRotation(At, FRotator(0.0f, (-Dir).Rotation().Yaw + Side * 25.0f * Dive, 0.0f));
				SetClip(C, T < 22.5f ? ClipPoint : (Dive > 0.0f ? ClipAlert : ClipAlert));
			}
		}
	}
	else if (T < 33.0f)
	{
		// Marcel and CJ squared up to a Thornback by the powder; the rest film from a distance.
		const FVector Side(-CrateDir.Y, CrateDir.X, 0.0f);
		if (Brute)
		{
			const FVector BruteAt = CrateAt + CrateDir * 230.0f + FVector(0, 0, 120);
			Brute->SetActorLocationAndRotation(BruteAt, FRotator(0.0f, CrateDir.Rotation().Yaw, 0.0f));
		}
		for (int32 i = 0; i < Crew.Num(); ++i)
		{
			if (ADCCCharacterBase* C = Crew[i])
			{
				FVector At;
				if (i == 2) { At = CrateAt + CrateDir * 470.0f - Side * 110.0f + FVector(0, 0, 120); }
				else if (i == 4) { At = CrateAt + CrateDir * 450.0f + Side * 120.0f + FVector(0, 0, 120); }
				else { At = CrateAt + CrateDir * (1050.0f + i * 50.0f) + Side * ((i - 2) * 110.0f) + FVector(0, 0, 120); }
				const bool bBlown = T > 30.1f && (i == 2 || i == 4);
				if (bBlown) { At += CrateDir * (Ease(Phase(T, 30.0f, 30.5f)) * 380.0f); }
				C->SetActorLocationAndRotation(At, FRotator(0.0f, (CrateAt - At).Rotation().Yaw, 0.0f));
				SetClip(C, (i == 2 || i == 4) && !bBlown ? ClipFight : (i == 0 ? ClipPoint : ClipAlert));
			}
		}
		if (Brute)
		{
			Brute->SetActorHiddenInGame(T > 30.3f);
		}
	}
	else if (T < 39.0f)
	{
		// In the firedamp: a tight huddle, every lamp burning blue.
		PlaceCrew(GasAt, Dir, 170.0f, GasAt + Dir * 1000.0f);
		for (int32 i = 0; i < Crew.Num(); ++i) { SetClip(Crew[i], T > 37.0f ? ClipAlert : (i == 3 ? ClipPoint : ClipWalk)); }
	}
	else if (T < 44.0f)
	{
		// Three knocks over their heads. They look up, then scatter as it comes down.
		const float Back = Ease(Phase(T, 41.6f, 42.2f));
		for (int32 i = 0; i < Crew.Num(); ++i)
		{
			if (ADCCCharacterBase* C = Crew[i])
			{
				const float A = i * (2.0f * PI / 5.0f);
				const FVector Ring(FMath::Cos(A), FMath::Sin(A), 0.0f);
				const FVector At = RockAt + Ring * (150.0f + 220.0f * Back) + FVector(0, 0, 120);
				C->SetActorLocationAndRotation(At, FRotator(0.0f, (RockAt - At).Rotation().Yaw, 0.0f));
				SetClip(C, Back > 0.0f ? ClipRun : ClipAlert);
			}
		}
	}
	if (Brute && (T < 27.0f || T >= 33.0f))
	{
		Brute->SetActorHiddenInGame(true);
	}
}

void ADCCHauntTrailerDirector::Shoot(float T)
{
	if (!Camera)
	{
		return;
	}
	const FVector Dir = (RunB - RunA).GetSafeNormal2D();
	const FVector Perp(-Dir.Y, Dir.X, 0.0f);
	FVector From, LookAt;
	float Fov = 55.0f;

	if (T < 5.0f)
	{
		const float A = Ease(Phase(T, 0.0f, 5.0f));
		From = Cage + FMath::Lerp(FVector(-1300, -500, 420), FVector(-800, -150, 230), A);
		LookAt = Cage + FVector(-80, 0, 150);
		Fov = 50.0f;
	}
	else if (T < 13.0f)
	{
		const float P = Ease(Phase(T, 5.0f, 13.0f));
		const FVector Lead = FMath::Lerp(RunA - Dir * 300.0f, FMath::Lerp(RunA, RunB, 0.55f), P);
		From = Lead + Dir * 700.0f + Perp * 120.0f + FVector(0, 0, 230);
		LookAt = Lead - Dir * 300.0f + FVector(0, 0, 130);
		Fov = 58.0f;
	}
	else if (T < 19.0f)
	{
		// Over the crew's shoulders at the ghost, pushing in.
		const float A = Ease(Phase(T, 13.0f, 19.0f));
		const FVector Side(-GhostDir.Y, GhostDir.X, 0.0f);
		From = GhostAt + GhostDir * FMath::Lerp(1150.0f, 900.0f, A) - Side * 180.0f + FVector(0, 0, 260);
		LookAt = GhostAt + GhostDir * 220.0f + FVector(0, 0, 120);
		Fov = FMath::Lerp(55.0f, 44.0f, A);
	}
	else if (T < 27.0f)
	{
		// Low at the end of the run: the crew on the rails, the cart coming at the lens behind them.
		From = FMath::Lerp(RunA, RunB, 1.0f) + Dir * 250.0f + Perp * 260.0f + FVector(0, 0, 120);
		LookAt = FMath::Lerp(RunA, RunB, 0.45f) + FVector(0, 0, 110);
		Fov = 52.0f;
	}
	else if (T < 33.0f)
	{
		const FVector Side(-CrateDir.Y, CrateDir.X, 0.0f);
		From = CrateAt + CrateDir * 1600.0f + Side * 150.0f + FVector(0, 0, 320);
		LookAt = CrateAt + CrateDir * 380.0f + FVector(0, 0, 120);
		Fov = 50.0f;
	}
	else if (T < 39.0f)
	{
		// In front of them, low, so the blue lamps shine at the lens.
		const float A = Ease(Phase(T, 33.0f, 39.0f));
		From = GasAt + Dir * FMath::Lerp(900.0f, 700.0f, A) + FVector(0, 0, 160);
		LookAt = GasAt + FVector(0, 0, 150);
		Fov = 52.0f;
	}
	else if (T < 44.0f)
	{
		From = RockAt + FVector(-900, -700, 520);
		LookAt = RockAt + FVector(0, 0, 160);
		Fov = 55.0f;
	}
	else
	{
		// Back in the cage, then pulled out for the extraction and the feed.
		const float A = Ease(Phase(T, 44.0f, 50.0f));
		From = Cage + FMath::Lerp(FVector(-700, -260, 220), FVector(-1100, -600, 520), A);
		LookAt = Cage + FVector(0, 0, 140);
		Fov = 52.0f;
	}
	From = ClearShot(LookAt, From);
	Camera->SetActorLocationAndRotation(From, (LookAt - From).Rotation());
	Camera->GetCameraComponent()->SetFieldOfView(Fov);
}

void ADCCHauntTrailerDirector::Tick(float DeltaSeconds)
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
	Cue(T);
	Pose(T);
	Shoot(T);
	APlayerController* PC = GetWorld()->GetFirstPlayerController();
	if (PC && Camera && PC->GetViewTarget() != Camera)
	{
		PC->SetViewTarget(Camera);
	}
	// With the HUD (the extraction bars and the feed are HUD), one frame per tick.
	if (bCapture && Elapsed >= PreRoll)
	{
		FScreenshotRequest::RequestScreenshot(FString::Printf(TEXT("%s/frame_%05d.png"), *CaptureDir, Frame++), /*bShowUI=*/true, /*bAddFilenameSuffix=*/false);
	}
	if (T >= Length)
	{
		bDone = true;
		ADCCHUD::bTrailerMinimal = false;
		UE_LOG(LogTemp, Display, TEXT("[DCC HauntTrailer] done, %d frames"), Frame);
		if (bCapture && PC)
		{
			PC->ConsoleCommand(TEXT("quit"));
		}
	}
}

static void DCCHauntTrailerCommand(const TArray<FString>& Args, UWorld* World)
{
	if (!World)
	{
		return;
	}
	FActorSpawnParameters Params;
	Params.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
	ADCCHauntTrailerDirector* Director = World->SpawnActor<ADCCHauntTrailerDirector>(FVector::ZeroVector, FRotator::ZeroRotator, Params);
	if (Director && Args.Num() > 0 && Args[0].Equals(TEXT("capture"), ESearchCase::IgnoreCase))
	{
		Director->bCapture = true;
		Director->CaptureDir = Args.Num() > 1 ? Args[1] : FString(TEXT("D:/Builds/HauntTrailerFrames"));
	}
	UE_LOG(LogTemp, Display, TEXT("[DCC HauntTrailer] rolling%s"), Director && Director->bCapture ? TEXT(", capturing frames") : TEXT(""));
}

static FAutoConsoleCommandWithWorldAndArgs GHauntTrailerCommand(
	TEXT("DCC.HauntTrailer"),
	TEXT("DCC.HauntTrailer [capture <dir>]: play the Dead Air ad (in a Dead Air round). Tools/make_trailer.py --haunt renders it."),
	FConsoleCommandWithWorldAndArgsDelegate::CreateStatic(&DCCHauntTrailerCommand));
