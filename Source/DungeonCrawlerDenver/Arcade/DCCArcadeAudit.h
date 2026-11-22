#pragma once

#include "CoreMinimal.h"
#include "Arcade/DCCArcadeArena.h"
#include "Characters/DCCEnemyCharacter.h"
#include "Stats/DCCStatsComponent.h"
#include "DCCArcadeAudit.generated.h"

class ADCCCharacterBase;

/**
 * The construct Stress Test puts in the lights. It does not fight, and it does
 * not fall over: the round is a number, not a kill. The mannequin and the
 * capsule are set up in the constructor so a client's aim trace hits the same
 * body the server scores.
 */
UCLASS()
class DUNGEONCRAWLERDENVER_API ADCCAuditConstruct : public ADCCEnemyCharacter
{
	GENERATED_BODY()

public:
	ADCCAuditConstruct();

protected:
	virtual void BeginPlay() override;
};

/**
 * Stress Test (the Den, 2026-11-14): a damage check. The party has forty
 * seconds on The Quarterly, a construct that records everything and dies of
 * nothing. Fists, weapons, spells, thrown props and explosives all count,
 * because they all already arrive through ApplyDamage.
 *
 * The goal is the party's total, scaled by how many crawlers showed up, so a
 * solo round and a full table ask the same thing of each person. Three ratings:
 * Meets Expectations, Exceeds Expectations, Standing Ovation. Tickets pay the
 * rating plus a little for what you personally landed. The Den board keeps
 * your best personal total (DCCProfile "Audit.BestDamage").
 */
UCLASS()
class DUNGEONCRAWLERDENVER_API ADCCArcadeAudit : public ADCCArcadeArena
{
	GENERATED_BODY()

public:
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;

	virtual FName GetGameId() const override { return TEXT("Audit"); }
	virtual FString GetTip() const override { return TEXT("Hit The Quarterly. Fists, weapons and spells all count."); }
	virtual bool KeepsStatusPanel() const override { return true; }

	static constexpr float StandbySeconds = 4.0f;
	static constexpr float RoundSeconds = 40.0f;
	static constexpr int32 MeetsEach = 80;
	static constexpr int32 ExceedsEach = 180;
	static constexpr int32 OvationEach = 340;

protected:
	virtual void ServerBegin() override;
	virtual void ServerTick(float DeltaSeconds) override;
	virtual void BuildLocal() override;
	virtual void GetStartTransforms(TArray<FTransform>& Out) const override;
	virtual FSky GetSky() const override;

private:
	struct FRecentHit
	{
		double Time = 0.0;
		int32 Amount = 0;
	};

	void SpawnConstruct();
	void KeepConstructStanding();
	void OnDamageLanded(ADCCCharacterBase* Dealer, ADCCCharacterBase* Target, int32 Amount);
	void RefreshStatus();
	void Wrap();

	int32 Goal(int32 Each) const { return Each * FMath::Max(1, RatedPlayers); }
	int32 Tier() const;

	UFUNCTION()
	void OnConstructHurt(UDCCStatsComponent* Stats, int32 NewHitPoints, int32 Delta);

	UPROPERTY()
	TObjectPtr<ADCCAuditConstruct> Construct;

	TMap<FString, int32> ByPlayer;
	TArray<FRecentHit> Recent;

	int32 PartyDamage = 0;
	int32 RatedPlayers = 1;
	bool bRosterLocked = false;
	bool bClockRunning = false;
	bool bWrapped = false;
	double LiveAt = 0.0;
	double EndsAt = 0.0;

	static constexpr int32 ConstructHitPoints = 1000000;
};
