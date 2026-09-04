#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "DCCCliffLift.generated.h"

class UStaticMeshComponent;

/**
 * A platform that slowly rises once requested, carrying whoever is standing on it -
 * built for the bartender's "ride up the cliff" offer near the Diving Pool, but not
 * specific to that NPC.
 *
 * Deliberately not physics-simulated: a Tick-driven Lerp between two authored Z
 * heights is all "slowly rises to the top" needs, and Unreal's own
 * CharacterMovementComponent already carries a standing character along with a
 * Movable actor's collision for free (the standard "moving platform" behaviour) - so
 * there is no custom carry logic to write here at all.
 *
 * Server-authoritative and one-shot, matching this project's "everything mechanical
 * is server-authoritative" rule even though a rising platform is cosmetic rather than
 * a dice roll: it is still a shared, physical, every-player-sees-it object, and two
 * clients disagreeing about where it is would be visibly wrong in exactly the way a
 * desynced door or elevator always is. RequestRaise() only ever does anything once;
 * after that every machine just plays out the same Lerp independently, driven off one
 * replicated server timestamp rather than a transform replicated every frame.
 */
UCLASS()
class DUNGEONCRAWLERDENVER_API ADCCCliffLift : public AActor
{
	GENERATED_BODY()

public:
	ADCCCliffLift();

	virtual void Tick(float DeltaSeconds) override;
	virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Lift")
	TObjectPtr<UStaticMeshComponent> PlatformMesh;

	/**
	 * How far above its starting position the platform rises.
	 *
	 * There is no way to know the right number from code alone - this has to be
	 * tuned in the editor once the lift is actually placed against the real
	 * clifftop geometry.
	 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Lift", meta = (ClampMin = "0.0"))
	float RaiseHeight = 2000.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Lift", meta = (ClampMin = "0.5"))
	float RaiseDurationSeconds = 6.0f;

	UFUNCTION(BlueprintPure, Category = "Lift")
	bool IsRising() const { return bIsRising; }

	UFUNCTION(BlueprintPure, Category = "Lift")
	bool IsAtTop() const { return bHasRisen; }

	/**
	 * Starts the rise if it has not already happened. Safe to call more than once,
	 * or from a client with no authority - both are silent no-ops, same as every
	 * other server-only mutator in this project.
	 */
	UFUNCTION(BlueprintCallable, Category = "Lift")
	void RequestRaise();

protected:
	virtual void BeginPlay() override;

private:
	/** True only while actively interpolating - see OnRep_Rising and Tick. */
	UPROPERTY(ReplicatedUsing = OnRep_Rising)
	bool bIsRising = false;

	/** True once it has reached RaiseHeight. Stays true forever - see class comment
	 *  on this being one-shot. */
	UPROPERTY(Replicated)
	bool bHasRisen = false;

	/** Server world time RequestRaise() was called. Every machine derives the same
	 *  Lerp alpha from this rather than trusting a transform to replicate smoothly. */
	UPROPERTY(Replicated)
	float RiseStartServerTime = 0.0f;

	/** Captured once in BeginPlay, so RaiseHeight is always relative to wherever
	 *  this was actually placed. */
	float StartZ = 0.0f;

	UFUNCTION()
	void OnRep_Rising();
};
