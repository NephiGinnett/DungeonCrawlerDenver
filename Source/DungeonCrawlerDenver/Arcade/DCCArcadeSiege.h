#pragma once

#include "CoreMinimal.h"
#include "Arcade/DCCArcadeArena.h"
#include "Items/DCCLootTypes.h"
#include "DCCArcadeSiege.generated.h"

class ADCCCharacterBase;
class ADCCEnemyCharacter;
class APlayerState;
class UNiagaraSystem;

/** One body waiting to step onto the road. */
struct FDCCSiegeSpawn
{
	FString Kind;
	FString Name;
	int32 Hp = 40;
	int32 Wave = 1;
	float Speed = 320.0f;
	float Gap = 0.65f;
};

/** One player's take from this round. Paid tickets pop as they happen. Salvage and the wave bonus wait for the end. */
struct FDCCSiegeScore
{
	TWeakObjectPtr<APlayerState> Who;
	int32 Kills = 0;
	int32 Tickets = 0;
	int32 Salvage = 0;
};

/** Crystal channel, power-up, and cooldown for one crawler. */
struct FDCCSiegeCaster
{
	TWeakObjectPtr<ADCCCharacterBase> Who;
	int32 Tower = -1;
	float Charge = 0.0f;
	bool bChanneling = false;
	double ReadyAt = 0.0;
	FName Buff;
	int32 BuffUntilWave = 0;
	bool bLargeMeshes = false;
};

struct FDCCSiegeCamp
{
	FVector At = FVector::ZeroVector;
	TWeakObjectPtr<ADCCEnemyCharacter> Live;
	int32 DeadOnWave = -99;
	int32 Clears = 0;
};

/**
 * Dungeon Crawler Tower Defense. The players are the towers.
 * One road, one wall, four crystal towers, two camps.
 */
UCLASS()
class DUNGEONCRAWLERDENVER_API ADCCArcadeSiege : public ADCCArcadeArena
{
	GENERATED_BODY()

public:
	ADCCArcadeSiege();

	virtual FName GetGameId() const override { return TEXT("Siege"); }
	virtual bool ShowsSpellBelt() const override { return true; }
	virtual bool KeepsStatusPanel() const override { return true; }
	virtual FString GetTip() const override;

	static ADCCArcadeSiege* Get(UWorld* World);

	bool HasBuff(const ADCCCharacterBase* Who, FName Buff) const;
	int32 CurrentWaveNumber() const { return CurrentWave; }
	void NoteHit(ADCCEnemyCharacter* Victim, ADCCCharacterBase* By);
	void ApplyCold(ADCCEnemyCharacter* Victim);

	UFUNCTION()
	void OnFighterDied(ADCCCharacterBase* Who);

	UFUNCTION(NetMulticast, Unreliable)
	void Multicast_Announce(const FString& Moment);
	void Multicast_Announce_Implementation(const FString& Moment);

	UFUNCTION(NetMulticast, Unreliable)
	void Multicast_CrystalCharge(ADCCCharacterBase* Who);
	void Multicast_CrystalCharge_Implementation(ADCCCharacterBase* Who);

	UFUNCTION(NetMulticast, Unreliable)
	void Multicast_CrystalFire(FVector From, FVector At);
	void Multicast_CrystalFire_Implementation(FVector From, FVector At);

	/** Dev commands. */
	void DevWave(int32 Wave);
	void DevWall(int32 Hp);
	void DevCamp(int32 Index);
	void DevPost();

protected:
	virtual void BuildLocal() override;
	virtual void ServerBegin() override;
	virtual void ServerTick(float DeltaSeconds) override;
	virtual void GetStartTransforms(TArray<FTransform>& Out) const override;
	virtual FSky GetSky() const override;
	virtual bool CanUse(const ADCCCharacterBase* Who) const override;
	virtual void Use(ADCCCharacterBase* Who) override;

private:
	struct FBody
	{
		FString Kind;
		FString Name;
		int32 Hp = 40;
		int32 Count = 1;
	};
	struct FWave
	{
		int32 Wave = 1;
		float Speed = 320.0f;
		float Gap = 0.65f;
		TArray<FBody> Bodies;
	};

	void LoadWaves();
	void BuildFallback(int32 Count);
	void StartWave(int32 Wave);
	void SpawnNext();
	ADCCEnemyCharacter* SpawnKind(const FString& Kind, const FVector& At, int32 Hp, const FString& Name, bool bMarcher);
	void DriveMarchers(float DeltaSeconds);
	void DriveCrystal(float DeltaSeconds);
	void DriveCamps();
	void UpdateBuffScales();
	void PayKill(ADCCEnemyCharacter* Victim);
	void OnCampDown(ADCCEnemyCharacter* Victim);
	void FireCrystal(FDCCSiegeCaster& Caster);
	void EndRound();
	void RefreshStatus();
	FDCCSiegeCaster* FindCaster(ADCCCharacterBase* Who, bool bCreate);
	bool PlayerNearRoad(const ADCCEnemyCharacter* Marcher) const;
	int32 NearestTower(const FVector& At, float Radius) const;
	int32 NearestPost(const FVector& At, float Radius) const;
	void GrantBuff(ADCCCharacterBase* Who, FName Buff);
	void RollHeldLoot();
	void ScaleHands(ADCCCharacterBase* Who, float Mul);
	UNiagaraSystem* LoadFx(const TCHAR* Path);

	TArray<FWave> Waves;
	TArray<FDCCSiegeSpawn> Queue;
	TArray<TWeakObjectPtr<ADCCEnemyCharacter>> Marchers;
	TArray<FVector> Road;
	TArray<FVector> TowerTops;
	TArray<FVector> Posts;
	TArray<FDCCSiegeCamp> Camps;
	TArray<FDCCSiegeCaster> Casters;
	TArray<FDCCSiegeScore> Scores;
	TArray<FDCCRolledLoot> Held;

	int32 CurrentWave = 0;
	int32 WavesCleared = 0;
	int32 WallHp = 2000;
	int32 WallMax = 2000;
	double WaveStartedAt = 0.0;
	double NextSpawnAt = 0.0;
	double NextWaveAt = 0.0;
	bool bWaveArmed = false;
	bool bWallWarned = false;
	bool bEnded = false;
	int32 CampGrant = 0;

	UPROPERTY(Transient)
	TObjectPtr<UNiagaraSystem> FxCharge;
	UPROPERTY(Transient)
	TObjectPtr<UNiagaraSystem> FxRelease;
	UPROPERTY(Transient)
	TObjectPtr<UNiagaraSystem> FxImpact;
};
