#include "Quests/DCCQuestMarkers_Floor01.h"

#include "Components/StaticMeshComponent.h"
#include "Engine/StaticMesh.h"
#include "UObject/ConstructorHelpers.h"

namespace
{
	/** A tall, thin, emissive-ready placeholder beacon until real art exists. */
	void ApplyPlaceholderBeacon(UStaticMeshComponent* Beacon)
	{
		if (!Beacon)
		{
			return;
		}

		static ConstructorHelpers::FObjectFinder<UStaticMesh> CubeMesh(
			TEXT("/Game/LevelPrototyping/Meshes/SM_Cube.SM_Cube"));
		if (CubeMesh.Succeeded())
		{
			Beacon->SetStaticMesh(CubeMesh.Object);
		}

		Beacon->SetRelativeScale3D(FVector(0.15f, 0.15f, 3.0f));
	}
}

ADCCQuestMarker_Orientation::ADCCQuestMarker_Orientation()
{
	ObjectiveTag = TEXT("Orientation");
	MarkerLabel = NSLOCTEXT("DCC", "MarkerOrientation", "ORIENTATION");
	ApplyPlaceholderBeacon(Beacon);
}

ADCCQuestMarker_EastLedge::ADCCQuestMarker_EastLedge()
{
	ObjectiveTag = TEXT("EastLedge");
	MarkerLabel = NSLOCTEXT("DCC", "MarkerEastLedge", "THE LEDGE");
	ApplyPlaceholderBeacon(Beacon);
}

ADCCQuestMarker_BarEntrance::ADCCQuestMarker_BarEntrance()
{
	ObjectiveTag = TEXT("BarEntrance");
	MarkerLabel = NSLOCTEXT("DCC", "MarkerBarEntrance", "THE BAR");
	ApplyPlaceholderBeacon(Beacon);
}
