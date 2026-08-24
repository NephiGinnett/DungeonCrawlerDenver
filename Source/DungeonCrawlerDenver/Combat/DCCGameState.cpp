#include "Combat/DCCGameState.h"

#include "Quests/DCCQuestComponent.h"
#include "Narration/DCCNarratorComponent.h"

#include "Characters/DCCCharacterBase.h"
#include "Skills/DCCSkillComponent.h"
#include "Favor/DCCFavorComponent.h"
#include "Equipment/DCCEquipmentComponent.h"
#include "Equipment/DCCEquipmentTypes.h"
#include "GameFramework/WorldSettings.h"
#include "Engine/World.h"
#include "EngineUtils.h"
#include "Net/UnrealNetwork.h"
#include "UObject/ConstructorHelpers.h"

ADCCGameState::ADCCGameState()
{
	PrimaryActorTick.bCanEverTick = false;

	// The quest log lives here rather than on a character because quests are
	// party-wide: five crawlers in one dungeon share one objective list, and a
	// per-player log would show the same objective at four different counts.
	Quests = CreateDefaultSubobject<UDCCQuestComponent>(TEXT("Quests"));

	// Sits next to the feed it reads. Inert until switched on in Project Settings.
	Narrator = CreateDefaultSubobject<UDCCNarratorComponent>(TEXT("Narrator"));

	// Same self-healing pattern as DCCAchievementComponent's table: resolves at
	// editor startup, so a table imported mid-session reads None until the next
	// restart rather than being broken.
	static ConstructorHelpers::FObjectFinder<UDataTable> DefaultChatterTable(
		TEXT("/Game/DCC/UI/DT_DCCChatterLines.DT_DCCChatterLines"));
	if (DefaultChatterTable.Succeeded())
	{
		ChatterTable = DefaultChatterTable.Object;
	}
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

	// Offered to the narrator last, and only ever offered - it returns immediately,
	// drops almost everything, and can never write back synchronously. Its own
	// output arrives later as an ordinary Announcement entry, which is why this
	// cannot recurse: Announcements it produces are throttled by the same timer
	// that gated the original request.
	if (Narrator)
	{
		Narrator->Consider(Entry);
	}

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

void ADCCGameState::LogRoll(const UObject* WorldContext, EDCCLogCategory Category, const FText& Text,
	ADCCCharacterBase* Instigator, ADCCCharacterBase* RollTarget, bool bCriticalSuccess, bool bCriticalFailure,
	EDCCAbility CheckAbility, FName CheckSkillId)
{
	const UWorld* World = GEngine ? GEngine->GetWorldFromContextObject(WorldContext, EGetWorldErrorMode::ReturnNull) : nullptr;
	if (!World || Text.IsEmpty())
	{
		return;
	}

	ADCCGameState* GameState = World->GetGameState<ADCCGameState>();
	if (!GameState || !GameState->HasAuthority())
	{
		return;
	}

	FDCCEventLogEntry Entry;
	Entry.Category = Category;
	Entry.Text = Text;
	Entry.TimeSeconds = World->GetTimeSeconds();
	Entry.Instigator = Instigator;
	Entry.RollTarget = RollTarget;
	Entry.bCriticalSuccess = bCriticalSuccess;
	Entry.bCriticalFailure = bCriticalFailure;
	Entry.CheckAbility = CheckAbility;
	Entry.CheckSkillId = CheckSkillId;

	GameState->EventLog.Add(Entry);

	while (GameState->EventLog.Num() > FMath::Max(1, GameState->MaxLogEntries))
	{
		GameState->EventLog.RemoveAt(0);
	}

	GameState->LastAnnouncedEntryCount = GameState->EventLog.Num();
	GameState->OnEventLogged.Broadcast(Entry);

	if (GameState->Narrator)
	{
		GameState->Narrator->Consider(Entry);
	}

	UE_LOG(LogTemp, Log, TEXT("DCCLog: %s"), *Entry.Text.ToString());
}

void ADCCGameState::AddChatMessage(EDCCChatChannel Channel, const FString& SenderName, ADCCCharacterBase* SenderCharacter,
	const FString& WhisperRecipientName, const FText& Text, bool bModerated)
{
	if (!HasAuthority() || Text.IsEmpty())
	{
		return;
	}

	FDCCChatMessage Message;
	Message.Channel = Channel;
	Message.SenderName = SenderName;
	Message.SenderCharacter = SenderCharacter;
	Message.WhisperRecipientName = WhisperRecipientName;
	Message.Text = Text;
	Message.TimeSeconds = GetWorld() ? GetWorld()->GetTimeSeconds() : 0.0f;
	Message.bModerated = bModerated;

	ChatMessages.Add(Message);

	while (ChatMessages.Num() > FMath::Max(1, MaxChatMessages))
	{
		ChatMessages.RemoveAt(0);
	}
}

void ADCCGameState::OnRep_ChatMessages()
{
	// Nothing to announce incrementally yet - the HUD reads GetChatMessages()
	// fresh each frame the same way it already does for the Feed, so this only
	// exists to satisfy ReplicatedUsing. Left as its own function rather than
	// wired to OnRep_EventLog's delegate so a future per-message notification
	// (e.g. a "new message" ping) has somewhere to go without touching the Feed.
}

void ADCCGameState::RollChatterBatch(int32 Count)
{
	if (!HasAuthority())
	{
		return;
	}

	UWorld* World = GetWorld();
	if (!World || !ChatterTable)
	{
		return;
	}

	// Living crawlers, for the buckets that need to name someone.
	TArray<ADCCCharacterBase*> LivingCrawlers;
	for (TActorIterator<ADCCCharacterBase> It(World); It; ++It)
	{
		if (ADCCCharacterBase* Candidate = *It)
		{
			if (Candidate->IsCrawler() && Candidate->IsAlive())
			{
				LivingCrawlers.Add(Candidate);
			}
		}
	}

	// Most recent Combat/Death/Encounter line, for the boss-battle bucket -
	// reuses the Feed's own history instead of standing up a separate
	// "notable moments" log nobody else reads.
	FString RecentEventText;
	for (int32 Index = EventLog.Num() - 1; Index >= 0; --Index)
	{
		const EDCCLogCategory Category = EventLog[Index].Category;
		if (Category == EDCCLogCategory::Combat || Category == EDCCLogCategory::Death
			|| Category == EDCCLogCategory::Encounter)
		{
			RecentEventText = EventLog[Index].Text.ToString();
			break;
		}
	}

	TArray<FDCCChatterLineDef*> AllRows;
	ChatterTable->GetAllRows(TEXT("RollChatterBatch"), AllRows);
	if (AllRows.IsEmpty())
	{
		return;
	}

	for (int32 Comment = 0; Comment < Count; ++Comment)
	{
		// Zoe's own percentages: 5% each for Sentiment/Equipment/BossBattle/
		// Appearance/Emote/AlienLanguage, a small moderation chance, everything
		// else (the majority) is Benign filler.
		const float Roll = FMath::FRand();
		EDCCChatterBucket Bucket = EDCCChatterBucket::Benign;

		if (Roll < 0.05f)                       Bucket = EDCCChatterBucket::Sentiment;
		else if (Roll < 0.10f)                  Bucket = EDCCChatterBucket::Equipment;
		else if (Roll < 0.15f)                  Bucket = EDCCChatterBucket::BossBattle;
		else if (Roll < 0.20f)                  Bucket = EDCCChatterBucket::Appearance;
		else if (Roll < 0.25f)                  Bucket = EDCCChatterBucket::Emote;
		else if (Roll < 0.30f)                  Bucket = EDCCChatterBucket::AlienLanguage;
		else if (Roll < 0.33f)                  Bucket = EDCCChatterBucket::Moderated;

		FString Name, Item, Event;
		bool bBucketHasData = true;

		switch (Bucket)
		{
		case EDCCChatterBucket::Sentiment:
		{
			// Positive about someone at >=60% Favor, negative about someone at
			// <=40% - pick whichever crawlers actually qualify, skip the
			// bucket for this comment if nobody does.
			TArray<ADCCCharacterBase*> Qualifying;
			for (ADCCCharacterBase* Crawler : LivingCrawlers)
			{
				if (const UDCCFavorComponent* FavorComp = Crawler->FindComponentByClass<UDCCFavorComponent>())
				{
					const float Percent = FMath::Clamp(FavorComp->Favor / (float)FMath::Max(1, FavorComp->MaxFavor), 0.0f, 1.0f);
					if (Percent >= 0.6f || Percent <= 0.4f)
					{
						Qualifying.Add(Crawler);
					}
				}
			}
			bBucketHasData = !Qualifying.IsEmpty();
			if (bBucketHasData)
			{
				Name = Qualifying[FMath::RandRange(0, Qualifying.Num() - 1)]->DisplayName.ToString();
			}
			break;
		}
		case EDCCChatterBucket::Equipment:
		{
			bBucketHasData = false;
			if (!LivingCrawlers.IsEmpty())
			{
				ADCCCharacterBase* Crawler = LivingCrawlers[FMath::RandRange(0, LivingCrawlers.Num() - 1)];
				if (const UDCCEquipmentComponent* Equip = Crawler->FindComponentByClass<UDCCEquipmentComponent>())
				{
					const FName EquippedId = Equip->GetEquippedId(EDCCEquipmentSlot::MainHand);
					FDCCEquipmentDef Def;
					if (!EquippedId.IsNone() && Equip->FindItem(EquippedId, Def))
					{
						Name = Crawler->DisplayName.ToString();
						Item = Def.DisplayName.ToString();
						bBucketHasData = true;
					}
				}
			}
			break;
		}
		case EDCCChatterBucket::BossBattle:
			bBucketHasData = !RecentEventText.IsEmpty();
			Event = RecentEventText;
			break;
		case EDCCChatterBucket::Appearance:
			bBucketHasData = !LivingCrawlers.IsEmpty();
			if (bBucketHasData)
			{
				Name = LivingCrawlers[FMath::RandRange(0, LivingCrawlers.Num() - 1)]->DisplayName.ToString();
			}
			break;
		default:
			break;
		}

		// A bucket that rolled but had nothing to talk about (no boss moment
		// yet, nobody equipped) falls back to Benign rather than posting a
		// line with an unfilled {token} in it.
		if (!bBucketHasData)
		{
			Bucket = EDCCChatterBucket::Benign;
		}

		TArray<FDCCChatterLineDef*> MatchingRows;
		for (FDCCChatterLineDef* Row : AllRows)
		{
			if (Row && Row->Bucket == Bucket)
			{
				MatchingRows.Add(Row);
			}
		}
		if (MatchingRows.IsEmpty())
		{
			continue;
		}

		const FDCCChatterLineDef* Chosen = MatchingRows[FMath::RandRange(0, MatchingRows.Num() - 1)];
		const FText Line = DCCChatter::SubstituteTokens(Chosen->Template, Name, Item, Event);

		AddChatMessage(EDCCChatChannel::Chatter, Chosen->SenderHandle, nullptr, FString(), Line,
			Bucket == EDCCChatterBucket::Moderated);
	}
}

void ADCCGameState::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);

	DOREPLIFETIME(ADCCGameState, bCombatPaused);
	DOREPLIFETIME(ADCCGameState, QueuedActions);
	DOREPLIFETIME(ADCCGameState, EventLog);
	DOREPLIFETIME(ADCCGameState, ChatMessages);
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
