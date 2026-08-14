#include "Rooms/DCCEncounter.h"

#include "Characters/DCCEnemyCharacter.h"
#include "Characters/DCCCharacterBase.h"
#include "Stats/DCCStatsComponent.h"
#include "Skills/DCCSkillComponent.h"
#include "Components/SceneComponent.h"
#include "Net/UnrealNetwork.h"
#include "Engine/World.h"
#include "EngineUtils.h"

ADCCEncounter::ADCCEncounter()
{
	PrimaryActorTick.bCanEverTick = false;

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

	FActorSpawnParameters SpawnParams;
	SpawnParams.Owner = this;
	// Enemies are placed by hand at authored points, so nudge them out of any
	// geometry they clip rather than refusing to spawn and silently emptying
	// the fight.
	SpawnParams.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AdjustIfPossibleButAlwaysSpawn;

	for (const FDCCEnemySpawn& Spawn : Enemies)
	{
		if (!Spawn.EnemyClass)
		{
			UE_LOG(LogTemp, Warning, TEXT("DCCEncounter '%s' has a spawn entry with no EnemyClass set; skipping it."), *GetName());
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
	}

	if (SpawnedEnemies.Num() == 0)
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

	if (GetLivingEnemyCount() == 0)
	{
		UE_LOG(LogTemp, Log, TEXT("DCCEncounter '%s' cleared."), *GetName());
		SetState(EDCCEncounterState::Cleared);
	}
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

	SetState(EDCCEncounterState::Dormant);
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
		AwardSkillAdvancement();
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
				UE_LOG(LogTemp, Log, TEXT("DCCEncounter '%s': %s advanced %d skill(s)."),
					*GetName(), *GetNameSafe(Character), Advanced);
			}
		}
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
