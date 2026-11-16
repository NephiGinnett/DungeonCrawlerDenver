#include "Characters/DCCEnemyCharacter_NuclearFamily.h"

#include "Characters/DCCForgeBody.h"
#include "Combat/DCCRadiationAura.h"
#include "Combat/DCCGameState.h"
#include "Stats/DCCStatsComponent.h"
#include "Skills/DCCSkillComponent.h"
#include "UI/DCCEventLogTypes.h"
#include "Components/SkeletalMeshComponent.h"
#include "Engine/SkeletalMesh.h"
#include "Engine/World.h"
#include "EngineUtils.h"
#include "UObject/ConstructorHelpers.h"

ADCCEnemyCharacter_NuclearFamily::ADCCEnemyCharacter_NuclearFamily()
{
	Radiation = CreateDefaultSubobject<UDCCRadiationAura>(TEXT("Radiation"));

	// Low damage, high aggro radius, slow approach. They are a zone, not a
	// duelist - the fight is about where you stand, not about trading hits.
	WeaponDamageDie = 4;
	AttackCooldownSeconds = 2.4f;
	AggroRadius = 1800.0f;

	// Deliberately loose. They amble after you rather than committing, which is
	// what lets a party kite them and makes standing still the actual mistake.
	LeashRadius = 3600.0f;

	GriefLine = NSLOCTEXT("DCC", "NuclearGrief",
		"The Family close ranks. Grief, like everything else here, is measured in millisieverts.");
}

void ADCCEnemyCharacter_NuclearFamily::BeginPlay()
{
	Super::BeginPlay();

	if (!HasAuthority())
	{
		return;
	}

	// Every relative listens to every other relative, including itself - the
	// handler skips the deceased, so a self-subscription is harmless and saves
	// maintaining a roster that a mid-fight spawn would invalidate anyway.
	for (TActorIterator<ADCCEnemyCharacter_NuclearFamily> It(GetWorld()); It; ++It)
	{
		if (ADCCEnemyCharacter_NuclearFamily* Relative = *It)
		{
			Relative->OnCharacterDied.AddUniqueDynamic(
				this, &ADCCEnemyCharacter_NuclearFamily::HandleRelativeDied);

			// And the reverse, so somebody spawning later still escalates the
			// ones already here.
			if (Relative != this)
			{
				OnCharacterDied.AddUniqueDynamic(
					Relative, &ADCCEnemyCharacter_NuclearFamily::HandleRelativeDied);
			}
		}
	}
}

void ADCCEnemyCharacter_NuclearFamily::HandleRelativeDied(ADCCCharacterBase* Character)
{
	// The one who died does not grieve for itself, and a corpse's aura is
	// already switched off by the component.
	if (!HasAuthority() || Character == this || !IsAlive() || !Radiation)
	{
		return;
	}

	Radiation->SetIntensity(Radiation->Intensity + GriefIntensityPerLoss);

	// Said once, by whoever is left, rather than once per survivor - three
	// identical grief lines would land as a bug rather than as a beat.
	static float LastGriefAnnounceTime = -100.0f;
	const float Now = GetWorld() ? GetWorld()->GetTimeSeconds() : 0.0f;

	if (!GriefLine.IsEmpty() && Now - LastGriefAnnounceTime > 1.0f)
	{
		LastGriefAnnounceTime = Now;
		ADCCGameState::Log(this, EDCCLogCategory::Announcement, GriefLine);
	}
}

// ---------------------------------------------------------------------------

ADCCEnemy_NuclearDad::ADCCEnemy_NuclearDad()
{
	DisplayName = NSLOCTEXT("DCC", "NuclearDadName", "Dad, Still Grilling");

	// Reskinned onto a shared-skeleton crawler until real art exists, so he at
	// least walks and swings rather than T-posing through the fight.
	// An NPC Forge person (2026-11-15). The family used to wear three of the crawlers' bodies.
	DCCForgeBody::Apply(*this, DCCForgeBody::Body(0), 1.08f);

	if (Stats)
	{
		Stats->Level = 2;
		Stats->AbilityScores.SetScore(EDCCAbility::Strength, 5);
		Stats->AbilityScores.SetScore(EDCCAbility::Intelligence, 2);
		Stats->AbilityScores.SetScore(EDCCAbility::Constitution, 6);
		Stats->AbilityScores.SetScore(EDCCAbility::Dexterity, 2);
		Stats->AbilityScores.SetScore(EDCCAbility::Charisma, 4);
	}

	// The widest aura in the family and the slowest legs. He is the reason the
	// barbecue is the most dangerous piece of furniture in the room.
	if (Radiation)
	{
		Radiation->Radius = 700.0f;
		Radiation->DoseGainPerSecond = 1.0f;
		Radiation->DamagePerDose = 2;
	}

	GriefIntensityPerLoss = 0.5f;
}

ADCCEnemy_NuclearMom::ADCCEnemy_NuclearMom()
{
	DisplayName = NSLOCTEXT("DCC", "NuclearMomName", "Mom, Insisting You Eat");

	// An NPC Forge person (2026-11-15). The family used to wear three of the crawlers' bodies.
	DCCForgeBody::Apply(*this, DCCForgeBody::Body(3), 1.0f);

	if (Stats)
	{
		Stats->Level = 2;
		Stats->AbilityScores.SetScore(EDCCAbility::Strength, 3);
		Stats->AbilityScores.SetScore(EDCCAbility::Intelligence, 4);
		Stats->AbilityScores.SetScore(EDCCAbility::Constitution, 5);
		Stats->AbilityScores.SetScore(EDCCAbility::Dexterity, 4);
		Stats->AbilityScores.SetScore(EDCCAbility::Charisma, 6);
	}

	// Tighter than Dad's but she actually follows, so retreating buys less time
	// against her than it does against him. Between them the party cannot simply
	// pick one direction and hold it.
	if (Radiation)
	{
		Radiation->Radius = 480.0f;
		Radiation->DoseGainPerSecond = 1.4f;
	}
}

ADCCEnemy_NuclearKid::ADCCEnemy_NuclearKid()
{
	DisplayName = NSLOCTEXT("DCC", "NuclearKidName", "The Kid, Showing You Something");

	// An NPC Forge person (2026-11-15). The family used to wear three of the crawlers' bodies.
	DCCForgeBody::Apply(*this, DCCForgeBody::Body(6), 0.72f);

	if (Stats)
	{
		Stats->Level = 1;
		Stats->AbilityScores.SetScore(EDCCAbility::Strength, 2);
		Stats->AbilityScores.SetScore(EDCCAbility::Intelligence, 3);
		Stats->AbilityScores.SetScore(EDCCAbility::Constitution, 2);
		Stats->AbilityScores.SetScore(EDCCAbility::Dexterity, 6);
		Stats->AbilityScores.SetScore(EDCCAbility::Charisma, 5);
	}

	// Small aura, fragile, quick. He is the obvious first target and the worst
	// possible first target, because the other two grieve hardest for him.
	if (Radiation)
	{
		Radiation->Radius = 380.0f;
		Radiation->DoseGainPerSecond = 1.1f;
		Radiation->DamagePerDose = 1;
	}

	// Twice the family rate. Killing him first roughly doubles the pressure from
	// Mom and Dad for the rest of the fight.
	GriefIntensityPerLoss = 0.9f;

	AttackCooldownSeconds = 1.4f;
}
