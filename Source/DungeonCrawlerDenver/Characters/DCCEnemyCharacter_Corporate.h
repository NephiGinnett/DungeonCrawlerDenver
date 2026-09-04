#pragma once

#include "CoreMinimal.h"
#include "Characters/DCCEnemyCharacter.h"
#include "DCCEnemyCharacter_Corporate.generated.h"

class ADCCEncounter;

// ---------------------------------------------------------------------------
// THREE MORE VERBS
//
// Continues the roster from Docs/next-evolution-plan.md. Same principle as
// Legacy System and Reconciliation: each of these refuses to be solved by the
// verb the party already has, and each takes an existing system and makes it
// load-bearing rather than adding a new one.
//
//   Intern Pool           -> "stop killing the adds"  (target priority)
//   The Notary            -> "hit the weak point"     (target priority, harder)
//   Escalation Specialist -> "stop casting"           (resource discipline)
//
// All three are corporate-satire named to match the floor, and all three are
// deliberately beatable by a party that never works out the trick - just
// slowly, and expensively. Nothing here is a hard gate, for the same reason
// nothing in Puzzles/ is.
// ---------------------------------------------------------------------------

/**
 * THE INTERN POOL - a manager who is not, personally, a threat.
 *
 * Stands at the back generating unpaid labour indefinitely. The interns are
 * weak, apologetic and endless; the manager is the encounter. A party that
 * commits to clearing adds will be there all day, and the fight only ends when
 * somebody looks past the swarm at the person quietly producing it.
 *
 * Spawned interns are registered with the owning encounter via
 * ADCCEncounter::RegisterSpawnedEnemy - the fix from the 2026-08-22
 * reconciliation pass, without which everything summoned mid-fight is invisible
 * to clear-tracking. Learned the hard way on the Balloon Payment.
 */
UCLASS()
class DUNGEONCRAWLERDENVER_API ADCCEnemyCharacter_InternManager : public ADCCEnemyCharacter
{
	GENERATED_BODY()

public:
	ADCCEnemyCharacter_InternManager();

	virtual void BeginPlay() override;
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;

	/** What it produces. Defaults to the intern class below. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Interns")
	TSubclassOf<ADCCEnemyCharacter> InternClass;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Interns", meta = (ClampMin = "1.0"))
	float InternIntervalSeconds = 6.0f;

	/** Stops producing at this many living interns, so an ignored manager does
	 *  not eventually fill the room and cost the frame budget. It is a war of
	 *  attrition, not a memory leak. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Interns", meta = (ClampMin = "1"))
	int32 MaxLiveInterns = 6;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Interns", meta = (ClampMin = "0.0"))
	float InternSpawnRadius = 320.0f;

protected:
	FTimerHandle InternTimer;

	/** Server only. */
	void SpawnIntern();

	/** Living interns this manager is responsible for. Weak, so a destroyed
	 *  intern drops out of the count without needing a death delegate. */
	UPROPERTY(Transient)
	TArray<TWeakObjectPtr<ADCCEnemyCharacter>> LiveInterns;
};

/** One intern. Weak everywhere, and not the problem. */
UCLASS()
class DUNGEONCRAWLERDENVER_API ADCCEnemyCharacter_Intern : public ADCCEnemyCharacter
{
	GENERATED_BODY()

public:
	ADCCEnemyCharacter_Intern();
};

/**
 * THE NOTARY - cannot be harmed until its Seal is broken.
 *
 * Immune to everything while the Seal actor it is bound to is alive. The Seal
 * is fragile and elsewhere in the room, so the fight is a target-priority
 * problem rather than a damage one: the party has to split attention while the
 * Notary keeps working.
 *
 * Uses the same ModifyIncomingDamage hook as Legacy System, which means - as
 * with Legacy System - skills and thrown props also bypass the immunity. That
 * is again deliberate rather than an oversight: it is the crowbar.
 */
UCLASS()
class DUNGEONCRAWLERDENVER_API ADCCEnemyCharacter_Notary : public ADCCEnemyCharacter
{
	GENERATED_BODY()

public:
	ADCCEnemyCharacter_Notary();

	/**
	 * Break this and the Notary becomes killable. Any character will do -
	 * authored as a second, deliberately fragile placed actor rather than a
	 * bespoke class, so a floor can make the seal anything it likes.
	 */
	UPROPERTY(EditInstanceOnly, Category = "Notary")
	TObjectPtr<ADCCCharacterBase> Seal;

	virtual int32 ModifyIncomingDamage(int32 Damage, ADCCCharacterBase* Attacker) override;

protected:
	float LastSealNoticeTime = -100.0f;

	UPROPERTY(EditDefaultsOnly, Category = "Notary")
	float SealNoticeIntervalSeconds = 3.0f;
};

/**
 * ESCALATION SPECIALIST - gets worse every time anyone uses a skill.
 *
 * Gains a permanent stacking damage bonus on every skill activation by any
 * crawler in the encounter, so the party's most reliable answer to a hard
 * enemy is the thing making this one harder. Beatable with basic attacks and
 * patience.
 *
 * This is the first enemy in the project that punishes a RESOURCE pattern
 * rather than a positioning one, and it exists because "spam your best skill"
 * is currently the correct answer to almost everything.
 */
UCLASS()
class DUNGEONCRAWLERDENVER_API ADCCEnemyCharacter_EscalationSpecialist : public ADCCEnemyCharacter
{
	GENERATED_BODY()

public:
	ADCCEnemyCharacter_EscalationSpecialist();

	virtual void BeginPlay() override;

	/** Damage-die bonus added per stack. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Escalation")
	int32 DamagePerStack = 2;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Escalation")
	int32 MaxStacks = 10;

	UPROPERTY(BlueprintReadOnly, Category = "Escalation")
	int32 Stacks = 0;

	/** How far away a skill still counts as escalating this one. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Escalation")
	float EscalationRadius = 2200.0f;

protected:
	/** Bound to every crawler's skill component in the world at BeginPlay. */
	UFUNCTION()
	void HandleSkillActivated(class UDCCSkillComponent* CastingSkills, const struct FDCCSkillResult& Result);

	int32 BaseWeaponDamageDie = 6;
};
