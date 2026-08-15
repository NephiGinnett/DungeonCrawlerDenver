#include "Combat/DCCGameState.h"

#include "Characters/DCCCharacterBase.h"
#include "Skills/DCCSkillComponent.h"
#include "GameFramework/WorldSettings.h"
#include "Engine/World.h"
#include "Net/UnrealNetwork.h"

ADCCGameState::ADCCGameState()
{
	PrimaryActorTick.bCanEverTick = false;
}

void ADCCGameState::AddLogEntry(EDCCLogCategory Category, const FText& Text)
{
	// Clients simply ignore the call rather than warning. Narrating is something
	// gameplay code does incidentally, and making every call site check authority
	// first would add noise to a dozen places for no benefit.
	if (!HasAuthority() || Text.IsEmpty())
	{
		return;
	}

	FDCCEventLogEntry Entry;
	Entry.Category = Category;
	Entry.Text = Text;
	Entry.TimeSeconds = GetWorld() ? GetWorld()->GetTimeSeconds() : 0.0f;

	EventLog.Add(Entry);

	// Oldest first out. RemoveAt on a small array is cheaper than the churn of a
	// ring buffer, and keeps the array in display order for the HUD.
	while (EventLog.Num() > FMath::Max(1, MaxLogEntries))
	{
		EventLog.RemoveAt(0);
	}

	LastAnnouncedEntryCount = EventLog.Num();

	// The server never receives its own OnRep, so announce here.
	OnEventLogged.Broadcast(Entry);

	UE_LOG(LogTemp, Log, TEXT("DCCLog: %s"), *Entry.Text.ToString());
}

void ADCCGameState::Log(const UObject* WorldContext, EDCCLogCategory Category, const FText& Text)
{
	const UWorld* World = GEngine ? GEngine->GetWorldFromContextObject(WorldContext, EGetWorldErrorMode::ReturnNull) : nullptr;
	if (!World)
	{
		return;
	}

	if (ADCCGameState* GameState = World->GetGameState<ADCCGameState>())
	{
		GameState->AddLogEntry(Category, Text);
	}
}

void ADCCGameState::OnRep_EventLog()
{
	// Replication hands over the whole array, so announce only what is new since
	// the last update. Entries dropped off the front would otherwise make the
	// count go backwards and replay the entire history.
	const int32 FirstNew = FMath::Clamp(LastAnnouncedEntryCount, 0, EventLog.Num());

	for (int32 Index = FirstNew; Index < EventLog.Num(); ++Index)
	{
		OnEventLogged.Broadcast(EventLog[Index]);
	}

	LastAnnouncedEntryCount = EventLog.Num();
}

void ADCCGameState::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);

	DOREPLIFETIME(ADCCGameState, bCombatPaused);
	DOREPLIFETIME(ADCCGameState, QueuedActions);
	DOREPLIFETIME(ADCCGameState, EventLog);
}

void ADCCGameState::SetCombatPaused(bool bNewPaused)
{
	if (!HasAuthority())
	{
		UE_LOG(LogTemp, Warning, TEXT("DCCGameState::SetCombatPaused called on a client; ignoring. Pause state is server-authoritative."));
		return;
	}

	if (bCombatPaused == bNewPaused)
	{
		return;
	}

	bCombatPaused = bNewPaused;

	// Resolve on the way out of a pause, before anything else observes the
	// resumed world - otherwise AI gets a frame to act ahead of the queue.
	if (!bCombatPaused)
	{
		ResolveQueuedActions();
	}

	ApplyTimeDilation();

	// The server does not receive its own OnRep.
	OnCombatPauseChanged.Broadcast(this, bCombatPaused);
	BP_OnCombatPauseChanged(bCombatPaused);

	UE_LOG(LogTemp, Log, TEXT("DCCCombat: %s"), bCombatPaused ? TEXT("paused - queueing actions") : TEXT("resumed"));
}

bool ADCCGameState::QueueAction(ADCCCharacterBase* Character, FName SkillId, AActor* Target)
{
	if (!HasAuthority())
	{
		UE_LOG(LogTemp, Warning, TEXT("DCCGameState::QueueAction called on a client; ignoring."));
		return false;
	}

	if (!Character || !Character->IsAlive())
	{
		return false;
	}

	// Queueing outside a pause would let a player bank actions for later, which
	// is not what the pause is for.
	if (!bCombatPaused)
	{
		return false;
	}

	FDCCQueuedAction Action;
	Action.Instigator = Character;
	Action.SkillId = SkillId;
	Action.Target = Target;

	// Initiative comes from the action itself: a skill's cooldown, or the
	// character's attack cooldown for a basic swing.
	Action.InitiativeCost = Character->AttackCooldownSeconds;
	if (!SkillId.IsNone() && Character->Skills)
	{
		FDCCSkillDef Skill;
		if (Character->Skills->FindSkill(SkillId, Skill))
		{
			Action.InitiativeCost = Skill.CooldownSeconds;
		}
	}

	// One action per character - re-queueing replaces, so changing your mind
	// during the pause is free.
	for (FDCCQueuedAction& Existing : QueuedActions)
	{
		if (Existing.Instigator == Character)
		{
			Existing = Action;
			return true;
		}
	}

	QueuedActions.Add(Action);
	return true;
}

bool ADCCGameState::ClearQueuedAction(ADCCCharacterBase* Character)
{
	if (!HasAuthority())
	{
		return false;
	}

	return QueuedActions.RemoveAll([Character](const FDCCQueuedAction& Action)
	{
		return Action.Instigator == Character;
	}) > 0;
}

bool ADCCGameState::HasQueuedAction(const ADCCCharacterBase* Character) const
{
	for (const FDCCQueuedAction& Action : QueuedActions)
	{
		if (Action.Instigator == Character)
		{
			return true;
		}
	}

	return false;
}

void ADCCGameState::ResolveQueuedActions()
{
	if (!HasAuthority() || QueuedActions.Num() == 0)
	{
		return;
	}

	// Cheapest action first. Stable so that equal-cost actions keep the order
	// they were queued in, which makes resolution reproducible rather than
	// dependent on array churn.
	TArray<FDCCQueuedAction> Ordered = QueuedActions;
	Ordered.StableSort([](const FDCCQueuedAction& A, const FDCCQueuedAction& B)
	{
		return A.InitiativeCost < B.InitiativeCost;
	});

	int32 NumResolved = 0;

	for (const FDCCQueuedAction& Action : Ordered)
	{
		// Named Actor, not Instigator: AActor declares an Instigator member and
		// the engine builds with shadowing warnings as errors.
		ADCCCharacterBase* Actor = Action.Instigator.Get();

		// Re-checked here rather than trusted from queue time: an earlier action
		// in this same resolve may have killed the actor or the target.
		if (!Actor || !Actor->IsAlive())
		{
			continue;
		}

		if (Action.SkillId.IsNone())
		{
			ADCCCharacterBase* Target = Cast<ADCCCharacterBase>(Action.Target.Get());
			if (!Target)
			{
				Target = Actor->FindNearestHostileInRange();
			}

			if (Target && Target->IsAlive())
			{
				// Cooldown deliberately bypassed here - see PerformAttack.
				Actor->PerformAttack(Target, /*bIgnoreCooldown=*/true);
				++NumResolved;
			}
		}
		else if (Actor->Skills)
		{
			Actor->Skills->TryActivateSkill(Action.SkillId, Action.Target.Get());
			++NumResolved;
		}
	}

	QueuedActions.Reset();

	OnActionsResolved.Broadcast(NumResolved);

	UE_LOG(LogTemp, Log, TEXT("DCCCombat: resolved %d queued action(s)."), NumResolved);
}

void ADCCGameState::ApplyTimeDilation()
{
	if (!bSlowTimeWhilePaused)
	{
		return;
	}

	UWorld* World = GetWorld();
	AWorldSettings* Settings = World ? World->GetWorldSettings() : nullptr;
	if (!Settings)
	{
		return;
	}

	// Set on the server; WorldSettings replicates it, so clients slow with us
	// rather than each having to be told separately.
	Settings->SetTimeDilation(bCombatPaused ? PausedTimeDilation : 1.0f);
}

void ADCCGameState::OnRep_CombatPaused()
{
	// Clients mirror the visual effect but never resolve anything.
	ApplyTimeDilation();

	OnCombatPauseChanged.Broadcast(this, bCombatPaused);
	BP_OnCombatPauseChanged(bCombatPaused);
}
