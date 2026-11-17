#pragma once

#include "CoreMinimal.h"

/**
 * Fourteener's items (2026-11-13). Found in supply chests on the ledges, all of them
 * in the Outfitter's Crate at Fort Tiner, and artifacts in rare caches. Nothing here
 * is Dungeon Crawler Denver gear: it lives for the round (tools and boosts) or goes
 * into the arcade profile's trophy vault (artifacts).
 *
 *   Tool      kept, used from its belt slot
 *   Boost     works the moment it is picked up
 *   Artifact  carried; lost if you die; banked as a trophy if you are alive when the
 *             round ends. Some sell at the Den's Prize Counter.
 */
namespace DCCClimbItems
{
	enum class EKind : uint8 { Tool, Boost, Artifact };

	struct FItem
	{
		FName Id;
		const TCHAR* Name;
		const TCHAR* Blurb;
		EKind Kind;
		/** Tickets at the Prize Counter, for artifacts that sell. 0 = a keepsake, not for sale. */
		int32 SellValue = 0;
		FLinearColor Colour;
		/** Hunger this food restores (0-100; 0 = not food). Food is a tool: its belt slot eats it. */
		float Food = 0.0f;
	};

	const TArray<FItem>& All();
	const FItem* Find(FName Id);

	/** Tickets at Fort Tiner's Trading Post (2026-11-15). 0 = not sold there (artifacts). */
	int32 SupplyPrice(FName Id);

	/** What a ledge chest can hold: tools and boosts only, every one helpful for the climb. */
	TArray<FName> ChestPool();
	/** What an artifact cache can hold. */
	TArray<FName> ArtifactPool();
	/** Food only (food caches on the ledges, 2026-11-13). */
	TArray<FName> FoodPool();

	// Ids the arena checks for.
	extern const FName GrapplingHook, Piton, TrailMix, ChalkBag, Crampons, WindCloak, HardHat, SafetyLine;
	extern const FName GranolaBar, GreenChileBurrito, Jerky, Apple;
	extern const FName CeremonialKnife, PlatinumOre, AssayLedger, ClaimLantern;
}
