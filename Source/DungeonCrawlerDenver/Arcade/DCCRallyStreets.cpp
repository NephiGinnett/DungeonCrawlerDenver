// Repo Rally street fun (2026-11-13, Zoe: "cool ramps and boosts, and downhill speedy parts", "a nice night sky").
// ADCCArcadeKart::BuildStreetFun, kept apart from DCCArcadeGames.cpp. Built locally on every machine with the city,
// so the ramps collide everywhere and every machine knows where the boost pads are (FDCCRallyAmbience reads them).

#include "Arcade/DCCArcadeGames.h"
#include "Engine/PostProcessVolume.h"
#include "Engine/TextRenderActor.h"
#include "Engine/DecalActor.h"
#include "Components/DecalComponent.h"
#include "Materials/MaterialInterface.h"

namespace
{
	const TCHAR* NeonPink = TEXT("/Game/Synthwave_city/Material/MI_neon_1");
	const TCHAR* NeonCyan = TEXT("/Game/Synthwave_city/Material/MI_neon_4");
	const TCHAR* Glow = TEXT("/Game/Synthwave_city/Material/MI_light");

	/** This machine's night grade, so DCC.RallyExposure can change it live. */
	TWeakObjectPtr<APostProcessVolume> GRallyGrade;

	TAutoConsoleVariable<float> CVarRallyExposure(
		TEXT("DCC.RallyExposure"), -0.5f,
		TEXT("Repo Rally night exposure, EV100 (lower is brighter). 2026-11-14 default -0.5."),
		FConsoleVariableDelegate::CreateLambda([](IConsoleVariable* Var)
		{
			if (APostProcessVolume* Grade = GRallyGrade.Get())
			{
				Grade->Settings.AutoExposureMinBrightness = Var->GetFloat();
				Grade->Settings.AutoExposureMaxBrightness = Var->GetFloat();
			}
		}),
		ECVF_Default);
}

void ADCCArcadeKart::BuildStreetFun()
{
	const FVector O = GetActorLocation();
	const FLinearColor Deck(0.05f, 0.03f, 0.08f);
	const FLinearColor Pink(1.0f, 0.2f, 0.85f), Cyan(0.2f, 0.9f, 1.0f);
	FRandomStream R(2049);
	BoostPads.Reset();

	auto Pad = [&](const FVector& At, float Yaw)
	{
		// A glowing chevron pad. Drive over it for a boost (FDCCRallyAmbience).
		if (AActor* Base = Block(At + FVector(0, 0, 20), FVector(900, 700, 8), Cyan, Yaw, false)) { Paint(Base, NeonCyan); }
		for (int32 c = 0; c < 3; ++c)
		{
			const FVector Fwd = FRotator(0, Yaw, 0).Vector();
			if (AActor* Chevron = Block(At + Fwd * (-250.0f + c * 250.0f) + FVector(0, 0, 26), FVector(90, 520, 6), Pink, Yaw, false)) { Paint(Chevron, NeonPink); }
		}
		BoostPads.Add(FBox(At - FVector(470, 470, 60), At + FVector(470, 470, 160)));
	};

	// --- the night sky: a moon, and stars above the haze ------------------------------------------
	// Night exposure: auto exposure kept brightening the dark sky back to dusk, so it is pinned
	// to a narrow range here (a second unbound volume, higher priority than the arena's grade).
	{
		FActorSpawnParameters Params;
		Params.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
		if (APostProcessVolume* NightGrade = GetWorld()->SpawnActor<APostProcessVolume>(FVector::ZeroVector, FRotator::ZeroRotator, Params))
		{
			NightGrade->SetReplicates(false);
			NightGrade->bUnbound = true;
			NightGrade->Priority = 10.0f;
			FPostProcessSettings& G = NightGrade->Settings;
			// Pinned (min = max), so the exposure does not wander between the sky and a neon sign. Lower EV is a
			// brighter picture. 2026-11-14 ("still really dark"): was 0.3..1.2 with a -0.6 bias. DCC.RallyExposure
			// changes it live, for tuning.
			const float Ev = CVarRallyExposure.GetValueOnGameThread();
			G.bOverride_AutoExposureMinBrightness = true;
			G.AutoExposureMinBrightness = Ev;
			G.bOverride_AutoExposureMaxBrightness = true;
			G.AutoExposureMaxBrightness = Ev;
			G.bOverride_AutoExposureBias = true;
			G.AutoExposureBias = 0.0f;
			GRallyGrade = NightGrade;
			G.bOverride_BloomIntensity = true;
			G.BloomIntensity = 0.6f; // a little more glow on the neon at night
			LocalActors.Add(NightGrade);
		}
	}
	if (AActor* Moon = Shape(TEXT("Sphere"), O + FVector(-60000, 45000, 52000), FVector(9000, 9000, 9000), FLinearColor(1.0f, 0.95f, 0.9f), FRotator::ZeroRotator, false))
	{
		Paint(Moon, Glow);
	}
	for (int32 s = 0; s < 260; ++s)
	{
		const float A = R.FRandRange(0.0f, 2.0f * PI);
		const float El = FMath::Asin(R.FRandRange(0.25f, 1.0f)); // more of them high up
		const float Dist = 150000.0f;
		const FVector At = O + FVector(FMath::Cos(A) * FMath::Cos(El), FMath::Sin(A) * FMath::Cos(El), FMath::Sin(El)) * Dist;
		const float Size = R.FRandRange(150.0f, 420.0f);
		if (AActor* Star = Shape(TEXT("Sphere"), At, FVector(Size), FLinearColor::White, FRotator::ZeroRotator, false))
		{
			Paint(Star, s % 7 == 0 ? NeonCyan : s % 11 == 0 ? NeonPink : Glow);
		}
	}

	// --- kicker ramps: a jump in one lane of every other street segment ----------------------------
	for (int32 i = -StreetRadius; i <= StreetRadius; ++i)
	{
		for (int32 Axis = 0; Axis < 2; ++Axis)
		{
			for (int32 Seg = -StreetRadius; Seg < StreetRadius; ++Seg)
			{
				if ((i + Seg + Axis) % 2 != 0)
				{
					continue;
				}
				const float Along = (Seg + 0.5f) * StreetSpacing + R.FRandRange(-3000.0f, 3000.0f);
				const float Lane = ((i + Seg) % 4 < 2 ? -1.0f : 1.0f) * 500.0f;
				const bool bForward = R.FRand() < 0.5f;
				const float Yaw = (Axis == 0 ? 0.0f : 90.0f) + (bForward ? 0.0f : 180.0f);
				const FVector At = O + (Axis == 0 ? FVector(Along, i * StreetSpacing + Lane, 0) : FVector(i * StreetSpacing + Lane, Along, 0));
				const FVector2D Local(At.X - O.X, At.Y - O.Y);
				if (FVector2D::Distance(Local, YardOffset()) < 9000.0f || FVector2D::Distance(Local, GasOffset(0)) < 4000.0f || FVector2D::Distance(Local, GasOffset(1)) < 4000.0f)
				{
					continue;
				}
				// 14 m long, 16 degrees: a short ramp that throws you.
				const float Pitch = 16.0f, Len = 1400.0f;
				const FVector Center = At + FVector(0, 0, 10.0f + 0.5f * Len * FMath::Sin(FMath::DegreesToRadians(Pitch)));
				if (AActor* Kicker = Shape(TEXT("Cube"), Center, FVector(Len, 800, 50), Deck, FRotator(Pitch, Yaw, 0.0f), true))
				{
					(void)Kicker;
				}
				const FVector Fwd = FRotator(0, Yaw, 0).Vector();
				const FVector Side = FRotator(0, Yaw + 90.0f, 0).Vector();
				for (int32 Edge = -1; Edge <= 1; Edge += 2)
				{
					if (AActor* Stripe = Shape(TEXT("Cube"), Center + Side * (Edge * 400.0f) + FVector(0, 0, 32), FVector(Len, 30, 12), Pink, FRotator(Pitch, Yaw, 0.0f), false))
					{
						Paint(Stripe, Edge < 0 ? NeonPink : NeonCyan);
					}
				}
				// A pad just before it, so you hit the lip fast.
				Pad(At - Fwd * 1500.0f, Yaw);
			}
		}
	}

	// --- skyways: up a ramp to a neon deck over the crossings, then a long boosted descent ----------
	struct FSkyway { int32 Axis; float Fixed; float From; float To; float Side; float Rise; };
	const FSkyway Skyways[] = {
		{ 0, StreetSpacing * 1.0f + 500.0f, -14000.0f, 32000.0f, 1.0f, 900.0f },  // along X, on the y=+1 street's north lane
		{ 1, StreetSpacing * 1.0f - 500.0f, -34000.0f, 14000.0f, -1.0f, 900.0f }, // along Y, on the x=+1 street's west lane (clear of gas station 0)
		// Higher deck on the south street. The yard is far to the -X/-Y corner, and this run stays east of it.
		{ 0, -StreetSpacing - 500.0f, -4000.0f, 52000.0f, -1.0f, 2400.0f },
	};
	const float Width = 1000.0f;
	for (const FSkyway& Sky : Skyways)
	{
		const float Rise = Sky.Rise;
		const float RampLen = FMath::Max(6000.0f, Rise * 4.5f);
		const float DownLen = FMath::Max(9000.0f, Rise * 6.0f);
		const float Yaw = Sky.Axis == 0 ? 0.0f : 90.0f;
		auto Point = [&](float Along, float Z) { return O + (Sky.Axis == 0 ? FVector(Along, Sky.Fixed, Z) : FVector(Sky.Fixed, Along, Z)); };
		const float UpPitch = FMath::RadiansToDegrees(FMath::Atan2(Rise, RampLen));
		const float DownPitch = FMath::RadiansToDegrees(FMath::Atan2(Rise, DownLen));
		const float DeckFrom = Sky.From + RampLen, DeckTo = Sky.To - DownLen;
		// Up.
		Shape(TEXT("Cube"), Point(Sky.From + RampLen * 0.5f, Rise * 0.5f - 15.0f), FVector(FMath::Sqrt(RampLen * RampLen + Rise * Rise), Width, 60), Deck, FRotator(UpPitch, Yaw, 0), true);
		// Deck.
		Shape(TEXT("Cube"), Point((DeckFrom + DeckTo) * 0.5f, Rise - 30.0f), FVector(DeckTo - DeckFrom, Width, 60), Deck, FRotator(0, Yaw, 0), true);
		// Down: longer and shallower, so it is the fast part.
		Shape(TEXT("Cube"), Point(DeckTo + DownLen * 0.5f, Rise * 0.5f - 15.0f), FVector(FMath::Sqrt(DownLen * DownLen + Rise * Rise), Width, 60), Deck, FRotator(-DownPitch, Yaw, 0), true);
		// Low neon rails both sides along the whole run (they keep you on the deck), pillars under the deck.
		for (int32 Edge = -1; Edge <= 1; Edge += 2)
		{
			const FVector Off = (Sky.Axis == 0 ? FVector(0, 1, 0) : FVector(1, 0, 0)) * (Edge * (Width * 0.5f + 20.0f));
			if (AActor* Rail = Shape(TEXT("Cube"), Point((DeckFrom + DeckTo) * 0.5f, Rise + 30.0f) + Off, FVector(DeckTo - DeckFrom, 40, 80), Pink, FRotator(0, Yaw, 0), true))
			{
				Paint(Rail, Edge < 0 ? NeonPink : NeonCyan);
			}
			if (AActor* RailDown = Shape(TEXT("Cube"), Point(DeckTo + DownLen * 0.5f, Rise * 0.5f + 45.0f) + Off, FVector(FMath::Sqrt(DownLen * DownLen + Rise * Rise), 40, 80), Cyan, FRotator(-DownPitch, Yaw, 0), true))
			{
				Paint(RailDown, Edge < 0 ? NeonPink : NeonCyan);
			}
		}
		for (float A = DeckFrom + 2000.0f; A < DeckTo; A += 4000.0f)
		{
			const FVector Off = (Sky.Axis == 0 ? FVector(0, 1, 0) : FVector(1, 0, 0)) * (Sky.Side * (Width * 0.5f + 120.0f));
			Shape(TEXT("Cylinder"), Point(A, Rise * 0.5f) + Off, FVector(120, 120, Rise), Deck, FRotator::ZeroRotator, true);
			Light(Point(A, Rise + 300.0f), (int32(A / 4000.0f) % 2) ? Pink : Cyan, 4000.0f, 1800.0f);
		}
		// Boost pads down the descent: the downhill speedy part.
		for (int32 p = 0; p < 3; ++p)
		{
			const float A = DeckTo + DownLen * (0.2f + 0.3f * p);
			const float Z = Rise * (1.0f - (A - DeckTo) / DownLen);
			Pad(Point(A, Z), Yaw);
		}
	}

	// --- loose pads around the grid ------------------------------------------------------------------
	for (int32 n = 0; n < 14; ++n)
	{
		const int32 i = R.RandRange(-StreetRadius, StreetRadius);
		const int32 Axis = R.RandRange(0, 1);
		const float Along = R.FRandRange(-StreetRadius * StreetSpacing + 3000.0f, StreetRadius * StreetSpacing - 3000.0f);
		const float Lane = R.FRand() < 0.5f ? -500.0f : 500.0f;
		const FVector At = O + (Axis == 0 ? FVector(Along, i * StreetSpacing + Lane, 0) : FVector(i * StreetSpacing + Lane, Along, 0));
		const FVector2D Local(At.X - O.X, At.Y - O.Y);
		if (FVector2D::Distance(Local, YardOffset()) < 9000.0f)
		{
			continue;
		}
		Pad(At, Axis == 0 ? (R.FRand() < 0.5f ? 0.0f : 180.0f) : (R.FRand() < 0.5f ? 90.0f : 270.0f));
	}
}

void ADCCArcadeKart::DressStreetLife()
{
	// Towers sit on the four corners (TowerInset / TowerFit). Street life stays in the courtyard and in
	// the sidewalk gap, short of the asphalt. None of it sits on a lane. The low sidewalk furniture
	// collides; the tall signs, holograms and the extra dressing added with the wider streets do not.
	const FVector O = GetActorLocation();
	FRandomStream R(7731);
	const float S = StreetSpacing;

	static const TCHAR* Benches[] = {
		TEXT("/Game/Cyberpunk_City/Meshes/Bench/SM_bench"),
		TEXT("/Game/Sci_Fi_city/Meshes/Props/SM_bench"),
		TEXT("/Game/Sci_Fi_city/Meshes/Props/SM_round_bench"),
	};
	static const TCHAR* Cans[] = {
		TEXT("/Game/Cyberpunk_City/Meshes/Trashcan/SM_trashcan_01"),
		TEXT("/Game/Cyberpunk_City/Meshes/Trashcan/SM_trashcan_02"),
		TEXT("/Game/Sci_Fi_city/Meshes/Props/SM_trashcan"),
	};
	static const TCHAR* Neons[] = {
		TEXT("/Game/Cyberpunk_City/Meshes/Neon/SM_neon_01"),
		TEXT("/Game/Cyberpunk_City/Meshes/Neon/SM_neon_02"),
	};
	static const TCHAR* Signs[] = {
		TEXT("/Game/Cyberpunk_City/Meshes/Sign/SM_sign_02"),
		TEXT("/Game/Cyberpunk_City/Meshes/Sign/SM_sign_04"),
		TEXT("/Game/Cyberpunk_City/Meshes/Sign/SM_sign_08"),
		TEXT("/Game/Synthwave_city/Meshes/Road_sign/SM_road_sign"),
	};
	static const TCHAR* Holos[] = {
		TEXT("/Game/Cyberpunk_City/Meshes/Hologram/SM_hologram_01"),
		TEXT("/Game/Cyberpunk_City/Meshes/Hologram/SM_hologram_03"),
		TEXT("/Game/Cyberpunk_City/Meshes/Hologram/SM_hologram_06"),
		TEXT("/Game/Synthwave_city/Meshes/Hologram_decoration/SM_hologram_decoration"),
	};
	static const TCHAR* Gardens[] = {
		TEXT("/Game/Sci_Fi_city/Meshes/Garden/SM_garden_01"),
		TEXT("/Game/Sci_Fi_city/Meshes/Garden/SM_garden_02"),
		TEXT("/Game/Sci_Fi_city/Meshes/Garden/Sm_circular_garden"),
		TEXT("/Game/Sci_Fi_city/Meshes/Garden/Sm_circular_garden_02"),
	};
	static const TCHAR* Flyers[] = {
		TEXT("/Game/Cyberpunk_City/Meshes/Flying_car_01/SM_flying_car_01"),
		TEXT("/Game/Cyberpunk_City/Meshes/Flying_car_01/SM_flying_car_02"),
		TEXT("/Game/Cyberpunk_City/Meshes/Flying_car_01/SM_flying_car_bis"),
	};

	for (int32 Ix = -StreetRadius; Ix < StreetRadius; ++Ix)
	{
		for (int32 Iy = -StreetRadius; Iy < StreetRadius; ++Iy)
		{
			if (Ix == -StreetRadius && Iy == -StreetRadius)
			{
				continue; // the yard has its own pass below
			}
			const FVector Cell = O + FVector((Ix + 0.5f) * S, (Iy + 0.5f) * S, 0);

			// Courtyard: the open cross between the four towers (|x| and |y| under ~9 m).
			Prop(Gardens[R.RandRange(0, UE_ARRAY_COUNT(Gardens) - 1)], Cell + FVector(R.FRandRange(-250.0f, 250.0f), R.FRandRange(-250.0f, 250.0f), 0),
				R.FRandRange(0.0f, 360.0f), FVector(700, 700, 0), true);
			Prop(TEXT("/Game/Synthwave_city/Meshes/Floor/SM_floor_02"), Cell, R.RandRange(0, 3) * 90.0f, FVector(1200, 1200, 0), false);
			if (R.FRand() < 0.7f)
			{
				Prop(TEXT("/Game/Sci_Fi_city/Meshes/Landing_pad/SM_landing_pad"), Cell, R.RandRange(0, 3) * 90.0f, FVector(520, 520, 0), false);
				Prop(Flyers[R.RandRange(0, UE_ARRAY_COUNT(Flyers) - 1)], Cell + FVector(0, 0, R.FRandRange(1400.0f, 2600.0f)),
					R.FRandRange(0.0f, 360.0f), FVector(480, 200, 0), false);
			}
			Prop(TEXT("/Game/Sci_Fi_city/Meshes/Lamp/SM_lamp_neon"), Cell + FVector(420, -420, 0), 45.0f, FVector(0, 0, 220), false);

			// One storefront in the sidewalk gap of each face. The towers cover the corners.
			const float Walk = StreetSpacing * 0.5f - RoadHalf - 900.0f;
			for (int32 Edge = 0; Edge < 4; ++Edge)
			{
				const FVector Out = (Edge == 0) ? FVector(1, 0, 0) : (Edge == 1) ? FVector(-1, 0, 0) : (Edge == 2) ? FVector(0, 1, 0) : FVector(0, -1, 0);
				const FVector Along = FVector(-Out.Y, Out.X, 0);
				const float Face = Out.Rotation().Yaw;
				const FVector Front = Cell + Out * Walk;
				const int32 Kind = R.RandRange(0, 3);
				if (Kind == 0)
				{
					const FString Kiosk = FString::Printf(TEXT("/Game/Cyberpunk_City/Meshes/Street_modules/SM_street_module_0%d"), 1 + R.RandRange(0, 2));
					Prop(*Kiosk, Front, Face, FVector(420, 280, 0), true);
				}
				else if (Kind == 1)
				{
					Prop(Neons[R.RandRange(0, 1)], Front, Face, FVector(0, 0, 300), false);
				}
				else if (Kind == 2)
				{
					Prop(Signs[R.RandRange(0, 3)], Front, Face, FVector(0, 0, 340), false);
				}
				else
				{
					Prop(Holos[R.RandRange(0, 3)], Front, Face, FVector(0, 0, 240), false);
				}
				Prop(Benches[R.RandRange(0, 2)], Front + Along * 380.0f, Face + 90.0f, FVector(0, 0, 85), true);
				Prop(Cans[R.RandRange(0, 2)], Front - Along * 380.0f, Face, FVector(0, 0, 110), true);
				if (R.FRand() < 0.4f)
				{
					Prop(TEXT("/Game/Synthwave_city/Meshes/Column/SM_circular_column"), Front + Along * 680.0f, Face, FVector(0, 0, 420), true);
				}
				// More of the block, still off the lane, and not a new wall to get a mark stuck in.
				Prop(Gardens[R.RandRange(0, UE_ARRAY_COUNT(Gardens) - 1)], Front - Out * 220.0f + Along * 980.0f, Face, FVector(220, 220, 0), false);
				if (R.FRand() < 0.35f)
				{
					// Parked above the sidewalk, not in a lane, and it does not collide.
					Prop(Flyers[R.RandRange(0, UE_ARRAY_COUNT(Flyers) - 1)], Front + Along * 1500.0f + FVector(0, 0, 80), Face + 15.0f, FVector(420, 180, 0), false);
				}
				if (R.FRand() < 0.55f)
				{
					Prop(Holos[R.RandRange(0, UE_ARRAY_COUNT(Holos) - 1)], Front + Along * -920.0f, Face, FVector(0, 0, 260), false);
				}
				if ((Ix + Iy + Edge) % 3 == 0)
				{
					Prop(TEXT("/Game/Synthwave_city/Meshes/Light_base/SM_light_base"), Front + Along * 1400.0f, Face, FVector(0, 0, 50), false);
					Light(Front + Along * 1400.0f + FVector(0, 0, 160), (Edge % 2) ? FLinearColor(1.0f, 0.2f, 0.85f) : FLinearColor(0.2f, 0.9f, 1.0f), 1800.0f, 700.0f);
				}
			}
		}
	}

	// The yard: a fence on three sides and either side of the gate, so the impound reads as a lot.
	// The opening faces +X, where the trucks leave. The drop pad, the bays and the pump stay inside.
	const FVector Yard = O + FVector(YardOffset().X, YardOffset().Y, 0);
	auto FenceRun = [&](const FVector& From, const FVector& To)
	{
		const FVector Delta = To - From;
		const float Len = Delta.Size2D();
		const FVector Dir = Delta.GetSafeNormal2D();
		const float Yaw = Dir.Rotation().Yaw;
		for (float D = 180.0f; D < Len - 180.0f; D += 400.0f)
		{
			Prop(TEXT("/Game/Synthwave_city/Meshes/Fence/SM_fence"), From + Dir * D, Yaw, FVector(380, 0, 170), false);
		}
	};
	const float E = 6400.0f;
	FenceRun(Yard + FVector(-E, -E, 0), Yard + FVector(-E, E, 0));
	FenceRun(Yard + FVector(-E, E, 0), Yard + FVector(E, E, 0));
	FenceRun(Yard + FVector(-E, -E, 0), Yard + FVector(E, -E, 0));
	FenceRun(Yard + FVector(E, -E, 0), Yard + FVector(E, -2400, 0));
	FenceRun(Yard + FVector(E, 2400, 0), Yard + FVector(E, E, 0));

	Prop(TEXT("/Game/Sci_Fi_city/Meshes/Antennas/SM_antenna_02"), Yard + FVector(-2800, 4000, 0), 15.0f, FVector(0, 0, 640), false);
	Prop(TEXT("/Game/Cyberpunk_City/Meshes/Hologram/SM_hologram_08"), Yard + FVector(-600, 2800, 0), 90.0f, FVector(0, 0, 280), false);
	Prop(TEXT("/Game/Sci_Fi_city/Meshes/Props/SM_round_bench"), Yard + FVector(2400, -3400, 0), 15.0f, FVector(0, 0, 80), false);
	Prop(TEXT("/Game/Cyberpunk_City/Meshes/Trashcan/SM_trashcan_01"), Yard + FVector(3100, -2800, 0), 0.0f, FVector(0, 0, 110), false);
	Prop(TEXT("/Game/Synthwave_city/Meshes/Light_base/SM_light_base"), Yard + FVector(0, 0, 0), 0.0f, FVector(0, 0, 40), false);
	for (int32 i = 0; i < 3; ++i)
	{
		Prop(TEXT("/Game/Sci_Fi_city/Meshes/Crates/SM_crate_holder_01"), Yard + FVector(-5400, 400.0f + i * 1100.0f, 0), 12.0f * i, FVector(0, 0, 180), false);
		const FString Crate = FString::Printf(TEXT("/Game/Cyberpunk_City/Meshes/Crates/SM_crate_0%d"), 1 + (i % 4));
		Prop(*Crate, Yard + FVector(-5000, 600.0f + i * 1100.0f, 0), -8.0f * i, FVector(0, 0, 90), false);
	}

	// Above each canopy, so it cannot sit in the drive-through or in the tower beside the station.
	for (int32 g = 0; g < 2; ++g)
	{
		const FVector At = O + FVector(GasOffset(g).X, GasOffset(g).Y, 0);
		Prop(Holos[g], At + FVector(0, 0, 900), g ? 30.0f : 0.0f, FVector(0, 0, 220), false);
	}

	// Crossing arches on every other intersection. The mesh does not collide, so a truck drives under it.
	for (int32 Ix = -StreetRadius; Ix <= StreetRadius; Ix += 2)
	{
		for (int32 Iy = -StreetRadius; Iy <= StreetRadius; Iy += 2)
		{
			if (Ix == -StreetRadius && Iy == -StreetRadius)
			{
				continue;
			}
			const FVector Cross = O + FVector(Ix * S, Iy * S, 0);
			Prop(TEXT("/Game/Synthwave_city/Meshes/Arch/SM_arch"), Cross, ((Ix + Iy) & 2) ? 0.0f : 90.0f, FVector(500, 4200, 0), false);
			Light(Cross + FVector(0, 0, 700), ((Ix + Iy) & 2) ? FLinearColor(1.0f, 0.2f, 0.85f) : FLinearColor(0.2f, 0.9f, 1.0f), 2500.0f, 900.0f);
		}
	}

	// Graffiti. A board in each sidewalk gap (the towers leave that gap open, so the board always has
	// somewhere to stand), more on the yard fence, and a couple on the station pillars. The names are
	// the crew and the companies that already own this fiction. Nothing here collides.
	{
		FRandomStream Tag(4402);
		static const TCHAR* Words[] = {
			TEXT("ZOE"), TEXT("SCOTT"), TEXT("CJ"), TEXT("MARCEL"), TEXT("VAUGHN"), TEXT("KYLE"),
			TEXT("WAS HERE"), TEXT("THE CREW"), TEXT("MILE HIGH"), TEXT("MHRS"), TEXT("BORANT"),
			TEXT("VALTAY"), TEXT("LIQUIDATOR"), TEXT("SUMMIT"), TEXT("VANTAGE"), TEXT("BRIGHT PATH"),
			TEXT("KMHR"), TEXT("5280"), TEXT("FORECLOSED"), TEXT("THE TAB"), TEXT("NOT YOURS"),
			TEXT("REPO ME"), TEXT("SYSTEM"), TEXT("OWNED"), TEXT("DEBT"), TEXT("HOOKED"),
		};
		const FColor Inks[] = {
			FColor(255, 70, 170), FColor(70, 230, 255), FColor(255, 220, 60), FColor(170, 255, 120), FColor(245, 245, 255),
		};
		const FLinearColor BoardColour(0.025f, 0.012f, 0.04f);
		auto Word = [&]() { return Words[Tag.RandRange(0, UE_ARRAY_COUNT(Words) - 1)]; };
		auto Spray = [&](const FVector& At, float Yaw, float Size)
		{
			const FColor Ink = Inks[Tag.RandRange(0, UE_ARRAY_COUNT(Inks) - 1)];
			if (ATextRenderActor* Sign = Label(At, Yaw, Word(), Ink, Size))
			{
				Sign->SetActorRotation(FRotator(Tag.FRandRange(-7.0f, 7.0f), Yaw, Tag.FRandRange(-16.0f, 16.0f)));
			}
		};

		for (int32 Ix = -StreetRadius; Ix < StreetRadius; ++Ix)
		{
			for (int32 Iy = -StreetRadius; Iy < StreetRadius; ++Iy)
			{
				if (Ix == -StreetRadius && Iy == -StreetRadius)
				{
					continue;
				}
				const FVector Cell = O + FVector((Ix + 0.5f) * S, (Iy + 0.5f) * S, 0);
				for (int32 Edge = 0; Edge < 4; ++Edge)
				{
					const FVector Out = (Edge == 0) ? FVector(1, 0, 0) : (Edge == 1) ? FVector(-1, 0, 0) : (Edge == 2) ? FVector(0, 1, 0) : FVector(0, -1, 0);
					const FVector Along = FVector(-Out.Y, Out.X, 0);
					const float Face = Out.Rotation().Yaw;
					const FVector Front = Cell + Out * 8200.0f;
					const float Side = Tag.FRand() < 0.5f ? 1.0f : -1.0f;
					const FVector BoardAt = Front + Along * Side * 480.0f + FVector(0, 0, 780);
					Block(BoardAt, FVector(20, 620, 1500), BoardColour, Face, true);
					Spray(Front + Out * 50.0f + FVector(0, 0, 220), Face, Tag.FRandRange(44.0f, 72.0f));
					for (int32 Line = 0; Line < 3; ++Line)
					{
						const float Z = 480.0f - Line * 400.0f;
						Spray(BoardAt + Out * 24.0f + Along * Tag.FRandRange(-30.0f, 30.0f) + FVector(0, 0, Z),
							Face, Tag.FRandRange(56.0f, 92.0f));
					}
				}
			}
		}

		auto FenceTags = [&](const FVector& From, const FVector& To, float InwardYaw)
		{
			const FVector Delta = To - From;
			const float Len = Delta.Size2D();
			const FVector Dir = Delta.GetSafeNormal2D();
			const FVector In = FRotator(0.0f, InwardYaw, 0.0f).Vector();
			for (float D = 600.0f; D < Len - 600.0f; D += Tag.FRandRange(900.0f, 1600.0f))
			{
				Spray(From + Dir * D + In * 90.0f + FVector(0, 0, Tag.FRandRange(170.0f, 340.0f)), InwardYaw, Tag.FRandRange(48.0f, 84.0f));
			}
		};
		FenceTags(Yard + FVector(-E, -E, 0), Yard + FVector(-E, E, 0), 0.0f);
		FenceTags(Yard + FVector(-E, E, 0), Yard + FVector(E, E, 0), -90.0f);
		FenceTags(Yard + FVector(-E, -E, 0), Yard + FVector(E, -E, 0), 90.0f);
		FenceTags(Yard + FVector(E, -E, 0), Yard + FVector(E, -2600, 0), 180.0f);
		FenceTags(Yard + FVector(E, 2600, 0), Yard + FVector(E, E, 0), 180.0f);

		for (int32 g = 0; g < 2; ++g)
		{
			const FVector At = O + FVector(GasOffset(g).X, GasOffset(g).Y, 0);
			Spray(At + FVector(1300, 200, 260), 90.0f, 64.0f);
			Spray(At + FVector(-200, 1100, 240), 0.0f, 58.0f);
		}
	}

	// Painted graffiti, wall grime and puddles (2026-11-14, Zoe: "I didn't really see much graffiti"). The text
	// boards above were the only tags, and text reads as text. These are the Cyberpunk pack's vandalism decals,
	// sprayed onto the real tower faces: a trace from the street finds the wall, so a gap in the skyline gets
	// nothing and a thin tower still gets painted where it actually is. Puddles go in the gutters, where the
	// neon reflects in them. All local, no collision.
	{
		FRandomStream Graf(9113);
		auto Mat = [](const FString& Path) { return LoadObject<UMaterialInterface>(nullptr, *(Path + TEXT(".") + FPaths::GetBaseFilename(Path))); };
		TArray<UMaterialInterface*> WallTags, Grime, Puddles;
		for (int32 i = 1; i <= 10; ++i)
		{
			if (UMaterialInterface* M = Mat(FString::Printf(TEXT("/Game/Cyberpunk_City/Materials/Decals/MI_decal_vandalism_%02d"), i))) { WallTags.Add(M); }
		}
		for (int32 i = 1; i <= 7; ++i)
		{
			if (UMaterialInterface* M = Mat(FString::Printf(TEXT("/Game/Abandonned_Brutalist/Materials/MI_decal_0%d"), i))) { Grime.Add(M); }
		}
		for (const TCHAR* P : { TEXT("/Game/Cyberpunk_City/Materials/MI_decal__puddle"), TEXT("/Game/Abandonned_Brutalist/Materials/MI_decal__puddle") })
		{
			if (UMaterialInterface* M = Mat(P)) { Puddles.Add(M); }
		}

		auto Decal = [&](UMaterialInterface* M, const FVector& At, const FRotator& Rot, const FVector& HalfSize, int32 Order)
		{
			if (!M)
			{
				return;
			}
			FActorSpawnParameters Params;
			Params.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
			if (ADecalActor* D = GetWorld()->SpawnActor<ADecalActor>(At, Rot, Params))
			{
				D->SetReplicates(false);
				D->SetDecalMaterial(M);
				D->GetDecal()->DecalSize = HalfSize;
				D->GetDecal()->SortOrder = Order;
				D->GetDecal()->FadeScreenSize = 0.0f; // stays painted at distance
				LocalActors.Add(D);
			}
		};

		FCollisionObjectQueryParams Statics(ECC_WorldStatic);
		for (int32 Ix = -StreetRadius; Ix < StreetRadius; ++Ix)
		{
			for (int32 Iy = -StreetRadius; Iy < StreetRadius; ++Iy)
			{
				if (Ix == -StreetRadius && Iy == -StreetRadius)
				{
					continue;
				}
				const FVector Cell = O + FVector((Ix + 0.5f) * S, (Iy + 0.5f) * S, 0);
				for (int32 Edge = 0; Edge < 4; ++Edge)
				{
					const FVector Out = (Edge == 0) ? FVector(1, 0, 0) : (Edge == 1) ? FVector(-1, 0, 0) : (Edge == 2) ? FVector(0, 1, 0) : FVector(0, -1, 0);
					const FVector Along = FVector(-Out.Y, Out.X, 0);
					// The two towers on this face sit at +-4300 along it; spray each a few times.
					for (int32 Side = -1; Side <= 1; Side += 2)
					{
						const int32 Count = Graf.RandRange(2, 4);
						for (int32 k = 0; k < Count; ++k)
						{
							const float A = Side * Graf.FRandRange(1800.0f, 6800.0f);
							const float Z = Graf.FRandRange(180.0f, 900.0f);
							const FVector From = Cell + Out * 9400.0f + Along * A + FVector(0, 0, Z);
							FHitResult Hit;
							if (!GetWorld()->LineTraceSingleByObjectType(Hit, From, From - Out * 3600.0f, Statics) || Hit.Distance < 300.0f)
							{
								continue;
							}
							// Only near-vertical walls that face the street.
							if (FMath::Abs(Hit.ImpactNormal.Z) > 0.3f || FVector::DotProduct(Hit.ImpactNormal, Out) < 0.5f)
							{
								continue;
							}
							const FRotator Rot = (-Hit.ImpactNormal).Rotation() + FRotator(0.0f, 0.0f, Graf.FRandRange(-8.0f, 8.0f));
							const bool bTag = WallTags.Num() > 0 && (Grime.IsEmpty() || Graf.FRand() < 0.72f);
							if (bTag)
							{
								const float Wd = Graf.FRandRange(380.0f, 720.0f);
								Decal(WallTags[Graf.RandRange(0, WallTags.Num() - 1)], Hit.ImpactPoint, Rot, FVector(60.0f, Wd, Wd * Graf.FRandRange(0.45f, 0.75f)), 2);
							}
							else
							{
								const float Wd = Graf.FRandRange(500.0f, 1100.0f);
								Decal(Grime[Graf.RandRange(0, Grime.Num() - 1)], Hit.ImpactPoint + FVector(0, 0, 200), Rot, FVector(60.0f, Wd, Wd * 0.9f), 1);
							}
						}
					}
					// Puddles in the gutter on this side of the street, between the parked cars and the curb.
					if (Puddles.Num() > 0)
					{
						const int32 N = Graf.RandRange(1, 3);
						for (int32 k = 0; k < N; ++k)
						{
							const FVector At = Cell + Out * Graf.FRandRange(8800.0f, 9100.0f) + Along * Graf.FRandRange(-8000.0f, 8000.0f) + FVector(0, 0, 40);
							const float Rad = Graf.FRandRange(220.0f, 520.0f);
							Decal(Puddles[Graf.RandRange(0, Puddles.Num() - 1)], At, FRotator(-90.0f, Graf.FRandRange(0.0f, 360.0f), 0.0f), FVector(120.0f, Rad, Rad * Graf.FRandRange(0.5f, 0.9f)), 0);
						}
					}
				}
			}
		}

		// The yard fence and the station pillars get tags too, at eye level, where a driver looks.
		for (int32 g = 0; g < 2 && WallTags.Num() > 0; ++g)
		{
			const FVector At = O + FVector(GasOffset(g).X, GasOffset(g).Y, 0);
			for (int32 k = 0; k < 4; ++k)
			{
				const FVector Spot = At + FVector(Graf.FRandRange(-2400.0f, 2400.0f), Graf.FRandRange(-2400.0f, 2400.0f), 30.0f);
				const float Rad = Graf.FRandRange(260.0f, 460.0f);
				Decal(WallTags[Graf.RandRange(0, WallTags.Num() - 1)], Spot, FRotator(-90.0f, Graf.FRandRange(0.0f, 360.0f), 0.0f), FVector(80.0f, Rad, Rad * 0.6f), 1);
			}
		}
	}
}
