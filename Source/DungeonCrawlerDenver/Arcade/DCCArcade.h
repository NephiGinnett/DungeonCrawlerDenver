#pragma once

#include "CoreMinimal.h"

class UWorld;
class APlayerState;

/**
 * The Den: one host, several games (2026-11-13). Players gather in the lobby,
 * vote by standing on a game, and the host travels everyone into it. Each game
 * is a separate level load with its own GameMode, picked with the standard
 * "?game=" travel option, and every one of them returns to the lobby.
 *
 * Proof of concept for: several multiplayer games under one roof, with each
 * player's data kept separately per game and never lost between runs.
 *
 *   Dungeon Crawler Denver   the main game: Concourse Zero and the World Dungeon
 *   Fourteener               co-op climbing up a peak (original; Colorado's 14ers)
 *   Repo Rally               a city shift: aim the hook, tow the mark to the yard
 *   Neon Mile                endless synthwave driving, dodge the traffic
 *   Stress Test              forty seconds on a construct; fists, weapons and spells all count
 *
 * Records live in Session/DCCProfile (Profile_<Player>.sav, one number per
 * game per stat); the DCD character save stays separate in DCC_<Crawler>.sav.
 */
namespace DCCArcade
{
	struct FGame
	{
		FName Id;
		const TCHAR* Title;
		const TCHAR* Pitch;
		const TCHAR* Map;
		const TCHAR* GameMode;
		FName RecordStat;
		bool bHigherIsBetter;
		const TCHAR* RecordLabel;
		const TCHAR* Unit;
		FLinearColor Colour;
	};

	/** The games you can vote for, in lobby order. */
	const TArray<FGame>& Games();
	const FGame* Find(FName Id);

	/** Server. Takes everyone to a game ("Crawler", "Haunt", "Climb", "Kart", "Neon", "Audit"), to "Lobby", or to FrontDoorId. */
	bool Travel(UWorld* World, FName GameId);

	/**
	 * A launched game (not Play-in-Editor) that did not start at the front door
	 * goes there, once per run of the program: pick a crawler, then the Den.
	 * Skip with -nolobby.
	 */
	void MaybeStartInLobby(UWorld* World);

	/** Travel(World, FrontDoorId) goes to character select, which then goes to the Den. */
	inline const TCHAR* const FrontDoorId = TEXT("FrontDoor");

	/** The character select stage a launched game starts on. */
	bool IsFrontDoor(const UWorld* World);

	/** The pose bench running as its own program (ADCCBenchGameMode). */
	bool IsBenchTool(const UWorld* World);

	/** True when the loaded level is one of the arcade's own games or the lobby. */
	bool IsArcadeWorld(const UWorld* World);

	/**
	 * True in a game you can leave. False in the Safe Room, on the front door,
	 * and in the pose bench, which are already home.
	 */
	bool CanLeaveForLobby(const UWorld* World);

	/** "1,234 m", "1:02.5" and so on. */
	FString FormatRecord(const FGame& Game, float Value);

	/**
	 * Series lock (2026-11-19). Fourteener is open. A summit opens Dead Air and Stress Test.
	 * Filming Dead Air and landing a hit in Stress Test opens Gate Run and Repo Rally.
	 * Neon Mile and Dungeon Crawler Denver stay open. Zoe can vote for any of them.
	 */
	bool IsSeriesOpen(const APlayerState* Player, FName GameId);
	/** Why a pad refused this player. Empty when the game is open. */
	FString SeriesLockLine(const APlayerState* Player, FName GameId);
	/** The one-line goal shown on entering a game. Empty for the lobby. Every game, including the dungeon, has one. */
	FString SeriesQuestLine(FName GameId);

	/**
	 * What the OBJECTIVES panel should say (2026-11-21). Each entry is the current step of a short line.
	 * GameId None is the Den and the front door: the main line, plus a simple start for any line not begun.
	 * A game id is just that game's step, or the handoff into the next line once its own step is done.
	 */
	struct FBoardStep
	{
		FString Line;
		FString Now;
		FString Then;
	};
	void BoardSteps(const APlayerState* Player, FName GameId, TArray<FBoardStep>& Out);
	/** The same payout for every series quest. */
	int32 SeriesQuestXp();
	/** Marks Quest.<GameId>, grants Xp once, and puts that game's clear prize in the bag. True the first time. */
	bool CompleteSeriesQuest(APlayerState* Player, FName GameId, int32 Xp);
	/** The one prize for clearing this game, for the gold line. Null when the game has none. */
	const TCHAR* ClearPrizeLine(FName GameId);
}
