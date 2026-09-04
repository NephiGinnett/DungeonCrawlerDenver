#pragma once

#include "CoreMinimal.h"
#include "Engine/DataTable.h"
#include "DCCDialogueTypes.generated.h"

/**
 * One authored line of NPC dialogue.
 *
 * Authored rather than generated, deliberately. The AI narrator improvises
 * commentary on resolved events (see Docs/ai-narrator-integration.md), but these
 * lines carry plot - the bartender's hint teaches the boss fight, and the escalating
 * complaints are how the party learns who Voldemortgage is. A model reinventing them
 * per session would contradict itself between playthroughs, which is exactly the
 * failure the narrator's "never invent" rule exists to prevent.
 */
USTRUCT(BlueprintType)
struct FDCCDialogueLine : public FTableRowBase
{
	GENERATED_BODY()

	/** Stable identifier. Must match the row name. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Dialogue")
	FName LineId;

	/**
	 * Which NPC says it. An NPC plays every line sharing its speaker id, in
	 * table order.
	 *
	 * Grouping by speaker rather than listing lines on the actor keeps the whole
	 * floor's script in one file that reads top to bottom, which matters when the
	 * lines are meant to escalate across rooms.
	 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Dialogue")
	FName SpeakerId;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Dialogue")
	FText Text;

	/**
	 * Seconds this line stays on screen. Zero uses the NPC's default.
	 *
	 * Per-line because a four-word aside and a three-clause complaint should not
	 * hold the screen for the same time.
	 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Dialogue", meta = (ClampMin = "0.0"))
	float DisplaySeconds = 0.0f;

	/**
	 * Held back until the party has heard everything else this speaker has.
	 *
	 * The Safe Room posture from the tone bible: the bartender gives genuinely
	 * useful hints "eventually, if you're patient". This is that, mechanically -
	 * the hint is a reward for listening rather than the first thing said.
	 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Dialogue")
	bool bIsHint = false;

	/** Author-facing only. Never shown in game. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Dialogue")
	FString Notes;
};

/**
 * A row in the "E on an NPC" menu - ADCCCharacterBase::NPCInteractionOptions,
 * drawn by ADCCHUD::DrawNPCInteractionMenu. Deliberately small next to
 * EDCCInteractionOption (the ally menu): a plain ADCCDialogueNPC has nothing
 * to do but talk, and a vendor adds exactly one more verb on top of that.
 *
 * No separate "ask for rumors" row on purpose - FDCCDialogueLine::bIsHint is
 * "held back until the party has heard everything else... a reward for
 * listening", and a menu shortcut straight to the hint would undercut that.
 * Talk already surfaces it once earned.
 */
UENUM(BlueprintType)
enum class EDCCNPCInteractionOption : uint8
{
	/** Says the NPC's next unsaid line - same verb as walking up already triggers, on demand. */
	Talk,

	/** Vendor only - opens ADCCVendorNPC's existing shop menu. */
	Shop,

	/** Only shown when this NPC has at least one not-yet-active, not-yet-complete entry in OfferedQuestIds. */
	Quests,

	/** ADCCDialogueNPC_LiftAttendant only - only shown when LinkedLift is set. */
	RideLift,

	Leave,

	MAX UMETA(Hidden)
};
