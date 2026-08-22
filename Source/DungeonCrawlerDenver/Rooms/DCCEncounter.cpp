#include "Rooms/DCCEncounter.h"

#include "Characters/DCCEnemyCharacter.h"
#include "Characters/DCCCharacterBase.h"
#include "Stats/DCCStatsComponent.h"
#include "Skills/DCCSkillComponent.h"
#include "Stats/DCCRuleset.h"
#include "Progression/DCCAchievementComponent.h"
#include "Quests/DCCQuestComponent.h"
#include "Combat/DCCGameState.h"
#include "UI/DCCEventLogTypes.h"
#include "Components/SceneComponent.h"
#include "Net/UnrealNetwork.h"
#include "Engine/World.h"
#include "EngineUtils.h"

ADCCEncounter::ADCCEncounter()
{
	// Ticks only to evaluate wave triggers, and only on the server while a fight
	// is live. Four times a second is ample - a wave arriving up to 250ms late is
	// imperceptible, and it keeps a level full of dormant encounters near-free.
	PrimaryActorTick.bCanEverTick = true;
	PrimaryActorTick.TickInterval = 0.25f;

	bReplicates = true;
	bAlwaysRelevant = true;

	// The encounter is a logical node, not a visible object; it only needs a
	// transform so its spawn points have something to be relative to.
	USceneComponent* Root = CreateDefaultSubobject<USceneComponent>(TEXT("Root"));
	SetRootComponent(Root);
}

void ADCCEncounter::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);

	DOREPLIFETIME(ADCCEncounter, State);
}

const TArray<FDCCEnemySpawn>& ADCCEncounter::GetWaveSpawns(int32 WaveIndex, TArray<FDCCEnemySpawn>& Scratch) const
{
	if (Waves.IsValidIndex(WaveIndex))
	{
		return Waves[WaveIndex].Enemies;
	}

	// No waves authored: the legacy Enemies list is wave 0 and there is nothing
	// after it. Scratch exists only so this can return a reference in both cases.
	Scratch = (WaveIndex == 0) ? Enemies : TArray<FDCCEnemySpawn>();

	return Scratch;
}

ADCCEnemyCharacter* ADCCEncounter::GetBoss() const
{
	return (Boss && Boss->IsAlive()) ? Boss.Get() : nullptr;
}

void ADCCEncounter::SpawnWave(int32 WaveIndex)
{
	UWorld* World = GetWorld();
	if (!HasAuthority() || !World)
	{
		return;
	}

	TArray<FDCCEnemySpawn> Scratch;
	const TArray<FDCCEnemySpawn>& Spawns = GetWaveSpawns(WaveIndex, Scratch);

	FActorSpawnParameters SpawnParams;
	SpawnParams.Owner = this;
	// Enemies are placed by hand at authored points, so nudge them out of any
	// geometry they clip rather than refusing to spawn and silently emptying
	// the fight.
	SpawnParams.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AdjustIfPossibleButAlwaysSpawn;

	for (const FDCCEnemySpawn& Spawn : Spawns)
	{
		if (!Spawn.EnemyClass)
		{
			UE_LOG(LogTemp, Warning, TEXT("DCCEncounter '%s' wave %d has a spawn entry with no EnemyClass; skipping it."),
				*GetName(), WaveIndex);
			continue;
		}

		const FVector SpawnLocation = GetActorTransform().TransformPosition(Spawn.RelativeLocation);

		ADCCEnemyCharacter* Enemy = World->SpawnActor<ADCCEnemyCharacter>(
			Spawn.EnemyClass,
			SpawnLocation,
			GetActorRotation(),
			SpawnParams);

		if (!Enemy)
		{
			continue;
		}

		if (UDCCStatsComponent* EnemyStats = Enemy->Stats)
		{
			EnemyStats->Level = Spawn.Level;
			if (Spawn.bOverrideAbilityScores)
			{
				EnemyStats->AbilityScores = Spawn.AbilityScores;
			}

			// Stats were already initialised from the class defaults when the
			// component spawned, so re-derive max HP against what we just set.
			EnemyStats->RefreshDerivedStats(/*bRefillToFull=*/true);
		}

		Enemy->OnCharacterDied.AddDynamic(this, &ADCCEncounter::HandleEnemyDied);

		SpawnedEnemies.Add(Enemy);

		if (Spawn.bIsBoss)
		{
			Boss = Enemy;
		}
	}

	LastWaveTime = World->GetTimeSeconds();
	NextWaveIndex = FMath::Max(NextWaveIndex, WaveIndex + 1);

	if (Waves.IsValidIndex(WaveIndex) && !Waves[WaveIndex].Announcement.IsEmpty())
	{
		ADCCGameState::Log(this, EDCCLogCategory::Encounter, Waves[WaveIndex].Announcement);
	}

	OnWaveSpawned.Broadcast(WaveIndex);
}

void ADCCEncounter::EvaluateNextWave()
{
	UWorld* World = GetWorld();
	if (!World || !Waves.IsValidIndex(NextWaveIndex))
	{
		return;
	}

	const FDCCEncounterWave& Wave = Waves[NextWaveIndex];

	// The delay is a floor for every trigger, not just Delay itself. Without it a
	// PreviousCleared wave lands on the same frame as the last kill, which reads
	// as the game cheating rather than as reinforcements.
	if (World->GetTimeSeconds() - LastWaveTime < Wave.DelaySeconds)
	{
		return;
	}

	switch (Wave.Trigger)
	{
	case EDCCWaveTrigger::Immediate:
	case EDCCWaveTrigger::Delay:
		SpawnWave(NextWaveIndex);
		break;

	case EDCCWaveTrigger::PreviousCleared:
		if (GetLivingEnemyCount() == 0)
		{
			SpawnWave(NextWaveIndex);
		}
		break;

	case EDCCWaveTrigger::HealthThreshold:
	{
		// Deliberately reads the boss even once it is dead-but-not-destroyed, via
		// SpawnedEnemies rather than GetBoss - a boss killed in one burst past the
		// threshold should still have paid its summon, or the fight loses its
		// second half to a lucky crit.
		const UDCCStatsComponent* BossStats = Boss ? Boss->Stats : nullptr;
		if (BossStats && BossStats->GetHealthFraction() <= Wave.HealthThresholdFraction)
		{
			SpawnWave(NextWaveIndex);
		}
		break;
	}

	case EDCCWaveTrigger::Manual:
	default:
		break;
	}
}

void ADCCEncounter::Tick(float DeltaSeconds)
{
	Super::Tick(DeltaSeconds);

	if (!HasAuthority() || State != EDCCEncounterState::Active)
	{
		return;
	}

	EvaluateNextWave();
}

int32 ADCCEncounter::GetLivingEnemyCount() const
{
	int32 Count = 0;
	for (const TObjectPtr<ADCCEnemyCharacter>& Enemy : SpawnedEnemies)
	{
		if (Enemy && Enemy->IsAlive())
		{
			++Count;
		}
	}

	return Count;
}

void ADCCEncounter::StartEncounter()
{
	if (!HasAuthority())
	{
		UE_LOG(LogTemp, Warning, TEXT("DCCEncounter::StartEncounter called on a client; ignoring. Encounters are server-authoritative."));
		return;
	}

	if (State != EDCCEncounterState::Dormant)
	{
		return;
	}

	UWorld* World = GetWorld();
	if (!World)
	{
		return;
	}

	NextWaveIndex = 0;
	SpawnWave(0);

	// Any further Immediate waves land in the same frame, so a fight can open with
	// several distinct groups without one of them arriving a tick late.
	while (Waves.IsValidIndex(NextWaveIndex) && Waves[NextWaveIndex].Trigger == EDCCWaveTrigger::Immediate)
	{
		SpawnWave(NextWaveIndex);
	}

	// Captured now rather than read off SpawnedEnemies at clear time, because by
	// then every enemy is dead and some have already been destroyed by lifespan -
	// the payout would shrink depending on how long the party took.
	LastSpawnedCount = SpawnedEnemies.Num();
	LastSpawnedAverageLevel = 1;

	if (SpawnedEnemies.Num() > 0)
	{
		int32 LevelTotal = 0;
		for (const ADCCEnemyCharacter* Enemy : SpawnedEnemies)
		{
			LevelTotal += (Enemy && Enemy->Stats) ? Enemy->Stats->Level : 1;
		}

		LastSpawnedAverageLevel = FMath::Max(1, LevelTotal / SpawnedEnemies.Num());
	}

	if (SpawnedEnemies.Num() == 0 && NextWaveIndex >= GetWaveCount())
	{
		UE_LOG(LogTemp, Warning, TEXT("DCCEncounter '%s' started with no enemies; marking cleared immediately."), *GetName());
		SetState(EDCCEncounterState::Cleared);
		return;
	}

	UE_LOG(LogTemp, Log, TEXT("DCCEncounter '%s' started with %d enemies."), *GetName(), SpawnedEnemies.Num());

	SetState(EDCCEncounterState::Active);
}

void ADCCEncounter::HandleEnemyDied(ADCCCharacterBase* Character)
{
	if (!HasAuthority() || State != EDCCEncounterState::Active)
	{
		return;
	}

	if (GetLivingEnemyCount() > 0)
	{
		return;
	}

	// An empty field is not a cleared fight while waves are still owed. Without
	// this check, killing the opening group would end a boss encounter before the
	// boss ever appeared.
	if (NextWaveIndex < GetWaveCount())
	{
		return;
	}

	UE_LOG(LogTemp, Log, TEXT("DCCEncounter '%s' cleared after %d wave(s)."), *GetName(), NextWaveIndex);
	SetState(EDCCEncounterState::Cleared);
}

void ADCCEncounter::ResetEncounter()
{
	if (!HasAuthority())
	{
		UE_LOG(LogTemp, Warning, TEXT("DCCEncounter::ResetEncounter called on a client; ignoring."));
		return;
	}

	for (const TObjectPtr<ADCCEnemyCharacter>& Enemy : SpawnedEnemies)
	{
		if (Enemy)
		{
			Enemy->OnCharacterDied.RemoveDynamic(this, &ADCCEncounter::HandleEnemyDied);
			Enemy->Destroy();
		}
	}

	SpawnedEnemies.Reset();

	// Rewound too, or a re-armed encounter would think every wave had already
	// been paid and clear itself the moment the first group died.
	Boss = nullptr;
	NextWaveIndex = 0;
	LastWaveTime = 0.0f;

	SetState(EDCCEncounterState::Dormant);
}

void ADCCEncounter::RegisterSpawnedEnemy(ADCCEnemyCharacter* Enemy)
{
	if (!HasAuthority() || !Enemy || SpawnedEnemies.Contains(Enemy))
	{
		return;
	}

	// Exactly SpawnWave's own registration - the death hookup and roster
	// membership, just for an enemy this encounter did not spawn itself.
	Enemy->OnCharacterDied.AddDynamic(this, &ADCCEncounter::HandleEnemyDied);
	SpawnedEnemies.Add(Enemy);
}

void ADCCEncounter::SetState(EDCCEncounterState NewState)
{
	if (State == NewState)
	{
		return;
	}

	State = NewState;

	// Clearing an encounter is this game's stand-in for the tabletop's "end of
	// each floor": the moment marked skills roll to advance. Chosen because it is
	// an ending the players already feel, so progress lands as a reward for the
	// fight they just won rather than arriving from a timer they cannot see.
	//
	// Server-only and before the notification, so any HUD reacting to Cleared is
	// already looking at the new Ranks.
	if (HasAuthority() && NewState == EDCCEncounterState::Cleared)
	{
		ADCCGameState::Log(this, EDCCLogCategory::Encounter, FText::Format(
			NSLOCTEXT("DCC", "LogCleared", "{0} cleared. Viewership was adequate."),
			FText::FromString(GetName())));

		AwardSkillAdvancement();
		AwardClearExperience();
	}

	// The server does not receive its own OnRep, so mirror the notification here.
	OnRep_State();
}

void ADCCEncounter::AwardSkillAdvancement()
{
	UWorld* World = GetWorld();
	if (!World)
	{
		return;
	}

	// Every living crawler in the level, rather than only those standing in the
	// room. With one room that is the same set, and when floors exist this should
	// narrow to the encounter's own participants - survivors of a fight they fled
	// should not advance from it.
	for (TActorIterator<ADCCCharacterBase> It(World); It; ++It)
	{
		ADCCCharacterBase* Character = *It;
		if (!Character || !Character->IsCrawler() || !Character->IsAlive())
		{
			continue;
		}

		if (UDCCSkillComponent* Skills = Character->Skills)
		{
			const int32 Advanced = Skills->RunSkillAdvancement();
			if (Advanced > 0)
			{
				UDCCAchievementComponent::RecordFor(Character, EDCCAchievementStat::SkillRanksGained, Advanced);

				UE_LOG(LogTemp, Log, TEXT("DCCEncounter '%s': %s advanced %d skill(s)."),
					*GetName(), *GetNameSafe(Character), Advanced);
			}
		}
	}
}

bool ADCCEncounter::IsFloorCleared() const
{
	UWorld* World = GetWorld();
	if (!World)
	{
		return false;
	}

	// Every encounter on the same floor number, not just this room's. With one
	// floor that is all of them, and it already reads correctly once floors exist.
	for (TActorIterator<ADCCEncounter> It(World); It; ++It)
	{
		const ADCCEncounter* Other = *It;
		if (!Other || Other->FloorNumber != FloorNumber)
		{
			continue;
		}

		if (Other->GetState() != EDCCEncounterState::Cleared)
		{
			return false;
		}
	}

	return true;
}

void ADCCEncounter::AwardClearExperience()
{
	UWorld* World = GetWorld();
	if (!World)
	{
		return;
	}

	const bool bFloorCleared = IsFloorCleared();

	// Gathered before paying anyone, so that a level-up from the clear bonus
	// cannot change what the floor bonus is worth partway through the loop.
	TArray<ADCCCharacterBase*> Survivors;
	for (TActorIterator<ADCCCharacterBase> It(World); It; ++It)
	{
		ADCCCharacterBase* Character = *It;
		if (Character && Character->IsCrawler() && Character->IsAlive() && Character->Stats)
		{
			Survivors.Add(Character);
		}
	}

	for (ADCCCharacterBase* Character : Survivors)
	{
		UDCCRuleset* Ruleset = Character->Stats->GetRuleset();
		if (!Ruleset)
		{
			continue;
		}

		UDCCAchievementComponent::RecordFor(Character, EDCCAchievementStat::EncountersCleared);

		const int32 LevelBefore = Character->Stats->Level;

		// Paid in full to each survivor rather than split. Splitting would punish
		// the party for bringing everyone, which is backwards for a co-op game.
		int32 Gained = Ruleset->GetExperienceForEncounterClear(
			FMath::Max(1, LastSpawnedCount), LastSpawnedAverageLevel);

		if (bFloorCleared)
		{
			Gained += Ruleset->GetExperienceForFloorClear(FloorNumber, /*EncounterCount=*/1);
			UDCCAchievementComponent::RecordFor(Character, EDCCAchievementStat::FloorsCleared);
		}

		Character->Stats->AddExperience(Gained);

		if (Character->Stats->Level > LevelBefore)
		{
			Character->AnnounceLevelUp(LevelBefore);
		}
	}

	ADCCGameState::Log(this, EDCCLogCategory::Experience, FText::Format(
		NSLOCTEXT("DCC", "LogClearXP", "Clear bonus paid to {0} survivor(s)."),
		FText::AsNumber(Survivors.Num())));

	// Reported by actor name so a quest can name a specific room, while an
	// objective with no TargetId counts any encounter anywhere.
	UDCCQuestComponent::NotifyEvent(this, EDCCObjectiveType::ClearEncounter, GetFName());

	if (bFloorCleared)
	{
		UDCCQuestComponent::NotifyEvent(this, EDCCObjectiveType::ClearFloor,
			FName(*FString::FromInt(FloorNumber)));
	}

	if (bFloorCleared)
	{
		// The floor is the unit the books care about, so it gets the announcement
		// voice rather than a line in the combat feed.
		ADCCGameState::Log(this, EDCCLogCategory::Announcement, FText::Format(
			NSLOCTEXT("DCC", "LogFloorCleared",
				"Floor {0} is clear. Production thanks you for your cooperation, and reminds you there are more floors."),
			FText::AsNumber(FloorNumber)));
	}
}

void ADCCEncounter::OnRep_State()
{
	OnEncounterStateChanged.Broadcast(State);

	if (State == EDCCEncounterState::Cleared)
	{
		BP_OnEncounterCleared();
	}
}
