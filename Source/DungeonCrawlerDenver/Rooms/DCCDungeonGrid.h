#pragma once

#include "CoreMinimal.h"
#include "UObject/Object.h"
#include "DCCDungeonGrid.generated.h"

/**
 * One floor's layout, ported from Elyra's Tales (game/scripts/dungeon.gd).
 *
 * Rooms are placed with a gap between them. The first is the start. The one
 * farthest from it is the stairs. The rest are shuffled. Corridors join each
 * room to the nearest room already joined, then a couple of loops, then a
 * direct corridor from the start to the stairs so a floor is never only a maze.
 *
 * Enemies walk this grid. A generated floor has no baked navmesh, and a
 * runtime nav rebuild is what kept the older frontier generator's fights
 * standing still.
 */
UCLASS()
class DUNGEONCRAWLERDENVER_API UDCCDungeonGrid : public UObject
{
	GENERATED_BODY()

public:
	struct FRoom
	{
		int32 X = 0;
		int32 Y = 0;
		int32 W = 0;
		int32 H = 0;
		FName Kind;
		FIntPoint Center() const { return FIntPoint(X + W / 2, Y + H / 2); }
	};

	void SetOrigin(const FVector& InOrigin, float InTileSize);

	/** start, then FightRooms of "fight", ChestRooms of "chest", then "stairs". */
	/**
	 * How a layout is shaped. The defaults are the World Dungeon's: big rooms joined to their nearest neighbour, a couple
	 * of loops, and a shortcut. Dead Air's mine (2026-11-16, Zoe: "make the mine shaft in dead air much more linear.
	 * Many spaces now felt like a big square") uses small rooms strung on one winding drift.
	 */
	struct FStyle
	{
		int32 RoomMin = 5, RoomMax = 7;
		int32 GrandMin = 7, GrandMax = 9;
		/** Join each room to the one before it (a chain) rather than to the nearest joined room. */
		bool bChain = false;
		int32 Loops = 2;
		bool bShortcut = true;
	};
	bool Build(int32 InSize, int32 InSeed, int32 FightRooms, int32 ChestRooms, const FStyle& Style = FStyle());

	int32 GetSize() const { return Size; }
	float GetTileSize() const { return TileSize; }
	const FVector& GetOrigin() const { return Origin; }
	const TArray<FRoom>& GetRooms() const { return Rooms; }
	int32 GetSeed() const { return Seed; }

	bool IsOpen(int32 X, int32 Y) const;
	int32 RoomIndexAt(int32 X, int32 Y) const;

	FIntPoint CellOf(const FVector& World) const;
	FVector CellCenter(FIntPoint Cell) const;

	/**
	 * World point to walk toward next. False when From and Goal are on the
	 * same cell or no path exists; the caller then walks straight at the goal.
	 */
	bool NextWaypoint(const FVector& From, const FVector& Goal, FVector& OutWaypoint) const;

	int32 RandRange(int32 Min, int32 Max);
	float FRand();

	int32 CountOpen() const;

private:
	bool InBounds(int32 X, int32 Y) const;
	void SetOpen(int32 X, int32 Y);
	void Dig(int32 X, int32 Y);
	void Corridor(FIntPoint A, FIntPoint B);
	bool FindPath(FIntPoint Start, FIntPoint Goal, TArray<FIntPoint>& OutPath) const;
	FIntPoint NearestOpen(FIntPoint Cell) const;

	FVector Origin = FVector::ZeroVector;
	float TileSize = 400.0f;
	int32 Size = 0;
	int32 Seed = 0;
	TArray<uint8> Cells;
	TArray<int32> RoomAt;
	TArray<FRoom> Rooms;
	FRandomStream Stream;
};
