#include "UI/DCCHUD.h"

#include "Characters/DCCCharacterBase.h"
#include "Stats/DCCStatsComponent.h"
#include "Stats/DCCRuleset.h"
#include "Skills/DCCSkillComponent.h"
#include "Items/DCCInventoryComponent.h"
#include "Items/DCCCarryComponent.h"
#include "Items/DCCPickup.h"
#include "Combat/DCCGameState.h"
#include "Engine/Canvas.h"
#include "Engine/Engine.h"

namespace
{
	constexpr float LineHeight = 20.0f;
	constexpr float SectionGap = 14.0f;

	const FLinearColor HeadingColour(1.0f, 0.82f, 0.35f);
	const FLinearColor BodyColour(0.92f, 0.94f, 1.0f);
	const FLinearColor DimColour(0.62f, 0.66f, 0.78f);
}

ADCCCharacterBase* ADCCHUD::GetOwningCrawler() const
{
	return PlayerOwner ? Cast<ADCCCharacterBase>(PlayerOwner->GetPawn()) : nullptr;
}

void ADCCHUD::DrawHUD()
{
	Super::DrawHUD();

	if (!Canvas)
	{
		return;
	}

	ADCCCharacterBase* Crawler = GetOwningCrawler();

	// The feed is always on: it is the only way to see what the dice did, and
	// hiding it behind a menu would defeat the point.
	float FeedY = DrawFeed(24.0f, Canvas->SizeY - (FeedLineCount + 1) * LineHeight - 24.0f);

	if (!bShowMenu || !Crawler)
	{
		if (!bShowMenu)
		{
			DrawText(TEXT("[Tab] Character"), DimColour, 24.0f, 24.0f);
		}
		return;
	}

	// A dark backing panel, otherwise light text vanishes against the sky.
	DrawRect(FLinearColor(0.02f, 0.03f, 0.06f, 0.78f), 16.0f, 16.0f, 460.0f, Canvas->SizeY - 32.0f);

	float Y = 28.0f;
	const float X = 32.0f;

	DrawText(TEXT("CHARACTER"), HeadingColour, X, Y);
	Y += LineHeight + 4.0f;

	Y = DrawStats(X, Y, Crawler);
	Y += SectionGap;
	Y = DrawSkills(X, Y, Crawler);
	Y += SectionGap;
	Y = DrawInventory(X, Y, Crawler);
}

float ADCCHUD::DrawStats(float X, float Y, ADCCCharacterBase* Crawler)
{
	UDCCStatsComponent* Stats = Crawler->Stats;
	if (!Stats)
	{
		return Y;
	}

	DrawText(TEXT("Stats"), HeadingColour, X, Y);
	Y += LineHeight;

	UDCCRuleset* Ruleset = Stats->GetRuleset();
	const UEnum* AbilityEnum = StaticEnum<EDCCAbility>();

	for (int32 Index = 0; Index < static_cast<int32>(EDCCAbility::MAX); ++Index)
	{
		const EDCCAbility Ability = static_cast<EDCCAbility>(Index);
		const int32 Score = Stats->AbilityScores.GetScore(Ability);
		const int32 Modifier = Stats->GetAbilityModifier(Ability);

		// Both numbers, because the ruleset uses them for different things - the
		// score decides what you can lift, the modifier is what you add to a roll.
		const FString Line = FString::Printf(TEXT("  %-14s %2d   (+%d)"),
			AbilityEnum ? *AbilityEnum->GetDisplayNameTextByValue(Index).ToString() : TEXT("?"),
			Score, Modifier);

		DrawText(Line, BodyColour, X, Y);
		Y += LineHeight;
	}

	Y += 4.0f;
	DrawText(FString::Printf(TEXT("  Health   %d / %d"), Stats->CurrentHitPoints, Stats->MaxHitPoints), BodyColour, X, Y);
	Y += LineHeight;
	DrawText(FString::Printf(TEXT("  Mana     %d / %d"), Stats->CurrentManaPoints, Stats->MaxManaPoints), BodyColour, X, Y);
	Y += LineHeight;

	if (Ruleset)
	{
		// Surfaced because it is the number that makes Strength legible: it says
		// exactly which props this character can pick up.
		DrawText(FString::Printf(TEXT("  Can lift %.0f kg"),
			Ruleset->GetCarryMassLimit(Stats->AbilityScores)), DimColour, X, Y);
		Y += LineHeight;
	}

	return Y;
}

float ADCCHUD::DrawSkills(float X, float Y, ADCCCharacterBase* Crawler)
{
	UDCCSkillComponent* Skills = Crawler->Skills;
	if (!Skills)
	{
		return Y;
	}

	DrawText(TEXT("Skills"), HeadingColour, X, Y);
	Y += LineHeight;

	if (Skills->KnownSkills.IsEmpty())
	{
		DrawText(TEXT("  (none yet - try something untrained)"), DimColour, X, Y);
		return Y + LineHeight;
	}

	for (const FDCCKnownSkill& Known : Skills->KnownSkills)
	{
		FDCCSkillDef Definition;
		const bool bResolved = Skills->FindSkill(Known.SkillId, Definition);

		const FString Name = bResolved && !Definition.DisplayName.IsEmpty()
			? Definition.DisplayName.ToString()
			: Known.SkillId.ToString();

		// The advancement mark is shown because it is otherwise invisible state
		// that decides whether this skill can rank up at the next interval.
		const FString Line = FString::Printf(TEXT("  %-22s Rank %-2d %s"),
			*Name, Known.Rank, Known.bAdvancementMark ? TEXT("*") : TEXT(""));

		DrawText(Line, BodyColour, X, Y);
		Y += LineHeight;
	}

	DrawText(TEXT("  * used since last advancement"), DimColour, X, Y);
	return Y + LineHeight;
}

float ADCCHUD::DrawInventory(float X, float Y, ADCCCharacterBase* Crawler)
{
	DrawText(TEXT("Inventory"), HeadingColour, X, Y);
	Y += LineHeight;

	// Held and stowed are genuinely different states - one is a physical object
	// in your hands that can be thrown, the other is a record in a bag - so the
	// menu shows them separately rather than pretending they are one list.
	if (UDCCCarryComponent* Carry = Crawler->Carry)
	{
		if (ADCCPickup* Held = Carry->CarriedPickup)
		{
			const FString Name = Held->DisplayName.IsEmpty()
				? Held->GetClass()->GetName()
				: Held->DisplayName.ToString();

			DrawText(FString::Printf(TEXT("  Holding: %s (%.0f kg)"), *Name, Held->MassKilograms),
				BodyColour, X, Y);
		}
		else
		{
			DrawText(TEXT("  Holding: nothing"), DimColour, X, Y);
		}
		Y += LineHeight;
	}

	UDCCInventoryComponent* Inventory = Crawler->FindComponentByClass<UDCCInventoryComponent>();
	if (!Inventory)
	{
		return Y;
	}

	DrawText(FString::Printf(TEXT("  Stowed %d / %d  (%.0f kg)"),
		Inventory->Items.Num(), Inventory->Capacity, Inventory->GetTotalMass()), DimColour, X, Y);
	Y += LineHeight;

	for (int32 Index = 0; Index < Inventory->Items.Num(); ++Index)
	{
		const FDCCInventoryItem& Item = Inventory->Items[Index];
		DrawText(FString::Printf(TEXT("    %d. %s (%.0f kg)"),
			Index + 1, *Item.DisplayName.ToString(), Item.MassKilograms), BodyColour, X, Y);
		Y += LineHeight;
	}

	return Y;
}

float ADCCHUD::DrawFeed(float X, float Y)
{
	const ADCCGameState* GameState = GetWorld() ? GetWorld()->GetGameState<ADCCGameState>() : nullptr;
	if (!GameState)
	{
		return Y;
	}

	const TArray<FDCCEventLogEntry>& Log = GameState->GetEventLog();
	const int32 First = FMath::Max(0, Log.Num() - FeedLineCount);

	for (int32 Index = First; Index < Log.Num(); ++Index)
	{
		// Older lines dim toward the top, so the eye lands on what just happened.
		const float Age = static_cast<float>(Log.Num() - Index) / FMath::Max(1, FeedLineCount);
		const FLinearColor Colour = FMath::Lerp(BodyColour, DimColour, Age);

		DrawText(Log[Index].Text.ToString(), Colour, X, Y);
		Y += LineHeight;
	}

	return Y;
}
