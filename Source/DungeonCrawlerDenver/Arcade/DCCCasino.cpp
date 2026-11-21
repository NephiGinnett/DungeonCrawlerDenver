#include "Arcade/DCCCasino.h"

#include "Arcade/DCCArcade.h"
#include "Arcade/DCCArcadeGames.h"
#include "Arcade/DCCWalletComponent.h"
#include "Progression/DCCAchievementComponent.h"
#include "Characters/DCCCharacterBase.h"
#include "Session/DCCProfile.h"
#include "Animation/AnimationAsset.h"
#include "Components/CapsuleComponent.h"
#include "Components/SkeletalMeshComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Engine/PointLight.h"
#include "Engine/SkeletalMesh.h"
#include "Engine/StaticMesh.h"
#include "Engine/TextRenderActor.h"
#include "Engine/World.h"
#include "Components/PointLightComponent.h"
#include "Components/TextRenderComponent.h"
#include "EngineUtils.h"
#include "TimerManager.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "GameFramework/PlayerController.h"
#include "GameFramework/PlayerState.h"
#include "HAL/IConsoleManager.h"
#include "Materials/MaterialInstanceDynamic.h"
#include "Materials/MaterialInterface.h"
#include "Net/UnrealNetwork.h"
#include "UObject/ConstructorHelpers.h"

namespace
{
	const FLinearColor CarpetGold(0.72f, 0.55f, 0.12f);
	const FLinearColor Felt(0.04f, 0.22f, 0.10f);
	const FLinearColor Wood(0.30f, 0.15f, 0.06f);
	const FLinearColor Ink(0.06f, 0.04f, 0.08f);

	FLinearColor CarpetColour(int32 X, int32 Y, bool bCasino)
	{
		const int32 H = FMath::Abs(X * 3 + Y * 5);
		const FLinearColor Burgundy(0.40f, 0.05f, 0.09f);
		const FLinearColor Navy(0.06f, 0.08f, 0.22f);
		const FLinearColor Wine(0.26f, 0.04f, 0.12f);
		const FLinearColor Teal(0.04f, 0.26f, 0.28f);
		const FLinearColor Plum(0.22f, 0.06f, 0.18f);
		if (bCasino)
		{
			switch (H % 6)
			{
			case 0: return CarpetGold;
			case 1: return Burgundy;
			case 2: return Navy;
			case 3: return Wine;
			case 4: return Plum;
			default: return Teal;
			}
		}
		// Brighter repeat, the arcade-and-movie-palace mix.
		switch (H % 8)
		{
		case 0: return FLinearColor(0.55f, 0.08f, 0.10f);
		case 1: return Navy;
		case 2: return FLinearColor(0.75f, 0.55f, 0.08f);
		case 3: return Burgundy;
		case 4: return FLinearColor(0.10f, 0.18f, 0.48f);
		case 5: return Wine;
		case 6: return FLinearColor(0.08f, 0.34f, 0.16f);
		default: return Teal;
		}
	}

	const TCHAR* GameTitle(int32 Table)
	{
		switch (Table)
		{
		case 0: return TEXT("Skee-Ball");
		case 1: return TEXT("Air Hockey");
		case 2: return TEXT("Roulette");
		case 3: return TEXT("Blackjack");
		case 4: return TEXT("The Wheel");
		default: return TEXT("Casino");
		}
	}

	const int32 WheelPay[] = { 1, 2, 5, 10, 20, 0 };
	const TCHAR* WheelWord[] = { TEXT("1"), TEXT("2"), TEXT("5"), TEXT("10"), TEXT("20"), TEXT("Bust") };
	const FLinearColor PitColours[] = {
		FLinearColor(1.0f, 0.15f, 0.2f), FLinearColor(1.0f, 0.75f, 0.1f), FLinearColor(0.2f, 0.45f, 1.0f),
		FLinearColor(0.15f, 0.8f, 0.35f), FLinearColor(0.7f, 0.2f, 0.9f), FLinearColor(0.10f, 0.10f, 0.12f),
	};
	const FLinearColor RingColours[] = {
		CarpetGold, FLinearColor(0.75f, 0.12f, 0.10f), FLinearColor(0.15f, 0.35f, 0.90f),
		FLinearColor(0.10f, 0.62f, 0.28f), FLinearColor(0.92f, 0.92f, 0.88f),
	};

	void Tint(AActor* Actor, const FLinearColor& Colour)
	{
		UStaticMeshComponent* Mesh = Actor ? Actor->FindComponentByClass<UStaticMeshComponent>() : nullptr;
		UMaterialInstanceDynamic* Mid = Mesh ? Cast<UMaterialInstanceDynamic>(Mesh->GetMaterial(0)) : nullptr;
		if (Mid)
		{
			Mid->SetVectorParameterValue(TEXT("Color"), Colour);
			Mid->SetVectorParameterValue(TEXT("BaseColor"), Colour);
		}
	}

	void Say(ATextRenderActor* Sign, const FString& Text, const FColor& Colour)
	{
		if (UTextRenderComponent* Render = Sign ? Sign->GetTextRender() : nullptr)
		{
			Render->SetText(FText::FromString(Text));
			Render->SetTextRenderColor(Colour);
		}
	}

	/** A slice lying in the YZ plane, face toward the room (-X). Degrees, 0 is up. */
	FRotator WallSlice(float Degrees)
	{
		return FRotator(90.0f - Degrees, 90.0f, 90.0f);
	}

	FVector WallRadial(float Degrees, float Radius)
	{
		const float A = FMath::DegreesToRadians(Degrees);
		return FVector(0.0f, FMath::Sin(A) * Radius, FMath::Cos(A) * Radius);
	}

	/** Flat on a table, long axis pointing out from the hub. Degrees, 0 is +X. */
	FRotator TableSlice(float Degrees)
	{
		return FRotator(0.0f, Degrees, 0.0f);
	}

	int32 SkeePoints(double Mark)
	{
		const double FromCentre = FMath::Abs(Mark - 0.5);
		if (FromCentre < 0.05) return 50;
		if (FromCentre < 0.12) return 40;
		if (FromCentre < 0.20) return 30;
		if (FromCentre < 0.32) return 20;
		return 10;
	}

	FString CrawlerName(const ADCCCharacterBase* Who)
	{
		const APlayerState* State = Who ? Who->GetPlayerState() : nullptr;
		return State ? State->GetPlayerName() : FString();
	}

	void Tell(ADCCCharacterBase* Who, const FString& Line)
	{
		if (Who && Who->Wallet)
		{
			Who->Wallet->Tell(Line);
		}
	}
}

int32 DCCCasinoHandValue(uint8 A, uint8 B, uint8 C, uint8 D, uint8 E)
{
	const uint8 Cards[] = { A, B, C, D, E };
	int32 Total = 0;
	int32 Aces = 0;
	for (uint8 Card : Cards)
	{
		if (Card == 0)
		{
			continue;
		}
		if (Card == 1)
		{
			++Aces;
			Total += 11;
		}
		else
		{
			Total += FMath::Min<int32>(Card, 10);
		}
	}
	while (Total > 21 && Aces > 0)
	{
		Total -= 10;
		--Aces;
	}
	return Total;
}

FString DCCCasinoCardName(uint8 Rank)
{
	switch (Rank)
	{
	case 1: return TEXT("A");
	case 11: return TEXT("J");
	case 12: return TEXT("Q");
	case 13: return TEXT("K");
	default: return Rank ? FString::FromInt(Rank) : FString();
	}
}

FString DCCCasinoHandText(uint8 A, uint8 B, uint8 C, uint8 D, uint8 E, bool bHideHole)
{
	const uint8 Cards[] = { A, B, C, D, E };
	FString Out;
	for (int32 i = 0; i < 5; ++i)
	{
		if (Cards[i] == 0)
		{
			break;
		}
		if (!Out.IsEmpty())
		{
			Out += TEXT(" ");
		}
		Out += (bHideHole && i == 1) ? FString(TEXT("?")) : DCCCasinoCardName(Cards[i]);
	}
	return Out;
}

bool DCCCasinoRedPocket(int32 Number)
{
	static const int32 Reds[] = { 1, 3, 5, 7, 9, 12, 14, 16, 18, 19, 21, 23, 25, 27, 30, 32, 34, 36 };
	for (int32 Red : Reds)
	{
		if (Red == Number)
		{
			return true;
		}
	}
	return false;
}

// ---- Layout ---------------------------------------------------------------------------------------------------------

void ADCCCasinoCascade::DenBounds(float& L, float& W)
{
	const int32 Pads = DCCArcade::Games().Num();
	L = FMath::Max(2600.0f, (Pads - 1) * 500.0f + 1100.0f);
	W = 1700.0f;
}

ADCCCasinoCascade::FRoom ADCCCasinoCascade::RoomFor(const FVector& O)
{
	float L = 0.0f;
	float W = 0.0f;
	DenBounds(L, W);
	FRoom R;
	R.HalfX = 2400.0f;
	R.HalfY = W;
	// Overlaps the Safe Room floor by a metre so the doorway has no gap.
	R.Center = O + FVector(L - 100.0f + R.HalfX, 0.0f, 0.0f);
	R.Door = O + FVector(L - 180.0f, 0.0f, 120.0f);

	auto World = [&R](float X, float Y)
	{
		return R.Center + FVector(X, Y, 0.0f);
	};
	auto FaceToward = [](const FVector& From, const FVector& To)
	{
		FRotator Face = (To - From).Rotation();
		Face.Pitch = 0.0f;
		Face.Roll = 0.0f;
		return Face;
	};

	// Games sit on the walls. The middle, from the west door toward the wheel, stays open.
	// Skee-Ball: six lanes along the north wall. The rings are Tables[0]. Throwers face the wall.
	{
		const float LaneX0 = -1680.0f;
		const float LaneStep = 270.0f;
		const float RingY = R.HalfY - 190.0f;
		const float ThrowY = RingY - 760.0f;
		R.Tables[0] = World(LaneX0 + LaneStep * 2.5f, RingY);
		for (int32 i = 0; i < SeatCount; ++i)
		{
			FSpot& Spot = R.Seats[0][i];
			Spot.At = World(LaneX0 + i * LaneStep, ThrowY) + FVector(0.0f, 0.0f, 100.0f);
			Spot.Face = FRotator(0.0f, 90.0f, 0.0f);
		}
		R.Staff[0].At = World(LaneX0 + LaneStep * 5.0f + 240.0f, ThrowY + 60.0f) + FVector(0.0f, 0.0f, 92.0f);
		R.Staff[0].Face = FRotator(0.0f, 180.0f, 0.0f);
	}

	// Air hockey, southwest, clear of the door. 0-2 gold (south), 3-5 blue (north).
	{
		const FVector At = World(-1860.0f, -980.0f);
		R.Tables[1] = At;
		for (int32 i = 0; i < 3; ++i)
		{
			FSpot& South = R.Seats[1][i];
			South.At = At + FVector((i - 1) * 180.0f, -310.0f, 100.0f);
			South.Face = FRotator(0.0f, 90.0f, 0.0f);
			FSpot& North = R.Seats[1][i + 3];
			North.At = At + FVector((i - 1) * 180.0f, 310.0f, 100.0f);
			North.Face = FRotator(0.0f, -90.0f, 0.0f);
		}
		R.Staff[1].At = At + FVector(520.0f, 0.0f, 92.0f);
		R.Staff[1].Face = FRotator(0.0f, 180.0f, 0.0f);
	}

	auto Ring = [&](int32 Table, const FVector& At, float Radius, float CentreYaw, bool bArc)
	{
		R.Tables[Table] = At;
		for (int32 i = 0; i < SeatCount; ++i)
		{
			const float Along = bArc ? (-60.0f + (i + 0.5f) * (120.0f / SeatCount)) : (360.0f * i / SeatCount);
			const FVector Dir = FRotator(0.0f, CentreYaw + Along, 0.0f).Vector();
			FSpot& Spot = R.Seats[Table][i];
			Spot.At = At + Dir * Radius + FVector(0.0f, 0.0f, 100.0f);
			Spot.Face = FaceToward(Spot.At, At);
		}
	};

	// Roulette against the north wall, east of the lanes. The arc opens south, into the room.
	{
		const FVector At = World(980.0f, R.HalfY - 520.0f);
		Ring(2, At, 360.0f, -90.0f, true);
		R.Staff[2].At = At + FVector(0.0f, 280.0f, 92.0f);
		R.Staff[2].Face = FRotator(0.0f, -90.0f, 0.0f);
	}

	// Blackjack against the south wall. The arc opens north. June stands on the wall side.
	{
		const FVector At = World(-520.0f, -R.HalfY + 560.0f);
		Ring(3, At, 380.0f, 90.0f, true);
		R.Staff[3].At = At + FVector(0.0f, -300.0f, 92.0f);
		R.Staff[3].Face = FRotator(0.0f, 90.0f, 0.0f);
	}

	// The wheel on the east wall, north of the title, so the door does not look straight into the back of someone's head.
	{
		const FVector At = World(R.HalfX - 420.0f, 420.0f);
		Ring(4, At, 520.0f, 180.0f, true);
		R.Staff[4].At = At + FVector(180.0f, -640.0f, 92.0f);
		R.Staff[4].Face = FRotator(0.0f, 100.0f, 0.0f);
	}

	// Bar along the south wall, east half. Bud stands at the open west end, in the room.
	R.Staff[5].At = World(400.0f, -R.HalfY + 420.0f) + FVector(0.0f, 0.0f, 92.0f);
	R.Staff[5].Face = FRotator(0.0f, 0.0f, 0.0f);
	return R;
}

// ---- The room -------------------------------------------------------------------------------------------------------

void ADCCCasinoCascade::Dress(ADCCArcadeLobby* Lobby)
{
	if (!Lobby)
	{
		return;
	}
	float L = 0.0f;
	float W = 0.0f;
	DenBounds(L, W);
	const float H = 900.0f;
	const FVector O = Lobby->GetActorLocation();
	const FRoom R = RoomFor(O);
	const FLinearColor Wall(0.08f, 0.06f, 0.12f);
	const TCHAR* WallMat = TEXT("/Game/Synthwave_city/Material/MI_synthwave_wall2");

	auto Carpet = [&](const FVector& Center, float HalfX, float HalfY, bool bCasino)
	{
		constexpr float Tile = 400.0f;
		for (float X = -HalfX + Tile * 0.5f; X < HalfX - 30.0f; X += Tile)
		{
			for (float Y = -HalfY + Tile * 0.5f; Y < HalfY - 30.0f; Y += Tile)
			{
				const int32 IX = FMath::RoundToInt(X / Tile);
				const int32 IY = FMath::RoundToInt(Y / Tile);
				Lobby->Block(Center + FVector(X, Y, 2.0f), FVector(Tile - 10.0f, Tile - 10.0f, 4.0f),
					CarpetColour(IX, IY, bCasino), 0.0f, false);
				if (((IX + IY) & 1) == 0)
				{
					Lobby->Shape(TEXT("Cube"), Center + FVector(X, Y, 5.5f), FVector(72.0f, 72.0f, 2.5f),
						CarpetGold, FRotator(0.0f, 45.0f, 0.0f), false);
				}
			}
		}
	};

	Carpet(O, L, W, false);
	Carpet(R.Center, R.HalfX, R.HalfY, true);

	// East wall of the Safe Room, with a door into the casino. The lintel keeps the ceiling sealed.
	const float DoorHalf = 340.0f;
	const float SideLen = W - DoorHalf;
	const float SideCenter = (W + DoorHalf) * 0.5f;
	if (AActor* South = Lobby->Block(O + FVector(L + 20.0f, -SideCenter, H * 0.5f), FVector(40.0f, SideLen, H), Wall))
	{
		Lobby->Paint(South, WallMat);
	}
	if (AActor* North = Lobby->Block(O + FVector(L + 20.0f, SideCenter, H * 0.5f), FVector(40.0f, SideLen, H), Wall))
	{
		Lobby->Paint(North, WallMat);
	}
	if (AActor* Lintel = Lobby->Block(O + FVector(L + 20.0f, 0.0f, 690.0f), FVector(40.0f, DoorHalf * 2.0f, 420.0f), Wall))
	{
		Lobby->Paint(Lintel, WallMat);
	}
	Lobby->Sign(O + FVector(L - 50.0f, 0.0f, 560.0f), 180.0f, TEXT("CASINO CASCADE"), FColor(255, 196, 80), 64.0f);
	Lobby->Sign(O + FVector(L - 50.0f, 0.0f, 490.0f), 180.0f,
		TEXT("Whoever is at the table plays. The rest of the room can keep voting."), FColor(230, 214, 180), 18.0f);

	// Ceiling over both rooms, so the sky stays outside.
	Lobby->Block(O + FVector(0.0f, 0.0f, 930.0f), FVector(L * 2.0f + 200.0f, W * 2.0f + 200.0f, 50.0f), FLinearColor(0.05f, 0.04f, 0.07f));
	Lobby->Block(R.Center + FVector(0.0f, 0.0f, 930.0f), FVector(R.HalfX * 2.0f + 120.0f, R.HalfY * 2.0f + 120.0f, 50.0f),
		FLinearColor(0.10f, 0.04f, 0.06f));

	const FLinearColor BulbColours[] = {
		FLinearColor(1.0f, 0.15f, 0.2f), FLinearColor(1.0f, 0.75f, 0.1f), FLinearColor(0.2f, 0.45f, 1.0f),
		FLinearColor(0.15f, 0.8f, 0.35f), FLinearColor(0.7f, 0.2f, 0.9f),
	};
	int32 Bulb = 0;
	for (float X = -L + 300.0f; X < L - 200.0f; X += 800.0f)
	{
		for (float Y = -W + 300.0f; Y < W - 200.0f; Y += 800.0f)
		{
			const FLinearColor Colour = BulbColours[Bulb % UE_ARRAY_COUNT(BulbColours)];
			++Bulb;
			if (AActor* Panel = Lobby->Block(O + FVector(X, Y, 890.0f), FVector(340.0f, 340.0f, 8.0f), Colour * 0.55f, 0.0f, false))
			{
				Lobby->Paint(Panel, TEXT("/Game/Synthwave_city/Material/MI_light"));
			}
			Lobby->Light(O + FVector(X, Y, 840.0f), Colour, 3500.0f, 900.0f);
		}
	}
	for (float X = -L + 200.0f; X <= L - 200.0f; X += 280.0f)
	{
		if (AActor* BulbMesh = Lobby->Shape(TEXT("Sphere"), O + FVector(X, 0.0f, 820.0f), FVector(16.0f, 16.0f, 16.0f),
			BulbColours[(FMath::RoundToInt(FMath::Abs(X)) / 280) % UE_ARRAY_COUNT(BulbColours)], FRotator::ZeroRotator, false))
		{
			Lobby->Paint(BulbMesh, TEXT("/Game/Synthwave_city/Material/MI_light"));
		}
	}
	// A few stars over the open middle, above head height, off the vote pads.
	for (int32 i = -2; i <= 2; ++i)
	{
		const FVector At = O + FVector(i * 700.0f, -200.0f, 760.0f);
		Lobby->Shape(TEXT("Cone"), At, FVector(36.0f, 36.0f, 28.0f), BulbColours[(i + 5) % UE_ARRAY_COUNT(BulbColours)],
			FRotator::ZeroRotator, false);
	}

	// Casino shell. No west wall: that side is the door.
	const FVector C = R.Center;
	Lobby->Block(C + FVector(0.0f, 0.0f, -20.0f), FVector(R.HalfX * 2.0f, R.HalfY * 2.0f, 40.0f), Ink);
	auto CasinoWall = [&](const FVector& At, const FVector& Size)
	{
		if (AActor* Face = Lobby->Block(At, Size, FLinearColor(0.12f, 0.04f, 0.06f)))
		{
			Lobby->Paint(Face, TEXT("/Game/Synthwave_city/Material/MI_synthwave_wall1"));
		}
	};
	CasinoWall(C + FVector(0.0f, R.HalfY + 20.0f, H * 0.5f), FVector(R.HalfX * 2.0f + 80.0f, 40.0f, H));
	CasinoWall(C + FVector(0.0f, -(R.HalfY + 20.0f), H * 0.5f), FVector(R.HalfX * 2.0f + 80.0f, 40.0f, H));
	CasinoWall(C + FVector(R.HalfX + 20.0f, 0.0f, H * 0.5f), FVector(40.0f, R.HalfY * 2.0f, H));
	Lobby->Sign(C + FVector(R.HalfX - 50.0f, 0.0f, 760.0f), 180.0f, TEXT("CASINO CASCADE"), FColor(255, 210, 120), 80.0f);
	Lobby->Sign(C + FVector(R.HalfX - 50.0f, 0.0f, 680.0f), 180.0f, TEXT("Games on the walls. The floor stays open."), FColor(230, 210, 180), 22.0f);
	// The walk from the door to the wheel. Games stay off it.
	Lobby->Block(C + FVector(-500.0f, 0.0f, 6.0f), FVector(2600.0f, 280.0f, 2.0f), FLinearColor(0.04f, 0.015f, 0.03f), 0.0f, false);

	for (int32 i = 0; i < 4; ++i)
	{
		Lobby->Light(C + FVector((i - 1.5f) * 900.0f, 0.0f, 760.0f), FLinearColor(1.0f, 0.72f, 0.35f), 6000.0f, 1400.0f);
	}

	// The games themselves are built on the casino actor, so the wheel, the ball and the puck can move.
}

// ---- The floor ------------------------------------------------------------------------------------------------------

ADCCArcadeLobby* ADCCCasinoCascade::FindLobby() const
{
	UWorld* World = GetWorld();
	if (!World)
	{
		return nullptr;
	}
	for (TActorIterator<ADCCArcadeLobby> It(World); It; ++It)
	{
		return *It;
	}
	return nullptr;
}

void ADCCCasinoCascade::BuildFloor()
{
	if (bFloorBuilt)
	{
		return;
	}
	ADCCArcadeLobby* Lobby = FindLobby();
	UWorld* World = GetWorld();
	if (!Lobby || !World)
	{
		return;
	}
	bFloorBuilt = true;
	const FRoom R = Room();

	const TCHAR* BarMesh = TEXT("/Game/Diner/Meshes/Blocking/SM_bar");
	const TCHAR* BarTop = TEXT("/Game/Diner/Meshes/Blocking/SM_bar_top");
	const TCHAR* Stool = TEXT("/Game/Diner/Meshes/Blocking/SM_stool");
	const TCHAR* Table = TEXT("/Game/Diner/Meshes/Blocking/SM_table");
	const TCHAR* Glass = TEXT("/Game/Diner/Meshes/Blocking/SM_glass");
	const TCHAR* Bottle = TEXT("/Game/Cave_Camp/Meshes/SM_bottle");
	const TCHAR* WoodGlass = TEXT("/Game/Cave_Camp/Meshes/SM_wooden_glass");
	const TCHAR* Mug = TEXT("/Game/Legendary_Forge/Meshes/Mug/Sm_mug");
	const TCHAR* Plate = TEXT("/Game/Diner/Meshes/SM_plate");
	const TCHAR* Neon = TEXT("/Game/Cyberpunk_City/Meshes/Neon/SM_neon_01");
	const TCHAR* NeonB = TEXT("/Game/Beach_house/Meshes/Neon/SM_neon");
	const TCHAR* Bench = TEXT("/Game/Cyberpunk_City/Meshes/Bench/SM_bench");
	const TCHAR* Couch = TEXT("/Game/Beach_house/Meshes/Furniture/Couch/SM_small_couch");
	const TCHAR* Plant = TEXT("/Game/Beach_house/Meshes/Plants/SM_plants_01");
	const TCHAR* Ceil = TEXT("/Game/Beach_house/Meshes/Lamp/SM_lamp_ceiling");
	const TCHAR* Rug = TEXT("/Game/Legendary_Forge/Meshes/Caroet/SM_carpet_01");
	const TCHAR* Wide = TEXT("June shows 21. The table pays everyone who stood.     ");

	auto Glow = [&](const FVector& At) -> APointLight*
	{
		FActorSpawnParameters Params;
		Params.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
		APointLight* Lit = World->SpawnActor<APointLight>(At, FRotator::ZeroRotator, Params);
		if (!Lit)
		{
			return nullptr;
		}
		Lit->SetReplicates(false);
		if (UPointLightComponent* Comp = Cast<UPointLightComponent>(Lit->GetLightComponent()))
		{
			Comp->SetMobility(EComponentMobility::Movable);
			Comp->SetIntensity(1600.0f);
			Comp->SetAttenuationRadius(700.0f);
			Comp->SetLightColor(FLinearColor(1.0f, 0.82f, 0.42f));
		}
		return Lit;
	};
	auto Hide = [](AActor* Actor)
	{
		if (Actor)
		{
			Actor->SetActorHiddenInGame(true);
		}
		return Actor;
	};

	for (int32 t = 0; t < TableCount; ++t)
	{
		for (int32 s = 0; s < SeatCount; ++s)
		{
			const FLinearColor MarkColour = (t == 1 && s >= 3) ? FLinearColor(0.15f, 0.35f, 0.9f) : CarpetGold * 0.85f;
			Lobby->Shape(TEXT("Cylinder"), FVector(R.Seats[t][s].At.X, R.Seats[t][s].At.Y, 6.0f),
				FVector(58.0f, 58.0f, 3.0f), MarkColour, FRotator::ZeroRotator, false);
		}
		PitLights[t] = Glow(R.Tables[t] + FVector(0.0f, 0.0f, 420.0f));
	}

	// Skee-Ball lanes, rings toward the north wall.
	for (int32 i = 0; i < SeatCount; ++i)
	{
		const FVector Throw(R.Seats[0][i].At.X, R.Seats[0][i].At.Y, 0.0f);
		const FVector Far(Throw.X, R.Tables[0].Y, 0.0f);
		const FVector Mid = (Throw + Far) * 0.5f + FVector(0.0f, 0.0f, 28.0f);
		const FLinearColor Lane(0.55f, 0.36f, 0.14f);
		Lobby->Shape(TEXT("Cube"), Mid, FVector(150.0f, 700.0f, 16.0f), Wood, FRotator(-8.0f, 0.0f, 0.0f), true);
		Lobby->Shape(TEXT("Cube"), Mid + FVector(0.0f, 0.0f, 12.0f), FVector(118.0f, 660.0f, 5.0f),
			Lane, FRotator(-8.0f, 0.0f, 0.0f), false);
		Lobby->Shape(TEXT("Cube"), Mid + FVector(78.0f, 0.0f, 22.0f), FVector(10.0f, 700.0f, 36.0f), Wood, FRotator(-8.0f, 0.0f, 0.0f), true);
		Lobby->Shape(TEXT("Cube"), Mid + FVector(-78.0f, 0.0f, 22.0f), FVector(10.0f, 700.0f, 36.0f), Wood, FRotator(-8.0f, 0.0f, 0.0f), true);
		const FVector Board = Far + FVector(0.0f, 36.0f, 130.0f);
		Lobby->Block(Board + FVector(0.0f, 18.0f, 0.0f), FVector(168.0f, 16.0f, 230.0f), Ink);
		// Concentric rings on the board. Ring 0 is the 50, the small centre, closest to the throw.
		for (int32 Ring = 4; Ring >= 0; --Ring)
		{
			const float Diameter = 44.0f + Ring * 28.0f;
			SkeeRings[i][Ring] = Lobby->Shape(TEXT("Cylinder"),
				Board + FVector(0.0f, -6.0f - (4 - Ring) * 5.0f, 0.0f),
				FVector(Diameter, Diameter, 8.0f),
				RingColours[Ring], FRotator(90.0f, 90.0f, 0.0f), false);
		}
		Lobby->Shape(TEXT("Sphere"), Throw + FVector(0.0f, 36.0f, 30.0f), FVector(22.0f, 22.0f, 22.0f), CarpetGold, FRotator::ZeroRotator, false);
		SkeeBalls[i] = Hide(Lobby->Shape(TEXT("Sphere"), FVector(0.0f, 0.0f, -400.0f),
			FVector(18.0f, 18.0f, 18.0f), CarpetGold, FRotator::ZeroRotator, false));
	}
	Lobby->Sign(R.Tables[0] + FVector(0.0f, 70.0f, 210.0f), -90.0f, TEXT("50     40     30     20     10"),
		FColor(255, 220, 140), 16.0f, false);
	Marquees[0] = Lobby->Sign(FVector(R.Tables[0].X, R.Center.Y + R.HalfY - 36.0f, 500.0f), -90.0f, Wide,
		FColor(255, 220, 160), 22.0f);
	Lobby->Prop(Neon, FVector(R.Tables[0].X, R.Center.Y + R.HalfY - 28.0f, 580.0f), 0.0f, FVector(1700.0f, 16.0f, 20.0f), false, true);
	Lobby->Prop(NeonB, FVector(R.Tables[2].X, R.Center.Y + R.HalfY - 28.0f, 580.0f), 0.0f, FVector(760.0f, 16.0f, 20.0f), false, true);

	// Air hockey. A diner table is the cabinet. The ice, the rails and the goals sit on it.
	{
		const FVector At = R.Tables[1];
		const FLinearColor Ice(0.62f, 0.86f, 0.95f);
		const FLinearColor Blue(0.12f, 0.32f, 0.92f);
		Lobby->Prop(Rug, At, 0.0f, FVector(340.0f, 620.0f, 0.0f), false);
		Lobby->Block(At + FVector(0.0f, 0.0f, 40.0f), FVector(260.0f, 540.0f, 80.0f), Wood);
		Lobby->Block(At + FVector(0.0f, 0.0f, 86.0f), FVector(210.0f, 460.0f, 6.0f), Ice, 0.0f, false);
		Lobby->Block(At + FVector(0.0f, 0.0f, 90.0f), FVector(210.0f, 4.0f, 2.0f), Blue, 0.0f, false);
		Lobby->Shape(TEXT("Cylinder"), At + FVector(0.0f, 0.0f, 90.0f), FVector(70.0f, 70.0f, 2.0f), Blue, FRotator::ZeroRotator, false);
		Lobby->Block(At + FVector(-112.0f, 0.0f, 100.0f), FVector(8.0f, 470.0f, 16.0f), CarpetGold);
		Lobby->Block(At + FVector(112.0f, 0.0f, 100.0f), FVector(8.0f, 470.0f, 16.0f), Blue);
		Lobby->Block(At + FVector(-48.0f, 228.0f, 110.0f), FVector(8.0f, 8.0f, 56.0f), CarpetGold);
		Lobby->Block(At + FVector(48.0f, 228.0f, 110.0f), FVector(8.0f, 8.0f, 56.0f), CarpetGold);
		Lobby->Block(At + FVector(0.0f, 228.0f, 140.0f), FVector(104.0f, 8.0f, 8.0f), CarpetGold);
		Lobby->Block(At + FVector(-48.0f, -228.0f, 110.0f), FVector(8.0f, 8.0f, 56.0f), Blue);
		Lobby->Block(At + FVector(48.0f, -228.0f, 110.0f), FVector(8.0f, 8.0f, 56.0f), Blue);
		Lobby->Block(At + FVector(0.0f, -228.0f, 140.0f), FVector(104.0f, 8.0f, 8.0f), Blue);
		Puck = Lobby->Shape(TEXT("Sphere"), At + FVector(0.0f, 0.0f, 100.0f), FVector(18.0f, 18.0f, 10.0f), Ink, FRotator::ZeroRotator, false);
		Mallets[0] = Lobby->Shape(TEXT("Cylinder"), At + FVector(0.0f, -150.0f, 100.0f), FVector(32.0f, 32.0f, 10.0f), CarpetGold, FRotator::ZeroRotator, false);
		Mallets[1] = Lobby->Shape(TEXT("Cylinder"), At + FVector(0.0f, 150.0f, 100.0f), FVector(32.0f, 32.0f, 10.0f), Blue, FRotator::ZeroRotator, false);
		Marquees[1] = Lobby->Sign(At + FVector(0.0f, 300.0f, 230.0f), -90.0f, Wide, FColor(210, 230, 255), 20.0f);
	}

	// Three stools on the open side. The diner chair is a booth, and six of them became a wall in front of the felt.
	auto Stools = [&](int32 Index)
	{
		const int32 Use[] = { 1, 2, 4 };
		for (int32 s : Use)
		{
			const FSpot& Spot = R.Seats[Index][s];
			const FVector At = FVector(Spot.At.X, Spot.At.Y, 0.0f) - Spot.Face.Vector() * 30.0f;
			if (!Lobby->Prop(Stool, At, Spot.Face.Yaw, FVector(0.0f, 0.0f, 72.0f), false))
			{
				Lobby->Shape(TEXT("Cylinder"), At + FVector(0.0f, 0.0f, 36.0f), FVector(28.0f, 28.0f, 64.0f), Ink, FRotator::ZeroRotator, false);
			}
		}
	};

	// Roulette. Wood bowl on the back half of the felt, a ring of pockets, and the three bets in front.
	{
		const FVector At = R.Tables[2];
		const FVector Bowl = At + FVector(0.0f, 60.0f, 0.0f);
		const FLinearColor PocketRed(0.82f, 0.08f, 0.09f);
		const FLinearColor PocketBlack(0.07f, 0.06f, 0.07f);
		const FLinearColor PocketGreen(0.05f, 0.62f, 0.22f);
		Lobby->Prop(Rug, At, 30.0f, FVector(460.0f, 360.0f, 0.0f), false);
		if (!Lobby->Prop(Table, At, 0.0f, FVector(340.0f, 210.0f, 74.0f), true))
		{
			Lobby->Block(At + FVector(0.0f, 0.0f, 36.0f), FVector(340.0f, 210.0f, 72.0f), Wood);
		}
		Lobby->Block(At + FVector(0.0f, -20.0f, 78.0f), FVector(300.0f, 180.0f, 8.0f), Felt, 0.0f, false);
		Lobby->Shape(TEXT("Cylinder"), Bowl + FVector(0.0f, 0.0f, 96.0f), FVector(210.0f, 210.0f, 18.0f), Wood, FRotator::ZeroRotator, false);
		Lobby->Shape(TEXT("Cylinder"), Bowl + FVector(0.0f, 0.0f, 108.0f), FVector(196.0f, 196.0f, 6.0f), CarpetGold, FRotator::ZeroRotator, false);
		RouletteDisc = Lobby->Shape(TEXT("Cylinder"), Bowl + FVector(0.0f, 0.0f, 114.0f), FVector(176.0f, 176.0f, 8.0f), Felt, FRotator::ZeroRotator, false);
		Lobby->Shape(TEXT("Cylinder"), Bowl + FVector(0.0f, 0.0f, 122.0f), FVector(40.0f, 40.0f, 12.0f), CarpetGold, FRotator::ZeroRotator, false);
		Lobby->Shape(TEXT("Cone"), Bowl + FVector(0.0f, -112.0f, 128.0f), FVector(22.0f, 22.0f, 36.0f), CarpetGold, FRotator(0.0f, 0.0f, 90.0f), false);
		for (int32 i = 0; i < 12; ++i)
		{
			const FLinearColor Colour = (i == 0) ? PocketGreen : ((i % 2) ? PocketRed : PocketBlack);
			const float A = i * 30.0f;
			RoulettePockets[i] = Lobby->Shape(TEXT("Cube"), Bowl + FVector(0.0f, 0.0f, 132.0f),
				FVector(40.0f, 30.0f, 22.0f), Colour, TableSlice(A), false);
			RouletteFrets[i] = Lobby->Shape(TEXT("Cube"), Bowl + FVector(0.0f, 0.0f, 133.0f),
				FVector(46.0f, 5.0f, 16.0f), CarpetGold, TableSlice(A + 15.0f), false);
		}
		RouletteZero = Lobby->Sign(Bowl + FVector(0.0f, 0.0f, 156.0f), -90.0f, TEXT("0"), FColor::White, 28.0f, false);
		RouletteBallActor = Lobby->Shape(TEXT("Sphere"), Bowl + FVector(90.0f, 0.0f, 136.0f), FVector(16.0f, 16.0f, 16.0f), FLinearColor::White, FRotator::ZeroRotator, false);
		const TCHAR* Bets[] = { TEXT("RED"), TEXT("BLACK"), TEXT("0") };
		const FLinearColor BetColours[] = { PocketRed, PocketBlack, PocketGreen };
		for (int32 i = 0; i < 3; ++i)
		{
			const FVector Spot = At + FVector(-90.0f + i * 90.0f, -88.0f, 88.0f);
			Lobby->Block(Spot, FVector(72.0f, 36.0f, 6.0f), BetColours[i], 0.0f, false);
			Lobby->Sign(Spot + FVector(0.0f, -32.0f, 22.0f), -90.0f, Bets[i], FColor::White, 26.0f, false);
		}
		Stools(2);
		Marquees[2] = Lobby->Sign(FVector(At.X, R.Center.Y + R.HalfY - 36.0f, 440.0f), -90.0f, Wide, FColor(255, 210, 160), 22.0f);
	}

	// Blackjack. Same real table, cards that come out when June deals.
	{
		const FVector At = R.Tables[3];
		Lobby->Prop(Rug, At, 10.0f, FVector(500.0f, 380.0f, 0.0f), false);
		if (!Lobby->Prop(Table, At, 90.0f, FVector(360.0f, 220.0f, 74.0f), true))
		{
			Lobby->Block(At + FVector(0.0f, 0.0f, 36.0f), FVector(320.0f, 200.0f, 72.0f), Wood);
		}
		Lobby->Block(At + FVector(0.0f, 0.0f, 80.0f), FVector(280.0f, 160.0f, 8.0f), Felt, 0.0f, false);
		for (int32 Row = 0; Row < 7; ++Row)
		{
			for (int32 Card = 0; Card < 5; ++Card)
			{
				Cards[Row][Card] = Hide(Lobby->Shape(TEXT("Cube"), FVector(0.0f, 0.0f, -400.0f),
					FVector(22.0f, 32.0f, 2.0f), FLinearColor(0.93f, 0.9f, 0.84f), FRotator::ZeroRotator, false));
			}
		}
		Stools(3);
		Marquees[3] = Lobby->Sign(FVector(At.X, R.Center.Y - R.HalfY + 36.0f, 420.0f), 90.0f, Wide, FColor(255, 220, 170), 22.0f);
	}

	// The wheel. Six slices, narrow at the hub, and a still key beside it so the pays can be read from the aisle.
	{
		const FVector Hub = R.Tables[4] + FVector(0.0f, 0.0f, 470.0f);
		const float BandR[] = { 64.0f, 118.0f, 174.0f };
		const float BandLen[] = { 56.0f, 60.0f, 64.0f };
		const float BandWide[] = { 32.0f, 80.0f, 128.0f };
		Lobby->Shape(TEXT("Cylinder"), FVector(R.Tables[4].X, R.Tables[4].Y, 230.0f), FVector(48.0f, 48.0f, 460.0f), Wood, FRotator::ZeroRotator, true);
		Lobby->Shape(TEXT("Cylinder"), Hub + FVector(16.0f, 0.0f, 0.0f), FVector(520.0f, 520.0f, 16.0f), CarpetGold, FRotator(90.0f, 0.0f, 0.0f), false);
		PrizeDisc = Lobby->Shape(TEXT("Cylinder"), Hub, FVector(450.0f, 450.0f, 18.0f), Ink, FRotator(90.0f, 0.0f, 0.0f), false);
		Lobby->Shape(TEXT("Cylinder"), Hub + FVector(-28.0f, 0.0f, 0.0f), FVector(70.0f, 70.0f, 14.0f), CarpetGold, FRotator(90.0f, 0.0f, 0.0f), false);
		for (int32 i = 0; i < 6; ++i)
		{
			const float A = i * 60.0f;
			for (int32 Band = 0; Band < 3; ++Band)
			{
				PrizeParts[i][Band] = Lobby->Shape(TEXT("Cube"), Hub + FVector(-22.0f, 0.0f, 0.0f) + WallRadial(A, BandR[Band]),
					FVector(BandLen[Band], BandWide[Band], 16.0f), PitColours[i], WallSlice(A), false);
			}
			const FColor InkOn = (i == 1 || i == 3) ? FColor(24, 18, 16) : FColor::White;
			WheelLabels[i] = Lobby->Sign(Hub + FVector(-46.0f, 0.0f, 0.0f) + WallRadial(A, 168.0f), 180.0f, WheelWord[i], InkOn, 44.0f, false);
			const FVector Key = Hub + FVector(-30.0f, -340.0f, 150.0f - i * 64.0f);
			Lobby->Block(Key, FVector(12.0f, 168.0f, 54.0f), PitColours[i], 0.0f, false);
			Lobby->Sign(Key + FVector(-14.0f, 0.0f, 0.0f), 180.0f, WheelWord[i], InkOn, 32.0f, false);
		}
		Lobby->Shape(TEXT("Cone"), Hub + FVector(-40.0f, 0.0f, 248.0f), FVector(44.0f, 44.0f, 78.0f), CarpetGold, FRotator(180.0f, 0.0f, 0.0f), false);
		Marquees[4] = Lobby->Sign(FVector(R.Center.X + R.HalfX - 36.0f, R.Tables[4].Y, 790.0f), 180.0f, Wide, FColor(255, 214, 120), 26.0f);
		Lobby->Prop(Neon, FVector(R.Center.X + R.HalfX - 24.0f, R.Tables[4].Y, 850.0f), 90.0f, FVector(520.0f, 16.0f, 16.0f), false, true);
	}

	// Bar. The diner counter, stools, and the bottles that are already in the packs.
	{
		const FVector Bar = R.Center + FVector(1280.0f, -R.HalfY + 250.0f, 0.0f);
		if (!Lobby->Prop(BarMesh, Bar, 0.0f, FVector(1480.0f, 88.0f, 108.0f), true, true))
		{
			Lobby->Block(Bar + FVector(0.0f, 0.0f, 54.0f), FVector(1480.0f, 88.0f, 108.0f), Wood);
		}
		Lobby->Prop(BarTop, Bar + FVector(0.0f, 16.0f, 112.0f), 0.0f, FVector(1540.0f, 108.0f, 12.0f), false, true);
		for (int32 i = 0; i < 6; ++i)
		{
			const FVector At = Bar + FVector(-600.0f + i * 230.0f, 130.0f, 0.0f);
			if (!Lobby->Prop(Stool, At, -90.0f, FVector(0.0f, 0.0f, 78.0f), true))
			{
				Lobby->Shape(TEXT("Cylinder"), At + FVector(0.0f, 0.0f, 40.0f), FVector(32.0f, 32.0f, 74.0f), Ink, FRotator::ZeroRotator, false);
			}
		}
		const TCHAR* Bits[] = { Bottle, Glass, Mug, Plate, WoodGlass, Bottle, Glass, Mug };
		for (int32 i = 0; i < UE_ARRAY_COUNT(Bits); ++i)
		{
			Lobby->Prop(Bits[i], Bar + FVector(-460.0f + i * 110.0f, -8.0f, 124.0f), i * 28.0f, FVector(0.0f, 0.0f, (i % 2) ? 24.0f : 18.0f), false);
		}
		Lobby->Sign(FVector(Bar.X, R.Center.Y - R.HalfY + 36.0f, 300.0f), 90.0f, TEXT("BAR AND BUD"), FColor(120, 220, 140), 36.0f);
		Lobby->Sign(FVector(Bar.X, R.Center.Y - R.HalfY + 36.0f, 250.0f), 90.0f, TEXT("Cola, a red, and something legal"), FColor(210, 220, 200), 16.0f);
		Lobby->Light(Bar + FVector(0.0f, 40.0f, 220.0f), FLinearColor(0.4f, 1.0f, 0.5f), 2200.0f, 640.0f);
		Lobby->Prop(NeonB, FVector(Bar.X, R.Center.Y - R.HalfY + 30.0f, 360.0f), 0.0f, FVector(1300.0f, 16.0f, 18.0f), false, true);
	}

	// Watching spots, off the aisle. A bench by the door, a couch aimed at the wheel, plants in the corners.
	Lobby->Prop(Bench, R.Center + FVector(-R.HalfX + 260.0f, 680.0f, 0.0f), 90.0f, FVector(170.0f, 52.0f, 78.0f), true);
	Lobby->Prop(Couch, R.Center + FVector(-180.0f, -520.0f, 0.0f), 0.0f, FVector(190.0f, 78.0f, 70.0f), false);
	Lobby->Prop(Plant, R.Center + FVector(-R.HalfX + 160.0f, R.HalfY - 150.0f, 0.0f), 20.0f, FVector(0.0f, 0.0f, 130.0f), true);
	Lobby->Prop(Plant, R.Center + FVector(R.HalfX - 180.0f, -R.HalfY + 180.0f, 0.0f), -30.0f, FVector(0.0f, 0.0f, 130.0f), true);
	for (int32 i = 0; i < 3; ++i)
	{
		Lobby->Prop(Ceil, R.Center + FVector(-700.0f + i * 700.0f, 0.0f, 860.0f), 0.0f, FVector(0.0f, 0.0f, 46.0f), false);
	}
	for (int32 i = 0; i < 10; ++i)
	{
		Chase[i] = Lobby->Shape(TEXT("Sphere"), R.Center + FVector(-1500.0f + i * 300.0f, 0.0f, 800.0f),
			FVector(14.0f, 14.0f, 14.0f), PitColours[i % 5], FRotator::ZeroRotator, false);
	}
}

void ADCCCasinoCascade::AnimateFloor(float DeltaSeconds)
{
	if (!bFloorBuilt)
	{
		return;
	}
	const FRoom R = Room();
	const bool bEdges = bVisualCaught;
	const double Now = EstimatedNow();
	auto Advance = [&](FSpin& Spin, float& Angle, float Idle)
	{
		if (Spin.bLive)
		{
			Spin.Age += DeltaSeconds;
			const float U = FMath::Clamp(Spin.Age / 4.2f, 0.0f, 1.0f);
			const float Eased = 1.0f - FMath::Pow(1.0f - U, 3.0f);
			Angle = FMath::Lerp(Spin.From, Spin.To, Eased);
			if (U >= 1.0f)
			{
				Spin.bLive = false;
				Angle = FMath::Fmod(Spin.To, 360.0f);
				if (Angle < 0.0f)
				{
					Angle += 360.0f;
				}
			}
		}
		else
		{
			Angle += DeltaSeconds * Idle;
		}
	};
	auto Arm = [&](FSpin& Spin, float& Angle, float Target)
	{
		Spin.bLive = true;
		Spin.Age = 0.0f;
		Spin.From = Angle;
		Spin.To = Target;
		while (Spin.To < Spin.From + 520.0f)
		{
			Spin.To += 360.0f;
		}
	};

	if (bEdges && Tables.IsValidIndex(4) && Tables[4].Phase == 3 && SeenPhase[4] != 3 && Tables[4].Result >= 0)
	{
		Arm(WheelSpin, WheelAngle, -60.0f * Tables[4].Result);
	}
	if (bEdges && Tables.IsValidIndex(2) && Tables[2].Phase == 3 && SeenPhase[2] != 3 && Tables[2].Result >= 0)
	{
		Arm(RouletteSpinAnim, RouletteAngle, (Tables[2].Result / 37.0f) * 360.0f);
	}
	if (!bVisualCaught)
	{
		if (Tables.IsValidIndex(4) && Tables[4].Result >= 0)
		{
			WheelAngle = -60.0f * Tables[4].Result;
		}
		if (Tables.IsValidIndex(2) && Tables[2].Result >= 0)
		{
			RouletteAngle = (Tables[2].Result / 37.0f) * 360.0f;
		}
	}
	Advance(WheelSpin, WheelAngle, 10.0f);
	Advance(RouletteSpinAnim, RouletteAngle, 8.0f);

	if (PrizeDisc)
	{
		// Spin in the plane of the wall. A roll on the pitched disc turns it edge-on.
		PrizeDisc->SetActorRotation(WallSlice(WheelAngle));
	}
	const FVector Hub = R.Tables[4] + FVector(0.0f, 0.0f, 470.0f);
	const float BandR[] = { 64.0f, 118.0f, 174.0f };
	for (int32 i = 0; i < 6; ++i)
	{
		const float A = WheelAngle + i * 60.0f;
		for (int32 Band = 0; Band < 3; ++Band)
		{
			if (PrizeParts[i][Band])
			{
				PrizeParts[i][Band]->SetActorLocation(Hub + FVector(-22.0f, 0.0f, 0.0f) + WallRadial(A, BandR[Band]));
				PrizeParts[i][Band]->SetActorRotation(WallSlice(A));
			}
		}
		if (WheelLabels[i])
		{
			// Stay upright so 10 and Bust can be read while the slice turns.
			WheelLabels[i]->SetActorLocation(Hub + FVector(-46.0f, 0.0f, 0.0f) + WallRadial(A, 168.0f));
			WheelLabels[i]->SetActorRotation(FRotator(0.0f, 180.0f, 0.0f));
		}
	}

	const FVector WheelHub = R.Tables[2] + FVector(0.0f, 60.0f, 132.0f);
	for (int32 i = 0; i < 12; ++i)
	{
		const float Deg = RouletteAngle + i * 30.0f;
		const float A = FMath::DegreesToRadians(Deg);
		const FVector Spoke(FMath::Cos(A), FMath::Sin(A), 0.0f);
		if (RoulettePockets[i])
		{
			RoulettePockets[i]->SetActorLocation(WheelHub + Spoke * 78.0f);
			RoulettePockets[i]->SetActorRotation(TableSlice(Deg));
		}
		if (RouletteFrets[i])
		{
			const float Fret = Deg + 15.0f;
			const float FR = FMath::DegreesToRadians(Fret);
			RouletteFrets[i]->SetActorLocation(WheelHub + FVector(FMath::Cos(FR) * 78.0f, FMath::Sin(FR) * 78.0f, 1.0f));
			RouletteFrets[i]->SetActorRotation(TableSlice(Fret));
		}
		if (i == 0 && RouletteZero)
		{
			RouletteZero->SetActorLocation(WheelHub + Spoke * 78.0f + FVector(0.0f, 0.0f, 18.0f));
			RouletteZero->SetActorRotation(FRotator(0.0f, -90.0f, 0.0f));
		}
	}
	if (RouletteBallActor)
	{
		float Orbit = -RouletteAngle * 1.35f;
		float Radius = 100.0f;
		if (RouletteSpinAnim.bLive)
		{
			const float U = FMath::Clamp(RouletteSpinAnim.Age / 4.2f, 0.0f, 1.0f);
			Radius = FMath::Lerp(104.0f, 78.0f, U * U);
		}
		else if (Tables.IsValidIndex(2) && Tables[2].Result >= 0 && Tables[2].Phase == 3)
		{
			Orbit = (Tables[2].Result / 37.0f) * 360.0f;
			Radius = 78.0f;
		}
		const float A = FMath::DegreesToRadians(Orbit);
		RouletteBallActor->SetActorLocation(WheelHub + FVector(FMath::Cos(A) * Radius, FMath::Sin(A) * Radius, 10.0f));
	}

	// Skee balls. A new BallSerial launches one, up the lane and into the ring it earned.
	for (int32 s = 0; s < SeatCount; ++s)
	{
		const FDCCCasinoSeatNet* Seat = Seats.IsValidIndex(s) ? &Seats[s] : nullptr;
		if (bEdges && Seat && Seat->BallSerial != SeenSerial[s] && Seat->LastBall > 0 && SkeeBalls[s])
		{
			BallT[s] = 0.001f;
			BallPts[s] = Seat->LastBall;
			SkeeBalls[s]->SetActorHiddenInGame(false);
		}
		if (Seat)
		{
			SeenSerial[s] = Seat->BallSerial;
		}
		if (!SkeeBalls[s] || BallT[s] <= 0.0f)
		{
			continue;
		}
		BallT[s] += DeltaSeconds / 1.05f;
		int32 Step = 4;
		if (BallPts[s] >= 50) Step = 0;
		else if (BallPts[s] >= 40) Step = 1;
		else if (BallPts[s] >= 30) Step = 2;
		else if (BallPts[s] >= 20) Step = 3;
		const FVector Start(R.Seats[0][s].At.X, R.Seats[0][s].At.Y, 64.0f);
		const FVector End(Start.X, R.Tables[0].Y + 16.0f, 130.0f);
		const float T = FMath::Clamp(BallT[s], 0.0f, 1.0f);
		FVector At = FMath::Lerp(Start, End, T);
		At.Z += FMath::Sin(T * PI) * 120.0f;
		SkeeBalls[s]->SetActorLocation(At);
		for (int32 Ring = 0; Ring < 5; ++Ring)
		{
			const bool bFlash = Ring == Step && BallT[s] > 1.0f && BallT[s] < 1.45f;
			Tint(SkeeRings[s][Ring], bFlash ? FLinearColor(1.0f, 0.95f, 0.7f) : RingColours[Ring]);
		}
		if (BallT[s] > 1.9f)
		{
			BallT[s] = 0.0f;
			SkeeBalls[s]->SetActorHiddenInGame(true);
		}
	}

	// Air hockey puck, and a mallet that leans in when that side has shot.
	if (Puck)
	{
		FVector At = R.Tables[1] + FVector(0.0f, 0.0f, 100.0f);
		if (bEdges && Tables.IsValidIndex(1))
		{
			if (Tables[1].ScoreA != SeenScoreA)
			{
				PuckHit = 0.42f;
				PuckSide = 1;
			}
			else if (Tables[1].ScoreB != SeenScoreB)
			{
				PuckHit = 0.42f;
				PuckSide = -1;
			}
		}
		if (PuckHit > 0.0f)
		{
			PuckHit = FMath::Max(0.0f, PuckHit - DeltaSeconds);
			const float U = 1.0f - FMath::Clamp(PuckHit / 0.42f, 0.0f, 1.0f);
			At.Y += PuckSide * FMath::Lerp(0.0f, 190.0f, U);
		}
		else if (Tables.IsValidIndex(1) && Tables[1].Phase == 2)
		{
			const double MarkAt = Mark(Now, 0);
			At.Y += FMath::Lerp(-150.0, 150.0, MarkAt);
			At.X += FMath::Sin(Now * 3.2) * 28.0;
		}
		Puck->SetActorLocation(At);
	}
	if (Tables.IsValidIndex(1))
	{
		int32 Done[2] = { 0, 0 };
		for (int32 s = 0; s < SeatCount; ++s)
		{
			if (Seats.IsValidIndex(SeatCount + s) && Seats[SeatCount + s].bDone)
			{
				++Done[s < 3 ? 0 : 1];
			}
		}
		for (int32 Side = 0; Side < 2; ++Side)
		{
			if (Done[Side] < SeenMallet[Side])
			{
				SeenMallet[Side] = uint8(Done[Side]);
			}
			else if (bEdges && Done[Side] > SeenMallet[Side])
			{
				MalletT[Side] = 1.0f;
				SeenMallet[Side] = uint8(Done[Side]);
			}
			MalletT[Side] = FMath::Max(0.0f, MalletT[Side] - DeltaSeconds * 1.6f);
		}
		if (Mallets[0])
		{
			Mallets[0]->SetActorLocation(R.Tables[1] + FVector(0.0f, -150.0f + MalletT[0] * 55.0f, 100.0f));
		}
		if (Mallets[1])
		{
			Mallets[1]->SetActorLocation(R.Tables[1] + FVector(0.0f, 150.0f - MalletT[1] * 55.0f, 100.0f));
		}
	}

	// Cards on the felt. The dealer's hole card stays face down until she shows.
	if (Tables.IsValidIndex(3))
	{
		const FDCCCasinoTableNet& Pit = Tables[3];
		const FVector Table = R.Tables[3];
		auto Lay = [&](int32 Row, const FVector& Origin, const FVector& Right, uint8 Rank0, uint8 Rank1, uint8 Rank2, uint8 Rank3, uint8 Rank4, bool bHideHole)
		{
			const uint8 Ranks[] = { Rank0, Rank1, Rank2, Rank3, Rank4 };
			for (int32 c = 0; c < 5; ++c)
			{
				AActor* Card = Cards[Row][c];
				if (!Card)
				{
					continue;
				}
				if (Ranks[c] == 0 || Pit.Phase < 2)
				{
					Card->SetActorHiddenInGame(true);
					continue;
				}
				Card->SetActorHiddenInGame(false);
				Card->SetActorLocation(Origin + Right * (c * 26.0f) + FVector(0.0f, 0.0f, 88.0f));
				Card->SetActorRotation(FRotator(0.0f, Right.Rotation().Yaw + 90.0f, 0.0f));
				const bool bDown = bHideHole && c == 1;
				const FLinearColor Colour = bDown ? FLinearColor(0.08f, 0.04f, 0.16f)
					: ((Ranks[c] % 2) ? FLinearColor(0.72f, 0.08f, 0.10f) : FLinearColor(0.93f, 0.90f, 0.84f));
				Tint(Card, Colour);
			}
		};
		Lay(6, Table + FVector(-50.0f, -36.0f, 0.0f), FVector(1.0f, 0.0f, 0.0f),
			Pit.Dealer0, Pit.Dealer1, Pit.Dealer2, Pit.Dealer3, Pit.Dealer4, Pit.bShowDealer == 0);
		for (int32 s = 0; s < SeatCount; ++s)
		{
			if (!Seats.IsValidIndex(3 * SeatCount + s))
			{
				continue;
			}
			const FDCCCasinoSeatNet& Seat = Seats[3 * SeatCount + s];
			const FVector To = (Table - FVector(R.Seats[3][s].At.X, R.Seats[3][s].At.Y, 0.0f)).GetSafeNormal2D();
			const FVector Right(-To.Y, To.X, 0.0f);
			const FVector Origin = FMath::Lerp(FVector(R.Seats[3][s].At.X, R.Seats[3][s].At.Y, 0.0f), Table, 0.40f);
			Lay(s, Origin, Right, Seat.Card0, Seat.Card1, Seat.Card2, Seat.Card3, Seat.Card4, false);
		}
	}

	bool bCelebration = WheelSpin.bLive || RouletteSpinAnim.bLive;
	for (int32 t = 0; t < TableCount; ++t)
	{
		if (!Tables.IsValidIndex(t))
		{
			continue;
		}
		const FDCCCasinoTableNet& Pit = Tables[t];
		if (Pit.Phase == 3)
		{
			bCelebration = true;
		}
		FString Line;
		FColor Colour(255, 220, 170);
		if (t == 4 && WheelSpin.bLive && WheelSpin.Age < 3.1f)
		{
			Line = TEXT("The wheel is turning.");
			Colour = FColor(255, 196, 80);
		}
		else if (t == 2 && RouletteSpinAnim.bLive && RouletteSpinAnim.Age < 3.1f)
		{
			Line = TEXT("The ball is in the wheel.");
			Colour = FColor(255, 196, 80);
		}
		else if (Pit.Phase >= 2 && !Pit.Line.IsEmpty())
		{
			Line = Pit.Line;
			Colour = Pit.Phase == 3 ? FColor(255, 230, 140) : FColor(230, 214, 190);
		}
		else if (t == 0)
		{
			Line = TEXT("Three balls. The rings pay 10 to 50.");
		}
		else if (t == 1)
		{
			Line = Pit.Phase == 0 ? FString(TEXT("First to 3. Gold south, blue north.")) : FString::Printf(TEXT("Gold %d, blue %d."), Pit.ScoreA, Pit.ScoreB);
		}
		else if (t == 2)
		{
			Line = TEXT("Red, black, or green. Five tickets.");
		}
		else if (t == 3)
		{
			Line = TEXT("June deals. Five tickets, or the house comps.");
		}
		else
		{
			Line = TEXT("Pick a wedge. Bust pays nothing.");
		}
		Say(Marquees[t], Line, Colour);
		if (UPointLightComponent* Lit = PitLights[t] ? Cast<UPointLightComponent>(PitLights[t]->GetLightComponent()) : nullptr)
		{
			const bool bHot = Pit.Phase == 3 || (t == 4 && WheelSpin.bLive) || (t == 2 && RouletteSpinAnim.bLive);
			const float Pulse = bHot ? (1.5f + 0.7f * FMath::Sin(float(Now) * 9.0f)) : 0.65f;
			Lit->SetIntensity(1500.0f * Pulse);
			Lit->SetLightColor(bHot ? FLinearColor(1.0f, 0.72f, 0.25f) : FLinearColor(1.0f, 0.84f, 0.5f));
		}
		SeenPhase[t] = Pit.Phase;
		SeenResult[t] = Pit.Result;
	}
	if (Tables.IsValidIndex(1))
	{
		SeenScoreA = Tables[1].ScoreA;
		SeenScoreB = Tables[1].ScoreB;
	}

	for (int32 i = 0; i < 10; ++i)
	{
		const float Wave = bCelebration ? (0.55f + 0.45f * FMath::Sin(float(Now) * 7.0f - i * 0.7f)) : 0.22f;
		Tint(Chase[i], PitColours[i % 5] * (0.35f + Wave));
	}

	if (!bVisualCaught)
	{
		for (int32 s = 0; s < SeatCount && Seats.IsValidIndex(s); ++s)
		{
			SeenSerial[s] = Seats[s].BallSerial;
		}
		bVisualCaught = true;
	}
}

// ---- Staff ----------------------------------------------------------------------------------------------------------

ADCCCasinoStaff::ADCCCasinoStaff()
{
	DialogueTable = nullptr;
	bLoopWhenExhausted = false;
	TalkRadius = 420.0f;
	SpeechColour = FLinearColor(1.0f, 0.86f, 0.55f);

	static ConstructorHelpers::FObjectFinder<UAnimationAsset> Idle(TEXT("/Game/Characters/Mannequins/Anims/Unarmed/MM_Idle.MM_Idle"));
	IdleAnimation = Idle.Object;
	static ConstructorHelpers::FObjectFinder<USkeletalMesh> Manny(TEXT("/Game/Characters/Mannequins/Meshes/SKM_Manny_Simple.SKM_Manny_Simple"));
	if (Mesh && Manny.Succeeded())
	{
		Mesh->SetSkeletalMeshAsset(Manny.Object);
	}
}

void ADCCCasinoStaff::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);
	DOREPLIFETIME(ADCCCasinoStaff, Post);
}

void ADCCCasinoStaff::BuildScript()
{
	Script.Reset();
	NextLineIndex = 0;
}

void ADCCCasinoStaff::BeginPlay()
{
	SpeakerId = TEXT("CasinoStaff");
	bLoopWhenExhausted = false;
	Super::BeginPlay();
	ApplyLook();
}

void ADCCCasinoStaff::OnRep_Role()
{
	ApplyLook();
}

void ADCCCasinoStaff::ApplyLook()
{
	if (!Mesh || (bDressed && AppliedPost == Post))
	{
		return;
	}
	for (UStaticMeshComponent* Part : Worn)
	{
		if (Part)
		{
			Part->DestroyComponent();
		}
	}
	Worn.Reset();
	static const TCHAR* Names[] = {
		TEXT("Lane Okada"), TEXT("Puck Ruiz"), TEXT("Vale Mercer"), TEXT("June Pell"), TEXT("Otto Voss"), TEXT("Bud Alvarez"),
	};
	static const TCHAR* Jobs[] = {
		TEXT("Skee-Ball"), TEXT("Air Hockey"), TEXT("Roulette"), TEXT("Blackjack"), TEXT("The Wheel"), TEXT("Bar"),
	};
	const int32 Index = FMath::Clamp<int32>(Post, 0, 5);
	DisplayName = FText::FromString(FString::Printf(TEXT("%s - %s"), Names[Index], Jobs[Index]));
	const bool bQuinn = Index == 2 || Index == 3 || Index == 5;
	if (bQuinn)
	{
		if (USkeletalMesh* Quinn = LoadObject<USkeletalMesh>(nullptr, TEXT("/Game/Characters/Mannequins/Meshes/SKM_Quinn_Simple.SKM_Quinn_Simple")))
		{
			Mesh->SetSkeletalMeshAsset(Quinn);
		}
	}
	if (GetWorld() && GetWorld()->GetNetMode() == NM_DedicatedServer)
	{
		AppliedPost = Post;
		bDressed = true;
		ShowIdleText();
		return;
	}

	const FLinearColor Vests[] = {
		FLinearColor(0.75f, 0.15f, 0.18f), FLinearColor(0.1f, 0.25f, 0.55f), FLinearColor(0.12f, 0.12f, 0.14f),
		FLinearColor(0.1f, 0.1f, 0.12f), FLinearColor(0.35f, 0.08f, 0.1f), FLinearColor(0.08f, 0.22f, 0.12f),
	};
	const FLinearColor Trims[] = {
		FLinearColor(1.0f, 0.8f, 0.2f), FLinearColor(0.7f, 0.85f, 1.0f), CarpetGold, CarpetGold, CarpetGold, FLinearColor(0.3f, 0.8f, 0.35f),
	};
	UStaticMesh* Cube = LoadObject<UStaticMesh>(nullptr, TEXT("/Engine/BasicShapes/Cube.Cube"));
	UStaticMesh* Sphere = LoadObject<UStaticMesh>(nullptr, TEXT("/Engine/BasicShapes/Sphere.Sphere"));
	UMaterialInterface* Base = LoadObject<UMaterialInterface>(nullptr, TEXT("/Engine/BasicShapes/BasicShapeMaterial.BasicShapeMaterial"));
	auto Wear = [&](UStaticMesh* MeshAsset, const FVector& Rel, const FVector& Size, const FLinearColor& Colour)
	{
		if (!MeshAsset || !Collision)
		{
			return;
		}
		UStaticMeshComponent* Part = NewObject<UStaticMeshComponent>(this);
		Part->SetStaticMesh(MeshAsset);
		Part->SetupAttachment(Collision.Get());
		Part->SetCollisionEnabled(ECollisionEnabled::NoCollision);
		Part->SetRelativeLocation(Rel);
		Part->SetRelativeScale3D(Size / 100.0f);
		Part->SetCastShadow(false);
		if (Base)
		{
			UMaterialInstanceDynamic* Mid = UMaterialInstanceDynamic::Create(Base, Part);
			Mid->SetVectorParameterValue(TEXT("Color"), Colour);
			Mid->SetVectorParameterValue(TEXT("BaseColor"), Colour);
			Part->SetMaterial(0, Mid);
		}
		Part->RegisterComponent();
		Worn.Add(Part);
	};
	Wear(Cube, FVector(8.0f, 0.0f, 8.0f), FVector(22.0f, 36.0f, 42.0f), Vests[Index]);
	Wear(Cube, FVector(16.0f, 0.0f, 22.0f), FVector(6.0f, 16.0f, 18.0f), Trims[Index]);
	Wear(Sphere, FVector(14.0f, 0.0f, 42.0f), FVector(8.0f, 14.0f, 8.0f), Trims[Index]);
	if (Index == 5)
	{
		Wear(Sphere, FVector(12.0f, 14.0f, 18.0f), FVector(8.0f, 8.0f, 8.0f), FLinearColor(0.15f, 0.55f, 0.2f));
	}
	else if (Index == 4)
	{
		Wear(Sphere, FVector(0.0f, 0.0f, 78.0f), FVector(22.0f, 22.0f, 10.0f), CarpetGold);
	}
	AppliedPost = Post;
	bDressed = true;
	ShowIdleText();
}

void ADCCCasinoStaff::SayRules()
{
	const TCHAR* Text = TEXT("Ask me again in a second.");
	switch (Post)
	{
	case 0:
		Text = TEXT("Three balls. Roll when the mark sits in the gold for a fifty. Tickets are the score, split by ten. Your friends do not have to be here.");
		break;
	case 1:
		Text = TEXT("Gold on the near side, blue on the far side. First to three. Shoot when the bar is on your colour. One player takes the house.");
		break;
	case 2:
		Text = TEXT("Red or black pays even. Green pays fourteen times. Five tickets, or the house comps you if you are broke. Spin when this table is ready.");
		break;
	case 3:
		Text = TEXT("You and whoever sits. Hit or stand. I stand on seventeen. Two cards that make twenty-one pay a little more. Empty chairs stay empty.");
		break;
	case 4:
		Text = TEXT("Pick a wedge. If the wheel lands on it, it pays that many times your five. Bust pays nothing. The Safe Room does not have to sit down.");
		break;
	default:
		Text = TEXT("Cola is 2 tickets. The red is 4. The legal bud is 6. I pour. The tables start themselves.");
		break;
	}
	FDCCDialogueLine Line;
	Line.Text = FText::FromString(Text);
	Line.DisplaySeconds = 8.0f;
	ShowLine(Line);
}

// ---- The pit --------------------------------------------------------------------------------------------------------

ADCCCasinoCascade::ADCCCasinoCascade()
{
	PrimaryActorTick.bCanEverTick = true;
	bReplicates = true;
	bAlwaysRelevant = true;
	SetReplicatingMovement(false);
	SetReplicateMovement(false);
	SetNetUpdateFrequency(15.0f);
	SetRootComponent(CreateDefaultSubobject<USceneComponent>(TEXT("Root")));
}

void ADCCCasinoCascade::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);
	DOREPLIFETIME(ADCCCasinoCascade, Tables);
	DOREPLIFETIME(ADCCCasinoCascade, Seats);
	DOREPLIFETIME(ADCCCasinoCascade, ServerClock);
}

void ADCCCasinoCascade::OnRep_Clock()
{
	if (const UWorld* World = GetWorld())
	{
		ClockOffset = ServerClock - World->GetTimeSeconds();
	}
}

double ADCCCasinoCascade::EstimatedNow() const
{
	const UWorld* World = GetWorld();
	if (!World)
	{
		return 0.0;
	}
	return HasAuthority() ? World->GetTimeSeconds() : World->GetTimeSeconds() + ClockOffset;
}

double ADCCCasinoCascade::Mark(double Now, int32 Salt)
{
	const double Wrapped = FMath::Fmod(Now * 0.55 + double(Salt) * 0.17, 1.0);
	return Wrapped < 0.0 ? Wrapped + 1.0 : Wrapped;
}

ADCCCasinoCascade* ADCCCasinoCascade::Get(UWorld* World)
{
	if (!World)
	{
		return nullptr;
	}
	for (TActorIterator<ADCCCasinoCascade> It(World); It; ++It)
	{
		return *It;
	}
	return nullptr;
}

void ADCCCasinoCascade::DrinkInfo(int32 Index, FString& Label, int32& Price, FString& Spoken)
{
	switch (Index)
	{
	case 0:
		Label = TEXT("Cascade Cola");
		Price = 2;
		Spoken = TEXT("Cold. The ice is a suggestion.");
		break;
	case 1:
		Label = TEXT("House Red");
		Price = 4;
		Spoken = TEXT("It is red. That is the review.");
		break;
	default:
		Label = TEXT("Legal Bud");
		Price = 6;
		Spoken = TEXT("On the menu. The label has been read.");
		break;
	}
}

void ADCCCasinoCascade::EnsureArrays()
{
	if (Tables.Num() == TableCount && Seats.Num() == TableCount * SeatCount)
	{
		return;
	}
	Tables.SetNum(TableCount);
	Seats.SetNum(TableCount * SeatCount);
}

void ADCCCasinoCascade::BeginPlay()
{
	Super::BeginPlay();
	BuildFloor();
	if (HasAuthority())
	{
		EnsureArrays();
		SpawnStaff();
	}
}

void ADCCCasinoCascade::SpawnStaff()
{
	UWorld* World = GetWorld();
	if (!World)
	{
		return;
	}
	const FRoom Layout = Room();
	for (int32 i = 0; i < 6; ++i)
	{
		const FTransform Xform(Layout.Staff[i].Face, Layout.Staff[i].At);
		ADCCCasinoStaff* Staff = World->SpawnActorDeferred<ADCCCasinoStaff>(ADCCCasinoStaff::StaticClass(), Xform, this);
		if (!Staff)
		{
			continue;
		}
		Staff->Post = uint8(i);
		Staff->FinishSpawning(Xform);
	}
}

int32 ADCCCasinoCascade::FindSeat(const APlayerState* State, int32& Table, int32& Seat) const
{
	Table = -1;
	Seat = -1;
	if (!State || State->GetPlayerName().IsEmpty())
	{
		return -1;
	}
	const FString& Name = State->GetPlayerName();
	for (int32 i = 0; i < Seats.Num(); ++i)
	{
		if (Seats[i].Name == Name)
		{
			Table = i / SeatCount;
			Seat = i % SeatCount;
			return i;
		}
	}
	return -1;
}

bool ADCCCasinoCascade::IsSeated(const ADCCCharacterBase* Who) const
{
	int32 Table = -1;
	int32 Seat = -1;
	return Who && FindSeat(Who->GetPlayerState(), Table, Seat) >= 0;
}

bool ADCCCasinoCascade::IsSeatedState(const APlayerState* State) const
{
	int32 Table = -1;
	int32 Seat = -1;
	return FindSeat(State, Table, Seat) >= 0;
}

bool ADCCCasinoCascade::Near(const FRoom& Layout, int32 Table, const FVector& At) const
{
	if (!Tables.IsValidIndex(Table))
	{
		return false;
	}
	if (FVector::Dist2D(At, Layout.Tables[Table]) < 1100.0f)
	{
		return true;
	}
	if (Layout.Staff[Table].At.Z != 0.0f && FVector::Dist2D(At, Layout.Staff[Table].At) < 550.0f)
	{
		return true;
	}
	return false;
}

bool ADCCCasinoCascade::Contains(const FVector& At) const
{
	const FRoom Layout = Room();
	const FVector D = At - Layout.Center;
	// The west side includes the doorway. Standing in the opening is already in the pit.
	return D.X >= -(Layout.HalfX + 280.0f) && D.X <= Layout.HalfX
		&& FMath::Abs(D.Y) <= Layout.HalfY
		&& D.Z > -200.0f && D.Z < 1200.0f;
}

int32 ADCCCasinoCascade::ClosestTable(const FVector& At) const
{
	// 420 reaches a stool or a throw line. The aisle (the door, straight east) stays outside it.
	const FRoom Layout = Room();
	int32 Best = -1;
	float BestDist = 420.0f;
	for (int32 t = 0; t < TableCount; ++t)
	{
		float Dist = FVector::Dist2D(At, Layout.Tables[t]);
		for (int32 s = 0; s < SeatCount; ++s)
		{
			Dist = FMath::Min(Dist, FVector::Dist2D(At, Layout.Seats[t][s].At));
		}
		if (Dist < BestDist)
		{
			BestDist = Dist;
			Best = t;
		}
	}
	return Best;
}

int32 ADCCCasinoCascade::Occupied(int32 Table) const
{
	int32 Count = 0;
	for (int32 s = 0; s < SeatCount; ++s)
	{
		const int32 Index = Table * SeatCount + s;
		if (Seats.IsValidIndex(Index) && !Seats[Index].Name.IsEmpty())
		{
			++Count;
		}
	}
	return Count;
}

void ADCCCasinoCascade::Release(ADCCCharacterBase* Who)
{
	if (!Who)
	{
		return;
	}
	Held.Remove(Who);
	if (UCharacterMovementComponent* Move = Who->GetCharacterMovement())
	{
		if (Move->MovementMode == MOVE_None)
		{
			Move->SetMovementMode(MOVE_Walking);
		}
	}
}

void ADCCCasinoCascade::Sit(ADCCCharacterBase* Who, const FRoom& Layout, int32 Table, int32 Seat)
{
	const FSpot& Spot = Layout.Seats[Table][Seat];
	Who->SetActorLocation(Spot.At, false, nullptr, ETeleportType::TeleportPhysics);
	Who->SetActorRotation(Spot.Face);
	if (UCharacterMovementComponent* Move = Who->GetCharacterMovement())
	{
		Move->StopMovementImmediately();
		Move->DisableMovement();
	}
	Held.Add(Who);
}

int32 ADCCCasinoCascade::TakeStake(ADCCCharacterBase* Who, FDCCCasinoSeatNet& Seat)
{
	Seat.Stake = 0;
	APlayerState* State = Who ? Who->GetPlayerState() : nullptr;
	const FString Key = DCCProfile::KeyFor(State);
	if (Key.IsEmpty())
	{
		return 0;
	}
	if (!DCCProfile::SpendTickets(Key, Stake))
	{
		Tell(Who, TEXT("This one is on the house."));
		return 0;
	}
	if (Who->Wallet)
	{
		Who->Wallet->Tickets = DCCProfile::GetTickets(Key);
	}
	Seat.Stake = Stake;
	Tell(Who, TEXT("-5 tickets."));
	return Stake;
}

void ADCCCasinoCascade::Give(ADCCCharacterBase* Who, int32 Amount, const FString& Why)
{
	if (!Who)
	{
		return;
	}
	if (Amount <= 0)
	{
		Tell(Who, Why);
		return;
	}
	UDCCWalletComponent::AwardTickets(Who->GetPlayerState(), Amount, Why);
	// A push hands the stake back. That is not a win, and it must not fill the pit achievements.
	if (Why == TEXT("push"))
	{
		return;
	}
	UDCCAchievementComponent::RecordFor(Who, EDCCAchievementStat::CasinoTicketsWon, Amount);
	if (!Why.StartsWith(TEXT("skee")))
	{
		UDCCAchievementComponent::RecordFor(Who, EDCCAchievementStat::CasinoWins, 1);
	}
	if (Amount >= 50)
	{
		UDCCAchievementComponent::RecordFor(Who, EDCCAchievementStat::CasinoJackpot, 1);
	}
}

uint8 ADCCCasinoCascade::DrawRank() const
{
	return uint8(FMath::RandRange(1, 13));
}

void ADCCCasinoCascade::SetSeatCard(FDCCCasinoSeatNet& Seat, int32 Index, uint8 Rank)
{
	switch (Index)
	{
	case 0: Seat.Card0 = Rank; break;
	case 1: Seat.Card1 = Rank; break;
	case 2: Seat.Card2 = Rank; break;
	case 3: Seat.Card3 = Rank; break;
	default: Seat.Card4 = Rank; break;
	}
}

void ADCCCasinoCascade::SetDealerCard(FDCCCasinoTableNet& Table, int32 Index, uint8 Rank)
{
	switch (Index)
	{
	case 0: Table.Dealer0 = Rank; break;
	case 1: Table.Dealer1 = Rank; break;
	case 2: Table.Dealer2 = Rank; break;
	case 3: Table.Dealer3 = Rank; break;
	default: Table.Dealer4 = Rank; break;
	}
}

void ADCCCasinoCascade::Join(ADCCCharacterBase* Who, int32 Table)
{
	if (!HasAuthority() || !Who)
	{
		return;
	}
	EnsureArrays();
	const FString Name = CrawlerName(Who);
	if (Name.IsEmpty())
	{
		return;
	}
	if (Table == 5)
	{
		Tell(Who, TEXT("I pour. The tables are the games."));
		return;
	}
	if (!Tables.IsValidIndex(Table))
	{
		return;
	}
	const FRoom Layout = Room();
	if (!Near(Layout, Table, Who->GetActorLocation()))
	{
		Tell(Who, TEXT("Step up to the table."));
		return;
	}
	int32 HaveTable = -1;
	int32 HaveSeat = -1;
	if (FindSeat(Who->GetPlayerState(), HaveTable, HaveSeat) >= 0)
	{
		Tell(Who, HaveTable == Table ? FString(TEXT("You are already in.")) : FString(TEXT("Esc or Tab leaves the other game.")));
		return;
	}
	int32 Open = -1;
	for (int32 s = 0; s < SeatCount; ++s)
	{
		if (Seats[Table * SeatCount + s].Name.IsEmpty())
		{
			Open = s;
			break;
		}
	}
	if (Open < 0)
	{
		Tell(Who, TEXT("Every spot is taken."));
		return;
	}

	FDCCCasinoSeatNet& Seat = Seats[Table * SeatCount + Open];
	Seat = FDCCCasinoSeatNet();
	Seat.Name = Name;
	Sit(Who, Layout, Table, Open);

	FDCCCasinoTableNet& Pit = Tables[Table];
	const double Now = GetWorld()->GetTimeSeconds();
	if (Table == 0)
	{
		Pit.Phase = 2;
		Tell(Who, TEXT("You are on the lane. Roll when you like."));
	}
	else if (Pit.Phase == 0 || Pit.Phase == 3)
	{
		Pit.Phase = 1;
		Pit.EndsAt = Now + 6.0;
		Pit.Line = TEXT("Sitting down. Deal when you are ready, or wait a few seconds.");
		Tell(Who, TEXT("You are in. This table does not wait on the room."));
	}
	else if (Pit.Phase == 1)
	{
		Pit.EndsAt = FMath::Max(Pit.EndsAt, Now + 2.0);
		Tell(Who, TEXT("You are in."));
	}
	else
	{
		Tell(Who, TEXT("Next hand is yours. You can watch this one."));
	}
	MenuIndex.Remove(Who);
	ForceNetUpdate();
	UE_LOG(LogTemp, Log, TEXT("[DCC Casino] %s joined %s seat %d"), *Name, GameTitle(Table), Open);
}

void ADCCCasinoCascade::Leave(ADCCCharacterBase* Who)
{
	if (!HasAuthority() || !Who)
	{
		return;
	}
	int32 Table = -1;
	int32 Seat = -1;
	const int32 Index = FindSeat(Who->GetPlayerState(), Table, Seat);
	if (Index < 0)
	{
		Release(Who);
		return;
	}
	const bool bWasTurn = Tables.IsValidIndex(Table) && Tables[Table].Turn == Seat;
	Seats[Index] = FDCCCasinoSeatNet();
	Release(Who);
	if (Occupied(Table) == 0 && Tables.IsValidIndex(Table))
	{
		Tables[Table] = FDCCCasinoTableNet();
	}
	else if (bWasTurn)
	{
		AdvanceBlackjack(Table);
	}
	ForceNetUpdate();
	UE_LOG(LogTemp, Log, TEXT("[DCC Casino] %s left %s"), *CrawlerName(Who), GameTitle(Table));
}

void ADCCCasinoCascade::Buy(ADCCCharacterBase* Who, int32 Drink)
{
	if (!HasAuthority() || !Who)
	{
		return;
	}
	const FRoom Layout = Room();
	if (FVector::Dist2D(Who->GetActorLocation(), Layout.Staff[5].At) > 700.0f)
	{
		bool bClose = false;
		for (int32 i = 0; i < TableCount && !bClose; ++i)
		{
			bClose = Near(Layout, i, Who->GetActorLocation());
		}
		if (!bClose)
		{
			return;
		}
	}
	FString Label;
	int32 Price = 0;
	FString Spoken;
	DrinkInfo(FMath::Clamp(Drink, 0, DrinkCount - 1), Label, Price, Spoken);
	APlayerState* State = Who->GetPlayerState();
	const FString Key = DCCProfile::KeyFor(State);
	if (Key.IsEmpty() || !DCCProfile::SpendTickets(Key, Price))
	{
		Tell(Who, FString::Printf(TEXT("%s is %d tickets."), *Label, Price));
		return;
	}
	if (Who->Wallet)
	{
		Who->Wallet->Tickets = DCCProfile::GetTickets(Key);
	}
	Tell(Who, FString::Printf(TEXT("-%d tickets. %s"), Price, *Spoken));
	UE_LOG(LogTemp, Log, TEXT("[DCC Casino] %s bought %s"), *CrawlerName(Who), *Label);
}

void ADCCCasinoCascade::StartHand(int32 Table)
{
	if (!Tables.IsValidIndex(Table) || Occupied(Table) < 1)
	{
		return;
	}
	FDCCCasinoTableNet& Pit = Tables[Table];
	const double Now = GetWorld()->GetTimeSeconds();
	for (int32 s = 0; s < SeatCount; ++s)
	{
		FDCCCasinoSeatNet& Seat = Seats[Table * SeatCount + s];
		if (Seat.Name.IsEmpty())
		{
			continue;
		}
		Seat.bInHand = 1;
		Seat.bDone = 0;
		Seat.Rolls = 0;
		Seat.Score = 0;
		Seat.Card0 = Seat.Card1 = Seat.Card2 = Seat.Card3 = Seat.Card4 = 0;
	}
	Pit.bShowDealer = 0;
	Pit.Dealer0 = Pit.Dealer1 = Pit.Dealer2 = Pit.Dealer3 = Pit.Dealer4 = 0;
	Pit.Turn = 255;
	Pit.Result = -1;

	if (Table == 1)
	{
		Pit.Phase = 2;
		Pit.ScoreA = 0;
		Pit.ScoreB = 0;
		Pit.EndsAt = Now + 5.0;
		Pit.Line = TEXT("First to 3. Gold shoots the left mark, blue the right.");
		for (int32 s = 0; s < SeatCount; ++s)
		{
			Seats[Table * SeatCount + s].bDone = 0;
		}
		return;
	}

	if (Table == 2 || Table == 4)
	{
		const bool bWheel = Table == 4;
		Pit.Result = bWheel ? FMath::RandRange(0, 5) : FMath::RandRange(0, 36);
		FString Summary;
		for (int32 s = 0; s < SeatCount; ++s)
		{
			FDCCCasinoSeatNet& Seat = Seats[Table * SeatCount + s];
			if (Seat.Name.IsEmpty())
			{
				continue;
			}
			ADCCCharacterBase* Who = nullptr;
			for (TActorIterator<ADCCCharacterBase> It(GetWorld()); It; ++It)
			{
				if (CrawlerName(*It) == Seat.Name)
				{
					Who = *It;
					break;
				}
			}
			TakeStake(Who, Seat);
			bool bWin = false;
			int32 Pay = 0;
			FString Bet;
			if (bWheel)
			{
				int32 Pick = Seat.Choice >= 20 && Seat.Choice <= 25 ? int32(Seat.Choice) - 20 : 0;
				if (Seat.Choice == 255)
				{
					Pick = 0;
				}
				Bet = WheelWord[Pick];
				bWin = Pick == Pit.Result && WheelPay[Pick] > 0;
				if (bWin)
				{
					Pay = Seat.Stake > 0 ? Seat.Stake * WheelPay[Pick] : WheelPay[Pick];
				}
				else if (Pit.Result == Pick)
				{
					Bet = TEXT("Bust");
				}
			}
			else
			{
				int32 Pick = Seat.Choice >= 10 && Seat.Choice <= 12 ? int32(Seat.Choice) : 10;
				const bool bGreen = Pit.Result == 0;
				const bool bRed = DCCCasinoRedPocket(Pit.Result);
				Bet = Pick == 12 ? TEXT("green") : Pick == 11 ? TEXT("black") : TEXT("red");
				bWin = (Pick == 12 && bGreen) || (Pick == 10 && bRed) || (Pick == 11 && !bRed && !bGreen);
				if (bWin)
				{
					Pay = Seat.Stake > 0 ? (Pick == 12 ? Seat.Stake * 15 : Seat.Stake * 2) : (Pick == 12 ? 8 : 4);
				}
			}
			if (bWin)
			{
				Give(Who, Pay, FString::Printf(TEXT("%s hit"), *Bet));
			}
			else
			{
				Tell(Who, FString::Printf(TEXT("%s did not come up."), *Bet));
			}
		}
		if (bWheel)
		{
			Pit.Line = FString::Printf(TEXT("The wheel stops on %s."), WheelWord[Pit.Result]);
		}
		else
		{
			const TCHAR* Colour = Pit.Result == 0 ? TEXT("green") : DCCCasinoRedPocket(Pit.Result) ? TEXT("red") : TEXT("black");
			Pit.Line = FString::Printf(TEXT("Ball on %d, %s."), Pit.Result, Colour);
		}
		Pit.Phase = 3;
		Pit.EndsAt = Now + 6.0;
		UE_LOG(LogTemp, Log, TEXT("[DCC Casino] %s resolved: %s"), GameTitle(Table), *Pit.Line);
		return;
	}

	if (Table == 3)
	{
		for (int32 s = 0; s < SeatCount; ++s)
		{
			FDCCCasinoSeatNet& Seat = Seats[Table * SeatCount + s];
			if (Seat.Name.IsEmpty())
			{
				continue;
			}
			ADCCCharacterBase* Who = nullptr;
			for (TActorIterator<ADCCCharacterBase> It(GetWorld()); It; ++It)
			{
				if (CrawlerName(*It) == Seat.Name)
				{
					Who = *It;
					break;
				}
			}
			TakeStake(Who, Seat);
			SetSeatCard(Seat, 0, DrawRank());
			SetSeatCard(Seat, 1, DrawRank());
			if (DCCCasinoHandValue(Seat.Card0, Seat.Card1, 0, 0, 0) == 21)
			{
				Seat.bDone = 1;
			}
		}
		Pit.Dealer0 = DrawRank();
		Pit.Dealer1 = DrawRank();
		Pit.Phase = 2;
		Pit.Line = TEXT("June deals.");
		Pit.EndsAt = Now + 25.0;
		AdvanceBlackjack(Table);
		return;
	}
}

void ADCCCasinoCascade::AdvanceBlackjack(int32 Table)
{
	if (!Tables.IsValidIndex(Table))
	{
		return;
	}
	FDCCCasinoTableNet& Pit = Tables[Table];
	Pit.Turn = 255;
	for (int32 s = 0; s < SeatCount; ++s)
	{
		const FDCCCasinoSeatNet& Seat = Seats[Table * SeatCount + s];
		if (!Seat.Name.IsEmpty() && Seat.bInHand && !Seat.bDone)
		{
			Pit.Turn = uint8(s);
			Pit.EndsAt = GetWorld()->GetTimeSeconds() + 25.0;
			Pit.Line = FString::Printf(TEXT("%s to act."), *Seat.Name);
			return;
		}
	}
	FinishBlackjack(Table);
}

void ADCCCasinoCascade::FinishBlackjack(int32 Table)
{
	FDCCCasinoTableNet& Pit = Tables[Table];
	const bool bDealerNatural = DCCCasinoHandValue(Pit.Dealer0, Pit.Dealer1, 0, 0, 0) == 21;
	if (!bDealerNatural)
	{
		int32 Slot = Pit.Dealer1 ? (Pit.Dealer2 ? (Pit.Dealer3 ? (Pit.Dealer4 ? -1 : 4) : 3) : 2) : 1;
		while (Slot >= 0 && DCCCasinoHandValue(Pit.Dealer0, Pit.Dealer1, Pit.Dealer2, Pit.Dealer3, Pit.Dealer4) < 17)
		{
			SetDealerCard(Pit, Slot, DrawRank());
			++Slot;
			if (Slot > 4)
			{
				break;
			}
		}
	}
	Pit.bShowDealer = 1;
	const int32 Dealer = DCCCasinoHandValue(Pit.Dealer0, Pit.Dealer1, Pit.Dealer2, Pit.Dealer3, Pit.Dealer4);
	FString Line = FString::Printf(TEXT("June shows %d."), Dealer);
	for (int32 s = 0; s < SeatCount; ++s)
	{
		FDCCCasinoSeatNet& Seat = Seats[Table * SeatCount + s];
		if (Seat.Name.IsEmpty() || !Seat.bInHand)
		{
			continue;
		}
		ADCCCharacterBase* Who = nullptr;
		for (TActorIterator<ADCCCharacterBase> It(GetWorld()); It; ++It)
		{
			if (CrawlerName(*It) == Seat.Name)
			{
				Who = *It;
				break;
			}
		}
		const int32 Value = DCCCasinoHandValue(Seat.Card0, Seat.Card1, Seat.Card2, Seat.Card3, Seat.Card4);
		const bool bNatural = Seat.Card2 == 0 && Value == 21;
		const bool bBust = Value > 21;
		if (bBust || (!bNatural && Dealer <= 21 && Dealer > Value))
		{
			Tell(Who, FString::Printf(TEXT("%d. The house keeps it."), Value));
		}
		else if ((bNatural && bDealerNatural) || (Value == Dealer && Dealer <= 21))
		{
			if (Seat.Stake > 0)
			{
				Give(Who, Seat.Stake, TEXT("push"));
			}
			else
			{
				Tell(Who, TEXT("Push."));
			}
		}
		else
		{
			int32 Pay = 4;
			if (Seat.Stake > 0)
			{
				Pay = bNatural ? Seat.Stake * 2 + Seat.Stake / 2 : Seat.Stake * 2;
			}
			else if (bNatural)
			{
				Pay = 6;
			}
			Give(Who, Pay, bNatural ? FString(TEXT("twenty-one")) : FString(TEXT("the hand")));
		}
		Line += FString::Printf(TEXT(" %s %d."), *Seat.Name, Value);
	}
	Pit.Line = Line;
	Pit.Phase = 3;
	Pit.Turn = 255;
	Pit.EndsAt = GetWorld()->GetTimeSeconds() + 7.0;
	UE_LOG(LogTemp, Log, TEXT("[DCC Casino] blackjack: %s"), *Line);
}

void ADCCCasinoCascade::ResolveHockey(int32 Table)
{
	FDCCCasinoTableNet& Pit = Tables[Table];
	auto Side = [&](int32 First, bool& bHouse)
	{
		int32 Best = 1000;
		bHouse = true;
		bool bShot = false;
		for (int32 i = 0; i < 3; ++i)
		{
			const FDCCCasinoSeatNet& Seat = Seats[Table * SeatCount + First + i];
			if (Seat.Name.IsEmpty() || !Seat.bInHand)
			{
				continue;
			}
			bHouse = false;
			if (Seat.bDone)
			{
				bShot = true;
				Best = FMath::Min(Best, int32(Seat.Score));
			}
		}
		if (bHouse)
		{
			return 18;
		}
		return bShot ? Best : 100;
	};
	bool bHouseA = false;
	bool bHouseB = false;
	const int32 ErrA = Side(0, bHouseA);
	const int32 ErrB = Side(3, bHouseB);
	if (ErrA < ErrB)
	{
		++Pit.ScoreA;
	}
	else if (ErrB < ErrA)
	{
		++Pit.ScoreB;
	}
	for (int32 s = 0; s < SeatCount; ++s)
	{
		FDCCCasinoSeatNet& Seat = Seats[Table * SeatCount + s];
		Seat.bDone = 0;
		Seat.Score = 0;
	}
	const double Now = GetWorld()->GetTimeSeconds();
	if (Pit.ScoreA >= 3 || Pit.ScoreB >= 3)
	{
		const bool bGold = Pit.ScoreA >= 3;
		Pit.Line = FString::Printf(TEXT("%s takes it, %d to %d."), bGold ? TEXT("Gold") : TEXT("Blue"), Pit.ScoreA, Pit.ScoreB);
		for (int32 s = 0; s < SeatCount; ++s)
		{
			const bool bWin = bGold ? s < 3 : s >= 3;
			FDCCCasinoSeatNet& Seat = Seats[Table * SeatCount + s];
			if (Seat.Name.IsEmpty() || !Seat.bInHand || !bWin)
			{
				continue;
			}
			for (TActorIterator<ADCCCharacterBase> It(GetWorld()); It; ++It)
			{
				if (CrawlerName(*It) == Seat.Name)
				{
					Give(*It, 8, TEXT("air hockey"));
					break;
				}
			}
		}
		Pit.Phase = 3;
		Pit.EndsAt = Now + 6.0;
		UE_LOG(LogTemp, Log, TEXT("[DCC Casino] hockey: %s"), *Pit.Line);
		return;
	}
	Pit.EndsAt = Now + 5.0;
	Pit.Line = FString::Printf(TEXT("Gold %d, blue %d."), Pit.ScoreA, Pit.ScoreB);
}

void ADCCCasinoCascade::FinishShow(int32 Table)
{
	if (Occupied(Table) <= 0)
	{
		Tables[Table] = FDCCCasinoTableNet();
		return;
	}
	FDCCCasinoTableNet Cleared;
	Cleared.Phase = Table == 0 ? 2 : 1;
	Cleared.EndsAt = GetWorld()->GetTimeSeconds() + 6.0;
	Cleared.Line = Tables[Table].Line;
	for (int32 s = 0; s < SeatCount; ++s)
	{
		FDCCCasinoSeatNet& Seat = Seats[Table * SeatCount + s];
		const FString Name = Seat.Name;
		Seat = FDCCCasinoSeatNet();
		Seat.Name = Name;
	}
	Tables[Table] = Cleared;
}

void ADCCCasinoCascade::Choose(ADCCCharacterBase* Who, int32 ChoiceId)
{
	if (!HasAuthority() || !Who)
	{
		return;
	}
	if (ChoiceId == 0)
	{
		Leave(Who);
		return;
	}
	int32 Table = -1;
	int32 SeatIndex = -1;
	const int32 Index = FindSeat(Who->GetPlayerState(), Table, SeatIndex);
	if (Index < 0 || !Tables.IsValidIndex(Table))
	{
		return;
	}
	FDCCCasinoTableNet& Pit = Tables[Table];
	FDCCCasinoSeatNet& Seat = Seats[Index];
	const double Now = GetWorld()->GetTimeSeconds();

	if (Table == 0 && ChoiceId == 1)
	{
		const double At = Mark(Now, SeatIndex);
		const int32 Points = SkeePoints(At);
		Seat.Score = int16(Seat.Score + Points);
		++Seat.Rolls;
		Seat.LastBall = uint8(Points);
		++Seat.BallSerial;
		Tell(Who, FString::Printf(TEXT("%d."), Points));
		if (Seat.Rolls >= 3)
		{
			const int32 Tickets = FMath::Max(1, int32(Seat.Score) / 10);
			Give(Who, Tickets, FString::Printf(TEXT("skee-ball %d"), int32(Seat.Score)));
			Pit.Line = FString::Printf(TEXT("%s rolled %d."), *Seat.Name, int32(Seat.Score));
			Seat.Score = 0;
			Seat.Rolls = 0;
			UE_LOG(LogTemp, Log, TEXT("[DCC Casino] %s skee %d tickets"), *Seat.Name, Tickets);
		}
		ForceNetUpdate();
		return;
	}

	if (Pit.Phase == 1 && (ChoiceId == 10 || ChoiceId == 11 || ChoiceId == 12 || (ChoiceId >= 20 && ChoiceId <= 25)))
	{
		Seat.Choice = uint8(ChoiceId);
		bool bAll = true;
		for (int32 s = 0; s < SeatCount; ++s)
		{
			const FDCCCasinoSeatNet& Other = Seats[Table * SeatCount + s];
			if (!Other.Name.IsEmpty() && Other.Choice == 255)
			{
				bAll = false;
			}
		}
		if (bAll)
		{
			StartHand(Table);
		}
		ForceNetUpdate();
		return;
	}

	if (Pit.Phase == 1 && ChoiceId == 1)
	{
		StartHand(Table);
		ForceNetUpdate();
		return;
	}

	if (Table == 1 && Pit.Phase == 2 && ChoiceId == 1 && Seat.bInHand && !Seat.bDone)
	{
		const bool bGold = SeatIndex < 3;
		const double At = Mark(Now, 0);
		const double Sweet = bGold ? 0.22 : 0.78;
		Seat.Score = int16(FMath::Clamp(FMath::RoundToInt(FMath::Abs(At - Sweet) * 100.0), 0, 100));
		Seat.bDone = 1;
		bool bGoldShot = false;
		bool bBlueShot = false;
		bool bGoldPlayer = false;
		bool bBluePlayer = false;
		for (int32 s = 0; s < SeatCount; ++s)
		{
			const FDCCCasinoSeatNet& Other = Seats[Table * SeatCount + s];
			if (Other.Name.IsEmpty() || !Other.bInHand)
			{
				continue;
			}
			if (s < 3)
			{
				bGoldPlayer = true;
				bGoldShot |= Other.bDone != 0;
			}
			else
			{
				bBluePlayer = true;
				bBlueShot |= Other.bDone != 0;
			}
		}
		if ((!bGoldPlayer || bGoldShot) && (!bBluePlayer || bBlueShot))
		{
			ResolveHockey(Table);
		}
		ForceNetUpdate();
		return;
	}

	if (Table == 3 && Pit.Phase == 2 && Pit.Turn == SeatIndex && Seat.bInHand && !Seat.bDone)
	{
		if (ChoiceId == 30)
		{
			int32 Slot = -1;
			if (Seat.Card0 == 0) Slot = 0;
			else if (Seat.Card1 == 0) Slot = 1;
			else if (Seat.Card2 == 0) Slot = 2;
			else if (Seat.Card3 == 0) Slot = 3;
			else if (Seat.Card4 == 0) Slot = 4;
			if (Slot >= 0)
			{
				SetSeatCard(Seat, Slot, DrawRank());
			}
			const int32 Value = DCCCasinoHandValue(Seat.Card0, Seat.Card1, Seat.Card2, Seat.Card3, Seat.Card4);
			if (Value > 21)
			{
				Seat.bDone = 1;
				Tell(Who, FString::Printf(TEXT("Bust, %d."), Value));
				AdvanceBlackjack(Table);
			}
			else if (Value == 21 || Slot < 0)
			{
				Seat.bDone = 1;
				AdvanceBlackjack(Table);
			}
			else
			{
				Pit.EndsAt = Now + 25.0;
			}
		}
		else if (ChoiceId == 31)
		{
			Seat.bDone = 1;
			AdvanceBlackjack(Table);
		}
		ForceNetUpdate();
	}
}

void ADCCCasinoCascade::ServerTickGame()
{
	EnsureArrays();
	UWorld* World = GetWorld();
	if (!World)
	{
		return;
	}
	const double Now = World->GetTimeSeconds();
	ServerClock = Now;
	for (int32 t = 0; t < TableCount; ++t)
	{
		FDCCCasinoTableNet& Pit = Tables[t];
		if (Pit.Phase == 0)
		{
			continue;
		}
		if (Occupied(t) == 0)
		{
			Pit = FDCCCasinoTableNet();
			continue;
		}
		if (Pit.Phase == 1 && Now >= Pit.EndsAt)
		{
			StartHand(t);
		}
		else if (Pit.Phase == 2 && t == 1 && Now >= Pit.EndsAt)
		{
			ResolveHockey(t);
		}
		else if (Pit.Phase == 2 && t == 3 && Now >= Pit.EndsAt && Pit.Turn != 255)
		{
			Seats[t * SeatCount + Pit.Turn].bDone = 1;
			AdvanceBlackjack(t);
		}
		else if (Pit.Phase == 3 && Now >= Pit.EndsAt)
		{
			FinishShow(t);
		}
	}
}

void ADCCCasinoCascade::Tick(float DeltaSeconds)
{
	Super::Tick(DeltaSeconds);
	if (!bFloorBuilt)
	{
		BuildFloor();
	}
	AnimateFloor(DeltaSeconds);
	if (HasAuthority())
	{
		ServerTickGame();
	}

	UWorld* World = GetWorld();
	if (!World)
	{
		return;
	}
	const FRoom Layout = Room();
	for (TActorIterator<ADCCCharacterBase> It(World); It; ++It)
	{
		ADCCCharacterBase* Who = *It;
		UCharacterMovementComponent* Move = Who->GetCharacterMovement();
		if (!Move)
		{
			continue;
		}
		int32 Table = -1;
		int32 Seat = -1;
		if (FindSeat(Who->GetPlayerState(), Table, Seat) >= 0)
		{
			if (!Held.Contains(Who))
			{
				Move->StopMovementImmediately();
				Move->DisableMovement();
				Held.Add(Who);
			}
			if (HasAuthority())
			{
				const FSpot& Spot = Layout.Seats[Table][Seat];
				if (FVector::Dist2D(Who->GetActorLocation(), Spot.At) > 45.0f)
				{
					Who->SetActorLocation(Spot.At, false, nullptr, ETeleportType::TeleportPhysics);
				}
				Who->SetActorRotation(Spot.Face);
			}
		}
		else if (Held.Contains(Who))
		{
			Release(Who);
		}
	}
}

void ADCCCasinoCascade::FillOptions(const ADCCCharacterBase* Who, TArray<FString>& Labels, TArray<int32>& Ids) const
{
	Labels.Reset();
	Ids.Reset();
	int32 Table = -1;
	int32 Seat = -1;
	if (!Who || FindSeat(Who->GetPlayerState(), Table, Seat) < 0 || !Tables.IsValidIndex(Table))
	{
		return;
	}
	const FDCCCasinoTableNet& Pit = Tables[Table];
	const FDCCCasinoSeatNet& Mine = Seats[Table * SeatCount + Seat];
	auto Add = [&](const TCHAR* Label, int32 Id)
	{
		FString Row(Label);
		if (Mine.Choice == Id)
		{
			Row += TEXT("  (yours)");
		}
		Labels.Add(Row);
		Ids.Add(Id);
	};

	if (Table == 0)
	{
		Add(TEXT("Roll"), 1);
		Add(TEXT("Leave"), 0);
		return;
	}
	if (Pit.Phase == 1)
	{
		if (Table == 2)
		{
			Add(TEXT("Red"), 10);
			Add(TEXT("Black"), 11);
			Add(TEXT("Green"), 12);
			Add(TEXT("Spin"), 1);
		}
		else if (Table == 4)
		{
			for (int32 i = 0; i < 6; ++i)
			{
				Add(WheelWord[i], 20 + i);
			}
			Add(TEXT("Spin"), 1);
		}
		else if (Table == 1)
		{
			Add(TEXT("Drop the puck"), 1);
		}
		else
		{
			Add(TEXT("Deal"), 1);
		}
		Add(TEXT("Leave"), 0);
		return;
	}
	if (Pit.Phase == 2 && Table == 1)
	{
		Add(TEXT("Shoot"), 1);
		Add(TEXT("Leave"), 0);
		return;
	}
	if (Pit.Phase == 2 && Table == 3 && Pit.Turn == Seat && Mine.bInHand && !Mine.bDone)
	{
		Add(TEXT("Hit"), 30);
		Add(TEXT("Stand"), 31);
	}
	Add(TEXT("Leave"), 0);
}

int32 ADCCCasinoCascade::LocalIndex(const ADCCCharacterBase* Who) const
{
	const int32* Found = MenuIndex.Find(Who);
	return Found ? *Found : 0;
}

void ADCCCasinoCascade::LocalScroll(const ADCCCharacterBase* Who, int32 Delta)
{
	TArray<FString> Labels;
	TArray<int32> Ids;
	FillOptions(Who, Labels, Ids);
	if (Ids.IsEmpty())
	{
		return;
	}
	int32& Index = MenuIndex.FindOrAdd(Who);
	Index = ((Index + Delta) % Ids.Num() + Ids.Num()) % Ids.Num();
}

void ADCCCasinoCascade::LocalConfirm(ADCCCharacterBase* Who)
{
	if (!Who)
	{
		return;
	}
	TArray<FString> Labels;
	TArray<int32> Ids;
	FillOptions(Who, Labels, Ids);
	if (Ids.IsEmpty())
	{
		return;
	}
	int32& Index = MenuIndex.FindOrAdd(Who);
	Index = FMath::Clamp(Index, 0, Ids.Num() - 1);
	if (Who->HasAuthority())
	{
		Who->Server_CasinoAction_Implementation(2, Ids[Index]);
	}
	else
	{
		Who->Server_CasinoAction(2, Ids[Index]);
	}
}

void ADCCCharacterBase::Server_CasinoAction_Implementation(uint8 Action, int32 Arg)
{
	ADCCCasinoCascade* Casino = ADCCCasinoCascade::Get(GetWorld());
	if (!Casino)
	{
		return;
	}
	switch (Action)
	{
	case 1: Casino->Join(this, Arg); break;
	case 2: Casino->Choose(this, Arg); break;
	case 3: Casino->Leave(this); break;
	case 4: Casino->Buy(this, Arg); break;
	default: break;
	}
}

static FAutoConsoleCommandWithWorldAndArgs GCasinoGo(
	TEXT("DCC.CasinoGo"),
	TEXT("Dev: stand in the Casino Cascade doorway, looking in. Optional delay in seconds."),
	FConsoleCommandWithWorldAndArgsDelegate::CreateLambda([](const TArray<FString>& Args, UWorld* World)
	{
		if (!World)
		{
			return;
		}
		const float Delay = Args.Num() > 0 ? FCString::Atof(*Args.Last()) : 0.0f;
		auto Go = [](UWorld* W)
		{
			if (!W)
			{
				return;
			}
			ADCCArcadeLobby* Lobby = nullptr;
			for (TActorIterator<ADCCArcadeLobby> It(W); It; ++It)
			{
				Lobby = *It;
				break;
			}
			APlayerController* PC = W->GetFirstPlayerController();
			if (!Lobby || !PC || !PC->GetPawn())
			{
				return;
			}
			float L = 0.0f;
			float Wd = 0.0f;
			ADCCCasinoCascade::DenBounds(L, Wd);
			const FRotator Face(-4.0f, 6.0f, 0.0f);
			// Inside the doorway, a step south of center, so the east-wall wheel is not behind her head.
			APawn* Pawn = PC->GetPawn();
			Pawn->SetActorLocation(Lobby->GetActorLocation() + FVector(L - 280.0f, -140.0f, 120.0f), false, nullptr, ETeleportType::TeleportPhysics);
			Pawn->SetActorRotation(Face);
			PC->SetControlRotation(Face);
		};
		if (Delay <= 0.0f)
		{
			Go(World);
			return;
		}
		FTimerHandle Handle;
		TWeakObjectPtr<UWorld> Weak(World);
		World->GetTimerManager().SetTimer(Handle, FTimerDelegate::CreateLambda([Weak, Go]()
		{
			if (Weak.IsValid())
			{
				Go(Weak.Get());
			}
		}), Delay, false);
	}));

static FAutoConsoleCommandWithWorldAndArgs GCasinoSpot(
	TEXT("DCC.CasinoSpot"),
	TEXT("Dev: stand at a casino game, looking at it. Station 0 skee, 1 hockey, 2 roulette, 3 blackjack, 4 wheel. Delay is the last argument."),
	FConsoleCommandWithWorldAndArgsDelegate::CreateLambda([](const TArray<FString>& Args, UWorld* World)
	{
		if (!World)
		{
			return;
		}
		const int32 Which = Args.Num() > 0 ? FCString::Atoi(*Args[0]) : 0;
		const float Delay = Args.Num() > 1 ? FCString::Atof(*Args.Last()) : 0.0f;
		auto Go = [Which](UWorld* W)
		{
			if (!W)
			{
				return;
			}
			ADCCArcadeLobby* Lobby = nullptr;
			for (TActorIterator<ADCCArcadeLobby> It(W); It; ++It)
			{
				Lobby = *It;
				break;
			}
			APlayerController* PC = W->GetFirstPlayerController();
			if (!Lobby || !PC || !PC->GetPawn())
			{
				return;
			}
			const ADCCCasinoCascade::FRoom R = ADCCCasinoCascade::RoomFor(Lobby->GetActorLocation());
			const int32 Index = FMath::Clamp(Which, 0, ADCCCasinoCascade::TableCount - 1);
			// Stand on the open side of the machine, far enough that the camera sees the game.
			FVector Back(0.0f, -1.0f, 0.0f);
			float Dist = 700.0f;
			float LookZ = 80.0f;
			if (Index == 0)
			{
				Dist = 1080.0f;
				LookZ = 150.0f;
			}
			else if (Index == 1)
			{
				// From the aisle, looking south. The south wall is too close for a camera behind her.
				Back = FVector(0.0f, 1.0f, 0.0f);
				Dist = 860.0f;
				LookZ = 110.0f;
			}
			else if (Index == 3)
			{
				Back = FVector(0.0f, 1.0f, 0.0f);
				Dist = 680.0f;
			}
			else if (Index == 4)
			{
				Back = FVector(-1.0f, -0.15f, 0.0f).GetSafeNormal();
				Dist = 1400.0f;
				LookZ = 480.0f;
			}
			FVector At = R.Tables[Index] + Back * Dist;
			At.Z = 120.0f;
			const FVector Look(R.Tables[Index].X, R.Tables[Index].Y, LookZ);
			FRotator Face = (Look - FVector(At.X, At.Y, 170.0f)).Rotation();
			Face.Roll = 0.0f;
			APawn* Pawn = PC->GetPawn();
			Pawn->SetActorLocation(At, false, nullptr, ETeleportType::TeleportPhysics);
			Pawn->SetActorRotation(Face);
			PC->SetControlRotation(Face);
			if (UCharacterMovementComponent* Move = Pawn->FindComponentByClass<UCharacterMovementComponent>())
			{
				Move->StopMovementImmediately();
			}
		};
		if (Delay <= 0.0f)
		{
			Go(World);
			return;
		}
		FTimerHandle Handle;
		TWeakObjectPtr<UWorld> Weak(World);
		World->GetTimerManager().SetTimer(Handle, FTimerDelegate::CreateLambda([Weak, Go]()
		{
			if (Weak.IsValid())
			{
				Go(Weak.Get());
			}
		}), Delay, false);
	}));
