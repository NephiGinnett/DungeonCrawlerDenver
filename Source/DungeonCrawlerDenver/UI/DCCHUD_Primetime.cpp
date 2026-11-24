// PRIMETIME's HUD (2026-11-23, Docs/dead-air-primetime-plan.md): the show's banner, where to go, Hiss's couch with its
// three answer buttons, his verdict and the final ratings. Everything else (his voice and subtitles) comes through
// ShowBroadcast and the gold lower third.

#include "UI/DCCHUD.h"

#include "Arcade/DCCArcadeHaunt.h"
#include "Camera/PlayerCameraManager.h"
#include "Engine/Canvas.h"
#include "Engine/Engine.h"
#include "Engine/World.h"
#include "EngineUtils.h"
#include "GameFramework/PlayerController.h"
#include "GameFramework/PlayerState.h"

namespace
{
	const FLinearColor Gold(1.0f, 0.78f, 0.25f);
	const FLinearColor Ink(1.0f, 1.0f, 1.0f);
	const FLinearColor Dim(0.78f, 0.74f, 0.88f);
	const FLinearColor Panel(0.025f, 0.015f, 0.05f, 0.9f);
	const FLinearColor Pink(1.0f, 0.35f, 0.75f);
	const FLinearColor Good(0.45f, 1.0f, 0.6f);
	const FLinearColor Bad(1.0f, 0.38f, 0.35f);

	FLinearColor WithAlpha(FLinearColor C, float A)
	{
		C.A = A;
		return C;
	}
}

void ADCCHUD::DrawPrimetime()
{
	if (!Canvas || !GetWorld() || !PlayerOwner)
	{
		return;
	}
	const ADCCArcadeHaunt* Show = nullptr;
	for (TActorIterator<ADCCArcadeHaunt> It(GetWorld()); It; ++It)
	{
		Show = *It;
		break;
	}
	if (!Show || !Show->bPrimetime)
	{
		return;
	}
	const FString Me = PlayerOwner->PlayerState ? PlayerOwner->PlayerState->GetPlayerName() : FString();
	const FDCCShowGuest* Mine = Show->FindGuest(Me);
	const float W = Canvas->SizeX, H = Canvas->SizeY;
	const double Now = Show->NowServer();
	const float Left = FMath::Max(0.0f, float(Show->PhaseEndsAt - Now));
	UFont* Large = GEngine->GetLargeFont();
	UFont* Medium = GEngine->GetMediumFont();
	UFont* Small = GEngine->GetSmallFont();

	auto Centred = [this](const FString& Text, UFont* Font, float Scale, const FLinearColor& Colour, float CX, float Y)
	{
		float TW = 0.0f, TH = 0.0f;
		GetTextSize(Text, TW, TH, Font, Scale);
		DrawText(Text, Colour, CX - TW * 0.5f, Y, Font, Scale);
		return TH;
	};

	// ---- the banner ----
	FString Main, Sub;
	FLinearColor SubColour = Dim;
	switch (Show->ShowPhase)
	{
	case 0:
		Main = TEXT("THE SHOW STARTS IN A MOMENT");
		Sub = TEXT("Stand on your mark and smile.");
		break;
	case 1:
		Main = FString::Printf(TEXT("LEVEL %d OF %d   %s   %d:%02d"), Show->ShowLevel, ADCCArcadeHaunt::ShowLevels,
			ADCCArcadeHaunt::RoundName(Show->RoundKind), FMath::FloorToInt(Left / 60.0f), FMath::FloorToInt(FMath::Fmod(Left, 60.0f)));
		if (Show->RoundKind == 0)
		{
			Sub = TEXT("Get everyone down to the stope. First one there gets a bonus.");
		}
		else if (Show->RoundKind == 1)
		{
			int32 Done = 0;
			for (bool b : Show->GoalsDone) { Done += b ? 1 : 0; }
			Sub = FString::Printf(TEXT("Throw the switches in the dark: %d of %d"), Done, Show->Goals.Num());
		}
		else
		{
			Sub = FString::Printf(TEXT("Hold the ring in the stope: %d%%"), FMath::RoundToInt(Show->HoldProgress * 100.0f));
		}
		SubColour = Gold;
		break;
	case 2:
		Main = TEXT("THE COUCH");
		Sub = Show->Interviewee.IsEmpty() ? FString(TEXT("Hiss has a few words first.")) : FString::Printf(TEXT("In the hot seat: %s"), *Show->Interviewee);
		break;
	case 3:
		Main = FString::Printf(TEXT("NEXT: LEVEL %d   %s"), Show->ShowLevel, ADCCArcadeHaunt::RoundName(Show->RoundKind));
		Sub = TEXT("Hiss is deciding how much he likes you.");
		break;
	default:
		Main = TEXT("THAT'S THE SHOW");
		Sub = TEXT("Ratings are in.");
		break;
	}
	{
		float MW = 0.0f, MH = 0.0f, SW = 0.0f, SH = 0.0f;
		GetTextSize(Main, MW, MH, Large, 1.0f);
		GetTextSize(Sub, SW, SH, Medium, 1.0f);
		const float BW = FMath::Max(MW, SW) + 80.0f, BH = MH + SH + 40.0f;
		const float BX = (W - BW) * 0.5f, BY = 14.0f;
		DrawStyledRect(Panel, BX, BY, BW, BH, 10.0f, WithAlpha(Gold, 0.6f));
		// PRIMETIME, with the blinking LIVE dot, on a tab above it.
		const bool bOn = FMath::Fmod(GetWorld()->GetRealTimeSeconds(), 1.2f) < 0.8f;
		DrawStyledRect(Gold, W * 0.5f - 70.0f, BY - 2.0f, 140.0f, 20.0f, 6.0f, FLinearColor::Transparent);
		DrawRect(FLinearColor(0.85f, 0.08f, 0.08f, bOn ? 1.0f : 0.3f), W * 0.5f - 58.0f, BY + 5.0f, 7.0f, 7.0f);
		DrawText(TEXT("PRIMETIME  LIVE"), FLinearColor(0.08f, 0.04f, 0.02f), W * 0.5f - 46.0f, BY + 1.0f, Small, 1.0f);
		Centred(Main, Large, 1.0f, Ink, W * 0.5f, BY + 22.0f);
		Centred(Sub, Medium, 1.0f, SubColour, W * 0.5f, BY + 26.0f + MH);
		float Y = BY + BH + 6.0f;
		// The team's Blessing and Twist on this level, and your own.
		auto Chip = [&](const FString& Text, const FLinearColor& Colour)
		{
			float TW = 0.0f, TH = 0.0f;
			GetTextSize(Text, TW, TH, Small, 1.0f);
			DrawStyledRect(FLinearColor(Colour.R * 0.15f, Colour.G * 0.15f, Colour.B * 0.15f, 0.9f), (W - TW) * 0.5f - 12.0f, Y, TW + 24.0f, TH + 8.0f, 6.0f,
				WithAlpha(Colour, 0.7f));
			DrawText(Text, Colour, (W - TW) * 0.5f, Y + 4.0f, Small, 1.0f);
			Y += TH + 12.0f;
		};
		if (Show->ShowPhase == 1 || Show->ShowPhase == 3)
		{
			if (Show->TeamBlessing) { Chip(FString::Printf(TEXT("BLESSING  %s"), ADCCArcadeHaunt::BlessingName(Show->TeamBlessing)), Good); }
			if (Show->TeamTwist) { Chip(FString::Printf(TEXT("TWIST  %s"), ADCCArcadeHaunt::TwistName(Show->TeamTwist)), Bad); }
			if (Mine && Mine->Personal == 1) { Chip(TEXT("PRODUCER'S PET  Hiss pays back half of every hit you take"), Good); }
			if (Mine && Mine->Personal == 2) { Chip(TEXT("MARKED  a pair of slag dogs has your scent"), Bad); }
		}
	}

	// ---- in the audience ----
	if (Mine && Mine->bOut && (Show->ShowPhase == 1 || Show->ShowPhase == 2 || Show->ShowPhase == 3))
	{
		const FString Text = Show->ShowPhase == 1 ? TEXT("IN THE AUDIENCE   watching the crew on the big screen") : TEXT("IN THE AUDIENCE");
		float TW = 0.0f, TH = 0.0f;
		GetTextSize(Text, TW, TH, Medium, 1.0f);
		DrawStyledRect(Panel, (W - TW) * 0.5f - 18.0f, H - 96.0f, TW + 36.0f, TH + 14.0f, 8.0f, WithAlpha(Pink, 0.6f));
		DrawText(Text, Pink, (W - TW) * 0.5f, H - 89.0f, Medium, 1.0f);
	}

	// ---- where to go ----
	if (Show->ShowPhase == 1 && Mine && !Mine->bOut)
	{
		const APawn* Pawn = PlayerOwner->GetPawn();
		for (int32 g = 0; g < Show->Goals.Num(); ++g)
		{
			if (Show->GoalsDone.IsValidIndex(g) && Show->GoalsDone[g] && Show->RoundKind != 0)
			{
				continue;
			}
			const FVector World = Show->Goals[g] + FVector(0.0f, 0.0f, 160.0f);
			FVector2D Screen;
			const bool bOnScreen = PlayerOwner->ProjectWorldLocationToScreen(World, Screen, true)
				&& Screen.X > 30.0f && Screen.X < W - 30.0f && Screen.Y > 30.0f && Screen.Y < H - 30.0f;
			const FVector Eye = PlayerOwner->PlayerCameraManager ? PlayerOwner->PlayerCameraManager->GetCameraLocation() : World;
			const bool bBehind = FVector::DotProduct(World - Eye, PlayerOwner->GetControlRotation().Vector()) < 0.0f;
			if (!bOnScreen || bBehind)
			{
				// Off screen: a chip on the edge, in the direction of the goal.
				const FVector2D Centre(W * 0.5f, H * 0.5f);
				FVector2D Dir = bBehind ? FVector2D(Centre - Screen) : FVector2D(Screen - Centre);
				if (Dir.IsNearlyZero()) { Dir = FVector2D(0.0f, 1.0f); }
				Dir.Normalize();
				const float Scale = FMath::Min((W * 0.5f - 60.0f) / FMath::Max(0.01f, FMath::Abs(Dir.X)), (H * 0.5f - 60.0f) / FMath::Max(0.01f, FMath::Abs(Dir.Y)));
				Screen = Centre + Dir * Scale;
			}
			const float Dist = Pawn ? FVector::Dist(Pawn->GetActorLocation(), Show->Goals[g]) / 100.0f : 0.0f;
			const FString Label = Show->RoundKind == 1 ? FString::Printf(TEXT("SWITCH  %.0f m"), Dist)
				: Show->RoundKind == 0 ? FString::Printf(TEXT("STOPE  %.0f m"), Dist)
				: FString::Printf(TEXT("HOLD  %d%%  %.0f m"), FMath::RoundToInt(Show->HoldProgress * 100.0f), Dist);
			const float Pulse = 0.7f + 0.3f * FMath::Sin(GetWorld()->GetRealTimeSeconds() * 4.0f);
			DrawStyledRect(WithAlpha(Gold, 0.9f * Pulse), Screen.X - 9.0f, Screen.Y - 9.0f, 18.0f, 18.0f, 9.0f, FLinearColor::Transparent);
			float TW = 0.0f, TH = 0.0f;
			GetTextSize(Label, TW, TH, Small, 1.0f);
			DrawStyledRect(Panel, Screen.X - TW * 0.5f - 8.0f, Screen.Y + 14.0f, TW + 16.0f, TH + 6.0f, 5.0f, WithAlpha(Gold, 0.5f));
			DrawText(Label, Gold, Screen.X - TW * 0.5f, Screen.Y + 17.0f, Small, 1.0f);
		}
	}

	// ---- the couch: everyone's standing with Hiss, and the hot seat's buttons ----
	if (Show->ShowPhase == 2 || Show->ShowPhase == 3 || Show->ShowPhase == 4)
	{
		TArray<const FDCCShowGuest*> Order;
		for (const FDCCShowGuest& G : Show->Guests) { Order.Add(&G); }
		if (Show->ShowPhase == 4)
		{
			Order.Sort([](const FDCCShowGuest& A, const FDCCShowGuest& B) { return A.Ratings > B.Ratings; });
		}
		const float PX = 24.0f, PW = 360.0f;
		float Y = 150.0f;
		const float RowH = 46.0f;
		DrawStyledRect(Panel, PX, Y, PW, 38.0f + Order.Num() * RowH, 10.0f, WithAlpha(Gold, 0.5f));
		DrawText(Show->ShowPhase == 4 ? TEXT("FINAL RATINGS") : TEXT("HISS'S MOOD"), Gold, PX + 16.0f, Y + 10.0f, Medium, 1.0f);
		Y += 38.0f;
		for (const FDCCShowGuest* G : Order)
		{
			const bool bHot = G->Player == Show->Interviewee;
			if (bHot)
			{
				DrawStyledRect(WithAlpha(Gold, 0.18f), PX + 6.0f, Y, PW - 12.0f, RowH - 4.0f, 6.0f, FLinearColor::Transparent);
			}
			DrawText(G->Player, G->bOut ? Dim : Ink, PX + 16.0f, Y + 4.0f, Medium, 1.0f);
			DrawText(G->bOut ? FString::Printf(TEXT("OUT on level %d"), G->OutOnLevel) : FString::Printf(TEXT("%d ratings"), G->Ratings),
				G->bOut ? Bad : Dim, PX + 16.0f, Y + 24.0f, Small, 1.0f);
			// Seven pips, cold red to adoring pink, his mood lit.
			for (int32 p = -3; p <= 3; ++p)
			{
				const FLinearColor C = p < 0 ? Bad : p > 0 ? Pink : Dim;
				const bool bLit = p == G->Mood;
				DrawStyledRect(bLit ? C : WithAlpha(C, 0.22f), PX + 190.0f + (p + 3) * 22.0f, Y + 12.0f, bLit ? 16.0f : 12.0f, bLit ? 16.0f : 12.0f, 8.0f,
					FLinearColor::Transparent);
			}
			Y += RowH;
		}
	}

	if (Show->ShowPhase == 2 && !Show->Interviewee.IsEmpty())
	{
		const FDCCShowGuest* Hot = Show->FindGuest(Show->Interviewee);
		if (Show->Interviewee == Me && Hot && Hot->Answer < 0)
		{
			// The buttons: what you say decides how he treats you next level.
			struct FChoice { const TCHAR* Name; const TCHAR* What; FLinearColor Colour; };
			const FChoice Choices[] = {
				{ TEXT("CHARM"), TEXT("He warms up to you"), Pink },
				{ TEXT("BANTER"), TEXT("Good television"), Gold },
				{ TEXT("DEFY"), TEXT("He cools. The crowd goes wild."), Bad },
			};
			const float BW = 230.0f, BH = 74.0f, Gap = 18.0f;
			const float TotalW = BW * 3.0f + Gap * 2.0f;
			const float X0 = (W - TotalW) * 0.5f, Y0 = H * 0.42f;
			DrawStyledRect(Panel, X0 - 24.0f, Y0 - 64.0f, TotalW + 48.0f, BH + 96.0f, 12.0f, WithAlpha(Gold, 0.7f));
			Centred(FString::Printf(TEXT("ANSWER HISS   %ds"), FMath::CeilToInt(Left)), Large, 1.0f, Gold, W * 0.5f, Y0 - 52.0f);
			Centred(TEXT("How you answer decides how he treats you on the next level."), Small, 1.0f, Dim, W * 0.5f, Y0 - 22.0f);
			for (int32 i = 0; i < 3; ++i)
			{
				const float BX = X0 + i * (BW + Gap);
				DrawStyledRect(FLinearColor(Choices[i].Colour.R * 0.22f, Choices[i].Colour.G * 0.22f, Choices[i].Colour.B * 0.22f, 0.95f), BX, Y0, BW, BH, 10.0f,
					Choices[i].Colour);
				Centred(Choices[i].Name, Large, 1.0f, Choices[i].Colour, BX + BW * 0.5f, Y0 + 12.0f);
				Centred(Choices[i].What, Small, 1.0f, Dim, BX + BW * 0.5f, Y0 + 46.0f);
				AddUiHitBox(FVector2D(BX, Y0), FVector2D(BW, BH), FName(*FString::Printf(TEXT("Prime|%d"), i)), true, 200);
			}
			if (!bPrimeCursor)
			{
				bPrimeCursor = true;
				PlayerOwner->bShowMouseCursor = true;
				PlayerOwner->bEnableClickEvents = true;
				PlayerOwner->bEnableMouseOverEvents = true;
			}
		}
		else if (Hot && Hot->Answer >= 0)
		{
			static const TCHAR* Said[] = { TEXT("CHARMED HIM"), TEXT("BANTERED"), TEXT("DEFIED HIM") };
			const FString Text = FString::Printf(TEXT("%s  %s"), *Hot->Player, Said[FMath::Clamp(Hot->Answer, 0, 2)]);
			float TW = 0.0f, TH = 0.0f;
			GetTextSize(Text, TW, TH, Large, 1.0f);
			const FLinearColor C = Hot->Answer == 0 ? Pink : Hot->Answer == 1 ? Gold : Bad;
			DrawStyledRect(Panel, (W - TW) * 0.5f - 24.0f, H * 0.42f, TW + 48.0f, TH + 20.0f, 10.0f, WithAlpha(C, 0.7f));
			DrawText(Text, C, (W - TW) * 0.5f, H * 0.42f + 10.0f, Large, 1.0f);
		}
	}
	// The cursor goes back the moment the buttons do.
	const bool bButtons = Show->ShowPhase == 2 && Show->Interviewee == Me && Mine && Mine->Answer < 0;
	if (bPrimeCursor && !bButtons)
	{
		bPrimeCursor = false;
		ApplyCursorForOpenPanels();
	}
}
