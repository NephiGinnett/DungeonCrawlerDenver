#include "Favor/DCCFavorComponent.h"

#include "Progression/DCCAchievementComponent.h"
#include "Engine/World.h"
#include "Net/UnrealNetwork.h"

UDCCFavorComponent::UDCCFavorComponent()
{
	PrimaryComponentTick.bCanEverTick = false;
	SetIsReplicatedByDefault(true);
}

void UDCCFavorComponent::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);

	DOREPLIFETIME(UDCCFavorComponent, Favor);
	DOREPLIFETIME(UDCCFavorComponent, RecentChanges);
}

EDCCFavorTier UDCCFavorComponent::GetTierForFavor(int32 InFavor) const
{
	// Walk downward so the highest satisfied threshold wins, and so a shorter or
	// longer authored threshold list still behaves sensibly.
	for (int32 Index = TierThresholds.Num() - 1; Index >= 0; --Index)
	{
		if (InFavor >= TierThresholds[Index])
		{
			const int32 Tier = FMath::Min(Index + 1, static_cast<int32>(EDCCFavorTier::MAX) - 1);
			return static_cast<EDCCFavorTier>(Tier);
		}
	}

	return EDCCFavorTier::Unwatched;
}

float UDCCFavorComponent::GetTierProgress() const
{
	const EDCCFavorTier Tier = GetTier();
	const int32 TierIndex = static_cast<int32>(Tier);

	// Lower bound of the current tier, and of the next one.
	const int32 Floor = (TierIndex == 0 || !TierThresholds.IsValidIndex(TierIndex - 1))
		? MinFavor
		: TierThresholds[TierIndex - 1];

	const int32 Ceiling = TierThresholds.IsValidIndex(TierIndex)
		? TierThresholds[TierIndex]
		: MaxFavor;

	const int32 Span = Ceiling - Floor;
	if (Span <= 0)
	{
		return 1.0f;
	}

	return FMath::Clamp(static_cast<float>(Favor - Floor) / static_cast<float>(Span), 0.0f, 1.0f);
}

void UDCCFavorComponent::AwardFavor(int32 Delta, EDCCFavorEvent Reason, const FText& Note)
{
	if (GetOwnerRole() != ROLE_Authority)
	{
		UE_LOG(LogTemp, Warning, TEXT("DCCFavorComponent::AwardFavor called on a client; ignoring. Favor is server-authoritative."));
		return;
	}

	if (Delta == 0)
	{
		return;
	}

	const EDCCFavorTier OldTier = GetTier();
	const int32 Previous = Favor;

	Favor = FMath::Clamp(Favor + Delta, MinFavor, MaxFavor);

	// Clamping can swallow the whole award; report what actually landed rather
	// than what was asked for, so the ledger never lies to the narrator.
	const int32 AppliedDelta = Favor - Previous;
	if (AppliedDelta == 0)
	{
		return;
	}

	RecordChange(AppliedDelta, Reason, Note, OldTier);
}

void UDCCFavorComponent::SetFavor(int32 NewFavor, const FText& Note)
{
	if (GetOwnerRole() != ROLE_Authority)
	{
		UE_LOG(LogTemp, Warning, TEXT("DCCFavorComponent::SetFavor called on a client; ignoring."));
		return;
	}

	const EDCCFavorTier OldTier = GetTier();
	const int32 Previous = Favor;

	Favor = FMath::Clamp(NewFavor, MinFavor, MaxFavor);

	const int32 AppliedDelta = Favor - Previous;
	if (AppliedDelta == 0)
	{
		return;
	}

	RecordChange(AppliedDelta, EDCCFavorEvent::DirectorCall, Note, OldTier);
}

void UDCCFavorComponent::RecordChange(int32 Delta, EDCCFavorEvent Reason, const FText& Note, EDCCFavorTier OldTier)
{
	FDCCFavorChange Change;
	Change.Reason = Reason;
	Change.Delta = Delta;
	Change.NewTotal = Favor;
	Change.Note = Note;
	Change.TimeSeconds = GetWorld() ? GetWorld()->GetTimeSeconds() : 0.0f;

	RecentChanges.Add(Change);
	while (RecentChanges.Num() > FMath::Max(1, MaxLedgerEntries))
	{
		RecentChanges.RemoveAt(0);
	}

	// The server does not receive its own OnRep, so broadcast here; clients get
	// the equivalent from OnRep_Favor.
	OnFavorChanged.Broadcast(this, Change);

	const EDCCFavorTier NewTier = GetTier();
	if (NewTier != OldTier)
	{
		UE_LOG(LogTemp, Log, TEXT("DCCFavor: %s moved from tier %d to %d (favor %d)."),
			*GetNameSafe(GetOwner()), static_cast<int32>(OldTier), static_cast<int32>(NewTier), Favor);

		OnTierChanged.Broadcast(this, NewTier, OldTier);

		// "Sponsor Of The Week" - reaching the top tier at least once. This
		// function only ever runs from AwardFavor's already-authority-gated
		// path (OnRep_Favor has its own separate client-side tier broadcast
		// below, which never reaches here), so no further authority check is
		// needed.
		if (NewTier == EDCCFavorTier::Headliner)
		{
			UDCCAchievementComponent::RecordFor(GetOwner(), EDCCAchievementStat::FavorTierMaxed);
		}
	}
}

void UDCCFavorComponent::OnRep_Favor(int32 PreviousFavor)
{
	const EDCCFavorTier OldTier = GetTierForFavor(PreviousFavor);
	const EDCCFavorTier NewTier = GetTier();

	// Reconstruct the change from what replicated. The ledger entry itself
	// arrives separately, so prefer its detail when it has caught up.
	FDCCFavorChange Change;
	if (RecentChanges.Num() > 0 && RecentChanges.Last().NewTotal == Favor)
	{
		Change = RecentChanges.Last();
	}
	else
	{
		Change.Delta = Favor - PreviousFavor;
		Change.NewTotal = Favor;
	}

	OnFavorChanged.Broadcast(this, Change);

	if (NewTier != OldTier)
	{
		OnTierChanged.Broadcast(this, NewTier, OldTier);
	}
}
