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
	float MaxCruiseSpeed = 4200.0f;

	/** How long full throttle takes to wind up from a standstill to cruise.
	 *  A ramp rather than instant top speed - the point of the request was
	 *  that it should build. Raised from 3.5 (Zoe, 2026-09-03: acceleration
	 *  is "exciting for a moment" but "missing a certain weight... throwing
	 *  heavy weight around that drags a little" - see Movement->Deceleration
	 *  in the constructor for the other half of that ask). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Rover|Speed")
	float SecondsToReachCruise = 5.0f;

	/** Speed while boosting. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Rover|Boost")
	float BoostSpeed = 7000.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Rover|Boost")
	float BoostDurationSeconds = 2.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Rover|Boost")
	float BoostCooldownSeconds = 6.0f;

	UFUNCTION(BlueprintPure, Category = "Rover|Boost")
	bool IsBoosting() const { return bBoosting; }

	/** 0..1, for a HUD readout. 1 means ready. */
	UFUNCTION(BlueprintPure, Category = "Rover|Boost")
	float GetBoostChargeFraction() const;

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
	float HoverBobAmplitude = 12.0f;

	/** One full up-down cycle, in seconds. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Rover|Feel")
	float HoverBobPeriodSeconds = 3.0f;

	/** Extra nose-up pitch at full MaxCruiseSpeed, added on top of whatever
	 *  the surface itself already asks for. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Rover|Feel")
	float MaxSpeedTiltDegrees = 6.0f;

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
	float Gravity = 2800.0f;

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
	float FlightSpeed = 5200.0f;

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
	// FLIGHT ANGLE - "flaps"
	//
	// Hold left mouse button and drag up/down to set the nose pitch, like a
	// flap lever rather than a joystick: it holds wherever you left it when
	// you release the button, and only moves again when you click-drag it.
	// The angle is not just cosmetic - it drives climb/dive directly (sin of
	// the angle times FlightPitchLiftRate), on top of whatever Space/Ctrl are
	// doing, so a player can fly a smooth arc with the mouse instead of only
	// having the two blunt climb/descend keys.
	// ---------------------------------------------------------------------

	/** Degrees the nose pitches at full drag deflection. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Rover|Flight|Angle")
	float MaxFlightPitchDegrees = 35.0f;

	/** Vertical speed contributed at full pitch deflection, cm/s - what turns
	 *  the visual tilt into an actual climb or dive. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Rover|Flight|Angle")
	float FlightPitchLiftRate = 1800.0f;

	/** -1..1 change per pixel of mouse drag while the button is held. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Rover|Flight|Angle")
	float FlightPitchDragSensitivity = 0.01f;

	/** How fast the visual nose eases toward the dragged setting. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Rover|Flight|Angle")
	float FlightPitchAlignSpeed = 4.0f;

	/** -1..1. For a HUD readout - a flap gauge, an artificial horizon tick. */
	UFUNCTION(BlueprintPure, Category = "Rover|Flight|Angle")
	float GetFlightPitchInput() const { return FlightPitchInput; }

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
	// RADIO
	//
	// Zoe's ask, 2026-09-03: "a car radio button while driving... I will
	// import music but use samples for now so I know it's working." Only one
	// real music asset exists anywhere in this project's library
	// (SoulCity's Soul_Music_FlyUp01_Cue), so RadioTracks ships with exactly
	// that one entry - an array on purpose, not a single sound reference, so
	// dropping in real tracks later is a data change, not a code change.
	// ---------------------------------------------------------------------

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Rover|Radio")
	TArray<TObjectPtr<USoundBase>> RadioTracks;

	/** Replicated - the radio plays FROM the car, so everyone near it (not
	 *  just the driver) should hear the same thing. */
	UPROPERTY(ReplicatedUsing = OnRep_RadioOn, BlueprintReadOnly, Category = "Rover|Radio")
	bool bRadioOn = false;

	UFUNCTION(BlueprintPure, Category = "Rover|Radio")
	bool IsRadioOn() const { return bRadioOn; }

	/** Server only. Flips bRadioOn; ApplyRadioState makes the audio match. */
	void ToggleRadio();

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

	/** Blocking-hit handler: stop or bounce depending on what was hit. */
	virtual void NotifyHit(class UPrimitiveComponent* MyComp, AActor* Other,
		class UPrimitiveComponent* OtherComp, bool bSelfMoved, FVector HitLocation,
		FVector HitNormal, FVector NormalImpulse, const FHitResult& Hit) override;

	/** True when Other's bounds are comparable to or larger than the rover's. */
	bool IsLargerThanRover(const AActor* Other) const;

	/** Server only. Marks Piloting and awards XP for time spent actually driving. */
	void TickPilotingProgress(float DeltaSeconds);

	void ThrottleForwardPressed();
	void ThrottleForwardReleased();
	void ThrottleReversePressed();
	void ThrottleReverseReleased();
	void SteerLeftPressed();
	void SteerLeftReleased();
	void SteerRightPressed();
	void SteerRightReleased();
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

	UFUNCTION()
	void OnRep_RadioOn();

	/** Plays/stops RadioTracks[0] on RadioAudio to match bRadioOn. Runs on
	 *  every machine (called from both ToggleRadio, for the server's own
	 *  copy, and OnRep_RadioOn, for everyone else) - the audio itself is
	 *  local playback, only the on/off state is replicated. */
	void ApplyRadioState();

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Rover|Radio")
	TObjectPtr<class UAudioComponent> RadioAudio;

	void ClimbPressed();
	void ClimbReleased();
	void DescendPressed();
	void DescendReleased();

	void FlightPitchDragPressed();
	void FlightPitchDragReleased();
	void FlightPitchDrag(float Value);

	bool bFlightPitchDragHeld = false;

	/** -1..1. Set by dragging while bFlightPitchDragHeld; holds its value on
	 *  release, same reasoning as a physical flap lever - see the header
	 *  comment on this section. Reset to 0 in EndFlight. */
	float FlightPitchInput = 0.0f;

	/** Eased toward FlightPitchInput * MaxFlightPitchDegrees every Tick while
	 *  flying, so the nose tilts smoothly instead of snapping. Read by both
	 *  SnapToGround (the actual climb/dive) and AlignToSurface (the visual). */
	float FlightPitchAngleDegrees = 0.0f;

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

	float PilotingSecondsAccumulated = 0.0f;
	bool bDriftedCleanlyThisInterval = false;
};
