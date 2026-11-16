#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "DCCSupplyChest.generated.h"

class UPointLightComponent;
class USphereComponent;
class UStaticMeshComponent;
class UTextRenderComponent;

UENUM()
enum class EDCCChestKind : uint8
{
	/** On a ledge: one random climbing item (DCCClimbItems::ChestPool). */
	Supply,
	/** The Outfitter's Crate at Fort Tiner: one of every tool and boost. */
	Outfitter,
	/** A glowing cache: one artifact. */
	Artifact,
};

/**
 * A Fourteener chest (2026-11-13). Walk into it to open it. Every player gets their
 * own pick from every chest, once a round, so friends never race each other for
 * loot; it shows open only to those who have looted it. The server hands the item
 * to ADCCArcadeClimb::GiveItem.
 */
UCLASS()
class DUNGEONCRAWLERDENVER_API ADCCSupplyChest : public AActor
{
	GENERATED_BODY()

public:
	ADCCSupplyChest();

	virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;
	virtual void BeginPlay() override;
	virtual void Tick(float DeltaSeconds) override;

	UPROPERTY(Replicated)
	EDCCChestKind Kind = EDCCChestKind::Supply;

	/** For an artifact cache: what is inside. */
	UPROPERTY(Replicated)
	FName ArtifactId;

	/** Player names that have taken their pick. */
	UPROPERTY(Replicated)
	TArray<FString> LootedBy;

private:
	UFUNCTION()
	void HandleOverlap(UPrimitiveComponent* OverlappedComponent, AActor* OtherActor, UPrimitiveComponent* OtherComp,
		int32 OtherBodyIndex, bool bFromSweep, const FHitResult& SweepResult);

	UPROPERTY(VisibleAnywhere)
	TObjectPtr<USphereComponent> Trigger;

	UPROPERTY(VisibleAnywhere)
	TObjectPtr<UStaticMeshComponent> Closed;

	UPROPERTY(VisibleAnywhere)
	TObjectPtr<UStaticMeshComponent> Open;

	UPROPERTY(VisibleAnywhere)
	TObjectPtr<UPointLightComponent> Glow;

	UPROPERTY(VisibleAnywhere)
	TObjectPtr<UTextRenderComponent> Label;

	bool bShownOpen = false;
};
