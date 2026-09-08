#include "Vehicles/DCCRaceTrack.h"

#include "Vehicles/DCCRover.h"
#include "Characters/DCCCharacterBase.h"
#include "Combat/DCCGameState.h"
#include "UI/DCCEventLogTypes.h"
#include "Net/UnrealNetwork.h"
#include "Engine/World.h"

ADCCRaceTrack::ADCCRaceTrack()
{
	// Quarter-second is plenty - the only thing this tick does is count down
	// and compare lap totals, neither of which needs frame precision, and the
	// race itself is driven entirely by the rovers' own ticks.
	PrimaryActorTick.bCanEverTick = true;
	PrimaryActorTick.TickInterval = 0.25f;

	bReplicates = true;
	bAlwaysRelevant = true;

	RacerClass = ADCCRover::StaticClass();

	RacerNames = {
		NSLOCTEXT("DCC", "Racer1", "Repo Rocket"),
		NSLOCTEXT("DCC", "Racer2", "Bad Credit"),
		NSLOCTEXT("DCC", "Racer3", "Second Notice"),
		NSLOCTEXT("DCC", "Racer4", "Total Loss")
	};

	// Deliberately close together. A spread this narrow means the lead changes
	// hands over two laps instead of the fastest car simply leaving; the
	// jitter on top is what stops the same slot winning every single time.
	RacerSpeedScales = { 1.00f, 0.96f, 0.92f, 0.88f };
}

void ADCCRaceTrack::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);

	DOREPLIFETIME(ADCCRaceTrack, State);
	DOREPLIFETIME(ADCCRaceTrack, WinnerIndex);
}

void ADCCRaceTrack::BeginPlay()
{
	Super::BeginPlay();

	if (HasAuthority() && bAutoStartOnBeginPlay)
	{
		AutoStartRemaining = FMath::Max(0.0f, AutoStartDelaySeconds);
	}

	if (HasAuthority() && Waypoints.Num() < 3)
	{
		// Same shape as ADCCRoom's "Combat room with no Encounter assigned"
		// warning: an unfinished authoring job should say so at load rather
		// than looking like a broken race later.
		UE_LOG(LogTemp, Warning,
			TEXT("[DCC Race] %s has %d waypoints; a circuit needs at least 3."),
			*GetName(), Waypoints.Num());
	}
}

FText ADCCRaceTrack::GetRacerName(int32 Index) const
{
	if (RacerNames.IsValidIndex(Index))
	{
		return RacerNames[Index];
	}

	return FText::Format(NSLOCTEXT("DCC", "RacerFallbackName", "Car {0}"),
		FText::AsNumber(Index + 1));
}

void ADCCRaceTrack::SetState(EDCCRaceState NewState)
{
	State = NewState;
}

bool ADCCRaceTrack::GetGridTransform(int32 Index, FVector& OutLocation, FRotator& OutRotation) const
{
	if (Waypoints.Num() < 2)
	{
		return false;
	}

	// Facing comes from waypoint 0 -> 1 rather than from this actor's own
	// rotation, so the grid always points down the first straight no matter
	// how the track actor itself happens to be rotated in the level. Someone
	// re-authoring the ring should not also have to remember to spin the actor.
	const FVector Line = Waypoints[0];
	const FVector Forward = (Waypoints[1] - Waypoints[0]).GetSafeNormal2D();
	if (Forward.IsNearlyZero())
	{
		return false;
	}

	const FVector Right = FVector::CrossProduct(FVector::UpVector, Forward).GetSafeNormal();

	// Centred spread: 4 cars sit at -1.5, -0.5, +0.5, +1.5 spacings, so the
	// grid straddles the racing line instead of hanging off one side of it.
	const float Lateral = (static_cast<float>(Index) - (RacerCount - 1) * 0.5f) * GridSpacing;

	OutLocation = Line - Forward * GridSetback + Right * Lateral + FVector(0.0f, 0.0f, GridSpawnHeight);
	OutRotation = Forward.Rotation();
	return true;
}

void ADCCRaceTrack::ClearRace()
{
	if (!HasAuthority())
	{
		return;
	}

	for (ADCCRover* Racer : Racers)
	{
		if (!IsValid(Racer))
		{
			continue;
		}

		// A crawler who hijacked a racer mid-race is still inside it. Destroying
		// the car out from under them would leave them attached to a dead actor,
		// so that one gets released instead of deleted - it just stops being
		// part of the field.
		if (Racer->IsOccupied())
		{
			Racer->StopAutoPilot();
			continue;
		}

		Racer->Destroy();
	}

	Racers.Reset();
	WinnerIndex = INDEX_NONE;
	CountdownRemaining = 0.0f;
	LastCountdownAnnounced = -1;
	SetState(EDCCRaceState::Idle);
}

void ADCCRaceTrack::StartRace()
{
	UWorld* World = GetWorld();
	if (!HasAuthority() || !World)
	{
		return;
	}

	if (Waypoints.Num() < 3)
	{
		ADCCGameState::Log(this, EDCCLogCategory::Announcement,
			NSLOCTEXT("DCC", "RaceNoTrack", "The track is still under construction. Management apologises to nobody."));
		return;
	}

	// Repeating is the point (Zoe: "a bookie NPC that lets you repeat the
	// race"), so a start always tears the old field down first rather than
	// refusing because one is still standing there.
	ClearRace();

	const int32 Count = FMath::Max(1, RacerCount);

	FActorSpawnParameters Params;
	Params.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AdjustIfPossibleButAlwaysSpawn;
	Params.Owner = this;

	for (int32 i = 0; i < Count; ++i)
	{
		FVector Location;
		FRotator Rotation;
		if (!GetGridTransform(i, Location, Rotation))
		{
			continue;
		}

		TSubclassOf<ADCCRover> ClassToSpawn = RacerClass ? RacerClass : TSubclassOf<ADCCRover>(ADCCRover::StaticClass());
		ADCCRover* Racer = World->SpawnActor<ADCCRover>(ClassToSpawn, Location, Rotation, Params);
		if (!Racer)
		{
			continue;
		}

		const float BaseScale = RacerSpeedScales.IsValidIndex(i) ? RacerSpeedScales[i] : 1.0f;
		Racer->AutoPilotSpeedScale = BaseScale + FMath::FRandRange(-SpeedScaleJitter, SpeedScaleJitter);
		Racer->AutoPilotAcceptanceRadius = WaypointAcceptanceRadius;

		Racers.Add(Racer);
	}

	if (Racers.Num() == 0)
	{
		return;
	}

	CountdownRemaining = FMath::Max(0.0f, CountdownSeconds);
	LastCountdownAnnounced = -1;
	SetState(EDCCRaceState::Countdown);

	UE_LOG(LogTemp, Display, TEXT("[DCC Race] Grid set: %d of %d requested racers spawned."),
		Racers.Num(), Count);

	ADCCGameState::Log(this, EDCCLogCategory::Announcement, FText::Format(
		NSLOCTEXT("DCC", "RaceGridSet", "{0} cars on the grid. {1} laps. No refunds, no survivors, no stewards."),
		FText::AsNumber(Racers.Num()), FText::AsNumber(FMath::Max(1, LapsToWin))));
}

void ADCCRaceTrack::ArmRacers()
{
	for (ADCCRover* Racer : Racers)
	{
		if (!IsValid(Racer) || Racer->IsOccupied())
		{
			continue;
		}

		Racer->StartAutoPilotRoute(Waypoints, /*bLoop=*/true);
	}

	SetState(EDCCRaceState::Running);
	SecondsSinceProgressLog = 0.0f;

	UE_LOG(LogTemp, Display, TEXT("[DCC Race] Green flag - %d racers armed on a %d-point route."),
		Racers.Num(), Waypoints.Num());

	ADCCGameState::Log(this, EDCCLogCategory::Announcement,
		NSLOCTEXT("DCC", "RaceGo", "GREEN FLAG. Try to look excited, the cameras are on you."));
}

void ADCCRaceTrack::CheckForWinner()
{
	const int32 Target = FMath::Max(1, LapsToWin);

	for (int32 i = 0; i < Racers.Num(); ++i)
	{
		ADCCRover* Racer = Racers[i];
		if (!IsValid(Racer) || Racer->AutoPilotLapsCompleted < Target)
		{
			continue;
		}

		WinnerIndex = i;
		SetState(EDCCRaceState::Finished);

		UE_LOG(LogTemp, Display, TEXT("[DCC Race] Winner: %s (grid slot %d) after %d laps."),
			*GetRacerName(i).ToString(), i, Racer->AutoPilotLapsCompleted);

		// Everyone else stops driving. The winner keeps its route on purpose -
		// a car that slams to a halt on the line looks like the simulation
		// ended, and a victory lap costs nothing.
		for (int32 j = 0; j < Racers.Num(); ++j)
		{
			if (j != i && IsValid(Racers[j]))
			{
				Racers[j]->StopAutoPilot();
			}
		}

		ADCCGameState::Log(this, EDCCLogCategory::Announcement, FText::Format(
			NSLOCTEXT("DCC", "RaceWinner", "{0} takes it. The house thanks you for your continued participation."),
			GetRacerName(i)));
		return;
	}
}

void ADCCRaceTrack::Tick(float DeltaSeconds)
{
	Super::Tick(DeltaSeconds);

	if (!HasAuthority())
	{
		return;
	}

	if (AutoStartRemaining >= 0.0f)
	{
		AutoStartRemaining -= DeltaSeconds;
		if (AutoStartRemaining < 0.0f)
		{
			UE_LOG(LogTemp, Display, TEXT("[DCC Race] Auto-starting on %s."), *GetName());
			StartRace();
		}
		return;
	}

	if (State == EDCCRaceState::Countdown)
	{
		CountdownRemaining = FMath::Max(0.0f, CountdownRemaining - DeltaSeconds);

		const int32 WholeSecondsLeft = FMath::CeilToInt(CountdownRemaining);
		if (WholeSecondsLeft > 0 && WholeSecondsLeft <= 3 && WholeSecondsLeft != LastCountdownAnnounced)
		{
			LastCountdownAnnounced = WholeSecondsLeft;
			ADCCGameState::Log(this, EDCCLogCategory::Announcement,
				FText::AsNumber(WholeSecondsLeft));
		}

		if (CountdownRemaining <= 0.0f)
		{
			ArmRacers();
		}
		return;
	}

	if (State == EDCCRaceState::Running)
	{
		// A running race is invisible to anything outside the game - PIE and
		// Simulate both duplicate the world, so MCP reads the static editor
		// copy. This is the only trail that says whether the field is actually
		// getting round, so it stays in rather than being a debug-only hack.
		SecondsSinceProgressLog += DeltaSeconds;
		if (SecondsSinceProgressLog >= 2.0f)
		{
			SecondsSinceProgressLog = 0.0f;
			for (int32 i = 0; i < Racers.Num(); ++i)
			{
				const ADCCRover* Racer = Racers[i];
				if (!IsValid(Racer))
				{
					continue;
				}
				const FVector L = Racer->GetActorLocation();
				UE_LOG(LogTemp, Display,
					TEXT("[DCC Race] %s lap %d wp %d at (%.0f, %.0f, %.0f) speed %.0f"),
					*GetRacerName(i).ToString(), Racer->AutoPilotLapsCompleted,
					Racer->AutoPilotWaypointIndex, L.X, L.Y, L.Z, Racer->GetVelocity().Size());
			}
		}

		CheckForWinner();
	}
}
