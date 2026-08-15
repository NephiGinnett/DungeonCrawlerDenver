#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "Progression/DCCAchievementTypes.h"
#include "DCCAchievementComponent.generated.h"

class UDataTable;
class UDCCAchievementComponent;

DECLARE_DYNAMIC_MULTICAST_DELEGATE_TwoParams(FDCCAchievementUnlocked, UDCCAchievementComponent*, Component, const FDCCAchievementDef&, Achievement);

/**
 * Tracks one crawler's running totals and unlocks achievements off them.
 *
 * Server-authoritative for the same reason everything else is: an achievement
 * pays XP and moves AI Favor, so a client deciding it earned one is a client
 * deciding its own level.
 *
 * The design is deliberately one-way. Callers only ever say "this happened, this
 * many times" through Record; they never ask which achievements exist or check
 * a threshold themselves. That keeps the achievement list from leaking into
 * combat code, which is what makes it safe for Zoe to invent new ones mid-session
 * without touching anything that fights.
 */
UCLASS(ClassGroup = (DCC), meta = (BlueprintSpawnableComponent))
class DUNGEONCRAWLERDENVER_API UDCCAchievementComponent : public UActorComponent
{
	GENERATED_BODY()

public:
	UDCCAchievementComponent();

	virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;
	virtual void BeginPlay() override;

	/** The achievement list. Without one this component does nothing at all. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Achievements")
	TObjectPtr<UDataTable> AchievementTable;

	/**
	 * Per-achievement progress, replicated so the owning client's menu can show a
	 * progress bar without asking the server.
	 */
	UPROPERTY(BlueprintReadOnly, Replicated, Category = "Achievements")
	TArray<FDCCAchievementProgress> Progress;

	/** Running totals, indexed by EDCCAchievementStat. Replicated for the menu. */
	UPROPERTY(BlueprintReadOnly, Replicated, Category = "Achievements")
	TArray<int32> StatTotals;

	UPROPERTY(BlueprintAssignable, Category = "Achievements")
	FDCCAchievementUnlocked OnAchievementUnlocked;

	/**
	 * Server only. Adds to a running total and unlocks anything it crosses.
	 *
	 * The single entry point on purpose - see the class comment. Amount may be
	 * more than one so cumulative stats like damage can be recorded in a single
	 * call per hit rather than looping.
	 */
	UFUNCTION(BlueprintCallable, Category = "Achievements")
	void Record(EDCCAchievementStat Stat, int32 Amount = 1);

	UFUNCTION(BlueprintPure, Category = "Achievements")
	int32 GetStatTotal(EDCCAchievementStat Stat) const;

	UFUNCTION(BlueprintPure, Category = "Achievements")
	bool IsUnlocked(FName AchievementId) const;

	UFUNCTION(BlueprintPure, Category = "Achievements")
	int32 GetUnlockedCount() const;

	/** Every achievement, unlocked or not, minus secrets still hidden. */
	UFUNCTION(BlueprintCallable, Category = "Achievements")
	void GetVisibleAchievements(TArray<FDCCAchievementDef>& OutAchievements) const;

	/** Convenience for the static helper below. */
	UFUNCTION(BlueprintPure, Category = "Achievements")
	static UDCCAchievementComponent* Find(AActor* Actor);

	/**
	 * Records against an actor's component if it has one, and does nothing if it
	 * does not.
	 *
	 * Exists so combat code can record unconditionally without a null check at
	 * every call site - an enemy with no achievement component is the normal
	 * case, not an error worth branching on.
	 */
	static void RecordFor(AActor* Actor, EDCCAchievementStat Stat, int32 Amount = 1);

protected:
	/** Finds and unlocks anything the given stat's new total has crossed. */
	void EvaluateUnlocks(EDCCAchievementStat Stat);

	void Unlock(const FDCCAchievementDef& Achievement);

	FDCCAchievementProgress& FindOrAddProgress(FName AchievementId);
};
