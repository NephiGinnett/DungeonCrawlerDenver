// Dead Air, decorated (2026-11-16, Zoe: "we should have dead air MUCH more heavily decorated please :) several texture
// and prop and fillers pass"). ADCCArcadeHaunt::DressDetail, called after DressMine and DressCave on every machine.
// Everything comes from the mine's seed, so every player sees the same mine. Nothing here blocks the rails, the traps
// or the doorways: props hug the walls of the drifts and the edges of the stopes.
//
// Everything repeated is instanced (one holder actor, one component per mesh and material), so thousands of tiles,
// rocks and scraps cost a few dozen draws instead of thousands of actors.

#include "Arcade/DCCArcadeHaunt.h"

#include "Components/HierarchicalInstancedStaticMeshComponent.h"
#include "Engine/StaticMesh.h"
#include "Materials/MaterialInterface.h"
#include "Rooms/DCCDungeonGrid.h"

namespace
{
	const TCHAR* const RockWall = TEXT("/Game/Cave_Camp/Materials/MI_rock_03");
	const TCHAR* const RockRoof = TEXT("/Game/Cave_Camp/Materials/MI_rock_01");
	const TCHAR* const Wet = TEXT("/Game/Cave_Camp/Materials/MI_water_01");
	const TCHAR* const Wood = TEXT("/Game/Cave_Camp/Materials/MI_table");
	const TCHAR* const Canvas = TEXT("/Game/Cave_Camp/Materials/MI_Tent");
	const TCHAR* const Iron = TEXT("/Game/Cave_Camp/Materials/MI_barrel");

	struct FThing { const TCHAR* Mesh; FVector Fit; bool bSolid; };

	// Along the drift walls: the working clutter of a mine. Fit: a height (Z) or, for flat things, a width (X).
	const FThing WallProps[] = {
		{ TEXT("/Game/Cave_Camp/Meshes/Props_02/SM_barrel_01"), { 0, 0, 95 }, true },
		{ TEXT("/Game/Cave_Camp/Meshes/Props_02/SM_barrel_03"), { 0, 0, 95 }, true },
		{ TEXT("/Game/Cave_Camp/Meshes/Props_02/SM_crate_02"), { 0, 0, 80 }, true },
		{ TEXT("/Game/Cave_Camp/Meshes/Props_02/SM_crate_group_01"), { 0, 0, 120 }, true },
		{ TEXT("/Game/Cave_Camp/Meshes/Props_02/SM_bag"), { 0, 0, 60 }, false },
		{ TEXT("/Game/Desert_cave/Meshes/Wood_structures/Sm_rope_circular"), { 90, 0, 0 }, false },
		{ TEXT("/Game/Desert_cave/Meshes/Case/SM_case_02"), { 0, 0, 70 }, true },
		{ TEXT("/Game/Legendary_Forge/Meshes/Props/SM_basket"), { 0, 0, 55 }, false },
		{ TEXT("/Game/Desert_cave/Meshes/Cart/Sm_wheel"), { 95, 0, 0 }, false },
		{ TEXT("/Game/Viking_village/Meshes/Small_props/SM_barrel_group_01"), { 0, 0, 110 }, true },
		{ TEXT("/Game/Legendary_Forge/Meshes/Charcoal/SM_charcoal"), { 70, 0, 0 }, false },
		{ TEXT("/Game/Cave_Camp/Meshes/Props_02/SM_small_wood"), { 0, 0, 60 }, false },
	};

	// Leaning against the walls: tools, ladders, old boards.
	const FThing Leaners[] = {
		{ TEXT("/Game/Legendary_Forge/Meshes/Props/Tools/SM_tool_01"), { 0, 0, 130 }, false },
		{ TEXT("/Game/Legendary_Forge/Meshes/Props/Tools/SM_tool_03"), { 0, 0, 130 }, false },
		{ TEXT("/Game/Legendary_Forge/Meshes/Props/Tools/SM_tool_05"), { 0, 0, 120 }, false },
		{ TEXT("/Game/Desert_cave/Meshes/Wood_structures/SM_ladder"), { 0, 0, 300 }, false },
		{ TEXT("/Game/Desert_cave/Meshes/Wood_structures/Sm_wood_old"), { 0, 0, 180 }, false },
	};

	// The floor: scraps the miners dropped and the mountain shed.
	const FThing Filler[] = {
		{ TEXT("/Game/Legendary_Forge/Meshes/Rocks/Small_rocks/SM_small_rocks_individual_Plane_001"), { 0, 0, 18 }, false },
		{ TEXT("/Game/Legendary_Forge/Meshes/Rocks/Small_rocks/SM_small_rocks_individual_Plane_008"), { 0, 0, 22 }, false },
		{ TEXT("/Game/Cave_Camp/Meshes/SM_paper_01"), { 34, 0, 0 }, false },
		{ TEXT("/Game/Cave_Camp/Meshes/SM_paper_03"), { 34, 0, 0 }, false },
		{ TEXT("/Game/Cave_Camp/Meshes/SM_bottle"), { 0, 0, 26 }, false },
		{ TEXT("/Game/Cave_Camp/Meshes/SM_plate_01"), { 28, 0, 0 }, false },
		{ TEXT("/Game/SoulCave/Environment/Meshes/Rocks/SM_Cave_Rock_Small01"), { 0, 0, 30 }, false },
		{ TEXT("/Game/SoulCave/Environment/Meshes/Rocks/SM_Cave_Rock_Small02"), { 0, 0, 34 }, false },
		{ TEXT("/Game/Legendary_Forge/Meshes/Rocks/Medium_rock/SM_medium_rock"), { 0, 0, 45 }, false },
	};

	// Overhead.
	const TCHAR* const Drips[] = {
		TEXT("/Game/Cave_Camp/Meshes/Rocks/SM_stalactic_02"), TEXT("/Game/Cave_Camp/Meshes/Rocks/SM_stalactic_05"),
		TEXT("/Game/SoulCave/Environment/Meshes/Nature/SM_Cave_Roots01"),
	};

	// Signs the company put up, and some it should have.
	const TCHAR* const Notices[] = {
		TEXT("NO SMOKING  -  FIREDAMP"), TEXT("BLASTING  4 P.M.  -  CLEAR THE DRIFT"), TEXT("LEVEL 2  -  DRIFT #"),
		TEXT("CHECK YOUR LAMP"), TEXT("SAFETY FIRST  -  LUCKY TUESDAY MINING CO."), TEXT("ORE CHUTE  ->"),
		TEXT("DANGER  -  BAD AIR"), TEXT("STATION #"), TEXT("MEN WORKING BELOW"), TEXT("REPORT ALL KNOCKING TO THE SHIFT BOSS"),
	};
}

void ADCCArcadeHaunt::DressDetail()
{
	if (!Grid || !GetWorld())
	{
		return;
	}
	const FVector O = GetActorLocation();
	const int32 Size = Grid->GetSize();
	FRandomStream R(Seed * 41 + 17);
	auto Open = [this](int32 X, int32 Y) { return Grid->IsOpen(X, Y); };
	auto CellAt = [&](int32 X, int32 Y) { return O + FVector((X + 0.5f) * TileSize, (Y + 0.5f) * TileSize, 0.0f); };

	AActor* Holder = GetWorld()->SpawnActor<AActor>(AActor::StaticClass(), FTransform(O));
	if (!Holder)
	{
		return;
	}
	USceneComponent* HolderRoot = NewObject<USceneComponent>(Holder, TEXT("Root"));
	Holder->SetRootComponent(HolderRoot);
	HolderRoot->RegisterComponent();
	Holder->SetReplicates(false);
	LocalActors.Add(Holder);

	TMap<FString, UHierarchicalInstancedStaticMeshComponent*> Parts;
	auto Part = [&](const TCHAR* MeshPath, const TCHAR* MaterialPath, bool bCollide) -> UHierarchicalInstancedStaticMeshComponent*
	{
		const FString Key = FString::Printf(TEXT("%s|%s|%d"), MeshPath, MaterialPath ? MaterialPath : TEXT(""), bCollide ? 1 : 0);
		if (UHierarchicalInstancedStaticMeshComponent** Found = Parts.Find(Key))
		{
			return *Found;
		}
		const FString Path(MeshPath);
		UStaticMesh* Mesh = LoadObject<UStaticMesh>(nullptr, *(Path + TEXT(".") + FPaths::GetBaseFilename(Path)));
		UHierarchicalInstancedStaticMeshComponent* Comp = Mesh ? NewObject<UHierarchicalInstancedStaticMeshComponent>(Holder) : nullptr;
		if (Comp)
		{
			Comp->SetStaticMesh(Mesh);
			Comp->SetMobility(EComponentMobility::Static);
			Comp->SetCollisionProfileName(bCollide ? TEXT("BlockAll") : TEXT("NoCollision"));
			Comp->SetCastShadow(bCollide);
			if (MaterialPath)
			{
				const FString MatPath(MaterialPath);
				if (UMaterialInterface* Mat = LoadObject<UMaterialInterface>(nullptr, *(MatPath + TEXT(".") + FPaths::GetBaseFilename(MatPath))))
				{
					for (int32 Slot = 0; Slot < Comp->GetNumMaterials(); ++Slot)
					{
						Comp->SetMaterial(Slot, Mat);
					}
				}
			}
			Comp->SetupAttachment(HolderRoot);
			Comp->RegisterComponent();
			Holder->AddInstanceComponent(Comp);
		}
		else
		{
			UE_LOG(LogTemp, Warning, TEXT("[DCC Haunt] dressing mesh not found: %s"), MeshPath);
		}
		Parts.Add(Key, Comp);
		return Comp;
	};
	// A box (the engine cube is 100 cm) in a material: tiles, boards, beams.
	auto Slab = [&](const TCHAR* MaterialPath, const FVector& Center, const FVector& Extent, float Yaw)
	{
		if (UHierarchicalInstancedStaticMeshComponent* Comp = Part(TEXT("/Engine/BasicShapes/Cube"), MaterialPath, false))
		{
			Comp->AddInstance(FTransform(FRotator(0.0f, Yaw, 0.0f), Center, Extent / 100.0f), true);
		}
	};
	// A tube (the engine cylinder, 100 cm) lying along X or Y: ducts and pipes.
	auto Tube = [&](const TCHAR* MaterialPath, const FVector& Center, float Diameter, float Length, bool bAlongX)
	{
		if (UHierarchicalInstancedStaticMeshComponent* Comp = Part(TEXT("/Engine/BasicShapes/Cylinder"), MaterialPath, false))
		{
			Comp->AddInstance(FTransform(bAlongX ? FRotator(90, 0, 0) : FRotator(0, 0, 90), Center, FVector(Diameter, Diameter, Length) / 100.0f), true);
		}
	};
	// A mesh stood on the floor at Base, fitted to a height or a width like Prop(), turned to Yaw.
	auto Place = [&](const TCHAR* MeshPath, const FVector& Base, float Yaw, const FVector& Fit, bool bCollide, float Lean)
	{
		UHierarchicalInstancedStaticMeshComponent* Comp = Part(MeshPath, nullptr, bCollide);
		if (!Comp || !Comp->GetStaticMesh())
		{
			return;
		}
		const FBox Bounds = Comp->GetStaticMesh()->GetBoundingBox();
		const FVector MeshSize = Bounds.GetSize().ComponentMax(FVector(1.0f));
		float Uniform = 1.0f;
		if (Fit.Z > 0.0f) { Uniform = Fit.Z / MeshSize.Z; }
		else if (Fit.X > 0.0f) { Uniform = Fit.X / FMath::Max(MeshSize.X, MeshSize.Y); }
		const FVector S(Uniform);
		const FRotator Rot(0.0f, Yaw, Lean);
		const FVector Centre = Bounds.GetCenter() * S;
		const FVector Loc = Base - Rot.RotateVector(FVector(Centre.X, Centre.Y, 0.0f)) - FVector(0, 0, Bounds.Min.Z * S.Z);
		Comp->AddInstance(FTransform(Rot, Loc, S), true);
	};
	auto PlaceThing = [&](const FThing& Thing, const FVector& Base, float Yaw, float Scale, float Lean)
	{
		Place(Thing.Mesh, Base, Yaw, Thing.Fit * Scale, Thing.bSolid, Lean);
	};

	// ---- pass 1: textures --------------------------------------------------------------------------------------
	// Rock on every roof cell and every wall face, one tile per cell so the texture never stretches into a smear,
	// and seep water pooled in the low spots.
	for (int32 Y = 0; Y < Size; ++Y)
	{
		for (int32 X = 0; X < Size; ++X)
		{
			if (!Open(X, Y))
			{
				continue;
			}
			const FVector C = CellAt(X, Y);
			Slab(RockRoof, C + FVector(0, 0, WallHeight - 1.0f), FVector(TileSize, TileSize, 0.4f), 0.0f);
			for (const FIntPoint D : { FIntPoint(1, 0), FIntPoint(-1, 0), FIntPoint(0, 1), FIntPoint(0, -1) })
			{
				if (Open(X + D.X, Y + D.Y))
				{
					continue;
				}
				const FVector Out(D.X, D.Y, 0.0f);
				Slab(RockWall, C + Out * (TileSize * 0.5f - 16.0f) + FVector(0, 0, WallHeight * 0.5f),
					D.X != 0 ? FVector(1.0f, TileSize, WallHeight) : FVector(TileSize, 1.0f, WallHeight), 0.0f);
			}
			if (R.FRand() < 0.07f && !InFoyer(C))
			{
				const float W = R.FRandRange(120.0f, 260.0f);
				Slab(Wet, C + FVector(R.FRandRange(-120.0f, 120.0f), R.FRandRange(-120.0f, 120.0f), 1.2f), FVector(W, W * R.FRandRange(0.5f, 0.9f), 0.3f), R.FRandRange(0.0f, 180.0f));
			}
		}
	}

	// ---- pass 2: props ----------------------------------------------------------------------------------------
	int32 SpanIndex = 0;
	for (int32 Y = 0; Y < Size; ++Y)
	{
		for (int32 X = 0; X < Size; ++X)
		{
			FVector C;
			bool bX = false;
			if (!CorridorSpan(X, Y, C, bX) || InFoyer(C))
			{
				continue;
			}
			++SpanIndex;
			const FVector Along = bX ? FVector(1, 0, 0) : FVector(0, 1, 0);
			const FVector Across = bX ? FVector(0, 1, 0) : FVector(1, 0, 0);
			const float WallYaw = Along.Rotation().Yaw;
			for (int32 Side = -1; Side <= 1; Side += 2)
			{
				// Clutter against the wall: most spans get something on one side or both.
				if (R.FRand() < 0.55f)
				{
					const FThing& Thing = WallProps[R.RandRange(0, UE_ARRAY_COUNT(WallProps) - 1)];
					PlaceThing(Thing, C + Across * (Side * (TileSize - 95.0f)) + Along * R.FRandRange(-150.0f, 150.0f), WallYaw + R.FRandRange(-25.0f, 25.0f), R.FRandRange(0.9f, 1.15f), 0.0f);
				}
				// Tools and ladders tipped back against the rock.
				if (R.FRand() < 0.18f)
				{
					const FThing& Thing = Leaners[R.RandRange(0, UE_ARRAY_COUNT(Leaners) - 1)];
					PlaceThing(Thing, C + Across * (Side * (TileSize - 60.0f)) + Along * R.FRandRange(-180.0f, 180.0f), WallYaw, 1.0f, Side * 14.0f);
				}
			}
			// A ventilation duct of old canvas along the roof, and an air line and a water pipe along the other wall.
			Tube(Canvas, C + Across * (TileSize - 110.0f) + FVector(0, 0, WallHeight - 70.0f), 70.0f, TileSize + 10.0f, bX);
			Tube(Iron, C - Across * (TileSize - 50.0f) + FVector(0, 0, 330.0f), 14.0f, TileSize + 10.0f, bX);
			Tube(Wood, C - Across * (TileSize - 50.0f) + FVector(0, 0, 290.0f), 10.0f, TileSize + 10.0f, bX);
			// A miner's lamp on its chain every few sets: a dim, warm pool, so a drift is never pitch black for long.
			if (SpanIndex % 4 == 0)
			{
				const FVector Lamp = C + Across * (TileSize - 60.0f) + FVector(0, 0, 300.0f);
				Shape(TEXT("Sphere"), Lamp, FVector(18, 18, 22), FLinearColor(1.0f, 0.75f, 0.4f), FRotator::ZeroRotator, false);
				Light(Lamp, FLinearColor(1.0f, 0.62f, 0.3f), 900.0f, 650.0f);
				Place(TEXT("/Game/SoulCave/Environment/Meshes/Building/SM_Cave_Chain_02"), Lamp + FVector(0, 0, 18), 0.0f, FVector(0, 0, WallHeight - 320.0f), false, 0.0f);
			}
			// Company signs on boards.
			if (SpanIndex % 9 == 3)
			{
				const FString Text = FString(Notices[R.RandRange(0, UE_ARRAY_COUNT(Notices) - 1)]).Replace(TEXT("#"), *FString::FromInt(R.RandRange(2, 19)));
				const FVector Board = C - Across * (TileSize - 40.0f) + FVector(0, 0, 220.0f);
				Slab(Wood, Board, bX ? FVector(150, 6, 46) : FVector(6, 150, 46), 0.0f);
				Sign(Board + Across * 8.0f, Across.Rotation().Yaw, Text, FColor(235, 225, 200), 11.0f, false);
			}
			// A pulley and rope hanging from a cap, now and then.
			if (R.FRand() < 0.06f)
			{
				const FVector Hang = C + Across * (TileSize * 0.5f) + FVector(0, 0, WallHeight - 40.0f);
				Place(TEXT("/Game/Viking_village/Meshes/Drakkar/SM_pulley__01"), Hang - FVector(0, 0, 60), WallYaw, FVector(0, 0, 60), false, 0.0f);
				Place(TEXT("/Game/Desert_cave/Meshes/rope/SM_rope"), Hang - FVector(0, 0, 260), WallYaw, FVector(0, 0, 200), false, 0.0f);
			}
		}
	}

	// The stopes, where the ore came out: scaffolds, tables, ore heaps, boulders, stores, beams and drips.
	const TArray<UDCCDungeonGrid::FRoom>& GridRooms = Grid->GetRooms();
	for (int32 r = 1; r < GridRooms.Num(); ++r)
	{
		const UDCCDungeonGrid::FRoom& Room = GridRooms[r];
		const FVector Min = O + FVector(Room.X * TileSize, Room.Y * TileSize, 0.0f);
		const FVector Max = Min + FVector(Room.W * TileSize, Room.H * TileSize, 0.0f);
		// A point along the room's edge, Inset in from the wall, near a corner (doorways tend to sit mid-side).
		auto EdgePoint = [&](float Inset)
		{
			const int32 Side = R.RandRange(0, 3);
			const float T = R.FRand() < 0.5f ? R.FRandRange(0.06f, 0.3f) : R.FRandRange(0.7f, 0.94f);
			switch (Side)
			{
			case 0: return FVector(FMath::Lerp(Min.X, Max.X, T), Min.Y + Inset, Min.Z);
			case 1: return FVector(FMath::Lerp(Min.X, Max.X, T), Max.Y - Inset, Min.Z);
			case 2: return FVector(Min.X + Inset, FMath::Lerp(Min.Y, Max.Y, T), Min.Z);
			default: return FVector(Max.X - Inset, FMath::Lerp(Min.Y, Max.Y, T), Min.Z);
			}
		};
		if (R.FRand() < 0.6f)
		{
			Place(TEXT("/Game/Desert_cave/Meshes/Wood_structures/SM_wood_tower_01"), EdgePoint(160.0f), R.RandRange(0, 3) * 90.0f, FVector(0, 0, WallHeight - 40.0f), true, 0.0f);
		}
		if (R.FRand() < 0.5f)
		{
			Place(TEXT("/Game/Cave_Camp/Meshes/Props_02/SM_table_group_02"), EdgePoint(140.0f), R.FRandRange(0.0f, 360.0f), FVector(0, 0, 110), true, 0.0f);
		}
		const int32 Heaps = R.RandRange(1, 3);
		for (int32 i = 0; i < Heaps; ++i)
		{
			Place(R.FRand() < 0.5f ? TEXT("/Game/SoulCave/Environment/Meshes/Rocks/SM_Cave_StoneCluster01_COMBINED") : TEXT("/Game/SoulCave/Environment/Meshes/Rocks/SM_Cave_StoneCluster03"),
				EdgePoint(120.0f), R.FRandRange(0.0f, 360.0f), FVector(0, 0, R.FRandRange(60.0f, 120.0f)), true, 0.0f);
		}
		if (R.FRand() < 0.5f)
		{
			Place(R.FRand() < 0.5f ? TEXT("/Game/Desert_cave/Meshes/Rock/Sm_boulder_rock_01") : TEXT("/Game/Desert_cave/Meshes/Rock/Sm_boulder_rock_03"),
				EdgePoint(150.0f), R.FRandRange(0.0f, 360.0f), FVector(0, 0, R.FRandRange(120.0f, 200.0f)), true, 0.0f);
		}
		const int32 Stores = R.RandRange(2, 4);
		for (int32 i = 0; i < Stores; ++i)
		{
			PlaceThing(WallProps[R.RandRange(0, UE_ARRAY_COUNT(WallProps) - 1)], EdgePoint(90.0f), R.FRandRange(0.0f, 360.0f), 1.0f, 0.0f);
		}
		const bool bRoomX = Room.W >= Room.H;
		for (float T = 0.25f; T < 0.9f; T += 0.25f)
		{
			const FVector Beam = bRoomX ? FVector(FMath::Lerp(Min.X, Max.X, T), (Min.Y + Max.Y) * 0.5f, Min.Z + WallHeight - 30.0f)
				: FVector((Min.X + Max.X) * 0.5f, FMath::Lerp(Min.Y, Max.Y, T), Min.Z + WallHeight - 30.0f);
			Slab(Wood, Beam, bRoomX ? FVector(36, Room.H * TileSize - 40.0f, 36) : FVector(Room.W * TileSize - 40.0f, 36, 36), 0.0f);
		}
		const int32 Hangs = R.RandRange(3, 7);
		for (int32 i = 0; i < Hangs; ++i)
		{
			const float H = R.FRandRange(60.0f, 160.0f);
			Place(Drips[R.RandRange(0, UE_ARRAY_COUNT(Drips) - 1)], FVector(R.FRandRange(Min.X + 80.0f, Max.X - 80.0f), R.FRandRange(Min.Y + 80.0f, Max.Y - 80.0f), Min.Z + WallHeight - H),
				R.FRandRange(0.0f, 360.0f), FVector(0, 0, H), false, 0.0f);
		}
	}

	// ---- pass 3: filler ---------------------------------------------------------------------------------------
	// Pebbles, papers, bottles and plates, mostly near the walls, so no stretch of tunnel is bare.
	for (int32 Y = 0; Y < Size; ++Y)
	{
		for (int32 X = 0; X < Size; ++X)
		{
			if (!Open(X, Y))
			{
				continue;
			}
			const FVector C = CellAt(X, Y);
			if (InFoyer(C))
			{
				continue;
			}
			const int32 Count = R.RandRange(1, 3);
			for (int32 i = 0; i < Count; ++i)
			{
				FVector At = C + FVector(R.FRandRange(-170.0f, 170.0f), R.FRandRange(-170.0f, 170.0f), 0.0f);
				for (const FIntPoint D : { FIntPoint(1, 0), FIntPoint(-1, 0), FIntPoint(0, 1), FIntPoint(0, -1) })
				{
					if (!Open(X + D.X, Y + D.Y) && R.FRand() < 0.6f)
					{
						At += FVector(D.X, D.Y, 0.0f) * R.FRandRange(60.0f, 130.0f);
						break;
					}
				}
				PlaceThing(Filler[R.RandRange(0, UE_ARRAY_COUNT(Filler) - 1)], At + FVector(0, 0, 1.5f), R.FRandRange(0.0f, 360.0f), R.FRandRange(0.8f, 1.3f), 0.0f);
			}
		}
	}
}
