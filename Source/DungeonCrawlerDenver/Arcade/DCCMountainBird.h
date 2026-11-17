#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "DCCMountainBird.generated.h"

class UStaticMeshComponent;

/**
 * A Fourteener sky raptor (2026-11-14, Zoe: "stronger barriers to fourteener like giant birds that fly around and
 * dive bomb or throw rocks at the players").
 *
 * A puppet: ADCCArcadeClimb::TickBirds flies it on the server and decides what it does. It replicates where it is and
 * a State, and every machine draws the tells from that: a scream and a red line to the spot it is about to dive at
 * (step off that line and it misses), and a rock held in its talons while it lines up a drop.
 */
UCLASS()
class DUNGEONCRAWLERDENVER_API ADCCMountainBird : public AActor
{
	GENERATED_BODY()

public:
	ADCCMountainBird();

	virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;
	virtual void BeginPlay() override;
	virtual void Tick(float DeltaSeconds) override;

	enum : uint8 { Circle = 0, WindUp = 1, Dive = 2, Recover = 3, Carry = 4 };

	/** Server: change state (plays the scream on the server's own screen too). */
	void SetState(uint8 NewState);
	uint8 GetState() const { return State; }

	/** Where a dive is aimed. Fixed when the wind-up starts, so a climber who moves aside is missed. */
	UPROPERTY(Replicated)
	FVector_NetQuantize DiveTarget;

protected:
	UPROPERTY(ReplicatedUsing = OnRep_State)
	uint8 State = Circle;

	UFUNCTION()
	void OnRep_State();
	void OnStateChanged();

	UPROPERTY(VisibleAnywhere)
	TObjectPtr<UStaticMeshComponent> Body;

	/** The red line to DiveTarget during a wind-up. Drawn only, never collides. */
	UPROPERTY(VisibleAnywhere)
	TObjectPtr<UStaticMeshComponent> AimLine;
	/** When the current state began (local Age), for the shadow's sweep. */
	float StateStart = 0.0f;

	/** The rock in its talons while it carries one to drop. */
	UPROPERTY(VisibleAnywhere)
	TObjectPtr<UStaticMeshComponent> HeldRock;

	FRotator BodyRest = FRotator::ZeroRotator;
	float Age = 0.0f;
};
