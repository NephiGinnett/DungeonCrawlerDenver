#pragma once

#include "CoreMinimal.h"
#include "Skills/DCCSkillTypes.h"

/**
 * The class spell book (2026-11-22, Zoe: "new spells"). The spells the basic classes teach from level 7 to 20,
 * defined in code so they work without editing the skill DataTable. UDCCSkillComponent::FindSkill falls back here
 * when the table has no row of that name, so a row with the same id in DT_DCCSkills always wins: to retune one in
 * the editor, add the row there.
 *
 * Every spell is a Damage or Heal archetype, which the skill component resolves fully in C++ (cone, area or one
 * target). Power dice climb with the level that teaches them.
 */
namespace DCCSpellBook
{
	const TArray<FDCCSkillDef>& All();
	bool Find(FName SkillId, FDCCSkillDef& Out);
}
