#pragma once

#include "CoreMinimal.h"
#include "Characters/DCCEnemyCharacter.h"
#include "DCCEnemyCharacter_Unified.generated.h"

/**
 * Enemies from the uniform bone maker (2026-11-25, Tools/unify_rig.py + Tools/import_unified.py): Fab monsters whose own
 * rigs were rebuilt on the shared crawler skeleton, so the whole crawler animation set (walk, run, swings, casts,
 * emotes) plays on them through DCCCrawlerAnimSet. One small class per monster: the mesh, a size, a few stats.
 * Their physics assets are generated, untuned, so they puff out on death instead of ragdolling.
 */
UCLASS(Abstract)
class DUNGEONCRAWLERDENVER_API ADCCEnemyCharacter_Unified : public ADCCEnemyCharacter
{
	GENERATED_BODY()

public:
	ADCCEnemyCharacter_Unified();

protected:
	/** For a subclass constructor: the mesh, and the capsule fitted to its height in centimetres. */
	void UseUnifiedMesh(class USkeletalMesh* BodyMesh, float HeightCm);
};

/** The Mushroom Demon (Fab free sample): a lanky fungal stalker. */
UCLASS()
class DUNGEONCRAWLERDENVER_API ADCCEnemy_MushroomDemon : public ADCCEnemyCharacter_Unified
{
	GENERATED_BODY()

public:
	ADCCEnemy_MushroomDemon();
};

/** The Demon Bull (Fab free sample): a hunched brute with a flail. */
UCLASS()
class DUNGEONCRAWLERDENVER_API ADCCEnemy_DemonBull : public ADCCEnemyCharacter_Unified
{
	GENERATED_BODY()

public:
	ADCCEnemy_DemonBull();
};
