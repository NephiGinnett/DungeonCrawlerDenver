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
