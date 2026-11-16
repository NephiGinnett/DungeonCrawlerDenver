// The vehicle instrument cluster (2026-11-13; restyled 2026-11-14 as a solid dash pod with segmented arcs): a speedometer arc with a boost
// ring, and an altimeter tape beside it. Replaces the rough tick dial Zoe asked
// to have polished. Bottom centre of the screen, scaled to the screen height.
//
// Canvas has no arc primitive, so arcs are built from many short thick lines,
// close enough together to read as a solid band. Numbers use Slate fonts
// (FCanvasTextItem with FSlateFontInfo), which stay crisp at any size, unlike
// the engine's fixed bitmap fonts scaled up.

#include "UI/DCCHUD.h"

#include "Vehicles/DCCRover.h"
#include "Arcade/DCCArcadeArena.h"
#include "CanvasItem.h"
#include "CollisionQueryParams.h"
#include "Engine/Canvas.h"
#include "Engine/Font.h"
#include "Engine/World.h"
#include "Fonts/FontMeasure.h"
#include "Framework/Application/SlateApplication.h"
#include "Rendering/SlateRenderer.h"
#include "Styling/CoreStyle.h"
#include "GlobalRenderResources.h"

namespace DCCGauges
{
	const FLinearColor Track(1.0f, 1.0f, 1.0f, 0.10f);
	const FLinearColor Cool(0.20f, 0.85f, 1.0f);
	const FLinearColor Warm(1.0f, 0.78f, 0.25f);
	const FLinearColor Hot(1.0f, 0.25f, 0.65f);
	const FLinearColor Ink(0.96f, 0.95f, 1.0f);
	const FLinearColor Dim(0.70f, 0.68f, 0.80f, 0.85f);

	FSlateFontInfo Font(float Size, bool bBold = true)
	{
		// The engine's Roboto asset: Canvas draws nothing for FCoreStyle's default font (found 2026-11-14 - the
		// speed number had never been showing).
		static UFont* Roboto = LoadObject<UFont>(nullptr, TEXT("/Engine/EngineFonts/Roboto.Roboto"));
		return FSlateFontInfo(Roboto, FMath::Max(6, FMath::RoundToInt(Size)), bBold ? FName(TEXT("Bold")) : FName(TEXT("Regular")));
	}

	FVector2D Measure(const FString& Text, const FSlateFontInfo& FontInfo)
	{
		if (!FSlateApplication::IsInitialized())
		{
			return FVector2D(Text.Len() * FontInfo.Size * 0.6f, FontInfo.Size * 1.3f);
		}
		return FSlateApplication::Get().GetRenderer()->GetFontMeasureService()->Measure(Text, FontInfo);
	}

	/** Text with its anchor at X (0 left, 0.5 centre, 1 right) and its top at Y. */
	void Text(UCanvas* Canvas, const FString& Text, const FSlateFontInfo& FontInfo, const FLinearColor& Colour,
		float X, float Y, float Anchor = 0.5f, bool bShadow = true)
	{
		const FVector2D Size = Measure(Text, FontInfo);
		FCanvasTextItem Item(FVector2D(X - Size.X * Anchor, Y), FText::FromString(Text), FontInfo, Colour);
		if (bShadow)
		{
			Item.EnableShadow(FLinearColor(0.0f, 0.0f, 0.0f, 0.6f), FVector2D(1.5f, 1.5f));
		}
		Canvas->DrawItem(Item);
	}

	/** Colour along the speed arc: cool, then warm, then hot. */
	FLinearColor Along(float T)
	{
		return T < 0.55f
			? FLinearColor::LerpUsingHSV(Cool, Warm, T / 0.55f)
			: FLinearColor::LerpUsingHSV(Warm, Hot, (T - 0.55f) / 0.45f);
	}

	/** A filled ring segment (solid triangles, not stacked lines): radii R0..R1, screen degrees A0..A1. */
	void Band(UCanvas* Canvas, const FVector2D& C, float R0, float R1, float A0, float A1, const FLinearColor& Colour, int32 Steps = 0)
	{
		if (Steps <= 0)
		{
			Steps = FMath::Clamp(FMath::CeilToInt(FMath::Abs(A1 - A0) / 3.0f), 1, 128);
		}
		auto P = [&C](float Deg, float R)
		{
			const float Rad = FMath::DegreesToRadians(Deg);
			return FVector2D(C.X + FMath::Cos(Rad) * R, C.Y + FMath::Sin(Rad) * R);
		};
		TArray<FCanvasUVTri> Tris;
		Tris.Reserve(Steps * 2);
		for (int32 i = 0; i < Steps; ++i)
		{
			const float D0 = FMath::Lerp(A0, A1, float(i) / Steps), D1 = FMath::Lerp(A0, A1, float(i + 1) / Steps);
			const FVector2D Q0 = P(D0, R0), Q1 = P(D0, R1), Q2 = P(D1, R1), Q3 = P(D1, R0);
			FCanvasUVTri T;
			T.V0_Color = T.V1_Color = T.V2_Color = FLinearColor::White; // the item colour tints it (vertex alpha alone was ignored)
			T.V0_Pos = Q0; T.V1_Pos = Q1; T.V2_Pos = Q2; Tris.Add(T);
			T.V0_Pos = Q0; T.V1_Pos = Q2; T.V2_Pos = Q3; Tris.Add(T);
		}
		FCanvasTriangleItem Item(Tris, GWhiteTexture);
		Item.SetColor(Colour);
		Item.BlendMode = SE_BLEND_Translucent;
		Canvas->DrawItem(Item);
	}
}

void ADCCHUD::DrawSpeedGauge()
{
	using namespace DCCGauges;

	const ADCCRover* Rover = GetOwningRover();
	if (!Canvas || !Rover || !Rover->IsOccupied() || (GetArcadeArena() && GetArcadeArena()->bRoundOver))
	{
		return;
	}
	UWorld* World = GetWorld();
	const float S = FMath::Clamp(Canvas->SizeY / 1080.0f, 0.6f, 2.0f);
	const float Time = World ? World->GetRealTimeSeconds() : 0.0f;

	// ---- numbers -----------------------------------------------------------
	// Unreal units are centimetres, so cm/s * 0.036 is a real km/h.
	const FVector Velocity = Rover->GetVelocity();
	const float KmH = Velocity.Size2D() * 0.036f;
	const float DialMax = FMath::Max(100.0f, FMath::CeilToFloat(Rover->BoostSpeed * 0.036f / 50.0f) * 50.0f);
	const float Fraction = FMath::Clamp(KmH / DialMax, 0.0f, 1.0f);
	const bool bBoosting = Rover->IsBoosting();
	const float Charge = FMath::Clamp(Rover->GetBoostChargeFraction(), 0.0f, 1.0f);

	// Height above whatever is underneath, from the bottom of the rover.
	float Altitude = 0.0f;
	if (World)
	{
		const FBox Bounds = Rover->GetComponentsBoundingBox(/*bNonColliding=*/false);
		const FVector From(Rover->GetActorLocation().X, Rover->GetActorLocation().Y, Bounds.Min.Z + 5.0f);
		FCollisionQueryParams Params(SCENE_QUERY_STAT(DCCAltimeter), false, Rover);
		TArray<AActor*> Riders;
		Rover->GetAttachedActors(Riders, true, true);
		Params.AddIgnoredActors(Riders);
		FHitResult Hit;
		if (World->LineTraceSingleByChannel(Hit, From, From - FVector(0, 0, 200000.0f), ECC_Visibility, Params))
		{
			Altitude = FMath::Max(0.0f, (From.Z - Hit.ImpactPoint.Z) / 100.0f);
		}
	}
	const float Climb = Velocity.Z / 100.0f; // m/s

	// ---- layout: a dash pod sitting on the bottom edge (2026-11-14 polish) ---
	const float Radius = 112.0f * S;
	const FVector2D C(Canvas->SizeX * 0.5f, Canvas->SizeY - 18.0f * S);
	constexpr float StartDeg = 192.0f, EndDeg = 348.0f; // screen degrees, 0 = right, clockwise is down

	// The pod: a dark half-disc, a block down to the screen edge, and a neon rim.
	const float PodR = Radius + 34.0f * S;
	Band(Canvas, C, 0.0f, PodR, 180.0f, 360.0f, FLinearColor(0.025f, 0.012f, 0.045f, 0.9f), 60);
	DrawRect(FLinearColor(0.025f, 0.012f, 0.045f, 0.9f), C.X - PodR, C.Y, PodR * 2.0f, Canvas->SizeY - C.Y);
	Band(Canvas, C, PodR - 2.0f * S, PodR + 1.0f * S, 180.0f, 360.0f, FLinearColor(Cool.R, Cool.G, Cool.B, 0.55f), 60);
	Band(Canvas, C, PodR + 1.0f * S, PodR + 7.0f * S, 180.0f, 360.0f, FLinearColor(Cool.R, Cool.G, Cool.B, 0.08f), 60);

	// ---- speed: lit segments with gaps -------------------------------------
	constexpr int32 Segments = 34;
	const float Span = (EndDeg - StartDeg) / Segments;
	for (int32 i = 0; i < Segments; ++i)
	{
		const float T0 = float(i) / Segments;
		const bool bLit = (i + 0.5f) / Segments <= Fraction;
		FLinearColor Colour = bLit ? Along(T0) : FLinearColor(1.0f, 1.0f, 1.0f, 0.07f);
		if (bLit && bBoosting)
		{
			Colour = FMath::Lerp(Colour, FLinearColor::White, 0.2f + 0.2f * FMath::Sin(Time * 18.0f + T0 * 12.0f));
		}
		const float A0 = StartDeg + i * Span + Span * 0.12f, A1 = StartDeg + (i + 1) * Span - Span * 0.12f;
		Band(Canvas, C, Radius - 17.0f * S, Radius, A0, A1, Colour, 2);
		if (bLit)
		{
			// a soft glow outside the lit segment
			Band(Canvas, C, Radius, Radius + 5.0f * S, A0, A1, FLinearColor(Colour.R, Colour.G, Colour.B, 0.22f), 2);
		}
	}

	// Scale numbers every 100 km/h, just outside the band.
	for (float V = 0.0f; V <= DialMax + 0.1f; V += 50.0f)
	{
		const float Deg = FMath::Lerp(StartDeg, EndDeg, V / DialMax);
		const bool bMajor = FMath::IsNearlyZero(FMath::Fmod(V, 100.0f));
		const float Rad = FMath::DegreesToRadians(Deg);
		const FVector2D Tick0(C.X + FMath::Cos(Rad) * (Radius + 8.0f * S), C.Y + FMath::Sin(Rad) * (Radius + 8.0f * S));
		const FVector2D Tick1(C.X + FMath::Cos(Rad) * (Radius + (bMajor ? 15.0f : 12.0f) * S), C.Y + FMath::Sin(Rad) * (Radius + (bMajor ? 15.0f : 12.0f) * S));
		DrawLine(Tick0.X, Tick0.Y, Tick1.X, Tick1.Y, FLinearColor(1, 1, 1, bMajor ? 0.7f : 0.3f), (bMajor ? 2.0f : 1.0f) * S);
		if (bMajor)
		{
			const FVector2D L(C.X + FMath::Cos(Rad) * (Radius + 25.0f * S), C.Y + FMath::Sin(Rad) * (Radius + 25.0f * S));
			Text(Canvas, FString::Printf(TEXT("%d"), FMath::RoundToInt(V)), Font(8.5f * S, false), Dim, L.X, L.Y - 7.0f * S, 0.5f, false);
		}
	}

	// ---- boost: an inner row of segments ------------------------------------
	const FLinearColor BoostColour = bBoosting
		? FLinearColor::LerpUsingHSV(Hot, FLinearColor::White, 0.5f + 0.5f * FMath::Sin(Time * 20.0f))
		: (Charge >= 1.0f ? Cool : Warm);
	constexpr int32 BoostSegs = 20;
	const float BSpan = (EndDeg - StartDeg) / BoostSegs;
	for (int32 i = 0; i < BoostSegs; ++i)
	{
		const bool bLit = bBoosting || (i + 0.5f) / BoostSegs <= Charge;
		const float A0 = StartDeg + i * BSpan + BSpan * 0.18f, A1 = StartDeg + (i + 1) * BSpan - BSpan * 0.18f;
		Band(Canvas, C, Radius - 30.0f * S, Radius - 23.0f * S, A0, A1, bLit ? BoostColour : FLinearColor(1, 1, 1, 0.06f), 2);
	}

	// ---- readout -----------------------------------------------------------
	Text(Canvas, FString::Printf(TEXT("%d"), FMath::RoundToInt(KmH)), Font(40.0f * S), bBoosting ? Hot : Ink, C.X, C.Y - 92.0f * S);
	Text(Canvas, TEXT("KM/H"), Font(9.0f * S), Dim, C.X, C.Y - 42.0f * S);
	const FString BoostLabel = bBoosting ? TEXT("BOOST!")
		: Charge >= 1.0f ? TEXT("BOOST READY  -  LMB")
		: FString::Printf(TEXT("BOOST %d%%"), FMath::RoundToInt(Charge * 100.0f));
	const float Pulse = Charge >= 1.0f && !bBoosting ? 0.7f + 0.3f * FMath::Sin(Time * 5.0f) : 1.0f;
	FLinearColor LabelColour = bBoosting ? Hot : (Charge >= 1.0f ? Cool : Warm);
	LabelColour.A *= Pulse;
	Text(Canvas, BoostLabel, Font(9.0f * S), LabelColour, C.X, C.Y - 26.0f * S, 0.5f, false);

	// ---- air: only while the truck is off the ground ------------------------
	// It used to sit beside the dial all the time, reading 0. Now it slides in on a jump.
	static float AirShown = 0.0f;
	const bool bAir = Altitude > 1.2f;
	AirShown = FMath::FInterpTo(AirShown, bAir ? 1.0f : 0.0f, World ? World->GetDeltaSeconds() : 0.016f, 10.0f);
	if (AirShown > 0.02f)
	{
		const float PW = 128.0f * S, PH = 64.0f * S;
		const float PX = C.X + PodR + 16.0f * S, PY = Canvas->SizeY - PH - 24.0f * S + (1.0f - AirShown) * 30.0f * S;
		const float A = AirShown;
		DrawRect(FLinearColor(0.025f, 0.012f, 0.045f, 0.82f * A), PX, PY, PW, PH);
		DrawRect(FLinearColor(Cool.R, Cool.G, Cool.B, A), PX, PY, PW, 3.0f * S);
		Text(Canvas, TEXT("AIR"), Font(8.5f * S), FLinearColor(Dim.R, Dim.G, Dim.B, A), PX + 10.0f * S, PY + 8.0f * S, 0.0f, false);
		const FString AltText = Altitude < 10.0f ? FString::Printf(TEXT("%.1f m"), Altitude) : FString::Printf(TEXT("%d m"), FMath::RoundToInt(Altitude));
		Text(Canvas, AltText, Font(18.0f * S), FLinearColor(Ink.R, Ink.G, Ink.B, A), PX + 10.0f * S, PY + 22.0f * S, 0.0f);
		if (FMath::Abs(Climb) >= 0.5f)
		{
			const FString Rate = FString::Printf(TEXT("%s%.0f"), Climb > 0.0f ? TEXT("+") : TEXT(""), Climb);
			FLinearColor RC = Climb > 0.0f ? Cool : Warm;
			RC.A = A;
			Text(Canvas, Rate, Font(10.0f * S), RC, PX + PW - 10.0f * S, PY + 8.0f * S, 1.0f, false);
		}
	}
}
