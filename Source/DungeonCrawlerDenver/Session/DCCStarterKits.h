#pragma once

#include "CoreMinimal.h"

class ADCCCharacterBase;

/**
 * Starter kits (2026-11-16). Offered once per crawler, the first time they enter the World Dungeon.
 * The choice is a profile stat ("Dungeon.StarterKit", 1-based). The kit IS the basic class
 * (Classes/DCCBasicClasses): one Denver job per minigame, same index, same order. Do not reorder.
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
