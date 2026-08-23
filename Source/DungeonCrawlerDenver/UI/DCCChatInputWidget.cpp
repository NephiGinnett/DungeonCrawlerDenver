#include "UI/DCCChatInputWidget.h"

#include "Widgets/Input/SEditableTextBox.h"
#include "Framework/Application/SlateApplication.h"

TSharedRef<SWidget> UDCCChatInputWidget::RebuildWidget()
{
	SAssignNew(TextBox, SEditableTextBox)
		.OnTextCommitted(FOnTextCommitted::CreateUObject(this, &UDCCChatInputWidget::HandleTextCommitted))
		.HintText(NSLOCTEXT("DCC", "ChatInputHint", "Type a message..."));

	return TextBox.ToSharedRef();
}

void UDCCChatInputWidget::SetHintText(const FText& Hint)
{
	if (TextBox.IsValid())
	{
		TextBox->SetHintText(Hint);
	}
}

void UDCCChatInputWidget::FocusInput()
{
	if (TextBox.IsValid())
	{
		FSlateApplication::Get().SetKeyboardFocus(TextBox, EFocusCause::SetDirectly);
	}
}

void UDCCChatInputWidget::HandleTextCommitted(const FText& NewText, ETextCommit::Type CommitType)
{
	if (CommitType == ETextCommit::OnEnter && !NewText.IsEmptyOrWhitespace())
	{
		OnSubmitted.Broadcast(NewText.ToString());
	}
	else
	{
		OnCancelled.Broadcast();
	}

	if (TextBox.IsValid())
	{
		TextBox->SetText(FText::GetEmpty());
	}
}
