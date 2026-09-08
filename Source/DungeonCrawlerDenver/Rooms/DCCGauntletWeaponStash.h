#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "DCCGauntletWeaponStash.generated.h"

class ADCCGauntletEncounter;
class ADCCPickup_Equipment;

/**
 * A single ground weapon at the gauntlet that gets better as the kill count
 * climbs. Zoe, 2026-09-05: "provide a stash of weapons there that gets
 * better as you clear waves. Like you take the weapon and after stage 5 you
 * get a better weapon."
 *
 * Deliberately just an ordered list of existing DT_DCCEquipment rows rather
 * than new items - the whole point of ADCCPickup_Equipment (see its own
 * comment) is that the equipment table is already placeable without new
 * C++, and a tiered stash is just that pickup swapped out on a timer driven
 * by kills instead of hand-placed once. One pickup exists on the field at a
 * time; taking it and dying doesn't lose the *tier* - the stash simply
 * spawns the same tier again immediately, so a wipe costs the run, not the
 * gear progress within it.
 */
UCLASS()
class DUNGEONCRAWLERDENVER_API ADCCGauntletWeaponStash : public AActor
{
	GENERATED_BODY()

public:
	ADCCGauntletWeaponStash();

	virtual void BeginPlay() override;

	/** The gauntlet this stash upgrades alongside. Wire this to the placed ADCCGauntletEncounter. */
	UPROPERTY(EditInstanceOnly, Category = "Gauntlet|Stash")
	TObjectPtr<ADCCGauntletEncounter> Gauntlet;

	/**
	 * Ordered weak-to-strong. Index 0 is on the ground from the start;
	 * index N appears once KillCount >= N * KillsPerTier. The last entry
	 * stays up for the remainder of the gauntlet once reached, rather than
	 * running out of tiers.
	 */
	UPROPERTY(EditAnywhere, Category = "Gauntlet|Stash")
	TArray<FName> WeaponTierItemIds = {
		TEXT("CorporateLetterOpener"),
		TEXT("HedgeTrimmers"),
		TEXT("WallSword"),
		TEXT("ConfiscatedSMG"),
		TEXT("HuntingRifleColorado"),
		TEXT("RiotShotgun")
	};

	/** Kills between one tier and the next. "After stage 5" per Zoe's own wording. */
	UPROPERTY(EditAnywhere, Category = "Gauntlet|Stash", meta = (ClampMin = "1"))
	int32 KillsPerTier = 5;

protected:
	UFUNCTION()
	void HandleKillCountChanged(int32 NewKillCount);

private:
	/** Server only. Destroys whatever is currently on the ground and spawns TierIndex in its place. */
	void SpawnTier(int32 TierIndex);

	UPROPERTY(Transient)
	TObjectPtr<ADCCPickup_Equipment> CurrentPickup;

	int32 CurrentTierIndex = -1;
};
