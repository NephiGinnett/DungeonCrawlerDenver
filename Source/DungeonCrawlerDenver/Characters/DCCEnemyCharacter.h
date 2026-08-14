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

	/**
	 * Whether this enemy will use its known skills instead of only swinging.
	 *
	 * On by default, but it does nothing unless the character actually knows a
	 * skill - so a plain melee enemy needs no configuration to stay plain.
	 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "DCC|AI")
	bool bUseSkills = true;

	/**
	 * Minimum seconds between skill attempts, on top of each skill's own cooldown.
	 *
	 * Without this a caster empties its entire hotbar on the first frame it sees
	 * a target, which is both unreadable and unsurvivable.
	 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "DCC|AI", meta = (ClampMin = "0.0"))
	float SkillAttemptInterval = 2.5f;

	UFUNCTION(BlueprintPure, Category = "DCC|AI")
	ADCCCharacterBase* GetCurrentTarget() const { return CurrentTarget; }

	/** Dungeon-side. Enemies never count as room occupants for encounter triggering. */
	virtual bool IsCrawler() const override { return false; }

protected:
	virtual void BeginPlay() override;

	/** Nearest living player-controlled character inside AggroRadius, or null. */
	ADCCCharacterBase* FindNearestPlayerTarget() const;

	/**
	 * Server only. Casts the longest-range currently-usable skill at Target.
	 *
	 * Returns true if something was cast, which the caller treats as "this turn
	 * is spent" - so a caster that just threw a spell does not also close to
	 * melee in the same frame.
	 *
	 * Longest range wins deliberately: it makes casters hang back and use the
	 * reach their spells give them, instead of walking into melee to cast a
	 * ranged attack, which looks broken even when the numbers are right.
	 */
	bool TryUseSkill(ADCCCharacterBase* Target);

private:
	UPROPERTY(Transient)
	TObjectPtr<ADCCCharacterBase> CurrentTarget;

	/** Set while a move order is outstanding, so we do not re-issue it every frame. */
	bool bIsChasing = false;

	/** World seconds of the last skill attempt. Server-side only. */
	float LastSkillAttemptTime = TNumericLimits<float>::Lowest();
};
