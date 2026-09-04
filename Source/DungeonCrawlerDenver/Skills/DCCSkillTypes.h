#pragma once

#include "CoreMinimal.h"
#include "Engine/DataTable.h"
#include "Stats/DCCAbilityTypes.h"
#include "DCCSkillTypes.generated.h"

class UNiagaraSystem;
class UTexture2D;

// -----------------------------------------------------------------------------
// SKILL ARCHETYPE SYSTEM
//
// The design doc's compromise for DCC's freeform, player-invented skills: rather
// than bespoke code per idea, most skills are one of a small set of parameterised
// templates, reskinned with the player's own flavour, icon and animation. The
// target is ~90% of player requests covered by an archetype, with the remaining
// ~10% deliberately hand-built as signature moments.
//
// So the interesting content of a skill is DATA, not code. FDCCSkillDef is that
// data, authorable in a DataTable so Zoe can add a skill mid-session without a
// recompile - which is the entire point, since the tabletop game invents skills
// at the table.
// -----------------------------------------------------------------------------

UENUM(BlueprintType)
enum class EDCCSkillArchetype : uint8
{
	/** Deals damage. "I want to headbutt for bonus damage." */
	Damage             UMETA(DisplayName = "Damage"),

	/** Restores hit points. */
	Heal               UMETA(DisplayName = "Heal"),

	/** Applies a negative effect. "Throw sand in their eyes to blind them." */
	Debuff             UMETA(DisplayName = "Debuff"),

	/** Applies a positive effect. "Hype up the party before a fight." */
	Buff               UMETA(DisplayName = "Buff"),

	/** Repositioning. "Vault over this obstacle." */
	UtilityMovement    UMETA(DisplayName = "Utility - Movement"),

	/** Acts on the world. "Kick over this shelf to block a doorway." */
	UtilityEnvironment UMETA(DisplayName = "Utility - Environment"),

	/** Brings friends. "Call in a swarm of rats." */
	Summon             UMETA(DisplayName = "Summon"),

	/**
	 * A non-combat Skill Check with no activation of its own - Perception,
	 * Navigation, Intimidate, any Lore. Never routes through TryActivateSkill;
	 * the caller (an interaction, a climb attempt, a future trap/lock) reads
	 * GetSkillRank and calls UDCCRuleset::ResolveSkillCheck directly, then
	 * calls MarkSkillUsed itself. Every field below Archetype/CastingAbility on
	 * a Check row is unused - kept present rather than split into a second
	 * struct so this stays one shared table.
	 */
	Check              UMETA(DisplayName = "Check"),

	MAX                UMETA(Hidden)
};

UENUM(BlueprintType)
enum class EDCCSkillTargeting : uint8
{
	Self         UMETA(DisplayName = "Self"),
	SingleTarget UMETA(DisplayName = "Single Target"),
	Cone         UMETA(DisplayName = "Cone"),
	AreaOfEffect UMETA(DisplayName = "Area of Effect"),

	MAX          UMETA(Hidden)
};

/** Why an activation was refused. Surfaced to the player so failures explain themselves. */
UENUM(BlueprintType)
enum class EDCCSkillFailure : uint8
{
	None            UMETA(DisplayName = "None"),
	UnknownSkill    UMETA(DisplayName = "Unknown Skill"),
	Dead            UMETA(DisplayName = "Caster Is Down"),
	OnCooldown      UMETA(DisplayName = "On Cooldown"),
	NotEnoughMana   UMETA(DisplayName = "Not Enough Mana"),
	NoTarget        UMETA(DisplayName = "No Valid Target"),
	OutOfRange      UMETA(DisplayName = "Out Of Range"),
	NotAuthoritative UMETA(DisplayName = "Refused - Client"),

	/** Spell Skills cannot be attempted without Ranks in them, unlike Attack and Utility Skills. */
	RequiresTraining UMETA(DisplayName = "Requires Training"),

	MAX             UMETA(Hidden)
};

/**
 * One authorable skill or spell.
 *
 * Mechanical fields are deliberately generic so a single archetype covers many
 * reskins: "Headbutt" and "Shoulder Charge" are the same Damage row with
 * different names, dice and flavour.
 */
USTRUCT(BlueprintType)
struct FDCCSkillDef : public FTableRowBase
{
	GENERATED_BODY()

	/** Stable lookup key. Must be unique; this is what activation calls refer to. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Skill")
	FName SkillId;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Skill")
	FText DisplayName;

	/** The player's own words for it. Pure flavour - never read by resolution. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Skill", meta = (MultiLine = "true"))
	FText FlavorText;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Skill")
	EDCCSkillArchetype Archetype = EDCCSkillArchetype::Damage;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Skill")
	EDCCSkillTargeting Targeting = EDCCSkillTargeting::SingleTarget;

	/** Which stat drives this skill's check. DCC spells key off Intelligence. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Skill")
	EDCCAbility CastingAbility = EDCCAbility::Intelligence;

	/**
	 * Mana spent on a successful activation.
	 *
	 * Real DCC spends Mana in double digits against a pool equal to the caster's
	 * Intelligence score, so costs here are whole-pool decisions, not small change.
	 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Skill|Cost", meta = (ClampMin = "0"))
	int32 ManaCost = 0;

	/** Doubles as this skill's initiative weight for the pause/queue loop. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Skill|Cost", meta = (ClampMin = "0.0"))
	float CooldownSeconds = 6.0f;

	/**
	 * Seconds the caster is rooted after this resolves. Zero is instant, which is
	 * why it defaults to zero - every skill authored before this field existed
	 * keeps behaving exactly as it did.
	 *
	 * This is the *positional* price of a skill. Mana and cooldown both spend
	 * resources the caster cannot see on screen; standing still in a room that is
	 * actively trying to kill you is a cost the party can watch happen, which is
	 * what makes choosing to pay it a decision rather than arithmetic.
	 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Skill|Cost", meta = (ClampMin = "0.0"))
	float CastTimeSeconds = 0.0f;

	/** Max distance to a target at Rank 1. Ignored for Self. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Skill|Targeting", meta = (ClampMin = "0.0"))
	float Range = 600.0f;

	/**
	 * Extra reach per Rank above the first, in world units.
	 *
	 * Zero for almost everything - a fireball does not fly further because you
	 * have thrown more of them. It exists for the skills where reach IS the
	 * thing being practised, TargetedThrow being the case it was added for:
	 * a novice lobs a rock a short way, a veteran puts it across the room.
	 *
	 * Deliberately a flat per-Rank addition rather than a multiplier. Rank
	 * runs 0-20 and multiplying would make high Ranks absurd, whereas adding
	 * keeps the curve legible to whoever is authoring the row.
	 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Skill|Targeting", meta = (ClampMin = "0.0"))
	float RangePerRank = 0.0f;

	/** Cone/AoE size. Ignored for Self and SingleTarget. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Skill|Targeting", meta = (ClampMin = "0.0"))
	float Radius = 300.0f;

	/** Half-angle of the cone, in degrees. Only read when Targeting is Cone. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Skill|Targeting", meta = (ClampMin = "0.0", ClampMax = "180.0"))
	float ConeHalfAngleDegrees = 45.0f;

	/**
	 * Difficulty of the skill check. Set to 0 for skills that always land and
	 * only need their cost paid.
	 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Skill|Resolution", meta = (ClampMin = "0"))
	int32 DifficultyClass = 0;

	/** Die rolled for damage or healing. The caster's ability modifier is added. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Skill|Resolution", meta = (ClampMin = "0"))
	int32 PowerDie = 6;

	/** How long a Buff/Debuff lasts. Meaningless for instantaneous archetypes. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Skill|Resolution", meta = (ClampMin = "0.0"))
	float EffectDuration = 0.0f;

	/**
	 * Free-form tag the Blueprint layer switches on for archetypes this component
	 * does not resolve itself (Debuff, Buff, Utility, Summon). Keeps genuinely
	 * bespoke behaviour out of C++ and in the DM's hands.
	 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Skill|Resolution")
	FName EffectTag;

	/**
	 * Whether this skill may be attempted by someone who has no Ranks in it.
	 *
	 * The published rules let Attack and Utility Skills be tried untrained (at
	 * Disadvantage) but forbid it for Spell Skills, so this defaults to true and
	 * is switched off on spells. Kept as an authored flag rather than inferred
	 * from mana cost, because a free cantrip is still a spell.
	 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Skill|Advancement")
	bool bCanAttemptUntrained = true;

	/**
	 * Passive Skills resolve without a check and therefore never earn advancement
	 * marks; their Ranks come from gear, race or class instead.
	 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Skill|Advancement")
	bool bIsPassive = false;

	// -------------------------------------------------------------------------
	// PRESENTATION
	//
	// Authored per skill rather than hardcoded per archetype, because the whole
	// premise is that "Fire Fingers" and "Soul Collector" are the same Damage row
	// wearing different clothes. The clothes therefore have to be data too.
	//
	// Soft references so that a DataTable of fifty skills does not drag fifty
	// Niagara systems into memory the moment anything touches the table.
	// -------------------------------------------------------------------------

	/** Played on the caster when the skill goes off. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Skill|FX")
	TSoftObjectPtr<UNiagaraSystem> CastEffect;

	/** Played on each affected target. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Skill|FX")
	TSoftObjectPtr<UNiagaraSystem> ImpactEffect;

	/**
	 * Socket on the caster's mesh to spawn CastEffect at.
	 *
	 * Falls back to the actor origin when the socket is missing, so a skill still
	 * shows something on a skeleton that has no hand sockets - which is every
	 * Meshy-imported character until sockets are added by hand.
	 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Skill|FX")
	FName CastSocketName;

	/** Height above the target's origin to play ImpactEffect at. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Skill|FX")
	float ImpactEffectZOffset = 60.0f;

	/**
	 * Hotbar icon. Optional - a row with none set still draws exactly as every
	 * skill did before this field existed (the slot number and a text label,
	 * see DCCHUD::DrawHotbar), so nothing regresses for the ~50 skills already
	 * authored without one.
	 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Skill|FX")
	TSoftObjectPtr<UTexture2D> Icon;
};

/**
 * One skill as a specific character knows it: which skill, how good they are at
 * it, and whether they have used it since the last advancement roll.
 *
 * Deliberately separate from FDCCSkillDef. The definition is shared library data
 * that lives in a DataTable and is identical for everyone; Rank and the
 * advancement mark are per-character state. Storing whole definitions per
 * character would mean retuning a skill required editing every character who
 * knew it, and would replicate a large struct to say a small thing.
 */
USTRUCT(BlueprintType)
struct FDCCKnownSkill
{
	GENERATED_BODY()

	/** Row name in the owning component's SkillTable. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Skill")
	FName SkillId;

	/**
	 * Adds directly to this skill's checks.
	 *
	 * Capped at 15 on the tutorial floors by the ruleset rather than here, so the
	 * cap can be raised later without touching saved character data.
	 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Skill", meta = (ClampMin = "0"))
	int32 Rank = 1;

	/**
	 * Set by any attempt, successful or not, and cleared when advancement is
	 * rolled.
	 *
	 * A flag rather than a counter on purpose: the published rule marks a skill
	 * the first time it is used and ignores every use after that, which stops
	 * spamming a skill from being the fastest way to level it.
	 */
	UPROPERTY(BlueprintReadOnly, Category = "Skill")
	bool bAdvancementMark = false;
};

/**
 * Everything that happened when a skill was activated.
 *
 * Returned whole rather than as a bool so the phone HUD and DM client can narrate
 * the roll, and so the AI layer has real numbers to describe instead of inventing
 * them - the design doc is explicit that the AI narrates outcomes and never
 * decides them.
 */
USTRUCT(BlueprintType)
struct FDCCSkillResult
{
	GENERATED_BODY()

	UPROPERTY(BlueprintReadOnly, Category = "Skill")
	FName SkillId;

	UPROPERTY(BlueprintReadOnly, Category = "Skill")
	bool bActivated = false;

	UPROPERTY(BlueprintReadOnly, Category = "Skill")
	EDCCSkillFailure Failure = EDCCSkillFailure::None;

	/** The skill check, when the skill had a DifficultyClass to beat. */
	UPROPERTY(BlueprintReadOnly, Category = "Skill")
	FDCCCheckResult Check;

	/** Mana actually spent. */
	UPROPERTY(BlueprintReadOnly, Category = "Skill")
	int32 ManaSpent = 0;

	/** Total hit points dealt (Damage) or restored (Heal) across all targets. */
	UPROPERTY(BlueprintReadOnly, Category = "Skill")
	int32 TotalMagnitude = 0;

	/** The caster's Rank in this skill at the moment it was used. 0 when untrained. */
	UPROPERTY(BlueprintReadOnly, Category = "Skill")
	int32 SkillRank = 0;

	/** True when this was an untrained attempt, which rolls at Disadvantage. */
	UPROPERTY(BlueprintReadOnly, Category = "Skill")
	bool bWasUntrained = false;

	UPROPERTY(BlueprintReadOnly, Category = "Skill")
	TArray<TObjectPtr<AActor>> AffectedActors;
};
