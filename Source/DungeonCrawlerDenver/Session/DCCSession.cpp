#include "Session/DCCSession.h"
#include "Session/DCCUPnP.h"
#include "Session/DCCBuildStamp.h"

#include "Characters/DCCCharacterBase.h"
#include "Onboarding/DCCOnboardingComponent.h"
#include "Arcade/DCCWalletComponent.h"
#include "Session/DCCSaveSystem.h"
#include "Engine/Engine.h"
#include "GameFramework/GameModeBase.h"
#include "GameFramework/PlayerState.h"
#include "Engine/NetDriver.h"
#include "HAL/PlatformApplicationMisc.h"
#include "Engine/World.h"
#include "GameFramework/PlayerController.h"
#include "Kismet/GameplayStatics.h"
#include "Misc/ConfigCacheIni.h"
#include "IPAddress.h"
#include "SocketSubsystem.h"

namespace
{
	const TCHAR* ConfigSection = TEXT("/Script/DungeonCrawlerDenver.Party");

	// Crockford base32: no I, L, O or U, so a code read aloud cannot be misheard as a digit.
	const TCHAR* Alphabet = TEXT("0123456789ABCDEFGHJKMNPQRSTVWXYZ");

	FString GLastError;
	bool GFailureHandlersBound = false;

	int32 AlphabetIndex(TCHAR C)
	{
		C = FChar::ToUpper(C);
		if (C == TEXT('O')) { C = TEXT('0'); }
		if (C == TEXT('I') || C == TEXT('L')) { C = TEXT('1'); }
		for (int32 i = 0; i < 32; ++i)
		{
			if (Alphabet[i] == C)
			{
				return i;
			}
		}
		return INDEX_NONE;
	}

	bool ParseIPv4(const FString& Text, uint32& Out)
	{
		TArray<FString> Parts;
		if (Text.ParseIntoArray(Parts, TEXT("."), true) != 4)
		{
			return false;
		}
		Out = 0;
		for (const FString& Part : Parts)
		{
			if (!Part.IsNumeric())
			{
				return false;
			}
			const int32 Value = FCString::Atoi(*Part);
			if (Value < 0 || Value > 255)
			{
				return false;
			}
			Out = (Out << 8) | static_cast<uint32>(Value);
		}
		return true;
	}

	FString FormatIPv4(uint32 Ip)
	{
		return FString::Printf(TEXT("%u.%u.%u.%u"), (Ip >> 24) & 255, (Ip >> 16) & 255, (Ip >> 8) & 255, Ip & 255);
	}

	/** 0 = Tailscale, 1 = LAN, 2 = other, -1 = never useful to a friend. */
	int32 Rank(uint32 Ip, FString& OutLabel)
	{
		const uint32 A = Ip >> 24, B = (Ip >> 16) & 255;
		if (A == 127 || A == 0 || (A == 169 && B == 254))
		{
			return -1;
		}
		if (A == 100 && B >= 64 && B <= 127)
		{
			OutLabel = TEXT("Tailscale");
			return 0;
		}
		if (A == 10 || (A == 172 && B >= 16 && B <= 31) || (A == 192 && B == 168))
		{
			OutLabel = TEXT("Same Wi-Fi");
			return 1;
		}
		OutLabel = TEXT("This PC");
		return 2;
	}

	void HandleNetworkFailure(UWorld* World, UNetDriver* NetDriver, ENetworkFailure::Type FailureType, const FString& ErrorString)
	{
		GLastError = FString::Printf(TEXT("Lost the connection: %s"), *ErrorString);
		UE_LOG(LogTemp, Warning, TEXT("[DCC Session] %s"), *GLastError);
	}

	void HandleTravelFailure(UWorld* World, ETravelFailure::Type FailureType, const FString& ErrorString)
	{
		GLastError = FString::Printf(TEXT("Could not reach the host: %s"), *ErrorString);
		UE_LOG(LogTemp, Warning, TEXT("[DCC Session] %s"), *GLastError);
	}
}

namespace DCCSession
{
	const TArray<FName>& Roster()
	{
		static const TArray<FName> Names = {
			TEXT("CJ"), TEXT("Scott"), TEXT("Marcel"), TEXT("Vaughn"), TEXT("Kyle"), TEXT("Zoe")
		};
		return Names;
	}

	FString EncodeInvite(uint32 IPv4, int32 Port)
	{
		// 48 bits (32 address, 16 port) is 10 base32 characters, shown as two fives.
		uint64 Value = (static_cast<uint64>(IPv4) << 16) | static_cast<uint64>(Port & 0xFFFF);
		TCHAR Chars[10];
		for (int32 i = 9; i >= 0; --i)
		{
			Chars[i] = Alphabet[Value & 31];
			Value >>= 5;
		}
		return FString::ConstructFromPtrSize(Chars, 5) + TEXT("-") + FString::ConstructFromPtrSize(Chars + 5, 5);
	}

	FString ResolveJoinTarget(const FString& CodeOrAddress)
	{
		const FString Trimmed = CodeOrAddress.TrimStartAndEnd();

		FString Compact;
		for (const TCHAR C : Trimmed)
		{
			if (C != TEXT('-') && C != TEXT(' '))
			{
				Compact.AppendChar(C);
			}
		}

		if (Compact.Len() == 10 && !Trimmed.Contains(TEXT(".")) && !Trimmed.Contains(TEXT(":")))
		{
			uint64 Value = 0;
			bool bValid = true;
			for (const TCHAR C : Compact)
			{
				const int32 Index = AlphabetIndex(C);
				if (Index == INDEX_NONE)
				{
					bValid = false;
					break;
				}
				Value = (Value << 5) | static_cast<uint64>(Index);
			}
			if (bValid)
			{
				const uint32 Ip = static_cast<uint32>((Value >> 16) & 0xFFFFFFFFull);
				const int32 Port = static_cast<int32>(Value & 0xFFFF);
				return FString::Printf(TEXT("%s:%d"), *FormatIPv4(Ip), Port);
			}
		}

		return Trimmed;
	}

	FString FindInviteIn(const FString& Text)
	{
		TArray<FString> Tokens;
		Text.ParseIntoArrayWS(Tokens);
		for (FString Token : Tokens)
		{
			while (!Token.IsEmpty() && !FChar::IsAlnum(Token[Token.Len() - 1]))
			{
				Token.LeftChopInline(1);
			}
			while (!Token.IsEmpty() && !FChar::IsAlnum(Token[0]))
			{
				Token.RightChopInline(1);
			}
			if (Token.IsEmpty())
			{
				continue;
			}

			// An invite code decodes to an address; anything else comes back unchanged.
			if (ResolveJoinTarget(Token) != Token)
			{
				return Token;
			}

			// A bare address: digits and dots, optionally :port, with four parts.
			FString Host = Token, Port;
			Token.Split(TEXT(":"), &Host, &Port);
			TArray<FString> Parts;
			Host.ParseIntoArray(Parts, TEXT("."));
			const bool bDigits = Parts.Num() == 4 && Parts.FindByPredicate([](const FString& Part)
			{
				return Part.IsEmpty() || !Part.IsNumeric();
			}) == nullptr;
			if (bDigits && (Port.IsEmpty() || Port.IsNumeric()))
			{
				return Token;
			}
		}
		return FString();
	}

	bool JoinFromClipboard(APlayerController* LocalPlayer, FString& OutTarget)
	{
		FString Clipboard;
		FPlatformApplicationMisc::ClipboardPaste(Clipboard);
		const FString Invite = FindInviteIn(Clipboard);
		if (Invite.IsEmpty())
		{
			return false;
		}
		OutTarget = ResolveJoinTarget(Invite);
		Join(LocalPlayer, Invite);
		return true;
	}

	FString InviteMessage(const FString& InviteCode)
	{
		return FString::Printf(
			TEXT("Join my Dungeon Crawler Denver party! Copy this whole message, then click JOIN A PARTY in the game's main menu. Code: %s"),
			*InviteCode);
	}

	TArray<FHostAddress> GetHostAddresses(int32 Port)
	{
		TArray<TPair<int32, FHostAddress>> Ranked;

		if (ISocketSubsystem* Sockets = ISocketSubsystem::Get(PLATFORM_SOCKETSUBSYSTEM))
		{
			TArray<TSharedPtr<FInternetAddr>> Adapters;
			Sockets->GetLocalAdapterAddresses(Adapters);

			bool bCanBind = false;
			if (TSharedPtr<FInternetAddr> Host = Sockets->GetLocalHostAddr(*GLog, bCanBind))
			{
				Adapters.Add(Host);
			}

			TSet<uint32> Seen;
			for (const TSharedPtr<FInternetAddr>& Addr : Adapters)
			{
				uint32 Ip = 0;
				if (!Addr.IsValid() || !ParseIPv4(Addr->ToString(false), Ip) || Seen.Contains(Ip))
				{
					continue;
				}
				Seen.Add(Ip);

				FHostAddress Entry;
				const int32 Order = Rank(Ip, Entry.Label);
				if (Order < 0)
				{
					continue;
				}
				Entry.Address = FString::Printf(TEXT("%s:%d"), *FormatIPv4(Ip), Port);
				Entry.InviteCode = EncodeInvite(Ip, Port);
				Ranked.Add(TPair<int32, FHostAddress>(Order, Entry));
			}
		}

		Ranked.StableSort([](const TPair<int32, FHostAddress>& A, const TPair<int32, FHostAddress>& B)
		{
			return A.Key < B.Key;
		});

		TArray<FHostAddress> Out;
		for (const TPair<int32, FHostAddress>& Pair : Ranked)
		{
			Out.Add(Pair.Value);
		}
		// The router opened the port (DCCUPnP): the public address, for friends anywhere. After Tailscale, which
		// needs nothing from the router, and before the same-Wi-Fi one.
		if (DCCUPnP::GetState() == DCCUPnP::EState::Open && DCCUPnP::GetPublicIp() != 0)
		{
			FHostAddress Internet;
			Internet.Label = TEXT("Internet");
			Internet.Address = FString::Printf(TEXT("%s:%d"), *FormatIPv4(DCCUPnP::GetPublicIp()), Port);
			Internet.InviteCode = EncodeInvite(DCCUPnP::GetPublicIp(), Port);
			int32 At = 0;
			while (At < Out.Num() && Out[At].Label == TEXT("Tailscale")) { ++At; }
			Out.Insert(Internet, At);
		}
		return Out;
	}

	bool StartHosting(UWorld* World, APlayerController* LocalPlayer, FString& OutError)
	{
		if (!World)
		{
			OutError = TEXT("No world to host.");
			return false;
		}

		const ENetMode Mode = World->GetNetMode();
		if (Mode == NM_ListenServer || Mode == NM_DedicatedServer)
		{
			return true;
		}
		if (Mode == NM_Client)
		{
			OutError = TEXT("You are connected to someone else's game. Leave it first.");
			return false;
		}

		// Opening the running world keeps everything in progress: positions,
		// loot, onboarding. UWorld::Listen creates the game net driver, and the
		// driver picks up every replicated actor that already exists.
		FURL Url;
		Url.Port = DefaultPort;
		if (World->Listen(Url))
		{
			UE_LOG(LogTemp, Warning, TEXT("[DCC Session] Hosting on port %d without reloading."), DefaultPort);
			DCCUPnP::Start(DefaultPort);
			return true;
		}

		// Fallback: reopen this level as a listen server. Progress in the level is lost.
		if (LocalPlayer)
		{
			const FString Command = FString::Printf(TEXT("open %s?listen?Crawler=%s"),
				*UGameplayStatics::GetCurrentLevelName(World, true), *GetPreferredCrawler().ToString());
			UE_LOG(LogTemp, Warning, TEXT("[DCC Session] Listen failed in place; reopening: %s"), *Command);
			LocalPlayer->ConsoleCommand(Command);
			return true;
		}

		OutError = TEXT("Could not open the game to other players. Is port 7777 in use?");
		return false;
	}

	void Join(APlayerController* LocalPlayer, const FString& CodeOrAddress)
	{
		if (!LocalPlayer)
		{
			return;
		}

		EnsureFailureHandlers();

		const FString Target = ResolveJoinTarget(CodeOrAddress);
		if (Target.IsEmpty())
		{
			GLastError = TEXT("Type an invite code or an address first.");
			return;
		}

		GConfig->SetString(ConfigSection, TEXT("LastJoin"), *CodeOrAddress.TrimStartAndEnd(), GGameUserSettingsIni);
		GConfig->Flush(false, GGameUserSettingsIni);

		// The host checks the password hash before it lets us be this crawler (ADCCGameMode::InitNewPlayer).
		// ?Build= lets the host refuse a different build of the game with a clear message (Session/DCCBuildStamp).
		const FString Url = FString::Printf(TEXT("%s?Crawler=%s?Pw=%s?Build=%s"), *Target, *GetPreferredCrawler().ToString(),
			*GetSavedPasswordHash(GetPreferredCrawler()), *DCCBuildStamp::Get());
		UE_LOG(LogTemp, Warning, TEXT("[DCC Session] Joining %s as %s"), *Target, *GetPreferredCrawler().ToString());
		LocalPlayer->ClientTravel(Url, TRAVEL_Absolute);
	}

	void Leave(APlayerController* LocalPlayer)
	{
		if (LocalPlayer)
		{
			LocalPlayer->ConsoleCommand(TEXT("disconnect"));
		}
	}

	FString GetSavedPasswordHash(FName CrawlerId)
	{
		FString Value;
		if (GConfig)
		{
			GConfig->GetString(ConfigSection, *(FString(TEXT("Pw_")) + CrawlerId.ToString()), Value, GGameUserSettingsIni);
		}
		return Value;
	}

	void SavePasswordHash(FName CrawlerId, const FString& Hash)
	{
		if (GConfig)
		{
			GConfig->SetString(ConfigSection, *(FString(TEXT("Pw_")) + CrawlerId.ToString()), *Hash, GGameUserSettingsIni);
			GConfig->Flush(false, GGameUserSettingsIni);
		}
	}

	bool HasSeenGuide(const FString& PlayerKey, FName Guide)
	{
		FString Value;
		if (!GConfig || !GConfig->GetString(ConfigSection, *(FString(TEXT("Guides_")) + PlayerKey), Value, GGameUserSettingsIni))
		{
			return false;
		}
		TArray<FString> Seen;
		Value.ParseIntoArray(Seen, TEXT(","));
		return Seen.Contains(Guide.ToString());
	}

	void MarkGuideSeen(const FString& PlayerKey, FName Guide)
	{
		if (!GConfig || PlayerKey.IsEmpty() || Guide.IsNone() || HasSeenGuide(PlayerKey, Guide))
		{
			return;
		}
		FString Value;
		GConfig->GetString(ConfigSection, *(FString(TEXT("Guides_")) + PlayerKey), Value, GGameUserSettingsIni);
		Value = Value.IsEmpty() ? Guide.ToString() : Value + TEXT(",") + Guide.ToString();
		GConfig->SetString(ConfigSection, *(FString(TEXT("Guides_")) + PlayerKey), *Value, GGameUserSettingsIni);
		GConfig->Flush(false, GGameUserSettingsIni);
	}

	FName GetPreferredCrawler()
	{
		FString Value;
		if (GConfig && GConfig->GetString(ConfigSection, TEXT("Crawler"), Value, GGameUserSettingsIni) && !Value.IsEmpty())
		{
			return FName(*Value);
		}
		return Roster()[0];
	}

	void SetPreferredCrawler(FName CrawlerId)
	{
		if (!GConfig)
		{
			return;
		}
		GConfig->SetString(ConfigSection, TEXT("Crawler"), *CrawlerId.ToString(), GGameUserSettingsIni);
		GConfig->Flush(false, GGameUserSettingsIni);
	}

	bool SwitchCrawler(APlayerController* LocalPlayer, FName CrawlerId)
	{
		SetPreferredCrawler(CrawlerId);

		ADCCCharacterBase* Crawler = LocalPlayer ? Cast<ADCCCharacterBase>(LocalPlayer->GetPawn()) : nullptr;
		UWorld* World = LocalPlayer ? LocalPlayer->GetWorld() : nullptr;
		if (!Crawler || !Crawler->HasAuthority() || !Crawler->Onboarding || !World)
		{
			return false;
		}
		if (Crawler->Onboarding->CrawlerId == CrawlerId)
		{
			return true;
		}

		// Keep what the old crawler did here. The save system only lets the pawn
		// that loaded a crawler write it, so the new name can never overwrite the
		// new crawler's save with the old one's gear.
		DCCSave::Save(Crawler);

		Crawler->Onboarding->CrawlerId = CrawlerId;
		Crawler->Onboarding->ApplyCrawlerLook();
		if (AGameModeBase* GameMode = World->GetAuthGameMode())
		{
			if (LocalPlayer->PlayerState && LocalPlayer->PlayerState->GetPlayerName() != CrawlerId.ToString())
			{
				GameMode->ChangeName(LocalPlayer, CrawlerId.ToString(), false);
			}
		}
		// A new name is a new arcade profile: their tickets and their outfit.
		if (Crawler->Wallet)
		{
			Crawler->Wallet->LoadFromProfile();
		}
		return true;
	}

	FString GetLastJoinTarget()
	{
		FString Value;
		if (GConfig)
		{
			GConfig->GetString(ConfigSection, TEXT("LastJoin"), Value, GGameUserSettingsIni);
		}
		return Value;
	}

	FString TakeLastError()
	{
		FString Out = MoveTemp(GLastError);
		GLastError.Reset();
		return Out;
	}

	bool HasPendingError()
	{
		return !GLastError.IsEmpty();
	}

	void EnsureFailureHandlers()
	{
		if (GFailureHandlersBound || !GEngine)
		{
			return;
		}
		GEngine->OnNetworkFailure().AddStatic(&HandleNetworkFailure);
		GEngine->OnTravelFailure().AddStatic(&HandleTravelFailure);
		GFailureHandlersBound = true;
	}
}
