#pragma once

#include "CoreMinimal.h"
#include "Characters/DCCEnemyCharacter.h"
#include "DCCEnemyCharacter_DeadAir.generated.h"

class ADCCArcadeHaunt;

/**
 * Dead Air's own creatures (2026-11-16, Docs/enemy-roster-arcade.md): the Lucky Tuesday's history coming back wrong.
 * They replace the dungeon wildlife that used to stand in the stopes. Heard before they are seen.
 *
 *  - Lamp-eater: a see-through miner that creeps toward light. It only hunts someone whose lamp is on; switch your
 *    lamp off (L) and it loses you.
 *  - Tommyknocker: a small figure that never comes down off its spot. Three knocks, then the roof comes down where
 *    you are standing (Dead Air's rockfall, given a body). Kill it and the knocking stops.
 *  - Slag dog: a running lump of furnace slag, soot-black with embers in it. Fast, weak, in threes.
 *  - The shift boss: the deepest stope's one big presence. Slow, hits hard, carries a lamp that burns red.
 */
UCLASS()
class DUNGEONCRAWLERDENVER_API ADCCEnemyCharacter_LampEater : public ADCCEnemyCharacter
{
	GENERATED_BODY()

public:
	ADCCEnemyCharacter_LampEater();
	virtual void BeginPlay() override;
	virtual void Tick(float DeltaSeconds) override;
	virtual bool AcceptsTarget(const ADCCCharacterBase* Candidate) const override;

private:
	mutable TWeakObjectPtr<ADCCArcadeHaunt> Mine;
};

UCLASS()
class DUNGEONCRAWLERDENVER_API ADCCEnemyCharacter_Tommyknocker : public ADCCEnemyCharacter
{
	GENERATED_BODY()

public:
	ADCCEnemyCharacter_Tommyknocker();
	virtual void BeginPlay() override;
	virtual void Tick(float DeltaSeconds) override;

private:
	TWeakObjectPtr<ADCCArcadeHaunt> Mine;
	float NextKnockAt = 0.0f;
};

UCLASS()
class DUNGEONCRAWLERDENVER_API ADCCEnemyCharacter_SlagDog : public ADCCEnemyCharacter
{
	GENERATED_BODY()

public:
	ADCCEnemyCharacter_SlagDog();
	virtual void BeginPlay() override;
};

UCLASS()
class DUNGEONCRAWLERDENVER_API ADCCEnemyCharacter_ShiftBoss : public ADCCEnemyCharacter
{
	GENERATED_BODY()

public:
	ADCCEnemyCharacter_ShiftBoss();
	virtual void BeginPlay() override;
};
