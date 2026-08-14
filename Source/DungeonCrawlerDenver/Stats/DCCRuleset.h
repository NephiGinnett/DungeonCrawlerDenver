#pragma once

#include "CoreMinimal.h"
#include "UObject/Object.h"
#include "Stats/DCCAbilityTypes.h"
#include "DCCRuleset.generated.h"

/**
 * THE SWAP-IN POINT.
 *
 * Every piece of rules arithmetic in the game goes through this object rather
 * than being hardcoded at call sites. When the real DCC TTRPG ruleset ships,
 * it becomes a subclass (or a Blueprint child) overriding these methods, and
 * nothing that *calls* the rules has to change.
 *
 * The default implementation here is a standard d20-style placeholder, exactly
 * as the design doc specifies.
 *
 * Authority: ResolveCheck consumes randomness, so it must only be called on the
 * server. Clients receive the resulting FDCCCheckResult; they never roll.
 */
UCLASS(Blueprintable, BlueprintType)
class DUNGEONCRAWLERDENVER_API UDCCRuleset : public UObject
{
	GENERATED_BODY()

public:
	/**
	 * Modifier = floor((Score - 10) / 2).
	 *
	 * Note this genuinely needs a float floor rather than C++ integer division:
	 * integer division truncates toward zero, so a score of 7 would yield -1
	 * instead of the correct -2. Getting this wrong is subtle and only shows up
	 * on dumped stats, which is exactly when a player notices.
	 */
	UFUNCTION(BlueprintCallable, Category = "Ruleset")
	virtual int32 GetAbilityModifier(int32 Score) const;

	/** Sides on the check die. Isolated so a ruleset using a different core die can override it. */
	UFUNCTION(BlueprintCallable, Category = "Ruleset")
	virtual int32 GetCheckDieSides() const;

	/** Server-only. Rolls the die, applies modifiers, and reports the full arithmetic. */
	UFUNCTION(BlueprintCallable, Category = "Ruleset")
	virtual FDCCCheckResult ResolveCheck(
		const FDCCAbilityScoreSet& AbilityScores,
		EDCCAbility Ability,
		int32 DifficultyClass,
		int32 SituationalBonus = 0) const;

	/** Starting/derived hit points. Placeholder curve until the real ruleset lands. */
	UFUNCTION(BlueprintCallable, Category = "Ruleset")
	virtual int32 GetMaxHitPoints(const FDCCAbilityScoreSet& AbilityScores, int32 Level) const;

	/** How hard this character is to hit. The DC an incoming attack must beat. */
	UFUNCTION(BlueprintCallable, Category = "Ruleset")
	virtual int32 GetDefenceValue(const FDCCAbilityScoreSet& AbilityScores, int32 Level) const;

	/**
	 * Maximum spell resource.
	 *
	 * The published DCC preview is explicit that Intelligence grants Mana at a
	 * 1:1 ratio, so this placeholder already matches the real rule rather than
	 * inventing a curve we would have to unpick later.
	 */
	UFUNCTION(BlueprintCallable, Category = "Ruleset")
	virtual int32 GetMaxManaPoints(const FDCCAbilityScoreSet& AbilityScores, int32 Level) const;

	/**
	 * Server-only. Rolls to hit against the defender's defence value, then rolls
	 * damage if it lands.
	 *
	 * Attack and damage live here rather than on the character for the same
	 * reason every other rule does: when the real DCC ruleset lands, a subclass
	 * overrides this and no combat code changes.
	 */
	UFUNCTION(BlueprintCallable, Category = "Ruleset")
	virtual FDCCAttackResult ResolveAttack(
		const FDCCAbilityScoreSet& AttackerScores,
		EDCCAbility AttackAbility,
		int32 WeaponDamageDie,
		const FDCCAbilityScoreSet& DefenderScores,
		int32 DefenderLevel,
		int32 SituationalBonus = 0) const;

	// -------------------------------------------------------------------------
	// SKILL RANKS AND ADVANCEMENT
	//
	// Skills improve through use rather than through spent points: using one
	// marks it, and marked skills later roll to advance. The published rule makes
	// the roll self-limiting - you roll against your own current Rank - so no
	// experience curve is needed anywhere, and none is invented here.
	// -------------------------------------------------------------------------

	/** Highest Rank reachable on the tutorial floors. */
	UFUNCTION(BlueprintCallable, Category = "Ruleset|Skills")
	virtual int32 GetMaxSkillRank() const;

	/**
	 * Server-only. A skill check: the core check plus the character's Rank.
	 *
	 * An untrained attempt rolls at Disadvantage (2d20, keep the lower) and adds
	 * no Rank, which is what makes trying something you are bad at a real risk
	 * rather than a free lottery ticket.
	 */
	UFUNCTION(BlueprintCallable, Category = "Ruleset|Skills")
	virtual FDCCCheckResult ResolveSkillCheck(
		const FDCCAbilityScoreSet& AbilityScores,
		EDCCAbility Ability,
		int32 SkillRank,
		int32 DifficultyClass,
		bool bUntrained = false,
		int32 SituationalBonus = 0) const;

	/**
	 * Server-only. Rolls whether a marked skill gains a Rank.
	 *
	 * Rolls 1d20 against the skill's CURRENT Rank as the difficulty, so early
	 * ranks come almost for free and later ones rarely land. That single line is
	 * the entire progression curve; resist the urge to add a second one.
	 */
	UFUNCTION(BlueprintCallable, Category = "Ruleset|Skills")
	virtual bool ResolveAdvancementCheck(int32 CurrentRank) const;

	// -------------------------------------------------------------------------
	// PHYSICAL STAT EFFECTS
	//
	// These exist so ability scores are felt in the world rather than only on a
	// character sheet: a strong crawler lifts the heavy crate and throws it
	// across the room, a nimble one moves faster. They live on the ruleset with
	// every other piece of arithmetic, so the real DCC ruleset overrides them
	// alongside everything else.
	//
	// Reference point throughout: an average score of 10 (modifier 0) yields the
	// plain base value, so tuning the base tunes the "ordinary person" case and
	// the modifier only ever describes the deviation from it.
	// -------------------------------------------------------------------------

	/** A mass of this many kilograms is what the numbers below are tuned around. */
	static constexpr float ReferenceObjectMass = 25.0f;

	/**
	 * Heaviest object this character can lift, in kilograms. Strength-driven.
	 *
	 * Deliberately generous at the low end - a crawler who cannot pick up
	 * anything at all reads as a broken pickup prompt rather than as a
	 * characterisation of their Strength.
	 */
	UFUNCTION(BlueprintCallable, Category = "Ruleset|Physical")
	virtual float GetCarryMassLimit(const FDCCAbilityScoreSet& AbilityScores) const;

	/**
	 * Launch speed in cm/s for a thrown object of the given mass. Strength-driven,
	 * and reduced by mass so that heavy things are worth throwing for damage while
	 * light things are worth throwing for reach.
	 */
	UFUNCTION(BlueprintCallable, Category = "Ruleset|Physical")
	virtual float GetThrowSpeed(const FDCCAbilityScoreSet& AbilityScores, float ObjectMass) const;

	/**
	 * Damage a thrown object deals on impact.
	 *
	 * Scales with how fast it was actually travelling when it connected, not just
	 * with the thrower's Strength, so a spent object rolling to a stop cannot
	 * still kill someone.
	 */
	UFUNCTION(BlueprintCallable, Category = "Ruleset|Physical")
	virtual int32 GetThrowDamage(
		const FDCCAbilityScoreSet& AbilityScores,
		int32 ImpactDamageDie,
		float ImpactSpeed) const;

	/**
	 * Multiplier applied to walk speed, from Dexterity.
	 *
	 * Clamped hard on both ends: movement speed is the stat players feel most
	 * immediately, and an unclamped modifier turns a dumped or maxed score into
	 * either a crawl or a blur that breaks the animation blendspace.
	 */
	UFUNCTION(BlueprintCallable, Category = "Ruleset|Physical")
	virtual float GetMoveSpeedMultiplier(const FDCCAbilityScoreSet& AbilityScores) const;
};
