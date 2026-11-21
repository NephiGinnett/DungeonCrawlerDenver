// Dev-only console commands for checking screens from a script (2026-11-14). Players never need these: every
// one of these things has a button or happens by playing. Each takes an optional trailing delay in seconds, so a
// command line (-ExecCmds) can fire them after the level has loaded.
//   DCC.Counter <0|1|2> [delay]     open the Den counter on PRIZES / TROPHIES / CHOP SHOP
//   DCC.SkillTick <Skill> [delay]   one practice tick on a skill of the local crawler (grants it if unknown)
//   DCC.Scrap <amount> [delay]      give the local player scrap

#include "Arcade/DCCSpray.h"
#include "Session/DCCProfile.h"
#include "Arcade/DCCWalletComponent.h"
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
#include "Arcade/DCCArcade.h"
#include "Arcade/DCCArcadeGames.h"
#include "Arcade/DCCCasino.h"
#include "Arcade/DCCArcadeGate.h"
#include "Arcade/DCCArcadeHaunt.h"
#include "Items/DCCPickupVariants.h"
#include "Camera/CameraActor.h"
#include "EngineUtils.h"
#include "Characters/DCCEnemyCharacter.h"
#include "Equipment/DCCEquipmentComponent.h"
#include "Progression/DCCAchievementComponent.h"
#include "Characters/DCCAttackStyle.h"
#include "UI/DCCChatTypes.h"

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

// Spray tags without the picture dialog, for testing (2026-11-16): DCC.Spray <picture> <yaw> <pitch> [delay]. Grants the
// can on this (host) profile, uses the picture, turns the view, and sprays.
static FAutoConsoleCommandWithWorldAndArgs GDevSpray(TEXT("DCC.Spray"), TEXT("DCC.Spray <picture path> <yaw> <pitch> [delay]: (dev) spray a tag from a file."),
	FConsoleCommandWithWorldAndArgsDelegate::CreateLambda([](const TArray<FString>& Args, UWorld* World)
	{
		if (Args.Num() < 3)
		{
			return;
		}
		const FString Path = Args[0];
		const float Yaw = FCString::Atof(*Args[1]);
		const float Pitch = FCString::Atof(*Args[2]);
		After(World, Args.Num() > 3 ? FCString::Atof(*Args[3]) : 0.0f, [Path, Yaw, Pitch](UWorld* W)
		{
			APlayerController* PC = W ? W->GetFirstPlayerController() : nullptr;
			if (!PC || !PC->PlayerState)
			{
				return;
			}
			const FString Key = DCCProfile::KeyFor(PC->PlayerState);
			DCCProfile::GrantCosmetic(Key, DCCSpray::CanId);
			if (ADCCCharacterBase* Crawler = Cast<ADCCCharacterBase>(PC->GetPawn()))
			{
				if (Crawler->Wallet) { Crawler->Wallet->Owned = DCCProfile::GetCosmetics(Key); }
			}
			FString Error;
			if (!DCCSpray::UsePicture(Key, Path, Error))
			{
				UE_LOG(LogTemp, Warning, TEXT("[DCC Spray] %s"), *Error);
				return;
			}
			PC->SetControlRotation(FRotator(Pitch, Yaw, 0.0f));
			After(W, 0.5f, [PC](UWorld*) { DCCSpray::TryLocalSpray(PC); UE_LOG(LogTemp, Log, TEXT("[DCC Spray] sprayed (dev)")); });
		});
	}));

// Reactions and the belt grapple without the keys, for testing (2026-11-16).
//   DCC.React <1-8> [delay]   the local crawler reacts (1 cheer, 2 alert, 3 ?, 4 heart, 5 sweat, 6 skull, 7 note, 8 cold)
//   DCC.Grapple [delay]       throw the belt grappling hook (Fourteener, while climbing)
static FAutoConsoleCommandWithWorldAndArgs GDevReact(TEXT("DCC.React"), TEXT("DCC.React <1-8> [delay]: (dev) the local crawler reacts."),
	FConsoleCommandWithWorldAndArgsDelegate::CreateLambda([](const TArray<FString>& Args, UWorld* World)
	{
		const int32 Reaction = Args.Num() > 0 ? FCString::Atoi(*Args[0]) : 1;
		After(World, Args.Num() > 1 ? FCString::Atof(*Args[1]) : 0.0f, [Reaction](UWorld* W)
		{
			if (ADCCCharacterBase* Crawler = LocalCrawler(W)) { Crawler->React(uint8(Reaction), false); }
		});
	}));

static FAutoConsoleCommandWithWorldAndArgs GDevGrapple(TEXT("DCC.Grapple"), TEXT("DCC.Grapple [delay]: (dev) throw the belt grappling hook."),
	FConsoleCommandWithWorldAndArgsDelegate::CreateLambda([](const TArray<FString>& Args, UWorld* World)
	{
		After(World, Args.Num() > 0 ? FCString::Atof(*Args[0]) : 0.0f, [](UWorld* W)
		{
			if (ADCCCharacterBase* Crawler = LocalCrawler(W))
			{
				const float Before = Crawler->GetActorLocation().Z;
				Crawler->ThrowGrapple();
				UE_LOG(LogTemp, Log, TEXT("[DCC Grapple] thrown from z=%.0f"), Before);
			}
		});
	}));

// DCC.MenuTab <index> [delay]: (dev) open the Tab menu on a page (10 is SETTINGS).
static FAutoConsoleCommandWithWorldAndArgs GDevMenuTab(TEXT("DCC.MenuTab"), TEXT("DCC.MenuTab <index> [delay]: (dev) open the Tab menu on a page."),
	FConsoleCommandWithWorldAndArgsDelegate::CreateLambda([](const TArray<FString>& Args, UWorld* World)
	{
		const int32 Tab = Args.Num() > 0 ? FCString::Atoi(*Args[0]) : 0;
		After(World, Args.Num() > 1 ? FCString::Atof(*Args[1]) : 0.0f, [Tab](UWorld* W)
		{
			APlayerController* PC = W ? W->GetFirstPlayerController() : nullptr;
			if (ADCCHUD* Hud = PC ? Cast<ADCCHUD>(PC->GetHUD()) : nullptr)
			{
				Hud->OpenMenuTab(static_cast<EDCCMenuTab>(FMath::Clamp(Tab, 0, int32(EDCCMenuTab::MAX) - 1)));
			}
		});
	}));

// DCC.Belt <slot 1-9> [delay]: (dev) press a belt number key. DCC.Click [delay]: (dev) a left click.
// DCC.Emotes [delay]: (dev) open the B menu. For checking the belt (2026-11-16) in automated runs.
static FAutoConsoleCommandWithWorldAndArgs GDevBelt(TEXT("DCC.Belt"), TEXT("DCC.Belt <1-9> [delay]: (dev) press a belt number key."),
	FConsoleCommandWithWorldAndArgsDelegate::CreateLambda([](const TArray<FString>& Args, UWorld* World)
	{
		const int32 Slot = Args.Num() > 0 ? FCString::Atoi(*Args[0]) : 1;
		After(World, Args.Num() > 1 ? FCString::Atof(*Args[1]) : 0.0f, [Slot](UWorld* W)
		{
			if (ADCCCharacterBase* Crawler = LocalCrawler(W)) { Crawler->BeltKey(Slot - 1); }
		});
	}));

static FAutoConsoleCommandWithWorldAndArgs GDevWheel(TEXT("DCC.Wheel"), TEXT("DCC.Wheel <1|-1> [delay]: (dev) scroll the wheel. 1 is wheel-down."),
	FConsoleCommandWithWorldAndArgsDelegate::CreateLambda([](const TArray<FString>& Args, UWorld* World)
	{
		const int32 Delta = Args.Num() > 0 ? FCString::Atoi(*Args[0]) : 1;
		After(World, Args.Num() > 1 ? FCString::Atof(*Args[1]) : 0.0f, [Delta](UWorld* W)
		{
			if (ADCCCharacterBase* Crawler = LocalCrawler(W)) { Crawler->DevWheel(Delta); }
		});
	}));

static FAutoConsoleCommandWithWorldAndArgs GDevClick(TEXT("DCC.Click"), TEXT("DCC.Click [delay]: (dev) a left click."),
	FConsoleCommandWithWorldAndArgsDelegate::CreateLambda([](const TArray<FString>& Args, UWorld* World)
	{
		After(World, Args.Num() > 0 ? FCString::Atof(*Args[0]) : 0.0f, [](UWorld* W)
		{
			if (ADCCCharacterBase* Crawler = LocalCrawler(W)) { Crawler->DevClick(); }
		});
	}));

static FAutoConsoleCommandWithWorldAndArgs GDevOutfit(TEXT("DCC.Outfit"), TEXT("DCC.Outfit [delay]: (dev) in Fourteener, give one of every tool and boost."),
	FConsoleCommandWithWorldAndArgsDelegate::CreateLambda([](const TArray<FString>& Args, UWorld* World)
	{
		After(World, Args.Num() > 0 ? FCString::Atof(*Args[0]) : 0.0f, [](UWorld* W)
		{
			ADCCCharacterBase* Crawler = LocalCrawler(W);
			ADCCArcadeClimb* Climb = nullptr;
			for (TActorIterator<ADCCArcadeClimb> It(W); It; ++It) { Climb = *It; break; }
			if (!Crawler || !Climb)
			{
				UE_LOG(LogTemp, Warning, TEXT("[DCC Outfit] Fourteener is not running."));
				return;
			}
			Climb->GiveFullKit(Crawler);
			TArray<FDCCBeltEntry> Belt;
			Crawler->GetBelt(Belt);
			for (int32 i = 0; i < Belt.Num(); ++i)
			{
				UE_LOG(LogTemp, Log, TEXT("[DCC Outfit] slot %d: %s x%d"), i + 1, *Belt[i].Name, Belt[i].Count);
			}
		});
	}));

static FAutoConsoleCommandWithWorldAndArgs GDevEsc(TEXT("DCC.Esc"), TEXT("DCC.Esc [delay]: (dev) press Esc for the local crawler."),
	FConsoleCommandWithWorldAndArgsDelegate::CreateLambda([](const TArray<FString>& Args, UWorld* World)
	{
		After(World, Args.Num() > 0 ? FCString::Atof(*Args[0]) : 0.0f, [](UWorld* W)
		{
			ADCCCharacterBase* Crawler = LocalCrawler(W);
			if (!Crawler)
			{
				return;
			}
			APlayerController* PC = Cast<APlayerController>(Crawler->GetController());
			ADCCHUD* Hud = PC ? Cast<ADCCHUD>(PC->GetHUD()) : nullptr;
			const ADCCCasinoCascade* Casino = ADCCCasinoCascade::Get(W);
			const int32 MenuBefore = Hud && Hud->IsMenuVisible() ? 1 : 0;
			const int32 SideBefore = Crawler->HasSideMenu() ? 1 : 0;
			const int32 SeatBefore = Casino && Casino->IsSeated(Crawler) ? 1 : 0;
			Crawler->CloseInteraction();
			const int32 MenuAfter = Hud && Hud->IsMenuVisible() ? 1 : 0;
			const int32 SideAfter = Crawler->HasSideMenu() ? 1 : 0;
			const int32 SeatAfter = Casino && Casino->IsSeated(Crawler) ? 1 : 0;
			UE_LOG(LogTemp, Log, TEXT("[DCC Esc] menu %d->%d side %d->%d seat %d->%d"),
				MenuBefore, MenuAfter, SideBefore, SideAfter, SeatBefore, SeatAfter);
		});
	}));

static FAutoConsoleCommandWithWorldAndArgs GDevTab(TEXT("DCC.Tab"), TEXT("DCC.Tab [delay]: (dev) press Tab for the local crawler."),
	FConsoleCommandWithWorldAndArgsDelegate::CreateLambda([](const TArray<FString>& Args, UWorld* World)
	{
		After(World, Args.Num() > 0 ? FCString::Atof(*Args[0]) : 0.0f, [](UWorld* W)
		{
			ADCCCharacterBase* Crawler = LocalCrawler(W);
			if (!Crawler)
			{
				return;
			}
			APlayerController* PC = Cast<APlayerController>(Crawler->GetController());
			ADCCHUD* Hud = PC ? Cast<ADCCHUD>(PC->GetHUD()) : nullptr;
			const ADCCCasinoCascade* Casino = ADCCCasinoCascade::Get(W);
			const int32 MenuBefore = Hud && Hud->IsMenuVisible() ? 1 : 0;
			const int32 SideBefore = Crawler->HasSideMenu() ? 1 : 0;
			const int32 SeatBefore = Casino && Casino->IsSeated(Crawler) ? 1 : 0;
			Crawler->ToggleCharacterMenu();
			const int32 MenuAfter = Hud && Hud->IsMenuVisible() ? 1 : 0;
			const int32 SideAfter = Crawler->HasSideMenu() ? 1 : 0;
			const int32 SeatAfter = Casino && Casino->IsSeated(Crawler) ? 1 : 0;
			UE_LOG(LogTemp, Log, TEXT("[DCC Tab] menu %d->%d side %d->%d seat %d->%d"),
				MenuBefore, MenuAfter, SideBefore, SideAfter, SeatBefore, SeatAfter);
		});
	}));

static FAutoConsoleCommandWithWorldAndArgs GDevInteract(TEXT("DCC.Interact"), TEXT("DCC.Interact [delay]: (dev) press E for the local crawler."),
	FConsoleCommandWithWorldAndArgsDelegate::CreateLambda([](const TArray<FString>& Args, UWorld* World)
	{
		After(World, Args.Num() > 0 ? FCString::Atof(*Args[0]) : 0.0f, [](UWorld* W)
		{
			if (ADCCCharacterBase* Crawler = LocalCrawler(W)) { Crawler->HandleInteractPressed(); }
		});
	}));

// The only pickup Gate Run places is the keg, at 70 kg. A Strength 4 crawler lifts 60 kg, so the
// throw and stow check uses a bottle instead. Dev only. It does not pay gold.
static FAutoConsoleCommandWithWorldAndArgs GDevBottle(TEXT("DCC.Bottle"), TEXT("DCC.Bottle [delay]: (dev) put a bottle on the ground in front of the local crawler."),
	FConsoleCommandWithWorldAndArgsDelegate::CreateLambda([](const TArray<FString>& Args, UWorld* World)
	{
		After(World, Args.Num() > 0 ? FCString::Atof(*Args[0]) : 0.0f, [](UWorld* W)
		{
			ADCCCharacterBase* Crawler = LocalCrawler(W);
			if (!Crawler)
			{
				return;
			}
			const FVector At = Crawler->GetActorLocation() + Crawler->GetActorForwardVector() * 120.0f + FVector(0.0f, 0.0f, 40.0f);
			W->SpawnActor<ADCCPickup_Bottle>(ADCCPickup_Bottle::StaticClass(), FTransform(At));
			UE_LOG(LogTemp, Log, TEXT("[DCC Bottle] on the ground at %s"), *At.ToString());
		});
	}));

static FAutoConsoleCommandWithWorldAndArgs GDevEmotes(TEXT("DCC.Emotes"), TEXT("DCC.Emotes [delay]: (dev) open the B menu."),
	FConsoleCommandWithWorldAndArgsDelegate::CreateLambda([](const TArray<FString>& Args, UWorld* World)
	{
		After(World, Args.Num() > 0 ? FCString::Atof(*Args[0]) : 0.0f, [](UWorld* W)
		{
			if (ADCCCharacterBase* Crawler = LocalCrawler(W)) { Crawler->ToggleEmoteMenu(); }
		});
	}));

// DCC.NextFood [delay]: (dev) the middle mouse button: the belt's food slot moves on to the next food.
static FAutoConsoleCommandWithWorldAndArgs GDevNextFood(TEXT("DCC.NextFood"), TEXT("DCC.NextFood [delay]: (dev) pick the next food on the belt."),
	FConsoleCommandWithWorldAndArgsDelegate::CreateLambda([](const TArray<FString>& Args, UWorld* World)
	{
		After(World, Args.Num() > 0 ? FCString::Atof(*Args[0]) : 0.0f, [](UWorld* W)
		{
			if (ADCCCharacterBase* Crawler = LocalCrawler(W)) { Crawler->CycleFood(); }
		});
	}));

// DCC.Fight <seconds> [delay]: (dev) for automated fight checks (2026-11-17). For <seconds>, the local crawler keeps
// turning to the nearest living enemy, walks up to it, and attacks about twice a second, as a player would. Scripted
// clicks alone fired a ranged weapon at nothing, because a gun shoots where the camera points.
static FAutoConsoleCommandWithWorldAndArgs GDevFight(TEXT("DCC.Fight"), TEXT("DCC.Fight <seconds> [delay]: (dev) fight the nearest enemies."),
	FConsoleCommandWithWorldAndArgsDelegate::CreateLambda([](const TArray<FString>& Args, UWorld* World)
	{
		const float Seconds = Args.Num() > 0 ? FCString::Atof(*Args[0]) : 30.0f;
		After(World, Args.Num() > 1 ? FCString::Atof(*Args[1]) : 0.0f, [Seconds](UWorld* W)
		{
			struct FState { double Until = 0.0; double NextSwing = 0.0; };
			TSharedRef<FState> State = MakeShared<FState>();
			State->Until = W->GetTimeSeconds() + Seconds;
			TWeakObjectPtr<UWorld> Weak(W);
			TSharedRef<FTimerHandle> Handle = MakeShared<FTimerHandle>();
			W->GetTimerManager().SetTimer(*Handle, FTimerDelegate::CreateLambda([Weak, State, Handle]()
			{
				UWorld* World = Weak.Get();
				if (!World)
				{
					return;
				}
				ADCCCharacterBase* Me = LocalCrawler(World);
				APlayerController* PC = World->GetFirstPlayerController();
				if (!Me || !PC || !Me->IsAlive() || World->GetTimeSeconds() >= State->Until)
				{
					World->GetTimerManager().ClearTimer(*Handle);
					return;
				}
				ADCCEnemyCharacter* Target = nullptr;
				float Best = 6000.0f;
				for (TActorIterator<ADCCEnemyCharacter> It(World); It; ++It)
				{
					if (It->IsAlive() && !It->bIsAlly)
					{
						const float D = FVector::Dist(It->GetActorLocation(), Me->GetActorLocation());
						if (D < Best) { Best = D; Target = *It; }
					}
				}
				if (!Target)
				{
					return;
				}
				const FVector To = Target->GetActorLocation() + FVector(0, 0, 40) - Me->GetActorLocation();
				PC->SetControlRotation(To.Rotation());
				if (Best > 200.0f)
				{
					Me->AddMovementInput(To.GetSafeNormal2D(), 1.0f);
				}
				if (World->GetTimeSeconds() >= State->NextSwing)
				{
					State->NextSwing = World->GetTimeSeconds() + 0.5;
					Me->DevClick();
				}
			}), 0.03f, true);
		});
	}));

// DCC.Podium <tube 1-4> [delay]: (dev) stand the local crawler at a summoning tube's podium and open its panel.
// DCC.SummonFree <tube 1-4> [delay]: (dev) a pull at that tube that costs no tickets (so tests leave the profile alone).
static FAutoConsoleCommandWithWorldAndArgs GDevPodium(TEXT("DCC.Podium"), TEXT("DCC.Podium <tube 1-4> [delay]: (dev) go to a F.I.A.T. podium."),
	FConsoleCommandWithWorldAndArgsDelegate::CreateLambda([](const TArray<FString>& Args, UWorld* World)
	{
		const int32 Tube = FMath::Clamp(Args.Num() > 0 ? FCString::Atoi(*Args[0]) : 1, 1, ADCCArcadeLobby::TubeCount) - 1;
		After(World, Args.Num() > 1 ? FCString::Atof(*Args[1]) : 0.0f, [Tube](UWorld* W)
		{
			ADCCCharacterBase* Me = LocalCrawler(W);
			APlayerController* PC = W->GetFirstPlayerController();
			for (TActorIterator<ADCCArcadeLobby> It(W); It && Me && PC; ++It)
			{
				const FVector At = It->PodiumCenter(Tube) + FVector(60, 130, 100);
				Me->SetActorLocation(At, false, nullptr, ETeleportType::TeleportPhysics);
				PC->SetControlRotation((It->TubeCenter(Tube) + FVector(0, 0, 300) - At).Rotation());
				if (ADCCHUD* Hud = Cast<ADCCHUD>(PC->GetHUD())) { Hud->ToggleSummonPanel(Tube); }
			}
		});
	}));

static FAutoConsoleCommandWithWorldAndArgs GDevSummonFree(TEXT("DCC.SummonFree"), TEXT("DCC.SummonFree <tube 1-4> [delay]: (dev) a free summon."),
	FConsoleCommandWithWorldAndArgsDelegate::CreateLambda([](const TArray<FString>& Args, UWorld* World)
	{
		const int32 Tube = FMath::Clamp(Args.Num() > 0 ? FCString::Atoi(*Args[0]) : 1, 1, ADCCArcadeLobby::TubeCount) - 1;
		After(World, Args.Num() > 1 ? FCString::Atof(*Args[1]) : 0.0f, [Tube](UWorld* W)
		{
			ADCCCharacterBase* Me = LocalCrawler(W);
			APlayerController* PC = W->GetFirstPlayerController();
			if (ADCCHUD* Hud = PC ? Cast<ADCCHUD>(PC->GetHUD()) : nullptr)
			{
				if (Hud->IsSummonPanelOpen()) { Hud->ToggleSummonPanel(Tube); }
			}
			if (Me && Me->Wallet) { Me->Wallet->DoSummon(uint8(Tube), true); }
		});
	}));

// DCC.Equip <Row> [delay]: (dev) wear a gear row. DCC.Hit <BoxName> [delay]: (dev) press a canvas button by name.
static FAutoConsoleCommandWithWorldAndArgs GDevEquip(TEXT("DCC.Equip"), TEXT("DCC.Equip <RowName> [delay]: (dev) equip a gear row on the local crawler."),
	FConsoleCommandWithWorldAndArgsDelegate::CreateLambda([](const TArray<FString>& Args, UWorld* World)
	{
		if (Args.Num() < 1) { return; }
		const FString Id = Args[0];
		After(World, Args.Num() > 1 ? FCString::Atof(*Args[1]) : 0.0f, [Id](UWorld* W)
		{
			if (ADCCCharacterBase* Crawler = LocalCrawler(W)) { Crawler->DCCEquip(Id); }
		});
	}));

static FAutoConsoleCommandWithWorldAndArgs GDevHit(TEXT("DCC.Hit"), TEXT("DCC.Hit <BoxName> [delay]: (dev) click a canvas hit box by name, e.g. Off|0."),
	FConsoleCommandWithWorldAndArgsDelegate::CreateLambda([](const TArray<FString>& Args, UWorld* World)
	{
		if (Args.Num() < 1) { return; }
		const FString Box = Args[0];
		After(World, Args.Num() > 1 ? FCString::Atof(*Args[1]) : 0.0f, [Box](UWorld* W)
		{
			APlayerController* PC = W ? W->GetFirstPlayerController() : nullptr;
			if (ADCCHUD* Hud = PC ? Cast<ADCCHUD>(PC->GetHUD()) : nullptr)
			{
				Hud->NotifyHitBoxClick(FName(*Box));
			}
		});
	}));

static FAutoConsoleCommandWithWorldAndArgs GDevAchieve(TEXT("DCC.Achieve"), TEXT("DCC.Achieve [delay]: (dev) queue two achievement cards, one after the other."),
	FConsoleCommandWithWorldAndArgsDelegate::CreateLambda([](const TArray<FString>& Args, UWorld* World)
	{
		After(World, Args.Num() > 0 ? FCString::Atof(*Args[0]) : 0.0f, [](UWorld* W)
		{
			ADCCCharacterBase* Crawler = LocalCrawler(W);
			UDCCAchievementComponent* Achievements = Crawler ? UDCCAchievementComponent::Find(Crawler) : nullptr;
			if (!Achievements)
			{
				UE_LOG(LogTemp, Warning, TEXT("[DCC Achieve] no component"));
				return;
			}
			Achievements->DebugAnnounce(TEXT("First Light"), 40, 0);
			Achievements->DebugAnnounce(TEXT("Still Standing"), 25, 2);
		});
	}));

// DCC.Style <Row|none> [delay]: (dev) equip, then swing, so the log names the style and the clip.
static FAutoConsoleCommandWithWorldAndArgs GDevStyle(TEXT("DCC.Style"), TEXT("DCC.Style <RowName|none> [delay]: (dev) equip and swing."),
	FConsoleCommandWithWorldAndArgsDelegate::CreateLambda([](const TArray<FString>& Args, UWorld* World)
	{
		const FString Row = Args.Num() > 0 ? Args[0] : TEXT("none");
		After(World, Args.Num() > 1 ? FCString::Atof(*Args[1]) : 0.0f, [Row](UWorld* W)
		{
			ADCCCharacterBase* Crawler = LocalCrawler(W);
			if (!Crawler)
			{
				UE_LOG(LogTemp, Warning, TEXT("[DCC Style] no crawler"));
				return;
			}
			if (Row.Equals(TEXT("none"), ESearchCase::IgnoreCase) || Row.Equals(TEXT("hands"), ESearchCase::IgnoreCase))
			{
				Crawler->DCCUnequipSlot(0);
			}
			else
			{
				Crawler->DCCEquip(Row);
			}
			const FName Id = (Row.Equals(TEXT("none"), ESearchCase::IgnoreCase) || Row.Equals(TEXT("hands"), ESearchCase::IgnoreCase))
				? NAME_None : FName(*Row);
			FString Path;
			float Rate = 1.0f;
			bool bBorrowed = false;
			const DCCAttackStyle::EStyle Style = DCCAttackStyle::FromId(Id);
			DCCAttackStyle::ClipFor(Style, Path, Rate, bBorrowed);
			UE_LOG(LogTemp, Log, TEXT("[DCC Style] ask %s -> %s rate %.2f %s"),
				*Row, DCCAttackStyle::Name(Style), Rate, bBorrowed ? TEXT("borrowed") : TEXT("own clip"));
			Crawler->DebugSwing();
		});
	}));

// DCC.Whisper <Name> <Text> [delay]: (dev) a private line, and open that friend's thread.
static FAutoConsoleCommandWithWorldAndArgs GDevWhisper(TEXT("DCC.Whisper"), TEXT("DCC.Whisper <Name> <Text> [delay]: (dev) send a whisper and open the thread."),
	FConsoleCommandWithWorldAndArgsDelegate::CreateLambda([](const TArray<FString>& Args, UWorld* World)
	{
		const FString Name = Args.Num() > 0 ? Args[0] : TEXT("Scott");
		FString Text = Args.Num() > 1 ? Args[1] : TEXT("You up?");
		float Delay = 0.0f;
		if (Args.Num() > 2 && Args.Last().IsNumeric())
		{
			Delay = FCString::Atof(*Args.Last());
			Text.Empty();
			for (int32 i = 1; i < Args.Num() - 1; ++i)
			{
				if (!Text.IsEmpty()) { Text += TEXT(" "); }
				Text += Args[i];
			}
		}
		else if (Args.Num() > 2)
		{
			for (int32 i = 2; i < Args.Num(); ++i) { Text += TEXT(" "); Text += Args[i]; }
		}
		After(World, Delay, [Name, Text](UWorld* W)
		{
			ADCCCharacterBase* Crawler = LocalCrawler(W);
			APlayerController* PC = W ? W->GetFirstPlayerController() : nullptr;
			ADCCHUD* Hud = PC ? Cast<ADCCHUD>(PC->GetHUD()) : nullptr;
			if (Hud) { Hud->OpenSocialFriend(Name); }
			if (Crawler)
			{
				Crawler->Server_SendChatMessage(EDCCChatChannel::Whispers, Name, FText::FromString(Text));
				UE_LOG(LogTemp, Log, TEXT("[DCC Social] whisper to %s: %s"), *Name, *Text);
			}
		});
	}));

// DCC.RallyTrick <1-4> [delay]: (dev) queue a trick on the local truck. 1 front, 2 barrel, 3 back, 4 corkscrew.
static FAutoConsoleCommandWithWorldAndArgs GDevRallyTrick(TEXT("DCC.RallyTrick"), TEXT("DCC.RallyTrick <1-4> [delay]: (dev) queue an overhead trick."),
	FConsoleCommandWithWorldAndArgsDelegate::CreateLambda([](const TArray<FString>& Args, UWorld* World)
	{
		const uint8 Move = static_cast<uint8>(FMath::Clamp(Args.Num() > 0 ? FCString::Atoi(*Args[0]) : 1, 1, 4));
		After(World, Args.Num() > 1 ? FCString::Atof(*Args[1]) : 0.0f, [Move](UWorld* W)
		{
			ADCCRover* Truck = nullptr;
			if (APlayerController* PC = W ? W->GetFirstPlayerController() : nullptr)
			{
				Truck = Cast<ADCCRover>(PC->GetPawn());
			}
			if (!Truck)
			{
				for (TActorIterator<ADCCRover> It(W); It; ++It)
				{
					if (It->RallyTrick == 2 && It->Driver)
					{
						Truck = *It;
						break;
					}
				}
			}
			if (!Truck)
			{
				UE_LOG(LogTemp, Warning, TEXT("[DCC Drive] trick button %d: no truck"), Move);
				return;
			}
			Truck->Server_RallyPickTrick(Move);
			UE_LOG(LogTemp, Log, TEXT("[DCC Drive] trick button %d"), Move);
		});
	}));

// DCC.RallyPose <struggle|flip|drop|road|ramp> [delay]: (dev) put a mark on the first truck.
static FAutoConsoleCommandWithWorldAndArgs GDevRallyPose(TEXT("DCC.RallyPose"), TEXT("DCC.RallyPose <struggle|flip|drop|road|ramp> [delay]: (dev) pose a tow."),
	FConsoleCommandWithWorldAndArgsDelegate::CreateLambda([](const TArray<FString>& Args, UWorld* World)
	{
		const FString What = Args.Num() > 0 ? Args[0] : TEXT("flip");
		After(World, Args.Num() > 1 ? FCString::Atof(*Args[1]) : 0.0f, [What](UWorld* W)
		{
			for (TActorIterator<ADCCArcadeKart> It(W); It; ++It)
			{
				It->DebugPose(What);
				return;
			}
			UE_LOG(LogTemp, Warning, TEXT("[DCC Drive] pose %s: no rally"), *What);
		});
	}));

// DCC.GoTo <Game> [delay]: (dev) DCC.Arcade with a delay, for tests that travel mid-run (summoned allies following).
static FAutoConsoleCommandWithWorldAndArgs GDevGoTo(TEXT("DCC.GoTo"), TEXT("DCC.GoTo <Haunt|Climb|Kart|Neon|Gate|...> [delay]: (dev) travel to a game."),
	FConsoleCommandWithWorldAndArgsDelegate::CreateLambda([](const TArray<FString>& Args, UWorld* World)
	{
		const FName Game = Args.Num() > 0 ? FName(*Args[0]) : NAME_None;
		After(World, Args.Num() > 1 ? FCString::Atof(*Args[1]) : 0.0f, [Game](UWorld* W) { DCCArcade::Travel(W, Game); });
	}));
