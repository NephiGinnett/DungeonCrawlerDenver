#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "Quests/DCCQuestTypes.h"
#include "DCCQuestComponent.generated.h"

class UDataTable;
class UDCCQuestComponent;

DECLARE_DYNAMIC_MULTICAST_DELEGATE_TwoParams(FDCCQuestStarted, UDCCQuestComponent*, Component, const FDCCQuestDef&, Quest);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_TwoParams(FDCCQuestCompleted, UDCCQuestComponent*, Component, const FDCCQuestDef&, Quest);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FDCCObjectiveProgressed, UDCCQuestComponent*, Component);

/**
 * The party's quest log.
 *
 * Party-wide rather than per-character, and therefore living on the GameState:
 * five players in one dungeon are working on the same objective, and a per-player
 * log would mean the HUD showing "Kill 5" at four different counts to four people
 * standing in the same room.
 *
 * Server-authoritative. Progress arrives through Notify, which is the only entry
 * point gameplay code ever calls - see the comment on it for why that matters.
 */
UCLASS(ClassGroup = (DCC), meta = (BlueprintSpawnableComponent))
class DUNGEONCRAWLERDENVER_API UDCCQuestComponent : public UActorComponent
{
	GENERATED_BODY()

public:
	UDCCQuestComponent();

	virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;
	virtual void BeginPlay() override;

	/** The quest list. Without one this component does nothing at all. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Quests")
	TObjectPtr<UDataTable> QuestTable;

	/** Active and completed quests, replicated so every client's tracker agrees. */
	UPROPERTY(BlueprintReadOnly, ReplicatedUsing = OnRep_Quests, Category = "Quests")
	TArray<FDCCQuestState> Quests;

	UPROPERTY(BlueprintAssignable, Category = "Quests")
	FDCCQuestStarted OnQuestStarted;

	UPROPERTY(BlueprintAssignable, Category = "Quests")
	FDCCQuestCompleted OnQuestCompleted;

	UPROPERTY(BlueprintAssignable, Category = "Quests")
	FDCCObjectiveProgressed OnObjectiveProgressed;

	/** Server only. Begins a quest by id. Ignores one already started. */
	UFUNCTION(BlueprintCallable, Category = "Quests")
	bool StartQuest(FName QuestId);

	/**
	 * Server only. Reports that something happened, and advances anything waiting
	 * on it.
	 *
	 * The single entry point on purpose. Callers never ask which quests are
	 * active or whether an objective cares - they state a fact about the world
	 * and this decides what it means. That is what keeps the quest list out of
	 * combat code and lets Zoe write a new quest without touching anything that
	 * fights.
	 */
	UFUNCTION(BlueprintCallable, Category = "Quests")
	void Notify(EDCCObjectiveType Type, FName TargetId, int32 Amount = 1);

	/** Convenience: finds the party's quest component from anywhere. */
	UFUNCTION(BlueprintPure, Category = "Quests", meta = (WorldContext = "WorldContext"))
	static UDCCQuestComponent* Get(const UObject* WorldContext);

	/**
	 * Routes a Notify through Get, tolerating a null world or missing component.
	 *
	 * Exists so gameplay code stays one readable line, exactly like
	 * ADCCGameState::Log and UDCCAchievementComponent::RecordFor.
	 */
	static void NotifyEvent(const UObject* WorldContext, EDCCObjectiveType Type,
		FName TargetId = NAME_None, int32 Amount = 1);

	UFUNCTION(BlueprintPure, Category = "Quests")
	bool IsQuestActive(FName QuestId) const;

	UFUNCTION(BlueprintPure, Category = "Quests")
	bool IsQuestComplete(FName QuestId) const;

	/** Looks up a definition. False when the table is missing or the id is unknown. */
	UFUNCTION(BlueprintCallable, Category = "Quests")
	bool FindQuest(FName QuestId, FDCCQuestDef& OutQuest) const;

	/** Every quest currently in progress, in the order they were started. */
	UFUNCTION(BlueprintCallable, Category = "Quests")
	void GetActiveQuests(TArray<FDCCQuestDef>& OutQuests) const;

	/** Live progress for one quest, or null when it was never started. */
	const FDCCQuestState* FindState(FName QuestId) const;

	/**
	 * Whether a marker should currently be showing itself.
	 *
	 * Asked by DCCQuestMarker every second or so, which is why it is a cheap
	 * lookup rather than a delegate the markers subscribe to - markers come and
	 * go with streaming, and a subscription list would need unwinding.
	 */
	UFUNCTION(BlueprintPure, Category = "Quests")
	bool IsObjectiveTagActive(FName ObjectiveTag) const;

protected:
	UFUNCTION()
	void OnRep_Quests();

	/** Pays rewards, announces, and starts whatever follows. */
	void CompleteQuest(const FDCCQuestDef& Quest, FDCCQuestState& State);

	FDCCQuestState* FindMutableState(FName QuestId);
};
