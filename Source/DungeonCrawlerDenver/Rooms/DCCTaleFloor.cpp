#include "Rooms/DCCTaleFloor.h"
#include "Arcade/DCCWalletComponent.h"
#include "Progression/DCCAchievementComponent.h"
#include "Session/DCCStarterKits.h"
#include "Containers/Ticker.h"

#include "Rooms/DCCDungeonGrid.h"
#include "Characters/DCCCharacterBase.h"
#include "Characters/DCCEnemyCharacter.h"
#include "Characters/DCCEnemyCharacter_DebtEater.h"
#include "Characters/DCCEnemyCharacter_Wildlife.h"
#include "Characters/DCCEnemyCharacter_Corporate.h"
#include "Characters/DCCEnemyCharacter_Oathgate.h"
#include "Characters/DCCEnemyCharacter_NuclearFamily.h"
#include "Characters/DCCEnemyCharacter_TestBoss.h"
#include "Characters/DCCEnemyCharacter_Voldemortgage.h"
#include "Characters/DCCEnemyCharacter_SurveyDrone.h"
#include "Characters/DCCAllyCharacters.h"
#include "Favor/DCCFavorComponent.h"
#include "Onboarding/DCCOnboardingComponent.h"
#include "Onboarding/DCCOnboardingTypes.h"
#include "TimerManager.h"
#include "Combat/DCCGameState.h"
#include "Items/DCCLootContainerVariants.h"
#include "Onboarding/DCCOnboardingActors.h"
#include "Stats/DCCStatsComponent.h"
#include "UI/DCCEventLogTypes.h"
#include "Components/BoxComponent.h"
#include "Components/CapsuleComponent.h"
#include "Components/PointLightComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Components/TextRenderComponent.h"
#include "Engine/Engine.h"
#include "Engine/PointLight.h"
#include "Engine/StaticMesh.h"
#include "Engine/StaticMeshActor.h"
#include "Engine/TextRenderActor.h"
#include "Engine/World.h"
#include "EngineUtils.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "GameFramework/PlayerController.h"
#include "HAL/IConsoleManager.h"
#include "Materials/MaterialInstanceDynamic.h"
#include "UObject/ConstructorHelpers.h"
#include "Net/UnrealNetwork.h"
#include "Session/DCCSaveSystem.h"
#include "Rooms/DCCHub.h"
#include "Arcade/DCCArcade.h"
#include "Kismet/GameplayStatics.h"
#include "GameFramework/GameModeBase.h"
#include "Session/DCCProfile.h"
#include "Skills/DCCSkillComponent.h"
#include "Items/DCCInventoryComponent.h"
#include "GameFramework/PlayerState.h"

namespace
{
	TAutoConsoleVariable<int32> CVarQuickPlay(
		TEXT("dcc.AutoQuickPlay"),
		0,
		TEXT("1: every crawler skips the bedroom and starts on floor 1 of the World Dungeon, with the full HUD."),
		ECVF_Default);

	/** The roguelike's reward ladder: every two floors deeper is a better box. */
	EDCCLootBoxTier TierForFloor(int32 Floor)
	{
		const int32 Index = FMath::Clamp(1 + (Floor - 1) / 2, 1, static_cast<int32>(EDCCLootBoxTier::Celestial));
		return static_cast<EDCCLootBoxTier>(Index);
	}

	FString TierLabel(int32 Floor)
	{
		return StaticEnum<EDCCLootBoxTier>()->GetDisplayNameTextByValue(static_cast<int64>(TierForFloor(Floor))).ToString().ToUpper();
	}

	FText NameOf(const ADCCCharacterBase* Crawler)
	{
		if (!Crawler)
		{
			return FText::GetEmpty();
		}
		return Crawler->DisplayName.IsEmpty() ? FText::FromString(Crawler->GetActorNameOrLabel()) : Crawler->DisplayName;
	}

	void Announce(const UObject* WorldContext, const FText& Text)
	{
		UE_LOG(LogTemp, Log, TEXT("[DCC Tale] %s"), *Text.ToString());
		ADCCGameState::Log(WorldContext, EDCCLogCategory::Announcement, Text);
	}

	ADCCCharacterBase* LivingPlayer(AActor* Actor)
	{
		ADCCCharacterBase* Crawler = Cast<ADCCCharacterBase>(Actor);
		if (!Crawler || !Crawler->IsPlayerControlled() || !Crawler->IsAlive())
		{
			return nullptr;
		}
		return Crawler;
	}
}

ADCCTaleFloor::ADCCTaleFloor()
{
	PrimaryActorTick.bCanEverTick = false;

	// Replicated for the layout only (seed, floor number). Every machine builds
	// the floor's geometry itself from those; see OnRep_Layout.
	bReplicates = true;
	bAlwaysRelevant = true;
	SetReplicatingMovement(false);

	USceneComponent* Root = CreateDefaultSubobject<USceneComponent>(TEXT("Root"));
	SetRootComponent(Root);

	UpPad = CreateDefaultSubobject<UBoxComponent>(TEXT("UpPad"));
	DownPad = CreateDefaultSubobject<UBoxComponent>(TEXT("DownPad"));
	UpPad->SetupAttachment(Root);
	DownPad->SetupAttachment(Root);
	UpPad->SetBoxExtent(FVector(150.0f, 150.0f, 140.0f));
	DownPad->SetBoxExtent(FVector(150.0f, 150.0f, 140.0f));
	UpPad->SetCollisionProfileName(TEXT("OverlapAllDynamic"));
	DownPad->SetCollisionProfileName(TEXT("OverlapAllDynamic"));
	UpPad->SetGenerateOverlapEvents(true);
	DownPad->SetGenerateOverlapEvents(true);

	static ConstructorHelpers::FObjectFinder<UStaticMesh> Cube(TEXT("/Engine/BasicShapes/Cube.Cube"));
	if (Cube.Succeeded())
	{
		CubeMesh = Cube.Object;
	}

	static ConstructorHelpers::FObjectFinder<UMaterialInterface> Mat(TEXT("/Engine/BasicShapes/BasicShapeMaterial.BasicShapeMaterial"));
	if (Mat.Succeeded())
	{
		ShapeMaterial = Mat.Object;
	}
}

ADCCTaleFloor* ADCCTaleFloor::GetOrCreate(UWorld* World)
{
	if (!World || World->GetNetMode() == NM_Client)
	{
		return nullptr;
	}

	for (TActorIterator<ADCCTaleFloor> It(World); It; ++It)
	{
		return *It;
	}

	FActorSpawnParameters Params;
	Params.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
	// Clear of the hand-built map (origin) and the home wing (-30000, 30000), and
	// high enough that a landscape under the origin cannot swallow the floor.
	return World->SpawnActor<ADCCTaleFloor>(FVector(150000.0f, 150000.0f, 5000.0f), FRotator::ZeroRotator, Params);
}

void ADCCTaleFloor::BeginPlay()
{
	Super::BeginPlay();

	if (!CubeMesh)
	{
		CubeMesh = LoadObject<UStaticMesh>(nullptr, TEXT("/Engine/BasicShapes/Cube.Cube"));
	}
	if (!ShapeMaterial)
	{
		ShapeMaterial = LoadObject<UMaterialInterface>(nullptr, TEXT("/Engine/BasicShapes/BasicShapeMaterial.BasicShapeMaterial"));
	}

	if (HasAuthority())
	{
		UpPad->OnComponentBeginOverlap.AddDynamic(this, &ADCCTaleFloor::HandlePadOverlap);
		DownPad->OnComponentBeginOverlap.AddDynamic(this, &ADCCTaleFloor::HandlePadOverlap);
	}
}

bool ADCCTaleFloor::Enter(ADCCCharacterBase* Crawler, AActor* ReturnPoint)
{
	if (!HasAuthority() || !Crawler)
	{
		return false;
	}

	if (ReturnPoint)
	{
		HomeLocation = ReturnPoint->GetActorLocation() + FVector(0.0f, 0.0f, 120.0f);
	}

	const bool bFresh = bRunComplete || !bBuilt;
	if (bFresh)
	{
		FloorNumber = 1;
		bRunComplete = false;
		if (!BuildFloor())
		{
			return false;
		}
		BeginRun();
	}

	JoinRun(Crawler);
	TeleportToStart(Crawler);

	if (bFresh)
	{
		Announce(this, NSLOCTEXT("DCC", "TaleEnterRogue",
			"The World Dungeon goes down as far as you dare. Every floor cleared pays a better box; every floor deeper hits much harder. A SURFACE pad in each start room cashes you out."));
		Announce(this, NSLOCTEXT("DCC", "TaleEnter",
			"The World Dungeon is open for viewing. Fights are staffed. A chest is unattended. The stairs are in the farthest room, and they stay locked while management is still on the clock."));
	}
	return true;
}

bool ADCCTaleFloor::BuildFloor()
{
	if (!CubeMesh)
	{
		UE_LOG(LogTemp, Error, TEXT("[DCC Tale] Engine cube is missing. The floor was not built."));
		return false;
	}

	// Server only. The layout is a seed and a floor number; every machine
	// builds the walls from those itself (OnRep_Layout), and only what fights
	// or holds loot is spawned here and replicated.
	const int32 Seed = FMath::Rand();
	if (!BuildGeometry(Seed))
	{
		return false;
	}

	SpawnLightsAndContent();

	LayoutSeed = Seed;
	LayoutFloor = FloorNumber;
	bLayoutLive = true;
	++LayoutVersion;
	ForceNetUpdate();

	Announce(this, FText::Format(
		NSLOCTEXT("DCC", "TaleFloorOpen", "Floor {0}. The room at the far end is where the stairs are."),
		FText::AsNumber(FloorNumber)));
	return true;
}

bool ADCCTaleFloor::BuildGeometry(int32 Seed)
{
	ClearBuilt();

	if (!Grid)
	{
		Grid = NewObject<UDCCDungeonGrid>(this);
	}

	Grid->SetOrigin(GetActorLocation(), TileSize);
	// Deeper floors are bigger: one more fight room every three floors, up to six.
	const int32 Fights = FMath::Min(3 + (FloorNumber - 1) / 3, 6);
	if (!Grid->Build(32, Seed, Fights, 1))
	{
		return false;
	}

	const FLinearColor Corridor(0.08f, 0.08f, 0.09f);
	const int32 Size = Grid->GetSize();
	TArray<uint8> Used;
	Used.Init(0, Size * Size);

	for (int32 Y = 0; Y < Size; ++Y)
	{
		for (int32 X = 0; X < Size; ++X)
		{
			if (!Grid->IsOpen(X, Y) || Grid->RoomIndexAt(X, Y) != -1 || Used[Y * Size + X])
			{
				continue;
			}

			int32 W = 1;
			while (X + W < Size && Grid->IsOpen(X + W, Y) && Grid->RoomIndexAt(X + W, Y) == -1 && !Used[Y * Size + X + W])
			{
				++W;
			}

			int32 H = 1;
			bool bGrow = true;
			while (bGrow && Y + H < Size)
			{
				for (int32 XX = 0; XX < W; ++XX)
				{
					if (!Grid->IsOpen(X + XX, Y + H) || Grid->RoomIndexAt(X + XX, Y + H) != -1 || Used[(Y + H) * Size + X + XX])
					{
						bGrow = false;
						break;
					}
				}
				if (bGrow)
				{
					++H;
				}
			}

			for (int32 YY = 0; YY < H; ++YY)
			{
				for (int32 XX = 0; XX < W; ++XX)
				{
					Used[(Y + YY) * Size + X + XX] = 1;
				}
			}

			SpawnFloorRect(X, Y, W, H, Corridor);
		}
	}

	for (const UDCCDungeonGrid::FRoom& Room : Grid->GetRooms())
	{
		FLinearColor Color(0.16f, 0.15f, 0.13f);
		if (Room.Kind == TEXT("start")) { Color = FLinearColor(0.22f, 0.18f, 0.12f); }
		else if (Room.Kind == TEXT("fight")) { Color = FLinearColor(0.22f, 0.10f, 0.09f); }
		else if (Room.Kind == TEXT("chest")) { Color = FLinearColor(0.24f, 0.18f, 0.06f); }
		else if (Room.Kind == TEXT("stairs")) { Color = FLinearColor(0.10f, 0.13f, 0.22f); }
		SpawnFloorRect(Room.X, Room.Y, Room.W, Room.H, Color);
	}

	SpawnWalls();
	SpawnRoomDressing(Seed);
	bBuilt = true;

	const UDCCDungeonGrid::FRoom& StartRoom = Grid->GetRooms()[0];
	StartLocation = CellLocation(StartRoom.Center(), 110.0f);
	UpPad->SetWorldLocation(CellLocation(FIntPoint(StartRoom.X + 1, StartRoom.Y + 1), 100.0f));

	const UDCCDungeonGrid::FRoom& Stairs = Grid->GetRooms().Last();
	DownPad->SetWorldLocation(CellLocation(FIntPoint(Stairs.X + Stairs.W - 2, Stairs.Y + Stairs.H - 2), 100.0f));

	SpawnSign(UpPad->GetComponentLocation() + FVector(0.0f, 0.0f, 160.0f), TEXT("SURFACE"), FColor(120, 220, 140));
	SpawnSign(DownPad->GetComponentLocation() + FVector(0.0f, 0.0f, 180.0f), TEXT("STAIRS DOWN"), FColor(255, 196, 80));
	// The push-your-luck sign: what the next floor costs, and what clearing this one pays.
	SpawnSign(DownPad->GetComponentLocation() + FVector(0.0f, 0.0f, 250.0f),
		FString::Printf(TEXT("CLEAR: %s BOX   NEXT: THREAT +%d"), *TierLabel(FloorNumber), FloorNumber), FColor(255, 120, 90));
	SpawnSign(UpPad->GetComponentLocation() + FVector(0.0f, 0.0f, 230.0f), TEXT("cash out"), FColor(120, 220, 140));
	SpawnSign(StartLocation + FVector(0.0f, 0.0f, 220.0f),
		FString::Printf(TEXT("FLOOR %d"), FloorNumber), FColor(230, 220, 190));
	return true;
}

void ADCCTaleFloor::OnRep_Layout()
{
	// Clients only (a RepNotify never runs on the server). Rebuild the same
	// floor the host built, from the same seed, so walls, lights and signs
	// exist locally with their colours and text, and collision matches.
	if (!bLayoutLive)
	{
		ClearBuilt();
		return;
	}

	if (!CubeMesh)
	{
		CubeMesh = LoadObject<UStaticMesh>(nullptr, TEXT("/Engine/BasicShapes/Cube.Cube"));
	}
	// The initial bunch can land before BeginPlay has loaded these.
	if (!ShapeMaterial)
	{
		ShapeMaterial = LoadObject<UMaterialInterface>(nullptr, TEXT("/Engine/BasicShapes/BasicShapeMaterial.BasicShapeMaterial"));
	}
	FloorNumber = LayoutFloor;
	BuildGeometry(LayoutSeed);
}

void ADCCTaleFloor::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);
	DOREPLIFETIME(ADCCTaleFloor, LayoutVersion);
	DOREPLIFETIME(ADCCTaleFloor, LayoutSeed);
	DOREPLIFETIME(ADCCTaleFloor, LayoutFloor);
	DOREPLIFETIME(ADCCTaleFloor, bLayoutLive);
}

void ADCCTaleFloor::ClearBuilt()
{
	for (AActor* Actor : BuiltActors)
	{
		if (IsValid(Actor))
		{
			Actor->Destroy();
		}
	}
	BuiltActors.Reset();
	StairGuards.Reset();
	bBuilt = false;
}

void ADCCTaleFloor::SpawnBlock(const FVector& DesiredCenter, const FVector& WorldSize, const FLinearColor& Color)
{
	UWorld* World = GetWorld();
	if (!World || !CubeMesh)
	{
		return;
	}

	const FBoxSphereBounds Bounds = CubeMesh->GetBounds();
	const FVector MeshSize = Bounds.BoxExtent * 2.0f;
	if (MeshSize.GetMin() < 1.0f)
	{
		return;
	}

	const FVector Scale(WorldSize.X / MeshSize.X, WorldSize.Y / MeshSize.Y, WorldSize.Z / MeshSize.Z);
	const FVector Location = DesiredCenter - Bounds.Origin * Scale;
	const FTransform Transform(FRotator::ZeroRotator, Location, Scale);

	FActorSpawnParameters Params;
	Params.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
	AStaticMeshActor* Block = World->SpawnActorDeferred<AStaticMeshActor>(AStaticMeshActor::StaticClass(), Transform);
	if (!Block)
	{
		return;
	}

	Block->SetReplicates(false);

	if (UStaticMeshComponent* Mesh = Block->GetStaticMeshComponent())
	{
		Mesh->SetMobility(EComponentMobility::Movable);
		Mesh->SetStaticMesh(CubeMesh);
		Mesh->SetCollisionProfileName(TEXT("BlockAll"));
		Mesh->SetCastShadow(false);
		if (ShapeMaterial)
		{
			UMaterialInstanceDynamic* Mid = UMaterialInstanceDynamic::Create(ShapeMaterial, Mesh);
			Mid->SetVectorParameterValue(TEXT("Color"), Color);
			Mid->SetVectorParameterValue(TEXT("BaseColor"), Color);
			Mesh->SetMaterial(0, Mid);
		}
	}

	Block->FinishSpawning(Transform);
	BuiltActors.Add(Block);
}

void ADCCTaleFloor::SpawnFloorRect(int32 CellX, int32 CellY, int32 CellsWide, int32 CellsDeep, const FLinearColor& Color)
{
	const FVector Min = GetActorLocation() + FVector(CellX * TileSize, CellY * TileSize, 0.0f);
	const FVector Size(CellsWide * TileSize, CellsDeep * TileSize, FloorThickness);
	const FVector Center = Min + FVector(Size.X * 0.5f, Size.Y * 0.5f, -FloorThickness * 0.5f);
	SpawnBlock(Center, Size, Color);
}

void ADCCTaleFloor::SpawnWalls()
{
	const int32 Size = Grid->GetSize();

	struct FRun
	{
		int32 Line = 0;
		int32 A = 0;
		int32 B = 0;
	};

	TArray<FRun> Horizontal;
	TArray<FRun> Vertical;

	auto AddRun = [](TArray<FRun>& Runs, int32 Line, int32 A)
	{
		for (FRun& Run : Runs)
		{
			if (Run.Line == Line && Run.B == A)
			{
				Run.B = A + 1;
				return;
			}
		}
		FRun Run;
		Run.Line = Line;
		Run.A = A;
		Run.B = A + 1;
		Runs.Add(Run);
	};

	for (int32 Y = 0; Y < Size; ++Y)
	{
		for (int32 X = 0; X < Size; ++X)
		{
			if (!Grid->IsOpen(X, Y))
			{
				continue;
			}
			if (!Grid->IsOpen(X, Y + 1)) { AddRun(Horizontal, Y + 1, X); }
			if (!Grid->IsOpen(X, Y - 1)) { AddRun(Horizontal, Y, X); }
			if (!Grid->IsOpen(X + 1, Y)) { AddRun(Vertical, X + 1, Y); }
			if (!Grid->IsOpen(X - 1, Y)) { AddRun(Vertical, X, Y); }
		}
	}

	const FLinearColor Wall(0.05f, 0.05f, 0.06f);
	const FVector Origin = GetActorLocation();

	for (const FRun& Run : Horizontal)
	{
		const float X0 = Origin.X + Run.A * TileSize;
		const float X1 = Origin.X + Run.B * TileSize;
		const float Y = Origin.Y + Run.Line * TileSize;
		SpawnBlock(FVector((X0 + X1) * 0.5f, Y, FloorTopZ() + WallHeight * 0.5f),
			FVector(X1 - X0, WallThickness, WallHeight), Wall);
	}

	for (const FRun& Run : Vertical)
	{
		const float Y0 = Origin.Y + Run.A * TileSize;
		const float Y1 = Origin.Y + Run.B * TileSize;
		const float X = Origin.X + Run.Line * TileSize;
		SpawnBlock(FVector(X, (Y0 + Y1) * 0.5f, FloorTopZ() + WallHeight * 0.5f),
			FVector(WallThickness, Y1 - Y0, WallHeight), Wall);
	}
}

FVector ADCCTaleFloor::CellLocation(FIntPoint Cell, float ZOffset) const
{
	return Grid->CellCenter(Cell) + FVector(0.0f, 0.0f, ZOffset);
}

ADCCEnemyCharacter* ADCCTaleFloor::SpawnEnemy(TSubclassOf<ADCCEnemyCharacter> Class, const FVector& Location, bool bBoss)
{
	UWorld* World = GetWorld();
	if (!World || !Class)
	{
		return nullptr;
	}

	const FTransform Transform(FRotator(0.0f, Grid->FRand() * 360.0f, 0.0f), Location);
	ADCCEnemyCharacter* Enemy = World->SpawnActorDeferred<ADCCEnemyCharacter>(Class, Transform);
	if (!Enemy)
	{
		return nullptr;
	}

	Enemy->SetChaseGrid(Grid);
	Enemy->LeashRadius = FMath::Max(Enemy->LeashRadius, 9000.0f);
	if (Enemy->Stats)
	{
		// Each floor down is a lot stronger: levels climb with depth, and the
		// body stats climb every other floor. This is the authored stat block
		// being written on purpose, the same way an encounter spawn does it.
		const int32 Threat = FMath::Max(0, FloorNumber - 1);
		Enemy->Stats->Level = FMath::Max(Enemy->Stats->Level, FloorNumber + Threat / 2);
		for (const EDCCAbility Ability : { EDCCAbility::Strength, EDCCAbility::Constitution, EDCCAbility::Dexterity })
		{
			Enemy->Stats->AbilityScores.SetScore(Ability, Enemy->Stats->AbilityScores.GetScore(Ability) + Threat / 2);
		}
	}

	Enemy->FinishSpawning(Transform);

	if (Enemy->Stats)
	{
		Enemy->Stats->RefreshDerivedStats(true);
	}
	if (UCharacterMovementComponent* Movement = Enemy->GetCharacterMovement())
	{
		Movement->bUseRVOAvoidance = true;
		Movement->AvoidanceConsiderationRadius = 220.0f;
	}
	if (UCapsuleComponent* Capsule = Enemy->GetCapsuleComponent())
	{
		const FVector At = Enemy->GetActorLocation();
		Enemy->SetActorLocation(FVector(At.X, At.Y, FloorTopZ() + Capsule->GetScaledCapsuleHalfHeight() + 8.0f));
	}

	Enemy->OnCharacterDied.AddUniqueDynamic(this, &ADCCTaleFloor::HandleEnemyDied);

	BuiltActors.Add(Enemy);
	if (bBoss)
	{
		StairGuards.Add(Enemy);
	}
	return Enemy;
}

void ADCCTaleFloor::SpawnSign(const FVector& Location, const FString& Text, const FColor& Color)
{
	UWorld* World = GetWorld();
	if (!World)
	{
		return;
	}

	const FTransform Transform(FRotator(0.0f, 180.0f, 0.0f), Location);
	ATextRenderActor* Sign = World->SpawnActorDeferred<ATextRenderActor>(ATextRenderActor::StaticClass(), Transform);
	if (!Sign)
	{
		return;
	}

	Sign->SetReplicates(false);
	if (UTextRenderComponent* Render = Sign->GetTextRender())
	{
		Render->SetText(FText::FromString(Text));
		Render->SetTextRenderColor(Color);
		Render->SetHorizontalAlignment(EHTA_Center);
		Render->SetVerticalAlignment(EVRTA_TextCenter);
		Render->SetWorldSize(56.0f);
	}
	Sign->FinishSpawning(Transform);
	BuiltActors.Add(Sign);
}

void ADCCTaleFloor::SpawnRoomDressing(int32 Seed)
{
	// Runs on every machine, so it must not touch Grid's random stream (the
	// server keeps drawing from it for enemies). Its own stream, from the seed.
	UWorld* World = GetWorld();
	if (!World || !Grid)
	{
		return;
	}

	FRandomStream Dressing(Seed ^ 0x5EED1E55);
	CrateCells.Reset();

	UStaticMesh* Crate = LoadObject<UStaticMesh>(nullptr, TEXT("/Game/Ice_Station/Meshes/Crates/SM_crate_01.SM_crate_01"));
	float CrateScale = 1.0f;
	if (Crate)
	{
		const float Extent = Crate->GetBounds().BoxExtent.GetMax();
		CrateScale = Extent > 1.0f ? 80.0f / Extent : 1.0f;
	}

	for (const UDCCDungeonGrid::FRoom& Room : Grid->GetRooms())
	{
		const FVector Center = CellLocation(Room.Center(), 280.0f);
		FLinearColor LightColor(1.0f, 0.86f, 0.68f);
		if (Room.Kind == TEXT("fight")) { LightColor = FLinearColor(1.0f, 0.55f, 0.45f); }
		else if (Room.Kind == TEXT("chest")) { LightColor = FLinearColor(1.0f, 0.82f, 0.35f); }
		else if (Room.Kind == TEXT("stairs")) { LightColor = FLinearColor(0.65f, 0.75f, 1.0f); }

		FActorSpawnParameters Params;
		Params.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
		if (APointLight* Light = World->SpawnActor<APointLight>(Center, FRotator::ZeroRotator, Params))
		{
			Light->SetReplicates(false);
			if (UPointLightComponent* Comp = Cast<UPointLightComponent>(Light->GetLightComponent()))
			{
				Comp->SetMobility(EComponentMobility::Movable);
				Comp->SetIntensity(4000.0f);
				Comp->SetAttenuationRadius(1800.0f);
				Comp->SetLightColor(LightColor);
				Comp->SetCastShadows(false);
			}
			BuiltActors.Add(Light);
		}

		if (Room.Kind == TEXT("chest"))
		{
			SpawnSign(CellLocation(Room.Center(), 180.0f), TEXT("SUPPLIES"), FColor(255, 210, 90));
		}

		if (Room.Kind != TEXT("fight") || !Crate)
		{
			continue;
		}

		TArray<FIntPoint> Spots;
		for (int32 Y = Room.Y + 1; Y < Room.Y + Room.H - 1; ++Y)
		{
			for (int32 X = Room.X + 1; X < Room.X + Room.W - 1; ++X)
			{
				if (Grid->IsOpen(X, Y) && FIntPoint(X, Y) != Room.Center())
				{
					Spots.Add(FIntPoint(X, Y));
				}
			}
		}
		if (Spots.IsEmpty())
		{
			continue;
		}

		const FIntPoint Spot = Spots[Dressing.RandRange(0, Spots.Num() - 1)];
		CrateCells.Add(Spot);
		const FTransform Transform(FRotator(0.0f, Dressing.FRand() * 360.0f, 0.0f), CellLocation(Spot, 40.0f), FVector(CrateScale));
		if (AStaticMeshActor* Prop = World->SpawnActorDeferred<AStaticMeshActor>(AStaticMeshActor::StaticClass(), Transform))
		{
			Prop->SetReplicates(false);
			if (UStaticMeshComponent* Mesh = Prop->GetStaticMeshComponent())
			{
				Mesh->SetMobility(EComponentMobility::Movable);
				Mesh->SetStaticMesh(Crate);
				Mesh->SetCollisionProfileName(TEXT("BlockAll"));
			}
			Prop->FinishSpawning(Transform);
			BuiltActors.Add(Prop);
		}
	}
}

void ADCCTaleFloor::SpawnLightsAndContent()
{
	// Server only: everything here fights or holds loot, and replicates.
	UWorld* World = GetWorld();
	if (!World || !Grid || !HasAuthority())
	{
		return;
	}

	const TArray<TSubclassOf<ADCCEnemyCharacter>> Pool = {
		ADCCEnemyCharacter_DebtEater::StaticClass(),
		ADCCEnemyCharacter_Beholder::StaticClass(),
		ADCCEnemyCharacter_Thornback::StaticClass(),
		ADCCEnemyCharacter_BreakRoomMold::StaticClass(),
		ADCCEnemyCharacter_ShellCompany::StaticClass(),
		ADCCEnemyCharacter_Intern::StaticClass(),
		ADCCEnemyCharacter_Notary::StaticClass(),
		ADCCEnemyCharacter_EscalationSpecialist::StaticClass(),
		ADCCEnemyCharacter_FinePrint::StaticClass(),
		ADCCEnemyCharacter_UnclaimedProperty::StaticClass(),
		ADCCEnemyCharacter_Arbiter::StaticClass(),
		ADCCEnemyCharacter_SurveyDrone::StaticClass(),
		ADCCEnemy_NuclearDad::StaticClass()
	};

	const TArray<TSubclassOf<ADCCLootContainer>> Chests = {
		ADCCLootContainer_KitchenDrawer::StaticClass(),
		ADCCLootContainer_Armory::StaticClass(),
		ADCCLootContainer_Quartermaster::StaticClass(),
		ADCCLootContainer_ManagementLockbox::StaticClass()
	};

	int32 PoolCursor = 0;
	const int32 MobsPerFight = FMath::Min(1 + FloorNumber, 7);

	for (const UDCCDungeonGrid::FRoom& Room : Grid->GetRooms())
	{
		TArray<FIntPoint> Spots;
		for (int32 Y = Room.Y + 1; Y < Room.Y + Room.H - 1; ++Y)
		{
			for (int32 X = Room.X + 1; X < Room.X + Room.W - 1; ++X)
			{
				if (Grid->IsOpen(X, Y) && !CrateCells.Contains(FIntPoint(X, Y)))
				{
					Spots.Add(FIntPoint(X, Y));
				}
			}
		}
		Spots.Remove(Room.Center());

		if (Room.Kind == TEXT("fight"))
		{
			const int32 Count = FMath::Min(MobsPerFight, Spots.Num());
			for (int32 i = 0; i < Count; ++i)
			{
				const int32 Pick = Grid->RandRange(0, Spots.Num() - 1);
				const FIntPoint Spot = Spots[Pick];
				Spots.RemoveAt(Pick);
				SpawnEnemy(Pool[PoolCursor % Pool.Num()], CellLocation(Spot, 120.0f), false);
				++PoolCursor;
			}
		}
		else if (Room.Kind == TEXT("chest"))
		{
			// Better chests deeper down, matching the box ladder.
			const TSubclassOf<ADCCLootContainer> ChestClass = Chests[FMath::Min((FloorNumber - 1) / 2, Chests.Num() - 1)];
			const FTransform Transform(FRotator::ZeroRotator, CellLocation(Room.Center(), 40.0f));
			if (ADCCLootContainer* Chest = World->SpawnActorDeferred<ADCCLootContainer>(ChestClass, Transform))
			{
				Chest->FinishSpawning(Transform);
				BuiltActors.Add(Chest);
			}
		}
		else if (Room.Kind == TEXT("stairs"))
		{
			Spots.Remove(FIntPoint(Room.X + Room.W - 2, Room.Y + Room.H - 2));
			// Every fifth floor is a boss floor.
			const TSubclassOf<ADCCEnemyCharacter> BossClass = FloorNumber % 5 == 0
				? ADCCEnemyCharacter_Voldemortgage::StaticClass()
				: ADCCEnemyCharacter_TestBoss::StaticClass();
			SpawnEnemy(BossClass, CellLocation(Room.Center(), 150.0f), true);

			const int32 Adds = FMath::Min(FloorNumber, Spots.Num());
			for (int32 i = 0; i < Adds; ++i)
			{
				const int32 Pick = Grid->RandRange(0, Spots.Num() - 1);
				const FIntPoint Spot = Spots[Pick];
				Spots.RemoveAt(Pick);
				SpawnEnemy(ADCCEnemyCharacter_DebtEater::StaticClass(), CellLocation(Spot, 120.0f), true);
			}
		}
	}
}

void ADCCTaleFloor::TeleportToStart(ADCCCharacterBase* Crawler)
{
	PlaceAtStart(Crawler, 0);
}

void ADCCTaleFloor::PlaceAtStart(ADCCCharacterBase* Crawler, int32 Slot)
{
	// Slot 0 is the middle of the start room; the rest stand in a ring around it
	// so a party of five does not arrive inside itself.
	FVector At = StartLocation;
	if (Slot > 0)
	{
		const float Angle = Slot * (2.0f * PI / 6.0f);
		At += FVector(FMath::Cos(Angle), FMath::Sin(Angle), 0.0f) * 220.0f;
	}
	DCCOnboarding::TeleportCrawler(Crawler, At, FRotator(0.0f, 0.0f, 0.0f));
	ArmPad(Crawler);
}

void ADCCTaleFloor::TeleportHome(ADCCCharacterBase* Crawler)
{
	// Every run ends in Concourse Zero, the hub, where the shops are.
	if (!ADCCHub::SendToHub(Crawler))
	{
		DCCOnboarding::TeleportCrawler(Crawler, HomeLocation, FRotator(0.0f, 90.0f, 0.0f));
	}
	ArmPad(Crawler);
}

void ADCCTaleFloor::CollectParty(TArray<ADCCCharacterBase*>& Out) const
{
	Out.Reset();
	UWorld* World = GetWorld();
	if (!World)
	{
		return;
	}

	CollectPlayers(Out);

	for (const TWeakObjectPtr<ADCCEnemyCharacter>& StandIn : StandIns)
	{
		if (StandIn.IsValid() && (StandIn->IsAlive() || StandIn->IsDowned()) && Contains(StandIn->GetActorLocation()))
		{
			Out.Add(StandIn.Get());
		}
	}
}

void ADCCTaleFloor::CollectPlayers(TArray<ADCCCharacterBase*>& Out) const
{
	Out.Reset();
	UWorld* World = GetWorld();
	if (!World)
	{
		return;
	}

	// Downed counts: a crawler on the floor rolling death saves is still in the run.
	for (TActorIterator<ADCCCharacterBase> It(World); It; ++It)
	{
		ADCCCharacterBase* Crawler = *It;
		if (Crawler && Crawler->IsPlayerControlled() && (Crawler->IsAlive() || Crawler->IsDowned())
			&& Contains(Crawler->GetActorLocation()))
		{
			Out.Add(Crawler);
		}
	}
}

bool ADCCTaleFloor::Contains(const FVector& WorldPoint) const
{
	if (!Grid)
	{
		return false;
	}

	const FVector Local = WorldPoint - GetActorLocation();
	const float Extent = Grid->GetSize() * TileSize;
	return Local.X >= -600.0f && Local.Y >= -600.0f && Local.X <= Extent + 600.0f && Local.Y <= Extent + 600.0f
		&& WorldPoint.Z > GetActorLocation().Z - 800.0f && WorldPoint.Z < GetActorLocation().Z + 2500.0f;
}

bool ADCCTaleFloor::StairsAreOpen() const
{
	if (StairGuards.IsEmpty())
	{
		return false;
	}

	for (const TWeakObjectPtr<ADCCEnemyCharacter>& Guard : StairGuards)
	{
		if (Guard.IsValid() && Guard->IsAlive())
		{
			return false;
		}
	}
	return true;
}

void ADCCTaleFloor::ArmPad(ADCCCharacterBase* Crawler)
{
	if (Crawler && GetWorld())
	{
		PadReadyAt.Add(Crawler, GetWorld()->GetTimeSeconds() + 1.5);
	}
}

bool ADCCTaleFloor::PadCooling(ADCCCharacterBase* Crawler) const
{
	if (const double* Until = PadReadyAt.Find(Crawler))
	{
		return GetWorld() && GetWorld()->GetTimeSeconds() < *Until;
	}
	return false;
}

void ADCCTaleFloor::HandlePadOverlap(UPrimitiveComponent* OverlappedComponent, AActor* OtherActor,
	UPrimitiveComponent* OtherComp, int32 OtherBodyIndex, bool bFromSweep, const FHitResult& SweepResult)
{
	ADCCCharacterBase* Crawler = LivingPlayer(OtherActor);
	if (!HasAuthority() || !bBuilt || !Crawler || PadCooling(Crawler))
	{
		return;
	}

	if (OverlappedComponent == UpPad)
	{
		Announce(this, FText::Format(NSLOCTEXT("DCC", "TaleSurface",
			"{0} has left the World Dungeon. The archway will still be there."), NameOf(Crawler)));
		TeleportHome(Crawler);

		// The last player out ends the run. Whatever they earned per floor is already theirs.
		TArray<ADCCCharacterBase*> Left;
		CollectPlayers(Left);
		if (Left.IsEmpty())
		{
			FinishRun(/*bCleared=*/false, /*bFled=*/true);
		}
		return;
	}

	if (OverlappedComponent != DownPad)
	{
		return;
	}

	if (!StairsAreOpen())
	{
		const float Now = GetWorld()->GetTimeSeconds();
		if (Now >= NoticeCooldownUntil)
		{
			NoticeCooldownUntil = Now + 4.0f;
			Crawler->ShowLocalNotice(NSLOCTEXT("DCC", "TaleStairsLocked",
				"The stairs are reviewing your request. They open when this room's management is no longer alive."));
		}
		return;
	}

	TArray<ADCCCharacterBase*> Party;
	CollectParty(Party);
	if (!Party.Contains(Crawler))
	{
		Party.Add(Crawler);
	}

	// The stairs carry everyone in the party, downed included. A crawler left
	// on a floor that is about to be torn down would fall out of the world.
	for (ADCCCharacterBase* Member : Party)
	{
		if (Member->IsDowned())
		{
			Member->ReviveQuietly(0.25f);
		}
	}

	++FloorsCleared;
	PayForFloor(Party);

	// No last floor (2026-11-13): the run goes down until the party cashes out
	// at a SURFACE pad or runs out of rewrites.
	++FloorNumber;
	if (!BuildFloor())
	{
		--FloorNumber;
		return;
	}

	int32 Slot = 0;
	for (ADCCCharacterBase* Member : Party)
	{
		PlaceAtStart(Member, Slot++);
	}
}

// ---------------------------------------------------------------------------
// The run (Elyra's run.gd, cut down to what a 30-minute session needs)
// ---------------------------------------------------------------------------

void ADCCTaleFloor::BeginRun()
{
	RunMembers.Reset();
	GoldAtStart.Reset();
	SavesUsed.Reset();
	RunKills = 0;
	FloorsCleared = 0;
	NarratorSavesSpent = 0;
	RunStartedAt = GetWorld() ? GetWorld()->GetTimeSeconds() : 0.0;

	DismissStandIns();
	SpawnStandIns();
}

void ADCCTaleFloor::JoinRun(ADCCCharacterBase* Crawler)
{
	if (!Crawler || !Crawler->IsPlayerControlled())
	{
		return;
	}

	const TWeakObjectPtr<ADCCCharacterBase> Key(Crawler);
	if (!RunMembers.Contains(Key))
	{
		RunMembers.Add(Key);
		GoldAtStart.Add(Key, Crawler->Stats ? Crawler->Stats->Gold : 0);
	}
	// Every crawler gets an achievement for coming in (2026-11-16), and the first time, a starter kit to choose.
	UDCCAchievementComponent::RecordFor(Crawler, EDCCAchievementStat::EnteredWorldDungeon);
	if (DCCStarterKits::Chosen(Crawler) == 0 && Crawler->Wallet)
	{
		Crawler->Wallet->Client_OfferStarterKit();
	}
}

void ADCCTaleFloor::SpawnStandIns()
{
	UWorld* World = GetWorld();
	if (!World || !Grid)
	{
		return;
	}

	// Who is actually here. A stand-in never wears a body a player is using.
	TArray<FName> Taken;
	int32 Players = 0;
	for (FConstPlayerControllerIterator It = World->GetPlayerControllerIterator(); It; ++It)
	{
		const APlayerController* PC = It->Get();
		const ADCCCharacterBase* Pawn = PC ? Cast<ADCCCharacterBase>(PC->GetPawn()) : nullptr;
		if (!Pawn)
		{
			continue;
		}
		++Players;
		if (Pawn->Onboarding && !Pawn->Onboarding->CrawlerId.IsNone())
		{
			Taken.Add(Pawn->Onboarding->CrawlerId);
		}
	}

	struct FSeat
	{
		const TCHAR* Id;
		UClass* Class;
	};
	const FSeat Roster[] = {
		{ TEXT("CJ"), ADCCAlly_HockeyWings::StaticClass() },
		{ TEXT("Scott"), ADCCAlly_OfficeAirplane::StaticClass() },
		{ TEXT("Marcel"), ADCCAlly_DiceWinged::StaticClass() },
		{ TEXT("Kyle"), ADCCAlly_ArmsOut::StaticClass() },
		{ TEXT("Vaughn"), ADCCAlly_Vaughn::StaticClass() },
	};

	int32 Needed = PartySeats - FMath::Max(1, Players);
	int32 Slot = 1;
	for (const FSeat& Seat : Roster)
	{
		if (Needed <= 0)
		{
			break;
		}
		if (Taken.Contains(FName(Seat.Id)))
		{
			continue;
		}

		const FVector At = StartLocation + FVector(FMath::Cos(Slot * PI / 3.0f), FMath::Sin(Slot * PI / 3.0f), 0.0f) * 260.0f;
		const FTransform Transform(FRotator::ZeroRotator, At);
		ADCCEnemyCharacter* Ally = World->SpawnActorDeferred<ADCCEnemyCharacter>(Seat.Class, Transform, nullptr, nullptr,
			ESpawnActorCollisionHandlingMethod::AdjustIfPossibleButAlwaysSpawn);
		if (!Ally)
		{
			continue;
		}

		// bIsAlly is per-instance (a Blueprint default never reaches a placed or
		// spawned actor), so it is set here, before BeginPlay reads it.
		Ally->bIsAlly = true;
		Ally->DisplayName = FText::Format(NSLOCTEXT("DCC", "TaleStandInName", "{0} (stand-in)"), FText::FromString(Seat.Id));
		Ally->SetChaseGrid(Grid);
		Ally->FinishSpawning(Transform);

		if (Ally->Stats)
		{
			Ally->Stats->Level = FMath::Max(Ally->Stats->Level, 2);
			Ally->Stats->RefreshDerivedStats(true);
		}
		if (UCharacterMovementComponent* Movement = Ally->GetCharacterMovement())
		{
			Movement->bUseRVOAvoidance = true;
			Movement->AvoidanceConsiderationRadius = 220.0f;
		}

		StandIns.Add(Ally);
		++Slot;
		--Needed;
	}

	if (!StandIns.IsEmpty())
	{
		Announce(this, NSLOCTEXT("DCC", "TaleStandIns",
			"Empty party seats have been filled by stand-ins. They are crawlers from a parallel booking. Do not ask them about it."));
	}
}

void ADCCTaleFloor::DismissStandIns()
{
	for (const TWeakObjectPtr<ADCCEnemyCharacter>& StandIn : StandIns)
	{
		if (StandIn.IsValid())
		{
			StandIn->Destroy();
		}
	}
	StandIns.Reset();
}

void ADCCTaleFloor::PayForFloor(const TArray<ADCCCharacterBase*>& Party)
{
	const int32 Gold = 25 * FloorNumber;
	const int32 Experience = 60 * FloorNumber;

	for (ADCCCharacterBase* Member : Party)
	{
		if (!Member || !Member->Stats)
		{
			continue;
		}

		// A breather on the stairs: Elyra's supply cache, without the cache.
		Member->Stats->Heal(FMath::Max(1, FMath::RoundToInt(Member->Stats->MaxHitPoints * 0.3f)));
		Member->Stats->RestoreMana(FMath::Max(1, FMath::RoundToInt(Member->Stats->MaxManaPoints * 0.3f)));

		if (Member->IsPlayerControlled())
		{
			Member->Stats->AddGold(Gold);
			Member->Stats->AddExperience(Experience);

			// The reward ladder: a box whose grade climbs with depth.
			if (Member->Inventory)
			{
				Member->Inventory->GrantLootBox(TierForFloor(FloorNumber), 1);
			}
			DCCProfile::RecordBest(Member->GetPlayerState(), TEXT("Crawler"), TEXT("BestDepth"), FloorNumber);
		}

		// Skills grow by being used: everything used on this floor rolls to
		// advance now (and anything tried untrained may be learned), per the
		// books' end-of-floor advancement.
		if (Member->Skills)
		{
			Member->Skills->RunSkillAdvancement();
		}
	}

	Announce(this, FText::Format(NSLOCTEXT("DCC", "TaleFloorPaid",
		"Floor {0} cleared. Each crawler gets a {3} box, {1} gold and {2} experience, and every skill used down here rolls to improve. Floor {4} will be worse. Considerably."),
		FText::AsNumber(FloorNumber), FText::AsNumber(Gold), FText::AsNumber(Experience),
		StaticEnum<EDCCLootBoxTier>()->GetDisplayNameTextByValue(static_cast<int64>(TierForFloor(FloorNumber))),
		FText::AsNumber(FloorNumber + 1)));
}

void ADCCTaleFloor::FinishRun(bool bCleared, bool bFled)
{
	const double Minutes = GetWorld() ? (GetWorld()->GetTimeSeconds() - RunStartedAt) / 60.0 : 0.0;

	for (const TWeakObjectPtr<ADCCCharacterBase>& MemberPtr : RunMembers)
	{
		ADCCCharacterBase* Member = MemberPtr.Get();
		if (!Member || !Member->Stats)
		{
			continue;
		}

		// Cashing out alive pays a bonus per floor cleared. Falling does not:
		// that is the push-your-luck half of going deeper.
		if (bFled && FloorsCleared > 0)
		{
			Member->Stats->AddGold(20 * FloorsCleared);
			if (Member->Favor)
			{
				Member->Favor->AwardFavor(FMath::Min(10, FloorsCleared), EDCCFavorEvent::DirectorCall,
					NSLOCTEXT("DCC", "FavorTaleCashOut", "Knew when to stop. The audience respects it, grudgingly."));
			}
		}
		DCCProfile::Add(Member->GetPlayerState(), TEXT("Crawler"), TEXT("Runs"), 1.0f);

		const int32* Before = GoldAtStart.Find(MemberPtr);
		const int32 Earned = Member->Stats->Gold - (Before ? *Before : Member->Stats->Gold);
		const int32* Used = SavesUsed.Find(MemberPtr);

		if (!Member->Onboarding)
		{
			continue;
		}

		FDCCSystemWindowPayload Window;
		Window.Header = NSLOCTEXT("DCC", "TaleResultsHeader", "WORLD DUNGEON");
		Window.Style = bFled ? EDCCSystemWindowStyle::Reward : EDCCSystemWindowStyle::Warning;
		Window.Title = bFled
			? NSLOCTEXT("DCC", "TaleResultsCashOut", "Cashed Out")
			: NSLOCTEXT("DCC", "TaleResultsLost", "Run Ended");
		Window.Body = FText::Format(NSLOCTEXT("DCC", "TaleResultsBody",
			"Floors cleared: {0}  (your deepest ever: {1})\nParty kills: {2}\nGold earned: {3}\nRewrites used: {4} of {5}\nTime: {6} min\n\n{7}"),
			FText::AsNumber(FloorsCleared),
			FText::AsNumber(FMath::RoundToInt(DCCProfile::Get(Member->GetPlayerState(), TEXT("Crawler"), TEXT("BestDepth")))),
			FText::AsNumber(RunKills),
			FText::AsNumber(FMath::Max(0, Earned)), FText::AsNumber(Used ? *Used : 0), FText::AsNumber(NarratorSavesPerRun),
			FText::AsNumber(FMath::RoundToInt(Minutes)),
			bFled
				? NSLOCTEXT("DCC", "TaleResultsCashLine", "Cash-out bonus paid. Your boxes are waiting at the kiosk (O). Departures will build you a fresh dungeon whenever you like.")
				: NSLOCTEXT("DCC", "TaleResultsLostLine", "You keep everything you were paid. The archway will build you a new dungeon whenever you like."));
		Member->Onboarding->ShowWindow(Window);
	}

	DismissStandIns();
	ClearBuilt();
	bLayoutLive = false;
	++LayoutVersion;
	ForceNetUpdate();
	bRunComplete = true;
	RunMembers.Reset();

	DCCSave::SaveAll(GetWorld(), TEXT("Run finished"));
}

void ADCCTaleFloor::HandleEnemyDied(ADCCCharacterBase* Character)
{
	++RunKills;
}

bool ADCCTaleFloor::InterceptDeath(ADCCCharacterBase* Crawler)
{
	if (!Crawler || !Crawler->HasAuthority() || !Crawler->IsPlayerControlled() || !Crawler->IsDowned())
	{
		return false;
	}

	ADCCTaleFloor* Floor = nullptr;
	for (TActorIterator<ADCCTaleFloor> It(Crawler->GetWorld()); It; ++It)
	{
		Floor = *It;
		break;
	}
	if (!Floor || !Floor->bBuilt || !Floor->Contains(Crawler->GetActorLocation()))
	{
		return false;
	}

	Floor->JoinRun(Crawler);
	int32& Used = Floor->SavesUsed.FindOrAdd(Crawler);

	if (Used < NarratorSavesPerRun)
	{
		++Used;
		++Floor->NarratorSavesSpent;
		Crawler->ReviveQuietly(0.5f);

		const int32 Left = NarratorSavesPerRun - Used;
		Announce(Floor, FText::Format(NSLOCTEXT("DCC", "TaleNarratorSave",
			"The System has reviewed {0}'s death and found it premature for ratings purposes. The last ten seconds have been rewritten. Rewrites left this run: {1}."),
			NameOf(Crawler), FText::AsNumber(Left)));

		// Being saved is watchable. Being saved twice is a storyline.
		if (Crawler->Favor)
		{
			Crawler->Favor->AwardFavor(2, EDCCFavorEvent::NarrowSurvival,
				NSLOCTEXT("DCC", "FavorNarratorSave", "Came back from the dead, on air."));
		}
		return true;
	}

	// Out of rewrites: home, alive, with everything. The run carries on without them.
	Crawler->ReviveQuietly(0.25f);
	Announce(Floor, FText::Format(NSLOCTEXT("DCC", "TaleExpelled",
		"{0} has used up their rewrites and has been returned to the lobby, with their belongings, which is more than the lobby usually does."),
		NameOf(Crawler)));
	Floor->TeleportHome(Crawler);

	TArray<ADCCCharacterBase*> Left;
	Floor->CollectPlayers(Left);
	if (Left.IsEmpty())
	{
		Floor->FinishRun(/*bCleared=*/false, /*bFled=*/false);
	}
	return true;
}

void ADCCTaleFloor::ScheduleQuickPlay(ADCCCharacterBase* Crawler)
{
	// Quick play on: the cvar, or arriving from the arcade lobby (?Hub=1), which
	// skips the bedroom and city and starts at the gates.
	const AGameModeBase* Mode = Crawler ? Crawler->GetWorld()->GetAuthGameMode() : nullptr;
	const bool bFromArcade = Mode && UGameplayStatics::HasOption(Mode->OptionsString, TEXT("Hub"));
	if (!Crawler || !Crawler->HasAuthority() || (CVarQuickPlay.GetValueOnGameThread() == 0 && !bFromArcade)
		|| DCCArcade::IsArcadeWorld(Crawler->GetWorld()))
	{
		return;
	}

	// A beat after possession, so the game mode has restored the crawler's
	// identity and the onboarding settings have finished their BeginPlay.
	TWeakObjectPtr<ADCCCharacterBase> Weak(Crawler);
	FTimerHandle Handle;
	Crawler->GetWorldTimerManager().SetTimer(Handle, FTimerDelegate::CreateLambda([Weak]()
	{
		if (Weak.IsValid() && Weak->IsPlayerControlled())
		{
			QuickPlay(Weak.Get());
		}
	}), 1.5f, false);
}

void ADCCTaleFloor::QuickPlay(ADCCCharacterBase* Crawler)
{
	if (!Crawler || !Crawler->HasAuthority())
	{
		return;
	}

	if (Crawler->Onboarding)
	{
		// Complete issues the whole HUD and lets the plaza arch send them back down later.
		Crawler->Onboarding->AdvanceTo(EDCCOnboardingStage::Complete);
		Crawler->Onboarding->UnlockFeatures(DCCHud::AllFeatures);
	}

	// Quick play starts in the hub (shops, then the Departures gate), not on
	// floor 1. Someone already mid-run is left where they are.
	ADCCTaleFloor* Floor = GetOrCreate(Crawler->GetWorld());
	if (Floor && Floor->bBuilt && !Floor->bRunComplete && Floor->Contains(Crawler->GetActorLocation()))
	{
		return;
	}
	ADCCHub::SendToHub(Crawler);
}

void ADCCTaleFloor::EnterEveryone(UWorld* World)
{
	if (!World || World->GetNetMode() == NM_Client)
	{
		return;
	}
	ADCCTaleFloor* Floor = GetOrCreate(World);
	if (!Floor)
	{
		return;
	}
	for (FConstPlayerControllerIterator It = World->GetPlayerControllerIterator(); It; ++It)
	{
		ADCCCharacterBase* Crawler = It->Get() ? Cast<ADCCCharacterBase>(It->Get()->GetPawn()) : nullptr;
		if (!Crawler)
		{
			continue;
		}
		if (Crawler->Onboarding)
		{
			Crawler->Onboarding->AdvanceTo(EDCCOnboardingStage::Complete);
			Crawler->Onboarding->UnlockFeatures(DCCHud::AllFeatures);
		}
		if (Floor->bBuilt && !Floor->bRunComplete && Floor->Contains(Crawler->GetActorLocation()))
		{
			continue;
		}
		Floor->Enter(Crawler, ADCCHub::Find(World));
	}
}

void ADCCTaleFloor::QuickPlayEveryone(UWorld* World)
{
	if (!World || World->GetNetMode() == NM_Client)
	{
		return;
	}

	// Everyone connected, not just the host: this is how the table starts a run
	// together, and how a latecomer is brought into one already under way.
	for (FConstPlayerControllerIterator It = World->GetPlayerControllerIterator(); It; ++It)
	{
		if (APlayerController* PC = It->Get())
		{
			QuickPlay(Cast<ADCCCharacterBase>(PC->GetPawn()));
		}
	}
}

bool ADCCTaleFloor::IsRunLive(const UWorld* World)
{
	if (!World)
	{
		return false;
	}
	for (TActorIterator<ADCCTaleFloor> It(const_cast<UWorld*>(World)); It; ++It)
	{
		return It->bLayoutLive;
	}
	return false;
}

static void DCCEnterTaleCommand()
{
	if (!GEngine)
	{
		return;
	}

	for (const FWorldContext& Context : GEngine->GetWorldContexts())
	{
		UWorld* World = Context.World();
		if (!World || World->GetNetMode() == NM_Client)
		{
			continue;
		}
		if (Context.WorldType != EWorldType::PIE && Context.WorldType != EWorldType::Game)
		{
			continue;
		}

		ADCCCharacterBase* Crawler = nullptr;
		for (FConstPlayerControllerIterator It = World->GetPlayerControllerIterator(); It; ++It)
		{
			if (APlayerController* PC = It->Get())
			{
				Crawler = Cast<ADCCCharacterBase>(PC->GetPawn());
				if (Crawler)
				{
					break;
				}
			}
		}

		if (ADCCTaleFloor* Floor = ADCCTaleFloor::GetOrCreate(World))
		{
			Floor->Enter(Crawler, nullptr);
		}
		return;
	}
}

static FAutoConsoleCommand GEnterTaleCmd(
	TEXT("DCC.EnterTale"),
	TEXT("DCC.EnterTale [delay]: build the generated dungeon and teleport the player into the start room."),
	FConsoleCommandWithArgsDelegate::CreateLambda([](const TArray<FString>& Args)
	{
		const float Delay = Args.Num() > 0 ? FCString::Atof(*Args[0]) : 0.0f;
		if (Delay <= 0.0f)
		{
			DCCEnterTaleCommand();
			return;
		}
		// A one-shot ticker so it can be scripted from -ExecCmds before the player exists.
		FTSTicker::GetCoreTicker().AddTicker(FTickerDelegate::CreateLambda([](float) { DCCEnterTaleCommand(); return false; }), Delay);
	}));

static void DCCQuickPlayCommand()
{
	if (!GEngine)
	{
		return;
	}

	for (const FWorldContext& Context : GEngine->GetWorldContexts())
	{
		UWorld* World = Context.World();
		if (!World || World->GetNetMode() == NM_Client)
		{
			continue;
		}
		if (Context.WorldType != EWorldType::PIE && Context.WorldType != EWorldType::Game)
		{
			continue;
		}

		ADCCTaleFloor::QuickPlayEveryone(World);
		return;
	}
}

static FAutoConsoleCommand GQuickPlayCmd(
	TEXT("DCC.QuickPlay"),
	TEXT("Skip onboarding for every connected crawler: full HUD, and straight to Concourse Zero (the hub: shops, then the Departures gate)."),
	FConsoleCommandDelegate::CreateStatic(&DCCQuickPlayCommand));
