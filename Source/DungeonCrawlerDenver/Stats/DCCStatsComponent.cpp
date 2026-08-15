#include "Stats/DCCStatsComponent.h"

#include "Stats/DCCRuleset.h"
#include "Net/UnrealNetwork.h"

UDCCStatsComponent::UDCCStatsComponent()
{
	PrimaryComponentTick.bCanEverTick = false;
	bWantsInitializeComponent = true;
	SetIsReplicatedByDefault(true);

	RulesetClass = UDCCRuleset::StaticClass();
}

void UDCCStatsComponent::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);

	DOREPLIFETIME(UDCCStatsComponent, AbilityScores);
	DOREPLIFETIME(UDCCStatsComponent, CurrentHitPoints);
	DOREPLIFETIME(UDCCStatsComponent, MaxHitPoints);
	DOREPLIFETIME(UDCCStatsComponent, CurrentManaPoints);
	DOREPLIFETIME(UDCCStatsComponent, MaxManaPoints);
	DOREPLIFETIME(UDCCStatsComponent, CurrentStamina);
	DOREPLIFETIME(UDCCStatsComponent, MaxStamina);
	DOREPLIFETIME(UDCCStatsComponent, bIsPlayerCharacter);
	DOREPLIFETIME(UDCCStatsComponent, ExperiencePoints);
	DOREPLIFETIME(UDCCStatsComponent, ExperienceToNextLevel);
}

void UDCCStatsComponent::InitializeComponent()
{
	Super::InitializeComponent();

	// Only the server establishes starting values; clients get them by replication.
	if (GetOwnerRole() == ROLE_Authority)
	{
		RecalculateDerivedStats(/*bRefillToFull=*/true);
	}
}

UDCCRuleset* UDCCStatsComponent::GetRuleset() const
{
	if (CachedRuleset)
	{
		return CachedRuleset;
	}

	// Both branches are spelled as TSubclassOf on purpose: a bare StaticClass()
	// returns UClass*, and the mixed ternary is ambiguous under MSVC.
	const TSubclassOf<UDCCRuleset> FallbackClass = UDCCRuleset::StaticClass();
	const TSubclassOf<UDCCRuleset> ClassToUse = RulesetClass ? RulesetClass : FallbackClass;

	// Const-cast is contained here so callers get a clean BlueprintPure getter.
	// The ruleset is stateless, so caching it lazily is safe.
	UDCCStatsComponent* MutableThis = const_cast<UDCCStatsComponent*>(this);
	MutableThis->CachedRuleset = NewObject<UDCCRuleset>(MutableThis, ClassToUse);

	return MutableThis->CachedRuleset;
}

int32 UDCCStatsComponent::GetAbilityModifier(EDCCAbility Ability) const
{
	return GetRuleset()->GetAbilityModifier(AbilityScores.GetScore(Ability));
}

void UDCCStatsComponent::RecalculateDerivedStats(bool bRefillToFull)
{
	const int32 PreviousHitPoints = CurrentHitPoints;
	const int32 PreviousManaPoints = CurrentManaPoints;

	MaxHitPoints = GetRuleset()->GetMaxHitPoints(AbilityScores, Level);

	// The player's cushion is applied here rather than inside GetMaxHitPoints so
	// that the ruleset's own curve stays the honest answer to "how tough is this
	// character", and the concession to playability is visible as a separate step.
	if (bIsPlayerCharacter)
	{
		MaxHitPoints = FMath::Max(1,
			FMath::RoundToInt32(MaxHitPoints * GetRuleset()->GetPlayerToughnessMultiplier()));
	}

	CurrentHitPoints = bRefillToFull ? MaxHitPoints : FMath::Clamp(CurrentHitPoints, 0, MaxHitPoints);

	ExperienceToNextLevel = GetRuleset()->GetExperienceToNextLevel(Level);

	MaxManaPoints = GetRuleset()->GetMaxManaPoints(AbilityScores, Level);
	CurrentManaPoints = bRefillToFull ? MaxManaPoints : FMath::Clamp(CurrentManaPoints, 0, MaxManaPoints);

	MaxStamina = GetRuleset()->GetMaxStamina(AbilityScores, Level);
	CurrentStamina = bRefillToFull ? MaxStamina : FMath::Clamp(CurrentStamina, 0.0f, MaxStamina);

	if (CurrentHitPoints != PreviousHitPoints)
	{
		OnHealthChanged.Broadcast(this, CurrentHitPoints, CurrentHitPoints - PreviousHitPoints);
	}

	if (CurrentManaPoints != PreviousManaPoints)
	{
		OnManaChanged.Broadcast(this, CurrentManaPoints, CurrentManaPoints - PreviousManaPoints);
	}
}

bool UDCCStatsComponent::SpendMana(int32 Amount)
{
	if (GetOwnerRole() != ROLE_Authority)
	{
		UE_LOG(LogTemp, Warning, TEXT("DCCStatsComponent::SpendMana called on a client; ignoring. Mana is server-authoritative."));
		return false;
	}

	if (Amount < 0 || !CanAffordMana(Amount))
	{
		return false;
	}

	const int32 PreviousManaPoints = CurrentManaPoints;
	CurrentManaPoints -= Amount;

	if (CurrentManaPoints != PreviousManaPoints)
	{
		OnManaChanged.Broadcast(this, CurrentManaPoints, CurrentManaPoints - PreviousManaPoints);
	}

	return true;
}

void UDCCStatsComponent::RestoreMana(int32 Amount)
{
	if (GetOwnerRole() != ROLE_Authority)
	{
		UE_LOG(LogTemp, Warning, TEXT("DCCStatsComponent::RestoreMana called on a client; ignoring. Mana is server-authoritative."));
		return;
	}

	if (Amount <= 0)
	{
		return;
	}

	const int32 PreviousManaPoints = CurrentManaPoints;
	CurrentManaPoints = FMath::Min(MaxManaPoints, CurrentManaPoints + Amount);

	if (CurrentManaPoints != PreviousManaPoints)
	{
		OnManaChanged.Broadcast(this, CurrentManaPoints, CurrentManaPoints - PreviousManaPoints);
	}
}

void UDCCStatsComponent::OnRep_CurrentManaPoints(int32 PreviousManaPoints)
{
	OnManaChanged.Broadcast(this, CurrentManaPoints, CurrentManaPoints - PreviousManaPoints);
}

bool UDCCStatsComponent::ConsumeStamina(float Amount)
{
	if (GetOwnerRole() != ROLE_Authority)
	{
		return false;
	}

	if (CurrentStamina <= 0.0f)
	{
		return false;
	}

	CurrentStamina = FMath::Max(0.0f, CurrentStamina - FMath::Max(0.0f, Amount));

	// True while there is still something left to spend next frame, which is what
	// makes this usable directly as a "can keep going" test.
	return CurrentStamina > 0.0f;
}

void UDCCStatsComponent::RecoverStamina(float Amount)
{
	if (GetOwnerRole() != ROLE_Authority || Amount <= 0.0f)
	{
		return;
	}

	CurrentStamina = FMath::Min(MaxStamina, CurrentStamina + Amount);
}

void UDCCStatsComponent::ApplyDamage(int32 Amount)
{
	if (GetOwnerRole() != ROLE_Authority)
	{
		UE_LOG(LogTemp, Warning, TEXT("DCCStatsComponent::ApplyDamage called on a client; ignoring. Damage is server-authoritative."));
		return;
	}

	if (Amount <= 0 || !IsAlive())
	{
		return;
	}

	const int32 PreviousHitPoints = CurrentHitPoints;
	CurrentHitPoints = FMath::Max(0, CurrentHitPoints - Amount);

	// The server does not receive its own OnRep, so broadcast here. Clients get
	// the equivalent broadcast from OnRep_CurrentHitPoints.
	OnHealthChanged.Broadcast(this, CurrentHitPoints, CurrentHitPoints - PreviousHitPoints);

	if (CurrentHitPoints == 0 && PreviousHitPoints > 0)
	{
		OnDied.Broadcast(this);
	}
}

void UDCCStatsComponent::Heal(int32 Amount)
{
	if (GetOwnerRole() != ROLE_Authority)
	{
		UE_LOG(LogTemp, Warning, TEXT("DCCStatsComponent::Heal called on a client; ignoring. Healing is server-authoritative."));
		return;
	}

	if (Amount <= 0 || !IsAlive())
	{
		return;
	}

	const int32 PreviousHitPoints = CurrentHitPoints;
	CurrentHitPoints = FMath::Min(MaxHitPoints, CurrentHitPoints + Amount);

	if (CurrentHitPoints != PreviousHitPoints)
	{
		OnHealthChanged.Broadcast(this, CurrentHitPoints, CurrentHitPoints - PreviousHitPoints);
	}
}

FDCCCheckResult UDCCStatsComponent::ResolveCheck(EDCCAbility Ability, int32 DifficultyClass, int32 SituationalBonus)
{
	if (GetOwnerRole() != ROLE_Authority)
	{
		UE_LOG(LogTemp, Warning, TEXT("DCCStatsComponent::ResolveCheck called on a client; ignoring. Rolls are server-authoritative."));
		return FDCCCheckResult();
	}

	return GetRuleset()->ResolveCheck(AbilityScores, Ability, DifficultyClass, SituationalBonus);
}

void UDCCStatsComponent::RefreshDerivedStats(bool bRefillToFull)
{
	if (GetOwnerRole() != ROLE_Authority)
	{
		UE_LOG(LogTemp, Warning, TEXT("DCCStatsComponent::RefreshDerivedStats called on a client; ignoring. Derived stats are server-authoritative."));
		return;
	}

	// A ruleset swap or a stat change can both invalidate the cache.
	CachedRuleset = nullptr;

	RecalculateDerivedStats(bRefillToFull);
}

void UDCCStatsComponent::OnRep_CurrentHitPoints(int32 PreviousHitPoints)
{
	OnHealthChanged.Broadcast(this, CurrentHitPoints, CurrentHitPoints - PreviousHitPoints);

	if (CurrentHitPoints == 0 && PreviousHitPoints > 0)
	{
		OnDied.Broadcast(this);
	}
}

void UDCCStatsComponent::SetIsPlayerCharacter(bool bNewIsPlayerCharacter)
{
	if (GetOwnerRole() != ROLE_Authority || bIsPlayerCharacter == bNewIsPlayerCharacter)
	{
		return;
	}

	bIsPlayerCharacter = bNewIsPlayerCharacter;

	// The health fraction is preserved rather than refilled. Possession happens at
	// spawn where the two are the same, but a mid-match possession change - the DM
	// taking over an NPC, say - must not read as a free full heal.
	const float PreviousFraction = GetHealthFraction();

	RecalculateDerivedStats(/*bRefillToFull=*/false);

	CurrentHitPoints = FMath::Clamp(
		FMath::RoundToInt32(MaxHitPoints * PreviousFraction), 1, MaxHitPoints);
}

void UDCCStatsComponent::AddExperience(int32 Amount)
{
	if (GetOwnerRole() != ROLE_Authority || Amount <= 0)
	{
		return;
	}

	ExperiencePoints += Amount;
	OnExperienceChanged.Broadcast(this, Amount);

	// Loop, because a floor-clear bonus can be worth more than one level and the
	// remainder has to carry rather than evaporate.
	while (ExperienceToNextLevel > 0 && ExperiencePoints >= ExperienceToNextLevel)
	{
		ExperiencePoints -= ExperienceToNextLevel;
		++Level;

		// Levelling restores the character to full. It is the one moment the books
		// treat as a genuine reward rather than a number going up, and healing on
		// it is what makes clearing a floor feel like a checkpoint.
		RecalculateDerivedStats(/*bRefillToFull=*/true);

		OnLevelUp.Broadcast(this, Level);
	}
}

void UDCCStatsComponent::OnRep_ExperiencePoints(int32 PreviousExperience)
{
	OnExperienceChanged.Broadcast(this, ExperiencePoints - PreviousExperience);
}

void UDCCStatsComponent::OnRep_AbilityScores()
{
	// Clients mirror the server's derived values rather than recomputing them,
	// so a ruleset change can never desync max HP between machines.
	CachedRuleset = nullptr;
}
