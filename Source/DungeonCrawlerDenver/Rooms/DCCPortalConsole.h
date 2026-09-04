#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "DCCPortalConsole.generated.h"

class UStaticMeshComponent;
class UPointLightComponent;
class ADCCCharacterBase;
class ADCCExitPortal;

/**
 * The science lab's "turn on the portal" button.
 *
 * Deliberately a plain AActor with a TryPlayerInteract, same shape as
 * ADCCLootBoxStation - one specific job, found by the same generic
 * Visibility sweep every world interactable in this project already uses
 * (ADCCCharacterBase::SweepForInteractionCandidate), nothing to browse and
 * nothing to say. Split from ADCCExitPortal itself rather than folding the
 * button into the portal actor, because "press this switch" and "walk
 * through this doorway" are different interactions with different triggers -
 * exactly the reason ADCCOathgate's Trigger and ADCCFloorExit's Trigger are
 * each their own component, just one level up: here they are two actors so
 * the console can sit apart from the portal it switches on.
 */
UCLASS()
class DUNGEONCRAWLERDENVER_API ADCCPortalConsole : public AActor
{
	GENERATED_BODY()

public:
	ADCCPortalConsole();

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Portal")
	TObjectPtr<UStaticMeshComponent> Mesh;

	/** Reads red while off, green once pressed - legible at a glance, same convention as ADCCFloorExit's Sign. */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Portal")
	TObjectPtr<UPointLightComponent> IndicatorLight;

	/** Which portal this console switches on. Set once per placement. */
	UPROPERTY(EditInstanceOnly, Category = "Portal")
	TObjectPtr<ADCCExitPortal> Portal;

	/**
	 * Called from ADCCCharacterBase::HandleInteractPressed when this console is
	 * under the cursor. Local call, same shape as ADCCLootBoxStation's own -
	 * the actual activation is server-authoritative via
	 * ADCCCharacterBase::Server_ActivatePortalConsole, so there is nothing
	 * here a client could fake by skipping this function.
	 */
	void TryPlayerInteract(ADCCCharacterBase* Interactor);
};
