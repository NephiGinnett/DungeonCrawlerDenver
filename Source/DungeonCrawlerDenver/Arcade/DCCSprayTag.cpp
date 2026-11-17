#include "Arcade/DCCSprayTag.h"

#include "Arcade/DCCSpray.h"
#include "Components/DecalComponent.h"
#include "Engine/Texture2D.h"
#include "Net/UnrealNetwork.h"

ADCCSprayTag::ADCCSprayTag()
{
	PrimaryActorTick.bCanEverTick = false;
	bReplicates = true;
	bAlwaysRelevant = true; // a tag across the city should still be there when you drive up to it
	SetReplicateMovement(false);
	SetNetUpdateFrequency(1.0f);

	Decal = CreateDefaultSubobject<UDecalComponent>(TEXT("Decal"));
	RootComponent = Decal;
	// X is depth: shallow, so it paints the surface it hit and not the room behind.
	Decal->DecalSize = FVector(24.0f, DCCSpray::HalfSize, DCCSpray::HalfSize);
	Decal->SortOrder = 50;            // over the city's own grime and tags
	Decal->FadeScreenSize = 0.0f;     // stays painted at any distance
}

void ADCCSprayTag::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);
	DOREPLIFETIME_CONDITION(ADCCSprayTag, Pixels, COND_InitialOnly);
	DOREPLIFETIME_CONDITION(ADCCSprayTag, Painter, COND_InitialOnly);
}

void ADCCSprayTag::BeginPlay()
{
	Super::BeginPlay();
	if (Pixels.Num() == DCCSpray::Bytes)
	{
		Paint();
	}
}

void ADCCSprayTag::OnRep_Picture()
{
	Paint();
}

void ADCCSprayTag::Paint()
{
	if (GetNetMode() == NM_DedicatedServer || Picture)
	{
		return;
	}
	Picture = DCCSpray::MakeTexture(Pixels);
	if (UMaterialInterface* Material = DCCSpray::MakeMaterial(this, Picture))
	{
		Decal->SetDecalMaterial(Material);
	}
}
