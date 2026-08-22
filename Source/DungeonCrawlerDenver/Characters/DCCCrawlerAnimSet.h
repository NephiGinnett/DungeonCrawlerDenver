#pragma once

#include "CoreMinimal.h"

class ADCCEnemyCharacter;

// ---------------------------------------------------------------------------
// THE SHARED-SKELETON ANIMATION SET
//
// Every clip named here is authored against `SK_DCCCrawler_Skeleton`, the single
// skeleton all four Meshy crawlers were consolidated onto. Anything wearing that
// skeleton can play any of them, which is the entire payoff of the consolidation.
//
// Extracted out of `DCCAllyCharacters.cpp`, where it started life, because the
// Voldemortgage/DebtEater/TestBoss classes are reskins of the *same* skeleton and
// were shipping with every animation slot empty - so they would have stood
// perfectly still through their own boss fight. That is the identical
// unset-AttackAnimation failure that made ordinary combat look broken earlier,
// and it fails silently: `Multicast_PlayAttack` no-ops on a null animation.
//
// DELIBERATELY NOT applied on the `ADCCEnemyCharacter` base class. That base is
// also the parent of the Brute/Analyst/Courier mobs, which are still on the
// Mannequin skeleton - and playing a crawler clip on a Mannequin mesh is the
// `SkeletonToCompactPose.IsValidIndex` ensure hit earlier this session. Callers
// opt in per class, which keeps the two skeletons from ever meeting.
// ---------------------------------------------------------------------------

namespace DCCCrawlerAnimSet
{
	/**
	 * Fills every animation slot on a character built from the shared crawler
	 * skeleton: locomotion, attack, cast, climb, fist bump and the emote pool.
	 *
	 * A free function rather than a base-class method because the classes that
	 * call it have no relationship worth expressing in the type hierarchy - they
	 * just happen to share a skeleton.
	 *
	 * @param EmoteChanceOnKill  0 suppresses celebration entirely. Worth doing for
	 *                           anything whose death is supposed to land as a beat
	 *                           rather than a dance.
	 */
	void Apply(ADCCEnemyCharacter& Character, float EmoteChanceOnKill = 0.6f);
}
