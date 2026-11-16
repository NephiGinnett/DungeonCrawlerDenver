#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "DCCHub.generated.h"

class ADCCCharacterBase;
class UBoxComponent;
class UStaticMesh;
class UMaterialInterface;

/**
 * Concourse Zero: the hub between runs (2026-11-13).
 *
 * A sealed concourse of Denver International, repossessed along with the rest
 * of the city and reopened as Crawler Services. Elyra's Tales surfaces you in a
 * town with people in it; this is that town. Two shops (Rooms/../Dialogue/
 * DCCHubVendors), a sponsor kiosk (the loot box screen, O), and the
 * Departures gate into the World Dungeon (ADCCTaleFloor). Every run ends back
 * here, and it counts as a Safe Room.
 *
 * Built at runtime like the tale floor, for the same reason: geometry placed by
 * hand in World Partition kept failing. It is not random, so every machine
 * builds the same walls locally in BeginPlay; only the shopkeepers and the safe
 * room volume come from the server.
 */
UCLASS(meta = (DisplayName = "DCC Hub (Concourse Zero)"))
class DUNGEONCRAWLERDENVER_API ADCCHub : public AActor
{
	GENERATED_BODY()

public:
	ADCCHub();

	/** Server: finds or spawns the hub. Clients: finds it if it has replicated. */
	static ADCCHub* GetOrCreate(UWorld* World);
	static ADCCHub* Find(const UWorld* World);

	/** Server. Teleports a crawler into the hub, in a ring so a party does not stack. */
	static bool SendToHub(ADCCCharacterBase* Crawler);

	FVector GetArrivalLocation(int32 Slot) const;

protected:
	virtual void BeginPlay() override;

	UPROPERTY(VisibleAnywhere, Category = "Hub")
	TObjectPtr<UBoxComponent> DeparturesPad;

	UFUNCTION()
	void HandleDepartures(UPrimitiveComponent* OverlappedComponent, AActor* OtherActor,
		UPrimitiveComponent* OtherComp, int32 OtherBodyIndex, bool bFromSweep, const FHitResult& SweepResult);

private:
	void BuildLocal();
	void SpawnServerActors();
	void Block(const FVector& LocalCenter, const FVector& Size, const FLinearColor& Colour);
	void Sign(const FVector& LocalLocation, float Yaw, const FString& Text, const FColor& Colour, float Size);
	void Light(const FVector& LocalLocation, const FLinearColor& Colour, float Intensity, float Radius);

	UPROPERTY(Transient)
	TObjectPtr<UStaticMesh> CubeMesh;

	UPROPERTY(Transient)
	TObjectPtr<UMaterialInterface> ShapeMaterial;

	UPROPERTY(Transient)
	TArray<TObjectPtr<AActor>> LocalActors;

	TMap<TWeakObjectPtr<AActor>, double> PadCooldown;

	static constexpr float HalfLength = 2000.0f;
	static constexpr float HalfWidth = 900.0f;
	static constexpr float WallHeight = 700.0f;
};
