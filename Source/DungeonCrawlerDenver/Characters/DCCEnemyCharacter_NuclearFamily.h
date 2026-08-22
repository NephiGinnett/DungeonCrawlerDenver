#pragma once

#include "CoreMinimal.h"
#include "Characters/DCCEnemyCharacter.h"
#include "DCCEnemyCharacter_NuclearFamily.generated.h"

class UDCCRadiationAura;

// ---------------------------------------------------------------------------
// THE NUCLEAR FAMILY
//
// Rocky Flats sat sixteen miles upwind of Denver making plutonium triggers,
// leaked for decades, got raided by the FBI, and is now - genuinely, on purpose -
// a National Wildlife Refuge. The joke is already written; this roster only has
// to point at it.
//
// So: a 1950s promotional-film family, still cheerful, still doing the barbecue,
// gently luminous. They are not angry and they do not chase hard. They simply
// want you to stay a while, and staying is the thing that kills you.
//
// MECHANICALLY they are the counter-argument to every other fight on the floor.
// Debt Eaters and Voldemortgage both reward closing to melee and holding
// position. The Family punish exactly that: their threat is a dose that
// accumulates while you stand near them and decays when you back off, so the
// correct play is to strike and withdraw repeatedly. A party that has learned
// Floor 1's other lesson will lose to them on the first attempt, which is the
// point of putting them on Floor 1.
//
// GRIEF is the twist that stops "focus one down" being free. Each family member
// who dies raises the Intensity of everyone still standing - a wider, faster,
// harder-hitting aura. Killing the Kid first is a mistake you only make once.
// ---------------------------------------------------------------------------

/**
 * Shared base. Carries the aura and the grief bookkeeping; the three variants
 * below differ in stats, reach and how hard they escalate.
 */
UCLASS(Abstract)
class DUNGEONCRAWLERDENVER_API ADCCEnemyCharacter_NuclearFamily : public ADCCEnemyCharacter
{
	GENERATED_BODY()

public:
	ADCCEnemyCharacter_NuclearFamily();

	virtual void BeginPlay() override;

	/** The contamination they carry. */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "DCC|Radiation")
	TObjectPtr<UDCCRadiationAura> Radiation;

	/**
	 * Aura Intensity added to every surviving relative when one of them dies.
	 *
	 * The escalation is the whole encounter design: it converts "pick them off"
	 * into a real decision about order and pace, and it means the last one
	 * standing is genuinely dangerous rather than a mop-up.
	 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "DCC|Radiation", meta = (ClampMin = "0.0"))
	float GriefIntensityPerLoss = 0.45f;

	/** Line the System AI runs when a relative goes down. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "DCC|Radiation")
	FText GriefLine;

protected:
	/** Raises every surviving relative's Intensity. Server only. */
	UFUNCTION()
	void HandleRelativeDied(ADCCCharacterBase* Character);
};

/** Dad. Longest reach, slowest, never stops hosting. */
UCLASS()
class DUNGEONCRAWLERDENVER_API ADCCEnemy_NuclearDad : public ADCCEnemyCharacter_NuclearFamily
{
	GENERATED_BODY()

public:
	ADCCEnemy_NuclearDad();
};

/** Mom. Tighter aura, but she follows you, so backing off buys less. */
UCLASS()
class DUNGEONCRAWLERDENVER_API ADCCEnemy_NuclearMom : public ADCCEnemyCharacter_NuclearFamily
{
	GENERATED_BODY()

public:
	ADCCEnemy_NuclearMom();
};

/** The Kid. Fragile, fast, and grieved over hardest. Kill last. */
UCLASS()
class DUNGEONCRAWLERDENVER_API ADCCEnemy_NuclearKid : public ADCCEnemyCharacter_NuclearFamily
{
	GENERATED_BODY()

public:
	ADCCEnemy_NuclearKid();
};
