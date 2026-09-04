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

	/**
	 * Hit point multiplier for friendly NPCs. Between a mob's 1.0 and a player's.
	 *
	 * Not a rule from any book - the same concession as the player multiplier,
	 * scaled down because losing an ally should still be possible.
	 */
	UFUNCTION(BlueprintCallable, Category = "Ruleset")
	virtual float GetAllyToughnessMultiplier() const;

	/**
	 * Multiplier applied to a player-controlled character's hit points.
	 *
	 * Not a rule from any book - it is a concession to the fact that an NPC ally
	 * dying is a story beat while the player dying ends their evening. Until
	 * revives exist, the player's margin for a misread fight has to come from
	 * somewhere, and hit points are the least surprising place to put it.
	 *
	 * Lives on the ruleset rather than on the character so that turning it off
	 * later - when downed-and-revivable exists - is one override, not a hunt.
	 */
	UFUNCTION(BlueprintCallable, Category = "Ruleset")
	virtual float GetPlayerToughnessMultiplier() const;

	/**
	 * How hard this character is to hit. The DC an incoming attack must beat.
	 *
	 * EvadeBonus is equipment's contribution (a shield's FDCCEquipmentDef row,
	 * say) - kept as its own parameter rather than folded into AbilityScores
	 * because it is not a Dexterity change, it is a flat addition on top of
	 * whatever Dexterity already grants. Callers read it from
	 * UDCCEquipmentComponent::GetEvadeBonus(), which already summed it; this
	 * function just has nowhere else to put it before now.
	 */
	UFUNCTION(BlueprintCallable, Category = "Ruleset")
	virtual int32 GetDefenceValue(const FDCCAbilityScoreSet& AbilityScores, int32 Level, int32 EvadeBonus = 0) const;

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
	 *
	 * DefenderEvadeBonus is forwarded straight into GetDefenceValue - see that
	 * function's own comment. SituationalBonus stays what it always was: the
	 * ATTACKER's roll, not the defender's - the two are kept as separate
	 * parameters on purpose so a shield can never be mistaken for a debuff on
	 * whoever is swinging.
	 */
	UFUNCTION(BlueprintCallable, Category = "Ruleset")
	virtual FDCCAttackResult ResolveAttack(
		const FDCCAbilityScoreSet& AttackerScores,
		EDCCAbility AttackAbility,
		int32 WeaponDamageDie,
		const FDCCAbilityScoreSet& DefenderScores,
		int32 DefenderLevel,
		int32 SituationalBonus = 0,
		int32 DefenderEvadeBonus = 0) const;

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

	// -------------------------------------------------------------------------
	// STAMINA
	//
	// Not a published DCC resource - it is ours, added so that sustained physical
	// effort like climbing has a cost that is not a dice roll. Constitution
	// governs it, which matches how the books already use Con for Health and for
	// the Endurance checks that gate long grinding sessions.
	// -------------------------------------------------------------------------

	/**
	 * Total stamina. Constitution-driven, on the same 2-6 score scale, refined
	 * by Athletics Rank the same way Climb Rank refines climbing - a novice
	 * and a veteran share the same Constitution-set baseline, but conditioning
	 * raises the ceiling.
	 */
	UFUNCTION(BlueprintCallable, Category = "Ruleset|Stamina")
	virtual float GetMaxStamina(const FDCCAbilityScoreSet& AbilityScores, int32 Level, int32 AthleticsRank = 0) const;

	/** Stamina per second spent while hanging on a surface doing nothing. */
	UFUNCTION(BlueprintCallable, Category = "Ruleset|Stamina")
	virtual float GetClingStaminaCost(int32 ClimbSkillRank) const;

	/** Stamina per second spent while actively moving up or down. */
	UFUNCTION(BlueprintCallable, Category = "Ruleset|Stamina")
	virtual float GetClimbStaminaCost(int32 ClimbSkillRank) const;

	/** Vertical climb speed in cm/s, improved by Rank. */
	UFUNCTION(BlueprintCallable, Category = "Ruleset|Stamina")
	virtual float GetClimbSpeed(const FDCCAbilityScoreSet& AbilityScores, int32 ClimbSkillRank) const;

	/**
	 * Stamina per second recovered when not exerting. EquippedMassKg is a flat
	 * subtraction from the final rate rather than a multiplier - encumbrance
	 * should be able to stall recovery outright for someone badly overloaded,
	 * not just slow it, and a multiplier on an already-small number can never
	 * do that.
	 */
	UFUNCTION(BlueprintCallable, Category = "Ruleset|Stamina")
	virtual float GetStaminaRegenRate(const FDCCAbilityScoreSet& AbilityScores, int32 AthleticsRank = 0, float EquippedMassKg = 0.0f) const;

	/** Walk-speed multiplier while sprinting. Dexterity-driven. */
	UFUNCTION(BlueprintCallable, Category = "Ruleset|Stamina")
	virtual float GetSprintSpeedMultiplier(const FDCCAbilityScoreSet& AbilityScores) const;

	/** Stamina per second spent while actually sprinting, cheaper with Athletics Rank. */
	UFUNCTION(BlueprintCallable, Category = "Ruleset|Stamina")
	virtual float GetSprintStaminaCost(const FDCCAbilityScoreSet& AbilityScores, int32 AthleticsRank = 0) const;

	/**
	 * Kilograms of equipped gear before stamina regen starts suffering for it -
	 * "wearing heavy armour" made literal. A Heavy Armor Training Skill (Class-
	 * grantable, same mechanism as everything else in Classes/) raises this
	 * rather than reducing the penalty directly, so the two compose as "you can
	 * carry more before it costs you" rather than a flat discount.
	 */
	UFUNCTION(BlueprintCallable, Category = "Ruleset|Stamina")
	virtual float GetEncumbranceThresholdKg(int32 HeavyArmorTrainingRank) const;

	/**
	 * Server-only. A skill check: the core check plus the character's Rank.
	 *
	 * An untrained attempt rolls at Disadvantage (2d20, keep the lower) and adds
	 * no Rank, which is what makes trying something you are bad at a real risk
	 * rather than a free lottery ticket.
	 *
	 * bAdvantage is separate from bUntrained on purpose: a trained crawler with
	 * an Advantage-granting Race/Class benefit is a completely different case
	 * from an untrained one, and the two need to be able to coexist and cancel -
	 * Advantage and Disadvantage together fall back to a single ordinary roll,
	 * same as they would on paper.
	 */
	UFUNCTION(BlueprintCallable, Category = "Ruleset|Skills")
	virtual FDCCCheckResult ResolveSkillCheck(
		const FDCCAbilityScoreSet& AbilityScores,
		EDCCAbility Ability,
		int32 SkillRank,
		int32 DifficultyClass,
		bool bUntrained = false,
		int32 SituationalBonus = 0,
		bool bAdvantage = false) const;

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

	// -------------------------------------------------------------------------
	// SEGMENTED HEALTH
	//
	// DCC health is not a bar, it is ten blocks. Damage inside a block comes back
	// on its own after a moment; damage that breaks a block is real and stays
	// broken until something heals it.
	//
	// That single rule changes how a fight reads. Chip damage stops mattering, so
	// the party stops flinching at scratches - but every block lost is a visible,
	// permanent step towards the floor, and everyone at the table can count them
	// from across the room. It is the whole reason to show health as blocks rather
	// than as a percentage.
	// -------------------------------------------------------------------------

	/** Always ten in the published rules, whatever the hit point total is. */
	UFUNCTION(BlueprintCallable, Category = "Ruleset|Health")
	virtual int32 GetHealthSegmentCount() const;

	/**
	 * Hit points per block.
	 *
	 * A float because hit point totals are not obliged to divide by ten. A 30 HP
	 * crawler has blocks of 3; a 45 HP one has blocks of 4.5, and the bar still
	 * shows exactly ten of them.
	 */
	UFUNCTION(BlueprintCallable, Category = "Ruleset|Health")
	virtual float GetHealthSegmentSize(int32 MaxHitPoints) const;

	/**
	 * The hit point total natural regeneration climbs back to, and stops at.
	 *
	 * The top of the block currently being stood in. At 22 of 30 - seven whole
	 * blocks and a third of the eighth - this returns 24, so the eighth block
	 * refills and the fight resumes with eight blocks rather than seven. Already
	 * sitting exactly on a boundary returns that boundary, so nothing regenerates.
	 */
	UFUNCTION(BlueprintCallable, Category = "Ruleset|Health")
	virtual int32 GetSegmentRegenCeiling(int32 CurrentHitPoints, int32 MaxHitPoints) const;

	/**
	 * Regeneration speed, in hit points per second.
	 *
	 * Defined in *blocks* per second and converted, so refilling a block takes the
	 * same time for everyone. A tank with 200 hit points and a crawler with 30
	 * watch the same animation take the same length of time, which is what keeps
	 * the bar readable as a shared language at the table.
	 */
	UFUNCTION(BlueprintCallable, Category = "Ruleset|Health")
	virtual float GetHealthRegenRate(int32 MaxHitPoints) const;

	/** Quiet seconds required after taking damage before regeneration starts. */
	UFUNCTION(BlueprintCallable, Category = "Ruleset|Health")
	virtual float GetRegenDelaySeconds() const;

	/**
	 * Mana per second recovered out of combat.
	 *
	 * Mana previously never came back at all, which meant every caster in the game
	 * was permanently dry two spells into the first fight.
	 */
	UFUNCTION(BlueprintCallable, Category = "Ruleset|Health")
	virtual float GetManaRegenRate(int32 MaxManaPoints) const;

	// -------------------------------------------------------------------------
	// EXPERIENCE AND LEVELS
	//
	// A DIVERGENCE FROM THE PUBLISHED RULES, ON PURPOSE.
	//
	// The books are explicit that DCC has no experience points: levels come from
	// playtime (one per two hours), quests, and boss tier. That is a rule built
	// for a table where the GM is watching the clock, and it gives a video game
	// nothing to show a player who just won a fight.
	//
	// So XP exists here as the *mechanism* and the published sources are simply
	// the largest grants. Anyone wanting the book behaviour back overrides
	// GetExperienceForKill to return zero and drives levels from playtime alone;
	// nothing else has to change.
	// -------------------------------------------------------------------------

	/** Total XP needed to advance from the given level to the next one. */
	UFUNCTION(BlueprintCallable, Category = "Ruleset|Experience")
	virtual int32 GetExperienceToNextLevel(int32 CurrentLevel) const;

	/**
	 * XP for putting a character down.
	 *
	 * Scaled by the level gap so that farming things far below you stops paying,
	 * which is the only thing keeping a kill-based curve from rewarding the most
	 * boring possible play.
	 */
	UFUNCTION(BlueprintCallable, Category = "Ruleset|Experience")
	virtual int32 GetExperienceForKill(int32 VictimLevel, int32 KillerLevel) const;

	/** XP for every survivor when an encounter is cleared. */
	UFUNCTION(BlueprintCallable, Category = "Ruleset|Experience")
	virtual int32 GetExperienceForEncounterClear(int32 EnemyCount, int32 AverageEnemyLevel) const;

	/**
	 * XP for clearing every encounter on a floor.
	 *
	 * Deliberately much larger than the sum of its parts. The books make the
	 * floor the unit of progress, and a bonus that dwarfs the individual kills
	 * is what keeps "finish the floor" more attractive than "grind the room".
	 */
	UFUNCTION(BlueprintCallable, Category = "Ruleset|Experience")
	virtual int32 GetExperienceForFloorClear(int32 FloorNumber, int32 EncounterCount) const;

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
