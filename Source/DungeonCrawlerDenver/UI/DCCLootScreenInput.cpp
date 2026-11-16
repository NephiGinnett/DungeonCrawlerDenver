#include "UI/DCCLootScreenInput.h"

#include "UI/DCCHUD.h"
#include "Styling/CoreStyle.h"
#include "Widgets/Layout/SBorder.h"

void UDCCLootScreenInput::NativeOnInitialized()
{
	Super::NativeOnInitialized();
	SetIsFocusable(true);
}

TSharedRef<SWidget> UDCCLootScreenInput::RebuildWidget()
{
	// Transparent, but Visible (not HitTestInvisible), so it covers the whole
	// viewport and every click lands here rather than on the game.
	return SNew(SBorder)
		.BorderImage(FCoreStyle::Get().GetBrush("NoBorder"))
		.Visibility(EVisibility::Visible);
}

FReply UDCCLootScreenInput::NativeOnKeyDown(const FGeometry& InGeometry, const FKeyEvent& InKeyEvent)
{
	if (Hud.IsValid() && !InKeyEvent.IsRepeat() && Hud->HandleLootScreenKey(InKeyEvent.GetKey(), true))
	{
		return FReply::Handled();
	}
	return InKeyEvent.IsRepeat() ? FReply::Handled() : Super::NativeOnKeyDown(InGeometry, InKeyEvent);
}

FReply UDCCLootScreenInput::NativeOnKeyUp(const FGeometry& InGeometry, const FKeyEvent& InKeyEvent)
{
	if (Hud.IsValid() && Hud->HandleLootScreenKey(InKeyEvent.GetKey(), false))
	{
		return FReply::Handled();
	}
	return Super::NativeOnKeyUp(InGeometry, InKeyEvent);
}

FReply UDCCLootScreenInput::NativeOnMouseButtonDown(const FGeometry& InGeometry, const FPointerEvent& InMouseEvent)
{
	if (Hud.IsValid())
	{
		Hud->HandleLootScreenKey(InMouseEvent.GetEffectingButton(), true);
	}
	return FReply::Handled();
}

FReply UDCCLootScreenInput::NativeOnMouseButtonUp(const FGeometry& InGeometry, const FPointerEvent& InMouseEvent)
{
	if (Hud.IsValid())
	{
		Hud->HandleLootScreenKey(InMouseEvent.GetEffectingButton(), false);
	}
	return FReply::Handled();
}

FReply UDCCLootScreenInput::NativeOnMouseWheel(const FGeometry& InGeometry, const FPointerEvent& InMouseEvent)
{
	if (Hud.IsValid())
	{
		Hud->HandleLootScreenKey(InMouseEvent.GetWheelDelta() > 0.0f ? EKeys::Left : EKeys::Right, true);
	}
	return FReply::Handled();
}
