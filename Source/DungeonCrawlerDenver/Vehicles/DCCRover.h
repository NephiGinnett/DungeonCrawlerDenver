#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Pawn.h"
#include "DCCRover.generated.h"

class UStaticMeshComponent;
class UBoxComponent;
class USpringArmComponent;
class UCameraComponent;
class UFloatingPawnMovement;
class ADCCCharacterBase;

// ---------------------------------------------------------------------------
// SOMETHING TO DRIVE
//
// WHY THIS IS NOT A CHAOS VEHICLE
// ChaosVehiclesPlugin is enabled, but a Chaos wheeled vehicle needs a SKELETAL
// mesh rigged with wheel bones plus per-wheel setup classes, and this project
// has no such asset - every car under Content/ (the Synthwave sets) is a static
// prop with no skeleton at all. Checked all 267 vehicle-ish assets; there is
// not one SK_ among them.
//
// So this is an arcade rover: a Pawn with a static body, floating-pawn
// movement, and a downward trace that keeps it on the ground. It drives, it
// carries a crawler, and it needs no art anyone has to make first. If a rigged
// vehicle ever lands in Content/, replacing this with a real
// AWheeledVehiclePawn is a contained job - the enter/exit handshake below is
// the only part worth keeping.
//
// Deliberately not physics-simulated. A simulated car on a floor built out of
// scaled cubes and 3000-unit cliff meshes would spend most of its life on its
// roof, and the point of this is transport, not a driving model.
// ---------------------------------------------------------------------------

/**
 * A drivable rover. Walk up, press E to get in, press E again to get out.
 *
 * Server-authoritative possession like everything else: the client asks
 * (ADCCCharacterBase::Server_EnterRover), the server re-checks range and does
 * the actual possess.
 */
UCLASS(Blueprintable)
class DUNGEONCRAWLERDENVER_API ADCCRover : public APawn
{
	GENERATED_BODY()

public:
	ADCCRover();

	virtual void Tick(float DeltaSeconds) override;
	virtual void SetupPlayerInputComponent(UInputComponent* PlayerInputComponent) override;
	virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Rover")
	TObjectPtr<UBoxComponent> Collision;

	/**
	 * Repo Rally traffic (RallyDuty 2) is solid to trucks but not to itself (2026-11-13):
	 * the root box stops colliding, so traffic never jams, and this shell - WorldDynamic,
	 * blocking only Pawn - is what a truck's movement sweep runs into. Off for every other car.
	 */
	UPROPERTY(VisibleAnywhere, Category = "Rover")
	TObjectPtr<UBoxComponent> TrafficShell;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Rover")
	TObjectPtr<UStaticMeshComponent> Body;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Rover")
	TObjectPtr<USpringArmComponent> CameraBoom;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Rover")
	TObjectPtr<UCameraComponent> ChaseCamera;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Rover")
	TObjectPtr<UFloatingPawnMovement> Movement;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Rover")
	float TurnRateDegreesPerSecond = 95.0f;

	/**
	 * Floor on steering responsiveness, as a fraction of TurnRateDegreesPerSecond.
	 * Zoe, 2026-09-03: "turning left or right let's you turn slowly even if
	 * you aren't moving forward." Before this, SteerScale in Tick was purely
	 * Speed / (MaxCruiseSpeed * 0.30) - at a dead stop that is exactly 0, so
	 * A/D did nothing at all while parked. This is a hard floor under that
	 * same scale rather than a replacement for it, so steering still ramps
	 * up to full authority as the car picks up speed - only the bottom end
	 * changes.
	 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Rover", meta = (ClampMin = "0.0", ClampMax = "1.0"))
	float MinSteerScale = 0.3f;

	/** How far above the traced ground the actor origin sits.
	 *  MUST exceed the Collision box half-height (90) or the body parks
	 *  underground - at the original 60 the car sat 30 units into the floor.
	 *  Raised to 140 so it hovers with real clearance and small debris
	 *  simply passes underneath instead of stopping it. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Rover")
	float RideHeight = 140.0f;

	// ---------------------------------------------------------------------
	// SPEED
	// ---------------------------------------------------------------------

	/** Cruise speed reached by holding the throttle, cm/s. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Rover|Speed")
	float MaxCruiseSpeed = 5600.0f;

	/** How long full throttle takes to wind up from a standstill to cruise.
	 *  A ramp rather than instant top speed - the point of the request was
	 *  that it should build. Raised from 3.5 (Zoe, 2026-09-03: acceleration
	 *  is "exciting for a moment" but "missing a certain weight... throwing
	 *  heavy weight around that drags a little" - see Movement->Deceleration
	 *  in the constructor for the other half of that ask). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Rover|Speed")
	float SecondsToReachCruise = 5.8f;

	/** Speed while boosting. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Rover|Boost")
	float BoostSpeed = 11000.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Rover|Boost")
	float BoostDurationSeconds = 2.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Rover|Boost")
	float BoostCooldownSeconds = 6.0f;

	UFUNCTION(BlueprintPure, Category = "Rover|Boost")
	bool IsBoosting() const { return bBoosting; }

	/** 0..1, for a HUD readout. 1 means ready. */
	UFUNCTION(BlueprintPure, Category = "Rover|Boost")
	float GetBoostChargeFraction() const;

	/**
	 * Starts a full boost regardless of cooldown - a reward call (flight
	 * rings currently), not a player action, so it should never read as
	 * "sorry, still on cooldown" the way BoostPressed correctly does for a
	 * button press. Also clears BoostCooldownRemaining outright rather than
	 * just starting the boost on top of it, or the very next player-pressed
	 * boost attempt afterward would still be refused by a cooldown the
	 * player never actually spent.
	 */
	UFUNCTION(BlueprintCallable, Category = "Rover|Boost")
	void GrantBoost();

	/**
	 * Repo Rally writes this on the server (2026-11-13). 1 is a normal car.
	 * Replicated so the driver's own machine slows down too, instead of
	 * fighting the server for a frame and then snapping.
	 */
	UPROPERTY(Replicated)
	float RaceSpeedScale = 1.0f;

	/** Charges of "the next hit doesn't count" (studded tires). Server spends them. */
	UPROPERTY(Replicated)
	int32 RaceAbsorb = 0;

	/** What the rally HUD draws. The kit lives on the car because the crawler is unpossessed while driving, so a wallet on the crawler would not replicate to them. */
	UPROPERTY(Replicated)
	TArray<FName> RallyTools;

	UPROPERTY(Replicated)
	int32 RallyToolIndex = 0;

	/** "Studded x2   Quiet 12s   Drafting" */
	UPROPERTY(Replicated)
	FString RallyStatus;

	UPROPERTY(Replicated)
	TArray<FName> RallyCargo;

	/** Permanent for this race. 1 is a stock truck. The governor-delete bay raises it. */
	UPROPERTY(Replicated)
	float RallyEngine = 1.0f;

	/** Extra seconds on every boost, from the long-tank bay. */
	UPROPERTY(Replicated)
	float RallyBoostExtra = 0.0f;

	/** Which company body. -1 keeps the mesh the rover was built with. The city shift does not use this:
	 *  those five meshes are the old impound paint, and the grunge trucks are not imported yet. */
	UPROPERTY(ReplicatedUsing = OnRep_RallyBody)
	int32 RallyBody = -1;

	UFUNCTION()
	void OnRep_RallyBody();

	/** Server. Swaps the mesh. The city shift leaves this at -1. */
	void SetRallyBody(int32 Index);

	/** 0 a player's truck, 1 a marked car, 2 traffic, 3 a rival crew, 4 a parked blocker. The columns and the minimap read it. */
	UPROPERTY(Replicated)
	uint8 RallyDuty = 0;

	/** Painted on the column. Empty until the shift names the car. */
	UPROPERTY(Replicated)
	FString RallyPlate;

	/** 0 fresh, 1 on the bumper. Marks only. The column brightens as the reel closes. */
	UPROPERTY(Replicated)
	float RallyStrain = 0.0f;

	/**
	 * Server. While set, ground snap leaves this car's height alone. Repo Rally sets it on a mark
	 * that is hovering, or one being reeled onto a bumper, so the street trace does not plant it.
	 */
	UPROPERTY(Replicated)
	bool bRallyHeld = false;

	/** Repo Rally, on a player's truck: the car on its line (null if none), whether it is on the
	 *  bumper, and whether the truck is in the yard. The tow camera and the drop prompt read these. */
	UPROPERTY(Replicated)
	TObjectPtr<ADCCRover> RallyTow;

	UPROPERTY(Replicated)
	bool bRallyTowDocked = false;

	UPROPERTY(Replicated)
	bool bRallyInYard = false;

	/** Local to the driver. Right mouse. The magnet reticle reads this. The shot carries its own direction, so this is not replicated. */
	bool bHookAim = false;

	/**
	 * The repo wrecker: a crane and a magnet instead of the synthwave car.
	 * Player trucks and rival crews only. Marks, traffic and parked cars stay cars.
	 */
	UPROPERTY(ReplicatedUsing = OnRep_Wrecker)
	bool bWrecker = false;

	UFUNCTION()
	void OnRep_Wrecker();

	/** Server, or the trailer (which does not replicate). Swaps in the wrecker mesh. */
	void SetWrecker(bool bOn = true);

	/** Server. Multiplies top speed until the arena says otherwise. Below 0.40 also cuts a boost. */
	void SetRacePace(float Scale);

	/** Server. True if a studded-tire charge soaked this hit. */
	bool TryAbsorb();

	/** Server. Ignore the controls for a moment, the way a wall does. */
	void Stumble(float Seconds);

	/** Server. A boost that lasts at least Seconds. Nitro. */
	void ForceBoost(float Seconds);

	/** A notice on the driver's screen. The crawler has no controller while seated, so this has to leave from the car. */
	UFUNCTION(Client, Reliable)
	void Client_RallyNote(const FString& Message);

	/**
	 * Dispatch on the truck radio (2026-11-13): plays SourceArt/Voice/NPC/Dispatch/VO_<Row>.wav if it has
	 * been recorded in the Voice Studio. Returns nothing either way; the arena still sends the text note,
	 * so an unrecorded line is read instead of heard.
	 */
	UFUNCTION(Client, Reliable)
	void Client_RallyRadio(FName Row);

	/** The end-of-round vote while driving (the crawler is unpossessed, so its wallet cannot send it). */
	UFUNCTION(Server, Reliable)
	void Server_ArcadeVote(bool bPlayAgain);

	/**
	 * Network fix (2026-11-16, Repo Rally with Anke: "on my game she never moves while on hers she's driving
	 * around"). The truck's floating-pawn movement only ever ran on the driver's machine, so a remote driver's truck
	 * never moved on the host. The driver now sends where the truck is ~20 times a second; the host takes it, and
	 * everyone else sees the host's copy as usual. When the host moves the truck itself (a teleport: recovery,
	 * the yard, a respawn) it tells the driver with Client_ForceTransform and ignores their reports briefly.
	 */
	UFUNCTION(Server, Unreliable)
	void Server_DriveSync(FVector_NetQuantize10 Location, FRotator Rotation, FVector_NetQuantize10 Velocity, float Throttle, float Steer);

	UFUNCTION(Client, Reliable)
	void Client_ForceTransform(FVector_NetQuantize10 Location, FRotator Rotation);

	float DriveSyncTimer = 0.0f;
	double IgnoreDriveSyncUntil = 0.0;
	FVector LastDriveSyncLocation = FVector::ZeroVector;
	bool bHasDriveSync = false;

	/** Fuel 0-100, for the low-fuel beacons and arrow. Repo Rally only; -1 elsewhere. */
	UPROPERTY(Replicated)
	float RallyFuel = -1.0f;

	/** How far this truck's hook reaches (cm). The aim reticle and the lock read it. */
	UPROPERTY(Replicated)
	float RallyHookRange = 0.0f;

	/**
	 * The hook's target lock: the marked car nearest the aim within a few degrees and in range, with the
	 * direction a shot at it should take. Used by the reticle (to draw the pulsing ring) and by the shot,
	 * so what you see locked is what you hit. Null if nothing is locked.
	 */
	ADCCRover* FindHookLock(FVector& OutDirection) const;

	/** [Q] uses the rally tool. [F] switches. No-ops everywhere except Repo Rally. */
	void RallyToolPressed();
	void RallyCyclePressed() { Server_CycleRallyTool(); }

	UFUNCTION(Server, Reliable)
	void Server_UseRallyTool();

	/** Spray tags from the driver's seat (DCCSpray); on foot it is UDCCWalletComponent::Server_Spray. */
	UFUNCTION(Server, Reliable)
	void Server_Spray(FVector At, FVector Normal, FVector ViewDir, const TArray<uint8>& RGB);

	/** The aim camera is local, so the hook shot has to carry the direction the driver was looking. */
	UFUNCTION(Server, Reliable)
	void Server_FireHook(FVector Dir);

	UFUNCTION(Server, Reliable)
	void Server_CycleRallyTool();

	// ---------------------------------------------------------------------
	// DRIFT
	// ---------------------------------------------------------------------

	/** Turn-rate multiplier while drifting (Shift). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Rover|Drift")
	float DriftTurnMultiplier = 2.1f;

	/** How much the body keeps sliding along its OLD heading while drifting.
	 *  0 = grips like a rail, 1 = no grip at all. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Rover|Drift", meta = (ClampMin = "0.0", ClampMax = "1.0"))
	float DriftSlide = 0.55f;

	/** Visual lean into the drift, degrees. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Rover|Drift")
	float DriftBodyRollDegrees = 12.0f;

	UFUNCTION(BlueprintPure, Category = "Rover|Drift")
	bool IsDrifting() const { return bDrifting; }

	// ---------------------------------------------------------------------
	// FEEL - idle hover and speed tilt
	//
	// Zoe's ask, 2026-09-03: "I do want it to slowly float, and shift up
	// and down in angle with speed." Both are pure cosmetics layered on top
	// of SnapToGround/AlignToSurface's actual physics - see UpdateTrail's
	// drift-lean block and AlignToSurface's grounded branch for where each
	// one is actually applied.
	// ---------------------------------------------------------------------

	/** How far Body bobs up/down at rest, in units. Fades out with speed. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Rover|Feel")
	float HoverBobAmplitude = 8.0f;

	/** One full up-down cycle, in seconds. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Rover|Feel")
	float HoverBobPeriodSeconds = 3.0f;

	/** Extra nose-up pitch at full MaxCruiseSpeed, added on top of whatever
	 *  the surface itself already asks for. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Rover|Feel")
	float MaxSpeedTiltDegrees = 6.0f;

	/**
	 * Suspension - a critically-damped spring on Body's local Z, kicked by
	 * landing impact speed and otherwise just relaxing back to 0. Zoe,
	 * 2026-09-04: "the vehicle needs a little bit of give still" - landing
	 * used to snap straight to rest with no cushioning at all. Additive
	 * with HoverBobAmplitude above, not a replacement for it - the bob is a
	 * constant idle motion, this is a reaction to an actual event.
	 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Rover|Feel|Suspension")
	float SuspensionStiffness = 175.0f;

	/** Higher = settles faster/less bouncy. Kept well above the "critically damped" line (2*sqrt(Stiffness)) so this reads as a compress-and-settle, not a wobble. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Rover|Feel|Suspension")
	float SuspensionDamping = 34.0f;

	/** Units of initial compression per unit/s of landing speed. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Rover|Feel|Suspension")
	float SuspensionLandingKickScale = 0.02f;

	/** Hard clamp so a very hard landing compresses noticeably without ever visibly burying the body in the ground. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Rover|Feel|Suspension")
	float MaxSuspensionCompression = 40.0f;

	// ---------------------------------------------------------------------
	// SLOPES AND AIR
	// ---------------------------------------------------------------------

	/**
	 * Steepest surface the rover will drive up. Anything steeper is left to the
	 * collision box to stop, which is what keeps "climbs ramps" from quietly
	 * becoming "climbs buildings".
	 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Rover|Slopes", meta = (ClampMin = "0.0", ClampMax = "89.0"))
	float MaxClimbAngleDegrees = 50.0f;

	/** How quickly the body pitches to match the surface under it. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Rover|Slopes")
	float SurfaceAlignSpeed = 7.0f;

	/** Height above ground below which the rover counts as landed. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Rover|Slopes")
	float GroundedTolerance = 45.0f;

	/** Downward acceleration while airborne, cm/s^2. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Rover|Slopes")
	float Gravity = 3200.0f;

	UFUNCTION(BlueprintPure, Category = "Rover|Slopes")
	bool IsAirborne() const { return bAirborne; }

	/**
	 * Tallest obstruction the rover will haul itself over instead of crashing
	 * into. Anything up to this is a kerb; anything above it is a wall.
	 *
	 * This is what a hovering vehicle should be able to do, and its absence is
	 * why the rover could not leave the starting arena - every lip in the room
	 * was being treated as a mountain.
	 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Rover|Slopes")
	float MaxStepHeight = 260.0f;

	/** How fast it heaves itself up over a step, cm/s. Rate-limited on purpose:
	 *  snapping onto a ledge looks like teleporting. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Rover|Slopes")
	float StepClimbSpeed = 900.0f;

	/** Nose-up angle while crawling over something. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Rover|Slopes")
	float StepClimbPitchDegrees = 9.0f;

	/** How far ahead the ground is sampled, so a step is climbed BEFORE the
	 *  collision box reaches it rather than after it has already been stopped. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Rover|Slopes")
	float ForwardProbeDistance = 420.0f;

	// ---------------------------------------------------------------------
	// FLIGHT
	// ---------------------------------------------------------------------

	/**
	 * How far the rover must have FALLEN from its apex before flight can be
	 * engaged. Launching off a ramp and flying immediately would make the ramp
	 * pointless - committing to the drop first is what earns it.
	 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Rover|Flight")
	float FlightUnlockFallDistance = 450.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Rover|Flight")
	float FlightSpeed = 8000.0f;

	/** Vertical rate from the climb/descend keys while flying. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Rover|Flight")
	float FlightClimbRate = 1500.0f;

	/** Flight ends on its own after this long. 0 means never. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Rover|Flight")
	float FlightDurationSeconds = 12.0f;

	UFUNCTION(BlueprintPure, Category = "Rover|Flight")
	bool IsFlying() const { return bFlying; }

	/** True when the fall and boost conditions are met, so Space would fly. */
	UFUNCTION(BlueprintPure, Category = "Rover|Flight")
	bool CanEngageFlight() const;

	// ---------------------------------------------------------------------
	// AUTOPILOT - driverless traffic
	//
	// Zoe, 2026-09-04: "I placed several cars on the road. Can you animate
	// them to just travel forwards towards the end of the ramp and then
	// shoot off and stop running after that? I'd like to incentivize the
	// player to drive either with them or crash into them." No Driver is
	// ever involved - see the bAutoPilotActive branch in Tick() (drives it)
	// and UpdateGroundAndAir (switches it off the instant it leaves the
	// ground, which is what "shoot off... and stop" means mechanically: the
	// launch itself is just existing momentum plus gravity, same as a
	// player driving off the same ramp).
	// ---------------------------------------------------------------------

	/** Starts driving itself forward the moment it exists. Turns itself off on launch - see UpdateGroundAndAir. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Rover|AutoPilot")
	bool bAutoPilotActive = false;

	/** Steering while on autopilot. 0 keeps it dead straight down the road. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Rover|AutoPilot")
	float AutoPilotSteerInput = 0.0f;

	/** Boosts whenever one is off cooldown while autopiloting, so it is at or near full speed well before the ramp, not just at the last moment. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Rover|AutoPilot")
	bool bAutoPilotBoostOnLaunch = true;

	// ---------------------------------------------------------------------
	// AUTOPILOT ROUTES - racing
	//
	// Zoe, 2026-09-05: "can you make a race track for us? and have the NPCs
	// drive cars around the track?"
	//
	// A non-empty AutoPilotWaypoints turns the straight-line traffic autopilot
	// above into a route follower. The one behaviour that MUST change with it
	// is the launch shutoff in UpdateGroundAndAir: traffic exists to shoot off
	// a ramp and stop, but a racer that switched itself off the first time a
	// wheel left the tarmac would die at the first bump in the track. So both
	// shutoff sites are gated on having no route.
	// ---------------------------------------------------------------------

	/** World-space points to drive through in order. Empty = the old straight-line traffic behaviour. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Rover|AutoPilot")
	TArray<FVector> AutoPilotWaypoints;

	/**
	 * How close (in the XY plane only) counts as having reached a waypoint.
	 * Z is deliberately ignored - a track that climbs would otherwise leave a
	 * racer circling a point it can never be level with.
	 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Rover|AutoPilot")
	float AutoPilotAcceptanceRadius = 750.0f;

	/** Wrap to waypoint 0 after the last one (a lap) instead of stopping there. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Rover|AutoPilot")
	bool bAutoPilotLoop = true;

	/**
	 * Multiplies this racer's top speed. ADCCRaceTrack hands each entrant a
	 * slightly different value so the field spreads out instead of running as
	 * one indistinguishable block - there is nothing to bet on in a dead heat.
	 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Rover|AutoPilot")
	float AutoPilotSpeedScale = 1.0f;

	/** Which waypoint it is currently driving at. Replicated so a client HUD could read the field's order. */
	UPROPERTY(Replicated, BlueprintReadOnly, Category = "Rover|AutoPilot")
	int32 AutoPilotWaypointIndex = 0;

	/** Laps finished since the route started. ADCCRaceTrack polls this rather than needing a delegate. */
	UPROPERTY(Replicated, BlueprintReadOnly, Category = "Rover|AutoPilot")
	int32 AutoPilotLapsCompleted = 0;

	/** Server-side. Arms the route and starts driving it from the nearest sensible point. */
	UFUNCTION(BlueprintCallable, Category = "Rover|AutoPilot")
	void StartAutoPilotRoute(const TArray<FVector>& Route, bool bLoop);

	/** Server-side. Stops autopiloting and clears any route. */
	UFUNCTION(BlueprintCallable, Category = "Rover|AutoPilot")
	void StopAutoPilot();

	// ---------------------------------------------------------------------
	// PASSENGERS
	// ---------------------------------------------------------------------

	/**
	 * Allies riding along. They board on their own when the driver gets in and
	 * they are close enough, because asking a player to seat four AI companions
	 * by hand every time they get in a car is not a mechanic, it is a chore.
	 */
	UPROPERTY(Replicated, BlueprintReadOnly, Category = "Rover|Passengers")
	TArray<TObjectPtr<ADCCCharacterBase>> Passengers;

	/** How close an ally has to be to climb in. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Rover|Passengers")
	float PassengerBoardRadius = 1400.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Rover|Passengers", meta = (ClampMin = "0"))
	int32 MaxPassengers = 4;

	UFUNCTION(BlueprintPure, Category = "Rover|Passengers")
	int32 GetPassengerCount() const { return Passengers.Num(); }

	/** Server only. Sweeps for nearby allies and seats them. */
	void BoardNearbyAllies();

	/** Server only. Puts every passenger back on the ground beside the rover. */
	UFUNCTION(BlueprintCallable, Category = "Rover|Passengers")
	void EjectPassengers();

	/** Thruster trail, on while boosting or flying. */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Rover|FX")
	TObjectPtr<class UNiagaraComponent> BoostTrail;

	/**
	 * Uniform scale applied to BoostTrail while it is running. Zoe,
	 * 2026-09-05: "bigger particle effects when boosting while driving and
	 * flying." Applied in UpdateTrail rather than baked into the constructor
	 * so this stays a live tuning knob - editing it in the Details panel
	 * takes effect on the next boost instead of needing a rebuild.
	 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Rover|FX", meta = (ClampMin = "0.1"))
	float BoostTrailScale = 4.0f;

	// ---------------------------------------------------------------------
	// SEATING
	//
	// Occupants used to be hidden outright (SetActorHiddenInGame) the moment
	// they got in. Zoe, 2026-09-05: "make sure that you can see the character
	// models 'inside' the car as it moves" - so they stay visible and get
	// parked at these offsets instead, relative to Collision (the root box,
	// whose origin sits 90 units above the car mesh's own).
	//
	// There is no sitting animation on any skeleton in this project, so an
	// occupant stands in place. These defaults put a standing crawler's head
	// just under the roofline with the feet below the floor pan - which reads
	// correctly from outside through the windows, and is why the offsets are
	// EditAnywhere: a taller or shorter model will want its own nudge.
	// ---------------------------------------------------------------------

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Rover|Seating")
	FVector DriverSeatOffset = FVector(40.0f, -55.0f, -30.0f);

	/** One per passenger, in boarding order. A passenger past the end of this list reuses the last entry. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Rover|Seating")
	TArray<FVector> PassengerSeatOffsets = {
		FVector(40.0f, 55.0f, -30.0f),
		FVector(-70.0f, -55.0f, -30.0f),
		FVector(-70.0f, 55.0f, -30.0f),
		FVector(-150.0f, 0.0f, -30.0f)
	};

	/** How far to the side an occupant is placed when they get out. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Rover|Seating")
	float ExitSideDistance = 320.0f;

	// ---------------------------------------------------------------------
	// COLLISION
	// ---------------------------------------------------------------------

	/** Fraction of speed retained after clipping something small. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Rover|Collision", meta = (ClampMin = "0.0", ClampMax = "1.0"))
	float GlanceSpeedRetained = 0.6f;

	/** Push-back speed applied when the rover hits something bigger than it.
	 *  Deliberately gentle - "bounced back slowly", not launched. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Rover|Collision")
	float BounceBackSpeed = 700.0f;

	/** Seconds the controls are ignored after a real crash. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Rover|Collision")
	float CrashRecoverySeconds = 0.55f;

	// ---------------------------------------------------------------------
	// PILOTING SKILL
	// ---------------------------------------------------------------------

	/** Skill marked and rewarded by driving. Must exist in the driver's SkillTable. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Rover|Piloting")
	FName PilotingSkillId = TEXT("Piloting");

	/** Seconds of actual movement between piloting awards. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Rover|Piloting", meta = (ClampMin = "1.0"))
	float PilotingTickSeconds = 5.0f;

	/**
	 * How much faster a clean drift earns the interval.
	 *
	 * The reward for drifting is reaching the advancement MARK sooner, not extra
	 * XP - the mark is a binary flag rather than a counter, so marking twice in
	 * one interval would be worth nothing. Character XP is paid only when the
	 * Rank actually lands, in UDCCSkillComponent::RunSkillAdvancement.
	 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Rover|Piloting", meta = (ClampMin = "1.0"))
	float CleanDriftTimeMultiplier = 2.0f;

	/** Below this speed the rover does not count as being driven. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Rover|Piloting")
	float PilotingMinSpeed = 300.0f;

	/** Whoever is currently driving, or null. Replicated so remote clients can
	 *  see the rover is occupied. */
	UPROPERTY(ReplicatedUsing = OnRep_Driver, BlueprintReadOnly, Category = "Rover")
	TObjectPtr<ADCCCharacterBase> Driver;

	UFUNCTION(BlueprintPure, Category = "Rover")
	bool IsOccupied() const { return Driver != nullptr; }

	/**
	 * Server only. Adds bonus progress toward the driver's next Piloting
	 * mark - the same lever CleanDriftTimeMultiplier already pulls (see
	 * TickPilotingProgress's own comment on why a mark cannot simply be
	 * granted twice in one interval), just from an external event like a
	 * flight ring instead of an ongoing multiplier.
	 */
	void AwardPilotingBonusSeconds(float BonusSeconds);

	/**
	 * Server only. Starts the "getting in" beat (Zoe's ask, 2026-09-03: "a
	 * quick animation while standing near it, and then disappear") and
	 * defers the actual seat-and-possess to FinishEnteringRover once it's
	 * done playing - see that function for why this is two steps now
	 * instead of one.
	 */
	void EnterRover(ADCCCharacterBase* NewDriver);

	/** Server only. Puts the driver back on their feet beside the rover. */
	UFUNCTION(BlueprintCallable, Category = "Rover")
	void ExitRover();

	/** Interaction entry point, same shape as every other world interactable. */
	void TryPlayerInteract(ADCCCharacterBase* Interactor);

	// ---------------------------------------------------------------------
	// WINDOWS
	//
	// HONEST VERSION OF WHAT THIS IS: SM_car_01 carries its glass as ONE
	// "Window" material slot baked into the same rigid mesh as the body -
	// checked via get_material_slots before writing a line of this, same
	// as the armour-fitting rule elsewhere in this project. There is no
	// separate glass piece to translate into the door, and no tool in this
	// project's pipeline can split one out of an already-imported mesh
	// (no FBX export path is exposed through MCP, and this pack's original
	// source file isn't on disk here). So this is not a sliding window -
	// it is the whole car's glass fading transparent, via a dynamic
	// instance of /Game/DCC/Materials/M_CarWindow_Translucent (a DUPLICATE
	// of the shared Synthwave M_window master with blend mode flipped to
	// Translucent; the Opacity parameter was already wired to the Opacity
	// output in that graph, just inert under the original Opaque blend -
	// confirmed via MaterialTools.get_property_input before assuming it
	// needed rebuilding). Applied as a COMPONENT override on Body, so every
	// other window in the city keeps using the real, untouched M_window.
	//
	// One shared toggle, not independent driver/passenger sides - the mesh
	// has no seam to split them on. If a rigged replacement car (or a
	// manually re-exported SM_car_01) ever lands with body and glass as
	// separate pieces, that is the point to revisit this for real.
	// ---------------------------------------------------------------------

	/** Replicated - anyone looking at the rover, not just the driver,
	 *  should see whether it is buttoned up or open. */
	UPROPERTY(ReplicatedUsing = OnRep_WindowsDown, BlueprintReadOnly, Category = "Rover|Windows")
	bool bWindowsDown = false;

	UFUNCTION(BlueprintPure, Category = "Rover|Windows")
	bool AreWindowsDown() const { return bWindowsDown; }

	/** Server only. Flips bWindowsDown; UpdateWindowOpacity eases the visual
	 *  toward it every Tick rather than snapping. */
	void ToggleWindows();

	/** How long a full roll takes, seconds. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Rover|Windows", meta = (ClampMin = "0.05"))
	float WindowRollSeconds = 0.4f;

	// ---------------------------------------------------------------------
	// RADIO  (KMHR 5280)
	//
	// One station. The playlist is every music track in the project: Repo Rally,
	// Freaks, Swept Away, the SoulCity sample, and Meet the Whoops. [T] tunes in
	// or off. [C] moves to the next song while the station is on.
	// Tuning in does not start a file at the top. It drops into a song at a
	// random point in the middle, the way a station is already on. When that
	// file ends, the next one plays from the start and the list loops.
	// ---------------------------------------------------------------------

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Rover|Radio")
	TArray<TObjectPtr<USoundBase>> RadioTracks;

	/** Replicated - the radio plays FROM the car, so everyone near it (not
	 *  just the driver) should hear the same thing. */
	UPROPERTY(ReplicatedUsing = OnRep_RadioOn, BlueprintReadOnly, Category = "Rover|Radio")
	bool bRadioOn = false;

	UFUNCTION(BlueprintPure, Category = "Rover|Radio")
	bool IsRadioOn() const { return bRadioOn; }

	/** "KMHR 5280   REPO RALLY" while the station is on. Empty when it is off. */
	UFUNCTION(BlueprintPure, Category = "Rover|Radio")
	FString GetRadioLine() const;

	/**
	 * Server. Tunes the cab into KMHR. Track < 0 picks a song at random; otherwise
	 * that playlist index. Either way the file starts somewhere in its middle.
	 */
	void TuneIn(int32 Track = -1);

	/** Server only. Flips the station. Turning it on is a fresh tune-in. */
	void ToggleRadio();

	/** Server only. The next song, already in progress, while the station is on. */
	void CycleRadio();

protected:
	UFUNCTION()
	void OnRep_Driver();

	void ApplyDriverState();

	/** The actual seat-and-possess, run once the "getting in" beat EnterRover
	 *  started has finished playing (or immediately, if the driver has no
	 *  EnterVehicleAnimation configured). Re-checks the same guards EnterRover
	 *  already did - the driver may have died, been possessed elsewhere, or
	 *  the rover may already be occupied by the time this fires. */
	void FinishEnteringRover(ADCCCharacterBase* NewDriver);

	/** Keeps the body on whatever is under it. A trace rather than physics -
	 *  see the class comment on why this is not simulated. */
	void SnapToGround(float DeltaSeconds);

	/** Held-key handlers. Each pair just sets the -1/0/+1 input floats that
	 *  Tick consumes; see SetupPlayerInputComponent on why not axes. */
	void DriftPressed();
	void DriftReleased();
	void BoostPressed();
	void CameraLook(float Value);
	void CameraPitchLook(float Value);
	void HookAimPressed();

	/** Blocking-hit handler: stop or bounce depending on what was hit. */
	virtual void NotifyHit(class UPrimitiveComponent* MyComp, AActor* Other,
		class UPrimitiveComponent* OtherComp, bool bSelfMoved, FVector HitLocation,
		FVector HitNormal, FVector NormalImpulse, const FHitResult& Hit) override;

	/** True when Other's bounds are comparable to or larger than the rover's. */
	bool IsLargerThanRover(const AActor* Other) const;

	/** Server only. Marks Piloting and awards XP for time spent actually driving. */
	void TickPilotingProgress(float DeltaSeconds);

	void InputExit();

	void EjectPressed();
	void ApplyPassengerState();

	void WindowTogglePressed();

	UFUNCTION()
	void OnRep_WindowsDown();

	/** Eases WindowMID's Opacity parameter toward 0 (down) or 1 (up). Lazily
	 *  creates WindowMID from M_CarWindow_Translucent on first call. */
	void UpdateWindowOpacity(float DeltaSeconds);

	UPROPERTY(Transient)
	TObjectPtr<class UMaterialInstanceDynamic> WindowMID;

	float WindowOpacityCurrent = 1.0f;

	void RadioTogglePressed();
	void RadioChannelPressed();

	UFUNCTION()
	void OnRep_RadioOn();

	UFUNCTION()
	void OnRep_RadioTune();

	/** Plays or stops the current station track. The seek and the song are
	 *  already chosen (RadioStartOffset / RadioTrackIndex). Local playback;
	 *  the on/off state, the track and the seek are what replicate. */
	void ApplyRadioState();

	/** Server. When the current file runs out, the next playlist entry starts at the top. */
	void AdvanceRadio(float DeltaSeconds);

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Rover|Radio")
	TObjectPtr<class UAudioComponent> RadioAudio;

	/** Which playlist entry is on. Replicated so every cab hears the same song. */
	UPROPERTY(Replicated)
	int32 RadioTrackIndex = 0;

	/** Seconds into the file. Nonzero only for a tune-in; the following songs start at 0. */
	UPROPERTY(Replicated)
	float RadioStartOffset = 0.0f;

	/** Bumps on every tune-in, track change and shutoff, so clients replay once. */
	UPROPERTY(ReplicatedUsing = OnRep_RadioTune)
	int32 RadioTuneSerial = 0;

	/** Labels parallel to RadioTracks. Filled in the constructor, same on every machine. */
	TArray<FString> RadioTrackNames;

	/** Server clock for the file that is on. Not replicated; clients just play the seek they were given. */
	float RadioTrackElapsed = 0.0f;
	float RadioTrackPlaySeconds = 0.0f;

	/** Last RadioTuneSerial this machine actually started, so two rep notifies don't restart the song. */
	int32 AppliedRadioSerial = -1;

	void ApplyWreckerLook();
	void UpdateWreckerCrane(float DeltaSeconds);

	/** Crane arm, hinged at the knuckle. Magnet hangs on it until a lock, then rides the mark. */
	UPROPERTY(Transient)
	TObjectPtr<UStaticMeshComponent> CraneBoom;

	UPROPERTY(Transient)
	TObjectPtr<UStaticMeshComponent> Magnet;

	/** Magenta beam from the boom tip to the magnet while it is paid out. */
	UPROPERTY(Transient)
	TObjectPtr<UStaticMeshComponent> WinchLine;

	/** 0 magnet seated on the boom, 1 stuck to the mark. */
	float MagnetAlpha = 0.0f;

	/** Server+local. Turns flight on; assumes CanEngageFlight() already passed. */
	void EngageFlight();
	void EndFlight();

	/** Ground trace + slope handling + airborne fall. Replaces the old
	 *  unconditional Z lerp, which buried the nose in anything rising. */
	void UpdateGroundAndAir(float DeltaSeconds);

	/** Pitches/rolls the body to match the last surface normal. */
	void AlignToSurface(float DeltaSeconds);

	/** Ground height a little way in front, or -BIG_NUMBER if nothing is there. */
	float ProbeGroundAhead() const;

	/** Top of whatever was hit, measured just beyond the contact point. */
	float MeasureObstacleTop(const FHitResult& Hit) const;

	void UpdateTrail();

	float ThrottleInput = 0.0f;
	float SteerInput = 0.0f;
	float VerticalInput = 0.0f;

	/** Last tick's Space state, so the per-tick poll can still detect a fresh
	 *  press for CanEngageFlight() (a one-shot action) alongside the level
	 *  VerticalInput it also drives - see the poll block at the top of Tick(). */
	bool bWasSpaceDownLastTick = false;

	bool bAirborne = false;
	bool bClimbingStep = false;

	/** Z the rover has been asked to rise to by a contact this frame. */
	float PendingStepUpZ = 0.0f;
	bool bFlying = false;
	float FlightSecondsRemaining = 0.0f;

	/** Vertical speed while airborne. Separate from the movement component,
	 *  which only ever drives the horizontal plane here. */
	float VerticalVelocity = 0.0f;

	/** Highest Z reached since leaving the ground, and how far below it we are. */
	float ApexZ = 0.0f;
	float FallenDistance = 0.0f;

	/** Suspension spring state - see SuspensionStiffness's own comment. */
	float SuspensionOffset = 0.0f;
	float SuspensionVelocity = 0.0f;

	/**
	 * +1 if the driver walked up to the rover's right, -1 if its left, as
	 * measured at the moment they actually got in. Zoe, 2026-09-05: "make
	 * sure when you exit the car, you leave on the same side you got in in."
	 * Server-only - ExitRover is the only reader and only ever runs with
	 * authority, so this needs no replication.
	 */
	float DriverEntrySide = 1.0f;

	/** Parks an occupant at a seat offset, visible, on every machine. */
	void SeatOccupant(ADCCCharacterBase* Occupant, const FVector& SeatOffset);

	/** Surface normal of whatever was last under the rover. */
	FVector LastGroundNormal = FVector::UpVector;

	/** 0..1 throttle ramp, so speed builds rather than snapping to cruise. */
	float SpeedRamp = 0.0f;

	bool bDrifting = false;
	bool bBoosting = false;
	float BoostSecondsRemaining = 0.0f;
	float BoostCooldownRemaining = 0.0f;

	/** Controls ignored while this is running down, after a crash. */
	float CrashRecoveryRemaining = 0.0f;

	/** Camera yaw offset the player has dialled in, and where it is heading. */
	float CameraYawOffset = 0.0f;
	float CameraYawTarget = 0.0f;

	/** Pitch while the hook camera is up. The chase view ignores it and sits at -18. */
	float CameraPitch = -8.0f;

	float PilotingSecondsAccumulated = 0.0f;
	bool bDriftedCleanlyThisInterval = false;
};
