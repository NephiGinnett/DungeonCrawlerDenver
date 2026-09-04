#pragma once

#include "CoreMinimal.h"
#include "Dialogue/DCCDialogueNPC.h"
#include "DCCDialogueNPC_LiftAttendant.generated.h"

class ADCCCliffLift;

/**
 * A talker who can also send you up - built for the bartender's cliff-lift offer near
 * the Diving Pool, but usable on any ADCCDialogueNPC placement with a lift nearby.
 *
 * Ordinary approach-triggered dialogue (HandleTalkOverlapBegin, inherited unchanged)
 * still plays exactly as it does on any other ADCCDialogueNPC. Only a deliberate E
 * press changes meaning here - the same shape as ADCCVendorNPC redirecting E to a
 * shop instead of the next scripted line, not a new interaction system.
 */
UCLASS()
class DUNGEONCRAWLERDENVER_API ADCCDialogueNPC_LiftAttendant : public ADCCDialogueNPC
{
	GENERATED_BODY()

public:
	/** The platform this NPC sends the player up on. Does nothing if left unset. */
	UPROPERTY(EditInstanceOnly, Category = "Lift")
	TObjectPtr<ADCCCliffLift> LinkedLift;

	/** Said the first time someone requests the lift. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Lift")
	FText RequestLine;

	/** Said if E is pressed again once the lift is already moving or has arrived. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Lift")
	FText AlreadyGoingLine;

	ADCCDialogueNPC_LiftAttendant();

	/**
	 * The actual "send me up" behaviour, pulled out of TryPlayerInteract so it
	 * has a public, non-virtual entry point - ADCCCharacterBase's own NPC menu
	 * (EDCCNPCInteractionOption::RideLift, see HandleNPCInteractionConfirm)
	 * calls this directly rather than through TryPlayerInteract, same
	 * "Talk must not accidentally trigger this" reasoning that already keeps
	 * the Talk row calling SayNextLine directly instead of going through
	 * ADCCVendorNPC's own TryPlayerInteract override.
	 */
	UFUNCTION(BlueprintCallable, Category = "Lift")
	void RequestLift(ADCCCharacterBase* Interactor);

	virtual void TryPlayerInteract(ADCCCharacterBase* Interactor) override;
};
