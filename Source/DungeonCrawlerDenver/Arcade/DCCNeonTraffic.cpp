#include "Arcade/DCCNeonTraffic.h"

#include "Components/StaticMeshComponent.h"
#include "Engine/StaticMesh.h"
#include "Engine/World.h"
#include "GameFramework/GameStateBase.h"
#include "Materials/MaterialInstanceDynamic.h"
#include "Materials/MaterialInterface.h"
#include "Net/UnrealNetwork.h"
#include "UObject/ConstructorHelpers.h"

namespace
{
	// The car's parts, relative to its centre: offset and size in cm (the cube mesh is 100 cm).
	const FVector BodyAt(0, 0, -30), BodySize(380, 250, 110);
	const FVector CabinAt(20, 0, 60), CabinSize(210, 220, 80);
	const FVector TailLeftAt(-192, -85, -20), TailRightAt(-192, 85, -20), TailSize(8, 60, 22);
	const FVector GlowAt(0, 0, -88), GlowSize(400, 270, 6);
	const FVector CrashSize(380, 260, 220);

	UStaticMeshComponent* MakePart(AActor* Owner, USceneComponent* Root, const TCHAR* Name, UStaticMesh* Cube,
		const FVector& At, const FVector& Size, bool bCollide)
	{
		UStaticMeshComponent* Part = Owner->CreateDefaultSubobject<UStaticMeshComponent>(Name);
		Part->SetupAttachment(Root);
		Part->SetStaticMesh(Cube);
		Part->SetRelativeLocation(At);
		Part->SetRelativeScale3D(Size / 100.0f);
		Part->SetCollisionProfileName(bCollide ? TEXT("BlockAll") : TEXT("NoCollision"));
		Part->SetCastShadow(false);
		Part->SetGenerateOverlapEvents(false);
		return Part;
	}
}

ADCCNeonTrafficCar::ADCCNeonTrafficCar()
{
	PrimaryActorTick.bCanEverTick = true;
	bReplicates = true;
	SetReplicatingMovement(false); // every machine computes the position from the clock
	bAlwaysRelevant = true;        // a handful of cars; seeing them far up the road matters

	USceneComponent* Root = CreateDefaultSubobject<USceneComponent>(TEXT("Root"));
	RootComponent = Root;

	static ConstructorHelpers::FObjectFinder<UStaticMesh> CubeFinder(TEXT("/Engine/BasicShapes/Cube.Cube"));
	UStaticMesh* Cube = CubeFinder.Object;
	Body = MakePart(this, Root, TEXT("Body"), Cube, BodyAt, BodySize, true);
	Cabin = MakePart(this, Root, TEXT("Cabin"), Cube, CabinAt, CabinSize, false);
	TailLeft = MakePart(this, Root, TEXT("TailLeft"), Cube, TailLeftAt, TailSize, false);
	TailRight = MakePart(this, Root, TEXT("TailRight"), Cube, TailRightAt, TailSize, false);
	Underglow = MakePart(this, Root, TEXT("Underglow"), Cube, GlowAt, GlowSize, false);
}

void ADCCNeonTrafficCar::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);
	DOREPLIFETIME(ADCCNeonTrafficCar, StartLocation);
	DOREPLIFETIME(ADCCNeonTrafficCar, StartTime);
	DOREPLIFETIME(ADCCNeonTrafficCar, Speed);
	DOREPLIFETIME(ADCCNeonTrafficCar, bPink);
}

void ADCCNeonTrafficCar::Init(const FVector& InStart, double InStartTime, float InSpeed, bool bInPink)
{
	StartLocation = InStart;
	StartTime = InStartTime;
	Speed = InSpeed;
	bPink = bInPink;
}

void ADCCNeonTrafficCar::BeginPlay()
{
	Super::BeginPlay();
	Paint(); // the spawn bunch carries the replicated fields, so the colour is known here
	SetActorLocation(LocationAt(ClockOf(GetWorld())));
}

void ADCCNeonTrafficCar::Paint()
{
	UMaterialInterface* Base = LoadObject<UMaterialInterface>(nullptr, TEXT("/Engine/BasicShapes/BasicShapeMaterial.BasicShapeMaterial"));
	if (!Base)
	{
		return;
	}
	const FLinearColor PaintColour = bPink ? FLinearColor(0.55f, 0.08f, 0.35f) : FLinearColor(0.06f, 0.35f, 0.45f);
	const FLinearColor Glow = bPink ? FLinearColor(1.0f, 0.2f, 0.85f) : FLinearColor(0.2f, 1.0f, 0.95f);
	auto Tint = [Base](UStaticMeshComponent* Part, const FLinearColor& Colour)
	{
		UMaterialInstanceDynamic* Mid = UMaterialInstanceDynamic::Create(Base, Part);
		Mid->SetVectorParameterValue(TEXT("Color"), Colour);
		Mid->SetVectorParameterValue(TEXT("BaseColor"), Colour);
		Part->SetMaterial(0, Mid);
	};
	Tint(Body, PaintColour);
	Tint(Cabin, PaintColour * 0.5f);
	Tint(TailLeft, FLinearColor(3.0f, 0.1f, 0.1f));
	Tint(TailRight, FLinearColor(3.0f, 0.1f, 0.1f));
	Tint(Underglow, Glow * 2.5f);
}

double ADCCNeonTrafficCar::ClockOf(const UWorld* World)
{
	if (!World)
	{
		return 0.0;
	}
	const AGameStateBase* GameState = World->GetGameState();
	return GameState ? GameState->GetServerWorldTimeSeconds() : World->GetTimeSeconds();
}

FVector ADCCNeonTrafficCar::LocationAt(double Now) const
{
	return StartLocation + FVector(Speed * FMath::Max(0.0, Now - StartTime), 0.0f, 0.0f);
}

void ADCCNeonTrafficCar::Tick(float DeltaSeconds)
{
	Super::Tick(DeltaSeconds);
	SetActorLocation(LocationAt(ClockOf(GetWorld())));
}

FBox ADCCNeonTrafficCar::GetCrashBox() const
{
	const FVector At = GetActorLocation();
	return FBox(At - CrashSize * 0.5f, At + CrashSize * 0.5f);
}
