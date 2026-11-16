#include "Arcade/DCCArcadeHaunt.h"
#include "HAL/IConsoleManager.h"
#include "PhysicsEngine/BodySetup.h"
#include "Arcade/DCCWalletComponent.h"

#include "Characters/DCCCharacterBase.h"
#include "Characters/DCCEnemyCharacter.h"
#include "Characters/DCCEnemyCharacter_Corporate.h"
#include "Characters/DCCEnemyCharacter_Wildlife.h"
#include "Rooms/DCCDungeonGrid.h"
#include "Session/DCCProfile.h"
#include "Stats/DCCStatsComponent.h"
#include "Components/CapsuleComponent.h"
#include "Components/PointLightComponent.h"
#include "Components/SpotLightComponent.h"
#include "Engine/PointLight.h"
#include "Engine/World.h"
#include "EngineUtils.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "GameFramework/PlayerController.h"
#include "GameFramework/PlayerState.h"
#include "Net/UnrealNetwork.h"
#include "Arcade/DCCMineNotes.h"
#include "Arcade/DCCChopShop.h"
#include "UI/DCCHUD.h"
#include "NiagaraComponent.h"
#include "NiagaraFunctionLibrary.h"
#include "NiagaraSystem.h"
#include "InputCoreTypes.h"
#include "Components/HierarchicalInstancedStaticMeshComponent.h"
#include "Engine/StaticMesh.h"
#include "Combat/DCCGameState.h"
#include "UI/DCCEventLogTypes.h"
#include "Animation/AnimSequence.h"
#include "Materials/MaterialInstanceDynamic.h"
#include "Components/SkeletalMeshComponent.h"
#include "Engine/SkeletalMesh.h"
#include "Kismet/GameplayStatics.h"
#include "Sound/SoundAttenuation.h"
#include "Sound/SoundWaveProcedural.h"
#include "UObject/StrongObjectPtr.h"

namespace
{
	constexpr int32 KnockRate = 22050;

	/** Three knocks on rock, the way the stories say the trapped shift signalled: a dull thump with a hard edge. */
	const TArray<int16>& KnockPcm()
	{
		static TArray<int16> Pcm;
		if (Pcm.IsEmpty())
		{
			FRandomStream R(1893);
			const int32 N = int32(KnockRate * 1.1f);
			TArray<float> Mix;
			Mix.Init(0.0f, N);
			const float Times[] = { 0.0f, 0.3f, 0.6f };
			for (float At : Times)
			{
				const int32 Start = int32(At * KnockRate);
				for (int32 i = 0; i < int32(0.25f * KnockRate) && Start + i < N; ++i)
				{
					const float T = float(i) / KnockRate;
					const float Thump = FMath::Sin(2.0f * PI * (95.0f + 60.0f * FMath::Exp(-T * 40.0f)) * T) * FMath::Exp(-T * 22.0f);
					const float Edge = R.FRandRange(-1.0f, 1.0f) * FMath::Exp(-T * 300.0f) * 0.6f;
					const float Body = FMath::Sin(2.0f * PI * 340.0f * T) * FMath::Exp(-T * 60.0f) * 0.3f;
					Mix[Start + i] += Thump + Edge + Body;
				}
			}
			Pcm.SetNumUninitialized(N);
			for (int32 i = 0; i < N; ++i)
			{
				Pcm[i] = int16(FMath::Clamp(Mix[i] * 0.8f, -1.0f, 1.0f) * 26000.0f);
			}
		}
		return Pcm;
	}

	void PlayKnock(UWorld* World, const FVector& At)
	{
		static TStrongObjectPtr<USoundAttenuation> Att;
		if (!Att.IsValid())
		{
			Att.Reset(NewObject<USoundAttenuation>(GetTransientPackage()));
			Att->Attenuation.bAttenuate = true;
			Att->Attenuation.bSpatialize = true;
			Att->Attenuation.AttenuationShape = EAttenuationShape::Sphere;
			Att->Attenuation.AttenuationShapeExtents = FVector(500.0f, 0, 0);
			Att->Attenuation.FalloffDistance = 3500.0f;
		}
		const TArray<int16>& Pcm = KnockPcm();
		USoundWaveProcedural* Wave = NewObject<USoundWaveProcedural>(World);
		Wave->SetSampleRate(KnockRate);
		Wave->NumChannels = 1;
		Wave->Duration = float(Pcm.Num()) / KnockRate;
		Wave->SoundGroup = SOUNDGROUP_Effects;
		Wave->bLooping = false;
		Wave->QueueAudio(reinterpret_cast<const uint8*>(Pcm.GetData()), Pcm.Num() * sizeof(int16));
		UGameplayStatics::PlaySoundAtLocation(World, Wave, At, 1.0f, FMath::FRandRange(0.9f, 1.1f), 0.0f, Att.Get());
	}

	/** The cage bell: three strokes of an iron bell, struck at the top of the shaft and heard all the way down. */
	const TArray<int16>& BellPcm()
	{
		static TArray<int16> Pcm;
		if (Pcm.IsEmpty())
		{
			const int32 N = int32(KnockRate * 2.6f);
			TArray<float> Mix;
			Mix.Init(0.0f, N);
			const float Partials[] = { 1.0f, 2.76f, 5.40f, 8.93f };
			const float Gains[] = { 1.0f, 0.55f, 0.3f, 0.15f };
			for (int32 Stroke = 0; Stroke < 3; ++Stroke)
			{
				const int32 Start = int32(Stroke * 0.55f * KnockRate);
				for (int32 i = 0; Start + i < N; ++i)
				{
					const float T = float(i) / KnockRate;
					float V = 0.0f;
					for (int32 p = 0; p < 4; ++p)
					{
						V += FMath::Sin(2.0f * PI * 410.0f * Partials[p] * T) * Gains[p] * FMath::Exp(-T * (1.6f + p * 1.4f));
					}
					Mix[Start + i] += V * 0.45f;
				}
			}
			Pcm.SetNumUninitialized(N);
			for (int32 i = 0; i < N; ++i)
			{
				Pcm[i] = int16(FMath::Clamp(Mix[i], -1.0f, 1.0f) * 24000.0f);
			}
		}
		return Pcm;
	}

	void PlayBell(UWorld* World, const FVector& At)
	{
		const TArray<int16>& Pcm = BellPcm();
		USoundWaveProcedural* Wave = NewObject<USoundWaveProcedural>(World);
		Wave->SetSampleRate(KnockRate);
		Wave->NumChannels = 1;
		Wave->Duration = float(Pcm.Num()) / KnockRate;
		Wave->SoundGroup = SOUNDGROUP_Effects;
		Wave->bLooping = false;
		Wave->QueueAudio(reinterpret_cast<const uint8*>(Pcm.GetData()), Pcm.Num() * sizeof(int16));
		// Not spatialised: the bell is heard everywhere in the mine, which is the point of a bell.
		UGameplayStatics::PlaySound2D(World, Wave, 0.7f);
	}
}

void ADCCArcadeHaunt::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);
	DOREPLIFETIME(ADCCArcadeHaunt, Footage);
	DOREPLIFETIME(ADCCArcadeHaunt, TimeLeft);
	DOREPLIFETIME(ADCCArcadeHaunt, Seed);
	DOREPLIFETIME(ADCCArcadeHaunt, NotesRead);
	DOREPLIFETIME(ADCCArcadeHaunt, StoriesDone);
	DOREPLIFETIME(ADCCArcadeHaunt, Rocks);
	DOREPLIFETIME(ADCCArcadeHaunt, BellRings);
	DOREPLIFETIME(ADCCArcadeHaunt, Events);
	DOREPLIFETIME(ADCCArcadeHaunt, LampsOff);
	DOREPLIFETIME(ADCCArcadeHaunt, bCrustLeft);
	DOREPLIFETIME(ADCCArcadeHaunt, Lightsticks);
	DOREPLIFETIME(ADCCArcadeHaunt, ReelPhase);
	DOREPLIFETIME(ADCCArcadeHaunt, ExtractStart);
	DOREPLIFETIME(ADCCArcadeHaunt, ReelStart);
	DOREPLIFETIME(ADCCArcadeHaunt, Reel);
	DOREPLIFETIME(ADCCArcadeHaunt, Uploaders);
}

ADCCArcadeArena::FSky ADCCArcadeHaunt::GetSky() const
{
	FSky Sky;
	Sky.SunPitch = -10.0f;
	Sky.SunIntensity = 0.05f; // night
	Sky.SunColour = FLinearColor(0.5f, 0.6f, 1.0f);
	Sky.SkyIntensity = 0.05f;
	Sky.FogColour = FLinearColor(0.03f, 0.025f, 0.02f); // rock dust in the dark, not night air
	Sky.FogDensity = 0.05f;
	return Sky;
}

const FDCCFootageEntry* ADCCArcadeHaunt::FindEntry(const FString& Player) const
{
	return Footage.FindByPredicate([&Player](const FDCCFootageEntry& E) { return E.Player == Player; });
}

FDCCFootageEntry& ADCCArcadeHaunt::EntryFor(const FString& Player)
{
	if (FDCCFootageEntry* Found = Footage.FindByPredicate([&Player](const FDCCFootageEntry& E) { return E.Player == Player; }))
	{
		return *Found;
	}
	FDCCFootageEntry& Entry = Footage.AddDefaulted_GetRef();
	Entry.Player = Player;
	Entry.Tape = TapeSeconds;
	return Entry;
}

void ADCCArcadeHaunt::EnsureGrid()
{
	if (Grid || Seed == 0)
	{
		return;
	}
	Grid = NewObject<UDCCDungeonGrid>(this);
	Grid->SetOrigin(GetActorLocation(), TileSize);
	Grid->Build(GridSize, Seed, /*FightRooms=*/6, /*ChestRooms=*/0);

	const UDCCDungeonGrid::FRoom& Foyer = Grid->GetRooms()[0];
	const FVector Min = GetActorLocation() + FVector(Foyer.X * TileSize, Foyer.Y * TileSize, -100.0f);
	VanBox = FBox(Min + FVector(80, 80, 0), Min + FVector(Foyer.W * TileSize - 80, Foyer.H * TileSize - 80, 400.0f));
}

static TAutoConsoleVariable<int32> GDCCMineSeed(TEXT("dcc.MineSeed"), 0,
	TEXT("Dead Air: use this mine layout seed instead of a random one (0 = random). For trailers and testing."));

void ADCCArcadeHaunt::PrepareServer()
{
	// The seed decides the house, and the house decides where the van (and so
	// the PlayerStarts) are, so it is chosen before anyone logs in.
	Seed = FMath::RandRange(1, 1 << 20);
	// A fixed layout on request (dcc.MineSeed), so a trailer can use a mine that has every trap.
	if (const IConsoleVariable* Fixed = IConsoleManager::Get().FindConsoleVariable(TEXT("dcc.MineSeed")))
	{
		if (Fixed->GetInt() > 0) { Seed = Fixed->GetInt(); }
	}
	// Or on the command line (-MineSeed=N), since the mine is built before any ExecCmds run.
	int32 FromLine = 0;
	if (FParse::Value(FCommandLine::Get(), TEXT("MineSeed="), FromLine) && FromLine > 0) { Seed = FromLine; }
	EnsureGrid();
	EndsAt = -1.0;
}

void ADCCArcadeHaunt::GetStartTransforms(TArray<FTransform>& Out) const
{
	const FVector Centre = VanBox.IsValid ? VanBox.GetCenter() : GetActorLocation();
	for (int32 i = 0; i < 8; ++i)
	{
		const float A = i * (2.0f * PI / 8.0f);
		Out.Add(FTransform(FRotator::ZeroRotator, FVector(Centre.X, Centre.Y, GetActorLocation().Z + 120.0f) + FVector(FMath::Cos(A), FMath::Sin(A), 0) * 180.0f));
	}
}

void ADCCArcadeHaunt::BuildHouse()
{
	if (bHouseBuilt || !Grid)
	{
		return;
	}
	bHouseBuilt = true;

	const FVector O = GetActorLocation();
	const int32 Size = Grid->GetSize();
	// Underground since 2026-11-15: rock and packed dirt, not floorboards and wallpaper.
	const FLinearColor Floorboards(0.055f, 0.045f, 0.038f);
	const FLinearColor Wallpaper(0.075f, 0.065f, 0.055f);

	// Floors: one block per horizontal run of open cells.
	for (int32 Y = 0; Y < Size; ++Y)
	{
		for (int32 X = 0; X < Size;)
		{
			if (!Grid->IsOpen(X, Y))
			{
				++X;
				continue;
			}
			int32 Run = 1;
			while (X + Run < Size && Grid->IsOpen(X + Run, Y))
			{
				++Run;
			}
			Block(O + FVector((X + Run * 0.5f) * TileSize, (Y + 0.5f) * TileSize, -20), FVector(Run * TileSize, TileSize, 40), Floorboards);
			X += Run;
		}
	}

	// Walls on every open cell edge that faces a closed one, merged into runs.
	for (int32 Y = 0; Y <= Size; ++Y)
	{
		for (int32 X = 0; X < Size;)
		{
			const bool bEdge = Grid->IsOpen(X, Y) != Grid->IsOpen(X, Y - 1);
			if (!bEdge)
			{
				++X;
				continue;
			}
			int32 Run = 1;
			while (X + Run < Size && Grid->IsOpen(X + Run, Y) != Grid->IsOpen(X + Run, Y - 1))
			{
				++Run;
			}
			Block(O + FVector((X + Run * 0.5f) * TileSize, Y * TileSize, WallHeight * 0.5f), FVector(Run * TileSize, 30, WallHeight), Wallpaper);
			X += Run;
		}
	}
	for (int32 X = 0; X <= Size; ++X)
	{
		for (int32 Y = 0; Y < Size;)
		{
			const bool bEdge = Grid->IsOpen(X, Y) != Grid->IsOpen(X - 1, Y);
			if (!bEdge)
			{
				++Y;
				continue;
			}
			int32 Run = 1;
			while (Y + Run < Size && Grid->IsOpen(X, Y + Run) != Grid->IsOpen(X - 1, Y + Run))
			{
				++Run;
			}
			Block(O + FVector(X * TileSize, (Y + Run * 0.5f) * TileSize, WallHeight * 0.5f), FVector(30, Run * TileSize, WallHeight), Wallpaper);
			Y += Run;
		}
	}
	// The rock overhead.
	Block(O + FVector(Size * TileSize * 0.5f, Size * TileSize * 0.5f, WallHeight + 20), FVector(Size * TileSize + 200, Size * TileSize + 200, 40),
		FLinearColor(0.035f, 0.03f, 0.027f));

	// Stopes: a guttering lantern each, a lamp with no miner in it, sometimes the miner, and the thing in the deepest one.
	const TArray<UDCCDungeonGrid::FRoom>& Rooms = Grid->GetRooms();
	for (int32 r = 0; r < Rooms.Num(); ++r)
	{
		const UDCCDungeonGrid::FRoom& Room = Rooms[r];
		const FVector Centre = Grid->CellCenter(Room.Center());
		const bool bFoyer = r == 0;
		const bool bDeepest = r == Rooms.Num() - 1;

		FActorSpawnParameters Params;
		Params.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
		if (APointLight* Bulb = GetWorld()->SpawnActor<APointLight>(Centre + FVector(0, 0, WallHeight - 60), FRotator::ZeroRotator, Params))
		{
			Bulb->SetReplicates(false);
			const float Base = bFoyer ? 2500.0f : bDeepest ? 900.0f : 500.0f;
			if (UPointLightComponent* Light = Cast<UPointLightComponent>(Bulb->GetLightComponent()))
			{
				Light->SetMobility(EComponentMobility::Movable);
				Light->SetIntensity(Base);
				Light->SetAttenuationRadius(1100.0f);
				Light->SetLightColor(bFoyer ? FLinearColor(0.85f, 0.9f, 1.0f) : bDeepest ? FLinearColor(1.0f, 0.15f, 0.1f) : FLinearColor(1.0f, 0.58f, 0.25f));
				Light->SetCastShadows(false);
			}
			RoomLights.Add(Bulb);
			RoomLightBase.Add(Base);
			LocalActors.Add(Bulb);
		}

		if (bFoyer)
		{
			Block(Centre + FVector(0, 0, 2), FVector(Room.W * TileSize - 160, Room.H * TileSize - 160, 4), FLinearColor(0.15f, 0.25f, 0.12f));
			Sign(Centre + FVector(0, 0, 380), 180.0f, TEXT("MOUNT FOURTEENER"), FColor(200, 220, 255), 30.0f);
			Sign(Centre + FVector(0, 0, 330), 180.0f, TEXT("THE CAGE"), FColor(140, 255, 160), 60.0f);
			Sign(Centre + FVector(0, 0, 280), 180.0f, TEXT("LUCKY TUESDAY MINE  -  SHAFT No. 2  -  CLOSED 1893"), FColor(230, 200, 150), 22.0f);
			Sign(Centre + FVector(0, 0, 250), 180.0f, TEXT("Be in the cage when the clock runs out, or your tape stays down here."), FColor(200, 220, 200), 18.0f);
			Sign(Centre + FVector(0, 0, 225), 180.0f, TEXT("V records. Tape is limited. Ghosts, monsters, and each other's suffering all rate."), FColor(200, 220, 200), 18.0f);
			continue;
		}

		FRandomStream Stream(Seed * 31 + r);
		const int32 Count = bDeepest ? 1 : Stream.RandRange(1, 2);
		TArray<FIntPoint> OrbCells;
		for (int32 i = 0; i < Count; ++i)
		{
			const FIntPoint Cell(Room.X + Stream.RandRange(1, FMath::Max(1, Room.W - 2)), Room.Y + Stream.RandRange(1, FMath::Max(1, Room.H - 2)));
			OrbCells.Add(Cell);
			FAnomaly Anomaly;
			Anomaly.Location = Grid->CellCenter(Cell) + FVector(0, 0, bDeepest ? 180.0f : 120.0f + Stream.FRandRange(0, 120));
			Anomaly.Value = bDeepest ? 15.0f : 4.0f;
			const FVector OrbSize = bDeepest ? FVector(90) : FVector(35);
			if (AActor* Orb = Shape(TEXT("Sphere"), Anomaly.Location, OrbSize, bDeepest ? FLinearColor(1.0f, 0.2f, 0.2f) : FLinearColor(0.6f, 0.9f, 1.0f), FRotator::ZeroRotator, false))
			{
				Orb->SetActorEnableCollision(false);
				Anomaly.Visual = Orb;
			}
			Light(Anomaly.Location, bDeepest ? FLinearColor(1.0f, 0.1f, 0.1f) : FLinearColor(0.4f, 0.7f, 1.0f), bDeepest ? 3000.0f : 700.0f, 500.0f);
			Anomalies.Add(Anomaly);
		}

		// A ghost miner in most stopes: there, and then not, and gone if you walk up to it.
		if (!bDeepest && Stream.FRand() < 0.65f)
		{
			// Never in an orb's cell or next to one: an orb at chest height inside him read as a solid block in his
			// middle (2026-11-15). The stream is drawn the same number of times whatever happens, so every machine agrees.
			FIntPoint Cell(Room.X + Stream.RandRange(1, FMath::Max(1, Room.W - 2)), Room.Y + Stream.RandRange(1, FMath::Max(1, Room.H - 2)));
			for (int32 Try = 0; Try < 8; ++Try)
			{
				const FIntPoint Other(Room.X + Stream.RandRange(1, FMath::Max(1, Room.W - 2)), Room.Y + Stream.RandRange(1, FMath::Max(1, Room.H - 2)));
				auto Crowded = [&OrbCells](const FIntPoint& C) { for (const FIntPoint& O : OrbCells) { if (FMath::Abs(C.X - O.X) <= 1 && FMath::Abs(C.Y - O.Y) <= 1) { return true; } } return false; };
				if (Crowded(Cell) && !Crowded(Other)) { Cell = Other; }
			}
			FAnomaly Ghost;
			Ghost.bGhost = true;
			Ghost.Value = 9.0f;
			Ghost.Phase = Stream.FRandRange(0.0f, 100.0f);
			const FVector Feet = Grid->CellCenter(Cell);
			Ghost.Location = Feet + FVector(0, 0, 110.0f);
			Ghost.Visual = SpawnGhostMiner(Feet, Stream.FRandRange(0.0f, 360.0f), Stream.RandRange(0, 11));
			Anomalies.Add(Ghost);
		}
	}
	DressMine();
	DressCave();
	BuildDepth();
	PlaceBodies();
	PlaceTraps();

	// The Mine Outfitters, in the cage: crates, a lantern, and a pad that opens the counter on the OUTFITTERS tab.
	if (Grid->GetRooms().Num() > 0)
	{
		const UDCCDungeonGrid::FRoom& Cage = Grid->GetRooms()[0];
		OutfitPad = Grid->CellCenter(FIntPoint(Cage.X + Cage.W - 1, Cage.Y));
		Block(OutfitPad + FVector(0, 0, 3), FVector(TileSize * 0.8f, TileSize * 0.8f, 4), FLinearColor(0.2f, 0.6f, 0.4f), 0.0f, false);
		Prop(TEXT("/Game/Cave_Camp/Meshes/Props_02/SM_crate_group_02"), OutfitPad + FVector(TileSize * 0.3f, TileSize * 0.3f, 0), 200.0f, FVector(0, 0, 130), false);
		Light(OutfitPad + FVector(0, 0, 300), FLinearColor(0.5f, 1.0f, 0.6f), 2500.0f, 700.0f);
		Sign(OutfitPad + FVector(0, 0, 260), 180.0f, TEXT("MINE OUTFITTERS"), FColor(140, 255, 170), 30.0f);
		Sign(OutfitPad + FVector(0, 0, 225), 180.0f, TEXT("tanks  -  lamps  -  lightsticks  -  hats  -  rope"), FColor(200, 230, 210), 14.0f);
	}

	// Where each level of air tank runs out: a red lantern hung on a timber post in the tunnel, and a board.
	for (int32 Level = 0; Level < 3; ++Level)
	{
		const float Line = DCCChopShop::AirReach(Level);
		const int32 LineSize = Grid->GetSize();
		for (int32 Y = 0; Y < LineSize; ++Y)
		{
			bool bDone = false;
			for (int32 X = 0; X < LineSize && !bDone; ++X)
			{
				const float D = DepthMap.IsValidIndex(Y * LineSize + X) ? DepthMap[Y * LineSize + X] : 0.0f;
				if (Grid->IsOpen(X, Y) && Grid->RoomIndexAt(X, Y) == -1 && D >= Line && D < Line + 0.03f)
				{
					const FVector At = Grid->CellCenter(FIntPoint(X, Y));
					Block(At + FVector(0, 0, WallHeight * 0.5f), FVector(26, 26, WallHeight), FLinearColor(0.15f, 0.09f, 0.045f), 0.0f, false);
					Shape(TEXT("Sphere"), At + FVector(0, 30, WallHeight * 0.7f), FVector(26, 26, 32), FLinearColor(1.0f, 0.15f, 0.1f), FRotator::ZeroRotator, false);
					Light(At + FVector(0, 30, WallHeight * 0.7f), FLinearColor(1.0f, 0.15f, 0.1f), 1800.0f, 700.0f);
					Sign(At + FVector(0, 0, WallHeight * 0.55f), 180.0f, FString::Printf(TEXT("AIR LINE  -  %d TANK%s PAST HERE"), Level + 1, Level ? TEXT("S") : TEXT("")), FColor(255, 120, 100), 13.0f);
					bDone = true;
				}
			}
			if (bDone) { break; }
		}
	}
}

void ADCCArcadeHaunt::DressCave()
{
	// A dark, textured cave (2026-11-15, Zoe: "make it look a bit more like a dark textured cave"). The walls are
	// still the blocks that collide; real rock is laid along their faces so every tunnel reads as hewn stone, and
	// the floor and roof get rock materials. The rock is instanced, one component per mesh, not an actor each.
	const FVector O = GetActorLocation();
	const int32 Size = Grid->GetSize();
	FRandomStream R(Seed * 13 + 9);
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

	static const TCHAR* Faces[] = {
		TEXT("/Game/Cave_Camp/Meshes/Rocks/SM_rock_01"), TEXT("/Game/Cave_Camp/Meshes/Rocks/SM_rock_02"), TEXT("/Game/Cave_Camp/Meshes/Rocks/SM_rock_03"),
		TEXT("/Game/Cave_Camp/Meshes/Rocks/SM_rock_04"), TEXT("/Game/Cave_Camp/Meshes/Rocks/SM_rock_05"),
	};
	TArray<UHierarchicalInstancedStaticMeshComponent*> Parts;
	TArray<FBox> Bounds;
	for (const TCHAR* Path : Faces)
	{
		UStaticMesh* Mesh = LoadObject<UStaticMesh>(nullptr, *(FString(Path) + TEXT(".") + FPaths::GetBaseFilename(Path)));
		UHierarchicalInstancedStaticMeshComponent* Part = Mesh ? NewObject<UHierarchicalInstancedStaticMeshComponent>(Holder) : nullptr;
		if (Part)
		{
			Part->SetStaticMesh(Mesh);
			Part->SetMobility(EComponentMobility::Static);
			// Solid (2026-11-15). Meshes with no collision shape of their own get none from this; they are logged.
			Part->SetCollisionProfileName(TEXT("BlockAll"));
			Part->SetCastShadow(false);
			if (!Mesh->GetBodySetup() || Mesh->GetBodySetup()->AggGeom.GetElementCount() == 0)
			{
				UE_LOG(LogTemp, Warning, TEXT("[DCC Solid] cave rock %s has no simple collision"), *Mesh->GetName());
			}
			Part->SetupAttachment(HolderRoot);
			Part->RegisterComponent();
			Holder->AddInstanceComponent(Part);
		}
		Parts.Add(Part);
		Bounds.Add(Mesh ? Mesh->GetBoundingBox() : FBox(ForceInit));
	}
	auto Rock = [&](const FVector& Base, float Height, float Yaw, float Tilt, float Depth = 1.0f)
	{
		const int32 Pick = R.RandRange(0, Parts.Num() - 1);
		if (!Parts[Pick])
		{
			return;
		}
		const FVector MeshSize = Bounds[Pick].GetSize().ComponentMax(FVector(1.0f));
		const float Scale = Height / MeshSize.Z;
		const FVector S(Scale * R.FRandRange(0.8f, 1.2f), Scale * R.FRandRange(0.8f, 1.2f) * Depth, Scale);
		const FRotator Rot(Tilt, Yaw, R.FRandRange(-8.0f, 8.0f));
		const FVector Centre = Bounds[Pick].GetCenter() * S;
		const FVector Loc = Base - Rot.RotateVector(FVector(Centre.X, Centre.Y, 0.0f)) - FVector(0, 0, Bounds[Pick].Min.Z * S.Z);
		Parts[Pick]->AddInstance(FTransform(Rot, Loc, S), true);
	};

	// Rock along every wall face: one or two boulders per cell edge, set into the wall so only their faces show.
	for (int32 Y = 0; Y < Size; ++Y)
	{
		for (int32 X = 0; X < Size; ++X)
		{
			if (!Grid->IsOpen(X, Y))
			{
				continue;
			}
			const FVector C = O + FVector((X + 0.5f) * TileSize, (Y + 0.5f) * TileSize, 0.0f);
			const FIntPoint Sides[] = { FIntPoint(1, 0), FIntPoint(-1, 0), FIntPoint(0, 1), FIntPoint(0, -1) };
			for (const FIntPoint& Side : Sides)
			{
				if (Grid->IsOpen(X + Side.X, Y + Side.Y))
				{
					continue;
				}
				const FVector Out(Side.X, Side.Y, 0.0f);
				const FVector Along(-Side.Y, Side.X, 0.0f);
				const int32 Count = R.RandRange(1, 2);
				for (int32 k = 0; k < Count; ++k)
				{
					// Flattened against the face (its depth squashed, its length along the wall) and set back into it.
					const FVector Base = C + Out * (TileSize * 0.5f + 70.0f) + Along * R.FRandRange(-TileSize * 0.4f, TileSize * 0.4f) + FVector(0, 0, -30);
					Rock(Base, R.FRandRange(WallHeight * 0.7f, WallHeight * 1.05f), Along.Rotation().Yaw + R.FRandRange(-18.0f, 18.0f), R.FRandRange(-8.0f, 8.0f), 0.45f);
				}
				// Loose stone at the foot of the wall.
				if (R.FRand() < 0.45f)
				{
					Rock(C + Out * (TileSize * 0.5f - 60.0f) + Along * R.FRandRange(-TileSize * 0.4f, TileSize * 0.4f), R.FRandRange(40.0f, 90.0f), R.FRandRange(0.0f, 360.0f), 0.0f);
				}
			}
		}
	}

	// The floor in cave stone: one paper-thin painted tile per cell (a long strip stretches the texture into a smear,
	// and a thick tile's edges streak), just above the collision floor.
	for (int32 Y = 0; Y < Size; ++Y)
	{
		for (int32 X = 0; X < Size; ++X)
		{
			if (Grid->IsOpen(X, Y))
			{
				if (AActor* Tile = Block(O + FVector((X + 0.5f) * TileSize, (Y + 0.5f) * TileSize, 0.6f), FVector(TileSize, TileSize, 0.2f), FLinearColor(0.05f, 0.045f, 0.04f), 0.0f, false))
				{
					Paint(Tile, TEXT("/Game/Alien_Portal/Material/MI_rock_02"));
				}
			}
		}
	}

	// Mount Fourteener has never been left alone: marks from everyone who came after the miners.
	struct FMark { const TCHAR* Text; FColor Colour; };
	const FMark Marks[] = {
		{ TEXT("U.S.G.S. BENCH MARK  -  MT. FOURTEENER  -  1931  -  ELEV. 11,240"), FColor(200, 180, 120) },
		{ TEXT("COLO. SCHOOL OF MINES  -  FIELD STUDY 1967  -  DO NOT PROCEED BEYOND THIS POINT"), FColor(210, 210, 200) },
		{ TEXT("SPOOKSEEKERS WUZ HERE  04"), FColor(255, 90, 160) },
		{ TEXT("FOURTEENER RESORT & SPA  -  PHASE II  -  SURVEY STAKE 112"), FColor(255, 170, 60) },
		{ TEXT("ALPINE RESCUE  -  SEARCH GRID C  -  CLEARED 1988"), FColor(255, 120, 90) },
		{ TEXT("CLAIM: THE WHITE STREAK  -  E. ROURKE  -  1892  -  KEEP OUT"), FColor(230, 220, 190) },
		{ TEXT("TO FORT TINER VIA No. 1 ADIT  ->  (COLLAPSED)"), FColor(200, 200, 160) },
		{ TEXT("LEVEL 2  -  14,115 FT SUMMIT IS 3,960 FT ABOVE YOU"), FColor(200, 200, 160) },
	};
	TArray<FIntPoint> Walls;
	for (int32 Y = 0; Y < Size; ++Y)
	{
		for (int32 X = 0; X < Size; ++X)
		{
			if (Grid->IsOpen(X, Y) && Grid->RoomIndexAt(X, Y) == -1 && !Grid->IsOpen(X, Y + 1))
			{
				Walls.Add(FIntPoint(X, Y));
			}
		}
	}
	for (const FMark& Mark : Marks)
	{
		if (Walls.IsEmpty())
		{
			break;
		}
		const int32 Pick = R.RandRange(0, Walls.Num() - 1);
		const FIntPoint Cell = Walls[Pick];
		Walls.RemoveAt(Pick);
		// On the north face of a tunnel, facing back into it, at eye height.
		Sign(O + FVector((Cell.X + 0.5f) * TileSize, (Cell.Y + 1.0f) * TileSize - 110.0f, 190.0f), -90.0f, Mark.Text, Mark.Colour, 16.0f);
	}
}

AActor* ADCCArcadeHaunt::SpawnGhostMiner(const FVector& Feet, float Yaw, int32 BodyIndex)
{
	static const TCHAR* Forge[] = {
		TEXT("worker_301"), TEXT("worker_304"), TEXT("worker_307"), TEXT("worker_310"), TEXT("casual_300"), TEXT("casual_303"),
		TEXT("casual_306"), TEXT("casual_309"), TEXT("worker_301"), TEXT("worker_304"), TEXT("worker_307"), TEXT("worker_310"),
	};
	const TCHAR* Id = Forge[FMath::Clamp(BodyIndex, 0, 11)];
	USkeletalMesh* Body = LoadObject<USkeletalMesh>(nullptr, *FString::Printf(TEXT("/Game/DCC/Characters/NPCs/NPC_%s/SK_NPC_%s.SK_NPC_%s"), Id, Id, Id));
	UAnimSequence* Idle = LoadObject<UAnimSequence>(nullptr, TEXT("/Game/DCC/Characters/NPCs/Anims/NPC_MM_Idle.NPC_MM_Idle"));
	AActor* Ghost = GetWorld()->SpawnActor<AActor>(AActor::StaticClass(), FTransform(FRotator(0.0f, Yaw, 0.0f), Feet));
	if (!Ghost || !Body)
	{
		return Ghost;
	}
	Ghost->SetReplicates(false);
	USkeletalMeshComponent* Mesh = NewObject<USkeletalMeshComponent>(Ghost, TEXT("Miner"));
	Ghost->SetRootComponent(Mesh);
	Mesh->SetSkeletalMeshAsset(Body);
	Mesh->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	Mesh->SetCastShadow(false);
	Mesh->SetAnimationMode(EAnimationMode::AnimationSingleNode);
	Mesh->SetWorldLocationAndRotation(Feet, FRotator(0.0f, Yaw - 90.0f, 0.0f));
	Mesh->RegisterComponent();
	if (Idle)
	{
		Mesh->PlayAnimation(Idle, true);
		Mesh->SetPlayRate(0.45f); // slow, like something remembering how to stand
	}
	// Ghostly (2026-11-15, Zoe: "make the ghosts a little more evidently ghosts"): see-through and glowing pale blue,
	// with the faint drifting scan lines of the hologram material, instead of a solid man in a tinted vest. Every
	// slot gets the same instance, so nothing of his clothes shows. ClientTick shimmers its glow and floats him.
	// (M_hologram had bUsedWithSkeletalMesh switched on for this, and saved.)
	if (UMaterialInterface* Holo = LoadObject<UMaterialInterface>(nullptr, TEXT("/Game/Cyberpunk_City/Materials/Master/M_hologram.M_hologram")))
	{
		UMaterialInstanceDynamic* Spirit = UMaterialInstanceDynamic::Create(Holo, Ghost);
		Spirit->SetVectorParameterValue(TEXT("Emissive Color"), FLinearColor(0.45f, 0.8f, 1.0f, 1.0f));
		Spirit->SetScalarParameterValue(TEXT("Emissive Intensity"), 3.5f);
		Spirit->SetScalarParameterValue(TEXT("Tiling Hologram"), 14.0f);
		for (int32 Slot = 0; Slot < Mesh->GetNumMaterials(); ++Slot)
		{
			Mesh->SetMaterial(Slot, Spirit);
		}
	}
	else if (UMaterialInterface* Shade = LoadObject<UMaterialInterface>(nullptr, TEXT("/Game/DCC/Materials/M_CharacterShade.M_CharacterShade")))
	{
		UMaterialInstanceDynamic* Cold = UMaterialInstanceDynamic::Create(Shade, Ghost);
		Cold->SetVectorParameterValue(TEXT("Tint"), FLinearColor(0.35f, 0.6f, 1.0f, 1.0f));
		Cold->SetVectorParameterValue(TEXT("Edge"), FLinearColor(0.8f, 0.95f, 1.0f, 1.0f));
		Cold->SetScalarParameterValue(TEXT("RimStrength"), 1.0f);
		Mesh->SetOverlayMaterial(Cold);
	}
	// Cold light from nowhere, on him alone.
	UPointLightComponent* Cold = NewObject<UPointLightComponent>(Ghost, TEXT("Cold"));
	Cold->SetupAttachment(Mesh);
	Cold->SetRelativeLocation(FVector(0.0f, 60.0f, 150.0f));
	Cold->SetIntensity(1800.0f);
	Cold->SetAttenuationRadius(380.0f);
	Cold->SetLightColor(FLinearColor(0.45f, 0.75f, 1.0f));
	Cold->SetCastShadows(false);
	Cold->RegisterComponent();
	LocalActors.Add(Ghost);
	return Ghost;
}

bool ADCCArcadeHaunt::CorridorSpan(int32 X, int32 Y, FVector& Centre, bool& bAlongX) const
{
	auto Drift = [this](int32 CX, int32 CY) { return Grid->IsOpen(CX, CY) && Grid->RoomIndexAt(CX, CY) == -1; };
	if (!Grid || !Drift(X, Y))
	{
		return false;
	}
	// Along X: this cell and the one above it are the tunnel; rock beyond both; tunnel continues both ways.
	if (Drift(X, Y + 1) && !Grid->IsOpen(X, Y - 1) && !Grid->IsOpen(X, Y + 2)
		&& Drift(X - 1, Y) && Drift(X + 1, Y) && Drift(X - 1, Y + 1) && Drift(X + 1, Y + 1))
	{
		bAlongX = true;
		Centre = Grid->CellCenter(FIntPoint(X, Y)) + FVector(0.0f, TileSize * 0.5f, 0.0f);
		return true;
	}
	if (Drift(X + 1, Y) && !Grid->IsOpen(X - 1, Y) && !Grid->IsOpen(X + 2, Y)
		&& Drift(X, Y - 1) && Drift(X, Y + 1) && Drift(X + 1, Y - 1) && Drift(X + 1, Y + 1))
	{
		bAlongX = false;
		Centre = Grid->CellCenter(FIntPoint(X, Y)) + FVector(TileSize * 0.5f, 0.0f, 0.0f);
		return true;
	}
	return false;
}

void ADCCArcadeHaunt::DressMine()
{
	const FVector O = GetActorLocation();
	const int32 Size = Grid->GetSize();
	const FLinearColor Timber(0.15f, 0.09f, 0.045f);
	const FLinearColor Iron(0.11f, 0.11f, 0.12f);
	FRandomStream R(Seed * 7 + 3);
	auto Open = [this](int32 X, int32 Y) { return Grid->IsOpen(X, Y); };

	// Drifts: rails down the middle, ties under them, and a timber set every other cell. Corridors are two cells
	// wide, so this works on the middle line between the two (CorridorSpan).
	for (int32 Y = 0; Y < Size; ++Y)
	{
		for (int32 X = 0; X < Size; ++X)
		{
			FVector C;
			bool bX = false;
			if (!CorridorSpan(X, Y, C, bX))
			{
				continue; // a room, a bend, a junction, or the upper half of a tunnel
			}
			const FVector Along = bX ? FVector(1, 0, 0) : FVector(0, 1, 0);
			const FVector Across = bX ? FVector(0, 1, 0) : FVector(1, 0, 0);
			for (int32 Side = -1; Side <= 1; Side += 2)
			{
				Block(C + Across * (Side * 55.0f) + FVector(0, 0, 6), bX ? FVector(TileSize, 8, 8) : FVector(8, TileSize, 8), Iron, 0.0f, false);
			}
			for (float T : { -130.0f, 0.0f, 130.0f })
			{
				Block(C + Along * T + FVector(0, 0, 2), bX ? FVector(28, 170, 5) : FVector(170, 28, 5), Timber * 0.8f, 0.0f, false);
			}
			if ((X + Y) % 2 == 0)
			{
				for (int32 Side = -1; Side <= 1; Side += 2)
				{
					Block(C + Across * (Side * (TileSize - 35.0f)) + FVector(0, 0, WallHeight * 0.5f), FVector(34, 34, WallHeight), Timber, 0.0f, false);
				}
				Block(C + FVector(0, 0, WallHeight - 30.0f), bX ? FVector(40, TileSize * 2.0f - 20, 40) : FVector(TileSize * 2.0f - 20, 40, 40), Timber, 0.0f, false);
			}
		}
	}

	static const TCHAR* CartMeshes[] = { TEXT("/Game/Desert_cave/Meshes/Cart/SM_cart_01"), TEXT("/Game/Desert_cave/Meshes/Cart/SM_cart_02") };
	static const TCHAR* Stores[] = {
		TEXT("/Game/Cave_Camp/Meshes/Props_02/SM_barrel_01"), TEXT("/Game/Cave_Camp/Meshes/Props_02/SM_barrel_03"),
		TEXT("/Game/Cave_Camp/Meshes/Props_02/SM_crate_01"), TEXT("/Game/Cave_Camp/Meshes/Props_02/SM_crate_group_01"),
	};
	static const TCHAR* Spoil[] = {
		TEXT("/Game/Cave_Camp/Meshes/Rocks/SM_rock_01"), TEXT("/Game/Cave_Camp/Meshes/Rocks/SM_rock_02"), TEXT("/Game/Cave_Camp/Meshes/Rocks/SM_rock_03"),
		TEXT("/Game/Cave_Camp/Meshes/Rocks/SM_rock_04"), TEXT("/Game/Cave_Camp/Meshes/Rocks/SM_rock_05"),
	};
	static const TCHAR* Drips[] = {
		TEXT("/Game/Cave_Camp/Meshes/Rocks/SM_stalactic_01"), TEXT("/Game/Cave_Camp/Meshes/Rocks/SM_stalactic_03"),
		TEXT("/Game/Cave_Camp/Meshes/Rocks/SM_stalactic_05"), TEXT("/Game/Cave_Camp/Meshes/Rocks/SM_stalactic_07"),
	};

	const TArray<UDCCDungeonGrid::FRoom>& Rooms = Grid->GetRooms();
	for (int32 r = 0; r < Rooms.Num(); ++r)
	{
		const UDCCDungeonGrid::FRoom& Room = Rooms[r];
		const FVector Centre = Grid->CellCenter(Room.Center());
		auto RandomCell = [&](bool bEdge)
		{
			FIntPoint Cell(Room.X + R.RandRange(0, Room.W - 1), Room.Y + R.RandRange(0, Room.H - 1));
			if (bEdge)
			{
				if (R.FRand() < 0.5f) { Cell.X = R.FRand() < 0.5f ? Room.X : Room.X + Room.W - 1; }
				else { Cell.Y = R.FRand() < 0.5f ? Room.Y : Room.Y + Room.H - 1; }
			}
			return Grid->CellCenter(Cell);
		};

		if (r == 0)
		{
			// The cage: an iron frame round the lift floor, a crossbar overhead and the hoist rope going up into the dark.
			const FVector Half(Room.W * TileSize * 0.5f - 120.0f, Room.H * TileSize * 0.5f - 120.0f, 0.0f);
			for (int32 c = 0; c < 4; ++c)
			{
				const FVector Corner = Centre + FVector(c % 2 ? Half.X : -Half.X, c / 2 ? Half.Y : -Half.Y, WallHeight * 0.5f);
				Block(Corner, FVector(24, 24, WallHeight), Iron, 0.0f, false);
			}
			for (int32 Side = -1; Side <= 1; Side += 2)
			{
				Block(Centre + FVector(Side * Half.X, 0, WallHeight - 40.0f), FVector(20, Half.Y * 2.0f, 20), Iron, 0.0f, false);
				Block(Centre + FVector(0, Side * Half.Y, WallHeight - 40.0f), FVector(Half.X * 2.0f, 20, 20), Iron, 0.0f, false);
				Block(Centre + FVector(Side * Half.X, 0, 110.0f), FVector(14, Half.Y * 2.0f, 14), Iron, 0.0f, false); // a waist rail
			}
			Block(Centre + FVector(0, 0, WallHeight * 0.75f), FVector(12, 12, WallHeight * 0.5f), FLinearColor(0.3f, 0.25f, 0.18f), 0.0f, false);
			continue;
		}

		// A cart that never went back up, the stores it was hauling, spoil heaped against the rock, and drips overhead.
		Prop(CartMeshes[R.RandRange(0, 1)], RandomCell(false), R.FRandRange(0.0f, 360.0f), FVector(260, 0, 0), true);
		const int32 StoreCount = R.RandRange(1, 3);
		for (int32 i = 0; i < StoreCount; ++i)
		{
			Prop(Stores[R.RandRange(0, UE_ARRAY_COUNT(Stores) - 1)], RandomCell(true), R.FRandRange(0.0f, 360.0f), FVector(0, 0, R.FRandRange(80.0f, 130.0f)), true);
		}
		const int32 RockCount = R.RandRange(3, 6);
		for (int32 i = 0; i < RockCount; ++i)
		{
			Prop(Spoil[R.RandRange(0, UE_ARRAY_COUNT(Spoil) - 1)], RandomCell(true) + FVector(0, 0, -20), R.FRandRange(0.0f, 360.0f),
				FVector(0, 0, R.FRandRange(70.0f, 170.0f)), true);
		}
		const int32 DripCount = R.RandRange(2, 4);
		for (int32 i = 0; i < DripCount; ++i)
		{
			const float H = R.FRandRange(110.0f, 220.0f);
			Prop(Drips[R.RandRange(0, UE_ARRAY_COUNT(Drips) - 1)], RandomCell(false) + FVector(0, 0, WallHeight - H), R.FRandRange(0.0f, 360.0f), FVector(0, 0, H), false);
		}
		// Props in the corners holding the roof up.
		for (int32 c = 0; c < 4; ++c)
		{
			const FVector Corner = Centre + FVector((c % 2 ? 1 : -1) * (Room.W * TileSize * 0.5f - 60.0f), (c / 2 ? 1 : -1) * (Room.H * TileSize * 0.5f - 60.0f), WallHeight * 0.5f);
			Block(Corner, FVector(40, 40, WallHeight), Timber, 0.0f, false);
		}
	}
}

void ADCCArcadeHaunt::SpawnMonsters()
{
	if (bMonstersSpawned || !Grid || !HasAuthority())
	{
		return;
	}
	bMonstersSpawned = true;

	const TArray<TSubclassOf<ADCCEnemyCharacter>> Pool = {
		ADCCEnemyCharacter_Beholder::StaticClass(),
		ADCCEnemyCharacter_BreakRoomMold::StaticClass(),
		ADCCEnemyCharacter_Thornback::StaticClass(),
		ADCCEnemyCharacter_UnclaimedProperty::StaticClass(),
	};
	const TArray<UDCCDungeonGrid::FRoom>& Rooms = Grid->GetRooms();
	for (int32 r = 1; r < Rooms.Num(); ++r)
	{
		const FTransform At(FRotator::ZeroRotator, Grid->CellCenter(Rooms[r].Center()) + FVector(0, 0, 140));
		if (ADCCEnemyCharacter* Monster = GetWorld()->SpawnActorDeferred<ADCCEnemyCharacter>(Pool[r % Pool.Num()], At, nullptr, nullptr,
			ESpawnActorCollisionHandlingMethod::AdjustIfPossibleButAlwaysSpawn))
		{
			Monster->SetChaseGrid(Grid);
			Monster->LeashRadius = FMath::Max(Monster->LeashRadius, 6000.0f);
			Monster->FinishSpawning(At);
			Monsters.Add(Monster);
		}
	}
}

void ADCCArcadeHaunt::DebugGoTo(int32 RoomIndex, bool bDrift)
{
	if (!Grid || !HasAuthority())
	{
		return;
	}
	FVector Spot = FVector::ZeroVector;
	float Yaw = 0.0f;
	const TArray<UDCCDungeonGrid::FRoom>& Rooms = Grid->GetRooms();
	if (RoomIndex == -11)
	{
		// End the round now (tests extraction and the Scroller): everyone gets a little footage if they have none.
		TArray<APlayerController*> Crew;
		GetPlayers(Crew);
		for (APlayerController* PC : Crew)
		{
			if (PC && PC->PlayerState)
			{
				FDCCFootageEntry& Entry = EntryFor(PC->PlayerState->GetPlayerName());
				Entry.Footage = FMath::Max(Entry.Footage, 40.0f);
			}
		}
		EndsAt = ServerTime();
		return;
	}
	if (RoomIndex == -10)
	{
		// Everyone throws a lightstick where they are looking.
		TArray<APlayerController*> Throwers;
		GetPlayers(Throwers);
		for (APlayerController* PC : Throwers)
		{
			if (PC && PC->PlayerState && PC->PlayerCameraManager)
			{
				ThrowLightstick(PC->PlayerState->GetPlayerName(), PC->PlayerCameraManager->GetCameraLocation(), PC->GetControlRotation().Vector());
			}
		}
		return;
	}
	if (RoomIndex <= -4)
	{
		// Trap shortcuts: -4 a plank, -5 the cart run, -6 a powder crate, -7 a firedamp pocket, -8 the ledge, -9 bad air.
		const FVector Off(-260.0f, 0.0f, 0.0f);
		if (RoomIndex == -4 && Planks.Num() > 0) { Spot = Planks[0].At; } // on them: they give way after 2 s
		else if (RoomIndex == -5 && Carts.Num() > 0) { Spot = FMath::Lerp(Carts[0].A, Carts[0].B, 0.5f); }
		else if (RoomIndex == -6 && Crates.Num() > 0) { Spot = Crates[0].At + FVector(0, 300, 0); Yaw = -90.0f; }
		else if (RoomIndex == -7 && Pockets.Num() > 0) { Spot = Pockets[0].At; }
		else if (RoomIndex == -8 && !LedgeAt.IsZero()) { Spot = LedgeAt + FVector(250, 0, 0); Yaw = 180.0f; }
		else if (RoomIndex == -9 && BadAirRoom >= 0) { Spot = Grid->CellCenter(Grid->GetRooms()[BadAirRoom].Center()); }
	}
	else if (RoomIndex == -3)
	{
		// Read every note (tests story completion and the payoff ghosts).
		for (int32 b = 0; b < Bodies.Num(); ++b)
		{
			NotesRead.AddUnique(b);
			const int32 Story = DCCMineNotes::Notes()[Bodies[b].Note].Story;
			if (Story >= 0 && !StoriesDone.Contains(Story) && NotesReadInStory(Story) >= NotesInStory(Story))
			{
				StoriesDone.Add(Story);
				ADCCGameState::Log(this, EDCCLogCategory::Announcement, FText::FromString(DCCMineNotes::Stories()[Story].Complete));
				UE_LOG(LogTemp, Log, TEXT("[DCC Mine] %s"), DCCMineNotes::Stories()[Story].Complete);
			}
		}
		return;
	}
	if (RoomIndex == -2 && Bodies.Num() > 0)
	{
		Spot = Bodies[0].At + FVector(-150.0f, 0.0f, 0.0f);
	}
	else if (RoomIndex == -1)
	{
		// Facing the first ghost miner, from a few metres off (outside the distance where it fades).
		for (const FAnomaly& A : Anomalies)
		{
			if (A.bGhost)
			{
				// An open cell a few metres away, looking at it.
				const FVector Feet = A.Location - FVector(0, 0, 110.0f);
				for (const FVector Dir : { FVector(-1, 0, 0), FVector(1, 0, 0), FVector(0, -1, 0), FVector(0, 1, 0) })
				{
					const FVector Try = Feet + Dir * 600.0f;
					const FIntPoint Cell = Grid->CellOf(Try);
					if (Grid->IsOpen(Cell.X, Cell.Y))
					{
						Spot = Try;
						Yaw = (-Dir).Rotation().Yaw;
						break;
					}
				}
				break;
			}
		}
	}
	else if (bDrift)
	{
		// The Nth straight drift cell along X.
		int32 Seen = 0;
		for (int32 Y = 0; Y < Grid->GetSize() && Spot.IsZero(); ++Y)
		{
			for (int32 X = 0; X < Grid->GetSize(); ++X)
			{
				FVector C;
				bool bAlongX = false;
				if (CorridorSpan(X, Y, C, bAlongX) && Seen++ == RoomIndex)
				{
					Spot = C;
					Yaw = bAlongX ? 0.0f : 90.0f;
					break;
				}
			}
		}
	}
	else if (RoomIndex >= 0 && Rooms.Num() > 0)
	{
		const UDCCDungeonGrid::FRoom& Room = Rooms[FMath::Clamp(RoomIndex, 0, Rooms.Num() - 1)];
		Spot = Grid->CellCenter(Room.Center()) - FVector(TileSize, 0.0f, 0.0f);
		Yaw = 0.0f; // looking into the room from its west side
	}
	if (Spot.IsZero())
	{
		return;
	}
	TArray<APlayerController*> Players;
	GetPlayers(Players);
	for (APlayerController* PC : Players)
	{
		if (APawn* Pawn = PC->GetPawn())
		{
			Pawn->TeleportTo(Spot + FVector(0, 0, 120), FRotator(0.0f, Yaw, 0.0f));
			PC->SetControlRotation(FRotator(-8.0f, Yaw, 0.0f));
		}
	}
}

// ---- bodies and notes (2026-11-15, Docs/dead-air-mine-plan.md section 2) --------------------------------------------

int32 ADCCArcadeHaunt::NotesInStory(int32 Story) const
{
	int32 N = 0;
	for (const FBody& Body : Bodies)
	{
		N += DCCMineNotes::Notes()[Body.Note].Story == Story ? 1 : 0;
	}
	return N;
}

int32 ADCCArcadeHaunt::NotesReadInStory(int32 Story) const
{
	int32 N = 0;
	for (int32 Index : NotesRead)
	{
		N += Bodies.IsValidIndex(Index) && DCCMineNotes::Notes()[Bodies[Index].Note].Story == Story ? 1 : 0;
	}
	return N;
}

void ADCCArcadeHaunt::PlaceBodies()
{
	// Three of the five stories each round (all their notes), and three loose notes. From the seed, so every
	// machine puts the same body in the same place.
	FRandomStream R(Seed * 11 + 5);
	const TArray<DCCMineNotes::FNote>& Notes = DCCMineNotes::Notes();
	TArray<int32> Pick = { 0, 1, 2, 3, 4 };
	for (int32 i = Pick.Num() - 1; i > 0; --i) { Pick.Swap(i, R.RandRange(0, i)); }
	StoriesHere = { Pick[0], Pick[1], Pick[2] };
	TArray<int32> Chosen;
	TArray<int32> Loose;
	for (int32 n = 0; n < Notes.Num(); ++n)
	{
		if (Notes[n].Story < 0) { Loose.Add(n); }
		else if (StoriesHere.Contains(Notes[n].Story)) { Chosen.Add(n); }
	}
	for (int32 i = Loose.Num() - 1; i > 0; --i) { Loose.Swap(i, R.RandRange(0, i)); }
	for (int32 i = 0; i < 4 && i < Loose.Num(); ++i) { Chosen.Add(Loose[i]); }

	// Places: any open cell that is not the cage, not a stope centre, and not next to another body.
	TArray<FIntPoint> Cells;
	const int32 Size = Grid->GetSize();
	for (int32 Y = 0; Y < Size; ++Y)
	{
		for (int32 X = 0; X < Size; ++X)
		{
			const int32 Room = Grid->RoomIndexAt(X, Y);
			if (Grid->IsOpen(X, Y) && Room != 0)
			{
				Cells.Add(FIntPoint(X, Y));
			}
		}
	}
	for (int32 n : Chosen)
	{
		for (int32 Try = 0; Try < 40 && Cells.Num() > 0; ++Try)
		{
			const int32 Index = R.RandRange(0, Cells.Num() - 1);
			const FIntPoint Cell = Cells[Index];
			bool bCrowded = false;
			for (const FBody& Other : Bodies)
			{
				bCrowded |= FVector::Dist2D(Other.At, Grid->CellCenter(Cell)) < TileSize * 2.5f;
			}
			if (bCrowded && Try < 39)
			{
				continue;
			}
			Cells.RemoveAt(Index);
			FBody Body;
			Body.Note = n;
			// Against the side of the cell, slumped: a little off-centre, so it is not on the rails.
			Body.At = Grid->CellCenter(Cell) + FVector(R.FRandRange(-0.3f, 0.3f) * TileSize, R.FRandRange(-0.3f, 0.3f) * TileSize, 0.0f);
			Body.Visual = SpawnBody(Body.At, R.FRandRange(0.0f, 360.0f), R.RandRange(0, 11));
			Bodies.Add(Body);
			break;
		}
	}
}

AActor* ADCCArcadeHaunt::SpawnBody(const FVector& Feet, float Yaw, int32 BodyIndex)
{
	static const TCHAR* Forge[] = {
		TEXT("worker_301"), TEXT("worker_304"), TEXT("worker_307"), TEXT("worker_310"), TEXT("casual_300"), TEXT("casual_303"),
		TEXT("casual_306"), TEXT("casual_309"), TEXT("worker_301"), TEXT("worker_304"), TEXT("worker_307"), TEXT("worker_310"),
	};
	const TCHAR* Id = Forge[FMath::Clamp(BodyIndex, 0, 11)];
	USkeletalMesh* Mesh = LoadObject<USkeletalMesh>(nullptr, *FString::Printf(TEXT("/Game/DCC/Characters/NPCs/NPC_%s/SK_NPC_%s.SK_NPC_%s"), Id, Id, Id));
	UAnimSequence* Still = LoadObject<UAnimSequence>(nullptr, TEXT("/Game/DCC/Characters/NPCs/Anims/NPC_MM_Idle.NPC_MM_Idle"));
	AActor* Body = GetWorld()->SpawnActor<AActor>(AActor::StaticClass(), FTransform(FRotator(0.0f, Yaw, 0.0f), Feet));
	if (!Body)
	{
		return nullptr;
	}
	Body->SetReplicates(false);
	USceneComponent* Root = NewObject<USceneComponent>(Body, TEXT("Root"));
	Body->SetRootComponent(Root);
	Root->SetWorldLocation(Feet);
	Root->RegisterComponent();
	if (Mesh)
	{
		USkeletalMeshComponent* Skin = NewObject<USkeletalMeshComponent>(Body, TEXT("Remains"));
		Skin->SetupAttachment(Root);
		Skin->SetSkeletalMeshAsset(Mesh);
		Skin->SetCollisionEnabled(ECollisionEnabled::NoCollision);
		Skin->SetAnimationMode(EAnimationMode::AnimationSingleNode);
		// Laid flat on his back: the Forge bodies face +Y, so a quarter turn about their own X axis puts them down.
		// (The death clip ends kneeling, not lying, so it is not used.) Lifted a little so the back is on the floor.
		const FQuat Facing(FRotator(0.0f, Yaw - 90.0f, 0.0f));
		const FQuat Down(FVector::XAxisVector, FMath::DegreesToRadians(-90.0f));
		Skin->SetWorldLocationAndRotation(Feet + FVector(0, 0, 18.0f), Facing * Down);
		Skin->RegisterComponent();
		if (Still)
		{
			Skin->PlayAnimation(Still, false);
			Skin->SetPlayRate(0.0f); // frozen
		}
		// Grey with dust and age: the shade overlay pulled to stone.
		if (UMaterialInterface* Shade = LoadObject<UMaterialInterface>(nullptr, TEXT("/Game/DCC/Materials/M_CharacterShade.M_CharacterShade")))
		{
			UMaterialInstanceDynamic* Dust = UMaterialInstanceDynamic::Create(Shade, Body);
			Dust->SetVectorParameterValue(TEXT("Tint"), FLinearColor(0.42f, 0.4f, 0.37f, 1.0f));
			Dust->SetVectorParameterValue(TEXT("Edge"), FLinearColor(0.6f, 0.58f, 0.55f, 1.0f));
			Dust->SetScalarParameterValue(TEXT("RimStrength"), 0.15f);
			Skin->SetOverlayMaterial(Dust);
		}
	}
	// The note: a scrap of paper by the hand, and a faint light so a helmet lamp is not the only way to find it.
	if (AActor* Paper = Block(Feet + FVector(0, 0, 3), FVector(34, 24, 2), FLinearColor(0.85f, 0.8f, 0.65f), Yaw + 20.0f, false))
	{
		Paper->AttachToActor(Body, FAttachmentTransformRules::KeepWorldTransform);
	}
	UPointLightComponent* Glow = NewObject<UPointLightComponent>(Body, TEXT("NoteGlow"));
	Glow->SetupAttachment(Root);
	Glow->SetRelativeLocation(FVector(0, 0, 40));
	Glow->SetIntensity(350.0f);
	Glow->SetAttenuationRadius(260.0f);
	Glow->SetLightColor(FLinearColor(1.0f, 0.85f, 0.6f));
	Glow->SetCastShadows(false);
	Glow->RegisterComponent();
	LocalActors.Add(Body);
	return Body;
}

void ADCCArcadeHaunt::ReadNotes()
{
	TArray<APlayerController*> Players;
	GetPlayers(Players);
	const double Now = ServerTime();
	const TArray<DCCMineNotes::FNote>& Notes = DCCMineNotes::Notes();
	const TArray<DCCMineNotes::FStory>& Stories = DCCMineNotes::Stories();
	for (APlayerController* PC : Players)
	{
		const ADCCCharacterBase* Crawler = Cast<ADCCCharacterBase>(PC ? PC->GetPawn() : nullptr);
		const FString Name = PC && PC->PlayerState ? PC->PlayerState->GetPlayerName() : FString();
		if (!Crawler || Name.IsEmpty() || !Crawler->IsAlive())
		{
			continue;
		}
		for (int32 b = 0; b < Bodies.Num(); ++b)
		{
			if (FVector::Dist2D(Crawler->GetActorLocation(), Bodies[b].At) > 230.0f || ReadBy.FindOrAdd(Name).Contains(b))
			{
				continue;
			}
			ReadBy.FindOrAdd(Name).Add(b);
			const DCCMineNotes::FNote& Note = Notes[Bodies[b].Note];
			FDCCFootageEntry& Entry = EntryFor(Name);
			Entry.NoteTitle = Note.Story >= 0 ? FString::Printf(TEXT("%s  (%d / %d)"), Stories[Note.Story].Title,
				NotesReadInStory(Note.Story) + (NotesRead.Contains(b) ? 0 : 1), NotesInStory(Note.Story)) : FString(TEXT("A note"));
			Entry.NoteText = Note.Text;
			Entry.NoteUntil = Now + 10.0;
			// The audience loves a reading. Only on camera.
			if (Entry.bRecording)
			{
				Entry.Footage += 25.0f;
			}
			if (!NotesRead.Contains(b))
			{
				NotesRead.Add(b);
				if (Note.Story >= 0 && !StoriesDone.Contains(Note.Story) && NotesReadInStory(Note.Story) >= NotesInStory(Note.Story))
				{
					StoriesDone.Add(Note.Story);
					ADCCGameState::Log(this, EDCCLogCategory::Announcement, FText::FromString(Stories[Note.Story].Complete));
					UE_LOG(LogTemp, Log, TEXT("[DCC Mine] %s"), Stories[Note.Story].Complete);
				}
			}
			break;
		}
	}
}

void ADCCArcadeHaunt::ShowPayoff(int32 Story)
{
	// The payoff shot: the story's ghost, standing still in plain view (it does not flicker or flee), worth a lot.
	const TArray<UDCCDungeonGrid::FRoom>& Rooms = Grid->GetRooms();
	if (Rooms.Num() < 2)
	{
		return;
	}
	const DCCMineNotes::FStory& Info = DCCMineNotes::Stories()[Story];
	const int32 RoomIndex = Info.PayoffAt == 0 ? Rooms.Num() - 1 : Info.PayoffAt == 1 ? 0 : FMath::Max(1, Rooms.Num() / 2);
	const FVector Centre = Grid->CellCenter(Rooms[RoomIndex].Center()) + FVector(TileSize * 0.6f, 0, 0);
	FAnomaly Payoff;
	Payoff.Value = 30.0f;
	Payoff.Location = Centre + FVector(0, 0, 110.0f);
	if (Info.bBird)
	{
		// The canary: a small warm light at head height, bobbing.
		Payoff.Visual = Block(Payoff.Location + FVector(0, 0, 60), FVector(14, 14, 14), FLinearColor(1.0f, 0.85f, 0.2f), 0.0f, false);
		Light(Payoff.Location + FVector(0, 0, 60), FLinearColor(1.0f, 0.8f, 0.3f), 1500.0f, 400.0f);
	}
	else
	{
		Payoff.Visual = SpawnGhostMiner(Centre, 180.0f, Story * 2 + 1);
	}
	Anomalies.Add(Payoff);
}

// ---- rockfalls, warned by three knocks ------------------------------------------------------------------------------

void ADCCArcadeHaunt::TickRocksServer()
{
	const double Now = ServerTime();
	TArray<APlayerController*> Players;
	GetPlayers(Players);
	// Land the ones whose time has come: anyone under one takes a hit and goes down on one knee.
	for (const FDCCMineRock& Rock : Rocks)
	{
		if (Now < Rock.FallsAt + 0.6 || RocksLanded.Contains(Rock.Id))
		{
			continue;
		}
		RocksLanded.Add(Rock.Id);
		for (int32 c = 0; c < Crates.Num(); ++c)
		{
			if (!Crates[c].bGone && FVector::Dist2D(Crates[c].At, Rock.At) < 300.0f)
			{
				Blast(c);
			}
		}
		for (APlayerController* PC : Players)
		{
			ADCCCharacterBase* Crawler = Cast<ADCCCharacterBase>(PC ? PC->GetPawn() : nullptr);
			if (Crawler && Crawler->Stats && Crawler->IsAlive() && FVector::Dist2D(Crawler->GetActorLocation(), Rock.At) < 240.0f)
			{
				HitCrew(Crawler, 0.25f, 8, true, false);
			}
		}
	}
	Rocks.RemoveAll([&](const FDCCMineRock& Rock) { return Now > Rock.FallsAt + 30.0; });

	// A new one: every 20-35 s, every 4-7 s in the last 45 s (the cave-in), near someone, never in the cage.
	if (Now < NextRockAt || Players.IsEmpty() || !Grid)
	{
		return;
	}
	const bool bCaveIn = TimeLeft < 45.0f;
	NextRockAt = Now + (bCaveIn ? FMath::FRandRange(4.0, 7.0) : FMath::FRandRange(20.0, 35.0));
	APlayerController* Target = Players[FMath::RandRange(0, Players.Num() - 1)];
	const APawn* Pawn = Target ? Target->GetPawn() : nullptr;
	if (!Pawn)
	{
		return;
	}
	for (int32 Try = 0; Try < 12; ++Try)
	{
		const FVector Guess = Pawn->GetActorLocation() + FRotator(0.0f, FMath::FRandRange(0.0f, 360.0f), 0.0f).Vector() * FMath::FRandRange(300.0f, 1200.0f);
		const FIntPoint Cell = Grid->CellOf(Guess);
		if (!Grid->IsOpen(Cell.X, Cell.Y) || Grid->RoomIndexAt(Cell.X, Cell.Y) == 0 || InVan(Guess))
		{
			continue;
		}
		FDCCMineRock Rock;
		Rock.Id = NextRockId++;
		Rock.At = FVector(Guess.X, Guess.Y, GetActorLocation().Z);
		Rock.FallsAt = Now + (bCrustLeft ? 5.5 : 3.5); // the knocks, then a beat, then the roof (longer if the knockers were fed)
		Rocks.Add(Rock);
		break;
	}
}

void ADCCArcadeHaunt::TickRocksLocal()
{
	const double Now = ServerTime();
	for (const FDCCMineRock& Rock : Rocks)
	{
		if (!RocksKnocked.Contains(Rock.Id))
		{
			RocksKnocked.Add(Rock.Id);
			PlayKnock(GetWorld(), Rock.At + FVector(0, 0, WallHeight - 60.0f));
			// Dust trickling from the roof where it will come down.
			Burst(TEXT("/Game/Legendary_Forge/Fx/Smoke/NS_smoke"), Rock.At + FVector(0, 0, WallHeight - 60.0f), 4.0, 0.6f);
		}
		TWeakObjectPtr<AActor>& Visual = RockVisuals.FindOrAdd(Rock.Id);
		if (!Visual.IsValid() && Now >= Rock.FallsAt && Now < Rock.FallsAt + 1.0)
		{
			static const TCHAR* Rubble[] = { TEXT("/Game/Cave_Camp/Meshes/Rocks/SM_rock_02"), TEXT("/Game/Cave_Camp/Meshes/Rocks/SM_rock_04") };
			Visual = Prop(Rubble[Rock.Id % 2], Rock.At + FVector(0, 0, WallHeight - 140.0f), float(Rock.Id * 47 % 360), FVector(170, 170, 120), false);
			if (AActor* Placed = Visual.Get())
			{
				Placed->SetLifeSpan(25.0f); // the rubble settles, then the mine forgets it
			}
		}
		if (AActor* Falling = Visual.Get())
		{
			const float T = FMath::Clamp(float(Now - Rock.FallsAt) / 0.6f, 0.0f, 1.0f);
			const FVector Pos = Falling->GetActorLocation();
			Falling->SetActorLocation(FVector(Pos.X, Pos.Y, Rock.At.Z + (WallHeight - 140.0f) * (1.0f - T * T)));
		}
	}
}

// ---- traps (2026-11-15, Docs/dead-air-mine-plan.md section 1) --------------------------------------------------------
// Every trap warns first and can always be got past. Layouts come from the seed on every machine; what happens
// (a plank breaking, a cart running, a blast) is a replicated FDCCMineEvent, so everyone sees the same moment.

namespace
{
	/** A low boom: a noise burst with a falling thump under it. */
	const TArray<int16>& BoomPcm()
	{
		static TArray<int16> Pcm;
		if (Pcm.IsEmpty())
		{
			FRandomStream R(77);
			const int32 N = int32(KnockRate * 1.6f);
			Pcm.SetNumUninitialized(N);
			float Low = 0.0f;
			for (int32 i = 0; i < N; ++i)
			{
				const float T = float(i) / KnockRate;
				Low += (R.FRandRange(-1.0f, 1.0f) - Low) * 0.08f;
				const float Thump = FMath::Sin(2.0f * PI * (55.0f + 80.0f * FMath::Exp(-T * 8.0f)) * T) * FMath::Exp(-T * 3.0f);
				const float V = (Low * 3.0f * FMath::Exp(-T * 2.2f) + Thump * 0.8f);
				Pcm[i] = int16(FMath::Clamp(V, -1.0f, 1.0f) * 28000.0f);
			}
		}
		return Pcm;
	}

	/** A loaded cart on bad rails: a low rumble and a steel squeal, getting louder. */
	const TArray<int16>& CartPcm()
	{
		static TArray<int16> Pcm;
		if (Pcm.IsEmpty())
		{
			FRandomStream R(19);
			const int32 N = int32(KnockRate * 3.6f);
			Pcm.SetNumUninitialized(N);
			float Low = 0.0f;
			float Phase = 0.0f;
			for (int32 i = 0; i < N; ++i)
			{
				const float T = float(i) / KnockRate;
				const float Swell = FMath::Min(1.0f, T / 2.4f) * FMath::Min(1.0f, (3.6f - T) / 0.4f);
				Low += (R.FRandRange(-1.0f, 1.0f) - Low) * 0.05f;
				Phase += 2.0f * PI * (2400.0f + 300.0f * FMath::Sin(T * 7.0f)) / KnockRate;
				const float Squeal = FMath::Sin(Phase) * 0.18f * (FMath::Sin(T * 13.0f) > 0.3f ? 1.0f : 0.2f);
				const float Clack = (FMath::Fmod(T, 0.35f) < 0.02f) ? R.FRandRange(-0.6f, 0.6f) : 0.0f;
				Pcm[i] = int16(FMath::Clamp((Low * 2.5f + Squeal + Clack) * Swell, -1.0f, 1.0f) * 24000.0f);
			}
		}
		return Pcm;
	}

	void PlayPcm(UWorld* World, const TArray<int16>& Pcm, const FVector& At, float Volume, float Falloff)
	{
		USoundAttenuation* Att = NewObject<USoundAttenuation>(World);
		Att->Attenuation.bAttenuate = true;
		Att->Attenuation.bSpatialize = true;
		Att->Attenuation.AttenuationShape = EAttenuationShape::Sphere;
		Att->Attenuation.AttenuationShapeExtents = FVector(500.0f, 0, 0);
		Att->Attenuation.FalloffDistance = Falloff;
		USoundWaveProcedural* Wave = NewObject<USoundWaveProcedural>(World);
		Wave->SetSampleRate(KnockRate);
		Wave->NumChannels = 1;
		Wave->Duration = float(Pcm.Num()) / KnockRate;
		Wave->SoundGroup = SOUNDGROUP_Effects;
		Wave->bLooping = false;
		Wave->QueueAudio(reinterpret_cast<const uint8*>(Pcm.GetData()), Pcm.Num() * sizeof(int16));
		UGameplayStatics::PlaySoundAtLocation(World, Wave, At, Volume, 1.0f, 0.0f, Att);
	}

	FString NameOf(const APawn* Pawn)
	{
		const APlayerState* State = Pawn ? Pawn->GetPlayerState() : nullptr;
		return State ? State->GetPlayerName() : FString();
	}
}

void ADCCArcadeHaunt::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	ADCCCharacterBase::OnAnyDamageLanded.RemoveAll(this);
	Super::EndPlay(EndPlayReason);
}

void ADCCArcadeHaunt::AddEvent(uint8 Kind, int32 Index, const FVector& At, const FString& Who, bool bReverse)
{
	FDCCMineEvent Event;
	Event.Id = NextEventId++;
	Event.Kind = Kind;
	Event.Index = Index;
	Event.At = At;
	Event.Time = ServerTime();
	Event.Who = Who;
	Event.bReverse = bReverse;
	Events.Add(Event);
	UE_LOG(LogTemp, Log, TEXT("[DCC Mine] event %d (kind %d, trap %d) at %s"), Event.Id, Kind, Index, *At.ToString());
	Events.RemoveAll([Now = Event.Time](const FDCCMineEvent& E) { return Now - E.Time > 30.0; });
	ForceNetUpdate();
}

bool ADCCArcadeHaunt::EventActive(uint8 Kind, int32 Index, double Within, const FDCCMineEvent** Out) const
{
	const double Now = ServerTime();
	for (const FDCCMineEvent& E : Events)
	{
		if (E.Kind == Kind && E.Index == Index && Now - E.Time <= Within && Now >= E.Time)
		{
			if (Out) { *Out = &E; }
			return true;
		}
	}
	return false;
}

bool ADCCArcadeHaunt::IsBadAir(const FVector& Location) const
{
	if (!Grid || BadAirRoom < 0)
	{
		return false;
	}
	const FIntPoint Cell = Grid->CellOf(Location);
	return Grid->RoomIndexAt(Cell.X, Cell.Y) == BadAirRoom;
}

bool ADCCArcadeHaunt::IsInFiredamp(const FVector& Location) const
{
	for (const FPocket& Pocket : Pockets)
	{
		if (FVector::Dist2D(Location, Pocket.At) < Pocket.Radius)
		{
			return true;
		}
	}
	return false;
}

bool ADCCArcadeHaunt::IsLampOff(const FString& Player) const
{
	if (LampsOff.Contains(Player))
	{
		return true;
	}
	for (const FDCCMineEvent& E : Events)
	{
		if (E.Kind == 5 && E.Who == Player && ServerTime() - E.Time < 8.0)
		{
			return true; // a ghost blew it out
		}
	}
	return false;
}

void ADCCArcadeHaunt::SetLamp(const FString& Player, bool bOn)
{
	if (bOn) { LampsOff.Remove(Player); }
	else { LampsOff.AddUnique(Player); }
	ForceNetUpdate();
}

void ADCCArcadeHaunt::PlaceTraps()
{
	const FVector O = GetActorLocation();
	const int32 Size = Grid->GetSize();
	const TArray<UDCCDungeonGrid::FRoom>& Rooms = Grid->GetRooms();
	FRandomStream R(Seed * 17 + 23);
	const FLinearColor Timber(0.15f, 0.09f, 0.045f);

	// The middles of straight tunnel stretches, for planks, carts and gas.
	TArray<FVector> Straight;
	TMap<FIntPoint, bool> SpanAxis;
	for (int32 Y = 0; Y < Size; ++Y)
	{
		for (int32 X = 0; X < Size; ++X)
		{
			FVector C;
			bool bX = false;
			if (CorridorSpan(X, Y, C, bX))
			{
				SpanAxis.Add(FIntPoint(X, Y), bX);
				if (!InVan(C))
				{
					Straight.Add(C);
				}
			}
		}
	}
	for (int32 i = Straight.Num() - 1; i > 0; --i) { Straight.Swap(i, R.RandRange(0, i)); }
	// Some layouts have few straight stretches: then any tunnel cell will do (away from the cage).
	TArray<FVector> AnyDrift;
	for (int32 Y = 0; Y < Size; ++Y)
	{
		for (int32 X = 0; X < Size; ++X)
		{
			if (Grid->IsOpen(X, Y) && Grid->RoomIndexAt(X, Y) == -1 && !InVan(Grid->CellCenter(FIntPoint(X, Y))))
			{
				AnyDrift.Add(Grid->CellCenter(FIntPoint(X, Y)));
			}
		}
	}
	for (int32 i = AnyDrift.Num() - 1; i > 0; --i) { AnyDrift.Swap(i, R.RandRange(0, i)); }
	int32 Used = 0;
	int32 UsedAny = 0;
	auto TakeCell = [&](FVector& Out)
	{
		if (Used < Straight.Num()) { Out = Straight[Used++]; return true; }
		while (UsedAny < AnyDrift.Num())
		{
			const FVector Try = AnyDrift[UsedAny++];
			bool bClear = true;
			for (const FVector& Taken : Straight) { bClear &= FVector::Dist2D(Taken, Try) > TileSize * 3.0f; }
			if (bClear) { Out = Try; return true; }
		}
		return false;
	};

	// 1. Rotten planks over a winze: a black drop under a few grey boards. (Two of them.)
	for (int32 i = 0; i < 2; ++i)
	{
		FPlank Plank;
		if (!TakeCell(Plank.At)) { break; }
		Block(Plank.At + FVector(0, 0, 3), FVector(TileSize * 1.5f, TileSize * 1.5f, 2), FLinearColor(0.003f, 0.003f, 0.003f), 0.0f, false);
		for (int32 b = 0; b < 6; ++b)
		{
			const float Off = (b - 2.5f) * (TileSize * 0.13f);
			static const TCHAR* Boards[] = { TEXT("/Game/Cave_Camp/Meshes/Props_02/SM_plank_01"), TEXT("/Game/Cave_Camp/Meshes/Props_02/SM_plank_02"),
				TEXT("/Game/Cave_Camp/Meshes/Props_02/SM_plank_03"), TEXT("/Game/Cave_Camp/Meshes/Props_02/SM_plank_04") };
			if (AActor* Board = Prop(Boards[b % 4], Plank.At + FVector(Off * 2.0f, 0, 4), 90.0f + R.FRandRange(-5.0f, 5.0f), FVector(TileSize * 1.7f, TileSize * 0.22f, 0), false, true))
			{
				Plank.Boards.Add(Board);
			}
		}
		Planks.Add(Plank);
	}

	// The sump level: one short tunnel under the mine, and a ladder back up.
	{
		SumpStart = O + FVector(-6000.0f, -6000.0f, -1600.0f);
		const FVector Mid = SumpStart + FVector(TileSize * 1.5f, 0, 0);
		Block(Mid + FVector(0, 0, -20), FVector(TileSize * 4.0f, TileSize, 40), FLinearColor(0.04f, 0.035f, 0.03f));
		Block(Mid + FVector(0, 0, WallHeight + 20), FVector(TileSize * 4.0f, TileSize, 40), FLinearColor(0.03f, 0.025f, 0.02f));
		for (int32 Side = -1; Side <= 1; Side += 2)
		{
			Block(Mid + FVector(0, Side * TileSize * 0.5f, WallHeight * 0.5f), FVector(TileSize * 4.0f, 30, WallHeight), FLinearColor(0.06f, 0.05f, 0.04f));
			Block(Mid + FVector(Side * TileSize * 2.0f, 0, WallHeight * 0.5f), FVector(30, TileSize, WallHeight), FLinearColor(0.06f, 0.05f, 0.04f));
		}
		Block(Mid + FVector(0, 0, 4), FVector(TileSize * 3.8f, TileSize * 0.9f, 6), FLinearColor(0.02f, 0.05f, 0.07f), 0.0f, false); // standing water
		SumpLadder = SumpStart + FVector(TileSize * 3.3f, 0, 0);
		for (int32 Rung = 0; Rung < 8; ++Rung)
		{
			Block(SumpLadder + FVector(60, 0, 40 + Rung * 55), FVector(10, 120, 10), Timber, 0.0f, false);
		}
		Block(SumpLadder + FVector(60, -60, WallHeight * 0.5f), FVector(10, 10, WallHeight), Timber, 0.0f, false);
		Block(SumpLadder + FVector(60, 60, WallHeight * 0.5f), FVector(10, 10, WallHeight), Timber, 0.0f, false);
		Sign(SumpLadder + FVector(0, 0, 260), 180.0f, TEXT("THE SUMP  -  LADDER UP"), FColor(200, 220, 255), 24.0f);
		Light(Mid + FVector(0, 0, WallHeight - 80), FLinearColor(0.35f, 0.5f, 0.7f), 1200.0f, 1400.0f);
	}

	// 2. A runaway cart: the longest straight run of rails, a cart parked at one end.
	{
		FIntPoint BestA(0, 0), BestB(0, 0);
		int32 BestLen = 0;
		for (const TPair<FIntPoint, bool>& Span : SpanAxis)
		{
			const FIntPoint Dir = Span.Value ? FIntPoint(1, 0) : FIntPoint(0, 1);
			int32 Len = 0;
			FIntPoint Cell = Span.Key;
			while (const bool* Next = SpanAxis.Find(Cell + Dir))
			{
				if (*Next != Span.Value || Len >= 9) { break; }
				Cell += Dir;
				++Len;
			}
			if (Len > BestLen)
			{
				BestLen = Len;
				BestA = Span.Key;
				BestB = Cell;
			}
		}
		// Two steps is enough (2026-11-15): three-step straights were rare enough that most mines had no cart at all.
		// A span cell's neighbours along the drift are always tunnel, so each end reaches one tile further.
		if (BestLen >= 2)
		{
			FCartRun Run;
			FVector CA, CB;
			bool bAxis = false;
			CorridorSpan(BestA.X, BestA.Y, CA, bAxis);
			CorridorSpan(BestB.X, BestB.Y, CB, bAxis);
			const FVector Along = (CB - CA).GetSafeNormal();
			Run.A = CA - Along * TileSize;
			Run.B = CB + Along * TileSize;
			const float Yaw = (Run.B - Run.A).Rotation().Yaw;
			// Solid (2026-11-15): parked, you walk around it; running, it teleports along the rails and knocks you aside.
			Run.Cart = Prop(TEXT("/Game/Desert_cave/Meshes/Cart/SM_cart_02"), Run.A, Yaw, FVector(240, 0, 0), true);
			Carts.Add(Run);
			// The rails gleam here: a marked run.
			Sign((Run.A + Run.B) * 0.5f + FVector(0, 0, WallHeight - 60), Yaw + 90.0f, TEXT("ORE RUN  -  STAND CLEAR OF THE RAILS"), FColor(255, 200, 120), 16.0f);
		}
	}

	// 3. Old powder crates, against stope walls.
	for (int32 i = 0; i < 3 && Rooms.Num() > 2; ++i)
	{
		const UDCCDungeonGrid::FRoom& Room = Rooms[R.RandRange(1, Rooms.Num() - 2)];
		FCrate Crate;
		Crate.At = Grid->CellCenter(FIntPoint(Room.X + R.RandRange(0, Room.W - 1), R.FRand() < 0.5f ? Room.Y : Room.Y + Room.H - 1));
		if (AActor* Box = Prop(TEXT("/Game/Cave_Camp/Meshes/Props_02/SM_crate_02"), Crate.At, R.FRandRange(0.0f, 360.0f), FVector(0, 0, 90), true))
		{
			Crate.Visuals.Add(Box);
		}
		if (AActor* Stencil = Block(Crate.At + FVector(0, 0, 100), FVector(70, 70, 4), FLinearColor(0.6f, 0.08f, 0.05f), 0.0f, false))
		{
			Crate.Visuals.Add(Stencil);
		}
		Sign(Crate.At + FVector(0, 0, 135), 180.0f, TEXT("CLEAR CREEK GIANT POWDER"), FColor(230, 60, 40), 12.0f);
		Crates.Add(Crate);
	}

	// 4. Firedamp: two pockets in the drifts, where a lit lamp burns blue.
	for (int32 i = 0; i < 2; ++i)
	{
		FPocket Pocket;
		if (!TakeCell(Pocket.At)) { break; }
		Pockets.Add(Pocket);
		// Seen, not just felt: gas motes drifting in a faint blue, a timber sign "FIREDAMP" nailed up.
		Burst(TEXT("/Game/Alien_Cave_biome/FX/NS_spores"), Pocket.At + FVector(0, 0, 120), 0.0, 1.6f);
		Light(Pocket.At + FVector(0, 0, 220), FLinearColor(0.35f, 0.55f, 1.0f), 400.0f, Pocket.Radius);
	}

	// 5. Blackdamp: one middle stope with bad air (never the cage, never the deepest).
	if (Rooms.Num() > 3)
	{
		BadAirRoom = R.RandRange(1, Rooms.Num() - 2);
		const FVector C = Grid->CellCenter(Rooms[BadAirRoom].Center());
		Sign(C + FVector(0, 0, WallHeight - 80), 180.0f, TEXT("BAD AIR  -  CANDLE WILL NOT STAY LIT"), FColor(160, 170, 150), 18.0f);
		// A low grey-green murk on the floor of the stope, and the light it makes.
		Burst(TEXT("/Game/Alien_Portal/FX/Smoke/NS_Smoke_01"), C + FVector(0, 0, 20), 0.0, 2.5f);
		Light(C + FVector(0, 0, 80), FLinearColor(0.45f, 0.55f, 0.35f), 900.0f, TileSize * 2.5f);
	}

	// The knockers' ledge: a shelf in a stope, a tin plate on it.
	if (Rooms.Num() > 2)
	{
		const UDCCDungeonGrid::FRoom& Room = Rooms[R.RandRange(1, Rooms.Num() - 1)];
		LedgeAt = Grid->CellCenter(FIntPoint(Room.X, Room.Y + Room.H / 2)) + FVector(-TileSize * 0.3f, 0, 0);
		Prop(TEXT("/Game/Cave_Camp/Meshes/Props_02/SM_plank_03"), LedgeAt + FVector(0, 0, 104), 90.0f, FVector(170, 70, 0), false, true);
		Shape(TEXT("Cylinder"), LedgeAt + FVector(0, 0, 120), FVector(36, 36, 3), FLinearColor(0.5f, 0.5f, 0.52f), FRotator::ZeroRotator, false);
		Sign(LedgeAt + FVector(0, 0, 170), 0.0f, TEXT("for the knockers"), FColor(200, 190, 160), 12.0f);
	}

	if (HasAuthority())
	{
		ADCCCharacterBase::OnAnyDamageLanded.AddUObject(this, &ADCCArcadeHaunt::OnDamageLanded);
	}
}

void ADCCArcadeHaunt::OnDamageLanded(ADCCCharacterBase* Dealer, ADCCCharacterBase* Target, int32 Amount)
{
	// A fight next to a powder crate can set it off.
	if (!Target || Amount <= 0)
	{
		return;
	}
	for (int32 i = 0; i < Crates.Num(); ++i)
	{
		if (!Crates[i].bGone && FVector::Dist2D(Target->GetActorLocation(), Crates[i].At) < 260.0f && FMath::FRand() < 0.35f)
		{
			Blast(i);
		}
	}
}

void ADCCArcadeHaunt::Blast(int32 CrateIndex)
{
	if (!Crates.IsValidIndex(CrateIndex) || Crates[CrateIndex].bGone)
	{
		return;
	}
	Crates[CrateIndex].bGone = true;
	const FVector At = Crates[CrateIndex].At;
	AddEvent(3, CrateIndex, At);
	for (TActorIterator<ADCCCharacterBase> It(GetWorld()); It; ++It)
	{
		ADCCCharacterBase* Who = *It;
		if (Who->Stats && Who->IsAlive() && FVector::Dist(Who->GetActorLocation(), At) < 450.0f)
		{
			const bool bCrew = !Who->IsA<ADCCEnemyCharacter>();
			if (bCrew) { HitCrew(Who, 0.3f, 10, true, false); }
			else { Who->Stats->ApplyDamage(80); }
			Who->LaunchCharacter((Who->GetActorLocation() - (At)).GetSafeNormal2D() * 650.0f + FVector(0, 0, 260.0f), true, true);
		}
	}
	// One crate can set off the next.
	for (int32 i = 0; i < Crates.Num(); ++i)
	{
		if (!Crates[i].bGone && FVector::Dist(Crates[i].At, At) < 500.0f)
		{
			Blast(i);
		}
	}
}

void ADCCArcadeHaunt::TickTrapsServer(float DeltaSeconds)
{
	const double Now = ServerTime();
	TArray<APlayerController*> Players;
	GetPlayers(Players);
	TArray<ADCCCharacterBase*> Crew;
	for (APlayerController* PC : Players)
	{
		if (ADCCCharacterBase* C = Cast<ADCCCharacterBase>(PC ? PC->GetPawn() : nullptr))
		{
			if (C->IsAlive())
			{
				Crew.Add(C);
			}
		}
	}

	// 1. Planks: two on it at once, or one standing still on it for two seconds, and down they go.
	for (int32 i = 0; i < Planks.Num(); ++i)
	{
		FPlank& Plank = Planks[i];
		if (Plank.bBroken)
		{
			continue;
		}
		TArray<ADCCCharacterBase*> On;
		for (ADCCCharacterBase* C : Crew)
		{
			const FString Name = NameOf(C);
			if (FMath::Abs(C->GetActorLocation().X - Plank.At.X) < TileSize * 0.75f && FMath::Abs(C->GetActorLocation().Y - Plank.At.Y) < TileSize * 0.75f)
			{
				On.Add(C);
				const bool bStill = C->GetVelocity().SizeSquared2D() < FMath::Square(60.0f);
				Plank.StoodFor.FindOrAdd(Name) += bStill ? DeltaSeconds : 0.0f;
			}
			else
			{
				Plank.StoodFor.Remove(Name);
			}
		}
		bool bGive = On.Num() >= 2;
		for (const TPair<FString, float>& Pair : Plank.StoodFor)
		{
			bGive |= Pair.Value > 2.0f;
		}
		if (!bGive)
		{
			continue;
		}
		Plank.bBroken = true;
		AddEvent(1, i, Plank.At);
		for (int32 k = 0; k < On.Num(); ++k)
		{
			ADCCCharacterBase* C = On[k];
			SumpReturn.Add(NameOf(C), Plank.At + FVector(TileSize, 0, 120));
			const FDCCFootageEntry* Kit = KitOf(C);
			if (Kit && Kit->KitHarness > 0)
			{
				if (UDCCWalletComponent* Wallet = C->FindComponentByClass<UDCCWalletComponent>())
				{
					Wallet->Tell(TEXT("The planks go, and the rope catches you. You haul yourself back up onto the timbers."));
				}
				continue;
			}
			C->TeleportTo(SumpStart + FVector(k * 120.0f, 0, 140), C->GetActorRotation());
			HitCrew(C, 0.15f, 6, false, false);
		}
	}
	// The ladder up out of the sump.
	for (ADCCCharacterBase* C : Crew)
	{
		if (FVector::Dist2D(C->GetActorLocation(), SumpLadder) < 170.0f && FMath::Abs(C->GetActorLocation().Z - SumpLadder.Z) < 400.0f)
		{
			const FVector* Back = SumpReturn.Find(NameOf(C));
			C->TeleportTo(Back ? *Back : (VanBox.IsValid ? VanBox.GetCenter() + FVector(0, 0, 120) : GetActorLocation()), C->GetActorRotation());
		}
	}

	// 2. The cart: every 40-60 s, if someone is near the run. Three and a half seconds of noise, then it comes.
	for (int32 i = 0; i < Carts.Num(); ++i)
	{
		FCartRun& Run = Carts[i];
		const FDCCMineEvent* Running = nullptr;
		if (EventActive(2, i, 6.5, &Running) && Running)
		{
			const float T = FMath::Clamp(float(Now - Running->Time - 3.5) / 2.5f, 0.0f, 1.0f);
			if (T > 0.0f && T < 1.0f)
			{
				const FVector From = Running->bReverse ? Run.B : Run.A;
				const FVector To = Running->bReverse ? Run.A : Run.B;
				const FVector Pos = FMath::Lerp(From, To, T);
				for (TActorIterator<ADCCCharacterBase> It(GetWorld()); It; ++It)
				{
					ADCCCharacterBase* Who = *It;
					const FString Key = Who->GetName();
					if (Who->Stats && Who->IsAlive() && !Run.HitThisRun.Contains(Key) && FVector::Dist2D(Who->GetActorLocation(), Pos) < 170.0f)
					{
						Run.HitThisRun.Add(Key);
						const bool bCrew = !Who->IsA<ADCCEnemyCharacter>();
						if (bCrew) { HitCrew(Who, 0.2f, 8, true, false); }
						else { Who->Stats->ApplyDamage(60); }
						Who->LaunchCharacter((Who->GetActorLocation() - (Pos - (To - From).GetSafeNormal() * 200.0f)).GetSafeNormal2D() * 650.0f + FVector(0, 0, 260.0f), true, true);
					}
				}
				for (int32 c = 0; c < Crates.Num(); ++c)
				{
					if (!Crates[c].bGone && FVector::Dist2D(Crates[c].At, Pos) < 220.0f)
					{
						Blast(c);
					}
				}
			}
			continue;
		}
		if (NextCartAt <= 0.0)
		{
			NextCartAt = Now + FMath::FRandRange(30.0, 50.0);
		}
		if (Now < NextCartAt)
		{
			continue;
		}
		bool bNear = false;
		for (ADCCCharacterBase* C : Crew)
		{
			bNear |= FMath::PointDistToSegment(C->GetActorLocation(), Run.A, Run.B) < 1600.0f;
		}
		if (bNear)
		{
			Run.HitThisRun.Reset();
			AddEvent(2, i, Run.bAtB ? Run.B : Run.A, FString(), Run.bAtB);
			Run.bAtB = !Run.bAtB;
			NextCartAt = Now + FMath::FRandRange(40.0, 60.0);
		}
	}

	// 4. Firedamp: four seconds of lit lamps in a pocket and it flashes. Then it is quiet for a while.
	for (int32 i = 0; i < Pockets.Num(); ++i)
	{
		FPocket& Pocket = Pockets[i];
		if (Now < Pocket.QuietUntil)
		{
			continue;
		}
		bool bLit = false;
		for (ADCCCharacterBase* C : Crew)
		{
			bLit |= FVector::Dist2D(C->GetActorLocation(), Pocket.At) < Pocket.Radius && !IsLampOff(NameOf(C));
		}
		Pocket.Fuse = bLit ? Pocket.Fuse + DeltaSeconds : FMath::Max(0.0f, Pocket.Fuse - DeltaSeconds);
		if (Pocket.Fuse < 4.0f)
		{
			continue;
		}
		Pocket.Fuse = 0.0f;
		Pocket.QuietUntil = Now + 25.0;
		AddEvent(4, i, Pocket.At);
		for (ADCCCharacterBase* C : Crew)
		{
			if (C->Stats && FVector::Dist2D(C->GetActorLocation(), Pocket.At) < Pocket.Radius)
			{
				HitCrew(C, 0.2f, 8, false, true);
				C->LaunchCharacter((C->GetActorLocation() - (Pocket.At)).GetSafeNormal2D() * 650.0f + FVector(0, 0, 260.0f), true, true);
			}
		}
	}

	// 5. Blackdamp: eight seconds of bad air, then it starts to cost health.
	for (ADCCCharacterBase* C : Crew)
	{
		const FString Name = NameOf(C);
		if (!IsBadAir(C->GetActorLocation()))
		{
			BadAirFor.Remove(Name);
			continue;
		}
		float& For = BadAirFor.FindOrAdd(Name);
		const float Before = For;
		For += DeltaSeconds;
		if (For > 8.0f && FMath::FloorToInt(For) != FMath::FloorToInt(Before) && C->Stats)
		{
			HitCrew(C, 0.04f, 2, false, true);
		}
	}

	// Air: below the depth your tanks reach, the tank drains (12 s of it); above it, it refills. Empty costs health.
	for (ADCCCharacterBase* C : Crew)
	{
		FDCCFootageEntry* Kit = KitOf(C);
		if (!Kit)
		{
			continue;
		}
		const bool bDeep = DepthAt(C->GetActorLocation()) > DCCChopShop::AirReach(Kit->KitAir);
		const float Before = Kit->Air;
		Kit->Air = FMath::Clamp(Kit->Air + (bDeep ? -DeltaSeconds / 12.0f : DeltaSeconds / 4.0f), 0.0f, 1.0f);
		if (Kit->Air <= 0.0f && FMath::FloorToInt(Now * 1.0) != FMath::FloorToInt((Now - DeltaSeconds) * 1.0))
		{
			HitCrew(C, 0.05f, 3, false, false);
		}
		if (Before > 0.3f && Kit->Air <= 0.3f)
		{
			if (UDCCWalletComponent* Wallet = C->FindComponentByClass<UDCCWalletComponent>())
			{
				Wallet->Tell(TEXT("Your air is running low. This deep needs better tanks (Mine Outfitters, in the cage)."));
			}
		}
	}

	// The knockers' ledge: stand at it for three seconds and you leave the crust from your lunch. Rocks warn sooner.
	if (!bCrustLeft && !LedgeAt.IsZero())
	{
		for (ADCCCharacterBase* C : Crew)
		{
			const FString Name = NameOf(C);
			if (FVector::Dist2D(C->GetActorLocation(), LedgeAt) < 170.0f)
			{
				if ((LedgeFor.FindOrAdd(Name) += DeltaSeconds) >= 3.0f)
				{
					bCrustLeft = true;
					if (UDCCWalletComponent* Wallet = C->FindComponentByClass<UDCCWalletComponent>())
					{
						Wallet->Tell(TEXT("You leave the crust from your lunch on the tin plate. Somewhere in the rock, something knocks twice, politely."));
					}
					FDCCFootageEntry& Entry = EntryFor(Name);
					if (Entry.bRecording)
					{
						Entry.Footage += 15.0f;
					}
				}
			}
			else
			{
				LedgeFor.Remove(Name);
			}
		}
	}

	// The lamp-snuffer: every 60-90 s a ghost blows out the lamp of someone standing near it.
	if (NextSnuffAt <= 0.0)
	{
		NextSnuffAt = Now + FMath::FRandRange(45.0, 70.0);
	}
	if (Now >= NextSnuffAt)
	{
		NextSnuffAt = Now + FMath::FRandRange(60.0, 90.0);
		for (int32 a = 0; a < Anomalies.Num(); ++a)
		{
			if (!Anomalies[a].bGhost)
			{
				continue;
			}
			ADCCCharacterBase* Victim = nullptr;
			for (ADCCCharacterBase* C : Crew)
			{
				if (FVector::Dist2D(C->GetActorLocation(), Anomalies[a].Location) < 1400.0f && !IsLampOff(NameOf(C)))
				{
					Victim = C;
					break;
				}
			}
			if (Victim)
			{
				AddEvent(5, a, Anomalies[a].Location, NameOf(Victim));
				if (UDCCWalletComponent* Wallet = Victim->FindComponentByClass<UDCCWalletComponent>())
				{
					Wallet->Tell(TEXT("Cold air on your neck. Your lamp goes out."));
				}
				break;
			}
		}
	}
}

void ADCCArcadeHaunt::TickTrapsLocal(float DeltaSeconds)
{
	UWorld* World = GetWorld();
	const double Now = ServerTime();
	for (const FDCCMineEvent& E : Events)
	{
		const bool bFirst = !EventsPlayed.Contains(E.Id);
		if (bFirst)
		{
			EventsPlayed.Add(E.Id);
		}
		switch (E.Kind)
		{
		case 1: // planks give way
			if (bFirst && Planks.IsValidIndex(E.Index))
			{
				for (const TWeakObjectPtr<AActor>& Board : Planks[E.Index].Boards)
				{
					if (AActor* B = Board.Get()) { B->SetActorHiddenInGame(true); }
				}
				Planks[E.Index].bBroken = true;
				PlayKnock(World, E.At);
			}
			break;
		case 2: // the cart
			if (bFirst)
			{
				PlayPcm(World, CartPcm(), E.At, 1.0f, 4000.0f);
			}
			if (Carts.IsValidIndex(E.Index))
			{
				FCartRun& Run = Carts[E.Index];
				const float T = FMath::Clamp(float(Now - E.Time - 3.5) / 2.5f, 0.0f, 1.0f);
				const FVector From = E.bReverse ? Run.B : Run.A;
				const FVector To = E.bReverse ? Run.A : Run.B;
				if (AActor* Cart = Run.Cart.Get())
				{
					if (Now - E.Time < 7.0)
					{
						Cart->SetActorLocation(FMath::Lerp(From, To, T) + FVector(0, 0, Cart->GetActorLocation().Z - From.Z));
					}
				}
			}
			break;
		case 3: // powder blast
			if (bFirst)
			{
				PlayPcm(World, BoomPcm(), E.At, 1.4f, 6000.0f);
				if (Crates.IsValidIndex(E.Index))
				{
					for (const TWeakObjectPtr<AActor>& V : Crates[E.Index].Visuals)
					{
						if (AActor* A = V.Get()) { A->SetActorHiddenInGame(true); }
					}
					Crates[E.Index].bGone = true;
				}
				if (AActor* Fire = Shape(TEXT("Sphere"), E.At + FVector(0, 0, 100), FVector(500), FLinearColor(1.0f, 0.55f, 0.15f), FRotator::ZeroRotator, false))
				{
					Fire->SetLifeSpan(0.2f);
				}
				Burst(TEXT("/Game/Legendary_Forge/Fx/Fire/NS_fire"), E.At, 2.5, 2.0f);
				Burst(TEXT("/Game/Legendary_Forge/Fx/Sparks/NS_sparks"), E.At + FVector(0, 0, 80), 1.5, 1.5f);
				Burst(TEXT("/Game/Legendary_Forge/Fx/Smoke/NS_smoke"), E.At + FVector(0, 0, 60), 5.0, 1.2f);
				Light(E.At + FVector(0, 0, 150), FLinearColor(1.0f, 0.6f, 0.2f), 60000.0f, 1800.0f);
			}
			break;
		case 4: // firedamp flash
			if (bFirst)
			{
				PlayPcm(World, BoomPcm(), E.At, 1.0f, 5000.0f);
				if (AActor* Flash = Shape(TEXT("Sphere"), E.At + FVector(0, 0, WallHeight * 0.6f), FVector(900, 900, 300), FLinearColor(0.4f, 0.6f, 1.0f), FRotator::ZeroRotator, false))
				{
					Flash->SetLifeSpan(0.2f);
				}
				Burst(TEXT("/Game/Legendary_Forge/Fx/Fire/NS_fire"), E.At + FVector(0, 0, 150), 1.2, 3.0f);
			}
			break;
		case 5: // a lamp snuffed: cold breath at the victim's head
			if (bFirst)
			{
				for (TActorIterator<ADCCCharacterBase> It(World); It; ++It)
				{
					const APlayerState* State = It->GetPlayerState();
					if (State && State->GetPlayerName() == E.Who)
					{
						Burst(TEXT("/Game/Alien_Portal/FX/Smoke/NS_Smoke_02"), It->GetActorLocation() + FVector(0, 0, 90), 1.5, 0.3f);
					}
				}
			}
			break;
		default:
			break;
		}
	}
	// Bursts that have run their time.
	for (int32 b = Bursts.Num() - 1; b >= 0; --b)
	{
		if (Bursts[b].Until > 0.0 && Now >= Bursts[b].Until)
		{
			if (UNiagaraComponent* Comp = Bursts[b].Comp.Get())
			{
				Comp->Deactivate();
			}
			Bursts.RemoveAt(b);
		}
	}
	// A crust on the knockers' plate, once someone has left one.
	if (bCrustLeft && !bCrustShown && !LedgeAt.IsZero())
	{
		bCrustShown = true;
		Shape(TEXT("Sphere"), LedgeAt + FVector(0, 0, 128), FVector(18, 26, 8), FLinearColor(0.55f, 0.38f, 0.18f), FRotator::ZeroRotator, false);
	}
	// Lightsticks: thrown in an arc, then a green glow where they land, for the rest of the round.
	for (int32 s = 0; s < Lightsticks.Num(); ++s)
	{
		const FDCCLightstick& Stick = Lightsticks[s];
		TWeakObjectPtr<AActor>& Visual = StickVisuals.FindOrAdd(s);
		if (!Visual.IsValid())
		{
			Visual = Shape(TEXT("Cylinder"), Stick.From, FVector(7, 7, 34), FLinearColor(0.4f, 1.0f, 0.4f), FRotator(0.0f, 0.0f, 80.0f), false);
			if (AActor* V = Visual.Get())
			{
				Paint(V, TEXT("/Game/SoulCave/Environment/Materials/Building/M_Soul_Glowstick"));
			}
		}
		const float T = FMath::Clamp(float(Now - Stick.Time) / 0.45f, 0.0f, 1.0f);
		if (AActor* V = Visual.Get())
		{
			const FVector Arc = FMath::Lerp(Stick.From, Stick.At, T) + FVector(0, 0, 180.0f * 4.0f * T * (1.0f - T));
			V->SetActorLocationAndRotation(T < 1.0f ? Arc : Stick.At + FVector(0, 0, 4), FRotator(T < 1.0f ? T * 720.0f : 0.0f, 0.0f, 85.0f));
		}
		if (T >= 1.0f && !StickLit.Contains(s))
		{
			StickLit.Add(s);
			Light(Stick.At + FVector(0, 0, 40), FLinearColor(0.35f, 1.0f, 0.4f), 3500.0f, 900.0f);
		}
	}
	// The Outfitters' pad in the cage opens the counter on its tab (once per step onto it).
	if (APlayerController* PadPC = World->GetFirstPlayerController())
	{
		const APawn* PadPawn = PadPC->GetPawn();
		const bool bOnPad = PadPawn && !OutfitPad.IsZero() && FVector::Dist2D(PadPawn->GetActorLocation(), OutfitPad) < TileSize * 0.4f;
		if (bOnPad && !bWasOnOutfitPad)
		{
			if (ADCCHUD* Hud = Cast<ADCCHUD>(PadPC->GetHUD()))
			{
				Hud->OpenCosmeticShop(3);
			}
		}
		bWasOnOutfitPad = bOnPad;
	}
	// The blast and flash lights fade (Light() makes a permanent one, so the latest few are dimmed here).
	for (TActorIterator<APointLight> It(World); It; ++It)
	{
		ULightComponent* L = It->GetLightComponent();
		if (L && L->Intensity > 30000.0f)
		{
			L->SetIntensity(FMath::Max(0.0f, L->Intensity - DeltaSeconds * 120000.0f));
		}
	}

	// The canary's ghost leads toward the nearest note nobody has read, when its story is down here.
	const APawn* Local = World->GetFirstPlayerController() ? World->GetFirstPlayerController()->GetPawn() : nullptr;
	if (Local && StoriesHere.Contains(3))
	{
		if (!Canary.IsValid())
		{
			CanaryPos = Local->GetActorLocation() + FVector(0, 0, 160);
			Canary = Block(CanaryPos, FVector(14, 14, 14), FLinearColor(1.0f, 0.85f, 0.2f), 0.0f, false);
			if (AActor* Bird = Canary.Get())
			{
				UPointLightComponent* Glow = NewObject<UPointLightComponent>(Bird);
				Glow->SetupAttachment(Bird->GetRootComponent());
				Glow->SetIntensity(900.0f);
				Glow->SetAttenuationRadius(320.0f);
				Glow->SetLightColor(FLinearColor(1.0f, 0.8f, 0.3f));
				Glow->SetCastShadows(false);
				Glow->RegisterComponent();
			}
		}
		FVector Goal = Local->GetActorLocation();
		float Best = TNumericLimits<float>::Max();
		for (int32 b = 0; b < Bodies.Num(); ++b)
		{
			const float D = FVector::Dist2D(Bodies[b].At, Local->GetActorLocation());
			if (!NotesRead.Contains(b) && D < Best)
			{
				Best = D;
				Goal = Bodies[b].At;
			}
		}
		// A few metres ahead of you along that line, bobbing, never far.
		const FVector ToGoal = (Goal - Local->GetActorLocation()).GetSafeNormal2D();
		const FVector Want = Local->GetActorLocation() + ToGoal * FMath::Min(420.0f, Best) + FVector(0, 0, 150 + 20 * FMath::Sin(Now * 3.0));
		CanaryPos = FMath::VInterpTo(CanaryPos, Want, DeltaSeconds, 2.0f);
		if (AActor* Bird = Canary.Get())
		{
			Bird->SetActorLocation(CanaryPos);
		}
	}

	// Bad air: the local player's seconds in it (the HUD's canary and vignette read this).
	LocalBadAirSeconds = Local && IsBadAir(Local->GetActorLocation()) ? LocalBadAirSeconds + DeltaSeconds : 0.0f;

	// The lamp key.
	if (APlayerController* PC = World->GetFirstPlayerController())
	{
		if (PC->WasInputKeyJustPressed(EKeys::G) && Local && PC->PlayerCameraManager)
		{
			if (UDCCWalletComponent* Wallet = Local->FindComponentByClass<UDCCWalletComponent>())
			{
				Wallet->Server_ThrowLightstick(PC->PlayerCameraManager->GetCameraLocation(), PC->GetControlRotation().Vector());
			}
		}
		if (PC->WasInputKeyJustPressed(EKeys::L) && Local)
		{
			if (UDCCWalletComponent* Wallet = Local->FindComponentByClass<UDCCWalletComponent>())
			{
				Wallet->Server_SetMineLamp(IsLampOff(NameOf(Local)));
			}
		}
	}
}

// ---- the Mine Outfitters' kit (2026-11-15): depth and air, lamps, lightsticks, the hat, the harness, the meter -----

void ADCCArcadeHaunt::BuildDepth()
{
	// Walking distance from the cage, through open cells, scaled so the farthest reach of the workings is 1.
	const int32 Size = Grid->GetSize();
	DepthMap.Init(-1.0f, Size * Size);
	const FIntPoint Start = Grid->GetRooms().Num() > 0 ? Grid->GetRooms()[0].Center() : FIntPoint(0, 0);
	TArray<FIntPoint> Queue = { Start };
	DepthMap[Start.Y * Size + Start.X] = 0.0f;
	float Far = 1.0f;
	for (int32 Head = 0; Head < Queue.Num(); ++Head)
	{
		const FIntPoint C = Queue[Head];
		const float D = DepthMap[C.Y * Size + C.X];
		for (const FIntPoint Step : { FIntPoint(1, 0), FIntPoint(-1, 0), FIntPoint(0, 1), FIntPoint(0, -1) })
		{
			const FIntPoint N = C + Step;
			if (N.X < 0 || N.Y < 0 || N.X >= Size || N.Y >= Size || !Grid->IsOpen(N.X, N.Y) || DepthMap[N.Y * Size + N.X] >= 0.0f)
			{
				continue;
			}
			DepthMap[N.Y * Size + N.X] = D + 1.0f;
			Far = FMath::Max(Far, D + 1.0f);
			Queue.Add(N);
		}
	}
	for (float& D : DepthMap)
	{
		D = D < 0.0f ? 0.0f : D / Far;
	}
}

float ADCCArcadeHaunt::DepthAt(const FVector& Location) const
{
	if (!Grid || DepthMap.IsEmpty())
	{
		return 0.0f;
	}
	const FIntPoint C = Grid->CellOf(Location);
	const int32 Size = Grid->GetSize();
	return C.X >= 0 && C.Y >= 0 && C.X < Size && C.Y < Size ? DepthMap[C.Y * Size + C.X] : 0.0f;
}

float ADCCArcadeHaunt::NearestGas(const FVector& Location) const
{
	float Best = TNumericLimits<float>::Max();
	for (const FPocket& Pocket : Pockets)
	{
		Best = FMath::Min(Best, FVector::Dist2D(Location, Pocket.At) - Pocket.Radius);
	}
	return Best;
}

void ADCCArcadeHaunt::LoadKit(FDCCFootageEntry& Entry, APlayerController* PC)
{
	if (Entry.bKit || !PC || !PC->PlayerState)
	{
		return;
	}
	const FString Key = DCCProfile::KeyFor(PC->PlayerState);
	if (Key.IsEmpty())
	{
		return;
	}
	Entry.bKit = true;
	Entry.KitAir = uint8(DCCChopShop::GetLevel(Key, DCCChopShop::AirTank));
	Entry.KitLamp = uint8(DCCChopShop::GetLevel(Key, DCCChopShop::MineLamp));
	Entry.KitHat = uint8(DCCChopShop::GetLevel(Key, DCCChopShop::HardHat));
	Entry.KitHarness = uint8(DCCChopShop::GetLevel(Key, DCCChopShop::Harness));
	Entry.KitGas = uint8(DCCChopShop::GetLevel(Key, DCCChopShop::GasMeter));
	Entry.Sticks = DCCChopShop::StickCount(DCCChopShop::GetLevel(Key, DCCChopShop::Lightsticks));
	Entry.Tape += DCCChopShop::TapeBonus(DCCChopShop::GetLevel(Key, DCCChopShop::TapeReel));
	Entry.Air = 1.0f;
}

FDCCFootageEntry* ADCCArcadeHaunt::KitOf(const APawn* Pawn)
{
	const APlayerState* State = Pawn ? Pawn->GetPlayerState() : nullptr;
	return State ? Footage.FindByPredicate([&State](const FDCCFootageEntry& E) { return E.Player == State->GetPlayerName(); }) : nullptr;
}

void ADCCArcadeHaunt::HitCrew(ADCCCharacterBase* Crawler, float Fraction, int32 Min, bool bHat, bool bGas)
{
	if (!Crawler || !Crawler->Stats || !Crawler->IsAlive())
	{
		return;
	}
	float Scale = 1.0f;
	if (const FDCCFootageEntry* Kit = KitOf(Crawler))
	{
		Scale *= bHat ? DCCChopShop::HardHatDamage(Kit->KitHat) : 1.0f;
		Scale *= bGas && Kit->KitGas >= 2 ? 0.5f : 1.0f;
	}
	Crawler->Stats->ApplyDamage(FMath::Max(1, FMath::RoundToInt(FMath::Max(float(Min), Crawler->Stats->MaxHitPoints * Fraction) * Scale)));
}

void ADCCArcadeHaunt::ThrowLightstick(const FString& Player, const FVector& Eye, const FVector& Dir)
{
	FDCCFootageEntry* Entry = Footage.FindByPredicate([&Player](const FDCCFootageEntry& E) { return E.Player == Player; });
	if (!Entry || Entry->Sticks <= 0 || Lightsticks.Num() >= 60)
	{
		return;
	}
	--Entry->Sticks;
	// Where it lands: along the look, up to 16 m, then down to the floor.
	const FVector Start = Eye + Dir * 120.0f;
	FHitResult Hit;
	FCollisionQueryParams Query(SCENE_QUERY_STAT(DCCLightstick), false);
	FVector Land = Start + Dir * 1600.0f;
	if (GetWorld()->LineTraceSingleByChannel(Hit, Start, Land, ECC_Visibility, Query))
	{
		Land = Hit.Location - Dir * 30.0f;
	}
	if (GetWorld()->LineTraceSingleByChannel(Hit, Land, Land - FVector(0, 0, 2000.0f), ECC_Visibility, Query))
	{
		Land = Hit.Location + FVector(0, 0, 6.0f);
	}
	FDCCLightstick Stick;
	Stick.From = Eye;
	Stick.At = Land;
	Stick.Time = ServerTime();
	Lightsticks.Add(Stick);
	ForceNetUpdate();
}

void ADCCArcadeHaunt::Burst(const TCHAR* SystemPath, const FVector& At, double Seconds, float Scale)
{
	const FString Path(SystemPath);
	UNiagaraSystem* System = LoadObject<UNiagaraSystem>(nullptr, *(Path + TEXT(".") + FPaths::GetBaseFilename(Path)));
	if (!System)
	{
		return;
	}
	if (UNiagaraComponent* Comp = UNiagaraFunctionLibrary::SpawnSystemAtLocation(GetWorld(), System, At, FRotator::ZeroRotator, FVector(Scale), true))
	{
		FBurst B;
		B.Comp = Comp;
		B.Until = Seconds > 0.0 ? ServerTime() + Seconds : 0.0; // 0 = stays
		Bursts.Add(B);
	}
}

bool ADCCArcadeHaunt::InVan(const FVector& Location) const
{
	return VanBox.IsValid && VanBox.IsInsideXY(Location);
}

void ADCCArcadeHaunt::ClientTick(float DeltaSeconds)
{
	EnsureGrid();
	BuildHouse();

	const float Time = GetWorld() ? GetWorld()->GetTimeSeconds() : 0.0f;

	// Bulbs that do not trust the wiring.
	for (int32 i = 0; i < RoomLights.Num(); ++i)
	{
		if (APointLight* Bulb = RoomLights[i].Get())
		{
			const float Noise = FMath::PerlinNoise1D(Time * 2.3f + i * 13.7f);
			const float Out = Noise < -0.35f ? 0.05f : 0.65f + 0.35f * Noise;
			Bulb->GetLightComponent()->SetIntensity(RoomLightBase[i] * Out);
		}
	}

	// Anomalies drift. Ghost miners flicker instead, and are gone when someone walks right up to one.
	const APawn* LocalPawn = GetWorld() && GetWorld()->GetFirstPlayerController() ? GetWorld()->GetFirstPlayerController()->GetPawn() : nullptr;
	for (int32 i = 0; i < Anomalies.Num(); ++i)
	{
		if (Anomalies[i].bGhost)
		{
			if (AActor* Ghost = Anomalies[i].Visual.Get())
			{
				const float Flicker = FMath::PerlinNoise1D(Time * 0.6f + Anomalies[i].Phase);
				const bool bClose = LocalPawn && FVector::Dist2D(LocalPawn->GetActorLocation(), Anomalies[i].Location) < 450.0f;
				bool bInStickLight = false;
				for (const FDCCLightstick& Stick : Lightsticks)
				{
					bInStickLight |= FVector::Dist2D(Stick.At, Anomalies[i].Location) < 900.0f;
				}
				const bool bSnuffing = EventActive(5, i, 8.0) || bInStickLight;
				const bool bShow = bSnuffing || (!bClose && Flicker > -0.2f);
				if (Ghost->IsHidden() == bShow)
				{
					Ghost->SetActorHiddenInGame(!bShow);
				}
				if (bShow)
				{
					// Afloat a hand's width off the floor, bobbing slowly, and his glow fading in and out instead of a
					// lamp switching: the flicker noise drives the brightness, so he thins out before he goes.
					const float Bob = FMath::Sin(Time * 0.9f + Anomalies[i].Phase) * 9.0f;
					Ghost->SetActorLocation(Anomalies[i].Location - FVector(0, 0, 110.0f - 26.0f - Bob));
					if (USkeletalMeshComponent* Body = Cast<USkeletalMeshComponent>(Ghost->GetRootComponent()))
					{
						if (UMaterialInstanceDynamic* Spirit = Cast<UMaterialInstanceDynamic>(Body->GetMaterial(0)))
						{
							const float Glow = bSnuffing ? 3.5f : FMath::Lerp(1.0f, 4.2f, FMath::SmoothStep(-0.2f, 0.35f, Flicker));
							Spirit->SetScalarParameterValue(TEXT("Emissive Intensity"), Glow * (0.9f + 0.1f * FMath::Sin(Time * 13.0f + i)));
						}
					}
				}
			}
			continue;
		}
		if (AActor* Orb = Anomalies[i].Visual.Get())
		{
			Orb->SetActorLocation(Anomalies[i].Location + FVector(0, 0, FMath::Sin(Time * 1.4f + i) * 18.0f));
			Orb->AddActorLocalRotation(FRotator(0, DeltaSeconds * 40.0f, 0));
		}
	}

	// The highlight reel's recording, the red lights over whoever is filming, and the reel itself.
	if (ReelPhase == 0)
	{
		RecordFrame();
	}
	TickRecLights();
	TickReelLocal(DeltaSeconds);

	// Knocks are not ambience any more (2026-11-15): three knocks means the roof is about to come down, there.
	TickRocksLocal();
	TickTrapsLocal(DeltaSeconds);
	// Payoff ghosts for finished stories, and the cage bell.
	for (int32 Story : StoriesDone)
	{
		if (!PayoffsShown.Contains(Story))
		{
			PayoffsShown.Add(Story);
			ShowPayoff(Story);
		}
	}
	if (BellRings != BellsHeard && Grid)
	{
		BellsHeard = BellRings;
		PlayBell(GetWorld(), VanBox.IsValid ? VanBox.GetCenter() + FVector(0, 0, 300) : GetActorLocation());
	}

	// A helmet lamp on every crawler, built locally on every machine.
	for (TActorIterator<ADCCCharacterBase> It(GetWorld()); It; ++It)
	{
		ADCCCharacterBase* Crawler = *It;
		if (Crawler->IsA<ADCCEnemyCharacter>() || Flashlights.Contains(Crawler))
		{
			continue;
		}
		USpotLightComponent* Torch = NewObject<USpotLightComponent>(Crawler);
		Torch->SetupAttachment(Crawler->GetRootComponent());
		Torch->SetRelativeLocation(FVector(18, 0, 82)); // on the helmet, not in the hand
		Torch->SetIntensity(4500.0f);
		Torch->SetOuterConeAngle(28.0f);
		Torch->SetInnerConeAngle(12.0f);
		Torch->SetAttenuationRadius(2400.0f);
		Torch->SetLightColor(FLinearColor(1.0f, 0.88f, 0.68f)); // a carbide lamp's yellow
		Torch->SetCastShadows(true);
		Torch->RegisterComponent();
		Flashlights.Add(Crawler, Torch);
	}
	// Lamps: off when switched off (L) or snuffed by a ghost; burning blue in firedamp.
	for (const TPair<TWeakObjectPtr<AActor>, TWeakObjectPtr<USpotLightComponent>>& Pair : Flashlights)
	{
		const APawn* Pawn = Cast<APawn>(Pair.Key.Get());
		USpotLightComponent* Torch = Pair.Value.Get();
		if (!Pawn || !Torch)
		{
			continue;
		}
		const APlayerState* LampOwner = Pawn->GetPlayerState();
		const bool bOff = LampOwner && IsLampOff(LampOwner->GetPlayerName());
		Torch->SetVisibility(!bOff);
		// The lamp you bought: carbide, brighter carbide, an electric cap lamp, an arc lamp.
		const FDCCFootageEntry* Kit = LampOwner ? FindEntry(LampOwner->GetPlayerName()) : nullptr;
		const int32 Lamp = Kit ? FMath::Clamp(int32(Kit->KitLamp), 0, 3) : 0;
		static const float Intensity[] = { 4500.0f, 6500.0f, 9000.0f, 12500.0f };
		static const float Range[] = { 2400.0f, 3000.0f, 3700.0f, 4600.0f };
		static const float Cone[] = { 28.0f, 30.0f, 33.0f, 37.0f };
		Torch->SetIntensity(Intensity[Lamp]);
		Torch->SetAttenuationRadius(Range[Lamp]);
		Torch->SetOuterConeAngle(Cone[Lamp]);
		Torch->SetLightColor(IsInFiredamp(Pawn->GetActorLocation()) ? FLinearColor(0.35f, 0.55f, 1.0f)
			: Lamp >= 2 ? FLinearColor(0.95f, 0.95f, 1.0f) : FLinearColor(1.0f, 0.88f, 0.68f));
	}
	// Point the torch where the crawler is looking, when we know it.
	for (const TPair<TWeakObjectPtr<AActor>, TWeakObjectPtr<USpotLightComponent>>& Pair : Flashlights)
	{
		const APawn* Pawn = Cast<APawn>(Pair.Key.Get());
		USpotLightComponent* Torch = Pair.Value.Get();
		if (Pawn && Torch)
		{
			const FRotator Aim = Pawn->IsLocallyControlled() || HasAuthority() ? Pawn->GetControlRotation() : Pawn->GetBaseAimRotation();
			Torch->SetWorldRotation(Aim);
		}
	}
}

void ADCCArcadeHaunt::Score(float DeltaSeconds)
{
	UWorld* World = GetWorld();
	const double Now = ServerTime();

	// Who just got hurt, for the "teammate in trouble, on camera" bonus.
	for (TActorIterator<ADCCCharacterBase> It(World); It; ++It)
	{
		ADCCCharacterBase* Crawler = *It;
		if (Crawler->IsA<ADCCEnemyCharacter>() || !Crawler->Stats)
		{
			continue;
		}
		int32& Last = LastHealth.FindOrAdd(Crawler, Crawler->Stats->CurrentHitPoints);
		if (Crawler->Stats->CurrentHitPoints < Last)
		{
			LastHurtAt.Add(Crawler, Now);
		}
		Last = Crawler->Stats->CurrentHitPoints;
	}

	TArray<APlayerController*> Players;
	GetPlayers(Players);
	for (APlayerController* PC : Players)
	{
		ADCCCharacterBase* Camera = Cast<ADCCCharacterBase>(PC->GetPawn());
		const FString Name = PC->PlayerState ? PC->PlayerState->GetPlayerName() : FString();
		if (!Camera || Name.IsEmpty())
		{
			continue;
		}
		FDCCFootageEntry& Entry = EntryFor(Name);
		LoadKit(Entry, PC);

		// Dying costs half the tape. Once per death.
		if ((Camera->IsDowned() || !Camera->IsAlive()) && !Penalised.Contains(Camera))
		{
			Penalised.Add(Camera);
			Entry.Footage *= 0.5f;
		}
		else if (Camera->IsAlive() && !Camera->IsDowned())
		{
			Penalised.Remove(Camera);
		}

		Entry.bRecording = Camera->bRecordingFootage && Entry.Tape > 0.0f && Camera->IsAlive();
		Entry.LiveShot = 0.0f;
		if (!Entry.bRecording)
		{
			if (Camera->bRecordingFootage && Entry.Tape <= 0.0f)
			{
				Camera->bRecordingFootage = false; // out of tape
			}
			continue;
		}

		const FVector Eye = Camera->GetPawnViewLocation();
		const FVector Look = PC->GetControlRotation().Vector();
		const float MaxRange = 2500.0f;
		const float CosHalfAngle = FMath::Cos(FMath::DegreesToRadians(25.0f));

		auto Rate = [&](const FVector& Where, float Value, const AActor* Subject)
		{
			const FVector To = Where - Eye;
			const float Distance = To.Size();
			if (Distance > MaxRange || Distance < 1.0f)
			{
				return 0.0f;
			}
			const float Dot = FVector::DotProduct(Look, To / Distance);
			if (Dot < CosHalfAngle)
			{
				return 0.0f;
			}
			FCollisionQueryParams Query(SCENE_QUERY_STAT(DCCHauntShot), false, Camera);
			if (Subject)
			{
				Query.AddIgnoredActor(Subject);
			}
			FHitResult Hit;
			if (World->LineTraceSingleByChannel(Hit, Eye, Where, ECC_Visibility, Query))
			{
				return 0.0f; // something is in the way
			}
			const float Centred = (Dot - CosHalfAngle) / (1.0f - CosHalfAngle);
			const float Close = 1.0f - Distance / MaxRange;
			return Value * (0.4f + 0.6f * Centred) * (0.3f + 0.7f * Close);
		};

		float Shot = 0.0f;
		// What is best in frame, for the highlight reel (label and where).
		float BestPart = 0.0f;
		uint8 BestLabel = 0;
		FVector BestWhere = FVector::ZeroVector;
		auto Note = [&](float Value, uint8 Label, const FVector& Where)
		{
			Shot += Value;
			if (Value > BestPart)
			{
				BestPart = Value;
				BestLabel = Label;
				BestWhere = Where;
			}
		};
		for (const FAnomaly& Anomaly : Anomalies)
		{
			const AActor* Seen = Anomaly.Visual.Get();
			if (Anomaly.bGhost && (!Seen || Seen->IsHidden()))
			{
				continue; // it was not there just then
			}
			Note(Rate(Anomaly.Location, Anomaly.Value, Seen), Anomaly.Value >= 30.0f ? 12 : Anomaly.bGhost ? 2 : Anomaly.Value >= 15.0f ? 3 : 1, Anomaly.Location);
		}
		// Trap moments on camera: a blast, a flash, the cart coming through, the planks going, a ghost at a lamp.
		for (const FDCCMineEvent& E : Events)
		{
			const double Age = Now - E.Time;
			const bool bLive = (E.Kind == 2 && Age > 3.5 && Age < 6.0) || (E.Kind == 5 && Age < 8.0) || ((E.Kind == 1 || E.Kind == 3 || E.Kind == 4) && Age < 2.0);
			if (bLive)
			{
				static const uint8 EventLabel[] = { 0, 10, 9, 7, 8, 11 };
				Note(Rate(E.At + FVector(0, 0, 100), E.Kind == 5 ? 20.0f : 35.0f, nullptr), EventLabel[FMath::Clamp(int32(E.Kind), 0, 5)], E.At);
			}
		}
		for (const TWeakObjectPtr<ADCCEnemyCharacter>& MonsterPtr : Monsters)
		{
			const ADCCEnemyCharacter* Monster = MonsterPtr.Get();
			if (!Monster || !Monster->IsAlive())
			{
				continue;
			}
			const ADCCCharacterBase* Victim = Monster->GetCurrentTarget();
			const bool bAttacking = Victim && FVector::Dist(Victim->GetActorLocation(), Monster->GetActorLocation()) < 450.0f;
			Note(Rate(Monster->GetActorLocation() + FVector(0, 0, 40), bAttacking ? 25.0f : 10.0f, Monster), bAttacking ? 5 : 4, Monster->GetActorLocation());
		}
		for (TActorIterator<ADCCCharacterBase> It(World); It; ++It)
		{
			const ADCCCharacterBase* Mate = *It;
			if (Mate == Camera || Mate->IsA<ADCCEnemyCharacter>())
			{
				continue;
			}
			const double* Hurt = LastHurtAt.Find(Mate);
			const float Value = Mate->IsDowned() ? 20.0f : (Hurt && Now - *Hurt < 2.0) ? 30.0f : 0.0f;
			if (Value > 0.0f)
			{
				Note(Rate(Mate->GetActorLocation() + FVector(0, 0, 40), Value, Mate), 6, Mate->GetActorLocation());
			}
		}

		Entry.LiveShot = Shot;
		if (Shot > 0.5f)
		{
			ShotLog.FindOrAdd(Name).Add({ Now, Shot, BestLabel, BestWhere });
		}
		Entry.Footage += Shot * DeltaSeconds;
		Entry.Tape = FMath::Max(0.0f, Entry.Tape - DeltaSeconds);
	}
}

void ADCCArcadeHaunt::Wrap()
{
	if (bWrapped)
	{
		return;
	}
	bWrapped = true;

	TArray<APlayerController*> Players;
	GetPlayers(Players);
	int32 TeamViews = 0;
	for (APlayerController* PC : Players)
	{
		const APawn* Pawn = PC->GetPawn();
		const ADCCCharacterBase* Crawler = Cast<ADCCCharacterBase>(Pawn);
		const FString Name = PC->PlayerState ? PC->PlayerState->GetPlayerName() : FString();
		if (Name.IsEmpty())
		{
			continue;
		}
		FDCCFootageEntry& Entry = EntryFor(Name);
		const bool bMadeIt = Crawler && Crawler->IsAlive() && !Crawler->IsDowned() && InVan(Crawler->GetActorLocation());
		Entry.Views = bMadeIt ? FMath::RoundToInt(Entry.Footage * ViewsPerPoint) : 0;
		Entry.bRecording = false;
		TeamViews += Entry.Views;

		DCCProfile::Add(PC->PlayerState, TEXT("Haunt"), TEXT("Plays"), 1.0f);
		if (Entry.Views > 0)
		{
			DCCProfile::RecordBest(PC->PlayerState, TEXT("Haunt"), TEXT("BestViews"), static_cast<float>(Entry.Views));
			DCCProfile::Add(PC->PlayerState, TEXT("Haunt"), TEXT("TotalViews"), static_cast<float>(Entry.Views));
			UDCCWalletComponent::AwardTickets(PC->PlayerState, FMath::Max(1, Entry.Views / 100), TEXT("footage uploaded"));
		}
		// Credits for the Mine Outfitters: footage that made it up, every note you read, and every story the crew told.
		{
			const TSet<int32>* Read = ReadBy.Find(Name);
			const int32 Credits = Entry.Views / 150 + (Read ? Read->Num() : 0) + 8 * StoriesDone.Num();
			UDCCWalletComponent::AwardCredits(PC->PlayerState, Credits, TEXT("Dead Air (footage, notes and stories)"));
		}
	}

	StatusLines.Reset();
	StatusLines.Add(FString::Printf(TEXT("UPLOADED. The crew pulled %s views."), *FText::AsNumber(TeamViews).ToString()));
	FString Line;
	for (const FDCCFootageEntry& Entry : Footage)
	{
		Line += FString::Printf(TEXT("%s%s %s"), Line.IsEmpty() ? TEXT("") : TEXT("     "), *Entry.Player,
			Entry.Views > 0 ? *FString::Printf(TEXT("%s views"), *FText::AsNumber(Entry.Views).ToString()) : TEXT("(tape left in the mine)"));
	}
	StatusLines.Add(Line);

	// Data extraction, then the highlight reel, then the vote (TickReelServer). The monsters are done.
	for (const TWeakObjectPtr<ADCCEnemyCharacter>& Monster : Monsters)
	{
		if (ADCCEnemyCharacter* M = Monster.Get()) { M->Destroy(); }
	}
	Uploaders.Reset();
	for (const FDCCFootageEntry& Entry : Footage)
	{
		if (Entry.Views > 0) { Uploaders.Add(Entry.Player); }
	}
	BuildReel();
	ExtractStart = ServerTime();
	ReelPhase = 1;
	ForceNetUpdate();
}

void ADCCArcadeHaunt::ServerTick(float DeltaSeconds)
{
	if (!bHouseBuilt)
	{
		return; // ClientTick builds it on this machine too, this same frame or the next
	}
	SpawnMonsters();
	if (EndsAt < 0.0)
	{
		EndsAt = ServerTime() + RoundSeconds;
	}
	TimeLeft = FMath::Max(0.0f, static_cast<float>(EndsAt - ServerTime()));

	Title = TEXT("DEAD AIR  -  the Lucky Tuesday mine, inside Mount Fourteener");
	Accent = FLinearColor(0.45f, 1.0f, 0.55f);

	if (bWrapped)
	{
		TickReelServer();
		return;
	}
	if (TimeLeft <= 0.0f)
	{
		Wrap();
		return;
	}

	ScoreTimer += DeltaSeconds;
	if (ScoreTimer >= 0.2f)
	{
		Score(ScoreTimer);
		ReadNotes();
		ScoreTimer = 0.0f;
	}
	TickRocksServer();
	TickTrapsServer(DeltaSeconds);

	// The cage bell. Until The Bell at 4:15 is finished it lies: three rings now and then, and the status line
	// swears the cage is going up. Finished, it rings once, honestly, 30 s before the cage really goes.
	const double BellNow = ServerTime();
	const bool bBellHonest = StoriesDone.Contains(4);
	if (NextLieAt <= 0.0)
	{
		NextLieAt = BellNow + FMath::FRandRange(70.0, 110.0);
	}
	if (!bBellHonest && BellNow >= NextLieAt && TimeLeft > 50.0f)
	{
		++BellRings;
		LieUntil = BellNow + 7.0;
		NextLieAt = BellNow + FMath::FRandRange(80.0, 120.0);
	}
	if (bBellHonest && !bHonestBellRung && TimeLeft <= 30.0f)
	{
		bHonestBellRung = true;
		++BellRings;
	}

	StatusLines.Reset();
	int32 Projected = 0;
	FString Crew;
	for (const FDCCFootageEntry& Entry : Footage)
	{
		Projected += FMath::RoundToInt(Entry.Footage * ViewsPerPoint);
		Crew += FString::Printf(TEXT("%s%s %s%s"), Crew.IsEmpty() ? TEXT("") : TEXT("     "), *Entry.Player,
			*FText::AsNumber(FMath::RoundToInt(Entry.Footage * ViewsPerPoint)).ToString(), Entry.bRecording ? TEXT(" (REC)") : TEXT(""));
	}
	StatusLines.Add(FString::Printf(TEXT("%d:%02d until the cage goes up.   Projected: %s views"),
		FMath::FloorToInt(TimeLeft / 60.0f), FMath::FloorToInt(FMath::Fmod(TimeLeft, 60.0f)), *FText::AsNumber(Projected).ToString()));
	if (!Crew.IsEmpty())
	{
		StatusLines.Add(Crew);
	}
	if (BellNow < LieUntil)
	{
		StatusLines.Add(TEXT("THE BELL. Three rings. The cage is going up. GET TO THE CAGE."));
	}
	if (TimeLeft < 45.0f)
	{
		StatusLines.Add(bHonestBellRung ? TEXT("The bell, honestly this time. Pike is holding the cage. GET BACK TO THE CAGE.")
			: TEXT("The roof is coming in. GET BACK TO THE CAGE."));
	}
	// Which stories are down here, and how far the crew has read into each.
	FString Stories;
	for (int32 Story : StoriesHere)
	{
		Stories += FString::Printf(TEXT("%s%s %d/%d%s"), Stories.IsEmpty() ? TEXT("Notes:  ") : TEXT("     "), DCCMineNotes::Stories()[Story].Title,
			NotesReadInStory(Story), NotesInStory(Story), StoriesDone.Contains(Story) ? TEXT(" (told)") : TEXT(""));
	}
	if (!Stories.IsEmpty())
	{
		StatusLines.Add(Stories);
	}
}
