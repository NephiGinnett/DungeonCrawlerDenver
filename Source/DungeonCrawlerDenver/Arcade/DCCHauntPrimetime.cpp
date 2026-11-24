// PRIMETIME: Dead Air as Hiss Cranson's show (2026-11-23, Docs/dead-air-primetime-plan.md).
//
// Zoe: "have it be Hiss Cranson hosting his talk show and sending the players through various difficult encounters and
// then interviewing them ... interview the survivors at the end of each round and either play an antagonizing or
// friendly role either buffing or adding more traps for the players ... players could still be descending the mountain
// ... the TV studio be where players visit between rounds. And players who are out could just sit in the audience along
// with random npcs."
//
// The loop: the opening in the studio, then ShowLevels levels down the Lucky Tuesday. Each level is one kind of round
// laid over the mine (get to the stope, throw three switches in the dark, hold a ring while things come), and the last
// is the finale in the deepest stope with the shift boss. After a level, survivors go up to the studio for the couch:
// Hiss asks each one a question and they answer Charm, Banter or Defy. His mood toward each guest, and the team's
// average, become Blessings and Twists on the next level. Whoever is out watches from the audience among the extras.

#include "Arcade/DCCArcadeHaunt.h"

#include "Animation/AnimationAsset.h"
#include "Arcade/DCCAudience.h"
#include "Arcade/DCCWalletComponent.h"
#include "Characters/DCCCharacterBase.h"
#include "Characters/DCCEnemyCharacter_DeadAir.h"
#include "Combat/DCCGameState.h"
#include "Components/SkeletalMeshComponent.h"
#include "Dialogue/DCCVoiceLines.h"
#include "Dom/JsonObject.h"
#include "Engine/SkeletalMesh.h"
#include "Animation/SkeletalMeshActor.h"
#include "Engine/World.h"
#include "EngineUtils.h"
#include "GameFramework/PlayerController.h"
#include "GameFramework/PlayerState.h"
#include "HAL/IConsoleManager.h"
#include "Misc/FileHelper.h"
#include "Misc/Paths.h"
#include "Rooms/DCCDungeonGrid.h"
#include "Serialization/JsonReader.h"
#include "Serialization/JsonSerializer.h"
#include "Session/DCCProfile.h"
#include "Stats/DCCStatsComponent.h"
#include "UI/DCCHUD.h"

static TAutoConsoleVariable<int32> CVarDeadAirClassic(TEXT("dcc.DeadAirClassic"), 0,
	TEXT("1 plays the old found-footage Dead Air (body cam, tape, cage) instead of PRIMETIME."));

namespace
{
	struct FShowLine
	{
		FString Row, Tag, Text;
	};

	const TArray<FShowLine>& ShowLines()
	{
		static TArray<FShowLine> Lines;
		if (!Lines.IsEmpty())
		{
			return Lines;
		}
		const FString Candidates[] = {
			FPaths::Combine(FPaths::ProjectDir(), TEXT("SourceArt/Voice/NPC/talkshow_lines.json")),
			FPaths::Combine(FPaths::ProjectDir(), TEXT("Voice/NPC/talkshow_lines.json")),
		};
		for (const FString& Path : Candidates)
		{
			FString Raw;
			TSharedPtr<FJsonObject> Root;
			if (FFileHelper::LoadFileToString(Raw, *Path) && FJsonSerializer::Deserialize(TJsonReaderFactory<>::Create(Raw), Root) && Root.IsValid())
			{
				for (const TSharedPtr<FJsonValue>& Value : Root->GetArrayField(TEXT("lines")))
				{
					const TSharedPtr<FJsonObject> O = Value->AsObject();
					if (O.IsValid() && O->GetStringField(TEXT("tag")).StartsWith(TEXT("P_")))
					{
						Lines.Add({ O->GetStringField(TEXT("row")), O->GetStringField(TEXT("tag")), O->GetStringField(TEXT("text")) });
					}
				}
				break;
			}
		}
		return Lines;
	}

	/** Hiss's lines waiting to be said, in order, one at a time (server). */
	struct FQueued
	{
		FString Row, Text;
	};
	TMap<TWeakObjectPtr<const ADCCArcadeHaunt>, TArray<FQueued>> GQueue;
	TMap<TWeakObjectPtr<const ADCCArcadeHaunt>, double> GNextLineAt;

	float LineSeconds(const FString& Text)
	{
		TArray<FString> Words;
		Text.ParseIntoArrayWS(Words);
		return FMath::Max(2.8f, Words.Num() / 2.6f) + 0.6f;
	}

	const TCHAR* NpcBodies[] = {
		TEXT("casual_300"), TEXT("casual_303"), TEXT("casual_306"), TEXT("casual_309"), TEXT("office_302"), TEXT("office_305"),
		TEXT("office_308"), TEXT("worker_301"), TEXT("worker_304"), TEXT("worker_307"), TEXT("worker_310"),
	};

	FVector GuestMark(const FVector& O, int32 Slot) { return O + FVector(-760.0f, -60.0f + Slot * 130.0f, 170.0f); }
	FVector HotSeat(const FVector& O) { return O + FVector(-880.0f, -250.0f, 170.0f); }
	FVector AudienceSeat(const FVector& O, int32 Slot) { return O + FVector(330.0f, -260.0f + Slot * 130.0f, 120.0f); }
}

const TCHAR* ADCCArcadeHaunt::RoundName(uint8 Kind)
{
	switch (Kind)
	{
	case 0: return TEXT("THE GAUNTLET");
	case 1: return TEXT("LIGHTS OUT");
	case 2: return TEXT("HOLD OUT");
	default: return TEXT("THE FINALE");
	}
}

const TCHAR* ADCCArcadeHaunt::BlessingName(uint8 Id)
{
	switch (Id)
	{
	case 1: return TEXT("OVERTIME: +25 seconds");
	case 2: return TEXT("LIGHTING DEPARTMENT: your lamps stay on");
	case 3: return TEXT("EXTRA TAKES: one of the audience comes back");
	default: return TEXT("");
	}
}

const TCHAR* ADCCArcadeHaunt::TwistName(uint8 Id)
{
	switch (Id)
	{
	case 1: return TEXT("SPECIAL GUEST: something new is waiting at the stope");
	case 2: return TEXT("TIGHT SCHEDULE: 20 seconds off the clock");
	case 3: return TEXT("BUDGET CUTS: your lamps start off");
	default: return TEXT("");
	}
}

FDCCShowGuest& ADCCArcadeHaunt::GuestFor(const FString& Player)
{
	if (FDCCShowGuest* Found = Guests.FindByPredicate([&Player](const FDCCShowGuest& G) { return G.Player == Player; }))
	{
		return *Found;
	}
	FDCCShowGuest& Guest = Guests.AddDefaulted_GetRef();
	Guest.Player = Player;
	// Somebody joining mid-episode starts in the audience until the next Extra Takes, so nobody drops in mid-level.
	Guest.bOut = ShowPhase != 0;
	return Guest;
}

bool ADCCArcadeHaunt::Hiss(const TCHAR* Tag)
{
	TArray<const FShowLine*> Pool;
	for (const FShowLine& Line : ShowLines())
	{
		if (Line.Tag == Tag && !LinesUsed.Contains(Line.Row))
		{
			Pool.Add(&Line);
		}
	}
	if (Pool.IsEmpty())
	{
		// All said once this episode: start that tag over.
		for (const FShowLine& Line : ShowLines())
		{
			if (Line.Tag == Tag)
			{
				LinesUsed.Remove(Line.Row);
				Pool.Add(&Line);
			}
		}
	}
	if (Pool.IsEmpty())
	{
		UE_LOG(LogTemp, Warning, TEXT("[DCC Primetime] no line tagged %s"), Tag);
		return false;
	}
	const FShowLine* Line = Pool[FMath::RandRange(0, Pool.Num() - 1)];
	LinesUsed.Add(Line->Row);
	GQueue.FindOrAdd(this).Add({ Line->Row, Line->Text });
	if (FCString::Strncmp(Tag, TEXT("P_Q_"), 4) == 0)
	{
		CouchQuestion = Line->Text;
	}
	return true;
}

void ADCCArcadeHaunt::Multicast_HissSays_Implementation(const FString& Row, const FString& Text)
{
	UWorld* World = GetWorld();
	APlayerController* PC = World ? World->GetFirstPlayerController() : nullptr;
	if (!PC || !PC->IsLocalController())
	{
		return;
	}
	// His voice, right at the listener: he is on every speaker in the mine and in the studio.
	const FString Wav = DCCVoice::FindLine(TEXT("Hiss"), FName(*Row));
	if (!Wav.IsEmpty() && PC->PlayerCameraManager)
	{
		DCCVoice::PlayAt(this, Wav, PC->PlayerCameraManager->GetCameraLocation(), 1.4f);
	}
	if (ADCCHUD* Hud = Cast<ADCCHUD>(PC->GetHUD()))
	{
		Hud->ShowBroadcast(TEXT("Hiss Cranson"), Text, LineSeconds(Text) + 0.8f);
	}
}

void ADCCArcadeHaunt::BuildStudio()
{
	if (bStudioBuilt)
	{
		return;
	}
	bStudioBuilt = true;
	const FVector O = StudioOrigin();
	const FLinearColor Night(0.02f, 0.012f, 0.04f);
	const FLinearColor Plum(0.16f, 0.05f, 0.22f);
	const FLinearColor Gold(1.0f, 0.75f, 0.25f);

	// The room: a closed sound stage, so nothing of the mountain shows.
	Block(O + FVector(0, 0, -20), FVector(2800, 2800, 40), Night);
	Block(O + FVector(-1400, 0, 450), FVector(40, 2800, 940), FLinearColor(0.05f, 0.02f, 0.08f));
	Block(O + FVector(1400, 0, 450), FVector(40, 2800, 940), Night);
	Block(O + FVector(0, -1400, 450), FVector(2800, 40, 940), Night);
	Block(O + FVector(0, 1400, 450), FVector(2800, 40, 940), Night);
	Block(O + FVector(0, 0, 930), FVector(2800, 2800, 40), FLinearColor(0.01f, 0.01f, 0.015f), 0.0f, false);

	// The stage at the -X end: a raised plum deck with a gold lip, the desk, the couch.
	Block(O + FVector(-900, 0, 30), FVector(1000, 2400, 60), Plum);
	Block(O + FVector(-398, 0, 32), FVector(8, 2400, 62), Gold, 0.0f, false);
	Prop(TEXT("/Game/Dam_city/Meshes/Desk/SM_desk.SM_desk"), O + FVector(-1000, -420, 60), 90.0f, FVector(240, 120, 0), true);
	Prop(TEXT("/Game/Beach_house/Meshes/Furniture/Couch/SM_couch.SM_couch"), O + FVector(-1050, 280, 60), 0.0f, FVector(320, 120, 0), true);
	// The hot seat: a gold disc where the guest stands for their question.
	Shape(TEXT("Cylinder"), HotSeat(O) - FVector(0, 0, 108), FVector(160, 160, 4), Gold, FRotator::ZeroRotator, false);

	// The back wall: the show's name, where it is coming from, and the ON AIR light. All facing the room (+X).
	Sign(O + FVector(-1375, 0, 640), 0.0f, TEXT("PRIMETIME"), FColor(255, 200, 80), 110.0f);
	Sign(O + FVector(-1375, 0, 520), 0.0f, TEXT("LIVE FROM THE LUCKY TUESDAY"), FColor(235, 200, 255), 34.0f);
	Sign(O + FVector(-1375, 620, 760), 0.0f, TEXT("ON AIR"), FColor(255, 60, 70), 40.0f);
	Block(O + FVector(-1378, 0, 300), FVector(6, 2400, 16), Gold, 0.0f, false);

	// Hiss behind his desk, in his own idle.
	FActorSpawnParameters Params;
	Params.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
	Params.ObjectFlags |= RF_Transient;
	auto Person = [this, &Params](const TCHAR* Body, const FVector& At, float Yaw) -> ASkeletalMeshActor*
	{
		USkeletalMesh* Mesh = LoadObject<USkeletalMesh>(nullptr, *FString::Printf(TEXT("/Game/DCC/Characters/NPCs/NPC_%s/SK_NPC_%s.SK_NPC_%s"), Body, Body, Body));
		UAnimationAsset* Idle = LoadObject<UAnimationAsset>(nullptr, TEXT("/Game/DCC/Characters/NPCs/Anims/NPC_MM_Idle.NPC_MM_Idle"));
		if (!Mesh)
		{
			return nullptr;
		}
		ASkeletalMeshActor* Actor = GetWorld()->SpawnActor<ASkeletalMeshActor>(At, FRotator(0.0f, Yaw, 0.0f), Params);
		if (!Actor)
		{
			return nullptr;
		}
		USkeletalMeshComponent* Skin = Actor->GetSkeletalMeshComponent();
		Skin->SetSkeletalMeshAsset(Mesh);
		Skin->SetCollisionEnabled(ECollisionEnabled::NoCollision);
		if (Idle)
		{
			Skin->PlayAnimation(Idle, true);
			Skin->SetPosition(FMath::FRandRange(0.0f, 3.0f));
		}
		StudioActors.Add(Actor);
		return Actor;
	};
	Person(TEXT("office_311"), O + FVector(-1140, -420, 60), 0.0f);

	// The audience: three risers facing the stage, extras in most seats, the front middle kept for the crew who are out.
	for (int32 Row = 0; Row < 3; ++Row)
	{
		const float X = 330.0f + Row * 260.0f;
		const float Z = Row * 70.0f;
		Block(O + FVector(X + 60, 0, Z * 0.5f + 5), FVector(260, 2200, Z + 10), FLinearColor(0.06f, 0.03f, 0.08f));
		for (int32 Seat = 0; Seat < 12; ++Seat)
		{
			const float Y = -715.0f + Seat * 130.0f;
			Block(O + FVector(X + 70, Y, Z + 30), FVector(60, 90, 50), FLinearColor(0.35f, 0.05f, 0.1f), 0.0f, false);
			const bool bCrewSeat = Row == 0 && Seat >= 4 && Seat <= 8;
			if (!bCrewSeat && FMath::FRand() < 0.62f)
			{
				Person(NpcBodies[FMath::RandRange(0, UE_ARRAY_COUNT(NpcBodies) - 1)], O + FVector(X + 40, Y, Z + 10), 180.0f);
			}
		}
	}

	// Light: a warm key over the stage, the show's pink and blue from the sides, a dim wash on the audience.
	Light(O + FVector(-600, 0, 800), FLinearColor(1.0f, 0.86f, 0.65f), 16000.0f, 2600.0f);
	Light(O + FVector(-900, -900, 400), FLinearColor(1.0f, 0.3f, 0.65f), 6000.0f, 1400.0f);
	Light(O + FVector(-900, 900, 400), FLinearColor(0.35f, 0.6f, 1.0f), 6000.0f, 1400.0f);
	Light(O + FVector(600, 0, 700), FLinearColor(0.6f, 0.5f, 0.9f), 5000.0f, 2200.0f);
}

void ADCCArcadeHaunt::SendToStudio(APawn* Pawn, bool bAudience, int32 Slot)
{
	if (!Pawn)
	{
		return;
	}
	const FVector O = StudioOrigin();
	if (ADCCCharacterBase* Crawler = Cast<ADCCCharacterBase>(Pawn))
	{
		if (Crawler->IsDowned())
		{
			Crawler->ReviveQuietly(0.5f);
		}
		// A breather between levels: half your health back.
		if (Crawler->Stats && !bAudience)
		{
			Crawler->Stats->Heal(Crawler->Stats->MaxHitPoints / 2);
		}
	}
	const FVector Dest = bAudience ? AudienceSeat(O, Slot % 5) : GuestMark(O, Slot % 5);
	const float Yaw = 180.0f; // facing the stage, so the camera looks at Hiss and the set
	if (!Pawn->TeleportTo(Dest, FRotator(0.0f, Yaw, 0.0f)))
	{
		Pawn->SetActorLocation(Dest, false, nullptr, ETeleportType::TeleportPhysics);
	}
	if (ADCCCharacterBase* Moved = Cast<ADCCCharacterBase>(Pawn))
	{
		Moved->ForgetFall();
	}
	if (APlayerController* PC = Cast<APlayerController>(Pawn->GetController()))
	{
		PC->SetControlRotation(FRotator(-10.0f, Yaw, 0.0f));
		PC->SetViewTargetWithBlend(Pawn, 0.3f);
	}
}

void ADCCArcadeHaunt::SendToMine(APawn* Pawn, int32 Slot)
{
	if (!Pawn)
	{
		return;
	}
	const float A = Slot * (2.0f * PI / 5.0f);
	const FVector Dest = LevelStart + FVector(FMath::Cos(A) * 170.0f, FMath::Sin(A) * 170.0f, 120.0f);
	FRotator Face = FRotator::ZeroRotator;
	if (Goals.Num() > 0)
	{
		Face = (Goals[0] - LevelStart).GetSafeNormal2D().Rotation();
	}
	if (!Pawn->TeleportTo(Dest, Face))
	{
		Pawn->SetActorLocation(Dest, false, nullptr, ETeleportType::TeleportPhysics);
	}
	if (ADCCCharacterBase* Moved = Cast<ADCCCharacterBase>(Pawn))
	{
		Moved->ForgetFall();
	}
	if (APlayerController* PC = Cast<APlayerController>(Pawn->GetController()))
	{
		PC->SetControlRotation(FRotator(-8.0f, Face.Yaw, 0.0f));
		PC->SetViewTargetWithBlend(Pawn, 0.3f);
	}
}

void ADCCArcadeHaunt::PlanLevel(int32 Level)
{
	ShowLevel = Level;
	LevelStart = Goals.Num() > 0 ? Goals.Last()
		: VanBox.IsValid ? FVector(VanBox.GetCenter().X, VanBox.GetCenter().Y, GetActorLocation().Z) : GetActorLocation();
	if (RoomsByDepth.IsEmpty() && Grid)
	{
		const TArray<UDCCDungeonGrid::FRoom>& Rooms = Grid->GetRooms();
		for (int32 r = 1; r < Rooms.Num(); ++r)
		{
			RoomsByDepth.Add(r);
		}
		RoomsByDepth.Sort([this, &Rooms](int32 A, int32 B)
		{
			return DepthAt(Grid->CellCenter(Rooms[A].Center())) < DepthAt(Grid->CellCenter(Rooms[B].Center()));
		});
	}
	// This level's kind: the finale last, otherwise never the same twice running.
	const uint8 Previous = RoundKind;
	if (Level >= ShowLevels)
	{
		RoundKind = 3;
	}
	else
	{
		do { RoundKind = uint8(FMath::RandRange(0, 2)); } while (Level > 1 && RoundKind == Previous);
	}
	Goals.Reset();
	GoalsDone.Reset();
	if (!Grid || RoomsByDepth.IsEmpty())
	{
		return;
	}
	const TArray<UDCCDungeonGrid::FRoom>& Rooms = Grid->GetRooms();
	auto At = [this, &Rooms](int32 Room) { return Grid->CellCenter(Rooms[Room].Center()); };
	// The stope for this level: a step deeper each time, the deepest for the finale.
	const int32 N = RoomsByDepth.Num();
	const int32 PrevIndex = RoomsByDepth.IndexOfByKey(LastGoalRoom);
	int32 Index = FMath::Clamp(FMath::RoundToInt(float(Level) / ShowLevels * (N - 1)), 0, N - 1);
	Index = FMath::Max(Index, FMath::Min(N - 1, PrevIndex + 1));
	const int32 GoalRoom = RoomsByDepth[Index];
	if (RoundKind == 1)
	{
		// Three switches: in the stopes between the last level's and this one, the last one in this level's stope.
		TArray<int32> Between;
		for (int32 i = FMath::Max(0, PrevIndex + 1); i < Index; ++i)
		{
			Between.Add(RoomsByDepth[i]);
		}
		while (Between.Num() > 2)
		{
			Between.RemoveAt(FMath::RandRange(0, Between.Num() - 1));
		}
		for (int32 Room : Between)
		{
			Goals.Add(At(Room));
		}
		// Not enough stopes on the way: the rest go round the edge of the goal stope.
		const FVector Centre = At(GoalRoom);
		const float Spread = Rooms[GoalRoom].W * TileSize * 0.3f;
		while (Goals.Num() < 2)
		{
			Goals.Add(Centre + FVector(Goals.Num() == 0 ? Spread : -Spread, Goals.Num() == 0 ? Spread : -Spread, 0.0f));
		}
		Goals.Add(Centre);
	}
	else
	{
		Goals.Add(At(GoalRoom));
	}
	GoalsDone.Init(false, Goals.Num());
	LastGoalRoom = GoalRoom;
}

void ADCCArcadeHaunt::StartLevel()
{
	ShowPhase = 1;
	bLevelCleared = false;
	HoldProgress = 0.0f;
	SeenDown.Reset();
	PetHealth.Reset();
	HoldWaveAt = 0.0;
	Interviewee.Reset();
	CouchQuestion.Reset();
	const double Now = ServerTime();
	float Seconds = LevelSeconds + (TeamBlessing == 1 ? 25.0f : 0.0f) - (TeamTwist == 2 ? 20.0f : 0.0f);
	if (RoundKind == 3)
	{
		Seconds += 30.0f;
	}
	PhaseEndsAt = Now + Seconds;

	switch (RoundKind)
	{
	case 0: Hiss(TEXT("P_LEVEL_GAUNTLET")); break;
	case 1: Hiss(TEXT("P_LEVEL_LIGHTS")); break;
	case 2: Hiss(TEXT("P_LEVEL_HOLD")); break;
	default: Hiss(TEXT("P_LEVEL_FINALE")); break;
	}

	// Down they go: everyone still in to the start of this level, the audience stays put and watches.
	TArray<APlayerController*> Players;
	GetPlayers(Players);
	APawn* Watchable = nullptr;
	int32 Slot = 0;
	for (APlayerController* PC : Players)
	{
		if (!PC || !PC->PlayerState || !PC->GetPawn())
		{
			continue;
		}
		FDCCShowGuest& Guest = GuestFor(PC->PlayerState->GetPlayerName());
		if (Guest.bOut)
		{
			continue;
		}
		SendToMine(PC->GetPawn(), Slot++);
		if (!Watchable) { Watchable = PC->GetPawn(); }
		const FString Name = Guest.Player;
		if (TeamTwist == 3)
		{
			SetLamp(Name, false);
		}
		// Marked: a pair of slag dogs let loose a stope away.
		if (Guest.Personal == 2 && Grid && RoomsByDepth.Num() > 1)
		{
			const TArray<UDCCDungeonGrid::FRoom>& Rooms = Grid->GetRooms();
			const int32 Room = RoomsByDepth[FMath::Clamp(RoomsByDepth.IndexOfByKey(LastGoalRoom) - 1, 0, RoomsByDepth.Num() - 1)];
			for (int32 d = 0; d < 2; ++d)
			{
				const FVector Where = Grid->CellCenter(Rooms[Room].Center()) + FVector(0, d * 160.0f, 140.0f);
				if (ADCCEnemyCharacter* Dog = GetWorld()->SpawnActorDeferred<ADCCEnemyCharacter>(ADCCEnemyCharacter_SlagDog::StaticClass(),
					FTransform(Where), nullptr, nullptr, ESpawnActorCollisionHandlingMethod::AdjustIfPossibleButAlwaysSpawn))
				{
					Dog->SetChaseGrid(Grid);
					Dog->LeashRadius = 8000.0f;
					Dog->FinishSpawning(FTransform(Where));
					Monsters.Add(Dog);
				}
			}
		}
	}
	// Special Guest: something extra at the stope.
	if (TeamTwist == 1 && Goals.Num() > 0 && Grid)
	{
		const FVector Where = Goals.Last() + FVector(0, 0, 140);
		if (ADCCEnemyCharacter* Guest = GetWorld()->SpawnActorDeferred<ADCCEnemyCharacter>(ADCCEnemyCharacter_LampEater::StaticClass(),
			FTransform(Where), nullptr, nullptr, ESpawnActorCollisionHandlingMethod::AdjustIfPossibleButAlwaysSpawn))
		{
			Guest->SetChaseGrid(Grid);
			Guest->LeashRadius = 8000.0f;
			Guest->FinishSpawning(FTransform(Where));
			Monsters.Add(Guest);
		}
	}
	// The audience watches someone who is still down there.
	for (APlayerController* PC : Players)
	{
		const FDCCShowGuest* Guest = PC && PC->PlayerState ? FindGuest(PC->PlayerState->GetPlayerName()) : nullptr;
		if (Guest && Guest->bOut && Watchable)
		{
			PC->SetViewTargetWithBlend(Watchable, 0.6f);
		}
	}
	UE_LOG(LogTemp, Log, TEXT("[DCC Primetime] level %d: %s, %d goal(s), %.0f s, blessing %d twist %d"), ShowLevel, RoundName(RoundKind), Goals.Num(),
		Seconds, TeamBlessing, TeamTwist);
}

void ADCCArcadeHaunt::EndLevel(bool bCleared)
{
	bLevelCleared = bCleared;
	Hiss(bCleared ? TEXT("P_CLEAR") : TEXT("P_FAIL"));
	TArray<APlayerController*> Players;
	GetPlayers(Players);
	int32 Survivors = 0, GuestSlot = 0, SeatSlot = 0;
	for (APlayerController* PC : Players)
	{
		if (!PC || !PC->PlayerState)
		{
			continue;
		}
		FDCCShowGuest& Guest = GuestFor(PC->PlayerState->GetPlayerName());
		ADCCCharacterBase* Crawler = Cast<ADCCCharacterBase>(PC->GetPawn());
		const bool bStanding = Crawler && Crawler->IsAlive() && !Crawler->IsDowned();
		if (!Guest.bOut && !bStanding)
		{
			Guest.bOut = true;
			Guest.OutOnLevel = ShowLevel;
		}
		if (Guest.bOut)
		{
			SendToStudio(PC->GetPawn(), true, SeatSlot++);
			continue;
		}
		++Survivors;
		Guest.Ratings += bCleared ? 150 : 100;
		Guest.Mood = FMath::Clamp(Guest.Mood + (bCleared ? 1 : -1), -3, 3);
		Guest.Personal = 0;
		SendToStudio(PC->GetPawn(), false, GuestSlot++);
		if (bCleared)
		{
			DCCAudience::Moment(this, Crawler, DCCAudience::EMoment::RoomClear);
		}
	}
	// Lamps back on for the studio.
	for (const FDCCShowGuest& Guest : Guests)
	{
		SetLamp(Guest.Player, true);
	}
	TeamBlessing = 0;
	TeamTwist = 0;
	if (Survivors == 0)
	{
		Hiss(TEXT("P_WIPE"));
		EndEpisode();
		return;
	}
	StartCouch();
}

void ADCCArcadeHaunt::StartCouch()
{
	ShowPhase = 2;
	Interviewee.Reset();
	CouchQuestion.Reset();
	CouchQueue.Reset();
	// First, a word for whoever did not make it this level.
	int32 SendOffs = 0;
	for (const FDCCShowGuest& Guest : Guests)
	{
		if (Guest.bOut && Guest.OutOnLevel == ShowLevel && SendOffs < 2)
		{
			Hiss(TEXT("P_SENDOFF"));
			++SendOffs;
		}
	}
	Hiss(TEXT("P_COUCH"));
	for (const FDCCShowGuest& Guest : Guests)
	{
		if (!Guest.bOut)
		{
			CouchQueue.Add(Guest.Player);
		}
	}
	bCouchAnswered = true; // nobody yet: NextCouchGuest runs when the welcome is done
	CouchReplyAt = 0.0;
	PhaseEndsAt = 0.0;
}

void ADCCArcadeHaunt::NextCouchGuest()
{
	if (CouchQueue.IsEmpty())
	{
		Interviewee.Reset();
		CouchQuestion.Reset();
		if (ShowLevel >= ShowLevels)
		{
			EndEpisode();
		}
		else
		{
			StartVerdict();
		}
		return;
	}
	Interviewee = CouchQueue[0];
	CouchQueue.RemoveAt(0);
	FDCCShowGuest& Guest = GuestFor(Interviewee);
	Guest.Answer = -1;
	bCouchAnswered = false;
	// To the hot seat, facing Hiss.
	TArray<APlayerController*> Players;
	GetPlayers(Players);
	for (APlayerController* PC : Players)
	{
		if (PC && PC->PlayerState && PC->PlayerState->GetPlayerName() == Interviewee && PC->GetPawn())
		{
			const FVector Seat = HotSeat(StudioOrigin());
			PC->GetPawn()->TeleportTo(Seat, FRotator(0.0f, -147.0f, 0.0f));
			PC->SetControlRotation(FRotator(-8.0f, -147.0f, 0.0f));
			if (ADCCCharacterBase* Moved = Cast<ADCCCharacterBase>(PC->GetPawn()))
			{
				Moved->ForgetFall();
			}
		}
	}
	Hiss(Guest.Mood >= 2 ? TEXT("P_Q_WARM") : Guest.Mood <= -2 ? TEXT("P_Q_COLD") : TEXT("P_Q_NEUTRAL"));
	PhaseEndsAt = ServerTime() + 16.0;
}

void ADCCArcadeHaunt::AnswerCouch(APlayerController* PC, int32 Answer)
{
	if (!bPrimetime || ShowPhase != 2 || bCouchAnswered || !PC || !PC->PlayerState || PC->PlayerState->GetPlayerName() != Interviewee)
	{
		return;
	}
	Answer = FMath::Clamp(Answer, 0, 2);
	FDCCShowGuest& Guest = GuestFor(Interviewee);
	Guest.Answer = Answer;
	bCouchAnswered = true;
	// Charm warms him, Banter is good television, Defy cools him and the crowd loves it.
	static const int32 MoodBy[] = { 1, 0, -1 };
	static const int32 RatingsBy[] = { 40, 70, 160 };
	static const TCHAR* Replies[] = { TEXT("P_R_CHARM"), TEXT("P_R_BANTER"), TEXT("P_R_DEFY") };
	Guest.Mood = FMath::Clamp(Guest.Mood + MoodBy[Answer], -3, 3);
	Guest.Ratings += RatingsBy[Answer];
	if (ADCCCharacterBase* Crawler = Cast<ADCCCharacterBase>(PC->GetPawn()))
	{
		if (Answer > 0)
		{
			DCCAudience::Moment(this, Crawler, Answer == 2 ? DCCAudience::EMoment::PerfectTrick : DCCAudience::EMoment::Trick);
		}
	}
	Hiss(Replies[Answer]);
	UE_LOG(LogTemp, Log, TEXT("[DCC Primetime] %s answers %d; mood now %d"), *Interviewee, Answer, Guest.Mood);
}

void ADCCArcadeHaunt::StartVerdict()
{
	ShowPhase = 3;
	PlanLevel(ShowLevel + 1);
	// Personal verdicts: his favourites get looked after, his problems get hunted.
	float Team = 0.0f;
	int32 In = 0;
	bool bAnyOut = false;
	for (FDCCShowGuest& Guest : Guests)
	{
		if (Guest.bOut)
		{
			bAnyOut = true;
			continue;
		}
		Team += Guest.Mood;
		++In;
		Guest.Personal = Guest.Mood >= 2 ? 1 : Guest.Mood <= -2 ? 2 : 0;
	}
	Team = In > 0 ? Team / In : 0.0f;
	for (const FDCCShowGuest& Guest : Guests)
	{
		if (!Guest.bOut && Guest.Personal == 1) { Hiss(TEXT("P_BLESS_PET")); break; }
	}
	for (const FDCCShowGuest& Guest : Guests)
	{
		if (!Guest.bOut && Guest.Personal == 2) { Hiss(TEXT("P_TWIST_MARKED")); break; }
	}
	// The team's: a Blessing when he likes them, a Twist when he does not.
	TeamBlessing = 0;
	TeamTwist = 0;
	if (bLevelCleared && bAnyOut && Team >= 0.0f)
	{
		TeamBlessing = 3;
		// The audience member he likes best comes back.
		FDCCShowGuest* Back = nullptr;
		for (FDCCShowGuest& Guest : Guests)
		{
			if (Guest.bOut && (!Back || Guest.Mood > Back->Mood)) { Back = &Guest; }
		}
		if (Back)
		{
			Back->bOut = false;
			Back->OutOnLevel = 0;
		}
	}
	else if (Team >= 1.0f)
	{
		TeamBlessing = RoundKind == 1 ? 2 : 1;
	}
	if (Team <= -1.0f)
	{
		TeamTwist = uint8(FMath::RandRange(1, 3));
	}
	static const TCHAR* BlessTags[] = { TEXT(""), TEXT("P_BLESS_OVERTIME"), TEXT("P_BLESS_LIGHTS"), TEXT("P_BLESS_TAKES") };
	static const TCHAR* TwistTags[] = { TEXT(""), TEXT("P_TWIST_GUEST"), TEXT("P_TWIST_SCHEDULE"), TEXT("P_TWIST_DARK") };
	if (TeamBlessing) { Hiss(BlessTags[TeamBlessing]); }
	if (TeamTwist) { Hiss(TwistTags[TeamTwist]); }
	if (!TeamBlessing && !TeamTwist) { Hiss(TEXT("P_VERDICT_NONE")); }
	PhaseEndsAt = 0.0; // set when he has finished talking
	UE_LOG(LogTemp, Log, TEXT("[DCC Primetime] verdict: team mood %.1f, blessing %d, twist %d, next %s"), Team, TeamBlessing, TeamTwist, RoundName(RoundKind));
}

void ADCCArcadeHaunt::EndEpisode()
{
	ShowPhase = 4;
	Interviewee.Reset();
	Hiss(TEXT("P_SIGNOFF"));
	TArray<APlayerController*> Players;
	GetPlayers(Players);
	for (APlayerController* PC : Players)
	{
		if (!PC || !PC->PlayerState)
		{
			continue;
		}
		const FDCCShowGuest* Guest = FindGuest(PC->PlayerState->GetPlayerName());
		const int32 Ratings = Guest ? Guest->Ratings + (Guest->bOut ? 0 : 200) : 0;
		DCCProfile::Add(PC->PlayerState, TEXT("Haunt"), TEXT("Plays"), 1.0f);
		DCCProfile::RecordBest(PC->PlayerState, TEXT("Haunt"), TEXT("BestRatings"), float(Ratings));
		UDCCWalletComponent::AwardTickets(PC->PlayerState, FMath::Clamp(Ratings / 30, 1, 40), TEXT("PRIMETIME ratings"));
		SendToStudio(PC->GetPawn(), Guest && Guest->bOut, 0);
	}
	PhaseEndsAt = ServerTime() + 14.0;
}

void ADCCArcadeHaunt::DebugShow(int32 What)
{
	if (!HasAuthority() || !bPrimetime)
	{
		return;
	}
	if (What == 1 && ShowPhase == 1) { EndLevel(true); }
	else if (What == 2 && ShowPhase == 1) { EndLevel(false); }
	else if (What == 3 && ShowPhase == 2) { CouchQueue.Reset(); bCouchAnswered = true; CouchReplyAt = 0.0; GQueue.FindOrAdd(this).Reset(); }
	else if (What == 0 && ShowPhase == 0) { PhaseEndsAt = ServerTime(); }
}

void ADCCArcadeHaunt::OnRespawned(APlayerController* PC)
{
	if (!bPrimetime || !PC || !PC->PlayerState)
	{
		return;
	}
	// Dead on the show means out: to the audience, to watch the rest.
	FDCCShowGuest& Guest = GuestFor(PC->PlayerState->GetPlayerName());
	if (!Guest.bOut && ShowPhase == 1)
	{
		Guest.bOut = true;
		Guest.OutOnLevel = ShowLevel;
	}
	SendToStudio(PC->GetPawn(), Guest.bOut, Guests.IndexOfByPredicate([&Guest](const FDCCShowGuest& G) { return G.Player == Guest.Player; }));
	if (ShowPhase == 1)
	{
		TArray<APlayerController*> Players;
		GetPlayers(Players);
		for (APlayerController* Other : Players)
		{
			const FDCCShowGuest* Them = Other && Other->PlayerState ? FindGuest(Other->PlayerState->GetPlayerName()) : nullptr;
			if (Them && !Them->bOut && Other->GetPawn())
			{
				PC->SetViewTargetWithBlend(Other->GetPawn(), 0.6f);
				break;
			}
		}
	}
}

void ADCCArcadeHaunt::TickShow(float DeltaSeconds)
{
	const double Now = ServerTime();
	Title.Reset(); // the show draws its own banner (UI/DCCHUD_Primetime.cpp)
	Accent = FLinearColor(1.0f, 0.78f, 0.25f);

	// Hiss, one line at a time.
	TArray<FQueued>& Queue = GQueue.FindOrAdd(this);
	double& NextLine = GNextLineAt.FindOrAdd(this);
	if (!Queue.IsEmpty() && Now >= NextLine)
	{
		const FQueued Line = Queue[0];
		Queue.RemoveAt(0);
		Multicast_HissSays(Line.Row, Line.Text);
		NextLine = Now + LineSeconds(Line.Text);
	}
	const bool bQuiet = Queue.IsEmpty() && Now >= NextLine;

	TArray<APlayerController*> Players;
	GetPlayers(Players);
	TSet<FString> Here;
	for (APlayerController* PC : Players)
	{
		if (PC && PC->PlayerState)
		{
			Here.Add(PC->PlayerState->GetPlayerName());
			GuestFor(PC->PlayerState->GetPlayerName());
		}
	}
	// Someone who has left (or a name that changed as they joined) is not a guest any more.
	Guests.RemoveAll([&Here](const FDCCShowGuest& G) { return !Here.Contains(G.Player); });

	switch (ShowPhase)
	{
	case 0: // the opening
	{
		if (PhaseEndsAt <= 0.0)
		{
			if (Players.IsEmpty())
			{
				return;
			}
			int32 Slot = 0;
			for (APlayerController* PC : Players)
			{
				SendToStudio(PC ? PC->GetPawn() : nullptr, false, Slot++);
			}
			Hiss(TEXT("P_OPEN"));
			PhaseEndsAt = Now + 6.0;
		}
		StatusLines = { TEXT("The show is about to start. Stand on your mark.") };
		if (Now >= PhaseEndsAt && bQuiet)
		{
			PlanLevel(1);
			StartLevel();
		}
		break;
	}
	case 1: // a level
	{
		const float Left = FMath::Max(0.0f, float(PhaseEndsAt - Now));
		TArray<ADCCCharacterBase*> Standing;
		for (APlayerController* PC : Players)
		{
			const FDCCShowGuest* Guest = PC && PC->PlayerState ? FindGuest(PC->PlayerState->GetPlayerName()) : nullptr;
			ADCCCharacterBase* Crawler = PC ? Cast<ADCCCharacterBase>(PC->GetPawn()) : nullptr;
			if (!Guest || Guest->bOut || !Crawler)
			{
				continue;
			}
			// Hiss notices when someone goes down.
			if (Crawler->IsDowned() || !Crawler->IsAlive())
			{
				if (!SeenDown.Contains(Guest->Player) && Now >= NextDownLineAt)
				{
					SeenDown.Add(Guest->Player);
					Hiss(TEXT("P_DOWN"));
					NextDownLineAt = Now + 6.0;
				}
				continue;
			}
			Standing.Add(Crawler);
			// Producer's Pet: Hiss quietly pays back half of every hit.
			if (Guest->Personal == 1 && Crawler->Stats)
			{
				const int32 Hp = Crawler->Stats->CurrentHitPoints;
				const int32* Was = PetHealth.Find(Guest->Player);
				if (Was && Hp < *Was)
				{
					Crawler->Stats->Heal((*Was - Hp) / 2);
				}
				PetHealth.Add(Guest->Player, Crawler->Stats->CurrentHitPoints);
			}
			// Lights Out: lamps stay off, unless the Lighting Department is on your side.
			if (RoundKind == 1 && TeamBlessing != 2 && !IsLampOff(Guest->Player))
			{
				SetLamp(Guest->Player, false);
			}
		}
		if (Standing.IsEmpty())
		{
			EndLevel(false);
			return;
		}
		// The goals.
		bool bDone = false;
		if (RoundKind == 0)
		{
			int32 There = 0;
			for (ADCCCharacterBase* Crawler : Standing)
			{
				if (Goals.Num() > 0 && FVector::Dist2D(Crawler->GetActorLocation(), Goals[0]) < 600.0f)
				{
					++There;
					if (!GoalsDone[0] && Crawler->GetPlayerState())
					{
						GoalsDone[0] = true; // first one there: a little extra
						GuestFor(Crawler->GetPlayerState()->GetPlayerName()).Ratings += 60;
					}
				}
			}
			bDone = There == Standing.Num();
		}
		else if (RoundKind == 1)
		{
			for (int32 g = 0; g < Goals.Num(); ++g)
			{
				for (ADCCCharacterBase* Crawler : Standing)
				{
					if (!GoalsDone[g] && FVector::Dist2D(Crawler->GetActorLocation(), Goals[g]) < 300.0f)
					{
						GoalsDone[g] = true;
						if (Crawler->GetPlayerState())
						{
							GuestFor(Crawler->GetPlayerState()->GetPlayerName()).Ratings += 40;
						}
					}
				}
			}
			bDone = !GoalsDone.Contains(false);
		}
		else
		{
			// Hold Out and the finale: fill the ring by standing in it; things come while you do.
			bool bIn = false;
			for (ADCCCharacterBase* Crawler : Standing)
			{
				bIn |= Goals.Num() > 0 && FVector::Dist2D(Crawler->GetActorLocation(), Goals[0]) < 550.0f;
			}
			const float Need = RoundKind == 3 ? HoldSeconds * 1.4f : HoldSeconds;
			if (bIn)
			{
				HoldProgress = FMath::Min(1.0f, HoldProgress + DeltaSeconds / Need);
				if (Now >= HoldWaveAt && Grid)
				{
					HoldWaveAt = Now + 7.0;
					for (int32 d = 0; d < 2; ++d)
					{
						const FVector Where = Goals[0] + FRotator(0.0f, FMath::FRandRange(0.0f, 360.0f), 0.0f).Vector() * 700.0f + FVector(0, 0, 140);
						if (ADCCEnemyCharacter* Dog = GetWorld()->SpawnActorDeferred<ADCCEnemyCharacter>(ADCCEnemyCharacter_SlagDog::StaticClass(),
							FTransform(Where), nullptr, nullptr, ESpawnActorCollisionHandlingMethod::AdjustIfPossibleButAlwaysSpawn))
						{
							Dog->SetChaseGrid(Grid);
							Dog->LeashRadius = 6000.0f;
							Dog->FinishSpawning(FTransform(Where));
							Monsters.Add(Dog);
						}
					}
				}
			}
			bDone = HoldProgress >= 1.0f;
		}
		StatusLines.Reset();
		StatusLines.Add(FString::Printf(TEXT("LEVEL %d OF %d   %s   %d:%02d"), ShowLevel, ShowLevels, RoundName(RoundKind),
			FMath::FloorToInt(Left / 60.0f), FMath::FloorToInt(FMath::Fmod(Left, 60.0f))));
		if (bDone)
		{
			EndLevel(true);
			return;
		}
		if (Left <= 0.0f)
		{
			EndLevel(false);
			return;
		}
		break;
	}
	case 2: // the couch
	{
		StatusLines = { FString::Printf(TEXT("THE COUCH   after level %d"), ShowLevel) };
		if (bCouchAnswered)
		{
			// Wait for the welcome (or his reply) to finish, then the next guest.
			if (bQuiet)
			{
				NextCouchGuest();
			}
		}
		else if (Now >= PhaseEndsAt)
		{
			// Nothing said: dead air on his couch. That counts as banter, barely.
			APlayerController* Guest = nullptr;
			for (APlayerController* PC : Players)
			{
				if (PC && PC->PlayerState && PC->PlayerState->GetPlayerName() == Interviewee) { Guest = PC; }
			}
			if (Guest)
			{
				AnswerCouch(Guest, 1);
			}
			else
			{
				bCouchAnswered = true;
			}
		}
		break;
	}
	case 3: // the verdict
	{
		StatusLines = { FString::Printf(TEXT("NEXT: LEVEL %d, %s"), ShowLevel, RoundName(RoundKind)) };
		if (bQuiet)
		{
			if (PhaseEndsAt <= 0.0)
			{
				PhaseEndsAt = Now + 2.0;
			}
			else if (Now >= PhaseEndsAt)
			{
				StartLevel();
			}
		}
		break;
	}
	default: // the end
	{
		StatusLines = { TEXT("That's the show.") };
		if (PhaseEndsAt > 0.0 && Now >= PhaseEndsAt && bQuiet && !bRoundOver)
		{
			PhaseEndsAt = 0.0;
			ReturnToLobbyIn(25.0f);
		}
		break;
	}
	}
}
