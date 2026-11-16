// Fourteener's strands and layers (2026-11-13, Zoe: "less of one giant pillar and more of a winding tree,
// several strands and layers"). ADCCArcadeClimb::BuildStrands, called at the end of BuildMountain.
//
// The trunk (the two stacked blocks) stays: ledges, ice, chests, rockfall, gusts, High Camp and the climb
// test all live on its four faces. Around it:
//   strands  four rock spires that wind around the trunk as they rise - each 16 m segment steps a little
//            further round - so their segments overhang one another. Different heights, so the
//            silhouette is a cluster, not a box. Any rock face can be climbed, so they are new routes.
//   layers   broad rock shelves wrapping part-way round the trunk at three heights: big places to rest,
//            and thick overhangs to crawl over on the way up.
// Built locally on every machine (no replication), like the rest of the mountain.

#include "Arcade/DCCArcadeGames.h"

namespace
{
	const TCHAR* CliffMat = TEXT("/Game/Legendary_Forge/Materials/Mi_rock_cliff_02");
	const TCHAR* Boulders[] = {
		TEXT("/Game/Legendary_Forge/Meshes/Rocks/Large_rocks/SM_Large_rock_01"),
		TEXT("/Game/Legendary_Forge/Meshes/Rocks/Large_rocks/SM_large_rock_02"),
		TEXT("/Game/Legendary_Forge/Meshes/Rocks/Small_rocks/SM_rock_assembly_01"),
	};
}

void ADCCArcadeClimb::BuildStrands()
{
	const FVector O = GetActorLocation();
	const FLinearColor Rock(0.34f, 0.3f, 0.27f);

	// --- strands -----------------------------------------------------------------------------------------
	struct FStrand { float StartAngle; int32 Segments; float Turn; float Size; };
	const FStrand Strands[] = {
		{ 45.0f, 4, 22.0f, 1000.0f },   // NE: a stubby one, to 64 m
		{ 135.0f, 7, -18.0f, 900.0f },  // NW: to 112 m, winding the other way
		{ 225.0f, 9, 20.0f, 850.0f },   // SW: the tall one, to 144 m
		{ 315.0f, 5, -25.0f, 950.0f },  // SE: to 80 m, beside High Camp
	};
	const float SegH = 1600.0f;
	for (int32 s = 0; s < UE_ARRAY_COUNT(Strands); ++s)
	{
		const FStrand& Strand = Strands[s];
		for (int32 k = 0; k < Strand.Segments; ++k)
		{
			const float Z = k * SegH;
			const float Angle = FMath::DegreesToRadians(Strand.StartAngle + Strand.Turn * k);
			// Hug the trunk: corners of the wide block below High Camp, the narrow block above.
			const float Radius = (Z + SegH * 0.5f < MidZ ? WideHalf : NarrowHalf) * 1.25f + Strand.Size * 0.25f;
			const float Taper = 1.0f - 0.04f * k;
			const FVector Center = O + FVector(FMath::Cos(Angle) * Radius, FMath::Sin(Angle) * Radius, Z + SegH * 0.5f);
			if (AActor* Seg = Block(Center, FVector(Strand.Size * Taper, Strand.Size * Taper, SegH), Rock * (0.85f + 0.05f * (k % 3)), Strand.StartAngle + Strand.Turn * k))
			{
				Paint(Seg, CliffMat);
			}
			// A boulder on every third segment's outer edge: something to aim for.
			if (k % 3 == 1)
			{
				const FVector Out = FVector(FMath::Cos(Angle), FMath::Sin(Angle), 0.0f);
				Prop(Boulders[(s + k) % 3], Center + Out * (Strand.Size * 0.5f * Taper + 80.0f) - FVector(0, 0, SegH * 0.2f), Out.Rotation().Yaw, FVector(600, 600, 380), false, true);
			}
		}
	}

	// --- layers ------------------------------------------------------------------------------------------
	struct FLayer { float Z; float FromAngle; float Span; };
	const FLayer Shelves[] = {
		{ 4000.0f, 20.0f, 200.0f },    // 40 m: wraps from the north-east round past the express face
		{ 11000.0f, 160.0f, 170.0f },  // 110 m: the west side
		{ 13500.0f, 290.0f, 150.0f },  // 135 m: under the summit, south-east
	};
	for (const FLayer& Layer : Shelves)
	{
		const float Half = Layer.Z < MidZ ? WideHalf : NarrowHalf;
		const int32 Pieces = FMath::Max(3, FMath::RoundToInt(Layer.Span / 22.0f));
		for (int32 p = 0; p < Pieces; ++p)
		{
			const float A = FMath::DegreesToRadians(Layer.FromAngle + Layer.Span * (p + 0.5f) / Pieces);
			const FVector Dir(FMath::Cos(A), FMath::Sin(A), 0.0f);
			// Out to just past the trunk's surface in that direction (a square's edge, not a circle).
			const float Edge = Half / FMath::Max(FMath::Abs(Dir.X), FMath::Abs(Dir.Y));
			const FVector Center = O + Dir * (Edge + 250.0f) + FVector(0, 0, Layer.Z);
			if (AActor* Shelf = Block(Center, FVector(900, 700, 360), Rock * 1.1f, FMath::RadiansToDegrees(A)))
			{
				Paint(Shelf, CliffMat);
			}
			if (p % 2 == 0)
			{
				Prop(Boulders[p % 3], Center + Dir * 380.0f - FVector(0, 0, 260), FMath::RadiansToDegrees(A) + 90.0f, FVector(700, 700, 300), false, true);
			}
		}
	}
}
