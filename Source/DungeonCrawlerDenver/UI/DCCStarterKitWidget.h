#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "DCCStarterKitWidget.generated.h"

/** Pick your starter kit (DCCStarterKits): five cards, one click. Opened by UDCCWalletComponent::Client_OfferStarterKit. */
UCLASS()
class DUNGEONCRAWLERDENVER_API UDCCStarterKitWidget : public UUserWidget
{
	GENERATED_BODY()

public:
	TFunction<void()> OnClosed;

protected:
	virtual TSharedRef<SWidget> RebuildWidget() override;
};
