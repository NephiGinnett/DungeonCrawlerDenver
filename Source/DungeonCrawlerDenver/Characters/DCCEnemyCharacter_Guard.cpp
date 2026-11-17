#include "Characters/DCCEnemyCharacter_Guard.h"

#include "Stats/DCCStatsComponent.h"
#include "Components/SkeletalMeshComponent.h"
#include "Engine/SkeletalMesh.h"
#include "UObject/ConstructorHelpers.h"
#include "Characters/DCCForgeBody.h"

ADCCEnemyCharacter_Guard::ADCCEnemyCharacter_Guard()
{
	DisplayName = NSLOCTEXT("DCC", "GuardName", "Security");

	// An NPC Forge person (2026-11-16): the old cartoon minion body looked like someone else's character.
	DCCForgeBody::Apply(*this, DCCForgeBody::Body(4), 1.15f);

	// Placeholder stat block - no DT_DCCEnemies.json row exists for a bar
	// guard yet, so these are picked to read as "physically imposing but not
	// a real threat unless provoked" rather than tuned against any encounter.
	// Retune freely; nothing else depends on these specific numbers.
	if (Stats)
	{
		Stats->Level = 1;
		Stats->AbilityScores.SetScore(EDCCAbility::Strength, 5);
		Stats->AbilityScores.SetScore(EDCCAbility::Intelligence, 3);
		Stats->AbilityScores.SetScore(EDCCAbility::Constitution, 5);
		Stats->AbilityScores.SetScore(EDCCAbility::Dexterity, 3);
		Stats->AbilityScores.SetScore(EDCCAbility::Charisma, 3);
	}

	WeaponDamageDie = 6;
	AttackCooldownSeconds = 1.5f;
	AttackRange = 160.0f;

	// Never used while passive (see bPassiveUntilProvoked), but left at a
	// sane value in case a designer flips passivity off for a variant.
	AggroRadius = 1200.0f;

	bIsAlly = false;
	bPassiveUntilProvoked = true;
}
