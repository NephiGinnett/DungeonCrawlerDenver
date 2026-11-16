#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "DCCCharacterSelectWidget.generated.h"

/**
 * The main menu (2026-11-13), the first thing a launched game shows, over the
 * front door stage (ADCCArcadeFrontDoor) where your crawler stands.
 *
 * Everything is a button; nothing needs typing:
 *   - the six crawlers (the body on stage swaps as you pick), with their records
 *   - PLAY: into the Den, solo
 *   - HOST A PARTY: into the Den, open to friends; the party menu there has the invite
 *   - JOIN A PARTY: joins the invite on the clipboard (copy the host's message, click)
 *   - QUIT
 *
 * The crawler pick is remembered between launches, so a returning player just
 * presses PLAY (or Enter).
 *
 * Plain C++ Slate inside a UUserWidget, like the party menu.
 */
UCLASS()
class DUNGEONCRAWLERDENVER_API UDCCCharacterSelectWidget : public UUserWidget
{
	GENERATED_BODY()

public:
	/** Shows the stored pick on the stage and picks up any connection error to show. */
	void Refresh();

protected:
	virtual void NativeOnInitialized() override;
	virtual TSharedRef<SWidget> RebuildWidget() override;
	virtual void ReleaseSlateResources(bool bReleaseChildren) override;
	virtual FReply NativeOnKeyDown(const FGeometry& InGeometry, const FKeyEvent& InKeyEvent) override;
	virtual FReply NativeOnMouseButtonDown(const FGeometry& InGeometry, const FPointerEvent& InMouseEvent) override;

private:
	void Pick(FName CrawlerId);
	void Step(int32 Direction);
	void Play();
	void HostParty();
	void JoinParty();
	void RejoinLast();
	void Quit();
	void OpenPoseBench();
	void OpenShop();
	FText RecordsFor(FName CrawlerId) const;
	void SetMessage(const FString& Text, bool bError);

	// Crawler passwords (2026-11-14). The box holds what was typed; a PC that has entered the right password
	// before remembers its hash, so the box can stay empty.
	/** The hash to use for the picked crawler: what is typed, else what this PC remembers. */
	FString PasswordHash() const;
	/** True if that hash opens the picked crawler on THIS machine's saves (solo and host play). */
	bool IsUnlocked() const;
	/** Refuses with a message if the picked crawler is locked. Remembers the hash when it is right. */
	bool RequireUnlocked();
	void SaveNewPassword();

	TSharedPtr<class SEditableTextBox> PasswordBox;
	TSharedPtr<class SEditableTextBox> NewPasswordBox;
	bool bChangingPassword = false;

	FString Message;
	bool bMessageIsError = false;
	bool bLeaving = false;
};
