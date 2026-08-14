#pragma once

#include "CoreMinimal.h"
#include "Characters/DCCCharacterBase.h"
#include "DCCEnemyCharacter.generated.h"

/**
 * A hostile combatant.
 *
 * The AI here is deliberately the simplest thing that produces a real fight:
 * acquire the nearest living player inside the aggro radius, close to melee
 * range, swing on cooldown. No behaviour tree, because a tree would need asset
 * authoring before the first encounter could be played at all, and this layer
 * is meant to be replaced by StateTree once encounters need real tactics.
 *
 * Runs on the server only. Clients see position through movement replication
 * and outcomes through the stats component.
 */
UCLASS()
class DUNGEONCRAWLERDENVER_API ADCCEnemyCharacter : public ADCCCharacterBase
{
	GENERATED_BODY()

public:
	ADCCEnemyCharacter();

	virtual void Tick(float DeltaSeconds) override;

	/** How far this enemy will notice a player from. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "DCC|AI", meta = (ClampMin = "0.0"))
	float AggroRadius = 1500.0f;

	/**
	 * Once aggroed, how far the target may get before this enemy gives up.
	 * Larger than AggroRadius so enemies do not visibly flicker in and out of
	 * pursuit at the boundary.
	 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "DCC|AI", meta = (ClampMin = "0.0"))
	float LeashRadius = 3000.0f;

	UFUNCTION(BlueprintPure, Category = "DCC|AI")
	ADCCCharacterBase* GetCurrentTarget() const { return CurrentTarget; }

	/** Dungeon-side. Enemies never count as room occupants for encounter triggering. */
	virtual bool IsCrawler() const override { return false; }

protected:
	virtual void BeginPlay() override;

	/** Nearest living player-controlled character inside AggroRadius, or null. */
	ADCCCharacterBase* FindNearestPlayerTarget() const;

private:
	UPROPERTY(Transient)
	TObjectPtr<ADCCCharacterBase> CurrentTarget;

	/** Set while a move order is outstanding, so we do not re-issue it every frame. */
	bool bIsChasing = false;
};
