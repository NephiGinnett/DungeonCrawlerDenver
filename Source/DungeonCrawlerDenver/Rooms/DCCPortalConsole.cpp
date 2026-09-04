#include "Rooms/DCCPortalConsole.h"

#include "Rooms/DCCExitPortal.h"
#include "Characters/DCCCharacterBase.h"
#include "Components/StaticMeshComponent.h"
#include "Components/PointLightComponent.h"
#include "UObject/ConstructorHelpers.h"

ADCCPortalConsole::ADCCPortalConsole()
{
	PrimaryActorTick.bCanEverTick = false;
	bReplicates = true;

	Mesh = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("Mesh"));
	SetRootComponent(Mesh);

	static ConstructorHelpers::FObjectFinder<UStaticMesh> ConsoleMesh(
		TEXT("/Game/LevelPrototyping/Meshes/SM_Cube.SM_Cube"));
	if (ConsoleMesh.Succeeded())
	{
		Mesh->SetStaticMesh(ConsoleMesh.Object);
	}

	// A waist-height podium until there is real art for it - same
	// placeholder-until-real-art convention as every other prop in this project.
	Mesh->SetRelativeScale3D(FVector(0.8f, 0.8f, 1.0f));

	IndicatorLight = CreateDefaultSubobject<UPointLightComponent>(TEXT("IndicatorLight"));
	IndicatorLight->SetupAttachment(Mesh);
	IndicatorLight->SetRelativeLocation(FVector(0.0f, 0.0f, 55.0f));
	IndicatorLight->IntensityUnits = ELightUnits::Candelas;
	IndicatorLight->SetIntensity(600.0f);
	IndicatorLight->SetLightColor(FLinearColor(0.85f, 0.15f, 0.15f));
	IndicatorLight->SetAttenuationRadius(200.0f);
	IndicatorLight->SetCastShadows(false);
}

void ADCCPortalConsole::TryPlayerInteract(ADCCCharacterBase* Interactor)
{
	if (!Interactor || !Portal)
	{
		return;
	}

	if (Portal->IsActive())
	{
		Interactor->ShowLocalNotice(NSLOCTEXT("DCC", "PortalAlreadyOn", "Already running."));
		return;
	}

	if (Interactor->HasAuthority())
	{
		Portal->Activate();
	}
	else
	{
		Interactor->Server_ActivatePortalConsole(this);
	}

	// Local, immediate feedback for whoever pressed it - the portal's own
	// OnRep_Active is what makes the actual portal appear on every machine
	// (including this one), this is purely "the button now looks pressed."
	IndicatorLight->SetLightColor(FLinearColor(0.20f, 0.85f, 0.30f));
}
