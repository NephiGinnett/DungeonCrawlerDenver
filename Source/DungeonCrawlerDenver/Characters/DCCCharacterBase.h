#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Character.h"
#include "Stats/DCCAbilityTypes.h"
#include "Items/DCCLootTypes.h"
#include "Items/DCCLootSource.h"
#include "Items/DCCConsumableTypes.h"
#include "Items/DCCLootBoxTypes.h"
#include "UI/DCCInteractionTypes.h"
#include "Dialogue/DCCDialogueTypes.h"
#include "UI/DCCChatTypes.h"
#include "Skills/DCCSkillTypes.h"
#include "Stats/DCCDamageTypes.h"
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
class UDCCOnboardingComponent;
class UDCCWalletComponent;
struct FDCCEquipmentDef;
class ADCCCharacterBase;
class ADCCDialogueNPC;
class ADCCDialogueNPC_RaceBookie;
class ADCCLootBoxStation;
class ADCCPortalConsole;
class ADCCOathgate;
class ADCCCliffLift;
class ADCCEnemyCharacter;
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
 * Server only. A hit that actually applied damage: melee, ranged, spells,
 * thrown props and explosives. The amount is what the caller passed in,
 * before the target's resistances. Static for the same reason as
 * OnAnyAttackAttempted, so a meter does not have to know every fighter.
 */
DECLARE_MULTICAST_DELEGATE_ThreeParams(FDCCDamageLanded, ADCCCharacterBase* /*Dealer*/, ADCCCharacterBase* /*Target*/, int32 /*Amount*/);

/**
 * Common ground for anything that fights: players and enemies both derive from
 * this, so an encounter never has to care which side of the fight it is looking
 * at. That symmetry is deliberate - the DM client inspects and overrides both
 * through the same component.
 *
 * Server-authoritative. PerformAttack refuses to run on a client; clients see
 * the outcome through replicated hit points and the cosmetic multicast.
 */
/**
 * One slot on the belt (2026-11-16, Zoe: "I feel we have 1000 different keys and could better consolidate a few into
 * a selected item + click rather than a dedicated key"). The number keys pick a slot. A skill is armed as before; a
 * thing you aim (a lightstick, the spray can, a throw) is armed and the next left click uses it; anything else (a
 * tool, food, the lamp, the body cam, the grappling hook) is used the moment its number is pressed. On a wall your
 * hands are busy, so every slot works straight from its number there. Built fresh each time it is asked for, from
 * what this crawler has right now in this game. Local.
 */
enum class EDCCBelt : uint8 { Skill, Grapple, Tool, Quick, Spray, Lightstick, Lamp, BodyCam, Throw, Stow, HotMic };

struct FDCCBeltEntry
{
	EDCCBelt Kind = EDCCBelt::Skill;
	/** The skill, the tool or the consumable this slot is for (none for the rest). */
	FName ItemId;
	FString Name;
	/** How many are left, or -1 when that does not apply. */
	int32 Count = -1;
	/** Seconds until it can be used again (a skill, the hook). */
	float Cooldown = 0.0f;
	/** A switch that is on (the lamp, the body cam). */
	bool bOn = false;
	/** Armed and then used with a click, rather than used on the key. */
	bool bAimed = false;
	/** The food slot: how many different foods and drinks you have (the middle mouse button picks among them). */
	int32 Choices = 0;
};

UCLASS(Abstract)
class DUNGEONCRAWLERDENVER_API ADCCCharacterBase : public ACharacter, public IDCCLootSource
{
	GENERATED_BODY()

public:
	ADCCCharacterBase();

	virtual void Tick(float DeltaSeconds) override;

	// --- The Integrity Office (Session/DCCIntegrity, 2026-11-22) ---------------------------------------------------
	/** Strikes this visit. 1 or 2 puts UNDER REVIEW on the nameplate, 3 or more CHEATER. Server writes, all see it. */
	UPROPERTY(ReplicatedUsing = OnRep_CheatStrikes, BlueprintReadOnly, Category = "DCC|Integrity")
	uint8 CheatStrikes = 0;
	UFUNCTION()
	void OnRep_CheatStrikes();
	/** Server: set the count and redraw the nameplate here. */
	void NoteCheatStrikes(int32 Count);
	/** A guest's game: what its own screen shows, every few seconds (DCCIntegrity::CheckReport). */
	UFUNCTION(Server, Unreliable)
	void Server_IntegrityReport(const TArray<int32>& Seen);
	/** The host's own crawler (or a solo game): the only one the DM's buttons answer to. */
	bool IsHostsOwnCrawler() const { return HasAuthority() && IsLocallyControlled(); }
	float IntegrityReportIn = 5.0f;

	/** A hit with its dealer, for everyone's damage numbers (colour and damage skin, UI/DCCDamageSkins). Cosmetic. */
	UFUNCTION(NetMulticast, Unreliable)
	void Multicast_DamageNumber(int32 Amount, ADCCCharacterBase* Dealer);

	/** The Hot Mic (UDCCWalletComponent): bFired false is going live (feedback whine), true is the amplified spell going
	 *  off (the holder's clip, or an air horn). Every machine plays it at this crawler. */
	UFUNCTION(NetMulticast, Reliable)
	void Multicast_HotMic(bool bFired);
	/** Redraw the nameplate (a title arrived). */
	void RefreshNameplateNow() { RefreshNameplate(); }

	/**
	 * Server only. Where a character learns it belongs to a person.
	 *
	 * The toughness bonus is applied here rather than authored on the Blueprint
	 * because the same Blueprint can be both: BP_DCCEnemy_HockeyWings is CJ as an
	 * AI ally today and CJ as a player's crawler the moment he joins.
	 */
	virtual void PossessedBy(AController* NewController) override;
	/** Network fix (2026-11-16): a locally built floor or prop is never a movement base. See the .cpp. */
	virtual void SetBase(UPrimitiveComponent* NewBase, const FName BoneName = NAME_None, bool bNotifyActor = true) override;
	virtual void SetBase(FMovementBaseInterfaceData* MovementBaseInterfaceData, const FName BoneName = NAME_None, bool bNotifyActor = true) override;
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;

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

	/** Who this crawler is (CJ, Scott...), their onboarding stage, and which HUD parts they have been issued. See Onboarding/DCCOnboardingComponent.h. */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "DCC")
	TObjectPtr<UDCCOnboardingComponent> Onboarding;

	/** Arcade tickets, owned cosmetics and the outfit drawn on this crawler. See Arcade/DCCWalletComponent.h. */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "DCC")
	TObjectPtr<UDCCWalletComponent> Wallet;

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
	 * WeaponDamageDie, or the best equipped weapon's die (rarity bonus
	 * included) if any exceeds it. Same "equipment wins when it says
	 * anything" shape as GetEffectiveAttackRange, and the actual fix for
	 * "a frying pan should hit harder than a bare fist" - PerformAttack was
	 * reading the raw WeaponDamageDie member directly and ignoring equipped
	 * gear entirely until this existed.
	 */
	UFUNCTION(BlueprintPure, Category = "DCC|Combat")
	int32 GetEffectiveWeaponDamageDie() const;

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

	/** See FDCCDamageLanded. Server-side only. */
	static FDCCDamageLanded OnAnyDamageLanded;

	/** Server only. Call after ApplyDamage with the amount that was passed in. Misses do not call this. */
	void NoteDamageDealtTo(ADCCCharacterBase* Target, int32 Amount);

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

	/**
	 * The struck character stops dead for this long before the flinch (2026-11-16, Zoe, after Elyra's Tales' hit-stop:
	 * "a small stutter / pausing the defender"). Only the one who was hit pauses, on every screen; nobody else's game
	 * does, which is what a whole-game freeze would do to a five-player round. Zero turns it off.
	 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "DCC|Animation", meta = (ClampMin = "0.0"))
	float HitStutterSeconds = 0.08f;
	FTimerHandle HitStutterTimer;

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

	/** Plays EnterVehicleAnimation on every machine - called from
	 *  ADCCRover::EnterRover, which needs this public since it's a
	 *  cross-class call, not an internal one like Multicast_PlayEmote. */
	UFUNCTION(NetMulticast, Reliable)
	void Multicast_PlayEnterVehicleAnimation();
	void Multicast_PlayEnterVehicleAnimation_Implementation();

	// -------------------------------------------------------------------------
	// EMOTE MENU - player-triggered, not the random kill-celebration above
	//
	// EmoteAnimations/PlayEmote already existed for the kill-celebration path
	// (DCCCrawlerAnimSet::Apply curates a real pool - dances, gestures - for
	// anything on the shared crawler skeleton) but nothing let a PLAYER choose
	// and fire one on demand. This reuses that same array and the same
	// PlayOneShotAnimation/Multicast plumbing, just with an explicit index
	// instead of a random one, and a lightweight picker to choose it - same
	// wheel-to-choose/E-to-confirm shape as every other menu in this file.
	// -------------------------------------------------------------------------

	UFUNCTION(BlueprintPure, Category = "DCC|Animation")
	bool IsEmoteMenuOpen() const { return bEmoteMenuOpen; }

	UFUNCTION(BlueprintPure, Category = "DCC|Animation")
	int32 GetEmoteMenuIndex() const { return EmoteMenuIndex; }

	/** Opens/closes the picker. No-ops open if EmoteAnimations is empty - a
	 *  character with nothing to perform has nothing to browse. */
	void ToggleEmoteMenu();

	/** Moves the highlight. Wraps. */
	void CycleEmoteMenu(int32 Delta);

	/** E on the open picker: asks the server to play the highlighted emote,
	 *  then closes the menu locally (same "client asks, server re-validates"
	 *  shape as everywhere else - the index is a nomination, not a command). */
	void ConfirmEmote();

	/** Client asks; server re-checks the index is still valid before honouring
	 *  it, same reasoning as every other Server_ RPC in this file. */
	UFUNCTION(Server, Reliable)
	void Server_RequestEmoteAtIndex(int32 Index);
	void Server_RequestEmoteAtIndex_Implementation(int32 Index);

	/** Plays EmoteAnimations[Index] on every machine and logs the same Feed
	 *  line the random kill-celebration path already uses. */
	UFUNCTION(NetMulticast, Reliable)
	void Multicast_PlayEmoteAtIndex(int32 Index);
	void Multicast_PlayEmoteAtIndex_Implementation(int32 Index);

	/**
	 * Reactions (Characters/DCCReactions, 2026-11-16): a bubble over the head, often with a move. Server only. Automatic
	 * ones (bAuto) wait out a short cooldown, so a busy fight does not become a storm of bubbles; ones a player picks,
	 * and the big moments (going down, a revive, the summit), go straight through.
	 */
	void React(uint8 Reaction, bool bAuto = true);
	/** A moment (DCCReactions::EMoment) happened: react the way this player chose for it (SETTINGS tab). Server. */
	void ReactTo(uint8 Moment, bool bAuto = true);
	UFUNCTION(Server, Reliable)
	void Server_RequestReaction(uint8 Reaction);
	/** This player's choice of bubble and move for each moment, from their own PC. */
	UFUNCTION(Server, Reliable)
	void Server_SetReactionChoices(const TArray<uint8>& Choices);
	UFUNCTION(NetMulticast, Unreliable)
	void Multicast_React(uint8 Reaction, uint8 Move);
	TArray<uint8> ReactionChoices;
	bool bReactionChoicesSent = false;
	uint8 GetReaction() const { return ActiveReaction; }
	/** Right mouse held on a ranged weapon. Local. Left click fires only while this is true. */
	bool IsAiming() const { return bIsAiming; }
	float GetReactionStart() const { return ReactionStartTime; }
	uint8 ActiveReaction = 0;
	float ReactionStartTime = -100.0f;
	float NextAutoReactionTime = 0.0f;
	float NextPickedReactionTime = 0.0f;
	bool bOutOfBreath = false;

	/** Played once per swing. Without this, attacks are invisible. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "DCC|Animation")
	TObjectPtr<UAnimationAsset> AttackAnimation;

	/** Played while ascending a climb point, time-scaled to the ascent. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "DCC|Animation")
	TObjectPtr<UAnimationAsset> ClimbAnimation;

	/**
	 * Played once, standing beside the rover, before the crawler hides and
	 * possession hands over to it - the beat that represents getting in,
	 * per Zoe's ask 2026-09-03 ("a quick animation... then disappear"). No
	 * literal "get in a car" clip exists in the shared crawler set, so this
	 * is a placeholder repurposing whatever reads closest - see
	 * DCCCrawlerAnimSet::Apply for which clip that is right now.
	 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "DCC|Animation")
	TObjectPtr<UAnimationAsset> EnterVehicleAnimation;

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
	 * Which DCCRoom this character is currently standing in, per the server's
	 * existing occupancy tracking (ADCCRoom::AddOccupant/RemoveOccupant) - not a
	 * second room-tracking system, just that one's result mirrored onto the
	 * character so the HUD can read it without touching ADCCRoom itself.
	 * RoomId is kept alongside the display name purely so RemoveOccupant can
	 * tell "the room I'm leaving" from "the room I'm still in" when two room
	 * volumes briefly overlap - it is not otherwise meant to be read directly.
	 */
	UPROPERTY(Replicated, BlueprintReadOnly, Category = "DCC|Room")
	FText CurrentRoomDisplayName;

	UPROPERTY(BlueprintReadOnly, Category = "DCC|Room")
	FName CurrentRoomId;

	/**
	 * Mirrored the same way CurrentRoomDisplayName is, the moment this
	 * character enters or leaves a room - true only while standing in one
	 * whose RoomType is SafeRoom. The one thing loot boxes gate on
	 * (Server_OpenLootBox) rather than something a client-side check could
	 * lie about.
	 */
	UPROPERTY(Replicated, BlueprintReadOnly, Category = "DCC|Room")
	bool bInSafeRoom = false;

	UFUNCTION(BlueprintPure, Category = "DCC|Room")
	bool IsInSafeRoom() const { return bInSafeRoom; }

	/**
	 * Server only. Called by ADCCRoom::AddOccupant on every room entry.
	 * No-ops on a room already visited - VisitedRoomIds only ever grows, and
	 * the Achievements Record call only fires on the entries that actually
	 * grow it, which is what makes "The Full Tour" a plain threshold
	 * achievement (Threshold = however many rooms a floor has) rather than a
	 * bespoke "visited all N" check anywhere in code.
	 */
	void RecordRoomVisited(FName RoomId);

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

	/** Shown above the head. Defaults to the actor label when left empty.
	 *  Replicated (2026-11-05) so a server-side rename - a solo trial's rolled
	 *  'Kombucha Mother' - reaches clients' nameplates. Set before FinishSpawning
	 *  and it arrives with the first bunch, ahead of the client's BeginPlay. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Replicated, Category = "DCC|Allegiance")
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

	/**
	 * True when this clip was authored for the same skeleton this mesh uses.
	 *
	 * The engine's own failure here is an ensure plus a garbage pose, which is
	 * unreadable at a glance; refusing the clip leaves the character in its last
	 * good pose and prints which asset is on the wrong rig.
	 *
	 * Public rather than protected: callers that need to know WHETHER a clip
	 * would play - not just have PlayOneShotAnimation silently no-op it - are
	 * cross-class. ADCCRover::EnterRover is the reason this moved; locking the
	 * driver for an animation's duration when that animation was never going
	 * to render at all is its own bug, not a cosmetic one.
	 */
	bool CanPlayOnThisSkeleton(const UAnimationAsset* Animation) const;

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
	// Routed to whichever menu is actually open. With none open, and with the
	// interaction list empty, the wheel steps the belt's aimed slots (see
	// HandleScroll). An armed skill is left alone.
	void InteractionScrollUp() { HandleScroll(-1); }
	void InteractionScrollDown() { HandleScroll(1); }
	void HandleScroll(int32 Delta);

	UFUNCTION(BlueprintCallable, Category = "DCC|Interaction")
	void CloseInteraction();

	/** Talk, loot, vendor, emote, and the interaction list. Not the Tab sheet, and not a casino seat. */
	bool HasSideMenu() const;
	void CloseSideMenus();
	void LeaveCasinoSeat();

	// -------------------------------------------------------------------------
	// NPC MENU - "E on a non-combat NPC" (ADCCDialogueNPC and its ADCCVendorNPC
	// subclass), not layered onto InteractionTarget above for the same reason
	// the loot menu is not: InteractionTarget is typed ADCCCharacterBase*, and
	// ADCCDialogueNPC is deliberately NOT one (see that class's own header
	// comment on why). A separate small menu, same wheel-to-choose/E-to-confirm
	// shape and the same visual panel style as every other menu in this file.
	// -------------------------------------------------------------------------

	/** The NPC currently showing its Talk/Shop/Leave menu, or null. */
	UFUNCTION(BlueprintPure, Category = "DCC|Interaction")
	ADCCDialogueNPC* GetNPCInteractionTarget() const { return NPCInteractionTarget; }

	/** True when the crosshair is on a speaker, so E opens their menu instead of a casino seat. */
	bool IsDialogueNPCUnderCursor() const { return FindDialogueNPCUnderCursor() != nullptr; }

	UFUNCTION(BlueprintPure, Category = "DCC|Interaction")
	const TArray<EDCCNPCInteractionOption>& GetNPCInteractionOptions() const { return NPCInteractionOptions; }

	UFUNCTION(BlueprintPure, Category = "DCC|Interaction")
	int32 GetNPCInteractionIndex() const { return NPCInteractionIndex; }

	/** False: the top menu (Talk/Shop/Quests/Leave). True: the offered-quests sub-page. */
	UFUNCTION(BlueprintPure, Category = "DCC|Interaction")
	bool IsNPCShowingQuests() const { return bNPCShowingQuests; }

	/** Casino Cascade's drink list, on the same menu. Three drinks, then Back. */
	bool IsCasinoShopOpen() const { return bCasinoShopOpen; }

	/**
	 * NPCInteractionTarget's OfferedQuestIds, filtered down to ones not
	 * already active or complete for the party - what the Quests sub-page
	 * actually lists, and what decides whether the Quests row shows at all.
	 */
	UFUNCTION(BlueprintCallable, Category = "DCC|Interaction")
	void GetOfferableQuestIds(TArray<FName>& OutIds) const;

	/** Moves the highlight. Wraps. */
	UFUNCTION(BlueprintCallable, Category = "DCC|Interaction")
	void CycleNPCInteractionMenu(int32 Delta);

	UFUNCTION(BlueprintCallable, Category = "DCC|Interaction")
	void CloseNPCInteraction();

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

	// -------------------------------------------------------------------------
	// AWAITING OTHER CRAWLERS
	//
	// Set by walking into an active ADCCExitPortal - the science lab's "end of
	// the built content" exit. A terminal state like death, not a menu: the
	// HUD takes the whole screen over (ADCCHUD::DrawAwaitingOtherCrawlersScreen,
	// checked ahead of even the death screen) rather than layering a banner on
	// top of normal play, because there genuinely is nowhere further to go
	// yet - this is the honest way to say so instead of quietly doing nothing.
	// -------------------------------------------------------------------------

	UPROPERTY(BlueprintReadOnly, ReplicatedUsing = OnRep_AwaitingOtherCrawlers, Category = "DCC|Portal")
	bool bAwaitingOtherCrawlers = false;

	UFUNCTION(BlueprintPure, Category = "DCC|Portal")
	bool IsAwaitingOtherCrawlers() const { return bAwaitingOtherCrawlers; }

	/** Server only. Called by ADCCExitPortal when a living crawler walks into an active one. */
	void EnterAwaitingOtherCrawlers();

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

	/**
	 * Server only. Rolls Chatter lines - see ADCCGameState::RollChatterBatch
	 * for why this is client-driven rather than a background timer.
	 *
	 * Called twice by DCCHUD: once with the opening burst when the Chatter tab
	 * is opened, then repeatedly with a Count of 1-2 while it stays open, which
	 * is what makes the channel look like it has been running the whole time
	 * without anything running while nobody is watching.
	 *
	 * Unreliable on purpose: this fires every couple of seconds per viewer and
	 * a dropped one costs a comment nobody knew was coming. The server
	 * rate-limits regardless (ADCCGameState::TryRollChatterBatch), so a client
	 * asking too fast gets refused rather than obeyed.
	 */
	UFUNCTION(Server, Unreliable)
	void Server_RequestChatterBatch(int32 Count);

	/**
	 * Client's request to ride a specific ADCCCliffLift, e.g. via
	 * ADCCDialogueNPC_LiftAttendant. Public for the same reason
	 * Server_SendChatMessage is - the dialogue NPC calling it is not a member
	 * of this class. The lift itself only ever mutates its replicated state
	 * from server code (ADCCCliffLift::RequestRaise already no-ops without
	 * authority), so this exists purely to get the request from whichever
	 * client pressed E onto the server at all - same "nominate, don't dictate"
	 * shape as Server_RequestGenericClimb, re-checking distance rather than
	 * trusting the client was actually close enough.
	 */
	UFUNCTION(Server, Reliable)
	void Server_RequestCliffLift(ADCCCliffLift* Lift);

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

	/** Set by Server_BleedOut: the saves that are left roll back to back. Cleared when the crawler goes down again. */
	bool bBleedingOutFast = false;

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
	 * The downed crawler chose not to wait. The next death-save is a failure
	 * right now, and the usual ending follows: a tale-floor rewrite if one is
	 * left, otherwise the body stays down. Space, or the button on the down screen.
	 */
	UFUNCTION(Server, Reliable)
	void Server_BleedOut();

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

	/**
	 * Server only. Stands a downed crawler back up with no rescuer, no log line and
	 * no Favor. For the System doing it (the tale floor's narrator rewrite, the
	 * stairs carrying a downed crawler down): the caller says what happened.
	 */
	void ReviveQuietly(float HealthFraction);

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

	/**
	 * Server only. Called once per encounter, on every crawler in the room,
	 * the instant its fight goes Active - see ADCCRoom::HandleEncounterStateChanged.
	 * Checks equipped gear for a cursed item that forces an action at combat
	 * start (Supercan's costume is the first) and, if one is worn, runs it: a
	 * swept forced hop plus an optional gesture animation and feed line. A
	 * no-op for everyone not wearing something cursed, which in practice is
	 * everyone.
	 */
	void NotifyCombatStarted();

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

	// -------------------------------------------------------------------------
	// OATHGATE - "Enter the gate?" confirm prompt
	//
	// Walking into the trigger no longer teleports on contact - it arms this
	// small local prompt instead, same wheel-to-choose/E-to-confirm shape as
	// the loot and vendor menus. Server tells this specific client to show it
	// (Client_ShowOathgatePrompt, fired from ADCCOathgate::HandleTriggerOverlap)
	// rather than the client opening it unprompted, because eligibility (alive,
	// crawler, off cooldown) is the server's call, not something to fake open
	// locally. Confirming "Enter" round-trips through Server_ActivateOathgate,
	// which re-validates before ADCCOathgate::RequestActivation actually moves
	// anyone - the usual "client picks, server re-validates" shape.
	// -------------------------------------------------------------------------

	/** The Oathgate currently showing its confirm prompt on this client, or null. */
	UFUNCTION(BlueprintPure, Category = "DCC|Oathgate")
	class ADCCOathgate* GetOathgateTarget() const { return OathgateTargetActor; }

	UFUNCTION(BlueprintPure, Category = "DCC|Oathgate")
	int32 GetOathgateMenuIndex() const { return OathgateMenuIndex; }

	/** Moves the highlight between Enter/Turn Away. Wraps. */
	UFUNCTION(BlueprintCallable, Category = "DCC|Oathgate")
	void CycleOathgateMenu(int32 Delta);

	UFUNCTION(BlueprintCallable, Category = "DCC|Oathgate")
	void CloseOathgatePrompt();

	/** Confirms whichever row is highlighted - Enter teleports, Turn Away just closes the prompt. */
	UFUNCTION(BlueprintCallable, Category = "DCC|Oathgate")
	void HandleOathgateMenuConfirm();

	/** Server -> this client only. Gate null closes the prompt (used when the player walks back out without deciding). */
	UFUNCTION(Client, Reliable)
	void Client_ShowOathgatePrompt(class ADCCOathgate* Gate);

	UFUNCTION(Server, Reliable)
	void Server_ActivateOathgate(class ADCCOathgate* Gate);
	void Server_ActivateOathgate_Implementation(class ADCCOathgate* Gate);

	/** Same "client nominates, server acts" shape - Console re-validates nothing itself, it just calls Portal->Activate(). */
	UFUNCTION(Server, Reliable)
	void Server_ActivatePortalConsole(ADCCPortalConsole* Console);
	void Server_ActivatePortalConsole_Implementation(ADCCPortalConsole* Console);

	/** Crowbar request on a Puzzles/ door. Server re-checks range, then lets the
	 *  door itself decide (Favor cost, Strength gate) - see ADCCPuzzleDoor. */
	UFUNCTION(Server, Reliable)
	void Server_ForcePuzzleDoor(class ADCCPuzzleDoor* Door);
	void Server_ForcePuzzleDoor_Implementation(class ADCCPuzzleDoor* Door);

	/** Flip one breaker in a Lights-Out panel. Range-checked server-side. */
	UFUNCTION(Server, Reliable)
	void Server_FlipBreakerSwitch(class ADCCBreakerSwitch* Switch);
	void Server_FlipBreakerSwitch_Implementation(class ADCCBreakerSwitch* Switch);

	/** Hand the carried prop over as collateral. The lock decides whether it
	 *  qualifies; this only re-checks range. */
	UFUNCTION(Server, Reliable)
	void Server_DepositToEscrow(class ADCCEscrowLock* Lock);
	void Server_DepositToEscrow_Implementation(class ADCCEscrowLock* Lock);

	/** Get into a rover. Server re-checks range, then the rover does the
	 *  possess - see Vehicles/DCCRover.h. */
	UFUNCTION(Server, Reliable)
	void Server_EnterRover(class ADCCRover* Rover);
	void Server_EnterRover_Implementation(class ADCCRover* Rover);

	/** Get out again. No range check: the driver is inside it by definition. */
	UFUNCTION(Server, Reliable)
	void Server_ExitRover(class ADCCRover* Rover);
	void Server_ExitRover_Implementation(class ADCCRover* Rover);

	/** Turf the ally passengers out. No range check - the driver is inside it. */
	UFUNCTION(Server, Reliable)
	void Server_EjectRoverPassengers(class ADCCRover* Rover);
	void Server_EjectRoverPassengers_Implementation(class ADCCRover* Rover);

	/** Roll the rover's windows up/down. No range check - the driver is inside
	 *  it by definition. See ADCCRover::ToggleWindows for what this actually
	 *  does to the mesh (a material fade, not a moving piece of glass). */
	UFUNCTION(Server, Reliable)
	void Server_ToggleRoverWindows(class ADCCRover* Rover);
	void Server_ToggleRoverWindows_Implementation(class ADCCRover* Rover);

	/** Toggle the rover's radio. No range check - the driver is inside it by
	 *  definition, same reasoning as Server_ToggleRoverWindows. */
	UFUNCTION(Server, Reliable)
	void Server_ToggleRoverRadio(class ADCCRover* Rover);
	void Server_ToggleRoverRadio_Implementation(class ADCCRover* Rover);

	/** Next KMHR track, while the driver is in the rover and the station is on. */
	UFUNCTION(Server, Reliable)
	void Server_CycleRoverRadio(class ADCCRover* Rover);
	void Server_CycleRoverRadio_Implementation(class ADCCRover* Rover);

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
	 * Equips Items[Index] from this crawler's own pack, if it is a piece of
	 * gear - the Tab menu's Inventory page calls this on a clicked row. Same
	 * routing reason as DCCEmptyStowedRemains: UDCCInventoryComponent::EquipItem
	 * refuses outright without authority.
	 */
	UFUNCTION(BlueprintCallable, Category = "DCC|Loot")
	void DCCEquipItem(int32 Index);

	/** Takes one worn slot off. The Character tab's UNEQUIP button calls this. Same routing as DCCEquipItem. */
	UFUNCTION(BlueprintCallable, Category = "DCC|Loot")
	void DCCUnequipSlot(uint8 Slot);

	/** Public so the menu can ask. The belt only reads the front of the skill list. */
	void DCCPutSkillOnBelt(int32 Index);

	/** Dev. Plays the attack clip for whatever is in the main hand. */
	void DebugSwing();

	/** Any player, from the menu. Takes the whole party back to the Safe Room. */
	UFUNCTION(Server, Reliable)
	void Server_BailToLobby();

	/**
	 * Drinks one stowed consumable from this crawler's own pack - the Tab
	 * menu's Inventory page calls this on a clicked row. Same routing reason
	 * as DCCEmptyStowedRemains: UDCCInventoryComponent::ConsumeStowedConsumable
	 * refuses outright without authority.
	 */
	UFUNCTION(BlueprintCallable, Category = "DCC|Vendor")
	void DCCConsumeStowedDrink(int32 Index);

	/**
	 * Opens one owned loot box of Tier from the Inventory page. Routed
	 * through the server for the same reason DCCConsumeStowedDrink is - the
	 * safe-room check has to be re-verified there too, not trusted from
	 * whatever the client's HUD happened to show.
	 */
	UFUNCTION(BlueprintCallable, Category = "DCC|LootBox")
	void OpenLootBox(EDCCLootBoxTier Tier);

	/**
	 * Claims every unclaimed achievement reward, then opens every owned loot
	 * box across every tier, one at a time, laid out in a line rather than
	 * piled on one spot - Zoe's own request, so a whole session's backlog of
	 * rewards reads as a line of loot instead of a heap. Claiming is
	 * unconditional, matching the existing per-achievement Claim button; the
	 * box-opening half re-verifies IsInSafeRoom() server-side exactly like
	 * OpenLootBox does, and simply skips (with a local notice) if that fails
	 * - a call made outside a Safe Room still banks the XP/Favor, it just
	 * leaves the boxes sealed for later.
	 */
	UFUNCTION(BlueprintCallable, Category = "DCC|LootBox")
	void ClaimAndOpenAllLootBoxes();

	/**
	 * The loot box screen (O). Asks the server to open Count boxes of Tier, or
	 * every box owned when Tier is None. Gear goes into the bag, gold into the
	 * purse, boons apply at once, and the result comes back through
	 * Client_LootBoxReveal for the screen to show.
	 */
	void RequestOpenLootBoxes(EDCCLootBoxTier Tier, int32 Count);

	// --- Weapon skills and explosives (2026-11-13) -------------------------
	/** The equipped main-hand item's row, if any. */
	bool GetMainHandDef(FDCCEquipmentDef& Out) const;

	/** Marks a known skill for advancement, or records an untrained attempt (how new skills are learned). */
	void NoteSkillUse(FName SkillId, int32 Rank);

	/** Server. Handling check, throw, scatter on a miss, then Detonate where it lands. */
	FDCCAttackResult ThrowExplosive(ADCCCharacterBase* Target, const FDCCEquipmentDef& Def);

	/** Server. Damages everyone in Def.SplashRadius of Where (thrower and party included), half at the edge. */
	void Detonate(const FVector& Where, const FDCCEquipmentDef& Def, int32 Damage);

	UFUNCTION(NetMulticast, Unreliable)
	void Multicast_Explosion(FVector Where, float Radius, bool bFire);

	static int32 RollD20ForSkill(bool bDisadvantage)
	{
		const int32 First = FMath::RandRange(1, 20);
		return bDisadvantage ? FMath::Min(First, FMath::RandRange(1, 20)) : First;
	}

	/** O. Opens or closes the loot box screen. Local only. */
	void ToggleLootBoxScreen();

	/** F3. Opens or closes the gear bench. Local only. */
	void ToggleGearBench();

	/**
	 * Boxes open in a Safe Room, or anywhere nothing hostile is close. The
	 * second half is what lets them open on a World Dungeon floor, which has no
	 * Safe Room, once the room is clear.
	 */
	bool CanOpenLootBoxesHere(FText& OutWhyNot) const;

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

	/**
	 * Poses cyclable with the mouse wheel while posing (2026-08-28) - see
	 * CycleSelfiePose. Authored per-character/Blueprint, since it has to be
	 * something that skeleton can actually play; empty means scrolling does
	 * nothing and the character just holds whatever it was already doing.
	 * The shared-skeleton animation set (A_DCC_*) is the obvious source once
	 * someone picks a handful worth offering.
	 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "DCC|Selfie")
	TArray<TObjectPtr<UAnimationAsset>> SelfiePoseAnimations;

	/**
	 * How close a living hostile can be before EnterSelfieMode refuses outright
	 * (2026-08-28) - see EnterSelfieMode's own comment on why this exists
	 * alongside, not instead of, SelfieMaxRealSeconds.
	 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "DCC|Selfie", meta = (ClampMin = "0.0"))
	float SelfieHostileRefuseRadius = 1500.0f;

	/** Advances/retreats through SelfiePoseAnimations and plays the newly selected
	 *  one, restarting the pose-hold so the shutter captures it rather than
	 *  catching the swap mid-transition. Routed here from InteractionScrollUp/Down
	 *  while posing - see those functions' own dispatch. */
	UFUNCTION(BlueprintCallable, Category = "DCC|Selfie")
	void CycleSelfiePose(int32 Delta);

	/** 0-1 progress toward the shutter actually firing - the HUD's viewfinder
	 *  overlay reads this for a countdown bar. Zero whenever not posing. */
	UFUNCTION(BlueprintPure, Category = "DCC|Selfie")
	float GetSelfiePoseProgress() const;

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

	/** F2. Opens the party menu (host, invite, join). Local only. */
	void ToggleSessionMenu();

	/** Server. Spawns one of anything at the caller's feet. */
	UFUNCTION(Server, Reliable)
	void Server_AdminSpawn(TSubclassOf<AActor> ActorClass, bool bAsAlly);

	/** Server. Kills every living hostile in the level. */
	UFUNCTION(Server, Reliable)
	void Server_AdminClearHostiles();

	/**
	 * Server. Starts or resets whichever ADCCGauntletEncounter is nearest to
	 * the caller - lets a DM force a gauntlet from the admin panel without
	 * physically standing in its start volume, the same convenience
	 * Server_AdminSpawn gives placed encounters.
	 */
	UFUNCTION(Server, Reliable)
	void Server_AdminGauntlet(bool bReset);

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

	// --- Hold-to-climb (2026-11-13, Zoe's design) ------------------------------
	// 1. Equip the Climbing skill (belt). 2. Hold the left mouse button: you grab
	// the wall in front of you and climb (also when you walk into a wall while
	// holding). 3. Let go of the button, or run out of stamina, and you drop.
	// Right mouse, held, climbs down. Without Climbing equipped a wall is never
	// grabbed, so a fight near a wall never turns into a climb.

	/** Climbing is the equipped skill. */
	bool IsClimbingEquipped() const { return SelectedSkillId == FName(TEXT("Climbing")); }

	/** Server: drop off the wall now (a rock hit, a gust). */
	void ForceReleaseClimb();

	/** Local, for the automated climb test (DCC.ClimbTest): as if the left mouse were held or released. */
	void DebugSetClimbHold(bool bHeld) { bClimbHeld = bHeld; bDescendHeld = false; ClimbGrabRetry = 0.0f; }

	/** The belt grappling hook (Fourteener). The hook slot calls this. */
	void ThrowGrapple();

	/** Server: move this climber somewhere at once, on every machine including their own (a grappling hook). */
	void SnapTo(const FVector& Location);

	UFUNCTION(Client, Reliable)
	void Client_SnapTo(FVector Location);

	/** Server: a short notice on this player's own screen (hazard warnings, checkpoints). */
	void NotifyClimber(const FText& Message) { Client_ShowClimbNotice(Message); }

	/**
	 * Server-side multipliers the arena (or anything else) sets each tick: on the
	 * cost of climbing (ice x2, a gust x3, roped in x0.6) and on resting stamina
	 * regeneration (a camp x2). Reset to 1 when nothing is setting them.
	 */
	float ClimbStaminaMultiplier = 1.0f;

	/**
	 * Arcade fairness (2026-11-14). When > 0, climbing stamina is measured against this "reference bar"
	 * instead of the crawler's own: costs and recovery scale by MaxStamina / this, and the up-climb is charged
	 * per metre rather than per second. So a levelled DCD crawler (bar 360) and a fresh one (bar 120) both get
	 * about 1.3 pitches per bar. Fourteener sets it (ADCCArcadeClimb::TickKits); 0 everywhere else.
	 */
	float ClimbStaminaReferenceMax = 0.0f;
	/** Set while climbing: sideways is slow (a fraction of the climb speed), not the 6 m/s flying default. */
	bool bClimbFlyTuned = false;

	/**
	 * A short screen shake for this machine's own player (2026-11-13: a rock
	 * landing nearby). Zero-sum wobble on the control rotation, so the view
	 * comes back exactly where it was. Local only; call it on the client.
	 */
	void AddScreenShake(float Strength, float Seconds);

	/**
	 * Hotbar slots 2 and 3 (2026-11-13, Zoe): they start empty and fill with the
	 * first useful things picked up - a Fourteener tool, a consumable - in the
	 * order they arrive. The key uses it; a slot whose item runs out empties
	 * again for the next one. Each entry is an item id: a DCCClimbItems id or a
	 * consumable's ItemId. Owner-only.
	 */
	UPROPERTY(Replicated)
	TArray<FName> QuickSlots;

	/** Server: put a newly picked-up useful item into the first empty quick slot. */
	void OfferQuickItem(FName ItemId);
	/** How many of this quick item the crawler still has (tools from the wallet mirror, consumables from the pack). */
	int32 QuickItemCount(FName ItemId) const;
	void UseQuickSlot(int32 Index);

	UFUNCTION(Server, Reliable)
	void Server_UseQuickSlot(int32 Index);

	// --- The belt (see FDCCBeltEntry) ---------------------------------------------------------------------------
	/** What is on this crawler's belt right now, slot 1 first. At most 9 slots (the number keys). */
	void GetBelt(TArray<FDCCBeltEntry>& Out) const;
	/** A number key: arm, use or put away the slot at Index (0-based). */
	void BeltKey(int32 Index);
	/** (dev) a left click, for automated checks (DCC.Click). */
	void DevClick() { OnPrimaryPressed(); OnPrimaryReleased(); }
	/** (dev) the scroll wheel. Positive is wheel-down, the same direction InteractionScrollDown uses. */
	void DevWheel(int32 Delta) { HandleScroll(Delta); }
	/** Step through aimed belt slots: nothing armed, then each aimed slot, then nothing again. */
	void CycleBeltAim(int32 Delta);
	/** The aimed slot waiting for a click, if any. */
	bool IsBeltArmed(const FDCCBeltEntry& Entry) const { return ArmedBeltKind == int32(Entry.Kind) && ArmedBeltItem == Entry.ItemId; }
	bool HasBeltArmed() const { return ArmedBeltKind >= 0; }
	/** The name of the armed slot, for the HUD's hint. */
	FString GetArmedBeltName() const;
	/** Put the armed slot away. */
	void DisarmBelt() { ArmedBeltKind = -1; ArmedBeltItem = NAME_None; }
	/** In one of the arcade games (not the Dungeon Crawler Denver runs). */
	bool IsInArcade() const;
	/** Use an entry from GetBelt now. */
	void UseBeltEntry(const FDCCBeltEntry& Entry);
	/**
	 * Food and drink share one belt slot (2026-11-17, Zoe: "make food sit on one spot and make it so you can press a
	 * button ... to cycle through what you have"). These are the ones you have, in a fixed order; the slot shows and
	 * eats SelectedFood. The middle mouse button (CycleFood) moves on to the next.
	 */
	void GetFoods(TArray<FName>& Out) const;
	void CycleFood();
	/** Where the food slot's food is in GetFoods (0 when nothing has been picked yet). */
	int32 GetFoodIndex() const { TArray<FName> Foods; GetFoods(Foods); return FMath::Max(0, Foods.IndexOfByKey(SelectedFood)); }
	mutable FName SelectedFood;
	/** Where the picked food was, so when the last one is eaten the slot moves to the next food, not the first. */
	mutable int32 SelectedFoodAt = 0;

	/** Server: use a Fourteener tool or a consumable by id (a belt slot). */
	UFUNCTION(Server, Reliable)
	void Server_UseBeltItem(FName ItemId);
	float StaminaRegenMultiplier = 1.0f;

	/** Seconds continuously on the ground. Stamina does not come back during a stall or a fall. Server only. */
	float StaminaGroundedFor = 0.0f;

	/** Server: no fall damage until this world time (roped in, a respawn). */
	double FallSafeUntil = 0.0;
	/** A teleport is not a fall (2026-11-23: PRIMETIME's trip from the studio to the mine counted as 300 m). */
	void ForgetFall(double SafeSeconds = 2.0);

	/** Falls up to this many metres are free; past it, damage grows until FatalFallMetres. */
	UPROPERTY(EditDefaultsOnly, Category = "DCC|Climb", meta = (ClampMin = "0.0"))
	float SafeFallMetres = 6.0f;

	UPROPERTY(EditDefaultsOnly, Category = "DCC|Climb", meta = (ClampMin = "1.0"))
	float FatalFallMetres = 20.0f;

	virtual void Landed(const FHitResult& Hit) override;

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

	/**
	 * Local only - a camera FOV nudge, not a gameplay state, so nothing here
	 * needs to replicate. Gated on a ranged weapon being equipped (see
	 * OnSecondaryPressed) - right-click has nothing to zoom in on for melee
	 * or bare hands, so it stays a no-op for them exactly as it always was.
	 */
	void BeginAimZoom();
	void EndAimZoom();

	/** True while right-click is held on a ranged weapon. Local UI/camera state only. */
	UPROPERTY(Transient)
	bool bIsAiming = false;

	/** FOV captured the moment aiming began, so EndAimZoom restores exactly what was there rather than a hardcoded default. */
	float PreAimFOV = 90.0f;

	/** How far in "slight zoom" goes - 1.0 would be no zoom at all. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "DCC|Combat", meta = (ClampMin = "0.1", ClampMax = "1.0"))
	float AimZoomFactor = 0.7f;

	void ReleaseClimbIfClimbing();

	void AcceptPrompt() { ConfirmPendingClimb(true); }
	void DeclinePrompt() { ConfirmPendingClimb(false); }

	void StartSprint() { SetSprintInput(true); }
	void StopSprint() { SetSprintInput(false); }

	/** Arms the Nth known skill. One handler per key; BindKey carries no payload. */
	void SelectSkillSlot(int32 Index);
	// Hotbar (2026-11-13): 1 = your first skill, 2 and 3 = quick items, 4 and 5 = your next two skills.
	// The belt's number keys (2026-11-16): one handler per key, BindKey carries no payload.
	void BeltKey1() { BeltKey(0); }
	void BeltKey2() { BeltKey(1); }
	void BeltKey3() { BeltKey(2); }
	void BeltKey4() { BeltKey(3); }
	void BeltKey5() { BeltKey(4); }
	void BeltKey6() { BeltKey(5); }
	void BeltKey7() { BeltKey(6); }
	void BeltKey8() { BeltKey(7); }
	void BeltKey9() { BeltKey(8); }
public:
	/** The B menu's camera rows, between the reactions and the moves: the photo pose, the selfie camera. */
	static constexpr int32 EmoteCameraRows = 2;
protected:
	int32 ArmedBeltKind = -1;
	FName ArmedBeltItem;
	/** P and M belong to the dungeon runs; the arcade games ignore them. */
	void PausePressed() { if (!IsInArcade()) { RequestTogglePause(); } }
	void MountPressed() { if (!IsInArcade()) { ToggleMount(); } }

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

	/**
	 * Safe to call anywhere, same shape as AttemptAttackNearest - but targets
	 * whatever the camera is pointed at (out to the equipped weapon's range)
	 * rather than the nearest hostile in a radius. What a ranged weapon's
	 * MainHand click calls instead of AttemptAttackNearest - see
	 * OnPrimaryPressed. Trace happens locally (this client's camera is the
	 * only accurate one) and the result is sent as a nomination, not a fact -
	 * Server_RangedAttack re-validates it the same way Server_CastSkill does.
	 */
	UFUNCTION(BlueprintCallable, Category = "DCC|Combat")
	void AttemptRangedAttack();

	/** Any player may pause. Routes to the server; safe to call from a client. */
	UFUNCTION(BlueprintCallable, Category = "DCC|Combat")
	void RequestTogglePause();

	/** Nearest living hostile within attack range, or null. */
	UFUNCTION(BlueprintPure, Category = "DCC|Combat")
	ADCCCharacterBase* FindNearestHostileInRange() const;

	/**
	 * The first living hostile the camera is looking at, out to MaxRange, or
	 * null. Local only - this is what AttemptRangedAttack traces with; the
	 * server never calls this itself (see Server_RangedAttack's comment).
	 */
	UFUNCTION(BlueprintPure, Category = "DCC|Combat")
	ADCCCharacterBase* FindHostileUnderCrosshair(float MaxRange) const;

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

	/**
	 * Same trust shape as Server_CastSkill, for the same reason: "whatever the
	 * camera is pointed at" cannot be re-derived server-side, so the client's
	 * FindHostileUnderCrosshair result is carried across as a nomination.
	 * ResolveOrQueueRangedAttack re-checks alive/hostile/range before it
	 * becomes an actual PerformAttack call.
	 */
	UFUNCTION(Server, Reliable)
	void Server_RangedAttack(ADCCCharacterBase* NominatedTarget);

	UFUNCTION(Server, Reliable)
	void Server_TogglePause();

	UFUNCTION(Server, Reliable)
	void Server_StowCarried();

	UFUNCTION(Server, Reliable)
	void Server_EmptyStowedRemains(int32 Index);

	// Named EquipInventoryItem rather than EquipItem - Server_EquipItem(FName)
	// already exists above as the DM's `DCC.EquipItem` cheat, and UFUNCTIONs
	// cannot be overloaded by parameter type the way plain C++ functions can.
	UFUNCTION(Server, Reliable)
	void Server_EquipInventoryItem(int32 Index);

	UFUNCTION(Server, Reliable)
	void Server_UnequipSlot(uint8 Slot);

	/** Move a known skill to the front of the list. The belt only reads the front. */
	UFUNCTION(Server, Reliable)
	void Server_PutSkillOnBelt(int32 Index);

	UFUNCTION(Server, Reliable)
	void Server_ConsumeStowedDrink(int32 Index);

	UFUNCTION(Server, Reliable)
	void Server_OpenLootBox(EDCCLootBoxTier Tier);

	UFUNCTION(Server, Reliable)
	void Server_ClaimAndOpenAllLootBoxes();

	UFUNCTION(Server, Reliable)
	void Server_OpenLootBoxBatch(EDCCLootBoxTier Tier, int32 Count);

	UFUNCTION(Client, Reliable)
	void Client_LootBoxReveal(const FDCCLootBoxReveal& Reveal);
	void Server_ClaimAndOpenAllLootBoxes_Implementation();

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
	void OnRep_ActiveClimbPoint(ADCCClimbPoint* PreviousActiveClimbPoint);

	/**
	 * Snaps the owning client to a server-computed climb position it would
	 * otherwise never learn about.
	 *
	 * AActor::SetActorLocation on the server is NOT delivered to the pawn's
	 * own owning (autonomous proxy) client - ReplicatedMovement is
	 * COND_SimulatedOrPhysics, i.e. everyone EXCEPT the owner. Every other
	 * player watching a climb sees it fine through ordinary movement
	 * replication; the climber's own screen does not, and instead sits still
	 * until the normal movement-error correction eventually yanks it into
	 * place - which is what read as being "pushed back" at climb start and
	 * "dropped off the side" at the top. OnRep_ActiveClimbPoint closes the
	 * start-of-climb gap by recomputing the same attach point locally on
	 * every machine; this RPC closes the end-of-climb gap, since that
	 * location is only known once, server-side, at the moment a climb ends -
	 * either by reaching the top (bLandOnGround = true, switches the client
	 * to MOVE_Walking) or by an early release / stamina exhaustion mid-climb
	 * (bLandOnGround = false, switches the client to MOVE_Falling from the
	 * server's real position rather than the client's own drifted cosmetic
	 * guess). See DISPATCH.md, "Climbing fixes" (2026-08-28) and the
	 * mid-climb-release follow-up (2026-08-29).
	 */
	UFUNCTION(Client, Reliable)
	void Client_SyncClimbLocation(FVector NewLocation, bool bLandOnGround);

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
	 * Last chance to change a blow before it lands, on the character RECEIVING
	 * it. Server-side; returns the damage actually to be applied.
	 *
	 * Sits on the attack path only (ADCCCharacterBase::PerformAttack), NOT
	 * inside UDCCStatsComponent::ApplyDamage - deliberately, and for the same
	 * reason UDCCBossComponent's damage multiplier is applied at the same
	 * spot: Stats/ has to stay ruleset-agnostic and know nothing about
	 * particular monsters.
	 *
	 * The useful consequence is that damage arriving by other routes - a
	 * thrown crate (ADCCPickup), a radiation aura, a scripted kill - does not
	 * pass through here at all. That is what lets a subclass be immune to
	 * being hit while still being destructible by something heavy, which is
	 * exactly what ADCCEnemyCharacter_LegacySystem is built on.
	 */
	virtual int32 ModifyIncomingDamage(int32 Damage, ADCCCharacterBase* Attacker);

	/**
	 * Logs the level and records it against achievements.
	 *
	 * Public because XP arrives from several places - kills here, clear bonuses
	 * from the encounter - and all of them should announce it identically.
	 */
	UFUNCTION(BlueprintCallable, Category = "DCC|Experience")
	void AnnounceLevelUp(int32 PreviousLevel);

	/**
	 * Level-up window (2026-11-22, Zoe: "isekai style pop up boxes"). Server: every level gained, from any source,
	 * lands here; a burst of levels (a boss and a contract in the same second) becomes one System window that says
	 * what changed: health, the stat point, the spell the class just taught, and what is next.
	 */
	UFUNCTION()
	void HandleLevelUp(UDCCStatsComponent* LevelledStats, int32 NewLevel);

	/** Dev (DCC.Cast): cast a skill the way the belt does, server side. */
	void DevCastSkill(FName SkillId, AActor* Target) { ResolveOrQueueSkill(SkillId, Target); }

private:
	void FlushLevelUp();
	int32 PendingLevelFrom = 0;
	int32 PendingMaxHpFrom = 0;
	FTimerHandle LevelUpTimer;

public:

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

	/** Server-side. The main-hand swing, with nobody to hit. No roll, no damage, no skill mark. */
	void PracticeSwing();

	/** Server-side: cast now, or park the intent if the world is paused. */
	void ResolveOrQueueSkill(FName SkillId, AActor* Target);

	/**
	 * Server-side: re-validates NominatedTarget (alive, hostile, in range -
	 * never trusted outright) then fires now, or parks the intent if the
	 * world is paused. A target that fails validation still plays the swing,
	 * with no roll and no damage.
	 */
	void ResolveOrQueueRangedAttack(ADCCCharacterBase* NominatedTarget);

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
	void Multicast_PlayCast(float HoldSeconds, FName SkillId);

	/**
	 * Plays the flinch on every machine. Unreliable, like the other cosmetics -
	 * a dropped reaction costs a frame of feedback, not correctness.
	 */
	UFUNCTION(NetMulticast, Unreliable)
	void Multicast_PlayHitReaction(EDCCDamageType DamageType);

	/**
	 * Server. Staggers this character away from a point and fires the reaction.
	 *
	 * The shove is applied here rather than in the multicast because movement is
	 * server-authoritative - launching on every machine independently would have
	 * five clients disagreeing about where the target ended up.
	 */
	void ReactToHitFrom(const FVector& ImpactSource, EDCCDamageType DamageType = EDCCDamageType::Physical);

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

	/**
	 * Same shape as FindDialogueNPCUnderCursor, one class down - a loot box
	 * station is not an ADCCCharacterBase either and has an even smaller
	 * interaction (open, or explain there's nothing to open), so it gets its
	 * own tiny check rather than folding into the dialogue or loot branches.
	 */
	ADCCLootBoxStation* FindLootBoxStationUnderCursor() const;

	/** Same shape again, one class further down - the science lab's portal switch. */
	class ADCCPortalConsole* FindPortalConsoleUnderCursor() const;

	/** Rebuilds NPCInteractionOptions for whichever NPC NPCInteractionTarget is. */
	void BuildNPCInteractionOptions();

	/** Confirms the highlighted row of the NPC menu. */
	void HandleNPCInteractionConfirm();

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

	// --- The shop screen (2026-11-13) ---------------------------------------
	// Buying puts gear in the bag (not straight on, as the old menu did) and
	// potions in the pack. Selling pays half the item's value. Both re-check
	// the vendor, the range, the stock and the gold on the server.
	UFUNCTION(Server, Reliable)
	void Server_ShopBuy(AActor* VendorActor, FName ItemId, bool bConsumable);

	UFUNCTION(Server, Reliable)
	void Server_ShopSell(AActor* VendorActor, int32 BagIndex);

public:
	/** What the shop pays for the bag item at Index: half its value, at least 1. */
	int32 GetShopSellPrice(int32 BagIndex) const;

	/** Asks the server; safe to call on any machine. */
	void ShopBuy(AActor* VendorActor, FName ItemId, bool bConsumable);
	void ShopSell(AActor* VendorActor, int32 BagIndex);

	/**
	 * Both the NPC and the id are re-checked here - NPC->OfferedQuestIds must
	 * actually contain QuestId and NPC must be in range - same "client picks,
	 * server re-validates" shape as the loot/vendor RPCs above. The quest
	 * component itself lives on GameState (party-wide, not per-character),
	 * reached through UDCCQuestComponent::Get.
	 */
	UFUNCTION(Server, Reliable)
	void Server_AcceptQuestFromNPC(ADCCDialogueNPC* NPC, FName QuestId);
	void Server_AcceptQuestFromNPC_Implementation(ADCCDialogueNPC* NPC, FName QuestId);

	/**
	 * Pays the bookie's stake and sends the field away. Range re-checked
	 * server-side exactly like the quest RPC above - the wager is real gold,
	 * so a client asking from across the map must not be taken at its word.
	 */
	UFUNCTION(Server, Reliable)
	void Server_RequestRaceBet(ADCCDialogueNPC_RaceBookie* Bookie);
	void Server_RequestRaceBet_Implementation(ADCCDialogueNPC_RaceBookie* Bookie);

	/**
	 * Casino Cascade. The client names the verb. The server checks the seat.
	 * 1 sit at a table (Arg is the employee's post). 2 the highlighted row.
	 * 3 stand up. 4 buy a drink (Arg is the drink).
	 */
	UFUNCTION(Server, Reliable)
	void Server_CasinoAction(uint8 Action, int32 Arg);
	void Server_CasinoAction_Implementation(uint8 Action, int32 Arg);

	/** E on an arcade mark (a tower, a podium, a pad). The arena checks distance again. */
	UFUNCTION(Server, Reliable)
	void Server_ArenaUse();
	void Server_ArenaUse_Implementation();

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

	/** Whichever NPC has its Talk/Shop/Leave menu open on this client. Local - same as InteractionTarget. */
	UPROPERTY(Transient)
	TObjectPtr<ADCCDialogueNPC> NPCInteractionTarget;

	UPROPERTY(Transient)
	TArray<EDCCNPCInteractionOption> NPCInteractionOptions;

	int32 NPCInteractionIndex = 0;

	/** False: top menu. True: the offered-quests submenu owns NPCInteractionIndex instead. */
	bool bNPCShowingQuests = false;

	/** Casino staff shop: three drinks and Back, using NPCInteractionIndex. */
	bool bCasinoShopOpen = false;

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

	/** Whichever Oathgate has its "Enter the gate?" prompt open on this client. Local - set by Client_ShowOathgatePrompt only. */
	UPROPERTY(Transient)
	TObjectPtr<class ADCCOathgate> OathgateTargetActor;

	/** Row 0 = Enter, row 1 = Turn Away. */
	int32 OathgateMenuIndex = 0;

	UFUNCTION()
	void OnRep_Downed();

	UFUNCTION()
	void OnRep_AwaitingOtherCrawlers();

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

	/**
	 * Server only. Set true the moment LootOne, LootAll, or StowInto is called,
	 * regardless of whether loot remained afterward - "someone dealt with this
	 * body" is a different question from "is it empty now", and the body
	 * collector cares about the former. Never reset - a corpse only dies once.
	 */
	bool bLootHandled = false;

	/** Started in FinishDying; fires CheckForBodyCollector once, then is done. */
	FTimerHandle BodyCollectorTimer;

	/**
	 * Every RoomId this crawler has ever entered, server-side only - not
	 * replicated, since only the server needs it (RecordRoomVisited's own
	 * Achievements call). Never shrinks; a room only needs to be counted once.
	 */
	TArray<FName> VisitedRoomIds;

	/**
	 * Server only, called BodyCollectorDelaySeconds after death. Spawns
	 * BodyCollectorClass and hands it this corpse, unless something already
	 * handled it (bLootHandled) or no collector class is configured.
	 */
	void CheckForBodyCollector();

	/** World seconds the last reaction played, for the cooldown above. */
	float LastHitReactionTime = -100.0f;

	FTimerHandle DeathSaveTimer;
	FTimerHandle ReviveTimer;
	FTimerHandle LootBoxRevealTimer;

	/** Sealed-box props standing in the reveal line, cleared away the moment
	 *  LootBoxRevealTimer resolves - see ClaimAndOpenAllLootBoxes. */
	UPROPERTY(Transient)
	TArray<TObjectPtr<AActor>> LootBoxRevealProps;

	/** Server-only concepts do not apply here - Selfie Mode is local, so this is not replicated. */
	bool bInSelfieMode = false;

	/** Local to this client, same reasoning as bInSelfieMode - the menu
	 *  itself is never anyone else's business, only the RPC it fires. */
	bool bEmoteMenuOpen = false;
	int32 EmoteMenuIndex = 0;

	FTimerHandle SelfieShutterTimer;
	FTimerHandle SelfieExitTimer;

	/**
	 * Wall-clock (GetWorld()->GetRealTimeSeconds(), NOT dilated) timestamp Selfie
	 * Mode was last entered - -1 means not currently in it. Exists solely so Tick
	 * can force an exit if SelfieShutterTimer/SelfieExitTimer haven't fired within
	 * SelfieMaxRealSeconds of real time.
	 *
	 * Found the hard way (2026-08-26): this project's own combat-pause system
	 * (ADCCGameState::SetCombatPaused) sets GLOBAL WorldSettings::TimeDilation to
	 * PausedTimeDilation (0.03 by default) while paused, and every FTimerManager
	 * timer - including SelfieShutterTimer/SelfieExitTimer above - slows down
	 * right along with it, because that is just how UWorld's timer manager works;
	 * there is no "ignore dilation" flag on a plain SetTimer call. Pausing combat
	 * during or right after taking a selfie stretches the normal ~1.2s pose+flash
	 * hold to roughly 40 real seconds, which reads exactly like "the camera is
	 * stuck on my own face and will not let go" - Zoe hit this live. Pressing `C`
	 * again always escapes it instantly (ToggleSelfieMode clears both timers
	 * unconditionally, so it does not depend on them ever firing), but nothing
	 * previously forced an exit if the player didn't know that. This real-time
	 * ceiling is the backstop: whatever the reason a Selfie timer stalls -
	 * combat pause, or anything else that might slow or jam world time in the
	 * future - the camera is guaranteed back within SelfieMaxRealSeconds of
	 * actual wall-clock time, full stop.
	 */
	double SelfieModeEnteredRealTimeSeconds = -1.0;

	/**
	 * See SelfieModeEnteredRealTimeSeconds. Generous margin above the normal
	 * ~1.2s pose+flash hold, so this never fires in ordinary play - it only
	 * matters when something has stalled the normal exit path.
	 *
	 * Raised from 5.0 to 8.0 (2026-08-28) alongside adding pose cycling and the
	 * nearby-hostile refusal above: posing is now a real browse-then-commit
	 * interaction rather than an instant snap, so it earns a little more real
	 * time - and it's safe to give it that room precisely because the refusal
	 * check now keeps this whole feature out of active danger in the first
	 * place, which is the actual risk a longer ceiling would otherwise reopen.
	 */
	UPROPERTY(EditDefaultsOnly, Category = "DCC|Selfie", meta = (ClampMin = "1.0"))
	float SelfieMaxRealSeconds = 8.0f;

	/** Which entry of SelfiePoseAnimations is currently active - see CycleSelfiePose. */
	int32 CurrentSelfiePoseIndex = 0;

	/**
	 * Whether any living hostile is within Range of this character - reused by
	 * EnterSelfieMode's nearby-danger refusal (2026-08-28). Not a general combat
	 * utility; kept narrow and private to that one use rather than promoted to
	 * a public API nothing else has asked for yet.
	 */
	bool HasNearbyLivingHostile(float Range) const;

	/**
	 * Whatever other camera components were active before SelfieCamera took
	 * over, so ExitSelfieMode can hand the view back to exactly what was
	 * showing rather than guessing at the character Blueprint's own rig.
	 */
	UPROPERTY(Transient)
	TArray<TWeakObjectPtr<UCameraComponent>> DeactivatedCamerasForSelfie;

	void EnterSelfieMode();

	/**
	 * Unconditional by design - never add a hostile/range/any-other gate here.
	 * EnterSelfieMode is where danger gets refused; once in, getting back out
	 * must always work no matter what. Confirmed 2026-08-29 while chasing a
	 * "spawned in in selfie mode and can't get out" report: this function
	 * already had no such gate, so the actual fix was Tick's new proactive
	 * force-exit (see Tick's own comment) rather than anything here - keep it
	 * that way.
	 */
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

	// -------------------------------------------------------------------------
	// BODY COLLECTOR
	//
	// Dungeon Crawler Carl's floor mobs collect corpses (and repair structures)
	// overnight - this is the collection half only. A corpse nobody has taken
	// loot from, stowed, or emptied within BodyCollectorDelaySeconds gets a
	// minion sent for it: walk over, a short "checking" beat, then it carries
	// the body's loot to a designated collection chest (any placed
	// ADCCLootContainer tagged "BodyCollectorDestination" - see
	// ADCCEnemyCharacter::FindBodyCollectorDestination) and deposits it there
	// before retiring. Ambient/environmental, not combat-gated - runs during
	// combat pause too, since a paused fight is not a paused dungeon.
	//
	// Applies uniformly to players and enemies, same as the loot system itself
	// already does. An unlooted PLAYER corpse can be swept up by a collector
	// ~20s after death even if nobody ever reached it - today that only
	// otherwise happens once someone actively loots it empty
	// (LootedCorpseLifeSpan, above) - but unlike this feature's first pass,
	// nothing is actually lost: the gear is retrievable from the destination
	// chest, just delayed rather than immediately at hand. Still worth a
	// player knowing where the collection point is before this reaches real
	// play, and still worth deciding whether a slow/AFK player should really
	// have their loadout swept away mid-fight while they're still down.
	// -------------------------------------------------------------------------

	/**
	 * Who gets spawned to do the collecting. Left unset by default; if unset,
	 * CheckForBodyCollector falls back to BP_DCCEnemy - the bare, unthemed
	 * enemy Blueprint - specifically so a floor mob doing ambient
	 * corpse-collection never wears a named crawler's or a boss's own face.
	 * Override per-Blueprint if a real body-collector visual gets authored
	 * later. If that fallback Blueprint is ever renamed or removed, this
	 * whole system no-ops safely - see CheckForBodyCollector.
	 *
	 * DO NOT resolve that fallback with a constructor-time
	 * ConstructorHelpers::FClassFinder. An earlier version did exactly that
	 * and hung the editor on every single launch (2026-08-28 incident, see
	 * DISPATCH.md): this constructor runs for every ADCCCharacterBase
	 * subclass's CDO, including ADCCEnemyCharacter's own, and BP_DCCEnemy is
	 * itself parented to ADCCEnemyCharacter - so building that Blueprint's
	 * class required ADCCEnemyCharacter's CDO to already exist, which is
	 * exactly the CDO whose own construction was still on the call stack.
	 * The fallback is resolved lazily instead, on first actual use, well
	 * after every CDO in the module exists.
	 */
	UPROPERTY(EditDefaultsOnly, Category = "DCC|BodyCollector")
	TSubclassOf<ADCCEnemyCharacter> BodyCollectorClass;

	/** How long a corpse can sit untouched before a collector is sent for it. */
	UPROPERTY(EditDefaultsOnly, Category = "DCC|BodyCollector", meta = (ClampMin = "0.0"))
	float BodyCollectorDelaySeconds = 20.0f;

	/** How far from the corpse the collector spawns, so it visibly arrives rather than appearing on top of the body. */
	UPROPERTY(EditDefaultsOnly, Category = "DCC|BodyCollector", meta = (ClampMin = "0.0"))
	float BodyCollectorSpawnRadius = 600.0f;

public:
	/** The body was swapped (UDCCOnboardingComponent::ApplyCrawlerLook): forget which loop was playing so the next
	 *  tick starts it on the new mesh. Without this the cache still says "idle" and the new body stands in T-pose. */
	void RestartLoopAnimation() { ActiveLoopAnimation = nullptr; OneShotAnimationUntil = 0.0f; }

	/** Set by the gear bench (F3) while it plays animations by hand; locomotion leaves the mesh alone. */
	bool bBenchPosing = false;

	/**
	 * V: the body cam is rolling (Dead Air, Arcade/DCCArcadeHaunt). Replicated so
	 * everyone's HUD and the server's scoring agree on who is filming. Any game
	 * can read it; only Dead Air scores it today.
	 */
	UPROPERTY(Replicated, BlueprintReadOnly, Category = "DCC|Camera")
	bool bRecordingFootage = false;

	void ToggleRecording();

	/**
	 * The selfie cam (2026-11-16, Zoe: "a selfie facing camera for when they prefer"): X swings the camera round to
	 * look back at your face while you keep moving and filming, the way a vlogger holds a phone. X again puts it back.
	 * Unlike the C pose selfie, play does not stop. Local: only this player's view changes.
	 */
	void ToggleFacingCamera();
	bool IsFacingCamera() const { return bFacingCamera; }
	bool bFacingCamera = false;
	bool bFacingCameraWasControlRotation = true;
	void TickFacingCamera(float DeltaSeconds);
	/** Body cam zoom. Local FOV only, and only while the cam is rolling. */
	void NudgeBodyCamZoom(int32 Delta);
	void RestoreBodyCamZoom();
	float BodyCamBaseFOV = 90.0f;
	bool bBodyCamZoomed = false;

	UFUNCTION(Server, Reliable)
	void Server_SetRecording(bool bRecording);

	/** Whether any loot-menu action (LootOne, LootAll, or StowInto) has ever been taken on this corpse. */
	bool HasLootBeenHandled() const { return bLootHandled; }

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

	/** Local: the left mouse button is held with Climbing equipped. */
	bool bClimbHeld = false;
	bool bDescendHeld = false;
	float ClimbGrabRetry = 0.0f;

	/** Local: looks for a wall right in front and asks the server to grab it. Quiet: no notices. */
	bool TryGrabWallAhead();

	/** Local: a wall under the cursor, in reach, while Climbing is not equipped: say how to climb it. */
	bool HintEquipClimbingIfWall();

	void TickHoldToClimb(float DeltaSeconds);

	/**
	 * Local: while climbing, a wide shot from behind (2026-11-13). The boom pulls
	 * out and up, turns to face the wall, and stops doing collision tests so a
	 * ledge or a passing rock never snaps it into the climber's head or the rock.
	 * Everything eases back when the climb ends.
	 */
	void TickClimbCamera(float DeltaSeconds);
	float ClimbCamBlend = 0.0f;
	float BaseArmLength = -1.0f;
	FVector BaseSocketOffset = FVector::ZeroVector;
	bool bBaseCollisionTest = true;
	bool bBaseCameraLag = false;

	/**
	 * Where a climber should be this frame on the way to Target: the height at
	 * once, but sideways onto the wall at no more than 7 m/s. Grabbing used to
	 * snap the crawler onto the wall in one frame (81 cm from a step away, 191 cm
	 * from the back of High Camp), found by DCC.ClimbTest (2026-11-13).
	 */
	FVector ClimbStepToward(const FVector& Target, float DeltaSeconds) const;

	// --- Crawling over a ledge (2026-11-13) --------------------------------------
	// Climbing into the underside of a ledge used to teleport the climber onto its
	// top. Now they crawl it like an ant over a boulder: out along the underside
	// to the lip, up around the edge, and in onto the top. The path is worked out
	// on the server and sent to the climber's own machine, so both play it.

	/** Server: start crawling over the ledge above. False if there is no room on top. */
	bool BeginLedgeCrawl(const FHitResult& Underside);

	UFUNCTION(Client, Reliable)
	void Client_BeginLedgeCrawl(const TArray<FVector>& Path);

	/** Moves along CrawlPath. True while still crawling. On the server, finishing stands the climber on top. */
	bool TickLedgeCrawl(float DeltaSeconds);

	TArray<FVector> CrawlPath;

	// Screen shake (AddScreenShake) and the climbing tilt (TickClimbTilt): the body
	// leans with the surface as it crawls over a ledge, like an ant over a boulder.
	double ShakeStarted = 0.0, ShakeUntil = 0.0;
	float ShakeStrength = 0.0f;
	FRotator ShakeApplied = FRotator::ZeroRotator;
	void TickScreenShake();
	FQuat ClimbTilt = FQuat::Identity;
	FVector CrawlDirection = FVector::ZeroVector;
	FTransform BaseMeshRelative;
	bool bBaseMeshRelativeSet = false;
	void TickClimbTilt(float DeltaSeconds);

	/**
	 * The climb animation keeps pace with the climber (2026-11-13): its play rate
	 * is the real vertical speed over ClimbAnimNaturalSpeed, the speed it looks
	 * right at 1x. Hanging still pauses it, climbing down plays it backwards, a
	 * faster climber's arms move faster. Measured from position on every machine,
	 * so other players see the same pace.
	 */
	// Measured: A_DCC_ClimbLoop (the stair clip made seamless and in place, Tools/make_seamless_loop.py)
	// lifts the hips 85 cm per 2.9 s cycle in its original form, so 1x looks right at ~29 cm/s.
	static constexpr float ClimbAnimNaturalSpeed = 70.0f; // was 29: looked far faster than the climber moved (Zoe)
	float SmoothedClimbSpeed = 0.0f;
	float LastClimbAnimZ = 0.0f;
	void TickClimbAnimRate(float DeltaSeconds);
	/** This climber's speed on the wall (cm/s), from the ruleset; the ledge crawl moves at a multiple of it. */
	float GetClimbSpeedNow() const;
	float CrawlDistance = 0.0f;
	bool bCrawling = false;

	/** Server: the highest point of the current fall, for fall damage. */
	float FallPeakZ = 0.0f;

	/**
	 * Position-only cosmetic mirror of TickStamina's climbing branch, run on
	 * the owning client (never the server, never a simulated proxy watching
	 * someone else climb - see the call site in Tick()). No stamina is spent
	 * and no achievement/quest/log side effects fire here; those remain
	 * exclusively server-authoritative. This exists purely so the climbing
	 * player's own screen moves in real time with their own held input
	 * instead of sitting frozen until a movement-error correction catches it
	 * up to wherever the server has already gone - see
	 * Client_SyncClimbLocation's comment for the underlying replication gap.
	 */
	void TickClimbMovementCosmetic(float DeltaSeconds);

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
