#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "DCCRoundOverWidget.generated.h"

class ADCCArcadeArena;

/**
 * End of an arcade round (2026-11-13): two big buttons, PLAY AGAIN and BACK TO
 * THE DEN, with the live vote count and the time left. Majority wins; see
 * ADCCArcadeArena::ReturnToLobbyIn. The HUD shows it while the arena's
 * bRoundOver is set.
 */
UCLASS()
class DUNGEONCRAWLERDENVER_API UDCCRoundOverWidget : public UUserWidget
{
	GENERATED_BODY()

public:
	TWeakObjectPtr<ADCCArcadeArena> Arena;

protected:
	virtual TSharedRef<SWidget> RebuildWidget() override;

private:
	void Vote(bool bPlayAgain);

	/** 0 not voted, 1 again, 2 Den. Local, for highlighting. */
	int32 MyVote = 0;
};
