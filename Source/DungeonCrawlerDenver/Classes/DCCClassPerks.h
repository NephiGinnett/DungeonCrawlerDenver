#pragma once

#include "CoreMinimal.h"

class ADCCCharacterBase;
struct FDCCSkillDef;

/**
 * Class perk trees (2026-11-22, Zoe: "make sure our skill tree options are humorous, in the tone of dungeon crawler
 * carl / the TTRPG"). Each basic class has four tiers, unlocked at levels 4, 8, 12 and 16, and each tier is a choice
 * between two perks. Most perks are a trade: something gets better, sometimes something gets worse. The blurb is the
 * System AI's commentary.
 *
 * The picks live on UDCCClassComponent::Perks (saved), as perk ids. Ids carry the class index, so a crawler who
 * changes class keeps the old picks on file and they simply stop counting.
 *
 * Every number is a percent except KillHeal (hit points per kill). Mana and Cooldown are changes to the cost and the
 * wait, so negative is good there. Taken is damage removed, so positive is good.
 */
namespace DCCClassPerks
{
	struct FPerk
	{
		const TCHAR* Name;
		const TCHAR* Blurb;
		int32 Melee = 0;
		int32 Ranged = 0;
		int32 Spell = 0;
		int32 Heal = 0;
		int32 Taken = 0;
		int32 Mana = 0;
		int32 Cooldown = 0;
		int32 Radius = 0;
		int32 KillHeal = 0;
		int32 Xp = 0;
	};

	constexpr int32 TierCount = 4;
	int32 TierLevel(int32 Tier); // 4, 8, 12, 16

	/** The two options of a tier for a class (index into DCCBasicClasses::All()). Null when out of range. */
	const FPerk* Get(int32 ClassIndex, int32 Tier, int32 Option);
	/** "C2T1O0": class 2, tier 1, option 0. */
	FName IdFor(int32 ClassIndex, int32 Tier, int32 Option);
	/** The option picked at a tier for this crawler's current class, or -1. */
	int32 Picked(const ADCCCharacterBase* Crawler, int32 Tier);
	/** Tiers unlocked by level and not picked yet. */
	int32 Unpicked(const ADCCCharacterBase* Crawler);

	/** "Melee +15%   Heals -10%" */
	FString EffectLine(const FPerk& Perk);

	/** The sum of the crawler's picked perks for one field. */
	int32 Sum(const ADCCCharacterBase* Crawler, int32 FPerk::* Field);

	/** Mana cost, cooldown and area of a skill after the crawler's perks. Applied wherever a skill is looked up. */
	void AdjustSkill(const ADCCCharacterBase* Crawler, FDCCSkillDef& Skill);
}
