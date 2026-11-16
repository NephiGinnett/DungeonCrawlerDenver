#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "Session/DCCSession.h"
#include "DCCSessionMenuWidget.generated.h"

class SEditableTextBox;
class SVerticalBox;

/**
 * The party menu (F2): pick your crawler, host, invite, join, see who is here,
 * and (host only) start the run for everyone.
 *
 * Plain C++ Slate inside a UUserWidget, like UDCCChatInputWidget, so there is
 * no Widget Blueprint to author or keep in sync. Everything it shows is read
 * live (Text_Lambda / Visibility_Lambda), so it never shows stale state after a
 * join or a host.
 */
UCLASS()
class DUNGEONCRAWLERDENVER_API UDCCSessionMenuWidget : public UUserWidget
{
	GENERATED_BODY()

public:
	/** Called by the Close button. The HUD owns opening and closing. */
	TFunction<void()> OnCloseRequested;

	/** Re-reads addresses and any connection error. Call when the menu opens. */
	void Refresh();

protected:
	virtual TSharedRef<SWidget> RebuildWidget() override;
	virtual void NativeTick(const FGeometry& MyGeometry, float InDeltaTime) override;
	int32 SeenUPnPVersion = -1;
	virtual void ReleaseSlateResources(bool bReleaseChildren) override;

private:
	ENetMode GetNetModeSafe() const;
	bool IsSolo() const { return GetNetModeSafe() == NM_Standalone; }
	bool IsHost() const { return GetNetModeSafe() == NM_ListenServer; }
	bool IsClient() const { return GetNetModeSafe() == NM_Client; }

	FText GetStatusText() const;
	FText GetPartyText() const;
	FText GetStartRunLabel() const;

	void PickCrawler(FName CrawlerId);
	/** Sends the host a password change for the crawler you are (hashes only). */
	void ChangePassword();
	FText GetPasswordNote() const;
	TSharedPtr<SEditableTextBox> OldPasswordBox;
	TSharedPtr<SEditableTextBox> NewPasswordBox;
	void Host();
	void JoinFromBox();
	void JoinFromClipboard();
	void StartRun();
	void Leave();
	void CopyInvite(const DCCSession::FHostAddress& Address);
	void RebuildInviteList();

	TSharedPtr<SEditableTextBox> JoinBox;
	TSharedPtr<SVerticalBox> InviteList;
	TArray<DCCSession::FHostAddress> Addresses;
	FString Message;
	bool bMessageIsError = false;
};
