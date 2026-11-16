// Fourteener sky raptors (2026-11-14). ADCCArcadeClimb::SpawnBirds and ::TickBirds, kept apart from DCCArcadeGames.cpp.
// The birds are puppets (ADCCMountainBird); all of this runs on the server.
//
//   Circle   orbit the peak, drifting up and down between High Camp and the summit
//   WindUp   1.6 s: fly out from the face to a point above a climber, scream, and paint a red line at where they
//            are NOW. The line does not follow them: that is the dodge.
//   Dive     fly down the line fast. Anyone it passes close to is hit: damage, and a climber is knocked off.
//   Carry    fly to a point over a climber's head with a rock in the talons, then let go (a normal rockfall)
//   Recover  climb back out to the circle

#include "Arcade/DCCArcadeGames.h"
#include "Arcade/DCCMountainBird.h"
#include "Arcade/DCCNeonTraffic.h"
#include "Characters/DCCCharacterBase.h"
#include "Stats/DCCStatsComponent.h"
#include "Engine/World.h"
#include "EngineUtils.h"
#include "GameFramework/Character.h"
#include "HAL/IConsoleManager.h"
#include "GameFramework/PlayerController.h"
#include "TimerManager.h"

namespace
{
	constexpr float OrbitOut = 2600.0f;   // from the mountain's middle
	constexpr float DiveSpeed = 2600.0f;  // cm/s down the line
	constexpr float WindUpTime = 1.6f;
	constexpr float HitRadius = 210.0f;

	bool HazardsOn()
	{
		static IConsoleVariable* Var = IConsoleManager::Get().FindConsoleVariable(TEXT("dcc.ClimbHazards"));
		return !Var || Var->GetInt() != 0;
	}
}

void ADCCArcadeClimb::SpawnBirds()
{
	for (int32 i = 0; i < 2; ++i)
	{
		FBird& Bird = Birds.AddDefaulted_GetRef();
		Bird.Angle = i * PI;
		Bird.Height = FMath::Lerp(MidZ, TopZ, 0.35f + 0.4f * i);
		Bird.Phase = i * 1.7f;
		// The first one comes for someone after the calm start; the second a little later.
		Bird.NextAttackAt = ServerTime() + 90.0 + i * 25.0;
		const FVector At = GetActorLocation() + FVector(FMath::Cos(Bird.Angle) * OrbitOut, FMath::Sin(Bird.Angle) * OrbitOut, Bird.Height);
		FActorSpawnParameters Params;
		Params.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
		Bird.Actor = GetWorld()->SpawnActor<ADCCMountainBird>(ADCCMountainBird::StaticClass(), At, FRotator::ZeroRotator, Params);
	}
}

void ADCCArcadeClimb::TickBirds(float DeltaSeconds)
{
	const double Now = ServerTime();
	const FVector O = GetActorLocation();

	// Climbers worth going after: on the wall, high enough that a fall is a setback rather than a non-event.
	TArray<ADCCCharacterBase*> Climbers;
	for (TActorIterator<ADCCCharacterBase> It(GetWorld()); It; ++It)
	{
		// Only near the top (2026-11-16, Zoe: "the birds need to leave you alone until you're near the top"): a
		// quarter of the way from High Camp to the summit and up.
		if (It->IsPlayerControlled() && It->IsAlive() && It->IsClimbing() && It->GetActorLocation().Z - O.Z > MidZ + (TopZ - MidZ) * 0.25f)
		{
			Climbers.Add(*It);
		}
	}

	for (FBird& Bird : Birds)
	{
		ADCCMountainBird* Actor = Bird.Actor.Get();
		if (!Actor)
		{
			continue;
		}
		const FVector Here = Actor->GetActorLocation();
		auto FlyTo = [&](const FVector& Goal, float Speed)
		{
			const FVector D = Goal - Here;
			const float Step = FMath::Min(D.Size(), Speed * DeltaSeconds);
			const FVector Next = Here + D.GetSafeNormal() * Step;
			if (!D.IsNearlyZero())
			{
				FRotator Face = D.Rotation();
				Face.Pitch = FMath::Clamp(Face.Pitch, -60.0f, 35.0f);
				Actor->SetActorRotation(FMath::RInterpTo(Actor->GetActorRotation(), Face, DeltaSeconds, 4.0f));
			}
			Actor->SetActorLocation(Next);
			return D.Size() <= Step + 1.0f;
		};

		switch (Actor->GetState())
		{
		case ADCCMountainBird::Circle:
		default:
		{
			Bird.Angle += DeltaSeconds * 0.16f;
			const float Z = Bird.Height + 900.0f * FMath::Sin(float(Now) * 0.13f + Bird.Phase);
			const FVector Goal = O + FVector(FMath::Cos(Bird.Angle) * OrbitOut, FMath::Sin(Bird.Angle) * OrbitOut, Z);
			FlyTo(Goal, 1400.0f);
			// Bank into the turn.
			FRotator R = Actor->GetActorRotation();
			R.Roll = -22.0f;
			Actor->SetActorRotation(R);

			if (!HazardsOn() || bPaidOut || Now < Bird.NextAttackAt || Climbers.IsEmpty())
			{
				break;
			}
			ADCCCharacterBase* Prey = Climbers[FMath::RandRange(0, Climbers.Num() - 1)];
			const FVector P = Prey->GetActorLocation();
			Bird.Prey = Prey;
			Bird.PreyFace = FaceOf(P);
			Bird.bHit = false;
			const FVector N = FaceNormal(Bird.PreyFace);
			if (FMath::FRand() < 0.55f)
			{
				// A dive: hover out from the wall and above them, then come down the line.
				Bird.From = P + N * 1700.0f + FVector(0, 0, 700.0f);
				Bird.To = P + N * 60.0f;                       // where they ARE: stop just off the rock
				Actor->DiveTarget = Bird.To;
				Actor->SetState(ADCCMountainBird::WindUp);
				Bird.StateUntil = Now + WindUpTime;
			}
			else
			{
				// A drop: carry a rock to a point over their head.
				Bird.To = P + N * 140.0f + FVector(0, 0, 1300.0f);
				Actor->SetState(ADCCMountainBird::Carry);
				Bird.StateUntil = Now + 8.0;               // gives up if it cannot get there
			}
			break;
		}
		case ADCCMountainBird::WindUp:
		{
			FlyTo(Bird.From, 2200.0f);
			// Face the target while it screams at it.
			const FVector Look = FVector(Actor->DiveTarget) - Actor->GetActorLocation();
			Actor->SetActorRotation(FMath::RInterpTo(Actor->GetActorRotation(), Look.Rotation(), DeltaSeconds, 8.0f));
			if (Now >= Bird.StateUntil)
			{
				Actor->SetState(ADCCMountainBird::Dive);
			}
			break;
		}
		case ADCCMountainBird::Dive:
		{
			const bool bArrived = FlyTo(Bird.To, DiveSpeed);
			const FVector At = Actor->GetActorLocation();
			for (TActorIterator<ADCCCharacterBase> It(GetWorld()); It && !Bird.bHit; ++It)
			{
				ADCCCharacterBase* Crawler = *It;
				if (!Crawler->IsPlayerControlled() || !Crawler->IsAlive() || FVector::Dist(Crawler->GetActorLocation(), At) > HitRadius)
				{
					continue;
				}
				Bird.bHit = true;
				if (Crawler->Stats)
				{
					Crawler->Stats->ApplyDamage(FMath::Max(1, FMath::CeilToInt(Crawler->Stats->MaxHitPoints * 0.12f)));
				}
				if (Crawler->IsClimbing())
				{
					Crawler->ForceReleaseClimb();
					Crawler->NotifyClimber(NSLOCTEXT("DCC", "BirdHit", "The raptor slams into you and tears you off the wall!"));
				}
				else
				{
					Crawler->NotifyClimber(NSLOCTEXT("DCC", "BirdHitLedge", "The raptor rakes you as it passes."));
				}
			}
			if (bArrived)
			{
				if (!Bird.bHit && Bird.Prey.IsValid())
				{
					Bird.Prey->NotifyClimber(NSLOCTEXT("DCC", "BirdMiss", "The raptor hits rock where you were. Missed."));
				}
				Actor->SetState(ADCCMountainBird::Recover);
				Bird.StateUntil = Now + 3.0;
			}
			break;
		}
		case ADCCMountainBird::Carry:
		{
			// Track the climber a little, so a rock still has to be dodged rather than outrun.
			if (const ADCCCharacterBase* Prey = Bird.Prey.Get())
			{
				const FVector P = Prey->GetActorLocation();
				Bird.To = FMath::Lerp(Bird.To, P + FaceNormal(Bird.PreyFace) * 140.0f + FVector(0, 0, 1300.0f), FMath::Min(1.0f, DeltaSeconds * 0.6f));
			}
			const bool bThere = FlyTo(Bird.To, 1900.0f);
			if (bThere || Now >= Bird.StateUntil)
			{
				if (bThere)
				{
					// Let go: a rockfall from the talons, with the half-second of warning a rock gets at the top.
					const FVector Start = Actor->GetActorLocation() - FVector(0, 0, 90.0f);
					float StopZ = O.Z + 55.0f;
					FHitResult Hit;
					FCollisionQueryParams Params;
					for (TActorIterator<ACharacter> It(GetWorld()); It; ++It)
					{
						Params.AddIgnoredActor(*It);
					}
					Params.AddIgnoredActor(Actor);
					if (GetWorld()->SweepSingleByChannel(Hit, Start, Start - FVector(0, 0, 20000.0f), FQuat::Identity, ECC_WorldStatic,
						FCollisionShape::MakeSphere(45.0f), Params))
					{
						StopZ = Hit.Location.Z + 10.0f;
					}
					FDCCRockfall Rock;
					Rock.Id = NextRockId++;
					Rock.Start = Start;
					Rock.StartTime = ADCCNeonTrafficCar::ClockOf(GetWorld()) + 0.5;
					Rock.StopZ = StopZ;
					Rocks.Add(Rock);
					if (Bird.Prey.IsValid())
					{
						Bird.Prey->NotifyClimber(NSLOCTEXT("DCC", "BirdDrop", "A raptor drops a rock on you. Move!"));
					}
				}
				Actor->SetState(ADCCMountainBird::Recover);
				Bird.StateUntil = Now + 3.0;
			}
			break;
		}
		case ADCCMountainBird::Recover:
		{
			const FVector Out = Actor->GetActorLocation() - O;
			const FVector Dir = FVector(Out.X, Out.Y, 0.0f).GetSafeNormal();
			Bird.Angle = FMath::Atan2(Dir.Y, Dir.X);
			FlyTo(O + Dir * OrbitOut + FVector(0, 0, Bird.Height), 1700.0f);
			if (Now >= Bird.StateUntil)
			{
				Actor->SetState(ADCCMountainBird::Circle);
				// Busier near the top of a run: two birds, one attack every 11-18 s each.
				Bird.NextAttackAt = Now + FMath::FRandRange(35.0f, 55.0f); // was 11-18: far too often
			}
			break;
		}
		}
	}
}

void ADCCArcadeClimb::DebugBirdAttack(bool bDrop)
{
	APlayerController* PC = GetWorld()->GetFirstPlayerController();
	ADCCCharacterBase* Prey = PC ? Cast<ADCCCharacterBase>(PC->GetPawn()) : nullptr;
	if (!Prey || Birds.IsEmpty() || !Birds[0].Actor.IsValid())
	{
		return;
	}
	FBird& Bird = Birds[0];
	ADCCMountainBird* Actor = Bird.Actor.Get();
	const FVector P = Prey->GetActorLocation();
	Bird.Prey = Prey;
	Bird.PreyFace = FaceOf(P);
	Bird.bHit = false;
	const FVector Back = -Prey->GetActorForwardVector();
	// Bring it in close enough to film: behind and above the player.
	Actor->SetActorLocation(P + Back * 2600.0f + FVector(0, 0, 1500.0f));
	if (bDrop)
	{
		Bird.To = P + FVector(0, 0, 1300.0f);
		Actor->SetState(ADCCMountainBird::Carry);
		Bird.StateUntil = ServerTime() + 8.0;
	}
	else
	{
		Bird.From = P + Back * 1400.0f + FVector(0, 0, 600.0f);
		Bird.To = P;
		Actor->DiveTarget = Bird.To;
		Actor->SetState(ADCCMountainBird::WindUp);
		Bird.StateUntil = ServerTime() + 1.6;
	}
}

static FAutoConsoleCommandWithWorldAndArgs GBirdAttack(TEXT("DCC.BirdAttack"), TEXT("DCC.BirdAttack [drop] [delay]: (dev) a Fourteener bird attacks you now."),
	FConsoleCommandWithWorldAndArgsDelegate::CreateLambda([](const TArray<FString>& Args, UWorld* World)
	{
		const bool bDrop = Args.Num() > 0 && Args[0].Equals(TEXT("drop"), ESearchCase::IgnoreCase);
		const float Delay = Args.Num() > 1 ? FCString::Atof(*Args[1]) : 0.0f;
		auto Run = [bDrop, Weak = TWeakObjectPtr<UWorld>(World)]()
		{
			if (UWorld* W = Weak.Get())
			{
				for (TActorIterator<ADCCArcadeClimb> It(W); It; ++It) { It->DebugBirdAttack(bDrop); }
			}
		};
		if (Delay <= 0.0f) { Run(); return; }
		FTimerHandle Handle;
		World->GetTimerManager().SetTimer(Handle, FTimerDelegate::CreateLambda(Run), Delay, false);
	}));
