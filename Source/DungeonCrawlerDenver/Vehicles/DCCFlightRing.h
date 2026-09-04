#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "DCCFlightRing.generated.h"

class UStaticMeshComponent;
class UBoxComponent;
class UNiagaraComponent;
class ADCCRover;

/**
 * A floating ring the rover flies through - a pure teaching aid for the
 * flight system (Zoe's ask, 2026-09-03: "a few floating rings... to fly
 * through as they learn"), no gameplay stakes attached.
 *
 * Deliberately no collision on the ring mesh itself - a slightly-missed pass
 * should never crash or bounce the player mid-lesson. The only thing that
 * matters is PassThroughVolume, a thin box sized to the ring's own opening,
 * which is what actually detects a clean pass. Repeatable on purpose: this
 * is a practice course, not a one-shot achievement, so there is no "already
 * collected" gate on the notice itself - fly back through as many times as
 * it takes to get a feel for the controls.
 *
 * bHasEverBeenPassed exists ONLY to drive the "next objective" glow below,
 * not to block anything - a ring you have already cleared stays fully
 * flyable, it just stops being the one that's lit up.
 */
UCLASS(Blueprintable)
class DUNGEONCRAWLERDENVER_API ADCCFlightRing : public AActor
{
	GENERATED_BODY()

public:
	ADCCFlightRing();

	virtual void BeginPlay() override;
	virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "FlightRing")
	TObjectPtr<UStaticMeshComponent> RingMesh;

	/** Sized to the ring's inner opening, thin along the fly-through axis
	 *  (local +X) - see the class comment on why this drives detection
	 *  instead of the ring mesh's own collision. */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "FlightRing")
	TObjectPtr<UBoxComponent> PassThroughVolume;

	/**
	 * A blue portal-energy glow (Alien_Portal's own FX, matching the ring
	 * mesh's own pack for once - the project's VFX library is otherwise thin
	 * on anything glowing/techy). Starts inactive; only the current "next
	 * objective" ring in its course ever turns it on - see
	 * RefreshObjectiveHighlight.
	 */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "FlightRing")
	TObjectPtr<UNiagaraComponent> ObjectiveGlow;

	/** 1-based position in its course, purely for the notice text ("Ring 2
	 *  of 5") and for deciding which ring in the course is "next" - no other
	 *  mechanical effect. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "FlightRing")
	int32 RingIndex = 1;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "FlightRing")
	int32 RingCount = 1;

	/**
	 * Bonus progress toward the driver's next Piloting mark, in the same
	 * "seconds of piloting" units ADCCRover::PilotingTickSeconds (5s) counts
	 * in - the honest reward lever that system already uses. A clean pass
	 * is worth less than a full interval on its own; the rest still has to
	 * come from actually flying.
	 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "FlightRing")
	float PilotingBonusSeconds = 2.0f;

	/** Server-authoritative (see NotifyActorBeginOverlap on why it is gated
	 *  to HasAuthority rather than trusting every machine's own overlap).
	 *  Replicated so every viewer agrees on which ring is glowing. */
	UPROPERTY(ReplicatedUsing = OnRep_HasEverBeenPassed, BlueprintReadOnly, Category = "FlightRing")
	bool bHasEverBeenPassed = false;

protected:
	virtual void NotifyActorBeginOverlap(AActor* OtherActor) override;

	UFUNCTION()
	void OnRep_HasEverBeenPassed();

	/**
	 * Turns ObjectiveGlow on if this ring is the lowest-RingIndex ring in its
	 * course (matched by RingCount, in case more than one course ever
	 * exists) that has not been passed yet - off otherwise. Run from a
	 * lightweight repeating timer rather than Tick, since checking five
	 * actors twice a second costs nothing and this has no reason to be
	 * exactly frame-accurate.
	 */
	void RefreshObjectiveHighlight();

	FTimerHandle ObjectiveHighlightTimer;
};
