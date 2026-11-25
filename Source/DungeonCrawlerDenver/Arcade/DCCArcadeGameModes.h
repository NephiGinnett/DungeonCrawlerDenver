#pragma once

#include "CoreMinimal.h"
#include "Combat/DCCGameMode.h"
#include "DCCArcadeGameModes.generated.h"

class ADCCArcadeArena;

/**
 * Base for the arcade's game modes. Picked per travel with "?game=" (see
 * DCCArcade::Travel), on the engine's empty map. Copies the pawn and player
 * controller from the main game's GameMode Blueprint, so every game uses the
 * same crawler, input and HUD, and spawns its arena (and PlayerStarts) before
 * anyone logs in. Seamless travel keeps each player's name, and so their saves,
 * from game to game.
 */
UCLASS(Abstract)
class DUNGEONCRAWLERDENVER_API ADCCArcadeGameMode : public ADCCGameMode
{
	GENERATED_BODY()

public:
	ADCCArcadeGameMode();
	virtual void InitGame(const FString& MapName, const FString& Options, FString& ErrorMessage) override;

protected:
	virtual TSubclassOf<ADCCArcadeArena> GetArenaClass() const { return nullptr; }
};

UCLASS()
class DUNGEONCRAWLERDENVER_API ADCCLobbyGameMode : public ADCCArcadeGameMode
{
	GENERATED_BODY()
protected:
	virtual TSubclassOf<ADCCArcadeArena> GetArenaClass() const override;
};

UCLASS()
class DUNGEONCRAWLERDENVER_API ADCCClimbGameMode : public ADCCArcadeGameMode
{
	GENERATED_BODY()
protected:
	virtual TSubclassOf<ADCCArcadeArena> GetArenaClass() const override;
};

UCLASS()
class DUNGEONCRAWLERDENVER_API ADCCKartGameMode : public ADCCArcadeGameMode
{
	GENERATED_BODY()
protected:
	virtual TSubclassOf<ADCCArcadeArena> GetArenaClass() const override;
};

UCLASS()
class DUNGEONCRAWLERDENVER_API ADCCKartPracticeGameMode : public ADCCArcadeGameMode
{
	GENERATED_BODY()
protected:
	virtual TSubclassOf<ADCCArcadeArena> GetArenaClass() const override;
};

UCLASS()
class DUNGEONCRAWLERDENVER_API ADCCNeonGameMode : public ADCCArcadeGameMode
{
	GENERATED_BODY()
protected:
	virtual TSubclassOf<ADCCArcadeArena> GetArenaClass() const override;
};

/**
 * The pose bench as its own program (Tools/pose_bench.bat, "Pose bench"
 * shortcut): the front door's stage with the gear bench open on it and
 * nothing else. Saving writes the same DCCBench.json the game reads.
 */
UCLASS()
class DUNGEONCRAWLERDENVER_API ADCCBenchGameMode : public ADCCArcadeGameMode
{
	GENERATED_BODY()
protected:
	virtual TSubclassOf<ADCCArcadeArena> GetArenaClass() const override;
};

/** Where a launched game starts: pick your crawler, then on to the Den. */
UCLASS()
class DUNGEONCRAWLERDENVER_API ADCCFrontDoorGameMode : public ADCCArcadeGameMode
{
	GENERATED_BODY()
protected:
	virtual TSubclassOf<ADCCArcadeArena> GetArenaClass() const override;
};

UCLASS()
class DUNGEONCRAWLERDENVER_API ADCCHauntGameMode : public ADCCArcadeGameMode
{
	GENERATED_BODY()
protected:
	virtual TSubclassOf<ADCCArcadeArena> GetArenaClass() const override;
};

UCLASS()
class DUNGEONCRAWLERDENVER_API ADCCAuditGameMode : public ADCCArcadeGameMode
{
	GENERATED_BODY()
protected:
	virtual TSubclassOf<ADCCArcadeArena> GetArenaClass() const override;
};

/** Gate Run (Arcade/DCCArcadeGate). */
UCLASS()
class DUNGEONCRAWLERDENVER_API ADCCGateGameMode : public ADCCArcadeGameMode
{
	GENERATED_BODY()
protected:
	virtual TSubclassOf<ADCCArcadeArena> GetArenaClass() const override;
};

/** Dungeon Crawler Tower Defense (Arcade/DCCArcadeSiege). */
UCLASS()
class DUNGEONCRAWLERDENVER_API ADCCSiegeGameMode : public ADCCArcadeGameMode
{
	GENERATED_BODY()
protected:
	virtual TSubclassOf<ADCCArcadeArena> GetArenaClass() const override;
};

/** The Champions' Ring (Arcade/DCCArcadeRing). */
UCLASS()
class DUNGEONCRAWLERDENVER_API ADCCRingGameMode : public ADCCArcadeGameMode
{
	GENERATED_BODY()
protected:
	virtual TSubclassOf<ADCCArcadeArena> GetArenaClass() const override;
};
