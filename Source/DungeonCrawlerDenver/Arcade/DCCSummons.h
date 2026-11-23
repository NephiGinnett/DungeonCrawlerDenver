#pragma once

#include "CoreMinimal.h"

class ADCCArcadeArena;
class ADCCCharacterBase;
class ADCCSummonedAlly;
class UDataTable;

/**
 * The summoning tubes in the Den (2026-11-17, Zoe: "tubes that come down from the ceiling ... use tickets to summon an
 * ally (an ai controlled copy of one of the friends at base level) that comes down the tube ... assign each player a
 * rarity based on their popularity score and make the pedestal a fun gacha system where allies are sometimes spawned
 * as rarer varieties that glow slightly or come with different weapons").
 *
 *  - A pull costs Price tickets at a tube's podium (ADCCArcadeLobby, the south wall).
 *  - Who comes down: one of the friends (DCCSession::Roster), never yourself. Each friend has a rarity from their
 *    popularity on the host's profiles (Dead Air views plus wins): the most popular is Legendary, then Epic, two Rare,
 *    the rest Common. A pull rolls the rarity first (TierWeights), then a friend of that rarity.
 *  - Variants (2026-11-17, Zoe: "shining variants and shadow versions, just the regular models but with a shiny
 *    overlay"): Shiny (a glowing overlay in the rarity's colour; likelier the rarer the friend), Shadow (a dark overlay
 *    with lit edges and a violet light), and, on top of either, Armed (comes holding a weapon).
 *  - The ally is a level 1 copy of that friend. It follows whoever summoned it, in the Safe Room and into every
 *    game, until it goes down. At most MaxAllies each. Kept in memory on the host for as long as the party plays.
 */
namespace DCCSummons
{
	constexpr int32 Price = 100;
	/** Ten at once for the price of nine, with at least one Rare or better (2026-11-22). */
	constexpr int32 TenPrice = Price * 9;
	constexpr int32 MaxAllies = 2;
	constexpr int32 TierCount = 4;

	const TCHAR* TierName(uint8 Tier);
	FLinearColor TierColour(uint8 Tier);

	/** 0 plain, 1 Shiny, 2 Shadow. */
	enum EVariant : uint8 { Plain = 0, Shiny = 1, Shadow = 2 };
	const TCHAR* VariantName(uint8 Variant);
	constexpr float ShadowChance = 0.10f;

	struct FSummon
	{
		FName Friend;
		uint8 Tier = 0;
		uint8 Variant = Plain;
		/** A weapon it carries (the Armed variant), or none. */
		FName Weapon;
	};

	/** Each friend's rarity, in DCCSession::Roster() order, from the host's profiles. Server. */
	void ComputeTiers(TArray<uint8>& OutTiers);
	/** The chance of each friend in a pull by Summoner (0 for the summoner), in Roster() order. */
	TArray<float> FriendOdds(const TArray<uint8>& Tiers, const FString& Summoner);
	/** The Shiny chance for a friend of this rarity, and the Armed chance. */
	float ShinyChance(uint8 Tier);
	constexpr float ArmedChance = 0.18f;

	/** Server: roll one pull. */
	FSummon Roll(const TArray<uint8>& Tiers, const FString& Summoner, const UDataTable* EquipmentTable);

	/** Server: everyone's allies, by player name. */
	TMap<FString, TArray<FSummon>>& Party();
	/** Server: an ally that is already standing in this world for Player (the one that just came down a tube). */
	void Track(UWorld* World, const FString& Player, ADCCSummonedAlly* Ally);

	/** Server: spawn the ally for Summon at Where, following Owner. Descends from DropHeight above Where first when > 0. */
	ADCCSummonedAlly* SpawnAlly(UWorld* World, const FSummon& Summon, ADCCCharacterBase* Owner, const FVector& Where, float DropHeight,
		const FVector& Exit = FVector(0.0f, 1.0f, 0.0f));

	/** Server. Spawn each player's summoned allies beside them, in whatever game they just entered. Safe to call often. */
	void BringAlong(UWorld* World);
	/** Server, every arena tick. */
	void TickArena(ADCCArcadeArena* Arena);
}
