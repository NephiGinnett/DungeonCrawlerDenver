#pragma once

#include "CoreMinimal.h"
#include "Characters/DCCEnemyCharacter.h"
#include "DCCEnemyCharacter_Voldemortgage.generated.h"

class UDCCBossComponent;

/**
 * Lord Voldemortgage, Floor One's boss, from `Docs/DT_DCCEnemies.json` and
 * `Docs/floor-01-design.md`.
 *
 * Placed here as a real, tuned class rather than left for a Blueprint, so the
 * moment his model lands the only remaining step is
 * `SkeletalMeshTools.import_file` and a one-line mesh swap in this constructor
 * - not re-deriving numbers under time pressure.
 *
 * Tier 1-2 "Neighborhood boss", deliberately NOT the books' tier-6 Floor Boss:
 * the party will be level 2 and this is a tutorial floor. STR 3 / DEX 3 mean he
 * is never physically dangerous and never pretends to be; CON 7 buys the fight
 * enough length for its structure to play out; INT 8 pays for the summons.
 *
 * The phase structure - three escalating charges via `PhaseThresholds`
 * (Opening Statement / Late Notice / Final Demand, at 75% / 45% / 20% HP, each
 * with a thicker minion swarm) - is `UDCCBossComponent`'s job, not this
 * class's - summon counts are wave data on whatever `ADCCEncounter` eventually
 * hosts this fight, and the charge/lien mechanics are already generic. This
 * class only owns his numbers and his voice. See this class's own constructor
 * for the actual thresholds; this comment described the older single-charge
 * version and drifted out of sync when the three-phase escalation replaced it.
 */
UCLASS()
class DUNGEONCRAWLERDENVER_API ADCCEnemyCharacter_Voldemortgage : public ADCCEnemyCharacter
{
	GENERATED_BODY()

public:
	/**
	 * The Balloon Payment made visible: a sphere over his head that inflates as
	 * the channel runs and deflates when it ends.
	 *
	 * A component on the boss rather than a Niagara effect, because it has to be
	 * readable at arena distance and hold a stable silhouette while the party
	 * decides whether to run at him or shoot it. Scaled from the component's
	 * ChargeProgress every frame.
	 */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Boss")
	TObjectPtr<class UStaticMeshComponent> BalloonMesh;

	/**
	 * "CHANNELING - BALLOON PAYMENT", floating above the balloon while it
	 * inflates.
	 *
	 * The balloon itself already telegraphs the charge, but only to someone
	 * who already knows what an inflating sphere over his head means - this
	 * says it in words, the same way the Falls' climb hints exist so a
	 * mechanic never depends on the party having been told the rules outside
	 * the game. A TextRenderComponent for the same reason BalloonMesh is a
	 * plain mesh and not a Niagara effect: legible at arena distance, no
	 * asset to author, works the moment it compiles.
	 */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Boss")
	TObjectPtr<class UTextRenderComponent> ChannelingLabel;

	virtual void Tick(float DeltaSeconds) override;

	ADCCEnemyCharacter_Voldemortgage();

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "DCC")
	TObjectPtr<UDCCBossComponent> Boss;

	/**
	 * Height of the balloon's rest position above his root, in local Z. A
	 * property rather than a buried literal on purpose: it is tuned against
	 * whichever mesh is currently equipped, and the mesh has already changed
	 * once (the DiceWinged stand-in to the real model) - the next time it
	 * changes, this should be a Details-panel nudge, not a recompile.
	 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Boss")
	float BalloonBaseHeight = 220.0f;

protected:
	virtual void BeginPlay() override;
};
