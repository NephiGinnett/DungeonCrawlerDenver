#pragma once

#include "CoreMinimal.h"
#include "Skills/DCCSkillTypes.h"
#include "Stats/DCCDamageTypes.h"

/**
 * The class spell book (2026-11-22, Zoe: "new spells"). The spells the basic classes teach from level 7 to 20,
 * defined in code so they work without editing the skill DataTable. UDCCSkillComponent::FindSkill falls back here
 * when the table has no row of that name, so a row with the same id in DT_DCCSkills always wins: to retune one in
 * the editor, add the row there.
 *
 * Every spell is a Damage or Heal archetype, which the skill component resolves fully in C++ (cone, area or one
 * target). Power dice climb with the level that teaches them.
 */
class UNiagaraComponent;
class UNiagaraSystem;

namespace DCCSpellBook
{
	const TArray<FDCCSkillDef>& All();
	bool Find(FName SkillId, FDCCSkillDef& Out);

	/**
	 * The one spell-effect table (Fab plan A2). Fills cast, impact and impact height
	 * when this id has a row. Skills with no row keep whatever they already had.
	 */
	void ApplyPresentation(FDCCSkillDef& Skill);

	/** Niagara spawn scales for that row. Both come back 1 when the id is not in the table. */
	void ScalesFor(FName SkillId, float& OutCastScale, float& OutImpactScale);

	/** Turns a spawned system off so a looping aura cannot sit on a crawler. */
	void StopAfter(UNiagaraComponent* Component, float Seconds);

	/**
	 * One puff at a point. MaxDistance above 0 skips it when the local camera is farther
	 * than that (uu). 2500 is about 25 m, which is the cap for weapon hits and death puffs.
	 */
	void SpawnPuff(const UObject* WorldContext, UNiagaraSystem* System, const FVector& At, float Scale, float LifeSeconds, float MaxDistance);

	const TCHAR* DeathPuffPath();
	const TCHAR* HitPuffPath(EDCCDamageType Type);
}
