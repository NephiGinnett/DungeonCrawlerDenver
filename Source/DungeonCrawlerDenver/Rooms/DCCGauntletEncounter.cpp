#include "Rooms/DCCGauntletEncounter.h"

#include "Characters/DCCEnemyCharacter.h"
#include "Characters/DCCCharacterBase.h"
#include "Stats/DCCStatsComponent.h"
#include "Stats/DCCRuleset.h"
#include "Progression/DCCAchievementComponent.h"
#include "Quests/DCCQuestComponent.h"
#include "Combat/DCCGameState.h"
#include "UI/DCCEventLogTypes.h"
#include "Components/BoxComponent.h"
#include "Net/UnrealNetwork.h"
#include "Engine/World.h"
#include "EngineUtils.h"

ADCCGauntletEncounter::ADCCGauntletEncounter()
{
	// Same cadence as ADCCEncounter: cheap enough to leave running, frequent
	// enough that a replacement spawn never reads as a delayed one.
	PrimaryActorTick.bCanEverTick = true;
	PrimaryActorTick.TickInterval = 0.25f;

	bReplicates = true;
	bAlwaysRelevant = true;

	StartVolume = CreateDefaultSubobject<UBoxComponent>(TEXT("StartVolume"));
	SetRootComponent(StartVolume);
	StartVolume->SetBoxExtent(FVector(300.0f, 300.0f, 250.0f));
	StartVolume->SetCollisionEnabled(ECollisionEnabled::QueryOnly);
	StartVolume->SetCollisionResponseToAllChannels(ECR_Ignore);
	StartVolume->SetCollisionResponseToChannel(ECC_Pawn, ECR_Overlap);
	// Deliberately no SetCollisionObjectType override here, unlike
	// DCCFlightRing::PassThroughVolume - that fix was needed because the
	// ROVER overrides its own collision to Block almost everything. A plain
	// crawler capsule uses the stock Pawn preset, which already responds
	// Overlap to a QueryOnly WorldDynamic volume, so the default object type
	// is correct as-is (same pattern ADCCArrivalTrigger's Trigger uses).
}

void ADCCGauntletEncounter::BeginPlay()
{
	Super::BeginPlay();

	StartVolume->OnComponentBeginOverlap.AddDynamic(this, &ADCCGauntletEncounter::HandleStartVolumeOverlap);
}

void ADCCGauntletEncounter::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);

	DOREPLIFETIME(ADCCGauntletEncounter, State);
	DOREPLIFETIME(ADCCGauntletEncounter, KillCount);
}

void ADCCGauntletEncounter::HandleStartVolumeOverlap(UPrimitiveComponent*, AActor* OtherActor,
	UPrimitiveComponent*, int32, bool, const FHitResult&)
{
	if (!HasAuthority() || State != EDCCGauntletState::Dormant)
	{
		return;
	}

	const ADCCCharacterBase* Crawler = Cast<ADCCCharacterBase>(OtherActor);
	if (Crawler && Crawler->IsCrawler())
	{
		StartGauntlet();
	}
}

void ADCCGauntletEncounter::StartGauntlet()
{
	if (!HasAuthority())
	{
		UE_LOG(LogTemp, Warning, TEXT("DCCGauntletEncounter::StartGauntlet called on a client; ignoring."));
		return;
	}

	if (State != EDCCGauntletState::Dormant)
	{
		return;
	}

	if (EnemyPool.IsEmpty())
	{
		UE_LOG(LogTemp, Warning, TEXT("DCCGauntletEncounter '%s' has an empty EnemyPool; refusing to start."), *GetName());
		return;
	}

	KillCount = 0;
	OnRep_KillCount(); // Server doesn't get its own OnRep.
	SetState(EDCCGauntletState::Active);

	ADCCGameState::Log(this, EDCCLogCategory::Encounter, FText::Format(
		NSLOCTEXT("DCC", "GauntletStart", "The gauntlet begins. {0} to go."),
		FText::AsNumber(TargetKillCount)));
}

void ADCCGauntletEncounter::Tick(float DeltaSeconds)
{
	Super::Tick(DeltaSeconds);

	if (!HasAuthority() || State != EDCCGauntletState::Active)
	{
		return;
	}

	const UWorld* World = GetWorld();
	if (!World)
	{
		return;
	}

	if (World->GetTimeSeconds() - LastSpawnTime < MinSpawnGapSeconds)
	{
		return;
	}

	// KillCount + LivingSpawns.Num() is what's left to ever spawn, not just
	// what's alive right now - without this, the last couple of kills would
	// keep topping the field back up to MaxConcurrentEnemies past the target.
	while (LivingSpawns.Num() < MaxConcurrentEnemies
		&& KillCount + LivingSpawns.Num() < TargetKillCount)
	{
		SpawnOne();
	}
}

void ADCCGauntletEncounter::SpawnOne()
{
	UWorld* World = GetWorld();
	if (!World || EnemyPool.IsEmpty())
	{
		return;
	}

	const TSubclassOf<ADCCEnemyCharacter> EnemyClass = EnemyPool[FMath::RandRange(0, EnemyPool.Num() - 1)];
	if (!EnemyClass)
	{
		return;
	}

	const FVector RelativeLocation = SpawnPoints.IsEmpty()
		? FVector::ZeroVector
		: SpawnPoints[FMath::RandRange(0, SpawnPoints.Num() - 1)];
	const FVector SpawnLocation = GetActorTransform().TransformPosition(RelativeLocation);

	FActorSpawnParameters SpawnParams;
	SpawnParams.Owner = this;
	// Same reasoning as ADCCEncounter::SpawnWave: hand-placed points can clip
	// geometry, and eating the spawn silently would make the field shrink
	// below MaxConcurrentEnemies for no visible reason.
	SpawnParams.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AdjustIfPossibleButAlwaysSpawn;

	ADCCEnemyCharacter* Enemy = World->SpawnActor<ADCCEnemyCharacter>(
		EnemyClass, SpawnLocation, GetActorRotation(), SpawnParams);
	if (!Enemy)
	{
		return;
	}

	if (UDCCStatsComponent* EnemyStats = Enemy->Stats)
	{
		EnemyStats->Level = SpawnLevel;
		EnemyStats->RefreshDerivedStats(/*bRefillToFull=*/true);
	}

	Enemy->OnCharacterDied.AddDynamic(this, &ADCCGauntletEncounter::HandleEnemyDied);
	LivingSpawns.Add(Enemy);
	LastSpawnTime = World->GetTimeSeconds();
}

void ADCCGauntletEncounter::HandleEnemyDied(ADCCCharacterBase* Character)
{
	if (!HasAuthority() || State != EDCCGauntletState::Active)
	{
		return;
	}

	ADCCEnemyCharacter* Enemy = Cast<ADCCEnemyCharacter>(Character);
	if (!Enemy || !LivingSpawns.Contains(Enemy))
	{
		return;
	}

	LivingSpawns.Remove(Enemy);
	++KillCount;
	OnRep_KillCount();

	// Every ten rather than every kill - a running count is already visible
	// on the HUD gauge, so the feed only needs to mark the milestones.
	if (KillCount % 10 == 0 && KillCount < TargetKillCount)
	{
		ADCCGameState::Log(this, EDCCLogCategory::Encounter, FText::Format(
			NSLOCTEXT("DCC", "GauntletProgress", "{0} / {1}."),
			FText::AsNumber(KillCount), FText::AsNumber(TargetKillCount)));
	}

	if (KillCount >= TargetKillCount)
	{
		AwardVictory();
	}
}

void ADCCGauntletEncounter::AwardVictory()
{
	UWorld* World = GetWorld();
	if (!World)
	{
		return;
	}

	SetState(EDCCGauntletState::Victory);

	ADCCGameState::Log(this, EDCCLogCategory::Announcement, FText::Format(
		NSLOCTEXT("DCC", "GauntletCleared", "The gauntlet is cleared. {0} down. Production is thrilled."),
		FText::AsNumber(TargetKillCount)));

	// Gold, not experience. UDCCRuleset::GetExperienceForEncounterClear scales
	// with enemy count (40 * Count * Level) precisely because a normal
	// encounter's Count is a handful - feeding it TargetKillCount (100) would
	// hand out several times the entire 1500 XP needed to hit the level-5
	// tutorial cap (see CLAUDE.md's levelling section) in one fight, which
	// defeats the whole point of that cap. This is a side arena, not a floor
	// clear, so its reward stays in gold and bragging rights.
	for (TActorIterator<ADCCCharacterBase> It(World); It; ++It)
	{
		ADCCCharacterBase* Character = *It;
		if (!Character || !Character->IsCrawler() || !Character->IsAlive())
		{
			continue;
		}

		if (Character->Stats && VictoryGoldReward > 0)
		{
			Character->Stats->AddGold(VictoryGoldReward);
		}

		// Reused rather than a new achievement stat: a cleared gauntlet is,
		// in every way an achievement cares about, a cleared encounter.
		UDCCAchievementComponent::RecordFor(Character, EDCCAchievementStat::EncountersCleared);
	}

	// Reported by actor name, matching ADCCEncounter::AwardClearExperience -
	// a quest can name this specific gauntlet, or use an empty TargetId to
	// mean "any."
	UDCCQuestComponent::NotifyEvent(this, EDCCObjectiveType::Custom, TEXT("GauntletCleared"), 1);
}

void ADCCGauntletEncounter::ResetGauntlet()
{
	if (!HasAuthority())
	{
		UE_LOG(LogTemp, Warning, TEXT("DCCGauntletEncounter::ResetGauntlet called on a client; ignoring."));
		return;
	}

	for (const TObjectPtr<ADCCEnemyCharacter>& Enemy : LivingSpawns)
	{
		if (Enemy)
		{
			Enemy->OnCharacterDied.RemoveDynamic(this, &ADCCGauntletEncounter::HandleEnemyDied);
			Enemy->Destroy();
		}
	}
	LivingSpawns.Reset();

	KillCount = 0;
	OnRep_KillCount();
	LastSpawnTime = 0.0f;

	SetState(EDCCGauntletState::Dormant);
}

ADCCGauntletEncounter* ADCCGauntletEncounter::FindNearest(const UObject* WorldContext, FVector Location)
{
	const UWorld* World = GEngine ? GEngine->GetWorldFromContextObject(WorldContext, EGetWorldErrorMode::ReturnNull) : nullptr;
	if (!World)
	{
		return nullptr;
	}

	ADCCGauntletEncounter* Nearest = nullptr;
	float NearestDistSq = TNumericLimits<float>::Max();

	for (TActorIterator<ADCCGauntletEncounter> It(World); It; ++It)
	{
		ADCCGauntletEncounter* Candidate = *It;
		if (!Candidate)
		{
			continue;
		}

		const float DistSq = FVector::DistSquared(Candidate->GetActorLocation(), Location);
		if (DistSq < NearestDistSq)
		{
			NearestDistSq = DistSq;
			Nearest = Candidate;
		}
	}

	return Nearest;
}

void ADCCGauntletEncounter::SetState(EDCCGauntletState NewState)
{
	if (State == NewState)
	{
		return;
	}

	State = NewState;
	OnRep_State(); // Server doesn't get its own OnRep.
}

void ADCCGauntletEncounter::OnRep_State()
{
	OnGauntletStateChanged.Broadcast(State);
}

void ADCCGauntletEncounter::OnRep_KillCount()
{
	OnKillCountChanged.Broadcast(KillCount);
}
