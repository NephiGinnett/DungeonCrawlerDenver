#include "Rooms/DCCDungeonGenerator.h"

#include "Rooms/DCCRoom.h"
#include "Rooms/DCCEncounter.h"
#include "Rooms/DCCDungeonFrontierTrigger.h"
#include "Characters/DCCEnemyCharacter.h"
#include "Dialogue/DCCDialogueNPC.h"
#include "Components/SkeletalMeshComponent.h"
#include "Engine/SkeletalMesh.h"
#include "Characters/DCCEnemyCharacter_DebtEater.h"
#include "Items/DCCLootTableRoller.h"
#include "Components/BoxComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Engine/StaticMeshActor.h"
#include "Engine/StaticMesh.h"
#include "Engine/World.h"
#include "UObject/ConstructorHelpers.h"

ADCCDungeonGenerator::ADCCDungeonGenerator()
{
	PrimaryActorTick.bCanEverTick = false;

	// No client-relevant state of its own - every actor it spawns replicates
	// itself, the same as ADCCEncounter's own on-demand enemy spawns already
	// do. Nothing here needs an RPC.
	bReplicates = false;

	RootComponent = CreateDefaultSubobject<USceneComponent>(TEXT("Root"));

	// A blockout kit, matching every other room built by hand this session -
	// there is no art budget for hand-modelling unique geometry per generated
	// room, and a grey box that is walkable and legible beats a gap where a
	// room should be.
	static ConstructorHelpers::FObjectFinder<UStaticMesh> Cube(
		TEXT("/Game/LevelPrototyping/Meshes/SM_Cube.SM_Cube"));
	if (Cube.Succeeded())
	{
		CubeMesh = Cube.Object;
	}

	static ConstructorHelpers::FObjectFinder<UStaticMesh> Crate(
		TEXT("/Game/Ice_Station/Meshes/Crates/SM_crate_01.SM_crate_01"));
	if (Crate.Succeeded())
	{
		PropMeshA = Crate.Object;
	}

	static ConstructorHelpers::FObjectFinder<UStaticMesh> Barrel(
		TEXT("/Game/Cave_Camp/Meshes/Props_02/SM_barrel_01.SM_barrel_01"));
	if (Barrel.Succeeded())
	{
		PropMeshB = Barrel.Object;
	}

	static ConstructorHelpers::FObjectFinder<UStaticMesh> Lamp(
		TEXT("/Game/Diner/Meshes/Blocking/SM_lamp_02.SM_lamp_02"));
	if (Lamp.Succeeded())
	{
		PropMeshC = Lamp.Object;
	}

	// Defaulted to the floor's existing mobs, so a placed generator works with
	// zero configuration - edit CombatEnemyPool in the Details panel to retune.
	static ConstructorHelpers::FClassFinder<ADCCEnemyCharacter> BruteBP(
		TEXT("/Game/DCC/Characters/BP_DCCEnemy_Brute.BP_DCCEnemy_Brute_C"));
	static ConstructorHelpers::FClassFinder<ADCCEnemyCharacter> CourierBP(
		TEXT("/Game/DCC/Characters/BP_DCCEnemy_Courier.BP_DCCEnemy_Courier_C"));
	static ConstructorHelpers::FClassFinder<ADCCEnemyCharacter> AnalystBP(
		TEXT("/Game/DCC/Characters/BP_DCCEnemy_Analyst.BP_DCCEnemy_Analyst_C"));

	if (BruteBP.Succeeded())
	{
		CombatEnemyPool.Add(BruteBP.Class);
	}

	if (CourierBP.Succeeded())
	{
		CombatEnemyPool.Add(CourierBP.Class);
	}

	if (AnalystBP.Succeeded())
	{
		CombatEnemyPool.Add(AnalystBP.Class);
	}

	CombatEnemyPool.Add(ADCCEnemyCharacter_DebtEater::StaticClass());

	static ConstructorHelpers::FObjectFinder<USkeletalMesh> Wanderer(
		TEXT("/Game/Characters/Mannequins/Meshes/SKM_Manny_Simple.SKM_Manny_Simple"));
	if (Wanderer.Succeeded())
	{
		WandererMesh = Wanderer.Object;
	}
}

void ADCCDungeonGenerator::BeginPlay()
{
	Super::BeginPlay();

	if (bAutoStartOnBeginPlay && HasAuthority())
	{
		GenerateStartRoom();
	}
}

FVector ADCCDungeonGenerator::CellCenter(FIntPoint Cell) const
{
	// The generator's own placed position IS cell (0,0) - no separate origin
	// property to keep in sync with where it was dropped in the level.
	return GetActorLocation() + FVector(Cell.X * CellSize, Cell.Y * CellSize, 0.0f);
}

EDCCRoomType ADCCDungeonGenerator::PickRoomType()
{
	const float Roll = RandomStream.FRand();

	if (Roll < 0.40f) { return EDCCRoomType::Combat; }
	if (Roll < 0.60f) { return EDCCRoomType::LootVault; }
	if (Roll < 0.75f) { return EDCCRoomType::SafeRoom; }
	if (Roll < 0.90f) { return EDCCRoomType::Narrative; }
	return EDCCRoomType::Puzzle;
}

AActor* ADCCDungeonGenerator::SpawnMeshActor(UStaticMesh* Mesh, const FVector& Location, const FRotator& Rotation, const FVector& Scale)
{
	UWorld* World = GetWorld();
	if (!World || !Mesh)
	{
		return nullptr;
	}

	FActorSpawnParameters Params;
	Params.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;

	AStaticMeshActor* MeshActor = World->SpawnActor<AStaticMeshActor>(Location, Rotation, Params);
	if (!MeshActor)
	{
		return nullptr;
	}

	if (UStaticMeshComponent* Comp = MeshActor->GetStaticMeshComponent())
	{
		// Static mobility (the actor's default) cannot be rescaled at runtime -
		// this is spawned, unbaked geometry, not level-authored static geo.
		Comp->SetMobility(EComponentMobility::Movable);
		Comp->SetStaticMesh(Mesh);
	}

	MeshActor->SetActorScale3D(Scale);

	return MeshActor;
}

void ADCCDungeonGenerator::BuildWallSide(const FVector& Center, EDCCCardinalDirection Side, bool bOpen)
{
	constexpr float Thickness = 50.0f;
	constexpr float DoorWidth = 320.0f;
	const float Half = RoomSize * 0.5f;

	// North/South walls run along X with thickness in Y; East/West run along Y
	// with thickness in X. Always axis-aligned boxes - a grid dungeon never
	// needs a rotated wall, which sidesteps the compound-rotation math that
	// bit the Falls-to-Kitchens catwalk earlier this session.
	const bool bRunsAlongX =
		(Side == EDCCCardinalDirection::North || Side == EDCCCardinalDirection::South);

	if (bRunsAlongX)
	{
		const float WallY = (Side == EDCCCardinalDirection::North)
			? Center.Y + Half
			: Center.Y - Half - Thickness;

		const float MinX = Center.X - Half;
		const float MaxX = Center.X + Half;

		if (!bOpen)
		{
			SpawnMeshActor(CubeMesh, FVector(MinX, WallY, 0.0f), FRotator::ZeroRotator,
				FVector((MaxX - MinX) / 100.0f, Thickness / 100.0f, WallHeight / 100.0f));
			return;
		}

		const float GapMinX = Center.X - DoorWidth * 0.5f;
		const float GapMaxX = Center.X + DoorWidth * 0.5f;

		SpawnMeshActor(CubeMesh, FVector(MinX, WallY, 0.0f), FRotator::ZeroRotator,
			FVector((GapMinX - MinX) / 100.0f, Thickness / 100.0f, WallHeight / 100.0f));
		SpawnMeshActor(CubeMesh, FVector(GapMaxX, WallY, 0.0f), FRotator::ZeroRotator,
			FVector((MaxX - GapMaxX) / 100.0f, Thickness / 100.0f, WallHeight / 100.0f));
	}
	else
	{
		const float WallX = (Side == EDCCCardinalDirection::East)
			? Center.X + Half
			: Center.X - Half - Thickness;

		const float MinY = Center.Y - Half;
		const float MaxY = Center.Y + Half;

		if (!bOpen)
		{
			SpawnMeshActor(CubeMesh, FVector(WallX, MinY, 0.0f), FRotator::ZeroRotator,
				FVector(Thickness / 100.0f, (MaxY - MinY) / 100.0f, WallHeight / 100.0f));
			return;
		}

		const float GapMinY = Center.Y - DoorWidth * 0.5f;
		const float GapMaxY = Center.Y + DoorWidth * 0.5f;

		SpawnMeshActor(CubeMesh, FVector(WallX, MinY, 0.0f), FRotator::ZeroRotator,
			FVector(Thickness / 100.0f, (GapMinY - MinY) / 100.0f, WallHeight / 100.0f));
		SpawnMeshActor(CubeMesh, FVector(WallX, GapMaxY, 0.0f), FRotator::ZeroRotator,
			FVector(Thickness / 100.0f, (MaxY - GapMaxY) / 100.0f, WallHeight / 100.0f));
	}
}

void ADCCDungeonGenerator::SpawnCorridorStub(const FVector& Center, EDCCCardinalDirection Direction)
{
	const float Half = RoomSize * 0.5f;
	const float CellHalf = CellSize * 0.5f;
	const float StripWidth = 400.0f;
	const float StripLength = CellHalf - Half;

	if (StripLength <= 0.0f)
	{
		return;
	}

	switch (Direction)
	{
	case EDCCCardinalDirection::North:
		SpawnMeshActor(CubeMesh, FVector(Center.X - StripWidth * 0.5f, Center.Y + Half, -20.0f),
			FRotator::ZeroRotator, FVector(StripWidth / 100.0f, StripLength / 100.0f, 0.2f));
		break;

	case EDCCCardinalDirection::South:
		SpawnMeshActor(CubeMesh, FVector(Center.X - StripWidth * 0.5f, Center.Y - CellHalf, -20.0f),
			FRotator::ZeroRotator, FVector(StripWidth / 100.0f, StripLength / 100.0f, 0.2f));
		break;

	case EDCCCardinalDirection::East:
		SpawnMeshActor(CubeMesh, FVector(Center.X + Half, Center.Y - StripWidth * 0.5f, -20.0f),
			FRotator::ZeroRotator, FVector(StripLength / 100.0f, StripWidth / 100.0f, 0.2f));
		break;

	case EDCCCardinalDirection::West:
		SpawnMeshActor(CubeMesh, FVector(Center.X - CellHalf, Center.Y - StripWidth * 0.5f, -20.0f),
			FRotator::ZeroRotator, FVector(StripLength / 100.0f, StripWidth / 100.0f, 0.2f));
		break;

	default:
		break;
	}
}

ADCCRoom* ADCCDungeonGenerator::BuildRoomShell(FIntPoint Cell, const TSet<EDCCCardinalDirection>& OpenDirections, EDCCRoomType Type)
{
	UWorld* World = GetWorld();
	if (!World || !CubeMesh)
	{
		return nullptr;
	}

	const FVector Center = CellCenter(Cell);
	const float Half = RoomSize * 0.5f;

	SpawnMeshActor(CubeMesh, FVector(Center.X - Half, Center.Y - Half, -20.0f), FRotator::ZeroRotator,
		FVector(RoomSize / 100.0f, RoomSize / 100.0f, 0.2f));
	SpawnMeshActor(CubeMesh, FVector(Center.X - Half, Center.Y - Half, WallHeight), FRotator::ZeroRotator,
		FVector(RoomSize / 100.0f, RoomSize / 100.0f, 0.2f));

	for (uint8 i = 0; i < static_cast<uint8>(EDCCCardinalDirection::MAX); ++i)
	{
		const EDCCCardinalDirection Side = static_cast<EDCCCardinalDirection>(i);
		const bool bOpen = OpenDirections.Contains(Side);

		BuildWallSide(Center, Side, bOpen);

		if (bOpen)
		{
			SpawnCorridorStub(Center, Side);
		}
	}

	// Deferred rather than a plain SpawnActor: BeginPlay used to fire here, before
	// RoomType or Encounter were ever set (both callers set them afterwards, and
	// PopulateCombatRoom's Encounter does not even exist until after this
	// function returns) - so ADCCRoom::BeginPlay always saw RoomType at its class
	// default (Combat) and Encounter still null, meaning OnEncounterStateChanged
	// never bound on any generated room. See GenerateRoomAt/GenerateStartRoom,
	// which now call FinishSpawning only after PopulateRoom has set both.
	const FTransform SpawnTransform(FRotator::ZeroRotator, Center);
	ADCCRoom* Room = World->SpawnActorDeferred<ADCCRoom>(ADCCRoom::StaticClass(), SpawnTransform);
	if (!Room)
	{
		return nullptr;
	}

	Room->RoomId = FName(*FString::Printf(TEXT("Generated_%d_%d"), Cell.X, Cell.Y));
	Room->RoomType = Type;

	// Bounds is protected on ADCCRoom - FindComponentByClass reaches it without
	// needing a new accessor or a friend declaration. Safe pre-FinishSpawning:
	// components exist from the constructor, deferred spawn only holds back
	// PostActorConstruction/BeginPlay.
	if (UBoxComponent* Box = Room->FindComponentByClass<UBoxComponent>())
	{
		Box->SetBoxExtent(FVector(Half, Half, WallHeight * 0.5f));
	}

	return Room;
}

void ADCCDungeonGenerator::SpawnFrontierTrigger(ADCCRoom* FromRoom, FIntPoint FromCell, EDCCCardinalDirection OpenDirection)
{
	UWorld* World = GetWorld();
	if (!World)
	{
		return;
	}

	const FIntPoint TargetCell = FromCell + DCCCardinal::Offset(OpenDirection);
	if (Rooms.Contains(TargetCell))
	{
		return;
	}

	const FVector Center = CellCenter(FromCell);
	const float Half = RoomSize * 0.5f;
	FVector TriggerLocation = Center;

	switch (OpenDirection)
	{
	case EDCCCardinalDirection::North: TriggerLocation += FVector(0.0f, Half + 100.0f, 100.0f); break;
	case EDCCCardinalDirection::South: TriggerLocation += FVector(0.0f, -(Half + 100.0f), 100.0f); break;
	case EDCCCardinalDirection::East:  TriggerLocation += FVector(Half + 100.0f, 0.0f, 100.0f); break;
	case EDCCCardinalDirection::West:  TriggerLocation += FVector(-(Half + 100.0f), 0.0f, 100.0f); break;
	default: break;
	}

	FActorSpawnParameters Params;
	ADCCDungeonFrontierTrigger* Trigger = World->SpawnActor<ADCCDungeonFrontierTrigger>(
		TriggerLocation, FRotator::ZeroRotator, Params);

	if (Trigger)
	{
		Trigger->Generator = this;
		Trigger->TargetCell = TargetCell;
		Trigger->EntryDirection = DCCCardinal::Opposite(OpenDirection);
	}
}

void ADCCDungeonGenerator::GenerateStartRoom()
{
	if (!HasAuthority() || Rooms.Num() > 0)
	{
		return;
	}

	RandomStream.Initialize(Seed != 0 ? Seed : FMath::Rand());

	const FIntPoint Cell = FIntPoint::ZeroValue;

	TArray<EDCCCardinalDirection> AllDirs = {
		EDCCCardinalDirection::North, EDCCCardinalDirection::East,
		EDCCCardinalDirection::South, EDCCCardinalDirection::West
	};

	for (int32 i = AllDirs.Num() - 1; i > 0; --i)
	{
		const int32 j = RandomStream.RandRange(0, i);
		AllDirs.Swap(i, j);
	}

	TSet<EDCCCardinalDirection> OpenDirs;
	const int32 OpenCount = RandomStream.RandRange(2, 3);
	for (int32 i = 0; i < OpenCount && i < AllDirs.Num(); ++i)
	{
		OpenDirs.Add(AllDirs[i]);
	}

	// The start room is always a Safe Room on purpose: players arriving at a
	// freshly generated dungeon should get their bearings, not an ambush.
	ADCCRoom* Room = BuildRoomShell(Cell, OpenDirs, EDCCRoomType::SafeRoom);
	if (!Room)
	{
		return;
	}

	Rooms.Add(Cell, Room);
	++RoomsGenerated;

	PopulateRoom(Room, EDCCRoomType::SafeRoom, CellCenter(Cell));

	// Only now does BeginPlay run - RoomType and (for a Combat room) Encounter
	// are both already set, so the room's own state-change binding sees them.
	Room->FinishSpawning(FTransform(FRotator::ZeroRotator, CellCenter(Cell)));

	for (EDCCCardinalDirection Dir : OpenDirs)
	{
		SpawnFrontierTrigger(Room, Cell, Dir);
	}
}

void ADCCDungeonGenerator::GenerateRoomAt(FIntPoint Cell, EDCCCardinalDirection EntryDirection)
{
	if (!HasAuthority() || Rooms.Contains(Cell) || RoomsGenerated >= MaxRooms)
	{
		return;
	}

	const EDCCRoomType Type = PickRoomType();

	TSet<EDCCCardinalDirection> OpenDirs;
	OpenDirs.Add(EntryDirection);

	TArray<EDCCCardinalDirection> Candidates;
	for (uint8 i = 0; i < static_cast<uint8>(EDCCCardinalDirection::MAX); ++i)
	{
		const EDCCCardinalDirection Dir = static_cast<EDCCCardinalDirection>(i);
		if (Dir == EntryDirection)
		{
			continue;
		}

		// Only ever expand into a cell nothing has claimed - this is what keeps
		// the layout a tree instead of a graph that needs cycle-handling.
		const FIntPoint Neighbor = Cell + DCCCardinal::Offset(Dir);
		if (!Rooms.Contains(Neighbor))
		{
			Candidates.Add(Dir);
		}
	}

	for (int32 i = Candidates.Num() - 1; i > 0; --i)
	{
		const int32 j = RandomStream.RandRange(0, i);
		Candidates.Swap(i, j);
	}

	const int32 RemainingBudget = FMath::Max(0, MaxRooms - RoomsGenerated - 1);
	const int32 ExtraToOpen = FMath::Clamp(RandomStream.RandRange(0, 2), 0, FMath::Min(Candidates.Num(), RemainingBudget));

	for (int32 i = 0; i < ExtraToOpen; ++i)
	{
		OpenDirs.Add(Candidates[i]);
	}

	ADCCRoom* Room = BuildRoomShell(Cell, OpenDirs, Type);
	if (!Room)
	{
		return;
	}

	Rooms.Add(Cell, Room);
	++RoomsGenerated;

	PopulateRoom(Room, Type, CellCenter(Cell));

	// Only now does BeginPlay run - see BuildRoomShell's comment.
	Room->FinishSpawning(FTransform(FRotator::ZeroRotator, CellCenter(Cell)));

	for (EDCCCardinalDirection Dir : OpenDirs)
	{
		// The entry side already connects back to where the player came from -
		// only the newly opened sides need a fresh frontier.
		if (Dir != EntryDirection && RoomsGenerated < MaxRooms)
		{
			SpawnFrontierTrigger(Room, Cell, Dir);
		}
	}
}

void ADCCDungeonGenerator::PopulateRoom(ADCCRoom* Room, EDCCRoomType Type, const FVector& Center)
{
	switch (Type)
	{
	case EDCCRoomType::Combat:    PopulateCombatRoom(Room, Center); break;
	case EDCCRoomType::LootVault: PopulateLootRoom(Center); break;
	case EDCCRoomType::SafeRoom:  PopulateSafeRoom(Center); break;
	case EDCCRoomType::Narrative: PopulateNarrativeRoom(Center); break;
	case EDCCRoomType::Puzzle:    PopulatePuzzleRoom(Center); break;
	default: break;
	}
}

void ADCCDungeonGenerator::PopulateCombatRoom(ADCCRoom* Room, const FVector& Center)
{
	UWorld* World = GetWorld();
	if (!World || !Room || CombatEnemyPool.IsEmpty())
	{
		return;
	}

	FActorSpawnParameters Params;
	ADCCEncounter* Encounter = World->SpawnActor<ADCCEncounter>(Center, FRotator::ZeroRotator, Params);
	if (!Encounter)
	{
		return;
	}

	Encounter->FloorNumber = FloorNumber;

	const int32 EnemyCount = RandomStream.RandRange(MinEnemiesPerRoom, MaxEnemiesPerRoom);
	for (int32 i = 0; i < EnemyCount; ++i)
	{
		FDCCEnemySpawn Spawn;
		Spawn.EnemyClass = CombatEnemyPool[RandomStream.RandRange(0, CombatEnemyPool.Num() - 1)];

		FVector Dir2D = RandomStream.GetUnitVector();
		Dir2D.Z = 0.0f;
		Dir2D = Dir2D.GetSafeNormal();

		const float Dist = RandomStream.FRandRange(200.0f, RoomSize * 0.35f);
		Spawn.RelativeLocation = Dir2D * Dist;
		Spawn.Level = FloorNumber;

		Encounter->Enemies.Add(Spawn);
	}

	Room->Encounter = Encounter;
}

void ADCCDungeonGenerator::PopulateLootRoom(const FVector& Center)
{
	if (!LootTablesAsset || LootVaultTableRows.IsEmpty())
	{
		return;
	}

	const FName RowName = LootVaultTableRows[RandomStream.RandRange(0, LootVaultTableRows.Num() - 1)];
	DCCLootTableRoller::RollAndSpawn(GetWorld(), LootTablesAsset, RowName, Center);
}

void ADCCDungeonGenerator::PopulateSafeRoom(const FVector& Center)
{
	ScatterProps(Center, PropMeshC, 2);
}

void ADCCDungeonGenerator::PopulateNarrativeRoom(const FVector& Center)
{
	// Real presence, not just dressing - a speaker anyone can walk up to and
	// talk to. Deliberately generic lines (DT_DCCDialogue's "Wanderer" rows)
	// rather than an attempt at Floor 1's escalating, room-specific voice -
	// see the class comment for why that stays a hand-authoring concern.
	SpawnDialogueNPC(Center, NarrativeSpeakerId);
	ScatterProps(Center, PropMeshA, 1);
}

void ADCCDungeonGenerator::PopulatePuzzleRoom(const FVector& Center)
{
	// The "puzzle" is spatial rather than logical: something worth climbing,
	// which the universal-surface climbing system already makes free - no
	// bespoke mechanic to author, just a reason to look up.
	SpawnClimbableStack(Center);
	ScatterProps(Center, PropMeshB, 2);
}

void ADCCDungeonGenerator::ScatterProps(const FVector& Center, UStaticMesh* Mesh, int32 Count)
{
	if (!Mesh)
	{
		return;
	}

	for (int32 i = 0; i < Count; ++i)
	{
		FVector Dir2D = RandomStream.GetUnitVector();
		Dir2D.Z = 0.0f;
		Dir2D = Dir2D.GetSafeNormal();

		const float Dist = RandomStream.FRandRange(150.0f, RoomSize * 0.35f);
		const FVector Location = Center + Dir2D * Dist;
		const float Yaw = RandomStream.FRandRange(0.0f, 360.0f);

		SpawnMeshActor(Mesh, Location, FRotator(0.0f, Yaw, 0.0f), FVector(1.0f));
	}
}

void ADCCDungeonGenerator::SpawnClimbableStack(const FVector& Center)
{
	if (!PropMeshA)
	{
		return;
	}

	// Ice_Station/Cave_Camp crates are ~100 units tall (see CLAUDE.md's bounds
	// checks on this exact mesh family) - four stacked clears 400 units, tall
	// enough that reaching the top is a genuine climb rather than a step-up.
	// Named LayerCount rather than Layers - AActor already declares a Layers
	// property (editor layer membership) and shadowing it is a compile error.
	constexpr int32 LayerCount = 4;
	constexpr float LayerHeight = 100.0f;

	for (int32 i = 0; i < LayerCount; ++i)
	{
		// A little horizontal jitter per layer so the stack reads as piled
		// crates rather than one suspiciously precise column.
		const FVector2D Jitter = FMath::RandPointInCircle(20.0f);
		const FVector Location = Center + FVector(Jitter.X, Jitter.Y, i * LayerHeight);
		const float Yaw = RandomStream.FRandRange(0.0f, 360.0f);

		SpawnMeshActor(PropMeshA, Location, FRotator(0.0f, Yaw, 0.0f), FVector(1.0f));
	}
}

ADCCDialogueNPC* ADCCDungeonGenerator::SpawnDialogueNPC(const FVector& Location, FName SpeakerId)
{
	UWorld* World = GetWorld();
	if (!World)
	{
		return nullptr;
	}

	FActorSpawnParameters Params;
	ADCCDialogueNPC* NPC = World->SpawnActor<ADCCDialogueNPC>(Location, FRotator::ZeroRotator, Params);
	if (NPC)
	{
		// DialogueTable defaults to DT_DCCDialogue via the class's own
		// constructor - only the speaker needs setting here.
		NPC->SpeakerId = SpeakerId;

		// The class ships with no default mesh (every hand-placed
		// ADCCDialogueNPC needs one set explicitly too - the bartender was
		// invisible for exactly this reason until it was). The mannequin
		// rather than a crawler skeleton: a generated "Wanderer" is not one
		// of the five named crawlers, so borrowing their rig would be a lie
		// the model tells before the dialogue does. Cached in the
		// constructor - FObjectFinder cannot be used outside one.
		if (WandererMesh)
		{
			if (USkeletalMeshComponent* MeshComp = NPC->FindComponentByClass<USkeletalMeshComponent>())
			{
				MeshComp->SetSkeletalMesh(WandererMesh);
			}
		}
	}

	return NPC;
}
