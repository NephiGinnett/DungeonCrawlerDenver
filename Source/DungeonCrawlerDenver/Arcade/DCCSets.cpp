#include "Arcade/DCCSets.h"

#include "Arcade/DCCSetDoor.h"
#include "Camera/CameraActor.h"
#include "Camera/CameraComponent.h"
#include "Engine/DirectionalLight.h"
#include "Engine/ExponentialHeightFog.h"
#include "Engine/Level.h"
#include "Engine/LevelScriptActor.h"
#include "Engine/LevelStreamingDynamic.h"
#include "Components/StaticMeshComponent.h"
#include "Engine/Light.h"
#include "Engine/StaticMesh.h"
#include "GameFramework/Volume.h"
#include "GameFramework/Info.h"
#include "Engine/PostProcessVolume.h"
#include "Engine/SkyLight.h"
#include "Engine/World.h"
#include "EngineUtils.h"
#include "GameFramework/Pawn.h"
#include "GameFramework/PlayerController.h"
#include "GameFramework/PlayerStart.h"
#include "GameFramework/WorldSettings.h"
#include "HAL/IConsoleManager.h"
#include "Misc/PackageName.h"
#include "TimerManager.h"

namespace
{
	struct FSetAlias
	{
		const TCHAR* Name;
		const TCHAR* Path;
	};

	// The sets worth loading. Docs/kits.md says what each is for; SourceArt/Kits/*.json lists the rest of the maps.
	const FSetAlias GAliases[] = {
		{ TEXT("OldMine"),        TEXT("/Game/Scene_OldMine/Maps/Industrial_OldMine") },
		{ TEXT("PowerPlant"),     TEXT("/Game/AbandonedPowerPlant/Maps/PowerPlant") },
		{ TEXT("Studio"),         TEXT("/Game/BroadcastStudio/Maps/Showcase") },
		{ TEXT("Clinic"),         TEXT("/Game/Clinic/Maps/Overview") },        // its Clinic map is a grey blockout
		{ TEXT("Metro"),          TEXT("/Game/MetroMaintenanceStation/Maps/Metro1") },
		{ TEXT("SpaceStation"),   TEXT("/Game/SciFi_Space_Station/Level/L_SciFi_Space_Station") },
		{ TEXT("Saloon"),         TEXT("/Game/Scene_Saloon/Maps/Historic_Saloon") },
		{ TEXT("Elderboom"),      TEXT("/Game/ElderboomVillage/Maps/L_ElderboomVillage") },
		{ TEXT("ForestVillage"),  TEXT("/Game/Forest_village/Level/L_showcase_level") },
		{ TEXT("Inferno"),        TEXT("/Game/Inferno_World_Free/Maps/Demonstration") },
		{ TEXT("SciFiInterior1"), TEXT("/Game/StarterBundle/ModularSci_Int/Maps/SciFi_INT_EX1") },
		{ TEXT("SciFiInterior2"), TEXT("/Game/StarterBundle/ModularSci_Int/Maps/SciFi_INT_EX2") },
		{ TEXT("SciFiInterior3"), TEXT("/Game/StarterBundle/ModularSci_Int/Maps/SciFi_INT_EX3") },
		{ TEXT("CommandCenter"),  TEXT("/Game/StarterBundle/ModularSci_Comm/Maps/SciFi_COMM_EX1") },
		{ TEXT("HallwayA"),       TEXT("/Game/StarterBundle/ModularScifiHallways/Maps/SciFi_Example_A_Ext") },
		{ TEXT("HallwayB"),       TEXT("/Game/StarterBundle/ModularScifiHallways/Maps/SciFi_Example_B_Ext") },
		{ TEXT("EngiRoom"),       TEXT("/Game/ModSci_Engineer/Maps/Example_Dynamic") },
		{ TEXT("SciInterior"),    TEXT("/Game/ModSciInteriors/Maps/Example_Dynamic") },
		{ TEXT("Gothic"),         TEXT("/Game/Gothic_props_1/Maps/Gothic_props_1") },
		{ TEXT("Banners"),        TEXT("/Game/Torn_Banner_Free/Scenes/Torn_Banner_Free_demoscene") },
	};

	bool IsSkyActor(const AActor* Actor)
	{
		if (Actor->IsA<ASkyLight>() || Actor->IsA<AExponentialHeightFog>() || Actor->IsA<APostProcessVolume>())
		{
			return true;
		}
		const FString Cls = Actor->GetClass()->GetName();
		const FString Name = Actor->GetName();
		if (Name.Contains(TEXT("SkyDome")) || Name.Contains(TEXT("Sky_Dome")) || Name.Contains(TEXT("SkySphere")) || Name.Contains(TEXT("Skybox")))
		{
			return true; // a static mesh dome with a sky material
		}
		return Cls.Contains(TEXT("SkyAtmosphere")) || Cls.Contains(TEXT("VolumetricCloud")) || Cls.Contains(TEXT("SkySphere"))
			|| Cls.Contains(TEXT("Sky_Sphere")) || Cls.Contains(TEXT("Ultra_Dynamic")) || Cls.Contains(TEXT("BP_Sky"));
	}

	// Strips what a set must not bring with it, and measures what is left.
	FBox Settle(ULevel* Level, const DCCSets::FOptions& Options)
	{
		TArray<FBox> Boxes;
		TArray<AActor*> Doomed;
		int32 Doors = 0;
		for (AActor* Actor : TArray<AActor*>(Level->Actors))
		{
			if (!Actor || Actor->IsA<AWorldSettings>() || Actor->IsA<ALevelScriptActor>())
			{
				continue;
			}
			const FString Cls = Actor->GetClass()->GetName();
			bool bRemove = Actor->IsA<APlayerStart>() || Actor->IsA<APawn>() || Actor->IsA<ACameraActor>()
				|| Cls.Contains(TEXT("LevelSequenceActor"));
			bRemove = bRemove || (!Options.bKeepSun && Actor->IsA<ADirectionalLight>());
			bRemove = bRemove || (!Options.bKeepSky && IsSkyActor(Actor));
			bRemove = bRemove || (!Options.bKeepLocalLights && Actor->IsA<ALight>() && !Actor->IsA<ADirectionalLight>());
			if (!bRemove && Options.RemoveMeshes.Num() > 0)
			{
				TArray<UStaticMeshComponent*> Meshes;
				Actor->GetComponents(Meshes);
				for (const UStaticMeshComponent* Mesh : Meshes)
				{
					const FString MeshName = Mesh && Mesh->GetStaticMesh() ? Mesh->GetStaticMesh()->GetName() : FString();
					for (const FString& Prefix : Options.RemoveMeshes)
					{
						bRemove = bRemove || (!MeshName.IsEmpty() && MeshName.StartsWith(Prefix));
					}
				}
			}
			if (bRemove)
			{
				Doomed.Add(Actor); // destroyed after the loop: destroying shifts Level->Actors under the iterator
				continue;
			}
			if (Options.bWorkingDoors)
			{
				TArray<UStaticMeshComponent*> Leaves;
				Actor->GetComponents(Leaves);
				for (UStaticMeshComponent* Leaf : Leaves)
				{
					if (UDCCSetDoorComponent::LooksLikeDoor(Leaf))
					{
						UDCCSetDoorComponent* Door = NewObject<UDCCSetDoorComponent>(Actor);
						Door->RegisterComponent();
						Door->Setup(Leaf);
						++Doors;
					}
				}
			}
			// Volumes (post process, lightmass, nav) and helpers have no geometry to stand on.
			if (Actor->IsA<AVolume>() || Actor->IsA<AInfo>() || Actor->IsA<ALight>() || Cls.Contains(TEXT("ReflectionCapture")) || Cls.Contains(TEXT("Note")))
			{
				continue;
			}
			const FBox Box = Actor->GetComponentsBoundingBox(false);
			// Sky domes and backdrop planes would make every set look a kilometre wide.
			if (Box.IsValid && Box.GetSize().GetMax() < 60000.0)
			{
				Boxes.Add(Box);
			}
		}
		for (AActor* Actor : Doomed)
		{
			Actor->Destroy();
		}
		const int32 Removed = Doomed.Num();
		// A stray piece left far from the scene (a reference model, a forgotten test cube) would drag the bounds out
		// to it, so keep only boxes whose centre sits inside the middle 90% of centres, widened by a quarter.
		FBox Bounds(ForceInit);
		if (Boxes.Num() > 0)
		{
			FVector Lo, Hi;
			for (int32 Axis = 0; Axis < 3; ++Axis)
			{
				TArray<double> C;
				for (const FBox& B : Boxes) { C.Add(B.GetCenter()[Axis]); }
				C.Sort();
				const double A = C[FMath::FloorToInt(C.Num() * 0.05)];
				const double Z = C[FMath::Min(C.Num() - 1, FMath::FloorToInt(C.Num() * 0.95))];
				const double Pad = (Z - A) * 0.25 + 100.0;
				Lo[Axis] = A - Pad;
				Hi[Axis] = Z + Pad;
			}
			for (const FBox& B : Boxes)
			{
				const FVector C = B.GetCenter();
				if (C.X >= Lo.X && C.X <= Hi.X && C.Y >= Lo.Y && C.Y <= Hi.Y && C.Z >= Lo.Z && C.Z <= Hi.Z)
				{
					Bounds += B;
				}
			}
		}
		UE_LOG(LogTemp, Log, TEXT("[DCC Sets] %s ready: %d actors, %d removed, %d working doors, bounds %s size %s"),
			*Level->GetOuter()->GetName(), Level->Actors.Num(), Removed, Doors, *Bounds.GetCenter().ToString(), *Bounds.GetSize().ToString());
		return Bounds;
	}
}

FString DCCSets::Resolve(const FString& NameOrPath)
{
	for (const FSetAlias& Alias : GAliases)
	{
		if (NameOrPath.Equals(Alias.Name, ESearchCase::IgnoreCase))
		{
			return Alias.Path;
		}
	}
	FString Path = NameOrPath;
	int32 Dot = INDEX_NONE;
	if (Path.FindLastChar(TEXT('.'), Dot))
	{
		Path = Path.Left(Dot); // "/Game/X/Map.Map" -> "/Game/X/Map"
	}
	return Path;
}

TArray<FString> DCCSets::Aliases()
{
	TArray<FString> Out;
	for (const FSetAlias& Alias : GAliases)
	{
		Out.Add(Alias.Name);
	}
	return Out;
}

ULevelStreamingDynamic* DCCSets::Load(UWorld* World, const FString& NameOrPath, const FVector& At, const FRotator& Rot,
	const FString& InstanceName, const FOptions& Options, TFunction<void(const FBox&)> OnReady)
{
	if (!World)
	{
		return nullptr;
	}
	const FString Path = Resolve(NameOrPath);
	if (!FPackageName::DoesPackageExist(Path))
	{
		UE_LOG(LogTemp, Warning, TEXT("[DCC Sets] no map at %s"), *Path);
		return nullptr;
	}
	bool bOk = false;
	ULevelStreamingDynamic* Set = ULevelStreamingDynamic::LoadLevelInstance(World, Path, At, Rot, bOk, InstanceName);
	if (!bOk || !Set)
	{
		UE_LOG(LogTemp, Warning, TEXT("[DCC Sets] could not load %s"), *Path);
		return nullptr;
	}
	UE_LOG(LogTemp, Log, TEXT("[DCC Sets] loading %s as %s at %s"), *Path, *InstanceName, *At.ToString());
	// Poll until it is shown: no UFUNCTION needed, and a set that never shows simply never calls back.
	TSharedRef<FTimerHandle> Handle = MakeShared<FTimerHandle>();
	TWeakObjectPtr<ULevelStreamingDynamic> Weak(Set);
	TWeakObjectPtr<UWorld> WeakWorld(World);
	World->GetTimerManager().SetTimer(*Handle, FTimerDelegate::CreateLambda([Weak, WeakWorld, Handle, Options, OnReady]()
	{
		UWorld* W = WeakWorld.Get();
		ULevelStreamingDynamic* S = Weak.Get();
		if (!W || !S)
		{
			if (W)
			{
				W->GetTimerManager().ClearTimer(*Handle);
			}
			return;
		}
		ULevel* Level = S->GetLoadedLevel();
		if (!Level || !S->IsLevelVisible())
		{
			return;
		}
		// Copy what is needed first: clearing the timer destroys this lambda and everything it captured, Options and
		// OnReady included, while it is still running (2026-11-25: the door list read as empty, then crashed).
		const DCCSets::FOptions Kept = Options;
		const TFunction<void(const FBox&)> Ready = OnReady;
		W->GetTimerManager().ClearTimer(*Handle);
		const FBox Bounds = Settle(Level, Kept);
		if (Ready)
		{
			Ready(Bounds);
		}
	}), 0.25f, true);
	return Set;
}

int32 DCCSets::LockDoorsNear(UWorld* World, const FVector& At, float Radius, bool bLock)
{
	if (!World || Radius <= 0.0f)
	{
		return 0;
	}
	int32 Touched = 0;
	int32 Seen = 0;
	float Nearest = MAX_flt;
	FVector NearestAt = FVector::ZeroVector;
	for (TActorIterator<AActor> It(World); It; ++It)
	{
		TArray<UDCCSetDoorComponent*> Doors;
		It->GetComponents(Doors);
		for (UDCCSetDoorComponent* Door : Doors)
		{
			if (!Door)
			{
				continue;
			}
			const FVector Middle = Door->ClosedMiddle();
			const float D = FVector::Dist2D(Middle, At);
			++Seen;
			if (D < Nearest)
			{
				Nearest = D;
				NearestAt = Middle;
			}
			if (D <= Radius && FMath::Abs(Middle.Z - At.Z) <= 400.0f)
			{
				Door->SetLocked(bLock);
				++Touched;
			}
		}
	}
	if (Touched == 0 && Seen > 0 && bLock)
	{
		static float LastMiss = -10.0f;
		const float Now = World->GetTimeSeconds();
		if (Now - LastMiss > 2.0f)
		{
			LastMiss = Now;
			UE_LOG(LogTemp, Log, TEXT("[DCC Sets] no door within %.0f of %s. nearest %.0f at %s (%d doors)"),
				Radius, *At.ToCompactString(), Nearest, *NearestAt.ToCompactString(), Seen);
		}
	}
	return Touched;
}

void DCCSets::Unload(ULevelStreamingDynamic* Set)
{
	if (Set)
	{
		Set->SetShouldBeLoaded(false);
		Set->SetShouldBeVisible(false);
		Set->SetIsRequestingUnloadAndRemoval(true);
	}
}

// ---------------------------------------------------------------------------------------------------------------------
// Dev preview (Claude's room-building tool, not a player feature).
//   DCC.Set <alias|/Game/path> [sky] [delay]  load a set 600 m above the player, with its own lighting ("sky" keeps
//                                             its sun and sky too), and tour it with the camera.
//   DCC.SetView <0-6> [delay]                 0 outside corner, 1-4 inside facing four ways, 5 from above, 6 the player.
//   DCC.SetWalk [delay]                       drop the player onto the set's middle, to judge scale.
// ---------------------------------------------------------------------------------------------------------------------
namespace
{
	FBox GPreviewBounds(ForceInit);
	TWeakObjectPtr<ACameraActor> GPreviewCamera;
	int32 GPreviewCount = 0;

	void After(UWorld* World, float Delay, TFunction<void(UWorld*)> Fn)
	{
		if (!World)
		{
			return;
		}
		if (Delay <= 0.0f)
		{
			Fn(World);
			return;
		}
		FTimerHandle Unused;
		TWeakObjectPtr<UWorld> Weak(World);
		World->GetTimerManager().SetTimer(Unused, FTimerDelegate::CreateLambda([Weak, Fn]()
		{
			if (UWorld* W = Weak.Get())
			{
				Fn(W);
			}
		}), Delay, false);
	}

	double GPreviewFloor = 0.0;

	// The floor under a point of the set, found by a trace from its top.
	// The lowest surface under one point. A roofed hall's first hit from above is its roof (2026-11-25).
	double LowestAt(UWorld* World, double X, double Y, bool& bHit)
	{
		double Floor = GPreviewBounds.Min.Z;
		double Top = GPreviewBounds.Max.Z + 100.0;
		bHit = false;
		for (int32 i = 0; i < 8; ++i)
		{
			FHitResult Hit;
			if (!World->LineTraceSingleByChannel(Hit, FVector(X, Y, Top), FVector(X, Y, GPreviewBounds.Min.Z - 100.0), ECC_Visibility))
			{
				break;
			}
			bHit = true;
			Floor = Hit.ImpactPoint.Z;
			Top = Floor - 8.0;
		}
		return Floor;
	}

	// The set's main floor: the most common lowest surface on a grid over it. One point is not enough: the middle of
	// the Power Plant is over a basement hole, which put the floor 3 m underground (2026-11-25).
	double MainFloor(UWorld* World)
	{
		TMap<int32, int32> Votes;
		const FVector Size = GPreviewBounds.GetSize();
		for (int32 i = 1; i < 12; ++i)
		{
			for (int32 j = 1; j < 12; ++j)
			{
				bool bHit = false;
				const double Z = LowestAt(World, GPreviewBounds.Min.X + Size.X * i / 12.0, GPreviewBounds.Min.Y + Size.Y * j / 12.0, bHit);
				if (bHit)
				{
					Votes.FindOrAdd(FMath::RoundToInt32(Z / 20.0))++;
				}
			}
		}
		int32 Best = INDEX_NONE, BestVotes = 0;
		for (const TPair<int32, int32>& V : Votes)
		{
			if (V.Value > BestVotes) { Best = V.Key; BestVotes = V.Value; }
		}
		return Best == INDEX_NONE ? GPreviewBounds.Min.Z : Best * 20.0;
	}

	// 0 outside corner, 1-4 inside the middle at head height facing +X, +Y, -X, -Y, 5 straight down, 6 the player.
	void SetView(UWorld* World, int32 View)
	{
		APlayerController* PC = World ? World->GetFirstPlayerController() : nullptr;
		if (!PC || !GPreviewBounds.IsValid)
		{
			return;
		}
		if (View >= 6)
		{
			PC->SetViewTarget(PC->GetPawn());
			return;
		}
		ACameraActor* Camera = GPreviewCamera.Get();
		if (!Camera)
		{
			Camera = World->SpawnActor<ACameraActor>();
			GPreviewCamera = Camera;
		}
		const FVector C = GPreviewBounds.GetCenter();
		const FVector E = GPreviewBounds.GetExtent();
		FVector From;
		FRotator Rot;
		if (View >= 1 && View <= 4)
		{
			From = FVector(C.X, C.Y, GPreviewFloor + 230.0);
			Rot = FRotator(-6.0f, 90.0f * (View - 1), 0.0f);
		}
		else if (View == 5)
		{
			From = C + FVector(0.0, -1.0, FMath::Max(E.X, E.Y) * 1.6 + E.Z);
			Rot = (C - From).Rotation();
		}
		else
		{
			From = C + FVector(-E.X * 1.1, -E.Y * 1.1, E.Z * 0.9 + 300.0);
			Rot = (C - From).Rotation();
		}
		Camera->SetActorLocationAndRotation(From, Rot);
		if (UCameraComponent* Lens = Camera->GetCameraComponent())
		{
			Lens->SetFieldOfView(View >= 1 && View <= 4 ? 100.0f : 75.0f);
		}
		PC->SetViewTarget(Camera);
		UE_LOG(LogTemp, Log, TEXT("[DCC Sets] view %d from %s"), View, *From.ToString());
	}

	// Each view in turn, for a test run's screenshots: "[DCC Sets] tour start" is the line to wait for.
	void Tour(UWorld* World)
	{
		UE_LOG(LogTemp, Log, TEXT("[DCC Sets] tour start"));
		static const int32 Order[] = { 1, 2, 3, 4, 0, 5 };
		for (int32 i = 0; i < UE_ARRAY_COUNT(Order); ++i)
		{
			const int32 View = Order[i];
			After(World, 1.0f + 6.0f * i, [View](UWorld* W) { SetView(W, View); });
		}
	}
}

static FAutoConsoleCommandWithWorldAndArgs GDevSet(TEXT("DCC.Set"),
	TEXT("DCC.Set <alias|/Game/path> [sky] [delay]: (dev) load a Fab sample map as a set above the player and look at it."),
	FConsoleCommandWithWorldAndArgsDelegate::CreateLambda([](const TArray<FString>& Args, UWorld* World)
	{
		if (Args.Num() == 0)
		{
			UE_LOG(LogTemp, Log, TEXT("[DCC Sets] sets: %s"), *FString::Join(DCCSets::Aliases(), TEXT(", ")));
			return;
		}
		const FString Name = Args[0];
		const bool bSky = Args.Contains(TEXT("sky"));
		const float Delay = Args.Num() > 1 && FCString::IsNumeric(*Args.Last()) ? FCString::Atof(*Args.Last()) : 0.0f;
		After(World, Delay, [Name, bSky](UWorld* W)
		{
			APlayerController* PC = W->GetFirstPlayerController();
			const FVector Base = PC && PC->GetPawn() ? PC->GetPawn()->GetActorLocation() : FVector::ZeroVector;
			DCCSets::FOptions Options;
			Options.bKeepSun = bSky;
			Options.bKeepSky = bSky;
			const FString Instance = FString::Printf(TEXT("DCCSetPreview_%d"), ++GPreviewCount);
			TWeakObjectPtr<UWorld> Weak(W);
			DCCSets::Load(W, Name, Base + FVector(0.0, 0.0, 60000.0), FRotator::ZeroRotator, Instance, Options,
				[Weak](const FBox& Bounds)
				{
					GPreviewBounds = Bounds;
					if (UWorld* World2 = Weak.Get())
					{
						const FVector C = Bounds.GetCenter();
						GPreviewFloor = MainFloor(World2);
						// Give textures a few seconds to stream in before the first shot.
						After(World2, 4.0f, [](UWorld* W) { Tour(W); });
					}
				});
		});
	}));

static FAutoConsoleCommandWithWorldAndArgs GDevSetView(TEXT("DCC.SetView"),
	TEXT("DCC.SetView <0-6> [delay]: (dev) 0 outside, 1-4 inside facing four ways, 5 from above, 6 back to the player."),
	FConsoleCommandWithWorldAndArgsDelegate::CreateLambda([](const TArray<FString>& Args, UWorld* World)
	{
		const int32 View = Args.Num() > 0 ? FCString::Atoi(*Args[0]) : 0;
		const float Delay = Args.Num() > 1 ? FCString::Atof(*Args[1]) : 0.0f;
		After(World, Delay, [View](UWorld* W) { SetView(W, View); });
	}));

static FAutoConsoleCommandWithWorldAndArgs GDevSetWalk(TEXT("DCC.SetWalk"),
	TEXT("DCC.SetWalk [delay]: (dev) drop the player onto the middle of the previewed set."),
	FConsoleCommandWithWorldAndArgsDelegate::CreateLambda([](const TArray<FString>& Args, UWorld* World)
	{
		const float Delay = Args.Num() > 0 ? FCString::Atof(*Args[0]) : 0.0f;
		After(World, Delay, [](UWorld* W)
		{
			APlayerController* PC = W->GetFirstPlayerController();
			APawn* Pawn = PC ? PC->GetPawn() : nullptr;
			if (!Pawn || !GPreviewBounds.IsValid)
			{
				return;
			}
			const FVector C = GPreviewBounds.GetCenter();
			FHitResult Hit;
			FCollisionQueryParams Params;
			Params.AddIgnoredActor(Pawn);
			FVector At = FVector(C.X, C.Y, GPreviewBounds.Min.Z + 200.0);
			if (W->LineTraceSingleByChannel(Hit, FVector(C.X, C.Y, GPreviewBounds.Max.Z - 50.0), FVector(C.X, C.Y, GPreviewBounds.Min.Z - 100.0), ECC_Visibility, Params))
			{
				At = Hit.ImpactPoint + FVector(0.0, 0.0, 120.0);
			}
			Pawn->TeleportTo(At, Pawn->GetActorRotation());
			PC->SetViewTarget(Pawn);
			UE_LOG(LogTemp, Log, TEXT("[DCC Sets] player dropped at %s"), *At.ToString());
		});
	}));

// The probes trace the Pawn channel: what blocks walking. Some packs' walls ignore Visibility (the Power Plant's end
// facade does), so a Visibility probe walks straight through them (2026-11-25).
// DCC.SetProbe [step] [delay]: (dev) map the previewed set for building around it. On a grid (step cm, default 200) it
// traces straight down through every surface (roof, floors, ground) and logs "[DCC Probe] x y z1 z2 ..." relative to
// the set's bounds minimum, top surface first. Read with Tools/set_probe_map.py.
static FAutoConsoleCommandWithWorldAndArgs GDevSetProbe(TEXT("DCC.SetProbe"),
	TEXT("DCC.SetProbe [step] [delay]: (dev) log a surface map of the previewed set."),
	FConsoleCommandWithWorldAndArgsDelegate::CreateLambda([](const TArray<FString>& Args, UWorld* World)
	{
		const float Step = Args.Num() > 0 ? FMath::Max(50.0f, FCString::Atof(*Args[0])) : 200.0f;
		const float Delay = Args.Num() > 1 ? FCString::Atof(*Args[1]) : 0.0f;
		After(World, Delay, [Step](UWorld* W)
		{
			if (!GPreviewBounds.IsValid)
			{
				return;
			}
			const FVector Min = GPreviewBounds.Min;
			const FVector Max = GPreviewBounds.Max;
			UE_LOG(LogTemp, Log, TEXT("[DCC Probe] begin step %.0f min %s size %s"), Step, *Min.ToString(), *(Max - Min).ToString());
			for (double X = Min.X; X <= Max.X; X += Step)
			{
				for (double Y = Min.Y; Y <= Max.Y; Y += Step)
				{
					FString Line = FString::Printf(TEXT("[DCC Probe] %.0f %.0f"), X - Min.X, Y - Min.Y);
					double Top = Max.Z + 200.0;
					for (int32 Hit = 0; Hit < 8; ++Hit)
					{
						FHitResult R;
						if (!W->LineTraceSingleByChannel(R, FVector(X, Y, Top), FVector(X, Y, Min.Z - 300.0), ECC_Pawn))
						{
							break;
						}
						Line += FString::Printf(TEXT(" %.0f"), R.ImpactPoint.Z - Min.Z);
						Top = R.ImpactPoint.Z - 8.0;
					}
					UE_LOG(LogTemp, Log, TEXT("%s"), *Line);
				}
			}
			UE_LOG(LogTemp, Log, TEXT("[DCC Probe] end"));
		});
	}));

// DCC.SetProbeWalls [step] [height] [delay]: (dev) find walls and openings in the previewed set. Along every row (and
// then every column), a ray runs across the whole set at <height> cm above its floor (default 120) and logs where it
// crosses something: "[DCC ProbeW] X <y> hits..." and "[DCC ProbeW] Y <x> hits...", relative to the bounds minimum.
static FAutoConsoleCommandWithWorldAndArgs GDevSetProbeWalls(TEXT("DCC.SetProbeWalls"),
	TEXT("DCC.SetProbeWalls [step] [height] [delay]: (dev) log wall crossings of the previewed set."),
	FConsoleCommandWithWorldAndArgsDelegate::CreateLambda([](const TArray<FString>& Args, UWorld* World)
	{
		const float Step = Args.Num() > 0 ? FMath::Max(25.0f, FCString::Atof(*Args[0])) : 100.0f;
		const float Height = Args.Num() > 1 ? FCString::Atof(*Args[1]) : 120.0f;
		const float Delay = Args.Num() > 2 ? FCString::Atof(*Args[2]) : 0.0f;
		After(World, Delay, [Step, Height](UWorld* W)
		{
			if (!GPreviewBounds.IsValid)
			{
				return;
			}
			const FVector Min = GPreviewBounds.Min - FVector(300.0, 300.0, 0.0);
			const FVector Max = GPreviewBounds.Max + FVector(300.0, 300.0, 0.0);
			const double Z = GPreviewFloor + Height;
			auto Sweep = [W](const FVector& From, const FVector& To, int32 Axis, double Origin) -> FString
			{
				FString Out;
				FVector Start = From;
				for (int32 Hit = 0; Hit < 24; ++Hit)
				{
					FHitResult R;
					if (!W->LineTraceSingleByChannel(R, Start, To, ECC_Pawn))
					{
						break;
					}
					Out += FString::Printf(TEXT(" %.0f"), R.ImpactPoint[Axis] - Origin);
					// Step through the thing hit: find where the ray leaves it by tracing back from the far end.
					FHitResult Back;
					const FVector Inside = R.ImpactPoint + (To - From).GetSafeNormal() * 5.0;
					if (W->LineTraceSingleByChannel(Back, To, Inside, ECC_Pawn) && Back.GetActor() == R.GetActor())
					{
						Out += FString::Printf(TEXT("-%.0f"), Back.ImpactPoint[Axis] - Origin);
						Start = Back.ImpactPoint + (To - From).GetSafeNormal() * 5.0;
					}
					else
					{
						Start = Inside + (To - From).GetSafeNormal() * 20.0;
					}
				}
				return Out;
			};
			UE_LOG(LogTemp, Log, TEXT("[DCC ProbeW] begin step %.0f height %.0f min %s"), Step, Height, *GPreviewBounds.Min.ToString());
			for (double Y = Min.Y; Y <= Max.Y; Y += Step)
			{
				UE_LOG(LogTemp, Log, TEXT("[DCC ProbeW] X %.0f%s"), Y - GPreviewBounds.Min.Y,
					*Sweep(FVector(Min.X, Y, Z), FVector(Max.X, Y, Z), 0, GPreviewBounds.Min.X));
			}
			for (double X = Min.X; X <= Max.X; X += Step)
			{
				UE_LOG(LogTemp, Log, TEXT("[DCC ProbeW] Y %.0f%s"), X - GPreviewBounds.Min.X,
					*Sweep(FVector(X, Min.Y, Z), FVector(X, Max.Y, Z), 1, GPreviewBounds.Min.Y));
			}
			UE_LOG(LogTemp, Log, TEXT("[DCC ProbeW] end"));
		});
	}));
