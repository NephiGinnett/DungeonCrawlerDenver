#pragma once

#include "CoreMinimal.h"
#include "Arcade/DCCArcadeArena.h"
#include "DCCArcadeFrontDoor.generated.h"

class ACameraActor;

/**
 * The front door (2026-11-13): where a launched game starts. A small lit stage
 * with your crawler on it and a camera on them, while the HUD shows the
 * character select (UDCCCharacterSelectWidget). Picking swaps the body on the
 * stage; confirming takes you to the Den.
 *
 * It is its own level load rather than an overlay in the Den because a
 * player's crawler is their identity everywhere (their name, their saves):
 * choosing before the Den means the Den, and every game after it, sees the
 * right name from the first login.
 */
UCLASS()
class DUNGEONCRAWLERDENVER_API ADCCArcadeFrontDoor : public ADCCArcadeArena
{
	GENERATED_BODY()

protected:
	virtual void BuildLocal() override;
	virtual void ClientTick(float DeltaSeconds) override;
	virtual void GetStartTransforms(TArray<FTransform>& Out) const override;
	virtual FSky GetSky() const override;

private:
	UPROPERTY(Transient)
	TObjectPtr<ACameraActor> StageCamera;
};
