#pragma once

#include "CoreMinimal.h"

class ADCCCharacterBase;

/**
 * Basic classes (2026-11-20). The starter kit IS the class, in the same order as
 * DCCStarterKits::All(). One Earth job per minigame: a repo driver, a ridge hand, a
 * call sign, a line coach, a mile runner, a gate warden, a floor walker. The books'
 * build tool prices a class as a job with a wide benefit (a whole kind of action, or a
 * small group of them), not one named trick. These are the boring version of that:
 * a percent that grows with level, and five lessons. A lesson grants a real skill.
 *
 * Nothing here is a DataTable. The Third Floor class rows can still load later; this is
 * the set a crawler can pick today, from the Tab menu's Character page or the starter-kit cards.
 */
namespace DCCBasicClasses
{
	struct FNode
	{
		int32 Level = 1;
		const TCHAR* Title = TEXT("");
		const TCHAR* SkillId = TEXT("");
		const TCHAR* Line = TEXT("");
	};

	struct FClass
	{
		const TCHAR* Name = TEXT("");
		const TCHAR* Blurb = TEXT("");
		FLinearColor Colour = FLinearColor::White;
		/** Percent added per character level. A specialist sets one. A wide job sets a few, smaller. */
		int32 MeleePerLevel = 0;
		int32 RangedPerLevel = 0;
		int32 SpellPerLevel = 0;
		int32 HealPerLevel = 0;
		/** Percent of incoming damage removed per level. */
		int32 TakenPerLevel = 0;
		/** Levels 1-5 teach the job's skills; 7, 10, 13, 16 and 20 its spells (2026-11-22). Unused slots have Level 0. */
		FNode Nodes[10] = {};
	};

	const TArray<FClass>& All();

	/** -1 when this crawler has not picked a kit. Otherwise the kit index. */
	int32 IndexOf(const ADCCCharacterBase* Crawler);

	const FClass* Find(const ADCCCharacterBase* Crawler);

	/** Server. Grants every lesson this crawler's level has reached. Already-known skills stay. Returns what was
	 *  newly learned ("Final Notice, Hold the Line"); bTell also says so on the crawler's screen. */
	FString ApplyLearned(ADCCCharacterBase* Crawler, bool bTell = true);

	/** The next lesson above this level, or null. */
	const FNode* NextLesson(const ADCCCharacterBase* Crawler);

	/** The level the class bonuses count: full rate to 5, half rate after (so level 20 counts as 12). */
	int32 BonusLevel(int32 Level);

	int32 MeleePercent(const ADCCCharacterBase* Crawler);
	int32 RangedPercent(const ADCCCharacterBase* Crawler);
	int32 SpellPercent(const ADCCCharacterBase* Crawler);
	int32 HealPercent(const ADCCCharacterBase* Crawler);
	/** 1 when this crawler is not a Tank. Otherwise less than 1. */
	float DamageTakenScale(const ADCCCharacterBase* Crawler);

	/** "Melee hits +40%" and so on. Empty when they have not picked. */
	FString BonusLine(const ADCCCharacterBase* Crawler);
}
