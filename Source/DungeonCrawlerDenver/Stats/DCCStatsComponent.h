#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "Stats/DCCAbilityTypes.h"
#include "Stats/DCCDamageTypes.h"
#include "DCCStatsComponent.generated.h"

class UDCCRuleset;
class UDCCStatsComponent;

DECLARE_DYNAMIC_MULTICAST_DELEGATE_ThreeParams(FDCCHealthChanged, UDCCStatsComponent*, Stats, int32, NewHitPoints, int32, Delta);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_ThreeParams(FDCCManaChanged, UDCCStatsComponent*, Stats, int32, NewManaPoints, int32, Delta);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FDCCDied, UDCCStatsComponent*, Stats);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_TwoParams(FDCCExperienceChanged, UDCCStatsComponent*, Stats, int32, Gained);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_TwoParams(FDCCLevelUp, UDCCStatsComponent*, Stats, int32, NewLevel);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_TwoParams(FDCCGoldChanged, UDCCStatsComponent*, Stats, int32, Delta);

/**
 * Holds one character's ability scores and hit points, and is the single place
 * gameplay code asks "what happens?".
 *
 * Attach to players and enemies alike - an encounter only needs both sides to
 * speak the same language, and keeping enemies on the same component means the
 * DM client can inspect and override them the same way it does players.
 *
 * Server-authoritative: damage, healing and checks are refused on clients. All
 * state reaches clients by replication, so nothing here trusts a client's word
 * about its own health.
 */
UCLASS(ClassGroup = (DCC), meta = (BlueprintSpawnableComponent))
class DUNGEONCRAWLERDENVER_API UDCCStatsComponent : public UActorComponent
{
	GENERATED_BODY()

public:
	UDCCStatsComponent();

	virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;
	virtual void InitializeComponent() override;

	/** Which ruleset this character resolves against. Swap this to swap the rules. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Stats")
	TSubclassOf<UDCCRuleset> RulesetClass;

	/**
	 * The UNENHANCED layer: base scores, plus anything permanent - potions and
	 * level-ups. This is the authored value and the one on the character sheet.
	 *
	 * The books split stats into Unenhanced and Enhanced, and the distinction is
	 * load-bearing rather than cosmetic: losing a sword must not un-drink a
	 * potion. Keeping the authored field as the base means gear can be added and
	 * removed non-destructively, because the number it modifies is never the
	 * number it wrote to.
	 */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, ReplicatedUsing = OnRep_AbilityScores, Category = "Stats")
	FDCCAbilityScoreSet AbilityScores;

	/**
	 * The ENHANCED layer: AbilityScores plus equipment, and later spells and
	 * buffs. Derived - never author this.
	 *
	 * Replicated rather than recomputed per machine because a client would
	 * otherwise need the whole equipment table and every buff's rules to arrive
	 * at the same answer, and any disagreement shows up as a wrong number on a
	 * character sheet nobody can explain.
	 */
	UPROPERTY(BlueprintReadOnly, Replicated, Category = "Stats")
	FDCCAbilityScoreSet EnhancedScores;

	/** One EDCCResistanceLevel per EDCCDamageType, indexed by the enum value -
	 *  same array-indexed-by-enum shape as FDCCAbilityScoreSet::Scores.
	 *  Rebuilt by RecalculateDamageResistances; read via GetResistanceLevel. */
	UPROPERTY(BlueprintReadOnly, Replicated, Category = "Stats")
	TArray<uint8> DamageResistanceLevels;

	/**
	 * What the rules should read. Every check, attack, carry limit and throw
	 * resolves against Enhanced; only advancement writes to Unenhanced.
	 */
	UFUNCTION(BlueprintPure, Category = "Stats")
	const FDCCAbilityScoreSet& GetEnhanced() const { return EnhancedScores; }

	/**
	 * Server only. Rebuilds EnhancedScores from AbilityScores plus equipment.
	 *
	 * Called whenever either side changes. Cheap enough to do eagerly, and doing
	 * it eagerly means no call site has to remember to ask for a refresh.
	 *
	 * bRefillToFull defaults false - swapping gear mid-session must not
	 * rubber-band a damaged character back to full HP/Mana just because a
	 * ring changed hands. The one exception is the very first call after
	 * spawn: InitializeComponent seeds EnhancedScores from the base layer
	 * alone and refills to full against THAT (equipment may not exist on the
	 * actor yet), so a character with stat-boosting starting gear would
	 * otherwise spawn permanently pinned below its true, gear-inclusive max -
	 * clamped down to the old max next time this runs, never refilled up to
	 * the new one. UDCCEquipmentComponent::BeginPlay passes true for exactly
	 * this reason; every other caller should leave it false.
	 */
	void RecalculateEnhancedScores(bool bRefillToFull = false);

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Stats", meta = (ClampMin = "1"))
	int32 Level = 1;

	UPROPERTY(BlueprintReadOnly, ReplicatedUsing = OnRep_CurrentHitPoints, Category = "Stats")
	int32 CurrentHitPoints = 0;

	UPROPERTY(BlueprintReadOnly, Replicated, Category = "Stats")
	int32 MaxHitPoints = 0;

	UPROPERTY(BlueprintReadOnly, ReplicatedUsing = OnRep_CurrentManaPoints, Category = "Stats")
	int32 CurrentManaPoints = 0;

	UPROPERTY(BlueprintReadOnly, Replicated, Category = "Stats")
	int32 MaxManaPoints = 0;

	/**
	 * Sustained-effort resource, drained by climbing.
	 *
	 * A float rather than an integer because it is spent continuously per second
	 * rather than in discrete costs like Mana - rounding a continuous drain to
	 * whole points would make it visibly step.
	 *
	 * Not replicated with a RepNotify: it changes every frame while climbing, so
	 * it is a plain replicated value the HUD polls, rather than a delegate firing
	 * sixty times a second.
	 */
	UPROPERTY(BlueprintReadOnly, Replicated, Category = "Stats")
	float CurrentStamina = 0.0f;

	UPROPERTY(BlueprintReadOnly, Replicated, Category = "Stats")
	float MaxStamina = 0.0f;

	UFUNCTION(BlueprintPure, Category = "Stats")
	float GetStaminaFraction() const { return MaxStamina > 0.0f ? CurrentStamina / MaxStamina : 0.0f; }

	/**
	 * Clamped at both ends on purpose.
	 *
	 * Damage overkill drives CurrentHitPoints negative, and an unclamped fraction
	 * would hand the HUD a negative width - which draws as nothing, or worse, as
	 * a bar that never visibly empties. Dead reads as exactly zero here.
	 */
	UFUNCTION(BlueprintPure, Category = "Stats")
	float GetHealthFraction() const
	{
		return MaxHitPoints > 0 ? FMath::Clamp(static_cast<float>(CurrentHitPoints) / MaxHitPoints, 0.0f, 1.0f) : 0.0f;
	}

	UFUNCTION(BlueprintPure, Category = "Stats")
	float GetManaFraction() const
	{
		return MaxManaPoints > 0 ? FMath::Clamp(static_cast<float>(CurrentManaPoints) / MaxManaPoints, 0.0f, 1.0f) : 0.0f;
	}

	// -------------------------------------------------------------------------
	// SEGMENTED HEALTH
	// -------------------------------------------------------------------------

	/**
	 * Server only. Regenerates health toward the top of the current block, and
	 * mana toward full.
	 *
	 * Called from the owning character's Tick. Both accumulate fractionally and
	 * only commit whole points, because hit points are integers and a rate below
	 * one per second would otherwise round to nothing forever.
	 */
	UFUNCTION(BlueprintCallable, Category = "Stats")
	void TickRegeneration(float DeltaSeconds);

	/** Whole blocks currently filled. */
	UFUNCTION(BlueprintPure, Category = "Stats")
	int32 GetFullHealthSegments() const;

	/** How full the partially-drained block is, 0-1. Zero when sitting on a boundary. */
	UFUNCTION(BlueprintPure, Category = "Stats")
	float GetPartialHealthSegmentFraction() const;

	/** What natural regeneration will climb back to. Drives the HUD's ghost fill. */
	UFUNCTION(BlueprintPure, Category = "Stats")
	int32 GetRegenCeiling() const;

	/** True while the post-damage delay is still running. */
	UFUNCTION(BlueprintPure, Category = "Stats")
	bool IsRegenSuppressed() const;

	/**
	 * Whether the ruleset's player toughness bonus applies to this character.
	 *
	 * Set from ADCCCharacterBase::PossessedBy rather than authored, so a
	 * Blueprint cannot forget it and an NPC ally cannot accidentally claim it by
	 * being placed with the wrong flag ticked.
	 */
	UPROPERTY(BlueprintReadOnly, Replicated, Category = "Stats")
	bool bIsPlayerCharacter = false;

	/**
	 * Friendly NPC. Gets a survivability cushion like a player, but smaller.
	 *
	 * Without this an ally is statistically a mob wearing a different nameplate,
	 * and dies at exactly the same rate as the things it is fighting - which is
	 * what was observed. The party's own NPCs going down as fast as the enemy
	 * makes recruiting anyone feel pointless.
	 */
	UPROPERTY(BlueprintReadOnly, Replicated, Category = "Stats")
	bool bIsAllyCharacter = false;

	UFUNCTION(BlueprintCallable, Category = "Stats")
	void SetIsAllyCharacter(bool bNewIsAlly);

	/**
	 * Ignores all incoming damage. A DM tool, not a game mechanic.
	 *
	 * Lives on the stats component rather than the character because ApplyDamage
	 * is the single funnel every damage source already goes through - anywhere
	 * else and something would eventually route around it.
	 */
	UPROPERTY(BlueprintReadOnly, Replicated, Category = "Stats")
	bool bInvulnerable = false;

	UFUNCTION(BlueprintCallable, Category = "Stats")
	void SetInvulnerable(bool bNewInvulnerable);

	/** Server only. Marks this as a player's character and re-derives hit points. */
	UFUNCTION(BlueprintCallable, Category = "Stats")
	void SetIsPlayerCharacter(bool bNewIsPlayerCharacter);

	// -------------------------------------------------------------------------
	// EXPERIENCE
	// -------------------------------------------------------------------------

	UPROPERTY(BlueprintReadOnly, ReplicatedUsing = OnRep_ExperiencePoints, Category = "Stats")
	int32 ExperiencePoints = 0;

	/** XP needed to reach the next level. Derived, replicated so the HUD can draw a bar. */
	UPROPERTY(BlueprintReadOnly, Replicated, Category = "Stats")
	int32 ExperienceToNextLevel = 0;

	UFUNCTION(BlueprintPure, Category = "Stats")
	float GetExperienceFraction() const
	{
		return ExperienceToNextLevel > 0
			? FMath::Clamp(static_cast<float>(ExperiencePoints) / ExperienceToNextLevel, 0.0f, 1.0f)
			: 0.0f;
	}

	/**
	 * Server only. Adds XP and levels up as many times as the total allows.
	 *
	 * Loops rather than levelling once because a floor-clear bonus can legitimately
	 * be worth more than a single level, and swallowing the remainder would quietly
	 * lose progress the party earned.
	 */
	UFUNCTION(BlueprintCallable, Category = "Stats")
	void AddExperience(int32 Amount);

	// -------------------------------------------------------------------------
	// GOLD
	//
	// A real resource rather than a score, because Floor 1's boss spends the whole
	// fight taking it: Repossession Nip steals gold and feeds it to Voldemortgage
	// as healing, and his death lien is repaid in gold. A number that only goes up
	// could not carry either of those.
	// -------------------------------------------------------------------------

	UPROPERTY(BlueprintReadOnly, ReplicatedUsing = OnRep_Gold, Category = "Stats")
	int32 Gold = 0;

	UPROPERTY(BlueprintAssignable, Category = "Stats")
	FDCCGoldChanged OnGoldChanged;

	/** Server only. Negative amounts are ignored; use SpendGold or StealGold. */
	UFUNCTION(BlueprintCallable, Category = "Stats")
	void AddGold(int32 Amount);

	/**
	 * Server only. Returns false and spends nothing if unaffordable, so callers
	 * can use it directly as the gate on a purchase.
	 */
	UFUNCTION(BlueprintCallable, Category = "Stats")
	bool SpendGold(int32 Amount);

	/**
	 * Server only. Takes up to Amount and reports how much was actually taken.
	 *
	 * Partial rather than all-or-nothing on purpose: a Debt Eater biting a crawler
	 * who has three gold left should get three, not fail. Returning the real figure
	 * is what lets the thief convert it - Voldemortgage heals for exactly what was
	 * taken, so an almost-broke party starves him.
	 */
	UFUNCTION(BlueprintCallable, Category = "Stats")
	int32 StealGold(int32 Amount);

	UPROPERTY(BlueprintAssignable, Category = "Stats")
	FDCCExperienceChanged OnExperienceChanged;

	UPROPERTY(BlueprintAssignable, Category = "Stats")
	FDCCLevelUp OnLevelUp;

	/**
	 * Server only. Spends stamina and reports whether any was left.
	 *
	 * Returns false once empty so callers can use it directly as the condition
	 * that ends a sustained action.
	 */
	UFUNCTION(BlueprintCallable, Category = "Stats")
	bool ConsumeStamina(float Amount);

	/** Server only. Recovers stamina, clamped to the maximum. */
	UFUNCTION(BlueprintCallable, Category = "Stats")
	void RecoverStamina(float Amount);

	/** Fires on server and on every client that receives the change. */
	UPROPERTY(BlueprintAssignable, Category = "Stats")
	FDCCHealthChanged OnHealthChanged;

	UPROPERTY(BlueprintAssignable, Category = "Stats")
	FDCCManaChanged OnManaChanged;

	UPROPERTY(BlueprintAssignable, Category = "Stats")
	FDCCDied OnDied;

	UFUNCTION(BlueprintPure, Category = "Stats")
	bool IsAlive() const { return CurrentHitPoints > 0; }

	UFUNCTION(BlueprintPure, Category = "Stats")
	int32 GetAbilityModifier(EDCCAbility Ability) const;

	UFUNCTION(BlueprintPure, Category = "Stats")
	UDCCRuleset* GetRuleset() const;

	/**
	 * Server only. Negative amounts are ignored; use Heal instead.
	 *
	 * DamageType defaults to Physical - the untyped bucket everything that
	 * predates this system already deals, and nothing is ever resistant to it
	 * by design, so every existing call site keeps working unchanged.
	 * Resistant/Vulnerable/Immune are applied here, before the amount is
	 * clamped and subtracted, so an Immune hit already reads as "no damage"
	 * to every check below rather than needing a second early-out.
	 */
	UFUNCTION(BlueprintCallable, Category = "Stats")
	void ApplyDamage(int32 Amount, EDCCDamageType DamageType = EDCCDamageType::Physical);

	/** This character's current reaction to DamageType, merged from Equipment
	 *  and Class - see RecalculateDamageResistances. */
	UFUNCTION(BlueprintPure, Category = "Stats")
	EDCCResistanceLevel GetResistanceLevel(EDCCDamageType DamageType) const;

	/**
	 * Server only. Rebuilds DamageResistanceLevels from scratch from Equipment
	 * and Class, the same "recompute, never adjust in place" shape
	 * RecalculateEnhancedScores already uses. Called from there, so anything
	 * that already triggers a stat recalc (equipping gear, selecting a Class)
	 * keeps resistances in sync for free.
	 */
	void RecalculateDamageResistances();

	UFUNCTION(BlueprintCallable, Category = "Stats")
	void Heal(int32 Amount);

	/**
	 * Server only. Adds a temporary, signed nudge to one ability score, folded
	 * into EnhancedScores alongside Equipment and Class until it expires - see
	 * FDCCTimedAbilityModifier. Ruleset-agnostic on purpose: this layer has no
	 * idea a drink is what's calling it (see ADCCCharacterBase's own
	 * consumable-effect translation, which is where that concept lives).
	 */
	UFUNCTION(BlueprintCallable, Category = "Stats")
	void ApplyTimedAbilityModifier(EDCCAbility Ability, int32 Amount, float DurationSeconds);

	/**
	 * Server only. Returns false and spends nothing if the cost is not
	 * affordable, so callers can use it directly as the gate on casting.
	 */
	UFUNCTION(BlueprintCallable, Category = "Stats")
	bool SpendMana(int32 Amount);

	UFUNCTION(BlueprintCallable, Category = "Stats")
	void RestoreMana(int32 Amount);

	UFUNCTION(BlueprintPure, Category = "Stats")
	bool CanAffordMana(int32 Amount) const { return Amount <= CurrentManaPoints; }

	/** Server only. Returns a zeroed result if called on a client. */
	UFUNCTION(BlueprintCallable, Category = "Stats")
	FDCCCheckResult ResolveCheck(EDCCAbility Ability, int32 DifficultyClass, int32 SituationalBonus = 0);

	/**
	 * Server only. Re-derives max hit points after Level or AbilityScores were
	 * changed at runtime.
	 *
	 * Needed because components initialise during actor spawn, before a spawner
	 * has had any chance to stamp per-instance values onto them - an encounter
	 * setting a per-spawn level is exactly that case.
	 */
	UFUNCTION(BlueprintCallable, Category = "Stats")
	void RefreshDerivedStats(bool bRefillToFull = true);

protected:
	UFUNCTION()
	void OnRep_CurrentHitPoints(int32 PreviousHitPoints);

	UFUNCTION()
	void OnRep_CurrentManaPoints(int32 PreviousManaPoints);

	UFUNCTION()
	void OnRep_AbilityScores();

	UFUNCTION()
	void OnRep_ExperiencePoints(int32 PreviousExperience);

	UFUNCTION()
	void OnRep_Gold(int32 PreviousGold);

private:
	/** Recalculates MaxHitPoints from the ruleset, clamping current HP to fit. */
	void RecalculateDerivedStats(bool bRefillToFull);

	/**
	 * Removes any TimedAbilityModifiers whose ExpiryWorldTime has passed and
	 * recalculates if that changed anything. Called from TickRegeneration,
	 * which every character already ticks - a modifier that outlives its
	 * duration by up to one tick is not worth a dedicated timer over.
	 */
	void PruneExpiredTimedAbilityModifiers();

	/** Not replicated - EnhancedScores (the result) already is, same as Equipment's contribution. */
	UPROPERTY(Transient)
	TArray<FDCCTimedAbilityModifier> TimedAbilityModifiers;

	UPROPERTY(Transient)
	TObjectPtr<UDCCRuleset> CachedRuleset;

	/** Seconds since the last damage. Gates regeneration. */
	float TimeSinceDamage = 0.0f;

	/** Sub-point regeneration carried between ticks. */
	float HealthRegenCarry = 0.0f;
	float ManaRegenCarry = 0.0f;
};
