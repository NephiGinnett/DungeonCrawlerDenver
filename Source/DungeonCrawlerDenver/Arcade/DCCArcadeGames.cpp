#include "Arcade/DCCArcadeGames.h"
#include "Characters/DCCReactions.h"
#include "Progression/DCCAchievementComponent.h"
#include "Arcade/DCCBounties.h"
#include "Combat/DCCGameState.h"
#include "Sound/SoundBase.h"
#include "Components/AudioComponent.h"
#include "Kismet/GameplayStatics.h"

#include "Arcade/DCCArcade.h"
#include "Components/CapsuleComponent.h"
#include "Rooms/DCCClimbPoint.h"
#include "Arcade/DCCClimbItems.h"
#include "Arcade/DCCRallyItems.h"
#include "Arcade/DCCImpoundNPC.h"
#include "Arcade/DCCSupplyChest.h"
#include "Stats/DCCStatsComponent.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "Components/PointLightComponent.h"
#include "Engine/PointLight.h"
#include "Arcade/DCCWalletComponent.h"
#include "Arcade/DCCChopShop.h"
#include "Arcade/DCCFortTiner.h"
#include "UI/DCCHUD.h"
#include "Arcade/DCCNeonTraffic.h"
#include "Characters/DCCCharacterBase.h"
#include "Session/DCCProfile.h"
#include "Vehicles/DCCRover.h"
#include "Engine/TextRenderActor.h"
#include "Components/TextRenderComponent.h"
#include "Materials/MaterialInstanceDynamic.h"
#include "Engine/World.h"
#include "HAL/IConsoleManager.h"
#include "EngineUtils.h"
#include "GameFramework/FloatingPawnMovement.h"
#include "GameFramework/PlayerController.h"
#include "GameFramework/PlayerState.h"
#include "Net/UnrealNetwork.h"
#include "Arcade/DCCFireworks.h"
#include "Arcade/DCCClimbAmbience.h"
#include "Arcade/DCCDenDecor.h"
#include "Arcade/DCCChopShop.h"
#include "Arcade/DCCRallyAmbience.h"
#include "Stats/DCCStatsComponent.h"
#include "Skills/DCCSkillComponent.h"

namespace
{
	FColor ToColor(const FLinearColor& C)
	{
		return C.ToFColor(true);
	}

	FString PlayerName(const APlayerState* PlayerState)
	{
		return PlayerState ? PlayerState->GetPlayerName() : FString(TEXT("?"));
	}
}

// ===========================================================================
// The Den (lobby)
// ===========================================================================

ADCCArcadeArena::FSky ADCCArcadeLobby::GetSky() const
{
	FSky Sky;
	Sky.SunPitch = -55.0f;
	Sky.SunIntensity = 3.0f;
	Sky.FogColour = FLinearColor(0.35f, 0.2f, 0.5f);
	Sky.FogDensity = 0.02f;
	return Sky;
}

FVector ADCCArcadeLobby::PadCenter(int32 Index) const
{
	const int32 Count = DCCArcade::Games().Num();
	return GetActorLocation() + FVector((Index - (Count - 1) * 0.5f) * 1000.0f, 900.0f, 0.0f);
}

void ADCCArcadeLobby::GetStartTransforms(TArray<FTransform>& Out) const
{
	for (int32 i = 0; i < 8; ++i)
	{
		Out.Add(FTransform(FRotator(0, 90, 0), GetActorLocation() + FVector((i - 3.5f) * 260.0f, -900.0f, 120.0f)));
	}
}

void ADCCArcadeLobby::BuildLocal()
{
	const FVector O = GetActorLocation();
	// Pads are a metre apart in a row. Six games need a wider room than the original five.
	const int32 PadCount = DCCArcade::Games().Num();
	const float L = FMath::Max(2600.0f, (PadCount - 1) * 500.0f + 1100.0f), W = 1700.0f, H = 900.0f;
	Block(O + FVector(0, 0, -20), FVector(L * 2, W * 2, 40), FLinearColor(0.06f, 0.04f, 0.10f));
	Block(O + FVector(0, 0, 1), FVector(L * 2 - 300, 300, 4), FLinearColor(0.5f, 0.1f, 0.6f));
	Block(O + FVector(0, W + 20, H * 0.5f), FVector(L * 2 + 80, 40, H), FLinearColor(0.08f, 0.06f, 0.12f));
	Block(O + FVector(0, -W - 20, H * 0.5f), FVector(L * 2 + 80, 40, H), FLinearColor(0.08f, 0.06f, 0.12f));
	Block(O + FVector(L + 20, 0, H * 0.5f), FVector(40, W * 2, H), FLinearColor(0.08f, 0.06f, 0.12f));
	Block(O + FVector(-L - 20, 0, H * 0.5f), FVector(40, W * 2, H), FLinearColor(0.08f, 0.06f, 0.12f));

	// Text faces +X at yaw 0; the north wall's signs turn to face the room (-Y).
	Sign(O + FVector(0, W - 40, 780), -90.0f, TEXT("THE DEN"), FColor(255, 120, 230), 120.0f);
	Sign(O + FVector(0, W - 40, 680), -90.0f, TEXT("Stand on a game to vote. Majority wins. Everyone goes together."), FColor(200, 190, 220), 30.0f);

	const TArray<DCCArcade::FGame>& Games = DCCArcade::Games();
	for (int32 i = 0; i < Games.Num(); ++i)
	{
		const DCCArcade::FGame& Game = Games[i];
		const FVector Pad = PadCenter(i);
		// No collision: you stand on the floor, and the pad sinks under your feet (TickPads).
		// 14 cm tall (it was 6) so it can press 10 cm and still show; TickPads lifts the dressing on it to match.
		PadTops.Add(Block(Pad + FVector(0, 0, 7), FVector(700, 700, 14), Game.Colour * 0.6f, 0.0f, false));
		Block(Pad + FVector(0, 520, 220), FVector(420, 180, 440), Game.Colour * 0.35f);
		Sign(Pad + FVector(0, 420, 520), -90.0f, FString(Game.Title).ToUpper(), ToColor(Game.Colour), 44.0f);
		Sign(Pad + FVector(0, 420, 470), -90.0f, Game.Pitch, FColor(210, 205, 220), 14.0f);
		// The game's record, on its own board where you vote for it (2026-11-16: it used to be a long list on screen).
		RecordSigns.Add(Sign(Pad + FVector(0, 420, 428), -90.0f, TEXT(" "), FColor(255, 214, 120), 22.0f, false));
		RecordShown.Add(FString());
		Light(Pad + FVector(0, 0, 400), Game.Colour, 9000.0f, 900.0f);
		FDCCDenDecor::Dress(this, Game.Id, Pad, Game.Colour);
	}
	for (int32 i = -2; i <= 2; ++i)
	{
		Light(O + FVector(i * 1000.0f, -400, H - 100), FLinearColor(0.8f, 0.6f, 1.0f), 3000.0f, 1600.0f);
	}

	// The prize counter (2026-11-13; rebuilt 2026-11-14 as a proper booth): a gold pad in front of a counter with
	// a striped awning, three lit shelves of prizes, a marquee of bulbs and a ticket-roll on the counter.
	const FVector Shop = ShopPadCenter();
	const FLinearColor Prize(1.0f, 0.75f, 0.25f), Plum(0.22f, 0.1f, 0.28f), Rose(1.0f, 0.3f, 0.75f), Aqua(0.3f, 0.9f, 1.0f);
	Block(Shop + FVector(0, 0, 3), FVector(600, 600, 6), Prize * 0.7f, 0.0f, false);
	Block(Shop + FVector(0, 0, 5), FVector(520, 520, 4), FLinearColor(0.12f, 0.07f, 0.03f), 0.0f, false);
	Block(Shop + FVector(0, 0, 7), FVector(440, 440, 4), Prize * 0.9f, 0.0f, false);
	// Counter: a plum front with a gold top and a kick strip of light.
	Block(Shop + FVector(0, -540, 60), FVector(820, 140, 120), Plum);
	Block(Shop + FVector(0, -540, 126), FVector(850, 170, 12), Prize);
	if (AActor* Kick = Block(Shop + FVector(0, -468, 12), FVector(820, 6, 14), Rose, 0.0f, false)) { Paint(Kick, TEXT("/Game/Synthwave_city/Material/MI_neon_1")); }
	// Back wall, side posts and three shelves.
	Block(Shop + FVector(0, -680, 360), FVector(860, 20, 600), FLinearColor(0.10f, 0.05f, 0.13f));
	for (int32 Side = -1; Side <= 1; Side += 2)
	{
		Block(Shop + FVector(Side * 430.0f, -600, 360), FVector(24, 180, 600), Plum);
	}
	for (int32 Row = 0; Row < 3; ++Row)
	{
		const float Z = 210.0f + Row * 130.0f;
		Block(Shop + FVector(0, -650, Z), FVector(820, 70, 8), Prize * 0.8f, 0.0f, false);
		for (int32 k = -3; k <= 3; ++k)
		{
			// Prizes: the shop's own shapes, a different mix on each shelf.
			const int32 Pick = (k + 3 + Row * 2) % 4;
			const FLinearColor C = Pick == 0 ? Rose : Pick == 1 ? Aqua : Pick == 2 ? Prize : FLinearColor(0.5f, 1.0f, 0.4f);
			const TCHAR* ShapeName = Pick == 0 ? TEXT("Cone") : Pick == 1 ? TEXT("Sphere") : Pick == 2 ? TEXT("Cylinder") : TEXT("Cube");
			Shape(ShapeName, Shop + FVector(k * 112.0f, -650, Z + 40.0f), FVector(46, 46, 62), C, FRotator(0, k * 20.0f, 0), false);
		}
		Light(Shop + FVector(0, -560, Z + 70.0f), Row % 2 ? Aqua : Rose, 1500.0f, 500.0f);
	}
	// Awning: stripes in the two prize colours, tilted out over the counter.
	for (int32 k = -4; k <= 4; ++k)
	{
		Shape(TEXT("Cube"), Shop + FVector(k * 98.0f, -500, 640), FVector(98, 300, 10), k % 2 ? Prize : Rose, FRotator(0, 0, -18.0f), false);
	}
	// Marquee bulbs along the awning's front edge.
	for (int32 k = -8; k <= 8; ++k)
	{
		if (AActor* Bulb = Shape(TEXT("Sphere"), Shop + FVector(k * 52.0f, -352, 592), FVector(18, 18, 18), FLinearColor(3.0f, 2.4f, 1.0f), FRotator::ZeroRotator, false))
		{
			Paint(Bulb, TEXT("/Game/Synthwave_city/Material/MI_light"));
		}
	}
	// A roll of tickets on the counter.
	Shape(TEXT("Cylinder"), Shop + FVector(-300, -520, 160), FVector(70, 70, 40), Rose, FRotator(90, 0, 0), false);
	Block(Shop + FVector(-190, -520, 136), FVector(160, 40, 4), Rose, 0.0f, false);
	// Text faces +X at yaw 0; the south wall's signs turn to face the room (+Y).
	Sign(Shop + FVector(0, -340, 720), 90.0f, TEXT("PRIZE COUNTER"), FColor(255, 200, 90), 74.0f);
	Sign(Shop + FVector(0, -465, 90), 90.0f, TEXT("STAND ON THE GOLD PAD"), FColor(255, 225, 150), 26.0f);
	Sign(Shop + FVector(0, -465, 56), 90.0f, TEXT("tickets from every game buy looks"), FColor(220, 210, 230), 16.0f);
	Light(Shop + FVector(0, -200, 520), Prize, 9000.0f, 1300.0f);

	// The Chop Shop (2026-11-14): the same counter screen on its upgrades tab. A cyan pad in front of a garage
	// bay: a roll-up door frame, a tyre stack, an engine on a stand, a pegboard of tools.
	const FVector Chop = ChopPadCenter();
	const FLinearColor Steel(0.2f, 0.22f, 0.26f), Rubber(0.03f, 0.03f, 0.035f), Hazard(1.0f, 0.75f, 0.1f);
	Block(Chop + FVector(0, 0, 3), FVector(600, 600, 6), Aqua * 0.6f, 0.0f, false);
	Block(Chop + FVector(0, 0, 5), FVector(520, 520, 4), FLinearColor(0.02f, 0.06f, 0.08f), 0.0f, false);
	for (int32 k = -2; k <= 2; ++k)
	{
		// hazard chevrons across the pad
		Shape(TEXT("Cube"), Chop + FVector(k * 100.0f, 0, 8), FVector(40, 420, 3), k % 2 ? Hazard : Rubber, FRotator(0, 35.0f, 0), false);
	}
	Block(Chop + FVector(0, -680, 360), FVector(860, 20, 600), FLinearColor(0.06f, 0.08f, 0.1f));
	for (int32 Side = -1; Side <= 1; Side += 2)
	{
		Block(Chop + FVector(Side * 430.0f, -600, 360), FVector(30, 180, 600), Steel);
	}
	Block(Chop + FVector(0, -600, 650), FVector(890, 180, 30), Steel);
	for (int32 k = 0; k < 6; ++k)
	{
		// the roll-up door, half open
		Block(Chop + FVector(0, -520, 620.0f - k * 34.0f), FVector(820, 10, 28), k % 2 ? Steel * 1.3f : Steel * 0.9f, 0.0f, false);
	}
	Block(Chop + FVector(0, -540, 60), FVector(820, 140, 120), Steel);
	Block(Chop + FVector(0, -540, 126), FVector(850, 170, 12), Hazard);
	if (AActor* Kick = Block(Chop + FVector(0, -468, 12), FVector(820, 6, 14), Aqua, 0.0f, false)) { Paint(Kick, TEXT("/Game/Synthwave_city/Material/MI_neon_4")); }
	for (int32 k = 0; k < 4; ++k)
	{
		Shape(TEXT("Cylinder"), Chop + FVector(330, -330, 30.0f + k * 52.0f), FVector(150, 150, 50), Rubber, FRotator(0, k * 17.0f, 0), true); // tyres
	}
	// An engine block on a stand.
	Block(Chop + FVector(-300, -360, 60), FVector(120, 120, 120), Steel * 0.7f);
	Block(Chop + FVector(-300, -360, 165), FVector(170, 130, 90), FLinearColor(0.5f, 0.12f, 0.08f), 0.0f, false);
	for (int32 k = -1; k <= 1; ++k)
	{
		Shape(TEXT("Cylinder"), Chop + FVector(-300 + k * 50.0f, -360, 230), FVector(30, 30, 50), Steel * 1.6f, FRotator::ZeroRotator, false);
	}
	// Pegboard: wrenches as bars and a few sockets.
	Block(Chop + FVector(0, -668, 330), FVector(560, 6, 260), FLinearColor(0.16f, 0.12f, 0.08f), 0.0f, false);
	for (int32 k = -3; k <= 3; ++k)
	{
		Shape(TEXT("Cube"), Chop + FVector(k * 70.0f, -660, 340.0f + (k % 2) * 30.0f), FVector(14, 6, 130.0f - FMath::Abs(k) * 12.0f), Steel * 2.0f, FRotator(0, 0, k * 6.0f), false);
	}
	Sign(Chop + FVector(0, -505, 700), 90.0f, TEXT("THE CHOP SHOP"), FColor(90, 230, 255), 74.0f);
	Sign(Chop + FVector(0, -465, 90), 90.0f, TEXT("STAND ON THE CYAN PAD"), FColor(170, 240, 255), 26.0f);
	Sign(Chop + FVector(0, -465, 56), 90.0f, TEXT("scrap from the driving games buys truck upgrades"), FColor(220, 225, 235), 16.0f);
	Light(Chop + FVector(0, -200, 520), Aqua, 9000.0f, 1300.0f);
	Light(Chop + FVector(-300, -360, 320), FLinearColor(1.0f, 0.5f, 0.2f), 2500.0f, 500.0f);
}

void ADCCArcadeLobby::TickPads(float DeltaSeconds)
{
	UWorld* World = GetWorld();
	const TArray<DCCArcade::FGame>& Games = DCCArcade::Games();
	if (!World || PadTops.Num() != Games.Num())
	{
		return;
	}
	// The first time: everything lying on each pad (its colour slab and the dressing on top of it) rides down with it.
	if (PadRiders.IsEmpty())
	{
		PadRiders.SetNum(Games.Num());
		PadSink.Init(0.0f, Games.Num());
		for (TActorIterator<AActor> It(World); It; ++It)
		{
			AActor* Actor = *It;
			if (!Actor || Actor == this || Cast<APawn>(Actor) || !Actor->GetRootComponent())
			{
				continue;
			}
			const FVector L = Actor->GetActorLocation();
			for (int32 i = 0; i < Games.Num(); ++i)
			{
				const FVector D = L - PadCenter(i);
				if (FMath::Abs(D.X) <= 360.0f && FMath::Abs(D.Y) <= 360.0f && D.Z > -5.0f && D.Z < 40.0f)
				{
					Actor->GetRootComponent()->SetMobility(EComponentMobility::Movable);
					// The dressing was placed for the old 6 cm pad: lift it onto the 14 cm one.
					const FVector Rest = Actor == PadTops[i].Get() ? L : L + FVector(0.0f, 0.0f, 8.0f);
					Actor->SetActorLocation(Rest);
					PadRiders[i].Add({ Actor, Rest });
					break;
				}
			}
		}
	}
	bSomeoneOnPad = false;
	for (int32 i = 0; i < Games.Num(); ++i)
	{
		// Anyone standing on it (crawlers only: a player's pawn has a PlayerState).
		bool bOccupied = false;
		for (TActorIterator<APawn> It(World); It && !bOccupied; ++It)
		{
			const FVector D = It->GetActorLocation() - PadCenter(i);
			bOccupied = It->GetPlayerState() && FMath::Abs(D.X) <= 350.0f && FMath::Abs(D.Y) <= 350.0f && FMath::Abs(D.Z) < 250.0f;
		}
		bSomeoneOnPad |= bOccupied;
		// Down 10 cm quickly (its top stays just above the floor), back up a little slower, like a plate on a spring.
		const float Target = bOccupied ? 10.0f : 0.0f;
		const float Was = PadSink[i];
		PadSink[i] = FMath::FInterpConstantTo(PadSink[i], Target, DeltaSeconds, bOccupied ? 70.0f : 35.0f);
		if (PadSink[i] != Was)
		{
			for (const TPair<TWeakObjectPtr<AActor>, FVector>& Rider : PadRiders[i])
			{
				if (AActor* Actor = Rider.Key.Get())
				{
					Actor->SetActorLocation(Rider.Value - FVector(0.0f, 0.0f, PadSink[i]));
				}
			}
		}
	}
}

void ADCCArcadeLobby::ClientTick(float DeltaSeconds)
{
	TickPads(DeltaSeconds);
	// Each pad's board shows its game's record, kept current as people play.
	for (int32 i = 0; i < RecordSigns.Num() && i < PadRecords.Num(); ++i)
	{
		if (RecordShown[i] != PadRecords[i])
		{
			RecordShown[i] = PadRecords[i];
			if (ATextRenderActor* Text = RecordSigns[i].Get())
			{
				Text->GetTextRender()->SetText(FText::FromString(PadRecords[i]));
			}
		}
	}
	// The Den's playlist: the songs already in the game, one after another, low, with a breath between them.
	if (UWorld* MusicWorld = GetWorld(); MusicWorld && MusicWorld->GetNetMode() != NM_DedicatedServer)
	{
		const double Now = MusicWorld->GetTimeSeconds();
		if (Now >= NextDenSong && (!DenSong.IsValid() || !DenSong->IsPlaying()))
		{
			static const TCHAR* Playlist[] = {
				TEXT("/Game/Songs/Meet_the_Whoops.Meet_the_Whoops"), TEXT("/Game/Songs/Swept_Away.Swept_Away"),
				TEXT("/Game/Songs/Repo_Rally.Repo_Rally"), TEXT("/Game/Songs/Platinum_Dust_Mine.Platinum_Dust_Mine"),
			};
			if (DenSongIndex < 0)
			{
				DenSongIndex = FMath::RandRange(0, UE_ARRAY_COUNT(Playlist) - 1);
			}
			NextDenSong = Now + 5.0;
			for (int32 Try = 0; Try < UE_ARRAY_COUNT(Playlist); ++Try)
			{
				DenSongIndex = (DenSongIndex + 1) % UE_ARRAY_COUNT(Playlist);
				if (USoundBase* Song = LoadObject<USoundBase>(nullptr, Playlist[DenSongIndex]))
				{
					DenSong = UGameplayStatics::SpawnSound2D(MusicWorld, Song, 0.22f, 1.0f, 0.0f, nullptr, false, true);
					NextDenSong = Now + FMath::Max(20.0f, Song->GetDuration()) + 4.0;
					break;
				}
			}
		}
	}

	// Opens the shop on stepping onto the counter's pad, not every frame while
	// standing there: DONE closes it, and it stays closed until you step off and on.
	APlayerController* PC = GetWorld() ? GetWorld()->GetFirstPlayerController() : nullptr;
	const APawn* Pawn = PC && PC->IsLocalController() ? PC->GetPawn() : nullptr;
	if (!Pawn)
	{
		return;
	}
	auto OnPad = [Pawn](const FVector& Centre)
	{
		const FVector D = Pawn->GetActorLocation() - Centre;
		return FMath::Abs(D.X) <= 300.0f && FMath::Abs(D.Y) <= 300.0f && FMath::Abs(D.Z) < 400.0f;
	};
	const bool bOnPad = OnPad(ShopPadCenter());
	const bool bOnChop = OnPad(ChopPadCenter());
	if ((bOnPad && !bWasOnShopPad) || (bOnChop && !bWasOnChopPad))
	{
		if (ADCCHUD* Hud = Cast<ADCCHUD>(PC->GetHUD()))
		{
			Hud->OpenCosmeticShop(bOnChop ? 2 : 0);
		}
	}
	bWasOnShopPad = bOnPad;
	bWasOnChopPad = bOnChop;
}

void ADCCArcadeLobby::ServerTick(float DeltaSeconds)
{
	RefreshTimer -= DeltaSeconds;
	if (RefreshTimer > 0.0f || bLeaving)
	{
		return;
	}
	RefreshTimer = 0.25f;

	const TArray<DCCArcade::FGame>& Games = DCCArcade::Games();
	TArray<int32> Votes;
	Votes.Init(0, Games.Num());

	TArray<APlayerController*> Players;
	GetPlayers(Players);
	int32 Present = 0;
	int32 Voted = 0;
	for (APlayerController* PC : Players)
	{
		const APawn* Pawn = PC->GetPawn();
		if (!Pawn)
		{
			continue;
		}
		++Present;
		for (int32 i = 0; i < Games.Num(); ++i)
		{
			const FVector D = Pawn->GetActorLocation() - PadCenter(i);
			if (FMath::Abs(D.X) <= 350.0f && FMath::Abs(D.Y) <= 350.0f)
			{
				++Votes[i];
				++Voted;
				break;
			}
		}
	}

	int32 Leader = 0;
	for (int32 i = 1; i < Votes.Num(); ++i)
	{
		if (Votes[i] > Votes[Leader])
		{
			Leader = i;
		}
	}

	// Everyone voted: 5 seconds. Anyone voted: 20 seconds for the rest to choose.
	const double Now = ServerTime();
	if (Voted == 0)
	{
		CountdownEndsAt = -1.0;
		bAllVotedCountdown = false;
	}
	else if (Voted >= Present && !bAllVotedCountdown)
	{
		bAllVotedCountdown = true;
		CountdownEndsAt = CountdownEndsAt < 0.0 ? Now + 5.0 : FMath::Min(CountdownEndsAt, Now + 5.0);
	}
	else if (CountdownEndsAt < 0.0)
	{
		CountdownEndsAt = Now + 20.0;
	}
	if (Voted < Present)
	{
		bAllVotedCountdown = false;
	}

	Title = TEXT("THE DEN  -  pick the next game");
	Accent = FLinearColor(1.0f, 0.45f, 0.9f);
	StatusLines.Reset();
	// The HUD draws the Den as a strip of game logos with the players on each (ADCCHUD::DrawDenStrip); the records go
	// on the pads' boards (ClientTick). Nothing here is a long list any more.
	PadVotes.SetNum(Games.Num());
	PadRecords.SetNum(Games.Num());
	for (int32 i = 0; i < Games.Num(); ++i)
	{
		const DCCArcade::FGame& Game = Games[i];
		const DCCArcade::FGame* GamePtr = &Game;
		PadVotes[i] = uint8(FMath::Clamp(Votes[i], 0, 255));
		PadRecords[i] = FString::Printf(TEXT("%s: %s"), Game.RecordLabel,
			*DCCProfile::Leaderboard(GetWorld(), Game.Id, Game.RecordStat, Game.bHigherIsBetter,
				[GamePtr](float V) { return DCCArcade::FormatRecord(*GamePtr, V); }));
	}
	LeadingPad = Voted > 0 ? Leader : -1;
	CountdownLine.Reset();
	if (CountdownEndsAt > 0.0)
	{
		const int32 Left = FMath::Max(0, FMath::CeilToInt(CountdownEndsAt - Now));
		CountdownLine = FString::Printf(TEXT("Starting %s in %d s  (%d of %d voted)"), Games[Leader].Title, Left, Voted, Present);
		StatusLines.Add(CountdownLine);
		if (Now >= CountdownEndsAt)
		{
			bLeaving = true;
			DCCArcade::Travel(GetWorld(), Games[Leader].Id);
		}
	}
}

void ADCCArcadeLobby::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);
	DOREPLIFETIME(ADCCArcadeLobby, PadVotes);
	DOREPLIFETIME(ADCCArcadeLobby, PadRecords);
	DOREPLIFETIME(ADCCArcadeLobby, CountdownLine);
	DOREPLIFETIME(ADCCArcadeLobby, LeadingPad);
}

// ===========================================================================
// Fourteener (co-op climbing)
// ===========================================================================

ADCCArcadeArena::FSky ADCCArcadeClimb::GetSky() const
{
	FSky Sky;
	Sky.SunPitch = -35.0f;
	Sky.SunIntensity = 4.0f; // was 9: auto exposure washed the crawlers out (Zoe, 2026-11-13)
	Sky.FogColour = FLinearColor(0.6f, 0.7f, 0.85f);
	Sky.FogDensity = 0.008f;
	return Sky;
}

void ADCCArcadeClimb::GetStartTransforms(TArray<FTransform>& Out) const
{
	// Around the campfire in the middle of Fort Tiner, facing the gate and the peak.
	for (int32 i = 0; i < 8; ++i)
	{
		const float A = PI * (0.25f + 1.5f * i / 7.0f); // a horseshoe, open toward the gate
		Out.Add(FTransform(FRotator(0, 180, 0), FortCenter() + FVector(-FMath::Cos(A) * 520.0f, FMath::Sin(A) * 520.0f, 120.0f)));
	}
}

void ADCCArcadeClimb::BuildLocal()
{
	Block(GetActorLocation() + FVector(0, 0, -20), FVector(22000, 17000, 40), FLinearColor(0.25f, 0.33f, 0.2f));
	BuildMountain();
	BuildFort();
}

FVector ADCCArcadeClimb::FaceNormal(int32 Face)
{
	switch (Face & 3)
	{
	case 0: return FVector(1, 0, 0);
	case 1: return FVector(0, 1, 0);
	case 2: return FVector(-1, 0, 0);
	default: return FVector(0, -1, 0);
	}
}

FVector ADCCArcadeClimb::FaceAlong(int32 Face)
{
	const FVector N = FaceNormal(Face);
	return FVector(-N.Y, N.X, 0.0f);
}

FString ADCCArcadeClimb::FaceName(int32 Face)
{
	switch (Face & 3)
	{
	case 0: return TEXT("EXPRESS (east)");
	case 1: return TEXT("north");
	case 2: return TEXT("west");
	default: return TEXT("south");
	}
}

int32 ADCCArcadeClimb::FaceOf(const FVector& Location) const
{
	const FVector D = Location - GetActorLocation();
	return FMath::Abs(D.X) >= FMath::Abs(D.Y) ? (D.X >= 0.0f ? 0 : 2) : (D.Y >= 0.0f ? 1 : 3);
}

const TArray<ADCCArcadeClimb::FIce>& ADCCArcadeClimb::IcePatches()
{
	// Ice on the switchback faces only: the express face is fast enough already.
	static const TArray<FIce> Ice = []()
	{
		TArray<FIce> Out;
		for (int32 Face = 1; Face < 4; ++Face)
		{
			const float Lower[] = { 800.0f + Face * 300.0f, 2700.0f + Face * 200.0f };
			const float Upper[] = { 5500.0f + Face * 300.0f, 7700.0f + Face * 200.0f };
			for (int32 i = 0; i < 2; ++i)
			{
				Out.Add({ Face, Lower[i], Lower[i] + 380.0f, (i ? -1.0f : 1.0f) * (180.0f + Face * 60.0f), 600.0f });
				Out.Add({ Face, Upper[i], Upper[i] + 320.0f, (i ? 1.0f : -1.0f) * (120.0f + Face * 40.0f), 480.0f });
			}
		}
		return Out;
	}();
	return Ice;
}

TArray<float> ADCCArcadeClimb::LedgeHeights(int32 Face)
{
	TArray<float> Heights;
	const float Step = IsExpress(Face) ? 1500.0f : 800.0f; // longer pitches: each one costs most of a novice's stamina
	for (float Z = Step; Z < TopZ - 300.0f; Z += Step)
	{
		if (FMath::Abs(Z - MidZ) > 100.0f)
		{
			Heights.Add(Z);
		}
	}
	return Heights;
}

namespace FortTinerArt
{
	// Models from the installed packs. A missing one costs decoration, never a crash (ADCCArcadeArena::Prop).
	const TCHAR* CliffMat = TEXT("/Game/Legendary_Forge/Materials/Mi_rock_cliff_02");
	const TCHAR* RockA = TEXT("/Game/Legendary_Forge/Meshes/Rocks/Large_rocks/SM_Large_rock_01");
	const TCHAR* RockB = TEXT("/Game/Legendary_Forge/Meshes/Rocks/Large_rocks/SM_large_rock_02");
	const TCHAR* RockPile = TEXT("/Game/Legendary_Forge/Meshes/Rocks/Small_rocks/SM_rock_assembly_01");
	const TCHAR* SmallRockA = TEXT("/Game/Viking_village/Meshes/Small_rocks/SM_rock_01");
	const TCHAR* SmallRockB = TEXT("/Game/Viking_village/Meshes/Small_rocks/SM_rock_03");
	const TCHAR* RangeA = TEXT("/Game/Viking_village/Meshes/Mountains/SM_mountain");
	const TCHAR* RangeB = TEXT("/Game/Viking_village/Meshes/Mountains/SM_mountain_02");
	const TCHAR* PineA = TEXT("/Game/Viking_village/Biome/Meshes/Tree/SM_pine_tree_Var_01");
	const TCHAR* PineB = TEXT("/Game/Viking_village/Biome/Meshes/Tree/SM_pine_tree_Var_03");
	const TCHAR* Tree = TEXT("/Game/Viking_village/Biome/Meshes/Tree/SM_Forest_tree_01");
	const TCHAR* Bush = TEXT("/Game/Viking_village/Biome/Meshes/Vegetation/SM_bush_01");
	const TCHAR* Grass = TEXT("/Game/Viking_village/Meshes/Grasss_02/SM_grass_01");
	const TCHAR* Cabin1 = TEXT("/Game/Viking_village/Meshes/Architecture/SM_Cabin_01");
	const TCHAR* Cabin2 = TEXT("/Game/Viking_village/Meshes/Architecture/SM_Cabin_02");
	const TCHAR* Bench = TEXT("/Game/Viking_village/Meshes/Table/SM_bench_01");
	const TCHAR* Barrels = TEXT("/Game/Viking_village/Meshes/Small_props/SM_barrel_group_01");
	const TCHAR* Crates = TEXT("/Game/Viking_village/Meshes/Small_props/SM_crate_group_01");
	const TCHAR* Sack = TEXT("/Game/Viking_village/Meshes/Small_props/SM_bag");
	const TCHAR* Firewood = TEXT("/Game/Viking_village/Meshes/Architecture/SM_small_wood");
	const TCHAR* Pulley = TEXT("/Game/Viking_village/Meshes/Drakkar/SM_pulley__01");
	const TCHAR* Forge = TEXT("/Game/Legendary_Forge/Meshes/Forge/SM_forge_02");
	const TCHAR* Anvil = TEXT("/Game/Legendary_Forge/Meshes/Anvil_02/SM_anvil_02");
	const TCHAR* Bellows = TEXT("/Game/Legendary_Forge/Meshes/Bellows/SM_bellows");
	const TCHAR* Grinder = TEXT("/Game/Legendary_Forge/Meshes/Grinding_wheel/SM_grinding_wheel");
	const TCHAR* Rack = TEXT("/Game/Legendary_Forge/Meshes/Tool_holder/SM_weapon_holder");
	const TCHAR* Brazier = TEXT("/Game/Legendary_Forge/Meshes/Brasero/SM_brasero_01");
	const TCHAR* Ladder = TEXT("/Game/Legendary_Forge/Meshes/Ladder/SM_ladder");
	const TCHAR* Tower = TEXT("/Game/Desert_cave/Meshes/Wood_structures/SM_wood_tower_01");
	const TCHAR* CartA = TEXT("/Game/Desert_cave/Meshes/Cart/SM_cart_01");
	const TCHAR* CartB = TEXT("/Game/Desert_cave/Meshes/Cart/SM_cart_02");
	const TCHAR* Flag = TEXT("/Game/Kingdom_Capital/Meshes/Flags/SM_flag_01");
	const TCHAR* Tent = TEXT("/Game/Cave_Camp/Meshes/Props_02/SM_tent_01");
	const TCHAR* TentB = TEXT("/Game/Cave_Camp/Meshes/Props_02/SM_tent_05");
	const TCHAR* Tents = TEXT("/Game/Cave_Camp/Meshes/Props_02/SM_tent_group");
	const TCHAR* Tables = TEXT("/Game/Cave_Camp/Meshes/Props_02/SM_table_group_01");
	const TCHAR* CampCrates = TEXT("/Game/Cave_Camp/Meshes/Props_02/SM_crate_group_02");
}

void ADCCArcadeClimb::BuildMountain()
{
	using namespace FortTinerArt;
	const FVector O = GetActorLocation();
	const FLinearColor Rock(0.32f, 0.29f, 0.26f);
	const FLinearColor Ledge(0.46f, 0.41f, 0.33f);

	// The peak: a wide lower block up to High Camp, a narrow one to the summit,
	// in the forge pack's mountain rock. No slab on top: the top of the narrow
	// block is the summit, so the last face rounds straight onto it with no
	// overhang to pull up under (Zoe, 2026-11-13: "a roof they have to clip through").
	Block(O + FVector(0, 0, TopZ * 0.5f), FVector(NarrowHalf * 2, NarrowHalf * 2, TopZ), Rock);
	Block(O + FVector(0, 0, MidZ * 0.5f), FVector(WideHalf * 2, WideHalf * 2, MidZ), Rock * 0.9f);

	// Rock skin: 3 m tiles of cliff rock over every face and top, so the texture
	// repeats at rock scale instead of one image stretched over a 24 m face.
	// Paper-thin and collision-free, a centimetre proud of the wall: climbing
	// never touches them.
	auto Clad = [this, &O](const FVector& Center, const FVector& Size)
	{
		if (AActor* Tile = Block(Center, Size, FLinearColor(0.4f, 0.37f, 0.33f), 0.0f, false))
		{
			Paint(Tile, CliffMat);
		}
	};
	for (int32 Face = 0; Face < 4; ++Face)
	{
		const FVector N = FaceNormal(Face), T = FaceAlong(Face);
		struct FBand { float Half; float Z0; float Z1; };
		for (const FBand& Band : { FBand{ WideHalf, 0.0f, MidZ }, FBand{ NarrowHalf, MidZ, TopZ } })
		{
			const int32 Across = FMath::CeilToInt(Band.Half * 2.0f / 600.0f);
			const float TileW = Band.Half * 2.0f / Across;
			for (float Z = Band.Z0; Z < Band.Z1 - 1.0f; Z += 600.0f)
			{
				const float TileH = FMath::Min(600.0f, Band.Z1 - Z);
				for (int32 i = 0; i < Across; ++i)
				{
					const float Lateral = -Band.Half + TileW * (i + 0.5f);
					const FVector Center = O + N * (Band.Half + 1.0f) + T * Lateral + FVector(0, 0, Z + TileH * 0.5f);
					Clad(Center, FVector(N.X != 0 ? 2.0f : TileW, N.Y != 0 ? 2.0f : TileW, TileH));
				}
			}
		}
	}
	for (float X = -WideHalf; X < WideHalf - 1.0f; X += 400.0f)
	{
		for (float Y = -WideHalf; Y < WideHalf - 1.0f; Y += 400.0f)
		{
			const bool bUnderPeak = FMath::Abs(X + 200.0f) < NarrowHalf && FMath::Abs(Y + 200.0f) < NarrowHalf;
			if (!bUnderPeak)
			{
				Clad(O + FVector(X + 200.0f, Y + 200.0f, MidZ + 1.0f), FVector(400, 400, 2)); // High Camp's shelf
			}
		}
	}
	for (float X = -NarrowHalf; X < NarrowHalf - 1.0f; X += 350.0f)
	{
		for (float Y = -NarrowHalf; Y < NarrowHalf - 1.0f; Y += 350.0f)
		{
			Clad(O + FVector(X + 175.0f, Y + 175.0f, TopZ + 1.0f), FVector(350, 350, 2)); // the summit
		}
	}

	// Ledges are rounded rock outcrops (2026-11-13). What you stand on is a hidden
	// flat slab inside each rock, so landing, resting and the climb's mantle stay
	// exact while the rock shape is free to bulge. The express face gets one every
	// 12 m; the switchbacks one every 6 m, staggered so a dropped climber always
	// has one close below.
	const TCHAR* Outcrops[] = { RockA, RockB, RockPile };
	for (int32 Face = 0; Face < 4; ++Face)
	{
		const FVector N = FaceNormal(Face), T = FaceAlong(Face);
		const TArray<float> Heights = LedgeHeights(Face);
		for (int32 k = 0; k < Heights.Num(); ++k)
		{
			const float Z = Heights[k];
			const float Lateral = ((k % 3) - 1) * 300.0f;
			const FVector Center = O + N * (HalfWidthAt(Z) + 200.0f) + T * Lateral + FVector(0, 0, Z - 20.0f);
			// As deep as the boulder, so a climber meets its underside where they can
			// see it, then crawls out, around and over (ADCCCharacterBase::BeginLedgeCrawl).
			// 4.2 m deep since 2026-11-13 (was 3.2): a longer, harder crawl out and over.
			// Bigger again (2026-11-13): 5.6 m deep, 9 m wide.
			if (AActor* Slab = Block(Center + N * 70.0f + FVector(0, 0, -80.0f), FVector(N.X != 0 ? 560 : 900, N.Y != 0 ? 560 : 900, 200), Ledge))
			{
				Slab->SetActorHiddenInGame(true);
			}
			const FVector RockBase = O + N * (HalfWidthAt(Z) + 150.0f) + T * Lateral + FVector(0, 0, Z - 200.0f);
			if (!Prop(Outcrops[(Face + k) % 3], RockBase, N.Rotation().Yaw + (k % 2) * 180.0f, FVector(640, 980, 260), false, true))
			{
				// No rock pack installed: show the slab so the ledge is still visible.
				Shape(TEXT("Sphere"), Center + FVector(0, 0, -40), FVector(N.X != 0 ? 340 : 660, N.Y != 0 ? 340 : 660, 150), Ledge, FRotator::ZeroRotator, false);
			}
		}
	}

	BuildStrands();

	// Overhangs (2026-11-13: "more pronounced, so the mountain is harder to climb").
	// One between every pair of ledges: a 5 m wide wedge of rock jutting 1.6 m out,
	// its underside and top both steep. Unlike a ledge it cannot be crawled over
	// (BeginLedgeCrawl needs a flat top), so a climber under it has to traverse
	// sideways around it - slower, and on stamina. Placed off-centre from the ledge
	// line so the route zigzags.
	for (int32 Face = 0; Face < 4; ++Face)
	{
		const FVector N = FaceNormal(Face), T = FaceAlong(Face);
		const TArray<float> Heights = LedgeHeights(Face);
		for (int32 k = 0; k + 1 < Heights.Num(); ++k)
		{
			const float Z = (Heights[k] + Heights[k + 1]) * 0.5f;
			if (FMath::Abs(Z - MidZ) < 500.0f || Z > TopZ - 600.0f)
			{
				continue;
			}
			const float Half = HalfWidthAt(Z);
			const float Lateral = FMath::Fmod(Face * 173.0f + k * 397.0f, FMath::Max(1.0f, (Half - 300.0f) * 2.0f)) - (Half - 300.0f);
			const FVector Center = O + N * (Half + 70.0f) + T * Lateral + FVector(0, 0, Z);
			// The wedge: a block whose long side runs along the face, rolled 45 degrees
			// about that axis, so both faces you could meet are slopes, not a floor or a roof.
			// 2.2 m square rolled 45 degrees: juts 1.6 m and leaves 2.4 m over the ledge below, room to stand.
			if (AActor* Jut = Shape(TEXT("Cube"), Center + N * 20.0f, FVector(720, 220, 220), Ledge, FRotator(0.0f, T.Rotation().Yaw, 45.0f), true))
			{
				Jut->SetActorHiddenInGame(true);
			}
			Prop(Outcrops[(Face + k + 1) % 3], Center + N * 10.0f - FVector(0, 0, 150.0f), N.Rotation().Yaw + 90.0f, FVector(740, 740, 420), false, true);
		}
	}

	// Ice: pale patches on the rock face, no collision of their own.
	for (const FIce& Ice : IcePatches())
	{
		const FVector N = FaceNormal(Ice.Face), T = FaceAlong(Ice.Face);
		const float MidHeight = (Ice.ZMin + Ice.ZMax) * 0.5f;
		// A flattened sphere half sunk into the rock and painted with real ice, so it
		// reads as a glaze with soft edges rather than a blank panel (Zoe, 2026-11-13).
		const FVector Center = O + N * HalfWidthAt(MidHeight) + T * Ice.Lateral + FVector(0, 0, MidHeight);
		const FVector Size(N.X != 0 ? 40 : Ice.Width * 1.15f, N.Y != 0 ? 40 : Ice.Width * 1.15f, (Ice.ZMax - Ice.ZMin) * 1.1f);
		if (AActor* Glaze = Shape(TEXT("Sphere"), Center, Size, FLinearColor(0.75f, 0.92f, 1.0f), FRotator::ZeroRotator, false))
		{
			Paint(Glaze, TEXT("/Game/Ice_Station/Materials/Ice/MI_ice_01"));
		}
	}

	// High Camp: the shelf where the mountain narrows. Tents and supplies in its
	// corners, a flag and the sign on the fort side.
	const float Shelf = (WideHalf + NarrowHalf) * 0.5f; // the middle of High Camp's shelf
	Prop(Tent, O + FVector(Shelf, Shelf, MidZ), 225.0f, FVector(320, 320, 0), true);
	Prop(TentB, O + FVector(-Shelf, -Shelf, MidZ), 45.0f, FVector(320, 320, 0), true);
	Prop(Barrels, O + FVector(-Shelf, Shelf, MidZ), 0.0f, FVector(0, 0, 130), true);
	Prop(CampCrates, O + FVector(Shelf, -Shelf, MidZ), 30.0f, FVector(0, 0, 130), true);
	Shape(TEXT("Cylinder"), O + FVector(Shelf, 300, MidZ + 200), FVector(14, 14, 400), FLinearColor(0.85f, 0.85f, 0.85f), FRotator::ZeroRotator, false);
	Block(O + FVector(Shelf, 230, MidZ + 360), FVector(6, 130, 80), FLinearColor(1.0f, 0.55f, 0.1f), 0.0f, false);
	Sign(O + FVector(WideHalf + 10, 0, MidZ + 160), 0.0f, TEXT("HIGH CAMP  11,560 FT"), FColor(255, 214, 140), 40.0f);
	Sign(O + FVector(WideHalf + 10, 0, MidZ + 110), 0.0f, TEXT("Checkpoint. Fall above here and you come back here."), FColor(235, 230, 220), 16.0f);

	// The summit: the top of the peak itself, with a cairn, a brazier, the flag and its sign.
	Prop(RockPile, O + FVector(220, 220, TopZ), 30.0f, FVector(0, 0, 140), true);
	Prop(Brazier, O + FVector(-260, -260, TopZ), 0.0f, FVector(0, 0, 120), true);
	Light(O + FVector(-260, -260, TopZ + 180), FLinearColor(1.0f, 0.6f, 0.25f), 6000.0f, 900.0f);
	Shape(TEXT("Cylinder"), O + FVector(0, 0, TopZ + 300), FVector(24, 24, 600), FLinearColor(0.9f, 0.9f, 0.9f), FRotator::ZeroRotator, false);
	Block(O + FVector(80, 0, TopZ + 520), FVector(140, 10, 90), FLinearColor(0.9f, 0.2f, 0.2f), 0.0f, false);
	Sign(O + FVector(0, 0, TopZ + 200), 0.0f, TEXT("SUMMIT  14,115 FT"), FColor(255, 255, 255), 70.0f);
	Light(O + FVector(0, 0, TopZ + 700), FLinearColor(1.0f, 0.9f, 0.6f), 9000.0f, 4000.0f);

	// Boulders at the foot of the four corners, scree around the base.
	for (int32 i = 0; i < 4; ++i)
	{
		const FVector Corner(i % 2 ? WideHalf + 150.0f : -WideHalf - 150.0f, i / 2 ? WideHalf + 150.0f : -WideHalf - 150.0f, 0.0f);
		Prop(i % 2 ? RockA : RockB, O + Corner, i * 77.0f, FVector(900, 900, 700), true, true);
	}
	FRandomStream Scree(14115);
	for (int32 i = 0; i < 40; ++i)
	{
		const float A = Scree.FRandRange(0.0f, 2.0f * PI);
		const float R = Scree.FRandRange(WideHalf + 300.0f, WideHalf + 1100.0f);
		Prop(i % 2 ? SmallRockA : SmallRockB, O + FVector(FMath::Cos(A) * R, FMath::Sin(A) * R, 0), Scree.FRandRange(0, 360), FVector(0, 0, Scree.FRandRange(40, 120)), true);
	}

	// The trail from the fort gate to the foot of the rock, and the route board.
	for (int32 Step = 0; Step < 9; ++Step)
	{
		Paint(Block(O + FVector(WideHalf + 1700 - 1700 + (Step + 0.5f) * (3400.0f / 9.0f), 0, 1), FVector(3400.0f / 9.0f, 360, 4), FLinearColor(0.42f, 0.32f, 0.2f)), TEXT("/Game/Dam_city/Materials/Tiled_materials/MI_soil"));
	}
	// Faces the fort (+X), where everyone arrives; it used to face the rock (Zoe, 2026-11-13).
	Sign(O + FVector(WideHalf + 600, 0, 340), 0.0f, TEXT("FOURTEENER  ->  SUMMIT 14,115 FT"), FColor(220, 235, 255), 50.0f);
	const float Board = WideHalf + 350.0f;
	Paint(Block(O + FVector(Board - 15, -520, 200), FVector(20, 620, 300), FLinearColor(0.3f, 0.19f, 0.1f)), TEXT("/Game/Beach_house/Material/MI_wood_exterior"));
	Sign(O + FVector(Board, -520, 320), 0.0f, TEXT("THE ROUTES"), FColor(255, 214, 140), 34.0f, false);
	Sign(O + FVector(Board, -520, 280), 0.0f, TEXT("EXPRESS (this face): ledges every 12 m, double rockfall"), FColor(255, 160, 140), 13.0f, false);
	Sign(O + FVector(Board, -520, 255), 0.0f, TEXT("SWITCHBACKS (other faces): ledges every 6 m, watch the ice"), FColor(170, 220, 255), 13.0f, false);
	Sign(O + FVector(Board, -520, 225), 0.0f, TEXT("Equip Climbing. HOLD left mouse to climb, let go to drop."), FColor(235, 230, 220), 13.0f, false);
	Sign(O + FVector(Board, -520, 200), 0.0f, TEXT("Chests on the ledges hold climbing gear. Tools go on your belt: press their number."), FColor(235, 230, 220), 13.0f, false);
	Sign(O + FVector(Board, -520, 175), 0.0f, TEXT("Rope in a friend from above. Rest together to pitch a camp."), FColor(235, 230, 220), 13.0f, false);
	Prop(CartA, O + FVector(WideHalf + 900, 420, 0), 15.0f, FVector(0, 0, 160), true);

	// The wider world: a mountain range on three sides, a forest across the meadow.
	Prop(RangeA, O + FVector(-30000, 0, -300), 0.0f, FVector(0, 0, 16000));
	Prop(RangeB, O + FVector(-20000, 25000, -300), 60.0f, FVector(0, 0, 12000));
	Prop(RangeA, O + FVector(-18000, -26000, -300), 140.0f, FVector(0, 0, 13000));
	Prop(RangeB, O + FVector(12000, -30000, -300), 200.0f, FVector(0, 0, 10000));
	Prop(RangeA, O + FVector(15000, 29000, -300), 300.0f, FVector(0, 0, 11000));

	FRandomStream Woods(1859);
	int32 Placed = 0;
	for (int32 Tries = 0; Tries < 600 && Placed < 120; ++Tries)
	{
		const FVector P(Woods.FRandRange(-10500.0f, 10500.0f), Woods.FRandRange(-8000.0f, 8000.0f), 0.0f);
		const FVector Fort = FortCenter() - O;
		const bool bNearMountain = FMath::Abs(P.X) < WideHalf + 1100.0f && FMath::Abs(P.Y) < WideHalf + 1100.0f;
		const bool bNearFort = FMath::Abs(P.X - Fort.X) < 2100.0f && FMath::Abs(P.Y) < 2000.0f;
		const bool bOnTrail = P.X > WideHalf && P.X < Fort.X && FMath::Abs(P.Y) < 700.0f;
		if (bNearMountain || bNearFort || bOnTrail)
		{
			continue;
		}
		const int32 Kind = Woods.RandRange(0, 9);
		const TCHAR* Mesh = Kind < 5 ? PineA : Kind < 8 ? PineB : Kind < 9 ? Tree : Bush;
		const float Height = Mesh == Bush ? Woods.FRandRange(120, 220) : Woods.FRandRange(1000, 1800);
		Prop(Mesh, O + P, Woods.FRandRange(0, 360), FVector(0, 0, Height), Mesh != Bush);
		++Placed;
	}
	for (int32 i = 0; i < 280; ++i)
	{
		const FVector P(Woods.FRandRange(-10500.0f, 10500.0f), Woods.FRandRange(-8000.0f, 8000.0f), 0.0f);
		const FVector Fort = FortCenter() - O;
		if ((FMath::Abs(P.X) < WideHalf + 200.0f && FMath::Abs(P.Y) < WideHalf + 200.0f) || (FMath::Abs(P.X - Fort.X) < 1300.0f && FMath::Abs(P.Y) < 1300.0f))
		{
			continue;
		}
		Prop(Grass, O + P, Woods.FRandRange(0, 360), FVector(0, 0, Woods.FRandRange(50, 90)));
	}
}

void ADCCArcadeClimb::BuildFort()
{
	// Fort Tiner: a log palisade with its gate toward the peak, two watchtowers,
	// three cabins, a campfire, a flag and a goat. All local, all cosmetic, built
	// from engine shapes so it needs no imported art.
	const FVector C = FortCenter();
	const float Half = 1200.0f;
	const FLinearColor Log(0.36f, 0.22f, 0.11f), DarkLog(0.22f, 0.13f, 0.07f);
	const FLinearColor Dirt(0.45f, 0.34f, 0.21f), Roof(0.3f, 0.1f, 0.08f);

	// Texture pass (2026-11-16): packed soil in 4 m tiles, so the texture keeps its scale (one 24 m block smeared it).
	for (int32 TX = 0; TX < 6; ++TX)
	{
		for (int32 TY = 0; TY < 6; ++TY)
		{
			Paint(Block(C + FVector(-Half + 200.0f + TX * 400.0f, -Half + 200.0f + TY * 400.0f, 1), FVector(400, 400, 4), Dirt), TEXT("/Game/Dam_city/Materials/Tiled_materials/MI_soil"));
		}
	}

	// The palisade: pointed logs, with a gap for the gate on the side facing the peak (-X).
	for (int32 SideIndex = 0; SideIndex < 4; ++SideIndex)
	{
		for (float T = -Half; T <= Half + 1.0f; T += 150.0f)
		{
			FVector P = SideIndex == 0 ? FVector(-Half, T, 0) : SideIndex == 1 ? FVector(Half, T, 0)
				: SideIndex == 2 ? FVector(T, -Half, 0) : FVector(T, Half, 0);
			if (SideIndex == 0 && FMath::Abs(T) < 350.0f)
			{
				continue; // the gate
			}
			const float H = 380.0f + 40.0f * FMath::Sin(T * 0.37f);
			// Bark, not flat orange (texture pass, 2026-11-16).
			Paint(Shape(TEXT("Cylinder"), C + P + FVector(0, 0, H * 0.5f), FVector(70, 70, H), Log), TEXT("/Game/Ancient_civilization/Materials/MI_tree_bark"));
			Paint(Shape(TEXT("Cone"), C + P + FVector(0, 0, H + 25.0f), FVector(70, 70, 50), DarkLog, FRotator::ZeroRotator, false), TEXT("/Game/Ancient_civilization/Materials/MI_Bark_01"));
		}
	}

	// The gate: two tall posts, a crossbeam, and the town's name.
	for (const float Y : { -380.0f, 380.0f })
	{
		Paint(Shape(TEXT("Cylinder"), C + FVector(-Half, Y, 300), FVector(90, 90, 600), DarkLog), TEXT("/Game/Ancient_civilization/Materials/MI_tree_bark"));
	}
	Paint(Block(C + FVector(-Half, 0, 560), FVector(60, 860, 70), Log), TEXT("/Game/Beach_house/Material/MI_wood_exterior"));
	// The gate sign faces into the fort, where everyone spawns (2026-11-13). The old outward-facing
	// copy showed through backwards from inside, so it is gone.
	Sign(C + FVector(-Half + 10, 0, 760), 0.0f, TEXT("FORT TINER"), FColor(255, 214, 140), 60.0f);
	Sign(C + FVector(-Half + 10, 0, 700), 0.0f, TEXT("est. 1859   pop. 6 (and a goat)"), FColor(235, 220, 200), 20.0f);
	// The inside of the gate, for crawlers setting out from the campfire. Sign text
	// shows mirrored from behind, so the outward ones need a board between them and
	// these.
	Paint(Block(C + FVector(-Half + 10, 0, 520), FVector(16, 700, 200), FLinearColor(0.3f, 0.19f, 0.1f), 0.0f, false), TEXT("/Game/Beach_house/Material/MI_wood_exterior"));
	Sign(C + FVector(-Half + 25, 0, 560), 0.0f, TEXT("TO THE SUMMIT"), FColor(255, 214, 140), 56.0f);
	Sign(C + FVector(-Half + 25, 0, 470), 0.0f, TEXT("Rest on the ledges. Come back down."), FColor(235, 220, 200), 22.0f);

	// Watchtowers on the two front corners, with flags, and braziers at the gate.
	using namespace FortTinerArt;
	for (const float Y : { -Half, Half })
	{
		const FVector T = C + FVector(-Half, Y, 0);
		if (!Prop(Tower, T, Y > 0 ? 0.0f : 180.0f, FVector(0, 0, 820), true))
		{
			for (const FVector Leg : { FVector(-90, -90, 0), FVector(90, -90, 0), FVector(-90, 90, 0), FVector(90, 90, 0) })
			{
				Shape(TEXT("Cylinder"), T + Leg + FVector(0, 0, 350), FVector(40, 40, 700), DarkLog);
			}
			Paint(Block(T + FVector(0, 0, 640), FVector(280, 280, 30), Log), TEXT("/Game/Beach_house/Material/MI_wood_exterior"));
			Shape(TEXT("Cone"), T + FVector(0, 0, 820), FVector(360, 360, 220), Roof, FRotator::ZeroRotator, false);
		}
		Prop(Flag, T + FVector(0, 0, 820), 90.0f, FVector(0, 0, 320));
		Prop(Brazier, C + FVector(-Half - 220, Y > 0 ? 470.0f : -470.0f, 0), 0.0f, FVector(0, 0, 130), true);
		Light(C + FVector(-Half - 220, Y > 0 ? 470.0f : -470.0f, 200), FLinearColor(1.0f, 0.55f, 0.2f), 5000.0f, 800.0f);
	}

	// Cabins: the trading post, the bunkhouse and the assay office.
	struct FCabin { FVector At; FVector Size; float Yaw; const TCHAR* Name; };
	const FCabin Cabins[] = {
		{ FVector(250, Half - 330, 0), FVector(700, 460, 320), 0.0f, TEXT("TRADING POST") },
		{ FVector(Half - 330, 0, 0), FVector(460, 800, 300), 0.0f, TEXT("BUNKHOUSE") },
		{ FVector(250, -Half + 330, 0), FVector(600, 460, 300), 0.0f, TEXT("ASSAY OFFICE") },
	};
	int32 CabinIndex = 0;
	for (const FCabin& Cabin : Cabins)
	{
		const FVector P = C + Cabin.At;
		const FVector ToFireDir = (C - P).GetSafeNormal2D();
		// A real log cabin from the Viking pack, facing the fire; the old box
		// cabin only if the pack is missing.
		if (Prop(CabinIndex++ % 2 ? Cabin2 : Cabin1, P, ToFireDir.Rotation().Yaw - 90.0f, FVector(Cabin.Size.X + 160, Cabin.Size.Y + 160, 0), true))
		{
			const float SignYaw = ToFireDir.Rotation().Yaw;
			Sign(P + ToFireDir * (FMath::Abs(ToFireDir.X) > 0.5f ? Cabin.Size.X : Cabin.Size.Y) * 0.6f + FVector(0, 0, 380), SignYaw, Cabin.Name, FColor(255, 225, 170), 34.0f);
			continue;
		}
		Block(P + FVector(0, 0, Cabin.Size.Z * 0.5f), Cabin.Size, Log);
		// A pitched roof from two tilted slabs along the cabin's long side.
		const bool bLongX = Cabin.Size.X >= Cabin.Size.Y;
		const float Span = bLongX ? Cabin.Size.Y : Cabin.Size.X;
		for (const float Side : { -1.0f, 1.0f })
		{
			const FVector Offset = bLongX ? FVector(0, Side * Span * 0.25f, 0) : FVector(Side * Span * 0.25f, 0, 0);
			const FRotator Tilt = bLongX ? FRotator(0, 0, -Side * 28.0f) : FRotator(Side * 28.0f, 0, 0);
			const FVector Size = bLongX ? FVector(Cabin.Size.X + 60, Span * 0.62f, 24) : FVector(Span * 0.62f, Cabin.Size.Y + 60, 24);
			Shape(TEXT("Cube"), P + Offset + FVector(0, 0, Cabin.Size.Z + Span * 0.13f), Size, Roof, Tilt);
		}
		// A door facing the campfire, and the name over it.
		const FVector ToFire = (C - P).GetSafeNormal2D();
		const FVector DoorAt = P + FVector(ToFire.X * Cabin.Size.X * 0.5f, ToFire.Y * Cabin.Size.Y * 0.5f, 110);
		Block(DoorAt, FVector(FMath::Abs(ToFire.X) > 0.5f ? 12 : 120, FMath::Abs(ToFire.Y) > 0.5f ? 12 : 120, 220), DarkLog, 0.0f, false);
		const float SignYaw = ToFire.Rotation().Yaw;
		Sign(DoorAt + ToFire * 20.0f + FVector(0, 0, 170), SignYaw, Cabin.Name, FColor(255, 225, 170), 30.0f);
	}

	// The Trading Post's counter (2026-11-15, Zoe: "it'd be nice if you could just buy items in the base camp"):
	// a lit pad at its door. Step on it and the counter opens on its tab: climbing kit for tickets, this round only.
	{
		const FVector Pad = TradingPostPad();
		Block(Pad + FVector(0, 0, 3), FVector(360, 360, 4), FLinearColor(0.85f, 0.55f, 0.15f), 0.0f, false);
		Paint(Block(Pad + FVector(0, 150, 55), FVector(300, 50, 110), FLinearColor(0.36f, 0.22f, 0.11f)), TEXT("/Game/Beach_house/Material/MI_wood_exterior")); // the counter
		Prop(CampCrates, Pad + FVector(-230, 200, 0), 20.0f, FVector(0, 0, 110), true);
		Sign(Pad + FVector(0, 130, 140), -90.0f, TEXT("BUY HERE"), FColor(255, 214, 140), 18.0f);
		Light(Pad + FVector(0, 0, 260), FLinearColor(1.0f, 0.7f, 0.35f), 3000.0f, 700.0f);
	}

	// The campfire: a ring of stones, firewood, flames, and a light that flickers.
	for (int32 i = 0; i < 10; ++i)
	{
		const float A = 2.0f * PI * i / 10.0f;
		if (!Prop(i % 2 ? SmallRockA : SmallRockB, C + FVector(FMath::Cos(A) * 115.0f, FMath::Sin(A) * 115.0f, 0), i * 37.0f, FVector(0, 0, 45)))
		{
			Shape(TEXT("Sphere"), C + FVector(FMath::Cos(A) * 110.0f, FMath::Sin(A) * 110.0f, 18), FVector(55, 55, 40), FLinearColor(0.35f, 0.34f, 0.33f), FRotator::ZeroRotator, false);
		}
	}
	if (!Prop(Firewood, C, 0.0f, FVector(170, 170, 0)))
	{
		Shape(TEXT("Cylinder"), C + FVector(0, 0, 25), FVector(26, 26, 170), DarkLog, FRotator(90, 0, 0), false);
		Shape(TEXT("Cylinder"), C + FVector(0, 0, 32), FVector(26, 26, 170), DarkLog, FRotator(90, 90, 0), false);
	}
	Shape(TEXT("Cone"), C + FVector(0, 0, 80), FVector(90, 90, 120), FLinearColor(3.0f, 1.0f, 0.15f), FRotator::ZeroRotator, false);
	Shape(TEXT("Cone"), C + FVector(15, 10, 70), FVector(50, 50, 80), FLinearColor(3.0f, 2.2f, 0.4f), FRotator::ZeroRotator, false);
	if (UWorld* World = GetWorld())
	{
		FActorSpawnParameters Params;
		Params.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
		if (APointLight* Fire = World->SpawnActor<APointLight>(C + FVector(0, 0, 160), FRotator::ZeroRotator, Params))
		{
			Fire->SetMobility(EComponentMobility::Movable);
			Fire->PointLightComponent->SetLightColor(FLinearColor(1.0f, 0.55f, 0.2f));
			Fire->PointLightComponent->SetIntensity(14000.0f);
			Fire->PointLightComponent->SetAttenuationRadius(1500.0f);
			Fire->PointLightComponent->SetCastShadows(false);
			LocalActors.Add(Fire);
			Campfire = Fire;
		}
	}
	// Log benches around the fire, leaving the gate side open.
	for (const float A : { 0.0f, 90.0f, 270.0f })
	{
		const FVector Dir = FRotator(0, A, 0).Vector();
		if (!Prop(Bench, C + Dir * 310.0f, A + 90.0f, FVector(300, 300, 0), true))
		{
			Shape(TEXT("Cylinder"), C + Dir * 300.0f + FVector(0, 0, 25), FVector(50, 50, 260), Log, FRotator(90, A + 90.0f, 0));
		}
	}

	// The blacksmith, by the assay office: forge, anvil, bellows, grinding wheel, the tool rack.
	Prop(Forge, C + FVector(820, -620, 0), 180.0f, FVector(0, 0, 280), true);
	Light(C + FVector(820, -620, 160), FLinearColor(1.0f, 0.45f, 0.15f), 4000.0f, 600.0f);
	Prop(Anvil, C + FVector(620, -470, 0), 30.0f, FVector(0, 0, 90), true);
	Prop(Bellows, C + FVector(990, -460, 0), 270.0f, FVector(0, 0, 110), true);
	Prop(Grinder, C + FVector(560, -780, 0), 0.0f, FVector(0, 0, 140), true);
	Prop(Rack, C + FVector(1080, -780, 0), 270.0f, FVector(0, 0, 210), true);

	// Supplies and camp life.
	Prop(Barrels, C + FVector(-120, 860, 0), 20.0f, FVector(0, 0, 130), true);
	Prop(Crates, C + FVector(640, 860, 0), 75.0f, FVector(0, 0, 140), true);
	Prop(Tables, C + FVector(-760, -200, 0), 0.0f, FVector(320, 320, 0), true); // clear of the start ring around the fire
	Prop(Tents, C + FVector(-620, -880, 0), 60.0f, FVector(460, 460, 0), true);
	Prop(Sack, C + FVector(700, 640, 0), 0.0f, FVector(0, 0, 70));
	Prop(Sack, C + FVector(760, 690, 0), 40.0f, FVector(0, 0, 60));
	Prop(CampCrates, C + FVector(Half - 220, -250, 0), 90.0f, FVector(0, 0, 130), true);
	Prop(Pulley, C + FVector(-450, 650, 0), 0.0f, FVector(0, 0, 260), true);
	Prop(CartB, C + FVector(-700, 870, 0), 20.0f, FVector(0, 0, 170), true);
	for (const float Y : { 450.0f, -450.0f })
	{
		Prop(Ladder, C + FVector(Half - 90, Y, 0), 180.0f, FVector(0, 0, 420));
	}

	// The flag, the well and the goat.
	Shape(TEXT("Cylinder"), C + FVector(-500, -650, 450), FVector(18, 18, 900), FLinearColor(0.8f, 0.8f, 0.8f));
	Block(C + FVector(-500, -560, 820), FVector(6, 180, 110), FLinearColor(0.7f, 0.12f, 0.12f), 0.0f, false);
	Sign(C + FVector(-505, -560, 805), 180.0f, TEXT("FT"), FColor(255, 255, 255), 40.0f);
	Paint(Shape(TEXT("Cylinder"), C + FVector(-450, 650, 60), FVector(160, 160, 120), FLinearColor(0.45f, 0.44f, 0.42f)), TEXT("/Game/Abandonned_Brutalist/Materials/MI_stones"));
	Paint(Block(C + FVector(-450, 650, 260), FVector(200, 30, 20), Log, 0.0f, false), TEXT("/Game/Beach_house/Material/MI_wood_exterior"));
	{
		const FVector G = C + FVector(Half - 320, Half - 300, 0);
		const FLinearColor Goat(0.9f, 0.88f, 0.82f);
		Block(G + FVector(0, 0, 70), FVector(110, 50, 50), Goat);
		Block(G + FVector(65, 0, 105), FVector(40, 32, 40), Goat, 0.0f, false);
		for (const FVector Leg : { FVector(-40, -18, 0), FVector(40, -18, 0), FVector(-40, 18, 0), FVector(40, 18, 0) })
		{
			Shape(TEXT("Cylinder"), G + Leg + FVector(0, 0, 25), FVector(12, 12, 50), Goat * 0.8f, FRotator::ZeroRotator, false);
		}
		Shape(TEXT("Cone"), G + FVector(70, -10, 135), FVector(8, 8, 26), FLinearColor(0.3f, 0.25f, 0.2f), FRotator(0, 0, -15), false);
		Shape(TEXT("Cone"), G + FVector(70, 10, 135), FVector(8, 8, 26), FLinearColor(0.3f, 0.25f, 0.2f), FRotator(0, 0, 15), false);
		Sign(G + FVector(0, 0, 190), 180.0f, TEXT("THE MAYOR"), FColor(255, 240, 200), 18.0f);
	}

	// Lanterns on posts along the inside of the walls.
	for (const FVector P : { FVector(-Half + 200, -700, 0), FVector(-Half + 200, 700, 0), FVector(Half - 200, -900, 0), FVector(Half - 200, 900, 0) })
	{
		Shape(TEXT("Cylinder"), C + P + FVector(0, 0, 130), FVector(16, 16, 260), DarkLog, FRotator::ZeroRotator, false);
		Shape(TEXT("Sphere"), C + P + FVector(0, 0, 270), FVector(30, 30, 36), FLinearColor(3.0f, 2.0f, 0.6f), FRotator::ZeroRotator, false);
		Light(C + P + FVector(0, 0, 280), FLinearColor(1.0f, 0.75f, 0.4f), 2500.0f, 700.0f);
	}
}

void ADCCArcadeClimb::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);
	DOREPLIFETIME(ADCCArcadeClimb, Rocks);
	DOREPLIFETIME(ADCCArcadeClimb, GustFace);
	DOREPLIFETIME(ADCCArcadeClimb, bGustActive);
	DOREPLIFETIME(ADCCArcadeClimb, Camps);
}

void ADCCArcadeClimb::ClientTick(float DeltaSeconds)
{
	// The Trading Post pad opens the counter on its tab, once per step onto it.
	if (APlayerController* PadPC = GetWorld() ? GetWorld()->GetFirstPlayerController() : nullptr)
	{
		const APawn* PadPawn = PadPC->GetPawn();
		const FVector D = PadPawn ? PadPawn->GetActorLocation() - TradingPostPad() : FVector(1e6f);
		const bool bOnPad = FMath::Abs(D.X) <= 200.0f && FMath::Abs(D.Y) <= 200.0f && FMath::Abs(D.Z) < 300.0f;
		if (bOnPad && !bWasOnTradingPad)
		{
			if (ADCCHUD* Hud = Cast<ADCCHUD>(PadPC->GetHUD()))
			{
				Hud->OpenCosmeticShop(4);
			}
		}
		bWasOnTradingPad = bOnPad;
	}
	UWorld* World = GetWorld();
	if (!World)
	{
		return;
	}
	FDCCClimbAmbience::Tick(this, DeltaSeconds);
	const double Clock = ADCCNeonTrafficCar::ClockOf(World);
	const float T = static_cast<float>(World->GetTimeSeconds());

	if (APointLight* Fire = Campfire.Get())
	{
		const float Flicker = 0.8f + 0.12f * FMath::Sin(T * 9.0f) + 0.08f * FMath::Sin(T * 23.0f + 1.3f);
		Fire->PointLightComponent->SetIntensity(14000.0f * Flicker);
	}

	// Rocks: glowing at the top until their time, then falling to where they stop.
	TSet<int32> Live;
	for (const FDCCRockfall& Rock : Rocks)
	{
		Live.Add(Rock.Id);
		TWeakObjectPtr<AActor>& Visual = RockVisuals.FindOrAdd(Rock.Id);
		if (!Visual.IsValid())
		{
			Visual = Shape(TEXT("Sphere"), Rock.Start, FVector(120, 110, 100), FLinearColor(0.38f, 0.33f, 0.3f), FRotator::ZeroRotator, false);
		}
		AActor* Actor = Visual.Get();
		if (!Actor)
		{
			continue;
		}
		const float Age = static_cast<float>(Clock - Rock.StartTime);
		if (Age < 0.0f)
		{
			// The warning: it shakes in place at the top.
			Actor->SetActorLocation(Rock.Start + FVector(FMath::Sin(T * 40.0f) * 8.0f, FMath::Cos(T * 33.0f) * 8.0f, 0.0f));
		}
		else
		{
			const float Z = FMath::Max(Rock.StopZ, Rock.Start.Z - 0.5f * Gravity * Age * Age);
			Actor->SetActorLocationAndRotation(FVector(Rock.Start.X, Rock.Start.Y, Z), FRotator(Age * 200.0f, Age * 90.0f, 0.0f));
		}
	}
	for (auto It = RockVisuals.CreateIterator(); It; ++It)
	{
		if (!Live.Contains(It.Key()))
		{
			if (AActor* Actor = It.Value().Get())
			{
				Actor->Destroy();
			}
			It.RemoveCurrent();
		}
	}

	// Gust: white streaks sweeping across the face it is hitting.
	const bool bShowGust = bGustActive && GustFace >= 0;
	if (bShowGust && GustStreaks.IsEmpty())
	{
		for (int32 i = 0; i < 14; ++i)
		{
			GustStreaks.Add(Shape(TEXT("Cube"), GetActorLocation(), FVector(10, 10, 10), FLinearColor(2.0f, 2.0f, 2.2f), FRotator::ZeroRotator, false));
		}
	}
	else if (!bShowGust && !GustStreaks.IsEmpty())
	{
		for (TWeakObjectPtr<AActor>& Streak : GustStreaks)
		{
			if (AActor* Actor = Streak.Get()) { Actor->Destroy(); }
		}
		GustStreaks.Reset();
	}
	if (bShowGust)
	{
		const FVector N = FaceNormal(GustFace), Along = FaceAlong(GustFace);
		for (int32 i = 0; i < GustStreaks.Num(); ++i)
		{
			AActor* Actor = GustStreaks[i].Get();
			if (!Actor) { continue; }
			const float Z = 300.0f + (i * 680.0f + 140.0f * FMath::Sin(T + i));
			const float Phase = FMath::Fmod(T * 1400.0f + i * 517.0f, WideHalf * 2.0f) - WideHalf;
			const FVector At = GetActorLocation() + N * (HalfWidthAt(FMath::Fmod(Z, TopZ)) + 140.0f + (i % 3) * 60.0f) + Along * Phase + FVector(0, 0, FMath::Fmod(Z, TopZ));
			Actor->SetActorLocation(At);
			Actor->SetActorRotation(Along.Rotation());
			Actor->SetActorScale3D(FVector(3.2f, 0.06f, 0.06f));
		}
	}

	// Camps: a tent and a small flag at each one.
	for (; CampsBuilt < Camps.Num(); ++CampsBuilt)
	{
		const FVector At = Camps[CampsBuilt];
		Shape(TEXT("Cone"), At + FVector(0, 0, 70), FVector(180, 180, 140), FLinearColor(1.0f, 0.45f, 0.1f), FRotator::ZeroRotator, false);
		Shape(TEXT("Cylinder"), At + FVector(80, 60, 110), FVector(6, 6, 220), FLinearColor(0.85f, 0.85f, 0.85f), FRotator::ZeroRotator, false);
		Block(At + FVector(80, 25, 200), FVector(4, 70, 40), FLinearColor(0.2f, 0.8f, 0.4f), 0.0f, false);
		Light(At + FVector(0, 0, 160), FLinearColor(1.0f, 0.7f, 0.4f), 3000.0f, 500.0f);
	}

	// Ropes: drawn on every machine from what it can already see (who is
	// climbing, who is standing), by the same rule the server uses.
	TArray<TPair<FVector, FVector>> Pairs;
	TArray<ADCCCharacterBase*> Crawlers;
	for (TActorIterator<ADCCCharacterBase> It(World); It; ++It)
	{
		if (It->IsPlayerControlled() || It->GetPlayerState())
		{
			Crawlers.Add(*It);
		}
	}
	for (ADCCCharacterBase* Climber : Crawlers)
	{
		if (!Climber->IsClimbing())
		{
			continue;
		}
		for (ADCCCharacterBase* Belayer : Crawlers)
		{
			const FVector D = Belayer->GetActorLocation() - Climber->GetActorLocation();
			if (Belayer != Climber && IsStanding(Belayer) && D.Z > 150.0f && D.Z < 1200.0f && D.Size2D() < 500.0f)
			{
				Pairs.Add({ Belayer->GetActorLocation() + FVector(0, 0, 30), Climber->GetActorLocation() + FVector(0, 0, 50) });
				break;
			}
		}
	}
	while (RopeVisuals.Num() < Pairs.Num())
	{
		RopeVisuals.Add(Shape(TEXT("Cylinder"), GetActorLocation(), FVector(5, 5, 100), FLinearColor(0.95f, 0.75f, 0.2f), FRotator::ZeroRotator, false));
	}
	for (int32 i = 0; i < RopeVisuals.Num(); ++i)
	{
		AActor* Rope = RopeVisuals[i].Get();
		if (!Rope) { continue; }
		Rope->SetActorHiddenInGame(i >= Pairs.Num());
		if (i < Pairs.Num())
		{
			const FVector From = Pairs[i].Key, To = Pairs[i].Value;
			const FVector Dir = (To - From);
			Rope->SetActorLocationAndRotation((From + To) * 0.5f, FRotationMatrix::MakeFromZ(Dir.GetSafeNormal()).Rotator());
			Rope->SetActorScale3D(FVector(0.05f, 0.05f, Dir.Size() / 100.0f));
		}
	}
}

bool ADCCArcadeClimb::IsStanding(const ADCCCharacterBase* Crawler) const
{
	const UCharacterMovementComponent* Movement = Crawler ? Crawler->GetCharacterMovement() : nullptr;
	return Crawler && Crawler->IsAlive() && !Crawler->IsClimbing() && Movement && Movement->IsMovingOnGround();
}

void ADCCArcadeClimb::Warn(int32 Face, const FString& Message)
{
	// Hazards are heard now, not read (Zoe, 2026-11-13): FDCCClimbAmbience plays the
	// rock cracking loose where it is and the gust whooshing past, and shakes the
	// screen when a rock lands close. Logged for debugging only.
	UE_LOG(LogTemp, Verbose, TEXT("[DCC Climb] face %d: %s"), Face, *Message);
}

void ADCCArcadeClimb::QueueRock()
{
	// Twice as likely on the express face. Half from the summit, half from
	// High Camp's lip, so both halves of the climb get them.
	const int32 Face = FMath::FRand() < 0.4f ? 0 : 1 + FMath::RandRange(0, 2);
	const bool bUpper = FMath::FRand() < 0.5f;
	const float TopOfFall = bUpper ? TopZ + 120.0f : MidZ + 120.0f;
	const FVector N = FaceNormal(Face), T = FaceAlong(Face);
	const float Out = (bUpper ? NarrowHalf : WideHalf) + 75.0f;
	const float Halfspan = (bUpper ? NarrowHalf : WideHalf) - 70.0f;
	const FVector Start = GetActorLocation() + N * Out + T * FMath::FRandRange(-Halfspan, Halfspan) + FVector(0, 0, TopOfFall);

	// Where it will stop: the first thing below it (a ledge, High Camp, the ground).
	float StopZ = GetActorLocation().Z + 55.0f;
	FHitResult Hit;
	FCollisionQueryParams Params;
	for (TActorIterator<ACharacter> It(GetWorld()); It; ++It)
	{
		Params.AddIgnoredActor(*It);
	}
	if (GetWorld()->SweepSingleByChannel(Hit, Start, Start - FVector(0, 0, TopOfFall + 500.0f), FQuat::Identity, ECC_WorldStatic,
		FCollisionShape::MakeSphere(45.0f), Params))
	{
		StopZ = Hit.Location.Z + 10.0f;
	}

	FDCCRockfall Rock;
	Rock.Id = NextRockId++;
	Rock.Start = Start;
	Rock.StartTime = ADCCNeonTrafficCar::ClockOf(GetWorld()) + 1.5; // 1.5 s of shaking at the top first
	Rock.StopZ = StopZ;
	Rocks.Add(Rock);
	Warn(Face, FString::Printf(TEXT("ROCKFALL on the %s face! Get under a ledge."), *FaceName(Face)));
}

void ADCCArcadeClimb::TickRocks()
{
	const double Clock = ADCCNeonTrafficCar::ClockOf(GetWorld());
	for (int32 i = Rocks.Num() - 1; i >= 0; --i)
	{
		const FDCCRockfall& Rock = Rocks[i];
		const float Age = static_cast<float>(Clock - Rock.StartTime);
		if (Age < 0.0f)
		{
			continue;
		}
		const float Z = Rock.Start.Z - 0.5f * Gravity * Age * Age;
		if (Z <= Rock.StopZ)
		{
			if (Age > FMath::Sqrt(2.0f * FMath::Max(0.0f, Rock.Start.Z - Rock.StopZ) / Gravity) + 1.5f)
			{
				Rocks.RemoveAt(i); // rested a moment, gone
			}
			continue;
		}
		const FVector At(Rock.Start.X, Rock.Start.Y, Z);
		for (TActorIterator<ADCCCharacterBase> It(GetWorld()); It; ++It)
		{
			ADCCCharacterBase* Crawler = *It;
			const uint64 Key = (uint64(Rock.Id) << 32) | uint64(Crawler->GetUniqueID());
			if (!Crawler->IsPlayerControlled() || !Crawler->IsAlive() || RockHits.Contains(Key)
				|| FVector::Dist(Crawler->GetActorLocation(), At) > 120.0f)
			{
				continue;
			}
			RockHits.Add(Key);
			FClimbKit* Kit = Crawler->GetPlayerState() ? Kits.Find(Crawler->GetPlayerState()) : nullptr;
			if (Kit && Kit->HardHats > 0)
			{
				--Kit->HardHats;
				SyncKit(Crawler);
				Crawler->NotifyClimber(NSLOCTEXT("DCC", "RockHat", "CLONK. The rock bounces off your hard hat. You keep your grip."));
				continue;
			}
			if (Crawler->Stats)
			{
				Crawler->Stats->ApplyDamage(FMath::Max(1, FMath::CeilToInt(Crawler->Stats->MaxHitPoints * 0.15f)));
			}
			Crawler->ForceReleaseClimb();
			Crawler->NotifyClimber(NSLOCTEXT("DCC", "RockHit", "A rock clips you and knocks you off the wall!"));
		}
	}
}

static TAutoConsoleVariable<int32> CVarClimbHazards(TEXT("dcc.ClimbHazards"), 1,
	TEXT("Fourteener rockfall and gusts: 1 on, 0 off. DCC.ClimbTest turns them off while it runs."));

void ADCCArcadeClimb::TickHazardsAndTeam(float DeltaSeconds)
{
	UWorld* World = GetWorld();
	const double Now = ServerTime();
	const bool bHazards = CVarClimbHazards.GetValueOnGameThread() != 0;

	// --- schedule hazards ---------------------------------------------------
	if (bHazards && Now >= NextRockAt)
	{
		QueueRock();
		NextRockAt = Now + FMath::FRandRange(6.0f, 10.0f);
	}
	if (bHazards && GustFace < 0 && Now >= NextGustAt)
	{
		GustFace = FMath::RandRange(0, 3);
		bGustActive = false;
		GustStartsAt = Now + 3.0;
		GustEndsAt = GustStartsAt + 4.0;
		Warn(GustFace, FString::Printf(TEXT("A GUST is coming for the %s face in 3 s. Find a ledge!"), *FaceName(GustFace)));
	}
	if (GustFace >= 0 && !bGustActive && Now >= GustStartsAt)
	{
		bGustActive = true;
		// Heard, not read (2026-11-13): FDCCClimbAmbience plays the whoosh and raises the wind.
	}
	if (GustFace >= 0 && bGustActive && Now >= GustEndsAt)
	{
		GustFace = -1;
		bGustActive = false;
		NextGustAt = Now + FMath::FRandRange(18.0f, 32.0f);
	}

	// --- per player: multipliers, belays, checkpoints -----------------------
	TArray<ADCCCharacterBase*> Crawlers;
	for (TActorIterator<ADCCCharacterBase> It(World); It; ++It)
	{
		if (It->IsPlayerControlled() && It->IsAlive())
		{
			Crawlers.Add(*It);
			// Another 15% on the 2026-11-14 ease (0.85 -> 0.74). The first ledge was still
			// out of reach. Ice, gusts, a belay and chalk stack on this same baseline.
			// And down to 0.5 (2026-11-16, Zoe: "I still can't reliably make it up to the first step").
			It->ClimbStaminaMultiplier = 0.5f;
			It->StaminaRegenMultiplier = 1.0f;
			It->ClimbStaminaReferenceMax = 120.0f; // the fair bar: see ADCCCharacterBase::ClimbStaminaReferenceMax
		}
	}

	for (ADCCCharacterBase* Crawler : Crawlers)
	{
		const FVector At = Crawler->GetActorLocation();
		const FVector Local = At - GetActorLocation();
		const int32 Face = FaceOf(At);
		APlayerState* PlayerState = Crawler->GetPlayerState();

		if (Crawler->IsClimbing())
		{
			// Ice.
			const FVector T = FaceAlong(Face);
			const float Lateral = FVector::DotProduct(Local, T);
			for (const FIce& Ice : IcePatches())
			{
				if (Ice.Face == Face && Local.Z >= Ice.ZMin - 80.0f && Local.Z <= Ice.ZMax + 80.0f && FMath::Abs(Lateral - Ice.Lateral) <= Ice.Width * 0.5f + 40.0f)
				{
					const FClimbKit* Kit = PlayerState ? Kits.Find(PlayerState) : nullptr;
					Crawler->ClimbStaminaMultiplier *= Kit && Kit->bCrampons ? 1.0f : 2.0f;
					break;
				}
			}
			// Gust.
			if (bGustActive && Face == GustFace)
			{
				const FClimbKit* Kit = PlayerState ? Kits.Find(PlayerState) : nullptr;
				const bool bCloak = Kit && Kit->bWindCloak;
				Crawler->ClimbStaminaMultiplier *= bCloak ? 1.0f : 3.0f;
				// The shove itself is applied on the server AND the climber's machine (FDCCClimbAmbience),
				// so prediction does not erase it. Here: a tired climber can be torn off the wall.
				if (!bCloak && Crawler->Stats && Crawler->Stats->MaxStamina > 0.0f
					&& Crawler->Stats->CurrentStamina < Crawler->Stats->MaxStamina * 0.3f && FMath::FRand() < World->GetDeltaSeconds() * 0.8f)
				{
					Crawler->ForceReleaseClimb();
				}
			}
			// Roped in by someone standing above.
			for (ADCCCharacterBase* Belayer : Crawlers)
			{
				const FVector D = Belayer->GetActorLocation() - At;
				if (Belayer != Crawler && IsStanding(Belayer) && D.Z > 150.0f && D.Z < 1200.0f && D.Size2D() < 500.0f)
				{
					Crawler->ClimbStaminaMultiplier *= 0.6f;
					Crawler->FallSafeUntil = World->GetTimeSeconds() + 0.8; // still caught for a moment after letting go
					if (APlayerState* BelayerState = Belayer->GetPlayerState())
					{
						float& Credit = BelayCredit.FindOrAdd(BelayerState);
						Credit += DeltaSeconds;
						if (Credit >= 2.0f)
						{
							Credit -= 2.0f;
							AddPoints(BelayerState, 1, TEXT("belaying a teammate"));
						}
					}
					break;
				}
			}
		}

		// High Camp: standing on the shelf where the mountain narrows.
		const float Ring = FMath::Max(FMath::Abs(Local.X), FMath::Abs(Local.Y));
		if (PlayerState && IsStanding(Crawler) && Local.Z > MidZ - 20.0f && Local.Z < MidZ + 300.0f && Ring > NarrowHalf && Ring < WideHalf + 20.0f)
		{
			if (!ReachedHighCamp.Contains(PlayerState))
			{
				ReachedHighCamp.Add(PlayerState);
				AddPoints(PlayerState, 20, TEXT("High Camp"));
				Crawler->NotifyClimber(NSLOCTEXT("DCC", "HighCamp", "High Camp! Fall from here on and you come back here."));
			}
			const FVector* Current = Checkpoints.Find(PlayerState);
			if (!Current || Current->Z < At.Z - 50.0f)
			{
				Checkpoints.Add(PlayerState, At);
			}
		}

		// Camp rest.
		for (const FVector& Camp : Camps)
		{
			if (FVector::Dist(Camp, At) < 450.0f && IsStanding(Crawler))
			{
				Crawler->StaminaRegenMultiplier = 2.0f;
				break;
			}
		}
	}

	TickKits(DeltaSeconds);

	// --- camps: two or more resting together on a ledge for a few seconds ---
	TSet<FIntVector> Touched;
	for (int32 i = 0; i < Crawlers.Num(); ++i)
	{
		ADCCCharacterBase* A = Crawlers[i];
		if (!IsStanding(A) || A->GetActorLocation().Z - GetActorLocation().Z < 400.0f)
		{
			continue; // the ground and the fort do not count
		}
		for (int32 j = i + 1; j < Crawlers.Num(); ++j)
		{
			ADCCCharacterBase* B = Crawlers[j];
			const FVector D = B->GetActorLocation() - A->GetActorLocation();
			if (!IsStanding(B) || D.Size2D() > 400.0f || FMath::Abs(D.Z) > 100.0f)
			{
				continue;
			}
			const FVector Mid = (A->GetActorLocation() + B->GetActorLocation()) * 0.5f;
			const FIntVector Cell(FMath::RoundToInt(Mid.X / 300.0f), FMath::RoundToInt(Mid.Y / 300.0f), FMath::RoundToInt(Mid.Z / 300.0f));
			Touched.Add(Cell);
			const bool bNearCamp = Camps.ContainsByPredicate([&Mid](const FVector& C) { return FVector::Dist(C, Mid) < 600.0f; });
			if (bNearCamp || Camps.Num() >= 6)
			{
				continue;
			}
			float& Progress = CampProgress.FindOrAdd(Cell);
			Progress += DeltaSeconds;
			if (Progress >= 3.0f)
			{
				const FVector CampAt = Mid - FVector(0, 0, 88.0f); // on the ledge, not at the hips
				Camps.Add(CampAt);
				for (ADCCCharacterBase* Camper : Crawlers)
				{
					APlayerState* PlayerState = Camper->GetPlayerState();
					if (PlayerState && FVector::Dist(Camper->GetActorLocation(), Mid) < 500.0f)
					{
						AddPoints(PlayerState, 25, TEXT("pitched a camp"));
						Camper->NotifyClimber(NSLOCTEXT("DCC", "CampPitched", "Camp pitched! Rest here twice as fast, and the crew respawns here."));
					}
				}
				// A camp is everyone's checkpoint if it is higher than theirs.
				for (ADCCCharacterBase* Anyone : Crawlers)
				{
					if (APlayerState* PlayerState = Anyone->GetPlayerState())
					{
						const FVector* Current = Checkpoints.Find(PlayerState);
						if (!Current || Current->Z < CampAt.Z)
						{
							Checkpoints.Add(PlayerState, CampAt + FVector(120, 0, 100));
						}
					}
				}
			}
		}
	}
	for (auto It = CampProgress.CreateIterator(); It; ++It)
	{
		if (!Touched.Contains(It.Key()))
		{
			It.RemoveCurrent();
		}
	}
}

void ADCCArcadeClimb::OnRespawned(APlayerController* PC)
{
	APawn* Pawn = PC ? PC->GetPawn() : nullptr;
	const FVector* Checkpoint = PC && PC->PlayerState ? Checkpoints.Find(PC->PlayerState) : nullptr;
	if (!Pawn || !Checkpoint)
	{
		return;
	}
	Pawn->TeleportTo(*Checkpoint + FVector(0, 0, 100), FRotator(0, 180, 0), false, true);
	if (ADCCCharacterBase* Crawler = Cast<ADCCCharacterBase>(Pawn))
	{
		Crawler->FallSafeUntil = GetWorld()->GetTimeSeconds() + 3.0;
		Crawler->NotifyClimber(NSLOCTEXT("DCC", "Checkpoint", "Back at your checkpoint. Careful this time."));
	}
}

void ADCCArcadeClimb::SpawnTownsfolk()
{
	struct FPlace { const TCHAR* Who; FVector At; float Yaw; };
	const FVector C = FortCenter();
	const FPlace Places[] = {
		{ TEXT("Dusty"), C + FVector(-1000, 520, 100), 200.0f },  // by the gate
		{ TEXT("MaOre"), C + FVector(250, 700, 100), -90.0f },    // on the trading post's porch
		{ TEXT("Gus"), C + FVector(250, -280, 100), 150.0f },     // at the campfire
		{ TEXT("Bea"), C + FVector(-1000, -520, 100), 160.0f },   // by the gate, the other side
	};
	for (const FPlace& Place : Places)
	{
		const FTransform Where(FRotator(0, Place.Yaw, 0), Place.At);
		if (ADCCFortTinerNPC* NPC = GetWorld()->SpawnActorDeferred<ADCCFortTinerNPC>(ADCCFortTinerNPC::StaticClass(), Where,
			nullptr, nullptr, ESpawnActorCollisionHandlingMethod::AdjustIfPossibleButAlwaysSpawn))
		{
			NPC->Who = FName(Place.Who);
			NPC->FinishSpawning(Where);
		}
	}
}

void ADCCArcadeClimb::ServerBegin()
{
	StartedAt = ServerTime();
	NextRockAt = StartedAt + 20.0;   // a calm start, so people can leave the fort
	NextGustAt = StartedAt + 35.0;
	SpawnTownsfolk();
	SpawnChests();
	SpawnBirds();
}

// ---------------------------------------------------------------------------
// Fourteener's items (2026-11-13): chests, tools, boosts, artifacts
// ---------------------------------------------------------------------------

void ADCCArcadeClimb::SpawnChests()
{
	UWorld* World = GetWorld();
	const FVector O = GetActorLocation();
	auto Spawn = [World](const FVector& At, float Yaw, EDCCChestKind Kind, FName Artifact)
	{
		const FTransform Where(FRotator(0, Yaw, 0), At);
		if (ADCCSupplyChest* Chest = World->SpawnActorDeferred<ADCCSupplyChest>(ADCCSupplyChest::StaticClass(), Where,
			nullptr, nullptr, ESpawnActorCollisionHandlingMethod::AlwaysSpawn))
		{
			Chest->Kind = Kind;
			Chest->ArtifactId = Artifact;
			Chest->FinishSpawning(Where);
		}
	};

	// (The Outfitter's Crate, "one of everything", was a dev chest: removed 2026-11-16 at Zoe's request. The Trading
	// Post sells kit for tickets instead.)
	// Food at the fort: a burrito for everyone before the climb (2026-11-13: "plenty of food").
	const TArray<FName> Foods = DCCClimbItems::FoodPool();
	for (int32 f = 0; f < 3 && Foods.Num() > 0; ++f)
	{
		Spawn(FortCenter() + FVector(-300 + f * 300.0f, -620, 0), 160.0f, EDCCChestKind::Supply, f == 0 ? DCCClimbItems::GreenChileBurrito : Foods[f % Foods.Num()]);
	}

	// Supply chests on the ledges: every third switchback ledge, every other express one,
	// sitting out toward the ledge's edge so the crawl-over never lands in one.
	struct FSpot { FVector At; float Yaw; int32 Face; float Z; };
	TArray<FSpot> UpperSpots;
	for (int32 Face = 0; Face < 4; ++Face)
	{
		const FVector N = FaceNormal(Face), T = FaceAlong(Face);
		const TArray<float> Heights = LedgeHeights(Face);
		for (int32 k = 0; k < Heights.Num(); ++k)
		{
			const float Z = Heights[k];
			const float Lateral = ((k % 3) - 1) * 300.0f;
			const FVector Spot = O + N * (HalfWidthAt(Z) + 230.0f) + T * (Lateral + 200.0f) + FVector(0, 0, Z);
			const bool bChest = IsExpress(Face) ? (k % 2 == 0) : (k % 3 == 1);
			if (bChest)
			{
				Spawn(Spot, N.Rotation().Yaw + 180.0f, EDCCChestKind::Supply, NAME_None);
			}
			else if (Foods.Num() > 0)
			{
				// Every other ledge carries food, so a hungry climber is never more than a pitch from a meal.
				Spawn(Spot, N.Rotation().Yaw + 180.0f, EDCCChestKind::Supply, Foods[(Face * 7 + k) % Foods.Num()]);
			}
			if (!bChest && Z > MidZ)
			{
				UpperSpots.Add({ O + N * (HalfWidthAt(Z) + 230.0f) + T * (Lateral - 180.0f) + FVector(0, 0, Z), float(N.Rotation().Yaw + 180.0f), Face, float(Z) });
			}
		}
	}
	// Two at High Camp.
	const float Shelf = (WideHalf + NarrowHalf) * 0.5f;
	Spawn(O + FVector(Shelf, -300, MidZ), 90.0f, EDCCChestKind::Supply, NAME_None);
	Spawn(O + FVector(-Shelf, 300, MidZ), 270.0f, EDCCChestKind::Supply, NAME_None);

	// Artifact caches, somewhere different every round: platinum on the exposed express
	// face, the knife on a high switchback ledge, and one more keepsake up top.
	auto Pick = [&UpperSpots](TFunction<bool(const FSpot&)> Want) -> const FSpot*
	{
		TArray<const FSpot*> Matches;
		for (const FSpot& Spot : UpperSpots)
		{
			if (Want(Spot)) { Matches.Add(&Spot); }
		}
		return Matches.IsEmpty() ? nullptr : Matches[FMath::RandRange(0, Matches.Num() - 1)];
	};
	if (const FSpot* Spot = Pick([](const FSpot& S) { return IsExpress(S.Face); }))
	{
		Spawn(Spot->At, Spot->Yaw, EDCCChestKind::Artifact, DCCClimbItems::PlatinumOre);
	}
	if (const FSpot* Spot = Pick([](const FSpot& S) { return !IsExpress(S.Face) && S.Z > TopZ * 0.7f; }))
	{
		Spawn(Spot->At, Spot->Yaw, EDCCChestKind::Artifact, DCCClimbItems::CeremonialKnife);
	}
	const FName Keepsake = FMath::RandBool() ? DCCClimbItems::AssayLedger : DCCClimbItems::ClaimLantern;
	const FVector Corner(FMath::RandBool() ? NarrowHalf - 170.0f : -(NarrowHalf - 170.0f), FMath::RandBool() ? NarrowHalf - 170.0f : -(NarrowHalf - 170.0f), TopZ);
	Spawn(O + Corner, FMath::FRandRange(0.0f, 360.0f), EDCCChestKind::Artifact, Keepsake);
}

ADCCArcadeClimb::FClimbKit* ADCCArcadeClimb::KitOf(const ADCCCharacterBase* Crawler)
{
	APlayerState* PlayerState = Crawler ? Crawler->GetPlayerState() : nullptr;
	return PlayerState ? &Kits.FindOrAdd(PlayerState) : nullptr;
}

void ADCCArcadeClimb::SyncKit(ADCCCharacterBase* Crawler)
{
	FClimbKit* Kit = KitOf(Crawler);
	UDCCWalletComponent* Wallet = Crawler ? Crawler->Wallet.Get() : nullptr;
	if (!Kit || !Wallet)
	{
		return;
	}
	Wallet->ClimbTools = Kit->Tools;
	Wallet->CarriedArtifacts = Kit->Artifacts;
	TArray<FString> Boosts;
	const double Now = ServerTime();
	if (Now < Kit->ChalkUntil) { Boosts.Add(FString::Printf(TEXT("Chalk %d s"), FMath::CeilToInt(Kit->ChalkUntil - Now))); }
	if (Kit->bCrampons) { Boosts.Add(TEXT("Crampons")); }
	if (Kit->bWindCloak) { Boosts.Add(TEXT("Wind Cloak")); }
	if (Kit->HardHats > 0) { Boosts.Add(Kit->HardHats > 1 ? FString::Printf(TEXT("Hard Hat x%d"), Kit->HardHats) : FString(TEXT("Hard Hat"))); }
	if (Kit->SafetyLines > 0) { Boosts.Add(Kit->SafetyLines > 1 ? FString::Printf(TEXT("Safety Line x%d"), Kit->SafetyLines) : FString(TEXT("Safety Line"))); }
	Wallet->ClimbBoosts = FString::Join(Boosts, TEXT("   "));
}

void ADCCArcadeClimb::GiveFullKit(ADCCCharacterBase* Crawler)
{
	if (!Crawler)
	{
		return;
	}
	for (const FName& Id : DCCClimbItems::ChestPool())
	{
		GiveItem(Crawler, Id, /*bQuiet=*/true);
	}
	Crawler->NotifyClimber(NSLOCTEXT("DCC", "OutfitterTaken", "Outfitted: one of every tool and boost. They are on your belt: press a tool's number."));
}

void ADCCArcadeClimb::GiveItem(ADCCCharacterBase* Crawler, FName ItemId, bool bQuiet)
{
	FClimbKit* Kit = KitOf(Crawler);
	const DCCClimbItems::FItem* Item = DCCClimbItems::Find(ItemId);
	if (!Kit || !Item)
	{
		return;
	}
	FString Message;
	switch (Item->Kind)
	{
	case DCCClimbItems::EKind::Tool:
		// Everyone has a hook on their belt now: a spare one in a chest winds yours straight back in.
		if (ItemId == DCCClimbItems::GrapplingHook)
		{
			GrappleReadyAt.Add(Crawler->GetPlayerState(), 0.0);
			if (Crawler->Wallet) { Crawler->Wallet->Client_GrappleThrown(0.0f); }
			Message = TEXT("Found a spare line: your grappling hook is ready again.");
			break;
		}
		Kit->Tools.Add(ItemId);
		Kit->Selected = Kit->Tools.Num() - 1;
		Crawler->OfferQuickItem(ItemId);
		Message = FString::Printf(TEXT("Found: %s. %s  (on your belt)"), Item->Name, Item->Blurb);
		break;
	case DCCClimbItems::EKind::Boost:
		if (ItemId == DCCClimbItems::ChalkBag) { Kit->ChalkUntil = FMath::Max(Kit->ChalkUntil, ServerTime()) + 90.0; }
		else if (ItemId == DCCClimbItems::Crampons) { Kit->bCrampons = true; }
		else if (ItemId == DCCClimbItems::WindCloak) { Kit->bWindCloak = true; }
		else if (ItemId == DCCClimbItems::HardHat) { ++Kit->HardHats; }
		else if (ItemId == DCCClimbItems::SafetyLine) { ++Kit->SafetyLines; }
		Message = FString::Printf(TEXT("%s: %s"), Item->Name, Item->Blurb);
		break;
	case DCCClimbItems::EKind::Artifact:
		Kit->Artifacts.Add(ItemId);
		Message = FString::Printf(TEXT("ARTIFACT: %s! Keep it safe: lose it if you die. Alive at the end of the round, it's yours."), Item->Name);
		AddPoints(Crawler->GetPlayerState(), 30, TEXT("found an artifact"));
		break;
	}
	if (!bQuiet && Crawler->Wallet)
	{
		Crawler->Wallet->Tell(Message);
	}
	SyncKit(Crawler);
}

void ADCCArcadeClimb::UseToolId(ADCCCharacterBase* Crawler, FName ToolId)
{
	FClimbKit* Kit = KitOf(Crawler);
	const int32 Index = Kit ? Kit->Tools.IndexOfByKey(ToolId) : INDEX_NONE;
	if (Index == INDEX_NONE)
	{
		return;
	}
	Kit->Selected = Index;
	UseTool(Crawler);
}

void ADCCArcadeClimb::UseTool(ADCCCharacterBase* Crawler)
{
	FClimbKit* Kit = KitOf(Crawler);
	if (!Kit || Kit->Tools.IsEmpty() || !Crawler->IsAlive())
	{
		return;
	}
	Kit->Selected = FMath::Clamp(Kit->Selected, 0, Kit->Tools.Num() - 1);
	const FName Tool = Kit->Tools[Kit->Selected];
	UWorld* World = GetWorld();
	FCollisionQueryParams Params(SCENE_QUERY_STAT(DCCClimbTool), false, Crawler);
	bool bUsed = false;

	if (Tool == DCCClimbItems::GrapplingHook)
	{
		// An old one from before the belt hook: it works like a level-0 throw.
		if (!GrappleUp(Crawler, DCCChopShop::GrappleReach(0)))
		{
			return;
		}
		bUsed = true;
	}
	else if (Tool == DCCClimbItems::Piton)
	{
		// Your respawn point: the first solid footing below where you are.
		FVector Probe = Crawler->GetActorLocation();
		if (Crawler->ActiveClimbPoint && Crawler->ActiveClimbPoint->IsWallPlane())
		{
			Probe += Crawler->ActiveClimbPoint->WallNormal * 150.0f;
		}
		FHitResult Floor;
		if (!World->LineTraceSingleByChannel(Floor, Probe, Probe - FVector(0, 0, 20000.0f), ECC_WorldStatic, Params)
			|| Floor.ImpactPoint.Z - GetActorLocation().Z < 300.0f)
		{
			Crawler->Wallet->Tell(TEXT("Too close to the ground to bother with a piton. Get higher first."));
			return;
		}
		if (APlayerState* PlayerState = Crawler->GetPlayerState())
		{
			Checkpoints.Add(PlayerState, Floor.ImpactPoint + FVector(0, 0, 100.0f));
		}
		Crawler->Wallet->Tell(TEXT("Piton set. If you fall, you come back to the ledge below it."));
		bUsed = true;
	}
	else if (const DCCClimbItems::FItem* Food = DCCClimbItems::Find(Tool); Food && Food->Food > 0.0f)
	{
		Kit->Hunger = FMath::Min(100.0f, Kit->Hunger + Food->Food);
		if (Crawler->Stats)
		{
			// Trail mix is a full second wind; other food a little.
			Crawler->Stats->RecoverStamina(Tool == DCCClimbItems::TrailMix ? Crawler->Stats->MaxStamina : Crawler->Stats->MaxStamina * 0.2f);
		}
		Crawler->Wallet->Tell(FString::Printf(TEXT("%s. Hunger %d%%."), Food->Name, FMath::RoundToInt(100.0f - Kit->Hunger)));
		bUsed = true;
	}

	if (bUsed)
	{
		Kit->Tools.RemoveAt(Kit->Selected);
		Kit->Selected = Kit->Tools.IsEmpty() ? 0 : Kit->Selected % Kit->Tools.Num();
		SyncKit(Crawler);
	}
}

bool ADCCArcadeClimb::GrappleUp(ADCCCharacterBase* Crawler, float Reach)
{
	ADCCClimbPoint* Point = nullptr;
	if (Crawler)
	{
		Point = Crawler->ActiveClimbPoint;
	}
	UCapsuleComponent* Capsule = Crawler ? Crawler->GetCapsuleComponent() : nullptr;
	UWorld* World = GetWorld();
	if (!Point || !Capsule || !World)
	{
		if (Crawler && Crawler->Wallet) { Crawler->Wallet->Tell(TEXT("The hook needs a wall: use it while you're climbing.")); }
		return false;
	}
	FCollisionQueryParams Params(SCENE_QUERY_STAT(DCCGrapple), false, Crawler);
	const FVector From = Crawler->GetActorLocation();
	const float TargetZ = FMath::Min(From.Z + Reach, Point->GetTopZ() - 40.0f);
	FVector To = Point->GetAttachLocationFor(Crawler, TargetZ);
	FHitResult Hit;
	if (World->SweepSingleByChannel(Hit, From, To, FQuat::Identity, ECC_Pawn,
		FCollisionShape::MakeCapsule(Capsule->GetScaledCapsuleRadius() - 2.0f, Capsule->GetScaledCapsuleHalfHeight() - 2.0f), Params))
	{
		To = From + (To - From) * FMath::Max(0.0f, Hit.Time - 0.02f); // up to the ledge, then crawl over it as usual
	}
	Crawler->SnapTo(To);
	return true;
}

void ADCCArcadeClimb::BeltGrapple(ADCCCharacterBase* Crawler)
{
	APlayerState* PlayerState = Crawler ? Crawler->GetPlayerState() : nullptr;
	if (!PlayerState || !Crawler->IsAlive() || !Crawler->Wallet)
	{
		return;
	}
	const double Now = ServerTime();
	double& Ready = GrappleReadyAt.FindOrAdd(PlayerState);
	if (Now < Ready)
	{
		Crawler->Wallet->Tell(FString::Printf(TEXT("The line is still winding in: %d s."), FMath::CeilToInt(Ready - Now)));
		return;
	}
	const int32 Level = DCCChopShop::GetLevel(DCCProfile::KeyFor(PlayerState), DCCChopShop::Grapple);
	if (!GrappleUp(Crawler, DCCChopShop::GrappleReach(Level)))
	{
		return;
	}
	const float Cooldown = DCCChopShop::GrappleCooldown(Level);
	Ready = Now + Cooldown;
	UE_LOG(LogTemp, Log, TEXT("[DCC Grapple] %s, level %d: now at z=%.0f, back in %.0f s"), *PlayerState->GetPlayerName(), Level, Crawler->GetActorLocation().Z, Cooldown);
	Crawler->Wallet->Client_GrappleThrown(Cooldown);
	Crawler->Wallet->Tell(TEXT("The hook bites. Up you go."));
}

void ADCCArcadeClimb::TickKits(float StepSeconds)
{
	UWorld* World = GetWorld();
	// Everyone gets a kit from the start, not at their first pickup, so hunger runs for all (2026-11-13).
	for (TActorIterator<ADCCCharacterBase> It(World); It; ++It)
	{
		if (It->IsPlayerControlled() && It->IsAlive())
		{
			KitOf(*It);
		}
	}
	for (auto It = Kits.CreateIterator(); It; ++It)
	{
		APlayerState* PlayerState = It.Key().Get();
		if (!PlayerState)
		{
			It.RemoveCurrent();
			continue;
		}
		FClimbKit& Kit = It.Value();
		ADCCCharacterBase* Crawler = Cast<ADCCCharacterBase>(PlayerState->GetPawn());
		// Cold up high: now and then, a snowflake over anyone above High Camp.
		if (Crawler && Crawler->IsAlive() && Crawler->GetActorLocation().Z - GetActorLocation().Z > MidZ && FMath::FRand() < StepSeconds / 50.0f)
		{
			Crawler->ReactTo(DCCReactions::MomentCold);
		}

		// Dead: artifacts stay on the mountain.
		if ((!Crawler || !Crawler->IsAlive()) && !Kit.Artifacts.IsEmpty())
		{
			if (Crawler && Crawler->Wallet)
			{
				Crawler->Wallet->Tell(TEXT("Your artifacts tumble down the mountain without you. Gone."));
			}
			Kit.Artifacts.Reset();
		}
		if (!Crawler || !Crawler->IsAlive())
		{
			continue;
		}

		// Hunger (2026-11-13): about 6 minutes from fed to starving, twice as fast on the wall.
		// It caps max stamina at 35%..100%; starving also halves recovery. Food lifts it.
		{
			// The step since the last call (this runs four times a second, not every frame). It used to read the
			// frame's delta, so hunger drained about fifteen times slower than meant (fixed 2026-11-15).
			const float Dt = StepSeconds;
			// Halved with the hunger-rate fix (2026-11-16): at the real rate it ate the stamina bar too fast.
			Kit.Hunger = FMath::Max(0.0f, Kit.Hunger - Dt * (Crawler->IsClimbing() ? 0.2f : 0.1f));
			if (Crawler->Stats && Crawler->Stats->MaxStamina > 0.0f)
			{
				const float Cap = Crawler->Stats->MaxStamina * (0.35f + 0.65f * Kit.Hunger / 100.0f);
				if (Crawler->Stats->CurrentStamina > Cap)
				{
					Crawler->Stats->ConsumeStamina(Crawler->Stats->CurrentStamina - Cap);
				}
			}
			if (Kit.Hunger <= 0.0f)
			{
				Crawler->StaminaRegenMultiplier *= 0.5f;
			}
			if (Kit.HungerTold == 0 && Kit.Hunger < 45.0f) { Kit.HungerTold = 1; Crawler->Wallet->Tell(TEXT("Your stomach growls. Hungry climbers tire fast: eat something.")); }
			if (Kit.HungerTold == 1 && Kit.Hunger < 10.0f) { Kit.HungerTold = 2; Crawler->Wallet->Tell(TEXT("Starving. Your arms are shaking. Find food.")); }
			if (Kit.Hunger > 60.0f) { Kit.HungerTold = 0; }
			if (Crawler->Wallet && FMath::Abs(Crawler->Wallet->ClimbHunger - Kit.Hunger) > 0.5f)
			{
				Crawler->Wallet->ClimbHunger = Kit.Hunger;
			}
		}

		// Chalk.
		if (ServerTime() < Kit.ChalkUntil && Crawler->IsClimbing())
		{
			Crawler->ClimbStaminaMultiplier *= 0.6f;
		}
		// Safety line: a real fall (not a hop) is caught.
		const UCharacterMovementComponent* Movement = Crawler->GetCharacterMovement();
		if (Kit.SafetyLines > 0 && Movement && Movement->IsFalling() && !Crawler->IsClimbing() && Crawler->GetVelocity().Z < -900.0f
			&& World->GetTimeSeconds() >= Crawler->FallSafeUntil)
		{
			--Kit.SafetyLines;
			Crawler->FallSafeUntil = World->GetTimeSeconds() + 4.0;
			Crawler->Wallet->Tell(TEXT("Your safety line snaps taut. That fall won't hurt."));
		}
		SyncKit(Crawler);
	}
}


void ADCCArcadeClimb::AddPoints(APlayerState* PlayerState, int32 Amount, const TCHAR* Why)
{
	if (!PlayerState || Amount <= 0)
	{
		return;
	}
	Points.FindOrAdd(PlayerState) += Amount;
	UE_LOG(LogTemp, Log, TEXT("[DCC Climb] %s +%d (%s)"), *PlayerName(PlayerState), Amount, Why);
}

void ADCCArcadeClimb::PayOut()
{
	if (bPaidOut)
	{
		return;
	}
	bPaidOut = true;

	// Artifacts come home with anyone still alive.
	for (TPair<TWeakObjectPtr<APlayerState>, FClimbKit>& Pair : Kits)
	{
		APlayerState* PlayerState = Pair.Key.Get();
		ADCCCharacterBase* Crawler = PlayerState ? Cast<ADCCCharacterBase>(PlayerState->GetPawn()) : nullptr;
		if (!Crawler || !Crawler->IsAlive() || Pair.Value.Artifacts.IsEmpty())
		{
			continue;
		}
		TArray<FString> Names;
		for (const FName& Id : Pair.Value.Artifacts)
		{
			DCCProfile::AddTrophy(DCCProfile::KeyFor(PlayerState), Id);
			if (const DCCClimbItems::FItem* Item = DCCClimbItems::Find(Id)) { Names.Add(Item->Name); }
		}
		Pair.Value.Artifacts.Reset();
		if (Crawler->Wallet)
		{
			Crawler->Wallet->RefreshTrophies();
			Crawler->Wallet->Tell(FString::Printf(TEXT("Brought home: %s. They're in your trophy vault at the Prize Counter."), *FString::Join(Names, TEXT(", "))));
		}
		SyncKit(Crawler);
	}

	for (const TPair<TWeakObjectPtr<APlayerState>, int32>& Pair : Points)
	{
		if (APlayerState* PlayerState = Pair.Key.Get())
		{
			DCCProfile::RecordBest(PlayerState, TEXT("Climb"), TEXT("BestPoints"), float(Pair.Value));
			UDCCWalletComponent::AwardTickets(PlayerState, Pair.Value, TEXT("Fourteener points"));
		}
	}
}

void ADCCArcadeClimb::ServerTick(float DeltaSeconds)
{
	if (!bPaidOut)
	{
		TickRocks();
		TickBirds(DeltaSeconds);
		TeamTimer -= DeltaSeconds;
		if (TeamTimer <= 0.0f)
		{
			TickHazardsAndTeam(0.25f - TeamTimer);
			TeamTimer = 0.25f;
		}
	}

	RefreshTimer -= DeltaSeconds;
	if (RefreshTimer > 0.0f)
	{
		return;
	}
	RefreshTimer = 0.5f;

	const float Elapsed = static_cast<float>(ServerTime() - StartedAt);
	TArray<APlayerController*> Players;
	GetPlayers(Players);
	TArray<FString> Rows;
	int32 Present = 0;
	for (APlayerController* PC : Players)
	{
		const APawn* Pawn = PC->GetPawn();
		APlayerState* PlayerState = PC->PlayerState;
		if (!Pawn || !PlayerState)
		{
			continue;
		}
		++Present;
		if (!SessionBest.Contains(PlayerState))
		{
			UE_LOG(LogTemp, Verbose, TEXT("[DCC Climb] %s starts at %s (fort centre %s), facing %.0f"), *PlayerName(PlayerState),
				*Pawn->GetActorLocation().ToCompactString(), *FortCenter().ToCompactString(), Pawn->GetActorRotation().Yaw);
		}
		const float Height = FMath::Clamp(Pawn->GetActorLocation().Z - GetActorLocation().Z - 90.0f, 0.0f, TopZ);
		const float Feet = TrailheadFeet + (SummitFeet - TrailheadFeet) * Height / TopZ; // the trailhead is not at sea level

		// A point for every new 100 ft of your best this round.
		float& Best = SessionBest.FindOrAdd(PlayerState, TrailheadFeet);
		const int32 OldBuckets = FMath::FloorToInt((Best - TrailheadFeet) / 100.0f);
		const int32 NewBuckets = FMath::FloorToInt((Feet - TrailheadFeet) / 100.0f);
		if (NewBuckets > OldBuckets)
		{
			AddPoints(PlayerState, NewBuckets - OldBuckets, TEXT("height"));
			// Climbing experience: a little for every new 100 ft, so it climbs as you do.
			if (const ADCCCharacterBase* Crawler = Cast<ADCCCharacterBase>(Pawn); Crawler && Crawler->Stats)
			{
				Crawler->Stats->AddExperience(2 * (NewBuckets - OldBuckets));
			}
		}
		if (Feet > Best)
		{
			if (Feet > Best + 50.0f)
			{
				DCCProfile::RecordBest(PlayerState, TEXT("Climb"), TEXT("BestHeight"), FMath::RoundToFloat(Feet));
			}
			Best = Feet;
		}

		if (Height >= TopZ - 60.0f && !Summited.Contains(PlayerState))
		{
			Summited.Add(PlayerState);
			if (ADCCCharacterBase* Climber = Cast<ADCCCharacterBase>(PlayerState->GetPawn()))
			{
				Climber->ReactTo(DCCReactions::MomentWin, false);
			}
			DCCProfile::Add(PlayerState, TEXT("Climb"), TEXT("Summits"), 1.0f);
			DCCProfile::RecordBest(PlayerState, TEXT("Climb"), TEXT("FastestSummit"), Elapsed, /*bHigherIsBetter=*/false);
			AddPoints(PlayerState, 100, TEXT("summit"));
			if (!bFirstSummitTaken)
			{
				bFirstSummitTaken = true;
				AddPoints(PlayerState, 50, TEXT("first to the top"));
			}
			AddPoints(PlayerState, FMath::Max(0, FMath::RoundToInt((RoundSeconds - Elapsed) / 4.0f)), TEXT("beat the clock"));

			// The celebration: fanfare, banner and fireworks over the peak, for everyone.
			FActorSpawnParameters Params;
			Params.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
			if (ADCCFireworks* Show = GetWorld()->SpawnActorDeferred<ADCCFireworks>(ADCCFireworks::StaticClass(),
				FTransform(GetActorLocation() + FVector(0, 0, TopZ + 200.0f)), nullptr, nullptr, ESpawnActorCollisionHandlingMethod::AlwaysSpawn))
			{
				Show->Seed = FMath::Rand();
				Show->Climber = PlayerName(PlayerState);
				Show->FinishSpawning(FTransform(GetActorLocation() + FVector(0, 0, TopZ + 200.0f)));
			}
			if (const ADCCCharacterBase* Crawler = Cast<ADCCCharacterBase>(Pawn))
			{
				if (Crawler->Stats)
				{
					Crawler->Stats->AddExperience(150);
				}
				if (Crawler->Skills)
				{
					Crawler->Skills->MarkSkillUsed(TEXT("Climbing"));
				}
				if (Crawler->Wallet)
				{
					Crawler->Wallet->Tell(TEXT("SUMMIT!  +150 climbing XP"));
				}
			}
		}
		Rows.Add(FString::Printf(TEXT("%s %s ft  %d pts%s"), *PlayerName(PlayerState), *FText::AsNumber(FMath::RoundToInt(Feet)).ToString(),
			Points.FindRef(PlayerState), Summited.Contains(PlayerState) ? TEXT(" (SUMMIT)") : TEXT("")));
	}

	const bool bEveryoneUp = Present > 0 && Summited.Num() >= Present;
	if (bEveryoneUp && !bTeamBonusPaid && Present > 1)
	{
		bTeamBonusPaid = true;
		for (const TWeakObjectPtr<APlayerState>& Climber : Summited)
		{
			AddPoints(Climber.Get(), 50, TEXT("the whole crew made it"));
		}
	}

	Title = TEXT("FOURTEENER  -  get everyone to the summit");
	Accent = FLinearColor(0.55f, 0.85f, 1.0f);
	StatusLines.Reset();
	StatusLines.Add(FString::Join(Rows, TEXT("     ")));
	const DCCArcade::FGame* Game = DCCArcade::Find(TEXT("Climb"));
	StatusLines.Add(FString::Printf(TEXT("Best ever: %s"), *DCCProfile::Leaderboard(GetWorld(), TEXT("Climb"), TEXT("BestHeight"), true,
		[Game](float V) { return DCCArcade::FormatRecord(*Game, V); })));
	StatusLines.Add(FString::Printf(TEXT("Time: %d:%02d of %d:00.  Equip Climbing, HOLD left mouse to climb, let go to drop. Points become tickets."),
		FMath::FloorToInt(Elapsed / 60.0f), FMath::FloorToInt(FMath::Fmod(Elapsed, 60.0f)), FMath::RoundToInt(RoundSeconds / 60.0f)));
	if (!HazardLine.IsEmpty() && ServerTime() < HazardLineUntil)
	{
		StatusLines.Add(TEXT("!") + HazardLine); // "!" = an alert: it shows even after the status box steps aside
	}

	if (bEveryoneUp || Elapsed >= RoundSeconds)
	{
		StatusLines.Add(bEveryoneUp ? TEXT("!Everyone made the summit!") : TEXT("!Out of daylight."));
		PayOut();
		ReturnToLobbyIn(25.0f);
	}
}

// ===========================================================================
// Repo Rally (a shift in the city: aim the hook, tow the mark to the yard)
// ===========================================================================

namespace
{
	struct FRepoRole
	{
		const TCHAR* Name;
		float Cost;
		float Pace;
		float Engine;
		float Hook;
		float Line;
		float Burn;
		float BoostExtra;
		bool bNet;
	};

	const FRepoRole& RepoRole(int32 Role)
	{
		static const FRepoRole Roles[] = {
			// Hook is reel speed. Line is how far the aimed shot reaches.
			{ TEXT("Stock"),  0.0f,  0.52f, 1.00f, 1.00f, 4500.0f, 1.00f, 0.0f, false },
			{ TEXT("Runner"), 22.0f, 0.52f, 1.22f, 0.72f, 3200.0f, 1.15f, 0.0f, false },
			{ TEXT("Net"),    22.0f, 0.48f, 1.00f, 1.00f, 4200.0f, 1.00f, 0.0f, true },
			{ TEXT("Winch"),  28.0f, 0.48f, 1.00f, 1.50f, 6500.0f, 1.00f, 0.0f, false },
			{ TEXT("Tank"),   16.0f, 0.50f, 1.00f, 1.00f, 4500.0f, 0.55f, 0.8f, false },
		};
		return Roles[FMath::Clamp(Role, 0, 4)];
	}

	const TCHAR* MarkNames[] = {
		TEXT("Hot Check"), TEXT("Unpaid Tab"), TEXT("Skip Week"), TEXT("Courtesy Car"),
		TEXT("Broken Lease"), TEXT("No Insurance"), TEXT("Cash Only"), TEXT("Final Notice"),
		TEXT("Short Pay"), TEXT("After Hours"),
	};
}

FVector2D ADCCArcadeKart::GasOffset(int32 Index)
{
	// Just off the intersection, on the shoulder, clear of the wider asphalt.
	const float Off = RoadHalf + 1600.0f;
	return Index == 0
		? FVector2D(StreetSpacing + Off, -StreetSpacing + Off)
		: FVector2D(-StreetSpacing - Off, StreetSpacing - Off);
}

FVector2D ADCCArcadeKart::YardOffset()
{
	// The southwest block, between the outer streets. That block is the yard.
	const float At = (-StreetRadius + 0.5f) * StreetSpacing;
	return FVector2D(At, At);
}

ADCCArcadeArena::FSky ADCCArcadeKart::GetSky() const
{
	FSky Sky;
	// Night, with the last of a synthwave sunset on the horizon (2026-11-13: "a nice night sky"): the sun
	// just down, a deep violet sky, low purple haze. Stars and a moon are built in BuildStreetFun.
	Sky.SunPitch = 7.0f; // just below the horizon: a glow on the skyline, a dark sky above
	Sky.SunIntensity = 1.1f;
	Sky.SunColour = FLinearColor(1.0f, 0.35f, 0.55f);
	// 2026-11-14 ("it is still really dark"): the sky fill is what lights the streets between the neon. The
	// captured night sky is dim, so this is high; the exposure is pinned in BuildStreetFun (DCC.RallyExposure).
	Sky.SkyIntensity = 2.6f;
	Sky.FogColour = FLinearColor(0.36f, 0.14f, 0.48f);
	Sky.FogDensity = 0.004f;
	return Sky;
}

FVector ADCCArcadeKart::Crossing(int32 Ix, int32 Iy) const
{
	const FVector O = GetActorLocation();
	return O + FVector(Ix * StreetSpacing, Iy * StreetSpacing, 160.0f);
}

void ADCCArcadeKart::FillRoute(int32 Which, TArray<FVector>& Out) const
{
	Out.Reset();
	const int32 R = StreetRadius;
	auto Add = [&](int32 X, int32 Y) { Out.Add(Crossing(X, Y)); };
	switch (Which & 3)
	{
	case 0: // the long way around the outside
		Add(-R, -R); Add(R, -R); Add(R, R); Add(-R, R);
		break;
	case 1: // north-south snake, off the yard
		Add(-R, -1); Add(R, -1); Add(R, 0); Add(-R, 0); Add(-R, 1); Add(R, 1);
		break;
	case 2:
		Add(-1, -R); Add(-1, R); Add(0, R); Add(0, -R); Add(1, -R); Add(1, R);
		break;
	default:
		Add(-1, -1); Add(1, -1); Add(1, 1); Add(-1, 1);
		break;
	}
}

FTransform ADCCArcadeKart::GridSlot(int32 Index) const
{
	const FVector Yard = GetActorLocation() + FVector(YardOffset().X, YardOffset().Y, 160.0f);
	const int32 Row = Index / 2;
	const float Side = (Index % 2 == 0) ? -460.0f : 460.0f;
	return FTransform(FRotator::ZeroRotator, Yard + FVector(-1600.0f - Row * 820.0f, Side, 0.0f));
}

void ADCCArcadeKart::GetStartTransforms(TArray<FTransform>& Out) const
{
	// Behind your truck, not inside it (2026-11-13: the view opened inside the car for a few seconds
	// until you were seated). The truck itself still spawns on GridSlot.
	for (int32 i = 0; i < 8; ++i)
	{
		FTransform Slot = GridSlot(i);
		Slot.AddToTranslation(FVector(-1100.0f, 0.0f, 120.0f));
		Out.Add(Slot);
	}
}

ATextRenderActor* ADCCArcadeKart::Label(const FVector& At, float Yaw, const FString& Text, const FColor& Colour, float Size)
{
	UWorld* World = GetWorld();
	if (!World)
	{
		return nullptr;
	}
	const FTransform Transform(FRotator(0.0f, Yaw, 0.0f), At);
	ATextRenderActor* Actor = World->SpawnActorDeferred<ATextRenderActor>(ATextRenderActor::StaticClass(), Transform);
	if (!Actor)
	{
		return nullptr;
	}
	Actor->SetReplicates(false);
	if (UTextRenderComponent* Render = Actor->GetTextRender())
	{
		Render->SetText(FText::FromString(Text));
		Render->SetTextRenderColor(Colour);
		Render->SetHorizontalAlignment(EHTA_Center);
		Render->SetVerticalAlignment(EVRTA_TextCenter);
		Render->SetWorldSize(Size);
	}
	Actor->FinishSpawning(Transform);
	LocalActors.Add(Actor);
	return Actor;
}

void ADCCArcadeKart::BuildLocal()
{
	DressCity();
	DressStreetLife();
	DressUrbanClutter();
	BuildStreetFun();
	SpawnWalkers();
}

void ADCCArcadeKart::DressCity()
{
	// A vaporwave city at dusk (2026-11-13, Zoe: "our cool futuristic vaporwave city style", "much more highly
	// decorated"). Real models from Synthwave_city / Cyberpunk_City / Sci_Fi_city; neon from Synthwave's MI_neon_*.
	// The grid, the yard, the bays, the pumps and the stations are where the shift's code expects them.
	const FVector O = GetActorLocation();
	const FLinearColor Ground(0.035f, 0.02f, 0.05f);
	const FLinearColor Asphalt(0.03f, 0.03f, 0.045f);
	const FLinearColor Night(0.06f, 0.04f, 0.09f);
	const FLinearColor Magenta(1.0f, 0.2f, 0.85f);
	const FLinearColor Cyan(0.2f, 0.9f, 1.0f);
	const FLinearColor Amber(1.0f, 0.6f, 0.2f);
	static const TCHAR* NeonA = TEXT("/Game/Synthwave_city/Material/MI_neon_1");
	static const TCHAR* NeonB = TEXT("/Game/Synthwave_city/Material/MI_neon_4");
	static const TCHAR* Buildings[] = {
		TEXT("/Game/Synthwave_city/Meshes/Buildings/SM_building_01"), TEXT("/Game/Synthwave_city/Meshes/Buildings/SM_building_03"),
		TEXT("/Game/Synthwave_city/Meshes/Buildings/SM_building_05"), TEXT("/Game/Synthwave_city/Meshes/Buildings/SM_building_07"),
		TEXT("/Game/Synthwave_city/Meshes/Buildings/SM_building_09"), TEXT("/Game/Synthwave_city/Meshes/Buildings/SM_building_11"),
		TEXT("/Game/Cyberpunk_City/Meshes/Background_buildings/SM_building_05"), TEXT("/Game/Cyberpunk_City/Meshes/Background_buildings/SM_building_08"),
		TEXT("/Game/Cyberpunk_City/Meshes/Background_buildings/SM_building_11"), TEXT("/Game/Synthwave_city/Meshes/Buildings/SM_dome_building"),
	};
	static const TCHAR* Adverts[] = {
		TEXT("/Game/Synthwave_city/Meshes/Advertisements/SM_advertisement_02"), TEXT("/Game/Synthwave_city/Meshes/Advertisements/SM_advertisement_05"),
		TEXT("/Game/Synthwave_city/Meshes/Advertisements/SM_advertisement_08"),
	};
	static const TCHAR* Holos[] = {
		TEXT("/Game/Cyberpunk_City/Meshes/Hologram/SM_hologram_02"), TEXT("/Game/Cyberpunk_City/Meshes/Hologram/SM_hologram_05"),
		TEXT("/Game/Cyberpunk_City/Meshes/Hologram/SM_hologram_09"),
	};
	static const TCHAR* Parked[] = {
		TEXT("/Game/Synthwave_city/Meshes/Car/SM_car_B"), TEXT("/Game/Synthwave_city/Meshes/Car/SM_car_B1"),
		TEXT("/Game/Synthwave_city/Meshes/Car/SM_car_B2"), TEXT("/Game/Synthwave_city/Meshes/Car/SM_car_02"),
	};
	static const TCHAR* Palm = TEXT("/Game/Synthwave_city/Meshes/Tree/SM_palm_tree_01");
	static const TCHAR* Palm2 = TEXT("/Game/Synthwave_city/Meshes/Tree/SM_palm_tree_02");
	static const TCHAR* Lamp = TEXT("/Game/Synthwave_city/Meshes/Street_lamp/SM_street_lamp");
	FRandomStream R(1985);

	Block(O + FVector(0, 0, -50), FVector(MapHalf * 2.0f, MapHalf * 2.0f, 80), Ground);
	// A low wall so a truck that leaves the map bumps, instead of driving off the slab.
	const float Wall = MapHalf - 1500.0f;
	for (int32 Side = -1; Side <= 1; Side += 2)
	{
		if (AActor* W = Block(O + FVector(0, Side * Wall, 120), FVector(MapHalf * 2.0f, 400, 280), Night)) { Paint(W, NeonA); }
		if (AActor* W = Block(O + FVector(Side * Wall, 0, 120), FVector(400, MapHalf * 2.0f, 280), Night)) { Paint(W, NeonB); }
	}

	// The far horizon: the big striped sun and a range of wireframe-ish mountains, beyond the wall.
	Prop(TEXT("/Game/Synthwave_city/Meshes/Sun/SM_sun"), O + FVector(MapHalf + 30000.0f, 0, -2000), 90.0f, FVector(0, 0, 36000));
	for (int32 m = -2; m <= 2; ++m)
	{
		Prop(m % 2 ? TEXT("/Game/Synthwave_city/Meshes/Mountain/SM_mountain") : TEXT("/Game/Synthwave_city/Meshes/Mountain/SM_mountain_02"),
			O + FVector(MapHalf + 12000.0f, m * 22000.0f, -200), 90.0f, FVector(0, 26000, 9000));
	}

	// Roads: dark asphalt, a glowing edge on each side, dashes down the middle, lamps and palms on the curb.
	const float RoadLen = (StreetRadius * 2) * StreetSpacing + 6000.0f;
	int32 LampCount = 0;
	for (int32 i = -StreetRadius; i <= StreetRadius; ++i)
	{
		const float At = i * StreetSpacing;
		for (int32 Axis = 0; Axis < 2; ++Axis)
		{
			const FVector Along = Axis == 0 ? FVector(1, 0, 0) : FVector(0, 1, 0);
			const FVector Across = Axis == 0 ? FVector(0, 1, 0) : FVector(1, 0, 0);
			const FVector Mid = O + Across * At;
			Block(Mid + FVector(0, 0, 8), Axis == 0 ? FVector(RoadLen, RoadHalf * 2.0f, 16) : FVector(RoadHalf * 2.0f, RoadLen, 16), Asphalt);
			for (int32 Edge = -1; Edge <= 1; Edge += 2)
			{
				const FVector EdgeAt = Mid + Across * (Edge * (RoadHalf - 50.0f)) + FVector(0, 0, 18);
				if (AActor* Strip = Block(EdgeAt, Axis == 0 ? FVector(RoadLen, 40, 6) : FVector(40, RoadLen, 6), Edge < 0 ? Magenta : Cyan, 0.0f, false))
				{
					Paint(Strip, Edge < 0 ? NeonA : NeonB);
				}
			}
			for (float D = -RoadLen * 0.5f + 1500.0f; D < RoadLen * 0.5f; D += 2400.0f)
			{
				Block(Mid + Along * D + FVector(0, 0, 18), Axis == 0 ? FVector(700, 40, 4) : FVector(40, 700, 4), FLinearColor(0.9f, 0.85f, 1.0f), 0.0f, false);
			}
			// Curb dressing every 5 m along both sides, skipping the crossings.
			for (float D = -RoadLen * 0.5f + 2500.0f; D < RoadLen * 0.5f - 2500.0f; D += 5000.0f)
			{
				const float ToCrossing = FMath::Abs(FMath::Fmod(D + StreetSpacing * 10.0f, StreetSpacing) - StreetSpacing * 0.5f);
				if (ToCrossing > StreetSpacing * 0.5f - (RoadHalf + 1200.0f))
				{
					continue;
				}
				for (int32 Edge = -1; Edge <= 1; Edge += 2)
				{
					const FVector Curb = Mid + Along * D + Across * (Edge * (RoadHalf + 500.0f));
					const bool bLamp = (FMath::RoundToInt(D / 5000.0f) + Edge) % 2 == 0;
					if (bLamp)
					{
						Prop(Lamp, Curb, (Across * -Edge).Rotation().Yaw, FVector(0, 0, 900));
						if (LampCount++ % 4 == 0)
						{
							Light(Curb + FVector(0, 0, 820) - Across * Edge * 300.0f, Edge < 0 ? Magenta : Cyan, 5000.0f, 2600.0f);
						}
					}
					else
					{
						Prop(R.FRand() < 0.5f ? Palm : Palm2, Curb, R.FRandRange(0.0f, 360.0f), FVector(0, 0, R.FRandRange(900.0f, 1400.0f)));
					}
				}
			}
			// Parked and abandoned cars in the lanes: you steer around them.
			for (int32 Seg = -StreetRadius; Seg < StreetRadius; ++Seg)
			{
				for (int32 c = 0; c < 2; ++c)
				{
					const float D = (Seg + 0.5f) * StreetSpacing + R.FRandRange(-6000.0f, 6000.0f);
					// Shoulder, inside the asphalt and outside the lane traffic uses (±550).
					const float Lane = (R.FRand() < 0.5f ? -1.0f : 1.0f) * R.FRandRange(RoadHalf - 750.0f, RoadHalf - 450.0f);
					const FVector Spot = Mid + Along * D + Across * Lane;
					const FVector2D Local(Spot.X - O.X, Spot.Y - O.Y);
					const FVector2D YardAt = YardOffset();
					bool bClear = FVector2D::Distance(Local, YardAt) > 9000.0f;
					for (int32 g = 0; g < 2; ++g)
					{
						bClear = bClear && FVector2D::Distance(Local, GasOffset(g)) > 4000.0f;
					}
					if (!bClear)
					{
						continue;
					}
					const float Yaw = Along.Rotation().Yaw + R.FRandRange(-25.0f, 25.0f) + (R.FRand() < 0.5f ? 180.0f : 0.0f);
					Prop(Parked[R.RandRange(0, UE_ARRAY_COUNT(Parked) - 1)], Spot, Yaw, FVector(640, 360, 0));
					if (AActor* Hit = Block(Spot + FVector(0, 0, 90), FVector(620, 300, 180), Asphalt, Yaw, true))
					{
						Hit->SetActorHiddenInGame(true);
					}
				}
			}
		}
	}

	// The blocks: towers and domes, billboards, holograms on the corners.
	for (int32 Ix = -StreetRadius; Ix < StreetRadius; ++Ix)
	{
		for (int32 Iy = -StreetRadius; Iy < StreetRadius; ++Iy)
		{
			if (Ix == -StreetRadius && Iy == -StreetRadius)
			{
				continue; // the yard
			}
			const FVector Cell = O + FVector((Ix + 0.5f) * StreetSpacing, (Iy + 0.5f) * StreetSpacing, 0);
			const float Lot = StreetSpacing - (RoadHalf + 700.0f) * 2.0f;
			Block(Cell + FVector(0, 0, 4), FVector(Lot, Lot, 10), Night, 0.0f, false);
			for (int32 q = 0; q < 4; ++q)
			{
				if (R.FRand() < 0.15f)
				{
					continue; // a gap, so the skyline is not a wall
				}
				const FVector Spot = Cell + FVector((q % 2 ? 1 : -1) * TowerInset, (q / 2 ? 1 : -1) * TowerInset, 0);
				const float H = R.FRandRange(6000.0f, 16000.0f);
				Prop(Buildings[R.RandRange(0, UE_ARRAY_COUNT(Buildings) - 1)], Spot, R.RandRange(0, 3) * 90.0f, FVector(TowerFit, TowerFit, H), true);
			}
			const float Walk = StreetSpacing * 0.5f - RoadHalf - 700.0f;
			Prop(Adverts[R.RandRange(0, UE_ARRAY_COUNT(Adverts) - 1)], Cell + FVector(-Walk, R.FRandRange(-4000.0f, 4000.0f), 0), 90.0f, FVector(0, 0, 2200));
			Prop(Holos[R.RandRange(0, UE_ARRAY_COUNT(Holos) - 1)], Cell + FVector(Walk * 0.55f, Walk * 0.55f, 0), R.FRandRange(0.0f, 360.0f), FVector(0, 0, 1400));
			Light(Cell + FVector(Walk * 0.55f, Walk * 0.55f, 900), (Ix + Iy) % 2 ? Magenta : Cyan, 6000.0f, 2400.0f);
		}
	}

	// Two street stations. The canopy does not collide: you drive under it and sit.
	for (int32 g = 0; g < 2; ++g)
	{
		const FVector2D Off = GasOffset(g);
		const FVector At = O + FVector(Off.X, Off.Y, 0);
		Block(At + FVector(0, 0, 20), FVector(2800, 2200, 20), Night, 0.0f, false);
		if (AActor* Canopy = Block(At + FVector(0, 0, 520), FVector(3000, 2400, 80), Magenta, 0.0f, false)) { Paint(Canopy, NeonA); Canopies.Add(Canopy); }
		for (int32 c = 0; c < 4; ++c)
		{
			Block(At + FVector(c % 2 ? 1100 : -1100, c / 2 ? 900 : -900, 260), FVector(80, 80, 520), Night, 0.0f, false);
		}
		Label(At + FVector(0, -1400, 700), 180.0f, TEXT("GAS"), FColor(120, 240, 255), 160.0f);
		Label(At + FVector(0, -1400, 520), 180.0f, TEXT("ON THE TAB"), FColor(255, 140, 230), 48.0f);
		Light(At + FVector(0, 0, 460), Cyan, 5000.0f, 1800.0f);
	}

	// The yard: dispatch, the bays, the gate.
	const FVector Yard = O + FVector(YardOffset().X, YardOffset().Y, 0);
	Block(Yard + FVector(0, 0, 4), FVector(13000, 13000, 10), FLinearColor(0.07f, 0.04f, 0.1f), 0.0f, false);
	Prop(TEXT("/Game/Cyberpunk_City/Meshes/Buildings_modules/SM_street_building_01"), Yard + FVector(-2800, 3200, 0), 180.0f, FVector(2400, 1800, 0), true);
	Label(Yard + FVector(-2800, 2200, 620), 90.0f, TEXT("MILE HIGH RECOVERY"), FColor(255, 140, 230), 70.0f);
	Label(Yard + FVector(-2800, 2200, 500), 90.0f, TEXT("DISPATCH"), FColor(120, 240, 255), 40.0f);
	Block(Yard + FVector(-2200, 2600, 40), FVector(900, 700, 50), FLinearColor(0.15f, 0.5f, 0.3f), 0.0f, false);
	Label(Yard + FVector(-2200, 2100, 160), 90.0f, TEXT("YARD PUMP"), FColor(160, 255, 190), 36.0f);
	Block(Yard + FVector(-1400, 1400, 16), FVector(900, 700, 12), FLinearColor(1.0f, 0.8f, 0.3f) * 0.45f, 0.0f, false);
	Label(Yard + FVector(-1400, 1000, 160), 90.0f, TEXT("SPARE CAN"), FColor(255, 210, 80), 36.0f);
	Label(Yard + FVector(-1400, 1000, 100), 90.0f, TEXT("TAB  14"), FColor(210, 200, 230), 28.0f);
	Label(Yard + FVector(-2200, 2100, 100), 90.0f, TEXT("FREE UNTIL YOU OWE"), FColor(200, 220, 230), 28.0f);
	Light(Yard + FVector(-2200, 2400, 500), Magenta, 5000.0f, 2400.0f);

	const int32 BayRole[] = { 1, 2, 3, 4 };
	const FLinearColor BayColour[] = { FLinearColor(1.0f, 0.85f, 0.2f), Magenta, Cyan, FLinearColor(0.3f, 1.0f, 0.5f) };
	for (int32 b = 0; b < 4; ++b)
	{
		const float Side = b < 2 ? -5200.0f + b * 1600.0f : 3600.0f + (b - 2) * 1600.0f;
		const FVector Bay = Yard + FVector(1400.0f, Side, 16);
		const FRepoRole& BayJob = RepoRole(BayRole[b]);
		Block(Bay, FVector(1400, 1100, 16), BayColour[b] * 0.45f, 0.0f, false);
		Light(Bay + FVector(0, 0, 400), BayColour[b], 2500.0f, 900.0f);
		Label(Bay + FVector(-900, 0, 180), 180.0f, BayJob.Name, FColor(240, 235, 255), 56.0f);
		Label(Bay + FVector(-900, 0, 100), 180.0f, FString::Printf(TEXT("TAB  %.0f"), BayJob.Cost), FColor(210, 200, 230), 32.0f);
	}

	// The gate faces the grid (the trucks point +X, toward the city): a neon arch.
	const FVector Gate = Yard + FVector(5200, 0, 0);
	Prop(TEXT("/Game/Synthwave_city/Meshes/Arch/SM_arch"), Gate, 0.0f, FVector(600, 3600, 0));
	for (int32 Side = -1; Side <= 1; Side += 2)
	{
		Block(Gate + FVector(0, Side * 1600.0f, 400), FVector(80, 80, 800), Night);
	}
	if (AActor* Bar = Block(Gate + FVector(0, 0, 860), FVector(80, 3400, 100), Magenta, 0.0f, false)) { Paint(Bar, NeonA); }
	Label(Gate + FVector(-60, 0, 1000), 180.0f, TEXT("REPO RALLY"), FColor(255, 140, 230), 90.0f);
	Label(Gate + FVector(-60, 0, 720), 180.0f, TEXT("HOOK. TOW. DROP."), FColor(120, 240, 255), 40.0f);
	Label(Yard + FVector(2800, 0, 180), 180.0f, TEXT("DROP TOWS HERE"), FColor(160, 255, 190), 44.0f);
	if (AActor* Drop = Block(Yard + FVector(2800, 0, 14), FVector(2200, 2200, 6), FLinearColor(0.3f, 1.0f, 0.5f), 0.0f, false)) { Paint(Drop, NeonB); }

	const FVector RulesBoard = Yard + FVector(3600, -2400, 0);
	Block(RulesBoard + FVector(0, 0, 280), FVector(40, 1500, 560), FLinearColor(0.04f, 0.02f, 0.06f));
	const TCHAR* Rules[] = {
		TEXT("RIGHT MOUSE aims.  Q fires the magnet."),
		TEXT("It locks on and yanks the car in."),
		TEXT("Drive it here and Q to drop it."),
		TEXT("Quick and clean pays. A scarred car pays worse."),
		TEXT("A hook in the wrong car is a fine."),
		TEXT("GAS is on the tab. The tab can go red."),
		TEXT("In debt, other crews hunt you."),
		TEXT("Bays: runner, net, winch, tank."),
	};
	for (int32 i = 0; i < 8; ++i)
	{
		Label(RulesBoard + FVector(-40, 0, 500 - i * 52), 180.0f, Rules[i], FColor(230, 220, 255), 26.0f);
	}
	for (int32 i = 0; i < 5; ++i)
	{
		Prop(TEXT("/Game/Sci_Fi_city/Meshes/Crates/SM_crate_03"), Yard + FVector(-4600, -2000 + i * 700, 0), 15.0f * i, FVector(0, 0, 160));
	}
}

void ADCCArcadeKart::ServerBegin()
{
	GoAt = ServerTime() + 18.0;
	SpawnStaff();
	for (int32 i = 0; i < 4; ++i)
	{
		SpawnMark(i);
	}

	// Two other crews. They work the marks until somebody owes money.
	const TCHAR* RivalNames[] = { TEXT("Night Shift"), TEXT("Other Crew") };
	for (int32 i = 0; i < 2; ++i)
	{
		const FTransform Where(FRotator::ZeroRotator, Crossing(StreetRadius - i, StreetRadius));
		FActorSpawnParameters Params;
		Params.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AdjustIfPossibleButAlwaysSpawn;
		if (ADCCRover* Car = GetWorld()->SpawnActor<ADCCRover>(ADCCRover::StaticClass(), Where, Params))
		{
			Car->RallyDuty = 3;
			Car->SetWrecker(true);
			Car->RallyPlate = RivalNames[i];
			Car->bAutoPilotBoostOnLaunch = false;
			Car->AutoPilotAcceptanceRadius = 900.0f;
			Car->SetRacePace(0.50f);
			FTruck Rival;
			Rival.Car = Car;
			Rival.Name = RivalNames[i];
			Rival.BasePace = 0.50f;
			Rival.Route = i;
			Rival.Fuel = 100.0f;
			Trucks.Add(Rival);
		}
	}

	// Traffic is a ghost. It slows you when you drive through it, and it does not jam the grid into a parking lot.
	for (int32 i = -StreetRadius; i <= StreetRadius; ++i)
	{
		for (int32 Axis = 0; Axis < 2; ++Axis)
		{
			const FVector A = Axis == 0 ? Crossing(-StreetRadius, i) : Crossing(i, -StreetRadius);
			const FVector B = Axis == 0 ? Crossing(StreetRadius, i) : Crossing(i, StreetRadius);
			const FVector Dir = (B - A).GetSafeNormal2D();
			const FRotator Face = Dir.Rotation();
			// Six per street, in the lanes (shoulders stay for the parked cars). Solid to trucks,
			// still passing through each other, and never dropped into a block.
			const float Span = FVector::Dist2D(A, B);
			const FVector Right = FVector(-Dir.Y, Dir.X, 0.0f);
			const float Laterals[] = { -900.0f, -300.0f, 300.0f, 900.0f, -600.0f, 600.0f };
			for (int32 Lane = 0; Lane < 6; ++Lane)
			{
			const float Along = Span * ((Lane + 0.5f) / 6.0f);
			const float Lateral = Laterals[Lane];
			const FVector At = A + Dir * Along + Right * Lateral + FVector(0, 0, 0);
			FActorSpawnParameters Params;
			Params.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
			if (ADCCRover* Car = GetWorld()->SpawnActor<ADCCRover>(ADCCRover::StaticClass(), FTransform(Face, At), Params))
			{
				Car->RallyDuty = 2;
				Car->RallyPlate = TEXT("Traffic");
				Car->bAutoPilotBoostOnLaunch = false;
				Car->AutoPilotAcceptanceRadius = 800.0f;
				Car->SetRacePace(0.30f + (Axis == 0 ? 0.0f : 0.04f));
				FTruck Traffic;
				Traffic.Car = Car;
				Traffic.Name = TEXT("Traffic");
				Traffic.BasePace = Car->RaceSpeedScale;
				Traffic.Route = 100 + (Axis * 10) + (i + StreetRadius);
				// Heading for the next crossing along its street (TickTraffic takes it from there).
				const int32 Step = Lane % 2 == 0 ? 1 : -1;
				const int32 Next = FMath::Clamp(FMath::RoundToInt(((Axis == 0 ? At.X : At.Y) - (Axis == 0 ? GetActorLocation().X : GetActorLocation().Y)) / StreetSpacing) + Step, -StreetRadius, StreetRadius);
				Traffic.To = Axis == 0 ? FIntPoint(Next, i) : FIntPoint(i, Next);
				Traffic.From = Axis == 0 ? FIntPoint(Next - Step, i) : FIntPoint(i, Next - Step);
				Car->bAutoPilotActive = false;
				Traffic.BasePace = 0.30f + 0.04f * ((i + Lane + Axis) % 4);
				Trucks.Add(Traffic);
			}
			}
		}
	}
}

FVector ADCCArcadeKart::ParkSpot(int32 Slot) const
{
	// On the sidewalk, off the middle of the face (that is where the storefront stands) and
	// clear of the towers. The old spot sat 62 m from the block centre, which is inside a tower.
	const int32 Which = Slot & 3;
	const int32 StreetY[4] = { -1, 0, -1, 1 };
	const int32 BlockX[4] = { -1, 0, 0, -1 };
	const float AlongFrac[4] = { 0.28f, 0.72f, 0.30f, 0.70f };
	const float Side = (Which & 1) ? 1.0f : -1.0f;
	const float X = (BlockX[Which] + AlongFrac[Which]) * StreetSpacing;
	const float Y = StreetY[Which] * StreetSpacing + Side * (RoadHalf + 780.0f);
	return GetActorLocation() + FVector(X, Y, 160.0f);
}

FVector ADCCArcadeKart::Bumper(const ADCCRover* Truck) const
{
	// Where a locked mark is pulled to. A wrecker's magnet hangs about 640 cm
	// behind the cab, so the mark ends up under the disc. A plain car still
	// uses the old hitch, just behind its own bumper.
	if (!Truck)
	{
		return FVector::ZeroVector;
	}
	const float Back = Truck->bWrecker ? 700.0f : 780.0f;
	return Truck->GetActorLocation() - Truck->GetActorForwardVector() * Back;
}

bool ADCCArcadeKart::InYard(const ADCCRover* Car) const
{
	if (!Car)
	{
		return false;
	}
	const FVector Yard = GetActorLocation() + FVector(YardOffset().X, YardOffset().Y, 0.0f);
	return FVector::Dist2D(Car->GetActorLocation(), Yard) < 7200.0f;
}

void ADCCArcadeKart::SpawnMark(int32 Slot)
{
	if (!Marks.IsValidIndex(Slot))
	{
		Marks.SetNum(Slot + 1);
	}
	FMark& Mark = Marks[Slot];
	for (FTruck& Truck : Trucks)
	{
		if (Truck.HookedId == Slot)
		{
			Truck.HookedId = -1;
		}
	}
	if (ADCCRover* Old = Mark.Car.Get())
	{
		Old->bRallyHeld = false;
		Old->StopAutoPilot();
		Old->Destroy();
	}
	for (const TWeakObjectPtr<ADCCRover>& Block : Mark.Blockers)
	{
		if (ADCCRover* Box = Block.Get())
		{
			Box->Destroy();
		}
	}
	Mark.Blockers.Reset();
	Mark.Name = MarkNames[NamesUsed % 10];
	++NamesUsed;
	Mark.Kind = FMath::Clamp(Wave, 0, 3);
	Mark.Damage = 0.0f;
	Mark.HookedAt = -1.0;
	Mark.bShootsBack = false;
	Mark.Tower = nullptr;
	Mark.bDocked = false;
	Mark.bAlerted = false;
	Mark.Hover = 0.0f;
	Mark.ScarTold = 0;
	Mark.bHeavy = false;
	Mark.bRetired = false;
	Mark.bHeavyTold = false;
	Mark.PayMul = 1.0f;
	Mark.Route = (Slot + NamesUsed) % 4;
	Mark.BasePace = Mark.Kind >= 2 ? 0.38f + (Slot % 4) * 0.04f : 0.0f;
	if (Mark.Kind == 3)
	{
		Mark.BasePace += 0.06f;
	}

	TArray<FVector> Route;
	FillRoute(Mark.Route, Route);
	const bool bParked = Mark.Kind < 2;
	const FVector At = bParked ? ParkSpot(Slot) : (Route.Num() > 0 ? Route[0] : Crossing(0, 0));
	const FRotator Face = bParked ? FRotator(0.0f, (Slot * 40) % 180, 0.0f) : FRotator::ZeroRotator;
	FActorSpawnParameters Params;
	Params.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AdjustIfPossibleButAlwaysSpawn;
	if (ADCCRover* Car = GetWorld()->SpawnActor<ADCCRover>(ADCCRover::StaticClass(), FTransform(Face, At), Params))
	{
		Car->RallyDuty = 1;
		Car->RallyPlate = Mark.Name;
		Car->RallyStrain = 0.0f;
		Car->bRallyHeld = false;
		Car->bAutoPilotBoostOnLaunch = false;
		Car->AutoPilotAcceptanceRadius = 1100.0f;
		Car->SetRacePace(bParked ? 0.20f : Mark.BasePace);
		Mark.Car = Car;
		if (!bParked && bGo)
		{
			Car->StartAutoPilotRoute(Route, true);
		}
	}

	// Boxed-in: four parked cars around the mark. Rover-vs-rover does not block (both are pawns),
	// so the scrape is checked by distance while the line is pulling. A straight yank hits one.
	// A diagonal pull threads the gap.
	if (Mark.Kind == 1)
	{
		// Along the curb, not into the tower or out into the lane.
		const FVector Offsets[] = {
			FVector(1200, 0, 0), FVector(-1200, 0, 0), FVector(2400, 280, 0), FVector(-2400, -280, 0),
		};
		for (int32 i = 0; i < 4; ++i)
		{
			FActorSpawnParameters BoxParams;
			BoxParams.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
			const FTransform Where(FRotator(0.0f, 20.0f * i, 0.0f), At + Offsets[i]);
			if (ADCCRover* Box = GetWorld()->SpawnActor<ADCCRover>(ADCCRover::StaticClass(), Where, BoxParams))
			{
				Box->RallyDuty = 4;
				Box->RallyPlate = TEXT("Parked");
				Box->bAutoPilotBoostOnLaunch = false;
				Box->SetRacePace(0.20f);
				Mark.Blockers.Add(Box);
			}
		}
	}
}

void ADCCArcadeKart::SpawnBonus()
{
	if (bBonusSpawned)
	{
		return;
	}
	bBonusSpawned = true;
	const int32 Slot = Marks.Num();
	SpawnMark(Slot);
	if (!Marks.IsValidIndex(Slot))
	{
		return;
	}
	FMark& Mark = Marks[Slot];
	Mark.bHeavy = true;
	Mark.PayMul = 2.6f;
	Mark.Name = TEXT("Arrears Coach");
	Mark.Kind = 2;
	Mark.BasePace = 0.34f;
	if (ADCCRover* Car = Mark.Car.Get())
	{
		Car->RallyPlate = Mark.Name;
		Car->SetActorScale3D(FVector(1.85f, 1.35f, 1.15f));
		Car->SetRacePace(Mark.BasePace);
		TArray<FVector> Route;
		FillRoute(Mark.Route, Route);
		if (bGo && Route.Num() > 0)
		{
			Car->SetActorLocation(Route[0] + FVector(0.0f, 0.0f, 40.0f));
			Car->StartAutoPilotRoute(Route, true);
		}
	}
	for (FTruck& Truck : Trucks)
	{
		if (Truck.bPlayer)
		{
			Tell(Truck.Car.Get(), TEXT("Bonus on the board. The Arrears Coach pays big, and it is heavy. A bored-out engine is what keeps your speed."));
		}
	}
}

void ADCCArcadeKart::SeatPlayers()
{
	TArray<APlayerController*> Players;
	GetPlayers(Players);
	for (APlayerController* PC : Players)
	{
		ADCCCharacterBase* Crawler = Cast<ADCCCharacterBase>(PC->GetPawn());
		if (!Crawler || Seated.Contains(PC))
		{
			continue;
		}
		const FTransform Slot = GridSlot(Seated.Num());
		FActorSpawnParameters Params;
		Params.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AdjustIfPossibleButAlwaysSpawn;
		if (ADCCRover* Car = GetWorld()->SpawnActor<ADCCRover>(ADCCRover::StaticClass(), Slot, Params))
		{
			// Beside the truck, not in it. E gets in (the same interact every rover already has).
			// The shift report puts them back on their feet.
			const FVector Beside = Slot.GetLocation() + FVector(-900.0f, 0.0f, 80.0f);
			Crawler->SetActorLocation(Beside);
			Car->RallyDuty = 0;
			Car->SetWrecker(true);
			Car->bAutoPilotBoostOnLaunch = false;
			Car->SetRacePace(0.52f);
			FTruck Truck;
			Truck.Car = Car;
			Truck.Name = PlayerName(PC->PlayerState);
			Truck.bPlayer = true;
			Truck.Tools.Add(DCCRallyItems::Hook);
			Truck.BasePace = 0.52f;
			Truck.Ledger = 48.0f;
			{
				// What this driver has bolted on at the Chop Shop.
				const FString ChopKey = DCCProfile::KeyFor(PC->PlayerState);
				Truck.ChopPlating = DCCChopShop::GetLevel(ChopKey, DCCChopShop::Plating);
				Truck.ChopTank = DCCChopShop::GetLevel(ChopKey, DCCChopShop::Tank);
				Truck.ChopEngine = DCCChopShop::GetLevel(ChopKey, DCCChopShop::Engine);
				Truck.ChopLine = DCCChopShop::GetLevel(ChopKey, DCCChopShop::Line);
				Truck.ChopPit = DCCChopShop::GetLevel(ChopKey, DCCChopShop::PitCrew);
			}
			Truck.Fuel = DCCChopShop::TankCapacity(Truck.ChopTank);
			Car->RallyPlate = Truck.Name;
			// KMHR comes up as you sit down, already in the middle of Repo Rally.
			// [T] tunes out, or back in on a random song, still mid-file.
			Car->TuneIn(0);
			Trucks.Add(Truck);
			Seated.Add(PC);
			SyncKit(Trucks.Last());
		}
	}
}

void ADCCArcadeKart::SpawnStaff()
{
	const FVector Yard = GetActorLocation() + FVector(YardOffset().X, YardOffset().Y, 90.0f);
	struct FPlace { const TCHAR* Who; FVector At; float Yaw; float Radius; };
	const FPlace Places[] = {
		{ TEXT("Rhee"), Yard + FVector(-1800, 4600, 0), -90.0f, 2400.0f },
		{ TEXT("Jan"),  Yard + FVector(-4200, 1800, 0), 0.0f, 2000.0f },
		{ TEXT("Diaz"), Yard + FVector(3600, -1800, 0), 180.0f, 2000.0f },
	};
	for (const FPlace& Place : Places)
	{
		const FTransform Where(FRotator(0.0f, Place.Yaw, 0.0f), Place.At);
		if (ADCCImpoundNPC* NPC = GetWorld()->SpawnActorDeferred<ADCCImpoundNPC>(ADCCImpoundNPC::StaticClass(), Where,
			nullptr, nullptr, ESpawnActorCollisionHandlingMethod::AlwaysSpawn))
		{
			NPC->Who = FName(Place.Who);
			NPC->TalkRadius = Place.Radius;
			NPC->FinishSpawning(Where);
		}
	}
}

void ADCCArcadeKart::ServerTick(float DeltaSeconds)
{
	SeatPlayers();
	TickTraffic();
	TickThugs(DeltaSeconds);
	const double Now = ServerTime();

	// The briefing on the radio, while the countdown runs (the HUD shows the marks at the same time).
	if (!bGo && !bBriefed && Now >= GoAt - 15.0 && Seated.Num() > 0)
	{
		bBriefed = true;
		for (FTruck& Truck : Trucks)
		{
			if (Truck.bPlayer)
			{
				Radio(Truck.Car.Get(), TEXT("RR_Disp_Brief"));
			}
		}
	}

	if (!bGo && Now >= GoAt)
	{
		bGo = true;
		ShiftEnd = Now + ShiftSeconds;
		ShiftEndsAt = ShiftEnd;
		for (int32 i = 0; i < Marks.Num(); ++i)
		{
			ADCCRover* Car = Marks[i].Car.Get();
			if (Car && Marks[i].Kind >= 2)
			{
				TArray<FVector> Route;
				FillRoute(Marks[i].Route, Route);
				Car->StartAutoPilotRoute(Route, true);
			}
		}
		for (FTruck& Truck : Trucks)
		{
			ADCCRover* Car = Truck.Car.Get();
			if (!Car || Truck.bPlayer)
			{
				continue;
			}
			if (Car->RallyDuty == 2 && false)
			{
				const int32 Code = Truck.Route - 100;
				const int32 Axis = Code / 10;
				const int32 Index = (Code % 10) - StreetRadius;
				TArray<FVector> Route;
				if (Axis == 0)
				{
					Route.Add(Crossing(-StreetRadius, Index));
					Route.Add(Crossing(StreetRadius, Index));
				}
				else
				{
					Route.Add(Crossing(Index, -StreetRadius));
					Route.Add(Crossing(Index, StreetRadius));
				}
				Car->StartAutoPilotRoute(Route, true);
			}
			else if (Car->RallyDuty == 3)
			{
				TArray<FVector> Route;
				FillRoute(Truck.Route, Route);
				Car->StartAutoPilotRoute(Route, true);
			}
		}
		for (FTruck& Truck : Trucks)
		{
			if (Truck.bPlayer)
			{
				Tell(Truck.Car.Get(), TEXT("Flag's down. Right mouse aims the hook. Amber columns are the marks."));
				Radio(Truck.Car.Get(), TEXT("RR_Disp_Go"));
			}
		}
	}

	if (bGo && !bPaid)
	{
		TickCity(DeltaSeconds);
	}
	RefreshBoard();

	Title = TEXT("REPO RALLY  -  hook, tow, drop");
	Accent = FLinearColor(0.85f, 0.55f, 0.28f);
	StatusLines.Reset();
	if (!bGo)
	{
		// The briefing panel (HUD) carries the instructions now; this is just the countdown.
		StatusLines.Add(FString::Printf(TEXT("Shift starts in %d"), FMath::Max(0, FMath::CeilToInt(GoAt - Now))));
	}
	else
	{
		const int32 Left = FMath::Max(0, FMath::CeilToInt(ShiftEnd - Now));
		StatusLines.Add(FString::Printf(TEXT("Shift  %d:%02d"), Left / 60, Left % 60));
		FString MarksLine;
		for (const FMark& Mark : Marks)
		{
			if (!Mark.Car.IsValid())
			{
				continue;
			}
			FString State;
			if (Mark.bDocked)
			{
				State = TEXT("TOW");
			}
			else if (Mark.Tower.IsValid())
			{
				State = FString::Printf(TEXT("%d%%"), FMath::RoundToInt(FMath::Clamp(Mark.Car->RallyStrain, 0.0f, 1.0f) * 100.0f));
			}
			else if (Mark.bAlerted)
			{
				State = TEXT("UP");
			}
			else if (Mark.Kind == 0)
			{
				State = TEXT("parked");
			}
			else if (Mark.Kind == 1)
			{
				State = TEXT("boxed");
			}
			else
			{
				State = TEXT("running");
			}
			MarksLine += FString::Printf(TEXT("%s%s %s"), MarksLine.IsEmpty() ? TEXT("") : TEXT("    "), *Mark.Name, *State);
		}
		if (!MarksLine.IsEmpty())
		{
			StatusLines.Add(MarksLine);
		}
		FString Debt;
		for (const FTruck& Truck : Trucks)
		{
			if (Truck.bPlayer && Truck.Ledger < 0.0f)
			{
				Debt += FString::Printf(TEXT("%s%s %.0f"), Debt.IsEmpty() ? TEXT("") : TEXT(", "), *Truck.Name, Truck.Ledger);
			}
		}
		if (!Debt.IsEmpty())
		{
			StatusLines.Add(TEXT("! IN DEBT  ") + Debt + TEXT("  — other crews are hunting"));
		}
	}

	TArray<const FTruck*> Order;
	for (const FTruck& Truck : Trucks)
	{
		if (Truck.bPlayer)
		{
			Order.Add(&Truck);
		}
	}
	Order.Sort([](const FTruck& A, const FTruck& B)
	{
		if (A.Score != B.Score) { return A.Score > B.Score; }
		return A.Nabs > B.Nabs;
	});
	FString Standings;
	for (int32 i = 0; i < Order.Num(); ++i)
	{
		Standings += FString::Printf(TEXT("%s%d. %s  %.0f"), i ? TEXT("    ") : TEXT(""), i + 1, *Order[i]->Name, Order[i]->Score);
	}
	if (!Standings.IsEmpty())
	{
		StatusLines.Add(Standings);
	}
	const DCCArcade::FGame* Game = DCCArcade::Find(TEXT("Kart"));
	StatusLines.Add(FString::Printf(TEXT("Best shifts ever: %s"), *DCCProfile::Leaderboard(GetWorld(), TEXT("Kart"), TEXT("BestScore"), true,
		[Game](float V) { return Game ? DCCArcade::FormatRecord(*Game, V) : FString::SanitizeFloat(V); })));

	if (bGo && !bPaid && !bWarned30 && ShiftEnd - Now <= 30.0)
	{
		bWarned30 = true;
		for (FTruck& Truck : Trucks)
		{
			if (Truck.bPlayer)
			{
				Radio(Truck.Car.Get(), TEXT("RR_Disp_Last30"));
			}
		}
	}

	if (bGo && !bPaid && Now >= ShiftEnd)
	{
		bPaid = true;
		for (int32 i = 0; i < Order.Num(); ++i)
		{
			const FTruck* Truck = Order[i];
			ADCCRover* Car = Truck->Car.Get();
			APlayerState* PlayerState = Car ? Car->GetPlayerState() : nullptr;
			if (!PlayerState)
			{
				continue;
			}
			int32 Tickets = Truck->Score <= 0.0f ? 8 : i == 0 ? 70 : i == 1 ? 50 : i == 2 ? 36 : 24;
			Tickets += FMath::Clamp(FMath::RoundToInt(Truck->Score / 5.0f), 0, 40);
			const_cast<FTruck*>(Truck)->Tickets = Tickets; // the shift report shows it
			FString Why = FString::Printf(TEXT("Repo Rally, %d nab%s, banked %.0f"),
				Truck->Nabs, Truck->Nabs == 1 ? TEXT("") : TEXT("s"), Truck->Score);
			if (Truck->Ledger < 0.0f)
			{
				Why += TEXT(". Finished on the tab");
			}
			DCCProfile::Add(PlayerState, TEXT("Kart"), TEXT("Shifts"), 1.0f);
			DCCProfile::Add(PlayerState, TEXT("Kart"), TEXT("Nabs"), Truck->Nabs);
			if (i == 0 && Truck->Score > 0.0f)
			{
				DCCProfile::Add(PlayerState, TEXT("Kart"), TEXT("Wins"), 1.0f);
			}
			DCCProfile::RecordBest(PlayerState, TEXT("Kart"), TEXT("BestScore"), Truck->Score, true);
			if (Truck->BestNab > 0.0f)
			{
				DCCProfile::RecordBest(PlayerState, TEXT("Kart"), TEXT("BestNab"), Truck->BestNab, false);
			}
			UDCCWalletComponent::AwardTickets(PlayerState, Tickets, Why);
		}
		ReturnToLobbyIn(15.0f);
	}
}

void ADCCArcadeKart::TickCity(float DeltaSeconds)
{
	const double Now = ServerTime();
	const FVector O = GetActorLocation();
	const FVector Yard = O + FVector(YardOffset().X, YardOffset().Y, 0);

	Nets.RemoveAll([Now](const FDCCRallyBoot& Net) { return Net.Until <= Now; });

	// The shift starts with parked cars and gets meaner on a clock, whether or not anyone has dropped one.
	{
		const double Elapsed = Now - (ShiftEnd - ShiftSeconds);
		const int32 Want = Elapsed >= 130.0 ? 3 : Elapsed >= 85.0 ? 2 : Elapsed >= 40.0 ? 1 : 0;
		if (Want > Wave)
		{
			Wave = Want;
			const TCHAR* Line = Wave == 1
				? TEXT("The easy ones are gone. These are boxed in. Yanking one out scars it.")
				: Wave == 2
				? TEXT("They're running. Hook one, get it on the bumper, bring it home.")
				: TEXT("Some of them spook. Get close, or miss the hook, and they leave the ground.");
			TArray<int32> Upgrade;
			for (int32 i = 0; i < Marks.Num(); ++i)
			{
				if (!Marks[i].bHeavy && !Marks[i].Tower.IsValid() && Marks[i].Kind < Wave)
				{
					Upgrade.Add(i);
				}
			}
			for (int32 Slot : Upgrade)
			{
				SpawnMark(Slot);
			}
			for (FTruck& Truck : Trucks)
			{
				if (Truck.bPlayer)
				{
					Tell(Truck.Car.Get(), Line);
					Radio(Truck.Car.Get(), Wave == 1 ? TEXT("RR_Disp_Wave1") : Wave == 2 ? TEXT("RR_Disp_Wave2") : TEXT("RR_Disp_Wave3"));
				}
			}
		}
		if (!bBonusSpawned && Elapsed >= 28.0)
		{
			SpawnBonus();
		}
	}

	for (FTruck& Truck : Trucks)
	{
		ADCCRover* Car = Truck.Car.Get();
		if (!Truck.bPlayer || !Car)
		{
			continue;
		}
		const float Cap = DCCChopShop::TankCapacity(Truck.ChopTank);
		const float Speed = Car->GetVelocity().Size2D();
		if (Speed > 250.0f)
		{
			const float Cruise = RepoRole(Truck.Role).Pace * RepoRole(Truck.Role).Engine * 5600.0f;
			const float Fast = Speed > Cruise * 1.25f ? 2.2f : 1.0f;
			Truck.Fuel = FMath::Max(0.0f, Truck.Fuel - 1.55f * RepoRole(Truck.Role).Burn * DCCChopShop::FuelBurn(Truck.ChopTank) * Fast * DeltaSeconds);
		}

		bool bAtStreet = false;
		for (int32 g = 0; g < 2; ++g)
		{
			const FVector2D Off = GasOffset(g);
			if (FVector::Dist2D(Car->GetActorLocation(), O + FVector(Off.X, Off.Y, 0)) < 900.0f && Speed < 700.0f)
			{
				bAtStreet = true;
			}
		}
		const FVector Company = Yard + FVector(-2200, 2600, 0);
		const bool bAtYard = FVector::Dist2D(Car->GetActorLocation(), Company) < 800.0f && Speed < 700.0f;
		if (bAtYard && Truck.Ledger >= 0.0f && Truck.Fuel < Cap)
		{
			Truck.Fuel = FMath::Min(Cap, Truck.Fuel + 22.0f * DCCChopShop::PitSpeed(Truck.ChopPit) * DeltaSeconds);
		}
		else if (bAtYard && Truck.Ledger < 0.0f && Now > Truck.NextTell)
		{
			Tell(Car, TEXT("Yard pump is closed. Jan does not fuel a truck that's on the tab."));
			Truck.NextTell = Now + 6.0;
		}
		else if (bAtStreet && Truck.Fuel < Cap)
		{
			const float Before = Truck.Ledger;
			const float Gain = FMath::Min(40.0f * DCCChopShop::PitSpeed(Truck.ChopPit) * DeltaSeconds, Cap - Truck.Fuel);
			Truck.Fuel += Gain;
			Truck.Ledger -= Gain * 0.08f;
			if (Before >= 0.0f && Truck.Ledger < 0.0f)
			{
				Truck.bDebt = true;
				Tell(Car, TEXT("The tab went red. Other crews will treat you like a skip."));
				Radio(Car, TEXT("RR_Disp_Debt"));
			}
		}

		// Sitting at a station patches the hull (2026-11-14, Zoe: "damage to the vehicle restored when you stay
		// in the gas station"). Free at the street stations; the yard does it too unless you owe.
		Truck.bRepairing = false;
		if ((bAtStreet || (bAtYard && Truck.Ledger >= 0.0f)) && Truck.Hull < 100.0f)
		{
			Truck.bRepairing = true;
			Truck.Hull = FMath::Min(100.0f, Truck.Hull + 20.0f * DCCChopShop::PitSpeed(Truck.ChopPit) * DeltaSeconds);
			if (Truck.Hull >= 100.0f)
			{
				Tell(Car, TEXT("Patched up. Hull's back to full."));
			}
		}

		if (Truck.Fuel <= 0.0f && Truck.Cans > 0)
		{
			Truck.Cans = 0;
			Truck.Fuel = Cap;
			Truck.bLimpTold = false;
			Tell(Car, TEXT("Spare can. The tank is full."));
		}
		if (Truck.Fuel <= 0.0f && !Truck.bLimpTold)
		{
			Truck.bLimpTold = true;
			Tell(Car, TEXT("Empty. You're limping. Find a station, or Jan if you still have tab."));
		}
		if (Truck.Fuel > 8.0f)
		{
			Truck.bLimpTold = false;
		}

		const int32 BayRoles[] = { 1, 2, 3, 4 };
		for (int32 b = 0; b < 4; ++b)
		{
			const float Side = b < 2 ? -5200.0f + b * 1600.0f : 3600.0f + (b - 2) * 1600.0f;
			const FVector Bay = Yard + FVector(1400.0f, Side, 0);
			const FString Key = FString::Printf(TEXT("%s|%d"), *Truck.Name, BayRoles[b]);
			const bool bIn = FVector::Dist2D(Car->GetActorLocation(), Bay) < 560.0f;
			if (bIn && !BayInside.Contains(Key))
			{
				BayInside.Add(Key);
				BuyRole(Truck, BayRoles[b]);
			}
			else if (!bIn)
			{
				BayInside.Remove(Key);
			}
		}

		// Jan's spare can. One in the bed at a time, fourteen on the tab.
		{
			const FVector CanPad = Yard + FVector(-1400.0f, 1400.0f, 0.0f);
			const FString CanKey = Truck.Name + TEXT("|can");
			const bool bOnCan = FVector::Dist2D(Car->GetActorLocation(), CanPad) < 520.0f;
			if (bOnCan && !BayInside.Contains(CanKey))
			{
				BayInside.Add(CanKey);
				if (Truck.Cans > 0)
				{
					Tell(Car, TEXT("There is already a can in the bed."));
				}
				else if (Truck.Ledger < 0.0f)
				{
					Tell(Car, TEXT("Jan wants the tab clear before she hands you a can."));
				}
				else
				{
					const float Before = Truck.Ledger;
					Truck.Ledger -= 14.0f;
					Truck.Cans = 1;
					Tell(Car, TEXT("Spare can in the bed. Fourteen on the tab. It dumps itself when you run dry."));
					if (Before >= 0.0f && Truck.Ledger < 0.0f)
					{
						Truck.bDebt = true;
						Tell(Car, TEXT("That put you on the tab. The yard pump is closed, and other crews will hunt you."));
						Radio(Car, TEXT("RR_Disp_Debt"));
					}
				}
			}
			else if (!bOnCan)
			{
				BayInside.Remove(CanKey);
			}
		}

		float Pace = RepoRole(Truck.Role).Pace * DCCChopShop::Pace(Truck.ChopEngine);
		if (Truck.HookedId >= 0 && Marks.IsValidIndex(Truck.HookedId))
		{
			const FMark& Hooked = Marks[Truck.HookedId];
			if (Hooked.bHeavy && Hooked.bDocked)
			{
				const float Eng = FMath::Clamp(Truck.ChopEngine / 3.0f, 0.0f, 1.0f);
				Pace *= FMath::Lerp(0.46f, 0.94f, Eng);
			}
		}
		if (Truck.Fuel <= 0.0f)
		{
			Pace *= 0.42f;
		}
		if (Now < Truck.HitUntil)
		{
			Pace = FMath::Min(Pace, 0.34f);
		}
		if (Truck.Hull <= 0.0f)
		{
			Pace = FMath::Min(Pace, 0.30f); // wrecked: limping to a station
		}
		else if (Truck.Hull < 50.0f)
		{
			Pace *= FMath::Lerp(0.72f, 1.0f, Truck.Hull / 50.0f);
		}
		Car->SetRacePace(Pace);

		const bool bNearYard = FVector::Dist2D(Car->GetActorLocation(), Yard) < 3800.0f;
		if (Truck.Ledger < 0.0f && bNearYard && Now > Truck.NextBark)
		{
			Tell(Car, TEXT("Rhee will not look at you. Diaz is already on the radio with the other crews."));
			Truck.NextBark = Now + 14.0;
		}
		if (Truck.Ledger >= 0.0f)
		{
			Truck.bDebt = false;
		}
	}

	// Rivals: hunt whoever owes, otherwise stay on a mark.
	for (FTruck& Rival : Trucks)
	{
		ADCCRover* Car = Rival.Car.Get();
		if (!Car || Car->RallyDuty != 3)
		{
			continue;
		}
		TArray<FTruck*> Debtors;
		for (FTruck& Truck : Trucks)
		{
			if (Truck.bPlayer && Truck.Ledger < 0.0f && Truck.Car.IsValid())
			{
				Debtors.Add(&Truck);
			}
		}
		Debtors.Sort([](const FTruck& A, const FTruck& B) { return A.Ledger < B.Ledger; });
		// Night Shift (route 0) peels off for the worst tab. The other crew stays on the marks
		// until a second truck owes, then it takes that one.
		FTruck* Prey = nullptr;
		if (Debtors.Num() > 0 && (Rival.Route == 0 || Debtors.Num() > 1))
		{
			Prey = Debtors[FMath::Min(Rival.Route, Debtors.Num() - 1)];
		}
		const bool bThisOneHunts = Prey != nullptr;
		if (bThisOneHunts)
		{
			const FVector Goal = Prey->Car->GetActorLocation();
			if (!Rival.bHunting || !Car->bAutoPilotActive)
			{
				Car->AutoPilotAcceptanceRadius = 280.0f;
				Car->StartAutoPilotRoute({ Goal }, true);
				Rival.bHunting = true;
			}
			else if (Car->AutoPilotWaypoints.Num() > 0)
			{
				Car->AutoPilotWaypoints[0] = Goal;
				Car->AutoPilotWaypointIndex = 0;
			}
			Rival.HookedId = -1;
			if (FVector::Dist2D(Car->GetActorLocation(), Goal) < 560.0f && Now > Rival.NextTell)
			{
				Prey->HitUntil = Now + 1.5;
				HurtTruck(*Prey, 12.0f, nullptr);
				Multicast_RallyHit(Car->GetActorLocation(), Prey->Car->GetActorLocation(), 0);
				const float Before = Prey->Ledger;
				Prey->Ledger -= 6.0f;
				Prey->bDebt = true;
				Tell(Prey->Car.Get(), FString::Printf(TEXT("%s clipped you. The tab got worse."), *Rival.Name));
				if (Before >= 0.0f)
				{
					Tell(Prey->Car.Get(), TEXT("You're on the tab now."));
				}
				Rival.NextTell = Now + 5.0;
			}
		}
		else
		{
			if (Rival.bHunting || !Car->bAutoPilotActive)
			{
				TArray<FVector> Route;
				FillRoute(Rival.Route, Route);
				Car->AutoPilotAcceptanceRadius = 900.0f;
				Car->StartAutoPilotRoute(Route, true);
				Rival.bHunting = false;
			}
			// Crews hunt debtors. They do not take marks. A mark is the players' job.
			Rival.HookedId = -1;
		}
		Car->SetRacePace(0.50f);
	}

	for (int32 mi = 0; mi < Marks.Num(); ++mi)
	{
		FMark& Mark = Marks[mi];
		ADCCRover* Car = Mark.Car.Get();
		if (!Car)
		{
			if (!Mark.bRetired && bGo && Now >= Mark.RespawnAt)
			{
				SpawnMark(mi);
			}
			continue;
		}

		bool bNet = false;
		for (const FDCCRallyBoot& Net : Nets)
		{
			if (FVector::Dist2D(Car->GetActorLocation(), Net.At) < 700.0f)
			{
				bNet = true;
				break;
			}
		}
		bool bTraffic = false;
		for (const FTruck& Truck : Trucks)
		{
			const ADCCRover* Other = Truck.Car.Get();
			if (Other && Other->RallyDuty == 2 && FVector::Dist2D(Car->GetActorLocation(), Other->GetActorLocation()) < 420.0f)
			{
				bTraffic = true;
				break;
			}
		}

		ADCCRover* TowerCar = Mark.Tower.Get();
		FTruck* Tower = TowerCar ? TruckFor(TowerCar) : nullptr;
		if (!Tower || Tower->HookedId != mi)
		{
			Tower = nullptr;
			TowerCar = nullptr;
			Mark.Tower = nullptr;
			Mark.bDocked = false;
		}

		// A skittish car bolts when a truck gets close, unless somebody already has a line on it.
		if (Mark.Kind == 3 && !Mark.bAlerted && !Tower)
		{
			for (FTruck& Truck : Trucks)
			{
				const ADCCRover* Who = Truck.Car.Get();
				if (Truck.bPlayer && Who && FVector::Dist(Who->GetActorLocation(), Car->GetActorLocation()) < 2200.0f)
				{
					AlertMark(mi, &Truck);
					break;
				}
			}
		}

		float Scale = Mark.BasePace;
		if (bNet) { Scale *= 0.40f; }
		if (bTraffic) { Scale *= 0.70f; }

		if (Tower && TowerCar)
		{
			const float SpeedNow = Car->GetVelocity().Size2D();
			const float Free = FMath::Max(400.0f, Mark.BasePace * 5600.0f * 0.85f);
			const float Ratio = Mark.Kind < 2 ? 0.0f : SpeedNow / Free;
			const float Ease = FMath::Lerp(1.7f, 0.45f, FMath::Clamp(Ratio, 0.0f, 1.0f));
			const FVector Goal = Bumper(TowerCar);
			const FVector Pos = Car->GetActorLocation();
			const float Dist = FVector::Dist(Pos, Goal);

			if (!Mark.bDocked && Dist < 240.0f)
			{
				Mark.bDocked = true;
				Tell(TowerCar, FString::Printf(TEXT("%s is locked on. Drive it to the yard and [Q] to drop it."), *Mark.Name));
				Radio(TowerCar, TEXT("RR_Disp_Bumper"));
				if (Mark.bHeavy && !Mark.bHeavyTold)
				{
					Mark.bHeavyTold = true;
					Tell(TowerCar, Tower->ChopEngine >= 3
						? TEXT("Heavy coach. This engine is pulling its weight.")
						: TEXT("Heavy coach. A stock engine crawls. The bored-out one keeps up."));
				}
			}

			float Drag = 1.0f;
			if (Mark.bDocked)
			{
				// A trailer, not a rigid boom (2026-11-14: "they just kind of clip into the side of a building").
				// It used to be snapped to one point 7 m behind the truck, so every turn swung it straight through
				// whatever was beside the road. Now its nose hangs under the magnet and its body trails along the
				// truck's own path, the way a towed car does, and it slides along walls instead of entering them.
				const FVector TFwd = TowerCar->GetActorForwardVector().GetSafeNormal2D();
				const FVector Hitch = TowerCar->GetActorLocation() - TFwd * (TowerCar->bWrecker ? 460.0f : 520.0f);
				FVector Trail = (Hitch - Pos).GetSafeNormal2D();
				if (Trail.IsNearlyZero())
				{
					Trail = TFwd;
				}
				const float TrailBack = Mark.bHeavy ? 640.0f : 250.0f;
				FVector Want = Hitch - Trail * TrailBack;
				Want.Z = Goal.Z;
				FHitResult Hit;
				Car->SetActorLocation(Want, true, &Hit);
				if (Hit.bBlockingHit && !Hit.bStartPenetrating)
				{
					// Slide along the wall for the rest of the move.
					const FVector Left = Want - Car->GetActorLocation();
					const FVector Slide = FVector::VectorPlaneProject(Left, Hit.Normal.GetSafeNormal2D());
					Car->SetActorLocation(Car->GetActorLocation() + Slide, true);
				}
				else if (Hit.bStartPenetrating)
				{
					// Already inside something (spawned that way, or a ramp): let it out rather than pin it.
					Car->SetActorLocation(Want, false, nullptr, ETeleportType::TeleportPhysics);
				}
				// Snagged far behind on a corner: pop it back under the magnet if that spot is clear.
				if (FVector::Dist2D(Car->GetActorLocation(), Hitch) > 900.0f)
				{
					Car->SetActorLocation(Goal, true);
				}
				Car->SetActorRotation(FRotator(0.0f, Trail.Rotation().Yaw, 0.0f));
				const float Cruise = RepoRole(Tower->Role).Pace * RepoRole(Tower->Role).Engine * 5600.0f;
				if (TowerCar->GetVelocity().Size2D() > Cruise * 1.25f)
				{
					Mark.Damage = FMath::Min(100.0f, Mark.Damage + 16.0f * DeltaSeconds);
				}
			}
			else
			{
				// Rover versus rover does not block, so a boxed-in pull is a distance check.
				// Straight through a parked car is slow and scars the mark. A gap does not.
				bool bScraping = false;
				auto Near = [&](const ADCCRover* Other)
				{
					if (!Other || Other == TowerCar || Other == Car || Other->RallyDuty == 2)
					{
						return;
					}
					if (FVector::Dist2D(Pos, Other->GetActorLocation()) < 520.0f)
					{
						bScraping = true;
					}
				};
				for (const TWeakObjectPtr<ADCCRover>& Block : Mark.Blockers)
				{
					Near(Block.Get());
				}
				for (const FTruck& Other : Trucks)
				{
					Near(Other.Car.Get());
				}
				if (bScraping)
				{
					Drag = 0.30f;
					Mark.Damage = FMath::Min(100.0f, Mark.Damage + 36.0f * DeltaSeconds);
				}
				// The magnet yanks. 2800 cm/s is a hard pull; the winch role is still 1.5x on top.
				// The coach is heavier. Engine levels get that yank back.
				const float Yank = Mark.bHeavy
					? FMath::Lerp(0.55f, 1.0f, FMath::Clamp(Tower->ChopEngine / 3.0f, 0.0f, 1.0f))
					: 1.0f;
				FVector Step = (Goal - Pos).GetSafeNormal() * (2800.0f * HookMul(*Tower) * Ease * Drag * Yank * DeltaSeconds);
				if (Step.SizeSquared() > FMath::Square(Dist))
				{
					Step = Goal - Pos;
				}
				// Swept, and sliding: the reel drags it around a corner, not through it (2026-11-14).
				FHitResult Hit;
				Car->SetActorLocation(Pos + Step, true, &Hit);
				if (Hit.bBlockingHit && !Hit.bStartPenetrating)
				{
					const FVector Slide = FVector::VectorPlaneProject(Pos + Step - Car->GetActorLocation(), Hit.Normal.GetSafeNormal2D());
					Car->SetActorLocation(Car->GetActorLocation() + Slide, true);
				}
				else if (Hit.bStartPenetrating)
				{
					Car->SetActorLocation(Pos + Step, false, nullptr, ETeleportType::TeleportPhysics);
				}
			}
			if (Car->Movement)
			{
				Car->Movement->StopMovementImmediately();
			}
			Car->bRallyHeld = true;
			Car->StopAutoPilot();
			Car->SetRacePace(0.20f);
			const float Line = FMath::Max(800.0f, HookLine(*Tower));
			Car->RallyStrain = Mark.bDocked ? 1.0f : FMath::Clamp(1.0f - (Dist - 220.0f) / Line, 0.0f, 1.0f);

			if (Mark.ScarTold < 1 && Mark.Damage >= 40.0f)
			{
				Mark.ScarTold = 1;
				Tell(TowerCar, TEXT("You're scarring it. The yard pays less for a beat-up car."));
			}
			else if (Mark.ScarTold < 2 && Mark.Damage >= 80.0f)
			{
				Mark.ScarTold = 2;
				Tell(TowerCar, TEXT("That one's wrecked. It will barely pay."));
			}
		}
		else if (Mark.bAlerted)
		{
			Mark.Hover = FMath::Min(1.0f, Mark.Hover + DeltaSeconds * 0.40f);
			FVector P = Car->GetActorLocation();
			const float GroundZ = GetActorLocation().Z + 160.0f;
			P.Z = FMath::FInterpTo(P.Z, GroundZ + Mark.Hover * 2600.0f, DeltaSeconds, 2.2f);
			Car->SetActorLocation(P);
			Car->bRallyHeld = true;

			const ADCCRover* Nearest = nullptr;
			float NearestDist = 0.0f;
			for (const FTruck& Truck : Trucks)
			{
				const ADCCRover* Who = Truck.Car.Get();
				if (!Truck.bPlayer || !Who)
				{
					continue;
				}
				const float D = FVector::Dist2D(Who->GetActorLocation(), P);
				if (!Nearest || D < NearestDist)
				{
					Nearest = Who;
					NearestDist = D;
				}
			}
			FVector Away = Nearest
				? (P - Nearest->GetActorLocation()).GetSafeNormal2D()
				: Car->GetActorForwardVector().GetSafeNormal2D();
			if (Away.IsNearlyZero())
			{
				Away = FVector(1, 0, 0);
			}
			FVector Goal = P + Away * 9000.0f;
			const FVector Centre = GetActorLocation();
			Goal.X = FMath::Clamp(Goal.X, Centre.X - MapHalf + 4000.0f, Centre.X + MapHalf - 4000.0f);
			Goal.Y = FMath::Clamp(Goal.Y, Centre.Y - MapHalf + 4000.0f, Centre.Y + MapHalf - 4000.0f);
			Goal.Z = P.Z;
			if (!Car->bAutoPilotActive || Car->AutoPilotWaypoints.Num() == 0)
			{
				Car->AutoPilotAcceptanceRadius = 500.0f;
				Car->StartAutoPilotRoute({ Goal }, true);
			}
			else
			{
				Car->AutoPilotWaypoints[0] = Goal;
				Car->AutoPilotWaypointIndex = 0;
			}
			Car->SetRacePace(FMath::Max(0.42f, Scale * 1.15f));
			Car->RallyStrain = 0.0f;
		}
		else
		{
			Car->bRallyHeld = false;
			Car->SetRacePace(Mark.Kind < 2 ? 0.20f : FMath::Max(0.22f, Scale));
			Car->RallyStrain = 0.0f;
			// A mark the shift has outgrown (cut loose after the wave moved on) comes back as the new kind.
			if (Mark.Kind < Wave)
			{
				for (const TWeakObjectPtr<ADCCRover>& Block : Mark.Blockers)
				{
					if (ADCCRover* Box = Block.Get())
					{
						Box->Destroy();
					}
				}
				Mark.Blockers.Reset();
				Car->Destroy();
				Mark.Car = nullptr;
				Mark.RespawnAt = Now;
			}
		}
	}

	for (FTruck& Truck : Trucks)
	{
		ADCCRover* Car = Truck.Car.Get();
		if (!Truck.bPlayer || !Car)
		{
			continue;
		}
		for (const FTruck& Other : Trucks)
		{
			const ADCCRover* Traffic = Other.Car.Get();
			if (!Traffic || Traffic->RallyDuty != 2)
			{
				continue;
			}
			if (FVector::Dist2D(Car->GetActorLocation(), Traffic->GetActorLocation()) < 400.0f)
			{
				Truck.HitUntil = FMath::Max(Truck.HitUntil, Now + 0.7);
				if (Now > Truck.NextTell)
				{
					Tell(Car, TEXT("Traffic. You lost your speed."));
					Truck.NextTell = Now + 4.0;
				}
				break;
			}
		}
		SyncKit(Truck);
	}
}

void ADCCArcadeKart::Release(int32 Slot, bool bTell)
{
	if (!Marks.IsValidIndex(Slot))
	{
		return;
	}
	FMark& Mark = Marks[Slot];
	if (ADCCRover* TowerCar = Mark.Tower.Get())
	{
		if (bTell)
		{
			Tell(TowerCar, TEXT("Line cut. No drop, no pay."));
			Radio(TowerCar, TEXT("RR_Disp_Cut"));
		}
	}
	for (FTruck& Truck : Trucks)
	{
		if (Truck.HookedId == Slot)
		{
			Truck.HookedId = -1;
		}
	}
	Mark.Tower = nullptr;
	Mark.bDocked = false;
	if (ADCCRover* Car = Mark.Car.Get())
	{
		Car->bRallyHeld = Mark.bAlerted;
		Car->StopAutoPilot();
		if (Mark.Kind >= 2 && !Mark.bAlerted && bGo)
		{
			TArray<FVector> Route;
			FillRoute(Mark.Route, Route);
			Car->SetRacePace(Mark.BasePace);
			Car->StartAutoPilotRoute(Route, true);
		}
		else if (!Mark.bAlerted)
		{
			Car->SetRacePace(0.20f);
		}
	}
}

void ADCCArcadeKart::Deliver(int32 Slot)
{
	if (!Marks.IsValidIndex(Slot))
	{
		return;
	}
	FMark& Mark = Marks[Slot];
	ADCCRover* TowerCar = Mark.Tower.Get();
	FTruck* Tower = TowerCar ? TruckFor(TowerCar) : nullptr;
	const double Now = ServerTime();
	const float Seconds = Mark.HookedAt > 0.0 ? static_cast<float>(Now - Mark.HookedAt) : 40.0f;
	const float Damage = FMath::Clamp(Mark.Damage, 0.0f, 100.0f);
	const float TimePay = FMath::Max(0.0f, 40.0f - Seconds);
	const float Clean = (100.0f - Damage) * 0.45f;
	float Pay = (40.0f + TimePay + Clean) * Mark.PayMul;
	const bool bWreck = Damage >= 80.0f;
	if (bWreck)
	{
		Pay *= 0.35f;
	}

	if (Tower)
	{
		const float Before = Tower->Ledger;
		Tower->Score += Pay;
		Tower->Ledger += 18.0f;
		++Tower->Nabs;
		if (Tower->BestNab <= 0.0f || Seconds < Tower->BestNab)
		{
			Tower->BestNab = Seconds;
		}
		Tower->HookedId = -1;
		// Scrap for the Chop Shop: every drop pays, a quick clean one pays more.
		if (TowerCar)
		{
			const int32 Scrap = Mark.bHeavy ? (bWreck ? 12 : 28) : (bWreck ? 4 : Seconds < 25.0f ? 15 : 10);
			UDCCWalletComponent::AwardScrap(TowerCar->GetPlayerState(), Scrap,
				Mark.bHeavy ? TEXT("the coach") : bWreck ? TEXT("a wreck is still parts") : TEXT("a clean drop"));
		}
		if (bWreck)
		{
			Tell(TowerCar, FString::Printf(TEXT("Dropped %s. %.0f points. You brought it in wrecked."), *Mark.Name, Pay));
			Radio(TowerCar, TEXT("RR_Disp_DropWreck"));
		}
		else
		{
			Tell(TowerCar, FString::Printf(TEXT("Dropped %s in %.0f seconds. %.0f points."), *Mark.Name, Seconds, Pay));
			Radio(TowerCar, TEXT("RR_Disp_Drop"));
		}
		if (Before < 0.0f && Tower->Ledger >= 0.0f)
		{
			Tower->bDebt = false;
			Tell(TowerCar, TEXT("Tab's clear. The yard pump is open."));
		}
		SyncKit(*Tower);
	}
	for (FTruck& Truck : Trucks)
	{
		if (Truck.HookedId == Slot)
		{
			Truck.HookedId = -1;
		}
	}
	if (ADCCRover* Car = Mark.Car.Get())
	{
		Car->bRallyHeld = false;
		Car->StopAutoPilot();
		Car->Destroy();
	}
	for (const TWeakObjectPtr<ADCCRover>& Block : Mark.Blockers)
	{
		if (ADCCRover* Box = Block.Get())
		{
			Box->Destroy();
		}
	}
	Mark.Blockers.Reset();
	Mark.Car = nullptr;
	Mark.Tower = nullptr;
	Mark.bDocked = false;
	Mark.HookedAt = -1.0;
	Mark.Damage = 0.0f;
	if (Mark.bHeavy)
	{
		Mark.bRetired = true;
		Mark.RespawnAt = Now + 100000.0;
	}
	else
	{
		Mark.RespawnAt = Now + 6.0;
	}
}

void ADCCArcadeKart::AlertMark(int32 Slot, FTruck* Who)
{
	if (!Marks.IsValidIndex(Slot))
	{
		return;
	}
	FMark& Mark = Marks[Slot];
	if (Mark.bAlerted || Mark.Kind != 3 || Mark.Tower.IsValid())
	{
		return;
	}
	Mark.bAlerted = true;
	if (ADCCRover* Car = Mark.Car.Get())
	{
		Car->bRallyHeld = true;
		Car->StopAutoPilot();
	}
	if (Who)
	{
		Tell(Who->Car.Get(), FString::Printf(TEXT("%s spooked. It's leaving the ground."), *Mark.Name));
	}
}

void ADCCArcadeKart::NoteDebt(FTruck& Truck, float Before)
{
	if (Before >= 0.0f && Truck.Ledger < 0.0f)
	{
		Truck.bDebt = true;
		Tell(Truck.Car.Get(), TEXT("The tab went red. Other crews will treat you like a skip."));
	}
}

void ADCCArcadeKart::ThrowNet(FTruck& Truck, const FVector& At)
{
	ADCCRover* Car = Truck.Car.Get();
	if (!Car)
	{
		return;
	}
	const double Now = ServerTime();
	if (Now < Truck.NetReadyAt)
	{
		Tell(Car, TEXT("The net is packing back up."));
		return;
	}
	FDCCRallyBoot Net;
	Net.Id = NextNetId++;
	Net.At = At;
	Net.At.Z = GetActorLocation().Z + 20.0f;
	Net.Until = Now + 7.0;
	Net.Owner = Truck.Name;
	Nets.Add(Net);
	Truck.NetReadyAt = Now + 9.0;
	Tell(Car, TEXT("Net's out. A slow car is easier to reel onto the bumper."));
}

void ADCCArcadeKart::BuyRole(FTruck& Truck, int32 RoleId)
{
	ADCCRover* Car = Truck.Car.Get();
	if (!Car)
	{
		return;
	}
	const FRepoRole& Def = RepoRole(RoleId);
	if (Truck.Role == RoleId)
	{
		Tell(Car, FString::Printf(TEXT("Already the %s truck."), Def.Name));
		return;
	}
	const float Before = Truck.Ledger;
	Truck.Ledger -= Def.Cost;
	Truck.Role = RoleId;
	Truck.BasePace = Def.Pace;
	Car->RallyEngine = Def.Engine;
	Car->RallyBoostExtra = Def.BoostExtra;
	Truck.Tools.Reset();
	Truck.Tools.Add(DCCRallyItems::Hook);
	if (Def.bNet)
	{
		Truck.Tools.Add(DCCRallyItems::Net);
	}
	Truck.Selected = 0;
	Car->SetRacePace(Def.Pace);
	FString Note = FString::Printf(TEXT("%s bolted on. Tab is %.0f."), Def.Name, Truck.Ledger);
	if (Def.bNet)
	{
		Note += TEXT(" [F] then [Q] throws the net.");
	}
	Tell(Car, Note);
	if (Before >= 0.0f && Truck.Ledger < 0.0f)
	{
		Truck.bDebt = true;
		Tell(Car, TEXT("That put you on the tab. Other crews are treating you like a skip, and the yard pump is closed."));
	}
	SyncKit(Truck);
}

void ADCCArcadeKart::SyncKit(FTruck& Truck)
{
	ADCCRover* Car = Truck.Car.Get();
	if (!Car)
	{
		return;
	}
	Car->RallyTools = Truck.Tools;
	if (!FMath::IsNearlyEqual(Car->RallyFuel, Truck.Fuel, 0.5f))
	{
		Car->RallyFuel = Truck.Fuel;
	}
	if (!FMath::IsNearlyEqual(Car->RallyHookRange, HookLine(Truck)))
	{
		Car->RallyHookRange = HookLine(Truck);
	}
	Car->RallyToolIndex = Truck.Tools.Num() > 0 ? FMath::Clamp(Truck.Selected, 0, Truck.Tools.Num() - 1) : 0;
	{
		const bool bHooked = Truck.HookedId >= 0 && Marks.IsValidIndex(Truck.HookedId) && Marks[Truck.HookedId].Car.IsValid();
		ADCCRover* Tow = bHooked ? Marks[Truck.HookedId].Car.Get() : nullptr;
		const bool bDocked = bHooked && Marks[Truck.HookedId].bDocked;
		const bool bYard = InYard(Car);
		if (Car->RallyTow != Tow) { Car->RallyTow = Tow; }
		if (Car->bRallyTowDocked != bDocked) { Car->bRallyTowDocked = bDocked; }
		if (Car->bRallyInYard != bYard) { Car->bRallyInYard = bYard; }
	}
	Car->RallyCargo.Reset();

	TArray<FString> Bits;
	Bits.Add(RepoRole(Truck.Role).Name);
	Bits.Add(FString::Printf(TEXT("Fuel %.0f%s"), Truck.Fuel, Truck.Cans > 0 ? TEXT("  CAN") : TEXT("")));
	if (Truck.Ledger < 0.0f)
	{
		Bits.Add(FString::Printf(TEXT("IN DEBT %.0f"), Truck.Ledger));
	}
	else
	{
		Bits.Add(FString::Printf(TEXT("Tab %.0f"), Truck.Ledger));
	}
	if (Truck.HookedId >= 0 && Marks.IsValidIndex(Truck.HookedId) && Marks[Truck.HookedId].Car.IsValid())
	{
		const FMark& Mark = Marks[Truck.HookedId];
		if (Mark.bDocked)
		{
			Bits.Add(FString::Printf(TEXT("ON THE BUMPER  %s"), *Mark.Name));
		}
		else
		{
			const int32 Pct = FMath::RoundToInt(FMath::Clamp(Mark.Car->RallyStrain, 0.0f, 1.0f) * 100.0f);
			Bits.Add(FString::Printf(TEXT("MAGNET %s %d%%"), *Mark.Name, Pct));
		}
	}
	else
	{
		Bits.Add(TEXT("[RMB] aim   [Q] hook"));
	}
	Bits.Add(FString::Printf(TEXT("Bank %.0f"), Truck.Score));
	const FString Status = FString::Join(Bits, TEXT("   "));
	if (Car->RallyStatus != Status)
	{
		Car->RallyStatus = Status;
	}
}

void ADCCArcadeKart::TickTraffic()
{
	// Every AI-driven car needs a controller (2026-11-13): the rover's floating-pawn movement only applies
	// input when a local controller possesses it, and cars spawned at runtime get none - so traffic, the
	// rival crews and running marks were all flooring a throttle nothing listened to.
	for (TActorIterator<ADCCRover> It(GetWorld()); It; ++It)
	{
		const uint8 Duty = It->RallyDuty;
		if ((Duty == 1 || Duty == 2 || Duty == 3) && !It->GetController())
		{
			It->SpawnDefaultController();
		}
	}

	// Where the first traffic car is, every 15 s: proof it is moving, in the log ([DCC Rally]).
	static double NextTrafficLog = 0.0;
	if (ServerTime() >= NextTrafficLog)
	{
		NextTrafficLog = ServerTime() + 15.0;
		for (const FTruck& Truck : Trucks)
		{
			if (const ADCCRover* Car = Truck.Car.Get(); Car && Car->RallyDuty == 2)
			{
				UE_LOG(LogTemp, Log, TEXT("[DCC Rally] traffic at %s heading for crossing %d,%d, speed %.0f"),
					*Car->GetActorLocation().ToCompactString(), Truck.To.X, Truck.To.Y, Car->GetVelocity().Size());
				break;
			}
		}
	}
	for (FTruck& Truck : Trucks)
	{
		ADCCRover* Car = Truck.Car.Get();
		if (!Car || Car->RallyDuty != 2)
		{
			continue;
		}
		const FVector Target = Crossing(Truck.To.X, Truck.To.Y);
		const bool bArrived = FVector::Dist2D(Car->GetActorLocation(), Target) < 1500.0f;
		if (Car->bAutoPilotActive && !bArrived)
		{
			continue;
		}
		if (bArrived)
		{
			// Pick the next crossing: straight on most of the time, a turn otherwise, never back.
			const FIntPoint Dir = Truck.To - Truck.From;
			TArray<FIntPoint> Options;
			const FIntPoint Straight = Truck.To + Dir;
			const FIntPoint Left = Truck.To + FIntPoint(-Dir.Y, Dir.X);
			const FIntPoint Right = Truck.To + FIntPoint(Dir.Y, -Dir.X);
			auto OnGrid = [](const FIntPoint& P) { return FMath::Abs(P.X) <= StreetRadius && FMath::Abs(P.Y) <= StreetRadius; };
			if (OnGrid(Straight)) { Options.Add(Straight); Options.Add(Straight); }
			if (OnGrid(Left)) { Options.Add(Left); }
			if (OnGrid(Right)) { Options.Add(Right); }
			if (Options.IsEmpty()) { Options.Add(Truck.From); } // a dead end: the only way back
			Truck.From = Truck.To;
			Truck.To = Options[FMath::RandRange(0, Options.Num() - 1)];
		}
		// Aim down the right-hand lane toward the next crossing.
		const FVector A = Crossing(Truck.From.X, Truck.From.Y), B = Crossing(Truck.To.X, Truck.To.Y);
		const FVector Heading = (B - A).GetSafeNormal2D();
		const FVector RightLane = FVector(-Heading.Y, Heading.X, 0.0f) * 700.0f;
		Car->AutoPilotAcceptanceRadius = 900.0f;
		Car->StartAutoPilotRoute({ B + RightLane }, false); // straight for the next crossing's lane: never back toward one behind
		Car->SetRacePace(Truck.BasePace);
	}
}

FLinearColor ADCCArcadeKart::MarkColour(const FString& Plate)
{
	static const FLinearColor Palette[] = {
		FLinearColor(1.0f, 0.55f, 0.08f), // amber
		FLinearColor(0.35f, 1.0f, 0.25f), // lime
		FLinearColor(0.2f, 0.85f, 1.0f),  // cyan
		FLinearColor(0.75f, 0.35f, 1.0f), // violet
		FLinearColor(1.0f, 0.25f, 0.6f),  // hot pink
		FLinearColor(1.0f, 0.95f, 0.3f),  // yellow
	};
	return Palette[GetTypeHash(Plate) % UE_ARRAY_COUNT(Palette)];
}

void ADCCArcadeKart::Radio(ADCCRover* Car, const TCHAR* Row)
{
	if (Car && Car->GetPlayerState())
	{
		Car->Client_RallyRadio(FName(Row));
	}
}

void ADCCArcadeKart::Tell(ADCCRover* Car, const FString& Message)
{
	if (Car)
	{
		Car->Client_RallyNote(Message);
	}
}

ADCCArcadeKart::FTruck* ADCCArcadeKart::TruckFor(ADCCRover* Car)
{
	for (FTruck& Truck : Trucks)
	{
		if (Truck.Car.Get() == Car)
		{
			return &Truck;
		}
	}
	return nullptr;
}

int32 ADCCArcadeKart::MarkIndex(const ADCCRover* Car) const
{
	if (!Car)
	{
		return -1;
	}
	for (int32 i = 0; i < Marks.Num(); ++i)
	{
		if (Marks[i].Car.Get() == Car)
		{
			return i;
		}
	}
	return -1;
}

float ADCCArcadeKart::HookLine(const FTruck& Truck) const
{
	if (!Truck.bPlayer)
	{
		return 1600.0f;
	}
	return RepoRole(Truck.Role).Line * DCCChopShop::HookLine(Truck.ChopLine);
}

float ADCCArcadeKart::HookMul(const FTruck& Truck) const
{
	if (!Truck.bPlayer)
	{
		return 1.15f;
	}
	return RepoRole(Truck.Role).Hook;
}

void ADCCArcadeKart::UseTool(ADCCRover* Car)
{
	FTruck* Truck = TruckFor(Car);
	if (!Truck || !Car)
	{
		return;
	}
	if (!bGo)
	{
		Tell(Car, TEXT("Wait for the flag. The bays are open if you want a different truck."));
		return;
	}
	const int32 Index = Truck->Tools.Num() > 0 ? FMath::Clamp(Truck->Selected, 0, Truck->Tools.Num() - 1) : 0;
	const FName Tool = Truck->Tools.IsValidIndex(Index) ? Truck->Tools[Index] : DCCRallyItems::Hook;
	if (Tool == DCCRallyItems::Net)
	{
		ThrowNet(*Truck, Car->GetActorLocation() + Car->GetActorForwardVector() * 1200.0f);
		return;
	}

	// Q without the aim camera is the tow, not the shot. The shot comes in through FireHook.
	if (Truck->HookedId >= 0 && Marks.IsValidIndex(Truck->HookedId))
	{
		FMark& Mark = Marks[Truck->HookedId];
		if (Mark.bDocked && InYard(Car))
		{
			Deliver(Truck->HookedId);
			return;
		}
		Release(Truck->HookedId, true);
		SyncKit(*Truck);
		return;
	}
	Tell(Car, TEXT("Right mouse aims the magnet. [Q] fires it."));
}

void ADCCArcadeKart::FireHook(ADCCRover* Car, FVector Dir)
{
	FTruck* Truck = TruckFor(Car);
	if (!Truck || !Car)
	{
		return;
	}
	if (!bGo)
	{
		Tell(Car, TEXT("Wait for the flag. The bays are open if you want a different truck."));
		return;
	}
	Dir = Dir.GetSafeNormal();
	if (Dir.IsNearlyZero())
	{
		Dir = Car->GetActorForwardVector();
	}
	// A downward aim is flattened. The street slab is the first thing a dipped shot would hit,
	// and the mark's body sits at the truck's own height. Upward aim is kept, for the ones that fly.
	if (Dir.Z < 0.12f)
	{
		Dir.Z = 0.0f;
		Dir = Dir.GetSafeNormal();
	}

	const int32 Index = Truck->Tools.Num() > 0 ? FMath::Clamp(Truck->Selected, 0, Truck->Tools.Num() - 1) : 0;
	const FName Tool = Truck->Tools.IsValidIndex(Index) ? Truck->Tools[Index] : DCCRallyItems::Hook;
	if (Tool == DCCRallyItems::Net)
	{
		ThrowNet(*Truck, Car->GetActorLocation() + Dir * 1200.0f);
		return;
	}
	if (Truck->Hull <= 0.0f)
	{
		Tell(Car, TEXT("Too wrecked to run the magnet. Sit under a gas station canopy to patch up."));
		return;
	}
	if (Truck->HookedId >= 0)
	{
		Tell(Car, TEXT("The magnet's already locked. Right mouse off, then [Q] drops it in the yard, or cuts it."));
		return;
	}

	const float Line = HookLine(*Truck);
	const FVector Start = Car->GetActorLocation() + FVector(0.0f, 0.0f, 20.0f);
	const FVector End = Start + Dir * Line;
	FHitResult Hit;
	FCollisionQueryParams Params(SCENE_QUERY_STAT(RepoHook), false, Car);
	const bool bHit = GetWorld()->SweepSingleByChannel(
		Hit, Start, End, FQuat::Identity, ECC_Visibility, FCollisionShape::MakeSphere(70.0f), Params);

	// A miss that passes close to a skittish car is enough to spook it.
	auto AlertAlongShot = [&]()
	{
		const FVector Span = End - Start;
		const float SpanLenSq = Span.SizeSquared();
		for (int32 i = 0; i < Marks.Num(); ++i)
		{
			ADCCRover* MarkCar = Marks[i].Car.Get();
			if (!MarkCar || Marks[i].Kind != 3 || Marks[i].bAlerted || Marks[i].Tower.IsValid() || SpanLenSq < 1.0f)
			{
				continue;
			}
			const float T = FMath::Clamp(FVector::DotProduct(MarkCar->GetActorLocation() - Start, Span) / SpanLenSq, 0.0f, 1.0f);
			const FVector Closest = Start + Span * T;
			if (FVector::Dist(Closest, MarkCar->GetActorLocation()) < 900.0f)
			{
				AlertMark(i, Truck);
			}
		}
	};

	ADCCRover* HitCar = bHit ? Cast<ADCCRover>(Hit.GetActor()) : nullptr;
	// A near miss still locks. The magnet grabs a mark within a few metres of the beam.
	if (!HitCar || HitCar->RallyDuty != 1)
	{
		const FVector Span = End - Start;
		const float SpanLenSq = Span.SizeSquared();
		ADCCRover* Near = nullptr;
		float Best = 520.0f;
		for (int32 i = 0; i < Marks.Num(); ++i)
		{
			ADCCRover* MarkCar = Marks[i].Car.Get();
			if (!MarkCar || Marks[i].Tower.IsValid() || SpanLenSq < 1.0f)
			{
				continue;
			}
			const float T = FMath::Clamp(FVector::DotProduct(MarkCar->GetActorLocation() - Start, Span) / SpanLenSq, 0.0f, 1.0f);
			const float Off = FVector::Dist(Start + Span * T, MarkCar->GetActorLocation());
			if (Off < Best)
			{
				Best = Off;
				Near = MarkCar;
			}
		}
		if (Near)
		{
			HitCar = Near;
		}
	}
	if (!HitCar || HitCar->RallyDuty != 1)
	{
		AlertAlongShot();
		if (HitCar)
		{
			const float Before = Truck->Ledger;
			Truck->Score -= 15.0f;
			Truck->Ledger -= 12.0f;
			++Truck->Fines;
			Tell(Car, TEXT("That's not your mark. Fine. The magnet scarred somebody else's car."));
			Radio(Car, TEXT("RR_Disp_Wrong"));
			NoteDebt(*Truck, Before);
			SyncKit(*Truck);
		}
		else
		{
			Tell(Car, bHit ? TEXT("The magnet hit a wall.") : TEXT("Magnet missed."));
		}
		return;
	}

	const int32 Slot = MarkIndex(HitCar);
	if (Slot < 0)
	{
		Tell(Car, TEXT("Magnet missed."));
		return;
	}
	if (Marks[Slot].Tower.IsValid())
	{
		Tell(Car, FString::Printf(TEXT("%s is already locked."), *Marks[Slot].Name));
		return;
	}

	FMark& Mark = Marks[Slot];
	Truck->HookedId = Slot;
	Mark.Tower = Car;
	Mark.HookedAt = ServerTime();
	Mark.bDocked = false;
	HitCar->bRallyHeld = true;
	HitCar->StopAutoPilot();
	if (HitCar->Movement)
	{
		HitCar->Movement->StopMovementImmediately();
	}
	Tell(Car, FString::Printf(TEXT("Locked %s. The magnet's got it."), *Mark.Name));
	Radio(Car, TEXT("RR_Disp_Hooked"));
	RollTrouble(Slot, *Truck);
	SyncKit(*Truck);
}

void ADCCArcadeKart::CycleTool(ADCCRover* Car)
{
	FTruck* Truck = TruckFor(Car);
	if (!Truck || Truck->Tools.Num() < 2)
	{
		return;
	}
	Truck->Selected = (Truck->Selected + 1) % Truck->Tools.Num();
	SyncKit(*Truck);
	const FName Tool = Truck->Tools[Truck->Selected];
	const DCCRallyItems::FItem* Item = DCCRallyItems::Find(Tool);
	Tell(Car, FString::Printf(TEXT("%s ready."), Item ? Item->Name : TEXT("Hook")));
}

void ADCCArcadeKart::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);
	DOREPLIFETIME(ADCCArcadeKart, Nets);
	DOREPLIFETIME(ADCCArcadeKart, ShiftEndsAt);
	DOREPLIFETIME(ADCCArcadeKart, Crew);
	DOREPLIFETIME(ADCCArcadeKart, Board);
	DOREPLIFETIME(ADCCArcadeKart, FlagAt);
	DOREPLIFETIME(ADCCArcadeKart, RallyWave);
}

void ADCCArcadeKart::RefreshBoard()
{
	FlagAt = GoAt;
	RallyWave = Wave;

	TArray<const FTruck*> Order;
	for (const FTruck& Truck : Trucks)
	{
		if (Truck.bPlayer)
		{
			Order.Add(&Truck);
		}
	}
	Order.Sort([](const FTruck& A, const FTruck& B)
	{
		if (A.Score != B.Score) { return A.Score > B.Score; }
		return A.Nabs > B.Nabs;
	});
	TArray<FDCCRallyCrewRow> NewCrew;
	for (const FTruck* Truck : Order)
	{
		FDCCRallyCrewRow& Row = NewCrew.AddDefaulted_GetRef();
		Row.Name = Truck->Name;
		Row.Score = FMath::RoundToFloat(Truck->Score);
		Row.Nabs = Truck->Nabs;
		Row.Ledger = FMath::RoundToFloat(Truck->Ledger);
		{
			const float Cap = DCCChopShop::TankCapacity(Truck->ChopTank);
			Row.Fuel = Cap > 1.0f ? FMath::Clamp(Truck->Fuel / Cap * 100.0f, 0.0f, 100.0f) : 0.0f;
		}
		Row.Fines = Truck->Fines;
		Row.Tickets = Truck->Tickets;
		Row.Role = Truck->Role;
		Row.BestNab = Truck->BestNab;
		Row.Hull = FMath::RoundToFloat(Truck->Hull);
		Row.bRepairing = Truck->bRepairing;
	}
	auto SameCrew = [](const FDCCRallyCrewRow& A, const FDCCRallyCrewRow& B)
	{
		return A.Name == B.Name && A.Score == B.Score && A.Nabs == B.Nabs && A.Ledger == B.Ledger && A.Fuel == B.Fuel
			&& A.Fines == B.Fines && A.Tickets == B.Tickets && A.Role == B.Role && A.BestNab == B.BestNab
			&& A.Hull == B.Hull && A.bRepairing == B.bRepairing;
	};
	bool bCrewSame = NewCrew.Num() == Crew.Num();
	for (int32 i = 0; bCrewSame && i < NewCrew.Num(); ++i) { bCrewSame = SameCrew(NewCrew[i], Crew[i]); }
	if (!bCrewSame) { Crew = MoveTemp(NewCrew); }

	TArray<FDCCRallyMarkRow> NewBoard;
	for (const FMark& Mark : Marks)
	{
		FDCCRallyMarkRow& Row = NewBoard.AddDefaulted_GetRef();
		Row.Plate = Mark.Name;
		ADCCRover* Car = Mark.Car.Get();
		if (!Car)
		{
			Row.State = 7;
			continue;
		}
		Row.Damage = FMath::RoundToFloat(Mark.Damage);
		if (const ADCCRover* Tower = Mark.Tower.Get())
		{
			Row.State = Mark.bDocked ? 6 : 5;
			Row.Strain = FMath::RoundToFloat(FMath::Clamp(Car->RallyStrain, 0.0f, 1.0f) * 20.0f) / 20.0f;
			for (const FTruck& Truck : Trucks)
			{
				if (Truck.Car.Get() == Tower) { Row.Tower = Truck.Name; break; }
			}
		}
		else if (Mark.bAlerted)
		{
			Row.State = 4;
		}
		else
		{
			Row.State = uint8(FMath::Clamp(Mark.Kind, 0, 3));
		}
	}
	auto SameMark = [](const FDCCRallyMarkRow& A, const FDCCRallyMarkRow& B)
	{
		return A.Plate == B.Plate && A.State == B.State && A.Strain == B.Strain && A.Damage == B.Damage && A.Tower == B.Tower;
	};
	bool bBoardSame = NewBoard.Num() == Board.Num();
	for (int32 i = 0; bBoardSame && i < NewBoard.Num(); ++i) { bBoardSame = SameMark(NewBoard[i], Board[i]); }
	if (!bBoardSame) { Board = MoveTemp(NewBoard); }
}

void ADCCArcadeKart::ClientTick(float DeltaSeconds)
{
	FDCCRallyAmbience::Tick(this, DeltaSeconds);
	TickWalkers(DeltaSeconds);
	const double Now = ServerTime();

	// The chase camera sits above a station roof, so the roof would hide your own truck while it fuels and repairs.
	if (UWorld* World = GetWorld())
	{
		const APlayerController* Local = World->GetFirstPlayerController();
		const APawn* Mine = Local ? Local->GetPawn() : nullptr;
		for (const TWeakObjectPtr<AActor>& Canopy : Canopies)
		{
			if (AActor* Roof = Canopy.Get())
			{
				const bool bUnder = Mine && FMath::Abs(Mine->GetActorLocation().X - Roof->GetActorLocation().X) < 1900.0f
					&& FMath::Abs(Mine->GetActorLocation().Y - Roof->GetActorLocation().Y) < 1600.0f;
				if (Roof->IsHidden() != bUnder)
				{
					Roof->SetActorHiddenInGame(bUnder);
				}
			}
		}
	}

	TSet<const ADCCRover*> Live;
	if (UWorld* World = GetWorld())
	{
		for (TActorIterator<ADCCRover> It(World); It; ++It)
		{
			const bool bMark = It->RallyDuty == 1;
			const bool bRival = It->RallyDuty == 3;
			if (!bMark && !bRival)
			{
				continue;
			}
			Live.Add(*It);
			const TWeakObjectPtr<ADCCRover> Key(*It);
			const FVector P = It->GetActorLocation();
			const float Strain = FMath::Clamp(It->RallyStrain, 0.0f, 1.0f);
			// Marks stay visible the whole shift as an edge outline in their plate colour (2026-11-14:
			// the full-body gold wash and the column over the roof were too much). Strength stays under 1
			// so the Fresnel reads as a rim. The reel still thickens it a little.
			if (bMark && It->Body)
			{
				TWeakObjectPtr<UMaterialInstanceDynamic>& Glow = MarkGlows.FindOrAdd(Key);
				if (!Glow.IsValid())
				{
					if (UMaterialInterface* Base = LoadObject<UMaterialInterface>(nullptr, TEXT("/Game/DCC/Vehicles/RepoTruck/M_MarkGlow.M_MarkGlow")))
					{
						UMaterialInstanceDynamic* Mid = UMaterialInstanceDynamic::Create(Base, It->Body);
						It->Body->SetOverlayMaterial(Mid);
						Glow = Mid;
					}
				}
				if (UMaterialInstanceDynamic* Mid = Glow.Get())
				{
					Mid->SetVectorParameterValue(TEXT("Color"), MarkColour(It->RallyPlate));
					Mid->SetScalarParameterValue(TEXT("Strength"), 0.38f + 0.22f * Strain);
				}
			}
			if (bMark)
			{
				if (FBeacon* Existing = Beacons.Find(Key))
				{
					if (AActor* Beam = Existing->Beam.Get()) { Beam->Destroy(); }
					if (AActor* Sign = Existing->Sign.Get()) { Sign->Destroy(); }
					Beacons.Remove(Key);
				}
				continue;
			}
			// Rival wreckers keep a short red column. Marks do not.
			const FLinearColor Colour(0.72f, 0.10f, 0.08f);
			FBeacon& Beacon = Beacons.FindOrAdd(Key);
			const float HalfH = 900.0f;
			if (!Beacon.Beam.IsValid())
			{
				Beacon.Beam = Shape(TEXT("Cylinder"), P + FVector(0, 0, HalfH),
					FVector(110.0f, 110.0f, HalfH * 2.0f),
					Colour, FRotator::ZeroRotator, false);
			}
			if (AActor* Beam = Beacon.Beam.Get())
			{
				Beam->SetActorLocation(P + FVector(0, 0, HalfH));
				if (UStaticMeshComponent* Mesh = Beam->FindComponentByClass<UStaticMeshComponent>())
				{
					if (UMaterialInstanceDynamic* Mid = Cast<UMaterialInstanceDynamic>(Mesh->GetMaterial(0)))
					{
						Mid->SetVectorParameterValue(TEXT("Color"), Colour);
						Mid->SetVectorParameterValue(TEXT("BaseColor"), Colour);
					}
				}
			}
			if (AActor* Sign = Beacon.Sign.Get())
			{
				Sign->Destroy();
				Beacon.Sign = nullptr;
			}
		}
	}
	for (auto It = Beacons.CreateIterator(); It; ++It)
	{
		if (Live.Contains(It.Key().Get()))
		{
			continue;
		}
		if (AActor* Beam = It.Value().Beam.Get())
		{
			Beam->Destroy();
		}
		if (AActor* Sign = It.Value().Sign.Get())
		{
			Sign->Destroy();
		}
		It.RemoveCurrent();
	}

	TSet<int32> LiveNets;
	for (const FDCCRallyBoot& Net : Nets)
	{
		if (Net.Until <= Now)
		{
			continue;
		}
		LiveNets.Add(Net.Id);
		TWeakObjectPtr<AActor>& Visual = NetVisuals.FindOrAdd(Net.Id);
		if (!Visual.IsValid())
		{
			Visual = Shape(TEXT("Cylinder"), Net.At + FVector(0, 0, 20), FVector(1400, 1400, 30),
				FLinearColor(0.85f, 0.42f, 0.12f), FRotator::ZeroRotator, false);
		}
		if (AActor* Actor = Visual.Get())
		{
			Actor->SetActorLocation(Net.At + FVector(0, 0, 24));
		}
	}
	for (auto It = NetVisuals.CreateIterator(); It; ++It)
	{
		if (!LiveNets.Contains(It.Key()))
		{
			if (AActor* Actor = It.Value().Get())
			{
				Actor->Destroy();
			}
			It.RemoveCurrent();
		}
	}
}

// ===========================================================================
// Neon Mile (endless synthwave driving)
// ===========================================================================

void ADCCArcadeNeon::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);
	DOREPLIFETIME(ADCCArcadeNeon, Seed);
}

ADCCArcadeArena::FSky ADCCArcadeNeon::GetSky() const
{
	FSky Sky;
	Sky.SunPitch = -6.0f; // a low sun on the horizon, the synthwave way
	Sky.SunIntensity = 2.5f;
	Sky.SunColour = FLinearColor(1.0f, 0.35f, 0.6f);
	Sky.SkyIntensity = 0.4f;
	Sky.FogColour = FLinearColor(0.5f, 0.1f, 0.5f);
	Sky.FogDensity = 0.012f; // thin enough to see the skyline and the sun
	return Sky;
}

void ADCCArcadeNeon::GetStartTransforms(TArray<FTransform>& Out) const
{
	for (int32 i = 0; i < 8; ++i)
	{
		Out.Add(FTransform(FRotator::ZeroRotator, GetActorLocation() + FVector(-600.0f - (i / 3) * 400.0f, ((i % 3) - 1) * LaneWidth, 140.0f)));
	}
}

void ADCCArcadeNeon::BuildLocal()
{
	// A striped retro sun at the end of the road. Horizontal bars of a disc,
	// yellow at the top to hot pink at the bottom, thinner toward the horizon.
	// ClientTick keeps it a fixed distance ahead of the leader.
	const int32 Bars = 9;
	const float Radius = 5200.0f;
	for (int32 i = 0; i < Bars; ++i)
	{
		const float T = (i + 0.5f) / Bars;                       // 0 top .. 1 bottom
		const float Y = Radius * (1.0f - 2.0f * T);              // height from the centre
		const float HalfWidth = FMath::Sqrt(FMath::Max(0.0f, Radius * Radius - Y * Y));
		const float Thickness = (2.0f * Radius / Bars) * (1.0f - 0.45f * T);
		const FLinearColor Colour = FMath::Lerp(FLinearColor(1.0f, 0.85f, 0.25f), FLinearColor(1.0f, 0.15f, 0.55f), T);
		const FVector Offset(0.0f, 0.0f, 2600.0f + Y);
		if (AActor* Bar = Block(GetActorLocation() + Offset, FVector(50.0f, HalfWidth * 2.0f, Thickness), Colour * 3.0f, 0.0f, false))
		{
			SunStripes.Add(Bar);
			SunOffsets.Add(Offset);
		}
	}
}

void ADCCArcadeNeon::BuildBoostPad(FChunk& Chunk, const FVector& At)
{
	// A glowing plate with three yellow chevrons pointing up the road.
	Block(At + FVector(0, 0, 3.0f), FVector(620.0f, 460.0f, 6.0f), FLinearColor(0.1f, 0.7f, 1.0f) * 1.5f, 0.0f, false);
	const FLinearColor Chevron(3.0f, 2.4f, 0.4f);
	const float Arm = 220.0f;
	const float Cos = FMath::Cos(FMath::DegreesToRadians(35.0f)), Sin = FMath::Sin(FMath::DegreesToRadians(35.0f));
	for (int32 k = -1; k <= 1; ++k)
	{
		const FVector Tip = At + FVector(k * 170.0f + 110.0f, 0.0f, 8.0f);
		Block(Tip + FVector(-Cos * Arm * 0.5f, Sin * Arm * 0.5f, 0.0f), FVector(Arm, 44.0f, 8.0f), Chevron, -35.0f, false);
		Block(Tip + FVector(-Cos * Arm * 0.5f, -Sin * Arm * 0.5f, 0.0f), FVector(Arm, 44.0f, 8.0f), Chevron, 35.0f, false);
	}
	const FVector Size(620.0f, 460.0f, 500.0f);
	Chunk.Pads.Add(FBox(At - FVector(Size.X, Size.Y, 0.0f) * 0.5f, At + FVector(Size.X * 0.5f, Size.Y * 0.5f, Size.Z)));
}

void ADCCArcadeNeon::BuildScenery(int32 Index, const FVector& O)
{
	// Scenery is cosmetic and never collides. Its own stream, so it never
	// changes where the traffic goes.
	FRandomStream Deco(Seed * 31 + Index * 977 + 5);
	const FLinearColor Magenta(1.0f, 0.2f, 0.9f), Cyan(0.15f, 0.9f, 1.0f);
	const FLinearColor Night(0.025f, 0.012f, 0.05f);
	const float L = ChunkLength;

	for (int32 Side = -1; Side <= 1; Side += 2)
	{
		const FLinearColor Neon = Side < 0 ? Cyan : Magenta;

		// Dark ground with a glowing grid, out to the skyline.
		Block(O + FVector(L * 0.5f, Side * (RoadHalfWidth + 3500.0f), -40.0f), FVector(L + 2.0f, 7000.0f, 20.0f), Night, 0.0f, false);
		for (int32 k = 0; k < 5; ++k)
		{
			Block(O + FVector(L * 0.5f, Side * (RoadHalfWidth + 500.0f + k * 1300.0f), -28.0f), FVector(L + 2.0f, 24.0f, 4.0f), Neon * 1.6f, 0.0f, false);
		}
		for (int32 k = 0; k < 2; ++k)
		{
			Block(O + FVector(k * L * 0.5f, Side * (RoadHalfWidth + 3500.0f), -28.0f), FVector(24.0f, 7000.0f, 4.0f), Neon * 1.6f, 0.0f, false);
		}

		// Light posts along the edge, glowing caps.
		for (int32 k = 0; k < 2; ++k)
		{
			const FVector Post = O + FVector(k * L * 0.5f + 250.0f, Side * (RoadHalfWidth + 120.0f), 150.0f);
			Block(Post, FVector(36.0f, 36.0f, 300.0f), FLinearColor(0.08f, 0.06f, 0.12f), 0.0f, false);
			Block(Post + FVector(0, 0, 170.0f), FVector(70.0f, 70.0f, 40.0f), Neon * 3.0f, 0.0f, false);
		}

		// A palm: tall trunk, a star of fronds.
		{
			const FVector Base = O + FVector(Deco.FRandRange(200.0f, L - 200.0f), Side * (RoadHalfWidth + Deco.FRandRange(700.0f, 1400.0f)), 0.0f);
			const float Height = Deco.FRandRange(600.0f, 900.0f);
			Block(Base + FVector(0, 0, Height * 0.5f), FVector(50.0f, 50.0f, Height), FLinearColor(0.12f, 0.05f, 0.15f), 0.0f, false);
			const float Spin = Deco.FRandRange(0.0f, 60.0f);
			for (int32 f = 0; f < 3; ++f)
			{
				Block(Base + FVector(0, 0, Height + 10.0f), FVector(560.0f, 80.0f, 18.0f), Neon * 0.9f, Spin + f * 60.0f, false);
			}
		}

		// The skyline: dark towers with a neon rim on top.
		for (int32 k = 0; k < 3; ++k)
		{
			const FVector Size(Deco.FRandRange(600.0f, 1400.0f), Deco.FRandRange(600.0f, 1400.0f), Deco.FRandRange(1500.0f, 5500.0f));
			const FVector Base = O + FVector(Deco.FRandRange(0.0f, L), Side * Deco.FRandRange(5200.0f, 8000.0f), 0.0f);
			Block(Base + FVector(0, 0, Size.Z * 0.5f), Size, FLinearColor(0.05f, 0.025f, 0.09f), 0.0f, false);
			Block(Base + FVector(0, 0, Size.Z + 20.0f), FVector(Size.X + 20.0f, Size.Y + 20.0f, 40.0f), (Deco.FRand() < 0.5f ? Cyan : Magenta) * 2.0f, 0.0f, false);
		}
	}

	// A neon arch over the road every 1 km, carrying the distance.
	if (Index % 5 == 0 && Index > 0)
	{
		const float H = 950.0f;
		Block(O + FVector(0, RoadHalfWidth + 220.0f, H * 0.5f), FVector(140.0f, 140.0f, H), FLinearColor(0.08f, 0.05f, 0.12f), 0.0f, false);
		Block(O + FVector(0, -RoadHalfWidth - 220.0f, H * 0.5f), FVector(140.0f, 140.0f, H), FLinearColor(0.08f, 0.05f, 0.12f), 0.0f, false);
		Block(O + FVector(0, 0, H), FVector(140.0f, RoadHalfWidth * 2.0f + 580.0f, 90.0f), Magenta * 3.0f, 0.0f, false);
		Block(O + FVector(0, 0, H - 80.0f), FVector(100.0f, RoadHalfWidth * 2.0f + 440.0f, 20.0f), Cyan * 3.0f, 0.0f, false);
		Sign(O + FVector(-80.0f, 0, H - 230.0f), 180.0f, FString::Printf(TEXT("%d M"), FMath::RoundToInt(Index * ChunkLength / 100.0f)), FColor(255, 120, 230), 120.0f);
	}
}

void ADCCArcadeNeon::BuildChunk(int32 Index)
{
	if (Chunks.Contains(Index) || Seed == 0)
	{
		return;
	}
	FChunk& Chunk = Chunks.Add(Index);
	BlockSink = &Chunk.Actors;

	const FVector O = GetActorLocation() + FVector(Index * ChunkLength, 0, 0);
	const FVector Mid = O + FVector(ChunkLength * 0.5f, 0, -15.0f);
	Block(Mid, FVector(ChunkLength + 2, RoadHalfWidth * 2, 30), FLinearColor(0.03f, 0.02f, 0.06f));
	Block(Mid + FVector(0, RoadHalfWidth - 20, 25), FVector(ChunkLength + 2, 40, 20), FLinearColor(1.0f, 0.2f, 0.9f) * 2.0f);
	Block(Mid + FVector(0, -RoadHalfWidth + 20, 25), FVector(ChunkLength + 2, 40, 20), FLinearColor(0.1f, 0.9f, 1.0f) * 2.0f);
	for (int32 Dash = 0; Dash < 4; ++Dash)
	{
		const float X = O.X + Dash * 500.0f + 120.0f;
		Block(FVector(X, O.Y - LaneWidth * 0.5f, O.Z + 2.0f), FVector(240, 16, 4), FLinearColor(0.8f, 0.8f, 1.0f), 0.0f, false);
		Block(FVector(X, O.Y + LaneWidth * 0.5f, O.Z + 2.0f), FVector(240, 16, 4), FLinearColor(0.8f, 0.8f, 1.0f), 0.0f, false);
	}
	Light(O + FVector(0, (Index % 2) ? 1200.0f : -1200.0f, 300), (Index % 2) ? FLinearColor(1.0f, 0.2f, 0.8f) : FLinearColor(0.2f, 0.8f, 1.0f), 9000.0f, 2600.0f);

	BuildScenery(Index, O);

	// Boost pads: about two in five stretches, in a random lane. Same seed,
	// same pads on every machine. Traffic is no longer part of the road: it
	// is moving cars the server spawns (UpdateTraffic).
	if (Index >= 2)
	{
		FRandomStream Pads(Seed * 104729 + Index);
		if (Pads.FRand() < 0.4f)
		{
			const int32 Lane = Pads.RandRange(-1, 1);
			BuildBoostPad(Chunk, O + FVector(Pads.FRandRange(400.0f, ChunkLength - 400.0f), Lane * LaneWidth, 0.0f));
		}
	}
	BlockSink = nullptr;
}

void ADCCArcadeNeon::ClientTick(float DeltaSeconds)
{
	// Stream the road around every car, on every machine.
	if (Seed == 0)
	{
		return;
	}
	float MinX = 0.0f, MaxX = 0.0f;
	for (TActorIterator<ADCCRover> It(GetWorld()); It; ++It)
	{
		const float X = It->GetActorLocation().X - GetActorLocation().X;
		MinX = FMath::Min(MinX, X);
		MaxX = FMath::Max(MaxX, X);
	}
	CheckBoostPads();

	// The sun stays out at the end of the road.
	for (int32 i = 0; i < SunStripes.Num(); ++i)
	{
		if (AActor* Bar = SunStripes[i].Get())
		{
			Bar->SetActorLocation(GetActorLocation() + FVector(MaxX + 26000.0f, 0.0f, 0.0f) + SunOffsets[i]);
		}
	}

	const int32 Low = FMath::Max(-1, FMath::FloorToInt(MinX / ChunkLength) - 1);
	const int32 High = FMath::FloorToInt(MaxX / ChunkLength) + 10;
	for (int32 i = Low; i <= High; ++i)
	{
		BuildChunk(i);
	}
	TArray<int32> Old;
	for (const TPair<int32, FChunk>& Pair : Chunks)
	{
		if (Pair.Key < Low - 1 || Pair.Key > High + 2)
		{
			Old.Add(Pair.Key);
		}
	}
	for (const int32 Index : Old)
	{
		DestroyLocal(Chunks[Index].Actors);
		Chunks.Remove(Index);
	}
}

void ADCCArcadeNeon::ServerBegin()
{
	Seed = FMath::RandRange(1, 1 << 20);
	StartedAt = ServerTime();
}

void ADCCArcadeNeon::SeatPlayers()
{
	TArray<APlayerController*> Players;
	GetPlayers(Players);
	for (APlayerController* PC : Players)
	{
		ADCCCharacterBase* Crawler = Cast<ADCCCharacterBase>(PC->GetPawn());
		if (!Crawler || Seated.Contains(PC))
		{
			continue;
		}
		const int32 Slot = Seated.Num();
		const FTransform At(FRotator::ZeroRotator, GetActorLocation() + FVector(200.0f - (Slot / 3) * 600.0f, ((Slot % 3) - 1) * LaneWidth, 140.0f));
		FActorSpawnParameters Params;
		Params.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AdjustIfPossibleButAlwaysSpawn;
		if (ADCCRover* Car = GetWorld()->SpawnActor<ADCCRover>(ADCCRover::StaticClass(), At, Params))
		{
			Crawler->SetActorLocation(At.GetLocation() + FVector(-700.0f, 280.0f, 40.0f));
			Seated.Add(PC);
		}
	}
}

void ADCCArcadeNeon::Crash(ADCCRover* Car, const TCHAR* Why)
{
	const double Now = ServerTime();
	if (const double* Until = CrashCooldown.Find(Car))
	{
		if (Now < *Until)
		{
			return;
		}
	}
	CrashCooldown.Add(Car, Now + 2.0);

	const float Metres = FMath::Max(0.0f, (Car->GetActorLocation().X - GetActorLocation().X) / 100.0f);
	if (APlayerState* PlayerState = Car->GetPlayerState())
	{
		DCCProfile::RecordBest(PlayerState, TEXT("Neon"), TEXT("BestDistance"), FMath::RoundToFloat(Metres));
		DCCProfile::Add(PlayerState, TEXT("Neon"), TEXT("Runs"), 1.0f);
		// A ticket for every 25 m of the run, paid at the crash.
		UDCCWalletComponent::AwardTickets(PlayerState, FMath::FloorToInt(Metres / 25.0f), FString::Printf(TEXT("%d m on Neon Mile"), FMath::RoundToInt(Metres)));
	}

	// Back to the start line for another go.
	if (UFloatingPawnMovement* Movement = Car->FindComponentByClass<UFloatingPawnMovement>())
	{
		Movement->StopMovementImmediately();
	}
	Car->TeleportTo(GetActorLocation() + FVector(200.0f, 0.0f, 140.0f), FRotator::ZeroRotator, false, true);
	UE_LOG(LogTemp, Log, TEXT("[DCC Neon] %s %s at %.0f m"), *PlayerName(Car->GetPlayerState()), Why, Metres);
}

void ADCCArcadeNeon::ServerTick(float DeltaSeconds)
{
	SeatPlayers();

	// Crashes: hit traffic, or leave the road.
	for (TActorIterator<ADCCRover> It(GetWorld()); It; ++It)
	{
		ADCCRover* Car = *It;
		if (!Car->GetPlayerState())
		{
			continue;
		}
		const FVector At = Car->GetActorLocation();
		const FVector Local = At - GetActorLocation();
		if (FMath::Abs(Local.Y) > RoadHalfWidth + 150.0f || Local.Z < -400.0f)
		{
			Crash(Car, TEXT("left the road"));
			continue;
		}
		for (const TWeakObjectPtr<ADCCNeonTrafficCar>& Weak : Traffic)
		{
			const ADCCNeonTrafficCar* Other = Weak.Get();
			// A forgiving margin: the rover is loose, a scrape is not a crash.
			if (Other && Other->GetCrashBox().ExpandBy(FVector(130, 100, 100)).IsInside(At))
			{
				Crash(Car, TEXT("hit traffic"));
				break;
			}
		}
	}

	TrafficTimer -= DeltaSeconds;
	if (TrafficTimer <= 0.0f)
	{
		TrafficTimer = 0.2f;
		UpdateTraffic();
	}

	RefreshTimer -= DeltaSeconds;
	if (RefreshTimer > 0.0f)
	{
		return;
	}
	RefreshTimer = 0.25f;

	TArray<FString> Now;
	for (TActorIterator<ADCCRover> It(GetWorld()); It; ++It)
	{
		if (APlayerState* PlayerState = It->GetPlayerState())
		{
			Now.Add(FString::Printf(TEXT("%s %s m"), *PlayerName(PlayerState),
				*FText::AsNumber(FMath::Max(0, FMath::RoundToInt((It->GetActorLocation().X - GetActorLocation().X) / 100.0f))).ToString()));
		}
	}
	const float Elapsed = static_cast<float>(ServerTime() - StartedAt);
	Title = TEXT("NEON MILE  -  how far can you get?");
	Accent = FLinearColor(1.0f, 0.3f, 0.9f);
	StatusLines.Reset();
	StatusLines.Add(FString::Join(Now, TEXT("     ")));
	const DCCArcade::FGame* Game = DCCArcade::Find(TEXT("Neon"));
	StatusLines.Add(FString::Printf(TEXT("Furthest ever: %s"), *DCCProfile::Leaderboard(GetWorld(), TEXT("Neon"), TEXT("BestDistance"), true,
		[Game](float V) { return DCCArcade::FormatRecord(*Game, V); })));
	StatusLines.Add(FString::Printf(TEXT("W to drive, A / D to steer, click to boost. Glowing chevrons boost you too. A crash sends you back to the start.   %d s left"),
		FMath::Max(0, FMath::CeilToInt(TimeLimit - Elapsed))));

	if (Elapsed >= TimeLimit)
	{
		// Count everyone's current run too, so a good run is not lost to the clock.
		for (TActorIterator<ADCCRover> It(GetWorld()); It; ++It)
		{
			if (APlayerState* PlayerState = It->GetPlayerState())
			{
				const float Metres = FMath::Max(0.0f, (It->GetActorLocation().X - GetActorLocation().X) / 100.0f);
				DCCProfile::RecordBest(PlayerState, TEXT("Neon"), TEXT("BestDistance"), FMath::RoundToFloat(Metres));
				if (!bPaid)
				{
					UDCCWalletComponent::AwardTickets(PlayerState, FMath::FloorToInt(Metres / 25.0f), FString::Printf(TEXT("%d m on Neon Mile"), FMath::RoundToInt(Metres)));
					UDCCWalletComponent::AwardScrap(PlayerState, FMath::FloorToInt(Metres / 120.0f), TEXT("Neon Mile distance"));
				}
			}
		}
		bPaid = true;
		ReturnToLobbyIn(12.0f);
	}
}

void ADCCArcadeNeon::CheckBoostPads()
{
	// On the driver's own machine (the rover moves there, so the shove is felt
	// at once) and on the server (which owns the result). Both call GrantBoost.
	const double Now = ServerTime();
	for (TActorIterator<ADCCRover> It(GetWorld()); It; ++It)
	{
		ADCCRover* Car = *It;
		if (!Car->GetPlayerState() || !(Car->IsLocallyControlled() || HasAuthority()))
		{
			continue;
		}
		if (const double* Until = PadCooldown.Find(Car))
		{
			if (Now < *Until)
			{
				continue;
			}
		}
		const FVector At = Car->GetActorLocation();
		const int32 Index = FMath::FloorToInt((At.X - GetActorLocation().X) / ChunkLength);
		const FChunk* Chunk = Chunks.Find(Index);
		if (!Chunk)
		{
			continue;
		}
		for (const FBox& Pad : Chunk->Pads)
		{
			if (Pad.IsInsideXY(At) && At.Z < Pad.Max.Z)
			{
				Car->GrantBoost();
				PadCooldown.Add(Car, Now + 1.0);
				break;
			}
		}
	}
}

bool ADCCArcadeNeon::HasTrafficIn(int32 ChunkIndex) const
{
	const float Low = GetActorLocation().X + ChunkIndex * ChunkLength - 500.0f;
	const float High = GetActorLocation().X + (ChunkIndex + 1) * ChunkLength + 500.0f;
	for (const TWeakObjectPtr<ADCCNeonTrafficCar>& Weak : Traffic)
	{
		if (const ADCCNeonTrafficCar* Car = Weak.Get())
		{
			const float X = Car->GetActorLocation().X;
			if (X >= Low && X <= High)
			{
				return true;
			}
		}
	}
	return false;
}

void ADCCArcadeNeon::SeedTraffic(int32 ChunkIndex)
{
	// Spaced for loose handling (2026-11-13): at most one car per 1000-unit
	// slot, so about 750+ units between cars, and never all three lanes shut.
	// Busier the further up the road; from 1.2 km a slot can hold two cars side
	// by side with one lane open. Every car drives at the same speed, so the
	// gaps it was spawned with are the gaps it keeps.
	LastSeeded.Add(ChunkIndex, ServerTime());
	const FVector O = GetActorLocation() + FVector(ChunkIndex * ChunkLength, 0, 0);
	const float Fill = FMath::Clamp(0.3f + ChunkIndex * 0.02f, 0.3f, 0.75f);
	const double Now = ADCCNeonTrafficCar::ClockOf(GetWorld());

	auto Spawn = [this, Now](const FVector& At)
	{
		const FTransform Where(FRotator::ZeroRotator, At);
		ADCCNeonTrafficCar* Car = GetWorld()->SpawnActorDeferred<ADCCNeonTrafficCar>(ADCCNeonTrafficCar::StaticClass(), Where,
			nullptr, nullptr, ESpawnActorCollisionHandlingMethod::AlwaysSpawn);
		if (Car)
		{
			Car->Init(At, Now, TrafficSpeed, FMath::FRand() < 0.5f);
			Car->FinishSpawning(Where);
			Traffic.Add(Car);
		}
	};

	for (int32 SlotIndex = 0; SlotIndex < 2; ++SlotIndex)
	{
		if (FMath::FRand() > Fill)
		{
			continue;
		}
		const float X = SlotIndex * 1000.0f + 500.0f + FMath::FRandRange(-120.0f, 120.0f);
		const int32 Lane = FMath::RandRange(-1, 1);
		Spawn(O + FVector(X, Lane * LaneWidth, 110.0f));
		if (ChunkIndex >= 6 && FMath::FRand() < 0.3f)
		{
			const int32 Other = Lane == 0 ? (FMath::FRand() < 0.5f ? -1 : 1) : 0;
			Spawn(O + FVector(X, Other * LaneWidth, 110.0f));
		}
	}
}

void ADCCArcadeNeon::UpdateTraffic()
{
	float MinX = TNumericLimits<float>::Max(), MaxX = -TNumericLimits<float>::Max();
	TArray<int32> Ahead;
	for (TActorIterator<ADCCRover> It(GetWorld()); It; ++It)
	{
		if (!It->GetPlayerState())
		{
			continue;
		}
		const float X = It->GetActorLocation().X - GetActorLocation().X;
		MinX = FMath::Min(MinX, X);
		MaxX = FMath::Max(MaxX, X);
		Ahead.AddUnique(FMath::FloorToInt(X / ChunkLength));
	}
	if (Ahead.IsEmpty())
	{
		return;
	}

	// Gone: fallen well behind everyone, or wandered far ahead of everyone.
	for (int32 i = Traffic.Num() - 1; i >= 0; --i)
	{
		ADCCNeonTrafficCar* Car = Traffic[i].Get();
		const float X = Car ? Car->GetActorLocation().X - GetActorLocation().X : 0.0f;
		if (!Car || X < MinX - 3.0f * ChunkLength || X > MaxX + 16.0f * ChunkLength)
		{
			if (Car)
			{
				Car->Destroy();
			}
			Traffic.RemoveAtSwap(i);
		}
	}

	// New: fill empty stretches out ahead of each car, far enough out (140 m+)
	// that they appear in the haze, not in your face. Never in the first 60 m.
	const double Now = ServerTime();
	for (const int32 Chunk : Ahead)
	{
		for (int32 Index = FMath::Max(3, Chunk + 7); Index <= Chunk + 11; ++Index)
		{
			const double* Last = LastSeeded.Find(Index);
			const bool bRecentlySeeded = Last && (Now - *Last) * TrafficSpeed < ChunkLength + 1000.0f;
			if (!bRecentlySeeded && !HasTrafficIn(Index))
			{
				SeedTraffic(Index);
			}
		}
	}
}

void ADCCArcadeClimb::OnTagged(ADCCCharacterBase* Painter, const FVector& At)
{
	const APlayerState* State = Painter ? Painter->GetPlayerState() : nullptr;
	// The top tenth of the climb: 144 m and up, where only the people who earned it stand.
	if (!State || At.Z - GetActorLocation().Z < TopZ * 0.9f || SummitTaggers.Contains(State->GetPlayerName()))
	{
		return;
	}
	SummitTaggers.Add(State->GetPlayerName());
	DCCBounties::Flag(State, TEXT("SummitTag")); // the bounty, even for someone who already has the achievement
	// Once ever, not once a round (Zoe, 2026-11-16): a crawler who already has Proof of Summit gets nothing new, and the
	// mountain does not announce it again.
	if (Painter->Achievements && Painter->Achievements->IsUnlocked(TEXT("ProofOfSummit")))
	{
		return;
	}
	UDCCAchievementComponent::RecordFor(Painter, EDCCAchievementStat::TaggedTheSummit);
	ADCCGameState::Log(this, EDCCLogCategory::Announcement,
		FText::FromString(FString::Printf(TEXT("%s put their tag on the top of Fourteener. Proof."), *State->GetPlayerName())));
}
