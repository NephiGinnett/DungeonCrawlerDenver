#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "Favor/DCCFavorTypes.h"
#include "DCCFavorComponent.generated.h"

class UDCCFavorComponent;

DECLARE_DYNAMIC_MULTICAST_DELEGATE_TwoParams(FDCCFavorChanged, UDCCFavorComponent*, Favor, const FDCCFavorChange&, Change);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_ThreeParams(FDCCTierChanged, UDCCFavorComponent*, Favor, EDCCFavorTier, NewTier, EDCCFavorTier, OldTier);

/**
 * One crawler's standing with the audience.
 *
 * Server-authoritative like everything else mechanical: clients display favor,
 * they never set it. Sits on the character rather than the PlayerState so that
 * NPCs and bosses can carry favor too - in the fiction they are performers on
 * the same show, and the DM client benefits from inspecting them identically.
 *
 * Deliberately has no opinion about what "entertaining" means. Detecting style
 * automatically is a post-v1 problem; this exists so that when Zoe calls a swing
 * live, there is a real number behind it that the whole game can react to.
 */
UCLASS(ClassGroup = (DCC), meta = (BlueprintSpawnableComponent))
class DUNGEONCRAWLERDENVER_API UDCCFavorComponent : public UActorComponent
{
	GENERATED_BODY()

public:
	UDCCFavorComponent();

	virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;

	UPROPERTY(BlueprintReadOnly, ReplicatedUsing = OnRep_Favor, Category = "Favor")
	int32 Favor = 0;

	/** Favor can go negative: actively boring is worse than unknown. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Favor")
	int32 MinFavor = -200;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Favor")
	int32 MaxFavor = 1000;

	/**
	 * Lower bound of each tier above Unwatched, in order. Authored rather than
	 * hardcoded because this is pure feel, and Zoe will retune it between
	 * sessions once she has watched the group play.
	 */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Favor")
	TArray<int32> TierThresholds = { 50, 150, 350, 700 };

	/** Fires on server and on every client that receives the change. */
	UPROPERTY(BlueprintAssignable, Category = "Favor")
	FDCCFavorChanged OnFavorChanged;

	/** The interesting one: sponsors, gifts and DM interventions hang off this. */
	UPROPERTY(BlueprintAssignable, Category = "Favor")
	FDCCTierChanged OnTierChanged;

	UFUNCTION(BlueprintPure, Category = "Favor")
	EDCCFavorTier GetTier() const { return GetTierForFavor(Favor); }

	UFUNCTION(BlueprintPure, Category = "Favor")
	EDCCFavorTier GetTierForFavor(int32 InFavor) const;

	/** 0..1 progress through the current tier, for a HUD meter. */
	UFUNCTION(BlueprintPure, Category = "Favor")
	float GetTierProgress() const;

	/** The ledger, newest last. Capped; this is for narration, not accounting. */
	UFUNCTION(BlueprintPure, Category = "Favor")
	const TArray<FDCCFavorChange>& GetRecentChanges() const { return RecentChanges; }

	/**
	 * Server only. The one way favor ever moves.
	 *
	 * Note is optional but strongly encouraged - it is what the AI narrator
	 * quotes when it explains to the table why the crowd just turned.
	 */
	UFUNCTION(BlueprintCallable, Category = "Favor", meta = (AdvancedDisplay = "Note"))
	void AwardFavor(int32 Delta, EDCCFavorEvent Reason, const FText& Note);

	/** Server only. Hard set, for DM correction and save-game restore. */
	UFUNCTION(BlueprintCallable, Category = "Favor")
	void SetFavor(int32 NewFavor, const FText& Note);

protected:
	UFUNCTION()
	void OnRep_Favor(int32 PreviousFavor);

	/** Most recent entries only; older ones are dropped. */
	UPROPERTY(BlueprintReadOnly, Replicated, Category = "Favor")
	TArray<FDCCFavorChange> RecentChanges;

	UPROPERTY(EditAnywhere, Category = "Favor", meta = (ClampMin = "1"))
	int32 MaxLedgerEntries = 24;

private:
	void RecordChange(int32 Delta, EDCCFavorEvent Reason, const FText& Note, EDCCFavorTier OldTier);
};
