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

	/**
	 * Seconds a caster stands still after casting, rooted in place.
	 *
	 * Casting on the move reads as nothing happening at all - the spell effect
	 * slides along behind a running character and the moment is lost. Holding
	 * still also gives the party a readable window: you can see who is casting
	 * and go and interrupt them.
	 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "DCC|AI", meta = (ClampMin = "0.0"))
	float CastHoldSeconds = 1.5f;

	UFUNCTION(BlueprintPure, Category = "DCC|AI")
	ADCCCharacterBase* GetCurrentTarget() const { return CurrentTarget; }

	/**
	 * Flips this character to the crawlers' side: an ally NPC rather than a mob.
	 *
	 * One flag rather than a separate ally class, because the behaviour is
	 * identical - find the nearest hostile, close, swing. Only the definition of
	 * "hostile" changes, and IsHostileTo already derives that from IsCrawler.
	 *
	 * Allies do count as room occupants, so a room full of friendly NPCs can arm
	 * an encounter. That is deliberate: the party's own NPCs walking into a trap
	 * should spring it.
	 */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "DCC|AI")
	bool bIsAlly = false;

	/** Dungeon-side unless bIsAlly flips it. Drives the whole faction check. */
	virtual bool IsCrawler() const override { return bIsAlly; }

	// -------------------------------------------------------------------------
	// REPOSSESSION NIP
	//
	// The Debt Eaters' actual threat. Their damage is trivial and always will be -
	// what makes them dangerous is that every bite converts the party's gold into
	// the boss's health, so ignoring them to focus the boss is a losing trade even
	// though it looks like the obvious play.
	// -------------------------------------------------------------------------

	/** Gold taken from the target on every landed hit. Zero disables it. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "DCC|Leech", meta = (ClampMin = "0"))
	int32 GoldStolenPerHit = 0;

	/**
	 * Hit points the summoner gains per gold stolen.
	 *
	 * Separate from the amount taken so the exchange rate is tunable: at 1.0 a
	 * broke party neuters the boss entirely, which is a legitimate and funny
	 * outcome the fight should be allowed to have.
	 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "DCC|Leech", meta = (ClampMin = "0.0"))
	float SummonerHealPerGold = 1.0f;

	/**
	 * Who the stolen gold feeds. Resolved from the encounter's boss when unset.
	 *
	 * Held as a soft reference rather than set at spawn because minions are
	 * summoned by waves that do not know which spawn was flagged as the boss.
	 */
	UPROPERTY(BlueprintReadOnly, Category = "DCC|Leech")
	TObjectPtr<ADCCCharacterBase> LeechBeneficiary;

protected:
	/** Applies Repossession Nip. Server only, called from PerformAttack. */
	virtual void OnDamageDealt(ADCCCharacterBase* Target, int32 Damage) override;

public:

protected:
	virtual void BeginPlay() override;

	/**
	 * Nearest living hostile inside AggroRadius, or null.
	 *
	 * Asks IsHostileTo rather than looking for a PlayerController, which is what
	 * makes allies work at all: a friendly NPC hunts mobs, a mob hunts crawlers
	 * and their allies, and neither needs to know which is player-controlled.
	 */
	ADCCCharacterBase* FindNearestHostileTarget() const;

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

	/** World time until which this character is rooted mid-cast. */
	float CastHoldUntilTime = 0.0f;
};
