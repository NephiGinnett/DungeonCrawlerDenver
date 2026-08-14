#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "Stats/DCCAbilityTypes.h"
#include "DCCStatsComponent.generated.h"

class UDCCRuleset;
class UDCCStatsComponent;

DECLARE_DYNAMIC_MULTICAST_DELEGATE_ThreeParams(FDCCHealthChanged, UDCCStatsComponent*, Stats, int32, NewHitPoints, int32, Delta);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_ThreeParams(FDCCManaChanged, UDCCStatsComponent*, Stats, int32, NewManaPoints, int32, Delta);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FDCCDied, UDCCStatsComponent*, Stats);

/**
 * Holds one character's ability scores and hit points, and is the single place
 * gameplay code asks "what happens?".
 *
 * Attach to players and enemies alike - an encounter only needs both sides to
 * speak the same language, and keeping enemies on the same component means the
 * DM client can inspect and override them the same way it does players.
 *
 * Server-authoritative: damage, healing and checks are refused on clients. All
 * state reaches clients by replication, so nothing here trusts a client's word
 * about its own health.
 */
UCLASS(ClassGroup = (DCC), meta = (BlueprintSpawnableComponent))
class DUNGEONCRAWLERDENVER_API UDCCStatsComponent : public UActorComponent
{
	GENERATED_BODY()

public:
	UDCCStatsComponent();

	virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;
	virtual void InitializeComponent() override;

	/** Which ruleset this character resolves against. Swap this to swap the rules. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Stats")
	TSubclassOf<UDCCRuleset> RulesetClass;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, ReplicatedUsing = OnRep_AbilityScores, Category = "Stats")
	FDCCAbilityScoreSet AbilityScores;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Stats", meta = (ClampMin = "1"))
	int32 Level = 1;

	UPROPERTY(BlueprintReadOnly, ReplicatedUsing = OnRep_CurrentHitPoints, Category = "Stats")
	int32 CurrentHitPoints = 0;

	UPROPERTY(BlueprintReadOnly, Replicated, Category = "Stats")
	int32 MaxHitPoints = 0;

	UPROPERTY(BlueprintReadOnly, ReplicatedUsing = OnRep_CurrentManaPoints, Category = "Stats")
	int32 CurrentManaPoints = 0;

	UPROPERTY(BlueprintReadOnly, Replicated, Category = "Stats")
	int32 MaxManaPoints = 0;

	/** Fires on server and on every client that receives the change. */
	UPROPERTY(BlueprintAssignable, Category = "Stats")
	FDCCHealthChanged OnHealthChanged;

	UPROPERTY(BlueprintAssignable, Category = "Stats")
	FDCCManaChanged OnManaChanged;

	UPROPERTY(BlueprintAssignable, Category = "Stats")
	FDCCDied OnDied;

	UFUNCTION(BlueprintPure, Category = "Stats")
	bool IsAlive() const { return CurrentHitPoints > 0; }

	UFUNCTION(BlueprintPure, Category = "Stats")
	int32 GetAbilityModifier(EDCCAbility Ability) const;

	UFUNCTION(BlueprintPure, Category = "Stats")
	UDCCRuleset* GetRuleset() const;

	/** Server only. Negative amounts are ignored; use Heal instead. */
	UFUNCTION(BlueprintCallable, Category = "Stats")
	void ApplyDamage(int32 Amount);

	UFUNCTION(BlueprintCallable, Category = "Stats")
	void Heal(int32 Amount);

	/**
	 * Server only. Returns false and spends nothing if the cost is not
	 * affordable, so callers can use it directly as the gate on casting.
	 */
	UFUNCTION(BlueprintCallable, Category = "Stats")
	bool SpendMana(int32 Amount);

	UFUNCTION(BlueprintCallable, Category = "Stats")
	void RestoreMana(int32 Amount);

	UFUNCTION(BlueprintPure, Category = "Stats")
	bool CanAffordMana(int32 Amount) const { return Amount <= CurrentManaPoints; }

	/** Server only. Returns a zeroed result if called on a client. */
	UFUNCTION(BlueprintCallable, Category = "Stats")
	FDCCCheckResult ResolveCheck(EDCCAbility Ability, int32 DifficultyClass, int32 SituationalBonus = 0);

	/**
	 * Server only. Re-derives max hit points after Level or AbilityScores were
	 * changed at runtime.
	 *
	 * Needed because components initialise during actor spawn, before a spawner
	 * has had any chance to stamp per-instance values onto them - an encounter
	 * setting a per-spawn level is exactly that case.
	 */
	UFUNCTION(BlueprintCallable, Category = "Stats")
	void RefreshDerivedStats(bool bRefillToFull = true);

protected:
	UFUNCTION()
	void OnRep_CurrentHitPoints(int32 PreviousHitPoints);

	UFUNCTION()
	void OnRep_CurrentManaPoints(int32 PreviousManaPoints);

	UFUNCTION()
	void OnRep_AbilityScores();

private:
	/** Recalculates MaxHitPoints from the ruleset, clamping current HP to fit. */
	void RecalculateDerivedStats(bool bRefillToFull);

	UPROPERTY(Transient)
	TObjectPtr<UDCCRuleset> CachedRuleset;
};
