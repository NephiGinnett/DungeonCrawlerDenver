#pragma once

#include "CoreMinimal.h"

class UWorld;
class APlayerController;

/**
 * Hosting and joining, for the party menu (UI/DCCSessionMenuWidget).
 *
 * The model is Docs/multiplayer-plan.md's: Zoe hosts a listen server, friends
 * connect straight to her address, over the same Wi-Fi or over Tailscale.
 * There is no matchmaking and no online subsystem, so nobody needs an account.
 *
 * An invite code is just the host's IPv4 address and port, packed into ten
 * letters ("K7M2Q-9TB4X") so it can be read out or pasted without mistakes.
 * The Join box accepts a code or a plain address.
 */
namespace DCCSession
{
	constexpr int32 DefaultPort = 7777;

	struct FHostAddress
	{
		/** "Same Wi-Fi", "Tailscale", "This PC". */
		FString Label;
		/** "192.168.1.20:7777" */
		FString Address;
		/** "K7M2Q-9TB4X" */
		FString InviteCode;
	};

	/** Every address friends could reach this machine on, best first. Tailscale before LAN. */
	TArray<FHostAddress> GetHostAddresses(int32 Port = DefaultPort);

	FString EncodeInvite(uint32 IPv4, int32 Port);

	/** A code becomes "a.b.c.d:port". Anything else is returned trimmed, as an address. */
	FString ResolveJoinTarget(const FString& CodeOrAddress);

	/**
	 * Opens the running game to other players without reloading the level, so
	 * nothing in progress is lost. Falls back to reopening the map with ?listen.
	 */
	bool StartHosting(UWorld* World, APlayerController* LocalPlayer, FString& OutError);

	/**
	 * The invite code (or IP address) inside any text, such as a whole pasted
	 * "Join my party..." message. Empty if there is none.
	 */
	FString FindInviteIn(const FString& Text);

	/** Joins whatever invite is on the clipboard. False (and nothing happens) if there is none. */
	bool JoinFromClipboard(APlayerController* LocalPlayer, FString& OutTarget);

	/** The message a host sends friends: says where to click, ends with the code. */
	FString InviteMessage(const FString& InviteCode);

	/** Travels to the host, telling it which crawler we want to be. */
	void Join(APlayerController* LocalPlayer, const FString& CodeOrAddress);

	/** Disconnects from the host (client) and returns to the default map. */
	void Leave(APlayerController* LocalPlayer);

	/** Remembered between launches, in GameUserSettings.ini. */
	FName GetPreferredCrawler();
	void SetPreferredCrawler(FName CrawlerId);
	FString GetLastJoinTarget();

	/**
	 * The password hash this PC remembers for a crawler (DCCProfile::HashPassword), so a returning player types
	 * it once. It is what Join sends the host. Empty if none was ever entered here.
	 */
	FString GetSavedPasswordHash(FName CrawlerId);
	void SavePasswordHash(FName CrawlerId, const FString& Hash);

	/**
	 * Menu guides (the explainer window on first opening a menu page, the first-time hints) show once per player,
	 * ever, not once per session (2026-11-14). Remembered on this PC, per crawler.
	 */
	bool HasSeenGuide(const FString& PlayerKey, FName Guide);
	void MarkGuideSeen(const FString& PlayerKey, FName Guide);

	/**
	 * Remembers CrawlerId and, where this machine owns the server (solo or
	 * host), makes the local player that crawler now: body, nameplate and
	 * player name. The name is what the next level and every arcade game use to
	 * find a player's saves, so changing the body alone would swap back on the
	 * next travel. The old crawler is saved first. False on a client, where the
	 * choice applies at the next join.
	 */
	bool SwitchCrawler(APlayerController* LocalPlayer, FName CrawlerId);

	/** The six crawlers, in menu order. */
	const TArray<FName>& Roster();

	/** Why the last connection failed, if it did. Cleared once read. */
	FString TakeLastError();
	bool HasPendingError();

	/** Hooks the engine's network and travel failure events. Safe to call more than once. */
	void EnsureFailureHandlers();
}
