#pragma once

#include "CoreMinimal.h"
#include "Engine/DataTable.h"
#include "DCCQuestTypes.generated.h"

/**
 * What an objective is waiting for.
 *
 * A small closed set, for the same reason the achievement stats are: every value
 * here is something the game already computes and already reports. A quest type
 * nothing fires is a quest that silently never completes, which is far worse
 * than not having the type at all.
 */
UENUM(BlueprintType)
enum class EDCCObjectiveType : uint8
{
	/** Put down N characters. TargetId empty means anything hostile. */
	Kill            UMETA(DisplayName = "Kill"),

	/** Clear N encounters. */
	ClearEncounter  UMETA(DisplayName = "Clear Encounter"),

	/** Clear a whole floor. */
	ClearFloor      UMETA(DisplayName = "Clear Floor"),

	/** Stand in a marker's trigger. TargetId matches the marker's ObjectiveTag. */
	ReachLocation   UMETA(DisplayName = "Reach Location"),

	/** Fist-bump N allies. */
	Befriend        UMETA(DisplayName = "Befriend"),

	/** Pick up N props. */
	Collect         UMETA(DisplayName = "Collect"),

	/** Climb N metres. */
	Climb           UMETA(DisplayName = "Climb"),

	/**
	 * Gain N Skill Ranks in TargetId (added 2026-09-03, for "dev goal" quests
	 * that challenge a specific new feature - "gain 2 levels in driving" was
	 * the prompt, i.e. TargetId = "Piloting", RequiredCount = 2). Fired from
	 * UDCCSkillComponent::RunSkillAdvancement at the exact point a rank
	 * actually lands - the same funnel Character XP already uses there.
	 */
	SkillRankGained UMETA(DisplayName = "Skill Rank Gained"),

	/**
	 * Fired only by Blueprint or console.
	 *
	 * The escape hatch that keeps the enum honest: anything the DM wants to
	 * invent at the table goes here instead of growing this list with types the
	 * C++ never fires.
	 */
	Custom          UMETA(DisplayName = "Custom"),

	MAX             UMETA(Hidden)
};

/** One line of a quest: the thing to do, and how many times. */
USTRUCT(BlueprintType)
struct FDCCQuestObjective
{
	GENERATED_BODY()

	/**
	 * The words shown on the HUD.
	 *
	 * Written as an instruction rather than a description - "Clear the holding
	 * pen", not "The holding pen must be cleared" - because it is read at a
	 * glance mid-fight.
	 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Objective")
	FText Description;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Objective")
	EDCCObjectiveType Type = EDCCObjectiveType::Kill;

	/**
	 * Which specific thing, when the type needs one.
	 *
	 * For ReachLocation this must match a DCCQuestMarker's ObjectiveTag. Left
	 * empty, most types accept anything of the right kind.
	 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Objective")
	FName TargetId;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Objective", meta = (ClampMin = "1"))
	int32 RequiredCount = 1;

	/**
	 * Optional; hidden objectives still track, they just are not listed.
	 *
	 * Useful for the twist half of a quest - the tone bible's rooms all have one,
	 * and revealing it in the tracker before it happens spoils it.
	 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Objective")
	bool bHidden = false;
};

/**
 * One quest, as a DataTable row.
 *
 * Data rather than code, same as skills and achievements: the tabletop game
 * invents these at the table and Zoe adding one mid-session must not cost a
 * recompile.
 */
USTRUCT(BlueprintType)
struct FDCCQuestDef : public FTableRowBase
{
	GENERATED_BODY()

	/** Stable identifier. Must match the row name. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Quest")
	FName QuestId;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Quest")
	FText Title;

	/** One or two sentences, shown when the quest starts. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Quest")
	FText Summary;

	/** The System AI's line when the quest is handed out. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Quest")
	FText StartAnnouncement;

	/** The System AI's line when it is finished. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Quest")
	FText CompletionAnnouncement;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Quest")
	TArray<FDCCQuestObjective> Objectives;

	/** Paid to every living crawler on completion. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Quest", meta = (ClampMin = "0"))
	int32 ExperienceReward = 0;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Quest")
	int32 FavorReward = 0;

	/**
	 * Started automatically when this one completes. Empty ends the chain.
	 *
	 * Chaining lives on the quest rather than in a separate questline asset
	 * because a floor is a short sequence, not a branching tree, and one field
	 * expresses that without a second concept to maintain.
	 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Quest")
	FName NextQuestId;

	/** Started at BeginPlay. Exactly one quest in a table should have this set. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Quest")
	bool bAutoStart = false;
};

/** Live progress on one objective. */
USTRUCT(BlueprintType)
struct FDCCObjectiveProgress
{
	GENERATED_BODY()

	UPROPERTY(BlueprintReadOnly, Category = "Quest")
	int32 Count = 0;

	UPROPERTY(BlueprintReadOnly, Category = "Quest")
	bool bComplete = false;
};

/** Live state of one active or finished quest. */
USTRUCT(BlueprintType)
struct FDCCQuestState
{
	GENERATED_BODY()

	UPROPERTY(BlueprintReadOnly, Category = "Quest")
	FName QuestId;

	UPROPERTY(BlueprintReadOnly, Category = "Quest")
	TArray<FDCCObjectiveProgress> Objectives;

	UPROPERTY(BlueprintReadOnly, Category = "Quest")
	bool bComplete = false;

	/** World seconds when it started, so the tracker can order active quests. */
	UPROPERTY(BlueprintReadOnly, Category = "Quest")
	float StartedAtSeconds = 0.0f;
};
