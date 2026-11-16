#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "DCCNeonTraffic.generated.h"

class UStaticMeshComponent;

/**
 * A Neon Mile traffic car (2026-11-13): drives slowly up the road so you
 * overtake it rather than dodge a parked block.
 *
 * The server spawns and removes them (ADCCArcadeNeon). Only where it started,
 * when, how fast and its colour replicate; every machine then works out where
 * it is from the shared server clock, so the motion is smooth everywhere with
 * no movement replication. Only the body collides; the arena's crash check
 * uses GetCrashBox().
 */
UCLASS()
class DUNGEONCRAWLERDENVER_API ADCCNeonTrafficCar : public AActor
{
	GENERATED_BODY()

public:
	ADCCNeonTrafficCar();

	virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;
	virtual void BeginPlay() override;
	virtual void Tick(float DeltaSeconds) override;

	/** Server, before FinishSpawning. */
	void Init(const FVector& InStart, double InStartTime, float InSpeed, bool bInPink);

	/** Where the crash check treats the car as solid. */
	FBox GetCrashBox() const;

	/** Where the car is at server time Now. */
	FVector LocationAt(double Now) const;

	/** The shared clock every machine moves the cars by. */
	static double ClockOf(const UWorld* World);

	UPROPERTY(Replicated)
	FVector StartLocation = FVector::ZeroVector;

	UPROPERTY(Replicated)
	double StartTime = 0.0;

	UPROPERTY(Replicated)
	float Speed = 0.0f;

	UPROPERTY(Replicated)
	bool bPink = false;

private:
	void Paint();

	UPROPERTY(VisibleAnywhere)
	TObjectPtr<UStaticMeshComponent> Body;

	UPROPERTY(VisibleAnywhere)
	TObjectPtr<UStaticMeshComponent> Cabin;

	UPROPERTY(VisibleAnywhere)
	TObjectPtr<UStaticMeshComponent> TailLeft;

	UPROPERTY(VisibleAnywhere)
	TObjectPtr<UStaticMeshComponent> TailRight;

	UPROPERTY(VisibleAnywhere)
	TObjectPtr<UStaticMeshComponent> Underglow;
};
