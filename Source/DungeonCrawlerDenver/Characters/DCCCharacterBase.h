#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Character.h"
#include "Stats/DCCAbilityTypes.h"
#include "DCCCharacterBase.generated.h"

class UDCCStatsComponent;
class UDCCSkillComponent;
class UDCCFavorComponent;
class UDCCCarryComponent;
class UDCCInventoryComponent;
class ADCCCharacterBase;
class UUserWidget;

DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FDCCCharacterDied, ADCCCharacterBase*, Character);

/**
 * Common ground for anything that fights: players and enemies both derive from
 * this, so an encounter never has to care which side of the fight it is looking
 * at. That symmetry is deliberate - the DM client inspects and overrides both
 * through the same component.
 *
 * Server-authoritative. PerformAttack refuses to run on a client; clients see
 * the outcome through replicated hit points and the cosmetic multicast.
 */
UCLASS(Abstract)
class DUNGEONCRAWLERDENVER_API ADCCCharacterBase : public ACharacter
{
	GENERATED_BODY()

public:
	ADCCCharacterBase();

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "DCC")
	TObjectPtr<UDCCStatsComponent> Stats;

	/**
	 * Known skills and spells. On enemies too, so a boss casts through exactly
	 * the same code path a player does - and so the DM client can inspect both
	 * the same way.
	 */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "DCC")
	TObjectPtr<UDCCSkillComponent> Skills;

	/**
	 * Standing with the audience. On every character, not just players, because
	 * in the fiction the whole dungeon is broadcast - and a boss with a following
	 * is a story beat worth having the data for.
	 */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "DCC")
	TObjectPtr<UDCCFavorComponent> Favor;

	/**
	 * Carrying and throwing props. On enemies too, so a brute hurling a crate at
	 * the party runs through exactly the same rules a player does.
	 */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "DCC")
	TObjectPtr<UDCCCarryComponent> Carry;

	/** Stowed items - the rucksack, as opposed to what is in your hands. */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "DCC")
	TObjectPtr<UDCCInventoryComponent> Inventory;

	/** Which ability drives this character's attacks. Reskinnable per archetype. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "DCC|Combat")
	EDCCAbility AttackAbility = EDCCAbility::Strength;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "DCC|Combat", meta = (ClampMin = "1"))
	int32 WeaponDamageDie = 6;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "DCC|Combat", meta = (ClampMin = "0.0"))
	float AttackRange = 200.0f;

	/**
	 * Doubles as this action's initiative/cooldown value. The design doc calls
	 * for per-action cooldowns specifically so unpausing does not degenerate
	 * into button-mashing; this is where that number lives.
	 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "DCC|Combat", meta = (ClampMin = "0.0"))
	float AttackCooldownSeconds = 1.5f;

	UPROPERTY(BlueprintAssignable, Category = "DCC")
	FDCCCharacterDied OnCharacterDied;

	/**
	 * Per-character HUD, created only for the local player.
	 *
	 * Lives on the character rather than the controller so the DM client can
	 * later spectate a player and get their HUD with them. Leave unset on
	 * enemies - they never build one.
	 */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "DCC|UI")
	TSubclassOf<UUserWidget> HUDWidgetClass;

	UPROPERTY(Transient, BlueprintReadOnly, Category = "DCC|UI")
	TObjectPtr<UUserWidget> HUDWidget;

	/**
	 * Walk speed before the Dexterity multiplier is applied.
	 *
	 * Matches the third-person template's default so an average-DEX crawler moves
	 * exactly as the template always did, and the stat reads as a modifier rather
	 * than as a silent re-tune of the whole game's movement.
	 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "DCC|Movement", meta = (ClampMin = "0.0"))
	float BaseWalkSpeed = 500.0f;

	// -------------------------------------------------------------------------
	// ANIMATION
	//
	// These drive the AnimationSingleNode path, not an AnimBlueprint. The
	// Meshy-imported skeletons have no AnimBP and authoring one per character
	// over MCP is not practical, so one-shot clips are played directly on the
	// mesh and the looping clip is restored afterwards. Leave any of them unset
	// and that action simply keeps the loop playing.
	// -------------------------------------------------------------------------

	/** The looping clip to return to. Usually the run or idle cycle. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "DCC|Animation")
	TObjectPtr<UAnimationAsset> LocomotionAnimation;

	/** Played once per swing. Without this, attacks are invisible. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "DCC|Animation")
	TObjectPtr<UAnimationAsset> AttackAnimation;

	/** Played while ascending a climb point, time-scaled to the ascent. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "DCC|Animation")
	TObjectPtr<UAnimationAsset> ClimbAnimation;

	/** Played once when casting, so a spell reads as deliberate. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "DCC|Animation")
	TObjectPtr<UAnimationAsset> CastAnimation;

	/**
	 * Plays a clip once and returns to LocomotionAnimation afterwards.
	 *
	 * Cosmetic only and safe to call anywhere; it no-ops without a mesh or a
	 * clip. RateScale squeezes the clip into a known duration, which is what lets
	 * a fixed-length climb animation match an authored ascent time.
	 */
	UFUNCTION(BlueprintCallable, Category = "DCC|Animation")
	void PlayOneShotAnimation(UAnimationAsset* Animation, float OverrideDurationSeconds = 0.0f);

	/** Convenience wrapper used by the climb point. */
	UFUNCTION(BlueprintCallable, Category = "DCC|Animation")
	void PlayClimbAnimation(float DurationSeconds);

	UFUNCTION(BlueprintPure, Category = "DCC")
	bool IsAlive() const;

	/**
	 * Re-applies BaseWalkSpeed scaled by the ruleset's Dexterity multiplier.
	 *
	 * Public and callable because ability scores can change after BeginPlay - an
	 * encounter stamping a per-spawn level, a buff, or the DM editing a stat live
	 * from the DM client all need the movement re-derived afterwards.
	 */
	UFUNCTION(BlueprintCallable, Category = "DCC|Movement")
	void ApplyDerivedMovementSpeed();

	/** Pick up the nearest prop in reach, or drop what is already held. */
	UFUNCTION(BlueprintCallable, Category = "DCC|Carry")
	void AttemptPickupOrDrop();

	/** Throw the carried prop, hard as this character's Strength allows. */
	UFUNCTION(BlueprintCallable, Category = "DCC|Carry")
	void AttemptThrow();

	/**
	 * Stow what is being held into the inventory, freeing the hands.
	 *
	 * Routed to the server, since it destroys a world actor and edits replicated
	 * state - a client deciding this locally would desync immediately.
	 */
	UFUNCTION(BlueprintCallable, Category = "DCC|Carry")
	void AttemptStow();

	/** Opens or closes the character menu. Local and cosmetic. */
	UFUNCTION(BlueprintCallable, Category = "DCC|UI")
	void ToggleCharacterMenu();

	/**
	 * Attempt the nearest climb point in reach.
	 *
	 * Routes to the server, which owns the roll. Anyone standing at the wall may
	 * try, which is the point - the table decides who risks it, not the game.
	 */
	UFUNCTION(BlueprintCallable, Category = "DCC|Climb")
	void AttemptClimb();

	UFUNCTION(BlueprintPure, Category = "DCC|Combat")
	bool IsAttackReady() const;

	UFUNCTION(BlueprintPure, Category = "DCC|Combat")
	bool IsInAttackRange(const ADCCCharacterBase* Target) const;

	/**
	 * Server only. Rolls the attack through the ruleset and applies the result.
	 * Returns a zeroed result (bHit false) if refused for any reason.
	 *
	 * bIgnoreCooldown exists for the pause/queue loop. While paused, world time
	 * is dilated almost to a stop, so a queued swing would still be inside its
	 * own cooldown on resume and would silently do nothing - the action would
	 * just evaporate. In the queued path the pause itself is the pacing
	 * mechanism and initiative order is the fairness mechanism, so the
	 * per-action cooldown must not also gate it.
	 */
	UFUNCTION(BlueprintCallable, Category = "DCC|Combat")
	FDCCAttackResult PerformAttack(ADCCCharacterBase* Target, bool bIgnoreCooldown = false);

	/**
	 * Safe to call anywhere, including on clients: it routes to the server
	 * rather than resolving locally. This is what input should call.
	 *
	 * While combat is paused this queues the swing instead of throwing it, so
	 * the same button means "attack" live and "plan an attack" in the pause.
	 */
	UFUNCTION(BlueprintCallable, Category = "DCC|Combat")
	void AttemptAttackNearest();

	/** Any player may pause. Routes to the server; safe to call from a client. */
	UFUNCTION(BlueprintCallable, Category = "DCC|Combat")
	void RequestTogglePause();

	/** Nearest living hostile within attack range, or null. */
	UFUNCTION(BlueprintPure, Category = "DCC|Combat")
	ADCCCharacterBase* FindNearestHostileInRange() const;

	/**
	 * Whether this character is on the crawlers' side rather than dungeon-side.
	 * ("Crawler" is the DCC ruleset's own word for a player character.)
	 *
	 * Deliberately NOT derived from "is possessed by a PlayerController".
	 * Overlap events fire while a pawn is still spawning, before any controller
	 * has possessed it, so a possession-based test reports false for a player
	 * who begins play already standing inside a trigger - which silently broke
	 * room entry. Class identity is correct from the moment the actor exists.
	 */
	UFUNCTION(BlueprintPure, Category = "DCC")
	virtual bool IsCrawler() const { return true; }

	/**
	 * Placeholder faction rule: crawlers and dungeon-side are hostile to each
	 * other, nothing is hostile to its own side. Enough for one encounter and
	 * deliberately trivial to replace when real factions exist.
	 */
	UFUNCTION(BlueprintPure, Category = "DCC|Combat")
	virtual bool IsHostileTo(const ADCCCharacterBase* Other) const;

protected:
	virtual void BeginPlay() override;
	virtual void SetupPlayerInputComponent(UInputComponent* PlayerInputComponent) override;

	/**
	 * Called on the owning client once the pawn is ready for local play. This
	 * is where the HUD is built, rather than BeginPlay, because BeginPlay can
	 * run before the controller has possessed us and IsLocallyControlled would
	 * still be false.
	 */
	virtual void PawnClientRestart() override;

	/** Target selection is repeated server-side; the client's pick is never trusted. */
	UFUNCTION(Server, Reliable)
	void Server_AttackNearest();

	UFUNCTION(Server, Reliable)
	void Server_TogglePause();

	UFUNCTION(Server, Reliable)
	void Server_StowCarried();

	UFUNCTION(Server, Reliable)
	void Server_AttemptClimb();

	/** Server-side: swing now, or park the intent if the world is paused. */
	void ResolveOrQueueAttack();

	UFUNCTION()
	virtual void HandleDied(UDCCStatsComponent* InStats);

	/**
	 * Spawns a floating damage or healing number over this character.
	 *
	 * Bound to the stats component's OnHealthChanged, which already fires on every
	 * machine - the server through ApplyDamage and clients through OnRep - so each
	 * machine produces its own local ticker and nothing is replicated.
	 */
	UFUNCTION()
	void HandleHealthChangedForTicker(UDCCStatsComponent* InStats, int32 NewHitPoints, int32 Delta);

	/** Cosmetic only - never gates or resolves anything. Hook animation here. */
	UFUNCTION(NetMulticast, Unreliable)
	void Multicast_PlayAttack(ADCCCharacterBase* Target, bool bHit);

	UFUNCTION(BlueprintImplementableEvent, Category = "DCC|Combat", meta = (DisplayName = "On Attack Performed"))
	void BP_OnAttackPerformed(ADCCCharacterBase* Target, bool bHit);

	UFUNCTION(BlueprintImplementableEvent, Category = "DCC", meta = (DisplayName = "On Died"))
	void BP_OnDied();

	/** Seconds the corpse stays before cleanup on the server. 0 keeps it forever. */
	UPROPERTY(EditDefaultsOnly, Category = "DCC")
	float CorpseLifeSpan = 0.0f;

private:
	/** Restores LocomotionAnimation when a one-shot clip finishes. */
	FTimerHandle AnimationResetTimer;

	/** Server-side only; clients never gate on this. */
	float LastAttackTime = TNumericLimits<float>::Lowest();

	bool bHasDied = false;
};
