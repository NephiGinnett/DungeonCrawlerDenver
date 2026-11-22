// The level-up System window (2026-11-22). Split out of DCCCharacterBase.cpp, which is long enough.

#include "Characters/DCCCharacterBase.h"

#include "Classes/DCCBasicClasses.h"
#include "Classes/DCCClassPerks.h"
#include "Onboarding/DCCOnboardingComponent.h"
#include "Skills/DCCSkillComponent.h"
#include "Stats/DCCRuleset.h"
#include "Stats/DCCStatsComponent.h"
#include "TimerManager.h"

void ADCCCharacterBase::HandleLevelUp(UDCCStatsComponent* LevelledStats, int32 NewLevel)
{
	if (!HasAuthority() || !Stats || !IsPlayerControlled())
	{
		return;
	}
	if (PendingLevelFrom <= 0)
	{
		PendingLevelFrom = NewLevel - 1;
		// The health ceiling before this burst: the same ratio the player's toughness cushion applies, one level down.
		UDCCRuleset* Ruleset = Stats->GetRuleset();
		const int32 RawNow = Ruleset ? Ruleset->GetMaxHitPoints(Stats->GetEnhanced(), NewLevel) : 0;
		const int32 RawThen = Ruleset ? Ruleset->GetMaxHitPoints(Stats->GetEnhanced(), PendingLevelFrom) : 0;
		PendingMaxHpFrom = RawNow > 0 ? FMath::RoundToInt(float(Stats->MaxHitPoints) * RawThen / RawNow) : Stats->MaxHitPoints;
	}
	// A burst of XP can raise several levels in one frame, or a few in a second (a room, a contract, the boss).
	GetWorldTimerManager().SetTimer(LevelUpTimer, FTimerDelegate::CreateUObject(this, &ADCCCharacterBase::FlushLevelUp), 0.6f, false);
}

void ADCCCharacterBase::FlushLevelUp()
{
	if (!Stats || PendingLevelFrom <= 0)
	{
		return;
	}
	const int32 From = PendingLevelFrom;
	const int32 To = Stats->Level;
	const int32 HpFrom = PendingMaxHpFrom;
	PendingLevelFrom = 0;
	PendingMaxHpFrom = 0;
	if (To <= From)
	{
		return;
	}

	// The class's lessons for the levels just reached. Learned quietly: the window says it.
	const FString Learned = DCCBasicClasses::ApplyLearned(this, false);

	TArray<FString> Lines;
	Lines.Add(FString::Printf(TEXT("Max health %d > %d. Health, mana and stamina refilled."), HpFrom, Stats->MaxHitPoints));
	const int32 Points = UDCCStatsComponent::StatPointsEarnedAt(To) - UDCCStatsComponent::StatPointsEarnedAt(From);
	if (Points > 0)
	{
		Lines.Add(FString::Printf(TEXT("+%d stat point%s. %d to spend: Tab, then the + on a score."), Points, Points == 1 ? TEXT("") : TEXT("s"),
			Stats->GetUnspentStatPoints()));
	}
	if (const DCCBasicClasses::FClass* MyClass = DCCBasicClasses::Find(this))
	{
		if (!Learned.IsEmpty())
		{
			Lines.Add(FString::Printf(TEXT("%s lesson: %s."), MyClass->Name, *Learned));
		}
		const FString Bonus = DCCBasicClasses::BonusLine(this);
		if (!Bonus.IsEmpty())
		{
			Lines.Add(FString::Printf(TEXT("%s: %s."), MyClass->Name, *Bonus));
		}
		if (const DCCBasicClasses::FNode* Next = DCCBasicClasses::NextLesson(this))
		{
			Lines.Add(FString::Printf(TEXT("Next lesson at level %d: %s."), Next->Level, Next->Title));
		}
	}
	else
	{
		Lines.Add(TEXT("No class yet. Pick one on the Character page and it starts teaching."));
	}
	int32 NewPerks = 0;
	for (int32 Tier = 0; Tier < DCCClassPerks::TierCount; ++Tier)
	{
		const int32 At = DCCClassPerks::TierLevel(Tier);
		NewPerks += From < At && To >= At && DCCBasicClasses::Find(this) ? 1 : 0;
	}
	if (NewPerks > 0)
	{
		Lines.Add(FString::Printf(TEXT("PERK%s UNLOCKED: %d waiting. Tab, then SKILLS. Choose wisely. Or don't. The audience prefers the other thing."),
			NewPerks > 1 ? TEXT("S") : TEXT(""), NewPerks));
	}
	if (Stats->IsAtMaxLevel())
	{
		Lines.Add(TEXT("That is the top. The System is out of numbers for you."));
	}

	UE_LOG(LogTemp, Log, TEXT("[DCC Level] %s %d > %d: %s"), *GetName(), From, To, *FString::Join(Lines, TEXT(" / ")));
	if (Onboarding)
	{
		FDCCSystemWindowPayload Window;
		Window.Header = FText::FromString(TEXT("LEVEL UP"));
		Window.Title = FText::FromString(To - From > 1 ? FString::Printf(TEXT("Level %d > %d"), From, To) : FString::Printf(TEXT("Level %d"), To));
		Window.Body = FText::FromString(FString::Join(Lines, TEXT("|")));
		Window.Style = EDCCSystemWindowStyle::Reward;
		Onboarding->ShowWindow(Window);
	}
}
