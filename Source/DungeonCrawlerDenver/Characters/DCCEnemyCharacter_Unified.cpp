#include "Characters/DCCEnemyCharacter_Unified.h"

#include "Characters/DCCCrawlerAnimSet.h"
#include "Skills/DCCSpellBook.h"
#include "NiagaraSystem.h"
#include "Components/CapsuleComponent.h"
#include "Components/SkeletalMeshComponent.h"
#include "Engine/SkeletalMesh.h"
#include "Stats/DCCStatsComponent.h"
#include "UObject/ConstructorHelpers.h"

ADCCEnemyCharacter_Unified::ADCCEnemyCharacter_Unified()
{
	bUseRagdollOnDeath = false;
	DeathDissolveEffect = TSoftObjectPtr<UNiagaraSystem>(FSoftObjectPath(DCCSpellBook::DeathPuffPath()));
	DCCCrawlerAnimSet::Apply(*this, 0.4f);
}

void ADCCEnemyCharacter_Unified::UseUnifiedMesh(USkeletalMesh* BodyMesh, float HeightCm)
{
	if (UCapsuleComponent* Capsule = GetCapsuleComponent())
	{
		const float Half = FMath::Max(44.0f, HeightCm * 0.5f);
		Capsule->InitCapsuleSize(FMath::Clamp(HeightCm * 0.2f, 30.0f, 70.0f), Half);
	}
	if (USkeletalMeshComponent* Body = GetMesh())
	{
		if (BodyMesh)
		{
			Body->SetSkeletalMesh(BodyMesh);
		}
		// The unify tool stands the model's feet on the origin, facing -Y; a character's mesh faces +X.
		Body->SetRelativeLocationAndRotation(FVector(0.0f, 0.0f, -FMath::Max(44.0f, HeightCm * 0.5f)), FRotator(0.0f, -90.0f, 0.0f));
	}
}

ADCCEnemy_MushroomDemon::ADCCEnemy_MushroomDemon()
{
	DisplayName = NSLOCTEXT("DCC", "MushroomDemonName", "Mushroom Demon");
	static ConstructorHelpers::FObjectFinder<USkeletalMesh> Body(
		TEXT("/Game/DCC/Characters/Unified/MushroomDemon/SK_MushroomDemon.SK_MushroomDemon"));
	UseUnifiedMesh(Body.Succeeded() ? Body.Object : nullptr, 210.0f);
	if (Stats)
	{
		Stats->Level = 3;
		Stats->AbilityScores.SetScore(EDCCAbility::Strength, 3);
		Stats->AbilityScores.SetScore(EDCCAbility::Dexterity, 5);
		Stats->AbilityScores.SetScore(EDCCAbility::Constitution, 3);
	}
	WeaponDamageDie = 6;
	AttackCooldownSeconds = 1.1f;
	AttackRange = 190.0f;
}

ADCCEnemy_DemonBull::ADCCEnemy_DemonBull()
{
	DisplayName = NSLOCTEXT("DCC", "DemonBullName", "Demon Bull");
	static ConstructorHelpers::FObjectFinder<USkeletalMesh> Body(
		TEXT("/Game/DCC/Characters/Unified/DemonBull/SK_DemonBull.SK_DemonBull"));
	UseUnifiedMesh(Body.Succeeded() ? Body.Object : nullptr, 260.0f);
	if (Stats)
	{
		Stats->Level = 5;
		Stats->AbilityScores.SetScore(EDCCAbility::Strength, 6);
		Stats->AbilityScores.SetScore(EDCCAbility::Constitution, 6);
		Stats->AbilityScores.SetScore(EDCCAbility::Dexterity, 2);
	}
	WeaponDamageDie = 10;
	AttackCooldownSeconds = 1.8f;
	AttackRange = 240.0f;
}
