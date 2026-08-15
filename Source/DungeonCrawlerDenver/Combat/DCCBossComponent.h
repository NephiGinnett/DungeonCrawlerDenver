#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "DCCBossComponent.generated.h"

class ADCCCharacterBase;
class ADCCEncounter;
class UDCCBossComponent;
class UDCCStatsComponent;

DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FDCCChargeStarted, UDCCBossComponent*, Boss);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_TwoParams(FDCCChargeResolved, UDCCBossComponent*, Boss, int32, Damage);

/**
 * The parts of a boss fight that a stat block cannot express.
 *
 * Waves already handle summoning (see FDCCEncounterWave), so what is left is the
 * thing that makes Voldemortgage's fight a *fight*: a long, visible, interruptible
 * wind-up whose damage is decided by how many minions are still breathing when it
 * lands.
 *
 * That single mechanic is the whole design. It converts "kill the boss" into "kill
 * the right things in the right order under a clock", which is the only shape of
 * encounter the pause-and-queue loop is actually built for - and the first fight
 * where pausing is correct play rather than a convenience.
 *
 * Server-authoritative throughout. Clients see the charge through replicated
 * progress and the resulting damage through replicated hit points.
 */
UCLASS(ClassGroup = (DCC), meta = (BlueprintSpawnableComponent))
class DUNGEONCRAWLERDENVER_API UDCCBossComponent : public UActorComponent
{
	GENERATED_BODY()

public:
	UDCCBossComponent();

	virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;
	virtual void BeginPlay() override;
	virtual void TickComponent(float DeltaSeconds, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction) override;

	// -------------------------------------------------------------------------
	// THE CHARGE
	// -------------------------------------------------------------------------

	/** Health fraction at or below which the charge begins. Zero disables it. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Boss|Charge", meta = (ClampMin = "0.0", ClampMax = "1.0"))
	float ChargeHealthThreshold = 0.5f;

	/**
	 * How long the wind-up lasts.
	 *
	 * Long on purpose. The party has to have time to see it, decide, pause, assign
	 * targets and act - nine seconds is roughly the three turns the fight was
	 * designed around, and anything much shorter turns a decision into a reflex.
	 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Boss|Charge", meta = (ClampMin = "1.0"))
	float ChargeDurationSeconds = 9.0f;

	/** Damage if every minion is dead when it lands. The "Minimum Payment". */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Boss|Charge", meta = (ClampMin = "0"))
	int32 ChargeBaseDamage = 6;

	/** Added per living minion at the moment it resolves. This is the lever. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Boss|Charge", meta = (ClampMin = "0"))
	int32 ChargeDamagePerMinion = 8;

	/** Crawlers beyond this range take nothing. Zero means the whole level. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Boss|Charge", meta = (ClampMin = "0.0"))
	float ChargeRadius = 4000.0f;

	/**
	 * Seconds of reduced defence and no summoning after the charge resolves.
	 *
	 * The party's actual window. Without it the fight is a treadmill; with it,
	 * surviving the big hit is itself the reward.
	 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Boss|Charge", meta = (ClampMin = "0.0"))
	float VulnerableSecondsAfterCharge = 8.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Boss|Charge")
	FText ChargeStartLine;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Boss|Charge")
	FText ChargeLandedLine;

	/** Used when the party cleared every minion first. Should sound disappointed. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Boss|Charge")
	FText ChargeFizzledLine;

	// -------------------------------------------------------------------------
	// THE LIEN
	// -------------------------------------------------------------------------

	/**
	 * Gold owed by a random crawler after this boss dies.
	 *
	 * Fires even if the boss dies before ever charging, so a fast aggressive party
	 * still meets the gimmick. Zero disables it.
	 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Boss|Lien", meta = (ClampMin = "0"))
	int32 LienGold = 25;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Boss|Lien")
	FText LienLine;

	// -------------------------------------------------------------------------

	/** Replicated 0-1 so every client can draw the same wind-up bar. */
	UPROPERTY(BlueprintReadOnly, Replicated, Category = "Boss")
	float ChargeProgress = 0.0f;

	UPROPERTY(BlueprintReadOnly, Replicated, Category = "Boss")
	bool bCharging = false;

	/** True during the post-charge window. Read by the ruleset-facing defence hook. */
	UPROPERTY(BlueprintReadOnly, Replicated, Category = "Boss")
	bool bVulnerable = false;

	UPROPERTY(BlueprintAssignable, Category = "Boss")
	FDCCChargeStarted OnChargeStarted;

	UPROPERTY(BlueprintAssignable, Category = "Boss")
	FDCCChargeResolved OnChargeResolved;

	/** Server only. Starts the wind-up now, ignoring the health threshold. */
	UFUNCTION(BlueprintCallable, Category = "Boss")
	void BeginCharge();

	/** Server only. Stops a charge without it landing. For scripted interrupts. */
	UFUNCTION(BlueprintCallable, Category = "Boss")
	void CancelCharge();

	/** Living enemies in this boss's encounter, excluding the boss itself. */
	UFUNCTION(BlueprintPure, Category = "Boss")
	int32 GetLivingMinionCount() const;

	/** What the charge would do if it landed right now. Drives the telegraph. */
	UFUNCTION(BlueprintPure, Category = "Boss")
	int32 GetProjectedChargeDamage() const;

protected:
	/** Applies the damage, announces, and opens the vulnerable window. */
	void ResolveCharge();

	/** Bound to the owner's death. Hands out the lien. */
	UFUNCTION()
	void HandleBossDied(ADCCCharacterBase* Character);

	/** The encounter that spawned this boss, or null when placed by hand. */
	ADCCEncounter* GetOwningEncounter() const;

	UDCCStatsComponent* GetOwnerStats() const;

private:
	/** Stops the threshold re-triggering every tick once it has fired. */
	bool bHasCharged = false;

	float VulnerableTimeRemaining = 0.0f;
};
