// Repo Rally's screens (2026-11-14, Zoe: "the game just needs a bit more pizzazz. Nice full menus, clear organization").
//
// Three phases, one look (dark violet panels, a neon strip on top, key caps for every control):
//   Before the flag   the WORK ORDER: tonight's marks, how a repo works in four steps, what pays, the controls.
//   During the shift  each thing in its own corner: the clock top middle, THE BOARD (marks) top left, CREW
//                     (standings) top right, YOUR TRUCK (fuel, tab, tool, radio) bottom right, the map bottom left.
//   After the shift   the SHIFT REPORT: everyone's drops, fastest, fines, bank and tickets, above the vote buttons.
// The data is replicated on the arena (ADCCArcadeKart::Crew / Board / FlagAt / RallyWave). This replaces the
// centred text box every arcade game shares (DrawArcadeStatus), for Repo Rally only.

#include "UI/DCCHUD.h"

#include "Arcade/DCCArcadeGames.h"
#include "Arcade/DCCChopShop.h"
#include "Arcade/DCCRallyItems.h"
#include "Arcade/DCCRallyThug.h"
#include "Arcade/DCCWalletComponent.h"
#include "Camera/PlayerCameraManager.h"
#include "CanvasItem.h"
#include "Fonts/FontMeasure.h"
#include "Framework/Application/SlateApplication.h"
#include "Rendering/SlateRenderer.h"
#include "Styling/CoreStyle.h"
#include "Vehicles/DCCRover.h"
#include "Characters/DCCCharacterBase.h"
#include "Skills/DCCSkillComponent.h"
#include "Session/DCCSession.h"
#include "Engine/Canvas.h"
#include "Engine/Engine.h"
#include "Engine/Font.h"
#include "EngineUtils.h"
#include "GameFramework/PlayerController.h"
#include "InputCoreTypes.h"
#include "GameFramework/PlayerState.h"

namespace DCCRallyUI
{
	const FLinearColor Back(0.035f, 0.018f, 0.065f, 0.88f);
	const FLinearColor Back2(0.07f, 0.04f, 0.12f, 0.9f);
	const FLinearColor Pink(1.0f, 0.32f, 0.82f);
	const FLinearColor Cyan(0.3f, 0.9f, 1.0f);
	const FLinearColor Amber(1.0f, 0.76f, 0.32f);
	const FLinearColor Green(0.4f, 1.0f, 0.55f);
	const FLinearColor Red(1.0f, 0.3f, 0.3f);
	const FLinearColor Ink(0.94f, 0.92f, 0.98f);
	const FLinearColor Dim(0.66f, 0.62f, 0.76f);

	const TCHAR* RoleName(int32 Role)
	{
		switch (Role)
		{
		case 1: return TEXT("RUNNER");
		case 2: return TEXT("NET");
		case 3: return TEXT("WINCH");
		case 4: return TEXT("TANK");
		default: return TEXT("STOCK");
		}
	}

	const TCHAR* WaveName(int32 Wave)
	{
		switch (Wave)
		{
		case 1: return TEXT("BOXED IN");
		case 2: return TEXT("RUNNERS");
		case 3: return TEXT("SKITTISH");
		default: return TEXT("PARKED");
		}
	}

	void StateText(const FDCCRallyMarkRow& Row, FString& Out, FLinearColor& Colour)
	{
		switch (Row.State)
		{
		case 0: Out = TEXT("parked"); Colour = Dim; break;
		case 1: Out = TEXT("boxed in"); Colour = Amber; break;
		case 2: Out = TEXT("running"); Colour = Amber; break;
		case 3: Out = TEXT("skittish"); Colour = Pink; break;
		case 4: Out = TEXT("SPOOKED - in the air"); Colour = Red; break;
		case 5: Out = FString::Printf(TEXT("on %s's line"), *Row.Tower); Colour = Cyan; break;
		case 6: Out = FString::Printf(TEXT("%s is towing it"), *Row.Tower); Colour = Green; break;
		default: Out = TEXT("new mark coming"); Colour = Dim * 0.8f; break;
		}
	}
}

// ---- crisp text ---------------------------------------------------------------------------------------------

namespace DCCRallyUI
{
	/** The Slate font that matches a bitmap engine font at a scale: Large reads as the bold title face. */
	FSlateFontInfo SlateFor(UFont* Font, float Scale)
	{
		const bool bLarge = Font == GEngine->GetLargeFont();
		const bool bMedium = Font == GEngine->GetMediumFont();
		// Pixel sizes chosen to match the bitmap fonts' line heights at scale 1.
		const float Base = bLarge ? 11.0f : bMedium ? 10.0f : 7.6f;
		// Canvas only draws a Slate font that has a font asset behind it, so this is the engine's Roboto asset,
		// not FCoreStyle's default (which drew nothing at all on a Canvas).
		static UFont* Roboto = LoadObject<UFont>(nullptr, TEXT("/Engine/EngineFonts/Roboto.Roboto"));
		return FSlateFontInfo(Roboto, FMath::Max(6, FMath::RoundToInt(Base * Scale)), bLarge || bMedium ? FName(TEXT("Bold")) : FName(TEXT("Regular")));
	}

	FVector2D Measure(const FString& Text, const FSlateFontInfo& Info)
	{
		if (!FSlateApplication::IsInitialized())
		{
			return FVector2D(Text.Len() * Info.Size * 0.6f, Info.Size * 1.3f);
		}
		return FSlateApplication::Get().GetRenderer()->GetFontMeasureService()->Measure(Text, Info);
	}
}

void ADCCHUD::RallyText(const FString& Text, const FLinearColor& Colour, float X, float Y, UFont* Font, float Scale, bool bShadow)
{
	if (!Canvas || Text.IsEmpty())
	{
		return;
	}
	FCanvasTextItem Item(FVector2D(X, Y), FText::FromString(Text), DCCRallyUI::SlateFor(Font, Scale), Colour);
	if (bShadow)
	{
		Item.EnableShadow(FLinearColor(0.0f, 0.0f, 0.0f, 0.55f * Colour.A), FVector2D(1.0f, 1.0f));
	}
	Canvas->DrawItem(Item);
}

void ADCCHUD::RallyTextSize(const FString& Text, float& OutW, float& OutH, UFont* Font, float Scale)
{
	const FVector2D Size = DCCRallyUI::Measure(Text, DCCRallyUI::SlateFor(Font, Scale));
	OutW = Size.X;
	OutH = Size.Y;
}

// ---- small drawing pieces ---------------------------------------------------------------------------------

void ADCCHUD::RallyPanel(float X, float Y, float W, float H, const FLinearColor& Accent)
{
	// A soft drop shadow, the body, a tinted header band, the neon strip on top, and corner brackets
	// (2026-11-14 polish pass: "menus like the board can also just use a bit more work and polish").
	DrawRect(FLinearColor(0.0f, 0.0f, 0.0f, 0.35f), X + 5.0f, Y + 6.0f, W, H);
	DrawRect(DCCRallyUI::Back, X, Y, W, H);
	const float Band = FMath::Min(H, 40.0f + 0.02f * H);
	for (int32 i = 0; i < 8; ++i)
	{
		const float A = 0.16f * (1.0f - i / 8.0f);
		DrawRect(FLinearColor(Accent.R, Accent.G, Accent.B, A), X, Y + i * Band / 8.0f, W, Band / 8.0f);
	}
	DrawRect(Accent, X, Y, W, 3.0f);
	const FLinearColor Edge(Accent.R, Accent.G, Accent.B, 0.22f);
	DrawRect(Edge, X, Y + H - 1.0f, W, 1.0f);
	DrawRect(Edge, X, Y, 1.0f, H);
	DrawRect(Edge, X + W - 1.0f, Y, 1.0f, H);
	const float L = 14.0f;
	DrawRect(Accent, X, Y + H - 2.0f, L, 2.0f);
	DrawRect(Accent, X, Y + H - L, 2.0f, L);
	DrawRect(Accent, X + W - L, Y + H - 2.0f, L, 2.0f);
	DrawRect(Accent, X + W - 2.0f, Y + H - L, 2.0f, L);
}

float ADCCHUD::RallyPill(float X, float Y, const FString& Text, const FLinearColor& Colour, float S)
{
	UFont* Font = GEngine->GetSmallFont();
	float W = 0.0f, H = 0.0f;
	RallyTextSize(Text, W, H, Font, 1.05f * S);
	const float PadX = 7.0f * S;
	// A dark base first, so a pill reads over a bright neon pad as well as over a panel.
	DrawRect(FLinearColor(0.03f, 0.015f, 0.05f, 0.82f), X, Y, W + PadX * 2.0f, H + 4.0f * S);
	DrawRect(FLinearColor(Colour.R, Colour.G, Colour.B, 0.2f), X, Y, W + PadX * 2.0f, H + 4.0f * S);
	DrawRect(Colour, X, Y, 2.0f * S, H + 4.0f * S);
	RallyText(Text, Colour, X + PadX, Y + 2.0f * S, Font, 1.05f * S);
	return W + PadX * 2.0f;
}

void ADCCHUD::RallyArrow(float CX, float CY, float Size, float Angle, const FLinearColor& Colour)
{
	const float R = FMath::DegreesToRadians(Angle);
	auto P = [&](float FX, float FY)
	{
		// FY is "forward" (up the screen), FX is right.
		return FVector2D(CX + FX * FMath::Cos(R) + FY * FMath::Sin(R), CY + FX * FMath::Sin(R) - FY * FMath::Cos(R));
	};
	const FVector2D Tip = P(0.0f, Size), L = P(-Size * 0.7f, -Size * 0.6f), Rt = P(Size * 0.7f, -Size * 0.6f), Back = P(0.0f, -Size * 0.2f);
	const float Th = FMath::Max(2.0f, Size * 0.28f);
	DrawLine(Tip.X, Tip.Y, L.X, L.Y, Colour, Th);
	DrawLine(Tip.X, Tip.Y, Rt.X, Rt.Y, Colour, Th);
	DrawLine(L.X, L.Y, Back.X, Back.Y, Colour, Th);
	DrawLine(Rt.X, Rt.Y, Back.X, Back.Y, Colour, Th);
}

float ADCCHUD::RallyKey(float X, float Y, const FString& Key, float S)
{
	UFont* Font = GEngine->GetSmallFont();
	float W = 0.0f, H = 0.0f;
	RallyTextSize(Key, W, H, Font, 1.15f * S);
	const float Pad = 7.0f * S;
	const float BoxW = FMath::Max(W + Pad * 2.0f, H + Pad);
	const float BoxH = H + 6.0f * S;
	DrawRect(FLinearColor(0.9f, 0.88f, 1.0f, 0.95f), X, Y, BoxW, BoxH);
	DrawRect(FLinearColor(0.55f, 0.5f, 0.7f, 1.0f), X, Y + BoxH - 3.0f * S, BoxW, 3.0f * S);
	RallyText(Key, FLinearColor(0.08f, 0.04f, 0.14f), X + (BoxW - W) * 0.5f, Y + 2.0f * S, Font, 1.15f * S);
	return BoxW;
}

void ADCCHUD::RallyBar(float X, float Y, float W, float H, float Fraction, const FLinearColor& Colour)
{
	DrawRect(FLinearColor(1.0f, 1.0f, 1.0f, 0.12f), X, Y, W, H);
	DrawRect(Colour, X, Y, W * FMath::Clamp(Fraction, 0.0f, 1.0f), H);
}

// ---- the dispatcher ---------------------------------------------------------------------------------------

void ADCCHUD::DrawRallyScreens(const ADCCArcadeKart* Kart)
{
	if (!Canvas || !Kart)
	{
		return;
	}
	if (Kart->bRoundOver)
	{
		DrawRallyReport(Kart);
		return;
	}
	if (!Kart->IsShiftOn())
	{
		// Remembered for this flag only. The next shift's work order comes back.
		static double ClosedForFlag = -1.0;
		if (PlayerOwner && (PlayerOwner->WasInputKeyJustPressed(EKeys::Enter) || PlayerOwner->WasInputKeyJustPressed(EKeys::Escape)))
		{
			ClosedForFlag = Kart->FlagAt;
			MarkGuideSeen(TEXT("RallyWorkOrder"));
		}
		if (ClosedForFlag != Kart->FlagAt)
		{
			DrawRallyBriefing(Kart);
			return;
		}
		const float S = FMath::Clamp(Canvas->SizeY / 1080.0f, 0.6f, 2.0f);
		const int32 Left = FMath::Max(0, FMath::CeilToInt(Kart->FlagAt - Kart->GetNow()));
		using namespace DCCRallyUI;
		RallyText(FString::Printf(TEXT("FLAG IN  %d"), Left), Amber, 28.0f * S, 24.0f * S, GEngine->GetLargeFont(), 1.6f * S);
	}
	else
	{
		MarkGuideSeen(TEXT("RallyWorkOrder")); // the shift has started: the lesson has been read
		DrawRallyClock(Kart);
		DrawRallyBoard(Kart);
		DrawRallyCrew(Kart);
		DrawRallyTruck(Kart, GetOwningRover());
		DrawRallyTrouble(Kart);
	}
	if (!GetOwningRover())
	{
		using namespace DCCRallyUI;
		const float S = FMath::Clamp(Canvas->SizeY / 1080.0f, 0.6f, 2.0f);
		const float KW = RallyKey(28.0f * S, Canvas->SizeY - 72.0f * S, TEXT("E"), S);
		RallyText(TEXT("get in your truck"), Ink, 36.0f * S + KW, Canvas->SizeY - 68.0f * S, GEngine->GetMediumFont(), 1.15f * S);
	}
}

// ---- before the flag: the work order ----------------------------------------------------------------------

void ADCCHUD::DrawRallyBriefing(const ADCCArcadeKart* Kart)
{
	using namespace DCCRallyUI;
	const float S = FMath::Clamp(Canvas->SizeY / 1080.0f, 0.6f, 2.0f);
	UFont* Small = GEngine->GetSmallFont();
	UFont* Medium = GEngine->GetMediumFont();
	UFont* Large = GEngine->GetLargeFont();
	const float T = float(GetWorld()->GetRealTimeSeconds());
	const ADCCRover* Me = GetOwningRover();

	const float W = FMath::Min(1180.0f * S, Canvas->SizeX - 48.0f);
	const bool bFirst = !IsGuideSeen(TEXT("RallyWorkOrder"));
	const float H = (bFirst ? 600.0f : 336.0f) * S;
	const float X = (Canvas->SizeX - W) * 0.5f;
	const float Y = FMath::Max(24.0f, Canvas->SizeY * 0.07f);
	RallyPanel(X, Y, W, H, Pink);

	// Header.
	const float P = 28.0f * S;
	RallyText(TEXT("MILE HIGH RECOVERY SOLUTIONS  -  IMPOUND DISPATCH"), Dim, X + P, Y + 18.0f * S, Small, 1.2f * S);
	RallyText(TEXT("NIGHT SHIFT WORK ORDER"), Pink, X + P, Y + 36.0f * S, Large, 2.2f * S);
	const int32 Left = FMath::Max(0, FMath::CeilToInt(Kart->FlagAt - Kart->GetNow()));
	{
		const FString Count = FString::Printf(TEXT("%d"), Left);
		float CW = 0.0f, CH = 0.0f;
		RallyTextSize(Count, CW, CH, Large, 3.4f * S);
		const float Pulse = Left <= 5 ? 0.6f + 0.4f * FMath::Abs(FMath::Sin(T * 6.0f)) : 1.0f;
		RallyText(TEXT("FLAG DROPS IN"), Dim, X + W - P - 230.0f * S, Y + 18.0f * S, Small, 1.2f * S);
		RallyText(Count, Amber * Pulse, X + W - P - CW, Y + 22.0f * S, Large, 3.4f * S);
	}
	DrawRect(FLinearColor(Pink.R, Pink.G, Pink.B, 0.35f), X + P, Y + 100.0f * S, W - P * 2.0f, 1.0f);

	// Left column: tonight's marks.
	const float ColY = Y + 118.0f * S;
	const float LX = X + P, LW = W * 0.38f;
	RallyText(TEXT("TONIGHT'S MARKS"), Cyan, LX, ColY - 4.0f * S, GEngine->GetLargeFont(), 1.7f * S);
	float RowY = ColY + 34.0f * S;
	for (const FDCCRallyMarkRow& Row : Kart->Board)
	{
		const FLinearColor C = ADCCArcadeKart::MarkColour(Row.Plate);
		DrawRect(Back2, LX, RowY, LW, 44.0f * S);
		// The same numbered colour tile the board and the world tags use.
		const int32 Num = Kart->Board.IndexOfByPredicate([&Row](const FDCCRallyMarkRow& R) { return R.Plate == Row.Plate; }) + 1;
		DrawRect(FLinearColor(C.R, C.G, C.B, 0.3f), LX, RowY, 44.0f * S, 44.0f * S);
		DrawRect(C, LX, RowY + 41.0f * S, 44.0f * S, 3.0f * S);
		{
			const FString N = FString::FromInt(Num);
			float NW = 0.0f, NH = 0.0f;
			RallyTextSize(N, NW, NH, GEngine->GetLargeFont(), 1.4f * S);
			RallyText(N, Ink, LX + (44.0f * S - NW) * 0.5f, RowY + (44.0f * S - NH) * 0.5f, GEngine->GetLargeFont(), 1.4f * S);
		}
		RallyText(Row.Plate, C, LX + 56.0f * S, RowY + 4.0f * S, Medium, 1.15f * S);
		FString State; FLinearColor SC;
		StateText(Row, State, SC);
		float Metres = -1.0f;
		for (TActorIterator<ADCCRover> It(GetWorld()); It; ++It)
		{
			if (It->RallyDuty == 1 && It->RallyPlate == Row.Plate && Me)
			{
				Metres = FVector::Dist(It->GetActorLocation(), Me->GetActorLocation()) / 100.0f;
			}
		}
		RallyText(State.ToUpper(), SC, LX + 56.0f * S, RowY + 25.0f * S, Small, 1.2f * S);
		if (Metres >= 0.0f)
		{
			const FString D = FString::Printf(TEXT("%.0f m"), Metres);
			float DW = 0.0f, DH = 0.0f;
			RallyTextSize(D, DW, DH, Medium, 1.15f * S);
			RallyText(D, Ink, LX + LW - DW - 12.0f * S, RowY + 10.0f * S, Medium, 1.15f * S);
		}
		RowY += 52.0f * S;
	}
	RallyText(TEXT("Each mark keeps its colour and number all shift."), Dim, LX, RowY + 4.0f * S, Small, 1.2f * S);

	if (bFirst)
	{
	// Right column: how a repo works.
	const float RX = X + W * 0.45f, RW = W * 0.55f - P;
	RallyText(TEXT("HOW A REPO WORKS"), Cyan, RX, ColY - 4.0f * S, GEngine->GetLargeFont(), 1.7f * S);
	struct FStep { const TCHAR* Key; const TCHAR* Title; const TCHAR* Line; FLinearColor Colour; };
	const FStep Steps[] = {
		{ TEXT("RMB"), TEXT("AIM"), TEXT("Hold right mouse on a mark until the ring locks."), Amber },
		{ TEXT("Q"), TEXT("HOOK"), TEXT("Fire the magnet. It reels the car onto your bumper."), Cyan },
		{ TEXT("W"), TEXT("TOW"), TEXT("Drive it home to the yard. Speeding scars it."), Pink },
		{ TEXT("Q"), TEXT("DROP"), TEXT("Inside the yard, drop it. That is the payday."), Green },
	};
	float SY = ColY + 34.0f * S;
	for (int32 i = 0; i < 4; ++i)
	{
		DrawRect(Back2, RX, SY, RW, 52.0f * S);
		DrawRect(Steps[i].Colour, RX, SY, 6.0f * S, 52.0f * S);
		RallyText(FString::Printf(TEXT("%d"), i + 1), Steps[i].Colour, RX + 18.0f * S, SY + 6.0f * S, Large, 1.3f * S);
		RallyKey(RX + 52.0f * S, SY + 13.0f * S, Steps[i].Key, S);
		const float TX0 = RX + 120.0f * S;
		RallyText(Steps[i].Title, Steps[i].Colour, TX0, SY + 4.0f * S, GEngine->GetLargeFont(), 1.4f * S);
		RallyText(Steps[i].Line, Ink, TX0, SY + 30.0f * S, Small, 1.2f * S);
		SY += 60.0f * S;
	}

	// Bottom row: what pays, and the controls.
	const float BY = Y + 434.0f * S; // right under the four steps, so the footer has its own line
	DrawRect(FLinearColor(Pink.R, Pink.G, Pink.B, 0.35f), X + P, BY - 14.0f * S, W - P * 2.0f, 1.0f);
	RallyText(TEXT("WHAT PAYS"), Amber, LX, BY - 4.0f * S, GEngine->GetLargeFont(), 1.7f * S);
	const TCHAR* Pays[] = {
		TEXT("+  Fast and clean pays the most"),
		TEXT("-  Scars and wrecks dock the pay"),
		TEXT("-  Hooking the wrong car is a fine"),
		TEXT("-  Gas goes on your tab. Red tab = hunted"),
		TEXT("!  Owners yell. Some fight. Gas stations fix your hull"),
	};
	float PY = BY + 30.0f * S;
	for (const TCHAR* Line : Pays)
	{
		RallyText(Line, Line[0] == TEXT('+') ? Green : Line[0] == TEXT('!') ? Red : Ink, LX, PY, Small, 1.2f * S);
		PY += 21.0f * S;
	}

	RallyText(TEXT("CONTROLS"), Amber, RX, BY - 4.0f * S, GEngine->GetLargeFont(), 1.7f * S);
	struct FCtl { const TCHAR* Keys[2]; const TCHAR* What; };
	const FCtl Ctls[] = {
		{ { TEXT("W"), TEXT("S") }, TEXT("drive") },
		{ { TEXT("A"), TEXT("D") }, TEXT("steer") },
		{ { TEXT("LMB"), nullptr }, TEXT("boost") },
		{ { TEXT("Shift"), nullptr }, TEXT("slide") },
		{ { TEXT("F"), nullptr }, TEXT("next tool") },
		{ { TEXT("T"), nullptr }, TEXT("radio") },
	};
	for (int32 i = 0; i < 6; ++i)
	{
		float CX = RX + (i % 3) * (RW / 3.0f);
		const float CY = BY + 32.0f * S + (i / 3) * 40.0f * S;
		for (const TCHAR* K : Ctls[i].Keys)
		{
			if (K)
			{
				CX += RallyKey(CX, CY, K, S) + 4.0f * S;
			}
		}
		RallyText(Ctls[i].What, Ink, CX + 4.0f * S, CY + 3.0f * S, Small, 1.2f * S);
	}

	}
	else
	{
		// A returning driver: the reminders that matter, in the space the lesson used to take.
		const float RX = X + W * 0.45f;
		RallyText(TEXT("YOU KNOW THE JOB"), Cyan, RX, ColY - 4.0f * S, GEngine->GetLargeFont(), 1.7f * S);
		const TCHAR* Lines[] = {
			TEXT("Right mouse aims, Q hooks, the yard pays."),
			TEXT("Fast and clean pays more. The wrong car is a fine."),
		};
		float LY = ColY + 38.0f * S;
		for (const TCHAR* Line : Lines)
		{
			RallyText(Line, Ink, RX, LY, Small, 1.2f * S);
			LY += 24.0f * S;
		}
	}

	const FString Foot = FString::Printf(TEXT("Shift: %d:%02d.   ENTER closes this.   E gets in the truck.   The truck that banks the most wins."),
		int32(ADCCArcadeKart::ShiftSeconds) / 60, int32(ADCCArcadeKart::ShiftSeconds) % 60);
	float FW = 0.0f, FH = 0.0f;
	RallyTextSize(Foot, FW, FH, Small, 1.2f * S);
	RallyText(Foot, Dim, X + (W - FW) * 0.5f, Y + H - FH - 12.0f * S, Small, 1.2f * S);
}

// ---- during the shift -------------------------------------------------------------------------------------

void ADCCHUD::DrawRallyClock(const ADCCArcadeKart* Kart)
{
	using namespace DCCRallyUI;
	const float S = FMath::Clamp(Canvas->SizeY / 1080.0f, 0.6f, 2.0f);
	UFont* Small = GEngine->GetSmallFont();
	UFont* Large = GEngine->GetLargeFont();
	const float T = float(GetWorld()->GetRealTimeSeconds());

	if (Kart->bPractice)
	{
		// Practice: no clock to show, just what this is and how to leave.
		const float PW = 380.0f * S, PH = 64.0f * S;
		const float PX = (Canvas->SizeX - PW) * 0.5f, PY = 16.0f;
		RallyPanel(PX, PY, PW, PH, Pink);
		RallyText(TEXT("REPO RALLY  -  PRACTICE"), Dim, PX + 14.0f * S, PY + 10.0f * S, Small, 1.2f * S);
		RallyText(TEXT("No clock, no pay. Leave from the Tab menu."), FLinearColor(0.85f, 0.83f, 0.92f), PX + 14.0f * S, PY + 34.0f * S, Small, 1.0f * S);
		return;
	}
	const double Left = FMath::Max(0.0, Kart->GetShiftEndsAt() - Kart->GetNow());
	const int32 Sec = FMath::CeilToInt(Left);
	const bool bLate = Left <= 30.0;

	const float W = 340.0f * S, H = 124.0f * S;
	const float X = (Canvas->SizeX - W) * 0.5f, Y = 16.0f;
	RallyPanel(X, Y, W, H, bLate ? Red : Pink);
	RallyText(TEXT("REPO RALLY  -  SHIFT"), Dim, X + 14.0f * S, Y + 10.0f * S, Small, 1.2f * S);

	const FString Clock = FString::Printf(TEXT("%d:%02d"), Sec / 60, Sec % 60);
	float CW = 0.0f, CH = 0.0f;
	RallyTextSize(Clock, CW, CH, Large, 2.4f * S);
	const float Pulse = bLate ? 0.65f + 0.35f * FMath::Abs(FMath::Sin(T * 5.0f)) : 1.0f;
	RallyText(Clock, (bLate ? Red : Ink) * Pulse, X + (W - CW) * 0.5f, Y + 30.0f * S, Large, 2.4f * S);

	RallyBar(X + 14.0f * S, Y + H - 30.0f * S, W - 28.0f * S, 5.0f * S, float(Left / ADCCArcadeKart::ShiftSeconds), bLate ? Red : Pink);
	const FString Wave = FString::Printf(TEXT("WAVE %d  -  %s"), Kart->RallyWave + 1, WaveName(Kart->RallyWave));
	float WW = 0.0f, WH = 0.0f;
	RallyTextSize(Wave, WW, WH, Small, 1.1f * S);
	RallyText(Wave, Amber, X + (W - WW) * 0.5f, Y + H - 22.0f * S, Small, 1.1f * S);

	// Alerts the arena flags with "!" (debt), as a strip under the clock.
	float AY = Y + H + 8.0f;
	for (const FString& Line : Kart->StatusLines)
	{
		if (!Line.StartsWith(TEXT("!")))
		{
			continue;
		}
		const FString Text = Line.RightChop(1).TrimStart();
		float AW = 0.0f, AH = 0.0f;
		RallyTextSize(Text, AW, AH, GEngine->GetMediumFont(), S);
		const float BX = (Canvas->SizeX - AW) * 0.5f - 16.0f;
		DrawRect(FLinearColor(0.3f, 0.02f, 0.05f, 0.85f), BX, AY, AW + 32.0f, AH + 10.0f);
		DrawRect(Red * (0.6f + 0.4f * FMath::Abs(FMath::Sin(T * 5.0f))), BX, AY, AW + 32.0f, 2.0f);
		RallyText(Text, FLinearColor(1.0f, 0.82f, 0.75f), BX + 16.0f, AY + 5.0f, GEngine->GetMediumFont(), S);
		AY += AH + 14.0f;
	}
}

void ADCCHUD::DrawRallyBoard(const ADCCArcadeKart* Kart)
{
	using namespace DCCRallyUI;
	const float S = FMath::Clamp(Canvas->SizeY / 1080.0f, 0.6f, 2.0f);
	UFont* Small = GEngine->GetSmallFont();
	const float T = float(GetWorld()->GetRealTimeSeconds());
	const ADCCRover* Me = GetOwningRover();
	const float ViewYaw = PlayerOwner && PlayerOwner->PlayerCameraManager ? PlayerOwner->PlayerCameraManager->GetCameraRotation().Yaw : 0.0f;

	const float W = 400.0f * S;
	const float RowH = 62.0f * S;
	const float H = 52.0f * S + Kart->Board.Num() * RowH + 8.0f * S;
	const float X = 24.0f, Y = 16.0f;
	RallyPanel(X, Y, W, H, Cyan);
	RallyText(TEXT("THE BOARD"), Cyan, X + 14.0f * S, Y + 8.0f * S, GEngine->GetLargeFont(), 1.7f * S);
	int32 Open = 0;
	for (const FDCCRallyMarkRow& Row : Kart->Board) { Open += Row.State <= 4 ? 1 : 0; }
	const FString Count = FString::Printf(TEXT("%d open  /  %d out"), Open, Kart->Board.Num());
	float CW = 0.0f, CH = 0.0f;
	RallyTextSize(Count, CW, CH, Small, 1.15f * S);
	RallyText(Count, Dim, X + W - CW - 14.0f * S, Y + 15.0f * S, Small, 1.15f * S);

	float RY = Y + 50.0f * S;
	for (int32 i = 0; i < Kart->Board.Num(); ++i)
	{
		const FDCCRallyMarkRow& Row = Kart->Board[i];
		const FLinearColor C = ADCCArcadeKart::MarkColour(Row.Plate);
		const bool bGone = Row.State == 7;
		const float RX = X + 8.0f * S, RW = W - 16.0f * S, RH = RowH - 8.0f * S;
		DrawRect(i % 2 ? Back2 : FLinearColor(Back2.R * 1.25f, Back2.G * 1.25f, Back2.B * 1.25f, Back2.A), RX, RY, RW, RH);

		// The colour tile: the mark's own glow, with its place on the board.
		const float Tile = RH - 10.0f * S;
		const FLinearColor TileC = bGone ? Dim * 0.4f : C;
		DrawRect(FLinearColor(TileC.R, TileC.G, TileC.B, 0.28f), RX + 5.0f * S, RY + 5.0f * S, Tile, Tile);
		DrawRect(TileC, RX + 5.0f * S, RY + 5.0f * S, Tile, 3.0f * S);
		{
			const FString N = FString::Printf(TEXT("%d"), i + 1);
			float NW = 0.0f, NH = 0.0f;
			RallyTextSize(N, NW, NH, GEngine->GetLargeFont(), 1.3f * S);
			RallyText(N, bGone ? Dim : Ink, RX + 5.0f * S + (Tile - NW) * 0.5f, RY + 5.0f * S + (Tile - NH) * 0.5f, GEngine->GetLargeFont(), 1.3f * S);
		}

		const float TX = RX + Tile + 16.0f * S;
		RallyText(Row.Plate, bGone ? Dim : C, TX, RY + 4.0f * S, Small, 1.35f * S);
		FString State; FLinearColor SC;
		StateText(Row, State, SC);
		if (Row.State == 4)
		{
			SC = SC * (0.6f + 0.4f * FMath::Abs(FMath::Sin(T * 6.0f)));
		}
		const float PillW = RallyPill(TX, RY + 27.0f * S, State.ToUpper(), SC, S);

		// Right side: how far, and which way (relative to where you are looking).
		if (!bGone && Me && Row.State != 6)
		{
			for (TActorIterator<ADCCRover> It(GetWorld()); It; ++It)
			{
				if (It->RallyDuty != 1 || It->RallyPlate != Row.Plate)
				{
					continue;
				}
				const FVector D = It->GetActorLocation() - Me->GetActorLocation();
				const FString Far = FString::Printf(TEXT("%.0f m"), D.Size2D() / 100.0f);
				float FW = 0.0f, FH = 0.0f;
				RallyTextSize(Far, FW, FH, Small, 1.25f * S);
				RallyText(Far, Ink, RX + RW - FW - 40.0f * S, RY + 6.0f * S, Small, 1.25f * S);
				RallyArrow(RX + RW - 18.0f * S, RY + 16.0f * S, 9.0f * S, D.Rotation().Yaw - ViewYaw, C);
				break;
			}
		}

		// Beside the state: the reel on a line, otherwise how scarred the car already is.
		const float BarX = TX + PillW + 10.0f * S;
		const float BarW = RX + RW - 10.0f * S - BarX;
		if (Row.State == 5 && BarW > 50.0f * S)
		{
			RallyText(TEXT("REEL"), Cyan, BarX, RY + 29.0f * S, Small, 0.95f * S);
			RallyBar(BarX + 40.0f * S, RY + 34.0f * S, BarW - 40.0f * S, 7.0f * S, Row.Strain, Cyan);
		}
		else if (!bGone && Row.Damage > 0.0f && BarW > 50.0f * S)
		{
			const FLinearColor DC = FLinearColor::LerpUsingHSV(Amber, Red, FMath::Clamp(Row.Damage / 100.0f, 0.0f, 1.0f));
			RallyText(FString::Printf(TEXT("SCARS %.0f%%"), Row.Damage), DC, BarX, RY + 29.0f * S, Small, 0.95f * S);
		}
		RY += RowH;
	}
}

void ADCCHUD::DrawRallyCrew(const ADCCArcadeKart* Kart)
{
	using namespace DCCRallyUI;
	if (Kart->Crew.IsEmpty())
	{
		return;
	}
	const float S = FMath::Clamp(Canvas->SizeY / 1080.0f, 0.6f, 2.0f);
	UFont* Small = GEngine->GetSmallFont();
	const FString MyName = PlayerOwner && PlayerOwner->PlayerState ? PlayerOwner->PlayerState->GetPlayerName() : FString();

	const float W = 400.0f * S;
	const float RowH = 44.0f * S;
	const float H = 76.0f * S + Kart->Crew.Num() * RowH + 4.0f * S;
	const float X = Canvas->SizeX - W - 24.0f, Y = 16.0f;
	RallyPanel(X, Y, W, H, Amber);
	RallyText(TEXT("CREW"), Amber, X + 14.0f * S, Y + 8.0f * S, GEngine->GetLargeFont(), 1.7f * S);
	const float C2 = X + 54.0f * S, C3 = X + W - 150.0f * S, C4 = X + W - 86.0f * S;
	RallyText(TEXT("DRIVER  -  hull / fuel"), Dim, C2, Y + 48.0f * S, Small, 1.0f * S);
	RallyText(TEXT("DROPS"), Dim, C3, Y + 48.0f * S, Small, 1.0f * S);
	RallyText(TEXT("BANKED"), Dim, C4, Y + 48.0f * S, Small, 1.0f * S);
	const FLinearColor Medal[] = { FLinearColor(1.0f, 0.8f, 0.3f), FLinearColor(0.78f, 0.82f, 0.92f), FLinearColor(0.85f, 0.55f, 0.35f) };
	float RY = Y + 70.0f * S;
	for (int32 i = 0; i < Kart->Crew.Num(); ++i)
	{
		const FDCCRallyCrewRow& Row = Kart->Crew[i];
		const bool bMe = Row.Name == MyName;
		const float RX = X + 8.0f * S, RW = W - 16.0f * S, RH = RowH - 6.0f * S;
		DrawRect(bMe ? FLinearColor(Amber.R, Amber.G, Amber.B, 0.16f) : Back2, RX, RY, RW, RH);
		if (bMe)
		{
			DrawRect(Amber, RX, RY, 3.0f * S, RH);
		}
		// Place medallion.
		const float M = RH - 12.0f * S;
		const FLinearColor MC = i < 3 && Row.Score > 0.0f ? Medal[i] : Dim * 0.6f;
		DrawRect(FLinearColor(MC.R, MC.G, MC.B, 0.3f), RX + 8.0f * S, RY + 6.0f * S, M, M);
		DrawRect(MC, RX + 8.0f * S, RY + 6.0f * S + M - 3.0f * S, M, 3.0f * S);
		{
			const FString N = FString::Printf(TEXT("%d"), i + 1);
			float NW = 0.0f, NH = 0.0f;
			RallyTextSize(N, NW, NH, Small, 1.25f * S);
			RallyText(N, Ink, RX + 8.0f * S + (M - NW) * 0.5f, RY + 6.0f * S + (M - NH) * 0.5f - 1.0f * S, Small, 1.25f * S);
		}
		const FLinearColor C = i == 0 && Row.Score > 0.0f ? Amber : Ink;
		FString Shown = Row.Name;
		float NW = 0.0f, NH = 0.0f;
		RallyTextSize(Shown, NW, NH, Small, 1.2f * S);
		const float NameRoom = C3 - C2 - (Row.Ledger < 0.0f ? 70.0f : 12.0f) * S;
		while (NW > NameRoom && Shown.Len() > 4)
		{
			Shown = Shown.LeftChop(4) + TEXT("..");
			RallyTextSize(Shown, NW, NH, Small, 1.2f * S);
		}
		RallyText(Shown, Row.Ledger < 0.0f ? Red : C, C2, RY + 3.0f * S, Small, 1.2f * S);
		// Under the name: the truck's hull and fuel, two thin bars, so you can see who is limping.
		const float BW = (C3 - C2 - 18.0f * S) * 0.5f;
		const FLinearColor HullC = FLinearColor::LerpUsingHSV(Red, Green, FMath::Clamp(Row.Hull / 100.0f, 0.0f, 1.0f));
		RallyBar(C2, RY + 29.0f * S, BW, 4.0f * S, Row.Hull / 100.0f, HullC);
		RallyBar(C2 + BW + 6.0f * S, RY + 29.0f * S, BW, 4.0f * S, Row.Fuel / 100.0f, Amber);
		if (Row.Ledger < 0.0f)
		{
			RallyPill(C3 - 62.0f * S, RY + 4.0f * S, TEXT("DEBT"), Red, S * 0.85f);
		}
		RallyText(FString::Printf(TEXT("%d"), Row.Nabs), C, C3 + 16.0f * S, RY + 7.0f * S, Small, 1.3f * S);
		RallyText(FString::Printf(TEXT("%.0f"), Row.Score), Amber, C4 + 8.0f * S, RY + 7.0f * S, Small, 1.3f * S);
		RY += RowH;
	}
}

void ADCCHUD::DrawRallyTruck(const ADCCArcadeKart* Kart, const ADCCRover* Rover)
{
	using namespace DCCRallyUI;
	if (!Rover)
	{
		return;
	}
	const float S = FMath::Clamp(Canvas->SizeY / 1080.0f, 0.6f, 2.0f);
	UFont* Small = GEngine->GetSmallFont();
	const float T = float(GetWorld()->GetRealTimeSeconds());
	const FString MyName = PlayerOwner && PlayerOwner->PlayerState ? PlayerOwner->PlayerState->GetPlayerName() : FString();
	const FDCCRallyCrewRow* Mine = Kart->Crew.FindByPredicate([&MyName](const FDCCRallyCrewRow& R) { return R.Name == MyName; });

	const FString Radio = Rover->GetRadioLine();
	const float W = 400.0f * S;
	const float H = (Radio.IsEmpty() ? 236.0f : 290.0f) * S;
	const float X = Canvas->SizeX - W - 24.0f;
	const float Y = Canvas->SizeY - H - 24.0f;
	RallyPanel(X, Y, W, H, Green);
	RallyText(TEXT("YOUR TRUCK"), Green, X + 14.0f * S, Y + 8.0f * S, GEngine->GetLargeFont(), 1.7f * S);
	if (Mine)
	{
		const FString RoleLabel = RoleName(Mine->Role);
		float RW = 0.0f, RH = 0.0f;
		RallyTextSize(RoleLabel, RW, RH, Small, 1.05f * S);
		RallyPill(X + W - RW - 30.0f * S, Y + 13.0f * S, RoleLabel, Cyan, S);
	}

	const float LX = X + 14.0f * S, BX = X + 84.0f * S, BW = W - 160.0f * S, VX = X + W - 66.0f * S;

	// Hull.
	const float Hull = Mine ? Mine->Hull : 100.0f;
	{
		const bool bWreck = Hull <= 0.0f;
		const float Blink = 0.6f + 0.4f * FMath::Abs(FMath::Sin(T * 6.0f));
		const FLinearColor HC = bWreck ? Red * Blink : Hull < 35.0f ? Red : FLinearColor::LerpUsingHSV(Amber, Green, FMath::Clamp((Hull - 35.0f) / 50.0f, 0.0f, 1.0f));
		RallyText(TEXT("HULL"), Dim, LX, Y + 52.0f * S, Small, 1.2f * S);
		RallyBar(BX, Y + 57.0f * S, BW, 11.0f * S, Hull / 100.0f, HC);
		for (int32 n = 1; n < 4; ++n) { DrawRect(FLinearColor(0, 0, 0, 0.5f), BX + BW * n / 4.0f, Y + 57.0f * S, 2.0f, 11.0f * S); }
		RallyText(bWreck ? FString(TEXT("WRECK")) : FString::Printf(TEXT("%.0f%%"), Hull), HC, VX, Y + 52.0f * S, Small, 1.2f * S);
	}

	// Fuel. The bar is full at this truck's tank, which the Chop Shop can raise.
	float Cap = 100.0f;
	if (const ADCCCharacterBase* Who = Rover->Driver.Get())
	{
		if (const UDCCWalletComponent* Wallet = Who->Wallet.Get())
		{
			Cap = DCCChopShop::TankCapacity(Wallet->ChopLevel(DCCChopShop::Tank));
		}
	}
	const float Fuel = Rover->RallyFuel >= 0.0f ? Rover->RallyFuel : (Mine ? Mine->Fuel : Cap);
	{
		const float Fraction = Cap > 1.0f ? Fuel / Cap : 0.0f;
		const bool bLow = Fraction < 0.35f;
		const FLinearColor FuelC = bLow ? Red * (0.6f + 0.4f * FMath::Abs(FMath::Sin(T * 5.0f))) : FLinearColor::LerpUsingHSV(Amber, Green, FMath::Clamp((Fraction - 0.35f) / 0.5f, 0.0f, 1.0f));
		RallyText(TEXT("FUEL"), Dim, LX, Y + 80.0f * S, Small, 1.2f * S);
		RallyBar(BX, Y + 85.0f * S, BW, 11.0f * S, Fraction, FuelC);
		RallyText(FString::Printf(TEXT("%.0f"), Fuel), FuelC, VX, Y + 80.0f * S, Small, 1.2f * S);
	}

	// Tab and bank, as two chips.
	if (Mine)
	{
		const float CY = Y + 110.0f * S, CWid = (W - 36.0f * S) * 0.5f;
		DrawRect(Back2, LX, CY, CWid, 30.0f * S);
		RallyText(TEXT("TAB"), Dim, LX + 8.0f * S, CY + 7.0f * S, Small, 1.05f * S);
		RallyText(Mine->Ledger < 0.0f ? FString::Printf(TEXT("%.0f  DEBT"), Mine->Ledger) : FString::Printf(TEXT("%.0f"), Mine->Ledger),
			Mine->Ledger < 0.0f ? Red : Ink, LX + 52.0f * S, CY + 5.0f * S, Small, 1.25f * S);
		const float C2X = LX + CWid + 8.0f * S;
		DrawRect(Back2, C2X, CY, CWid, 30.0f * S);
		RallyText(TEXT("BANKED"), Dim, C2X + 8.0f * S, CY + 7.0f * S, Small, 1.05f * S);
		RallyText(FString::Printf(TEXT("%.0f"), Mine->Score), Amber, C2X + 82.0f * S, CY + 5.0f * S, Small, 1.25f * S);
	}

	// What the Chop Shop has actually bolted on. Levels, so a buy is visible on the truck.
	float TY = Y + 146.0f * S;
	if (const ADCCCharacterBase* Who = Rover->Driver.Get())
	{
		if (const UDCCWalletComponent* Wallet = Who->Wallet.Get())
		{
			RallyText(FString::Printf(TEXT("PLATE %d    ENG %d    TANK %d    LINE %d    PIT %d"),
				Wallet->ChopLevel(DCCChopShop::Plating), Wallet->ChopLevel(DCCChopShop::Engine),
				Wallet->ChopLevel(DCCChopShop::Tank), Wallet->ChopLevel(DCCChopShop::Line),
				Wallet->ChopLevel(DCCChopShop::PitCrew)), Dim, LX, TY, Small, 1.0f * S);
			TY += 20.0f * S;
		}
	}

	// Tool.
	if (!Rover->RallyTools.IsEmpty())
	{
		const int32 i = FMath::Clamp(Rover->RallyToolIndex, 0, Rover->RallyTools.Num() - 1);
		const DCCRallyItems::FItem* Tool = DCCRallyItems::Find(Rover->RallyTools[i]);
		const float KW = RallyKey(LX, TY, TEXT("Q"), S);
		RallyText(Tool ? Tool->Name : TEXT("?"), Amber, LX + 8.0f * S + KW, TY + 2.0f * S, Small, 1.2f * S);
		if (Rover->RallyTools.Num() > 1)
		{
			const DCCRallyItems::FItem* Next = DCCRallyItems::Find(Rover->RallyTools[(i + 1) % Rover->RallyTools.Num()]);
			const float NX = X + W * 0.58f;
			const float FKW = RallyKey(NX, TY, TEXT("F"), S);
			RallyText(Next ? Next->Name : TEXT("?"), Dim, NX + 8.0f * S + FKW, TY + 2.0f * S, Small, 1.2f * S);
		}
	}
	else
	{
		const float KW = RallyKey(LX, TY, TEXT("RMB"), S);
		RallyText(TEXT("aim"), Ink, LX + 6.0f * S + KW, TY + 2.0f * S, Small, 1.2f * S);
		const float K2 = RallyKey(LX + 56.0f * S + KW, TY, TEXT("Q"), S);
		RallyText(TEXT("hook"), Ink, LX + 62.0f * S + KW + K2, TY + 2.0f * S, Small, 1.2f * S);
	}

	// Status line: repairs first, then what is on the line.
	TY += 34.0f * S;
	if (Mine && Mine->bRepairing)
	{
		const float Pulse = 0.6f + 0.4f * FMath::Abs(FMath::Sin(T * 4.0f));
		RallyPill(LX, TY - 2.0f * S, TEXT("+  REPAIRING - sit tight under the canopy"), Green * Pulse, S);
	}
	else if (Hull <= 0.0f)
	{
		RallyPill(LX, TY - 2.0f * S, TEXT("WRECKED - limp to a gas station"), Red, S);
	}
	else if (Rover->RallyTow)
	{
		const TCHAR* TowLine = TEXT("REELING IT IN...");
		if (Rover->RallyTrick == 1) { TowLine = TEXT("HOLD THE LINE - A and D"); }
		else if (Rover->RallyTrick == 2) { TowLine = TEXT("OVERHEAD - outer lane. Space, Shift, Ctrl, click"); }
		else if (Rover->bRallyTowDocked && Rover->bRallyInYard) { TowLine = TEXT("IN THE YARD - [Q] DROP IT"); }
		else if (Rover->bRallyTowDocked) { TowLine = TEXT("TOWING - [Q] lifts it"); }
		RallyPill(LX, TY - 2.0f * S, TowLine, Rover->bRallyTowDocked ? Green : Cyan, S);
	}
	else
	{
		RallyText(TEXT("Nothing on the line."), Dim, LX, TY, Small, 1.2f * S);
	}

	// The radio.
	if (!Radio.IsEmpty())
	{
		const float RY = Y + H - 52.0f * S;
		DrawRect(FLinearColor(Pink.R, Pink.G, Pink.B, 0.14f), X + 8.0f * S, RY, W - 16.0f * S, 44.0f * S);
		DrawRect(Pink, X + 8.0f * S, RY, 3.0f * S, 44.0f * S);
		RallyText(TEXT("RADIO"), Pink, X + 16.0f * S, RY + 2.0f * S, Small, 1.0f * S);
		FString Shown = Radio;
		float LW = 0.0f, LH = 0.0f;
		RallyTextSize(Shown, LW, LH, Small, 1.2f * S);
		while (LW > W - 34.0f * S && Shown.Len() > 4)
		{
			Shown = Shown.LeftChop(5) + TEXT("...");
			RallyTextSize(Shown, LW, LH, Small, 1.2f * S);
		}
		RallyText(Shown, Ink, X + 16.0f * S, RY + 21.0f * S, Small, 1.2f * S);
	}
}

void ADCCHUD::DrawRallyTrouble(const ADCCArcadeKart* Kart)
{
	using namespace DCCRallyUI;
	const float S = FMath::Clamp(Canvas->SizeY / 1080.0f, 0.6f, 2.0f);
	const float T = float(GetWorld()->GetRealTimeSeconds());
	UFont* Small = GEngine->GetSmallFont();

	// Tags over the owners who came out, and the line they are screaming.
	for (TActorIterator<ADCCRallyThug> It(GetWorld()); It; ++It)
	{
		ADCCRallyThug* Thug = *It;
		Thug->TryShout();
		const FVector Head = Thug->GetActorLocation() + FVector(0, 0, 230);
		const FVector P = Canvas->Project(Head);
		if (P.Z <= 0.0f || P.X < 0.0f || P.X > Canvas->SizeX || P.Y < 0.0f || P.Y > Canvas->SizeY)
		{
			continue;
		}
		const bool bDown = Thug->Mood == ADCCRallyThug::Down;
		const bool bGun = Thug->Kind == ADCCRallyThug::Shooter;
		const FString Tag = bDown ? FString(TEXT("KO")) : bGun ? FString(TEXT("ARMED OWNER")) : FString(TEXT("ANGRY OWNER"));
		const FLinearColor C = bDown ? Dim : Red * (0.7f + 0.3f * FMath::Abs(FMath::Sin(T * 7.0f)));
		float TW = 0.0f, TH = 0.0f;
		RallyTextSize(Tag, TW, TH, Small, 1.05f * S);
		RallyPill(P.X - TW * 0.5f - 7.0f * S, P.Y - TH, Tag, C, S);
		float Below = P.Y + 6.0f * S;
		if (!bDown && !Thug->Plate.IsEmpty())
		{
			const FString Sub = FString::Printf(TEXT("owns %s"), *Thug->Plate);
			float SW = 0.0f, SH = 0.0f;
			RallyTextSize(Sub, SW, SH, Small, 0.95f * S);
			RallyText(Sub, Ink, P.X - SW * 0.5f, Below, Small, 0.95f * S);
			Below += SH + 2.0f * S;
		}
		if (!bDown && GetWorld()->GetTimeSeconds() < Thug->ShoutUntil && !Thug->ShoutText.IsEmpty())
		{
			const float MaxW = 420.0f * S;
			const float Scale = 1.0f * S;
			TArray<FString> Words;
			Thug->ShoutText.ParseIntoArrayWS(Words);
			FString Line;
			auto Flush = [&]()
			{
				if (Line.IsEmpty())
				{
					return;
				}
				float LW = 0.0f, LH = 0.0f;
				RallyTextSize(Line, LW, LH, Small, Scale);
				RallyText(Line, Amber, P.X - LW * 0.5f, Below, Small, Scale);
				Below += LH + 1.0f * S;
				Line.Reset();
			};
			for (const FString& Word : Words)
			{
				const FString Trial = Line.IsEmpty() ? Word : Line + TEXT(" ") + Word;
				float LW = 0.0f, LH = 0.0f;
				RallyTextSize(Trial, LW, LH, Small, Scale);
				if (LW > MaxW && !Line.IsEmpty())
				{
					Flush();
					Line = Word;
				}
				else
				{
					Line = Trial;
				}
			}
			Flush();
		}
	}

	// The red edge when your truck takes a hit.
	const FString MyName = PlayerOwner && PlayerOwner->PlayerState ? PlayerOwner->PlayerState->GetPlayerName() : FString();
	if (const FDCCRallyCrewRow* Mine = Kart->Crew.FindByPredicate([&MyName](const FDCCRallyCrewRow& R) { return R.Name == MyName; }))
	{
		const double Now = GetWorld()->GetRealTimeSeconds();
		if (RallyLastHull >= 0.0f && Mine->Hull < RallyLastHull - 0.5f)
		{
			RallyHurtAt = Now;
		}
		RallyLastHull = Mine->Hull;
		const float A = FMath::Clamp(1.0f - float(Now - RallyHurtAt) / 0.6f, 0.0f, 1.0f) * 0.55f;
		if (A > 0.01f)
		{
			const float E = 70.0f * S;
			for (int32 i = 0; i < 6; ++i)
			{
				const float F = A * (1.0f - i / 6.0f);
				const float O = i * E / 6.0f;
				const FLinearColor Edge(1.0f, 0.08f, 0.1f, F * 0.4f);
				DrawRect(Edge, O, O, Canvas->SizeX - O * 2.0f, E / 6.0f);
				DrawRect(Edge, O, Canvas->SizeY - O - E / 6.0f, Canvas->SizeX - O * 2.0f, E / 6.0f);
				DrawRect(Edge, O, O, E / 6.0f, Canvas->SizeY - O * 2.0f);
				DrawRect(Edge, Canvas->SizeX - O - E / 6.0f, O, E / 6.0f, Canvas->SizeY - O * 2.0f);
			}
		}
	}
}

// ---- after the shift: the report --------------------------------------------------------------------------

void ADCCHUD::DrawRallyReport(const ADCCArcadeKart* Kart)
{
	using namespace DCCRallyUI;
	const float S = FMath::Clamp(Canvas->SizeY / 1080.0f, 0.6f, 2.0f);
	UFont* Small = GEngine->GetSmallFont();
	UFont* Medium = GEngine->GetMediumFont();
	UFont* Large = GEngine->GetLargeFont();
	const FString MyName = PlayerOwner && PlayerOwner->PlayerState ? PlayerOwner->PlayerState->GetPlayerName() : FString();

	const float RowH = 40.0f * S;
	const float W = FMath::Min(1000.0f * S, Canvas->SizeX - 48.0f);
	const float H = 250.0f * S + FMath::Max(1, Kart->Crew.Num()) * RowH;
	const float X = (Canvas->SizeX - W) * 0.5f;
	const float Y = FMath::Max(24.0f, Canvas->SizeY * 0.08f);
	RallyPanel(X, Y, W, H, Amber);
	const float P = 28.0f * S;
	RallyText(TEXT("MILE HIGH RECOVERY SOLUTIONS"), Dim, X + P, Y + 16.0f * S, Small, 1.2f * S);
	RallyText(TEXT("SHIFT REPORT"), Amber, X + P, Y + 32.0f * S, Large, 2.2f * S);

	{
		const bool bAnyone = !Kart->Crew.IsEmpty() && Kart->Crew[0].Score > 0.0f;
		const FString Top = bAnyone ? FString::Printf(TEXT("TOP DRIVER:  %s"), *Kart->Crew[0].Name) : FString(TEXT("Nobody brought a car home tonight."));
		float TW = 0.0f, TH = 0.0f;
		RallyTextSize(Top, TW, TH, Medium, 1.2f * S);
		RallyText(Top, Pink, X + W - P - TW, Y + 44.0f * S, Medium, 1.2f * S);
	}

	// The table.
	struct FCol { const TCHAR* Head; float At; };
	const FCol Cols[] = {
		{ TEXT("#"), 0.0f }, { TEXT("DRIVER"), 0.05f }, { TEXT("DROPS"), 0.36f }, { TEXT("FASTEST"), 0.46f },
		{ TEXT("FINES"), 0.57f }, { TEXT("TAB"), 0.66f }, { TEXT("BANKED"), 0.76f }, { TEXT("TICKETS"), 0.88f },
	};
	const float TX = X + P, TW = W - P * 2.0f;
	const float HeadY = Y + 104.0f * S;
	for (const FCol& C : Cols)
	{
		RallyText(C.Head, Dim, TX + TW * C.At, HeadY, Small, 1.2f * S);
	}
	DrawRect(FLinearColor(Amber.R, Amber.G, Amber.B, 0.4f), TX, HeadY + 22.0f * S, TW, 1.0f);
	float RY = HeadY + 30.0f * S;
	for (int32 i = 0; i < Kart->Crew.Num(); ++i)
	{
		const FDCCRallyCrewRow& Row = Kart->Crew[i];
		const bool bMe = Row.Name == MyName;
		DrawRect(bMe ? FLinearColor(Amber.R, Amber.G, Amber.B, 0.18f) : (i % 2 ? Back2 : FLinearColor(0, 0, 0, 0.0f)), TX - 8.0f * S, RY, TW + 16.0f * S, RowH - 4.0f * S);
		const FLinearColor C = i == 0 && Row.Score > 0.0f ? Amber : Ink;
		const float TextY = RY + 8.0f * S;
		RallyText(FString::Printf(TEXT("%d"), i + 1), C, TX + TW * Cols[0].At, TextY, Medium, 1.15f * S);
		RallyText(Row.Name, C, TX + TW * Cols[1].At, TextY, Medium, 1.15f * S);
		RallyText(FString::Printf(TEXT("%d"), Row.Nabs), Ink, TX + TW * Cols[2].At, TextY, Medium, 1.15f * S);
		RallyText(Row.BestNab > 0.0f ? FString::Printf(TEXT("%.0f s"), Row.BestNab) : TEXT("-"), Ink, TX + TW * Cols[3].At, TextY, Medium, 1.15f * S);
		RallyText(FString::Printf(TEXT("%d"), Row.Fines), Row.Fines > 0 ? Red : Ink, TX + TW * Cols[4].At, TextY, Medium, 1.15f * S);
		RallyText(FString::Printf(TEXT("%.0f"), Row.Ledger), Row.Ledger < 0.0f ? Red : Ink, TX + TW * Cols[5].At, TextY, Medium, 1.15f * S);
		RallyText(FString::Printf(TEXT("%.0f"), Row.Score), Amber, TX + TW * Cols[6].At, TextY, Medium, 1.15f * S);
		RallyText(Row.Tickets > 0 ? FString::Printf(TEXT("+%d"), Row.Tickets) : TEXT("-"), Green, TX + TW * Cols[7].At, TextY, Medium, 1.15f * S);
		RY += RowH;
	}

	// Your own night, in one line.
	for (int32 i = 0; i < Kart->Crew.Num(); ++i)
	{
		const FDCCRallyCrewRow& Row = Kart->Crew[i];
		if (Row.Name != MyName)
		{
			continue;
		}
		const TCHAR* Places[] = { TEXT("1st"), TEXT("2nd"), TEXT("3rd"), TEXT("4th"), TEXT("5th") };
		const FString Line = FString::Printf(TEXT("YOU:  %s of %d   -   %d drop%s   -   +%d tickets"),
			i < 5 ? Places[i] : TEXT("-"), Kart->Crew.Num(), Row.Nabs, Row.Nabs == 1 ? TEXT("") : TEXT("s"), Row.Tickets);
		float LW = 0.0f, LH = 0.0f;
		RallyTextSize(Line, LW, LH, Large, 1.6f * S);
		RallyText(Line, Green, X + (W - LW) * 0.5f, RY + 12.0f * S, Large, 1.6f * S);
	}

	// The all-time line the arena already keeps.
	for (const FString& Line : Kart->StatusLines)
	{
		if (Line.StartsWith(TEXT("Best shifts ever")))
		{
			float LW = 0.0f, LH = 0.0f;
			RallyTextSize(Line, LW, LH, Small, 1.2f * S);
			RallyText(Line, Dim, X + (W - LW) * 0.5f, Y + H - LH - 14.0f * S, Small, 1.2f * S);
		}
	}
}

// ---- the skill practice bar (every game) ------------------------------------------------------------------

void ADCCHUD::DrawSkillProgress()
{
	using namespace DCCRallyUI;
	if (!Canvas || !GetWorld())
	{
		return;
	}
	// Your crawler, or the one driving the car you are in.
	const ADCCCharacterBase* Crawler = GetOwningCrawler();
	const ADCCRover* Rover = GetOwningRover();
	if (!Crawler && Rover)
	{
		Crawler = Rover->Driver;
	}
	if (!Crawler || !Crawler->Skills)
	{
		return;
	}
	const double Now = GetWorld()->GetRealTimeSeconds();
	const bool bFresh = SkillBarCrawler.Get() != Crawler;
	if (bFresh)
	{
		SkillBarCrawler = Crawler;
		SkillBarSeen.Reset();
		SkillBarRanks.Reset();
	}

	const FDCCKnownSkill* Showing = nullptr;
	for (const FDCCKnownSkill& Known : Crawler->Skills->GetKnownSkills())
	{
		const float* Seen = SkillBarSeen.Find(Known.SkillId);
		const int32* Rank = SkillBarRanks.Find(Known.SkillId);
		if (!bFresh && Seen && Rank && (Known.Practice != *Seen || Known.Rank != *Rank))
		{
			if (SkillBarId != Known.SkillId)
			{
				SkillBarShown = *Seen; // start the fill from where this skill was
			}
			SkillBarId = Known.SkillId;
			SkillBarUntil = Now + 3.5;
			if (Known.Rank > *Rank)
			{
				SkillBarRankUpAt = Now;
				SkillBarShown = 0.0f;
			}
		}
		SkillBarSeen.Add(Known.SkillId, Known.Practice);
		SkillBarRanks.Add(Known.SkillId, Known.Rank);
		if (Known.SkillId == SkillBarId)
		{
			Showing = &Known;
		}
	}
	if (!Showing || Now > SkillBarUntil)
	{
		return;
	}

	const float S = FMath::Clamp(Canvas->SizeY / 1080.0f, 0.6f, 2.0f);
	UFont* Small = GEngine->GetSmallFont();
	const float Fade = FMath::Clamp(float(SkillBarUntil - Now) / 0.5f, 0.0f, 1.0f);
	SkillBarShown = FMath::FInterpTo(SkillBarShown, Showing->Practice, GetWorld()->GetDeltaSeconds(), 6.0f);
	const bool bRankUp = Now - SkillBarRankUpAt < 2.5;

	// "TargetedThrow" -> "TARGETED THROW"
	FString Name;
	const FString Raw = Showing->SkillId.ToString();
	for (int32 i = 0; i < Raw.Len(); ++i)
	{
		if (i > 0 && FChar::IsUpper(Raw[i]) && !FChar::IsUpper(Raw[i - 1]))
		{
			Name += TEXT(' ');
		}
		Name += FChar::ToUpper(Raw[i]);
	}

	// A small card, low and left of centre: clear of the dash pod, the map and the truck panel.
	const float W = 300.0f * S, H = 54.0f * S;
	const float X = Canvas->SizeX * 0.5f - W - (Rover ? 190.0f * S : -W * 0.5f);
	const float Y = Canvas->SizeY - H - (Rover ? 28.0f : 150.0f) * S;
	const FLinearColor Accent = bRankUp ? Amber : Cyan;
	auto A = [Fade](FLinearColor C) { C.A *= Fade; return C; };
	DrawRect(A(FLinearColor(0.0f, 0.0f, 0.0f, 0.3f)), X + 4.0f, Y + 5.0f, W, H);
	DrawRect(A(Back), X, Y, W, H);
	DrawRect(A(Accent), X, Y, W, 3.0f);
	RallyText(Name, A(Accent), X + 12.0f * S, Y + 8.0f * S, Small, 1.25f * S);
	const FString Right = bRankUp ? FString::Printf(TEXT("RANK UP!  %d"), Showing->Rank) : FString::Printf(TEXT("RANK %d"), Showing->Rank);
	float RW = 0.0f, RH = 0.0f;
	RallyTextSize(Right, RW, RH, Small, 1.25f * S);
	RallyText(Right, A(bRankUp ? Amber * (0.7f + 0.3f * FMath::Abs(FMath::Sin(float(Now) * 8.0f))) : Ink), X + W - RW - 12.0f * S, Y + 8.0f * S, Small, 1.25f * S);
	const float BX = X + 12.0f * S, BW = W - 24.0f * S, BY = Y + H - 18.0f * S;
	DrawRect(A(FLinearColor(1.0f, 1.0f, 1.0f, 0.12f)), BX, BY, BW, 8.0f * S);
	DrawRect(A(Accent), BX, BY, BW * FMath::Clamp(bRankUp ? 1.0f : SkillBarShown, 0.0f, 1.0f), 8.0f * S);
	// A bright cap on the leading edge, so a small gain still reads as movement.
	if (!bRankUp && SkillBarShown > 0.01f)
	{
		DrawRect(A(FLinearColor::White), BX + BW * FMath::Clamp(SkillBarShown, 0.0f, 1.0f) - 2.0f * S, BY - 1.0f * S, 2.0f * S, 10.0f * S);
	}
}

// ---- guides show once per player ---------------------------------------------------------------------------

bool ADCCHUD::IsGuideSeen(FName Guide)
{
	const FString Key = PlayerOwner && PlayerOwner->PlayerState ? PlayerOwner->PlayerState->GetPlayerName() : FString();
	if (Key != GuideCacheKey)
	{
		GuideCacheKey = Key;
		GuideCache.Reset();
	}
	if (const bool* Cached = GuideCache.Find(Guide))
	{
		return *Cached;
	}
	return GuideCache.Add(Guide, DCCSession::HasSeenGuide(Key, Guide));
}

void ADCCHUD::MarkGuideSeen(FName Guide)
{
	if (!IsGuideSeen(Guide))
	{
		DCCSession::MarkGuideSeen(GuideCacheKey, Guide);
		GuideCache.Add(Guide, true);
	}
}
