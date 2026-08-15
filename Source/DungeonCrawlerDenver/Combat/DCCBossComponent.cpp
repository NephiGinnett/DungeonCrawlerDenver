#include "Combat/DCCBossComponent.h"

#include "Characters/DCCCharacterBase.h"
#include "Stats/DCCStatsComponent.h"
#include "Rooms/DCCEncounter.h"
#include "Combat/DCCGameState.h"
#include "UI/DCCEventLogTypes.h"
#include "Engine/World.h"
#include "EngineUtils.h"
#include "Net/UnrealNetwork.h"

UDCCBossComponent::UDCCBossComponent()
{
	// Ticks to advance the charge. Five times a second is plenty for a nine-second
	// wind-up and keeps the replicated progress value from churning every frame.
	PrimaryComponentTick.bCanEverTick = true;
	PrimaryComponentTick.TickInterval = 0.2f;

	SetIsReplicatedByDefault(true);
}

void UDCCBossComponent::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);

	DOREPLIFETIME(UDCCBossComponent, ChargeProgress);
	DOREPLIFETIME(UDCCBossComponent, bCharging);
	DOREPLIFETIME(UDCCBossComponent, bVulnerable);
}

void UDCCBossComponent::BeginPlay()
{
	Super::BeginPlay();

	if (ADCCCharacterBase* Character = Cast<ADCCCharacterBase>(GetOwner()))
	{
		// Bound on every machine: the lien only pays out on the server, but the
		// binding is harmless elsewhere and keeps the two paths identical.
		Character->OnCharacterDied.AddDynamic(this, &UDCCBossComponent::HandleBossDied);
	}
}

UDCCStatsComponent* UDCCBossComponent::GetOwnerStats() const
{
	const ADCCCharacterBase* Character = Cast<ADCCCharacterBase>(GetOwner());

	return Character ? Character->Stats : nullptr;
}

ADCCEncounter* UDCCBossComponent::GetOwningEncounter() const
{
	// The encounter sets itself as the spawn Owner, so this is a direct lookup
	// rather than a search. A boss placed by hand in the level has no owner, which
	// is fine - it simply has no minions to count.
	return GetOwner() ? Cast<ADCCEncounter>(GetOwner()->GetOwner()) : nullptr;
}

int32 UDCCBossComponent::GetLivingMinionCount() const
{
	const ADCCEncounter* Encounter = GetOwningEncounter();
	if (!Encounter)
	{
		return 0;
	}

	// The encounter counts the boss among its living enemies, so subtract it -
	// otherwise the charge would always read one minion higher than the party can
	// see, and the arithmetic on screen would not match the arithmetic they did.
	const ADCCCharacterBase* Self = Cast<ADCCCharacterBase>(GetOwner());
	const int32 SelfAlive = (Self && Self->IsAlive()) ? 1 : 0;

	return FMath::Max(0, Encounter->GetLivingEnemyCount() - SelfAlive);
}

int32 UDCCBossComponent::GetProjectedChargeDamage() const
{
	return ChargeBaseDamage + (GetLivingMinionCount() * ChargeDamagePerMinion);
}

void UDCCBossComponent::BeginCharge()
{
	if (GetOwnerRole() != ROLE_Authority || bCharging)
	{
		return;
	}

	const ADCCCharacterBase* Self = Cast<ADCCCharacterBase>(GetOwner());
	if (!Self || !Self->IsAlive())
	{
		return;
	}

	bCharging = true;
	bHasCharged = true;
	ChargeProgress = 0.0f;

	if (!ChargeStartLine.IsEmpty())
	{
		ADCCGameState::Log(this, EDCCLogCategory::Announcement, ChargeStartLine);
	}

	OnChargeStarted.Broadcast(this);
}

void UDCCBossComponent::CancelCharge()
{
	if (GetOwnerRole() != ROLE_Authority || !bCharging)
	{
		return;
	}

	bCharging = false;
	ChargeProgress = 0.0f;
}

void UDCCBossComponent::ResolveCharge()
{
	bCharging = false;
	ChargeProgress = 0.0f;

	UWorld* World = GetWorld();
	AActor* Owner = GetOwner();
	if (!World || !Owner)
	{
		return;
	}

	const int32 Minions = GetLivingMinionCount();
	const int32 Damage = ChargeBaseDamage + (Minions * ChargeDamagePerMinion);

	int32 Hit = 0;

	for (TActorIterator<ADCCCharacterBase> It(World); It; ++It)
	{
		ADCCCharacterBase* Character = *It;
		if (!Character || !Character->IsCrawler() || !Character->IsAlive() || !Character->Stats)
		{
			continue;
		}

		if (ChargeRadius > 0.0f
			&& FVector::Dist(Owner->GetActorLocation(), Character->GetActorLocation()) > ChargeRadius)
		{
			continue;
		}

		Character->Stats->ApplyDamage(Damage);
		++Hit;
	}

	// Two different lines, because clearing the minions first has to *read* as a
	// win. Same damage number would be a mechanical reward with no acknowledgement.
	const FText Line = (Minions == 0 && !ChargeFizzledLine.IsEmpty())
		? ChargeFizzledLine
		: ChargeLandedLine;

	if (!Line.IsEmpty())
	{
		ADCCGameState::Log(this, EDCCLogCategory::Announcement, Line);
	}

	ADCCGameState::Log(this, EDCCLogCategory::Combat, FText::Format(
		NSLOCTEXT("DCC", "ChargeResolved", "{0} for {1} to {2} crawler(s). {3} collector(s) still standing."),
		FText::FromString(Owner->GetActorNameOrLabel()),
		FText::AsNumber(Damage),
		FText::AsNumber(Hit),
		FText::AsNumber(Minions)));

	bVulnerable = true;
	VulnerableTimeRemaining = VulnerableSecondsAfterCharge;

	OnChargeResolved.Broadcast(this, Damage);
}

void UDCCBossComponent::HandleBossDied(ADCCCharacterBase* Character)
{
	if (GetOwnerRole() != ROLE_Authority)
	{
		return;
	}

	bCharging = false;
	bVulnerable = false;

	if (LienGold <= 0)
	{
		return;
	}

	UWorld* World = GetWorld();
	if (!World)
	{
		return;
	}

	// Gathered first so the pick is uniform. Picking during iteration would bias
	// towards whoever the actor iterator happened to reach last.
	TArray<ADCCCharacterBase*> Candidates;
	for (TActorIterator<ADCCCharacterBase> It(World); It; ++It)
	{
		ADCCCharacterBase* Crawler = *It;
		if (Crawler && Crawler->IsCrawler() && Crawler->IsAlive() && Crawler->Stats)
		{
			Candidates.Add(Crawler);
		}
	}

	if (Candidates.IsEmpty())
	{
		return;
	}

	ADCCCharacterBase* Unlucky = Candidates[FMath::RandRange(0, Candidates.Num() - 1)];

	// Charged immediately rather than modelled as a recurring debuff. The
	// recurring version needs a status-effect system that does not exist yet, and
	// a debt that takes your gold the moment you win is legible now and can be
	// upgraded later without changing what it means.
	const int32 Taken = Unlucky->Stats->StealGold(LienGold);

	const FText Line = LienLine.IsEmpty()
		? FText::Format(
			NSLOCTEXT("DCC", "LienGeneric",
				"The lien transfers to {0}. {1} gold is collected before the body hits the floor."),
			FText::FromString(Unlucky->GetActorNameOrLabel()),
			FText::AsNumber(Taken))
		: LienLine;

	ADCCGameState::Log(this, EDCCLogCategory::Announcement, Line);
}

void UDCCBossComponent::TickComponent(float DeltaSeconds, ELevelTick TickType,
	FActorComponentTickFunction* ThisTickFunction)
{
	Super::TickComponent(DeltaSeconds, TickType, ThisTickFunction);

	if (GetOwnerRole() != ROLE_Authority)
	{
		return;
	}

	if (VulnerableTimeRemaining > 0.0f)
	{
		VulnerableTimeRemaining -= DeltaSeconds;
		if (VulnerableTimeRemaining <= 0.0f)
		{
			bVulnerable = false;
		}
	}

	const ADCCCharacterBase* Self = Cast<ADCCCharacterBase>(GetOwner());
	const UDCCStatsComponent* Stats = GetOwnerStats();
	if (!Self || !Stats || !Self->IsAlive())
	{
		return;
	}

	if (bCharging)
	{
		ChargeProgress = FMath::Clamp(ChargeProgress + (DeltaSeconds / ChargeDurationSeconds), 0.0f, 1.0f);

		if (ChargeProgress >= 1.0f)
		{
			ResolveCharge();
		}

		return;
	}

	// Fires once, on crossing the threshold. bHasCharged rather than a health
	// comparison each tick, so healing the boss above the line cannot re-arm it.
	if (!bHasCharged && ChargeHealthThreshold > 0.0f
		&& Stats->GetHealthFraction() <= ChargeHealthThreshold)
	{
		BeginCharge();
	}
}
