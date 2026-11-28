#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "DCCSnickTape.generated.h"

class ACameraActor;
class ADCCCharacterBase;
class AStaticMeshActor;
class FJsonObject;

/**
 * THE SNICK TAPE (2026-11-25): a PRIMETIME short, rendered by Tools/snick_tape.py.
 *
 * Hiss Cranson (the lizard, a static figure that nods while he talks) asks CJ and Vaughn about the snicks in the Black
 * Lodge set; they deny it; the tape rolls in Elderboom village (a DCCSets set), the two of them dancing close; back to
 * the Lodge for Hiss's last word.
 *
 * Two files drive it, both in Saved/SnickTape: timeline.json (from snick_tape.py: when each shot starts, who speaks,
 * which clip each crawler plays) and layout.json (where everyone stands, every camera, every light). The layout is read
 * at start, so framing and lighting change without a rebuild.
 *
 *   DCC.SnickTape                    play it in real time (for test shots)
 *   DCC.SnickTape at <seconds>       start partway, to look at one moment
 *   DCC.SnickTape capture <dir>      write every frame (snick_tape.py render)
 */
UCLASS(NotPlaceable)
class DUNGEONCRAWLERDENVER_API ADCCSnickTapeDirector : public AActor
{
	GENERATED_BODY()

public:
	ADCCSnickTapeDirector();
	virtual void Tick(float DeltaSeconds) override;

	bool bCapture = false;
	FString CaptureDir;
	float StartAt = 0.0f;

private:
	bool Load();
	void Setup();
	void Act(float T);
	FVector Lodge(const TArray<TSharedPtr<class FJsonValue>>* Values) const;
	FVector Snick(const TArray<TSharedPtr<class FJsonValue>>* Values) const;
	ADCCCharacterBase* SpawnCrawler(const FName& Id, const FVector& At, float Yaw);
	void SetClip(ADCCCharacterBase* Who, const FString& Clip);
	void PlaceForSnick(bool bSnick);
	void ApplyShot(const FString& Shot, float Alpha);

	TSharedPtr<FJsonObject> Timeline;
	TSharedPtr<FJsonObject> Layout;

	UPROPERTY(Transient)
	TObjectPtr<ACameraActor> Camera;
	UPROPERTY(Transient)
	TObjectPtr<ADCCCharacterBase> CJ;
	UPROPERTY(Transient)
	TObjectPtr<ADCCCharacterBase> Vaughn;
	UPROPERTY(Transient)
	TObjectPtr<AStaticMeshActor> Hiss;

	FVector LodgeOrigin = FVector::ZeroVector;
	FVector SnickOrigin = FVector::ZeroVector;
	FVector SnickSpot = FVector::ZeroVector;
	FTransform HissRest;
	bool bSnickReady = false;
	bool bInSnick = false;
	FString LastShot;
	float ShotStart = 0.0f;
	float Elapsed = 0.0f;
	float ReadyAt = -1.0f;
	int32 Frame = 0;
	bool bSetUp = false;
	bool bDone = false;
};
