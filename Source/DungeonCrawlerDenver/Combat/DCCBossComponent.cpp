#include "Combat/DCCBossComponent.h"

#include "Characters/DCCCharacterBase.h"
#include "Characters/DCCEnemyCharacter.h"
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
	DOREPLIFETIME(UDCCBossComponent, CurrentPhase);
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

	++CurrentPhase;
	++PhasesTriggered;

	// Captured at the start of every charge rather than once at BeginPlay: the
	// arena floor is not flat, and he does not necessarily launch from where he
	// spawned.
	GroundedZ = Self->GetActorLocation().Z;

	// First body arrives almost immediately, so the channel reads as dangerous
	// from the opening second rather than after a quiet interval.
	MinionSpawnCountdown = 0.5f;

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

	// Interrupted: he drops. The balloon deflating and the boss thumping back
	// into melee reach is the party's reward for breaking the channel, and it
	// has to be visible from the floor to land as one.
	SettleToGround();
}

void UDCCBossComponent::ResolveCharge()
{
	bCharging = false;

	// Down first, so the blast originates from ground level and the recovery
	// window happens somewhere the party can actually reach him.
	SettleToGround();

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

		TickBalloon(DeltaSeconds);

		if (ChargeProgress >= 1.0f)
		{
			ResolveCharge();
		}

		return;
	}

	// Multi-phase when thresholds are authored, single-shot otherwise. The
	// fallback keeps every boss written before phases existed behaving exactly
	// as it did rather than silently losing its one charge.
	if (PhaseThresholds.Num() > 0)
	{
		if (PhasesTriggered < PhaseThresholds.Num()
			&& Stats->GetHealthFraction() <= PhaseThresholds[PhasesTriggered])
		{
			BeginCharge();
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

float UDCCBossComponent::GetIncomingDamageMultiplier() const
{
	if (!bCharging)
	{
		return 1.0f;
	}

	// Scales with inflation rather than being a flat toggle, so hitting him early
	// in the channel is worth more than hitting him late. That is what makes
	// reacting fast mechanically better than reacting eventually.
	return FMath::Clamp(1.0f - ChargeDamageResistance * ChargeProgress, 0.05f, 1.0f);
}

void UDCCBossComponent::TickBalloon(float DeltaSeconds)
{
	AActor* Owner = GetOwner();
	if (!Owner)
	{
		return;
	}

	// Lift. Absolute from the height he started at, not additive per frame -
	// integrating a delta would drift him upward a little more on every charge
	// and eventually park him in the ceiling.
	const float TargetZ = GroundedZ + ChargeLiftHeight * ChargeProgress;
	FVector Location = Owner->GetActorLocation();
	Location.Z = FMath::FInterpTo(Location.Z, TargetZ, DeltaSeconds, 3.0f);
	Owner->SetActorLocation(Location);

	if (MinionSpawnInterval <= 0.0f || !MinionClass)
	{
		return;
	}

	MinionSpawnCountdown -= DeltaSeconds;
	if (MinionSpawnCountdown <= 0.0f)
	{
		MinionSpawnCountdown = MinionSpawnInterval;

		// Later phases arrive thicker. One extra body per phase reads clearly from
		// the floor without turning phase three into an unwinnable wall.
		const int32 Count = 1 + MinionsPerPhaseBonus * FMath::Max(0, CurrentPhase - 1);
		for (int32 Index = 0; Index < Count; ++Index)
		{
			SpawnChargeMinion();
		}
	}
}

void UDCCBossComponent::SpawnChargeMinion()
{
	UWorld* World = GetWorld();
	AActor* Owner = GetOwner();
	if (!World || !Owner || !MinionClass)
	{
		return;
	}

	// Placed on the ground beneath him rather than at his altitude, so the swarm
	// lands between the party and the boss instead of raining out of the sky.
	const FVector2D Offset = FMath::RandPointInCircle(MinionSpawnRadius);
	FVector SpawnLocation(
		Owner->GetActorLocation().X + Offset.X,
		Owner->GetActorLocation().Y + Offset.Y,
		GroundedZ);

	// GroundedZ is the boss's OWN floor height, captured at his own XY when the
	// charge began (see the comment where it's set) - reusing it verbatim here
	// is wrong on any arena floor that isn't perfectly flat, since Offset can
	// land the minion meters away over a rise or a dip. Re-trace straight down
	// at the actual spawn XY instead, same pattern as ADCCCrowdSpawner's
	// ground-snap, so a minion lands ON the floor there rather than embedded in
	// it (or floating above it) - the likely cause of a charge-spawned minion
	// turning up half-buried in terrain and only reachable at range.
	{
		FHitResult Hit;
		const FVector TraceStart = SpawnLocation + FVector(0.0f, 0.0f, 500.0f);
		const FVector TraceEnd = SpawnLocation - FVector(0.0f, 0.0f, 2000.0f);

		FCollisionQueryParams Params;
		Params.AddIgnoredActor(Owner);

		if (World->LineTraceSingleByChannel(Hit, TraceStart, TraceEnd, ECC_WorldStatic, Params))
		{
			SpawnLocation.Z = Hit.Location.Z;
		}
		// Nothing found straight down (a hole, or off the edge of the floor):
		// fall back to GroundedZ rather than skip the spawn outright - a charge
		// minion missing entirely reads as a bigger bug than one that lands a
		// little off, and the charge mechanic expects a fixed body count.
	}

	// Owned by the encounter, same as a wave-authored spawn - without this,
	// ADCCEnemyCharacter::OnDamageDealt's Cast<ADCCEncounter>(GetOwner()) lookup
	// (Repossession Nip's "who do I heal" question) fails for anything the
	// balloon spits out.
	ADCCEncounter* Encounter = GetOwningEncounter();

	FActorSpawnParameters Params;
	Params.Owner = Encounter ? static_cast<AActor*>(Encounter) : Owner;
	Params.SpawnCollisionHandlingOverride =
		ESpawnActorCollisionHandlingMethod::AdjustIfPossibleButAlwaysSpawn;

	ADCCCharacterBase* Minion = World->SpawnActor<ADCCCharacterBase>(MinionClass, SpawnLocation, FRotator::ZeroRotator, Params);

	// Folded into the encounter's own roster so GetLivingEnemyCount() (which
	// ChargeDamagePerMinion reads when the charge lands) can actually see it -
	// previously nothing registered these with SpawnedEnemies at all, so every
	// charge-spawned Debt Eater was mechanically invisible to the mechanic it
	// exists to feed.
	if (Encounter)
	{
		if (ADCCEnemyCharacter* MinionEnemy = Cast<ADCCEnemyCharacter>(Minion))
		{
			Encounter->RegisterSpawnedEnemy(MinionEnemy);
		}
	}
}

void UDCCBossComponent::SettleToGround()
{
	if (AActor* Owner = GetOwner())
	{
		FVector Location = Owner->GetActorLocation();
		Location.Z = GroundedZ;
		Owner->SetActorLocation(Location);
	}

	ChargeProgress = 0.0f;
}
