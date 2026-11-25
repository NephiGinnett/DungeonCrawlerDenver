#include "Session/DCCOnline.h"

#include "GenericPlatform/GenericPlatformMisc.h"
#include "HAL/PlatformMisc.h"
#include "IEOSSDKManager.h"
#include "Online/Auth.h"
#include "Online/Lobbies.h"
#include "Online/OnlineAsyncOpHandle.h"
#include "Online/OnlineResult.h"
#include "Online/OnlineServices.h"
#include "Online/OnlineServicesEpicCommon.h"
#include "eos_connect.h"
#include "eos_sdk.h"

using namespace UE::Online;

namespace
{
	const FName PartyName(TEXT("DCCParty"));
	const FName SchemaName(TEXT("DCCParty"));
	const FName HostAttr(TEXT("HostName"));
	const FName PlaceAttr(TEXT("Place"));

	DCCOnline::EState GState = DCCOnline::EState::Off;
	FString GStatus;
	FAccountId GAccount;
	FLobbyId GLobby;
	bool GHosting = false;
	TArray<TSharedRef<const FLobby>> GFound;

	IOnlineServicesPtr Services()
	{
		return GetServices(EOnlineServices::EpicGame);
	}

	void Fail(const FString& Why)
	{
		GState = DCCOnline::EState::Failed;
		GStatus = Why;
		UE_LOG(LogTemp, Warning, TEXT("[DCC Online] %s"), *Why);
	}

	void SignIn()
	{
		IOnlineServicesPtr Online = Services();
		IAuthPtr Auth = Online ? Online->GetAuthInterface() : nullptr;
		if (!Auth)
		{
			Fail(TEXT("Online is off (no EOS services). Invite codes still work."));
			return;
		}
		FAuthLogin::Params Params;
		Params.PlatformUserId = FPlatformMisc::GetPlatformUserForUserIndex(0);
		Params.CredentialsType = LoginCredentialsType::ExternalAuth;
		Params.CredentialsToken.Emplace<FExternalAuthToken>(FExternalAuthToken{ ExternalLoginType::DeviceIdAccessToken, FString() });
		Auth->Login(MoveTemp(Params)).OnComplete([](const TOnlineResult<FAuthLogin>& Result)
		{
			if (Result.IsOk())
			{
				GAccount = Result.GetOkValue().AccountInfo->AccountId;
				GState = DCCOnline::EState::Online;
				GStatus = TEXT("Online. Friends can find your party.");
				UE_LOG(LogTemp, Log, TEXT("[DCC Online] signed in"));
			}
			else
			{
				Fail(FString::Printf(TEXT("Could not sign in to Epic (%s). Invite codes still work."), *Result.GetErrorValue().GetLogString()));
			}
		});
	}

	// EOS's anonymous "this device" id has to exist before a device sign-in; making it twice is harmless.
	void EOS_CALL OnDeviceId(const EOS_Connect_CreateDeviceIdCallbackInfo* Data)
	{
		if (Data->ResultCode == EOS_EResult::EOS_Success || Data->ResultCode == EOS_EResult::EOS_DuplicateNotAllowed)
		{
			SignIn();
		}
		else
		{
			Fail(FString::Printf(TEXT("Could not make this PC's Epic id (%s). Invite codes still work."), ANSI_TO_TCHAR(EOS_EResult_ToString(Data->ResultCode))));
		}
	}

	const FString* AttrString(const FLobby& Lobby, FName Id)
	{
		const FSchemaVariant* Value = Lobby.Attributes.Find(Id);
		return Value && Value->GetType() == ESchemaAttributeType::String ? &Value->GetString() : nullptr;
	}
}

void DCCOnline::Start()
{
	if (GState == EState::SigningIn || GState == EState::Online)
	{
		return;
	}
	FString Secret;
	GConfig->GetString(TEXT("OnlineServices.EOS"), TEXT("ClientSecret"), Secret, GEngineIni);
	if (Secret.IsEmpty() || Secret.StartsWith(TEXT("PASTE_")))
	{
		Fail(TEXT("Online is not set up yet (no Client Secret). Invite codes still work."));
		return;
	}
	IOnlineServicesPtr Online = Services();
	if (!Online || Online->GetServicesProvider() != EOnlineServices::EpicGame)
	{
		Fail(TEXT("Online is off (EOS did not start). Invite codes still work."));
		return;
	}
	IEOSPlatformHandlePtr Platform = static_cast<FOnlineServicesEpicCommon*>(Online.Get())->GetEOSPlatformHandle();
	if (!Platform)
	{
		Fail(TEXT("Online is off (EOS did not start). Invite codes still work."));
		return;
	}
	GState = EState::SigningIn;
	GStatus = TEXT("Signing in to Epic...");
	EOS_Connect_CreateDeviceIdOptions Options = {};
	Options.ApiVersion = EOS_CONNECT_CREATEDEVICEID_API_LATEST;
	Options.DeviceModel = "PC";
	// *Platform needs a user conversion to EOS_HPlatform. Passing it straight into this extern "C"
	// call makes MSVC report C3861 instead of converting. Copy the handle out first.
	const EOS_HPlatform EosPlatform = *Platform;
	EOS_Connect_CreateDeviceId(EOS_Platform_GetConnectInterface(EosPlatform), &Options, nullptr, &OnDeviceId);
}

DCCOnline::EState DCCOnline::GetState() { return GState; }
bool DCCOnline::IsOnline() { return GState == EState::Online; }
bool DCCOnline::InParty() { return GLobby.IsValid(); }

FString DCCOnline::GetStatusText()
{
	return GState == EState::Off ? FString() : GStatus;
}

void DCCOnline::HostParty(const FString& HostName)
{
	IOnlineServicesPtr Online = Services();
	ILobbiesPtr Lobbies = Online ? Online->GetLobbiesInterface() : nullptr;
	if (!IsOnline() || !Lobbies || GLobby.IsValid())
	{
		return;
	}
	FCreateLobby::Params Params;
	Params.LocalAccountId = GAccount;
	Params.LocalName = PartyName;
	Params.SchemaId = SchemaName;
	Params.MaxMembers = 8;
	Params.JoinPolicy = ELobbyJoinPolicy::PublicAdvertised;
	Params.Attributes.Add(HostAttr, FSchemaVariant(HostName.Left(60)));
	Params.Attributes.Add(PlaceAttr, FSchemaVariant(FString(TEXT("Safe Room"))));
	GHosting = true;
	Lobbies->CreateLobby(MoveTemp(Params)).OnComplete([](const TOnlineResult<FCreateLobby>& Result)
	{
		if (Result.IsOk() && Result.GetOkValue().Lobby)
		{
			GLobby = Result.GetOkValue().Lobby->LobbyId;
			UE_LOG(LogTemp, Log, TEXT("[DCC Online] party listed"));
		}
		else
		{
			GHosting = false;
			UE_LOG(LogTemp, Warning, TEXT("[DCC Online] could not list the party: %s"), Result.IsOk() ? TEXT("no lobby") : *Result.GetErrorValue().GetLogString());
		}
	});
}

void DCCOnline::EndParty()
{
	IOnlineServicesPtr Online = Services();
	ILobbiesPtr Lobbies = Online ? Online->GetLobbiesInterface() : nullptr;
	if (!Lobbies || !GLobby.IsValid())
	{
		return;
	}
	FLeaveLobby::Params Params;
	Params.LocalAccountId = GAccount;
	Params.LobbyId = GLobby;
	GLobby = FLobbyId();
	GHosting = false;
	Lobbies->LeaveLobby(MoveTemp(Params)).OnComplete([](const TOnlineResult<FLeaveLobby>& Result)
	{
		UE_LOG(LogTemp, Log, TEXT("[DCC Online] left the party%s"), Result.IsOk() ? TEXT("") : TEXT(" (EOS said no; it times out on its own)"));
	});
}

void DCCOnline::FindParties(TFunction<void(const TArray<FParty>&, const FString&)> OnDone)
{
	IOnlineServicesPtr Online = Services();
	ILobbiesPtr Lobbies = Online ? Online->GetLobbiesInterface() : nullptr;
	if (!IsOnline() || !Lobbies)
	{
		OnDone({}, GStatus.IsEmpty() ? FString(TEXT("Online is off.")) : GStatus);
		return;
	}
	FFindLobbies::Params Params;
	Params.LocalAccountId = GAccount;
	Params.MaxResults = 20;
	Lobbies->FindLobbies(MoveTemp(Params)).OnComplete([OnDone](const TOnlineResult<FFindLobbies>& Result)
	{
		TArray<FParty> Out;
		GFound.Reset();
		if (!Result.IsOk())
		{
			OnDone(Out, FString::Printf(TEXT("Could not reach Epic (%s)."), *Result.GetErrorValue().GetLogString()));
			return;
		}
		for (const TSharedRef<const FLobby>& Lobby : Result.GetOkValue().Lobbies)
		{
			if (Lobby->OwnerAccountId == GAccount)
			{
				continue; // our own listing
			}
			FParty Party;
			const FString* Host = AttrString(*Lobby, HostAttr);
			const FString* Place = AttrString(*Lobby, PlaceAttr);
			Party.HostName = Host ? *Host : FString(TEXT("Someone"));
			Party.Place = Place ? *Place : FString();
			Party.Players = Lobby->Members.Num();
			Party.MaxPlayers = Lobby->MaxMembers;
			Out.Add(Party);
			GFound.Add(Lobby);
		}
		OnDone(Out, FString());
	});
}

void DCCOnline::JoinParty(int32 Index, TFunction<void(const FString&, const FString&)> OnDone)
{
	IOnlineServicesPtr Online = Services();
	ILobbiesPtr Lobbies = Online ? Online->GetLobbiesInterface() : nullptr;
	if (!IsOnline() || !Lobbies || !GFound.IsValidIndex(Index))
	{
		OnDone(FString(), TEXT("That party is gone. Refresh the list."));
		return;
	}
	EndParty();
	FJoinLobby::Params Params;
	Params.LocalAccountId = GAccount;
	Params.LocalName = PartyName;
	Params.LobbyId = GFound[Index]->LobbyId;
	Lobbies->JoinLobby(MoveTemp(Params)).OnComplete([OnDone](const TOnlineResult<FJoinLobby>& Result)
	{
		IOnlineServicesPtr Online2 = Services();
		if (!Result.IsOk() || !Result.GetOkValue().Lobby || !Online2)
		{
			OnDone(FString(), FString::Printf(TEXT("Could not join (%s)."), Result.IsOk() ? TEXT("no lobby") : *Result.GetErrorValue().GetLogString()));
			return;
		}
		GLobby = Result.GetOkValue().Lobby->LobbyId;
		FGetResolvedConnectString::Params Connect;
		Connect.LocalAccountId = GAccount;
		Connect.LobbyId = GLobby;
		TOnlineResult<FGetResolvedConnectString> Address = Online2->GetResolvedConnectString(MoveTemp(Connect));
		if (!Address.IsOk())
		{
			OnDone(FString(), FString::Printf(TEXT("Joined, but no address to travel to (%s)."), *Address.GetErrorValue().GetLogString()));
			return;
		}
		OnDone(Address.GetOkValue().ResolvedConnectString, FString());
	});
}
