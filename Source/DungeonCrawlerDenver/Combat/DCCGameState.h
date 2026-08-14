#pragma once

#include "CoreMinimal.h"
#include "GameFramework/GameStateBase.h"
#include "DCCGameState.generated.h"

class ADCCCharacterBase;
class ADCCGameState;

/**
 * One character's intent, parked until the world resumes.
 *
 * Holds the *request*, never a result. Everything about whether the action lands
 * is decided at resolve time on the server, so a queued action cannot be used to
 * pre-compute or lock in an outcome during the pause.
 */
USTRUCT(BlueprintType)
struct FDCCQueuedAction
{
	GENERATED_BODY()

	UPROPERTY(BlueprintReadOnly, Category = "Combat")
	TObjectPtr<ADCCCharacterBase> Instigator;

	/** None means the basic attack rather than a skill. */
	UPROPERTY(BlueprintReadOnly, Category = "Combat")
	FName SkillId;

	UPROPERTY(BlueprintReadOnly, Category = "Combat")
	TObjectPtr<AActor> Target;

	/**
	 * The action's cooldown, copied at queue time.
	 *
	 * This is the design doc's initiative value: resolution is ordered by it, so
	 * quick actions land before slow ones and unpausing does not degenerate into
	 * "everyone acts at once, fastest clicker wins".
	 */
	UPROPERTY(BlueprintReadOnly, Category = "Combat")
	float InitiativeCost = 0.0f;
};

DECLARE_DYNAMIC_MULTICAST_DELEGATE_TwoParams(FDCCCombatPauseChanged, ADCCGameState*, GameState, bool, bPaused);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FDCCActionsResolved, int32, NumResolved);

/**
 * Match-wide combat state: the real-time-with-pause loop.
 *
 * The world runs live by default. Any player may pause; while paused, every
 * player queues one action for their own character. On resume, queued actions
 * resolve in initiative order and the world continues live.
 *
 * Lives on the GameState because that is what GameState is for - state every
 * client needs to agree on - and because it gives a single authoritative place
 * to arbitrate a pause that any of five players can trigger.
 *
 * Requires the GameMode's GameStateClass to be set to this class.
 */
UCLASS()
class DUNGEONCRAWLERDENVER_API ADCCGameState : public AGameStateBase
{
	GENERATED_BODY()

public:
	ADCCGameState();

	virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;

	UFUNCTION(BlueprintPure, Category = "Combat")
	bool IsCombatPaused() const { return bCombatPaused; }

	/**
	 * Server only. Pausing is intentionally not restricted to one player - the
	 * design doc gives the whole table a global hotkey, so the first request
	 * wins and the rest are harmless no-ops.
	 */
	UFUNCTION(BlueprintCallable, Category = "Combat")
	void SetCombatPaused(bool bNewPaused);

	/**
	 * Server only. Records a character's intent. One action per character:
	 * queueing again replaces the previous choice, so a player can change their
	 * mind freely while the world is frozen.
	 */
	// Parameters are named Character, not Instigator: AActor already declares an
	// Instigator property and UHT refuses to let a UFUNCTION parameter shadow it.
	UFUNCTION(BlueprintCallable, Category = "Combat")
	bool QueueAction(ADCCCharacterBase* Character, FName SkillId, AActor* Target);

	UFUNCTION(BlueprintCallable, Category = "Combat")
	bool ClearQueuedAction(ADCCCharacterBase* Character);

	UFUNCTION(BlueprintPure, Category = "Combat")
	bool HasQueuedAction(const ADCCCharacterBase* Character) const;

	UFUNCTION(BlueprintPure, Category = "Combat")
	const TArray<FDCCQueuedAction>& GetQueuedActions() const { return QueuedActions; }

	UPROPERTY(BlueprintAssignable, Category = "Combat")
	FDCCCombatPauseChanged OnCombatPauseChanged;

	UPROPERTY(BlueprintAssignable, Category = "Combat")
	FDCCActionsResolved OnActionsResolved;

	/**
	 * How far time is slowed while paused. Not zero by default: a hard stop
	 * looks like a crash and upsets systems that divide by delta time, whereas a
	 * crawl reads unmistakably as "the game is waiting for you".
	 */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Combat", meta = (ClampMin = "0.0", ClampMax = "1.0"))
	float PausedTimeDilation = 0.03f;

	/** Set false to gate behaviour on IsCombatPaused alone without slowing time. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Combat")
	bool bSlowTimeWhilePaused = true;

protected:
	UPROPERTY(ReplicatedUsing = OnRep_CombatPaused, BlueprintReadOnly, Category = "Combat")
	bool bCombatPaused = false;

	/** Replicated so every client's HUD can show what the party intends to do. */
	UPROPERTY(Replicated, BlueprintReadOnly, Category = "Combat")
	TArray<FDCCQueuedAction> QueuedActions;

	UFUNCTION()
	void OnRep_CombatPaused();

	/** Server only. Runs every queued action in initiative order, then clears. */
	void ResolveQueuedActions();

	void ApplyTimeDilation();

	UFUNCTION(BlueprintImplementableEvent, Category = "Combat", meta = (DisplayName = "On Combat Pause Changed"))
	void BP_OnCombatPauseChanged(bool bPaused);
};
