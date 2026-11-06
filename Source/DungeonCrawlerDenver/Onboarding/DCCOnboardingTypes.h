#pragma once

#include "CoreMinimal.h"
#include "Engine/DataTable.h"
#include "DCCOnboardingTypes.generated.h"

class ADCCEnemyCharacter;
class USoundBase;

// ---------------------------------------------------------------------------
// THE FIRST TEN MINUTES
//
// Each crawler goes through these stages on their own:
//
//   Waking        - in their own bedroom, no HUD at all, the room is the tutorial
//   ItemFound     - picked up their personal item (CJ's board game, Scott's stick)
//   Announced     - the System has told the planet about the World Dungeon,
//                   and a giant arrow shows them where it is
//   InDungeon     - walked through the entrance and landed in a solo trial room
//   FirstFightWon - beat the random encounter rolled for them
//   SafeRoom      - reached the safe room, where the HUD gets issued
//   Complete      - every menu unlocked; normal play from here on
//
// The stages are per crawler, not party-wide, because five people never wake
// up in the same order. A shared gate would leave four of them waiting.
// The one place the party lines up again is the safe room.
// ---------------------------------------------------------------------------

UENUM(BlueprintType)
enum class EDCCOnboardingStage : uint8
{
	Waking,
	ItemFound,
	Announced,
	InDungeon,
	FirstFightWon,
	SafeRoom,
	Complete
};

/**
 * HUD elements and menu pages that onboarding can withhold. A bitmask, not a
 * set of bools, so a single DataTable column can unlock several at once.
 *
 * Kept coarse on purpose. Each value is something a player would describe as
 * one thing ("the health bar", "the skills page"), because the tutorial
 * window that unlocks it has to explain it in two sentences.
 */
UENUM(BlueprintType, meta = (Bitflags, UseEnumValuesAsMaskValuesInEditor = "true"))
enum class EDCCHudFeature : uint8
{
	None             = 0 UMETA(Hidden),
	Vitals           = 1 << 0,  // health, mana, stamina, nearby enemy bars
	Hotbar           = 1 << 1,
	QuestTracker     = 1 << 2,
	EventFeed        = 1 << 3,  // the dice log in the corner
	MenuCharacter    = 1 << 4,  // Tab: the character sheet (also unlocks Tab itself)
	MenuInventory    = 1 << 5,  // inventory + equipment
	MenuSkills       = 1 << 6,
	MenuSocial       = 1 << 7,  // party, talk, allies, whispers, chatter, achievements
};
ENUM_CLASS_FLAGS(EDCCHudFeature);

namespace DCCHud
{
	constexpr int32 AllFeatures = 0xFF;
	inline bool Has(int32 Mask, EDCCHudFeature Feature) { return (Mask & static_cast<int32>(Feature)) != 0; }
}

UENUM(BlueprintType)
enum class EDCCSystemWindowStyle : uint8
{
	/** Cyan. The default "the System would like a word" window. */
	Info,
	/** Gold. Quests, unlocks, rewards. */
	Reward,
	/** Red. Danger, warnings, the arrival of something with teeth. */
	Warning
};

/**
 * One isekai-style System window: the bright blue pop-up that tells a crawler
 * what just happened to them. Rows live in DT_DCCSystemWindows, so the
 * wording, the voice line and what each window unlocks can all change
 * without a recompile.
 */
USTRUCT(BlueprintType)
struct FDCCSystemWindowDef : public FTableRowBase
{
	GENERATED_BODY()

	/** Small caps line above the title. "NOTIFICATION", "QUEST", "WARNING". */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "SystemWindow")
	FText Header;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "SystemWindow")
	FText Title;

	/** Word-wrapped. Use | to force a paragraph break. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "SystemWindow")
	FText Body;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "SystemWindow")
	EDCCSystemWindowStyle Style = EDCCSystemWindowStyle::Info;

	/** The System AI reading it out. Optional; a window with no voice is just quieter. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "SystemWindow")
	TSoftObjectPtr<USoundBase> Voice;

	/** EDCCHudFeature bits granted when this window is shown. 0 for none. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "SystemWindow")
	int32 UnlockFeatures = 0;
};

/** One enemy type in a rolled encounter. */
USTRUCT(BlueprintType)
struct FDCCRandomEncounterSpawn
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Encounter")
	TSubclassOf<ADCCEnemyCharacter> EnemyClass;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Encounter", meta = (ClampMin = "1"))
	int32 Count = 1;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Encounter", meta = (ClampMin = "1"))
	int32 Level = 1;

	/**
	 * Renames the enemy for this encounter. Lets one model play several
	 * jokes: the slime is a Kombucha Mother in one roll and a Hot Tub
	 * Biofilm in another. Empty keeps the class's own name.
	 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Encounter")
	FText DisplayNameOverride;
};

/**
 * One row of the first-fight table. Weighted, so a rare joke can sit beside
 * a common one without either crowding the other out.
 */
USTRUCT(BlueprintType)
struct FDCCRandomEncounterRow : public FTableRowBase
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Encounter")
	FText EncounterName;

	/** What the System says when the fight starts, in a red Warning window. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Encounter")
	FText IntroLine;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Encounter")
	TSoftObjectPtr<USoundBase> IntroVoice;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Encounter", meta = (ClampMin = "0.0"))
	float Weight = 1.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Encounter")
	TArray<FDCCRandomEncounterSpawn> Spawns;
};

/**
 * The payload a client actually receives. Resolved on the server from a row
 * name, then sent whole, so a client never needs the DataTable loaded and a
 * trigger can build a one-off window that is not in any table.
 */
USTRUCT(BlueprintType)
struct FDCCSystemWindowPayload
{
	GENERATED_BODY()

	UPROPERTY(BlueprintReadOnly, Category = "SystemWindow")
	FText Header;

	UPROPERTY(BlueprintReadOnly, Category = "SystemWindow")
	FText Title;

	UPROPERTY(BlueprintReadOnly, Category = "SystemWindow")
	FText Body;

	UPROPERTY(BlueprintReadOnly, Category = "SystemWindow")
	EDCCSystemWindowStyle Style = EDCCSystemWindowStyle::Info;

	UPROPERTY(BlueprintReadOnly, Category = "SystemWindow")
	TSoftObjectPtr<USoundBase> Voice;
};
