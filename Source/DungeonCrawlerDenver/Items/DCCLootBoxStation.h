#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "Items/DCCLootBoxTypes.h"
#include "DCCLootBoxStation.generated.h"

class ADCCCharacterBase;
class UStaticMeshComponent;
class UPointLightComponent;

/**
 * A physical, world-placed "open your loot boxes here" prop for one tier.
 *
 * Loot boxes themselves are not physical objects anywhere else in this
 * project - UDCCInventoryComponent::LootBoxCounts is just a per-tier int,
 * granted invisibly the moment an achievement is claimed (see
 * DCCAchievementComponent.cpp). This actor is the one place that count
 * becomes something to walk up to: one station per tier, placed in a Safe
 * Room, interact to open ONE owned box of that tier. It never itself holds
 * or rolls any loot - ADCCCharacterBase::OpenLootBox and
 * UDCCInventoryComponent::TryOpenLootBox do all of that exactly as if this
 * station did not exist, Safe Room re-check included. This class is
 * presentation and a place to stand, nothing else.
 *
 * Deliberately a plain AActor rather than ADCCLootContainer or
 * ADCCDialogueNPC - it has no itemised contents to browse (unlike a chest)
 * and nothing to say (unlike an NPC). Found by the same generic Visibility
 * sweep every other interactable in this project already uses
 * (ADCCCharacterBase::SweepForInteractionCandidate) - a static mesh blocks
 * that channel by default, so no special-casing was needed there, only a
 * new Cast branch in HandleInteractPressed.
 *
 * Mesh is a placeholder, swappable per instance rather than hardcoded -
 * Zoe's own placeholder-until-real-art pattern already used for
 * Voldemortgage's balloon and the Jukebox pickup. Point Mesh at whichever
 * "Battle Royale Props" mesh reads as the right tier once that pack is
 * imported; no recompile needed, it is just a per-instance asset swap.
 */
UCLASS()
class DUNGEONCRAWLERDENVER_API ADCCLootBoxStation : public AActor
{
	GENERATED_BODY()

public:
	ADCCLootBoxStation();

	/**
	 * Which tier this station opens. One station should exist per tier that
	 * has a table entry.
	 *
	 * `None` is a deliberate second meaning, not an unset value: a station
	 * with `Tier == None` is the claim-everything terminal instead of a
	 * single-tier one - see TryPlayerInteract. `None` was already defined as
	 * "not a reward, the zero/default state" on EDCCLootBoxTier itself, and a
	 * per-tier station never legitimately wants that value for its own
	 * property, so reusing it here needed no new enum and no new class.
	 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "LootBox")
	EDCCLootBoxTier Tier = EDCCLootBoxTier::Bronze;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "LootBox")
	TObjectPtr<UStaticMeshComponent> Mesh;

	/**
	 * The rarity tell. Colour and intensity are authored per placed instance
	 * (matching how every other light this project places is tuned) rather
	 * than derived from Tier in code, so the ramp can be retuned by eye
	 * without a recompile.
	 */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "LootBox")
	TObjectPtr<UPointLightComponent> Glow;

	/**
	 * Called from ADCCCharacterBase::HandleInteractPressed when this station
	 * is under the cursor. A local call, same shape as
	 * ADCCDialogueNPC::TryPlayerInteract - the actual open (and its Safe Room
	 * re-check) happens server-side inside Interactor->OpenLootBox, so there
	 * is nothing here a client could fake by skipping this function.
	 */
	void TryPlayerInteract(ADCCCharacterBase* Interactor);
};
