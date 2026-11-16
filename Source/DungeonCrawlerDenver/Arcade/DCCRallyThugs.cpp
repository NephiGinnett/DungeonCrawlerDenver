// Repo Rally trouble (2026-11-14, Zoe: "build a chance for enemies to shoot at you or fight you, and make damage to
// the vehicle restored when you stay in the gas station").
//
// When a hook lands on a mark, the owner does not take it quietly:
//   Most of the time they come out of the building and scream (a Yeller). The words are Voice Studio lines.
//   About one time in three, after the first 20 seconds, they fight instead. A Brawler runs at the truck and
//     beats on it. A Shooter stops at range and fires. Driving into either at speed knocks them flat.
//   A car that was driving: the driver sometimes stays in and shoots. Otherwise someone bails out yelling.
// Hits take HULL off the truck (FTruck::Hull). Under half the truck is slower; at zero it is wrecked: the magnet
// lets go of the tow and will not fire again until the truck has sat at a gas station (DCCArcadeGames.cpp, the
// player loop in ServerTick, repairs 20 hull a second there). Rival crews' rams take hull too.

#include "Arcade/DCCArcadeGames.h"
#include "Arcade/DCCRallyThug.h"
#include "Arcade/DCCChopShop.h"
#include "Dialogue/DCCVoiceLines.h"
#include "Vehicles/DCCRover.h"
#include "Engine/World.h"
#include "EngineUtils.h"
#include "TimerManager.h"

namespace
{
	/** Where the ground is under a point, or the fallback height. */
	float GroundZ(UWorld* World, const FVector& At, float Fallback)
	{
		FHitResult Hit;
		FCollisionQueryParams Params(SCENE_QUERY_STAT(RallyThugGround), false);
		if (World->LineTraceSingleByObjectType(Hit, At + FVector(0, 0, 600), At - FVector(0, 0, 900),
			FCollisionObjectQueryParams(ECC_WorldStatic), Params))
		{
			return Hit.ImpactPoint.Z;
		}
		return Fallback;
	}
}

void ADCCArcadeKart::Multicast_RallyHit_Implementation(FVector_NetQuantize From, FVector_NetQuantize To, uint8 HitKind)
{
	ADCCRallyThug::PlayHit(GetWorld(), From, To, HitKind);
}

void ADCCArcadeKart::Multicast_RallyYell_Implementation(FName Speaker, FName Row, FVector_NetQuantize At)
{
	const FString Wav = DCCVoice::FindLine(Speaker, Row);
	if (!Wav.IsEmpty())
	{
		DCCVoice::PlayAt(this, Wav, FVector(At), 1.5f);
	}
}

void ADCCArcadeKart::NoteImpact(ADCCRover* Car, float SpeedCms)
{
	if (!HasAuthority() || !Car || SpeedCms < 700.0f)
	{
		return;
	}
	FTruck* Truck = TruckFor(Car);
	if (!Truck)
	{
		return;
	}
	const float Amount = FMath::GetMappedRangeValueClamped(
		FVector2D(700.0f, 2800.0f), FVector2D(8.0f, 28.0f), SpeedCms);
	HurtTruck(*Truck, Amount, TEXT("That one left a mark on the hull."));
}

void ADCCArcadeKart::HurtTruck(FTruck& Truck, float Amount, const TCHAR* Why)
{
	if (!Truck.bPlayer || Truck.Hull <= 0.0f || bRoundOver)
	{
		return;
	}
	if (Truck.Role == 4)
	{
		Amount *= 0.65f; // the tank shrugs some of it off
	}
	Amount *= DCCChopShop::DamageTaken(Truck.ChopPlating); // Chop Shop skid plating
	const double Now = ServerTime();
	const float Before = Truck.Hull;
	Truck.Hull = FMath::Max(0.0f, Truck.Hull - Amount);
	ADCCRover* Car = Truck.Car.Get();
	if (Why && Now >= Truck.NextHurtTell)
	{
		Tell(Car, Why);
		Truck.NextHurtTell = Now + 4.0;
	}
	if (Before > 50.0f && Truck.Hull <= 50.0f)
	{
		Tell(Car, TEXT("Hull at half and the truck's dragging. Sit under any gas station canopy to patch it up."));
	}
	if (Before > 20.0f && Truck.Hull <= 20.0f && Truck.Hull > 0.0f)
	{
		Tell(Car, TEXT("Hull's nearly gone. One more beating and the magnet lets go. Get to a station."));
	}
	if (Truck.Hull <= 0.0f)
	{
		Tell(Car, TEXT("WRECKED. The magnet let go. Limp to a gas station and sit until you're patched."));
		if (Truck.HookedId >= 0)
		{
			Release(Truck.HookedId, false);
		}
	}
}

void ADCCArcadeKart::RollTrouble(int32 Slot, FTruck& Truck)
{
	if (!Marks.IsValidIndex(Slot) || !Truck.bPlayer)
	{
		return;
	}
	FMark& Mark = Marks[Slot];
	// Fighters wait 20 seconds, so the first hook of the night is not a gun lesson.
	// Yellers do not wait: someone should be angry at you right away.
	const bool bCanFight = ServerTime() >= GoAt + 20.0 && FMath::FRand() < 0.34f;
	if (Mark.Kind >= 2)
	{
		if (bCanFight)
		{
			Mark.bShootsBack = true;
			Mark.NextShot = ServerTime() + 1.4;
			FString Speaker, Row, Text;
			ADCCRallyThug::YellFor(Mark.Name, Speaker, Row, Text);
			Tell(Truck.Car.Get(), FString::Printf(TEXT("%s  They're staying in the car with a gun."), *Text));
			if (ADCCRover* Car = Mark.Car.Get())
			{
				Multicast_RallyYell(FName(*Speaker), FName(*Row), Car->GetActorLocation());
			}
			return;
		}
		// Someone bails out and screams instead of shooting from the seat.
		if (FMath::FRand() < 0.6f)
		{
			SpawnOwner(Slot, Truck, true);
		}
		return;
	}
	if (bCanFight)
	{
		SpawnOwner(Slot, Truck, false);
		return;
	}
	if (FMath::FRand() < 0.7f)
	{
		SpawnOwner(Slot, Truck, true);
	}
}

void ADCCArcadeKart::SpawnOwner(int32 Slot, FTruck& Truck, bool bYellOnly)
{
	ADCCRover* MarkCar = Marks[Slot].Car.Get();
	ADCCRover* TruckCar = Truck.Car.Get();
	if (!MarkCar || !TruckCar)
	{
		return;
	}
	FString Speaker, Row, Text;
	ADCCRallyThug::YellFor(Marks[Slot].Name, Speaker, Row, Text);
	int32 Live = 0;
	for (const FThug& T : Thugs)
	{
		Live += (T.Actor.IsValid() && T.GoneAt <= 0.0) ? 1 : 0;
	}
	if (Live >= 5)
	{
		// No room for another body. They still get the words.
		Tell(TruckCar, Text);
		Multicast_RallyYell(FName(*Speaker), FName(*Row), TruckCar->GetActorLocation());
		return;
	}

	// Out of the building on the near side of the street: snap to the street the car is on, step toward the curb.
	const FVector O = GetActorLocation();
	const FVector Local = MarkCar->GetActorLocation() - O;
	const float DX = Local.X - FMath::RoundToFloat(Local.X / StreetSpacing) * StreetSpacing;
	const float DY = Local.Y - FMath::RoundToFloat(Local.Y / StreetSpacing) * StreetSpacing;
	FVector Out = FMath::Abs(DX) < FMath::Abs(DY) ? FVector(FMath::Sign(DX), 0, 0) : FVector(0, FMath::Sign(DY), 0);
	if (Out.IsNearlyZero())
	{
		Out = FVector(1, 0, 0);
	}
	FVector At = MarkCar->GetActorLocation() + Out * 1000.0f;
	At.Z = GroundZ(GetWorld(), At, MarkCar->GetActorLocation().Z - 60.0f);

	const bool bShooter = !bYellOnly && FMath::FRand() < 0.45f;
	const FRotator Face = (TruckCar->GetActorLocation() - At).GetSafeNormal2D().Rotation();
	const FTransform Where(FRotator(0.0f, Face.Yaw, 0.0f), At);
	ADCCRallyThug* Thug = GetWorld()->SpawnActorDeferred<ADCCRallyThug>(ADCCRallyThug::StaticClass(), Where, this, nullptr,
		ESpawnActorCollisionHandlingMethod::AlwaysSpawn);
	if (!Thug)
	{
		Tell(TruckCar, Text);
		Multicast_RallyYell(FName(*Speaker), FName(*Row), TruckCar->GetActorLocation());
		return;
	}
	Thug->Kind = bYellOnly ? ADCCRallyThug::Yeller : bShooter ? ADCCRallyThug::Shooter : ADCCRallyThug::Brawler;
	Thug->Plate = Marks[Slot].Name;
	Thug->FinishSpawning(Where);

	FThug& T = Thugs.AddDefaulted_GetRef();
	T.Actor = Thug;
	T.Target = TruckCar;
	T.GiveUpAt = ServerTime() + (bYellOnly ? 16.0 : 28.0);
	T.NextHit = ServerTime() + (bShooter ? 1.6 : 0.6);
	// The body plays the recording itself. The toast is the words, for whoever is looking at the road.
	Tell(TruckCar, bYellOnly ? Text
		: bShooter ? FString::Printf(TEXT("%s  They came out with a rifle."), *Text)
		: FString::Printf(TEXT("%s  They're coming at the truck."), *Text));
}

void ADCCArcadeKart::TickThugs(float DeltaSeconds)
{
	const double Now = ServerTime();
	UWorld* World = GetWorld();

	// Drivers who stayed in and shoot at the truck with the line, until it is dropped or cut.
	for (FMark& Mark : Marks)
	{
		ADCCRover* Car = Mark.Car.Get();
		ADCCRover* TowerCar = Mark.Tower.Get();
		if (!Mark.bShootsBack || !Car || !TowerCar || bRoundOver)
		{
			continue;
		}
		if (Now < Mark.NextShot)
		{
			continue;
		}
		Mark.NextShot = Now + FMath::FRandRange(1.3, 2.0);
		FTruck* Truck = TruckFor(TowerCar);
		if (!Truck)
		{
			continue;
		}
		const bool bHit = FMath::FRand() < 0.5f;
		const FVector From = Car->GetActorLocation() + FVector(0, 0, 110);
		const FVector To = TowerCar->GetActorLocation() + FVector(0, 0, 80)
			+ (bHit ? FVector::ZeroVector : FVector(FMath::FRandRange(-400.0f, 400.0f), FMath::FRandRange(-400.0f, 400.0f), FMath::FRandRange(60.0f, 300.0f)));
		Multicast_RallyHit(From, To, bHit ? 1 : 2);
		if (bHit)
		{
			HurtTruck(*Truck, 4.0f, TEXT("The driver's shooting at you from the tow. Get it to the yard."));
		}
	}

	for (int32 i = Thugs.Num() - 1; i >= 0; --i)
	{
		FThug& T = Thugs[i];
		ADCCRallyThug* Thug = T.Actor.Get();
		if (!Thug)
		{
			Thugs.RemoveAt(i);
			continue;
		}
		if (T.GoneAt > 0.0)
		{
			if (Now >= T.GoneAt)
			{
				Thug->Destroy();
				Thugs.RemoveAt(i);
			}
			continue;
		}

		const FVector Me = Thug->GetActorLocation();

		// Run down at speed: knocked flat, and the truck takes a little for it.
		bool bFlat = false;
		for (FTruck& Truck : Trucks)
		{
			ADCCRover* Car = Truck.Car.Get();
			if (Truck.bPlayer && Car && FVector::Dist2D(Car->GetActorLocation(), Me) < 360.0f && Car->GetVelocity().Size2D() > 850.0f)
			{
				Thug->SetMood(ADCCRallyThug::Down);
				T.GoneAt = Now + 4.0;
				Multicast_RallyHit(Car->GetActorLocation(), Me + FVector(0, 0, 100), 0);
				HurtTruck(Truck, 3.0f, nullptr);
				Tell(Car, TEXT("Owner flattened. They'll live. They will also call the news."));
				bFlat = true;
				break;
			}
		}
		if (bFlat)
		{
			continue;
		}

		ADCCRover* Car = T.Target.Get();
		FTruck* Truck = Car ? TruckFor(Car) : nullptr;
		if (!Car || !Truck || bRoundOver || Now > T.GiveUpAt || FVector::Dist2D(Car->GetActorLocation(), Me) > 9000.0f)
		{
			Thug->SetMood(ADCCRallyThug::Stand);
			T.GoneAt = Now + 3.0;
			continue;
		}

		const FVector Goal = Car->GetActorLocation();
		const FVector ToGoal = (Goal - Me).GetSafeNormal2D();
		const float Dist = FVector::Dist2D(Goal, Me);
		const bool bYeller = Thug->Kind == ADCCRallyThug::Yeller;
		const bool bShooter = Thug->Kind == ADCCRallyThug::Shooter;
		const float Reach = bYeller ? 780.0f : bShooter ? 3200.0f : 340.0f;
		Thug->SetActorRotation(FRotator(0.0f, ToGoal.Rotation().Yaw, 0.0f));

		if (Dist > Reach)
		{
			Thug->SetMood(ADCCRallyThug::Run);
			FVector Next = Me + ToGoal * FMath::Min(Dist - Reach * 0.9f, (bShooter ? 560.0f : 680.0f) * DeltaSeconds);
			Next.Z = GroundZ(World, Next, Me.Z);
			Thug->SetActorLocation(Next);
			continue;
		}

		Thug->SetMood(ADCCRallyThug::Attack);
		// A yeller shakes a fist and keeps screaming. They do not take hull.
		if (bYeller || Now < T.NextHit)
		{
			continue;
		}
		if (!bShooter)
		{
			T.NextHit = Now + 1.0;
			Multicast_RallyHit(Me + FVector(0, 0, 120), Goal + FVector(0, 0, 90), 0);
			Truck->HitUntil = FMath::Max(Truck->HitUntil, Now + 0.35);
			HurtTruck(*Truck, 4.0f, TEXT("The owner is beating on your truck. Drive, or run them over."));
			continue;
		}
		T.NextHit = Now + 1.4;
		// A moving truck is harder to hit.
		const float Chance = FMath::Clamp(0.75f - Car->GetVelocity().Size2D() / 5000.0f, 0.2f, 0.75f);
		const bool bHit = FMath::FRand() < Chance;
		const FVector To = Goal + FVector(0, 0, 80)
			+ (bHit ? FVector::ZeroVector : FVector(FMath::FRandRange(-500.0f, 500.0f), FMath::FRandRange(-500.0f, 500.0f), FMath::FRandRange(80.0f, 400.0f)));
		Multicast_RallyHit(Thug->MuzzleLocation(), To, bHit ? 1 : 2);
		if (bHit)
		{
			HurtTruck(*Truck, 5.0f, TEXT("The owner is shooting at your truck."));
		}
	}
}

void ADCCArcadeKart::DebugTrouble(const FString& What)
{
	FTruck* Truck = Trucks.FindByPredicate([](const FTruck& T) { return T.bPlayer && T.Car.IsValid(); });
	if (!Truck || !HasAuthority())
	{
		return;
	}
	ADCCRover* Car = Truck->Car.Get();
	if (What.Equals(TEXT("hurt"), ESearchCase::IgnoreCase))
	{
		HurtTruck(*Truck, 30.0f, TEXT("Test: 30 hull off."));
		return;
	}
	if (What.Equals(TEXT("station"), ESearchCase::IgnoreCase))
	{
		const FVector2D Off = GasOffset(0);
		FVector At = GetActorLocation() + FVector(Off.X, Off.Y, 0.0f);
		At.Z = Car->GetActorLocation().Z;
		Car->SetActorLocation(At, false, nullptr, ETeleportType::TeleportPhysics);
		return;
	}
	const bool bShooter = What.Equals(TEXT("shooter"), ESearchCase::IgnoreCase);
	const bool bYeller = What.Equals(TEXT("yeller"), ESearchCase::IgnoreCase);
	FVector At = Car->GetActorLocation() + Car->GetActorForwardVector() * (bShooter ? 2400.0f : 1400.0f) + Car->GetActorRightVector() * 300.0f;
	At.Z = GroundZ(GetWorld(), At, Car->GetActorLocation().Z - 60.0f);
	const FTransform Where(FRotator(0.0f, (Car->GetActorLocation() - At).Rotation().Yaw, 0.0f), At);
	if (ADCCRallyThug* Thug = GetWorld()->SpawnActorDeferred<ADCCRallyThug>(ADCCRallyThug::StaticClass(), Where, this, nullptr,
		ESpawnActorCollisionHandlingMethod::AlwaysSpawn))
	{
		Thug->Kind = bYeller ? ADCCRallyThug::Yeller : bShooter ? ADCCRallyThug::Shooter : ADCCRallyThug::Brawler;
		Thug->Plate = Marks.Num() > 0 ? Marks[0].Name : FString(TEXT("TEST"));
		Thug->FinishSpawning(Where);
		FThug& T = Thugs.AddDefaulted_GetRef();
		T.Actor = Thug;
		T.Target = Car;
		T.GiveUpAt = ServerTime() + 40.0;
		T.NextHit = ServerTime() + 1.0;
	}
}

static FAutoConsoleCommandWithWorldAndArgs GRallyTroubleCommand(
	TEXT("DCC.RallyTrouble"),
	TEXT("DCC.RallyTrouble brawler|shooter|yeller|hurt [delay seconds]: dev check for Repo Rally owners and truck hull."),
	FConsoleCommandWithWorldAndArgsDelegate::CreateLambda([](const TArray<FString>& Args, UWorld* World)
	{
		if (!World || Args.Num() < 1)
		{
			return;
		}
		const FString What = Args[0];
		const float Delay = Args.Num() > 1 ? FCString::Atof(*Args[1]) : 0.0f;
		auto Run = [What, WeakWorld = TWeakObjectPtr<UWorld>(World)]()
		{
			if (UWorld* W = WeakWorld.Get())
			{
				for (TActorIterator<ADCCArcadeKart> It(W); It; ++It)
				{
					It->DebugTrouble(What);
				}
			}
		};
		if (Delay <= 0.0f)
		{
			Run();
			return;
		}
		FTimerHandle Handle;
		World->GetTimerManager().SetTimer(Handle, FTimerDelegate::CreateLambda(Run), Delay, false);
	}));
