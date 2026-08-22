#pragma once

#include "CoreMinimal.h"
#include "DCCInteractionTypes.generated.h"

// -----------------------------------------------------------------------------
// TALKING TO PEOPLE
//
// The verbs a crawler can aim at another character who is not currently trying
// to kill them. Modelled on what a table actually does with an NPC rather than
// on what is cheap to implement: the published social skills are Persuasion,
// Deception and Intimidation, and a party's first three questions are almost
// always "who are you", "what do you know", and "will you come with us".
//
// Deliberately a small closed enum rather than data. Each option maps to real
// mechanics - a Charisma check, a party command, an inventory view - and a
// DataTable of options nobody has written code for would just be a menu that
// lies. Flavour text is data; the verbs are not.
// -----------------------------------------------------------------------------

UENUM(BlueprintType)
enum class EDCCInteractionOption : uint8
{
	/** Idle banter. Always available, costs nothing, advances nothing. */
	Chatter      UMETA(DisplayName = "Chatter"),

	/** Ask what they know about the floor. The hint channel. */
	AskAbout     UMETA(DisplayName = "Ask About This Place"),

	/** Charisma check. Warms them toward you, and can open Recruit. */
	Persuade     UMETA(DisplayName = "Persuade"),

	/**
	 * Charisma check with teeth. Succeeds harder and fails worse - the audience
	 * enjoys it either way, which is why it moves AI Favor when Persuade does not.
	 */
	Intimidate   UMETA(DisplayName = "Intimidate"),

	/** Ask them to join the party outright. Gated on already being friendly. */
	Recruit      UMETA(DisplayName = "Ask Them To Join"),

	/** Look at what they are carrying and wearing. */
	Inventory    UMETA(DisplayName = "Look At Their Gear"),

	/** Party command: resume escorting. */
	FollowMe     UMETA(DisplayName = "Follow Me"),

	/** Party command: stay put, whatever happens nearby. */
	HoldPosition UMETA(DisplayName = "Hold Position"),

	/** Pick a downed crawler back up before they bleed out. */
	Revive       UMETA(DisplayName = "Help Them Up"),

	/** Close the menu. Always last, always available. */
	Leave        UMETA(DisplayName = "Leave"),

	MAX          UMETA(Hidden)
};

/**
 * One row in the open interaction menu.
 *
 * Carries its own availability and the reason it is unavailable, so the menu can
 * *show* a greyed-out "Ask Them To Join" with "not friendly enough yet" beside
 * it. Hiding unavailable options would teach the player nothing about how to
 * make them available, which is the whole job of a menu like this.
 */
USTRUCT(BlueprintType)
struct FDCCInteractionEntry
{
	GENERATED_BODY()

	UPROPERTY(BlueprintReadOnly, Category = "Interaction")
	EDCCInteractionOption Option = EDCCInteractionOption::Chatter;

	UPROPERTY(BlueprintReadOnly, Category = "Interaction")
	FText Label;

	UPROPERTY(BlueprintReadOnly, Category = "Interaction")
	bool bAvailable = true;

	/** Shown beside a greyed row. Empty when the option is available. */
	UPROPERTY(BlueprintReadOnly, Category = "Interaction")
	FText UnavailableReason;
};
