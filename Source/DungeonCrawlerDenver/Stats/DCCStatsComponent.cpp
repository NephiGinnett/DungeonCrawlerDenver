#include "Stats/DCCStatsComponent.h"

#include "Stats/DCCRuleset.h"
#include "Equipment/DCCEquipmentComponent.h"
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
	DOREPLIFETIME(UDCCStatsComponent, EnhancedScores);
	DOREPLIFETIME(UDCCStatsComponent, CurrentHitPoints);
	DOREPLIFETIME(UDCCStatsComponent, MaxHitPoints);
	DOREPLIFETIME(UDCCStatsComponent, CurrentManaPoints);
	DOREPLIFETIME(UDCCStatsComponent, MaxManaPoints);
	DOREPLIFETIME(UDCCStatsComponent, CurrentStamina);
	DOREPLIFETIME(UDCCStatsComponent, MaxStamina);
	DOREPLIFETIME(UDCCStatsComponent, bIsPlayerCharacter);
	DOREPLIFETIME(UDCCStatsComponent, bIsAllyCharacter);
	DOREPLIFETIME(UDCCStatsComponent, bInvulnerable);
	DOREPLIFETIME(UDCCStatsComponent, ExperiencePoints);
	DOREPLIFETIME(UDCCStatsComponent, ExperienceToNextLevel);
	DOREPLIFETIME(UDCCStatsComponent, Gold);
}

int32 UDCCStatsComponent::GetFullHealthSegments() const
{
	if (CurrentHitPoints <= 0 || MaxHitPoints <= 0)
	{
		return 0;
	}

	const float SegmentSize = GetRuleset()->GetHealthSegmentSize(MaxHitPoints);

	return FMath::FloorToInt32((CurrentHitPoints / SegmentSize) + KINDA_SMALL_NUMBER);
}

float UDCCStatsComponent::GetPartialHealthSegmentFraction() const
{
	if (CurrentHitPoints <= 0 || MaxHitPoints <= 0)
	{
		return 0.0f;
	}

	const float SegmentSize = GetRuleset()->GetHealthSegmentSize(MaxHitPoints);
	const float Filled = CurrentHitPoints - (GetFullHealthSegments() * SegmentSize);

	return FMath::Clamp(Filled / SegmentSize, 0.0f, 1.0f);
}

int32 UDCCStatsComponent::GetRegenCeiling() const
{
	return GetRuleset()->GetSegmentRegenCeiling(CurrentHitPoints, MaxHitPoints);
}

bool UDCCStatsComponent::IsRegenSuppressed() const
{
	return TimeSinceDamage < GetRuleset()->GetRegenDelaySeconds();
}

void UDCCStatsComponent::TickRegeneration(float DeltaSeconds)
{
	if (GetOwnerRole() != ROLE_Authority || DeltaSeconds <= 0.0f)
	{
		return;
	}

	UDCCRuleset* Ruleset = GetRuleset();
	if (!Ruleset)
	{
		return;
	}

	TimeSinceDamage += DeltaSeconds;

	// Mana comes back regardless of recent damage. It is a casting resource, not a
	// wound, and gating it behind combat silence would mean a caster under
	// sustained pressure is exactly the caster who can never cast.
	if (CurrentManaPoints < MaxManaPoints)
	{
		ManaRegenCarry += Ruleset->GetManaRegenRate(MaxManaPoints) * DeltaSeconds;

		const int32 Whole = FMath::FloorToInt32(ManaRegenCarry);
		if (Whole > 0)
		{
			ManaRegenCarry -= Whole;
			RestoreMana(Whole);
		}
	}
	else
	{
		ManaRegenCarry = 0.0f;
	}

	if (!IsAlive() || IsRegenSuppressed())
	{
		// Carry is dropped rather than banked, so a long fight cannot store up a
		// burst of healing that lands the instant the party disengages.
		HealthRegenCarry = 0.0f;
		return;
	}

	const int32 Ceiling = GetRegenCeiling();
	if (CurrentHitPoints >= Ceiling)
	{
		HealthRegenCarry = 0.0f;
		return;
	}

	HealthRegenCarry += Ruleset->GetHealthRegenRate(MaxHitPoints) * DeltaSeconds;

	const int32 WholePoints = FMath::FloorToInt32(HealthRegenCarry);
	if (WholePoints <= 0)
	{
		return;
	}

	HealthRegenCarry -= WholePoints;

	// Clamped to the block ceiling, never to MaxHitPoints. This is the rule that
	// makes a lost block stay lost: regeneration finishes the block you are
	// standing in and then stops dead.
	const int32 PreviousHitPoints = CurrentHitPoints;
	CurrentHitPoints = FMath::Min(CurrentHitPoints + WholePoints, Ceiling);

	if (CurrentHitPoints != PreviousHitPoints)
	{
		OnHealthChanged.Broadcast(this, CurrentHitPoints, CurrentHitPoints - PreviousHitPoints);
	}
}

void UDCCStatsComponent::AddGold(int32 Amount)
{
	if (GetOwnerRole() != ROLE_Authority || Amount <= 0)
	{
		return;
	}

	Gold += Amount;
	OnGoldChanged.Broadcast(this, Amount);
}

bool UDCCStatsComponent::SpendGold(int32 Amount)
{
	if (GetOwnerRole() != ROLE_Authority || Amount <= 0 || Amount > Gold)
	{
		return false;
	}

	Gold -= Amount;
	OnGoldChanged.Broadcast(this, -Amount);

	return true;
}

int32 UDCCStatsComponent::StealGold(int32 Amount)
{
	if (GetOwnerRole() != ROLE_Authority || Amount <= 0 || Gold <= 0)
	{
		return 0;
	}

	// Takes what is there rather than failing. A bite that does nothing because
	// the victim is nearly broke reads as a bug; a bite that gets their last three
	// coins reads as the point.
	const int32 Taken = FMath::Min(Amount, Gold);

	Gold -= Taken;
	OnGoldChanged.Broadcast(this, -Taken);

	return Taken;
}

void UDCCStatsComponent::OnRep_Gold(int32 PreviousGold)
{
	OnGoldChanged.Broadcast(this, Gold - PreviousGold);
}

void UDCCStatsComponent::InitializeComponent()
{
	Super::InitializeComponent();

	// Only the server establishes starting values; clients get them by replication.
	if (GetOwnerRole() == ROLE_Authority)
	{
		// Seeded before anything derives from it. RecalculateDerivedStats reads
		// the Enhanced layer, so leaving this at its default would size Health and
		// Mana off a default score set rather than the authored one - and the
		// equipment component's own BeginPlay is too late, since it runs after
		// InitializeComponent and may not exist on this actor at all.
		EnhancedScores = AbilityScores;

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
	// Enhanced: a modifier is what you roll with, and gear is part of that.
	return GetRuleset()->GetAbilityModifier(EnhancedScores.GetScore(Ability));
}

void UDCCStatsComponent::RecalculateEnhancedScores(bool bRefillToFull)
{
	if (GetOwnerRole() != ROLE_Authority)
	{
		UE_LOG(LogTemp, Warning, TEXT("DCCStatsComponent::RecalculateEnhancedScores called on a client; ignoring."));
		return;
	}

	// Start from the permanent layer every time rather than adjusting in place,
	// so removing an item can never leave a residue behind. Recomputing from base
	// is the only version of this that is correct after an arbitrary sequence of
	// equips and unequips.
	EnhancedScores = AbilityScores;

	TArray<int32> Bonuses;
	if (const UDCCEquipmentComponent* Equipment = UDCCEquipmentComponent::Find(GetOwner()))
	{
		Equipment->AccumulateAbilityBonuses(Bonuses);
	}

	for (int32 Index = 0; Index < Bonuses.Num(); ++Index)
	{
		if (!EnhancedScores.Scores.IsValidIndex(Index) || Bonuses[Index] == 0)
		{
			continue;
		}

		// Floored at 1. A score of zero would divide the banded modifier table's
		// bottom rung out from under itself, and the books have no concept of a
		// zero stat anyway - cursed gear should make you bad at something, not
		// remove the something.
		EnhancedScores.Scores[Index] = FMath::Max(1, EnhancedScores.Scores[Index] + Bonuses[Index]);
	}

	// Constitution feeds Health and Intelligence feeds Mana, so a ring that moves
	// either has to move the pools with it or the numbers quietly disagree.
	// bRefillToFull is almost always false - see this function's header comment
	// for the one exception (the initial post-spawn call).
	RecalculateDerivedStats(bRefillToFull);
}

void UDCCStatsComponent::SetInvulnerable(bool bNewInvulnerable)
{
	if (GetOwnerRole() == ROLE_Authority)
	{
		bInvulnerable = bNewInvulnerable;
	}
}

void UDCCStatsComponent::SetIsAllyCharacter(bool bNewIsAlly)
{
	if (GetOwnerRole() != ROLE_Authority || bIsAllyCharacter == bNewIsAlly)
	{
		return;
	}

	bIsAllyCharacter = bNewIsAlly;

	// Refilled, because this runs at spawn and a freshly promoted ally arriving
	// on a fraction of its new maximum would look like it had already been hurt.
	RecalculateDerivedStats(/*bRefillToFull=*/true);
}

void UDCCStatsComponent::RecalculateDerivedStats(bool bRefillToFull)
{
	const int32 PreviousHitPoints = CurrentHitPoints;
	const int32 PreviousManaPoints = CurrentManaPoints;

	MaxHitPoints = GetRuleset()->GetMaxHitPoints(EnhancedScores, Level);

	// The player's cushion is applied here rather than inside GetMaxHitPoints so
	// that the ruleset's own curve stays the honest answer to "how tough is this
	// character", and the concession to playability is visible as a separate step.
	if (bIsPlayerCharacter)
	{
		MaxHitPoints = FMath::Max(1,
			FMath::RoundToInt32(MaxHitPoints * GetRuleset()->GetPlayerToughnessMultiplier()));
	}
	else if (bIsAllyCharacter)
	{
		// Smaller cushion than a player's, for the same reason players get one at
		// all: an ally dying is a story beat, but an ally dying every fight is
		// just attrition the party cannot do anything about.
		MaxHitPoints = FMath::Max(1,
			FMath::RoundToInt32(MaxHitPoints * GetRuleset()->GetAllyToughnessMultiplier()));
	}

	CurrentHitPoints = bRefillToFull ? MaxHitPoints : FMath::Clamp(CurrentHitPoints, 0, MaxHitPoints);

	ExperienceToNextLevel = GetRuleset()->GetExperienceToNextLevel(Level);

	MaxManaPoints = GetRuleset()->GetMaxManaPoints(EnhancedScores, Level);
	CurrentManaPoints = bRefillToFull ? MaxManaPoints : FMath::Clamp(CurrentManaPoints, 0, MaxManaPoints);

	MaxStamina = GetRuleset()->GetMaxStamina(EnhancedScores, Level);
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

	// Checked here because this is the one funnel every damage source shares -
	// melee, skills, the radiation aura and the boss charge all arrive through it.
	if (bInvulnerable)
	{
		return;
	}

	if (Amount <= 0 || !IsAlive())
	{
		return;
	}

	const int32 PreviousHitPoints = CurrentHitPoints;
	CurrentHitPoints = FMath::Max(0, CurrentHitPoints - Amount);

	// Restarts the quiet period. Sustained pressure therefore suppresses
	// regeneration entirely, which is what stops a block refilling mid-exchange.
	TimeSinceDamage = 0.0f;
	HealthRegenCarry = 0.0f;

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

	return GetRuleset()->ResolveCheck(EnhancedScores, Ability, DifficultyClass, SituationalBonus);
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
