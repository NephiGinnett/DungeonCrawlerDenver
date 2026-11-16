#include "Arcade/DCCArcade.h"

#include "Session/DCCSaveSystem.h"
#include "Arcade/DCCArcadeGameModes.h"
#include "Misc/CommandLine.h"
#include "Misc/Parse.h"
#include "Engine/World.h"
#include "HAL/IConsoleManager.h"

namespace DCCArcade
{
	const TArray<FGame>& Games()
	{
		static const TArray<FGame> List = {
			{ TEXT("Crawler"), TEXT("Dungeon Crawler Denver"),
				TEXT("Roguelike runs through the gates. Shop in Concourse Zero, then go down as far as you dare."),
				TEXT("/Game/ThirdPerson/Lvl_ThirdPerson?Hub=1"),
				TEXT("/Game/ThirdPerson/Blueprints/BP_ThirdPersonGameMode.BP_ThirdPersonGameMode_C"),
				TEXT("BestDepth"), true, TEXT("deepest floor"), TEXT("floor"), FLinearColor(1.0f, 0.75f, 0.3f) },
			{ TEXT("Haunt"), TEXT("Dead Air"),
				TEXT("Film the ghosts of the Lucky Tuesday, a silver mine deep inside Mount Fourteener, for a paranormal stream. Best footage gets the views. Be in the cage when it goes up."),
				TEXT("/Engine/Maps/Entry"), TEXT("/Script/DungeonCrawlerDenver.DCCHauntGameMode"),
				TEXT("BestViews"), true, TEXT("most views"), TEXT("views"), FLinearColor(0.45f, 1.0f, 0.55f) },
			{ TEXT("Climb"), TEXT("Fourteener"),
				TEXT("Co-op climbing. Click the rock to climb it, rest on ledges, reach the summit."),
				TEXT("/Engine/Maps/Entry"), TEXT("/Script/DungeonCrawlerDenver.DCCClimbGameMode"),
				TEXT("BestHeight"), true, TEXT("highest"), TEXT("ft"), FLinearColor(0.55f, 0.85f, 1.0f) },
			{ TEXT("Kart"), TEXT("Repo Rally"),
				TEXT("A shift in the city. Drive a wrecker, aim the hook, and tow marked cars back to the yard. Quick and clean pays. Don't hook the wrong car."),
				TEXT("/Engine/Maps/Entry"), TEXT("/Script/DungeonCrawlerDenver.DCCKartGameMode"),
				TEXT("BestScore"), true, TEXT("best shift"), TEXT("pts"), FLinearColor(0.85f, 0.5f, 0.25f) },
			{ TEXT("Neon"), TEXT("Neon Mile"),
				TEXT("Endless synthwave highway. Go as far as you can without hitting anything."),
				TEXT("/Engine/Maps/Entry"), TEXT("/Script/DungeonCrawlerDenver.DCCNeonGameMode"),
				TEXT("BestDistance"), true, TEXT("furthest"), TEXT("m"), FLinearColor(1.0f, 0.3f, 0.9f) },
			{ TEXT("Audit"), TEXT("Stress Test"),
				TEXT("Forty seconds on the construct. Fists, weapons and spells all count. The sponsor is taking notes."),
				TEXT("/Engine/Maps/Entry"), TEXT("/Script/DungeonCrawlerDenver.DCCAuditGameMode"),
				TEXT("BestDamage"), true, TEXT("hardest showing"), TEXT("dmg"), FLinearColor(1.0f, 0.55f, 0.15f) },
			{ TEXT("Gate"), TEXT("Gate Run"),
				TEXT("A gate opens over Denver. Walk in, clear 3 to 10 rooms of fights, trials and vaults, and drop the Gatekeeper before it collapses."),
				TEXT("/Engine/Maps/Entry"), TEXT("/Script/DungeonCrawlerDenver.DCCGateGameMode"),
				TEXT("BestScore"), true, TEXT("best run"), TEXT("pts"), FLinearColor(0.6f, 0.4f, 1.0f) },
		};
		return List;
	}

	const FGame* Find(FName Id)
	{
		return Games().FindByPredicate([Id](const FGame& Game) { return Game.Id == Id; });
	}

	FString FormatRecord(const FGame& Game, float Value)
	{
		if (FCString::Strcmp(Game.Unit, TEXT("s")) == 0)
		{
			const int32 Minutes = FMath::FloorToInt(Value / 60.0f);
			return FString::Printf(TEXT("%d:%04.1f"), Minutes, Value - Minutes * 60.0f);
		}
		if (FCString::Strcmp(Game.Unit, TEXT("floor")) == 0)
		{
			return FString::Printf(TEXT("floor %d"), FMath::RoundToInt(Value));
		}
		return FString::Printf(TEXT("%s %s"), *FText::AsNumber(FMath::RoundToInt(Value)).ToString(), Game.Unit);
	}

	bool IsArcadeWorld(const UWorld* World)
	{
		return World && World->GetAuthGameMode() && World->GetAuthGameMode()->IsA<ADCCArcadeGameMode>();
	}

	bool IsFrontDoor(const UWorld* World)
	{
		return World && World->GetAuthGameMode() && World->GetAuthGameMode()->IsA<ADCCFrontDoorGameMode>();
	}

	bool IsBenchTool(const UWorld* World)
	{
		return World && World->GetAuthGameMode() && World->GetAuthGameMode()->IsA<ADCCBenchGameMode>();
	}

	void MaybeStartInLobby(UWorld* World)
	{
		// The packaged game starts at the front door already (GameDefaultMap +
		// LocalMapOptions in DefaultEngine.ini). This catches a launch on any
		// other level, which still goes through character select first.
		static bool bDone = false;
		if (bDone || !World || World->WorldType != EWorldType::Game || World->GetNetMode() != NM_Standalone
			|| IsArcadeWorld(World) || FParse::Param(FCommandLine::Get(), TEXT("nolobby")))
		{
			return;
		}
		bDone = true;
		Travel(World, FName(FrontDoorId));
	}

	bool Travel(UWorld* World, FName GameId)
	{
		if (!World || World->GetNetMode() == NM_Client)
		{
			return false;
		}

		FString Map = TEXT("/Engine/Maps/Entry");
		FString Mode = TEXT("/Script/DungeonCrawlerDenver.DCCLobbyGameMode");
		if (GameId == FName(FrontDoorId))
		{
			Mode = TEXT("/Script/DungeonCrawlerDenver.DCCFrontDoorGameMode");
		}
		else if (const FGame* Game = Find(GameId))
		{
			Map = Game->Map;
			Mode = Game->GameMode;
		}

		// Everyone's progress is on disk before the level goes away.
		DCCSave::SaveAll(World, TEXT("Travelling"));

		const FString Url = FString::Printf(TEXT("%s?game=%s%s"), *Map, *Mode,
			World->GetNetMode() == NM_ListenServer ? TEXT("?listen") : TEXT(""));
		UE_LOG(LogTemp, Warning, TEXT("[DCC Arcade] Travelling to %s"), *Url);
		return World->ServerTravel(Url, /*bAbsolute=*/true);
	}
}

static void DCCArcadeCommand(const TArray<FString>& Args, UWorld* World)
{
	DCCArcade::Travel(World, Args.IsEmpty() ? FName(TEXT("Lobby")) : FName(*Args[0]));
}

static FAutoConsoleCommandWithWorldAndArgs GArcadeCmd(
	TEXT("DCC.Arcade"),
	TEXT("DCC.Arcade [Crawler|Haunt|Climb|Kart|Neon|Audit|Gate]: take everyone to the arcade lobby, or straight into a game."),
	FConsoleCommandWithWorldAndArgsDelegate::CreateStatic(&DCCArcadeCommand));
