#pragma once

#include "CoreMinimal.h"

class ADCCCharacterBase;

/**
 * Tower-defense damage tweaks, called from the melee hook and the spell hook.
 * Lives here so the skill component does not include the siege game.
 * No siege in the world: both functions return the damage unchanged.
 */
namespace DCCSiegeHooks
{
	int32 AdjustMelee(ADCCCharacterBase* Attacker, ADCCCharacterBase* Target, int32 Damage);
	int32 AdjustSpell(ADCCCharacterBase* Caster, ADCCCharacterBase* Target, int32 Magnitude);
}
