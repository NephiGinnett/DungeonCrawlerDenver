#pragma once

#include "CoreMinimal.h"
#include "Arcade/DCCArcadeArena.h"
#include "DCCArcadeRing.generated.h"

class ADCCCharacterBase;
class ADCCEnemyCharacter;
class APlayerController;

/** One step of the ladder, loaded from Content/DCC/Ring/ladder.json. */
struct FDCCRingStage
{
	int32 Stage = 1;
	FString Name;
	FString Kind;
	int32 Hp = 80;
	int32 Count = 1;
	FName Skill;
};

struct FDCCRingSeed
{
	FString Name;
	TWeakObjectPtr<APlayerController> PC;
	bool bBot = false;
	bool bChampion = false;
	bool bOut = false;
};

/**
 * The Champions' Ring. Clear stages, or step up for a short tournament.
 * Player-versus-player only happens between the two people in the live match.
 */
UCLASS()
class DUNGEONCRAWLERDENVER_API ADCCArcadeRing : public ADCCArcadeArena
{
	GENERATED_BODY()

public:
	ADCCArcadeRing();

	virtual FName GetGameId() const override { return TEXT("Ring"); }
	virtual bool ShowsSpellBelt() const override { return true; }
	virtual bool KeepsStatusPanel() const override { return true; }
	virtual FString GetTip() const override;
	virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;
	virtual bool PlayersMayFight(const ADCCCharacterBase* A, const ADCCCharacterBase* B) const override;
	virtual void OnRespawned(APlayerController* PC) override;

	void DevStage(int32 Stage);

	UFUNCTION()
	void OnFighterDied(ADCCCharacterBase* Who);

	UFUNCTION(NetMulticast, Unreliable)
	void Multicast_Announce(const FString& Moment);
	void Multicast_Announce_Implementation(const FString& Moment);

protected:
	virtual void BuildLocal() override;
	virtual void ServerBegin() override;
	virtual void ServerTick(float DeltaSeconds) override;
	virtual void GetStartTransforms(TArray<FTransform>& Out) const override;
	virtual FSky GetSky() const override;
	virtual bool CanUse(const ADCCCharacterBase* Who) const override;
	virtual void Use(ADCCCharacterBase* Who) override;

private:
	void LoadLadder();
	void BuildFallback();
	void StartStage(int32 Stage);
	void ClearStageEnemies();
	void OnStageClear();
	void GrantChampionSkill();
	void BeginTournament();
	void AdvanceBracket();
	void StartMatch(int32 SeedA, int32 SeedB);
	void FinishMatch(int32 WinnerSeed);
	void EndTournament(int32 WinnerSeed);
	void SendToStands(APlayerController* ExceptA, APlayerController* ExceptB);
	void PlaceFighter(APlayerController* PC, const FVector& At);
	ADCCEnemyCharacter* SpawnKind(const FString& Kind, const FVector& At, int32 Hp, const FString& Name);
	void RefreshStatus();
	bool StageClear() const;
	int32 SeedOf(const ADCCCharacterBase* Who) const;

	TArray<FDCCRingStage> Ladder;
	TArray<TWeakObjectPtr<ADCCEnemyCharacter>> Live;
	TArray<FDCCRingSeed> Seeds;
	TArray<int32> RoundWinners;
	TArray<TPair<int32, int32>> Matches;

	int32 Stage = 0;
	int32 BestBanked = 0;
	int32 MatchA = INDEX_NONE;
	int32 MatchB = INDEX_NONE;
	int32 MatchCursor = 0;
	int32 Humans = 0;
	double MatchEndsAt = 0.0;
	bool bStageLive = false;
	bool bTournament = false;
	bool bMatchLive = false;
	bool bAwaitingBodies = false;
	bool bEnded = false;

	UPROPERTY(Replicated)
	bool bPvpLive = false;

	/** Replicated so a client's hostility check sees the same pair the server is scoring. */
	UPROPERTY(Replicated)
	TObjectPtr<ADCCCharacterBase> FighterA;

	UPROPERTY(Replicated)
	TObjectPtr<ADCCCharacterBase> FighterB;
};
