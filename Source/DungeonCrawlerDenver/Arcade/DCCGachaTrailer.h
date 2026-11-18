#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "DCCGachaTrailer.generated.h"

class ACameraActor;
class ADCCArcadeLobby;
class ADCCCharacterBase;

/**
 * The summoning tubes ad (2026-11-17, Zoe: "a fun video of Marcel playing the gacha, summoning one friend, turning to a
 * pile of money or tickets, and then hitting the button many times and summoning too many allies including a few of
 * the rarer variants").
 *
 *    0- 5 s  wide on the four tubes; Marcel walks up to a podium
 *    5- 6 s  over his shoulder: he presses it
 *    6-10 s  the tube: a friend shoots down it and steps out of the cut-out in the glass
 *   10-13 s  Marcel turns to his pile of tickets and gold, and nods
 *   13-27 s  he hammers the button: friends rain down all four tubes, Shiny and Shadow ones among them
 *   27-34 s  far too many allies, all dancing round him
 *   34-40 s  the camera rises off the crowd; the title
 *
 * The pulls are scripted (who, which rarity, which variant), so the ad always shows the variety; everything else is the
 * game's own: the tubes' light and beam, the drop, the overlays, the banner and the allies following him. Run in the
 * Den: "DCC.GachaTrailer [capture <dir>]". Tools/make_trailer.py --gacha renders it.
 */
UCLASS(NotPlaceable)
class DUNGEONCRAWLERDENVER_API ADCCGachaTrailerDirector : public AActor
{
	GENERATED_BODY()

public:
	ADCCGachaTrailerDirector();
	virtual void Tick(float DeltaSeconds) override;

	bool bCapture = false;
	FString CaptureDir;
	static constexpr float Length = 40.0f;
	static constexpr float PreRoll = 4.0f;

private:
	void Setup();
	void BuildPile();
	void SetClip(ADCCCharacterBase* Who, const TCHAR* Path);
	void Act(float T);
	void Shoot(float T);
	void Pull(int32 Index);
	FVector Rel(float X, float Y, float Z) const;
	void Aim(const FVector& From, const FVector& At, float Fov);

	UPROPERTY(Transient)
	TObjectPtr<ACameraActor> Camera;
	UPROPERTY(Transient)
	TObjectPtr<ADCCCharacterBase> Marcel;
	UPROPERTY(Transient)
	TArray<TObjectPtr<ADCCCharacterBase>> Summoned;
	TWeakObjectPtr<ADCCArcadeLobby> Lobby;
	FVector Origin = FVector::ZeroVector;
	FVector Spot = FVector::ZeroVector;   // where Marcel stands at the podium
	FVector PileAt = FVector::ZeroVector; // his tickets
	int32 NextPull = 0;
	bool bDancing = false;
	/** The heap, bottom first, so it can be spent from the top down as he pulls. */
	TArray<TWeakObjectPtr<AActor>> PileProps;
	const TCHAR* MarcelClip = nullptr;
	float Elapsed = 0.0f;
	int32 Frame = 0;
	bool bSetUp = false;
	bool bDone = false;
};
