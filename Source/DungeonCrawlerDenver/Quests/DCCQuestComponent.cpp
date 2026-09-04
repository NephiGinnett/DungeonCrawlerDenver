#include "Quests/DCCQuestComponent.h"

#include "Characters/DCCCharacterBase.h"
#include "Stats/DCCStatsComponent.h"
#include "Favor/DCCFavorComponent.h"
#include "Combat/DCCGameState.h"
#include "UI/DCCEventLogTypes.h"
#include "Engine/World.h"
#include "Engine/Engine.h"
#include "EngineUtils.h"
#include "Net/UnrealNetwork.h"
#include "GameFramework/GameStateBase.h"
#include "Engine/DataTable.h"
#include "UObject/ConstructorHelpers.h"

UDCCQuestComponent::UDCCQuestComponent()
{
	PrimaryComponentTick.bCanEverTick = false;
	SetIsReplicatedByDefault(true);

	// Defaulted here for the same reason as the achievement table, and with one
	// extra: this component lives on ADCCGameState, which has no Blueprint child
	// to carry a Details-panel assignment at all.
	static ConstructorHelpers::FObjectFinder<UDataTable> DefaultTable(
		TEXT("/Game/DCC/Quests/DT_DCCQuests.DT_DCCQuests"));
	if (DefaultTable.Succeeded())
	{
		QuestTable = DefaultTable.Object;
	}
}

void UDCCQuestComponent::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);

	DOREPLIFETIME(UDCCQuestComponent, Quests);
	DOREPLIFETIME(UDCCQuestComponent, LastCompletedQuestId);
}

void UDCCQuestComponent::BeginPlay()
{
	Super::BeginPlay();

	if (GetOwnerRole() != ROLE_Authority || !QuestTable)
	{
		return;
	}

	static const FString Context(TEXT("UDCCQuestComponent::BeginPlay"));

	TArray<FDCCQuestDef*> Rows;
	QuestTable->GetAllRows<FDCCQuestDef>(Context, Rows);

	for (const FDCCQuestDef* Row : Rows)
	{
		if (Row && Row->bAutoStart)
		{
			StartQuest(Row->QuestId);
		}
	}
}

UDCCQuestComponent* UDCCQuestComponent::Get(const UObject* WorldContext)
{
	const UWorld* World = GEngine
		? GEngine->GetWorldFromContextObject(WorldContext, EGetWorldErrorMode::ReturnNull)
		: nullptr;

	AGameStateBase* GameState = World ? World->GetGameState() : nullptr;

	return GameState ? GameState->FindComponentByClass<UDCCQuestComponent>() : nullptr;
}

void UDCCQuestComponent::NotifyEvent(const UObject* WorldContext, EDCCObjectiveType Type,
	FName TargetId, int32 Amount)
{
	if (UDCCQuestComponent* Component = Get(WorldContext))
	{
		Component->Notify(Type, TargetId, Amount);
	}
}

bool UDCCQuestComponent::FindQuest(FName QuestId, FDCCQuestDef& OutQuest) const
{
	if (!QuestTable || QuestId.IsNone())
	{
		return false;
	}

	static const FString Context(TEXT("UDCCQuestComponent::FindQuest"));

	if (const FDCCQuestDef* Row = QuestTable->FindRow<FDCCQuestDef>(QuestId, Context, /*bWarnIfMissing=*/false))
	{
		OutQuest = *Row;
		return true;
	}

	return false;
}

const FDCCQuestState* UDCCQuestComponent::FindState(FName QuestId) const
{
	return Quests.FindByPredicate([QuestId](const FDCCQuestState& State)
	{
		return State.QuestId == QuestId;
	});
}

FDCCQuestState* UDCCQuestComponent::FindMutableState(FName QuestId)
{
	return Quests.FindByPredicate([QuestId](const FDCCQuestState& State)
	{
		return State.QuestId == QuestId;
	});
}

bool UDCCQuestComponent::IsQuestActive(FName QuestId) const
{
	const FDCCQuestState* State = FindState(QuestId);

	return State && !State->bComplete;
}

bool UDCCQuestComponent::IsQuestComplete(FName QuestId) const
{
	const FDCCQuestState* State = FindState(QuestId);

	return State && State->bComplete;
}

bool UDCCQuestComponent::StartQuest(FName QuestId)
{
	if (GetOwnerRole() != ROLE_Authority)
	{
		return false;
	}

	// Already started, in either sense. Re-starting a finished quest would reset
	// its counts and quietly re-pay its rewards.
	if (FindState(QuestId))
	{
		return false;
	}

	FDCCQuestDef Definition;
	if (!FindQuest(QuestId, Definition))
	{
		UE_LOG(LogTemp, Warning, TEXT("DCCQuest: no row '%s' in the quest table."), *QuestId.ToString());
		return false;
	}

	FDCCQuestState State;
	State.QuestId = QuestId;
	State.Objectives.SetNum(Definition.Objectives.Num());
	State.StartedAtSeconds = GetWorld() ? GetWorld()->GetTimeSeconds() : 0.0f;

	Quests.Add(State);

	const FText Line = Definition.StartAnnouncement.IsEmpty()
		? FText::Format(NSLOCTEXT("DCC", "QuestStartedGeneric", "New objective: {0}"), Definition.Title)
		: Definition.StartAnnouncement;

	ADCCGameState::Log(this, EDCCLogCategory::Announcement, Line);

	OnQuestStarted.Broadcast(this, Definition);

	// The server does not receive its own OnRep, so mirror it for anything - the
	// HUD included - that only listens to the notify.
	OnRep_Quests();

	// Objectives that are already satisfied at start time complete immediately.
	// A "clear the floor" quest handed out on an already-clear floor should not
	// sit in the tracker forever waiting for an event that cannot happen again.
	Notify(EDCCObjectiveType::MAX, NAME_None, 0);

	return true;
}

void UDCCQuestComponent::Notify(EDCCObjectiveType Type, FName TargetId, int32 Amount)
{
	if (GetOwnerRole() != ROLE_Authority || Quests.IsEmpty())
	{
		return;
	}

	bool bAnyProgress = false;

	// Indexed rather than ranged-for: CompleteQuest can start a follow-up quest,
	// which appends to Quests and would invalidate an iterator mid-loop.
	for (int32 QuestIndex = 0; QuestIndex < Quests.Num(); ++QuestIndex)
	{
		if (Quests[QuestIndex].bComplete)
		{
			continue;
		}

		FDCCQuestDef Definition;
		if (!FindQuest(Quests[QuestIndex].QuestId, Definition))
		{
			continue;
		}

		bool bAllComplete = true;

		for (int32 Index = 0; Index < Definition.Objectives.Num(); ++Index)
		{
			const FDCCQuestObjective& Objective = Definition.Objectives[Index];

			if (!Quests[QuestIndex].Objectives.IsValidIndex(Index))
			{
				// The table grew a new objective after the quest was already in
				// progress. Growing here keeps the save readable rather than
				// dropping the quest entirely.
				Quests[QuestIndex].Objectives.SetNum(Definition.Objectives.Num());
			}

			FDCCObjectiveProgress& Progress = Quests[QuestIndex].Objectives[Index];

			const bool bMatches = Objective.Type == Type
				&& (Objective.TargetId.IsNone() || Objective.TargetId == TargetId);

			if (!Progress.bComplete && bMatches && Amount > 0)
			{
				Progress.Count += Amount;
				bAnyProgress = true;
			}

			if (!Progress.bComplete && Progress.Count >= Objective.RequiredCount)
			{
				Progress.bComplete = true;
				bAnyProgress = true;

				if (!Objective.bHidden)
				{
					ADCCGameState::Log(this, EDCCLogCategory::Announcement, FText::Format(
						NSLOCTEXT("DCC", "ObjectiveDone", "Objective complete: {0}"),
						Objective.Description));
				}
			}

			bAllComplete &= Progress.bComplete;
		}

		if (bAllComplete && Definition.Objectives.Num() > 0)
		{
			CompleteQuest(Definition, Quests[QuestIndex]);
			bAnyProgress = true;
		}
	}

	if (bAnyProgress)
	{
		OnRep_Quests();
	}
}

void UDCCQuestComponent::CompleteQuest(const FDCCQuestDef& Quest, FDCCQuestState& State)
{
	State.bComplete = true;
	LastCompletedQuestId = Quest.QuestId;

	const FText Line = Quest.CompletionAnnouncement.IsEmpty()
		? FText::Format(NSLOCTEXT("DCC", "QuestDoneGeneric", "Objective complete: {0}"), Quest.Title)
		: Quest.CompletionAnnouncement;

	ADCCGameState::Log(this, EDCCLogCategory::Announcement, Line);

	UWorld* World = GetWorld();
	if (World && (Quest.ExperienceReward > 0 || Quest.FavorReward != 0))
	{
		// Paid to every living crawler rather than split, for the same reason the
		// encounter clear bonus is: splitting punishes the party for turning up
		// together.
		for (TActorIterator<ADCCCharacterBase> It(World); It; ++It)
		{
			ADCCCharacterBase* Character = *It;
			if (!Character || !Character->IsCrawler() || !Character->IsAlive())
			{
				continue;
			}

			if (Quest.ExperienceReward > 0 && Character->Stats)
			{
				const int32 LevelBefore = Character->Stats->Level;
				Character->Stats->AddExperience(Quest.ExperienceReward);

				if (Character->Stats->Level > LevelBefore)
				{
					Character->AnnounceLevelUp(LevelBefore);
				}
			}

			if (Quest.FavorReward != 0 && Character->Favor)
			{
				Character->Favor->AwardFavor(
					Quest.FavorReward, EDCCFavorEvent::DirectorCall, Quest.Title);
			}
		}
	}

	OnQuestCompleted.Broadcast(this, Quest);

	// Chained last, so the follow-up's announcement lands after this one's.
	if (!Quest.NextQuestId.IsNone())
	{
		StartQuest(Quest.NextQuestId);
	}
}

void UDCCQuestComponent::GetActiveQuests(TArray<FDCCQuestDef>& OutQuests) const
{
	OutQuests.Reset();

	for (const FDCCQuestState& State : Quests)
	{
		if (State.bComplete)
		{
			continue;
		}

		FDCCQuestDef Definition;
		if (FindQuest(State.QuestId, Definition))
		{
			OutQuests.Add(Definition);
		}
	}
}

bool UDCCQuestComponent::IsObjectiveTagActive(FName ObjectiveTag) const
{
	if (ObjectiveTag.IsNone())
	{
		return false;
	}

	for (const FDCCQuestState& State : Quests)
	{
		if (State.bComplete)
		{
			continue;
		}

		FDCCQuestDef Definition;
		if (!FindQuest(State.QuestId, Definition))
		{
			continue;
		}

		for (int32 Index = 0; Index < Definition.Objectives.Num(); ++Index)
		{
			const FDCCQuestObjective& Objective = Definition.Objectives[Index];

			if (Objective.TargetId != ObjectiveTag)
			{
				continue;
			}

			// An objective already met stops advertising itself, so a cleared
			// marker goes quiet instead of standing there glowing at a party
			// that has already done the thing.
			if (!State.Objectives.IsValidIndex(Index) || !State.Objectives[Index].bComplete)
			{
				return true;
			}
		}
	}

	return false;
}

void UDCCQuestComponent::OnRep_Quests()
{
	OnObjectiveProgressed.Broadcast(this);
}
