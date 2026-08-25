#include "Classes/DCCClassComponent.h"

#include "Stats/DCCStatsComponent.h"
#include "Skills/DCCSkillComponent.h"
#include "Equipment/DCCEquipmentComponent.h"
#include "Characters/DCCCharacterBase.h"
#include "Net/UnrealNetwork.h"
#include "UObject/ConstructorHelpers.h"

UDCCClassComponent::UDCCClassComponent()
{
	PrimaryComponentTick.bCanEverTick = false;
	SetIsReplicatedByDefault(true);

	// Same self-healing pattern as every other content table in this project -
	// resolves at editor startup, so a table imported mid-session reads None
	// until the next restart rather than being broken.
	static ConstructorHelpers::FObjectFinder<UDataTable> DefaultTable(
		TEXT("/Game/DCC/Classes/DT_DCCClasses.DT_DCCClasses"));
	if (DefaultTable.Succeeded())
	{
		ClassTable = DefaultTable.Object;
	}
}

void UDCCClassComponent::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);

	DOREPLIFETIME(UDCCClassComponent, SelectedClassId);
}

bool UDCCClassComponent::FindClass(FName ClassId, FDCCClassDef& OutClass) const
{
	if (!ClassTable || ClassId.IsNone())
	{
		return false;
	}

	if (const FDCCClassDef* Row = ClassTable->FindRow<FDCCClassDef>(ClassId, TEXT("FindClass")))
	{
		OutClass = *Row;
		return true;
	}

	return false;
}

bool UDCCClassComponent::SelectClass(FName ClassId)
{
	if (GetOwnerRole() != ROLE_Authority)
	{
		return false;
	}

	FDCCClassDef ClassDef;
	if (!FindClass(ClassId, ClassDef))
	{
		return false;
	}

	SelectedClassId = ClassId;
	ApplyBenefits(ClassDef);

	OnClassSelected.Broadcast(ClassId);

	return true;
}

void UDCCClassComponent::ApplyBenefits(const FDCCClassDef& ClassDef)
{
	AActor* Owner = GetOwner();
	if (!Owner)
	{
		return;
	}

	// Skill grants first: a genuine trained Rank via GrantSkill, not an
	// invisible overlay, so Advantage/Disadvantage and advancement treat a
	// Class-granted Skill exactly like one earned through play. Raises to
	// Amount if the character already knows the Skill at a lower Rank; never
	// lowers an already-higher Rank (GrantSkill's own existing behaviour).
	if (UDCCSkillComponent* Skills = Owner->FindComponentByClass<UDCCSkillComponent>())
	{
		for (const FDCCClassBenefit& Benefit : ClassDef.Benefits)
		{
			if (Benefit.Type == EDCCClassBenefitType::SkillBonus && !Benefit.SkillId.IsNone())
			{
				Skills->GrantSkill(Benefit.SkillId, Benefit.Amount);
			}
		}
	}

	// Move Speed: same override every sprint/climb tweak already uses, so this
	// composes with them rather than fighting for the same field. Summed
	// across every MoveSpeedBonus line rather than set once, in case a class
	// ever wants two separate movement benefits to stack.
	if (ADCCCharacterBase* Character = Cast<ADCCCharacterBase>(Owner))
	{
		float MoveSpeedBonus = 0.0f;
		for (const FDCCClassBenefit& Benefit : ClassDef.Benefits)
		{
			if (Benefit.Type == EDCCClassBenefitType::MoveSpeedBonus)
			{
				MoveSpeedBonus += Benefit.Amount;
			}
		}
		if (MoveSpeedBonus != 0.0f)
		{
			Character->SetClassMoveSpeedBonus(MoveSpeedBonus);
		}
	}

	// Starter kit: auto-equipped through the real Equipment system, not a
	// cosmetic-only grant - Zoe's own requirement that picking a Class hands
	// you its tools. Equip() already handles slot conflicts (last-wins), so no
	// special-casing is needed here even if two starter items share a slot.
	if (UDCCEquipmentComponent* Equipment = Owner->FindComponentByClass<UDCCEquipmentComponent>())
	{
		for (const FName& ItemId : ClassDef.StarterEquipmentIds)
		{
			Equipment->Equip(ItemId);
		}
	}

	// Stat bonuses last: Equipment's own Equip() calls already trigger a
	// recalculation, so folding this in afterward guarantees the Enhanced
	// layer reflects both the new starter kit AND the new Stat bonuses in one
	// final pass rather than racing whichever happened last.
	if (UDCCStatsComponent* Stats = Owner->FindComponentByClass<UDCCStatsComponent>())
	{
		Stats->RecalculateEnhancedScores();
	}
}

void UDCCClassComponent::AccumulateAbilityBonuses(TArray<int32>& OutBonuses) const
{
	OutBonuses.SetNumZeroed(static_cast<int32>(EDCCAbility::MAX));

	FDCCClassDef ClassDef;
	if (!FindClass(SelectedClassId, ClassDef))
	{
		return;
	}

	for (const FDCCClassBenefit& Benefit : ClassDef.Benefits)
	{
		if (Benefit.Type != EDCCClassBenefitType::StatBonus)
		{
			continue;
		}

		const int32 Index = static_cast<int32>(Benefit.Ability);
		if (OutBonuses.IsValidIndex(Index))
		{
			OutBonuses[Index] += Benefit.Amount;
		}
	}
}

TArray<FDCCModifierSource> UDCCClassComponent::GetAbilityBonusSources(EDCCAbility Ability) const
{
	TArray<FDCCModifierSource> Sources;

	FDCCClassDef ClassDef;
	if (!FindClass(SelectedClassId, ClassDef))
	{
		return Sources;
	}

	for (const FDCCClassBenefit& Benefit : ClassDef.Benefits)
	{
		if (Benefit.Type == EDCCClassBenefitType::StatBonus && Benefit.Ability == Ability && Benefit.Amount != 0)
		{
			FDCCModifierSource& Source = Sources.AddDefaulted_GetRef();
			Source.Label = !Benefit.SourceLabel.IsEmpty() ? Benefit.SourceLabel : ClassDef.DisplayName;
			Source.Amount = Benefit.Amount;
		}
	}

	return Sources;
}

TArray<FDCCModifierSource> UDCCClassComponent::GetSkillBonusSources(FName SkillId) const
{
	TArray<FDCCModifierSource> Sources;

	FDCCClassDef ClassDef;
	if (SkillId.IsNone() || !FindClass(SelectedClassId, ClassDef))
	{
		return Sources;
	}

	for (const FDCCClassBenefit& Benefit : ClassDef.Benefits)
	{
		if (Benefit.Type == EDCCClassBenefitType::SkillBonus && Benefit.SkillId == SkillId && Benefit.Amount != 0)
		{
			FDCCModifierSource& Source = Sources.AddDefaulted_GetRef();
			Source.Label = !Benefit.SourceLabel.IsEmpty() ? Benefit.SourceLabel : ClassDef.DisplayName;
			Source.Amount = Benefit.Amount;
		}
	}

	return Sources;
}

void UDCCClassComponent::AccumulateDamageResistances(TArray<FDCCResistanceSource>& OutSources) const
{
	FDCCClassDef ClassDef;
	if (!FindClass(SelectedClassId, ClassDef))
	{
		return;
	}

	for (const FDCCClassBenefit& Benefit : ClassDef.Benefits)
	{
		if (Benefit.Type == EDCCClassBenefitType::DamageResistance && Benefit.ResistLevel != EDCCResistanceLevel::Normal)
		{
			FDCCResistanceSource& Source = OutSources.AddDefaulted_GetRef();
			Source.Label = !Benefit.SourceLabel.IsEmpty() ? Benefit.SourceLabel : ClassDef.DisplayName;
			Source.Type = Benefit.ResistDamageType;
			Source.Level = Benefit.ResistLevel;
		}
	}
}

bool UDCCClassComponent::GetMountBenefit(float& OutMoveSpeedBonus, FText& OutLabel) const
{
	FDCCClassDef ClassDef;
	if (!FindClass(SelectedClassId, ClassDef))
	{
		return false;
	}

	for (const FDCCClassBenefit& Benefit : ClassDef.Benefits)
	{
		if (Benefit.Type == EDCCClassBenefitType::GrantsMount)
		{
			OutMoveSpeedBonus = static_cast<float>(Benefit.Amount);
			OutLabel = !Benefit.SourceLabel.IsEmpty() ? Benefit.SourceLabel : ClassDef.DisplayName;
			return true;
		}
	}

	return false;
}

UDCCClassComponent* UDCCClassComponent::Find(AActor* Actor)
{
	return Actor ? Actor->FindComponentByClass<UDCCClassComponent>() : nullptr;
}

void UDCCClassComponent::OnRep_SelectedClassId()
{
	OnClassSelected.Broadcast(SelectedClassId);
}
