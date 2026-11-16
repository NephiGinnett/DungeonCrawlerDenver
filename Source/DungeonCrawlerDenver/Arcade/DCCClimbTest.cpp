// DCC.ClimbTest [face|all] - an automated climb up Fourteener that looks for anything abnormal (2026-11-13).
//
// Zoe: "test run a spot or face and detect for abnormal movement". In a Fourteener game (play_game.bat Climb,
// or -ExecCmds="DCC.ClimbTest all"), this puts the local crawler at the foot of a face (0 east/express,
// 1 north, 2 west, 3 south), equips Climbing, "holds the mouse" (ADCCCharacterBase::DebugSetClimbHold), keeps
// stamina topped up so the run tests movement rather than endurance, and records every frame until the
// summit or a time limit. Then it logs one summary line per face, tagged [DCC ClimbTest]:
//
//   jumps     a frame where the crawler moved much further than climbing or walking can (a snap or teleport)
//   stuck     time spent holding the button without rising, below the top
//   drops     climbs that ended in a fall rather than a mantle, a top-out or the summit
//   inRock    frames with the capsule overlapping solid world geometry
//   camRock   frames with the camera inside solid world geometry
//   mantles   pull-ups onto a ledge (expected: one per ledge on the route)
//
// Every abnormal frame is logged as it happens too, with its position, so a bad spot can be found again.

#include "Arcade/DCCArcadeGames.h"
#include "Characters/DCCCharacterBase.h"
#include "Stats/DCCStatsComponent.h"
#include "Camera/PlayerCameraManager.h"
#include "Components/CapsuleComponent.h"
#include "Containers/Ticker.h"
#include "Engine/World.h"
#include "EngineUtils.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "GameFramework/PlayerController.h"
#include "HAL/IConsoleManager.h"

namespace
{
	struct FClimbTest
	{
		TWeakObjectPtr<UWorld> World;
		TArray<int32> Faces;
		int32 FaceIndex = -1;
		double FaceStarted = 0.0;
		double LastRiseAt = 0.0;
		FVector LastLocation = FVector::ZeroVector;
		bool bWasClimbing = false;
		float MaxZ = 0.0f;
		int32 Jumps = 0, Drops = 0, InRock = 0, CamRock = 0, Mantles = 0, Frames = 0;
		float MaxJump = 0.0f, Stuck = 0.0f;
		bool bWaitingForPawn = true;
		double SummitAt = -1.0;
		float StaminaSpent = 0.0f, MaxStaminaSeen = 0.0f;
		double WaitStarted = 0.0;
		FTSTicker::FDelegateHandle Handle;
	};
	FClimbTest GTest;

	const float Wide = 1200.0f, MidZ = 8000.0f, TopZ = 16000.0f; // ADCCArcadeClimb's WideHalf, MidZ, TopZ

	FVector Normal(int32 Face)
	{
		switch (Face & 3) { case 0: return FVector(1, 0, 0); case 1: return FVector(0, 1, 0); case 2: return FVector(-1, 0, 0); default: return FVector(0, -1, 0); }
	}

	ADCCCharacterBase* LocalCrawler(UWorld* World, APlayerController*& OutPC)
	{
		OutPC = World ? World->GetFirstPlayerController() : nullptr;
		return OutPC ? Cast<ADCCCharacterBase>(OutPC->GetPawn()) : nullptr;
	}

	FVector Mountain(UWorld* World)
	{
		for (TActorIterator<ADCCArcadeClimb> It(World); It; ++It)
		{
			return It->GetActorLocation();
		}
		return FVector::ZeroVector;
	}

	void StartFace(UWorld* World)
	{
		APlayerController* PC = nullptr;
		ADCCCharacterBase* Crawler = LocalCrawler(World, PC);
		if (!Crawler)
		{
			return;
		}
		const int32 Face = GTest.Faces[GTest.FaceIndex];
		const FVector N = Normal(Face);
		const FVector Start = Mountain(World) + N * (Wide + 120.0f) + FVector(0, 0, 100.0f);
		Crawler->DebugSetClimbHold(false);
		Crawler->ReleaseClimb();
		Crawler->TeleportTo(Start, (-N).Rotation(), false, true);
		PC->SetControlRotation(FRotator(-10.0f, (-N).Rotation().Yaw, 0.0f));
		if (!Crawler->IsClimbingEquipped())
		{
			Crawler->SelectSkill(TEXT("Climbing"));
		}
		Crawler->FallSafeUntil = World->GetTimeSeconds() + 100000.0; // the test measures movement, not damage
		Crawler->DebugSetClimbHold(true);
		GTest.FaceStarted = GTest.LastRiseAt = World->GetTimeSeconds();
		GTest.LastLocation = Start;
		GTest.bWasClimbing = false;
		GTest.MaxZ = Start.Z;
		GTest.Jumps = GTest.Drops = GTest.InRock = GTest.CamRock = GTest.Mantles = GTest.Frames = 0;
		GTest.MaxJump = GTest.Stuck = 0.0f;
		GTest.SummitAt = -1.0;
		GTest.StaminaSpent = 0.0f;
		UE_LOG(LogTemp, Display, TEXT("[DCC ClimbTest] face %d: start at %s"), Face, *Start.ToCompactString());
	}

	void FinishFace(UWorld* World, const TCHAR* Why)
	{
		const int32 Face = GTest.Faces[GTest.FaceIndex];
		const float Height = (GTest.MaxZ - Mountain(World).Z) / 100.0f;
		UE_LOG(LogTemp, Display,
			TEXT("[DCC ClimbTest] face %d DONE (%s) in %.0f s: top %.1f m of %.0f, mantles %d, jumps %d (max %.0f cm), stuck %.1f s, drops %d, inRock %d/%d frames, camRock %d/%d frames"),
			Face, Why, World->GetTimeSeconds() - GTest.FaceStarted, Height, TopZ / 100.0f, GTest.Mantles, GTest.Jumps, GTest.MaxJump,
			GTest.Stuck, GTest.Drops, GTest.InRock, GTest.Frames, GTest.CamRock, GTest.Frames);
		const float Metres = FMath::Max(1.0f, Height);
		UE_LOG(LogTemp, Display, TEXT("[DCC ClimbTest] face %d stamina: %.0f spent over %.0f m = %.1f per metre, %.0f%% of a %.0f bar per 6 m pitch"),
			Face, GTest.StaminaSpent, Metres, GTest.StaminaSpent / Metres, GTest.MaxStaminaSeen > 0 ? 600.0f * GTest.StaminaSpent / Metres / GTest.MaxStaminaSeen : 0.0f, GTest.MaxStaminaSeen);
	}

	bool Tick(float DeltaTime)
	{
		UWorld* World = GTest.World.Get();
		APlayerController* PC = nullptr;
		ADCCCharacterBase* Crawler = World ? LocalCrawler(World, PC) : nullptr;
		if (!World)
		{
			return false;
		}
		if (GTest.bWaitingForPawn)
		{
			// Give the game a few seconds to settle (possession, saves, the fort being built).
			if (!Crawler || World->GetTimeSeconds() - GTest.WaitStarted < 4.0)
			{
				return true;
			}
			GTest.bWaitingForPawn = false;
			GTest.FaceIndex = 0;
			StartFace(World);
			return true;
		}
		if (!Crawler || GTest.FaceIndex < 0 || GTest.FaceIndex >= GTest.Faces.Num())
		{
			return false;
		}

		if (Crawler->Stats)
		{
			GTest.StaminaSpent += FMath::Max(0.0f, Crawler->Stats->MaxStamina - Crawler->Stats->CurrentStamina);
			GTest.MaxStaminaSeen = Crawler->Stats->MaxStamina;
			Crawler->Stats->RecoverStamina(1000.0f);
		}

		// On a ledge or High Camp's shelf, walk to the wall the way a player would.
		if (GTest.SummitAt < 0.0 && !Crawler->IsClimbing() && Crawler->GetCharacterMovement() && !Crawler->GetCharacterMovement()->IsFalling())
		{
			Crawler->AddMovementInput(-Normal(GTest.Faces[GTest.FaceIndex]), 1.0f);
		}

		const FVector Location = Crawler->GetActorLocation();
		const float Moved = FVector::Dist(Location, GTest.LastLocation);
		const bool bClimbing = Crawler->IsClimbing();
		const UCharacterMovementComponent* Movement = Crawler->GetCharacterMovement();
		const bool bFalling = Movement && Movement->IsFalling();
		++GTest.Frames;

		// Jumps: further than any climb, walk or fall could go in one frame, allowing for a mantle.
		const float Allowed = 60.0f + 1500.0f * DeltaTime;
		const bool bMantled = GTest.bWasClimbing && !bClimbing && !bFalling; // crawled over a ledge, or topped out
		if (bMantled)
		{
			++GTest.Mantles;
		}
		else if (Moved > Allowed)
		{
			++GTest.Jumps;
			GTest.MaxJump = FMath::Max(GTest.MaxJump, Moved);
			UE_LOG(LogTemp, Warning, TEXT("[DCC ClimbTest] JUMP %.0f cm at %s -> %s (climbing %d, falling %d)"),
				Moved, *GTest.LastLocation.ToCompactString(), *Location.ToCompactString(), GTest.bWasClimbing, bFalling);
		}
		if (GTest.bWasClimbing && !bClimbing && bFalling)
		{
			++GTest.Drops;
			UE_LOG(LogTemp, Warning, TEXT("[DCC ClimbTest] DROP from the wall at %s"), *Location.ToCompactString());
		}

		// Stuck: holding on, not rising, not at the top.
		if (Location.Z > GTest.MaxZ + 5.0f)
		{
			GTest.MaxZ = Location.Z;
			GTest.LastRiseAt = World->GetTimeSeconds();
		}
		else if (World->GetTimeSeconds() - GTest.LastRiseAt > 1.0)
		{
			// Say what is in the way, once, so a stall can be found and fixed.
			if (GTest.Stuck < 0.5f && GTest.Stuck + DeltaTime >= 0.5f)
			{
				FHitResult Above;
				FCollisionQueryParams Q(SCENE_QUERY_STAT(DCCClimbTestStall), false, Crawler);
				const bool bHit = World->SweepSingleByChannel(Above, Location, Location + FVector(0, 0, 60), FQuat::Identity, ECC_Pawn,
					FCollisionShape::MakeCapsule(30.0f, 86.0f), Q);
				UE_LOG(LogTemp, Warning, TEXT("[DCC ClimbTest] STALL at %s (climbing %d): above %s, hit %s normal %s"),
					*Location.ToCompactString(), Crawler->IsClimbing(), bHit && Above.GetActor() ? *Above.GetActor()->GetName() : TEXT("nothing"),
					*Above.ImpactPoint.ToCompactString(), *Above.ImpactNormal.ToCompactString());
			}
			GTest.Stuck += DeltaTime;
		}

		// In rock: the capsule, slightly shrunk, overlapping static world geometry.
		FCollisionQueryParams Params(SCENE_QUERY_STAT(DCCClimbTest), false, Crawler);
		if (const UCapsuleComponent* Capsule = Crawler->GetCapsuleComponent())
		{
			if (World->OverlapBlockingTestByChannel(Location, FQuat::Identity, ECC_WorldStatic,
				FCollisionShape::MakeCapsule(Capsule->GetScaledCapsuleRadius() - 6.0f, Capsule->GetScaledCapsuleHalfHeight() - 6.0f), Params))
			{
				if (GTest.InRock++ % 30 == 0)
				{
					UE_LOG(LogTemp, Warning, TEXT("[DCC ClimbTest] IN ROCK at %s (climbing %d)"), *Location.ToCompactString(), bClimbing);
				}
			}
		}
		if (PC && PC->PlayerCameraManager)
		{
			const FVector Camera = PC->PlayerCameraManager->GetCameraLocation();
			if (World->OverlapBlockingTestByChannel(Camera, FQuat::Identity, ECC_WorldStatic, FCollisionShape::MakeSphere(6.0f), Params))
			{
				if (GTest.CamRock++ % 30 == 0)
				{
					UE_LOG(LogTemp, Warning, TEXT("[DCC ClimbTest] CAMERA IN ROCK at %s (crawler %s)"), *Camera.ToCompactString(), *Location.ToCompactString());
				}
			}
		}

		GTest.LastLocation = Location;
		GTest.bWasClimbing = bClimbing;

		const float Height = Location.Z - Mountain(World).Z;
		const double Elapsed = World->GetTimeSeconds() - GTest.FaceStarted;
		// Stay on the summit a few seconds so the fanfare and fireworks play out before the next face.
		if (Height >= TopZ + 60.0f && !bClimbing && !bFalling && GTest.SummitAt < 0.0)
		{
			GTest.SummitAt = World->GetTimeSeconds();
		}
		const bool bCelebrated = GTest.SummitAt >= 0.0 && World->GetTimeSeconds() - GTest.SummitAt > 8.0;
		const TCHAR* Done = bCelebrated ? TEXT("summit")
			: Elapsed > 300.0 ? TEXT("time limit")
			: (Elapsed > 20.0 && World->GetTimeSeconds() - GTest.LastRiseAt > 12.0) ? TEXT("stopped rising") : nullptr;
		if (Done)
		{
			FinishFace(World, Done);
			if (++GTest.FaceIndex >= GTest.Faces.Num())
			{
				Crawler->DebugSetClimbHold(false);
				UE_LOG(LogTemp, Display, TEXT("[DCC ClimbTest] all done"));
				return false;
			}
			StartFace(World);
		}
		return true;
	}

	void Command(const TArray<FString>& Args, UWorld* World)
	{
		if (!World)
		{
			return;
		}
		if (GTest.Handle.IsValid())
		{
			FTSTicker::GetCoreTicker().RemoveTicker(GTest.Handle);
		}
		GTest = FClimbTest();
		GTest.World = World;
		if (Args.IsEmpty() || Args[0].Equals(TEXT("all"), ESearchCase::IgnoreCase))
		{
			GTest.Faces = { 0, 1, 2, 3 };
		}
		else
		{
			GTest.Faces = { FCString::Atoi(*Args[0]) & 3 };
		}
		GTest.WaitStarted = World->GetTimeSeconds();
		if (IConsoleVariable* Hazards = IConsoleManager::Get().FindConsoleVariable(TEXT("dcc.ClimbHazards")))
		{
			Hazards->Set(0); // a rock knocking the climber off would read as a movement fault
		}
		GTest.Handle = FTSTicker::GetCoreTicker().AddTicker(FTickerDelegate::CreateStatic(&Tick));
		UE_LOG(LogTemp, Display, TEXT("[DCC ClimbTest] queued faces %d"), GTest.Faces.Num());
	}

	FAutoConsoleCommandWithWorldAndArgs GClimbTestCommand(
		TEXT("DCC.ClimbTest"),
		TEXT("DCC.ClimbTest [0-3|all]: automated climb of Fourteener's faces that logs jumps, stuck time, drops and rock overlaps."),
		FConsoleCommandWithWorldAndArgsDelegate::CreateStatic(&Command));
}
