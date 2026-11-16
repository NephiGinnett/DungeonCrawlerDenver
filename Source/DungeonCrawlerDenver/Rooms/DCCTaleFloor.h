#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "DCCTaleFloor.generated.h"

class ADCCCharacterBase;
class ADCCEnemyCharacter;
class UBoxComponent;
class UStaticMesh;
class UDCCDungeonGrid;

/**
 * A whole dungeon floor, built when someone walks in.
 *
 * Same shape as a floor of Elyra's Tales: a start room with the way back up,
 * fight rooms, a chest, and the stairs in the room farthest from the start.
 * Three floors, then the stairs send the party home. The archway (and the
 * trial's ONWARD pillar) call Enter.
 *
 * Geometry is runtime cubes, not placed level actors. That is the part that
 * kept failing when floors were authored by hand into World Partition.
 */
UCLASS(meta = (DisplayName = "DCC Tale Floor"))
class DUNGEONCRAWLERDENVER_API ADCCTaleFloor : public AActor
{
	GENERATED_BODY()

public:
	ADCCTaleFloor();

	static ADCCTaleFloor* GetOrCreate(UWorld* World);

	/**
	 * Server only. Builds the floor on the first visit (and after a finished
	 * run) and teleports Crawler to the start room.
	 * ReturnPoint is where the surface pad and the last stairs send them.
	 */
	bool Enter(ADCCCharacterBase* Crawler, AActor* ReturnPoint);

	/**
	 * Server only. Called when a downed crawler's death saves run out. On this
	 * floor nobody dies for good: the System rewrites the ending (a narrator
	 * save, NarratorSavesPerRun per crawler per run), and once those are spent
	 * the crawler is sent home alive. Returns false when the crawler is not on
	 * the floor, so the normal death runs.
	 */
	static bool InterceptDeath(ADCCCharacterBase* Crawler);

	/** Server only. With dcc.AutoQuickPlay 1, sends a newly possessed crawler to floor 1 after a moment. */
	static void ScheduleQuickPlay(ADCCCharacterBase* Crawler);

	/** Server only. Issues the full HUD, marks onboarding done, and enters the floor. */
	static void QuickPlay(ADCCCharacterBase* Crawler);

	/** Server only. QuickPlay for every connected crawler not already on the floor. */
	static void QuickPlayEveryone(UWorld* World);

	/** Server only. Everyone connected (not already on the floor) goes straight into the run. */
	static void EnterEveryone(UWorld* World);

	/** Any machine. Whether a floor is built right now (the replicated layout says so). */
	static bool IsRunLive(const UWorld* World);

	virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;

protected:
	virtual void BeginPlay() override;

	UPROPERTY(VisibleAnywhere, Category = "Tale")
	TObjectPtr<UBoxComponent> UpPad;

	UPROPERTY(VisibleAnywhere, Category = "Tale")
	TObjectPtr<UBoxComponent> DownPad;

	UFUNCTION()
	void HandleEnemyDied(ADCCCharacterBase* Character);

	// --- The layout, replicated (2026-11-12) ---------------------------------
	// The floor's geometry is not replicated. The server sends a seed and a
	// floor number, and each client builds the same walls, lights and signs from
	// them. Spawned blocks have no replicated material colours, sign text or
	// light settings, and a client with no local floor collision falls through.
	UPROPERTY(ReplicatedUsing = OnRep_Layout)
	int32 LayoutVersion = 0;

	UPROPERTY(Replicated)
	int32 LayoutSeed = 0;

	UPROPERTY(Replicated)
	int32 LayoutFloor = 1;

	UPROPERTY(Replicated)
	bool bLayoutLive = false;

	UFUNCTION()
	void OnRep_Layout();

	UFUNCTION()
	void HandlePadOverlap(UPrimitiveComponent* OverlappedComponent, AActor* OtherActor,
		UPrimitiveComponent* OtherComp, int32 OtherBodyIndex, bool bFromSweep, const FHitResult& SweepResult);

private:
	// --- The run (Elyra's run.gd, cut down) ----------------------------------
	void BeginRun();
	void JoinRun(ADCCCharacterBase* Crawler);
	void SpawnStandIns();
	void DismissStandIns();
	void PayForFloor(const TArray<ADCCCharacterBase*>& Party);
	void FinishRun(bool bCleared, bool bFled);
	void CollectPlayers(TArray<ADCCCharacterBase*>& Out) const;
	void PlaceAtStart(ADCCCharacterBase* Crawler, int32 Slot);

	bool BuildFloor();
	void ClearBuilt();
	void TeleportToStart(ADCCCharacterBase* Crawler);
	void TeleportHome(ADCCCharacterBase* Crawler);
	void CollectParty(TArray<ADCCCharacterBase*>& Out) const;
	bool Contains(const FVector& WorldPoint) const;
	bool StairsAreOpen() const;
	void ArmPad(ADCCCharacterBase* Crawler);
	bool PadCooling(ADCCCharacterBase* Crawler) const;

	void SpawnBlock(const FVector& DesiredCenter, const FVector& WorldSize, const FLinearColor& Color);
	void SpawnFloorRect(int32 CellX, int32 CellY, int32 CellsWide, int32 CellsDeep, const FLinearColor& Color);
	void SpawnWalls();
	void SpawnLightsAndContent();

	/** Every machine: grid from the seed, then floors, walls, lights, crates and signs. */
	bool BuildGeometry(int32 Seed);
	void SpawnRoomDressing(int32 Seed);

	/** Where the dressing put crates, so the server does not spawn an enemy inside one. */
	TSet<FIntPoint> CrateCells;
	ADCCEnemyCharacter* SpawnEnemy(TSubclassOf<ADCCEnemyCharacter> Class, const FVector& Location, bool bBoss);
	void SpawnSign(const FVector& Location, const FString& Text, const FColor& Color);

	FVector CellLocation(FIntPoint Cell, float ZOffset) const;
	float FloorTopZ() const { return GetActorLocation().Z; }

	UPROPERTY(Transient)
	TObjectPtr<UDCCDungeonGrid> Grid;

	UPROPERTY(Transient)
	TObjectPtr<UStaticMesh> CubeMesh;

	UPROPERTY(Transient)
	TObjectPtr<UMaterialInterface> ShapeMaterial;

	UPROPERTY(Transient)
	TArray<TObjectPtr<AActor>> BuiltActors;

	TArray<TWeakObjectPtr<ADCCEnemyCharacter>> StairGuards;
	TMap<ADCCCharacterBase*, double> PadReadyAt;

	/** AI crawlers who fill empty party seats. They follow the party down and leave when the run ends. */
	TArray<TWeakObjectPtr<ADCCEnemyCharacter>> StandIns;

	/** Players who have set foot on this run. They get the results window. */
	TArray<TWeakObjectPtr<ADCCCharacterBase>> RunMembers;
	TMap<TWeakObjectPtr<ADCCCharacterBase>, int32> GoldAtStart;
	TMap<TWeakObjectPtr<ADCCCharacterBase>, int32> SavesUsed;
	int32 RunKills = 0;
	int32 FloorsCleared = 0;
	int32 NarratorSavesSpent = 0;
	double RunStartedAt = 0.0;

	/** Rewrites per crawler per run (Elyra: "she rewrites the story to save you, 1 to 3 times a tale"). */
	static constexpr int32 NarratorSavesPerRun = 2;
	/** Players plus stand-ins. Empty seats are filled up to this. */
	static constexpr int32 PartySeats = 3;

	FVector HomeLocation = FVector(-27600.0f, 26500.0f, 200.0f);
	FVector StartLocation = FVector::ZeroVector;
	int32 FloorNumber = 1;
	bool bBuilt = false;
	bool bRunComplete = false;
	float NoticeCooldownUntil = 0.0f;

	// No MaxFloors since 2026-11-13: runs are endless (roguelike); see PayForFloor.
	static constexpr float TileSize = 400.0f;
	static constexpr float WallHeight = 500.0f;
	static constexpr float FloorThickness = 40.0f;
	static constexpr float WallThickness = 40.0f;
};
