#include "Puzzles/DCCBreakerSwitch.h"

#include "Puzzles/DCCPuzzleDoor.h"
#include "Characters/DCCCharacterBase.h"
#include "Components/StaticMeshComponent.h"
#include "Components/PointLightComponent.h"
#include "Engine/World.h"
#include "EngineUtils.h"
#include "Net/UnrealNetwork.h"
#include "UObject/ConstructorHelpers.h"

ADCCBreakerSwitch::ADCCBreakerSwitch()
{
	PrimaryActorTick.bCanEverTick = false;
	bReplicates = true;

	Mesh = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("Mesh"));
	SetRootComponent(Mesh);

	static ConstructorHelpers::FObjectFinder<UStaticMesh> CubeMesh(
		TEXT("/Engine/BasicShapes/Cube.Cube"));
	if (CubeMesh.Succeeded())
	{
		Mesh->SetStaticMesh(CubeMesh.Object);
	}
	Mesh->SetRelativeScale3D(FVector(0.3f, 0.3f, 0.6f));
	Mesh->SetCollisionEnabled(ECollisionEnabled::QueryAndPhysics);

	IndicatorLight = CreateDefaultSubobject<UPointLightComponent>(TEXT("IndicatorLight"));
	IndicatorLight->SetupAttachment(Mesh);
	IndicatorLight->SetIntensity(900.0f);
	IndicatorLight->SetAttenuationRadius(220.0f);
}

void ADCCBreakerSwitch::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);
	DOREPLIFETIME(ADCCBreakerSwitch, bOn);
}

void ADCCBreakerSwitch::BeginPlay()
{
	Super::BeginPlay();

	if (HasAuthority())
	{
		bOn = bStartsOn;
	}
	ApplyOnVisuals();
}

void ADCCBreakerSwitch::OnRep_On()
{
	ApplyOnVisuals();
}

void ADCCBreakerSwitch::ApplyOnVisuals()
{
	if (IndicatorLight)
	{
		IndicatorLight->SetLightColor(bOn ? FLinearColor::Green : FLinearColor::Red);
	}
}

void ADCCBreakerSwitch::Flip()
{
	if (!HasAuthority())
	{
		return;
	}

	// This switch and its neighbours, each exactly once. Toggling through a
	// gathered set rather than recursing keeps a mutual neighbour pair from
	// flipping each other back and forth forever.
	TSet<ADCCBreakerSwitch*> ToFlip;
	ToFlip.Add(this);
	for (ADCCBreakerSwitch* Neighbour : Neighbours)
	{
		if (Neighbour)
		{
			ToFlip.Add(Neighbour);
		}
	}

	for (ADCCBreakerSwitch* Switch : ToFlip)
	{
		Switch->bOn = !Switch->bOn;
		Switch->ApplyOnVisuals();
	}

	EvaluatePanel();
}

void ADCCBreakerSwitch::EvaluatePanel()
{
	if (!HasAuthority() || !LinkedDoor || LinkedDoor->IsOpen())
	{
		return;
	}

	UWorld* World = GetWorld();
	if (!World)
	{
		return;
	}

	for (TActorIterator<ADCCBreakerSwitch> It(World); It; ++It)
	{
		const ADCCBreakerSwitch* Switch = *It;
		if (Switch && Switch->PanelId == PanelId && !Switch->IsOn())
		{
			return;
		}
	}

	LinkedDoor->Unlock();
}

void ADCCBreakerSwitch::TryPlayerInteract(ADCCCharacterBase* Interactor)
{
	if (!Interactor)
	{
		return;
	}

	if (HasAuthority())
	{
		Flip();
	}
	else
	{
		Interactor->Server_FlipBreakerSwitch(this);
	}
}
