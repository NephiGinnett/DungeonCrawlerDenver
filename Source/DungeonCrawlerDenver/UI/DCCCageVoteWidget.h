#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "DCCCageVoteWidget.generated.h"

class ADCCArcadeHaunt;

/**
 * Dead Air's early ride up (2026-11-16). When every crawler still standing is back in the cage, this panel offers
 * SEND THE CAGE UP; once everyone has pressed it the round ends there, tapes counted as if the clock ran out.
 */
UCLASS()
class DUNGEONCRAWLERDENVER_API UDCCCageVoteWidget : public UUserWidget
{
	GENERATED_BODY()

public:
	TWeakObjectPtr<ADCCArcadeHaunt> Haunt;
	/** Local, for the button's look. Reset when the panel closes. */
	bool bVoted = false;

protected:
	virtual TSharedRef<SWidget> RebuildWidget() override;
};
