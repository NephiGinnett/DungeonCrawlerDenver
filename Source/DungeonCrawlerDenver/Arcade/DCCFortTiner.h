#pragma once

#include "CoreMinimal.h"
#include "Dialogue/DCCDialogueNPC.h"
#include "DCCFortTiner.generated.h"

class UDCCWalletComponent;

/**
 * Someone who lives at Fort Tiner, the base camp at the foot of Fourteener
 * (2026-11-13). Who they are (Sheriff Dusty, Ma Ore, Old Gus, Banjo Bea) and
 * everything they say comes from Tools/gen_fort_tiner_lines.py, which also feeds
 * the Voice Studio, so a line recorded there plays here with no import
 * (ADCCDialogueNPC::PlayVoiceFor). They wear a hat from the Den shop's catalogue.
 *
 * Spawned by the server (ADCCArcadeClimb) with Who set before it finishes
 * spawning; Who replicates, so every machine builds the same person.
 */
UCLASS()
class DUNGEONCRAWLERDENVER_API ADCCFortTinerNPC : public ADCCDialogueNPC
{
	GENERATED_BODY()

public:
	ADCCFortTinerNPC();

	virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;
	virtual void BeginPlay() override;
	virtual void SayNextLine() override;

	/** A speaker id from gen_fort_tiner_lines.py ("Dusty", "MaOre", "Gus", "Bea"). */
	UPROPERTY(Replicated)
	FName Who;

protected:
	virtual void BuildScript() override;
	/** Lines whose row contains "Summit" stay quiet until this machine's crawler has topped Fourteener. */
	void SkipSummitLines();

	UPROPERTY(VisibleAnywhere)
	TObjectPtr<UDCCWalletComponent> Outfit;
};
