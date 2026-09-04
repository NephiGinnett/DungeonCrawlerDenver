#pragma once

#include "CoreMinimal.h"
#include "Dialogue/DCCDialogueNPC.h"
#include "DCCDialogueNPC_Signatory.generated.h"

class ADCCComplianceDoor;

/**
 * A claimant who can sign off on a Puzzles/ ADCCComplianceDoor.
 *
 * A subclass rather than properties bolted onto ADCCDialogueNPC, for exactly
 * the reason ADCCDialogueNPC_LiftAttendant is one: most NPCs in this game will
 * never authorise anything, and every talker should not carry a door pointer
 * around for the three on Floor 1 that do.
 *
 * WHY TALKING IS ENOUGH TO SIGN
 * The dispute at the shaft head (Docs/floor-01-design.md §8) is not a
 * persuasion check and should not become one. All three claimants WANT the
 * party to go down - each of them believes a crawler walking in on their
 * authority strengthens their filing. The obstacle was never convincing
 * anyone; it is that the party must work out they only ever needed two of the
 * three, and pick which two. So talking to a claimant IS the transaction, and
 * an extra check in front of it would only add friction to a puzzle whose
 * whole point is that the friction is administrative rather than social.
 */
UCLASS()
class DUNGEONCRAWLERDENVER_API ADCCDialogueNPC_Signatory : public ADCCDialogueNPC
{
	GENERATED_BODY()

public:
	ADCCDialogueNPC_Signatory();

	/** The door this claimant can authorise. */
	UPROPERTY(EditInstanceOnly, Category = "Compliance")
	TObjectPtr<ADCCComplianceDoor> ComplianceDoor;

	/**
	 * Which signature this NPC provides. Must appear in the door's
	 * RecognisedSignatories or the door will (correctly) refuse it.
	 *
	 * Defaults to SpeakerId when left empty, since the dialogue rows and the
	 * door's signatory list already use the same names on purpose - one less
	 * place for a placement to silently disagree with itself.
	 */
	UPROPERTY(EditAnywhere, Category = "Compliance")
	FName SignatoryId;

	virtual void TryPlayerInteract(ADCCCharacterBase* Interactor) override;
};
