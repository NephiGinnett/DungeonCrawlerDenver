#pragma once

#include "CoreMinimal.h"

class ADCCCharacterBase;

/**
 * Starter kits (2026-11-16, Zoe: "let them choose which starter set they want (magic, tank, healer, etc) out of 3-5
 * options and give them a good weapon, piece of equipment, and maybe a few tickets or a potion"). Offered once per
 * crawler, the first time they enter the World Dungeon (ADCCTaleFloor::JoinRun). The choice is a profile stat
 * ("Dungeon.StarterKit", 1-based), so it follows the crawler between PCs with the rest of the profile.
 */
namespace DCCStarterKits
{
	struct FKit
	{
		const TCHAR* Name;
		const TCHAR* Blurb;
		FLinearColor Colour;
		TArray<FName> Gear;        // DT_DCCEquipment ids, equipped straight away
		TArray<FName> Skills;      // granted at Rank 1
		FName Consumable;          // DT_DCCConsumables id
		int32 ConsumableCount = 0;
		int32 Tickets = 25;
	};

	const TArray<FKit>& All();

	/** 0 if this crawler has not chosen yet, else the kit's index + 1. Server (the host's profile). */
	int32 Chosen(const ADCCCharacterBase* Crawler);

	/** Server: hand over kit Index (0-based). False if the crawler already chose. */
	bool Grant(ADCCCharacterBase* Crawler, int32 Index);
}
