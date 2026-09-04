#include "Progression/DCCAchievementComponent.h"

#include "Characters/DCCCharacterBase.h"
#include "Stats/DCCStatsComponent.h"
#include "Favor/DCCFavorComponent.h"
#include "Items/DCCInventoryComponent.h"
#include "Combat/DCCGameState.h"
#include "Engine/DataTable.h"
#include "Net/UnrealNetwork.h"
#include "UObject/ConstructorHelpers.h"

UDCCAchievementComponent::UDCCAchievementComponent()
{
	PrimaryComponentTick.bCanEverTick = false;
	SetIsReplicatedByDefault(true);

	// Defaulted here rather than left for a per-Blueprint assignment because the
	// failure mode is silent: no table means no rows means nothing ever fires,
	// with nothing logged. A default that is right for this project beats an
	// empty slot nobody notices. Still overridable in the Details panel.
	static ConstructorHelpers::FObjectFinder<UDataTable> DefaultTable(
		TEXT("/Game/DCC/Progression/DT_DCCAchievements.DT_DCCAchievements"));
	if (DefaultTable.Succeeded())
	{
		AchievementTable = DefaultTable.Object;
	}
}

void UDCCAchievementComponent::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);

	DOREPLIFETIME(UDCCAchievementComponent, Progress);
	DOREPLIFETIME(UDCCAchievementComponent, StatTotals);
}

void UDCCAchievementComponent::BeginPlay()
{
	Super::BeginPlay();

	// Achievements belong to players. They are global and personal - "put down 200
	// things", not "cleared room 3" - so a mob accumulating its own kill count is
	// replicated state nobody will ever read.
	//
	// Decided once here rather than checked in Record, because the answer cannot
	// change during a life and Record is called several times per swing.
	const ADCCCharacterBase* Character = Cast<ADCCCharacterBase>(GetOwner());
	bTracksAchievements = Character && Character->IsCrawler();

	if (!bTracksAchievements)
	{
		// Left empty rather than zero-filled, so a mob replicates two empty arrays
		// instead of a ten-element one.
		SetIsReplicated(false);
		return;
	}

	// Sized once so Record never has to grow it mid-fight, and so a stat added to
	// the enum later cannot index past the end of a save made before it existed.
	StatTotals.SetNumZeroed(static_cast<int32>(EDCCAchievementStat::MAX));
}

UDCCAchievementComponent* UDCCAchievementComponent::Find(AActor* Actor)
{
	return Actor ? Actor->FindComponentByClass<UDCCAchievementComponent>() : nullptr;
}

void UDCCAchievementComponent::RecordFor(AActor* Actor, EDCCAchievementStat Stat, int32 Amount)
{
	if (UDCCAchievementComponent* Component = Find(Actor))
	{
		Component->Record(Stat, Amount);
	}
}

int32 UDCCAchievementComponent::GetStatTotal(EDCCAchievementStat Stat) const
{
	const int32 Index = static_cast<int32>(Stat);

	return StatTotals.IsValidIndex(Index) ? StatTotals[Index] : 0;
}

void UDCCAchievementComponent::Record(EDCCAchievementStat Stat, int32 Amount)
{
	if (GetOwnerRole() != ROLE_Authority || Amount <= 0 || !bTracksAchievements)
	{
		return;
	}

	const int32 Index = static_cast<int32>(Stat);
	if (!StatTotals.IsValidIndex(Index))
	{
		// BeginPlay has not run yet, or the enum grew. Either way, growing here is
		// safer than dropping the record on the floor.
		StatTotals.SetNumZeroed(static_cast<int32>(EDCCAchievementStat::MAX));

		if (!StatTotals.IsValidIndex(Index))
		{
			return;
		}
	}

	StatTotals[Index] += Amount;

	EvaluateUnlocks(Stat);
}

FDCCAchievementProgress& UDCCAchievementComponent::FindOrAddProgress(FName AchievementId)
{
	for (FDCCAchievementProgress& Entry : Progress)
	{
		if (Entry.AchievementId == AchievementId)
		{
			return Entry;
		}
	}

	FDCCAchievementProgress NewEntry;
	NewEntry.AchievementId = AchievementId;

	return Progress[Progress.Add(NewEntry)];
}

void UDCCAchievementComponent::EvaluateUnlocks(EDCCAchievementStat Stat)
{
	if (!AchievementTable)
	{
		return;
	}

	const int32 Total = GetStatTotal(Stat);

	static const FString Context(TEXT("DCCAchievementComponent::EvaluateUnlocks"));

	TArray<FDCCAchievementDef*> Rows;
	AchievementTable->GetAllRows<FDCCAchievementDef>(Context, Rows);

	for (const FDCCAchievementDef* Row : Rows)
	{
		if (!Row || Row->Stat != Stat)
		{
			continue;
		}

		FDCCAchievementProgress& Entry = FindOrAddProgress(Row->AchievementId);
		Entry.Progress = Total;

		if (Entry.bUnlocked || Total < Row->Threshold)
		{
			continue;
		}

		Entry.bUnlocked = true;
		Entry.UnlockedAtSeconds = GetWorld() ? GetWorld()->GetTimeSeconds() : 0.0f;

		Unlock(*Row);
	}
}

void UDCCAchievementComponent::Unlock(const FDCCAchievementDef& Achievement)
{
	AActor* Owner = GetOwner();

	// The announcement is the whole point. An achievement nobody sees fire is a
	// counter, and the fiction is a broadcast - the audience finds out.
	const FText Line = Achievement.Announcement.IsEmpty()
		? FText::Format(
			NSLOCTEXT("DCC", "AchievementGenericLine", "Achievement unlocked: {0}."),
			Achievement.DisplayName)
		: Achievement.Announcement;

	ADCCGameState::Log(Owner, EDCCLogCategory::Achievement, Line);

	// Unlocking is not paying out. XP and Favor wait for Server_ClaimReward, so
	// a session that unlocks six achievements mid-fight gets six things to open
	// afterward rather than six numbers that ticked up somewhere off-screen.
	OnAchievementUnlocked.Broadcast(this, Achievement);

	UE_LOG(LogTemp, Log, TEXT("DCCAchievement: %s unlocked '%s'."),
		*GetNameSafe(Owner), *Achievement.AchievementId.ToString());
}

void UDCCAchievementComponent::GetPendingRewardIds(TArray<FName>& OutIds) const
{
	OutIds.Reset();

	for (const FDCCAchievementProgress& Entry : Progress)
	{
		if (Entry.bUnlocked && !Entry.bRewardClaimed)
		{
			OutIds.Add(Entry.AchievementId);
		}
	}
}

bool UDCCAchievementComponent::HasPendingRewards() const
{
	for (const FDCCAchievementProgress& Entry : Progress)
	{
		if (Entry.bUnlocked && !Entry.bRewardClaimed)
		{
			return true;
		}
	}

	return false;
}

void UDCCAchievementComponent::RequestClaimReward(FName AchievementId)
{
	if (GetOwnerRole() == ROLE_Authority)
	{
		Server_ClaimReward_Implementation(AchievementId);
		return;
	}

	Server_ClaimReward(AchievementId);
}

void UDCCAchievementComponent::Server_ClaimReward_Implementation(FName AchievementId)
{
	FDCCAchievementProgress* Entry = nullptr;
	for (FDCCAchievementProgress& Candidate : Progress)
	{
		if (Candidate.AchievementId == AchievementId)
		{
			Entry = &Candidate;
			break;
		}
	}

	// Not unlocked, or already opened - either way there is nothing to pay.
	// Re-checked here rather than trusted from the client, same reason every
	// other server entry point in this project re-checks its own gate.
	if (!Entry || !Entry->bUnlocked || Entry->bRewardClaimed || !AchievementTable)
	{
		return;
	}

	static const FString Context(TEXT("DCCAchievementClaim"));
	const FDCCAchievementDef* Row = AchievementTable->FindRow<FDCCAchievementDef>(AchievementId, Context, false);
	if (!Row)
	{
		return;
	}

	Entry->bRewardClaimed = true;

	AActor* Owner = GetOwner();

	if (Row->ExperienceReward > 0)
	{
		if (UDCCStatsComponent* Stats = Owner ? Owner->FindComponentByClass<UDCCStatsComponent>() : nullptr)
		{
			Stats->AddExperience(Row->ExperienceReward);
		}
	}

	if (Row->FavorReward != 0)
	{
		if (UDCCFavorComponent* FavorComponent = Owner ? Owner->FindComponentByClass<UDCCFavorComponent>() : nullptr)
		{
			FavorComponent->AwardFavor(
				Row->FavorReward,
				EDCCFavorEvent::DirectorCall,
				Row->DisplayName);
		}
	}

	if (Row->LootBoxReward != EDCCLootBoxTier::None)
	{
		if (UDCCInventoryComponent* Inventory = Owner ? Owner->FindComponentByClass<UDCCInventoryComponent>() : nullptr)
		{
			Inventory->GrantLootBox(Row->LootBoxReward);
		}
	}

	ADCCGameState::Log(Owner, EDCCLogCategory::Achievement, FText::Format(
		NSLOCTEXT("DCC", "AchievementRewardClaimed", "{0} opens the reward for {1}."),
		FText::FromString(GetNameSafe(Owner)), Row->DisplayName));

	OnRewardClaimed.Broadcast(this, *Row);
}

bool UDCCAchievementComponent::IsUnlocked(FName AchievementId) const
{
	for (const FDCCAchievementProgress& Entry : Progress)
	{
		if (Entry.AchievementId == AchievementId)
		{
			return Entry.bUnlocked;
		}
	}

	return false;
}

int32 UDCCAchievementComponent::GetUnlockedCount() const
{
	int32 Count = 0;

	for (const FDCCAchievementProgress& Entry : Progress)
	{
		Count += Entry.bUnlocked ? 1 : 0;
	}

	return Count;
}

void UDCCAchievementComponent::GetVisibleAchievements(TArray<FDCCAchievementDef>& OutAchievements) const
{
	OutAchievements.Reset();

	if (!AchievementTable)
	{
		return;
	}

	static const FString Context(TEXT("DCCAchievementComponent::GetVisibleAchievements"));

	TArray<FDCCAchievementDef*> Rows;
	AchievementTable->GetAllRows<FDCCAchievementDef>(Context, Rows);

	for (const FDCCAchievementDef* Row : Rows)
	{
		// A secret stays off the list until it fires, so the surprise survives a
		// player reading the menu before they have earned anything.
		if (!Row || (Row->bSecret && !IsUnlocked(Row->AchievementId)))
		{
			continue;
		}

		OutAchievements.Add(*Row);
	}
}
