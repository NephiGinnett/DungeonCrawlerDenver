// Dev-only console commands for checking screens from a script (2026-11-14). Players never need these: every
// one of these things has a button or happens by playing. Each takes an optional trailing delay in seconds, so a
// command line (-ExecCmds) can fire them after the level has loaded.
//   DCC.Counter <0|1|2> [delay]     open the Den counter on PRIZES / TROPHIES / CHOP SHOP
//   DCC.SkillTick <Skill> [delay]   one practice tick on a skill of the local crawler (grants it if unknown)
//   DCC.Scrap <amount> [delay]      give the local player scrap

#include "CoreMinimal.h"
#include "HAL/IConsoleManager.h"
#include "Engine/World.h"
#include "TimerManager.h"
#include "GameFramework/PlayerController.h"
#include "Characters/DCCCharacterBase.h"
#include "Skills/DCCSkillComponent.h"
#include "Arcade/DCCWalletComponent.h"
#include "Vehicles/DCCRover.h"
#include "UI/DCCHUD.h"
#include "Arcade/DCCArcadeArena.h"
#include "Arcade/DCCArcadeGate.h"
#include "Arcade/DCCArcadeHaunt.h"
#include "Camera/CameraActor.h"
#include "EngineUtils.h"

namespace
{
	void After(UWorld* World, float Delay, TFunction<void(UWorld*)> Run)
	{
		if (!World)
		{
			return;
		}
		if (Delay <= 0.0f)
		{
			Run(World);
			return;
		}
		FTimerHandle Handle;
		TWeakObjectPtr<UWorld> Weak(World);
		World->GetTimerManager().SetTimer(Handle, FTimerDelegate::CreateLambda([Weak, Run]() { if (Weak.IsValid()) { Run(Weak.Get()); } }), Delay, false);
	}

	ADCCCharacterBase* LocalCrawler(UWorld* World)
	{
		APlayerController* PC = World->GetFirstPlayerController();
		APawn* Pawn = PC ? PC->GetPawn() : nullptr;
		if (ADCCRover* Rover = Cast<ADCCRover>(Pawn))
		{
			return Rover->Driver;
		}
		return Cast<ADCCCharacterBase>(Pawn);
	}
}

static FAutoConsoleCommandWithWorldAndArgs GDevCounter(TEXT("DCC.Counter"), TEXT("DCC.Counter <tab 0-2> [delay]: (dev) open the Den counter."),
	FConsoleCommandWithWorldAndArgsDelegate::CreateLambda([](const TArray<FString>& Args, UWorld* World)
	{
		const int32 Tab = Args.Num() > 0 ? FCString::Atoi(*Args[0]) : 0;
		After(World, Args.Num() > 1 ? FCString::Atof(*Args[1]) : 0.0f, [Tab](UWorld* W)
		{
			APlayerController* PC = W->GetFirstPlayerController();
			if (ADCCHUD* Hud = PC ? Cast<ADCCHUD>(PC->GetHUD()) : nullptr)
			{
				Hud->CloseCosmeticShop();
				Hud->OpenCosmeticShop(Tab);
			}
		});
	}));

static FAutoConsoleCommandWithWorldAndArgs GDevSkillTick(TEXT("DCC.SkillTick"), TEXT("DCC.SkillTick <Skill> [delay]: (dev) one practice tick."),
	FConsoleCommandWithWorldAndArgsDelegate::CreateLambda([](const TArray<FString>& Args, UWorld* World)
	{
		if (Args.Num() < 1)
		{
			return;
		}
		const FName Skill(*Args[0]);
		After(World, Args.Num() > 1 ? FCString::Atof(*Args[1]) : 0.0f, [Skill](UWorld* W)
		{
			if (ADCCCharacterBase* Crawler = LocalCrawler(W))
			{
				if (Crawler->Skills)
				{
					if (Crawler->Skills->GetSkillRank(Skill) <= 0)
					{
						Crawler->Skills->GrantSkill(Skill, 1);
					}
					Crawler->Skills->MarkSkillUsed(Skill);
				}
			}
		});
	}));

static FAutoConsoleCommandWithWorldAndArgs GDevScrap(TEXT("DCC.Scrap"), TEXT("DCC.Scrap <amount> [delay]: (dev) give the local player scrap."),
	FConsoleCommandWithWorldAndArgsDelegate::CreateLambda([](const TArray<FString>& Args, UWorld* World)
	{
		const int32 Amount = Args.Num() > 0 ? FCString::Atoi(*Args[0]) : 100;
		After(World, Args.Num() > 1 ? FCString::Atof(*Args[1]) : 0.0f, [Amount](UWorld* W)
		{
			APlayerController* PC = W->GetFirstPlayerController();
			if (PC && PC->PlayerState)
			{
				UDCCWalletComponent::AwardScrap(PC->PlayerState, Amount, TEXT("dev"));
			}
		});
	}));

// A still camera for checking a place by eye (screenshots) and for trailer framing. Coordinates are relative to the
// running arcade arena's origin, so "DCC.Peek 12000 2600 300 90 -5 30" is the same spot in any Repo Rally round.
static FAutoConsoleCommandWithWorldAndArgs GDevPeek(TEXT("DCC.Peek"),
	TEXT("DCC.Peek <x> <y> <z> <yaw> [pitch] [delay]: (dev) look from a point relative to the arena. DCC.Peek off: back to the player."),
	FConsoleCommandWithWorldAndArgsDelegate::CreateLambda([](const TArray<FString>& Args, UWorld* World)
	{
		const bool bOff = Args.Num() > 0 && Args[0].Equals(TEXT("off"), ESearchCase::IgnoreCase);
		const FVector Rel = Args.Num() > 2 ? FVector(FCString::Atof(*Args[0]), FCString::Atof(*Args[1]), FCString::Atof(*Args[2])) : FVector::ZeroVector;
		const FRotator Rot(Args.Num() > 4 ? FCString::Atof(*Args[4]) : 0.0f, Args.Num() > 3 ? FCString::Atof(*Args[3]) : 0.0f, 0.0f);
		After(World, Args.Num() > 5 ? FCString::Atof(*Args[5]) : 0.0f, [bOff, Rel, Rot](UWorld* W)
		{
			APlayerController* PC = W->GetFirstPlayerController();
			if (!PC)
			{
				return;
			}
			static TWeakObjectPtr<ACameraActor> Cam;
			if (bOff)
			{
				PC->SetViewTargetWithBlend(PC->GetPawn(), 0.0f);
				return;
			}
			FVector Origin = FVector::ZeroVector;
			for (TActorIterator<ADCCArcadeArena> It(W); It; ++It)
			{
				Origin = It->GetActorLocation();
				break;
			}
			if (!Cam.IsValid())
			{
				Cam = W->SpawnActor<ACameraActor>(ACameraActor::StaticClass(), FTransform(Rot, Origin + Rel));
			}
			if (Cam.IsValid())
			{
				Cam->SetActorLocationAndRotation(Origin + Rel, Rot);
				PC->SetViewTargetWithBlend(Cam.Get(), 0.0f);
			}
		});
	}));

static FAutoConsoleCommandWithWorldAndArgs GDevGateIn(TEXT("DCC.GateIn"), TEXT("DCC.GateIn [room] [delay]: (dev) in Gate Run, open the gate and send everyone in."),
	FConsoleCommandWithWorldAndArgsDelegate::CreateLambda([](const TArray<FString>& Args, UWorld* World)
	{
		const int32 Room = Args.Num() > 0 ? FCString::Atoi(*Args[0]) : 0;
		After(World, Args.Num() > 1 ? FCString::Atof(*Args[1]) : 0.0f, [Room](UWorld* W)
		{
			for (TActorIterator<ADCCArcadeGate> It(W); It; ++It)
			{
				It->DebugEnterAll(Room);
			}
		});
	}));

static FAutoConsoleCommandWithWorldAndArgs GDevGateClear(TEXT("DCC.GateClear"), TEXT("DCC.GateClear <room|-1> [delay]: (dev) in Gate Run, knock out a room's foes and open its vault."),
	FConsoleCommandWithWorldAndArgsDelegate::CreateLambda([](const TArray<FString>& Args, UWorld* World)
	{
		const int32 Room = Args.Num() > 0 && !Args[0].Equals(TEXT("all"), ESearchCase::IgnoreCase) ? FCString::Atoi(*Args[0]) : -1;
		After(World, Args.Num() > 1 ? FCString::Atof(*Args[1]) : 0.0f, [Room](UWorld* W)
		{
			for (TActorIterator<ADCCArcadeGate> It(W); It; ++It)
			{
				It->DebugClearRoom(Room);
			}
		});
	}));

static FAutoConsoleCommandWithWorldAndArgs GDevMineGo(TEXT("DCC.MineGo"), TEXT("DCC.MineGo <room|driftN> [delay]: (dev) in Dead Air, put everyone in a stope (3) or a tunnel (drift2)."),
	FConsoleCommandWithWorldAndArgsDelegate::CreateLambda([](const TArray<FString>& Args, UWorld* World)
	{
		const FString Where = Args.Num() > 0 ? Args[0] : FString(TEXT("1"));
		const bool bDrift = Where.StartsWith(TEXT("drift"));
		static const TMap<FString, int32> Traps = { { TEXT("plank"), -4 }, { TEXT("cart"), -5 }, { TEXT("crate"), -6 }, { TEXT("gas"), -7 }, { TEXT("ledge"), -8 }, { TEXT("badair"), -9 }, { TEXT("stick"), -10 }, { TEXT("end"), -11 } };
		const int32* Trap = Traps.Find(Where.ToLower());
		const int32 Index = Trap ? *Trap : Where.Equals(TEXT("ghost"), ESearchCase::IgnoreCase) ? -1 : Where.Equals(TEXT("body"), ESearchCase::IgnoreCase) ? -2 : Where.Equals(TEXT("readall"), ESearchCase::IgnoreCase) ? -3
			: FCString::Atoi(bDrift ? *Where.Mid(5) : *Where);
		After(World, Args.Num() > 1 ? FCString::Atof(*Args[1]) : 0.0f, [Index, bDrift](UWorld* W)
		{
			for (TActorIterator<ADCCArcadeHaunt> It(W); It; ++It)
			{
				It->DebugGoTo(Index, bDrift);
			}
		});
	}));

static FAutoConsoleCommandWithWorldAndArgs GDevCredits(TEXT("DCC.Credits"), TEXT("DCC.Credits <amount> [delay]: (dev) give the local player Mine Outfitters credits."),
	FConsoleCommandWithWorldAndArgsDelegate::CreateLambda([](const TArray<FString>& Args, UWorld* World)
	{
		const int32 Amount = Args.Num() > 0 ? FCString::Atoi(*Args[0]) : 100;
		After(World, Args.Num() > 1 ? FCString::Atof(*Args[1]) : 0.0f, [Amount](UWorld* W)
		{
			APlayerController* PC = W->GetFirstPlayerController();
			if (PC && PC->PlayerState)
			{
				UDCCWalletComponent::AwardCredits(PC->PlayerState, Amount, TEXT("dev"));
			}
		});
	}));

// Put the local player somewhere, relative to the running arcade arena's origin (2026-11-15: testing the Trading Post
// pad and Gate Run rooms without walking there).
static FAutoConsoleCommandWithWorldAndArgs GDevTp(TEXT("DCC.Tp"),
	TEXT("DCC.Tp <x> <y> <z> [yaw] [delay]: (dev) move the local player to a point relative to the arena."),
	FConsoleCommandWithWorldAndArgsDelegate::CreateLambda([](const TArray<FString>& Args, UWorld* World)
	{
		if (Args.Num() < 3)
		{
			return;
		}
		const FVector Rel(FCString::Atof(*Args[0]), FCString::Atof(*Args[1]), FCString::Atof(*Args[2]));
		const float Yaw = Args.Num() > 3 ? FCString::Atof(*Args[3]) : 0.0f;
		After(World, Args.Num() > 4 ? FCString::Atof(*Args[4]) : 0.0f, [Rel, Yaw](UWorld* W)
		{
			APlayerController* PC = W->GetFirstPlayerController();
			APawn* Pawn = PC ? PC->GetPawn() : nullptr;
			if (!Pawn)
			{
				return;
			}
			FVector Origin = FVector::ZeroVector;
			for (TActorIterator<ADCCArcadeArena> It(W); It; ++It)
			{
				Origin = It->GetActorLocation();
				break;
			}
			Pawn->TeleportTo(Origin + Rel, FRotator(0.0f, Yaw, 0.0f));
			PC->SetControlRotation(FRotator(-15.0f, Yaw, 0.0f));
		});
	}));
