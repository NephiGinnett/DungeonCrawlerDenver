#include "UI/DCCHUD.h"

#include "Characters/DCCCharacterBase.h"
#include "Stats/DCCStatsComponent.h"
#include "Stats/DCCRuleset.h"
#include "Skills/DCCSkillComponent.h"
#include "Items/DCCInventoryComponent.h"
#include "Items/DCCCarryComponent.h"
#include "Items/DCCPickup.h"
#include "Progression/DCCAchievementComponent.h"
#include "Quests/DCCQuestComponent.h"
#include "Quests/DCCQuestMarker.h"
#include "Combat/DCCGameState.h"
#include "Rooms/DCCClimbPoint.h"
#include "Engine/Canvas.h"
#include "Engine/Engine.h"
#include "EngineUtils.h"

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
	DrawFeed(24.0f, Canvas->SizeY - (FeedLineCount + 1) * LineHeight - 24.0f);

	// Drawn before the death check, so the bars are still on screen underneath the
	// GAME OVER overlay showing exactly zero. Watching it sit empty is the point -
	// a bar that vanishes at the moment it matters teaches the player nothing.
	DrawVitals(Crawler);

	// Death takes over the screen. Checked on the stats rather than on a null
	// pawn, because the corpse is still possessed for a moment after dying.
	if (Crawler && Crawler->Stats && !Crawler->Stats->IsAlive())
	{
		DrawDeathScreen();
		return;
	}

	DrawQuestTracker();
	DrawInteractionPrompt(Crawler);
	DrawStaminaBar(Crawler);

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
	Y += SectionGap;
	Y = DrawAchievements(X, Y, Crawler);
}

void ADCCHUD::DrawQuestTracker()
{
	const UDCCQuestComponent* QuestLog = UDCCQuestComponent::Get(this);
	if (!QuestLog)
	{
		return;
	}

	TArray<FDCCQuestDef> Active;
	QuestLog->GetActiveQuests(Active);

	if (Active.IsEmpty())
	{
		return;
	}

	constexpr float PanelW = 340.0f;

	const float X = Canvas->SizeX - PanelW - 24.0f;
	float Y = 28.0f;

	// Measured before drawing so the backing panel is exactly as tall as the
	// content. A fixed-height panel either clips objectives or leaves a slab of
	// dead black over the play area.
	int32 Lines = 0;
	for (const FDCCQuestDef& Quest : Active)
	{
		++Lines;
		for (const FDCCQuestObjective& Objective : Quest.Objectives)
		{
			Lines += Objective.bHidden ? 0 : 1;
		}
	}

	DrawRect(FLinearColor(0.02f, 0.03f, 0.06f, 0.72f),
		X - 12.0f, Y - 10.0f, PanelW + 24.0f, (Lines + 1) * LineHeight + 24.0f);

	DrawText(TEXT("OBJECTIVES"), HeadingColour, X, Y);
	Y += LineHeight + 4.0f;

	for (const FDCCQuestDef& Quest : Active)
	{
		DrawText(Quest.Title.ToString(), HeadingColour, X, Y);
		Y += LineHeight;

		const FDCCQuestState* State = QuestLog->FindState(Quest.QuestId);

		for (int32 Index = 0; Index < Quest.Objectives.Num(); ++Index)
		{
			const FDCCQuestObjective& Objective = Quest.Objectives[Index];
			if (Objective.bHidden)
			{
				continue;
			}

			const bool bComplete = State && State->Objectives.IsValidIndex(Index)
				&& State->Objectives[Index].bComplete;

			const int32 Count = State && State->Objectives.IsValidIndex(Index)
				? State->Objectives[Index].Count : 0;

			// The count is only shown when there is more than one to do. "Reach
			// the ledge  0/1" is noise; "Kill interns  3/5" is the whole point.
			const FString Line = Objective.RequiredCount > 1
				? FString::Printf(TEXT("  %s %s  %d/%d"),
					bComplete ? TEXT("[X]") : TEXT("[ ]"),
					*Objective.Description.ToString(),
					FMath::Min(Count, Objective.RequiredCount), Objective.RequiredCount)
				: FString::Printf(TEXT("  %s %s"),
					bComplete ? TEXT("[X]") : TEXT("[ ]"),
					*Objective.Description.ToString());

			DrawText(Line, bComplete ? DimColour : BodyColour, X, Y);
			Y += LineHeight;
		}
	}

	// Distance to the nearest live marker, so "go there" has a direction even
	// before the beacon is on screen.
	const ADCCCharacterBase* Crawler = GetOwningCrawler();
	if (!Crawler || !GetWorld())
	{
		return;
	}

	float NearestDistance = TNumericLimits<float>::Max();
	FText NearestLabel;

	for (TActorIterator<ADCCQuestMarker> It(GetWorld()); It; ++It)
	{
		const ADCCQuestMarker* Marker = *It;
		if (!Marker || !Marker->IsMarkerActive())
		{
			continue;
		}

		const float Distance = FVector::Dist(Crawler->GetActorLocation(), Marker->GetGoalLocation());
		if (Distance < NearestDistance)
		{
			NearestDistance = Distance;
			NearestLabel = Marker->MarkerLabel;
		}
	}

	if (NearestDistance < TNumericLimits<float>::Max())
	{
		Y += 4.0f;

		// Metres, not centimetres. Nobody has ever wanted to read "4820 uu".
		DrawText(FString::Printf(TEXT("  -> %s  (%.0f m)"),
			*NearestLabel.ToString(), NearestDistance / 100.0f), HeadingColour, X, Y);
	}
}

void ADCCHUD::DrawMeter(float X, float Y, float Width, float Height, float Fraction,
	const FLinearColor& Fill, const FString& Label)
{
	const float Clamped = FMath::Clamp(Fraction, 0.0f, 1.0f);

	// The empty track is always drawn at full width, so an empty bar still reads
	// as "this meter exists and it is empty" rather than as nothing at all.
	DrawRect(FLinearColor(0.03f, 0.04f, 0.07f, 0.80f), X - 2.0f, Y - 2.0f, Width + 4.0f, Height + 4.0f);
	DrawRect(FLinearColor(0.12f, 0.13f, 0.18f, 0.90f), X, Y, Width, Height);

	if (Clamped > 0.0f)
	{
		DrawRect(Fill, X, Y, Width * Clamped, Height);
	}

	DrawText(Label, BodyColour, X + Width + 10.0f, Y - 2.0f);
}

void ADCCHUD::DrawVitals(ADCCCharacterBase* Crawler)
{
	UDCCStatsComponent* Stats = Crawler ? Crawler->Stats : nullptr;
	if (!Stats)
	{
		return;
	}

	constexpr float BarW = 240.0f;
	constexpr float BarH = 16.0f;

	const float X = 24.0f;
	float Y = Canvas->SizeY - 150.0f;

	// Health reddens as it drops rather than being flat red, so the difference
	// between "hurt" and "about to die" is visible without reading the number.
	const float HealthFraction = Stats->GetHealthFraction();
	const FLinearColor HealthFill = FMath::Lerp(
		FLinearColor(0.85f, 0.10f, 0.10f),
		FLinearColor(0.30f, 0.85f, 0.35f),
		HealthFraction);

	DrawMeter(X, Y, BarW, BarH, HealthFraction, HealthFill,
		FString::Printf(TEXT("HP  %d / %d"), FMath::Max(0, Stats->CurrentHitPoints), Stats->MaxHitPoints));
	Y += BarH + 8.0f;

	if (Stats->MaxManaPoints > 0)
	{
		DrawMeter(X, Y, BarW, BarH, Stats->GetManaFraction(), FLinearColor(0.30f, 0.55f, 0.95f),
			FString::Printf(TEXT("MP  %d / %d"), Stats->CurrentManaPoints, Stats->MaxManaPoints));
		Y += BarH + 8.0f;
	}

	DrawMeter(X, Y, BarW, BarH * 0.6f, Stats->GetExperienceFraction(), FLinearColor(0.85f, 0.70f, 0.25f),
		FString::Printf(TEXT("LVL %d   %d / %d XP"),
			Stats->Level, Stats->ExperiencePoints, Stats->ExperienceToNextLevel));
}

float ADCCHUD::DrawAchievements(float X, float Y, ADCCCharacterBase* Crawler)
{
	UDCCAchievementComponent* Achievements = Crawler->Achievements;
	if (!Achievements)
	{
		return Y;
	}

	TArray<FDCCAchievementDef> Visible;
	Achievements->GetVisibleAchievements(Visible);

	DrawText(FString::Printf(TEXT("Achievements  (%d)"), Achievements->GetUnlockedCount()),
		HeadingColour, X, Y);
	Y += LineHeight;

	if (Visible.IsEmpty())
	{
		DrawText(TEXT("  (no achievement table assigned)"), DimColour, X, Y);
		return Y + LineHeight;
	}

	// Unlocked first, then the closest to unlocking. A flat list would bury the
	// one the player is two kills away from under twenty they will never see.
	Visible.Sort([Achievements](const FDCCAchievementDef& A, const FDCCAchievementDef& B)
	{
		const bool bAUnlocked = Achievements->IsUnlocked(A.AchievementId);
		const bool bBUnlocked = Achievements->IsUnlocked(B.AchievementId);

		if (bAUnlocked != bBUnlocked)
		{
			return bAUnlocked;
		}

		const float AProgress = A.Threshold > 0
			? static_cast<float>(Achievements->GetStatTotal(A.Stat)) / A.Threshold : 0.0f;
		const float BProgress = B.Threshold > 0
			? static_cast<float>(Achievements->GetStatTotal(B.Stat)) / B.Threshold : 0.0f;

		return AProgress > BProgress;
	});

	int32 Shown = 0;
	for (const FDCCAchievementDef& Definition : Visible)
	{
		if (Shown++ >= 8)
		{
			DrawText(FString::Printf(TEXT("  ... and %d more"), Visible.Num() - 8), DimColour, X, Y);
			Y += LineHeight;
			break;
		}

		const bool bUnlocked = Achievements->IsUnlocked(Definition.AchievementId);
		const int32 Total = Achievements->GetStatTotal(Definition.Stat);

		const FString Line = bUnlocked
			? FString::Printf(TEXT("  [X] %s"), *Definition.DisplayName.ToString())
			: FString::Printf(TEXT("  [ ] %-26s %d / %d"),
				*Definition.DisplayName.ToString(), FMath::Min(Total, Definition.Threshold), Definition.Threshold);

		DrawText(Line, bUnlocked ? HeadingColour : DimColour, X, Y);
		Y += LineHeight;
	}

	return Y;
}

void ADCCHUD::DrawDeathScreen()
{
	// Dim the whole screen rather than clear it: the fight carrying on behind the
	// overlay is the point, since the rest of the party is still in it.
	DrawRect(FLinearColor(0.35f, 0.02f, 0.02f, 0.45f), 0.0f, 0.0f, Canvas->SizeX, Canvas->SizeY);

	const float CentreX = Canvas->SizeX * 0.5f;
	const float CentreY = Canvas->SizeY * 0.5f;

	// Per the tone bible the System AI never breaks to sincerity - when something
	// genuinely bad happens it gets quieter and more corporate, and that contrast
	// does the emotional work.
	const FString Headline = TEXT("GAME OVER");
	const FString Subtitle = TEXT("Your viewership has ended. Thank you for participating.");
	const FString Footer   = TEXT("A complimentary difficulty adjustment has been scheduled.");

	float HeadW = 0.0f, HeadH = 0.0f;
	GetTextSize(Headline, HeadW, HeadH, GEngine->GetLargeFont(), 2.0f);

	DrawText(Headline, FLinearColor(1.0f, 0.92f, 0.92f),
		CentreX - HeadW * 0.5f, CentreY - 60.0f, GEngine->GetLargeFont(), 2.0f);

	float SubW = 0.0f, SubH = 0.0f;
	GetTextSize(Subtitle, SubW, SubH, GEngine->GetMediumFont(), 1.0f);
	DrawText(Subtitle, FLinearColor(0.95f, 0.85f, 0.85f),
		CentreX - SubW * 0.5f, CentreY + 10.0f, GEngine->GetMediumFont(), 1.0f);

	float FootW = 0.0f, FootH = 0.0f;
	GetTextSize(Footer, FootW, FootH, GEngine->GetMediumFont(), 1.0f);
	DrawText(Footer, FLinearColor(0.8f, 0.7f, 0.7f),
		CentreX - FootW * 0.5f, CentreY + 40.0f, GEngine->GetMediumFont(), 1.0f);
}

void ADCCHUD::DrawInteractionPrompt(ADCCCharacterBase* Crawler)
{
	if (!Crawler || !GetWorld())
	{
		return;
	}

	// Three mutually exclusive states, checked most-committed first: already on a
	// wall, being asked about one, or looking for one.
	FString Prompt;
	FLinearColor Colour = HeadingColour;

	if (Crawler->IsClimbing())
	{
		Prompt = TEXT("Hold [LMB] to climb up, [RMB] to descend.  [Space] to let go.");
		Colour = BodyColour;
	}
	else if (const ADCCClimbPoint* Pending = Crawler->GetPendingClimbPoint())
	{
		// The question itself is authored on the climb point, so a particular
		// cliff can ask in its own voice.
		Prompt = FString::Printf(TEXT("%s   [Y] yes   [N] no"), *Pending->PromptText.ToString());
	}
	else if (!Crawler->GetSelectedSkillId().IsNone())
	{
		Prompt = TEXT("Click a surface to use the armed skill.");
		Colour = DimColour;
	}
	else
	{
		return;
	}

	float W = 0.0f, H = 0.0f;
	GetTextSize(Prompt, W, H, GEngine->GetMediumFont(), 1.0f);

	const float BoxX = Canvas->SizeX * 0.5f - W * 0.5f - 20.0f;
	const float BoxY = Canvas->SizeY * 0.62f;

	DrawRect(FLinearColor(0.02f, 0.03f, 0.06f, 0.78f), BoxX, BoxY - 8.0f, W + 40.0f, H + 20.0f);
	DrawText(Prompt, Colour, BoxX + 20.0f, BoxY, GEngine->GetMediumFont(), 1.0f);
}

void ADCCHUD::DrawStaminaBar(ADCCCharacterBase* Crawler)
{
	UDCCStatsComponent* Stats = Crawler ? Crawler->Stats : nullptr;
	if (!Stats || Stats->MaxStamina <= 0.0f)
	{
		return;
	}

	const float Fraction = Stats->GetStaminaFraction();

	// Hidden at full stamina unless it is actively being spent. A bar that is
	// always there while nothing is draining it just teaches players to ignore it.
	if (Fraction >= 0.999f && !Crawler->IsSprinting() && !Crawler->IsClimbing())
	{
		return;
	}

	constexpr float BarW = 260.0f;
	constexpr float BarH = 12.0f;

	const float BarX = Canvas->SizeX * 0.5f - BarW * 0.5f;
	const float BarY = Canvas->SizeY * 0.86f;

	DrawRect(FLinearColor(0.02f, 0.03f, 0.06f, 0.72f), BarX - 2.0f, BarY - 2.0f, BarW + 4.0f, BarH + 4.0f);

	// Reddens as it empties, so the "you are about to fall off this cliff"
	// warning is peripheral and does not need reading.
	const FLinearColor Fill = FMath::Lerp(
		FLinearColor(0.85f, 0.25f, 0.15f),
		FLinearColor(0.35f, 0.80f, 0.95f),
		FMath::Clamp(Fraction, 0.0f, 1.0f));

	DrawRect(Fill, BarX, BarY, BarW * FMath::Clamp(Fraction, 0.0f, 1.0f), BarH);
	DrawText(TEXT("Stamina"), DimColour, BarX, BarY - LineHeight);
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
	DrawText(FString::Printf(TEXT("  Stamina  %.0f / %.0f"), Stats->CurrentStamina, Stats->MaxStamina), BodyColour, X, Y);
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

	for (int32 Index = 0; Index < Skills->KnownSkills.Num(); ++Index)
	{
		const FDCCKnownSkill& Known = Skills->KnownSkills[Index];

		FDCCSkillDef Definition;
		const bool bResolved = Skills->FindSkill(Known.SkillId, Definition);

		const FString Name = bResolved && !Definition.DisplayName.IsEmpty()
			? Definition.DisplayName.ToString()
			: Known.SkillId.ToString();

		// The number is the key that arms this skill, so the list doubles as the
		// keybind reference rather than needing a second one somewhere else.
		const FString Slot = Index < 9
			? FString::Printf(TEXT("[%d]"), Index + 1)
			: TEXT("   ");

		// The advancement mark is shown because it is otherwise invisible state
		// that decides whether this skill can rank up at the next interval.
		const FString Line = FString::Printf(TEXT("  %s %-22s Rank %-2d %s"),
			*Slot, *Name, Known.Rank, Known.bAdvancementMark ? TEXT("*") : TEXT(""));

		const bool bArmed = Crawler->GetSelectedSkillId() == Known.SkillId;

		DrawText(Line, bArmed ? HeadingColour : BodyColour, X, Y);
		Y += LineHeight;
	}

	DrawText(TEXT("  * used since last advancement"), DimColour, X, Y);
	Y += LineHeight;

	if (!Crawler->GetSelectedSkillId().IsNone())
	{
		DrawText(TEXT("  Armed - click a target in the world"), HeadingColour, X, Y);
		Y += LineHeight;
	}

	return Y;
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
