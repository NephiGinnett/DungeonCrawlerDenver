#include "Rooms/DCCDungeonGrid.h"

#include "Algo/Reverse.h"

void UDCCDungeonGrid::SetOrigin(const FVector& InOrigin, float InTileSize)
{
	Origin = InOrigin;
	TileSize = FMath::Max(InTileSize, 50.0f);
}

bool UDCCDungeonGrid::InBounds(int32 X, int32 Y) const
{
	return X >= 0 && Y >= 0 && X < Size && Y < Size;
}

bool UDCCDungeonGrid::IsOpen(int32 X, int32 Y) const
{
	return InBounds(X, Y) && Cells[Y * Size + X] == 0;
}

int32 UDCCDungeonGrid::RoomIndexAt(int32 X, int32 Y) const
{
	return InBounds(X, Y) ? RoomAt[Y * Size + X] : -1;
}

void UDCCDungeonGrid::SetOpen(int32 X, int32 Y)
{
	if (InBounds(X, Y))
	{
		Cells[Y * Size + X] = 0;
	}
}

int32 UDCCDungeonGrid::RandRange(int32 Min, int32 Max)
{
	return Stream.RandRange(Min, Max);
}

float UDCCDungeonGrid::FRand()
{
	return Stream.FRand();
}

int32 UDCCDungeonGrid::CountOpen() const
{
	int32 Count = 0;
	for (uint8 Cell : Cells)
	{
		Count += Cell == 0 ? 1 : 0;
	}
	return Count;
}

FIntPoint UDCCDungeonGrid::CellOf(const FVector& World) const
{
	const FVector Local = World - Origin;
	return FIntPoint(
		FMath::FloorToInt(Local.X / TileSize),
		FMath::FloorToInt(Local.Y / TileSize));
}

FVector UDCCDungeonGrid::CellCenter(FIntPoint Cell) const
{
	return FVector(Origin.X + (Cell.X + 0.5f) * TileSize, Origin.Y + (Cell.Y + 0.5f) * TileSize, Origin.Z);
}

void UDCCDungeonGrid::Dig(int32 X, int32 Y)
{
	// Two cells wide, same as Elyra. A one-wide corridor jams a capsule.
	const FIntPoint Deltas[] = { FIntPoint(0, 0), FIntPoint(1, 0), FIntPoint(0, 1), FIntPoint(1, 1) };
	for (const FIntPoint& Delta : Deltas)
	{
		const int32 PX = X + Delta.X;
		const int32 PY = Y + Delta.Y;
		if (PX > 0 && PY > 0 && PX < Size - 1 && PY < Size - 1)
		{
			SetOpen(PX, PY);
		}
	}
}

void UDCCDungeonGrid::Corridor(FIntPoint A, FIntPoint B)
{
	FIntPoint Cursor = A;
	const bool bHorizontalFirst = Stream.FRand() < 0.5f;
	for (int32 Leg = 0; Leg < 2; ++Leg)
	{
		if ((Leg == 0) == bHorizontalFirst)
		{
			while (Cursor.X != B.X)
			{
				Cursor.X += FMath::Sign(B.X - Cursor.X);
				Dig(Cursor.X, Cursor.Y);
			}
		}
		else
		{
			while (Cursor.Y != B.Y)
			{
				Cursor.Y += FMath::Sign(B.Y - Cursor.Y);
				Dig(Cursor.X, Cursor.Y);
			}
		}
	}
}

bool UDCCDungeonGrid::Build(int32 InSize, int32 InSeed, int32 FightRooms, int32 ChestRooms, const FStyle& Style)
{
	Size = FMath::Clamp(InSize, 16, 48);
	Seed = InSeed;
	Stream.Initialize(Seed);

	Cells.Init(1, Size * Size);
	RoomAt.Init(-1, Size * Size);
	Rooms.Reset();

	TArray<FName> Want;
	Want.Add(TEXT("start"));
	for (int32 i = 0; i < FightRooms; ++i)
	{
		Want.Add(TEXT("fight"));
	}
	for (int32 i = 0; i < ChestRooms; ++i)
	{
		Want.Add(TEXT("chest"));
	}
	Want.Add(TEXT("stairs"));

	TArray<FRoom> Placed;
	int32 Tries = 0;
	while (Placed.Num() < Want.Num() && Tries < 4000)
	{
		++Tries;
		const bool bGrand = Placed.Num() == Want.Num() - 1;
		// The first room (the start) is never smaller than 3 cells: Dead Air's cage stands in it.
		const int32 Lo = Placed.Num() == 0 ? FMath::Max(Style.RoomMin, 3) : Style.RoomMin;
		const int32 Hi = Placed.Num() == 0 ? FMath::Max(Style.RoomMax, 3) : Style.RoomMax;
		const int32 W = bGrand ? Stream.RandRange(Style.GrandMin, Style.GrandMax) : Stream.RandRange(Lo, Hi);
		const int32 H = bGrand ? Stream.RandRange(Style.GrandMin, Style.GrandMax) : Stream.RandRange(Lo, Hi);
		if (W >= Size - 2 || H >= Size - 2)
		{
			continue;
		}

		FRoom Candidate;
		Candidate.X = Stream.RandRange(1, Size - W - 1);
		Candidate.Y = Stream.RandRange(1, Size - H - 1);
		Candidate.W = W;
		Candidate.H = H;

		bool bFits = true;
		for (const FRoom& Other : Placed)
		{
			// One cell of wall between rooms, so they don't melt into one hall.
			const bool bOverlapX = Candidate.X - 1 < Other.X + Other.W && Candidate.X + Candidate.W + 1 > Other.X;
			const bool bOverlapY = Candidate.Y - 1 < Other.Y + Other.H && Candidate.Y + Candidate.H + 1 > Other.Y;
			if (bOverlapX && bOverlapY)
			{
				bFits = false;
				break;
			}
		}

		if (bFits)
		{
			Placed.Add(Candidate);
		}
	}

	if (Placed.Num() < 2)
	{
		UE_LOG(LogTemp, Error, TEXT("[DCC Tale] room placement failed (seed %d, placed %d)"), Seed, Placed.Num());
		return false;
	}

	// Start is the first room placed. Stairs go in the room farthest from it.
	FRoom Start = Placed[0];
	TArray<FRoom> Rest;
	for (int32 i = 1; i < Placed.Num(); ++i)
	{
		Rest.Add(Placed[i]);
	}
	Rest.Sort([&Start](const FRoom& A, const FRoom& B)
	{
		const float DA = FVector2D::DistSquared(FVector2D(A.Center().X, A.Center().Y), FVector2D(Start.Center().X, Start.Center().Y));
		const float DB = FVector2D::DistSquared(FVector2D(B.Center().X, B.Center().Y), FVector2D(Start.Center().X, Start.Center().Y));
		return DA < DB;
	});

	if (Style.bChain && Rest.Num() > 1)
	{
		// A walk: from the start, always on to the nearest room not yet visited, so consecutive rooms are neighbours
		// and the corridor between each pair is short. The last one reached is the deepest.
		TArray<FRoom> Walk;
		FRoom Here = Start;
		TArray<FRoom> Left = Rest;
		while (Left.Num() > 0)
		{
			int32 Best = 0;
			float BestD = TNumericLimits<float>::Max();
			for (int32 k = 0; k < Left.Num(); ++k)
			{
				const float D = FVector2D::DistSquared(FVector2D(Left[k].Center().X, Left[k].Center().Y), FVector2D(Here.Center().X, Here.Center().Y));
				if (D < BestD) { BestD = D; Best = k; }
			}
			Here = Left[Best];
			Walk.Add(Here);
			Left.RemoveAt(Best);
		}
		Rest = Walk;
	}
	const FRoom Far = Rest.Pop();
	TArray<FName> Middle = Want;
	Middle.RemoveAt(0);
	Middle.Pop();
	for (int32 i = Middle.Num() - 1; i > 0; --i)
	{
		Middle.Swap(i, Stream.RandRange(0, i));
	}

	TArray<FRoom> Order;
	Order.Add(Start);
	Order.Append(Rest);
	Order.Add(Far);

	TArray<FName> Kinds;
	Kinds.Add(TEXT("start"));
	for (int32 i = 0; i < Rest.Num(); ++i)
	{
		Kinds.Add(i < Middle.Num() ? Middle[i] : FName(TEXT("fight")));
	}
	Kinds.Add(TEXT("stairs"));

	for (int32 i = 0; i < Order.Num(); ++i)
	{
		FRoom Room = Order[i];
		Room.Kind = Kinds[i];
		Rooms.Add(Room);
		for (int32 Y = Room.Y; Y < Room.Y + Room.H; ++Y)
		{
			for (int32 X = Room.X; X < Room.X + Room.W; ++X)
			{
				SetOpen(X, Y);
				RoomAt[Y * Size + X] = i;
			}
		}
	}

	TArray<int32> Joined;
	Joined.Add(0);
	for (int32 i = 1; i < Rooms.Num(); ++i)
	{
		if (Style.bChain)
		{
			Corridor(Rooms[i - 1].Center(), Rooms[i].Center()); // one drift, room after room
			Joined.Add(i);
			continue;
		}
		int32 Best = 0;
		float BestDistance = TNumericLimits<float>::Max();
		for (int32 J : Joined)
		{
			const float D = FVector2D::Distance(
				FVector2D(Rooms[i].Center().X, Rooms[i].Center().Y),
				FVector2D(Rooms[J].Center().X, Rooms[J].Center().Y));
			if (D < BestDistance)
			{
				BestDistance = D;
				Best = J;
			}
		}
		Corridor(Rooms[Best].Center(), Rooms[i].Center());
		Joined.Add(i);
	}

	for (int32 Loop = 0; Loop < Style.Loops && Rooms.Num() > 2; ++Loop)
	{
		const int32 A = Stream.RandRange(1, Rooms.Num() - 1);
		const int32 B = Stream.RandRange(1, Rooms.Num() - 1);
		if (A != B)
		{
			Corridor(Rooms[A].Center(), Rooms[B].Center());
		}
	}

	if (Style.bShortcut)
	{
		Corridor(Rooms[0].Center(), Rooms.Last().Center());
	}

	TArray<FIntPoint> Path;
	if (!FindPath(Rooms[0].Center(), Rooms.Last().Center(), Path))
	{
		UE_LOG(LogTemp, Error, TEXT("[DCC Tale] start does not reach the stairs (seed %d)"), Seed);
		return false;
	}

	UE_LOG(LogTemp, Log, TEXT("[DCC Tale] layout seed %d, %d rooms, %d open cells"), Seed, Rooms.Num(), CountOpen());
	return true;
}

FIntPoint UDCCDungeonGrid::NearestOpen(FIntPoint Cell) const
{
	if (IsOpen(Cell.X, Cell.Y))
	{
		return Cell;
	}

	for (int32 Radius = 1; Radius <= 4; ++Radius)
	{
		for (int32 Y = Cell.Y - Radius; Y <= Cell.Y + Radius; ++Y)
		{
			for (int32 X = Cell.X - Radius; X <= Cell.X + Radius; ++X)
			{
				if (IsOpen(X, Y))
				{
					return FIntPoint(X, Y);
				}
			}
		}
	}
	return FIntPoint(-1, -1);
}

bool UDCCDungeonGrid::FindPath(FIntPoint Start, FIntPoint Goal, TArray<FIntPoint>& OutPath) const
{
	OutPath.Reset();
	Start = NearestOpen(Start);
	Goal = NearestOpen(Goal);
	if (Start.X < 0 || Goal.X < 0)
	{
		return false;
	}
	if (Start == Goal)
	{
		OutPath.Add(Start);
		return true;
	}

	const int32 N = Size * Size;
	TArray<int32> GCost;
	TArray<int32> Parent;
	TArray<uint8> Closed;
	GCost.Init(MAX_int32, N);
	Parent.Init(-1, N);
	Closed.Init(0, N);

	auto IndexOf = [this](FIntPoint P) { return P.Y * Size + P.X; };
	auto Heuristic = [](FIntPoint A, FIntPoint B)
	{
		const int32 DX = FMath::Abs(A.X - B.X);
		const int32 DY = FMath::Abs(A.Y - B.Y);
		return 10 * (DX + DY) + 4 * FMath::Min(DX, DY);
	};

	TArray<FIntPoint> Open;
	Open.Add(Start);
	GCost[IndexOf(Start)] = 0;

	const FIntPoint Deltas[] = {
		FIntPoint(1, 0), FIntPoint(-1, 0), FIntPoint(0, 1), FIntPoint(0, -1),
		FIntPoint(1, 1), FIntPoint(1, -1), FIntPoint(-1, 1), FIntPoint(-1, -1)
	};

	while (Open.Num() > 0)
	{
		int32 BestSlot = 0;
		int32 BestF = MAX_int32;
		for (int32 i = 0; i < Open.Num(); ++i)
		{
			const int32 F = GCost[IndexOf(Open[i])] + Heuristic(Open[i], Goal);
			if (F < BestF)
			{
				BestF = F;
				BestSlot = i;
			}
		}

		const FIntPoint Current = Open[BestSlot];
		Open.RemoveAtSwap(BestSlot);
		const int32 CurrentIndex = IndexOf(Current);
		if (Closed[CurrentIndex])
		{
			continue;
		}
		Closed[CurrentIndex] = 1;

		if (Current == Goal)
		{
			for (FIntPoint Step = Goal; Step.X >= 0; )
			{
				OutPath.Add(Step);
				const int32 P = Parent[IndexOf(Step)];
				if (P < 0)
				{
					break;
				}
				Step = FIntPoint(P % Size, P / Size);
			}
			Algo::Reverse(OutPath);
			return true;
		}

		for (const FIntPoint& Delta : Deltas)
		{
			const FIntPoint Next(Current.X + Delta.X, Current.Y + Delta.Y);
			if (!IsOpen(Next.X, Next.Y) || Closed[IndexOf(Next)])
			{
				continue;
			}

			const bool bDiagonal = Delta.X != 0 && Delta.Y != 0;
			if (bDiagonal && (!IsOpen(Current.X + Delta.X, Current.Y) || !IsOpen(Current.X, Current.Y + Delta.Y)))
			{
				continue;
			}

			const int32 StepCost = bDiagonal ? 14 : 10;
			const int32 NewG = GCost[CurrentIndex] + StepCost;
			const int32 NextIndex = IndexOf(Next);
			if (NewG >= GCost[NextIndex])
			{
				continue;
			}

			GCost[NextIndex] = NewG;
			Parent[NextIndex] = CurrentIndex;
			Open.Add(Next);
		}
	}

	return false;
}

bool UDCCDungeonGrid::NextWaypoint(const FVector& From, const FVector& Goal, FVector& OutWaypoint) const
{
	const FIntPoint Start = NearestOpen(CellOf(From));
	const FIntPoint End = NearestOpen(CellOf(Goal));
	if (Start.X < 0 || End.X < 0 || Start == End)
	{
		return false;
	}

	TArray<FIntPoint> Path;
	if (!FindPath(Start, End, Path) || Path.Num() < 2)
	{
		return false;
	}

	int32 Index = 1;
	const FVector First = CellCenter(Path[1]);
	if (Path.Num() > 2 && FVector::Dist2D(From, First) < TileSize * 0.35f)
	{
		Index = 2;
	}

	OutWaypoint = CellCenter(Path[Index]);
	OutWaypoint.Z = From.Z;
	return true;
}
