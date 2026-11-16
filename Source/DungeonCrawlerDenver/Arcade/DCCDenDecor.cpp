#include "Arcade/DCCDenDecor.h"

#include "Arcade/DCCArcadeGames.h"

void FDCCDenDecor::Dress(ADCCArcadeLobby* Den, FName GameId, const FVector& Pad, const FLinearColor& Colour)
{
	if (!Den)
	{
		return;
	}
	// The pad is 7 m square; its sign block sits behind it at +520 (4.2 x 1.8 x 4.4 m), the wall at +800.
	// Pads are 10 m apart, so a scene keeps within +-4.6 m of its pad's centre line.
	const FVector Top = Pad + FVector(0, 520, 440);    // on top of the sign block
	const FVector Left = Pad + FVector(-380, 600, 0);  // beside it, on the floor
	const FVector Right = Pad + FVector(380, 600, 0);
	const FVector FrontL = Pad + FVector(-290, -290, 6);
	const FVector FrontR = Pad + FVector(290, -290, 6);

	if (GameId == TEXT("Crawler"))
	{
		Den->Prop(TEXT("/Game/UnrealBucket/Library/Various/Chest/SM_Chest_Open"), Top, 180.0f, FVector(0, 0, 110), true);
		Den->Prop(TEXT("/Game/Legendary_Forge/Meshes/Tool_holder/SM_weapon_holder"), Left, 180.0f, FVector(0, 0, 210), true);
		Den->Prop(TEXT("/Game/Legendary_Forge/Meshes/Brasero/SM_brasero_01"), Right, 0.0f, FVector(0, 0, 130), true);
		Den->Light(Right + FVector(0, 0, 170), FLinearColor(1.0f, 0.55f, 0.2f), 6000.0f, 700.0f);
		Den->Prop(TEXT("/Game/Cave_Camp/Meshes/Props_02/SM_crate_group_02"), FrontL, 30.0f, FVector(0, 0, 70), true);
		Den->Prop(TEXT("/Game/SoulCave/Environment/Meshes/Building/SM_Cave_Statue_01"), FrontR + FVector(0, 0, -6), 200.0f, FVector(0, 0, 160), true);
		Den->Light(Top + FVector(0, -150, 120), FLinearColor(1.0f, 0.8f, 0.4f), 3500.0f, 500.0f); // gold off the chest
	}
	else if (GameId == TEXT("Haunt"))
	{
		// The stream: an old TV on the sign block, its screen glowing static-green.
		Den->Prop(TEXT("/Game/Beach_house/Meshes/TV/SM_tv_02"), Top, 180.0f, FVector(0, 0, 120), true);
		Den->Block(Top + FVector(0, -60, 60), FVector(150, 6, 90), FLinearColor(0.4f, 1.6f, 0.5f), 0.0f, false);
		Den->Light(Top + FVector(0, -200, 60), FLinearColor(0.4f, 1.0f, 0.5f), 4500.0f, 800.0f);
		Den->Prop(TEXT("/Game/SoulCave/Environment/Meshes/Building/SM_Cave_Statue_01"), Left, 160.0f, FVector(0, 0, 260), true);
		Den->Prop(TEXT("/Game/SoulCave/Environment/Meshes/Building/SM_Cave_Chain_02"), Right + FVector(0, 0, 40), 0.0f, FVector(0, 0, 320));
		// A ghost: a pale, unlit-looking orb hanging over the pad's corner, with its own cold light.
		Den->Shape(TEXT("Sphere"), FrontR + FVector(0, 0, 230), FVector(60, 60, 80), FLinearColor(0.85f, 0.95f, 1.0f), FRotator::ZeroRotator, false);
		Den->Light(FrontR + FVector(0, 0, 230), FLinearColor(0.7f, 0.9f, 1.0f), 2000.0f, 400.0f);
	}
	else if (GameId == TEXT("Climb"))
	{
		Den->Prop(TEXT("/Game/Legendary_Forge/Meshes/Rocks/Large_rocks/SM_Large_rock_01"), Top + FVector(0, 0, -20), 30.0f, FVector(380, 200, 170), false, true);
		if (AActor* Ice = Den->Shape(TEXT("Sphere"), Top + FVector(60, -70, 90), FVector(120, 30, 80), FLinearColor(0.75f, 0.92f, 1.0f), FRotator::ZeroRotator, false))
		{
			Den->Paint(Ice, TEXT("/Game/Ice_Station/Materials/Ice/MI_ice_01"));
		}
		Den->Prop(TEXT("/Game/Viking_village/Biome/Meshes/Tree/SM_pine_tree_Var_01"), Left, 0.0f, FVector(0, 0, 520), true);
		Den->Prop(TEXT("/Game/Cave_Camp/Meshes/Props_02/SM_tent_01"), Right, 200.0f, FVector(200, 200, 0), true);
		// A coil of rope and a little flag on the summit.
		Den->Shape(TEXT("Cylinder"), FrontL + FVector(0, 0, 10), FVector(90, 90, 20), FLinearColor(0.75f, 0.55f, 0.25f), FRotator::ZeroRotator, false);
		Den->Shape(TEXT("Cylinder"), Top + FVector(-120, 0, 260), FVector(6, 6, 200), FLinearColor(0.9f, 0.9f, 0.9f), FRotator::ZeroRotator, false);
		Den->Block(Top + FVector(-80, 0, 330), FVector(70, 4, 45), FLinearColor(0.95f, 0.2f, 0.2f), 0.0f, false);
		Den->Light(Top + FVector(0, -200, 200), FLinearColor(0.7f, 0.85f, 1.0f), 3500.0f, 700.0f);
	}
	else if (GameId == TEXT("Kart"))
	{
		// A mark, nose-out, beside the sign block; the yard's amber beacon on top.
		Den->Prop(TEXT("/Game/Synthwave_city/Meshes/Car/SM_car_01"), Left + FVector(-20, -30, 0), 0.0f, FVector(220, 420, 0), true);
		Den->Shape(TEXT("Cylinder"), Top + FVector(0, 0, 120), FVector(36, 36, 240), Colour * 2.5f, FRotator::ZeroRotator, false);
		Den->Light(Top + FVector(0, 0, 260), FLinearColor(1.0f, 0.55f, 0.15f), 7000.0f, 900.0f);
		Den->Prop(TEXT("/Game/SoulCave/Environment/Meshes/Building/SM_Cave_Chain_02"), Right + FVector(0, 0, 30), 90.0f, FVector(0, 0, 300));
		Den->Shape(TEXT("Cone"), FrontL + FVector(0, 0, 40), FVector(45, 45, 80), FLinearColor(1.0f, 0.4f, 0.05f), FRotator::ZeroRotator, false);
		Den->Shape(TEXT("Cone"), FrontR + FVector(0, 0, 40), FVector(45, 45, 80), FLinearColor(1.0f, 0.4f, 0.05f), FRotator::ZeroRotator, false);
		Den->Prop(TEXT("/Game/Desert_cave/Meshes/Cart/Sm_wheel"), Right + FVector(0, -230, 0), 90.0f, FVector(0, 0, 90), true);
	}
	else if (GameId == TEXT("Neon"))
	{
		Den->Prop(TEXT("/Game/Beach_house/Meshes/Neon/SM_neon"), Top, 180.0f, FVector(380, 0, 0));
		Den->Prop(TEXT("/Game/Beach_house/Meshes/Plants/SM_palm_tree_01"), Left, 40.0f, FVector(0, 0, 560), true);
		Den->Prop(TEXT("/Game/Sci_Fi_city/Meshes/Lamp/SM_lamp_neon"), Right, 180.0f, FVector(0, 0, 380), true);
		// Glowing lane strips across the front of the pad, like the highway.
		Den->Block(Pad + FVector(-300, -330, 8), FVector(40, 140, 4), FLinearColor(0.3f, 1.6f, 2.0f), 0.0f, false);
		Den->Block(Pad + FVector(300, -330, 8), FVector(40, 140, 4), FLinearColor(0.3f, 1.6f, 2.0f), 0.0f, false);
		Den->Light(Top + FVector(-200, -150, 150), FLinearColor(1.0f, 0.25f, 0.9f), 5000.0f, 800.0f);
		Den->Light(Top + FVector(200, -150, 150), FLinearColor(0.25f, 0.9f, 1.0f), 5000.0f, 800.0f);
	}
	else if (GameId == TEXT("Audit"))
	{
		Den->Shape(TEXT("Cylinder"), Pad + FVector(0, 20, 8), FVector(240, 240, 8), FLinearColor(0.85f, 0.1f, 0.08f), FRotator::ZeroRotator, false);
		Den->Shape(TEXT("Cylinder"), Pad + FVector(0, 20, 12), FVector(120, 120, 8), FLinearColor(0.95f, 0.92f, 0.85f), FRotator::ZeroRotator, false);
		Den->Shape(TEXT("Cylinder"), Pad + FVector(0, 20, 16), FVector(40, 40, 8), Colour * 2.0f, FRotator::ZeroRotator, false);
		Den->Shape(TEXT("Sphere"), Top + FVector(0, -30, 70), FVector(55, 55, 55), FLinearColor(1.3f, 0.45f, 0.12f), FRotator::ZeroRotator, false);
		Den->Light(Top + FVector(0, -80, 40), FLinearColor(1.0f, 0.5f, 0.15f), 5000.0f, 700.0f);
	}
}
