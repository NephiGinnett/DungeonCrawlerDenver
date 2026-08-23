#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "DCCChatInputWidget.generated.h"

class SEditableTextBox;

/**
 * A single-line text box for chat entry, built entirely in C++ via Slate.
 *
 * DCCHUD's own UI is deliberately Canvas-drawn rather than UMG, because a UMG
 * *asset* (a WBP) needs the interactive UMG toolset in a live editor to author,
 * which this project's MCP workflow can't always reach - see DCCHUD.h's own
 * class comment. This class sidesteps that entirely: it is a C++ UUserWidget
 * with no WBP asset at all, building its Slate tree in RebuildWidget(), so it
 * works the moment it compiles, same as the rest of the HUD. Free-text keyboard
 * capture (cursor, selection, backspace, unicode) is the one thing Canvas
 * genuinely cannot do on its own, which is the only reason this widget exists
 * instead of one more DrawText call.
 */
UCLASS()
class DUNGEONCRAWLERDENVER_API UDCCChatInputWidget : public UUserWidget
{
	GENERATED_BODY()

public:
	DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FDCCChatInputCommitted, const FString&, SubmittedText);

	/** Fires when Enter is pressed with non-empty text. The box clears itself. */
	UPROPERTY(BlueprintAssignable, Category = "DCC|Chat")
	FDCCChatInputCommitted OnSubmitted;

	DECLARE_DYNAMIC_MULTICAST_DELEGATE(FDCCChatInputCancelled);

	/** Fires on Escape, or on committing with empty text. */
	UPROPERTY(BlueprintAssignable, Category = "DCC|Chat")
	FDCCChatInputCancelled OnCancelled;

	/** Optional placeholder shown when the box is empty - e.g. "Message Party..."
	 *  or "Whisper to Scott..." so the box makes sense without a separate label. */
	UFUNCTION(BlueprintCallable, Category = "DCC|Chat")
	void SetHintText(const FText& Hint);

	/** Grabs keyboard focus so typing goes into the box immediately. */
	UFUNCTION(BlueprintCallable, Category = "DCC|Chat")
	void FocusInput();

	virtual TSharedRef<SWidget> RebuildWidget() override;

private:
	void HandleTextCommitted(const FText& NewText, ETextCommit::Type CommitType);

	TSharedPtr<SEditableTextBox> TextBox;
};
