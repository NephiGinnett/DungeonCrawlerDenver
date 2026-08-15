#pragma once

#include "CoreMinimal.h"
#include "GameFramework/GameStateBase.h"
#include "UI/DCCEventLogTypes.h"
#include "DCCGameState.generated.h"

class ADCCCharacterBase;
class ADCCGameState;
class UDCCQuestComponent;
class UDCCNarratorComponent;

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
DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FDCCEventLogged, const FDCCEventLogEntry&, Entry);

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

	/** The party's shared quest log. Assign its QuestTable to give the game goals. */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "DCC|Quests")
	TObjectPtr<UDCCQuestComponent> Quests;

	/**
	 * The AI commentary layer. Reads this GameState's own feed.
	 *
	 * Off unless configured in Project Settings → Game → DCC Narrator, and the
	 * game plays identically without it.
	 */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "DCC|Narration")
	TObjectPtr<UDCCNarratorComponent> Narrator;

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

	// -------------------------------------------------------------------------
	// EVENT FEED
	// -------------------------------------------------------------------------

	/**
	 * Server only. Appends a line to the shared feed.
	 *
	 * Safe to call from anywhere on the server; it no-ops on clients rather than
	 * asserting, because the call sites are ordinary gameplay code that should not
	 * have to check authority before narrating something.
	 */
	UFUNCTION(BlueprintCallable, Category = "Log")
	void AddLogEntry(EDCCLogCategory Category, const FText& Text);

	/**
	 * Convenience for gameplay code: finds the GameState and logs in one call,
	 * tolerating a null world or a GameState of the wrong class.
	 *
	 * Exists so that call sites in characters, skills and encounters stay a single
	 * readable line instead of four lines of lookup and null-checking each time.
	 */
	static void Log(const UObject* WorldContext, EDCCLogCategory Category, const FText& Text);

	UFUNCTION(BlueprintPure, Category = "Log")
	const TArray<FDCCEventLogEntry>& GetEventLog() const { return EventLog; }

	/** Fires on the server and on every client as entries arrive. */
	UPROPERTY(BlueprintAssignable, Category = "Log")
	FDCCEventLogged OnEventLogged;

	/**
	 * Longest the feed grows before the oldest lines are dropped.
	 *
	 * Capped because the whole array replicates on change: unbounded history would
	 * mean an ever-growing payload for a HUD that only ever shows the last few.
	 */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Log", meta = (ClampMin = "1"))
	int32 MaxLogEntries = 50;

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

	/**
	 * The feed. Replicated as a whole array so a client joining mid-fight gets the
	 * recent history rather than starting blank.
	 */
	UPROPERTY(ReplicatedUsing = OnRep_EventLog, BlueprintReadOnly, Category = "Log")
	TArray<FDCCEventLogEntry> EventLog;

	UFUNCTION()
	void OnRep_CombatPaused();

	UFUNCTION()
	void OnRep_EventLog();

	/** Server only. Runs every queued action in initiative order, then clears. */
	void ResolveQueuedActions();

	void ApplyTimeDilation();

	UFUNCTION(BlueprintImplementableEvent, Category = "Combat", meta = (DisplayName = "On Combat Pause Changed"))
	void BP_OnCombatPauseChanged(bool bPaused);

private:
	/**
	 * How many entries this machine has already announced.
	 *
	 * OnRep hands over the whole array, not the delta, so without this a client
	 * would re-broadcast every historical line each time one new entry arrived.
	 */
	int32 LastAnnouncedEntryCount = 0;
};
