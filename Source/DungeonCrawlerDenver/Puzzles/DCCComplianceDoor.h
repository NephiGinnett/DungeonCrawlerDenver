#pragma once

#include "CoreMinimal.h"
#include "Puzzles/DCCPuzzleDoor.h"
#include "DCCComplianceDoor.generated.h"

// ---------------------------------------------------------------------------
// BUREAUCRACY AS A LOCK
//
// The flagship puzzle of Docs/next-evolution-plan.md, and the gate on the
// Platinum Mine (Docs/floor-01-design.md, "The Claim").
//
// The point is that the bureaucracy IS the mechanic, not a fantasy riddle
// wearing a corporate hat. The door does not want a key, a lever, or a
// sequence. It wants a form, correctly stamped, by parties who each believe
// the others have no standing to stamp anything.
//
// Three authorities are arguing over who may authorise entry to a confirmed
// platinum deposit. Any TWO of them signing is enough - which is the whole
// design: the party does not need to resolve the dispute, only to notice that
// they only ever needed two. Backing a corporation is a real choice with a
// real consequence attached (see Docs/floor-01-design.md), but it is not the
// only route, and the surveyor's line bypasses the argument entirely.
//
// And per the folder's standing rule, it is still a door. It can be broken.
// ---------------------------------------------------------------------------

/**
 * A door that opens on signatures rather than on force.
 *
 * Stamps are held here rather than on any one crawler's quest log, because a
 * five-player party should be able to split up and collect them in parallel -
 * whoever talks to a signatory advances the whole party's form. Server-side
 * and replicated so every client's HUD can show progress honestly.
 */
UCLASS(Blueprintable)
class DUNGEONCRAWLERDENVER_API ADCCComplianceDoor : public ADCCPuzzleDoor
{
	GENERATED_BODY()

public:
	ADCCComplianceDoor();

	virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;

	/**
	 * Everyone who could conceivably sign. Authored as the full cast rather
	 * than as "the two you need", so which two the party actually gets is
	 * theirs to decide.
	 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Compliance")
	TArray<FName> RecognisedSignatories;

	/**
	 * How many of them have to sign. Two of three by default - enough that the
	 * party must engage with the dispute, few enough that they never have to
	 * settle it.
	 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Compliance")
	int32 RequiredSignatureCount = 2;

	/** Who has actually signed so far. */
	UPROPERTY(Replicated, BlueprintReadOnly, Category = "Compliance")
	TArray<FName> CollectedSignatures;

	/**
	 * Server only. Records a signature and unlocks once enough are in.
	 * Idempotent per signatory - a signatory cannot pad the count by being
	 * asked twice.
	 */
	UFUNCTION(BlueprintCallable, Category = "Compliance")
	void GrantSignature(FName SignatoryId);

	UFUNCTION(BlueprintPure, Category = "Compliance")
	bool HasSignature(FName SignatoryId) const { return CollectedSignatures.Contains(SignatoryId); }

	virtual bool IsPuzzleSatisfied() const override
	{
		return CollectedSignatures.Num() >= RequiredSignatureCount;
	}

	/** Progress line for the HUD, e.g. "Authorisations: 1 of 2". */
	UFUNCTION(BlueprintPure, Category = "Compliance")
	FText GetProgressText() const;

	virtual void TryPlayerInteract(ADCCCharacterBase* Interactor) override;
};
