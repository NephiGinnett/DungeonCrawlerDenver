#pragma once

#include "CoreMinimal.h"
#include "Dialogue/DCCDialogueNPC.h"
#include "DCCShopkeeperNPC.generated.h"

/**
 * Cillinan, the alchemist behind the Den's prize counter (2026-11-23, Zoe: "Can you import that character into the game
 * as a shop keeper somewhere? Maybe at the prize counter? His name is Cillinan").
 *
 * The body is the Meshy alchemist imported with every PBR map (Tools/import_glb_pbr.py, /Game/DCC/Characters/Cillinan),
 * standing in its own sculpted pose: no animation yet (it is not on the Mannequin skeleton). Walk up and he greets you,
 * pitches two things on the counter and drops a quip; then he waits, and between customers he mutters. Lines from
 * SourceArt/Voice/NPC/cillinan_lines.json (Tools/gen_cillinan_lines.py), voices from SourceArt/Voice/NPC/Cillinan/.
 * Spawned by ADCCArcadeLobby::ServerBegin.
 */
UCLASS()
class DUNGEONCRAWLERDENVER_API ADCCShopkeeperNPC : public ADCCDialogueNPC
{
	GENERATED_BODY()

public:
	ADCCShopkeeperNPC();
	virtual void BeginPlay() override;
	virtual void Tick(float DeltaSeconds) override;

protected:
	virtual void BuildScript() override;

private:
	uint8 Phase = 0;            // 0 waiting, 1 a pitch to someone, 2 a mutter
	double NextPitchAt = 0.0;
	double NextMutterAt = 0.0;
	bool bMutterNext = false;
	/** A gentle breathing bob, since the body has no animation of its own. */
	float Breath = 0.0f;
	FVector MeshBase = FVector::ZeroVector;
};
