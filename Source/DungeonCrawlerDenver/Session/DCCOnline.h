#pragma once

#include "CoreMinimal.h"

/**
 * Epic Online Services: the party list and the relay (2026-11-25).
 *
 * Every PC signs in by itself the first time the game starts, with an anonymous EOS id made for that device (no Epic
 * account, no login screen). Hosting a party lists it with EOS; JOIN A PARTY shows every open party as a button; the
 * game then runs over EOS's relay, so nobody forwards ports or installs Tailscale. When EOS is not signed in (no
 * internet, Epic down, no Client Secret in DefaultEngine.ini) the game net driver falls back to plain IP and the old
 * invite codes (Session/DCCSession) still work.
 *
 * Built on the engine's Online Services plugin for EOS Game Services (OnlineServicesEOSGS) and its EOS net driver
 * (SocketSubsystemEOS, see DefaultEngine.ini). Settings: [OnlineServices.EOS] and [OnlineServices.Lobbies].
 */
namespace DCCOnline
{
	enum class EState : uint8
	{
		Off,        // not started yet
		SigningIn,
		Online,     // signed in: parties work
		Failed      // see GetStatusText(); invites still work
	};

	/** Signs this PC in, once. Safe to call every time the main menu opens. */
	void Start();

	EState GetState();
	bool IsOnline();

	/** One line for the menu: "Online. Friends can find your party." or why not. */
	FString GetStatusText();

	/** Lists this PC's party with EOS so friends see it under JOIN A PARTY. Call when hosting starts. */
	void HostParty(const FString& HostName);

	/** Stops listing (host) or leaves (guest) the party, if any. Called whenever the main menu opens. */
	void EndParty();

	/** True while this PC lists or is in a party. */
	bool InParty();

	struct FParty
	{
		FString HostName;
		FString Place;
		int32 Players = 0;
		int32 MaxPlayers = 0;
	};

	/** Asks EOS for open parties. OnDone gets them (or an error) on the game thread. */
	void FindParties(TFunction<void(const TArray<FParty>& Parties, const FString& Error)> OnDone);

	/**
	 * Joins party Index from the last FindParties. OnDone gets the address to travel to ("[EOS:...]") or an error.
	 * Travel with DCCSession::Join so the crawler, password and build check go along as usual.
	 */
	void JoinParty(int32 Index, TFunction<void(const FString& Address, const FString& Error)> OnDone);
}
