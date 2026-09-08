#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "DCCFlightRing.generated.h"

class UStaticMeshComponent;
class UBoxComponent;
class UNiagaraComponent;
class UNiagaraSystem;
class USoundBase;
class ADCCRover;

/**
 * A floating ring the rover flies through - a pure teaching aid for the
 * flight system (Zoe's ask, 2026-09-03: "a few floating rings... to fly
 * through as they learn"), no gameplay stakes attached.
 *
 * Deliberately no collision on the ring mesh itself - a slightly-missed pass
 * should never crash or bounce the player mid-lesson. The only thing that
 * matters is PassThroughVolume, a thin box sized to the ring's own opening,
 * which is what actually detects a clean pass.
 *
 * ONE-SHOT as of 2026-09-04 (Zoe: "make sure that the target rings can only
 * be triggered once, and disappear after being triggered") - bHasEverBeenPassed
 * now gates the pass reward itself, not just the "next objective" glow below,
 * and OnRep_HasEverBeenPassed hides the mesh and drops both components to
 * NoCollision the moment it flips true. See NotifyActorBeginOverlap's own
 * comment for the mechanism.
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

	/**
	 * One-shot burst spawned at the ring on a clean pass, separate from
	 * ObjectiveGlow - that component is the persistent "this is the next
	 * one" indicator and stays looping/attached; this is fire-and-forget
	 * flavour for the moment of passing itself. Zoe, 2026-09-04: "going
	 * through the rings play a small sound and flare to indicate you made
	 * it." Defaults to the same portal-energy system the ring already uses
	 * elsewhere, since this project's VFX library has little else glowing.
	 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "FlightRing")
	TObjectPtr<UNiagaraSystem> PassFlareEffect;

	/** Played at the ring on a clean pass. Unset by default - no suitable
	 *  chime/success SFX exists anywhere in the project yet to default to;
	 *  assign one once Zoe has a sound she likes. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "FlightRing")
	TObjectPtr<USoundBase> PassSound;

	/**
	 * Grants the driver a full boost on a clean pass, on top of
	 * PilotingBonusSeconds - see ADCCRover::GrantBoost for why this ignores
	 * the normal boost cooldown rather than merely starting it early.
	 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "FlightRing")
	bool bBoostOnPass = true;

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
