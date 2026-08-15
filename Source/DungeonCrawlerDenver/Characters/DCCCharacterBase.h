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
class UDCCAchievementComponent;
class ADCCCharacterBase;
class UUserWidget;
class UTextRenderComponent;

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

	virtual void Tick(float DeltaSeconds) override;

	/**
	 * Server only. Where a character learns it belongs to a person.
	 *
	 * The toughness bonus is applied here rather than authored on the Blueprint
	 * because the same Blueprint can be both: BP_DCCEnemy_HockeyWings is CJ as an
	 * AI ally today and CJ as a player's crawler the moment he joins.
	 */
	virtual void PossessedBy(AController* NewController) override;

	virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "DCC")
	TObjectPtr<UDCCStatsComponent> Stats;

	/**
	 * Floating name and allegiance, drawn above the head.
	 *
	 * A UTextRenderComponent rather than a UMG widget, for the same reason the
	 * damage tickers are: no asset to author, works the moment it compiles, and
	 * the colour is one call rather than a binding. It is what makes an ally
	 * legible at a glance instead of "the one in the hockey jersey".
	 */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "DCC")
	TObjectPtr<UTextRenderComponent> Nameplate;

	/** Tracks the party's achievements. Only meaningful on a crawler. */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "DCC")
	TObjectPtr<UDCCAchievementComponent> Achievements;

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

	/**
	 * Locomotion clips, chosen by ground speed each frame.
	 *
	 * Any of them may be left unset and the selection falls back through
	 * Run -> Walk -> Idle, so a character with only a run cycle still works - it
	 * just never stands still. This is a hand-rolled substitute for a locomotion
	 * blendspace, which would need an AnimBlueprint per skeleton.
	 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "DCC|Animation")
	TObjectPtr<UAnimationAsset> IdleAnimation;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "DCC|Animation")
	TObjectPtr<UAnimationAsset> WalkAnimation;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "DCC|Animation")
	TObjectPtr<UAnimationAsset> RunAnimation;

	/** Above this ground speed the character is considered moving at all. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "DCC|Animation", meta = (ClampMin = "0.0"))
	float WalkSpeedThreshold = 20.0f;

	/** Above this ground speed the run cycle is used instead of the walk. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "DCC|Animation", meta = (ClampMin = "0.0"))
	float RunSpeedThreshold = 320.0f;

	/**
	 * Victory emotes. One is picked at random when this character lands a kill.
	 *
	 * The showrunner fiction rewards playing to the crowd, so a kill that ends in
	 * a dance is mechanically pointless and tonally exactly right.
	 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "DCC|Animation")
	TArray<TObjectPtr<UAnimationAsset>> EmoteAnimations;

	/**
	 * Chance from 0 to 1 that a kill triggers an emote.
	 *
	 * Zero by default so mobs stay businesslike; allies are turned up, because a
	 * teammate celebrating is characterful and a mob doing it every time would
	 * quickly become noise.
	 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "DCC|Animation", meta = (ClampMin = "0.0", ClampMax = "1.0"))
	float EmoteChanceOnKill = 0.0f;

	/** Picks and plays a random emote. Does nothing without any configured. */
	UFUNCTION(BlueprintCallable, Category = "DCC|Animation")
	void PlayEmote();

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
	 * Played by both parties to a fist bump.
	 *
	 * No pack ships a fist-bump clip, so this falls back to a random emote when
	 * unset - an ally throwing a celebration at you reads close enough to a
	 * greeting that the moment still lands.
	 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "DCC|Animation")
	TObjectPtr<UAnimationAsset> FistBumpAnimation;

	// -------------------------------------------------------------------------
	// ALLEGIANCE
	//
	// Two separate ideas, deliberately not collapsed into one. IsCrawler decides
	// who may hit whom and is authored on the actor. Befriending is a thing a
	// player *does* in the world, and only ever applies to someone already on
	// your side - you cannot make friends with something trying to eat you.
	// -------------------------------------------------------------------------

	/**
	 * Whether the party has fist-bumped this character.
	 *
	 * Party-wide rather than per-player: in a five-player co-op two clients
	 * disagreeing about whether CJ is a friend would show two different colours
	 * over the same head, and nothing in the fiction wants that.
	 */
	UPROPERTY(BlueprintReadOnly, ReplicatedUsing = OnRep_Befriended, Category = "DCC|Allegiance")
	bool bBefriended = false;

	UFUNCTION(BlueprintPure, Category = "DCC|Allegiance")
	bool IsBefriended() const { return bBefriended; }

	/**
	 * How close you have to be to offer a fist bump. Generous, because missing a
	 * social interaction by 20cm is only ever annoying.
	 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "DCC|Allegiance", meta = (ClampMin = "0.0"))
	float FistBumpRange = 320.0f;

	/**
	 * Looks down the crosshair for a friendly face and bumps it.
	 *
	 * Returns true if it found one, so the caller knows whether the click was
	 * consumed or should fall through to an attack.
	 */
	UFUNCTION(BlueprintCallable, Category = "DCC|Allegiance")
	bool TryFistBumpUnderCursor();

	/** Colour this character's nameplate and health bar should use. */
	UFUNCTION(BlueprintPure, Category = "DCC|Allegiance")
	FLinearColor GetAllegianceColour() const;

	/** Nameplate text: the character's name plus its standing with the party. */
	UFUNCTION(BlueprintPure, Category = "DCC|Allegiance")
	FText GetNameplateText() const;

	/** Shown above the head. Defaults to the actor label when left empty. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "DCC|Allegiance")
	FText DisplayName;

	/**
	 * Plays a clip once; Tick restores the right locomotion loop afterwards.
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

	// -------------------------------------------------------------------------
	// CLIMBING
	//
	// Player-driven rather than prompted: open the menu, select the skill, click
	// a face, confirm, then hold to climb until stamina runs out. No roll gates
	// the attempt - anyone may try anything, and stamina decides how far they get.
	// -------------------------------------------------------------------------

	/** Skill armed from the character menu, awaiting a click on a target. */
	UPROPERTY(BlueprintReadOnly, Category = "DCC|Climb")
	FName SelectedSkillId;

	/** Surface clicked and awaiting a yes/no answer. Null when nothing is pending. */
	UPROPERTY(BlueprintReadOnly, Category = "DCC|Climb")
	TObjectPtr<ADCCClimbPoint> PendingClimbPoint;

	/** The surface currently being climbed, or null. */
	UPROPERTY(BlueprintReadOnly, ReplicatedUsing = OnRep_ActiveClimbPoint, Category = "DCC|Climb")
	TObjectPtr<ADCCClimbPoint> ActiveClimbPoint;

	UFUNCTION(BlueprintPure, Category = "DCC|Climb")
	bool IsClimbing() const { return ActiveClimbPoint != nullptr; }

	UFUNCTION(BlueprintPure, Category = "DCC|Climb")
	FName GetSelectedSkillId() const { return SelectedSkillId; }

	UFUNCTION(BlueprintPure, Category = "DCC|Climb")
	ADCCClimbPoint* GetPendingClimbPoint() const { return PendingClimbPoint; }

	/** Arms a skill for targeting. Passing None clears the selection. */
	UFUNCTION(BlueprintCallable, Category = "DCC|Climb")
	void SelectSkill(FName SkillId);

	/**
	 * Traces from the camera through the crosshair and, if it lands on a
	 * climbable face within reach, raises the confirmation box.
	 *
	 * Local: it only decides what to ask. The answer is what goes to the server.
	 */
	UFUNCTION(BlueprintCallable, Category = "DCC|Climb")
	void TrySelectClimbTargetUnderCursor();

	/** Answers the confirmation box. */
	UFUNCTION(BlueprintCallable, Category = "DCC|Climb")
	void ConfirmPendingClimb(bool bAccept);

	/** Held-input state, set by the climb keys and read on the server each tick. */
	UFUNCTION(BlueprintCallable, Category = "DCC|Climb")
	void SetClimbInput(bool bUp, bool bDown);

	/** Lets go and drops. Also called automatically when stamina runs out. */
	UFUNCTION(BlueprintCallable, Category = "DCC|Climb")
	void ReleaseClimb();

	// -------------------------------------------------------------------------
	// SPRINTING
	//
	// Shares the stamina pool with climbing on purpose. Sprinting to a wall and
	// then climbing it should cost more than doing either alone - one budget
	// makes that trade-off real without any extra rules.
	// -------------------------------------------------------------------------

	UPROPERTY(BlueprintReadOnly, ReplicatedUsing = OnRep_Sprinting, Category = "DCC|Movement")
	bool bIsSprinting = false;

	/** Held-input setter, bound to Shift. Safe to call from a client. */
	UFUNCTION(BlueprintCallable, Category = "DCC|Movement")
	void SetSprintInput(bool bWantsToSprint);

	UFUNCTION(BlueprintPure, Category = "DCC|Movement")
	bool IsSprinting() const { return bIsSprinting; }

protected:
	// Input handlers. Left click means three different things depending on
	// state, so the routing lives here rather than in the binding.
	void OnPrimaryPressed();
	void OnPrimaryReleased();
	void OnSecondaryPressed();
	void OnSecondaryReleased();

	void ReleaseClimbIfClimbing() { if (IsClimbing()) { ReleaseClimb(); } }

	void AcceptPrompt() { ConfirmPendingClimb(true); }
	void DeclinePrompt() { ConfirmPendingClimb(false); }

	void StartSprint() { SetSprintInput(true); }
	void StopSprint() { SetSprintInput(false); }

	/** Arms the Nth known skill. One handler per key; BindKey carries no payload. */
	void SelectSkillSlot(int32 Index);
	void SelectSkillSlot1() { SelectSkillSlot(0); }
	void SelectSkillSlot2() { SelectSkillSlot(1); }
	void SelectSkillSlot3() { SelectSkillSlot(2); }
	void SelectSkillSlot4() { SelectSkillSlot(3); }
	void SelectSkillSlot5() { SelectSkillSlot(4); }

public:

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
	void Server_BeginClimb(ADCCClimbPoint* Point);

	UFUNCTION(Server, Reliable)
	void Server_SetClimbInput(bool bUp, bool bDown);

	UFUNCTION(Server, Reliable)
	void Server_ReleaseClimb();

	UFUNCTION(Server, Reliable)
	void Server_SetSprinting(bool bNewSprinting);

	UFUNCTION()
	void OnRep_ActiveClimbPoint();

	UFUNCTION(Server, Reliable)
	void Server_FistBump(ADCCCharacterBase* Target);

	/** Cosmetic half of the bump: both parties turn and play the clip. */
	UFUNCTION(NetMulticast, Unreliable)
	void Multicast_PlayFistBump(ADCCCharacterBase* Partner);

	UFUNCTION()
	void OnRep_Befriended();

	/** Re-applies the nameplate's text and colour. Cheap; call whenever either moves. */
	void RefreshNameplate();

	/** Server only. Credits this character for putting Victim down. */
	void AwardKillExperience(ADCCCharacterBase* Victim);

public:
	/**
	 * Logs the level and records it against achievements.
	 *
	 * Public because XP arrives from several places - kills here, clear bonuses
	 * from the encounter - and all of them should announce it identically.
	 */
	UFUNCTION(BlueprintCallable, Category = "DCC|Experience")
	void AnnounceLevelUp(int32 PreviousLevel);

protected:

	UFUNCTION()
	void OnRep_Sprinting();

	/** Server only. Runs the climb and sprint stamina economy for one frame. */
	void TickStamina(float DeltaSeconds);

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

	/**
	 * Plays the victory emote on every machine.
	 *
	 * Multicast rather than local, because the server rolls the chance - letting
	 * each client roll separately would have half the table watching a dance the
	 * others never saw.
	 */
	UFUNCTION(NetMulticast, Unreliable)
	void Multicast_PlayEmote();

	UFUNCTION(BlueprintImplementableEvent, Category = "DCC|Combat", meta = (DisplayName = "On Attack Performed"))
	void BP_OnAttackPerformed(ADCCCharacterBase* Target, bool bHit);

	UFUNCTION(BlueprintImplementableEvent, Category = "DCC", meta = (DisplayName = "On Died"))
	void BP_OnDied();

	/** Seconds the corpse stays before cleanup on the server. 0 keeps it forever. */
	UPROPERTY(EditDefaultsOnly, Category = "DCC")
	float CorpseLifeSpan = 0.0f;

private:
	/**
	 * World time until which a one-shot clip owns the mesh.
	 *
	 * A timestamp rather than a timer handle: Tick already runs for locomotion,
	 * so comparing a deadline is simpler than juggling timer callbacks, and it
	 * cannot fire against a half-destroyed actor.
	 */
	float OneShotAnimationUntil = 0.0f;

	/** The loop currently playing, so Tick only swaps clips when it must. */
	UPROPERTY(Transient)
	TObjectPtr<UAnimationAsset> ActiveLoopAnimation;

	/** Run/Walk/Idle for the current ground speed, with fallbacks. */
	UAnimationAsset* SelectLocomotionAnimation() const;

	/** Puts the right loop on the mesh unless a one-shot is still playing. */
	void UpdateLocomotionAnimation();

	/** Held climb input, mirrored to the server. */
	bool bClimbInputUp = false;
	bool bClimbInputDown = false;

	/** Centimetres climbed since the last whole metre was recorded. */
	float ClimbDistanceAccumulator = 0.0f;

	/** Server-side only; clients never gate on this. */
	float LastAttackTime = TNumericLimits<float>::Lowest();

	bool bHasDied = false;
};
