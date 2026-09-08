#pragma once

#include "CoreMinimal.h"
#include "Dialogue/DCCDialogueNPC.h"
#include "DCCDialogueNPC_RaceBookie.generated.h"

class ADCCRaceTrack;
class ADCCCharacterBase;

/**
 * The NPC who runs the circuit. Talk to him to put a wager on and send the
 * field away; talk to him again once it's over to do it all over.
 *
 * Zoe, 2026-09-05: "maybe just add a bookie NPC that lets you repeat the race."
 * Repeating is the load-bearing part - ADCCRaceTrack::StartRace deliberately
 * never refuses on the grounds that a field already exists, so this needs no
 * reset verb and no second menu row.
 *
 * Same shape as ADCCDialogueNPC_LiftAttendant, and for the same reasons: a
 * public non-virtual entry point (PlaceBetAndStartRace) that ADCCCharacterBase's
 * own NPC menu calls directly, so the Talk row keeps meaning "say the next
 * scripted line" rather than accidentally taking somebody's money.
 */
UCLASS()
class DUNGEONCRAWLERDENVER_API ADCCDialogueNPC_RaceBookie : public ADCCDialogueNPC
{
	GENERATED_BODY()

public:
	ADCCDialogueNPC_RaceBookie();

	/** The circuit this bookie takes bets on. The menu row does not appear at all without one. */
	UPROPERTY(EditInstanceOnly, Category = "Bookie")
	TObjectPtr<ADCCRaceTrack> LinkedTrack;

	/**
	 * Gold taken per race. Free entry if 0 - which is a legitimate setup for a
	 * bookie whose real product is the spectacle, and keeps the feature usable
	 * by a broke party rather than gating it behind a currency grind.
	 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Bookie")
	int32 WagerAmount = 50;

	/** Multiplier on WagerAmount paid out when the backed car wins. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Bookie")
	float WinMultiplier = 3.0f;

	/** Who the current bet is on, and who placed it. Cleared on payout. */
	UPROPERTY(BlueprintReadOnly, Category = "Bookie")
	int32 BackedRacerIndex = INDEX_NONE;

	UPROPERTY(BlueprintReadOnly, Category = "Bookie")
	TObjectPtr<ADCCCharacterBase> Bettor;

	/**
	 * Server-side. Takes the wager, picks a car to back, and starts the race.
	 * Called directly by ADCCCharacterBase::HandleNPCInteractionConfirm for
	 * EDCCNPCInteractionOption::RaceBet.
	 */
	UFUNCTION(BlueprintCallable, Category = "Bookie")
	void PlaceBetAndStartRace(ADCCCharacterBase* Interactor);

	virtual void Tick(float DeltaSeconds) override;
	virtual void TryPlayerInteract(ADCCCharacterBase* Interactor) override;

protected:
	/** So the payout fires exactly once when the race resolves, rather than every tick after it. */
	bool bAwaitingResult = false;

	void SettleBet();
};
