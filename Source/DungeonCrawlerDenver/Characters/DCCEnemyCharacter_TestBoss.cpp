#include "Characters/DCCEnemyCharacter_TestBoss.h"

#include "Characters/DCCForgeBody.h"
#include "Combat/DCCBossComponent.h"
#include "Stats/DCCStatsComponent.h"
#include "Items/DCCPickup.h"
#include "Components/SkeletalMeshComponent.h"
#include "Engine/SkeletalMesh.h"
#include "UObject/ConstructorHelpers.h"

ADCCEnemyCharacter_TestBoss::ADCCEnemyCharacter_TestBoss()
{
	DisplayName = NSLOCTEXT("DCC", "TestBossName", "The Interim Manager");

	// An NPC Forge office worker in a manager's suit, scaled up so a boss reads as bigger than the mob beside it
	// (2026-11-15: enemies no longer wear the crawlers' bodies; this used to be CJ's).
	DCCForgeBody::Apply(*this, DCCForgeBody::Body(5), 1.3f);

	// Tougher and harder-hitting than a Brute, but not tuned for real - this
	// only has to survive long enough to demonstrate the charge and the lien.
	if (Stats)
	{
		Stats->AbilityScores.SetScore(EDCCAbility::Constitution, 8);
		Stats->AbilityScores.SetScore(EDCCAbility::Strength, 6);
	}
	WeaponDamageDie = 8;
	AttackRange = 250.0f;
	AttackCooldownSeconds = 2.0f;
	AggroRadius = 2000.0f;

	// A visible payday for testing the lien and the exit's gold banking in the
	// same pass - the coin drop on top of whatever the party is already carrying.
	{
		FDCCLootDrop BossPurse;
		BossPurse.PickupClass = LootTable.IsEmpty() ? nullptr : LootTable[0].PickupClass;
		if (BossPurse.PickupClass)
		{
			BossPurse.Chance = 1.0f;
			BossPurse.MinGold = 40;
			BossPurse.MaxGold = 80;
			LootTable.Add(BossPurse);
		}
	}

	Boss = CreateDefaultSubobject<UDCCBossComponent>(TEXT("Boss"));

	// Shorter and gentler than the real fight's tuning (9s / 6 base / 8 per
	// minion) - this exists to be finished in a minute or two, not to be a real
	// encounter.
	Boss->ChargeHealthThreshold = 0.5f;
	Boss->ChargeDurationSeconds = 6.0f;
	Boss->ChargeBaseDamage = 4;
	Boss->ChargeDamagePerMinion = 4;
	Boss->VulnerableSecondsAfterCharge = 6.0f;
	Boss->LienGold = 25;

	Boss->ChargeStartLine = NSLOCTEXT("DCC", "TestBossCharge",
		"The Interim Manager begins reviewing your account. This will take a moment.");
	Boss->ChargeLandedLine = NSLOCTEXT("DCC", "TestBossChargeLanded",
		"Your account has been reviewed.");
	Boss->ChargeFizzledLine = NSLOCTEXT("DCC", "TestBossFizzle",
		"Review cancelled. Somewhere, a manager is filing a complaint about you.");
	Boss->LienLine = NSLOCTEXT("DCC", "TestBossLien",
		"A balance remains on your account.");

	// Clips come with the Forge body (DCCForgeBody::Apply above).
}
