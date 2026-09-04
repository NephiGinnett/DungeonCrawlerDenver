#pragma once

#include "CoreMinimal.h"
#include "Characters/DCCEnemyCharacter.h"
#include "DCCEnemyCharacter_Mine.generated.h"

// ---------------------------------------------------------------------------
// ENEMIES THAT ARE VERBS, NOT STAT BLOCKS
//
// Per Docs/next-evolution-plan.md: the existing roster is well-populated but
// nearly monolithic in behaviour - everything closes to melee and swings, with
// The Auditor as the one ranged exception. These two do not add a bigger
// number. Each one refuses to be solved by the verb the party already has, and
// each takes a system that already exists but is barely used and makes it the
// only way through.
//
// They live down the mine (Docs/floor-01-design.md, "The Claim") because that
// is where the floor stops being about debt collection and starts being about
// what is actually underneath Denver.
// ---------------------------------------------------------------------------

/**
 * LEGACY SYSTEM - the verb is "hit it with something that isn't a weapon".
 *
 * Enormous, slow, and completely immune to being struck. Weapons glance off
 * it and so do fists. What does hurt it is anything arriving from a distance:
 * the TargetedThrow skill it was designed around, a thrown crate, or frankly
 * any other skill in the game.
 *
 * That falls out of where the damage hook sits rather than needing a special
 * case anywhere. ModifyIncomingDamage is consulted ONLY on the attack path
 * (ADCCCharacterBase::PerformAttack), while skill damage
 * (UDCCSkillComponent) and thrown-prop impact damage (ADCCPickup) both go
 * straight to the stats component. Immunity to one and vulnerability to the
 * others therefore needed no new plumbing at all - the two paths already
 * existed and simply had to be pointed at.
 *
 * Every skill working, not just TargetedThrow, is deliberate rather than
 * sloppy: it is this enemy's crowbar, per the standing rule in
 * Docs/next-evolution-plan.md. TargetedThrow is the intended answer and the
 * Game Guide teaches it as such, but a party that works out "stop swinging,
 * start casting" on its own has solved the puzzle just as legitimately and
 * should not be stonewalled for arriving by a different door.
 *
 * The design intent is to make throwing load-bearing. It has been a toy since
 * the day it was built; this is the first thing in the game that rewards it.
 */
UCLASS()
class DUNGEONCRAWLERDENVER_API ADCCEnemyCharacter_LegacySystem : public ADCCEnemyCharacter
{
	GENERATED_BODY()

public:
	ADCCEnemyCharacter_LegacySystem();

	/** Always returns 0 - see the class comment on why this is not invulnerability. */
	virtual int32 ModifyIncomingDamage(int32 Damage, ADCCCharacterBase* Attacker) override;

protected:
	/** Throttles the "that did nothing" feedback so a party wailing on it does
	 *  not flood the log with one line per swing. */
	float LastDeflectNoticeTime = -100.0f;

	UPROPERTY(EditDefaultsOnly, Category = "Legacy System")
	float DeflectNoticeIntervalSeconds = 3.0f;
};

/**
 * RECONCILIATION - the verb is "coordinate".
 *
 * Two bodies auditing each other. Damage to one is undone by the other unless
 * both are struck inside a short window, so a party focusing fire makes no
 * progress at all no matter how hard it hits. Split the party, strike
 * together, and it collapses quickly.
 *
 * This is the first enemy in the project actually designed FOR five players
 * rather than merely tolerating them - the existing roster all resolves fine
 * against one crawler swinging repeatedly. It is also the reason the pair is
 * authored as two placed actors pointing at each other rather than one actor
 * with two meshes: they have to be separable in space, or "hit both at once"
 * is not a tactical problem.
 */
UCLASS()
class DUNGEONCRAWLERDENVER_API ADCCEnemyCharacter_Reconciliation : public ADCCEnemyCharacter
{
	GENERATED_BODY()

public:
	ADCCEnemyCharacter_Reconciliation();

	/**
	 * The other half. For a PLACED pair, set both actors' Twin to each other,
	 * once each - the same symmetric authoring ADCCOathgate::PairedGate uses.
	 *
	 * For a SPAWNED pair it is left empty and resolved in BeginPlay, because an
	 * EditInstanceOnly pointer cannot be authored on something an encounter
	 * creates at runtime. Without that fallback a wave-spawned Reconciliation
	 * would quietly behave like an ordinary enemy - the mechanic gone, nothing
	 * logged, exactly the silent-no-op failure this project keeps getting bitten
	 * by (see CLAUDE.md on the unset AttackAnimation).
	 */
	UPROPERTY(EditInstanceOnly, BlueprintReadOnly, Category = "Reconciliation")
	TObjectPtr<ADCCEnemyCharacter_Reconciliation> Twin;

	virtual void BeginPlay() override;

	/** How long after the twin was hit a blow still counts as simultaneous. */
	UPROPERTY(EditDefaultsOnly, Category = "Reconciliation")
	float SimultaneousWindowSeconds = 2.5f;

	virtual int32 ModifyIncomingDamage(int32 Damage, ADCCCharacterBase* Attacker) override;

	/** World time this one last took a real hit. Read by the twin. */
	float GetLastHitTime() const { return LastHitTime; }

protected:
	float LastHitTime = -1000.0f;

	/** Server only. Undoes a hit on the twin's behalf, capped at its max HP. */
	void HealTwin(int32 Amount);
};
