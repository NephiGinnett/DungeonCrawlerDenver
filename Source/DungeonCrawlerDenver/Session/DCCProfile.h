#pragma once

#include "CoreMinimal.h"
#include "GameFramework/SaveGame.h"
#include "DCCProfile.generated.h"

class APlayerState;
class UWorld;

/**
 * A player's records across every game in the arcade (2026-11-13).
 *
 * Kept apart from the Dungeon Crawler Denver character save (UDCCCrawlerSave,
 * levels and gear) on purpose: each game owns its own numbers, so the kart
 * racer can never touch your inventory and the dungeon can never reset your
 * best lap. One file per player on the host: Saved/SaveGames/Profile_<Name>.sav.
 *
 * Stats are "Game.Stat" -> number, e.g. "Crawler.BestDepth", "Kart.BestLap",
 * "Climb.BestHeight", "Neon.BestDistance", plus "<Game>.Plays".
 */
UCLASS()
class DUNGEONCRAWLERDENVER_API UDCCProfileSave : public USaveGame
{
	GENERATED_BODY()

public:
	UPROPERTY()
	FString PlayerKey;

	UPROPERTY()
	TMap<FString, float> Stats;

	/** Arcade cosmetics bought in the Den shop (DCCCosmetics ids). Never DCD gear. */
	UPROPERTY()
	TArray<FName> Cosmetics;

	/** What is being worn: slot name -> cosmetic id. */
	UPROPERTY()
	TMap<FName, FName> Wearing;

	/** Artifacts brought home from the arcade games (DCCClimbItems), and how many. */
	UPROPERTY()
	TMap<FName, int32> Trophies;

	/**
	 * DCCProfile::HashPassword of this crawler's password (2026-11-14). Empty means it was never changed, which is
	 * the base password every crawler starts with ("bidet").
	 */
	UPROPERTY()
	FString PasswordHash;

	/**
	 * Bumped every time a host merges a visiting crawler's copy (2026-11-15, profile sync). A copy whose revision is
	 * older than the one the crawler last saw elsewhere is stale, and the crawler's own copy replaces it.
	 */
	UPROPERTY()
	int32 Revision = 0;
};

namespace DCCProfile
{
	/** The player's save key: their player name, which is their crawler (CJ, Scott...) when they joined as one. */
	FString KeyFor(const APlayerState* PlayerState);

	float Get(const APlayerState* PlayerState, FName Game, FName Stat, float Default = 0.0f);

	/** The same, by save key (a crawler name), for screens shown before anyone has joined. */
	float GetForKey(const FString& Key, FName Game, FName Stat, float Default = 0.0f);

	/** Keeps Value if it beats the stored one (or nothing is stored). True if it was a new best. */
	bool RecordBest(const APlayerState* PlayerState, FName Game, FName Stat, float Value, bool bHigherIsBetter = true);

	void Add(const APlayerState* PlayerState, FName Game, FName Stat, float Delta);

	/** Writes a stat outright, by save key (the Chop Shop's levels and scrap). */
	void SetForKey(const FString& Key, FName Game, FName Stat, float Value);

	// --- Crawler passwords (2026-11-14, Zoe: "password protect their characters so that friends can't hijack
	// them. Set all base passwords to bidet") -------------------------------------
	// The password belongs to the crawler's save, so it lives here, on the host. Only a hash is stored, and only a
	// hash crosses the network: the join URL carries ?Pw=<hash>, and ADCCGameMode::InitNewPlayer refuses a remote
	// player whose hash does not match. Solo and host players are checked at the main menu before they play.

	extern const TCHAR* const BasePassword;
	/** The hash for Password on crawler Key. The key is mixed in, so the same password on two crawlers differs. */
	FString HashPassword(const FString& Key, const FString& Password);
	/** True if Hash is this crawler's password (the base password if it was never changed). */
	bool CheckPassword(const FString& Key, const FString& Hash);
	/** Changes it if OldHash is right. NewHash must not be empty. */
	bool ChangePassword(const FString& Key, const FString& OldHash, const FString& NewHash);

	// --- Tickets and cosmetics (2026-11-13) -------------------------------------
	// Tickets are the arcade's own money: every minigame pays them out, the Den
	// shop takes them. Kept apart from DCD gold, like every other arcade record.

	int32 GetTickets(const FString& Key);
	/** Adds (or, negative, takes away) tickets. Returns the new balance. */
	int32 AddTickets(const FString& Key, int32 Delta);
	/** Takes Price if the balance covers it. */
	bool SpendTickets(const FString& Key, int32 Price);

	TArray<FName> GetCosmetics(const FString& Key);
	void GrantCosmetic(const FString& Key, FName CosmeticId);
	TMap<FName, FName> GetWearing(const FString& Key);
	/** CosmeticId None takes the slot off. */
	void SetWearing(const FString& Key, FName Slot, FName CosmeticId);

	TMap<FName, int32> GetTrophies(const FString& Key);
	void AddTrophy(const FString& Key, FName Id, int32 Count = 1);
	/** Takes one away (selling it). False if there was none. */
	bool TakeTrophy(const FString& Key, FName Id);

	// --- Carrying a crawler's profile between PCs (2026-11-15, Zoe: "will my friends be able to play offline without
	// me or will only I have their save?") -----------------------------------------------------------------------
	// Saves are written by whoever hosts. So a friend's progress used to split in two: their party profile on the
	// host's PC and their solo profile on their own. Now, when a friend joins, their game sends both its current copy
	// and the copy it last received from any host (the "base"). The host adds what changed since that base (tickets,
	// scrap, credits and counters by difference; best records by best; owned items by union) to its own copy, and
	// sends the merged copy back, again every few seconds while it changes. The friend keeps that as current and base.

	/** The profile as bytes (current copy on this PC). Empty if there is none. */
	TArray<uint8> Export(const FString& Key);
	/** The copy this PC last received from a host (ProfileBase_<Key>). Empty if it never has. */
	TArray<uint8> ExportBase(const FString& Key);
	/** Host: fold a visiting crawler's copy into this PC's copy of them. False if the bytes are not their profile. */
	bool MergeFromClient(const FString& Key, const TArray<uint8>& Current, const TArray<uint8>& Base);
	/** Visitor: the host's merged copy becomes this PC's current copy and its base. */
	void AdoptFromHost(const TArray<uint8>& Merged);
	/** True once after each write to Key's profile (the host's cue to send it back to that player). */
	bool TakeDirty(const FString& Key);

	/** "CJ 7   Scott 4" for everyone currently connected, best first. Server only. */
	FString Leaderboard(UWorld* World, FName Game, FName Stat, bool bHigherIsBetter, TFunction<FString(float)> Format);
}
