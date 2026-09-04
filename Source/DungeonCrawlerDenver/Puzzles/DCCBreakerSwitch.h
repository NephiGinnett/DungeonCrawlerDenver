#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "DCCBreakerSwitch.generated.h"

class UStaticMeshComponent;
class UPointLightComponent;
class ADCCCharacterBase;
class ADCCPuzzleDoor;

/**
 * One breaker in a panel where flipping any switch also flips its neighbours.
 *
 * "Lights Out", themed as balancing load across a grid that is visibly not
 * rated for what is being asked of it. Solved when every breaker in the group
 * is on at once.
 *
 * Neighbours are authored per placement rather than derived from world
 * position, because a hand-placed panel is not necessarily a tidy grid and
 * guessing adjacency from distance would make a puzzle whose difficulty
 * depended on how carefully someone dragged the actors around.
 *
 * There is no forcing on an individual switch - the crowbar lives on the door
 * the group unlocks, which is where it belongs.
 */
UCLASS(Blueprintable)
class DUNGEONCRAWLERDENVER_API ADCCBreakerSwitch : public AActor
{
	GENERATED_BODY()

public:
	ADCCBreakerSwitch();

	virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;
	virtual void BeginPlay() override;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Puzzle")
	TObjectPtr<UStaticMeshComponent> Mesh;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Puzzle")
	TObjectPtr<UPointLightComponent> IndicatorLight;

	/** Which panel this breaker belongs to. All switches sharing it solve together. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Puzzle")
	FName PanelId = TEXT("LabPanel");

	/** Flipped alongside this one. Author both directions for symmetry. */
	UPROPERTY(EditInstanceOnly, Category = "Puzzle")
	TArray<TObjectPtr<ADCCBreakerSwitch>> Neighbours;

	/** Unlocked when every breaker sharing PanelId is on. Set on one switch or all. */
	UPROPERTY(EditInstanceOnly, Category = "Puzzle")
	TObjectPtr<ADCCPuzzleDoor> LinkedDoor;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Puzzle")
	bool bStartsOn = false;

	UFUNCTION(BlueprintPure, Category = "Puzzle")
	bool IsOn() const { return bOn; }

	/** Server only. Flips this breaker and its neighbours, then tests the panel. */
	UFUNCTION(BlueprintCallable, Category = "Puzzle")
	void Flip();

	void TryPlayerInteract(ADCCCharacterBase* Interactor);

protected:
	UPROPERTY(ReplicatedUsing = OnRep_On, BlueprintReadOnly, Category = "Puzzle")
	bool bOn = false;

	UFUNCTION()
	void OnRep_On();

	void ApplyOnVisuals();

	/** Server only. Unlocks LinkedDoor when the whole panel reads on. */
	void EvaluatePanel();
};
