#include "Skills/DCCSkillComponent.h"

#include "Characters/DCCCharacterBase.h"
#include "Stats/DCCStatsComponent.h"
#include "Stats/DCCRuleset.h"
#include "Engine/World.h"
#include "Engine/DataTable.h"
#include "EngineUtils.h"
#include "Net/UnrealNetwork.h"

UDCCSkillComponent::UDCCSkillComponent()
{
	PrimaryComponentTick.bCanEverTick = false;
	SetIsReplicatedByDefault(true);
}

void UDCCSkillComponent::BeginPlay()
{
	Super::BeginPlay();

	// Server only: KnownSkills is replicated, so granting on clients too would
	// briefly double every character's kit until the next update overwrote it.
	if (GetOwnerRole() == ROLE_Authority)
	{
		GrantStartingSkills();
	}
}

int32 UDCCSkillComponent::GrantStartingSkills()
{
	if (GetOwnerRole() != ROLE_Authority)
	{
		UE_LOG(LogTemp, Warning, TEXT("DCCSkillComponent::GrantStartingSkills called on a client; ignoring."));
		return 0;
	}

	if (!SkillTable || StartingSkillIds.IsEmpty())
	{
		return 0;
	}

	int32 GrantedCount = 0;

	for (const FName& SkillId : StartingSkillIds)
	{
		if (SkillId.IsNone())
		{
			continue;
		}

		// GrantSkill validates the id against the table and logs a missing row,
		// which is the single most likely authoring mistake here - a renamed row
		// leaves a character silently unable to do the thing they were built around.
		if (GrantSkill(SkillId, StartingSkillRank))
		{
			++GrantedCount;
		}
	}

	return GrantedCount;
}

void UDCCSkillComponent::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);

	DOREPLIFETIME(UDCCSkillComponent, KnownSkills);
}

ADCCCharacterBase* UDCCSkillComponent::GetOwningCharacter() const
{
	return Cast<ADCCCharacterBase>(GetOwner());
}

UDCCStatsComponent* UDCCSkillComponent::GetOwnerStats() const
{
	const ADCCCharacterBase* Character = GetOwningCharacter();
	return Character ? Character->Stats : nullptr;
}

bool UDCCSkillComponent::FindSkill(FName SkillId, FDCCSkillDef& OutSkill) const
{
	if (!SkillTable || SkillId.IsNone())
	{
		return false;
	}

	static const FString ContextString(TEXT("DCCSkillComponent::FindSkill"));

	// Resolved from the table rather than from KnownSkills so that an untrained
	// attempt can still find the definition it needs to roll against.
	const FDCCSkillDef* Row = SkillTable->FindRow<FDCCSkillDef>(SkillId, ContextString, false);
	if (!Row)
	{
		return false;
	}

	OutSkill = *Row;

	if (OutSkill.SkillId.IsNone())
	{
		OutSkill.SkillId = SkillId;
	}

	return true;
}

FDCCKnownSkill* UDCCSkillComponent::FindKnownSkill(FName SkillId)
{
	return KnownSkills.FindByPredicate(
		[SkillId](const FDCCKnownSkill& Known) { return Known.SkillId == SkillId; });
}

const FDCCKnownSkill* UDCCSkillComponent::FindKnownSkill(FName SkillId) const
{
	return KnownSkills.FindByPredicate(
		[SkillId](const FDCCKnownSkill& Known) { return Known.SkillId == SkillId; });
}

bool UDCCSkillComponent::IsSkillKnown(FName SkillId) const
{
	return FindKnownSkill(SkillId) != nullptr;
}

int32 UDCCSkillComponent::GetSkillRank(FName SkillId) const
{
	const FDCCKnownSkill* Known = FindKnownSkill(SkillId);
	return Known ? Known->Rank : 0;
}

bool UDCCSkillComponent::HasAdvancementMark(FName SkillId) const
{
	const FDCCKnownSkill* Known = FindKnownSkill(SkillId);
	return Known && Known->bAdvancementMark;
}

void UDCCSkillComponent::MarkSkillUsed(FName SkillId)
{
	if (GetOwnerRole() != ROLE_Authority)
	{
		return;
	}

	if (FDCCKnownSkill* Known = FindKnownSkill(SkillId))
	{
		Known->bAdvancementMark = true;
	}
}

int32 UDCCSkillComponent::RunSkillAdvancement()
{
	if (GetOwnerRole() != ROLE_Authority)
	{
		UE_LOG(LogTemp, Warning, TEXT("DCCSkillComponent::RunSkillAdvancement called on a client; ignoring."));
		return 0;
	}

	UDCCStatsComponent* Stats = GetOwnerStats();
	UDCCRuleset* Ruleset = Stats ? Stats->GetRuleset() : nullptr;
	if (!Ruleset)
	{
		return 0;
	}

	int32 AdvancedCount = 0;

	for (FDCCKnownSkill& Known : KnownSkills)
	{
		if (!Known.bAdvancementMark)
		{
			continue;
		}

		if (Ruleset->ResolveAdvancementCheck(Known.Rank))
		{
			Known.Rank = FMath::Min(Known.Rank + 1, Ruleset->GetMaxSkillRank());
			++AdvancedCount;

			OnSkillAdvanced.Broadcast(this, Known.SkillId, Known.Rank);
			Multicast_SkillAdvanced(Known.SkillId, Known.Rank);

			UE_LOG(LogTemp, Log, TEXT("DCCSkill '%s' advanced to Rank %d for %s."),
				*Known.SkillId.ToString(), Known.Rank, *GetNameSafe(GetOwner()));
		}

		// Cleared whether or not the roll landed - a failed advancement check
		// consumes the mark, so progress needs fresh use rather than accumulating
		// attempts across encounters.
		Known.bAdvancementMark = false;
	}

	// One skill attempted untrained is learned outright. Exactly one, chosen at
	// random rather than "all of them", so flailing at every skill in the list is
	// not a strategy - which is the point of the published restriction.
	if (!UntrainedAttempts.IsEmpty())
	{
		TArray<FName> Candidates = UntrainedAttempts.Array();
		const FName Learned = Candidates[FMath::RandRange(0, Candidates.Num() - 1)];

		if (GrantSkill(Learned, 1))
		{
			++AdvancedCount;

			OnSkillAdvanced.Broadcast(this, Learned, 1);
			Multicast_SkillAdvanced(Learned, 1);

			UE_LOG(LogTemp, Log, TEXT("DCCSkill '%s' learned from untrained use by %s."),
				*Learned.ToString(), *GetNameSafe(GetOwner()));
		}

		UntrainedAttempts.Empty();
	}

	return AdvancedCount;
}

void UDCCSkillComponent::Multicast_SkillAdvanced_Implementation(FName SkillId, int32 NewRank)
{
	// The server already broadcast locally inside RunSkillAdvancement; re-firing
	// there would double every announcement.
	if (GetOwnerRole() != ROLE_Authority)
	{
		OnSkillAdvanced.Broadcast(this, SkillId, NewRank);
	}
}

float UDCCSkillComponent::GetRemainingCooldown(FName SkillId) const
{
	const UWorld* World = GetWorld();
	const float* EndTime = CooldownEndTimes.Find(SkillId);

	if (!World || !EndTime)
	{
		return 0.0f;
	}

	return FMath::Max(0.0f, *EndTime - World->GetTimeSeconds());
}

EDCCSkillFailure UDCCSkillComponent::CanActivate(FName SkillId, AActor* Target) const
{
	FDCCSkillDef Skill;
	if (!FindSkill(SkillId, Skill))
	{
		return EDCCSkillFailure::UnknownSkill;
	}

	// Attack and Utility Skills may be tried untrained, at Disadvantage. Spell
	// Skills and Passives may not - a crawler cannot bluff their way into casting
	// something they have never learned.
	if (!IsSkillKnown(SkillId) && (!Skill.bCanAttemptUntrained || Skill.bIsPassive))
	{
		return EDCCSkillFailure::RequiresTraining;
	}

	const ADCCCharacterBase* Character = GetOwningCharacter();
	if (!Character || !Character->IsAlive())
	{
		return EDCCSkillFailure::Dead;
	}

	if (GetRemainingCooldown(SkillId) > 0.0f)
	{
		return EDCCSkillFailure::OnCooldown;
	}

	const UDCCStatsComponent* Stats = GetOwnerStats();
	if (!Stats || !Stats->CanAffordMana(Skill.ManaCost))
	{
		return EDCCSkillFailure::NotEnoughMana;
	}

	// Self-targeted skills never need a target, and area skills fire from the
	// caster, so only single-target skills can fail on targeting.
	if (Skill.Targeting == EDCCSkillTargeting::SingleTarget)
	{
		if (!Target)
		{
			return EDCCSkillFailure::NoTarget;
		}

		if (FVector::Dist(Character->GetActorLocation(), Target->GetActorLocation()) > Skill.Range)
		{
			return EDCCSkillFailure::OutOfRange;
		}
	}

	return EDCCSkillFailure::None;
}

void UDCCSkillComponent::TryActivateSkill(FName SkillId, AActor* Target)
{
	if (GetOwnerRole() == ROLE_Authority)
	{
		FDCCSkillDef Skill;
		if (FindSkill(SkillId, Skill))
		{
			ActivateSkillInternal(Skill, Target);
		}
		return;
	}

	// Locally predicted refusals only stop obviously-doomed requests from
	// travelling; the server re-checks everything regardless.
	if (CanActivate(SkillId, Target) != EDCCSkillFailure::None)
	{
		return;
	}

	Server_ActivateSkill(SkillId, Target);
}

void UDCCSkillComponent::Server_ActivateSkill_Implementation(FName SkillId, AActor* Target)
{
	FDCCSkillDef Skill;
	if (!FindSkill(SkillId, Skill))
	{
		UE_LOG(LogTemp, Warning, TEXT("DCCSkillComponent: '%s' requested unknown skill '%s'."),
			*GetNameSafe(GetOwner()), *SkillId.ToString());
		return;
	}

	ActivateSkillInternal(Skill, Target);
}

FDCCSkillResult UDCCSkillComponent::ActivateSkillInternal(const FDCCSkillDef& Skill, AActor* Target)
{
	FDCCSkillResult Result;
	Result.SkillId = Skill.SkillId;

	if (GetOwnerRole() != ROLE_Authority)
	{
		Result.Failure = EDCCSkillFailure::NotAuthoritative;
		return Result;
	}

	// Re-validated server-side on purpose: the client's own CanActivate is a
	// courtesy for its HUD, never a permission.
	const EDCCSkillFailure Failure = CanActivate(Skill.SkillId, Target);
	if (Failure != EDCCSkillFailure::None)
	{
		Result.Failure = Failure;
		return Result;
	}

	UDCCStatsComponent* Stats = GetOwnerStats();
	UDCCRuleset* Ruleset = Stats ? Stats->GetRuleset() : nullptr;
	UWorld* World = GetWorld();
	if (!Stats || !Ruleset || !World)
	{
		Result.Failure = EDCCSkillFailure::UnknownSkill;
		return Result;
	}

	// Cost is paid before resolution: in DCC a failed spell still burns the Mana,
	// which is what makes a bad roll hurt.
	if (!Stats->SpendMana(Skill.ManaCost))
	{
		Result.Failure = EDCCSkillFailure::NotEnoughMana;
		return Result;
	}

	Result.ManaSpent = Skill.ManaCost;
	CooldownEndTimes.Add(Skill.SkillId, World->GetTimeSeconds() + Skill.CooldownSeconds);

	const bool bKnown = IsSkillKnown(Skill.SkillId);
	Result.bWasUntrained = !bKnown;
	Result.SkillRank = GetSkillRank(Skill.SkillId);

	// A DifficultyClass of 0 means the skill simply works once paid for.
	bool bSucceeded = true;
	if (Skill.DifficultyClass > 0)
	{
		Result.Check = Ruleset->ResolveSkillCheck(
			Stats->AbilityScores,
			Skill.CastingAbility,
			Result.SkillRank,
			Skill.DifficultyClass,
			Result.bWasUntrained);

		bSucceeded = Result.Check.bSuccess;
	}

	// Marked on the attempt, pass or fail. Passive skills never mark, and an
	// untrained attempt has no entry to mark - it is recorded separately below as
	// a candidate for being learned outright.
	if (!Skill.bIsPassive)
	{
		if (bKnown)
		{
			MarkSkillUsed(Skill.SkillId);
		}
		else
		{
			UntrainedAttempts.Add(Skill.SkillId);
		}
	}

	Result.bActivated = true;

	if (bSucceeded)
	{
		TArray<ADCCCharacterBase*> Targets;
		GatherTargets(Skill, Target, Targets);

		const int32 AbilityModifier = Ruleset->GetAbilityModifier(Stats->AbilityScores.GetScore(Skill.CastingAbility));

		for (ADCCCharacterBase* Affected : Targets)
		{
			UDCCStatsComponent* TargetStats = Affected ? Affected->Stats : nullptr;
			if (!TargetStats)
			{
				continue;
			}

			switch (Skill.Archetype)
			{
			case EDCCSkillArchetype::Damage:
			{
				// Rolled per target rather than once and shared, so an AoE reads
				// like several separate hits instead of a flat number.
				const int32 Magnitude = FMath::Max(1, FMath::RandRange(1, FMath::Max(1, Skill.PowerDie)) + AbilityModifier);
				TargetStats->ApplyDamage(Magnitude);
				Result.TotalMagnitude += Magnitude;
				Result.AffectedActors.Add(Affected);
				break;
			}
			case EDCCSkillArchetype::Heal:
			{
				const int32 Magnitude = FMath::Max(1, FMath::RandRange(1, FMath::Max(1, Skill.PowerDie)) + AbilityModifier);
				TargetStats->Heal(Magnitude);
				Result.TotalMagnitude += Magnitude;
				Result.AffectedActors.Add(Affected);
				break;
			}
			default:
				// Debuff / Buff / Utility / Summon have no single correct
				// mechanical meaning, so they are handed to Blueprint below
				// rather than guessed at here.
				Result.AffectedActors.Add(Affected);
				break;
			}
		}

		if (Skill.Archetype != EDCCSkillArchetype::Damage && Skill.Archetype != EDCCSkillArchetype::Heal)
		{
			TArray<AActor*> AsActors;
			AsActors.Reserve(Targets.Num());
			for (ADCCCharacterBase* Affected : Targets)
			{
				AsActors.Add(Affected);
			}

			BP_OnSkillEffect(Skill, AsActors);
		}
	}

	OnSkillActivated.Broadcast(this, Result);
	Multicast_SkillActivated(Result);

	UE_LOG(LogTemp, Log, TEXT("DCCSkill '%s' by %s: success=%d mana=%d magnitude=%d targets=%d"),
		*Skill.SkillId.ToString(), *GetNameSafe(GetOwner()), bSucceeded ? 1 : 0,
		Result.ManaSpent, Result.TotalMagnitude, Result.AffectedActors.Num());

	return Result;
}

void UDCCSkillComponent::GatherTargets(const FDCCSkillDef& Skill, AActor* Target, TArray<ADCCCharacterBase*>& OutTargets) const
{
	ADCCCharacterBase* Caster = GetOwningCharacter();
	UWorld* World = GetWorld();
	if (!Caster || !World)
	{
		return;
	}

	switch (Skill.Targeting)
	{
	case EDCCSkillTargeting::Self:
		OutTargets.Add(Caster);
		return;

	case EDCCSkillTargeting::SingleTarget:
		if (ADCCCharacterBase* Single = Cast<ADCCCharacterBase>(Target))
		{
			OutTargets.Add(Single);
		}
		return;

	case EDCCSkillTargeting::Cone:
	case EDCCSkillTargeting::AreaOfEffect:
	{
		const FVector Origin = Caster->GetActorLocation();
		const FVector Forward = Caster->GetActorForwardVector();
		const float RadiusSquared = FMath::Square(Skill.Radius);
		const float CosHalfAngle = FMath::Cos(FMath::DegreesToRadians(Skill.ConeHalfAngleDegrees));

		for (TActorIterator<ADCCCharacterBase> It(World); It; ++It)
		{
			ADCCCharacterBase* Candidate = *It;
			if (!Candidate || !Candidate->IsAlive())
			{
				continue;
			}

			// Damage archetypes should not catch the caster's own side in the
			// blast; support archetypes should not land on enemies. Hostility
			// already encodes both, so key off the archetype's intent.
			const bool bWantsHostile =
				Skill.Archetype == EDCCSkillArchetype::Damage ||
				Skill.Archetype == EDCCSkillArchetype::Debuff;

			if (Caster->IsHostileTo(Candidate) != bWantsHostile)
			{
				continue;
			}

			const FVector ToCandidate = Candidate->GetActorLocation() - Origin;
			if (ToCandidate.SizeSquared() > RadiusSquared)
			{
				continue;
			}

			if (Skill.Targeting == EDCCSkillTargeting::Cone)
			{
				const FVector Direction = ToCandidate.GetSafeNormal();
				if (!Direction.IsNearlyZero() && FVector::DotProduct(Forward, Direction) < CosHalfAngle)
				{
					continue;
				}
			}

			OutTargets.Add(Candidate);
		}
		return;
	}

	default:
		return;
	}
}

void UDCCSkillComponent::Multicast_SkillActivated_Implementation(const FDCCSkillResult& Result)
{
	// The server already broadcast locally; this is the clients' copy.
	if (GetOwnerRole() != ROLE_Authority)
	{
		OnSkillActivated.Broadcast(this, Result);
	}

	BP_OnSkillPlayed(Result);
}

bool UDCCSkillComponent::GrantSkill(FName SkillId, int32 InitialRank)
{
	if (GetOwnerRole() != ROLE_Authority)
	{
		UE_LOG(LogTemp, Warning, TEXT("DCCSkillComponent::GrantSkill called on a client; ignoring."));
		return false;
	}

	if (SkillId.IsNone())
	{
		UE_LOG(LogTemp, Warning, TEXT("DCCSkillComponent::GrantSkill refused a skill with no SkillId."));
		return false;
	}

	// Refuse ids the table cannot resolve. Otherwise a typo produces a skill the
	// character "knows" but can never use, which is far harder to diagnose than a
	// refusal at the moment of granting.
	FDCCSkillDef Definition;
	if (!FindSkill(SkillId, Definition))
	{
		UE_LOG(LogTemp, Warning, TEXT("DCCSkillComponent::GrantSkill: '%s' is not a row in the skill table."),
			*SkillId.ToString());
		return false;
	}

	const int32 ClampedRank = FMath::Max(0, InitialRank);

	if (FDCCKnownSkill* Existing = FindKnownSkill(SkillId))
	{
		// Already known: promote rather than refuse, so a sponsor drop of a skill
		// the crawler already has is an upgrade instead of a wasted reward.
		if (ClampedRank > Existing->Rank)
		{
			Existing->Rank = ClampedRank;
			return true;
		}

		return false;
	}

	FDCCKnownSkill Known;
	Known.SkillId = SkillId;
	Known.Rank = ClampedRank;

	KnownSkills.Add(Known);
	return true;
}

bool UDCCSkillComponent::RemoveSkill(FName SkillId)
{
	if (GetOwnerRole() != ROLE_Authority)
	{
		UE_LOG(LogTemp, Warning, TEXT("DCCSkillComponent::RemoveSkill called on a client; ignoring."));
		return false;
	}

	return KnownSkills.RemoveAll([SkillId](const FDCCKnownSkill& Known) { return Known.SkillId == SkillId; }) > 0;
}
