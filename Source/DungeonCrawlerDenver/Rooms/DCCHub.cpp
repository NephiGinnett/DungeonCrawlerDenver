#include "Rooms/DCCHub.h"

#include "Characters/DCCCharacterBase.h"
#include "Dialogue/DCCHubVendors.h"
#include "Onboarding/DCCOnboardingActors.h"
#include "Rooms/DCCRoom.h"
#include "Rooms/DCCTaleFloor.h"
#include "Combat/DCCGameState.h"
#include "UI/DCCEventLogTypes.h"
#include "Components/BoxComponent.h"
#include "Components/PointLightComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Components/TextRenderComponent.h"
#include "Engine/PointLight.h"
#include "Engine/StaticMesh.h"
#include "Engine/StaticMeshActor.h"
#include "Engine/TextRenderActor.h"
#include "Engine/World.h"
#include "EngineUtils.h"
#include "Materials/MaterialInstanceDynamic.h"
#include "UObject/ConstructorHelpers.h"

namespace
{
	// Clear of the hand-built map, the home wing and the tale floor (150000, 150000).
	const FVector HubOrigin(120000.0f, 150000.0f, 5000.0f);

	const FLinearColor Terrazzo(0.20f, 0.19f, 0.18f);
	const FLinearColor Carpet(0.08f, 0.16f, 0.30f);
	const FLinearColor WallColour(0.11f, 0.11f, 0.13f);
	const FLinearColor Counter(0.30f, 0.22f, 0.12f);
	const FLinearColor GateBlue(0.10f, 0.30f, 0.85f);
}

ADCCHub::ADCCHub()
{
	PrimaryActorTick.bCanEverTick = false;
	bReplicates = true;
	bAlwaysRelevant = true;
	SetReplicatingMovement(false);

	SetRootComponent(CreateDefaultSubobject<USceneComponent>(TEXT("Root")));

	DeparturesPad = CreateDefaultSubobject<UBoxComponent>(TEXT("DeparturesPad"));
	DeparturesPad->SetupAttachment(GetRootComponent());
	DeparturesPad->SetRelativeLocation(FVector(-HalfLength + 180.0f, 0.0f, 120.0f));
	DeparturesPad->SetBoxExtent(FVector(140.0f, 260.0f, 140.0f));
	DeparturesPad->SetCollisionProfileName(TEXT("OverlapAllDynamic"));
	DeparturesPad->SetGenerateOverlapEvents(true);

	static ConstructorHelpers::FObjectFinder<UStaticMesh> Cube(TEXT("/Engine/BasicShapes/Cube.Cube"));
	CubeMesh = Cube.Object;
	static ConstructorHelpers::FObjectFinder<UMaterialInterface> Mat(TEXT("/Engine/BasicShapes/BasicShapeMaterial.BasicShapeMaterial"));
	ShapeMaterial = Mat.Object;
}

ADCCHub* ADCCHub::Find(const UWorld* World)
{
	if (!World)
	{
		return nullptr;
	}
	for (TActorIterator<ADCCHub> It(const_cast<UWorld*>(World)); It; ++It)
	{
		return *It;
	}
	return nullptr;
}

ADCCHub* ADCCHub::GetOrCreate(UWorld* World)
{
	if (ADCCHub* Existing = Find(World))
	{
		return Existing;
	}
	if (!World || World->GetNetMode() == NM_Client)
	{
		return nullptr;
	}
	FActorSpawnParameters Params;
	Params.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
	return World->SpawnActor<ADCCHub>(HubOrigin, FRotator::ZeroRotator, Params);
}

FVector ADCCHub::GetArrivalLocation(int32 Slot) const
{
	// Mid-concourse, a little east of centre, facing the shops.
	FVector At = GetActorLocation() + FVector(500.0f, 0.0f, 110.0f);
	if (Slot > 0)
	{
		const float Angle = Slot * (2.0f * PI / 6.0f);
		At += FVector(FMath::Cos(Angle), FMath::Sin(Angle), 0.0f) * 200.0f;
	}
	return At;
}

bool ADCCHub::SendToHub(ADCCCharacterBase* Crawler)
{
	if (!Crawler || !Crawler->HasAuthority())
	{
		return false;
	}
	ADCCHub* Hub = GetOrCreate(Crawler->GetWorld());
	if (!Hub)
	{
		return false;
	}

	// A slot per player already here, so the party lands in a ring.
	int32 Slot = 0;
	for (TActorIterator<ADCCCharacterBase> It(Crawler->GetWorld()); It; ++It)
	{
		if (*It != Crawler && It->IsPlayerControlled()
			&& FVector::Dist(It->GetActorLocation(), Hub->GetActorLocation()) < HalfLength * 1.5f)
		{
			++Slot;
		}
	}
	DCCOnboarding::TeleportCrawler(Crawler, Hub->GetArrivalLocation(Slot), FRotator(0.0f, 90.0f, 0.0f));
	Hub->PadCooldown.Add(Crawler, Crawler->GetWorld()->GetTimeSeconds() + 2.0);
	return true;
}

void ADCCHub::BeginPlay()
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

	BuildLocal();

	if (HasAuthority())
	{
		DeparturesPad->OnComponentBeginOverlap.AddDynamic(this, &ADCCHub::HandleDepartures);
		SpawnServerActors();
	}
}

void ADCCHub::Block(const FVector& LocalCenter, const FVector& Size, const FLinearColor& Colour)
{
	UWorld* World = GetWorld();
	if (!World || !CubeMesh)
	{
		return;
	}
	const FBoxSphereBounds Bounds = CubeMesh->GetBounds();
	const FVector MeshSize = Bounds.BoxExtent * 2.0f;
	const FVector Scale(Size.X / MeshSize.X, Size.Y / MeshSize.Y, Size.Z / MeshSize.Z);
	const FTransform Transform(FRotator::ZeroRotator, GetActorLocation() + LocalCenter - Bounds.Origin * Scale, Scale);

	AStaticMeshActor* Actor = World->SpawnActorDeferred<AStaticMeshActor>(AStaticMeshActor::StaticClass(), Transform);
	if (!Actor)
	{
		return;
	}
	Actor->SetReplicates(false);
	if (UStaticMeshComponent* Mesh = Actor->GetStaticMeshComponent())
	{
		Mesh->SetMobility(EComponentMobility::Movable);
		Mesh->SetStaticMesh(CubeMesh);
		Mesh->SetCollisionProfileName(TEXT("BlockAll"));
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
	LocalActors.Add(Actor);
}

void ADCCHub::Sign(const FVector& LocalLocation, float Yaw, const FString& Text, const FColor& Colour, float Size)
{
	UWorld* World = GetWorld();
	if (!World)
	{
		return;
	}
	const FTransform Transform(FRotator(0.0f, Yaw, 0.0f), GetActorLocation() + LocalLocation);
	ATextRenderActor* Actor = World->SpawnActorDeferred<ATextRenderActor>(ATextRenderActor::StaticClass(), Transform);
	if (!Actor)
	{
		return;
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
	LocalActors.Add(Actor);
}

void ADCCHub::Light(const FVector& LocalLocation, const FLinearColor& Colour, float Intensity, float Radius)
{
	UWorld* World = GetWorld();
	if (!World)
	{
		return;
	}
	FActorSpawnParameters Params;
	Params.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
	if (APointLight* Actor = World->SpawnActor<APointLight>(GetActorLocation() + LocalLocation, FRotator::ZeroRotator, Params))
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
		LocalActors.Add(Actor);
	}
}

void ADCCHub::BuildLocal()
{
	const float L = HalfLength, W = HalfWidth, H = WallHeight;

	// Floor, the famous carpet down the middle, walls, and a ceiling so the
	// sky (and the rest of the map) is not what the camera sees.
	Block(FVector(0, 0, -20), FVector(L * 2, W * 2, 40), Terrazzo);
	Block(FVector(0, 0, 1), FVector(L * 2 - 200, 500, 4), Carpet);
	Block(FVector(0, W + 20, H * 0.5f), FVector(L * 2 + 80, 40, H), WallColour);
	Block(FVector(0, -W - 20, H * 0.5f), FVector(L * 2 + 80, 40, H), WallColour);
	Block(FVector(L + 20, 0, H * 0.5f), FVector(40, W * 2, H), WallColour);
	Block(FVector(-L - 20, 0, H * 0.5f), FVector(40, W * 2, H), WallColour);
	Block(FVector(0, 0, H + 20), FVector(L * 2 + 80, W * 2 + 80, 40), FLinearColor(0.06f, 0.06f, 0.07f));

	// Text faces +X at yaw 0, so each sign turns towards where you read it from.
	// Departures: a blue gate frame at the west end.
	const float GateX = -L + 180.0f;
	Block(FVector(GateX, -300, 260), FVector(80, 60, 520), GateBlue);
	Block(FVector(GateX, 300, 260), FVector(80, 60, 520), GateBlue);
	Block(FVector(GateX, 0, 540), FVector(80, 660, 60), GateBlue);
	Block(FVector(GateX, 0, 2), FVector(280, 520, 4), FLinearColor(0.05f, 0.12f, 0.35f));
	Sign(FVector(GateX + 60, 0, 640), 0.0f, TEXT("DEPARTURES  >  THE WORLD DUNGEON"), FColor(140, 190, 255), 54.0f);
	Sign(FVector(GateX + 60, 0, 600), 0.0f, TEXT("Gate 0. Walk through to start a run. Boarding is not optional."), FColor(170, 170, 190), 22.0f);
	Light(FVector(GateX + 150, 0, 300), FLinearColor(0.45f, 0.65f, 1.0f), 9000.0f, 1600.0f);

	// The two shops along the north wall, counters facing the concourse.
	Block(FVector(-500, W - 260, 55), FVector(700, 110, 110), Counter);
	Sign(FVector(-500, W - 60, 470), -90.0f, TEXT("DUTY FREE (DUTY MANDATORY)"), FColor(255, 210, 100), 46.0f);
	Sign(FVector(-500, W - 60, 420), -90.0f, TEXT("Weapons for every Attack Skill. Armour. Regret."), FColor(200, 190, 170), 20.0f);
	Light(FVector(-500, W - 300, 380), FLinearColor(1.0f, 0.85f, 0.6f), 6000.0f, 1200.0f);

	Block(FVector(800, W - 260, 55), FVector(700, 110, 110), Counter);
	Sign(FVector(800, W - 60, 470), -90.0f, TEXT("PHARMACY & PYROTECHNICS"), FColor(255, 150, 90), 46.0f);
	Sign(FVector(800, W - 60, 420), -90.0f, TEXT("First Aid. Explosives. Please do not mix them up."), FColor(200, 190, 170), 20.0f);
	Light(FVector(800, W - 300, 380), FLinearColor(1.0f, 0.7f, 0.5f), 6000.0f, 1200.0f);

	// The south wall: the sponsor kiosk and the house rules.
	Block(FVector(0, -W + 120, 120), FVector(260, 160, 240), FLinearColor(0.45f, 0.30f, 0.05f));
	Sign(FVector(0, -W + 60, 420), 90.0f, TEXT("SPONSOR KIOSK"), FColor(255, 215, 110), 46.0f);
	Sign(FVector(0, -W + 60, 370), 90.0f, TEXT("Press O to open your loot boxes. The sponsors are watching."), FColor(200, 190, 170), 20.0f);
	Light(FVector(0, -W + 300, 380), FLinearColor(1.0f, 0.82f, 0.4f), 5000.0f, 1100.0f);

	Sign(FVector(-1100, -W + 60, 420), 90.0f, TEXT("F2  Party     F3  Gear bench     Tab  Your crawler"), FColor(170, 200, 170), 24.0f);

	// The east wall: the name, and the horse.
	Sign(FVector(L - 60, 0, 560), 180.0f, TEXT("CONCOURSE ZERO"), FColor(235, 225, 200), 90.0f);
	Sign(FVector(L - 60, 0, 470), 180.0f, TEXT("Denver International  -  Repossessed  -  Crawler Services"), FColor(180, 175, 165), 26.0f);
	Sign(FVector(L - 60, 0, 300), 180.0f, TEXT("The blue horse has been repossessed. The eyes have not."), FColor(120, 140, 255), 22.0f);

	// Airport seating: rows of benches either side of the carpet.
	for (int32 i = 0; i < 4; ++i)
	{
		const float X = 1200.0f + i * 160.0f;
		Block(FVector(X, -480, 25), FVector(90, 420, 50), FLinearColor(0.15f, 0.15f, 0.17f));
		Block(FVector(X, 480, 25), FVector(90, 420, 50), FLinearColor(0.15f, 0.15f, 0.17f));
	}

	// Even, warm concourse lighting.
	for (int32 i = -3; i <= 3; ++i)
	{
		Light(FVector(i * 550.0f, 0, H - 80), FLinearColor(1.0f, 0.92f, 0.8f), 3500.0f, 1500.0f);
	}
}

void ADCCHub::SpawnServerActors()
{
	UWorld* World = GetWorld();
	if (!World)
	{
		return;
	}

	// The concourse is a Safe Room: boxes open here, late joiners arrive here.
	{
		const FTransform Transform(FRotator::ZeroRotator, GetActorLocation() + FVector(0, 0, WallHeight * 0.5f));
		if (ADCCRoom* Room = World->SpawnActorDeferred<ADCCRoom>(ADCCRoom::StaticClass(), Transform))
		{
			Room->RoomType = EDCCRoomType::SafeRoom;
			if (UBoxComponent* Box = Room->FindComponentByClass<UBoxComponent>())
			{
				Box->SetBoxExtent(FVector(HalfLength, HalfWidth, WallHeight * 0.5f));
			}
			Room->FinishSpawning(Transform);
		}
	}

	// The shopkeepers, behind their counters, facing the concourse.
	const FTransform DutyFree(FRotator(0, -90, 0), GetActorLocation() + FVector(-500, HalfWidth - 120, 100));
	World->SpawnActor<ADCCVendor_DutyFree>(ADCCVendor_DutyFree::StaticClass(), DutyFree);
	const FTransform Pyro(FRotator(0, -90, 0), GetActorLocation() + FVector(800, HalfWidth - 120, 100));
	World->SpawnActor<ADCCVendor_Pyrotechnics>(ADCCVendor_Pyrotechnics::StaticClass(), Pyro);
}

void ADCCHub::HandleDepartures(UPrimitiveComponent* OverlappedComponent, AActor* OtherActor,
	UPrimitiveComponent* OtherComp, int32 OtherBodyIndex, bool bFromSweep, const FHitResult& SweepResult)
{
	ADCCCharacterBase* Crawler = Cast<ADCCCharacterBase>(OtherActor);
	UWorld* World = GetWorld();
	if (!HasAuthority() || !World || !Crawler || !Crawler->IsPlayerControlled() || !Crawler->IsAlive())
	{
		return;
	}
	if (const double* Until = PadCooldown.Find(Crawler))
	{
		if (World->GetTimeSeconds() < *Until)
		{
			return;
		}
	}
	PadCooldown.Add(Crawler, World->GetTimeSeconds() + 2.0);

	if (ADCCTaleFloor* Floor = ADCCTaleFloor::GetOrCreate(World))
	{
		Floor->Enter(Crawler, this);
	}
}
