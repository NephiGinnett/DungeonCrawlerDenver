#pragma once

#include "CoreMinimal.h"
#include "Dialogue/DCCDialogueNPC.h"
#include "DCCImpoundNPC.generated.h"

class UDCCWalletComponent;

/**
 * Someone who works the Mile High impound during Repo Rally (2026-11-13).
 * Who they are (Lot Boss Rhee, Clerk Jan, Flagger Diaz) and everything they
 * say comes from Tools/gen_impound_lines.py, same path as Fort Tiner, so a
 * line recorded in the Voice Studio plays here with no import.
 *
 * Spawned by the server (ADCCArcadeKart) with Who set before it finishes
 * spawning. They talk to whoever is sitting in a car nearby: a rover does not
 * trip the talk sphere, so each machine counts its own driver by distance.
 */
UCLASS()
class DUNGEONCRAWLERDENVER_API ADCCImpoundNPC : public ADCCDialogueNPC
{
	GENERATED_BODY()

public:
	ADCCImpoundNPC();

	virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;
	virtual void BeginPlay() override;
	virtual void Tick(float DeltaSeconds) override;

	/** A speaker id from gen_impound_lines.py ("Rhee", "Jan", "Diaz"). */
	UPROPERTY(Replicated)
	FName Who;

protected:
	virtual void BuildScript() override;

	UPROPERTY(VisibleAnywhere)
	TObjectPtr<UDCCWalletComponent> Outfit;
};
