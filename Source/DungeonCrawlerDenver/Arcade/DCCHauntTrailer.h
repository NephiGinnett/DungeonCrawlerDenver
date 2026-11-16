#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "DCCHauntTrailer.generated.h"

class ACameraActor;
class ADCCArcadeHaunt;
class ADCCCharacterBase;
class ADCCEnemyCharacter;

/**
 * The Dead Air ad (2026-11-15, Zoe: "make one for Dead Air that features several eventful moments or traps and comments
 * loading onto a video post extraction. It's important to have scenes with multiple players together").
 *
 * The whole crew, together, in a Dead Air round inside Mount Fourteener:
 *    0- 5 s  the cage: five crawlers, lamps on, red REC lights blinking
 *    5-13 s  walking a railed drift in single file
 *   13-19 s  a ghost miner in the cold light; the crew points and films
 *   19-27 s  the runaway cart: the rumble, then everyone dives off the rails
 *   27-33 s  a fight beside the powder crates, and the blast
 *   33-39 s  firedamp: every lamp burns blue, then the flash
 *   39-44 s  three knocks, dust, the roof comes down
 *   44-48 s  everyone back in the cage
 *   48-52 s  DATA EXTRACTION, bars over every head
 *   52-64 s  the Scroller: each crew member's post, comments piling up
 *
 * The world's own systems do the work (lamps, REC lights, the traps' effects, the extraction and the feed); this
 * director only places the crew, triggers the moments and moves the camera. Run in a Dead Air round:
 * "DCC.HauntTrailer [capture <dir>]". Tools/make_trailer.py --haunt renders it. Set dcc.MineSeed first for a layout
 * that has every trap (the script does).
 */
UCLASS(NotPlaceable)
class DUNGEONCRAWLERDENVER_API ADCCHauntTrailerDirector : public AActor
{
	GENERATED_BODY()

public:
	ADCCHauntTrailerDirector();
	virtual void Tick(float DeltaSeconds) override;

	bool bCapture = false;
	FString CaptureDir;

	static constexpr float Length = 64.0f;
	static constexpr float PreRoll = 6.0f;

private:
	void Setup();
	ADCCCharacterBase* SpawnCrawler(FName Who, FName Cosmetic);
	void SetClip(ADCCCharacterBase* Crawler, const TCHAR* Path);
	void Pose(float T);
	void Shoot(float T);
	void Cue(float T);
	void PlaceCrew(const FVector& Centre, const FVector& Facing, float Spread, const FVector& Look);

	UPROPERTY(Transient)
	TObjectPtr<ACameraActor> Camera;

	UPROPERTY(Transient)
	TArray<TObjectPtr<ADCCCharacterBase>> Crew;

	UPROPERTY(Transient)
	TObjectPtr<ADCCEnemyCharacter> Brute;

	TWeakObjectPtr<ADCCArcadeHaunt> Haunt;

	// Where the beats happen, found in the mine's layout at setup.
	FVector Cage = FVector::ZeroVector;
	FVector RunA = FVector::ZeroVector;
	FVector RunB = FVector::ZeroVector;
	FVector GhostAt = FVector::ZeroVector;
	FVector CrateAt = FVector::ZeroVector;
	FVector GasAt = FVector::ZeroVector;
	FVector RockAt = FVector::ZeroVector;
	int32 GhostIndex = -1;
	/** Props hidden out of the crew's way for the ghost shot, shown again after it. */
	TArray<TWeakObjectPtr<AActor>> Cleared;
	bool bClearedHuddle = false;
	/** The most open way out of the ghost's stope and away from the powder, for the crew and the lens. */
	FVector GhostDir = FVector(1, 0, 0);
	FVector CrateDir = FVector(1, 0, 0);
	FVector OpenDir(const FVector& From) const;
	/** Pull the lens in front of any rock between it and what it is looking at. */
	FVector ClearShot(const FVector& LookAt, const FVector& From) const;

	/** Server clock at each beat, so the Scroller's posts replay those moments. */
	TMap<FName, double> BeatTime;
	TSet<FName> Fired;

	float Elapsed = 0.0f;
	int32 Frame = 0;
	bool bSetUp = false;
	bool bDone = false;
};
