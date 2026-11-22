#include "Arcade/DCCArcadeGate.h"

#include "Arcade/DCCWalletComponent.h"
#include "Characters/DCCCharacterBase.h"
#include "Characters/DCCEnemyCharacter.h"
#include "Characters/DCCEnemyCharacter_DebtEater.h"
#include "Characters/DCCEnemyCharacter_Wildlife.h"
#include "Characters/DCCEnemyCharacter_Corporate.h"
#include "Characters/DCCEnemyCharacter_Oathgate.h"
#include "Characters/DCCEnemyCharacter_NuclearFamily.h"
#include "Characters/DCCEnemyCharacter_TestBoss.h"
#include "Characters/DCCEnemyCharacter_Voldemortgage.h"
#include "Characters/DCCEnemyCharacter_SurveyDrone.h"
#include "Characters/DCCEnemyCharacter_Gate.h"
#include "Animation/AnimSequence.h"
#include "Components/SkeletalMeshComponent.h"
#include "Engine/SkeletalMesh.h"
#include "Materials/MaterialInstanceDynamic.h"
#include "Components/StaticMeshComponent.h"
#include "Combat/DCCGameState.h"
#include "Components/CapsuleComponent.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "GameFramework/PlayerController.h"
#include "GameFramework/PlayerState.h"
#include "Items/DCCLootContainerVariants.h"
#include "Items/DCCPickupVariants.h"
#include "Net/UnrealNetwork.h"
#include "NiagaraFunctionLibrary.h"
#include "NiagaraSystem.h"
#include "Puzzles/DCCBreakerSwitch.h"
#include "Puzzles/DCCPressurePlate.h"
#include "Rooms/DCCDungeonGrid.h"
#include "Session/DCCProfile.h"
#include "Arcade/DCCArcade.h"
#include "Stats/DCCStatsComponent.h"
#include "UI/DCCEventLogTypes.h"

namespace
{
	constexpr float Tile = 400.0f;
	constexpr float WallHeight = 520.0f;
	constexpr int32 GridSize = 26;

	const FLinearColor Violet(0.55f, 0.35f, 1.0f);
	const FLinearColor GateCyan(0.35f, 0.85f, 1.0f);

	void Say(const UObject* Context, const FString& Text)
	{
		UE_LOG(LogTemp, Log, TEXT("[DCC Gate] %s"), *Text);
		ADCCGameState::Log(Context, EDCCLogCategory::Announcement, FText::FromString(Text));
	}

	FLinearColor RoleColour(FName Role)
	{
		if (Role == TEXT("fight")) { return FLinearColor(1.0f, 0.35f, 0.3f); }
		if (Role == TEXT("trial")) { return FLinearColor(1.0f, 0.65f, 0.2f); }
		if (Role == TEXT("vault")) { return FLinearColor(0.35f, 1.0f, 0.6f); }
		if (Role == TEXT("treasure")) { return FLinearColor(1.0f, 0.85f, 0.3f); }
		if (Role == TEXT("boss")) { return FLinearColor(0.75f, 0.3f, 1.0f); }
		if (Role == TEXT("duel")) { return FLinearColor(1.0f, 0.78f, 0.3f); }
		if (Role == TEXT("ambush")) { return FLinearColor(0.3f, 0.8f, 0.75f); }
		if (Role == TEXT("shrine")) { return FLinearColor(0.5f, 1.0f, 0.8f); }
		if (Role == TEXT("gauntlet")) { return FLinearColor(1.0f, 0.42f, 0.2f); }
		if (Role == TEXT("toll")) { return FLinearColor(1.0f, 0.78f, 0.2f); }
		if (Role == TEXT("outage")) { return FLinearColor(0.9f, 0.2f, 0.15f); }
		if (Role == TEXT("clock")) { return FLinearColor(0.45f, 0.85f, 1.0f); }
		if (Role == TEXT("pen")) { return FLinearColor(0.85f, 0.45f, 0.25f); }
		return GateCyan; // start
	}

	FString RoleSign(FName Role)
	{
		if (Role == TEXT("fight")) { return TEXT("FIGHT"); }
		if (Role == TEXT("trial")) { return TEXT("TRIAL  -  step in the ring, survive three waves"); }
		if (Role == TEXT("vault")) { return TEXT("VAULT  -  solve it, or force it (E) for Favor"); }
		if (Role == TEXT("treasure")) { return TEXT("TREASURE"); }
		if (Role == TEXT("boss")) { return TEXT("THE GATEKEEPER"); }
		if (Role == TEXT("duel")) { return TEXT("THE CHAMPION"); }
		if (Role == TEXT("ambush")) { return TEXT("QUIET ROOM"); }
		if (Role == TEXT("shrine")) { return TEXT("REST SHRINE  -  stand in the Warden's light"); }
		if (Role == TEXT("gauntlet")) { return TEXT("THE GAUNTLET  -  dark tiles only, to the green pad"); }
		if (Role == TEXT("toll")) { return TEXT("THE TOLL  -  hold the booth. The line is already mad."); }
		if (Role == TEXT("outage")) { return TEXT("THE OUTAGE  -  reach the middle. The lights go first."); }
		if (Role == TEXT("clock")) { return TEXT("THE CLOCK  -  three waves, and they do not wait"); }
		if (Role == TEXT("pen")) { return TEXT("THE HOLDING PEN  -  clear the ring. The fence burns."); }
		return TEXT("GATE ENTRY");
	}

	int32 RolePoints(FName Role)
	{
		if (Role == TEXT("fight")) { return 100; }
		if (Role == TEXT("trial")) { return 160; }
		if (Role == TEXT("vault")) { return 120; }
		if (Role == TEXT("treasure")) { return 60; }
		if (Role == TEXT("boss")) { return 400; }
		if (Role == TEXT("duel")) { return 180; }
		if (Role == TEXT("ambush")) { return 120; }
		if (Role == TEXT("shrine")) { return 40; }
		if (Role == TEXT("gauntlet")) { return 150; }
		if (Role == TEXT("toll")) { return 140; }
		if (Role == TEXT("outage")) { return 140; }
		if (Role == TEXT("clock")) { return 170; }
		if (Role == TEXT("pen")) { return 150; }
		return 0;
	}
}

// ---- rank and roles: the same on every machine, from the seed -------------------------------------------------------

int32 ADCCArcadeGate::RankFor(int32 InRooms)
{
	// 3-4 E, 5 D, 6-7 C, 8 B, 9 A, 10 S
	return InRooms <= 4 ? 0 : InRooms == 5 ? 1 : InRooms <= 7 ? 2 : InRooms == 8 ? 3 : InRooms == 9 ? 4 : 5;
}

const TCHAR* ADCCArcadeGate::RankName(int32 Index)
{
	static const TCHAR* Names[] = { TEXT("E"), TEXT("D"), TEXT("C"), TEXT("B"), TEXT("A"), TEXT("S") };
	return Names[FMath::Clamp(Index, 0, 5)];
}

FName ADCCArcadeGate::RoleFor(int32 InSeed, int32 Index, int32 Count)
{
	if (Index == 0)
	{
		return TEXT("start");
	}
	if (Index == Count - 1)
	{
		return TEXT("boss");
	}
	// Half the middle rooms (at least one, once there are two) are the newer rounds: toll, outage, clock, pen.
	// The rest stay the older set. Shuffled, and the same seed builds the same list on every machine.
	const int32 Middle = FMath::Max(0, Count - 2);
	TArray<FName> Fresh = { TEXT("toll"), TEXT("outage"), TEXT("clock"), TEXT("pen") };
	TArray<FName> Older = {
		TEXT("vault"), TEXT("duel"), TEXT("gauntlet"), TEXT("shrine"),
		TEXT("ambush"), TEXT("trial"), TEXT("treasure"), TEXT("fight"),
	};
	FRandomStream R(InSeed * 13 + 7);
	auto Shuffle = [&R](TArray<FName>& List)
	{
		for (int32 i = List.Num() - 1; i > 0; --i)
		{
			List.Swap(i, R.RandRange(0, i));
		}
	};
	Shuffle(Fresh);
	Shuffle(Older);
	const int32 NewCount = FMath::Min(Fresh.Num(), Middle >= 2 ? FMath::Max(1, Middle / 2) : 0);
	TArray<FName> Plan;
	for (int32 i = 0; i < NewCount; ++i)
	{
		Plan.Add(Fresh[i]);
	}
	for (int32 i = 0; Plan.Num() < Middle && i < Older.Num(); ++i)
	{
		Plan.Add(Older[i]);
	}
	for (int32 i = NewCount; Plan.Num() < Middle && i < Fresh.Num(); ++i)
	{
		Plan.Add(Fresh[i]);
	}
	Shuffle(Plan);
	return Plan.IsValidIndex(Index - 1) ? Plan[Index - 1] : TEXT("fight");
}

void ADCCArcadeGate::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);
	DOREPLIFETIME(ADCCArcadeGate, Seed);
	DOREPLIFETIME(ADCCArcadeGate, RoomCount);
	DOREPLIFETIME(ADCCArcadeGate, bExitOpen);
	DOREPLIFETIME(ADCCArcadeGate, SlamCenter);
	DOREPLIFETIME(ADCCArcadeGate, SlamAt);
	DOREPLIFETIME(ADCCArcadeGate, SlamRadius);
}

ADCCArcadeArena::FSky ADCCArcadeGate::GetSky() const
{
	FSky Sky;
	Sky.SunPitch = -18.0f;
	Sky.SunIntensity = 1.4f;
	Sky.SunColour = FLinearColor(0.7f, 0.55f, 1.0f);
	Sky.SkyIntensity = 0.35f;
	Sky.FogColour = FLinearColor(0.16f, 0.1f, 0.3f);
	Sky.FogDensity = 0.012f;
	return Sky;
}

void ADCCArcadeGate::GetStartTransforms(TArray<FTransform>& Out) const
{
	for (int32 i = 0; i < 8; ++i)
	{
		Out.Add(FTransform(FRotator(0.0f, 90.0f, 0.0f), FVector((i - 3.5f) * 240.0f, -700.0f, 120.0f)));
	}
}

void ADCCArcadeGate::PrepareServer()
{
	Seed = FMath::RandRange(1, 1 << 20);
	// A fixed gate on request (-GateSeed=N), for testing and trailers.
	int32 FromLine = 0;
	if (FParse::Value(FCommandLine::Get(), TEXT("GateSeed="), FromLine) && FromLine > 0) { Seed = FromLine; }
	RoomCount = FRandomStream(Seed).RandRange(3, 10);
}

// ---- the plaza (every machine) ------------------------------------------------------------------------------------

void ADCCArcadeGate::DrawPortal(const FVector& At, float Height, const FLinearColor& Colour, float Yaw, bool bFrame, bool bPowered)
{
	if (bFrame)
	{
		Prop(TEXT("/Game/Alien_Portal/Meshes/Portal/SM_portal"), At, Yaw, FVector(0, 0, Height));
	}
	if (!bPowered)
	{
		return;
	}
	Prop(TEXT("/Game/Alien_Portal/Meshes/SM_portal_FX"), At, Yaw, FVector(0, 0, Height));
	// The shine: a ring of light, and the pack's sparks rising through it.
	Light(At + FVector(0, 0, Height * 0.5f), Colour, 60000.0f, Height * 2.5f);
	Light(At + FVector(0, 0, Height * 0.15f), Colour, 20000.0f, Height * 1.2f);
	if (UNiagaraSystem* Sparks = LoadObject<UNiagaraSystem>(nullptr, TEXT("/Game/Alien_Portal/FX/Particles/NS_blue_FX.NS_blue_FX")))
	{
		UNiagaraFunctionLibrary::SpawnSystemAtLocation(GetWorld(), Sparks, At + FVector(0, 0, Height * 0.4f));
	}
	Shape(TEXT("Cylinder"), At + FVector(0, 0, 3), FVector(Height * 0.9f, Height * 0.9f, 6), Colour * 0.5f, FRotator::ZeroRotator, false);
}

void ADCCArcadeGate::BuildLocal()
{
	// A dark plaza under a violet sky, ringed with broken towers, the gate hanging at the north end.
	const FLinearColor Stone(0.05f, 0.045f, 0.07f);
	// Tiled, not one slab: a single block showed one stone tile stretched across the whole plaza (2026-11-21).
	TiledBlock(FVector(0, 0, -20), FVector(9000, 9000, 40), Stone, TEXT("/Game/Ice_Station/Materials/MI_floor_tile2"), 500.0f);
	for (int32 i = 0; i < 18; ++i)
	{
		const float A = i * (2.0f * PI / 18.0f);
		const FVector At(FMath::Cos(A) * 5200.0f, FMath::Sin(A) * 5200.0f, 0.0f);
		const float H = 1600.0f + (i * 37 % 11) * 380.0f;
		const TCHAR* TowerMat = (i % 3 == 0) ? TEXT("/Game/Ice_Station/Materials/MI_concrete_block")
			: (i % 3 == 1) ? TEXT("/Game/Ice_Station/Materials/MI_concrete_block1")
			: TEXT("/Game/Abandonned_Brutalist/Materials/MI_concrete_01");
		// Stacked in storeys so the concrete keeps its scale all the way up.
		const int32 Storeys = FMath::Max(1, FMath::RoundToInt(H / 700.0f));
		for (int32 s = 0; s < Storeys; ++s)
		{
			const float Part = H / Storeys;
			Paint(Block(At + FVector(0, 0, Part * (s + 0.5f)), FVector(700, 700, Part), FLinearColor(0.035f, 0.03f, 0.05f), i * 23.0f), TowerMat);
		}
	}
	DrawPortal(EntryPortal(), 1200.0f, Violet, 0.0f);
	Sign(EntryPortal() + FVector(0, -500, 1450), -90.0f, TEXT("A GATE HAS OPENED"), FColor(200, 160, 255), 90.0f);
	Sign(EntryPortal() + FVector(0, -500, 1360), -90.0f, TEXT("Walk in when it opens. Clear the rooms. Drop the Gatekeeper."), FColor(220, 215, 235), 26.0f);
	Light(FVector(0, -400, 900), FLinearColor(0.5f, 0.45f, 0.7f), 1800.0f, 3000.0f);
	// The dungeon itself waits for the seed (ClientTick), which a client may not have yet.
}

void ADCCArcadeGate::ClientTick(float DeltaSeconds)
{
	if (!bDungeonBuilt && Seed != 0 && RoomCount > 0)
	{
		BuildDungeon();
	}
	// The ring was placed with the rooms. This is only the power coming on.
	if (bDungeonBuilt && bExitOpen && !bExitDrawn && Rooms.Num() > 0)
	{
		bExitDrawn = true;
		DrawPortal(Rooms.Last().Center, 700.0f, GateCyan, 0.0f, false, true);
	}
	const double Now = ServerTime();

	// The gauntlet: dark, amber (about to burn), red (burning). Recoloured only when a tile changes.
	for (FTile& T : Tiles)
	{
		AActor* Actor = T.Actor.Get();
		if (!Actor)
		{
			continue;
		}
		const bool bDone = Rooms.IsValidIndex(T.Room) && Rooms[T.Room].bCleared;
		const uint8 State = bDone ? 0 : TileState(T.Room, T.X, T.Y, Now);
		if (State == T.Shown)
		{
			continue;
		}
		T.Shown = State;
		if (UStaticMeshComponent* Mesh = Actor->FindComponentByClass<UStaticMeshComponent>())
		{
			if (UMaterialInstanceDynamic* Mid = Cast<UMaterialInstanceDynamic>(Mesh->GetMaterial(0)))
			{
				const FLinearColor Colour = State == 2 ? FLinearColor(1.0f, 0.15f, 0.05f) : State == 1 ? FLinearColor(0.9f, 0.55f, 0.1f) : FLinearColor(0.06f, 0.05f, 0.08f);
				Mid->SetVectorParameterValue(TEXT("Color"), Colour);
				Mid->SetVectorParameterValue(TEXT("BaseColor"), Colour);
			}
		}
	}

	// The Gatekeeper's slam: a red ring that closes in on the spot, then a flash as it lands.
	const bool bRing = SlamAt > 0.0 && Now > SlamAt - 1.7 && Now < SlamAt + 0.35;
	AActor* Ring = SlamRing.Get();
	if (bRing && !Ring)
	{
		Ring = Shape(TEXT("Cylinder"), SlamCenter + FVector(0, 0, 6), FVector(SlamRadius * 2.0f, SlamRadius * 2.0f, 8.0f), FLinearColor(1.0f, 0.1f, 0.15f), FRotator::ZeroRotator, false);
		SlamRing = Ring;
		SlamRingScale = Ring ? Ring->GetActorScale3D() : FVector::OneVector;
	}
	if (Ring)
	{
		Ring->SetActorHiddenInGame(!bRing);
		if (bRing)
		{
			const float Close = FMath::Clamp(float((Now - (SlamAt - 1.6)) / 1.6), 0.0f, 1.0f);
			const float Size = Now < SlamAt ? FMath::Lerp(0.3f, 1.0f, Close) : 1.15f;
			Ring->SetActorLocation(SlamCenter + FVector(0, 0, 6));
			Ring->SetActorScale3D(FVector(SlamRingScale.X * Size, SlamRingScale.Y * Size, SlamRingScale.Z));
			if (UStaticMeshComponent* Mesh = Ring->FindComponentByClass<UStaticMeshComponent>())
			{
				if (UMaterialInstanceDynamic* Mid = Cast<UMaterialInstanceDynamic>(Mesh->GetMaterial(0)))
				{
					const float Pulse = Now < SlamAt ? 0.5f + 0.5f * FMath::Sin(float(Now) * 18.0f) : 1.0f;
					const FLinearColor Colour = Now < SlamAt ? FLinearColor(0.6f + 0.4f * Pulse, 0.08f, 0.12f) : FLinearColor(1.0f, 0.85f, 0.7f);
					Mid->SetVectorParameterValue(TEXT("Color"), Colour);
					Mid->SetVectorParameterValue(TEXT("BaseColor"), Colour);
				}
			}
		}
	}
}

uint8 ADCCArcadeGate::TileState(int32 Room, int32 X, int32 Y, double Time)
{
	// Diagonal waves that sweep the floor: a quarter of the tiles burn on each beat, and the next beat's tiles warn
	// for the last half of this one. Every machine computes the same thing from the server clock.
	const double BeatSeconds = 1.3;
	const double Beats = Time / BeatSeconds + Room * 0.37;
	const int32 Beat = FMath::FloorToInt32(Beats);
	const double Into = Beats - Beat;
	auto Lit = [X, Y, Room](int32 B) { return ((X + 2 * Y + B + Room) % 4 + 4) % 4 == 0; };
	if (Lit(Beat))
	{
		return 2;
	}
	return Lit(Beat + 1) && Into > 0.5 ? 1 : 0;
}

void ADCCArcadeGate::DressRoom(int32 RoomIndex)
{
	const UDCCDungeonGrid::FRoom& Room = Grid->GetRooms()[RoomIndex];
	FRoomState& State = Rooms[RoomIndex];
	const FLinearColor Colour = RoleColour(State.Role);
	const FVector O = DungeonOrigin();
	const FVector Min = O + FVector(Room.X * Tile, Room.Y * Tile, 0.0f);
	const float Short = FMath::Min(Room.W, Room.H) * Tile;

	if (State.Role == TEXT("boss"))
	{
		// The Gatekeeper's hall: a ring of pillars, fire in four of them, a rune circle on the floor.
		const float R = Short * 0.38f;
		for (int32 k = 0; k < 8; ++k)
		{
			const float A = k * (2.0f * PI / 8.0f) + PI / 8.0f;
			const FVector P = State.Center + FVector(FMath::Cos(A) * R, FMath::Sin(A) * R, 0.0f);
			Paint(Shape(TEXT("Cylinder"), P + FVector(0, 0, WallHeight * 0.5f), FVector(90, 90, WallHeight), FLinearColor(0.08f, 0.05f, 0.12f)),
				TEXT("/Game/Ice_Station/Materials/MI_trim_2_metal"));
			if (k % 2 == 0)
			{
				Prop(TEXT("/Game/Legendary_Forge/Meshes/Brasero/SM_brasero_01"), P + (State.Center - P).GetSafeNormal2D() * 140.0f, 0.0f, FVector(0, 0, 120), true);
				if (UNiagaraSystem* Fire = LoadObject<UNiagaraSystem>(nullptr, TEXT("/Game/Legendary_Forge/Fx/Fire/NS_fire.NS_fire")))
				{
					UNiagaraFunctionLibrary::SpawnSystemAtLocation(GetWorld(), Fire, P + (State.Center - P).GetSafeNormal2D() * 140.0f + FVector(0, 0, 115));
				}
				Light(P + (State.Center - P).GetSafeNormal2D() * 140.0f + FVector(0, 0, 200), FLinearColor(1.0f, 0.45f, 0.2f), 4000.0f, 900.0f);
			}
		}
		Shape(TEXT("Cylinder"), State.Center + FVector(0, 0, 3), FVector(R * 1.3f, R * 1.3f, 4), Colour * 0.6f, FRotator::ZeroRotator, false);
		Shape(TEXT("Cylinder"), State.Center + FVector(0, 0, 5), FVector(R * 1.15f, R * 1.15f, 4), FLinearColor(0.05f, 0.03f, 0.08f), FRotator::ZeroRotator, false);
		Light(State.Center + FVector(0, 0, 420), FLinearColor(0.7f, 0.2f, 1.0f), 8000.0f, Short);
	}
	else if (State.Role == TEXT("duel"))
	{
		// A gold ring to fight in, and four banners.
		Shape(TEXT("Cylinder"), State.Center + FVector(0, 0, 3), FVector(1050, 1050, 4), Colour * 0.7f, FRotator::ZeroRotator, false);
		Shape(TEXT("Cylinder"), State.Center + FVector(0, 0, 5), FVector(980, 980, 4), FLinearColor(0.07f, 0.06f, 0.05f), FRotator::ZeroRotator, false);
		for (int32 k = 0; k < 4; ++k)
		{
			const float A = k * (PI * 0.5f) + PI * 0.25f;
			const FVector P = State.Center + FVector(FMath::Cos(A) * 640.0f, FMath::Sin(A) * 640.0f, 0.0f);
			Shape(TEXT("Cylinder"), P + FVector(0, 0, 200), FVector(16, 16, 400), FLinearColor(0.3f, 0.22f, 0.1f));
			Block(P + FVector(0, 0, 300), FVector(12, 120, 180), Colour, A * 180.0f / PI, false);
		}
	}
	else if (State.Role == TEXT("shrine"))
	{
		// A pillar of soft light, and the Warden standing by it.
		Shape(TEXT("Cylinder"), State.Center + FVector(0, 0, 3), FVector(520, 520, 4), Colour * 0.5f, FRotator::ZeroRotator, false);
		Shape(TEXT("Cylinder"), State.Center + FVector(0, 0, WallHeight * 0.5f), FVector(160, 160, WallHeight), Colour * 0.9f, FRotator::ZeroRotator, false);
		Light(State.Center + FVector(0, 0, 200), Colour, 12000.0f, 900.0f);
		const FVector WardenAt = State.Center + FVector(0, 330, 0);
		USkeletalMesh* Body = LoadObject<USkeletalMesh>(nullptr, TEXT("/Game/DCC/Characters/NPCs/NPC_office_311/SK_NPC_office_311.SK_NPC_office_311"));
		UAnimSequence* Idle = LoadObject<UAnimSequence>(nullptr, TEXT("/Game/DCC/Characters/NPCs/Anims/NPC_MM_Idle.NPC_MM_Idle"));
		if (Body)
		{
			if (AActor* Warden = GetWorld()->SpawnActor<AActor>(AActor::StaticClass(), FTransform(WardenAt)))
			{
				Warden->SetReplicates(false);
				USkeletalMeshComponent* Mesh = NewObject<USkeletalMeshComponent>(Warden, TEXT("Warden"));
				Warden->SetRootComponent(Mesh);
				Mesh->SetSkeletalMeshAsset(Body);
				Mesh->SetCollisionEnabled(ECollisionEnabled::NoCollision);
				Mesh->SetAnimationMode(EAnimationMode::AnimationSingleNode);
				// Facing the light (Forge bodies face +Y in their own space, hence the -90).
				Mesh->SetWorldLocationAndRotation(WardenAt, FRotator(0.0f, (State.Center - WardenAt).Rotation().Yaw - 90.0f, 0.0f));
				Mesh->SetWorldScale3D(FVector(1.05f));
				Mesh->RegisterComponent();
				if (Idle)
				{
					Mesh->PlayAnimation(Idle, true);
					Mesh->SetPlayRate(0.6f);
				}
				LocalActors.Add(Warden);
			}
		}
		Sign(WardenAt + FVector(0, 0, 240), -90.0f, TEXT("THE WARDEN"), FColor(170, 255, 210), 26.0f);
		Sign(WardenAt + FVector(0, 0, 214), -90.0f, TEXT("\"Rest. The gate can wait a minute.\""), FColor(220, 240, 230), 14.0f);
	}
	else if (State.Role == TEXT("ambush"))
	{
		// A break room nobody has used in a while: a table, crates, a dead lamp.
		Prop(TEXT("/Game/Cave_Camp/Meshes/Props_02/SM_table_group_01"), State.Center + FVector(-260, 160, 0), 20.0f, FVector(0, 0, 110), true);
		Prop(TEXT("/Game/Cave_Camp/Meshes/Props_02/SM_crate_group_02"), State.Center + FVector(300, -260, 0), 70.0f, FVector(0, 0, 120), true);
	}
	else if (State.Role == TEXT("gauntlet"))
	{
		// The room's floor as 2 m tiles (inside its walls), the far corner a green pad that ends it.
		State.TilesX = Room.W * 2 - 2;
		State.TilesY = Room.H * 2 - 2;
		State.TileMin = Min + FVector(TileSize, TileSize, 0.0f);
		State.Goal = State.TileMin + FVector((State.TilesX - 0.5f) * TileSize, (State.TilesY - 0.5f) * TileSize, 0.0f);
		for (int32 Y = 0; Y < State.TilesY; ++Y)
		{
			for (int32 X = 0; X < State.TilesX; ++X)
			{
				const FVector At = State.TileMin + FVector((X + 0.5f) * TileSize, (Y + 0.5f) * TileSize, 4.0f);
				if (FVector::Dist2D(At, State.Goal) < TileSize * 0.6f)
				{
					continue;
				}
				if (AActor* T = Block(At, FVector(TileSize - 14.0f, TileSize - 14.0f, 6.0f), FLinearColor(0.06f, 0.05f, 0.08f), 0.0f, false))
				{
					FTile Entry;
					Entry.Room = RoomIndex;
					Entry.X = X;
					Entry.Y = Y;
					Entry.Actor = T;
					Tiles.Add(Entry);
				}
			}
		}
		Block(State.Goal + FVector(0, 0, 5), FVector(TileSize - 14.0f, TileSize - 14.0f, 8.0f), FLinearColor(0.2f, 1.0f, 0.4f), 0.0f, false);
		Light(State.Goal + FVector(0, 0, 200), FLinearColor(0.3f, 1.0f, 0.5f), 5000.0f, 600.0f);
	}
	else if (State.Role == TEXT("toll"))
	{
		// A booth window and the ring you have to hold.
		Shape(TEXT("Cylinder"), State.Center + FVector(0, 0, 3), FVector(440, 440, 4), Colour * 0.7f, FRotator::ZeroRotator, false);
		Block(State.Center + FVector(0, 280, 90), FVector(280, 40, 180), FLinearColor(0.12f, 0.09f, 0.05f), 0.0f, true);
		Block(State.Center + FVector(0, 280, 200), FVector(300, 16, 24), Colour, 0.0f, false);
		Light(State.Center + FVector(0, 200, 220), Colour, 4000.0f, 500.0f);
	}
	else if (State.Role == TEXT("outage"))
	{
		// A dead break room. The emergency light is the only thing still on.
		Prop(TEXT("/Game/Cave_Camp/Meshes/Props_02/SM_crate_group_02"), State.Center + FVector(-280, 180, 0), 30.0f, FVector(0, 0, 120), true);
		Prop(TEXT("/Game/Cave_Camp/Meshes/Props_02/SM_table_group_01"), State.Center + FVector(220, -180, 0), -15.0f, FVector(0, 0, 110), true);
		Shape(TEXT("Cylinder"), State.Center + FVector(0, 0, 8), FVector(80, 80, 16), FLinearColor(0.8f, 0.1f, 0.08f), FRotator::ZeroRotator, false);
		Light(State.Center + FVector(0, 0, 160), FLinearColor(0.9f, 0.15f, 0.1f), 1800.0f, 700.0f);
	}
	else if (State.Role == TEXT("clock"))
	{
		// A pillar the waves are timed against, and a ring so the room reads as a clock.
		Shape(TEXT("Cylinder"), State.Center + FVector(0, 0, 3), FVector(520, 520, 4), Colour * 0.55f, FRotator::ZeroRotator, false);
		Shape(TEXT("Cylinder"), State.Center + FVector(0, 0, 160), FVector(70, 70, 320), FLinearColor(0.08f, 0.1f, 0.14f), FRotator::ZeroRotator, true);
		Light(State.Center + FVector(0, 0, 360), Colour, 5000.0f, 700.0f);
	}
	else if (State.Role == TEXT("pen"))
	{
		// A low fence. Inside is the fight. Outside, once it starts, the fence burns.
		Shape(TEXT("Cylinder"), State.Center + FVector(0, 0, 3), FVector(560, 560, 4), Colour * 0.45f, FRotator::ZeroRotator, false);
		for (int32 k = 0; k < 10; ++k)
		{
			const float A = k * (2.0f * PI / 10.0f);
			const FVector P = State.Center + FVector(FMath::Cos(A) * 300.0f, FMath::Sin(A) * 300.0f, 0.0f);
			Block(P + FVector(0, 0, 50), FVector(28, 28, 100), FLinearColor(0.25f, 0.12f, 0.06f), A * 180.0f / PI, true);
		}
		Light(State.Center + FVector(0, 0, 240), Colour, 4500.0f, 600.0f);
	}
}

void ADCCArcadeGate::SpawnShades(int32 RoomIndex, int32 Count)
{
	FRandomStream R(Seed * 53 + RoomIndex * 11 + Rooms[RoomIndex].Foes.Num());
	for (int32 i = 0; i < Count; ++i)
	{
		FVector At;
		if (FindRoomCell(RoomIndex, R, At))
		{
			SpawnFoe(ADCCEnemyCharacter_GateShade::StaticClass(), At, RoomIndex, false);
		}
	}
}

void ADCCArcadeGate::TickRoomsServer(double Now, const TArray<APlayerController*>& Players)
{
	// Who is in which room right now.
	TArray<TPair<APlayerController*, ADCCCharacterBase*>> InRoom;
	TArray<int32> RoomOf;
	for (APlayerController* PC : Players)
	{
		ADCCCharacterBase* Crawler = PC ? Cast<ADCCCharacterBase>(PC->GetPawn()) : nullptr;
		if (!Crawler || !Crawler->IsAlive() || !Grid)
		{
			continue;
		}
		const FIntPoint Cell = Grid->CellOf(Crawler->GetActorLocation());
		InRoom.Add({ PC, Crawler });
		RoomOf.Add(Grid->RoomIndexAt(Cell.X, Cell.Y));
	}

	bool bBossShown = false;
	for (int32 i = 0; i < Rooms.Num(); ++i)
	{
		FRoomState& State = Rooms[i];
		if (State.bCleared)
		{
			continue;
		}
		// The gauntlet: the green pad ends it; a burning tile hurts, twice a second at most.
		if (State.Role == TEXT("gauntlet") && State.TilesX > 0)
		{
			for (int32 p = 0; p < InRoom.Num(); ++p)
			{
				if (RoomOf[p] != i)
				{
					continue;
				}
				ADCCCharacterBase* Crawler = InRoom[p].Value;
				const FVector At = Crawler->GetActorLocation();
				if (FVector::Dist2D(At, State.Goal) < TileSize * 0.6f)
				{
					ClearRoom(i, TEXT("Gauntlet crossed"));
					break;
				}
				const int32 X = FMath::FloorToInt32((At.X - State.TileMin.X) / TileSize);
				const int32 Y = FMath::FloorToInt32((At.Y - State.TileMin.Y) / TileSize);
				if (X < 0 || Y < 0 || X >= State.TilesX || Y >= State.TilesY || TileState(i, X, Y, Now) != 2)
				{
					continue;
				}
				double& Next = NextBurnAt.FindOrAdd(InRoom[p].Key);
				if (Now >= Next && Crawler->Stats)
				{
					Next = Now + 0.5;
					Crawler->Stats->ApplyDamage(3 + RankIndex, EDCCDamageType::Fire);
				}
			}
			continue;
		}
		if (State.Role != TEXT("boss"))
		{
			continue;
		}
		// The Gatekeeper's hall.
		ADCCEnemyCharacter* Boss = State.Boss.Get();
		if (!Boss || !Boss->IsAlive() || !Boss->Stats)
		{
			continue;
		}
		TArray<ADCCCharacterBase*> Here;
		for (int32 p = 0; p < InRoom.Num(); ++p)
		{
			if (RoomOf[p] == i) { Here.Add(InRoom[p].Value); }
		}
		if (Here.IsEmpty())
		{
			continue;
		}
		const float Health = Boss->Stats->MaxHitPoints > 0 ? float(Boss->Stats->CurrentHitPoints) / Boss->Stats->MaxHitPoints : 0.0f;
		BossName = Boss->DisplayName.ToString();
		BossHealth = FMath::Clamp(Health, 0.0f, 1.0f);
		bBossShown = true;
		// Shades scale with the crawlers in the hall: one fewer than before when you are alone (1, then 2 more), the
		// old numbers for two, and one more per crawler past that (2026-11-17: solo crawlers never reached phase 2).
		const int32 ExtraCrawlers = Here.Num() - 2;
		if (State.Phase == 0 && Health <= 0.66f)
		{
			State.Phase = 1;
			SpawnShades(i, FMath::Max(1, 2 + RankIndex / 2 + ExtraCrawlers));
			Say(this, TEXT("The Gatekeeper calls the gate's shades out of the walls."));
		}
		else if (State.Phase == 1 && Health <= 0.33f)
		{
			State.Phase = 2;
			SpawnShades(i, FMath::Max(2, 3 + RankIndex / 2 + ExtraCrawlers));
			Say(this, TEXT("The Gatekeeper is losing. The whole gate is coming to help it. It slams faster."));
		}
		// Slams: a ring under someone, 1.6 s to get out of it, then it lands.
		if (State.NextSlamAt <= 0.0)
		{
			State.NextSlamAt = Now + 4.0;
		}
		if (bSlamResolved && Now >= State.NextSlamAt)
		{
			const ADCCCharacterBase* Target = Here[FMath::RandRange(0, Here.Num() - 1)];
			SlamCenter = FVector(Target->GetActorLocation().X, Target->GetActorLocation().Y, State.Center.Z);
			SlamRadius = 340.0f + 20.0f * RankIndex;
			SlamAt = Now + 1.6;
			bSlamResolved = false;
			UE_LOG(LogTemp, Log, TEXT("[DCC Gate] slam on %s, radius %.0f"), *Target->GetName(), SlamRadius);
			State.NextSlamAt = Now + (State.Phase == 0 ? 8.0 : State.Phase == 1 ? 6.5 : 5.0);
			ForceNetUpdate();
		}
	}
	// The slam lands on whoever is still inside the ring.
	if (!bSlamResolved && Now >= SlamAt)
	{
		bSlamResolved = true;
		int32 Hit = 0;
		for (const TPair<APlayerController*, ADCCCharacterBase*>& Pair : InRoom)
		{
			ADCCCharacterBase* Crawler = Pair.Value;
			if (FVector::Dist2D(Crawler->GetActorLocation(), SlamCenter) <= SlamRadius && Crawler->Stats)
			{
				Crawler->Stats->ApplyDamage(6 + 2 * RankIndex);
				Crawler->LaunchCharacter(FVector(0, 0, 560), false, true);
				++Hit;
			}
		}
		if (Hit == 0)
		{
			UE_LOG(LogTemp, Log, TEXT("[DCC Gate] slam missed everyone."));
		}
	}
	if (!bBossShown && BossHealth >= 0.0f)
	{
		BossHealth = -1.0f;
		BossName.Reset();
	}
}

void ADCCArcadeGate::BuildDungeon()
{
	bDungeonBuilt = true;
	if (!Grid)
	{
		Grid = NewObject<UDCCDungeonGrid>(this);
	}
	Grid->SetOrigin(DungeonOrigin(), Tile);
	if (!Grid->Build(GridSize, Seed, FMath::Max(1, RoomCount - 2), 0))
	{
		UE_LOG(LogTemp, Error, TEXT("[DCC Gate] The layout did not build (seed %d)."), Seed);
		return;
	}
	const TArray<UDCCDungeonGrid::FRoom>& GridRooms = Grid->GetRooms();
	const int32 Count = GridRooms.Num();

	Rooms.SetNum(Count);
	const FVector O = DungeonOrigin();
	const FLinearColor CorridorColour(0.05f, 0.05f, 0.07f);
	const FLinearColor WallColour(0.04f, 0.035f, 0.06f);

	// Floors: one slab per corridor cell run, one per room in the room's colour.
	for (int32 Y = 0; Y < GridSize; ++Y)
	{
		for (int32 X = 0; X < GridSize; ++X)
		{
			if (Grid->IsOpen(X, Y) && Grid->RoomIndexAt(X, Y) == -1)
			{
				Paint(Block(O + FVector((X + 0.5f) * Tile, (Y + 0.5f) * Tile, -20.0f), FVector(Tile, Tile, 40.0f), CorridorColour),
					TEXT("/Game/Ice_Station/Materials/MI_floor_tile2"));
			}
		}
	}
	for (int32 i = 0; i < Count; ++i)
	{
		const UDCCDungeonGrid::FRoom& Room = GridRooms[i];
		const FName RoomRole = RoleFor(Seed, i, Count);
		FRoomState& State = Rooms[i];
		State.Role = RoomRole;
		State.Center = Grid->CellCenter(Room.Center());
		const FLinearColor Colour = RoleColour(RoomRole);
		const TCHAR* FloorMat = TEXT("/Game/Ice_Station/Materials/MI_floor_tile");
		if (RoomRole == TEXT("boss")) { FloorMat = TEXT("/Game/Ice_Station/Materials/MI_floor_tile4"); }
		else if (RoomRole == TEXT("duel")) { FloorMat = TEXT("/Game/Abandonned_Brutalist/Materials/MI_stones"); }
		else if (RoomRole == TEXT("shrine")) { FloorMat = TEXT("/Game/Ice_Station/Materials/MI_floor_tile1"); }
		else if (RoomRole == TEXT("gauntlet")) { FloorMat = TEXT("/Game/Ice_Station/Materials/MI_floor_tile3"); }
		else if (RoomRole == TEXT("ambush") || RoomRole == TEXT("toll")) { FloorMat = TEXT("/Game/Abandonned_Brutalist/Materials/MI_concrete_tile"); }
		else if (RoomRole == TEXT("outage")) { FloorMat = TEXT("/Game/Abandonned_Brutalist/Materials/MI_concrete_01"); }
		else if (RoomRole == TEXT("clock")) { FloorMat = TEXT("/Game/Ice_Station/Materials/MI_floor_tile3"); }
		else if (RoomRole == TEXT("pen")) { FloorMat = TEXT("/Game/Abandonned_Brutalist/Materials/MI_stones"); }
		TiledBlock(O + FVector((Room.X + Room.W * 0.5f) * Tile, (Room.Y + Room.H * 0.5f) * Tile, -20.0f),
			FVector(Room.W * Tile, Room.H * Tile, 40.0f), FLinearColor(0.07f, 0.06f, 0.09f) + Colour * 0.04f, FloorMat, Tile);
		// A glowing inlay round the room's middle, so a room reads as a room from the doorway.
		Block(State.Center + FVector(0, 0, 2), FVector(Room.W * Tile - 500.0f, 30.0f, 4.0f), Colour * 0.8f, 0.0f, false);
		Block(State.Center + FVector(0, 0, 2), FVector(30.0f, Room.H * Tile - 500.0f, 4.0f), Colour * 0.8f, 0.0f, false);
		if (RoomRole == TEXT("outage"))
		{
			Light(State.Center + FVector(0, 0, 460), FLinearColor(0.45f, 0.08f, 0.06f), 1400.0f, FMath::Max(Room.W, Room.H) * Tile);
		}
		else
		{
			// Half white and much softer: at 9000 the room's colour flooded the floor flat and hid its texture (2026-11-21).
			Light(State.Center + FVector(0, 0, 460), FMath::Lerp(Colour, FLinearColor::White, 0.5f), 3200.0f, FMath::Max(Room.W, Room.H) * Tile);
		}
		Sign(State.Center + FVector(0, 0, 420), 0.0f, RoleSign(RoomRole), Colour.ToFColor(true), RoomRole == TEXT("boss") ? 60.0f : 34.0f);
		if (RoomRole == TEXT("trial"))
		{
			Shape(TEXT("Cylinder"), State.Center + FVector(0, 0, 4), FVector(420, 420, 8), Colour, FRotator::ZeroRotator, false);
		}
		DressRoom(i);
	}

	// Walls: merged runs along every open cell's closed sides (the World Dungeon's way, ADCCTaleFloor::SpawnWalls).
	struct FRun { int32 Line; int32 A; int32 B; };
	TArray<FRun> Horizontal, Vertical;
	auto AddRun = [](TArray<FRun>& Runs, int32 Line, int32 A)
	{
		for (FRun& Run : Runs)
		{
			if (Run.Line == Line && Run.B == A) { Run.B = A + 1; return; }
		}
		Runs.Add({ Line, A, A + 1 });
	};
	for (int32 Y = 0; Y < GridSize; ++Y)
	{
		for (int32 X = 0; X < GridSize; ++X)
		{
			if (!Grid->IsOpen(X, Y)) { continue; }
			if (!Grid->IsOpen(X, Y + 1)) { AddRun(Horizontal, Y + 1, X); }
			if (!Grid->IsOpen(X, Y - 1)) { AddRun(Horizontal, Y, X); }
			if (!Grid->IsOpen(X + 1, Y)) { AddRun(Vertical, X + 1, Y); }
			if (!Grid->IsOpen(X - 1, Y)) { AddRun(Vertical, X, Y); }
		}
	}
	for (const FRun& Run : Horizontal)
	{
		const float X0 = O.X + Run.A * Tile, X1 = O.X + Run.B * Tile, Yw = O.Y + Run.Line * Tile;
		// One panel per cell, so the trim repeats instead of stretching down the whole run.
		for (int32 Cell = Run.A; Cell < Run.B; ++Cell)
		{
			const float L = (Cell == Run.A ? 20.0f : 0.0f), R = (Cell == Run.B - 1 ? 20.0f : 0.0f);
			const float C0 = O.X + Cell * Tile - L, C1 = O.X + (Cell + 1) * Tile + R;
			Paint(Block(FVector((C0 + C1) * 0.5f, Yw, WallHeight * 0.5f), FVector(C1 - C0, 40.0f, WallHeight), WallColour),
				TEXT("/Game/Ice_Station/Materials/MI_trim_01_interior"));
		}
		(void)X0;
		(void)X1;
	}
	for (const FRun& Run : Vertical)
	{
		const float Y0 = O.Y + Run.A * Tile, Y1 = O.Y + Run.B * Tile, Xw = O.X + Run.Line * Tile;
		for (int32 Cell = Run.A; Cell < Run.B; ++Cell)
		{
			const float L = (Cell == Run.A ? 20.0f : 0.0f), R = (Cell == Run.B - 1 ? 20.0f : 0.0f);
			const float C0 = O.Y + Cell * Tile - L, C1 = O.Y + (Cell + 1) * Tile + R;
			Paint(Block(FVector(Xw, (C0 + C1) * 0.5f, WallHeight * 0.5f), FVector(40.0f, C1 - C0, WallHeight), WallColour),
				TEXT("/Game/Ice_Station/Materials/MI_trim_01_interior2"));
		}
		(void)Y0;
		(void)Y1;
	}
	// Inside a gate there is no sky: a lid over the whole layout, so the rooms are lit only by their own glow.
	// Plain and dark: painted, it showed one huge stretched stone overhead (2026-11-21).
	Block(O + FVector(GridSize * Tile * 0.5f, GridSize * Tile * 0.5f, WallHeight + 20.0f), FVector(GridSize * Tile + 400.0f, GridSize * Tile + 400.0f, 40.0f),
		FLinearColor(0.02f, 0.015f, 0.04f));
	// A thin violet glow along the foot of every wall.
	for (const FRun& Run : Horizontal)
	{
		const float X0 = O.X + Run.A * Tile, X1 = O.X + Run.B * Tile, Yw = O.Y + Run.Line * Tile;
		if (AActor* Strip = Block(FVector((X0 + X1) * 0.5f, Yw, 24.0f), FVector(X1 - X0, 56.0f, 10.0f), Violet, 0.0f, false))
		{
			Paint(Strip, TEXT("/Game/Synthwave_city/Material/MI_neon_1"));
		}
	}
	for (const FRun& Run : Vertical)
	{
		const float Y0 = O.Y + Run.A * Tile, Y1 = O.Y + Run.B * Tile, Xw = O.X + Run.Line * Tile;
		if (AActor* Strip = Block(FVector(Xw, (Y0 + Y1) * 0.5f, 24.0f), FVector(56.0f, Y1 - Y0, 10.0f), Violet, 0.0f, false))
		{
			Paint(Strip, TEXT("/Game/Synthwave_city/Material/MI_neon_1"));
		}
	}
	FString Roles;
	for (const FRoomState& State : Rooms)
	{
		if (!Roles.IsEmpty()) { Roles += TEXT(" "); }
		Roles += State.Role.ToString();
	}
	// The way home stands in the Gatekeeper's hall for the whole run. Dark until that room is clear.
	if (Rooms.Num() > 0)
	{
		const FVector ExitAt = Rooms.Last().Center;
		DrawPortal(ExitAt, 700.0f, GateCyan, 0.0f, true, false);
		Sign(ExitAt + FVector(0.0f, -420.0f, 240.0f), -90.0f, TEXT("WAY HOME"), FColor(150, 140, 170), 32.0f);
	}
	UE_LOG(LogTemp, Log, TEXT("[DCC Gate] Rank %s gate, %d rooms (seed %d): %s"), RankName(RankFor(Count)), Count, Seed, *Roles);
}

// ---- the run (server) ---------------------------------------------------------------------------------------------

void ADCCArcadeGate::ServerBegin()
{
	Title = TEXT("GATE RUN");
	Accent = Violet;
	OpensAt = ServerTime() + CountdownSeconds;
	BuildDungeon();
	RoomCount = Rooms.Num(); // what the grid could actually place
	RankIndex = RankFor(RoomCount);
	EndsAt = OpensAt + 150.0 + 45.0 * RoomCount;
	SpawnRoomContent();
	RefreshStatus();
	Say(this, FString::Printf(TEXT("A rank %s gate has opened over the plaza. %d rooms. It will not stay open forever."), RankName(RankIndex), RoomCount));
}

bool ADCCArcadeGate::FindRoomCell(int32 RoomIndex, FRandomStream& R, FVector& Out) const
{
	if (!Grid || !Grid->GetRooms().IsValidIndex(RoomIndex))
	{
		return false;
	}
	const UDCCDungeonGrid::FRoom& Room = Grid->GetRooms()[RoomIndex];
	for (int32 Try = 0; Try < 12; ++Try)
	{
		const FIntPoint Cell(R.RandRange(Room.X + 1, Room.X + Room.W - 2), R.RandRange(Room.Y + 1, Room.Y + Room.H - 2));
		if (Grid->IsOpen(Cell.X, Cell.Y) && Cell != Room.Center())
		{
			Out = Grid->CellCenter(Cell);
			return true;
		}
	}
	Out = Rooms[RoomIndex].Center + FVector(150, 150, 0);
	return true;
}

ADCCEnemyCharacter* ADCCArcadeGate::SpawnFoe(TSubclassOf<ADCCEnemyCharacter> Class, const FVector& At, int32 RoomIndex, bool bBoss)
{
	UWorld* World = GetWorld();
	if (!World || !Class)
	{
		return nullptr;
	}
	const FTransform Transform(FRotator(0.0f, FMath::FRand() * 360.0f, 0.0f), At + FVector(0, 0, 120.0f));
	ADCCEnemyCharacter* Foe = World->SpawnActorDeferred<ADCCEnemyCharacter>(Class, Transform, nullptr, nullptr,
		ESpawnActorCollisionHandlingMethod::AdjustIfPossibleButAlwaysSpawn);
	if (!Foe)
	{
		return nullptr;
	}
	Foe->SetChaseGrid(Grid);
	Foe->LeashRadius = FMath::Max(Foe->LeashRadius, 6000.0f);
	if (Foe->Stats)
	{
		// The rank is the threat: levels and body stats climb with it, the boss most of all.
		const int32 Threat = RankIndex + (bBoss ? 2 + RankIndex : 0);
		Foe->Stats->Level = FMath::Max(Foe->Stats->Level, 1 + Threat);
		for (const EDCCAbility Ability : { EDCCAbility::Strength, EDCCAbility::Constitution, EDCCAbility::Dexterity })
		{
			Foe->Stats->AbilityScores.SetScore(Ability, Foe->Stats->AbilityScores.GetScore(Ability) + Threat / 2 + (bBoss ? 2 : 0));
		}
	}
	if (bBoss)
	{
		Foe->DisplayName = FText::FromString(FString::Printf(TEXT("The Gatekeeper (rank %s)"), RankName(RankIndex)));
	}
	Foe->FinishSpawning(Transform);
	if (Foe->Stats)
	{
		Foe->Stats->RefreshDerivedStats(true);
	}
	if (UCharacterMovementComponent* Move = Foe->GetCharacterMovement())
	{
		Move->bUseRVOAvoidance = true;
		Move->AvoidanceConsiderationRadius = 220.0f;
	}
	if (bBoss)
	{
		Foe->SetActorScale3D(FVector(1.0f + 0.08f * RankIndex));
	}
	Foe->OnCharacterDied.AddUniqueDynamic(this, &ADCCArcadeGate::OnFoeDied);
	if (Rooms.IsValidIndex(RoomIndex))
	{
		Rooms[RoomIndex].Foes.Add(Foe);
	}
	return Foe;
}

void ADCCArcadeGate::SpawnWave(int32 RoomIndex, int32 Count)
{
	static const TArray<TSubclassOf<ADCCEnemyCharacter>> Pool = {
		ADCCEnemyCharacter_DebtEater::StaticClass(), ADCCEnemyCharacter_Beholder::StaticClass(), ADCCEnemyCharacter_Thornback::StaticClass(),
		ADCCEnemyCharacter_BreakRoomMold::StaticClass(), ADCCEnemyCharacter_ShellCompany::StaticClass(), ADCCEnemyCharacter_Intern::StaticClass(),
		ADCCEnemyCharacter_Notary::StaticClass(), ADCCEnemyCharacter_EscalationSpecialist::StaticClass(), ADCCEnemyCharacter_FinePrint::StaticClass(),
		ADCCEnemyCharacter_UnclaimedProperty::StaticClass(), ADCCEnemyCharacter_Arbiter::StaticClass(), ADCCEnemyCharacter_SurveyDrone::StaticClass(),
		ADCCEnemy_NuclearDad::StaticClass(),
	};
	FRandomStream R(Seed * 31 + RoomIndex * 101 + Rooms[RoomIndex].Foes.Num());
	for (int32 i = 0; i < Count; ++i)
	{
		FVector At;
		if (FindRoomCell(RoomIndex, R, At))
		{
			SpawnFoe(Pool[R.RandRange(0, Pool.Num() - 1)], At, RoomIndex, false);
		}
	}
}

void ADCCArcadeGate::SpawnChest(const FVector& At, int32 Tier)
{
	static const TArray<TSubclassOf<ADCCLootContainer>> Chests = {
		ADCCLootContainer_KitchenDrawer::StaticClass(), ADCCLootContainer_Armory::StaticClass(),
		ADCCLootContainer_Quartermaster::StaticClass(), ADCCLootContainer_ManagementLockbox::StaticClass(),
	};
	const FTransform Transform(FRotator(0.0f, 90.0f, 0.0f), At + FVector(0, 0, 40.0f));
	if (ADCCLootContainer* Chest = GetWorld()->SpawnActorDeferred<ADCCLootContainer>(Chests[FMath::Clamp(Tier, 0, Chests.Num() - 1)], Transform,
		nullptr, nullptr, ESpawnActorCollisionHandlingMethod::AdjustIfPossibleButAlwaysSpawn))
	{
		Chest->FinishSpawning(Transform);
	}
}

void ADCCArcadeGate::SpawnVault(int32 RoomIndex, bool bPlates)
{
	UWorld* World = GetWorld();
	const UDCCDungeonGrid::FRoom& Room = Grid->GetRooms()[RoomIndex];
	FRoomState& State = Rooms[RoomIndex];
	// The vault door stands in one corner; the prize appears behind where it stood.
	const FIntPoint Corner(Room.X + 1, Room.Y + 1);
	State.Prize = Grid->CellCenter(Corner);
	const FTransform DoorAt(FRotator(0.0f, 45.0f, 0.0f), State.Prize);
	ADCCPuzzleDoor* Door = World->SpawnActorDeferred<ADCCPuzzleDoor>(ADCCPuzzleDoor::StaticClass(), DoorAt, nullptr, nullptr,
		ESpawnActorCollisionHandlingMethod::AlwaysSpawn);
	if (!Door)
	{
		return;
	}
	Door->LockedDescription = FText::FromString(bPlates
		? TEXT("Sealed. The plate in the middle wants 140 kg: two crawlers, or one and the keg. Or force it (E), for Favor.")
		: TEXT("Sealed. Turn every breaker on at once. Each switch flips its neighbours. Or force it (E), for Favor."));
	Door->SolvedAnnouncement = FText::FromString(TEXT("Vault open. Somebody read the instructions. The audience is stunned."));
	Door->ForcedAnnouncement = FText::FromString(TEXT("Vault forced. Unprofessional. Ratings are up."));
	Door->FinishSpawning(DoorAt);
	Door->OnDoorOpened.AddUniqueDynamic(this, &ADCCArcadeGate::OnVaultOpened);
	State.Vault = Door;

	FRandomStream R(Seed * 17 + RoomIndex);
	if (bPlates)
	{
		const FTransform PlateAt(FRotator::ZeroRotator, State.Center + FVector(0, 0, 2));
		if (ADCCPressurePlate* Plate = World->SpawnActorDeferred<ADCCPressurePlate>(ADCCPressurePlate::StaticClass(), PlateAt))
		{
			Plate->RequiredMassKilograms = 140.0f;
			Plate->LinkedDoor = Door;
			Plate->FinishSpawning(PlateAt);
		}
		FVector KegAt;
		FindRoomCell(RoomIndex, R, KegAt);
		const FTransform KegT(FRotator::ZeroRotator, KegAt + FVector(0, 0, 60));
		World->SpawnActor<ADCCPickup_Keg>(ADCCPickup_Keg::StaticClass(), KegT);
	}
	else
	{
		// A row of breakers. Pressing one flips it and its neighbours (Lights Out). Start from all-on and press a
		// few at random, so the start is always solvable by pressing those again.
		const int32 Count = RankIndex >= 3 ? 4 : 3;
		TArray<bool> On;
		On.Init(true, Count);
		const int32 Presses = R.RandRange(1, Count - 1);
		for (int32 p = 0; p < Presses; ++p)
		{
			const int32 i = R.RandRange(0, Count - 1);
			for (int32 k = FMath::Max(0, i - 1); k <= FMath::Min(Count - 1, i + 1); ++k) { On[k] = !On[k]; }
		}
		bool bAllOn = true;
		for (bool b : On) { bAllOn &= b; }
		if (bAllOn) { On[0] = !On[0]; if (Count > 1) { On[1] = !On[1]; } } // never start solved

		TArray<ADCCBreakerSwitch*> Switches;
		const FName Panel(*FString::Printf(TEXT("Gate%d_%d"), Seed, RoomIndex));
		for (int32 i = 0; i < Count; ++i)
		{
			const FTransform At(FRotator(0.0f, 90.0f, 0.0f), State.Center + FVector((i - (Count - 1) * 0.5f) * 220.0f, 380.0f, 120.0f));
			ADCCBreakerSwitch* Switch = World->SpawnActorDeferred<ADCCBreakerSwitch>(ADCCBreakerSwitch::StaticClass(), At);
			if (!Switch) { continue; }
			Switch->PanelId = Panel;
			Switch->bStartsOn = On[i];
			Switch->LinkedDoor = Door;
			Switches.Add(Switch);
		}
		for (int32 i = 0; i < Switches.Num(); ++i)
		{
			if (i > 0) { Switches[i]->Neighbours.Add(Switches[i - 1]); }
			if (i + 1 < Switches.Num()) { Switches[i]->Neighbours.Add(Switches[i + 1]); }
		}
		for (int32 i = 0; i < Switches.Num(); ++i)
		{
			Switches[i]->FinishSpawning(FTransform(FRotator(0.0f, 90.0f, 0.0f), State.Center + FVector((i - (Count - 1) * 0.5f) * 220.0f, 380.0f, 120.0f)));
		}
	}
}

void ADCCArcadeGate::SpawnRoomContent()
{
	TArray<APlayerController*> Players;
	GetPlayers(Players);
	const int32 Extra = FMath::Max(0, Players.Num() - 1) / 2;
	for (int32 i = 0; i < Rooms.Num(); ++i)
	{
		FRoomState& State = Rooms[i];
		if (State.Role == TEXT("fight"))
		{
			SpawnWave(i, FMath::Min(2 + RankIndex / 2 + Extra, 7));
		}
		else if (State.Role == TEXT("treasure"))
		{
			SpawnChest(State.Center, FMath::Min(1 + RankIndex / 2, 3));
			SpawnWave(i, 1 + RankIndex / 3);
		}
		else if (State.Role == TEXT("vault"))
		{
			SpawnVault(i, ((Seed + i) % 2) == 0);
		}
		else if (State.Role == TEXT("trial") || State.Role == TEXT("clock"))
		{
			State.WavesLeft = 3;
		}
		else if (State.Role == TEXT("outage"))
		{
			State.WavesLeft = 2;
		}
		else if (State.Role == TEXT("boss"))
		{
			// The Gatekeeper, every rank (2026-11-15): its own body and its own fight. Rank scales it (SpawnFoe).
			State.Boss = SpawnFoe(ADCCEnemyCharacter_Gatekeeper::StaticClass(), State.Center + FVector(250, 0, 0), i, true);
			State.NextSlamAt = 0.0;
			SpawnShades(i, RankIndex / 2 + Extra); // more come at two-thirds and one-third
		}
		else if (State.Role == TEXT("duel"))
		{
			if (ADCCEnemyCharacter* Champion = SpawnFoe(ADCCEnemyCharacter_GateChampion::StaticClass(), State.Center + FVector(200, 0, 0), i, false))
			{
				Champion->DisplayName = FText::FromString(ADCCEnemyCharacter_GateChampion::RollName(Seed * 7 + i));
				if (Champion->Stats)
				{
					Champion->Stats->Level += 1;
					Champion->Stats->RefreshDerivedStats(true);
				}
			}
		}
		else if (State.Role == TEXT("start"))
		{
			State.bCleared = true;
		}
	}
}

void ADCCArcadeGate::OnFoeDied(ADCCCharacterBase* Character)
{
	// Rooms are checked in ServerTick. Kills pay a ticket each, to the whole party, in batches (2026-11-15:
	// "clearing the gates, missions, farming enemies, etc. should also add tickets").
	if (!bWrapped)
	{
		++PendingKills;
	}
}

void ADCCArcadeGate::PayParty(int32 Tickets, const FString& Why)
{
	if (Tickets <= 0)
	{
		return;
	}
	TArray<APlayerController*> Players;
	GetPlayers(Players);
	for (APlayerController* PC : Players)
	{
		if (PC && PC->PlayerState && (Entered.Contains(PC) || Entered.IsEmpty()))
		{
			UDCCWalletComponent::AwardTickets(PC->PlayerState, Tickets, Why);
		}
	}
}

void ADCCArcadeGate::OnVaultOpened(ADCCPuzzleDoor* Door, EDCCPuzzleSolveMethod Method)
{
	for (int32 i = 0; i < Rooms.Num(); ++i)
	{
		FRoomState& State = Rooms[i];
		if (State.Vault.Get() == Door && !State.bCleared)
		{
			SpawnChest(State.Prize, FMath::Min(1 + RankIndex / 2, 3));
			// A solved vault is worth more than a forced one.
			Score += Method == EDCCPuzzleSolveMethod::Solved ? 60 : 0;
			ClearRoom(i, Method == EDCCPuzzleSolveMethod::Solved ? TEXT("Vault solved") : TEXT("Vault forced"));
		}
	}
}

void ADCCArcadeGate::ClearRoom(int32 RoomIndex, const TCHAR* Why)
{
	FRoomState& State = Rooms[RoomIndex];
	if (State.bCleared)
	{
		return;
	}
	State.bCleared = true;
	Score += RolePoints(State.Role);
	// Every room pays as it falls, not just the end of the run.
	{
		const int32 Base = State.Role == TEXT("boss") ? 12
			: (State.Role == TEXT("trial") || State.Role == TEXT("clock")) ? 6
			: (State.Role == TEXT("duel") || State.Role == TEXT("gauntlet") || State.Role == TEXT("pen")) ? 5
			: (State.Role == TEXT("vault") || State.Role == TEXT("toll") || State.Role == TEXT("outage")) ? 4
			: (State.Role == TEXT("fight") || State.Role == TEXT("ambush")) ? 3
			: State.Role == TEXT("shrine") ? 1 : 2;
		PayParty(Base + RankIndex, FString::Printf(TEXT("gate %s"), *State.Role.ToString()));
	}
	if (State.Role == TEXT("trial"))
	{
		SpawnChest(State.Center, FMath::Min(1 + RankIndex / 2, 3));
	}
	if (State.Role == TEXT("duel"))
	{
		SpawnChest(State.Center, FMath::Min(1 + RankIndex / 2, 3));
	}
	if (State.Role == TEXT("boss"))
	{
		BossName.Reset();
		BossHealth = -1.0f;
		SlamAt = 0.0;
		bExitOpen = true;
		ForceNetUpdate();
		Say(this, TEXT("The Gatekeeper is down. The way home is open in its room. Step through."));
		return;
	}
	Say(this, FString::Printf(TEXT("%s. +%d."), Why, RolePoints(State.Role)));
}

void ADCCArcadeGate::SendIn(APawn* Pawn, int32 Slot)
{
	if (!Pawn || Rooms.IsEmpty())
	{
		return;
	}
	const FVector Spot = Rooms[0].Center + FVector(FMath::Cos(Slot * 1.3f) * 220.0f, FMath::Sin(Slot * 1.3f) * 220.0f, 120.0f);
	Pawn->TeleportTo(Spot, FRotator(0.0f, Pawn->GetActorRotation().Yaw, 0.0f));
}

void ADCCArcadeGate::DebugEnterAll(int32 RoomIndex)
{
	if (!HasAuthority() || Rooms.IsEmpty())
	{
		return;
	}
	bGateOpen = true;
	OpensAt = FMath::Min(OpensAt, ServerTime());
	TArray<APlayerController*> Players;
	GetPlayers(Players);
	for (APlayerController* PC : Players)
	{
		Entered.Add(PC);
		if (APawn* Pawn = PC->GetPawn())
		{
			const int32 Index = FMath::Clamp(RoomIndex, 0, Rooms.Num() - 1);
			const FVector Spot = Rooms[Index].Center + FVector(-180, 0, 120);
			if (!Pawn->TeleportTo(Spot, FRotator::ZeroRotator))
			{
				Pawn->SetActorLocation(Spot, false, nullptr, ETeleportType::TeleportPhysics);
			}
			PC->SetControlRotation(FRotator(-12.0f, 0.0f, 0.0f)); // facing +X, where the room's centre (and a boss) is
			UE_LOG(LogTemp, Log, TEXT("[DCC Gate] dev in, room %d, crawler at %s"), Index, *Pawn->GetActorLocation().ToString());
		}
	}
	for (int32 i = 0; i < Rooms.Num(); ++i)
	{
		UE_LOG(LogTemp, Log, TEXT("[DCC Gate] room %d %s at %s"), i, *Rooms[i].Role.ToString(), *Rooms[i].Center.ToString());
	}
}

void ADCCArcadeGate::DebugClearRoom(int32 RoomIndex)
{
	UE_LOG(LogTemp, Log, TEXT("[DCC Gate] dev clear, room %d."), RoomIndex);
	for (int32 i = 0; i < Rooms.Num(); ++i)
	{
		if (RoomIndex >= 0 && i != RoomIndex)
		{
			continue;
		}
		for (const TWeakObjectPtr<ADCCEnemyCharacter>& Foe : Rooms[i].Foes)
		{
			if (Foe.IsValid() && Foe->Stats && Foe->IsAlive())
			{
				Foe->Stats->ApplyDamage(100000);
			}
		}
		if (ADCCPuzzleDoor* Door = Rooms[i].Vault.Get())
		{
			Door->Unlock();
		}
		if ((Rooms[i].Role == TEXT("trial") || Rooms[i].Role == TEXT("clock") || Rooms[i].Role == TEXT("outage")) && !Rooms[i].bStarted)
		{
			Rooms[i].bStarted = true;
			Rooms[i].WavesLeft = 0;
		}
		if (Rooms[i].Role == TEXT("toll"))
		{
			Rooms[i].bStarted = true;
			Rooms[i].Held = 100.0;
		}
		if (Rooms[i].Role == TEXT("pen"))
		{
			Rooms[i].bStarted = true;
		}
	}
}

void ADCCArcadeGate::OnRespawned(APlayerController* PC)
{
	// Back in at the gate's entry room: the gate does not throw you out for dying, it just costs you the walk.
	if (bGateOpen && !bWrapped && PC && Entered.Contains(PC))
	{
		SendIn(PC->GetPawn(), Entered.Num());
	}
}

void ADCCArcadeGate::ServerTick(float DeltaSeconds)
{
	const double Now = ServerTime();
	Title = FString::Printf(TEXT("GATE RUN  -  RANK %s"), RankName(RankIndex));
	Accent = Violet;
	if (bWrapped)
	{
		return;
	}
	if (!bGateOpen && Now >= OpensAt)
	{
		bGateOpen = true;
		Say(this, TEXT("The gate is open. Walk into it."));
	}

	TArray<APlayerController*> Players;
	GetPlayers(Players);
	for (APlayerController* PC : Players)
	{
		ADCCCharacterBase* Crawler = PC ? Cast<ADCCCharacterBase>(PC->GetPawn()) : nullptr;
		if (!Crawler || !Crawler->IsAlive())
		{
			continue;
		}
		const FVector At = Crawler->GetActorLocation();
		// Into the gate.
		if (bGateOpen && FVector::Dist2D(At, EntryPortal()) < 320.0f && At.Z < 900.0f)
		{
			Entered.Add(PC);
			SendIn(Crawler, Entered.Num());
			continue;
		}
		if (!Grid || Rooms.IsEmpty())
		{
			continue;
		}
		const FIntPoint Cell = Grid->CellOf(At);
		const int32 RoomIndex = Grid->RoomIndexAt(Cell.X, Cell.Y);
		// A trial starts when someone steps into its ring.
		if (Rooms.IsValidIndex(RoomIndex) && Rooms[RoomIndex].Role == TEXT("trial") && !Rooms[RoomIndex].bStarted
			&& FVector::Dist2D(At, Rooms[RoomIndex].Center) < 260.0f)
		{
			Rooms[RoomIndex].bStarted = true;
			Rooms[RoomIndex].NextWaveAt = Now;
			Say(this, TEXT("Trial started. Three waves. Stay alive."));
		}
		// A quiet room is quiet until someone reaches the middle of it.
		if (Rooms.IsValidIndex(RoomIndex) && Rooms[RoomIndex].Role == TEXT("ambush") && !Rooms[RoomIndex].bStarted
			&& FVector::Dist2D(At, Rooms[RoomIndex].Center) < 320.0f)
		{
			Rooms[RoomIndex].bStarted = true;
			SpawnShades(RoomIndex, FMath::Min(3 + RankIndex / 2 + FMath::Max(0, Players.Num() - 1) / 2, 8));
			Say(this, TEXT("It was too quiet. The walls were never empty."));
		}
		// The clock starts the moment someone is in the room. The waves do not wait.
		if (Rooms.IsValidIndex(RoomIndex) && Rooms[RoomIndex].Role == TEXT("clock") && !Rooms[RoomIndex].bStarted)
		{
			Rooms[RoomIndex].bStarted = true;
			Rooms[RoomIndex].NextWaveAt = Now;
			Say(this, TEXT("The clock starts. Three waves. They do not wait for you to finish."));
		}
		// The outage waits until someone reaches the middle, then the lights go.
		if (Rooms.IsValidIndex(RoomIndex) && Rooms[RoomIndex].Role == TEXT("outage") && !Rooms[RoomIndex].bStarted
			&& FVector::Dist2D(At, Rooms[RoomIndex].Center) < 320.0f)
		{
			Rooms[RoomIndex].bStarted = true;
			Rooms[RoomIndex].NextWaveAt = Now;
			Say(this, TEXT("The lights go. Something in the walls was waiting for that."));
		}
		// The pen shuts when someone steps into the ring.
		if (Rooms.IsValidIndex(RoomIndex) && Rooms[RoomIndex].Role == TEXT("pen") && !Rooms[RoomIndex].bStarted
			&& FVector::Dist2D(At, Rooms[RoomIndex].Center) < 260.0f)
		{
			Rooms[RoomIndex].bStarted = true;
			SpawnWave(RoomIndex, FMath::Min(3 + RankIndex / 2 + FMath::Max(0, Players.Num() - 1) / 2, 7));
			Say(this, TEXT("The pen is shut. Stay inside the ring. The fence is worse."));
		}
		// The shrine: the Warden's light heals everyone who came into the gate, once.
		if (Rooms.IsValidIndex(RoomIndex) && Rooms[RoomIndex].Role == TEXT("shrine") && !Rooms[RoomIndex].bCleared
			&& FVector::Dist2D(At, Rooms[RoomIndex].Center) < 260.0f)
		{
			for (APlayerController* Other : Players)
			{
				ADCCCharacterBase* Mate = Other ? Cast<ADCCCharacterBase>(Other->GetPawn()) : nullptr;
				if (Mate && Mate->IsAlive() && Mate->Stats && (Entered.Contains(Other) || Entered.IsEmpty()))
				{
					Mate->Stats->Heal(Mate->Stats->MaxHitPoints);
					Mate->Stats->RecoverStamina(Mate->Stats->MaxStamina);
				}
			}
			Say(this, TEXT("The Warden: \"Rest. The gate can wait a minute. It has waited longer.\" Everyone is patched up."));
			ClearRoom(RoomIndex, TEXT("Rested at the shrine"));
		}
		// Home, through the exit.
		if (bExitOpen && FVector::Dist2D(At, Rooms.Last().Center) < 300.0f)
		{
			Wrap(true);
			return;
		}
	}

	// The toll: one person in the window keeps the count. Leaving pauses it.
	for (int32 i = 0; i < Rooms.Num(); ++i)
	{
		FRoomState& State = Rooms[i];
		if (State.bCleared || State.Role != TEXT("toll") || !Grid)
		{
			continue;
		}
		bool bHolding = false;
		for (APlayerController* PC : Players)
		{
			ADCCCharacterBase* Crawler = PC ? Cast<ADCCCharacterBase>(PC->GetPawn()) : nullptr;
			if (!Crawler || !Crawler->IsAlive())
			{
				continue;
			}
			const FIntPoint Cell = Grid->CellOf(Crawler->GetActorLocation());
			if (Grid->RoomIndexAt(Cell.X, Cell.Y) == i && FVector::Dist2D(Crawler->GetActorLocation(), State.Center) < 220.0f)
			{
				bHolding = true;
				break;
			}
		}
		if (!bHolding)
		{
			continue;
		}
		if (!State.bStarted)
		{
			State.bStarted = true;
			SpawnWave(i, FMath::Min(2 + RankIndex / 2 + FMath::Max(0, Players.Num() - 1) / 2, 6));
			Say(this, TEXT("The booth is open. Hold the window. The line is already mad."));
		}
		State.Held += DeltaSeconds;
	}

	// Trial waves, and rooms that have run out of foes.
	for (int32 i = 0; i < Rooms.Num(); ++i)
	{
		FRoomState& State = Rooms[i];
		if (State.bCleared)
		{
			continue;
		}
		const bool bTimedWaves = State.Role == TEXT("trial") || State.Role == TEXT("clock") || State.Role == TEXT("outage");
		if (bTimedWaves && State.bStarted && State.WavesLeft > 0 && Now >= State.NextWaveAt)
		{
			const int32 Wave = 4 - State.WavesLeft;
			const int32 Count = FMath::Min(1 + Wave + RankIndex / 2 + FMath::Max(0, Players.Num() - 1) / 2, 7);
			if (State.Role == TEXT("outage"))
			{
				SpawnShades(i, FMath::Max(2, Count));
			}
			else
			{
				SpawnWave(i, Count);
			}
			--State.WavesLeft;
			State.NextWaveAt = Now + (State.Role == TEXT("clock") ? 5.5 : State.Role == TEXT("outage") ? 7.0 : 9.0);
			continue;
		}
		// The pen's fence burns anyone in the room who is not inside the ring.
		if (State.Role == TEXT("pen") && State.bStarted)
		{
			for (APlayerController* PC : Players)
			{
				ADCCCharacterBase* Crawler = PC ? Cast<ADCCCharacterBase>(PC->GetPawn()) : nullptr;
				if (!Crawler || !Crawler->IsAlive() || !Crawler->Stats || !Grid)
				{
					continue;
				}
				const FVector At = Crawler->GetActorLocation();
				const FIntPoint Cell = Grid->CellOf(At);
				if (Grid->RoomIndexAt(Cell.X, Cell.Y) != i || FVector::Dist2D(At, State.Center) <= 280.0f)
				{
					continue;
				}
				double& Next = NextBurnAt.FindOrAdd(PC);
				if (Now >= Next)
				{
					Next = Now + 0.5;
					Crawler->Stats->ApplyDamage(2 + RankIndex, EDCCDamageType::Fire);
				}
			}
		}
		const bool bFightingRoom = State.Role == TEXT("fight") || State.Role == TEXT("boss") || State.Role == TEXT("treasure")
			|| State.Role == TEXT("duel") || (State.Role == TEXT("ambush") && State.bStarted)
			|| (State.Role == TEXT("pen") && State.bStarted)
			|| (State.Role == TEXT("toll") && State.bStarted && State.Held >= 8.0)
			|| ((State.Role == TEXT("trial") || State.Role == TEXT("clock") || State.Role == TEXT("outage"))
				&& State.bStarted && State.WavesLeft == 0);
		if (!bFightingRoom)
		{
			continue;
		}
		bool bAnyStanding = false;
		for (const TWeakObjectPtr<ADCCEnemyCharacter>& Foe : State.Foes)
		{
			if (Foe.IsValid() && Foe->IsAlive())
			{
				bAnyStanding = true;
				break;
			}
		}
		if (!bAnyStanding)
		{
			const TCHAR* Why = TEXT("Room clear");
			if (State.Role == TEXT("trial")) { Why = TEXT("Trial survived"); }
			else if (State.Role == TEXT("treasure")) { Why = TEXT("Treasure room clear"); }
			else if (State.Role == TEXT("duel")) { Why = TEXT("The Champion is down"); }
			else if (State.Role == TEXT("ambush")) { Why = TEXT("Ambush survived"); }
			else if (State.Role == TEXT("toll")) { Why = TEXT("Booth held"); }
			else if (State.Role == TEXT("outage")) { Why = TEXT("The lights came back"); }
			else if (State.Role == TEXT("clock")) { Why = TEXT("Clock ran out"); }
			else if (State.Role == TEXT("pen")) { Why = TEXT("Pen cleared"); }
			ClearRoom(i, Why);
		}
	}
	TickRoomsServer(Now, Players);

	if (PendingKills > 0 && Now >= NextKillPayout)
	{
		PayParty(PendingKills, PendingKills == 1 ? FString(TEXT("gate kill")) : FString::Printf(TEXT("%d gate kills"), PendingKills));
		PendingKills = 0;
		NextKillPayout = Now + 5.0;
	}
	if (Now >= EndsAt)
	{
		Say(this, TEXT("The gate is collapsing. Everyone out."));
		Wrap(false);
		return;
	}
	RefreshStatus();
}

void ADCCArcadeGate::RefreshStatus()
{
	const double Now = ServerTime();
	int32 Cleared = 0, Total = 0;
	bool bBossDown = false;
	for (const FRoomState& State : Rooms)
	{
		if (State.Role == TEXT("start")) { continue; }
		++Total;
		Cleared += State.bCleared ? 1 : 0;
		bBossDown |= State.Role == TEXT("boss") && State.bCleared;
	}
	StatusLines.Reset();
	if (!bGateOpen)
	{
		StatusLines.Add(FString::Printf(TEXT("Rank %s gate, %d rooms. It opens in %.0f."), RankName(RankIndex), RoomCount, FMath::Max(0.0, OpensAt - Now)));
		StatusLines.Add(TEXT("Gather at the gate. Gear, skills and spells all work in there."));
		return;
	}
	const float Left = FMath::Max(0.0f, static_cast<float>(EndsAt - Now));
	StatusLines.Add(FString::Printf(TEXT("%d:%02d left     rooms %d / %d     score %d"), FMath::FloorToInt(Left / 60.0f), FMath::FloorToInt(Left) % 60, Cleared, Total, Score));
	StatusLines.Add(bExitOpen ? TEXT("The way home is lit in the Gatekeeper's room. Step through.")
		: bBossDown ? TEXT("")
		: TEXT("The way home stands dark in the farthest room. It lights when the Gatekeeper is down."));
}

void ADCCArcadeGate::Wrap(bool bCleared)
{
	if (bWrapped)
	{
		return;
	}
	bWrapped = true;
	BossName.Reset();
	BossHealth = -1.0f;
	SlamAt = 0.0;
	const double Now = ServerTime();
	const int32 TimeBonus = bCleared ? FMath::Max(0, static_cast<int32>(EndsAt - Now)) * 2 : 0;
	Score += TimeBonus;
	// Rooms and kills already paid as they fell; this is the finish (and the time bonus).
	if (PendingKills > 0)
	{
		PayParty(PendingKills, TEXT("gate kills"));
		PendingKills = 0;
	}
	const int32 Tickets = FMath::Clamp(4 + TimeBonus / 30 + (bCleared ? 8 + RankIndex * 3 : 0), 4, 60);

	TArray<APlayerController*> Players;
	GetPlayers(Players);
	for (APlayerController* PC : Players)
	{
		if (!PC || !PC->PlayerState)
		{
			continue;
		}
		DCCProfile::Add(PC->PlayerState, TEXT("Gate"), TEXT("Runs"), 1.0f);
		DCCProfile::RecordBest(PC->PlayerState, TEXT("Gate"), TEXT("BestScore"), static_cast<float>(Score));
		if (bCleared)
		{
			DCCProfile::Add(PC->PlayerState, TEXT("Gate"), TEXT("Clears"), 1.0f);
			DCCArcade::CompleteSeriesQuest(PC->PlayerState, TEXT("Gate"), DCCArcade::SeriesQuestXp());
			DCCProfile::RecordBest(PC->PlayerState, TEXT("Gate"), TEXT("BestRank"), static_cast<float>(RankIndex));
		}
		UDCCWalletComponent::AwardTickets(PC->PlayerState, Tickets, TEXT("gate run"));
		// Out of the gate, back to the plaza.
		if (APawn* Pawn = PC->GetPawn())
		{
			Pawn->TeleportTo(FVector(0.0f, 400.0f, 140.0f), FRotator(0.0f, 90.0f, 0.0f));
		}
	}

	StatusLines.Reset();
	StatusLines.Add(bCleared
		? FString::Printf(TEXT("Rank %s gate cleared. Score %d (time bonus %d)."), RankName(RankIndex), Score, TimeBonus)
		: FString::Printf(TEXT("The gate collapsed. Score %d for what you cleared."), Score));
	StatusLines.Add(FString::Printf(TEXT("%d tickets each for the finish, on top of what the rooms and kills paid."), Tickets));
	ReturnToLobbyIn(20.0f);
}
