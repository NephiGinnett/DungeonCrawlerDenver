#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "DCCExitPortal.generated.h"

class UBoxComponent;
class UStaticMeshComponent;
class ADCCCharacterBase;

/**
 * A one-way "end of the built content" exit - not ADCCFloorExit (that one
 * gates on a boss kill and leads to a next floor that exists). This is the
 * science-lab console's portal: off until switched on by an
 * ADCCPortalConsole, then a walk-in that takes a crawler to the "awaiting
 * other crawlers" screen (see ADCCCharacterBase::EnterAwaitingOtherCrawlers)
 * rather than anywhere else in the level - there is nowhere else to send
 * them yet.
 *
 * The visual is a spawned instance of whatever Blueprint PortalVisualClass
 * points at (BP_portal from the Alien_Portal pack, by default) rather than
 * anything built here - "one of our fancy portals from the library" was the
 * ask, and that Blueprint already does the warping-space look on its own.
 * Spawned and destroyed locally on every machine from OnRep_Active/Activate
 * rather than relying on the visual's own replication (most asset-pack
 * Blueprints do not set bReplicates), same reasoning as
 * ADCCFloorExit::ApplyLockedVisuals running on every machine instead of
 * trusting one authoritative copy to propagate.
 */
UCLASS()
class DUNGEONCRAWLERDENVER_API ADCCExitPortal : public AActor
{
	GENERATED_BODY()

public:
	ADCCExitPortal();

	virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;

	/** The console's own dormant frame - stays visible whether the portal is on or not. */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Portal")
	TObjectPtr<UStaticMeshComponent> Frame;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Portal")
	TObjectPtr<UBoxComponent> Trigger;

	/** Spawned in front of Frame once Activate() runs. Defaults to Alien_Portal's BP_portal. */
	UPROPERTY(EditAnywhere, Category = "Portal")
	TSubclassOf<AActor> PortalVisualClass;

	/** Local only, one per machine - see this class's own header comment on why. */
	UPROPERTY(Transient)
	TObjectPtr<AActor> SpawnedVisual;

	UPROPERTY(BlueprintReadOnly, ReplicatedUsing = OnRep_Active, Category = "Portal")
	bool bActive = false;

	UFUNCTION(BlueprintPure, Category = "Portal")
	bool IsActive() const { return bActive; }

	/** Server only. Turns the portal on - spawns the visual, arms the trigger, everywhere. */
	UFUNCTION(BlueprintCallable, Category = "Portal")
	void Activate();

protected:
	virtual void BeginPlay() override;

	UFUNCTION()
	void OnRep_Active();

	UFUNCTION()
	void HandleTriggerOverlap(UPrimitiveComponent* OverlappedComponent, AActor* OtherActor,
		UPrimitiveComponent* OtherComp, int32 OtherBodyIndex, bool bFromSweep, const FHitResult& SweepResult);

	/** Runs on every machine: spawns or destroys SpawnedVisual and (re)sets the trigger's collision to match bActive. */
	void ApplyActiveVisuals();
};
