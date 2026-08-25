#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Character.h"
#include "Stats/DCCAbilityTypes.h"
#include "Items/DCCLootTypes.h"
#include "Items/DCCLootSource.h"
#include "Items/DCCConsumableTypes.h"
#include "UI/DCCInteractionTypes.h"
#include "UI/DCCChatTypes.h"
#include "Skills/DCCSkillTypes.h"
#include "Classes/DCCClassTypes.h"
#include "DCCCharacterBase.generated.h"

class UDCCStatsComponent;
class UDCCSkillComponent;
class UDCCFavorComponent;
class UDCCCarryComponent;
class UDCCInventoryComponent;
class UDCCAchievementComponent;
class UDCCEquipmentComponent;
class UDCCClassComponent;
class ADCCCharacterBase;
class ADCCDialogueNPC;
class UUserWidget;
class UTextRenderComponent;
class USoundBase;
class UCameraComponent;
class UNiagaraSystem;

DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FDCCCharacterDied, ADCCCharacterBase*, Character);

/**
 * Broadcast on the server whenever any character actually attempts a swing
 * (hit or miss - see PerformAttack). Plain C++ delegate, not a UPROPERTY, and
 * static rather than per-instance: a passive guard (see
 * ADCCEnemyCharacter::bPassiveUntilProvoked) needs to notice violence
 * anywhere nearby without holding a reference to every possible combatant
 * ahead of time, which a per-instance delegate would require.
 */
DECLARE_MULTICAST_DELEGATE_TwoParams(FDCCAttackAttempted, ADCCCharacterBase* /*Attacker*/, ADCCCharacterBase* /*Target*/);

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
class DUNGEONCRAWLERDENVER_API ADCCCharacterBase : public ACharacter, public IDCCLootSource
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

	/**
	 * Held out in front for a selfie. Inactive by default - only turned on for
	 * the brief window Selfie Mode is open, so it never competes with whatever
	 * camera rig the character Blueprint actually plays with day to day.
	 */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "DCC|Selfie")
	TObjectPtr<UCameraComponent> SelfieCamera;

	/** Tracks the party's achievements. Only meaningful on a crawler. */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "DCC")
	TObjectPtr<UDCCAchievementComponent> Achievements;

	/** Worn gear. Feeds the Enhanced stat layer and hangs meshes off sockets. */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "DCC")
	TObjectPtr<UDCCEquipmentComponent> Equipment;

	/** Third Floor Class selection. Also feeds the Enhanced stat layer, grants
	 *  Skill Ranks, and equips a starter kit through Equipment above - see
	 *  Docs/races-and-classes.md. */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "DCC")
	TObjectPtr<UDCCClassComponent> Class;

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

	/** Melee reach. Overridden by a ranged weapon - see GetEffectiveAttackRange. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "DCC|Combat", meta = (ClampMin = "0.0"))
	float AttackRange = 200.0f;

	/** AttackRange, or the longest reach among equipped weapons if any is ranged. */
	UFUNCTION(BlueprintPure, Category = "DCC|Combat")
	float GetEffectiveAttackRange() const;

	/**
	 * Doubles as this action's initiative/cooldown value. The design doc calls
	 * for per-action cooldowns specifically so unpausing does not degenerate
	 * into button-mashing; this is where that number lives.
	 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "DCC|Combat", meta = (ClampMin = "0.0"))
	float AttackCooldownSeconds = 1.5f;

	UPROPERTY(BlueprintAssignable, Category = "DCC")
	FDCCCharacterDied OnCharacterDied;

	/** See FDCCAttackAttempted's own comment. Server-side only - nothing broadcasts this on a client. */
	static FDCCAttackAttempted OnAnyAttackAttempted;

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

	// -------------------------------------------------------------------------
	// GETTING HIT
	//
	// Combat previously resolved with nothing happening to the person it happened
	// to: the attacker swung, a number floated up, and the target stood there
	// unmoved. That reads as two people ignoring each other, and it is the single
	// biggest reason fights here have felt weightless.
	//
	// Two halves, because no hit-reaction clip exists in the project yet:
	//
	//   1. HitReactionAnimation - the proper answer, left null until a reaction
	//      pack is imported. Anything assigned here is played on every machine.
	//   2. A stagger - a short shove away from whoever hit you. Needs no assets
	//      at all, works today, and is honestly most of the readability: bodies
	//      that move when struck look hit even with no bespoke animation.
	// -------------------------------------------------------------------------

	/** Played on the target when it takes damage. Optional. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "DCC|Animation")
	TObjectPtr<UAnimationAsset> HitReactionAnimation;

	/**
	 * How long the reaction is squeezed into.
	 *
	 * Short on purpose. The stand-in clip is not a flinch, it is a whole gesture
	 * compressed - at a quarter of a second that reads as a jolt, and at any
	 * longer it reads as the character doing something odd.
	 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "DCC|Animation", meta = (ClampMin = "0.05"))
	float HitReactionSeconds = 0.28f;

	/** Shove away from the attacker, in cm/s. Zero disables the stagger. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "DCC|Animation", meta = (ClampMin = "0.0"))
	float HitStaggerStrength = 260.0f;

	/**
	 * Minimum gap between reactions.
	 *
	 * Without it a fast attacker holds the target in a permanent flinch and it
	 * stops reading as individual hits - which is worse than no reaction at all.
	 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "DCC|Animation", meta = (ClampMin = "0.0"))
	float HitReactionCooldown = 0.22f;

	/** Above this ground speed the run cycle is used instead of the walk. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "DCC|Animation", meta = (ClampMin = "0.0"))
	float RunSpeedThreshold = 320.0f;

	/**
	 * Temporary cap on movement speed. Zero means "no override".
	 *
	 * Exists because derived speed is `BaseWalkSpeed * modifier`, which lands at
	 * 500+ for everybody - comfortably above RunSpeedThreshold. The consequence
	 * was that a character had exactly two states, standing still and sprinting,
	 * and an ally escorting the party therefore looked like it was permanently
	 * fleeing something. This is how a character asks to move at a walk.
	 */
	UPROPERTY(BlueprintReadOnly, Category = "DCC|Movement")
	float MovementSpeedOverride = 0.0f;

	UFUNCTION(BlueprintCallable, Category = "DCC|Movement")
	void SetMovementSpeedOverride(float NewSpeed);

	/**
	 * A flat, additive Class-benefit bonus (e.g. "+5ft Move"), unlike
	 * MovementSpeedOverride above, which caps speed rather than adding to it -
	 * the two would fight each other if this reused that field. Derived
	 * identically on every machine from the same replicated ClassComponent
	 * selection, so this itself does not need to replicate.
	 */
	UPROPERTY(BlueprintReadOnly, Category = "DCC|Movement")
	float ClassMoveSpeedBonus = 0.0f;

	UFUNCTION(BlueprintCallable, Category = "DCC|Movement")
	void SetClassMoveSpeedBonus(float NewBonus);

	// -------------------------------------------------------------------------
	// SOUND
	//
	// Hooks, not content. The project ships no audio at all, and the reason is
	// not that nobody wanted any - it is that there was nowhere to put a sound
	// once you had one. These sit on the multicast paths that already exist for
	// animation, so a clip assigned here is heard on every machine with no
	// further wiring.
	//
	// All optional. A null sound is silence, exactly as today.
	// -------------------------------------------------------------------------

	/** Played on every machine when this character swings. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "DCC|Audio")
	TObjectPtr<USoundBase> AttackSound;

	/** Played when this character is hurt, wherever it was standing. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "DCC|Audio")
	TObjectPtr<USoundBase> HurtSound;

	/** Played once, at the moment this character goes down for good. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "DCC|Audio")
	TObjectPtr<USoundBase> DeathSound;

	/** Played when a crawler drops to the floor but can still be saved. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "DCC|Audio")
	TObjectPtr<USoundBase> DownedSound;

	/** Played on the revived crawler when somebody gets them up. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "DCC|Audio")
	TObjectPtr<USoundBase> ReviveSound;

	/** Played when a skill goes off, on the caster. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "DCC|Audio")
	TObjectPtr<USoundBase> CastSound;

	/**
	 * Camera shake played on this character's own view when they are hurt.
	 *
	 * A hook like the sounds above, and unset by default - the Combat variant
	 * already ships `BP_CameraShake_Hit_Player`, which can be dropped straight in.
	 * Only ever plays for the player it happened to: shaking the whole party's
	 * cameras when one person is hit is how you get five people feeling every
	 * blow and none of them feeling their own.
	 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "DCC|Audio")
	TSubclassOf<class UCameraShakeBase> HurtCameraShake;

	/** Fire-and-forget 3D sound at this character's feet. Null is silence. */
	UFUNCTION(BlueprintCallable, Category = "DCC|Audio")
	void PlayCharacterSound(USoundBase* Sound) const;

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

	// -------------------------------------------------------------------------
	// TALKING TO PEOPLE
	//
	// `E` is contextual: an ally under the crosshair opens a conversation, and
	// anything else falls through to the pickup/drop it has always done. That
	// ordering matters - a crawler standing on a crate next to CJ should get the
	// conversation, because the crate is still there afterwards and CJ may not be.
	//
	// The menu itself is local and cosmetic. Everything it can *do* routes to the
	// server through Server_PerformInteraction, which re-validates range and
	// availability - a client may open any menu it likes and still be refused.
	// -------------------------------------------------------------------------

	/** Whoever this crawler is currently talking to, or null. */
	UFUNCTION(BlueprintPure, Category = "DCC|Interaction")
	ADCCCharacterBase* GetInteractionTarget() const { return InteractionTarget; }

	UFUNCTION(BlueprintPure, Category = "DCC|Interaction")
	const TArray<FDCCInteractionEntry>& GetInteractionOptions() const { return InteractionOptions; }

	UFUNCTION(BlueprintPure, Category = "DCC|Interaction")
	int32 GetInteractionIndex() const { return InteractionIndex; }

	/**
	 * Whose gear the HUD should be showing, or null.
	 *
	 * Separate from InteractionTarget because the gear panel outlives the choice
	 * that opened it - you pick "Look at their gear", read it, and the menu is
	 * still there underneath to pick something else.
	 */
	UFUNCTION(BlueprintPure, Category = "DCC|Interaction")
	ADCCCharacterBase* GetInspectedCharacter() const { return InspectedCharacter; }

	UFUNCTION(BlueprintCallable, Category = "DCC|Interaction")
	void SetInspectedCharacter(ADCCCharacterBase* Target) { InspectedCharacter = Target; }

	/**
	 * Opens a conversation with Target directly - the Tab menu's Chat tab uses
	 * this to start talking to a nearby friendly face picked from a list,
	 * rather than requiring the crosshair to be aimed at them.
	 *
	 * Skips the hostile-nearby confirm step FindInteractionTargetUnderCursor's
	 * caller applies: that step exists to catch an accidental E press near a
	 * fight, and picking a name from a menu is already the deliberate action
	 * it is meant to distinguish from. Still refuses a dead or hostile target,
	 * same as the crosshair path - and Server_PerformInteraction re-validates
	 * range and hostility itself regardless, so this cannot be used to talk to
	 * someone genuinely out of reach or on the other side.
	 */
	UFUNCTION(BlueprintCallable, Category = "DCC|Interaction")
	void OpenChat(ADCCCharacterBase* Target);

	/** How close you must be to start a conversation. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "DCC|Interaction", meta = (ClampMin = "0.0"))
	float InteractionRange = 400.0f;

	/**
	 * How close a living hostile must be for the game to treat you as "in a
	 * fight" for interaction purposes.
	 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "DCC|Interaction", meta = (ClampMin = "0.0"))
	float CombatProximityRadius = 1600.0f;

	/** Seconds the armed talk prompt stays live before it lapses. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "DCC|Interaction", meta = (ClampMin = "0.0"))
	float TalkConfirmSeconds = 2.5f;

	/** True when something hostile and alive is inside CombatProximityRadius. */
	UFUNCTION(BlueprintPure, Category = "DCC|Interaction")
	bool IsHostileNearby() const;

	/**
	 * Who the next E press would open a conversation with, while a fight is on.
	 *
	 * Read by the HUD to draw the "press E again to talk" prompt - the whole
	 * point of the two-step is that the player is told it exists.
	 */
	UFUNCTION(BlueprintPure, Category = "DCC|Interaction")
	ADCCCharacterBase* GetPendingTalkTarget() const;

	/** Difficulty of the Persuade check. Low - this is a friendly face. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "DCC|Interaction", meta = (ClampMin = "0"))
	int32 PersuadeDifficulty = 11;

	/** Intimidate is harder than Persuade, and worse when it misses. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "DCC|Interaction", meta = (ClampMin = "0"))
	int32 IntimidateDifficulty = 14;

	/** `E`. Opens a conversation, confirms the highlighted option, or picks up. */
	UFUNCTION(BlueprintCallable, Category = "DCC|Interaction")
	void HandleInteractPressed();

	/** Moves the highlight. Wraps, so the list is a loop rather than a wall. */
	UFUNCTION(BlueprintCallable, Category = "DCC|Interaction")
	void CycleInteractionOption(int32 Delta);

	// BindKey carries no payload, so the wheel needs one thunk per direction -
	// the same reason the five skill slots each have their own function.
	//
	// Routed to whichever menu is actually open. The three are mutually
	// exclusive in practice - opening one closes the others - but checking
	// VendorTargetActor and LootTargetActor first costs nothing and means the
	// wheel never silently does nothing because it was driving the wrong
	// menu's index.
	void InteractionScrollUp() { VendorTargetActor ? CycleVendorMenu(-1) : LootTargetActor ? CycleLootMenu(-1) : CycleInteractionOption(-1); }
	void InteractionScrollDown() { VendorTargetActor ? CycleVendorMenu(1) : LootTargetActor ? CycleLootMenu(1) : CycleInteractionOption(1); }

	UFUNCTION(BlueprintCallable, Category = "DCC|Interaction")
	void CloseInteraction();

	// -------------------------------------------------------------------------
	// GOING DOWN, AND GETTING BACK UP
	//
	// A crawler at zero HP is *downed*, not dead: still on the floor, still an
	// actor, still repossessable, and on a bleed-out timer. Anyone else on the
	// party can pick them back up before it expires.
	//
	// Split from death rather than bolted onto it because reviving a fully
	// destroyed pawn is a rebuild - the controller has been detached, the mesh is
	// simulating, and the capsule is gone. Keeping a downed crawler intact means
	// revive only has to undo three things instead of reconstruct them.
	//
	// Mobs are exempt. A Debt Eater that gets back up is not a story beat, it is
	// a bug report.
	// -------------------------------------------------------------------------

	/** On the floor at 0 HP, still savable. False once bled out. */
	UPROPERTY(BlueprintReadOnly, ReplicatedUsing = OnRep_Downed, Category = "DCC|Downed")
	bool bIsDowned = false;

	UFUNCTION(BlueprintPure, Category = "DCC|Downed")
	bool IsDowned() const { return bIsDowned; }

	/** True once death saves have run out. Permanent for now - see CLAUDE.md's
	 *  note on the 1-2 rare resurrection instances being a later, separate pass. */
	UFUNCTION(BlueprintPure, Category = "DCC|Downed")
	bool IsDead() const { return bHasDied; }

	/** Whether this crawler can still send a chat message - alive, or dead but
	 *  hasn't used their one final send yet. */
	UFUNCTION(BlueprintPure, Category = "DCC|Chat")
	bool CanSendChatMessage() const { return !bHasDied || !bHasSentFinalMessage; }

	/**
	 * Every source contributing to this character's current score for Ability -
	 * base score, Equipment, Class - in that order, for the Feed's
	 * click-to-inspect roll breakdown. Uses CURRENT stats, not a snapshot from
	 * whenever the clicked roll actually happened; the two agree in the
	 * overwhelming majority of cases (rolls are clicked shortly after they
	 * land) and a perfect historical snapshot isn't worth the replication cost
	 * of attaching a full breakdown to every single roll this game makes.
	 */
	UFUNCTION(BlueprintPure, Category = "DCC|Stats")
	TArray<FDCCModifierSource> GetAbilityScoreBreakdown(EDCCAbility Ability) const;

	/** Same idea as GetAbilityScoreBreakdown, for a Skill's Rank - earned Rank,
	 *  then any Class contribution. Empty for a Skill nobody has ever granted. */
	UFUNCTION(BlueprintPure, Category = "DCC|Stats")
	TArray<FDCCModifierSource> GetSkillRankBreakdown(FName SkillId) const;

	/**
	 * Server only. Posts Text into Channel, or into a Whisper to
	 * WhisperTargetName (ignored for every other channel).
	 *
	 * A dead crawler is allowed exactly one more send (bHasSentFinalMessage
	 * flips true right after it goes through), then refused - re-validated
	 * here rather than trusted from the client, same as every other action in
	 * this class. Public rather than protected, unlike most Server_ RPCs here,
	 * because DCCHUD calls it directly from the chat input widget's submit
	 * handler - the HUD is not a member of this class the way its own input
	 * handlers are.
	 */
	UFUNCTION(Server, Reliable)
	void Server_SendChatMessage(EDCCChatChannel Channel, const FString& WhisperTargetName, const FText& Text);

	/** Server only. Rolls a fresh Chatter batch - see
	 *  ADCCGameState::RollChatterBatch for why this is on-demand, not timed.
	 *  Public for the same reason Server_SendChatMessage is - DCCHUD calls it
	 *  directly when the Chatter tab opens. */
	UFUNCTION(Server, Reliable)
	void Server_RequestChatterBatch();

	/** Whether hitting 0 HP downs this character at all. False means straight to dead - the old mob behaviour. */
	UPROPERTY(EditDefaultsOnly, Category = "DCC|Downed")
	bool bCanBeDowned = true;

	// -------------------------------------------------------------------------
	// DEATH SAVES
	//
	// Replaces the old flat bleed-out countdown with the tabletop's own beat:
	// every few seconds, roll. Three successes stabilises on your own, at one
	// hit point; three failures ends it. A rescuer's revive still works at any
	// point in between and simply cuts the sequence short - the roll is what
	// happens while nobody has reached you yet, not a replacement for being
	// reached.
	// -------------------------------------------------------------------------

	/** Seconds between death-save rolls while downed. 6s matches a d&d/DCC combat round. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "DCC|Downed", meta = (ClampMin = "0.1"))
	float DeathSaveIntervalSeconds = 6.0f;

	/** d20 result needed to succeed. 11 means "above a 10", per how this was specified. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "DCC|Downed", meta = (ClampMin = "1", ClampMax = "20"))
	int32 DeathSaveDC = 11;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "DCC|Downed", meta = (ClampMin = "1"))
	int32 DeathSavesToStabilize = 3;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "DCC|Downed", meta = (ClampMin = "1"))
	int32 DeathSavesToExpire = 3;

	/** Replicated so every machine can draw the same pips. Reset when a new down-state begins. */
	UPROPERTY(BlueprintReadOnly, Replicated, Category = "DCC|Downed")
	int32 DeathSaveSuccesses = 0;

	UPROPERTY(BlueprintReadOnly, Replicated, Category = "DCC|Downed")
	int32 DeathSaveFailures = 0;

	UFUNCTION(BlueprintPure, Category = "DCC|Downed")
	int32 GetDeathSaveSuccesses() const { return DeathSaveSuccesses; }

	UFUNCTION(BlueprintPure, Category = "DCC|Downed")
	int32 GetDeathSaveFailures() const { return DeathSaveFailures; }

	/** Fraction of max HP restored by a RESCUED revive. Stabilising alone always gives exactly 1. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "DCC|Downed", meta = (ClampMin = "0.01", ClampMax = "1.0"))
	float ReviveHealthFraction = 0.35f;

	/**
	 * Seconds the rescuer is rooted while reviving.
	 *
	 * This is the whole cost of a revive, and it is a positional one: no check,
	 * no resource, just several seconds standing still in a room that is still
	 * trying to kill you. A dice roll here would make saving a friend feel
	 * arbitrary; a vulnerable window makes it feel brave.
	 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "DCC|Downed", meta = (ClampMin = "0.0"))
	float ReviveSeconds = 4.0f;

	/** Seconds until the next death-save roll. 0 when not downed. */
	UFUNCTION(BlueprintPure, Category = "DCC|Downed")
	float GetTimeUntilNextDeathSave() const;

	/**
	 * Server only. Starts the revive channel on Target - the one mechanic
	 * behind both a player's "Help Them Up" and an ally's own first aid AI, so
	 * the two can never drift out of sync with each other.
	 */
	UFUNCTION(BlueprintCallable, Category = "DCC|Downed")
	void BeginReviveChannel(ADCCCharacterBase* Target);

	/** Server only. Puts a downed crawler back on their feet. */
	UFUNCTION(BlueprintCallable, Category = "DCC|Downed")
	void Revive(ADCCCharacterBase* Rescuer);

	// -------------------------------------------------------------------------
	// LOOTING THE BODY
	//
	// Drops are rolled at the moment of death and held ON the corpse rather than
	// scattered across the floor. Same loot, later - but now the body is the
	// container, so a fight leaves a readable trail of who died where instead of
	// a carpet of physics props, and walking away from a corpse means leaving
	// something behind.
	//
	// A corpse and a chest (ADCCLootContainer) are unrelated actor types that
	// happen to share this one shape - see IDCCLootSource's own comment for why
	// that is an interface rather than a shared base class.
	// -------------------------------------------------------------------------

	virtual FText GetLootSourceName() const override;
	virtual bool HasAnyLoot() const override;
	virtual const TArray<FDCCRolledLoot>& GetPendingLootItems() const override;
	virtual FVector GetLootOrigin() const override;
	virtual void LootOne(int32 Index, ADCCCharacterBase* Looter) override;
	virtual void LootAll(ADCCCharacterBase* Looter) override;
	virtual void StowInto(ADCCCharacterBase* Looter) override;

	// -------------------------------------------------------------------------
	// THE LOOT MENU
	//
	// A second, parallel menu to the conversation one above - opened on a
	// corpse or a chest instead of a person, and deliberately NOT layered onto
	// InteractionTarget/InteractionOptions. Keeping the two apart means neither
	// has to special-case the other: the conversation menu never again needs a
	// bBody branch, and this one never needs to know what Persuade is.
	//
	// Same wheel-to-choose, E-to-confirm input as the conversation menu, and
	// the same "client picks, server re-validates" shape as everything else -
	// Server_LootOne/LootAll/StowLootSource all re-check range and authority.
	// -------------------------------------------------------------------------

	/** The corpse or chest currently open in the loot menu, or null. */
	UFUNCTION(BlueprintPure, Category = "DCC|Loot")
	AActor* GetLootTarget() const { return LootTargetActor; }

	/** False: the top menu (Check Inventory / Loot All / Stow / Leave). True: the itemised list. */
	UFUNCTION(BlueprintPure, Category = "DCC|Loot")
	bool IsShowingLootItems() const { return bLootShowingItems; }

	UFUNCTION(BlueprintPure, Category = "DCC|Loot")
	int32 GetLootMenuIndex() const { return LootMenuIndex; }

	/** Moves the highlight in whichever loot menu page is open. Wraps. */
	UFUNCTION(BlueprintCallable, Category = "DCC|Loot")
	void CycleLootMenu(int32 Delta);

	UFUNCTION(BlueprintCallable, Category = "DCC|Loot")
	void CloseLootMenu();

	// -------------------------------------------------------------------------
	// VENDOR - a dialogue NPC who also sells things (see ADCCVendorNPC)
	//
	// Deliberately its own menu rather than reusing the loot menu's shape -
	// buying is not looting. Equipment buys immediately; a drink instead arms
	// a tiny two-row sub-prompt (Drink Now / Stow For Later) - see
	// bVendorConfirmingDrink - because unlike equipment a drink has a real
	// choice attached to it. Same wheel-to-choose, E-to-confirm,
	// client-picks-server-re-validates shape as everything else.
	// -------------------------------------------------------------------------

	/** The vendor currently open in the shop menu, or null. */
	UFUNCTION(BlueprintPure, Category = "DCC|Vendor")
	AActor* GetVendorTarget() const { return VendorTargetActor; }

	UFUNCTION(BlueprintPure, Category = "DCC|Vendor")
	int32 GetVendorMenuIndex() const { return VendorMenuIndex; }

	/** True once a drink row has been picked and the Drink Now/Stow sub-prompt is showing. */
	UFUNCTION(BlueprintPure, Category = "DCC|Vendor")
	bool IsConfirmingVendorDrink() const { return bVendorConfirmingDrink; }

	UFUNCTION(BlueprintPure, Category = "DCC|Vendor")
	FName GetVendorPendingDrinkId() const { return VendorPendingDrinkId; }

	/** Opens Vendor's shop menu on this character. Local, like OpenChat. */
	void OpenVendorMenu(AActor* Vendor) { VendorTargetActor = Vendor; VendorMenuIndex = 0; }

	/** Moves the highlight in the shop menu, or in the Drink Now/Stow sub-prompt if that's open. Wraps. */
	UFUNCTION(BlueprintCallable, Category = "DCC|Vendor")
	void CycleVendorMenu(int32 Delta);

	UFUNCTION(BlueprintCallable, Category = "DCC|Vendor")
	void CloseVendorMenu();

	/**
	 * Empties one Stowed Remains record from this crawler's own pack, at their
	 * feet - the Tab menu's Inventory page calls this on a clicked row.
	 *
	 * Routed through the server like AttemptStow, because
	 * UDCCInventoryComponent::LootAllFromStowed refuses outright without
	 * authority - a client calling it directly would silently do nothing.
	 */
	UFUNCTION(BlueprintCallable, Category = "DCC|Loot")
	void DCCEmptyStowedRemains(int32 Index);

	/**
	 * Drinks one stowed consumable from this crawler's own pack - the Tab
	 * menu's Inventory page calls this on a clicked row. Same routing reason
	 * as DCCEmptyStowedRemains: UDCCInventoryComponent::ConsumeStowedConsumable
	 * refuses outright without authority.
	 */
	UFUNCTION(BlueprintCallable, Category = "DCC|Vendor")
	void DCCConsumeStowedDrink(int32 Index);

	/**
	 * Party command: stop escorting and stand still, or resume.
	 *
	 * A no-op on the base class - only ADCCEnemyCharacter has AI to stop. Declared
	 * here so the interaction menu can issue the order without caring what it is
	 * talking to, which is the same reason encounters take ADCCCharacterBase.
	 */
	UFUNCTION(BlueprintCallable, Category = "DCC|Interaction")
	virtual void SetHoldingPosition(bool bHold);

	// -------------------------------------------------------------------------
	// SELFIE MODE
	//
	// Entirely local and cosmetic - no server round trip, no replication, no
	// gameplay state changes. Holds SelfieCamera out in front for a beat, flips
	// the shutter (a quick white flash, matching a real phone), and requests a
	// screenshot the normal way - it lands in Saved/Screenshots/ exactly like
	// the console "shot" command's output would.
	// -------------------------------------------------------------------------

	/** `C`. Opens Selfie Mode, or cancels it early if already open. */
	UFUNCTION(BlueprintCallable, Category = "DCC|Selfie")
	void ToggleSelfieMode();

	UFUNCTION(BlueprintPure, Category = "DCC|Selfie")
	bool IsInSelfieMode() const { return bInSelfieMode; }

	/** Where the phone sits, relative to the capsule - in front of the face, arm's length. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "DCC|Selfie")
	FVector SelfieCameraOffset = FVector(110.0f, 0.0f, 40.0f);

	/** Yaw 180 so it looks back at the character, like a phone actually held out. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "DCC|Selfie")
	FRotator SelfieCameraRotation = FRotator(-8.0f, 180.0f, 0.0f);

	/** Seconds to hold the pose before the shutter fires - long enough to see it coming and smile. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "DCC|Selfie", meta = (ClampMin = "0.0"))
	float SelfiePoseSeconds = 0.8f;

	// -------------------------------------------------------------------------
	// TEST CHEATS
	//
	// Exec functions, so they run from the ~ console on the possessed pawn. These
	// exist because the alternative for trying a skill or a weapon is editing a
	// Blueprint, recompiling, and restarting - which is a five-minute round trip
	// for a question that deserves a five-second one.
	//
	// Server-guarded like everything else. A client typing DCC.GrantSkill gets
	// the request routed, re-validated, and applied authoritatively or not at all.
	// -------------------------------------------------------------------------

	/** `DCC.GrantSkill FireFingers 3` - learn any row in the skill table. */
	UFUNCTION(Exec, BlueprintCallable, Category = "DCC|Cheats")
	void DCCGrantSkill(const FString& SkillId, int32 Rank = 1);

	/** `DCC.Equip WallSword` - equip any row in the equipment table. */
	UFUNCTION(Exec, BlueprintCallable, Category = "DCC|Cheats")
	void DCCEquip(const FString& ItemId);

	// -------------------------------------------------------------------------
	// DM PANEL
	//
	// The same actions the exec commands expose, reachable as buttons. Zoe runs
	// this as the DM and should not have to remember command names mid-session -
	// the console is a programmer's affordance, a panel is a table's.
	//
	// Every one routes through a server RPC and is re-validated there, exactly
	// like the gameplay paths. A cheat that trusted the client would be the one
	// hole in an otherwise server-authoritative game.
	// -------------------------------------------------------------------------

	/** Opens or closes the DM panel. Local and cosmetic. */
	UFUNCTION(BlueprintCallable, Category = "DCC|Admin")
	void ToggleAdminPanel();

	/** Server. Spawns one of anything at the caller's feet. */
	UFUNCTION(Server, Reliable)
	void Server_AdminSpawn(TSubclassOf<AActor> ActorClass, bool bAsAlly);

	/** Server. Kills every living hostile in the level. */
	UFUNCTION(Server, Reliable)
	void Server_AdminClearHostiles();

	/** Server. Toggles damage immunity on the caller. */
	UFUNCTION(Server, Reliable)
	void Server_AdminSetInvulnerable(bool bInvulnerable);

	UFUNCTION(Server, Reliable)
	void Server_AdminAddGold(int32 Amount);

	/** Server. Grants a level, re-deriving the pools that depend on it. */
	UFUNCTION(Server, Reliable)
	void Server_AdminLevelUp();

	/** `DCC.Refill` - top up health, mana and stamina. */
	UFUNCTION(Exec, BlueprintCallable, Category = "DCC|Cheats")
	void DCCRefill();

	/** Server halves of the cheats above. */
	UFUNCTION(Server, Reliable)
	void Server_GrantSkill(FName SkillId, int32 Rank);

	UFUNCTION(Server, Reliable)
	void Server_EquipItem(FName ItemId);

	UFUNCTION(Server, Reliable)
	void Server_Refill();

	/** Opens or closes the character menu. Local and cosmetic. */
	UFUNCTION(BlueprintCallable, Category = "DCC|UI")
	void ToggleCharacterMenu();

	/** Flashes a message on this character's own HUD. No-op for AI. */
	void ShowLocalNotice(const FText& Message) const;

	/** Plain-English reason a skill was refused, for the notice above. */
	static FText DescribeSkillFailure(EDCCSkillFailure Failure, const FText& SkillName);

	/** Advances the HUD event feed to its next category filter. Local and cosmetic. */
	UFUNCTION(BlueprintCallable, Category = "DCC|UI")
	void CycleLogFilter();

	/** Pinned -> Minimized -> Hidden -> Pinned for the always-on Feed box.
	 *  Local and cosmetic - re-pinning is also reachable from the Feed tab. */
	UFUNCTION(BlueprintCallable, Category = "DCC|UI")
	void CycleFeedVisibility();

	/** Moves between character-menu pages. Local and cosmetic. */
	UFUNCTION(BlueprintCallable, Category = "DCC|UI")
	void CycleMenuTab(int32 Delta);

	void PreviousMenuTab() { CycleMenuTab(-1); }
	void NextMenuTab() { CycleMenuTab(1); }

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
	 * Traces from the camera through the crosshair for anything climbable and,
	 * if it finds something, consumes the click.
	 *
	 * Called from OnPrimaryPressed whenever a skill is armed (a climbable face
	 * always wins over casting), and otherwise only when there is no hostile in
	 * range to attack - see GenericClimbReachDistance for why attacking wins
	 * that race by default. Three outcomes when it does run:
	 *   - An authored ADCCClimbPoint in reach: raises the confirmation box, as
	 *     before.
	 *   - Any other wall-like surface in reach: starts climbing immediately via
	 *     Server_RequestGenericClimb, no confirmation - this is what makes an
	 *     un-authored rock face climbable at all.
	 *   - A wall-like surface out of reach: reports failure locally and still
	 *     consumes the click, so a click at a wall never just does nothing.
	 * Anything else under the cursor (floor, sky, a character) is not a climb
	 * attempt and returns false.
	 *
	 * Local: it only decides what to ask (or, for generic surfaces, what to
	 * request). The server re-validates everything before anything happens.
	 *
	 * @return True if the click was climb-related and should not be processed
	 *         further this frame.
	 */
	UFUNCTION(BlueprintCallable, Category = "DCC|Climb")
	bool TrySelectClimbTargetUnderCursor();

	/** Answers the confirmation box. */
	UFUNCTION(BlueprintCallable, Category = "DCC|Climb")
	void ConfirmPendingClimb(bool bAccept);

	/**
	 * What a click would target right now, for the given armed skill.
	 *
	 * Self, Cone and AreaOfEffect skills resolve against the caster - they are
	 * aimed by facing, not by clicking a specific actor, so this returns Self
	 * for those the instant a target is needed at all. SingleTarget traces from
	 * the camera through the crosshair for a hostile character within the
	 * skill's range.
	 *
	 * Pure and side-effect-free on purpose: the HUD calls this every frame to
	 * draw the targeting overlay, and the click handler calls the exact same
	 * function to decide what to cast at. The overlay can never show a target a
	 * click would not actually have hit, because there is only one function
	 * deciding that, not two that might disagree.
	 */
	UFUNCTION(BlueprintPure, Category = "DCC|Combat")
	AActor* FindSkillTargetUnderCursor(FName SkillId) const;

	/**
	 * Click handling for an armed non-climb skill: resolves the target via
	 * FindSkillTargetUnderCursor and fires or queues it.
	 *
	 * Only reached when TrySelectClimbTargetUnderCursor found no climbable face
	 * under the cursor - see OnPrimaryPressed. A climb point always wins the
	 * click regardless of which skill is armed, so aiming a spell at a wall you
	 * could climb offers the climb instead. Deliberate: there was no clean way
	 * to ask "is the armed skill specifically Climbing" without hard-coding a
	 * skill id, and falling through on a climb-miss needs none of that.
	 */
	UFUNCTION(BlueprintCallable, Category = "DCC|Combat")
	void TrySelectSkillTargetUnderCursor();

	/**
	 * How close a generic (un-authored) wall has to be for a click to climb it
	 * rather than report "too far to climb".
	 *
	 * A hostile in range already wins the click over climbing at all (see
	 * OnPrimaryPressed), so this only matters once there is nothing to fight -
	 * kept short anyway as a second line of defence, alongside the foot-level
	 * exclusion in TrySelectClimbTargetUnderCursor, against an ordinary
	 * movement click near a wall reading as a climb attempt.
	 */
	UPROPERTY(EditDefaultsOnly, Category = "DCC|Climb", meta = (ClampMin = "0.0"))
	float GenericClimbReachDistance = 260.0f;

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

	/**
	 * The "cheap version" of Pets/Mounts (see Classes/DCCClassTypes.h's
	 * GrantsMount comment) - a toggleable Move Speed bonus for classes that
	 * grant one, standing in for Rideshare Driver's sedan until a real
	 * spawned Pet/Mount actor exists. Refused silently (no state change) for
	 * a character whose Class never granted GrantsMount.
	 */
	UPROPERTY(BlueprintReadOnly, ReplicatedUsing = OnRep_Mounted, Category = "DCC|Movement")
	bool bIsMounted = false;

	/** Held-input setter, bound to M. Safe to call from a client. */
	UFUNCTION(BlueprintCallable, Category = "DCC|Movement")
	void ToggleMount();

	UFUNCTION(BlueprintPure, Category = "DCC|Movement")
	bool IsMounted() const { return bIsMounted; }

	// -------------------------------------------------------------------------
	// Casting
	//
	// A skill with a CastTimeSeconds roots its caster for that long. Without it a
	// skill costs Mana and a cooldown but no *position*, so there is nothing to
	// trade against - which is precisely the decision the pause/queue loop exists
	// to make interesting. Replicated rather than server-only because movement is
	// client-predicted: a client that does not know it is casting will run, then
	// get snapped back.
	// -------------------------------------------------------------------------

	UPROPERTY(BlueprintReadOnly, ReplicatedUsing = OnRep_Casting, Category = "DCC|Combat")
	bool bIsCasting = false;

	UFUNCTION(BlueprintPure, Category = "DCC|Combat")
	bool IsCasting() const { return bIsCasting; }

	/**
	 * Roots this character for Seconds. Server-only; a zero or negative duration
	 * is a no-op, which is what makes CastTimeSeconds default-0 mean "instant".
	 */
	void BeginCastLock(float Seconds);

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

	/**
	 * Carries a skill cast to the server, target included.
	 *
	 * Unlike Server_AttackNearest, the target here genuinely cannot be re-derived
	 * server-side - "nearest hostile" is objective, "whatever the camera is
	 * pointed at" is not. The server does not trust it as an identity, only as a
	 * nomination: ResolveOrQueueSkill and, beneath it,
	 * UDCCSkillComponent::CanActivate re-check range, allegiance and everything
	 * else before anything happens.
	 */
	UFUNCTION(Server, Reliable)
	void Server_CastSkill(FName SkillId, AActor* Target);

	UFUNCTION(Server, Reliable)
	void Server_TogglePause();

	UFUNCTION(Server, Reliable)
	void Server_StowCarried();

	UFUNCTION(Server, Reliable)
	void Server_EmptyStowedRemains(int32 Index);

	UFUNCTION(Server, Reliable)
	void Server_ConsumeStowedDrink(int32 Index);

	UFUNCTION(Server, Reliable)
	void Server_BeginClimb(ADCCClimbPoint* Point);

	/**
	 * Client's request to climb a generic surface with no authored
	 * ADCCClimbPoint. HitLocation/HitNormal are a nomination, not a fact: the
	 * server re-traces near HitLocation itself before trusting that a wall is
	 * really there, the same way Server_CastSkill re-validates a nominated
	 * target rather than trusting the client's aim.
	 */
	UFUNCTION(Server, Reliable)
	void Server_RequestGenericClimb(FVector HitLocation, FVector HitNormal);

	/** Routes a climb-failure message to the requesting client's HUD. */
	UFUNCTION(Client, Reliable)
	void Client_ShowClimbNotice(const FText& Message);

	UFUNCTION(Server, Reliable)
	void Server_SetClimbInput(bool bUp, bool bDown);

	UFUNCTION(Server, Reliable)
	void Server_ReleaseClimb();

	UFUNCTION(Server, Reliable)
	void Server_SetSprinting(bool bNewSprinting);

	UFUNCTION(Server, Reliable)
	void Server_ToggleMount();

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

	/**
	 * Server only. Called after a landed attack applies its damage.
	 *
	 * Empty here on purpose. It exists so on-hit behaviours - a Debt Eater biting
	 * gold loose and feeding it to its summoner - can be added on the subclass
	 * that needs them, rather than as properties every character drags around.
	 */
	virtual void OnDamageDealt(ADCCCharacterBase* Target, int32 Damage) {}

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

	UFUNCTION()
	void OnRep_Mounted();

	UFUNCTION()
	void OnRep_Casting();

	/** Clears bIsCasting when the cast timer elapses. Server only. */
	void EndCastLock();

	FTimerHandle CastLockTimer;

	/**
	 * Rolled on death, server side. Empty on players and allies, so only things
	 * meant to be looted ever drop anything.
	 *
	 * Lives on the base rather than on ADCCEnemyCharacter because death teardown
	 * is here, and a boss is not guaranteed to be an enemy subclass forever.
	 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "DCC|Loot")
	TArray<FDCCLootDrop> LootTable;

	/** Server only. Rolls every LootTable entry and spawns what hits. */
	/**
	 * Rolls this character's loot table once, at death, onto the body.
	 *
	 * Replaces the old DropLoot, which spawned pickups scattered on the floor the
	 * instant something died. Same rolls, same odds - the difference is that the
	 * result now waits on the corpse until somebody searches it.
	 */
	void RollLootOntoBody();

	/** Server only. Runs the climb and sprint stamina economy for one frame. */
	void TickStamina(float DeltaSeconds);

	/** Server-side: swing now, or park the intent if the world is paused. */
	void ResolveOrQueueAttack();

	/** Server-side: cast now, or park the intent if the world is paused. */
	void ResolveOrQueueSkill(FName SkillId, AActor* Target);

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
	 * The casting equivalent, and the reason it exists: ADCCEnemyCharacter plays
	 * CastAnimation itself before calling TryActivateSkill, but the *player's*
	 * cast path never did - so a crawler casting a skill was silent on every
	 * machine including their own, which reads as the skill not having fired.
	 *
	 * Held for HoldSeconds so a skill with a CastTimeSeconds root has an
	 * animation lasting as long as the character is actually stuck in place.
	 */
	UFUNCTION(NetMulticast, Unreliable)
	void Multicast_PlayCast(float HoldSeconds);

	/**
	 * Plays the flinch on every machine. Unreliable, like the other cosmetics -
	 * a dropped reaction costs a frame of feedback, not correctness.
	 */
	UFUNCTION(NetMulticast, Unreliable)
	void Multicast_PlayHitReaction();

	/**
	 * Server. Staggers this character away from a point and fires the reaction.
	 *
	 * The shove is applied here rather than in the multicast because movement is
	 * server-authoritative - launching on every machine independently would have
	 * five clients disagreeing about where the target ended up.
	 */
	void ReactToHitFrom(const FVector& ImpactSource);

	/**
	 * Server-side resolution of a chosen conversation option.
	 *
	 * Everything the menu can do lands here and is re-validated, because the menu
	 * is client-side: range, allegiance, and the option's own availability are
	 * all checked again rather than trusted.
	 */
	UFUNCTION(Server, Reliable)
	void Server_PerformInteraction(EDCCInteractionOption Option, ADCCCharacterBase* Target);

	/** The raw camera-forward sweep both FindInteractionTargetUnderCursor and FindLootTargetUnderCursor classify. */
	AActor* SweepForInteractionCandidate() const;

	/**
	 * Finds a living, non-hostile character under the crosshair within range.
	 *
	 * A corpse no longer qualifies - see FindLootTargetUnderCursor. Splitting
	 * the two is what let the bBody branch come out of BuildInteractionOptions
	 * entirely rather than growing a second special case next to Persuade.
	 */
	ADCCCharacterBase* FindInteractionTargetUnderCursor() const;

	/** Rebuilds the option list for the current target, with availability. */
	void BuildInteractionOptions();

	/** Finds a corpse or chest under the crosshair within range, or null. */
	AActor* FindLootTargetUnderCursor() const;

	/**
	 * Finds a non-combat speaker (bartender and the like) under the crosshair
	 * within range, or null.
	 *
	 * A separate check from FindInteractionTargetUnderCursor rather than a
	 * branch inside it - ADCCDialogueNPC is deliberately not an
	 * ADCCCharacterBase (see that class's own comment on why), so it needs its
	 * own cast, and its "conversation" is a single SayNextLine call rather than
	 * a whole BuildInteractionOptions menu.
	 */
	ADCCDialogueNPC* FindDialogueNPCUnderCursor() const;

	/** Confirms the highlighted row of whichever loot menu page is open. */
	void HandleLootMenuConfirm();

	/**
	 * Server-side resolution of the loot menu's three verbs.
	 *
	 * Same shape as Server_PerformInteraction: the menu is client-side, so
	 * range and existence are re-checked here rather than trusted. Three
	 * separate RPCs rather than one with an enum payload because each takes a
	 * different argument shape (an index, nothing, nothing) and the enum
	 * would only be reused by this one caller.
	 */
	UFUNCTION(Server, Reliable)
	void Server_LootOne(AActor* Source, int32 Index);

	UFUNCTION(Server, Reliable)
	void Server_LootAll(AActor* Source);

	UFUNCTION(Server, Reliable)
	void Server_StowLootSource(AActor* Source);

	/** Confirms the highlighted row of the vendor menu. */
	void HandleVendorMenuConfirm();

	/**
	 * Server-side resolution of the vendor menu's two verbs - buying a piece
	 * of equipment or a drink. Same "client picks, server re-validates" shape
	 * as the loot RPCs above: re-checks range, gold, and that VendorActor
	 * actually sells the thing being bought.
	 */
	UFUNCTION(Server, Reliable)
	void Server_PurchaseEquipment(AActor* VendorActor, FName ItemId);

	/** bDrinkNow: apply effects immediately. False: add to StowedConsumables instead. */
	UFUNCTION(Server, Reliable)
	void Server_PurchaseDrink(AActor* VendorActor, FName ConsumableItemId, bool bDrinkNow);

	/** Translates one consumable's Effects into calls on this character's own Stats. Server only. */
	void ApplyConsumableEffects(const TArray<FDCCConsumableEffect>& Effects);

	/** Runs one social check and narrates the result into the event feed. */
	void ResolveSocialCheck(ADCCCharacterBase* Target, bool bIntimidate);

	/** Whoever this crawler is talking to. Local - conversations are not shared. */
	UPROPERTY(Transient)
	TObjectPtr<ADCCCharacterBase> InteractionTarget;

	UPROPERTY(Transient)
	TArray<FDCCInteractionEntry> InteractionOptions;

	int32 InteractionIndex = 0;

	/** Local only - inspecting somebody's kit changes nothing about the world. */
	UPROPERTY(Transient)
	TObjectPtr<ADCCCharacterBase> InspectedCharacter;

	/** Armed by a first E press during combat, consumed by the second. */
	UPROPERTY(Transient)
	TObjectPtr<ADCCCharacterBase> PendingTalkTarget;

	double PendingTalkExpiry = 0.0;

	/** Whoever's loot menu this crawler has open. Local - same as InteractionTarget. */
	UPROPERTY(Transient)
	TObjectPtr<AActor> LootTargetActor;

	int32 LootMenuIndex = 0;

	/** False: top menu. True: the itemised Check Inventory submenu. */
	bool bLootShowingItems = false;

	/** Whichever vendor this crawler has the shop menu open on. Local - same as LootTargetActor. */
	UPROPERTY(Transient)
	TObjectPtr<AActor> VendorTargetActor;

	int32 VendorMenuIndex = 0;

	/** True once a drink row is picked and the Drink Now/Stow sub-prompt owns VendorMenuIndex instead. */
	bool bVendorConfirmingDrink = false;

	FName VendorPendingDrinkId;

	UFUNCTION()
	void OnRep_Downed();

	/** Shared teardown for going down and for dying outright. */
	void ApplyCollapsedBody();

	/** Undoes ApplyCollapsedBody. Only ever called on a downed crawler. */
	void RestoreStandingBody();

	/** Server only. One death-save roll. Stabilises, ends it, or just logs and waits for the next. */
	void RollDeathSave();

	/** Server only. Auto-stabilises at 1 HP after DeathSavesToStabilize successes - no rescuer needed. */
	void StabilizeAtOneHitPoint();

	/** Everything the full death teardown does, once revival is off the table. */
	void FinishDying();

	/**
	 * True when this clip was authored for the same skeleton this mesh uses.
	 *
	 * The engine's own failure here is an ensure plus a garbage pose, which is
	 * unreadable at a glance; refusing the clip leaves the character in its last
	 * good pose and prints which asset is on the wrong rig.
	 */
	bool CanPlayOnThisSkeleton(const UAnimationAsset* Animation) const;

	/** One warning per character, not one per frame. */
	mutable bool bWarnedSkeletonMismatch = false;

	/**
	 * Rolled at death and held until looted.
	 *
	 * Replicated - the itemised "Check Inventory" submenu has to read real
	 * item data on whichever machine is drawing it, not just the server. Before
	 * that submenu existed only bHasLoot needed to leave the server at all.
	 */
	UPROPERTY(Replicated)
	TArray<FDCCRolledLoot> PendingLoot;

	/** Replicated so a client can grey out "Search the body" without asking. */
	UPROPERTY(Replicated)
	bool bHasLoot = false;

	/** World seconds the last reaction played, for the cooldown above. */
	float LastHitReactionTime = -100.0f;

	FTimerHandle DeathSaveTimer;
	FTimerHandle ReviveTimer;

	/** Server-only concepts do not apply here - Selfie Mode is local, so this is not replicated. */
	bool bInSelfieMode = false;

	FTimerHandle SelfieShutterTimer;
	FTimerHandle SelfieExitTimer;

	/**
	 * Whatever other camera components were active before SelfieCamera took
	 * over, so ExitSelfieMode can hand the view back to exactly what was
	 * showing rather than guessing at the character Blueprint's own rig.
	 */
	UPROPERTY(Transient)
	TArray<TWeakObjectPtr<UCameraComponent>> DeactivatedCamerasForSelfie;

	void EnterSelfieMode();
	void ExitSelfieMode();

	/** The shutter: a quick flash, the actual screenshot request, then a short beat before ExitSelfieMode. */
	void TakeSelfieScreenshot();

	/**
	 * Mesh transform relative to the capsule, captured before anything ragdolls.
	 *
	 * Required to stand a revived crawler back up: once the mesh is simulating it
	 * has been detached, and reattaching without the original offset leaves them
	 * standing waist-deep in the floor.
	 */
	FTransform MeshRestTransform;

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

	/**
	 * Seconds the corpse stays before cleanup on the server. 0 keeps it forever.
	 *
	 * Zero on purpose: a body is a container now, and a container that evaporates
	 * on a timer turns "come back for the loot" into a trap.
	 */
	UPROPERTY(EditDefaultsOnly, Category = "DCC")
	float CorpseLifeSpan = 0.0f;

	/**
	 * On by default. A minority of imported skeletons turn out to have a
	 * physics asset whose auto-generated constraints fight each other badly
	 * enough that SetAllBodiesSimulatePhysics stretches the body into
	 * something grotesque rather than a believable fall (confirmed on the
	 * Debt Eater). Rather than hand-tune a Physics Asset in the Physics Asset
	 * Editor - which this project's enabled MCP toolset has no tool for at
	 * all - a character can just opt out here: ApplyCollapsedBody then freezes
	 * the mesh in its last pose (still fully lootable, still blocking the
	 * Visibility trace FindLootTargetUnderCursor needs) instead of simulating
	 * physics on it.
	 */
	UPROPERTY(EditDefaultsOnly, Category = "DCC")
	bool bUseRagdollOnDeath = true;

	/**
	 * Optional one-shot VFX played at the death location when
	 * bUseRagdollOnDeath is false, so "no ragdoll" still reads as a death beat
	 * rather than the character just going statue-still. Soft rather than a
	 * hard ConstructorHelpers load - this is cosmetic, not something anything
	 * else depends on existing.
	 */
	UPROPERTY(EditDefaultsOnly, Category = "DCC")
	TSoftObjectPtr<UNiagaraSystem> DeathDissolveEffect;

	/**
	 * Seconds an emptied body sticks around after being looted.
	 *
	 * The pair of these is the whole corpse-cleanup policy: a body with something
	 * on it waits indefinitely, and starts counting down only once it has nothing
	 * left to give. That keeps loot safe from a timer while still stopping a long
	 * session from accumulating every corpse it ever made.
	 */
	UPROPERTY(EditDefaultsOnly, Category = "DCC", meta = (ClampMin = "0.0"))
	float LootedCorpseLifeSpan = 25.0f;

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

	/**
	 * Replicated so clients can query IsDead() reliably (the Whispers/Allies
	 * chat tabs need to know), not because anything here reacts to the OnRep -
	 * FinishDying's own locally-triggered ApplyCollapsedBody/PlayCharacterSound
	 * calls are untouched, on purpose: that path is already tested, and this
	 * flag only needs to be *readable*, not to drive anything itself.
	 */
	UPROPERTY(Replicated)
	bool bHasDied = false;

	/**
	 * A dead crawler gets exactly one more chat send (any channel but Feed,
	 * which was never sendable anyway), then loses the ability to send again -
	 * see Server_SendChatMessage. Replicated so the local HUD can grey out the
	 * input box once it's used, rather than only finding out via a refused RPC.
	 */
	UPROPERTY(Replicated)
	bool bHasSentFinalMessage = false;
};
