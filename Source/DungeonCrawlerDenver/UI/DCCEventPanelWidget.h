#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "DCCEventPanelWidget.generated.h"

/**
 * OPEN MIC WEEK, as a gacha event page (2026-11-23, Zoe sent three event mock-ups: "could you build something like this
 * especially for the second and third image").
 *
 * Deep purple and gold: an ornate framed window with a starfield, the event plaque on top, Hiss Cranson's pitch, the
 * featured prize card (THE HOT MIC, framed like an SSR card), and the EVENT PROGRESS bar along the bottom with a
 * medallion per milestone (1 line a Bronze box, 3 a Silver box and 50 tickets, 6 a Gold box and the OPEN MIC title).
 *
 * The milestones match VoiceDrop/server.js. This week's line count comes from Content/DCC/VoiceCredits/credits.json
 * ("week": {"monday", "counts"}), which Tools/voice_inbox.py writes on every pull; with no pull yet it shows zero.
 * RECORD A LINE opens the Voice Drop site when [DCCVoiceDrop] Url is set in DefaultGame.ini.
 */
UCLASS()
class DUNGEONCRAWLERDENVER_API UDCCEventPanelWidget : public UUserWidget
{
	GENERATED_BODY()

public:
	TFunction<void()> OnCloseRequested;
	/** DCC.EventPanel [delay] [lines]: show this many lines instead of the real count (-1 = real). */
	static int32 PreviewLines;

protected:
	virtual TSharedRef<SWidget> RebuildWidget() override;
	virtual void NativeConstruct() override;
	virtual void NativeTick(const FGeometry& MyGeometry, float InDeltaTime) override;
	virtual FReply NativeOnKeyDown(const FGeometry& InGeometry, const FKeyEvent& InKeyEvent) override;

private:
	float Opened = 0.0f;
	int32 Lines = 0;
	FString Monday;
};
