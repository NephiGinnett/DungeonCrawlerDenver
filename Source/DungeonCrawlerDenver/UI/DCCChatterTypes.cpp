#include "UI/DCCChatterTypes.h"

FText DCCChatter::SubstituteTokens(const FText& Template, const FString& Name,
	const FString& Item, const FString& Event)
{
	FString Result = Template.ToString();

	if (!Name.IsEmpty())
	{
		Result = Result.Replace(TEXT("{Name}"), *Name);
	}
	if (!Item.IsEmpty())
	{
		Result = Result.Replace(TEXT("{Item}"), *Item);
	}
	if (!Event.IsEmpty())
	{
		Result = Result.Replace(TEXT("{Event}"), *Event);
	}

	return FText::FromString(Result);
}
