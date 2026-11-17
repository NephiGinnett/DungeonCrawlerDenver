#pragma once

#include "CoreMinimal.h"

/**
 * The Chop Shop (2026-11-14, Zoe: "a few upgrades you can make from 'the chop shop' using points similar to the
 * arcade tickets").
 *
 * Permanent upgrades for your truck, bought with SCRAP. Scrap is the driving games' own currency, kept apart from
 * tickets the same way tickets are kept apart from DCD gold: Repo Rally pays it per drop and per shift, Neon Mile
 * pays it per distance. Tickets buy looks at the Prize Counter; scrap buys performance here.
 *
 * Levels live in the arcade profile (DCCProfile, "Chop.<Id>") and the balance in "Chop.Scrap", on the host.
 * UDCCWalletComponent mirrors both for the shop screen. ADCCArcadeKart reads the levels when it seats a driver.
 */
namespace DCCChopShop
{
	struct FUpgrade
	{
		FName Id;
		const TCHAR* Name;
		/** What one level does, in plain words. */
		const TCHAR* Blurb;
		/** "-15% damage" per level, for the level pips. */
		const TCHAR* PerLevel;
		int32 MaxLevel = 3;
		/** Scrap for level 1; each next level costs this again on top (1x, 2x, 3x). */
		int32 BasePrice = 40;
		FLinearColor Colour;
		/** Mine Outfitters (Dead Air) rather than the truck: paid in CREDITS, not scrap (2026-11-15). */
		bool bMine = false;
		/** Fourteener gear at the Trading Post: paid in TICKETS (2026-11-16). */
		bool bTickets = false;
	};

	extern const FName Plating, Tank, Engine, Line, PitCrew;
	// Mine Outfitters: kit for Dead Air, bought with credits from footage, notes and finished stories.
	extern const FName AirTank, MineLamp, Lightsticks, HardHat, Harness, GasMeter, TapeReel;
	// Fourteener, at the Trading Post, for tickets.
	extern const FName Grapple;
	/** The belt grappling hook (2026-11-16, Zoe: "always on your belt and just on cool down after use and gets better
	 *  with upgrades"): how far it yanks you up the wall, and how long it takes to wind back in. */
	inline float GrappleReach(int32 Level) { return 600.0f + 200.0f * Level; }
	inline float GrappleCooldown(int32 Level) { return 40.0f - 8.0f * Level; }

	/** How deep (0 at the cage, 1 at the deepest stope) your air reaches. Level 0 is about half way. */
	inline float AirReach(int32 Level) { return FMath::Min(1.05f, 0.45f + 0.2f * Level); }
	/** Lightsticks you carry into a round. */
	inline int32 StickCount(int32 Level) { return 2 + 2 * Level; }
	/** Rock, cart and blast damage taken. */
	inline float HardHatDamage(int32 Level) { return 1.0f - 0.3f * Level; }
	/** Extra seconds of tape. */
	inline float TapeBonus(int32 Level) { return 30.0f * Level; }

	int32 GetCredits(const FString& Key);
	int32 AddCredits(const FString& Key, int32 Delta);

	const TArray<FUpgrade>& All();
	const FUpgrade* Find(FName Id);
	/** Scrap to go from Level to Level + 1. 0 when already maxed. */
	int32 PriceFor(const FUpgrade& Upgrade, int32 Level);

	// What the levels do. One place, so the shop text and the game agree.
	inline float DamageTaken(int32 Level) { return 1.0f - 0.20f * Level; }
	inline float FuelBurn(int32 Level) { return 1.0f - 0.20f * Level; }
	/** Litres the truck starts with and can hold. Level 0 is the stock 100. */
	inline float TankCapacity(int32 Level) { return 100.0f + 30.0f * FMath::Max(0, Level); }
	inline float Pace(int32 Level) { return 1.0f + 0.15f * Level; }
	inline float HookLine(int32 Level) { return 1.0f + 0.25f * Level; }
	inline float PitSpeed(int32 Level) { return 1.0f + 0.50f * Level; }

	// The balance and levels, on the host's profile, by player save key.
	int32 GetScrap(const FString& Key);
	int32 AddScrap(const FString& Key, int32 Delta);
	int32 GetLevel(const FString& Key, FName UpgradeId);
	/** Buys the next level if the scrap covers it. OutMessage says what happened either way. */
	bool BuyNext(const FString& Key, FName UpgradeId, FString& OutMessage);
}
