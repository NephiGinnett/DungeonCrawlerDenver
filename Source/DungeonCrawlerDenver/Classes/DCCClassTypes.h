#pragma once

#include "CoreMinimal.h"
#include "Engine/DataTable.h"
#include "Stats/DCCAbilityTypes.h"
#include "Stats/DCCDamageTypes.h"
#include "DCCClassTypes.generated.h"

// -----------------------------------------------------------------------------
// THE THIRD FLOOR CLASS SYSTEM
//
// See Docs/races-and-classes.md for the full point-buy Build System this data
// implements, and its "Notes / TODO" section for what's built vs. still open.
//
// Deliberately a small closed set of benefit types, same reasoning as
// EDCCAchievementStat: every value here is something the game already computes
// (a Stat, a Skill Rank), so adding a benefit type is an honest engineering
// cost rather than a promise the data can't keep. Resistance/Advantage-grant/
// Pet/movement-mode benefit types are NOT here yet because the systems they'd
// hook into don't exist - see the doc's gap analysis. A class row can still
// document those benefits in FlavorText; they just don't do anything mechanical
// until their own primitive is built.
// -----------------------------------------------------------------------------

UENUM(BlueprintType)
enum class EDCCClassBenefitType : uint8
{
	/** Adds Amount to the base (Unenhanced) score for Ability. */
	StatBonus   UMETA(DisplayName = "Stat Bonus"),

	/** Grants/raises SkillId to at least Rank Amount via GrantSkill - a real,
	 *  trained Rank, not an invisible overlay, so Advantage/Disadvantage and
	 *  advancement all treat it exactly like a Rank earned through play. */
	SkillBonus  UMETA(DisplayName = "Skill Bonus"),

	/** Adds Amount (cm/s) to the character's movement speed override. */
	MoveSpeedBonus UMETA(DisplayName = "Move Speed Bonus"),

	/** Sets ResistDamageType to ResistLevel - see EDCCResistanceLevel for how
	 *  multiple sources disagreeing (this and an equipped item, say) resolve. */
	DamageResistance UMETA(DisplayName = "Damage Resistance"),

	MAX         UMETA(Hidden)
};

/**
 * One line of a Class's benefit list.
 *
 * SourceLabel is shown verbatim in the Feed's click-to-inspect roll breakdown
 * (ADCCCharacterBase::GetAbilityScoreBreakdown / GetSkillRankBreakdown) - it is
 * the answer to "which specific skill, class, race is giving them that",
 * authored once here rather than reconstructed from the class name at display
 * time, so a class that grants the same Stat twice for two different reasons
 * can say so.
 */
USTRUCT(BlueprintType)
struct FDCCClassBenefit
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Benefit")
	EDCCClassBenefitType Type = EDCCClassBenefitType::StatBonus;

	/** Read for StatBonus only. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Benefit")
	EDCCAbility Ability = EDCCAbility::Strength;

	/** Read for SkillBonus only. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Benefit")
	FName SkillId;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Benefit")
	int32 Amount = 0;

	/** Read for DamageResistance only. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Benefit")
	EDCCDamageType ResistDamageType = EDCCDamageType::Physical;

	/** Read for DamageResistance only. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Benefit")
	EDCCResistanceLevel ResistLevel = EDCCResistanceLevel::Normal;

	/** e.g. "Class: Storm Chaser" - shown in the roll breakdown UI. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Benefit")
	FText SourceLabel;
};

/**
 * One Class, as a DataTable row - the Build System's output, authored rather
 * than computed at runtime. Point Build math happens once, by whoever designs
 * the class (see Docs/races-and-classes.md); this struct is just the result.
 */
USTRUCT(BlueprintType)
struct FDCCClassDef : public FTableRowBase
{
	GENERATED_BODY()

	/** Stable identifier. Must match the row name. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Class")
	FName ClassId;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Class")
	FText DisplayName;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Class", meta = (MultiLine = "true"))
	FText FlavorText;

	/** Alien Classes get popularity hooks Earth Classes don't, per the
	 *  sourcebook - not wired to anything yet; recorded for when Favor/Chatter
	 *  grows a class-aware hook. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Class")
	bool bIsAlienClass = false;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Class")
	TArray<FDCCClassBenefit> Benefits;

	/** DT_DCCEquipment row ids granted and auto-equipped the moment this Class
	 *  is selected - Zoe's own requirement that a Class hand you its tools, not
	 *  just abstract bonuses. Slot conflicts resolve last-wins, same as any
	 *  other Equip call. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Class")
	TArray<FName> StarterEquipmentIds;
};
