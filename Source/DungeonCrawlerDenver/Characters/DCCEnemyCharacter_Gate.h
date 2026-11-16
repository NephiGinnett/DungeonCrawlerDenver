#pragma once

#include "CoreMinimal.h"
#include "Characters/DCCEnemyCharacter.h"
#include "DCCEnemyCharacter_Gate.generated.h"

class UDCCBossComponent;

/**
 * Gate Run's own cast (2026-11-15, Zoe: "a boss room for gate run would be perfect. Even several room types if you
 * have ideas or can make characters for them"). NPC Forge bodies, each touched by the gate in its own way, so a
 * gate's people read as belonging to the gate and not to the street:
 *
 *  - The Gatekeeper: the boss in the farthest room. Big, violet-tinted, slams the floor where you stand (the arena
 *    telegraphs it), calls shades at two-thirds and one-third health, and still has the boss charge.
 *  - A Champion: the DUEL room's one fighter. Gold-tinted, tough, with a rolled name.
 *  - Gate Shades: see-through violet things that pour out of the AMBUSH room and the Gatekeeper's walls. Weak, fast.
 */
UCLASS()
class DUNGEONCRAWLERDENVER_API ADCCEnemyCharacter_Gatekeeper : public ADCCEnemyCharacter
{
	GENERATED_BODY()

public:
	ADCCEnemyCharacter_Gatekeeper();
	virtual void BeginPlay() override;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "DCC")
	TObjectPtr<UDCCBossComponent> Boss;
};

UCLASS()
class DUNGEONCRAWLERDENVER_API ADCCEnemyCharacter_GateChampion : public ADCCEnemyCharacter
{
	GENERATED_BODY()

public:
	ADCCEnemyCharacter_GateChampion();
	virtual void BeginPlay() override;
	/** A champion's name, from a seed, so every gate's champion is somebody. */
	static FString RollName(int32 Seed);
};

UCLASS()
class DUNGEONCRAWLERDENVER_API ADCCEnemyCharacter_GateShade : public ADCCEnemyCharacter
{
	GENERATED_BODY()

public:
	ADCCEnemyCharacter_GateShade();
	virtual void BeginPlay() override;
	virtual void Tick(float DeltaSeconds) override;
};
