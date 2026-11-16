#include "Skills/DCCSkillComponent.h"

#include "Characters/DCCCharacterBase.h"
#include "Stats/DCCStatsComponent.h"
#include "Stats/DCCRuleset.h"
#include "Engine/World.h"
#include "Engine/DataTable.h"
#include "EngineUtils.h"
#include "Net/UnrealNetwork.h"
#include "Components/SkeletalMeshComponent.h"
#include "Combat/DCCGameState.h"
#include "Quests/DCCQuestComponent.h"
#include "Progression/DCCAchievementComponent.h"
#include "UI/DCCEventLogTypes.h"
#include "NiagaraFunctionLibrary.h"
#include "NiagaraSystem.h"
#include "UObject/ConstructorHelpers.h"

UDCCSkillComponent::UDCCSkillComponent()
{
	PrimaryComponentTick.bCanEverTick = false;
	SetIsReplicatedByDefault(true);

	// Defaulted for the same reason as the achievement/quest/dialogue/equipment
	// tables: an unassigned table fails silently. This one hid for longer than
	// the others precisely because the silence is total - GrantStartingSkills
	// returns 0, KnownSkills stays empty, and every character just melees
	// forever with no error anywhere. Still overridable per-Blueprint.
	static ConstructorHelpers::FObjectFinder<UDataTable> DefaultTable(
		TEXT("/Game/DCC/Skills/DT_DCCSkills.DT_DCCSkills"));
	if (DefaultTable.Succeeded())
	{
		SkillTable = DefaultTable.Object;
	}
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
		AddPractice(*Known);
	}
}

void UDCCSkillComponent::AddPractice(FDCCKnownSkill& Known)
{
	const UWorld* World = GetWorld();
	if (!World)
	{
		return;
	}
	const double Now = World->GetTimeSeconds();
	double& Last = LastPracticeAt.FindOrAdd(Known.SkillId, -1000.0);
	if (Now - Last < PracticeInterval)
	{
		return;
	}
	Last = Now;

	const UDCCStatsComponent* Stats = GetOwnerStats();
	const UDCCRuleset* Ruleset = Stats ? Stats->GetRuleset() : nullptr;
	const int32 MaxRank = Ruleset ? Ruleset->GetMaxSkillRank() : 15;
	if (Known.Rank >= MaxRank)
	{
		Known.Practice = 1.0f; // a full bar at the cap
		return;
	}

	Known.Practice += 1.0f / FMath::Max(1.0f, PracticeTicksBase + PracticeTicksPerRank * Known.Rank);
	if (Known.Practice < 1.0f)
	{
		return;
	}

	// The bar filled: the Rank lands, through the same announcements as a rolled advancement.
	Known.Practice = 0.0f;
	Known.Rank = FMath::Min(Known.Rank + 1, MaxRank);
	OnSkillAdvanced.Broadcast(this, Known.SkillId, Known.Rank);
	Multicast_SkillAdvanced(Known.SkillId, Known.Rank);
	UDCCQuestComponent::NotifyEvent(this, EDCCObjectiveType::SkillRankGained, Known.SkillId, 1);
	if (AActor* SkillOwner = GetOwner())
	{
		if (UDCCStatsComponent* OwnerStats = SkillOwner->FindComponentByClass<UDCCStatsComponent>())
		{
			OwnerStats->AddExperience(ExperiencePerSkillRank * Known.Rank);
		}
	}
	ADCCGameState::Log(this, EDCCLogCategory::Advancement, FText::Format(
		NSLOCTEXT("DCC", "LogPractice", "{0} is now Rank {1} in {2}. Practice is permitted."),
		FText::FromString(GetNameSafe(GetOwner())),
		FText::AsNumber(Known.Rank),
		FText::FromName(Known.SkillId)));
	UE_LOG(LogTemp, Log, TEXT("DCCSkill '%s' practised to Rank %d for %s."),
		*Known.SkillId.ToString(), Known.Rank, *GetNameSafe(GetOwner()));
}

void UDCCSkillComponent::RecordUntrainedAttempt(FName SkillId)
{
	if (GetOwnerRole() != ROLE_Authority || IsSkillKnown(SkillId))
	{
		return;
	}

	UntrainedAttempts.Add(SkillId);

	// Learning by doing (2026-11-14): the arcade games never run the advancement roll that turns an untrained
	// attempt into a skill, so enough practice simply teaches it. Same rate limit as the practice bar.
	if (const UWorld* World = GetWorld())
	{
		const double Now = World->GetTimeSeconds();
		double& Last = LastPracticeAt.FindOrAdd(SkillId, -1000.0);
		if (Now - Last >= PracticeInterval)
		{
			Last = Now;
			if (++UntrainedTicks.FindOrAdd(SkillId) >= UntrainedTicksToLearn)
			{
				UntrainedTicks.Remove(SkillId);
				GrantSkill(SkillId, 1);
			}
		}
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

			// The one funnel every Skill Rank gain passes through, same
			// reasoning as the XP payment two lines below - a "gain N Ranks
			// in Piloting" dev-goal quest hooks in here, not at whatever
			// gameplay code happened to mark the skill.
			UDCCQuestComponent::NotifyEvent(this, EDCCObjectiveType::SkillRankGained, Known.SkillId, 1);

			// Character XP is paid HERE - on the rank actually landing - and
			// nowhere else. Getting better at something is the achievement; the
			// practice that led to it is not, or every repeatable action in the
			// game turns into an XP faucet you can idle against.
			//
			// Scaled by the new Rank so late ranks, which are much harder to
			// roll, are worth more than the first one.
			if (AActor* SkillOwner = GetOwner())
			{
				if (UDCCStatsComponent* OwnerStats = SkillOwner->FindComponentByClass<UDCCStatsComponent>())
				{
					OwnerStats->AddExperience(ExperiencePerSkillRank * Known.Rank);
				}
			}

			ADCCGameState::Log(this, EDCCLogCategory::Advancement, FText::Format(
				NSLOCTEXT("DCC", "LogAdvance", "{0} is now Rank {1} in {2}. Growth is permitted."),
				FText::FromString(GetNameSafe(GetOwner())),
				FText::AsNumber(Known.Rank),
				FText::FromName(Known.SkillId)));

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

TArray<FDCCBackgroundDef> UDCCSkillComponent::GetBackgroundOptions(EDCCBackgroundStage Stage) const
{
	TArray<FDCCBackgroundDef> Options;

	if (!BackgroundTable)
	{
		return Options;
	}

	static const FString ContextString(TEXT("DCCSkillComponent::GetBackgroundOptions"));

	for (const FName& RowName : BackgroundTable->GetRowNames())
	{
		const FDCCBackgroundDef* Row = BackgroundTable->FindRow<FDCCBackgroundDef>(RowName, ContextString, false);
		if (!Row || Row->Stage != Stage)
		{
			continue;
		}

		FDCCBackgroundDef Background = *Row;
		if (Background.BackgroundId.IsNone())
		{
			Background.BackgroundId = RowName;
		}

		Options.Add(Background);
	}

	return Options;
}

int32 UDCCSkillComponent::ApplyBackground(FName BackgroundId, const TArray<FName>& ChosenSkillIds)
{
	if (GetOwnerRole() != ROLE_Authority)
	{
		UE_LOG(LogTemp, Warning, TEXT("DCCSkillComponent::ApplyBackground called on a client; ignoring."));
		return 0;
	}

	if (!BackgroundTable || BackgroundId.IsNone())
	{
		return 0;
	}

	static const FString ContextString(TEXT("DCCSkillComponent::ApplyBackground"));

	const FDCCBackgroundDef* Background = BackgroundTable->FindRow<FDCCBackgroundDef>(BackgroundId, ContextString, false);
	if (!Background)
	{
		UE_LOG(LogTemp, Warning, TEXT("DCCSkillComponent::ApplyBackground: no background '%s'."),
			*BackgroundId.ToString());
		return 0;
	}

	int32 GrantedCount = 0;

	for (const FName& ChosenId : ChosenSkillIds)
	{
		// Never trust the choice: the client picks from the menu, it does not get
		// to invent menu items. Without this check a modified client could hand
		// itself any skill in the table at the background's Rank.
		if (!Background->SkillOptions.Contains(ChosenId))
		{
			UE_LOG(LogTemp, Warning,
				TEXT("DCCSkillComponent::ApplyBackground: '%s' is not offered by background '%s'; refused."),
				*ChosenId.ToString(), *BackgroundId.ToString());
			continue;
		}

		if (GrantedCount >= Background->SkillsToChoose)
		{
			break;
		}

		if (GrantSkill(ChosenId, Background->GrantRank))
		{
			++GrantedCount;
		}
	}

	return GrantedCount;
}

TArray<FDCCSkillRecommendation> UDCCSkillComponent::GetRecommendedSkills(int32 Count) const
{
	TArray<FDCCSkillRecommendation> Recommendations;

	const UDCCStatsComponent* Stats = GetOwnerStats();
	if (!SkillTable || !Stats)
	{
		return Recommendations;
	}

	// Which stat this character is actually best at, so the reason text can name
	// it rather than vaguely gesturing at "your stats".
	EDCCAbility BestAbility = EDCCAbility::Strength;
	int32 BestScore = TNumericLimits<int32>::Lowest();
	for (int32 Index = 0; Index < static_cast<int32>(EDCCAbility::MAX); ++Index)
	{
		const EDCCAbility Ability = static_cast<EDCCAbility>(Index);
		const int32 Score = Stats->GetEnhanced().GetScore(Ability);
		if (Score > BestScore)
		{
			BestScore = Score;
			BestAbility = Ability;
		}
	}

	const UEnum* AbilityEnum = StaticEnum<EDCCAbility>();
	static const FString ContextString(TEXT("DCCSkillComponent::GetRecommendedSkills"));

	for (const FName& RowName : SkillTable->GetRowNames())
	{
		const FDCCSkillDef* Row = SkillTable->FindRow<FDCCSkillDef>(RowName, ContextString, false);
		if (!Row || Row->bIsPassive)
		{
			continue;
		}

		const FName SkillId = Row->SkillId.IsNone() ? RowName : Row->SkillId;
		if (IsSkillKnown(SkillId))
		{
			continue;
		}

		const int32 CastingScore = Stats->GetEnhanced().GetScore(Row->CastingAbility);

		FDCCSkillRecommendation Recommendation;
		Recommendation.SkillId = SkillId;
		Recommendation.DisplayName = Row->DisplayName;

		// Scored on the raw score rather than the modifier: the banded modifier
		// table flattens 3, 4 and 5 all to +2, which would leave most of a
		// starting character's skills tied and the ordering arbitrary.
		Recommendation.Score = CastingScore * 10;

		const FText AbilityName = AbilityEnum
			? AbilityEnum->GetDisplayNameTextByValue(static_cast<int64>(Row->CastingAbility))
			: FText::GetEmpty();

		if (Row->CastingAbility == BestAbility)
		{
			Recommendation.Score += 25;
			Recommendation.Reason = FText::Format(
				NSLOCTEXT("DCC", "RecBest", "{0} is your strongest stat."), AbilityName);
		}
		else
		{
			Recommendation.Reason = FText::Format(
				NSLOCTEXT("DCC", "RecOk", "Uses {0}, where you score {1}."),
				AbilityName, FText::AsNumber(CastingScore));
		}

		// Flagged rather than filtered out. A spell you cannot afford yet is still
		// worth showing - it tells the player what raising Intelligence buys them.
		if (Row->ManaCost > Stats->MaxManaPoints)
		{
			Recommendation.bUnaffordable = true;
			Recommendation.Score -= 40;
			Recommendation.Reason = FText::Format(
				NSLOCTEXT("DCC", "RecCost", "Costs {0} Mana; your pool is {1}."),
				FText::AsNumber(Row->ManaCost), FText::AsNumber(Stats->MaxManaPoints));
		}

		Recommendations.Add(Recommendation);
	}

	Recommendations.Sort([](const FDCCSkillRecommendation& A, const FDCCSkillRecommendation& B)
	{
		return A.Score > B.Score;
	});

	if (Count > 0 && Recommendations.Num() > Count)
	{
		Recommendations.SetNum(Count);
	}

	return Recommendations;
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

float UDCCSkillComponent::GetEffectiveRange(const FDCCSkillDef& Skill) const
{
	if (Skill.RangePerRank <= 0.0f)
	{
		return Skill.Range;
	}

	// Rank 0 (untrained) and Rank 1 both get the base range - untrained use is
	// already penalised with Disadvantage, and stacking a shorter throw on top
	// would punish the same attempt twice.
	const int32 Rank = GetSkillRank(Skill.SkillId);
	const int32 RanksAboveFirst = FMath::Max(0, Rank - 1);

	return Skill.Range + Skill.RangePerRank * RanksAboveFirst;
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

		if (FVector::Dist(Character->GetActorLocation(), Target->GetActorLocation())
			> GetEffectiveRange(Skill))
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

	// Rooted at the same moment the cost is paid, and deliberately before the
	// roll: the commitment is made by casting, not by succeeding. A fizzle that
	// left the caster free to run would make failure cheaper than success.
	if (ADCCCharacterBase* Caster = GetOwningCharacter())
	{
		Caster->BeginCastLock(Skill.CastTimeSeconds);
	}

	const bool bKnown = IsSkillKnown(Skill.SkillId);
	Result.bWasUntrained = !bKnown;
	Result.SkillRank = GetSkillRank(Skill.SkillId);

	// A DifficultyClass of 0 means the skill simply works once paid for.
	bool bSucceeded = true;
	if (Skill.DifficultyClass > 0)
	{
		Result.Check = Ruleset->ResolveSkillCheck(
			Stats->GetEnhanced(),
			Skill.CastingAbility,
			Result.SkillRank,
			Skill.DifficultyClass,
			Result.bWasUntrained);

		bSucceeded = Result.Check.bSuccess;

		// "Untrained Genius" - Result.bWasUntrained and Skill.DifficultyClass
		// were already exactly what this needed; no new tracking turned out to
		// be required here, just reading two values this call site already has.
		if (Result.bWasUntrained && bSucceeded && Skill.DifficultyClass >= 15)
		{
			UDCCAchievementComponent::RecordFor(GetOwner(), EDCCAchievementStat::UntrainedHighDCSuccess);
		}
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

		const int32 AbilityModifier = Ruleset->GetAbilityModifier(Stats->GetEnhanced().GetScore(Skill.CastingAbility));

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
				if (ADCCCharacterBase* Caster = GetOwningCharacter())
				{
					Caster->NoteDamageDealtTo(Affected, Magnitude);
				}
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

	{
		const FText CasterName = FText::FromString(GetNameSafe(GetOwner()));
		const FText SkillName = Skill.DisplayName.IsEmpty()
			? FText::FromName(Skill.SkillId)
			: Skill.DisplayName;

		FText Line;
		if (!bSucceeded)
		{
			// A failed cast still burned the mana, which is the whole reason a bad
			// roll hurts - so the feed says so explicitly.
			Line = FText::Format(
				NSLOCTEXT("DCC", "LogSkillFail", "{0}'s {1} fizzles. {2} Mana, gone."),
				CasterName, SkillName, FText::AsNumber(Result.ManaSpent));
		}
		else if (Result.TotalMagnitude > 0)
		{
			Line = FText::Format(
				NSLOCTEXT("DCC", "LogSkillHit", "{0} lands {1} for {2}."),
				CasterName, SkillName, FText::AsNumber(Result.TotalMagnitude));
		}
		else
		{
			Line = FText::Format(
				NSLOCTEXT("DCC", "LogSkillUse", "{0} uses {1}."),
				CasterName, SkillName);
		}

		if (Result.bWasUntrained)
		{
			Line = FText::Format(
				NSLOCTEXT("DCC", "LogSkillUntrained", "{0} (untrained, and it showed)"), Line);
		}

		ADCCGameState::Log(this, EDCCLogCategory::Skill, Line);
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

	PlaySkillEffects(Result);

	BP_OnSkillPlayed(Result);
}

void UDCCSkillComponent::PlaySkillEffects(const FDCCSkillResult& Result)
{
	// Cosmetic only. A dedicated server has no business spawning particles, and
	// doing so would cost it work no one can see.
	UWorld* World = GetWorld();
	if (!World || World->GetNetMode() == NM_DedicatedServer)
	{
		return;
	}

	FDCCSkillDef Skill;
	if (!FindSkill(Result.SkillId, Skill))
	{
		return;
	}

	ADCCCharacterBase* Caster = GetOwningCharacter();

	if (Caster)
	{
		// LoadSynchronous rather than an async request: the effect is wanted on
		// this frame, and a spell whose flames arrive half a second late reads as
		// a bug. The systems involved are small, and the first cast warms them.
		if (UNiagaraSystem* CastSystem = Skill.CastEffect.LoadSynchronous())
		{
			USkeletalMeshComponent* Mesh = Caster->GetMesh();

			if (Mesh && !Skill.CastSocketName.IsNone() && Mesh->DoesSocketExist(Skill.CastSocketName))
			{
				UNiagaraFunctionLibrary::SpawnSystemAttached(
					CastSystem,
					Mesh,
					Skill.CastSocketName,
					FVector::ZeroVector,
					FRotator::ZeroRotator,
					EAttachLocation::SnapToTarget,
					true);
			}
			else
			{
				// No socket authored: fire from roughly chest height in front of
				// the caster, which reads acceptably for every archetype.
				const FVector Origin = Caster->GetActorLocation()
					+ Caster->GetActorForwardVector() * 60.0f
					+ FVector(0.0f, 0.0f, 40.0f);

				UNiagaraFunctionLibrary::SpawnSystemAtLocation(
					World, CastSystem, Origin, Caster->GetActorRotation());
			}
		}
	}

	if (UNiagaraSystem* ImpactSystem = Skill.ImpactEffect.LoadSynchronous())
	{
		for (const TObjectPtr<AActor>& Affected : Result.AffectedActors)
		{
			if (!Affected)
			{
				continue;
			}

			const FVector ImpactLocation =
				Affected->GetActorLocation() + FVector(0.0f, 0.0f, Skill.ImpactEffectZOffset);

			UNiagaraFunctionLibrary::SpawnSystemAtLocation(
				World, ImpactSystem, ImpactLocation, FRotator::ZeroRotator);
		}
	}
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
