#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "Rooms/DCCRoomTypes.h"
#include "Templates/SubclassOf.h"
#include "DCCDungeonGenerator.generated.h"

class ADCCRoom;
class ADCCEnemyCharacter;
class UDataTable;
class UStaticMesh;
class UStaticMeshComponent;
class ADCCDialogueNPC;
class USkeletalMesh;

// ---------------------------------------------------------------------------
// A GRID OF ROOMS THAT DOES NOT EXIST YET
//
// Floor 1 is - deliberately - entirely hand-authored: seven specific rooms,
// a named boss, a tone bible. That approach does not scale to "10-20 rooms to
// explore" on demand, and was never meant to; this is a second, parallel way
// to get content into the world; one level over from Floor 1's, not a
// replacement for it.
//
// The unit of generation is a grid cell (CellSize apart), each holding one
// square room (RoomSize across, centred in its cell) with 1-4 doorways on
// its cardinal sides. A cell's neighbours do not exist until a player
// actually walks toward an open doorway - see ADCCDungeonFrontierTrigger -
// which is what makes this "as they come up" rather than a big generation
// pass at level load. The topology is a tree, not a graph: a room only ever
// opens into a cell nothing has claimed yet, so there is no cycle-detection
// or loop-stitching to get wrong.
//
// Every room type spawns something with real presence, not just a decorated
// box: Combat gets an ADCCEncounter, Loot Vault rolls the new loot table
// system (equipment included), Narrative places a talking ADCCDialogueNPC,
// and Puzzle gets a crate stack tall enough to be worth climbing - which
// costs nothing extra thanks to the universal-surface climbing system, not a
// bespoke puzzle mechanic. What stays deliberately out of scope is bespoke
// PER-ROOM writing: the Narrative NPC speaks lines written to work in any
// generated room (DT_DCCDialogue's "Wanderer" rows), not the specific,
// escalating-grievance voice Floor 1's hand-authored NPCs use. That kind of
// room-specific authorship is exactly what stays a hand-building job.
// ---------------------------------------------------------------------------

UCLASS()
class DUNGEONCRAWLERDENVER_API ADCCDungeonGenerator : public AActor
{
	GENERATED_BODY()

public:
	ADCCDungeonGenerator();

	// --- Layout ---

	/** World units between adjacent cell centres. Must exceed RoomSize, so a corridor stub fits between rooms. */
	UPROPERTY(EditAnywhere, Category = "Dungeon|Layout", meta = (ClampMin = "100.0"))
	float CellSize = 2600.0f;

	/** The square footprint of one room, centred in its cell. */
	UPROPERTY(EditAnywhere, Category = "Dungeon|Layout", meta = (ClampMin = "100.0"))
	float RoomSize = 1600.0f;

	UPROPERTY(EditAnywhere, Category = "Dungeon|Layout", meta = (ClampMin = "100.0"))
	float WallHeight = 400.0f;

	/** Total rooms this generator will ever build, start room included. */
	UPROPERTY(EditAnywhere, Category = "Dungeon|Layout", meta = (ClampMin = "1"))
	int32 MaxRooms = 15;

	/** 0 randomizes the layout every session; any other value reproduces the same layout. */
	UPROPERTY(EditAnywhere, Category = "Dungeon|Layout")
	int32 Seed = 0;

	/** Passed to every spawned ADCCEncounter, for the floor-clear payout logic. */
	UPROPERTY(EditAnywhere, Category = "Dungeon|Layout", meta = (ClampMin = "1"))
	int32 FloorNumber = 1;

	/** Server only. Generates the start room. Called from BeginPlay unless bAutoStartOnBeginPlay is false. */
	UFUNCTION(BlueprintCallable, Category = "Dungeon")
	void GenerateStartRoom();

	/**
	 * Server only. Builds the room at Cell, entered from EntryDirection.
	 *
	 * No-ops if Cell is already built (defends against a trigger firing twice
	 * before it destroys itself) or the room budget is spent. This is what
	 * ADCCDungeonFrontierTrigger calls when a player reaches an open doorway.
	 */
	UFUNCTION(BlueprintCallable, Category = "Dungeon")
	void GenerateRoomAt(FIntPoint Cell, EDCCCardinalDirection EntryDirection);

	// --- Combat population ---

	/** Classes drawn from at random to fill a Combat room. Defaulted to the existing floor mobs; edit to retune. */
	UPROPERTY(EditAnywhere, Category = "Dungeon|Combat")
	TArray<TSubclassOf<ADCCEnemyCharacter>> CombatEnemyPool;

	UPROPERTY(EditAnywhere, Category = "Dungeon|Combat", meta = (ClampMin = "1"))
	int32 MinEnemiesPerRoom = 2;

	UPROPERTY(EditAnywhere, Category = "Dungeon|Combat", meta = (ClampMin = "1"))
	int32 MaxEnemiesPerRoom = 4;

	// --- Loot population ---

	/** Table of FDCCLootTableRow rows - see DCCLootTypes.h. Left unset, Loot Vault rooms generate empty. */
	UPROPERTY(EditAnywhere, Category = "Dungeon|Loot")
	TObjectPtr<UDataTable> LootTablesAsset;

	/** Row names eligible to fill a Loot Vault room. One is picked at random per room. */
	UPROPERTY(EditAnywhere, Category = "Dungeon|Loot")
	TArray<FName> LootVaultTableRows;

	// --- Narrative population ---

	/**
	 * SpeakerId a generated Narrative room's NPC uses, matched against
	 * DT_DCCDialogue the same way every other ADCCDialogueNPC works. This is
	 * one shared speaker rather than a pool, on purpose: their lines are
	 * written to be sayable in any generated room (see the DT_DCCDialogue rows
	 * added alongside this system), which is what a hand-authored Floor 1 NPC
	 * cannot be - Floor 1's dialogue is written for one specific room.
	 */
	UPROPERTY(EditAnywhere, Category = "Dungeon|Narrative")
	FName NarrativeSpeakerId = TEXT("Wanderer");

protected:
	virtual void BeginPlay() override;

	/** Off if something else (a quest trigger, a DM command) should decide when generation starts. */
	UPROPERTY(EditAnywhere, Category = "Dungeon")
	bool bAutoStartOnBeginPlay = true;

private:
	FVector CellCenter(FIntPoint Cell) const;
	EDCCRoomType PickRoomType();

	/** Spawns floor, ceiling and all four walls (gapped on open sides), plus a corridor stub per open side. */
	ADCCRoom* BuildRoomShell(FIntPoint Cell, const TSet<EDCCCardinalDirection>& OpenDirections, EDCCRoomType Type);

	/** One side's wall, or two segments with a doorway gap between them if bOpen. */
	void BuildWallSide(const FVector& Center, EDCCCardinalDirection Side, bool bOpen);

	/** A short floor strip from this room's wall out to the cell boundary, covering this room's half of the gap to its neighbour. */
	void SpawnCorridorStub(const FVector& Center, EDCCCardinalDirection Direction);

	void SpawnFrontierTrigger(ADCCRoom* FromRoom, FIntPoint FromCell, EDCCCardinalDirection OpenDirection);

	void PopulateRoom(ADCCRoom* Room, EDCCRoomType Type, const FVector& Center);
	void PopulateCombatRoom(ADCCRoom* Room, const FVector& Center);
	void PopulateLootRoom(const FVector& Center);
	void PopulateSafeRoom(const FVector& Center);
	void PopulateNarrativeRoom(const FVector& Center);
	void PopulatePuzzleRoom(const FVector& Center);
	void ScatterProps(const FVector& Center, UStaticMesh* Mesh, int32 Count);

	/** A stack of crates tall enough to be worth climbing - see the class comment on why this counts as the Puzzle room's content. */
	void SpawnClimbableStack(const FVector& Center);

	ADCCDialogueNPC* SpawnDialogueNPC(const FVector& Location, FName SpeakerId);

	AActor* SpawnMeshActor(UStaticMesh* Mesh, const FVector& Location, const FRotator& Rotation, const FVector& Scale);

	UPROPERTY()
	TObjectPtr<UStaticMesh> CubeMesh;

	UPROPERTY()
	TObjectPtr<UStaticMesh> PropMeshA;

	UPROPERTY()
	TObjectPtr<UStaticMesh> PropMeshB;

	UPROPERTY()
	TObjectPtr<UStaticMesh> PropMeshC;

	/** Mesh for a generated Narrative room's speaker - see SpawnDialogueNPC for why it is the mannequin, not a crawler skeleton. */
	UPROPERTY()
	TObjectPtr<USkeletalMesh> WandererMesh;

	/**
	 * Not a UPROPERTY. Spawned rooms are already GC-rooted by the level's own
	 * actor list, and FIntPoint as a reflected TMap key is untested territory
	 * this class does not need to be the one to find problems with.
	 */
	TMap<FIntPoint, ADCCRoom*> Rooms;

	int32 RoomsGenerated = 0;
	FRandomStream RandomStream;
};
