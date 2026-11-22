#include "Arcade/DCCArcadeArena.h"
#include "Arcade/DCCSummons.h"
#include "Arcade/DCCSpray.h"
#include "NavigationSystem.h"
#include "NavMesh/NavMeshBoundsVolume.h"
#include "Components/BrushComponent.h"
#include "PhysicsEngine/BodySetup.h"
#include "TimerManager.h"
#include "PhysicsEngine/BodySetup.h"
#include "Components/BoxComponent.h"

#include "Arcade/DCCArcade.h"
#include "Components/DirectionalLightComponent.h"
#include "Components/ExponentialHeightFogComponent.h"
#include "Components/PointLightComponent.h"
#include "Components/SkyAtmosphereComponent.h"
#include "Components/SkyLightComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Components/TextRenderComponent.h"
#include "Engine/DirectionalLight.h"
#include "Engine/ExponentialHeightFog.h"
#include "Engine/PointLight.h"
#include "Engine/SkyLight.h"
#include "Engine/PostProcessVolume.h"
#include "Engine/StaticMesh.h"
#include "Engine/StaticMeshActor.h"
#include "Engine/TextRenderActor.h"
#include "Engine/World.h"
#include "EngineUtils.h"
#include "Characters/DCCCharacterBase.h"
#include "Components/SkeletalMeshComponent.h"
#include "Misc/Paths.h"
#include "Materials/MaterialInterface.h"
#include "GameFramework/PlayerController.h"
#include "GameFramework/PlayerStart.h"
#include "GameFramework/GameModeBase.h"
#include "Materials/MaterialInstanceDynamic.h"
#include "Net/UnrealNetwork.h"
#include "UObject/ConstructorHelpers.h"
#include "Dialogue/DCCDialogueNPC.h"
#include "Arcade/DCCRallyThug.h"
#include "Vehicles/DCCRover.h"

ADCCArcadeArena::ADCCArcadeArena()
{
	PrimaryActorTick.bCanEverTick = true;
	PrimaryActorTick.TickInterval = 0.0f;
	bReplicates = true;
	bAlwaysRelevant = true;
	SetReplicatingMovement(false);
	SetRootComponent(CreateDefaultSubobject<USceneComponent>(TEXT("Root")));

	static ConstructorHelpers::FObjectFinder<UStaticMesh> Cube(TEXT("/Engine/BasicShapes/Cube.Cube"));
	CubeMesh = Cube.Object;
	static ConstructorHelpers::FObjectFinder<UMaterialInterface> Mat(TEXT("/Engine/BasicShapes/BasicShapeMaterial.BasicShapeMaterial"));
	ShapeMaterial = Mat.Object;
}

void ADCCArcadeArena::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);
	DOREPLIFETIME(ADCCArcadeArena, Title);
	DOREPLIFETIME(ADCCArcadeArena, StatusLines);
	DOREPLIFETIME(ADCCArcadeArena, Accent);
	DOREPLIFETIME(ADCCArcadeArena, bRoundOver);
	DOREPLIFETIME(ADCCArcadeArena, SecondsToDecide);
	DOREPLIFETIME(ADCCArcadeArena, VotesAgain);
	DOREPLIFETIME(ADCCArcadeArena, VotesDen);
	DOREPLIFETIME(ADCCArcadeArena, VotersExpected);
	DOREPLIFETIME(ADCCArcadeArena, BossName);
	DOREPLIFETIME(ADCCArcadeArena, BossHealth);
}

void ADCCArcadeArena::BeginPlay()
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
	SetupSky();
	BuildLocal();
	if (HasAuthority())
	{
		ServerBegin();
		DCCSpray::Restore(this); // the host keeps everyone's tags between sessions
		if (WantsNavmesh())
		{
			GetWorldTimerManager().SetTimer(NavTimer, this, &ADCCArcadeArena::UpdateNavBounds, 4.0f, true, 3.0f);
		}
	}
}

void ADCCArcadeArena::UpdateNavBounds()
{
	// The geometry settles in the first half minute (the casino floor, the Gate dungeon once its seed lands).
	if (++NavChecks >= 10)
	{
		GetWorldTimerManager().ClearTimer(NavTimer);
	}
	UWorld* World = GetWorld();
	UNavigationSystemV1* Nav = World ? FNavigationSystem::GetCurrent<UNavigationSystemV1>(World) : nullptr;
	if (!Nav)
	{
		if (NavChecks == 1)
		{
			UE_LOG(LogTemp, Warning, TEXT("[DCC Nav] no navigation system in %s: AI keeps to grids and straight lines."), *GetClass()->GetName());
		}
		return;
	}
	// Everything this arena built, except the sky and ground planes far bigger than anywhere anyone walks.
	FBox Box(ForceInit);
	for (const TObjectPtr<AActor>& Piece : LocalActors)
	{
		if (!Piece)
		{
			continue;
		}
		const FBox Bounds = Piece->GetComponentsBoundingBox(true);
		if (Bounds.IsValid && Bounds.GetExtent().GetMax() < 30000.0f)
		{
			Box += Bounds;
		}
	}
	if (!Box.IsValid)
	{
		return;
	}
	Box = Box.ExpandBy(FVector(400.0f, 400.0f, 600.0f));
	if (NavBox.IsValid && NavBox.IsInsideOrOn(Box.Min) && NavBox.IsInsideOrOn(Box.Max))
	{
		return; // already covered
	}
	NavBox = NavBox.IsValid ? NavBox + Box : Box;

	for (const TObjectPtr<AActor>& Old : NavVolumes)
	{
		if (ANavMeshBoundsVolume* Volume = Cast<ANavMeshBoundsVolume>(Old.Get()))
		{
			Nav->OnNavigationBoundsRemoved(Volume);
			Volume->Destroy();
		}
	}
	NavVolumes.Reset();

	// A bounds volume's size normally comes from a brush built in the editor. At runtime there is none, so it gets a
	// box collision shape instead, which is what the brush component measures when it has no brush.
	FActorSpawnParameters Params;
	Params.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
	ANavMeshBoundsVolume* Volume = World->SpawnActor<ANavMeshBoundsVolume>(NavBox.GetCenter(), FRotator::ZeroRotator, Params);
	UBrushComponent* Brush = Volume ? Volume->GetBrushComponent() : nullptr;
	if (!Brush)
	{
		return;
	}
	UBodySetup* Shape = NewObject<UBodySetup>(Brush);
	const FVector Size = NavBox.GetSize();
	Shape->AggGeom.BoxElems.Add(FKBoxElem(Size.X, Size.Y, Size.Z));
	Brush->BrushBodySetup = Shape;
	Brush->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	Brush->UpdateBounds();
	NavVolumes.Add(Volume);
	Nav->OnNavigationBoundsUpdated(Volume);
	UE_LOG(LogTemp, Log, TEXT("[DCC Nav] %s: navmesh bounds %s (%.0f x %.0f x %.0f)"), *GetClass()->GetName(),
		*NavBox.GetCenter().ToCompactString(), Size.X, Size.Y, Size.Z);
}

void ADCCArcadeArena::Tick(float DeltaSeconds)
{
	Super::Tick(DeltaSeconds);
	// Hands start empty in the arcade games (Zoe, 2026-11-13). Gear held in the
	// main game stays equipped in the save; it just is not drawn here, where it
	// does nothing. Cosmetics are not on hand sockets, so they still show.
	for (TActorIterator<ADCCCharacterBase> It(GetWorld()); It && !ShowsHeldGear(); ++It)
	{
		if (USkeletalMeshComponent* Body = It->GetMesh())
		{
			for (USceneComponent* Child : Body->GetAttachChildren())
			{
				const FName Socket = Child ? Child->GetAttachSocketName() : NAME_None;
				if (Socket == TEXT("hand_rSocket") || Socket == TEXT("hand_lSocket"))
				{
					if (!Child->bHiddenInGame)
					{
						Child->SetHiddenInGame(true, true);
					}
				}
			}
		}
	}
	ApplyCharacterShade();
	ClientTick(DeltaSeconds);
	if (!HasAuthority())
	{
		return;
	}
	ServerTick(DeltaSeconds);
	DCCSummons::TickArena(this); // the allies summoned in the Den follow their crawler into the next game

	// Minigames respawn the dead: nothing else in the project does, and a
	// player stuck with no body for the rest of a round is not a minigame.
	{
		TArray<APlayerController*> Players;
		GetPlayers(Players);
		for (APlayerController* PC : Players)
		{
			if (PC->GetPawn())
			{
				NoPawnSince.Remove(PC);
				continue;
			}
			const double Since = NoPawnSince.FindOrAdd(PC, ServerTime());
			if (ServerTime() - Since > 6.0)
			{
				NoPawnSince.Remove(PC);
				if (AGameModeBase* Mode = GetWorld()->GetAuthGameMode())
				{
					Mode->RestartPlayer(PC);
					OnRespawned(PC);
				}
			}
		}
	}

	if (bRoundOver)
	{
		TickRoundOver();
	}
	else if (ReturnAt > 0.0 && !bTravelling && ServerTime() >= ReturnAt)
	{
		bTravelling = true;
		DCCArcade::Travel(GetWorld(), TEXT("Lobby"));
	}
	else if (ReturnAt > 0.0 && !bTravelling)
	{
		const int32 Left = FMath::CeilToInt(ReturnAt - ServerTime());
		const FString Line = FString::Printf(TEXT("Back to the lobby in %d s"), Left);
		if (StatusLines.IsEmpty() || !StatusLines.Last().StartsWith(TEXT("Back to the lobby")))
		{
			StatusLines.Add(Line);
		}
		else
		{
			StatusLines.Last() = Line;
		}
	}
}

void ADCCArcadeArena::ReturnToLobbyIn(float Seconds)
{
	if (ReturnAt >= 0.0)
	{
		return;
	}
	if (GetGameId().IsNone())
	{
		ReturnAt = ServerTime() + Seconds;
		return;
	}
	ReturnAt = ServerTime() + FMath::Max(20.0f, Seconds);
	bRoundOver = true;
	for (TActorIterator<ADCCRover> It(GetWorld()); It; ++It)
	{
		if (It->IsOccupied())
		{
			It->ExitRover();
		}
	}
	RoundVotes.Reset();
	TickRoundOver();
}

void ADCCArcadeArena::CastVote(APlayerController* PC, bool bPlayAgain)
{
	if (bRoundOver && PC && !bTravelling)
	{
		RoundVotes.Add(PC, bPlayAgain);
		TickRoundOver();
	}
}

void ADCCArcadeArena::TickRoundOver()
{
	if (bTravelling)
	{
		return;
	}
	TArray<APlayerController*> Players;
	GetPlayers(Players);
	VotersExpected = Players.Num();
	VotesAgain = 0;
	VotesDen = 0;
	for (APlayerController* PC : Players)
	{
		if (const bool* Vote = RoundVotes.Find(PC))
		{
			(*Vote ? VotesAgain : VotesDen)++;
		}
	}
	SecondsToDecide = FMath::Max(0, FMath::CeilToInt(ReturnAt - ServerTime()));

	const bool bEveryoneVoted = VotersExpected > 0 && VotesAgain + VotesDen >= VotersExpected;
	if (bEveryoneVoted || ServerTime() >= ReturnAt)
	{
		bTravelling = true;
		const bool bAgain = VotesAgain > VotesDen;
		DCCArcade::Travel(GetWorld(), bAgain ? GetGameId() : FName(TEXT("Lobby")));
	}
}

double ADCCArcadeArena::ServerTime() const
{
	return GetWorld() ? GetWorld()->GetTimeSeconds() : 0.0;
}

void ADCCArcadeArena::GetStartTransforms(TArray<FTransform>& Out) const
{
	for (int32 i = 0; i < 8; ++i)
	{
		Out.Add(FTransform(FRotator::ZeroRotator, GetActorLocation() + FVector(0.0f, (i - 3.5f) * 250.0f, 120.0f)));
	}
}

void ADCCArcadeArena::SpawnStarts()
{
	UWorld* World = GetWorld();
	if (!World)
	{
		return;
	}
	PrepareServer();
	TArray<FTransform> Starts;
	GetStartTransforms(Starts);
	// /Engine/Maps/Entry ships with a PlayerStart of its own near the origin. With
	// nobody in the world yet every start is equally "far from other crawlers", so
	// that one won and every arcade game spawned players at it - inside the
	// mountain, in Fourteener (found 2026-11-13). Only the arena's starts count.
	for (TActorIterator<APlayerStart> It(World); It; ++It)
	{
		It->Destroy();
	}

	FActorSpawnParameters Params;
	Params.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
	int32 Made = 0;
	for (const FTransform& Start : Starts)
	{
		Made += World->SpawnActor<APlayerStart>(APlayerStart::StaticClass(), Start, Params) ? 1 : 0;
	}
	UE_LOG(LogTemp, Verbose, TEXT("[DCC Arcade] %s placed %d of %d player starts (first at %s)."), *GetClass()->GetName(), Made, Starts.Num(),
		Starts.IsEmpty() ? TEXT("-") : *Starts[0].GetLocation().ToCompactString());
}

void ADCCArcadeArena::GetPlayers(TArray<APlayerController*>& Out) const
{
	Out.Reset();
	if (const UWorld* World = GetWorld())
	{
		for (FConstPlayerControllerIterator It = World->GetPlayerControllerIterator(); It; ++It)
		{
			if (APlayerController* PC = It->Get())
			{
				Out.Add(PC);
			}
		}
	}
}

void ADCCArcadeArena::SetupSky()
{
	UWorld* World = GetWorld();
	if (!World)
	{
		return;
	}
	const FSky Sky = GetSky();
	FActorSpawnParameters Params;
	Params.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;

	// The empty map has nothing: no sun, no sky, no fog. Each game brings its own.
	if (ADirectionalLight* Sun = World->SpawnActor<ADirectionalLight>(FVector::ZeroVector, FRotator(Sky.SunPitch, 35.0f, 0.0f), Params))
	{
		Sun->SetReplicates(false);
		if (UDirectionalLightComponent* Light = Cast<UDirectionalLightComponent>(Sun->GetLightComponent()))
		{
			Light->SetMobility(EComponentMobility::Movable);
			// Shading pass (2026-11-13, Zoe: "characters look faded out under harsh
			// light"): the sun carries more of the light and the sky fill less, so
			// faces and folds get real shadow instead of flat, even brightness.
			Light->SetIntensity(Sky.SunIntensity * 1.5f);
			Light->SetLightColor(Sky.SunColour);
			Light->SetAtmosphereSunLight(true);
		}
		LocalActors.Add(Sun);
	}
	if (AActor* Atmosphere = World->SpawnActor<ASkyAtmosphere>(ASkyAtmosphere::StaticClass(), FTransform::Identity, Params))
	{
		Atmosphere->SetReplicates(false);
		LocalActors.Add(Atmosphere);
	}
	const FTransform SkyTransform(FRotator::ZeroRotator, FVector(0, 0, 500));
	if (ASkyLight* SkyLight = World->SpawnActorDeferred<ASkyLight>(ASkyLight::StaticClass(), SkyTransform))
	{
		SkyLight->SetReplicates(false);
		if (USkyLightComponent* Light = SkyLight->GetLightComponent())
		{
			Light->SetMobility(EComponentMobility::Movable);
			Light->bRealTimeCapture = true;
			Light->SetIntensity(Sky.SkyIntensity * 0.55f);
		}
		SkyLight->FinishSpawning(SkyTransform);
		LocalActors.Add(SkyLight);
	}
	if (AExponentialHeightFog* Fog = World->SpawnActor<AExponentialHeightFog>(FVector(0, 0, 0), FRotator::ZeroRotator, Params))
	{
		Fog->SetReplicates(false);
		if (UExponentialHeightFogComponent* Component = Fog->GetComponent())
		{
			Component->SetFogDensity(Sky.FogDensity * 0.3f); // the old haze bleached everything past a few metres
			Component->SetFogInscatteringColor(Sky.FogColour);
			if (Sky.FogStart > 0.0f)
			{
				// StartDistance is ignored while volumetric fog is on, so the band would bleach the climb.
				Component->SetVolumetricFog(false);
				Component->SetStartDistance(Sky.FogStart);
			}
		}
		LocalActors.Add(Fog);
	}

	// One unbound post-process volume for the whole arena: a touch darker and
	// more contrasty than auto exposure picks, much less bloom (the glare), a
	// little more colour. Values are gentle on purpose; tune here, not per game.
	if (APostProcessVolume* Grade = World->SpawnActor<APostProcessVolume>(FVector::ZeroVector, FRotator::ZeroRotator, Params))
	{
		Grade->SetReplicates(false);
		Grade->bUnbound = true;
		FPostProcessSettings& S = Grade->Settings;
		S.bOverride_AutoExposureBias = true;
		S.AutoExposureBias = -0.4f;
		S.bOverride_AutoExposureSpeedUp = true;
		S.AutoExposureSpeedUp = 1.5f;
		S.bOverride_AutoExposureSpeedDown = true;
		S.AutoExposureSpeedDown = 1.0f;
		S.bOverride_BloomIntensity = true;
		S.BloomIntensity = 0.25f;
		S.bOverride_ColorContrast = true;
		S.ColorContrast = FVector4(1.08f, 1.08f, 1.08f, 1.0f);
		S.bOverride_ColorSaturation = true;
		S.ColorSaturation = FVector4(1.1f, 1.1f, 1.1f, 1.0f);
		S.bOverride_VignetteIntensity = true;
		S.VignetteIntensity = 0.3f;
		LocalActors.Add(Grade);
	}
}

AActor* ADCCArcadeArena::Block(const FVector& Center, const FVector& Size, const FLinearColor& Colour, float Yaw, bool bCollide)
{
	UWorld* World = GetWorld();
	if (!World || !CubeMesh)
	{
		return nullptr;
	}
	const FBoxSphereBounds Bounds = CubeMesh->GetBounds();
	const FVector MeshSize = Bounds.BoxExtent * 2.0f;
	const FVector Scale(Size.X / MeshSize.X, Size.Y / MeshSize.Y, Size.Z / MeshSize.Z);
	const FRotator Rotation(0.0f, Yaw, 0.0f);
	const FTransform Transform(Rotation, Center - Rotation.RotateVector(Bounds.Origin * Scale), Scale);

	AStaticMeshActor* Actor = World->SpawnActorDeferred<AStaticMeshActor>(AStaticMeshActor::StaticClass(), Transform);
	if (!Actor)
	{
		return nullptr;
	}
	Actor->SetReplicates(false);
	if (UStaticMeshComponent* Mesh = Actor->GetStaticMeshComponent())
	{
		Mesh->SetMobility(EComponentMobility::Movable);
		Mesh->SetStaticMesh(CubeMesh);
		Mesh->SetCollisionProfileName(bCollide ? TEXT("BlockAll") : TEXT("NoCollision"));
		Mesh->SetCastShadow(false);
		if (ShapeMaterial)
		{
			UMaterialInstanceDynamic* Mid = UMaterialInstanceDynamic::Create(ShapeMaterial, Mesh);
			Mid->SetVectorParameterValue(TEXT("Color"), Colour);
			Mid->SetVectorParameterValue(TEXT("BaseColor"), Colour);
			Mesh->SetMaterial(0, Mid);
		}
	}
	Actor->FinishSpawning(Transform);
	(BlockSink ? *BlockSink : LocalActors).Add(Actor);
	return Actor;
}

AActor* ADCCArcadeArena::Prop(const TCHAR* MeshPath, const FVector& Base, float Yaw, const FVector& Fit, bool bCollide,
	bool bStretch, float Scale)
{
	UWorld* World = GetWorld();
	const FString Path(MeshPath);
	UStaticMesh* Mesh = LoadObject<UStaticMesh>(nullptr, *(Path + TEXT(".") + FPaths::GetBaseFilename(Path)));
	if (!World || !Mesh)
	{
		UE_LOG(LogTemp, Warning, TEXT("[DCC Arcade] prop mesh not found: %s"), MeshPath);
		return nullptr;
	}
	const FBox Bounds = Mesh->GetBoundingBox();
	const FVector Size = Bounds.GetSize().ComponentMax(FVector(1.0f));
	FVector S(Scale);
	if (bStretch)
	{
		S = FVector(Fit.X > 0 ? Fit.X / Size.X : 1.0f, Fit.Y > 0 ? Fit.Y / Size.Y : 1.0f, Fit.Z > 0 ? Fit.Z / Size.Z : 1.0f) * Scale;
	}
	else if (!Fit.IsNearlyZero())
	{
		float Uniform = TNumericLimits<float>::Max();
		if (Fit.X > 0) { Uniform = FMath::Min(Uniform, Fit.X / Size.X); }
		if (Fit.Y > 0) { Uniform = FMath::Min(Uniform, Fit.Y / Size.Y); }
		if (Fit.Z > 0) { Uniform = FMath::Min(Uniform, Fit.Z / Size.Z); }
		S = FVector(Uniform * Scale);
	}
	const FRotator Rotation(0.0f, Yaw, 0.0f);
	const FVector Center = Bounds.GetCenter() * S;
	const FVector Location = Base - Rotation.RotateVector(FVector(Center.X, Center.Y, 0.0f)) - FVector(0, 0, Bounds.Min.Z * S.Z);
	const FTransform Transform(Rotation, Location, S);

	AStaticMeshActor* Actor = World->SpawnActorDeferred<AStaticMeshActor>(AStaticMeshActor::StaticClass(), Transform);
	if (!Actor)
	{
		return nullptr;
	}
	Actor->SetReplicates(false);
	if (UStaticMeshComponent* Component = Actor->GetStaticMeshComponent())
	{
		Component->SetMobility(EComponentMobility::Movable);
		Component->SetStaticMesh(Mesh);
		Component->SetCollisionProfileName(bCollide ? TEXT("BlockAll") : TEXT("NoCollision"));
		Component->SetCanEverAffectNavigation(bCollide);
		Component->SetCastShadow(true);
	}
	Actor->FinishSpawning(Transform);
	// Many pack meshes ship with no collision shape, so "BlockAll" on them blocks nothing and players walk through
	// rocks and carts (2026-11-15). Those get an invisible box the size of the mesh.
	if (bCollide && (!Mesh->GetBodySetup() || Mesh->GetBodySetup()->AggGeom.GetElementCount() == 0))
	{
		if (UStaticMeshComponent* Component = Actor->GetStaticMeshComponent())
		{
			UBoxComponent* Box = NewObject<UBoxComponent>(Actor, TEXT("SolidBox"));
			Box->SetupAttachment(Component);
			Box->SetRelativeLocation(Bounds.GetCenter());
			Box->SetBoxExtent(Bounds.GetExtent() * 0.92f, false);
			Box->SetCollisionProfileName(TEXT("BlockAll"));
			Box->SetCanEverAffectNavigation(false);
			Box->RegisterComponent();
			Actor->AddInstanceComponent(Box);
		}
		static TSet<FString> Logged;
		if (!Logged.Contains(Mesh->GetName()))
		{
			Logged.Add(Mesh->GetName());
			UE_LOG(LogTemp, Display, TEXT("[DCC Solid] %s has no collision shape; gave it a box"), *Mesh->GetName());
		}
	}
	(BlockSink ? *BlockSink : LocalActors).Add(Actor);
	return Actor;
}

void ADCCArcadeArena::Paint(AActor* Actor, const TCHAR* MaterialPath)
{
	const FString Path(MaterialPath);
	UMaterialInterface* Material = LoadObject<UMaterialInterface>(nullptr, *(Path + TEXT(".") + FPaths::GetBaseFilename(Path)));
	UStaticMeshComponent* Component = Actor ? Actor->FindComponentByClass<UStaticMeshComponent>() : nullptr;
	if (Material && Component)
	{
		Component->SetMaterial(0, Material);
	}
}

void ADCCArcadeArena::TiledBlock(const FVector& Center, const FVector& Size, const FLinearColor& Colour, const TCHAR* MaterialPath,
	float Patch, bool bCollide, bool bSplitZ)
{
	const float Step = FMath::Max(50.0f, Patch);
	const int32 NX = FMath::Max(1, FMath::RoundToInt(Size.X / Step));
	const int32 NY = FMath::Max(1, FMath::RoundToInt(Size.Y / Step));
	const int32 NZ = bSplitZ ? FMath::Max(1, FMath::RoundToInt(Size.Z / Step)) : 1;
	const FVector Piece(Size.X / NX, Size.Y / NY, Size.Z / NZ);
	const FVector Corner = Center - Size * 0.5f;
	for (int32 X = 0; X < NX; ++X)
	{
		for (int32 Y = 0; Y < NY; ++Y)
		{
			for (int32 Z = 0; Z < NZ; ++Z)
			{
				const FVector At = Corner + FVector((X + 0.5f) * Piece.X, (Y + 0.5f) * Piece.Y, (Z + 0.5f) * Piece.Z);
				Paint(Block(At, Piece, Colour, 0.0f, bCollide), MaterialPath);
			}
		}
	}
}

AActor* ADCCArcadeArena::Shape(const TCHAR* ShapeName, const FVector& Center, const FVector& Size, const FLinearColor& Colour,
	const FRotator& Rotation, bool bCollide)
{
	UWorld* World = GetWorld();
	UStaticMesh* Mesh = LoadObject<UStaticMesh>(nullptr, *FString::Printf(TEXT("/Engine/BasicShapes/%s.%s"), ShapeName, ShapeName));
	if (!World || !Mesh)
	{
		return nullptr;
	}
	const FBoxSphereBounds Bounds = Mesh->GetBounds();
	const FVector MeshSize = Bounds.BoxExtent * 2.0f;
	const FVector Scale(Size.X / MeshSize.X, Size.Y / MeshSize.Y, Size.Z / MeshSize.Z);
	const FTransform Transform(Rotation, Center - Rotation.RotateVector(Bounds.Origin * Scale), Scale);

	AStaticMeshActor* Actor = World->SpawnActorDeferred<AStaticMeshActor>(AStaticMeshActor::StaticClass(), Transform);
	if (!Actor)
	{
		return nullptr;
	}
	Actor->SetReplicates(false);
	if (UStaticMeshComponent* Component = Actor->GetStaticMeshComponent())
	{
		Component->SetMobility(EComponentMobility::Movable);
		Component->SetStaticMesh(Mesh);
		Component->SetCollisionProfileName(bCollide ? TEXT("BlockAll") : TEXT("NoCollision"));
		Component->SetCanEverAffectNavigation(bCollide);
		Component->SetCastShadow(false);
		if (ShapeMaterial)
		{
			UMaterialInstanceDynamic* Mid = UMaterialInstanceDynamic::Create(ShapeMaterial, Component);
			Mid->SetVectorParameterValue(TEXT("Color"), Colour);
			Mid->SetVectorParameterValue(TEXT("BaseColor"), Colour);
			Component->SetMaterial(0, Mid);
		}
	}
	Actor->FinishSpawning(Transform);
	(BlockSink ? *BlockSink : LocalActors).Add(Actor);
	return Actor;
}

ATextRenderActor* ADCCArcadeArena::Sign(const FVector& Location, float Yaw, const FString& Text, const FColor& Colour, float Size, bool bBoard)
{
	UWorld* World = GetWorld();
	if (!World)
	{
		return nullptr;
	}
	const FTransform Transform(FRotator(0.0f, Yaw, 0.0f), Location);
	ATextRenderActor* Actor = World->SpawnActorDeferred<ATextRenderActor>(ATextRenderActor::StaticClass(), Transform);
	if (!Actor)
	{
		return nullptr;
	}
	Actor->SetReplicates(false);
	if (UTextRenderComponent* Render = Actor->GetTextRender())
	{
		Render->SetText(FText::FromString(Text));
		Render->SetTextRenderColor(Colour);
		Render->SetHorizontalAlignment(EHTA_Center);
		Render->SetVerticalAlignment(EVRTA_TextCenter);
		Render->SetWorldSize(Size);
	}
	Actor->FinishSpawning(Transform);
	(BlockSink ? *BlockSink : LocalActors).Add(Actor);
	// The board: weathered and dark, a little bigger than the text, just behind it (text faces along its own +X).
	if (bBoard)
	{
		int32 Lines = 1;
		int32 Longest = 0, Run = 0;
		for (const TCHAR C : Text)
		{
			if (C == TEXT('\n')) { ++Lines; Run = 0; continue; }
			Longest = FMath::Max(Longest, ++Run);
		}
		const float Width = Longest * Size * 0.56f + Size * 1.2f;
		const float Height = Lines * Size * 1.15f + Size * 0.8f;
		const FVector Back = FRotator(0.0f, Yaw, 0.0f).Vector();
		// Set back in proportion to the text, so a title's board and its subtitle's board never share a plane (no flicker
		// where stacked signs overlap; they read as one panel).
		const FVector BoardAt = Location - Back * (3.0f + Size * 0.06f);
		Block(BoardAt, FVector(4.0f, Width, Height), FLinearColor(0.07f, 0.055f, 0.045f), Yaw, false);
		// Standing free? Then it stands on two posts down to whatever is below, rather than hanging in the air. A board
		// already against a wall or resting on something finds the ground close and gets none.
		const FVector Bottom = BoardAt - Back * 6.0f - FVector(0.0f, 0.0f, Height * 0.5f);
		FHitResult Hit;
		FCollisionQueryParams Params(SCENE_QUERY_STAT(DCCSignPosts), false);
		FHitResult Wall;
		const bool bOnWall = World->LineTraceSingleByChannel(Wall, BoardAt, BoardAt - Back * 80.0f, ECC_WorldStatic, Params);
		if (!bOnWall && World->LineTraceSingleByChannel(Hit, Bottom, Bottom - FVector(0.0f, 0.0f, 1500.0f), ECC_WorldStatic, Params) && Hit.Distance > 25.0f)
		{
			const FVector Side = FRotator(0.0f, Yaw + 90.0f, 0.0f).Vector();
			const float Post = Hit.Distance + Height * 0.6f;
			for (const float S : { -1.0f, 1.0f })
			{
				const FVector Foot = Bottom + Side * S * (Width * 0.5f - 12.0f);
				Block(FVector(Foot.X, Foot.Y, Hit.ImpactPoint.Z + Post * 0.5f), FVector(9.0f, 9.0f, Post), FLinearColor(0.045f, 0.035f, 0.03f), Yaw, false);
			}
		}
	}
	return Actor;
}

void ADCCArcadeArena::Light(const FVector& Location, const FLinearColor& Colour, float Intensity, float Radius)
{
	UWorld* World = GetWorld();
	if (!World)
	{
		return;
	}
	FActorSpawnParameters Params;
	Params.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
	if (APointLight* Actor = World->SpawnActor<APointLight>(Location, FRotator::ZeroRotator, Params))
	{
		Actor->SetReplicates(false);
		if (UPointLightComponent* Comp = Cast<UPointLightComponent>(Actor->GetLightComponent()))
		{
			Comp->SetMobility(EComponentMobility::Movable);
			Comp->SetIntensity(Intensity);
			Comp->SetAttenuationRadius(Radius);
			Comp->SetLightColor(Colour);
			Comp->SetCastShadows(false);
		}
		(BlockSink ? *BlockSink : LocalActors).Add(Actor);
	}
}

void ADCCArcadeArena::DestroyLocal(TArray<TObjectPtr<AActor>>& Actors)
{
	for (AActor* Actor : Actors)
	{
		if (IsValid(Actor))
		{
			Actor->Destroy();
		}
	}
	Actors.Reset();
}

void ADCCArcadeArena::ApplyCharacterShade()
{
	UWorld* World = GetWorld();
	if (!World || GetNetMode() == NM_DedicatedServer)
	{
		return;
	}
	const float Now = World->GetTimeSeconds();
	if (Now < NextShadeCheck)
	{
		return;
	}
	NextShadeCheck = Now + 0.5f; // new arrivals (players, owners, staff) pick it up within half a second

	if (!CharacterShade)
	{
		const FSky Sky = GetSky();
		// Automatic: a dark scene blends its characters more; a bright day barely touches them.
		const float Strength = Sky.ShadeStrength >= 0.0f ? Sky.ShadeStrength : FMath::Clamp(0.4f - Sky.SunIntensity * 0.06f, 0.1f, 0.38f);
		const float Rim = Sky.ShadeRimStrength >= 0.0f ? Sky.ShadeRimStrength : FMath::Clamp(Strength * 1.3f, 0.15f, 0.5f);
		if (Strength <= 0.0f)
		{
			NextShadeCheck = TNumericLimits<float>::Max();
			return;
		}
		UMaterialInterface* Base = LoadObject<UMaterialInterface>(nullptr, TEXT("/Game/DCC/Materials/M_CharacterShade.M_CharacterShade"));
		if (!Base)
		{
			NextShadeCheck = TNumericLimits<float>::Max();
			return;
		}
		CharacterShade = UMaterialInstanceDynamic::Create(Base, this);
		// The scene's own colour, darkened, as the blend; the same hue, brightened, as the rim light.
		const FLinearColor Fog = Sky.FogColour;
		const float Peak = FMath::Max3(Fog.R, Fog.G, Fog.B);
		const FLinearColor Hue = Peak > 0.001f ? FLinearColor(Fog.R / Peak, Fog.G / Peak, Fog.B / Peak) : FLinearColor::White;
		// The material MULTIPLIES what is behind it (Modulate), so it can only darken and tint, whatever the
		// exposure is doing. The first version added light at the edges, which made a character in a dim room
		// (the main menu) glow paler instead (2026-11-14). Tint is the multiplier in the middle of the body; the
		// edges are spared by RimStrength, which reads as a soft rim without adding any light.
		const float K = FMath::Clamp(Strength * 2.0f, 0.2f, 0.8f);
		const FLinearColor Multiplier = FMath::Lerp(FLinearColor::White, Hue * 0.45f, K);
		CharacterShade->SetVectorParameterValue(TEXT("Tint"), FLinearColor(Multiplier.R, Multiplier.G, Multiplier.B, 1.0f));
		CharacterShade->SetVectorParameterValue(TEXT("Edge"), FLinearColor::White);
		CharacterShade->SetScalarParameterValue(TEXT("RimStrength"), Rim);
		UE_LOG(LogTemp, Log, TEXT("[DCC Arcade] character shade: strength %.2f, rim %.2f"), Strength, Rim);
	}

	auto Shade = [this](AActor* Actor)
	{
		TArray<USkeletalMeshComponent*> Bodies;
		Actor->GetComponents<USkeletalMeshComponent>(Bodies);
		for (USkeletalMeshComponent* Body : Bodies)
		{
			// Leave anything that already wears an overlay (a mark's glow) alone.
			if (Body && Body->GetOverlayMaterial() == nullptr)
			{
				Body->SetOverlayMaterial(CharacterShade);
			}
		}
	};
	for (TActorIterator<ADCCCharacterBase> It(World); It; ++It) { Shade(*It); }
	for (TActorIterator<ADCCDialogueNPC> It(World); It; ++It) { Shade(*It); }
	for (TActorIterator<ADCCRallyThug> It(World); It; ++It) { Shade(*It); }
}
