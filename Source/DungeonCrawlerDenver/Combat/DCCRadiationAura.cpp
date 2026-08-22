#include "Combat/DCCRadiationAura.h"

#include "Characters/DCCCharacterBase.h"
#include "Stats/DCCStatsComponent.h"
#include "Combat/DCCGameState.h"
#include "UI/DCCEventLogTypes.h"
#include "Engine/World.h"
#include "EngineUtils.h"

UDCCRadiationAura::UDCCRadiationAura()
{
	PrimaryComponentTick.bCanEverTick = true;

	// Four times a second is plenty for a hazard measured in seconds of standing
	// still, and keeps the world scan off the render frame budget.
	PrimaryComponentTick.TickInterval = 0.25f;
}

float UDCCRadiationAura::GetDoseFor(const ADCCCharacterBase* Character) const
{
	if (!Character)
	{
		return 0.0f;
	}

	const float* Found = Doses.Find(Character);
	return Found ? *Found : 0.0f;
}

float UDCCRadiationAura::GetTotalDoseOn(const ADCCCharacterBase* Character)
{
	if (!Character || !Character->GetWorld())
	{
		return 0.0f;
	}

	float Highest = 0.0f;

	// Highest rather than summed: two sources overlapping should feel like a
	// worse room, not like double jeopardy for one mistake.
	for (TObjectIterator<UDCCRadiationAura> It; It; ++It)
	{
		const UDCCRadiationAura* Aura = *It;
		if (Aura && Aura->GetWorld() == Character->GetWorld())
		{
			Highest = FMath::Max(Highest, Aura->GetDoseFor(Character));
		}
	}

	return Highest;
}

void UDCCRadiationAura::TickComponent(float DeltaSeconds, ELevelTick TickType,
	FActorComponentTickFunction* ThisTickFunction)
{
	Super::TickComponent(DeltaSeconds, TickType, ThisTickFunction);

	AActor* Owner = GetOwner();
	UWorld* World = GetWorld();
	if (!Owner || !World || Owner->GetLocalRole() != ROLE_Authority)
	{
		return;
	}

	// A dead source stops contaminating. The Nuclear Family raise each other's
	// Intensity on death instead, which is a deliberate escalation rather than
	// this leaking on forever.
	if (const ADCCCharacterBase* OwningCharacter = Cast<ADCCCharacterBase>(Owner))
	{
		if (!OwningCharacter->IsAlive())
		{
			return;
		}
	}

	TimeUntilDamage -= DeltaSeconds;
	const bool bApplyDamage = TimeUntilDamage <= 0.0f;
	if (bApplyDamage)
	{
		TimeUntilDamage = DamageInterval;
	}

	const FVector Origin = Owner->GetActorLocation();
	const float EffectiveRadius = Radius * Intensity;
	const float RadiusSquared = FMath::Square(EffectiveRadius);

	// Anyone previously exposed still needs their dose decayed, so the map is
	// walked rather than only the characters currently inside the radius.
	for (TActorIterator<ADCCCharacterBase> It(World); It; ++It)
	{
		ADCCCharacterBase* Character = *It;

		// Only crawlers. The family are not hurt by their own condition, and a
		// mob taking chip damage from an ally would read as a bug.
		if (!Character || !Character->IsCrawler() || !Character->IsAlive())
		{
			continue;
		}

		const bool bInside =
			FVector::DistSquared(Origin, Character->GetActorLocation()) <= RadiusSquared;

		float& Dose = Doses.FindOrAdd(Character);

		if (bInside)
		{
			Dose = FMath::Min(MaxDose, Dose + DoseGainPerSecond * Intensity * DeltaSeconds);
		}
		else
		{
			Dose = FMath::Max(0.0f, Dose - DoseDecayPerSecond * DeltaSeconds);
			continue;
		}

		if (!bApplyDamage || Dose <= SafeDose)
		{
			continue;
		}

		const int32 Damage = FMath::Max(1,
			FMath::RoundToInt((Dose - SafeDose) * DamagePerDose * Intensity));

		if (UDCCStatsComponent* Stats = Character->Stats)
		{
			Stats->ApplyDamage(Damage);

			// Announced only on the tick they cross out of safe, not every tick -
			// otherwise the feed becomes a wall of identical lines and stops
			// being read at all, which is worse than saying nothing.
			if (Dose - DoseGainPerSecond * Intensity * DamageInterval <= SafeDose)
			{
				ADCCGameState::Log(this, EDCCLogCategory::Combat, FText::Format(
					NSLOCTEXT("DCC", "LogIrradiated",
						"{0} is accumulating a dose. The Family are delighted to have company."),
					Character->DisplayName.IsEmpty()
						? FText::FromString(Character->GetActorNameOrLabel())
						: Character->DisplayName));
			}
		}
	}
}
