#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "DCCRaceTrack.generated.h"

class ADCCRover;
class ADCCCharacterBase;

UENUM(BlueprintType)
enum class EDCCRaceState : uint8
{
	/** Nothing on the grid. The bookie's Start row is what leaves this state. */
	Idle       UMETA(DisplayName = "Idle"),

	/** Cars are on the line, engines off, waiting out CountdownSeconds. */
	Countdown  UMETA(DisplayName = "Countdown"),

	/** Autopilot is armed on every entrant and laps are being counted. */
	Running    UMETA(DisplayName = "Running"),

	/** Somebody won. Cars are still on the track until the next start clears them. */
	Finished   UMETA(DisplayName = "Finished")
};

/**
 * A closed circuit that driverless rovers race around, and the thing a bookie
 * sells tickets to.
 *
 * Zoe, 2026-09-05: "can you make a race track for us? and have the NPCs drive
 * cars around the track? maybe just add a bookie NPC that lets you repeat the
 * race."
 *
 * The track itself is just `Waypoints` - an ordered ring of world positions.
 * Raw FVectors rather than placed marker actors on purpose: the whole layout
 * is authored over MCP in one `set_properties` call, and a ring of twelve
 * marker actors would be twelve spawn calls, twelve outliner entries and
 * twelve chances for one to get nudged out of the loop. Nothing about a
 * waypoint needs to exist in the world to do its job.
 *
 * Racers are plain ADCCRovers with no driver, following the route through
 * ADCCRover::StartAutoPilotRoute. That is deliberate rather than a shortcut:
 * a racer is mechanically identical to the car the player drives, so a crawler
 * can pull onto the circuit and race the field for real, and the pack behaves
 * exactly as the player's own car would over the same bumps.
 *
 * Server-authoritative like every other autonomous system here. Clients see
 * the race because the rovers themselves replicate; the placings, the lap
 * counting and the payout all resolve on the server and arrive as announcements.
 */
UCLASS()
class DUNGEONCRAWLERDENVER_API ADCCRaceTrack : public AActor
{
	GENERATED_BODY()

public:
	ADCCRaceTrack();

	virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;
	virtual void BeginPlay() override;
	virtual void Tick(float DeltaSeconds) override;

	// -----------------------------------------------------------------
	// TRACK
	// -----------------------------------------------------------------

	/**
	 * The circuit, in driving order. Index 0 is the start/finish line - a lap
	 * is counted when a racer wraps past the last point back to it.
	 *
	 * Needs at least 3 points to be a loop rather than a shuttle run. Corners
	 * want points on the apex, not just at the ends of straights, because a
	 * rover steers toward the point it is chasing and will cut anything it can
	 * see straight through.
	 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Race|Track")
	TArray<FVector> Waypoints;

	/** Handed to every entrant as its acceptance radius. Roughly half the track width is right. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Race|Track")
	float WaypointAcceptanceRadius = 750.0f;

	/** Laps to win. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Race|Track")
	int32 LapsToWin = 2;

	// -----------------------------------------------------------------
	// FIELD
	// -----------------------------------------------------------------

	/** What the entrants are. Defaults to a plain ADCCRover in the constructor. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Race|Field")
	TSubclassOf<ADCCRover> RacerClass;

	/** How many cars line up. Clamped to RacerNames' length so every car has a name to announce. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Race|Field")
	int32 RacerCount = 4;

	/**
	 * Names in grid order, used by every announcement. Authored rather than
	 * generated because "Number 3 wins" is not a thing anyone cheers for -
	 * see Docs/dungeon-tone-bible.md on the System AI's commentary voice.
	 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Race|Field")
	TArray<FText> RacerNames;

	/**
	 * Top-speed multiplier per grid slot, applied as ADCCRover::AutoPilotSpeedScale.
	 * A spread is what makes the race have an outcome; identical cars on an
	 * identical route finish in a dead heat every single time and there is
	 * nothing to bet on.
	 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Race|Field")
	TArray<float> RacerSpeedScales;

	/**
	 * Random +/- wobble added to each car's speed scale at the start of every
	 * race. Without it RacerSpeedScales alone makes the same car win forever,
	 * which turns the bookie into a vending machine.
	 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Race|Field")
	float SpeedScaleJitter = 0.08f;

	/** Sideways gap between cars on the start line, along the line's own right vector. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Race|Field")
	float GridSpacing = 420.0f;

	/** How far back from waypoint 0 the grid sits, so nobody starts already past the line. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Race|Field")
	float GridSetback = 500.0f;

	/** Z offset applied to every spawned racer, matching the rover's own ride height above a waypoint's authored Z. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Race|Field")
	float GridSpawnHeight = 120.0f;

	/** Seconds on the line before autopilot arms. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Race|Field")
	float CountdownSeconds = 4.0f;

	/**
	 * Run a race without waiting for the bookie. On by default because a dead
	 * circuit reads as broken scenery - a player who wanders in should find
	 * cars already going round, and the bookie's job is then to restart it with
	 * money on the line rather than to switch the whole place on.
	 *
	 * Also the only way this system can be observed from outside the game:
	 * Simulate-In-Editor duplicates the world, so MCP reads the static editor
	 * copy and never sees a running race. Auto-start plus the [DCC Race] log
	 * lines is what makes it verifiable at all.
	 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Race|Field")
	bool bAutoStartOnBeginPlay = true;

	/** Grace period after BeginPlay before an auto-start, so the level finishes streaming first. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Race|Field")
	float AutoStartDelaySeconds = 3.0f;

	// -----------------------------------------------------------------
	// STATE
	// -----------------------------------------------------------------

	UPROPERTY(Replicated, BlueprintReadOnly, Category = "Race|State")
	EDCCRaceState State = EDCCRaceState::Idle;

	UPROPERTY(BlueprintReadOnly, Category = "Race|State")
	TArray<TObjectPtr<ADCCRover>> Racers;

	/** Grid index of the winner once State is Finished, or INDEX_NONE. */
	UPROPERTY(Replicated, BlueprintReadOnly, Category = "Race|State")
	int32 WinnerIndex = INDEX_NONE;

	/**
	 * Server only. Clears any previous field, lines up a new one and starts the
	 * countdown. Safe to call at any time - repeating the race is the bookie's
	 * whole job, so this must never need a reset first.
	 */
	UFUNCTION(BlueprintCallable, Category = "Race")
	void StartRace();

	/** Server only. Despawns the field and returns to Idle. */
	UFUNCTION(BlueprintCallable, Category = "Race")
	void ClearRace();

	/** Display name for a grid slot, falling back to "Car N" if RacerNames is short. */
	UFUNCTION(BlueprintPure, Category = "Race")
	FText GetRacerName(int32 Index) const;

	/** True while a start would be pointless - already counting down or running. */
	UFUNCTION(BlueprintPure, Category = "Race")
	bool IsRaceInProgress() const { return State == EDCCRaceState::Countdown || State == EDCCRaceState::Running; }

protected:
	/** Time left in the Countdown state. */
	float CountdownRemaining = 0.0f;

	/** Whole seconds already called out, so the countdown announces once per second and not once per tick. */
	int32 LastCountdownAnnounced = -1;

	/** Counts down to the bAutoStartOnBeginPlay race; negative once it has fired. */
	float AutoStartRemaining = -1.0f;

	/** Throttles the per-racer progress logging while Running. */
	float SecondsSinceProgressLog = 0.0f;

	void SetState(EDCCRaceState NewState);

	/** Start-line transform for grid slot Index - position and facing derived from the first two waypoints. */
	bool GetGridTransform(int32 Index, FVector& OutLocation, FRotator& OutRotation) const;

	void ArmRacers();
	void CheckForWinner();
};
