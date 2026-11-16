#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "DCCTrailer.generated.h"

class ACameraActor;
class ADCCCharacterBase;

/**
 * The Fourteener ad (2026-11-13): a 45-second, five-shot camera cut of crawlers
 * on the mountain, played in a Fourteener round.
 *
 *    0- 9 s  establishing: over Fort Tiner to the peak
 *    9-18 s  Scott struggling up a switchback face
 *   18-27 s  Marcel and Vaughn resting on a rock ledge, slow orbit
 *   27-38 s  crane up the express face past CJ, toward the summit
 *   38-45 s  Kyle and Zoe at the summit, the fort and the range beyond
 *
 * The crawlers are posed bodies (no controllers), wearing Den shop cosmetics.
 * "DCC.Trailer" plays it; "DCC.Trailer capture <dir>" also writes every frame
 * as a 1920x1080 PNG and quits at the end. Run that with -benchmark -fps=30 so
 * each frame is exactly 1/30 s of game time; Tools/make_trailer.py does all of
 * it and adds the letterbox, the title and the song with ffmpeg.
 */
UCLASS()
class DUNGEONCRAWLERDENVER_API ADCCTrailerDirector : public AActor
{
	GENERATED_BODY()

public:
	ADCCTrailerDirector();
	virtual void Tick(float DeltaSeconds) override;

	bool bCapture = false;
	FString CaptureDir;

	static constexpr float Length = 45.0f;
	/** Seconds spent at the first shot before rolling, so textures and shaders settle. */
	static constexpr float PreRoll = 8.0f;

private:
	void Setup();
	ADCCCharacterBase* SpawnCrawler(FName Who, const FVector& At, float Yaw, FName Cosmetic, bool bClimbing);
	void Pose(float T);
	void Shoot(float T);

	UPROPERTY(Transient)
	TObjectPtr<ACameraActor> Camera;

	UPROPERTY(Transient)
	TArray<TObjectPtr<ADCCCharacterBase>> Crew;

	TWeakObjectPtr<ADCCCharacterBase> Scott, Marcel, Vaughn, CJ, Kyle, Zoe;
	FVector Origin = FVector::ZeroVector;
	float Elapsed = 0.0f;
	int32 Frame = 0;
	bool bSetUp = false;
	bool bDone = false;
};
