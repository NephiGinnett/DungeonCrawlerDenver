#include "Arcade/DCCArcade.h"

#include "Session/DCCSaveSystem.h"
#include "Session/DCCProfile.h"
#include "Arcade/DCCArcadeGameModes.h"
#include "Arcade/DCCWalletComponent.h"
#include "Characters/DCCCharacterBase.h"
#include "Stats/DCCStatsComponent.h"
#include "Misc/CommandLine.h"
#include "Misc/Parse.h"
#include "Engine/World.h"
#include "GameFramework/GameStateBase.h"
#include "GameFramework/Pawn.h"
#include "GameFramework/PlayerState.h"
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

	static bool IsZoe(const APlayerState* Player)
	{
		return Player && Player->GetPlayerName().Equals(TEXT("Zoe"), ESearchCase::IgnoreCase);
	}

	bool IsSeriesOpen(const APlayerState* Player, FName GameId)
	{
		if (GameId == TEXT("Crawler") || GameId == TEXT("Neon") || GameId == TEXT("Climb") || GameId.IsNone())
		{
			return true;
		}
		if (IsZoe(Player))
		{
			return true;
		}
		const FString Key = DCCProfile::KeyFor(Player);
		const bool bSummit = DCCProfile::GetForKey(Key, TEXT("Climb"), TEXT("Summits")) >= 1.0f;
		if (GameId == TEXT("Haunt") || GameId == TEXT("Audit"))
		{
			return bSummit;
		}
		const bool bFilmed = bSummit && DCCProfile::GetForKey(Key, TEXT("Haunt"), TEXT("BestViews")) > 0.0f;
		const bool bHit = bFilmed && DCCProfile::GetForKey(Key, TEXT("Audit"), TEXT("BestDamage")) > 0.0f;
		if (GameId == TEXT("Gate") || GameId == TEXT("Kart"))
		{
			return bHit;
		}
		return true;
	}

	FString SeriesLockLine(const APlayerState* Player, FName GameId)
	{
		if (IsSeriesOpen(Player, GameId))
		{
			return FString();
		}
		const FString Key = DCCProfile::KeyFor(Player);
		const bool bSummit = DCCProfile::GetForKey(Key, TEXT("Climb"), TEXT("Summits")) >= 1.0f;
		if (!bSummit || GameId == TEXT("Haunt") || GameId == TEXT("Audit"))
		{
			return TEXT("Locked. Reach the Fourteener summit to open this.");
		}
		const bool bFilmed = DCCProfile::GetForKey(Key, TEXT("Haunt"), TEXT("BestViews")) > 0.0f;
		const bool bHit = DCCProfile::GetForKey(Key, TEXT("Audit"), TEXT("BestDamage")) > 0.0f;
		if (!bFilmed && !bHit)
		{
			return TEXT("Locked. Finish Dead Air and Stress Test to open this.");
		}
		if (!bFilmed)
		{
			return TEXT("Locked. Film Dead Air and bring the tape up.");
		}
		return TEXT("Locked. Land your hits in Stress Test.");
	}

	FString SeriesQuestLine(FName GameId)
	{
		if (GameId == TEXT("Climb")) { return TEXT("QUEST  Reach the summit."); }
		if (GameId == TEXT("Haunt")) { return TEXT("QUEST  Film the dark and bring the tape up."); }
		if (GameId == TEXT("Audit")) { return TEXT("QUEST  Land your hits before the clock runs out."); }
		if (GameId == TEXT("Gate")) { return TEXT("QUEST  Clear the rooms and drop the Gatekeeper."); }
		if (GameId == TEXT("Kart")) { return TEXT("QUEST  Tow 10 cars to the yard."); }
		if (GameId == TEXT("Neon")) { return TEXT("QUEST  Drive 500 metres."); }
		if (GameId == TEXT("Crawler")) { return TEXT("QUEST  Clear a floor."); }
		return FString();
	}

	namespace
	{
		float Stat(const APlayerState* Player, const TCHAR* Game, const TCHAR* Name)
		{
			return Player ? DCCProfile::Get(Player, FName(Game), FName(Name)) : 0.0f;
		}

		void Step(TArray<FBoardStep>& Out, const TCHAR* Line, const TCHAR* Now, const TCHAR* Then)
		{
			FBoardStep& Added = Out.AddDefaulted_GetRef();
			Added.Line = Line;
			Added.Now = Now;
			Added.Then = Then ? Then : TEXT("");
		}

		// The current step, or false when that line is finished.
		bool Ascent(const APlayerState* Player, TArray<FBoardStep>& Out)
		{
			if (Stat(Player, TEXT("Climb"), TEXT("Summits")) >= 1.0f)
			{
				return false;
			}
			const float Height = Stat(Player, TEXT("Climb"), TEXT("BestHeight"));
			if (Height < 9100.0f)
			{
				Step(Out, TEXT("THE ASCENT"), TEXT("Walk out of Fort Tiner and start up the trail."), TEXT("High Camp, then the summit."));
			}
			else if (Height < 11500.0f)
			{
				Step(Out, TEXT("THE ASCENT"), TEXT("Reach High Camp. The board reads about 11,560."), TEXT("Then stand on the summit."));
			}
			else
			{
				Step(Out, TEXT("THE ASCENT"), TEXT("Stand on the summit."), TEXT("The mine opens after that."));
			}
			return true;
		}

		bool Tape(const APlayerState* Player, TArray<FBoardStep>& Out)
		{
			if (Stat(Player, TEXT("Haunt"), TEXT("BestViews")) > 0.0f)
			{
				return false;
			}
			if (Stat(Player, TEXT("Haunt"), TEXT("Plays")) < 1.0f)
			{
				Step(Out, TEXT("THE TAPE"), TEXT("Clock in at the Lucky Tuesday and take a shift."), TEXT("Bring the footage back to the cage."));
			}
			else
			{
				Step(Out, TEXT("THE TAPE"), TEXT("Get back to the cage alive. A wipe uploads nothing."), nullptr);
			}
			return true;
		}

		bool Audit(const APlayerState* Player, TArray<FBoardStep>& Out)
		{
			if (Stat(Player, TEXT("Audit"), TEXT("BestDamage")) > 0.0f)
			{
				return false;
			}
			if (Stat(Player, TEXT("Audit"), TEXT("Runs")) < 1.0f)
			{
				Step(Out, TEXT("THE AUDIT"), TEXT("Step onto The Quarterly in Stress Test."), TEXT("Land a hit. Fists count."));
			}
			else
			{
				Step(Out, TEXT("THE AUDIT"), TEXT("Land a hit before the clock runs out."), nullptr);
			}
			return true;
		}

		bool Gate(const APlayerState* Player, TArray<FBoardStep>& Out)
		{
			if (Stat(Player, TEXT("Quest"), TEXT("Gate")) >= 1.0f || Stat(Player, TEXT("Gate"), TEXT("Clears")) >= 1.0f)
			{
				return false;
			}
			if (Stat(Player, TEXT("Gate"), TEXT("Runs")) < 1.0f)
			{
				Step(Out, TEXT("THE GATE"), TEXT("Walk a gate and clear the rooms you can."), TEXT("Then drop the Gatekeeper."));
			}
			else
			{
				Step(Out, TEXT("THE GATE"), TEXT("Drop the Gatekeeper and take the way home."), nullptr);
			}
			return true;
		}

		bool Yard(const APlayerState* Player, TArray<FBoardStep>& Out)
		{
			if (Stat(Player, TEXT("Quest"), TEXT("Kart")) >= 1.0f)
			{
				return false;
			}
			if (Stat(Player, TEXT("Kart"), TEXT("Nabs")) < 1.0f)
			{
				Step(Out, TEXT("THE YARD"), TEXT("Hook one marked car and drop it in the yard."), TEXT("Then ten cars in one shift."));
			}
			else
			{
				Step(Out, TEXT("THE YARD"), TEXT("Tow 10 cars in one shift."), nullptr);
			}
			return true;
		}

		bool Strip(const APlayerState* Player, TArray<FBoardStep>& Out)
		{
			if (Stat(Player, TEXT("Quest"), TEXT("Neon")) >= 1.0f)
			{
				return false;
			}
			if (Stat(Player, TEXT("Neon"), TEXT("Runs")) < 1.0f && Stat(Player, TEXT("Neon"), TEXT("BestDistance")) < 50.0f)
			{
				Step(Out, TEXT("THE STRIP"), TEXT("Take the Neon Mile. Just drive."), TEXT("Then hold it for 500 metres."));
			}
			else
			{
				Step(Out, TEXT("THE STRIP"), TEXT("Hold the lane for 500 metres."), nullptr);
			}
			return true;
		}

		bool Run(const APlayerState* Player, TArray<FBoardStep>& Out)
		{
			if (Stat(Player, TEXT("Quest"), TEXT("Crawler")) >= 1.0f)
			{
				return false;
			}
			if (Stat(Player, TEXT("Crawler"), TEXT("Runs")) < 1.0f)
			{
				Step(Out, TEXT("THE RUN"), TEXT("Take Departures and go down one floor."), TEXT("Then clear a floor."));
			}
			else
			{
				Step(Out, TEXT("THE RUN"), TEXT("Clear a floor and take the stairs."), nullptr);
			}
			return true;
		}
	}

	void BoardSteps(const APlayerState* Player, FName GameId, TArray<FBoardStep>& Out)
	{
		Out.Reset();
		const bool bSummit = Stat(Player, TEXT("Climb"), TEXT("Summits")) >= 1.0f;
		const bool bFilmed = Stat(Player, TEXT("Haunt"), TEXT("BestViews")) > 0.0f;
		const bool bHit = Stat(Player, TEXT("Audit"), TEXT("BestDamage")) > 0.0f;

		if (GameId == TEXT("Climb"))
		{
			if (!Ascent(Player, Out) && (!bFilmed || !bHit))
			{
				Step(Out, TEXT("THE ASCENT"), TEXT("Summit logged."), bFilmed ? TEXT("Stress Test is the other door.") : TEXT("Dead Air is open. Take a shift."));
			}
			return;
		}
		if (GameId == TEXT("Haunt"))
		{
			if (!Tape(Player, Out) && !bHit)
			{
				Step(Out, TEXT("THE TAPE"), TEXT("The tape is up."), TEXT("Stress Test opens the gates and the yard."));
			}
			return;
		}
		if (GameId == TEXT("Audit"))
		{
			if (!Audit(Player, Out) && !bFilmed)
			{
				Step(Out, TEXT("THE AUDIT"), TEXT("They have your number."), TEXT("The tape still has to come up."));
			}
			return;
		}
		if (GameId == TEXT("Gate")) { Gate(Player, Out); return; }
		if (GameId == TEXT("Kart")) { Yard(Player, Out); return; }
		if (GameId == TEXT("Neon")) { Strip(Player, Out); return; }
		if (GameId == TEXT("Crawler")) { Run(Player, Out); return; }

		// The Den, the front door, anywhere that is not already inside a game.
		if (Ascent(Player, Out))
		{
			// A short one beside the mountain, so the first session has more than one door.
			if (Stat(Player, TEXT("Neon"), TEXT("Runs")) < 1.0f && Stat(Player, TEXT("Neon"), TEXT("BestDistance")) < 50.0f)
			{
				Step(Out, TEXT("THE STRIP"), TEXT("Take the Neon Mile. Just drive."), nullptr);
			}
			if (Stat(Player, TEXT("Crawler"), TEXT("Runs")) < 1.0f)
			{
				Step(Out, TEXT("THE RUN"), TEXT("Take Departures and go down one floor."), nullptr);
			}
			return;
		}
		const bool bTape = Tape(Player, Out);
		const bool bAudit = Audit(Player, Out);
		if (bTape || bAudit)
		{
			return;
		}
		Gate(Player, Out);
		Yard(Player, Out);
	}

	int32 SeriesQuestXp()
	{
		// 250 since the level cap went to 20 (2026-11-22): at 100 a first clear was a fifteenth of a level.
		return 250;
	}

	bool CompleteSeriesQuest(APlayerState* Player, FName GameId, int32 Xp)
	{
		if (!Player || GameId.IsNone() || Xp <= 0)
		{
			return false;
		}
		const FString Key = DCCProfile::KeyFor(Player);
		if (Key.IsEmpty() || DCCProfile::GetForKey(Key, TEXT("Quest"), GameId) >= 1.0f)
		{
			return false;
		}
		DCCProfile::SetForKey(Key, TEXT("Quest"), GameId, 1.0f);
		ADCCCharacterBase* Crawler = Cast<ADCCCharacterBase>(Player->GetPawn());
		if (Crawler && Crawler->Stats)
		{
			Crawler->Stats->AddExperience(Xp);
		}
		if (Crawler && Crawler->Wallet)
		{
			Crawler->Wallet->Tell(FString::Printf(TEXT("QUEST DONE  +%d XP"), Xp));
		}
		UE_LOG(LogTemp, Log, TEXT("[DCC Quest] %s finished %s  +%d XP"), *Key, *GameId.ToString(), Xp);
		return true;
	}

	bool IsArcadeWorld(const UWorld* World)
	{
		return World && World->GetAuthGameMode() && World->GetAuthGameMode()->IsA<ADCCArcadeGameMode>();
	}

	bool CanLeaveForLobby(const UWorld* World)
	{
		const AGameStateBase* GameState = World ? World->GetGameState() : nullptr;
		const UClass* Mode = GameState ? GameState->GameModeClass.Get() : nullptr;
		if (!Mode)
		{
			return false;
		}
		const FString Name = Mode->GetName();
		return !Name.Contains(TEXT("Lobby")) && !Name.Contains(TEXT("FrontDoor")) && !Name.Contains(TEXT("Bench"));
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
