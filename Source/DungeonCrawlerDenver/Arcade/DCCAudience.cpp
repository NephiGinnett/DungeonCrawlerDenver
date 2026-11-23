#include "Arcade/DCCAudience.h"

#include "Session/DCCProfile.h"

#include "Items/DCCLootBoxTypes.h"

#include "Items/DCCInventoryComponent.h"

#include "Arcade/DCCWalletComponent.h"
#include "Characters/DCCCharacterBase.h"
#include "Combat/DCCGameState.h"
#include "Engine/World.h"
#include "EngineUtils.h"
#include "Favor/DCCFavorComponent.h"
#include "GameFramework/PlayerState.h"
#include "HAL/PlatformTime.h"

namespace
{
	/** Handles for the reaction lines: in-universe viewers, same spirit as the Chatter table's. */
	const TCHAR* Handles[] = {
		TEXT("xXFloorFortyXx"), TEXT("DebtFreeDaydream"), TEXT("QuarterlyGremlin"), TEXT("ClipThatPls"), TEXT("RepoMenFanClub"),
		TEXT("SponsorWatch_9"), TEXT("ThirdShiftLurker"), TEXT("MileHighMod"), TEXT("BorantShillAcct7"), TEXT("NoNotesHonestly"),
		TEXT("LoanOfficerLarry"), TEXT("TwoCentsTooMany"), TEXT("CrawlerCam4K"), TEXT("ActuallyAnAlien"),
	};

	/** What chat says when it happens. {Name} is who did it. Short, no em dashes, the show's voice. */
	const TArray<const TCHAR*>& Lines(DCCAudience::EMoment Kind)
	{
		using EM = DCCAudience::EMoment;
		static const TArray<const TCHAR*> Kill = {
			TEXT("{Name} DELETED that guy"), TEXT("clip it clip it clip it"), TEXT("{Name} said no thank you"),
			TEXT("that one owed money anyway"), TEXT("{Name} is in their villain era"), TEXT("oof. respectfully. oof"),
		};
		static const TArray<const TCHAR*> Boss = {
			TEXT("THE GATEKEEPER IS DOWN"), TEXT("{Name} just paid off the whole loan"), TEXT("I screamed. my roommate screamed"),
			TEXT("put {Name} on the poster"), TEXT("SPONSORS ARE TYPING"),
		};
		static const TArray<const TCHAR*> Down = {
			TEXT("F for {Name}"), TEXT("{Name} is taking a little nap"), TEXT("get UP {Name}"), TEXT("someone revive {Name} I have money on this"),
			TEXT("the floor is undefeated"),
		};
		static const TArray<const TCHAR*> Revive = {
			TEXT("TEAMWORK"), TEXT("{Name} came back and chose violence"), TEXT("that revive was a love letter"), TEXT("we are so back"),
		};
		static const TArray<const TCHAR*> Trick = {
			TEXT("{Name} is styling on the tow truck"), TEXT("ok that flip was clean"), TEXT("the car is having the best day of its life"),
			TEXT("repo but make it ballet"),
		};
		static const TArray<const TCHAR*> Perfect = {
			TEXT("PERFECT?? {Name} you menace"), TEXT("frame perfect, someone check the tape"), TEXT("{Name} has done this before"),
			TEXT("the car did a FULL REPOSSESSION"), TEXT("ratings just doubled"),
		};
		static const TArray<const TCHAR*> Jackpot = {
			TEXT("{Name} is printing money, investigate"), TEXT("the house is sweating"), TEXT("JACKPOT JACKPOT JACKPOT"),
			TEXT("{Name} please buy me something"),
		};
		static const TArray<const TCHAR*> Achieve = {
			TEXT("achievement get, {Name}"), TEXT("{Name} unlocked a little treat"), TEXT("put it on the board"),
		};
		static const TArray<const TCHAR*> Level = {
			TEXT("{Name} leveled up, the numbers go up"), TEXT("ding"), TEXT("{Name} is getting dangerous"),
		};
		static const TArray<const TCHAR*> Room = {
			TEXT("room cleared, next"), TEXT("that was efficient. too efficient"), TEXT("{Name} cleaning house"),
		};
		static const TArray<const TCHAR*> Wipe = {
			TEXT("well. that happened"), TEXT("{Name} learned something today"), TEXT("we will be rewatching that"),
		};
		switch (Kind)
		{
		case EM::Kill: return Kill;
		case EM::BossDown: return Boss;
		case EM::Downed: return Down;
		case EM::Revive: return Revive;
		case EM::Trick: return Trick;
		case EM::PerfectTrick: return Perfect;
		case EM::Jackpot: return Jackpot;
		case EM::Achievement: return Achieve;
		case EM::LevelUp: return Level;
		case EM::RoomClear: return Room;
		default: return Wipe;
		}
	}

	/** How big a moment is: viewers gained, hype gained, favor earned, how likely chat is to react at all. */
	void Size(DCCAudience::EMoment Kind, int32& Viewers, float& Hype, int32& Favor, float& Talk)
	{
		using EM = DCCAudience::EMoment;
		switch (Kind)
		{
		case EM::Kill:         Viewers = 180;  Hype = 0.06f; Favor = 1; Talk = 0.35f; break;
		case EM::BossDown:     Viewers = 2500; Hype = 0.5f;  Favor = 6; Talk = 1.0f;  break;
		case EM::Downed:       Viewers = 600;  Hype = 0.12f; Favor = 0; Talk = 0.9f;  break;
		case EM::Revive:       Viewers = 900;  Hype = 0.18f; Favor = 3; Talk = 0.9f;  break;
		case EM::Trick:        Viewers = 250;  Hype = 0.07f; Favor = 1; Talk = 0.3f;  break;
		case EM::PerfectTrick: Viewers = 600;  Hype = 0.14f; Favor = 2; Talk = 0.6f;  break;
		case EM::Jackpot:      Viewers = 1200; Hype = 0.25f; Favor = 2; Talk = 1.0f;  break;
		case EM::Achievement:  Viewers = 400;  Hype = 0.08f; Favor = 1; Talk = 0.7f;  break;
		case EM::LevelUp:      Viewers = 500;  Hype = 0.1f;  Favor = 1; Talk = 0.8f;  break;
		case EM::RoomClear:    Viewers = 700;  Hype = 0.15f; Favor = 2; Talk = 0.6f;  break;
		default:               Viewers = 300;  Hype = 0.08f; Favor = 0; Talk = 0.7f;  break;
		}
	}

	TMap<FString, double>& TipAt()
	{
		static TMap<FString, double> Map;
		return Map;
	}

	FString NameOf(const ADCCCharacterBase* Who)
	{
		if (!Who)
		{
			return TEXT("someone");
		}
		return Who->DisplayName.IsEmpty() ? Who->GetName() : Who->DisplayName.ToString();
	}
}

namespace
{
	int32 FameFor(DCCAudience::EMoment Kind)
	{
		using EM = DCCAudience::EMoment;
		switch (Kind)
		{
		case EM::Kill: return 3;
		case EM::BossDown: return 40;
		case EM::Revive: return 12;
		case EM::Trick: return 3;
		case EM::PerfectTrick: return 8;
		case EM::Jackpot: return 10;
		case EM::Achievement: return 8;
		case EM::LevelUp: return 10;
		case EM::RoomClear: return 6;
		default: return 0;
		}
	}

	struct FFameWindow
	{
		double Start = 0.0;
		int32 Earned = 0;
	};

	void GiveFame(ADCCCharacterBase* Who, DCCAudience::EMoment Kind, float Hype)
	{
		APlayerState* Player = Who ? Who->GetPlayerState() : nullptr;
		if (!Player || !Who->IsPlayerControlled())
		{
			return;
		}
		int32 Gain = FMath::RoundToInt(FameFor(Kind) * (1.0f + FMath::Clamp(Hype, 0.0f, 1.0f)));
		if (Gain <= 0)
		{
			return;
		}
		const FString Key = DCCProfile::KeyFor(Player);
		// At most 150 a minute: the audience rewards a show, not a grind.
		static TMap<FString, FFameWindow> Windows;
		FFameWindow& Window = Windows.FindOrAdd(Key);
		const double Now = FPlatformTime::Seconds();
		if (Now - Window.Start > 60.0)
		{
			Window.Start = Now;
			Window.Earned = 0;
		}
		Gain = FMath::Min(Gain, 150 - Window.Earned);
		if (Gain <= 0)
		{
			return;
		}
		Window.Earned += Gain;

		static const TCHAR* Names[] = { TEXT("?"), TEXT("Bronze"), TEXT("Silver"), TEXT("Gold"), TEXT("Platinum"), TEXT("Legendary"), TEXT("Celestial") };
		int32 Fame = FMath::RoundToInt(DCCProfile::GetForKey(Key, TEXT("Audience"), TEXT("Fame"), 0.0f)) + Gain;
		int32 Level = FMath::RoundToInt(DCCProfile::GetForKey(Key, TEXT("Audience"), TEXT("FameLevel"), 0.0f));
		while (Fame >= DCCAudience::FameToNext(Level))
		{
			Fame -= DCCAudience::FameToNext(Level);
			++Level;
			const uint8 Tier = DCCAudience::FameReward(Level);
			if (Who->Inventory)
			{
				Who->Inventory->GrantLootBox(static_cast<EDCCLootBoxTier>(Tier));
			}
			if (Who->Wallet)
			{
				Who->Wallet->Client_FameLevel(Level, Tier);
			}
			if (ADCCGameState* State = Who->GetWorld() ? Who->GetWorld()->GetGameState<ADCCGameState>() : nullptr)
			{
				State->AddChatMessage(EDCCChatChannel::Chatter, TEXT("SponsorDesk"), nullptr, FString(),
					FText::FromString(FString::Printf(TEXT("%s hit Fame %d. A %s box is on its way."), *NameOf(Who), Level, Names[FMath::Min<int32>(Tier, 6)])), false);
			}
			UE_LOG(LogTemp, Log, TEXT("[DCC Fame] %s reached Fame %d: %s box"), *Key, Level, Names[FMath::Min<int32>(Tier, 6)]);
		}
		DCCProfile::SetForKey(Key, TEXT("Audience"), TEXT("Fame"), float(Fame));
		DCCProfile::SetForKey(Key, TEXT("Audience"), TEXT("FameLevel"), float(Level));
		if (Who->Wallet)
		{
			Who->Wallet->Fame = Fame;
			Who->Wallet->FameLevel = Level;
		}
	}
}

namespace DCCAudience
{
	void Moment(const UObject* WorldContext, ADCCCharacterBase* Who, EMoment Kind)
	{
		UWorld* World = WorldContext ? WorldContext->GetWorld() : nullptr;
		ADCCGameState* State = World ? World->GetGameState<ADCCGameState>() : nullptr;
		if (!State || !State->HasAuthority())
		{
			return;
		}
		int32 Viewers = 0, Favor = 0;
		float Hype = 0.0f, Talk = 0.0f;
		Size(Kind, Viewers, Hype, Favor, Talk);
		// Only the crawlers' moments are the show. A mob killing a mob is not content.
		const bool bCrawler = Who && Who->IsCrawler();
		if (!bCrawler && Kind != EMoment::BossDown)
		{
			return;
		}
		State->LiveViewers += FMath::RoundToInt(Viewers * FMath::FRandRange(0.7f, 1.3f));
		State->Hype = FMath::Min(1.0f, State->Hype + Hype);

		if (Who && Who->Favor && Favor > 0)
		{
			Who->Favor->AwardFavor(Favor, EDCCFavorEvent::DirectorCall, FText::FromString(TEXT("The audience liked that.")));
		}
		if (FMath::FRand() < Talk)
		{
			const TArray<const TCHAR*>& Pool = Lines(Kind);
			const FString Line = FString(Pool[FMath::RandRange(0, Pool.Num() - 1)]).Replace(TEXT("{Name}"), *NameOf(Who));
			State->AddChatMessage(EDCCChatChannel::Chatter, Handles[FMath::RandRange(0, UE_ARRAY_COUNT(Handles) - 1)], nullptr, FString(),
				FText::FromString(Line), false);
		}

		GiveFame(Who, Kind, State->Hype);
	}

	int32 FameToNext(int32 Level)
	{
		return 100 + 60 * FMath::Max(0, Level);
	}

	uint8 FameReward(int32 Level)
	{
		// Bronze, Bronze, Silver, Silver, Gold, Silver, Gold, Platinum, Gold, Legendary; then a Celestial every tenth
		// level, a Legendary every fifth, and Platinum or Gold between.
		using T = EDCCLootBoxTier;
		static const T Early[] = { T::Bronze, T::Bronze, T::Silver, T::Silver, T::Gold, T::Silver, T::Gold, T::Platinum, T::Gold, T::Legendary };
		if (Level >= 1 && Level <= 10)
		{
			return uint8(Early[Level - 1]);
		}
		if (Level % 10 == 0) { return uint8(T::Celestial); }
		if (Level % 5 == 0) { return uint8(T::Legendary); }
		return uint8(Level % 2 == 0 ? T::Platinum : T::Gold);
	}

	void Tick(ADCCGameState* State, float DeltaSeconds)
	{
		if (!State || !State->HasAuthority() || !State->GetWorld())
		{
			return;
		}
		int32 Crawlers = 0;
		for (TActorIterator<ADCCCharacterBase> It(State->GetWorld()); It; ++It)
		{
			Crawlers += It->IsPlayerControlled() ? 1 : 0;
		}
		if (Crawlers == 0)
		{
			return;
		}
		// Hype cools. Viewers drift toward what the hype deserves: a quiet room still has its regulars.
		State->Hype = FMath::Max(0.0f, State->Hype - 0.025f * DeltaSeconds);
		const float Target = 900.0f * Crawlers * (1.0f + 5.0f * State->Hype);
		const float Drift = (Target - State->LiveViewers) * FMath::Min(1.0f, 0.08f * DeltaSeconds);
		State->LiveViewers = FMath::Max(37, State->LiveViewers + FMath::RoundToInt(Drift + FMath::FRandRange(-6.0f, 6.0f) * Crawlers));

		// The chat never quite stops: one table line every few seconds, faster when the room is hot.
		State->NextAudienceChat -= DeltaSeconds;
		if (State->NextAudienceChat <= 0.0f)
		{
			State->NextAudienceChat = FMath::Lerp(9.0f, 2.5f, State->Hype) * FMath::FRandRange(0.7f, 1.3f);
			State->RollChatterBatch(1);
		}
	}
}
