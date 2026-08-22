#pragma once

#include "CoreMinimal.h"
#include "Characters/DCCEnemyCharacter.h"
#include "DCCAllyCharacters.generated.h"

// ---------------------------------------------------------------------------
// CJ, Scott, Kyle and Marcel's mesh/animation defaults, moved into C++.
//
// This exists because `ObjectTools.set_properties` cannot write Blueprint CDO
// properties - confirmed repeatedly this session - so `BP_DCCEnemy_HockeyWings`
// and friends could not be repointed to the shared skeleton and its animation
// set by automation. Reparenting each Blueprint to one of these (via
// `BlueprintTools.set_parent`) plus `ObjectTools.reset_properties` to clear the
// old baked-in mesh override lets the Blueprint inherit these defaults cleanly,
// which is a real, working path `set_properties` is not.
//
// DELIBERATELY NOT on the shared `ADCCEnemyCharacter` base. These clips are
// authored against `SK_DCCCrawler_Skeleton`; `ADCCEnemyCharacter` is also the
// parent of the Brute/Analyst/Courier mobs, which are still on the Mannequin
// skeleton (`SK_Mannequin`). Playing a crawler-skeleton clip on a Mannequin
// mesh is exactly the `SkeletonToCompactPose.IsValidIndex` ensure hit earlier
// this session - so each of these is its own class, one per crawler.
//
// The animation set is deliberately UNIFORM across all four - same walk, same
// run, same attack, same emote pool - because that was the explicit ask once
// the skeleton consolidation made it possible. It now lives in
// `DCCCrawlerAnimSet.h`, shared with the boss reskins on the same skeleton.
//
// SCOPE: animation and mesh ONLY. These classes deliberately do not touch
// `StartingSkillIds` or `SkillTable`, because the four Blueprints already carry
// curated per-character loadouts (CJ has FireFingers + MediaAnalyst at Rank 2;
// Kyle has ParkingLotSpecial + QuickWit; Marcel has MartialArts +
// FirstImpressions) and a Blueprint's explicitly-authored value wins over a C++
// CDO default. Setting them here could only ever be inert or wrong. The
// animation slots are the opposite case - every one of them reads `None` on all
// four Blueprints, which is the default, so these defaults do reach them. That
// asymmetry is the whole reason this class works at all.
// ---------------------------------------------------------------------------

/** CJ. Mesh unchanged - SK_HockeyWings was already on the shared skeleton. */
UCLASS()
class DUNGEONCRAWLERDENVER_API ADCCAlly_HockeyWings : public ADCCEnemyCharacter
{
	GENERATED_BODY()

public:
	ADCCAlly_HockeyWings();
};

/** Scott. */
UCLASS()
class DUNGEONCRAWLERDENVER_API ADCCAlly_OfficeAirplane : public ADCCEnemyCharacter
{
	GENERATED_BODY()

public:
	ADCCAlly_OfficeAirplane();
};

/** Kyle. */
UCLASS()
class DUNGEONCRAWLERDENVER_API ADCCAlly_ArmsOut : public ADCCEnemyCharacter
{
	GENERATED_BODY()

public:
	ADCCAlly_ArmsOut();
};

/** Marcel. */
UCLASS()
class DUNGEONCRAWLERDENVER_API ADCCAlly_DiceWinged : public ADCCEnemyCharacter
{
	GENERATED_BODY()

public:
	ADCCAlly_DiceWinged();
};
