#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "DCCRallyTrailer.generated.h"

class ACameraActor;
class AStaticMeshActor;
class ADCCCharacterBase;
class ADCCRover;

/**
 * The Repo Rally ad: a 45-second camera cut of the crew on a shift, played in a Repo Rally round.
 *
 *    0- 3 s  a short look off the gate into the city
 *    3-10 s  the four bays, each crawler on an emote
 *   10-16 s  the hook holds: the mark fights the line, stress lines on the hitch
 *   16-24 s  the pull: the mark comes in and the truck drags it out the gate
 *   24-34 s  Zoe in the cab, windows down, up the skyway
 *   34-45 s  the drop: the car on the bumper, the crew, the gate
 *
 * Same capture path as the Fourteener ad. "DCC.RallyTrailer capture <dir>" writes the frames.
 * Tools/make_trailer.py --rally runs it.
 */
UCLASS()
class DUNGEONCRAWLERDENVER_API ADCCRallyTrailerDirector : public AActor
{
	GENERATED_BODY()

public:
	ADCCRallyTrailerDirector();
	virtual void Tick(float DeltaSeconds) override;

	bool bCapture = false;
	FString CaptureDir;

	static constexpr float Length = 52.0f;
	/** 34-42 s: the owner comes for Kyle's truck. The drop follows. */
	static constexpr float DropStart = 42.0f;
	static constexpr float PreRoll = 8.0f;

private:
	void Setup();
	ADCCCharacterBase* SpawnCrawler(FName Who, const FVector& At, float Yaw, FName Cosmetic, const TCHAR* IdlePath);
	ADCCRover* SpawnTruck(const FVector& At, float Yaw, int32 Body, const FString& Plate);
	void Place(ADCCRover* Truck, const FVector& At, float Yaw);
	void Pose(float T);
	void Shoot(float T);

	UPROPERTY(Transient)
	TObjectPtr<ACameraActor> Camera;

	UPROPERTY(Transient)
	TObjectPtr<AStaticMeshActor> Cable;

	// Trailer-only. Neon flicks around the hitch while the mark is fighting the line.
	TArray<TObjectPtr<AStaticMeshActor>> Stress;

	UPROPERTY(Transient)
	TArray<TObjectPtr<ADCCCharacterBase>> Crew;

	TWeakObjectPtr<ADCCCharacterBase> Scott, CJ, Marcel, Vaughn, Kyle, Zoe;
	/** The angry owner and his friend with a rifle (the owner beat). Not "Owner": AActor has one. */
	TWeakObjectPtr<class ADCCRallyThug> Angry, Gunner;
	TWeakObjectPtr<class ATextRenderActor> ShoutCard;
	TSet<int32> ShotsFired;
	void OwnerBeat(float T);
	void SetClip(ADCCCharacterBase* Crawler, const TCHAR* Path);
	TWeakObjectPtr<ADCCRover> BayScott, BayCJ, BayMarcel, BayVaughn, HookTruck, HookMark, SkyTruck, DropMark;
	FVector Origin = FVector::ZeroVector;
	float Elapsed = 0.0f;
	int32 Frame = 0;
	bool bSetUp = false;
	bool bDone = false;
};
