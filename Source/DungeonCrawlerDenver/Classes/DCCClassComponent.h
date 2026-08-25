#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "Classes/DCCClassTypes.h"
#include "DCCClassComponent.generated.h"

DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FDCCClassSelected, FName, ClassId);

/**
 * The Third Floor Class selection - see Docs/races-and-classes.md.
 *
 * Shape mirrors UDCCEquipmentComponent one level up: a DataTable row per Class
 * holding typed benefits, folded into the character the same way Equipment
 * already folds gear bonuses into the Enhanced stat layer
 * (UDCCStatsComponent::RecalculateEnhancedScores calls both). Server-
 * authoritative like everything else mechanical - a client may ask to select a
 * Class, but the benefits, the Skill grants and the starter kit are all
 * decided and applied here.
 */
UCLASS(ClassGroup = (DCC), meta = (BlueprintSpawnableComponent))
class DUNGEONCRAWLERDENVER_API UDCCClassComponent : public UActorComponent
{
	GENERATED_BODY()

public:
	UDCCClassComponent();

	virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;

	/** The Class library. Same self-healing ConstructorHelpers pattern as every
	 *  other content table in this project. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Class")
	TObjectPtr<UDataTable> ClassTable;

	UPROPERTY(BlueprintReadOnly, ReplicatedUsing = OnRep_SelectedClassId, Category = "Class")
	FName SelectedClassId;

	UFUNCTION(BlueprintPure, Category = "Class")
	bool HasSelectedClass() const { return !SelectedClassId.IsNone(); }

	/** Looks the definition up in ClassTable. Works even before selection. */
	UFUNCTION(BlueprintPure, Category = "Class")
	bool FindClass(FName ClassId, FDCCClassDef& OutClass) const;

	/**
	 * Server only. Applies ClassId's full benefit list: Stat bonuses fold into
	 * the next RecalculateEnhancedScores, Skill bonuses are granted as real
	 * Ranks via UDCCSkillComponent::GrantSkill, and the starter kit is equipped
	 * through UDCCEquipmentComponent::Equip. Re-selecting replaces the
	 * previous Class's Stat/Skill contribution outright (RecalculateEnhanced-
	 * Scores always rebuilds from scratch), matching the sourcebook's own
	 * "reassign once per floor during a Long Rest" allowance - nothing here
	 * enforces the once-per-floor cadence yet, that is DM/UI discipline for now.
	 */
	UFUNCTION(BlueprintCallable, Category = "Class")
	bool SelectClass(FName ClassId);

	UPROPERTY(BlueprintAssignable, Category = "Class")
	FDCCClassSelected OnClassSelected;

	/** Total bonus per Ability from the selected Class's StatBonus benefits.
	 *  Read by UDCCStatsComponent::RecalculateEnhancedScores, exactly mirroring
	 *  UDCCEquipmentComponent::AccumulateAbilityBonuses. */
	void AccumulateAbilityBonuses(TArray<int32>& OutBonuses) const;

	/** Every StatBonus benefit on the selected Class touching Ability, one
	 *  entry per benefit line - the Feed roll-breakdown's "Class" row(s). */
	UFUNCTION(BlueprintPure, Category = "Class")
	TArray<FDCCModifierSource> GetAbilityBonusSources(EDCCAbility Ability) const;

	/** Every SkillBonus benefit on the selected Class touching SkillId - the
	 *  Feed roll-breakdown's "Class" row for a Skill-based check. */
	UFUNCTION(BlueprintPure, Category = "Class")
	TArray<FDCCModifierSource> GetSkillBonusSources(FName SkillId) const;

	/** Every DamageResistance benefit on the selected Class - merged with
	 *  Equipment's own list by UDCCStatsComponent::RecalculateDamageResistances. */
	void AccumulateDamageResistances(TArray<FDCCResistanceSource>& OutSources) const;

	/** True if the selected Class has a GrantsMount benefit, with its Move
	 *  Speed bonus and label. False (and both out params untouched) otherwise -
	 *  ToggleMount uses this to refuse the toggle for a class that never
	 *  granted one. */
	UFUNCTION(BlueprintPure, Category = "Class")
	bool GetMountBenefit(float& OutMoveSpeedBonus, FText& OutLabel) const;

	/** Convenience finder, mirroring UDCCAchievementComponent::Find. */
	UFUNCTION(BlueprintPure, Category = "Class")
	static UDCCClassComponent* Find(AActor* Actor);

protected:
	UFUNCTION()
	void OnRep_SelectedClassId();

	/** Server only. Re-derives everything downstream of a Class change. */
	void ApplyBenefits(const FDCCClassDef& ClassDef);
};
