#pragma once

#include "CoreMinimal.h"
#include "Quests/DCCQuestMarker.h"
#include "DCCQuestMarkers_Floor01.generated.h"

// ---------------------------------------------------------------------------
// One dedicated subclass per Floor 1 marker.
//
// ADCCQuestMarker's ObjectiveTag, MarkerLabel and Beacon mesh were left unset
// on the placed instances - the plan was to fill them in via the Details
// panel, but `ObjectTools.set_properties` cannot write to placed actors any
// more than it can Blueprint CDOs, so the markers existed, tracked correctly,
// and were invisible: present in the HUD tracker, nowhere in the world.
//
// Same fix as the ally characters and the DebtEater/Voldemortgage stat blocks:
// move the per-marker data into a C++ constructor default instead of an
// editor-time write. One class per named objective, since each marker needs
// its own tag - there is no generic "quest marker with the right tag" the way
// there was a generic "ally with the right mesh".
// ---------------------------------------------------------------------------

/** Room 01, near the west doorway. */
UCLASS()
class DUNGEONCRAWLERDENVER_API ADCCQuestMarker_Orientation : public ADCCQuestMarker
{
	GENERATED_BODY()

public:
	ADCCQuestMarker_Orientation();
};

/** The climb ledge above the Falls. */
UCLASS()
class DUNGEONCRAWLERDENVER_API ADCCQuestMarker_EastLedge : public ADCCQuestMarker
{
	GENERATED_BODY()

public:
	ADCCQuestMarker_EastLedge();
};

/** The Bar (Room 5) - roughly x9200, y5000 per floor-01-design.md. */
UCLASS()
class DUNGEONCRAWLERDENVER_API ADCCQuestMarker_BarEntrance : public ADCCQuestMarker
{
	GENERATED_BODY()

public:
	ADCCQuestMarker_BarEntrance();
};
