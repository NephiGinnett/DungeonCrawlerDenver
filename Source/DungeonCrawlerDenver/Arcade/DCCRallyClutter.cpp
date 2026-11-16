// Repo Rally: a denser, more cluttered city (2026-11-14, Zoe: "the city in repo could be much more urban and cluttered"),
// and people on the sidewalks (the NPC Forge bodies, Tools/npc_forge.py).
//
// Everything here is built on every machine from a fixed seed, so it matches everywhere without replication. It is
// instanced (one component per mesh), not one actor per prop: a block face holds a dozen shopfronts, so a few
// thousand pieces would be a few thousand actors otherwise.
//
// Where things go, measured from the middle of a street (RoadHalf 1800 is the asphalt edge):
//   1800-2400  the curb: lamps and palms (DressCity), the walkers' lane
//   ~2580      marks park here
//   ~2700      DressStreetLife's kiosks and one sign per face
//   2900-3000  trash, crates and boxes against the walls
//   3000+      the shopfront row (it collides, so a truck bumps a wall instead of driving into a lobby)
// Wires cross the street overhead, below the elevated deck. Nothing on the asphalt collides.

#include "Arcade/DCCArcadeGames.h"
#include "Vehicles/DCCRover.h"
#include "Animation/AnimSequence.h"
#include "Components/HierarchicalInstancedStaticMeshComponent.h"
#include "Components/SkeletalMeshComponent.h"
#include "Engine/SkeletalMesh.h"
#include "Engine/StaticMesh.h"
#include "EngineUtils.h"
#include "Kismet/GameplayStatics.h"

namespace
{
	struct FClutterSet
	{
		AActor* Owner = nullptr;
		TMap<FString, UHierarchicalInstancedStaticMeshComponent*> Parts;
		TMap<FString, FBox> Bounds;

		UHierarchicalInstancedStaticMeshComponent* Get(const TCHAR* MeshPath, bool bCollide, bool bShadow, FBox& OutBounds)
		{
			const FString Key = FString(MeshPath) + (bCollide ? TEXT("|c") : TEXT("|n"));
			if (UHierarchicalInstancedStaticMeshComponent** Found = Parts.Find(Key))
			{
				OutBounds = Bounds[Key];
				return *Found;
			}
			const FString Path(MeshPath);
			UStaticMesh* Mesh = LoadObject<UStaticMesh>(nullptr, *(Path + TEXT(".") + FPaths::GetBaseFilename(Path)));
			if (!Mesh || !Owner)
			{
				Parts.Add(Key, nullptr);
				Bounds.Add(Key, FBox(ForceInit));
				return nullptr;
			}
			UHierarchicalInstancedStaticMeshComponent* Comp = NewObject<UHierarchicalInstancedStaticMeshComponent>(Owner);
			Comp->SetStaticMesh(Mesh);
			Comp->SetMobility(EComponentMobility::Static);
			Comp->SetCollisionProfileName(bCollide ? TEXT("BlockAll") : TEXT("NoCollision"));
			Comp->SetCastShadow(bShadow);
			Comp->SetupAttachment(Owner->GetRootComponent());
			Comp->RegisterComponent();
			Owner->AddInstanceComponent(Comp);
			Parts.Add(Key, Comp);
			Bounds.Add(Key, Mesh->GetBoundingBox());
			OutBounds = Mesh->GetBoundingBox();
			return Comp;
		}

		/** Like ADCCArcadeArena::Prop: Base is the bottom middle, Fit scales it uniformly to the smallest fitting axis.
		 *  bAlongLongest turns the mesh so its longer side runs along Along. Returns the fitted size along Along. */
		float Add(const TCHAR* MeshPath, const FVector& Base, const FVector& Along, const FVector& Fit, bool bCollide,
			bool bShadow, bool bAlongLongest = true, float Lift = 0.0f, bool bStretch = false)
		{
			FBox B;
			UHierarchicalInstancedStaticMeshComponent* Comp = Get(MeshPath, bCollide, bShadow, B);
			if (!Comp)
			{
				return 0.0f;
			}
			const FVector Size = B.GetSize().ComponentMax(FVector(1.0f));
			const bool bXLong = Size.X >= Size.Y;
			// Fit.X is along the face, Fit.Y is depth, Fit.Z is height.
			const float LongSide = bAlongLongest ? (bXLong ? Size.X : Size.Y) : (bXLong ? Size.Y : Size.X);
			const float ShortSide = bAlongLongest ? (bXLong ? Size.Y : Size.X) : (bXLong ? Size.X : Size.Y);
			float Uniform = TNumericLimits<float>::Max();
			if (Fit.X > 0) { Uniform = FMath::Min(Uniform, Fit.X / LongSide); }
			if (Fit.Y > 0) { Uniform = FMath::Min(Uniform, Fit.Y / ShortSide); }
			if (Fit.Z > 0) { Uniform = FMath::Min(Uniform, Fit.Z / Size.Z); }
			if (Uniform == TNumericLimits<float>::Max()) { Uniform = 1.0f; }
			FVector S(Uniform);
			if (bStretch)
			{
				// Each axis to its own size (wires: the span across, a shallow sag), the long side along Along.
				const float SL = Fit.X > 0 ? Fit.X / LongSide : Uniform;
				const float SS = Fit.Y > 0 ? Fit.Y / ShortSide : SL;
				const float SZ = Fit.Z > 0 ? Fit.Z / Size.Z : SL;
				S = (bXLong == bAlongLongest) ? FVector(SL, SS, SZ) : FVector(SS, SL, SZ);
			}
			const float AlongYaw = Along.Rotation().Yaw;
			const float Yaw = (bXLong == bAlongLongest) ? AlongYaw : AlongYaw + 90.0f;
			const FRotator Rot(0.0f, Yaw, 0.0f);
			const FVector Center = B.GetCenter() * S;
			const FVector Loc = Base - Rot.RotateVector(FVector(Center.X, Center.Y, 0.0f)) - FVector(0, 0, B.Min.Z * S.Z - Lift);
			Comp->AddInstance(FTransform(Rot, Loc, S), true);
			return LongSide * (bStretch ? (Fit.X > 0 ? Fit.X / LongSide : Uniform) : Uniform);
		}
	};

	static const TCHAR* Shopfronts[] = {
		TEXT("/Game/Cyberpunk_City/Meshes/Buildings_modules/SM_building_module_01"), TEXT("/Game/Cyberpunk_City/Meshes/Buildings_modules/SM_building_module_02"),
		TEXT("/Game/Cyberpunk_City/Meshes/Buildings_modules/SM_building_module_03"), TEXT("/Game/Cyberpunk_City/Meshes/Buildings_modules/SM_building_module_04"),
		TEXT("/Game/Cyberpunk_City/Meshes/Buildings_modules/SM_building_module_05"), TEXT("/Game/Cyberpunk_City/Meshes/Buildings_modules/SM_building_module_06"),
		TEXT("/Game/Cyberpunk_City/Meshes/Buildings_modules/SM_building_module_07"), TEXT("/Game/Cyberpunk_City/Meshes/Buildings_modules/SM_building_module_08"),
		TEXT("/Game/Cyberpunk_City/Meshes/Buildings_modules/SM_building_module_09"), TEXT("/Game/Cyberpunk_City/Meshes/Buildings_modules/SM_building_module_10"),
		TEXT("/Game/Cyberpunk_City/Meshes/Buildings_modules/SM_building_module_11"), TEXT("/Game/Cyberpunk_City/Meshes/Buildings_modules/SM_street_building_01"),
	};
	static const TCHAR* WallSigns[] = {
		TEXT("/Game/Cyberpunk_City/Meshes/Sign/SM_sign_01"), TEXT("/Game/Cyberpunk_City/Meshes/Sign/SM_sign_03"), TEXT("/Game/Cyberpunk_City/Meshes/Sign/SM_sign_05"),
		TEXT("/Game/Cyberpunk_City/Meshes/Sign/SM_sign_06"), TEXT("/Game/Cyberpunk_City/Meshes/Sign/SM_sign_07"), TEXT("/Game/Cyberpunk_City/Meshes/Sign/SM_sign_09"),
		TEXT("/Game/Cyberpunk_City/Meshes/Sign/SM_sign_10"), TEXT("/Game/Cyberpunk_City/Meshes/Neon/SM_neon_01"), TEXT("/Game/Cyberpunk_City/Meshes/Neon/SM_neon_02"),
		TEXT("/Game/Synthwave_city/Meshes/Advertisements/SM_advertisement_01"), TEXT("/Game/Synthwave_city/Meshes/Advertisements/SM_advertisement_04"),
		TEXT("/Game/Synthwave_city/Meshes/Advertisements/SM_advertisement_07"), TEXT("/Game/Synthwave_city/Meshes/Advertisements/SM_advertisement_10"),
	};
	static const TCHAR* Aircons[] = {
		TEXT("/Game/Cyberpunk_City/Meshes/Aircon/SM_aircon_01"), TEXT("/Game/Cyberpunk_City/Meshes/Aircon/SM_aircon_02"), TEXT("/Game/Cyberpunk_City/Meshes/Aircon/SM_aircon_03"),
		TEXT("/Game/Cyberpunk_City/Meshes/Aircon/SM_aircon_04"), TEXT("/Game/Cyberpunk_City/Meshes/Aircon/SM_aircon_05"), TEXT("/Game/Cyberpunk_City/Meshes/Aircon/SM_aircon_06"),
		TEXT("/Game/Cyberpunk_City/Meshes/Aircon/SM_aircon_07"),
	};
	static const TCHAR* Junk[] = {
		TEXT("/Game/Cyberpunk_City/Meshes/Trashbag/SM_trashbag_group_01"), TEXT("/Game/Cyberpunk_City/Meshes/Trashbag/SM_trashbag_group_02"),
		TEXT("/Game/Cyberpunk_City/Meshes/Trashbag/SM_trashbag_group_03"), TEXT("/Game/Cyberpunk_City/Meshes/Trashbag/SM_trash_01"),
		TEXT("/Game/Cyberpunk_City/Meshes/Trashbag/SM_trash_03"), TEXT("/Game/Cyberpunk_City/Meshes/Trashbag/SM_trash_05"),
		TEXT("/Game/Cyberpunk_City/Meshes/Cardboard/SM_card_board_01"), TEXT("/Game/Cyberpunk_City/Meshes/Cardboard/SM_card_board_03"),
		TEXT("/Game/Cyberpunk_City/Meshes/Cardboard/SM_card_board_05"), TEXT("/Game/Cyberpunk_City/Meshes/Crates/SM_crate_01"),
		TEXT("/Game/Cyberpunk_City/Meshes/Crates/SM_crate_04"), TEXT("/Game/Cyberpunk_City/Meshes/Crates/SM_crate_06"),
		TEXT("/Game/Cyberpunk_City/Meshes/Trashcan/SM_trashcan_01"), TEXT("/Game/Sci_Fi_city/Meshes/Crates/SM_generator_01"),
	};
	static const TCHAR* Wires[] = {
		TEXT("/Game/Cyberpunk_City/Meshes/Cables/SM_wire_01"), TEXT("/Game/Cyberpunk_City/Meshes/Cables/SM_wire_02"), TEXT("/Game/Cyberpunk_City/Meshes/Cables/SM_wire_03"),
		TEXT("/Game/Cyberpunk_City/Meshes/Cables/SM_wire_04"), TEXT("/Game/Cyberpunk_City/Meshes/Cables/SM_wire_05"), TEXT("/Game/Cyberpunk_City/Meshes/Cables/SM_wire_06"),
	};

	template <int32 N>
	const TCHAR* Pick(FRandomStream& R, const TCHAR* (&List)[N]) { return List[R.RandRange(0, N - 1)]; }
}

void ADCCArcadeKart::DressUrbanClutter()
{
	UWorld* World = GetWorld();
	if (!World)
	{
		return;
	}
	const FVector O = GetActorLocation();
	const float S = StreetSpacing;
	FRandomStream R(5280);

	AActor* Holder = World->SpawnActor<AActor>(AActor::StaticClass(), FTransform(O));
	if (!Holder)
	{
		return;
	}
	USceneComponent* Root = NewObject<USceneComponent>(Holder, TEXT("Root"));
	Root->SetMobility(EComponentMobility::Static);
	Holder->SetRootComponent(Root);
	Root->RegisterComponent();
	Holder->SetReplicates(false);
	LocalActors.Add(Holder);

	FClutterSet Set;
	Set.Owner = Holder;

	auto NearReserved = [&](const FVector& At, float Clear)
	{
		const FVector2D Local(At.X - O.X, At.Y - O.Y);
		if (FVector2D::Distance(Local, YardOffset()) < 9000.0f + Clear)
		{
			return true;
		}
		for (int32 g = 0; g < 2; ++g)
		{
			if (FVector2D::Distance(Local, GasOffset(g)) < 3500.0f + Clear)
			{
				return true;
			}
		}
		return false;
	};

	// ---- block faces: shopfronts, signs, AC units, junk against the walls -------------------------------------------
	const float HalfFace = S * 0.5f - RoadHalf - 900.0f; // stop short of the crossings
	for (int32 Ix = -StreetRadius; Ix < StreetRadius; ++Ix)
	{
		for (int32 Iy = -StreetRadius; Iy < StreetRadius; ++Iy)
		{
			if (Ix == -StreetRadius && Iy == -StreetRadius)
			{
				continue; // the yard
			}
			const FVector Cell = O + FVector((Ix + 0.5f) * S, (Iy + 0.5f) * S, 0);
			for (int32 Edge = 0; Edge < 4; ++Edge)
			{
				const FVector Out = (Edge == 0) ? FVector(1, 0, 0) : (Edge == 1) ? FVector(-1, 0, 0) : (Edge == 2) ? FVector(0, 1, 0) : FVector(0, -1, 0);
				const FVector Along = FVector(-Out.Y, Out.X, 0);
				const FVector Inward = -Out;
				// The street's middle is S/2 out from the cell centre; the shopfront row starts 3000 back from it.
				const FVector Street = Cell + Out * (S * 0.5f);
				const FVector WallFront = Street + Inward * 3000.0f;

				float D = -HalfFace;
				int32 Count = 0;
				while (D < HalfFace - 600.0f)
				{
					const float Width = R.FRandRange(1300.0f, 2100.0f);
					const FVector Mid = WallFront + Along * (D + Width * 0.5f);
					if (NearReserved(Mid, Width * 0.5f))
					{
						D += Width;
						continue;
					}
					// An alley now and then: a gap with bins and bags in it.
					if (Count > 0 && R.FRand() < 0.12f)
					{
						for (int32 j = 0; j < 3; ++j)
						{
							Set.Add(Pick(R, Junk), Mid + Along * R.FRandRange(-Width * 0.3f, Width * 0.3f) + Inward * R.FRandRange(100.0f, 900.0f),
								Along.RotateAngleAxis(R.FRandRange(0.0f, 360.0f), FVector::UpVector), FVector(260, 0, 220), false, false);
						}
						D += Width;
						++Count;
						continue;
					}
					const float Height = R.FRandRange(900.0f, 2600.0f);
					// The shopfront: its front on WallFront, the body behind it. Depth is capped so it never reaches the towers.
					const float Placed = Set.Add(Pick(R, Shopfronts), Mid + Inward * 450.0f, Along, FVector(Width, 900.0f, Height), true, true);
					const float Used = FMath::Max(Placed, 600.0f);

					// A sign or two on the front, sticking out over the sidewalk.
					if (R.FRand() < 0.7f)
					{
						const FVector SignAt = WallFront + Along * (D + Used * R.FRandRange(0.25f, 0.75f)) + Out * 120.0f;
						const bool bBlade = R.FRand() < 0.5f; // blade signs stick out across, the rest lie flat on the wall
						Set.Add(Pick(R, WallSigns), SignAt, bBlade ? Out : Along, FVector(bBlade ? 420.0f : 700.0f, 0, R.FRandRange(220.0f, 420.0f)),
							false, false, true, R.FRandRange(380.0f, FMath::Max(420.0f, Height * 0.6f)));
					}
					// AC units and pipes high on the wall.
					const int32 Units = R.RandRange(0, 2);
					for (int32 u = 0; u < Units; ++u)
					{
						const FVector UnitAt = WallFront + Along * (D + Used * R.FRandRange(0.15f, 0.85f)) + Out * 60.0f;
						Set.Add(Pick(R, Aircons), UnitAt, Along, FVector(160, 120, 0), false, false, true, R.FRandRange(450.0f, FMath::Max(500.0f, Height - 200.0f)));
					}
					// Bags, boxes and crates against the wall.
					if (R.FRand() < 0.6f)
					{
						Set.Add(Pick(R, Junk), WallFront + Along * (D + Used * R.FRandRange(0.1f, 0.9f)) + Out * 90.0f,
							Along.RotateAngleAxis(R.FRandRange(-30.0f, 30.0f), FVector::UpVector), FVector(220, 0, 180), false, false);
					}
					D += Used + R.FRandRange(0.0f, 120.0f);
					++Count;
				}
			}
		}
	}

	// ---- wires across the streets, every few metres along each one, below the elevated deck --------------------------
	const float RoadLen = (StreetRadius * 2) * S;
	for (int32 i = -StreetRadius; i <= StreetRadius; ++i)
	{
		for (int32 Axis = 0; Axis < 2; ++Axis)
		{
			const FVector Along = Axis == 0 ? FVector(1, 0, 0) : FVector(0, 1, 0);
			const FVector Across = Axis == 0 ? FVector(0, 1, 0) : FVector(1, 0, 0);
			const FVector Mid = O + Across * (i * S);
			for (float D = -RoadLen * 0.5f + 1500.0f; D < RoadLen * 0.5f - 1500.0f; D += R.FRandRange(1800.0f, 3400.0f))
			{
				// Not over a crossing.
				const float ToCrossing = FMath::Abs(FMath::Fmod(D + S * 10.0f, S) - S * 0.5f);
				if (ToCrossing > S * 0.5f - (RoadHalf + 1500.0f))
				{
					continue;
				}
				const FVector At = Mid + Along * D;
				if (NearReserved(At, 0.0f))
				{
					continue;
				}
				Set.Add(Pick(R, Wires), At, Across, FVector(6000.0f, 30.0f, R.FRandRange(150.0f, 320.0f)), false, false, true, R.FRandRange(1500.0f, 1850.0f), true);
			}
		}
	}

	int32 Total = 0;
	for (const TPair<FString, UHierarchicalInstancedStaticMeshComponent*>& Part : Set.Parts)
	{
		Total += Part.Value ? Part.Value->GetInstanceCount() : 0;
	}
	UE_LOG(LogTemp, Log, TEXT("[DCC Rally] Urban clutter: %d pieces in %d instanced meshes."), Total, Set.Parts.Num());
}

// ---- people on the sidewalks ------------------------------------------------------------------------------------------

void ADCCArcadeKart::SpawnWalkers()
{
	UWorld* World = GetWorld();
	if (!World)
	{
		return;
	}
	static const TCHAR* Forge[] = {
		TEXT("casual_300"), TEXT("worker_301"), TEXT("office_302"), TEXT("casual_303"), TEXT("worker_304"), TEXT("office_305"),
		TEXT("casual_306"), TEXT("worker_307"), TEXT("office_308"), TEXT("casual_309"), TEXT("worker_310"), TEXT("office_311"),
	};
	TArray<USkeletalMesh*> Bodies;
	for (const TCHAR* Id : Forge)
	{
		const FString Path = FString::Printf(TEXT("/Game/DCC/Characters/NPCs/NPC_%s/SK_NPC_%s.SK_NPC_%s"), Id, Id, Id);
		if (USkeletalMesh* Mesh = LoadObject<USkeletalMesh>(nullptr, *Path))
		{
			Bodies.Add(Mesh);
		}
	}
	WalkAnim = LoadObject<UAnimSequence>(nullptr, TEXT("/Game/DCC/Characters/NPCs/Anims/NPC_MF_Unarmed_Walk_Fwd.NPC_MF_Unarmed_Walk_Fwd"));
	JogAnim = LoadObject<UAnimSequence>(nullptr, TEXT("/Game/DCC/Characters/NPCs/Anims/NPC_MF_Unarmed_Jog_Fwd.NPC_MF_Unarmed_Jog_Fwd"));
	if (Bodies.IsEmpty() || !WalkAnim)
	{
		UE_LOG(LogTemp, Warning, TEXT("[DCC Rally] No NPC Forge bodies or walk clip; the sidewalks stay empty."));
		return;
	}

	AActor* Holder = World->SpawnActor<AActor>(AActor::StaticClass(), FTransform(GetActorLocation()));
	if (!Holder)
	{
		return;
	}
	USceneComponent* Root = NewObject<USceneComponent>(Holder, TEXT("Root"));
	Holder->SetRootComponent(Root);
	Root->RegisterComponent();
	Holder->SetReplicates(false);
	LocalActors.Add(Holder);

	const FVector O = GetActorLocation();
	const float S = StreetSpacing;
	const float HalfFace = S * 0.5f - RoadHalf - 700.0f;
	FRandomStream R(303);
	for (int32 Ix = -StreetRadius; Ix < StreetRadius; ++Ix)
	{
		for (int32 Iy = -StreetRadius; Iy < StreetRadius; ++Iy)
		{
			if (Ix == -StreetRadius && Iy == -StreetRadius)
			{
				continue;
			}
			const FVector Cell = O + FVector((Ix + 0.5f) * S, (Iy + 0.5f) * S, 0);
			for (int32 Edge = 0; Edge < 4; ++Edge)
			{
				if (R.FRand() < 0.45f)
				{
					continue; // about two walkers a block
				}
				const FVector Out = (Edge == 0) ? FVector(1, 0, 0) : (Edge == 1) ? FVector(-1, 0, 0) : (Edge == 2) ? FVector(0, 1, 0) : FVector(0, -1, 0);
				FWalker W;
				W.Along = FVector(-Out.Y, Out.X, 0);
				W.Line = Cell + Out * (S * 0.5f) - Out * R.FRandRange(2080.0f, 2220.0f); // the walkers' lane, inside the lamps
				W.Half = HalfFace;
				W.D = R.FRandRange(-HalfFace, HalfFace);
				W.Dir = R.FRand() < 0.5f ? 1.0f : -1.0f;
				W.Pace = R.FRandRange(130.0f, 170.0f);
				USkeletalMeshComponent* Body = NewObject<USkeletalMeshComponent>(Holder);
				Body->SetSkeletalMeshAsset(Bodies[R.RandRange(0, Bodies.Num() - 1)]);
				Body->SetCollisionEnabled(ECollisionEnabled::NoCollision);
				Body->SetCastShadow(true);
				Body->bEnableUpdateRateOptimizations = true;
				Body->VisibilityBasedAnimTickOption = EVisibilityBasedAnimTickOption::OnlyTickPoseWhenRendered;
				Body->SetCachedMaxDrawDistance(18000.0f);
				Body->SetAnimationMode(EAnimationMode::AnimationSingleNode);
				Body->SetupAttachment(Root);
				Body->RegisterComponent();
				Body->PlayAnimation(WalkAnim, true);
				Body->SetPlayRate(W.Pace / 150.0f);
				W.Body = Body;
				Walkers.Add(W);
			}
		}
	}
	UE_LOG(LogTemp, Log, TEXT("[DCC Rally] %d people on the sidewalks."), Walkers.Num());
}

void ADCCArcadeKart::TickWalkers(float DeltaSeconds)
{
	if (Walkers.IsEmpty())
	{
		return;
	}
	// The trucks, once a frame. A truck coming close makes people hurry along.
	TArray<FVector, TInlineAllocator<16>> TruckSpots;
	for (TActorIterator<ADCCRover> It(GetWorld()); It; ++It)
	{
		if (It->GetVelocity().SizeSquared2D() > FMath::Square(600.0f))
		{
			TruckSpots.Add(It->GetActorLocation());
		}
	}
	for (FWalker& W : Walkers)
	{
		USkeletalMeshComponent* Body = W.Body.Get();
		if (!Body)
		{
			continue;
		}
		const FVector At = W.Line + W.Along * W.D;
		bool bScared = false;
		for (const FVector& T : TruckSpots)
		{
			if (FVector::DistSquared2D(T, At) < FMath::Square(1400.0f))
			{
				bScared = true;
				break;
			}
		}
		if (bScared != W.bHurry && JogAnim)
		{
			W.bHurry = bScared;
			Body->PlayAnimation(bScared ? JogAnim.Get() : WalkAnim.Get(), true);
			Body->SetPlayRate(bScared ? 1.0f : W.Pace / 150.0f);
		}
		const float Speed = W.bHurry ? 380.0f : W.Pace;
		W.D += W.Dir * Speed * DeltaSeconds;
		if (FMath::Abs(W.D) > W.Half)
		{
			W.D = FMath::Clamp(W.D, -W.Half, W.Half);
			W.Dir = -W.Dir; // end of the block: turn round
		}
		const FVector Heading = W.Along * W.Dir;
		// The Forge bodies face +Y, so they turn -90 from the way they walk (as ADCCRallyThug does).
		Body->SetWorldLocationAndRotation(W.Line + W.Along * W.D + FVector(0, 0, 4.0f), FRotator(0.0f, Heading.Rotation().Yaw - 90.0f, 0.0f));
	}
}
