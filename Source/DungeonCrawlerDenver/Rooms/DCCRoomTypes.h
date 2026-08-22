#pragma once

#include "CoreMinimal.h"
#include "DCCRoomTypes.generated.h"

/**
 * The reusable room templates from the design doc. Floors are assembled from
 * these rather than hand-built per node, so a whole floor stays achievable solo.
 */
UENUM(BlueprintType)
enum class EDCCRoomType : uint8
{
	Combat     UMETA(DisplayName = "Combat"),
	Puzzle     UMETA(DisplayName = "Puzzle"),
	LootVault  UMETA(DisplayName = "Loot Vault"),
	Narrative  UMETA(DisplayName = "Narrative"),
	SafeRoom   UMETA(DisplayName = "Safe Room"),

	MAX        UMETA(Hidden)
};

UENUM(BlueprintType)
enum class EDCCEncounterState : uint8
{
	/** Built but not yet triggered. Enemies do not exist yet. */
	Dormant  UMETA(DisplayName = "Dormant"),

	Active   UMETA(DisplayName = "Active"),

	/** Every spawned enemy is dead. */
	Cleared  UMETA(DisplayName = "Cleared"),

	MAX      UMETA(Hidden)
};

/** One of the four grid-aligned exits a generated room can open. */
UENUM(BlueprintType)
enum class EDCCCardinalDirection : uint8
{
	North UMETA(DisplayName = "North"),
	East  UMETA(DisplayName = "East"),
	South UMETA(DisplayName = "South"),
	West  UMETA(DisplayName = "West"),

	MAX   UMETA(Hidden)
};

namespace DCCCardinal
{
	/** Grid-space step (X,Y in cell units) for walking one cell in Direction. */
	inline FIntPoint Offset(EDCCCardinalDirection Direction)
	{
		switch (Direction)
		{
		case EDCCCardinalDirection::North: return FIntPoint(0, 1);
		case EDCCCardinalDirection::East:  return FIntPoint(1, 0);
		case EDCCCardinalDirection::South: return FIntPoint(0, -1);
		case EDCCCardinalDirection::West:  return FIntPoint(-1, 0);
		default:                           return FIntPoint(0, 0);
		}
	}

	/** The direction a neighbour sees this connection as, looking back. */
	inline EDCCCardinalDirection Opposite(EDCCCardinalDirection Direction)
	{
		switch (Direction)
		{
		case EDCCCardinalDirection::North: return EDCCCardinalDirection::South;
		case EDCCCardinalDirection::East:  return EDCCCardinalDirection::West;
		case EDCCCardinalDirection::South: return EDCCCardinalDirection::North;
		case EDCCCardinalDirection::West:  return EDCCCardinalDirection::East;
		default:                           return EDCCCardinalDirection::North;
		}
	}
}
