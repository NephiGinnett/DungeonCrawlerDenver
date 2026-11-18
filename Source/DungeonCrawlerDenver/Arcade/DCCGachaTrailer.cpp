#include "Arcade/DCCGachaTrailer.h"

#include "Arcade/DCCArcadeGames.h"
#include "Arcade/DCCSummons.h"
#include "Animation/AnimSequence.h"
#include "Camera/CameraActor.h"
#include "Camera/CameraComponent.h"
#include "Characters/DCCAllyCharacters.h"
#include "Characters/DCCCharacterBase.h"
#include "Components/CapsuleComponent.h"
#include "Components/SkeletalMeshComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Components/TextRenderComponent.h"
#include "Engine/StaticMesh.h"
#include "Engine/StaticMeshActor.h"
#include "Engine/World.h"
#include "EngineUtils.h"
#include "Equipment/DCCEquipmentComponent.h"
#include "Equipment/DCCEquipmentTypes.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "GameFramework/GameModeBase.h"
#include "GameFramework/PlayerController.h"
#include "HAL/IConsoleManager.h"
#include "Materials/MaterialInstanceDynamic.h"
#include "Onboarding/DCCOnboardingComponent.h"
#include "Session/DCCSession.h"
#include "Stats/DCCStatsComponent.h"
#include "UI/DCCHUD.h"
#include "UnrealClient.h"

namespace
{
	float Ease(float A) { return FMath::SmoothStep(0.0f, 1.0f, FMath::Clamp(A, 0.0f, 1.0f)); }
	float Phase(float T, float Start, float End) { return FMath::Clamp((T - Start) / (End - Start), 0.0f, 1.0f); }

	const TCHAR* ClipWalk = TEXT("/Game/DCC/Characters/Shared/AnimsFixed/A_DCC_DW_Walking.A_DCC_DW_Walking");
	const TCHAR* ClipPress = TEXT("/Game/DCC/Characters/Shared/AnimsFixed/A_DCC_DW_CallGesture.A_DCC_DW_CallGesture");
	const TCHAR* ClipShock = TEXT("/Game/DCC/Characters/Shared/AnimsFixed/A_DCC_OA_Alert.A_DCC_OA_Alert");
	const TCHAR* ClipNod = TEXT("/Game/DCC/Characters/Shared/AnimsFixed/A_DCC_AO_AgreeGesture.A_DCC_AO_AgreeGesture");
	const TCHAR* Dances[] = {
		TEXT("/Game/DCC/Characters/Shared/AnimsFixed/A_DCC_HW_BoomDance.A_DCC_HW_BoomDance"),
		TEXT("/Game/DCC/Characters/Shared/AnimsFixed/A_DCC_AO_AllNightDance.A_DCC_AO_AllNightDance"),
		TEXT("/Game/DCC/Characters/Shared/AnimsFixed/A_DCC_DW_CardioDance.A_DCC_DW_CardioDance"),
		TEXT("/Game/DCC/Characters/Shared/AnimsFixed/A_DCC_AO_SpinJump.A_DCC_AO_SpinJump"),
	};

	// The pulls, in order: the first on its own, then the button-mashing. Friend, variant (0 plain, 1 Shiny, 2 Shadow),
	// armed. Marcel is pulling, so he never comes down himself.
	struct FScripted { const TCHAR* Friend; uint8 Variant; bool bArmed; };
	const FScripted Pulls[] = {
		{ TEXT("Kyle"), 0, false },
		{ TEXT("Scott"), 0, false }, { TEXT("Vaughn"), 2, false }, { TEXT("CJ"), 1, false }, { TEXT("Zoe"), 0, false },
		{ TEXT("Kyle"), 0, true }, { TEXT("Scott"), 1, false }, { TEXT("Vaughn"), 0, false }, { TEXT("Zoe"), 2, false },
		{ TEXT("CJ"), 0, false }, { TEXT("Kyle"), 2, false }, { TEXT("Scott"), 0, true }, { TEXT("Zoe"), 1, false },
		{ TEXT("Vaughn"), 1, false }, { TEXT("CJ"), 2, false }, { TEXT("Kyle"), 0, false },
	};
	constexpr int32 PullCount = UE_ARRAY_COUNT(Pulls);
	constexpr float FirstPullAt = 6.3f;
	constexpr float SpamFrom = 13.0f;
	constexpr float SpamEvery = 0.85f;
	float PullTime(int32 i) { return i == 0 ? FirstPullAt : SpamFrom + (i - 1) * SpamEvery; }

	AStaticMeshActor* Prop(UWorld* World, const TCHAR* Shape, const FVector& At, const FVector& Size, const FRotator& Rot, const FLinearColor& Colour)
	{
		UStaticMesh* Mesh = LoadObject<UStaticMesh>(nullptr, *FString::Printf(TEXT("/Engine/BasicShapes/%s.%s"), Shape, Shape));
		UMaterialInterface* Base = LoadObject<UMaterialInterface>(nullptr, TEXT("/Engine/BasicShapes/BasicShapeMaterial.BasicShapeMaterial"));
		FActorSpawnParameters Params;
		Params.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
		AStaticMeshActor* Actor = World->SpawnActor<AStaticMeshActor>(At, Rot, Params);
		if (!Actor || !Mesh)
		{
			return Actor;
		}
		UStaticMeshComponent* Component = Actor->GetStaticMeshComponent();
		Component->SetMobility(EComponentMobility::Movable);
		Component->SetStaticMesh(Mesh);
		Component->SetCollisionEnabled(ECollisionEnabled::NoCollision);
		Actor->SetActorScale3D(Size / 100.0f);
		if (UMaterialInstanceDynamic* Mid = Base ? UMaterialInstanceDynamic::Create(Base, Actor) : nullptr)
		{
			Mid->SetVectorParameterValue(TEXT("Color"), Colour);
			Component->SetMaterial(0, Mid);
		}
		return Actor;
	}
}

ADCCGachaTrailerDirector::ADCCGachaTrailerDirector()
{
	PrimaryActorTick.bCanEverTick = true;
	PrimaryActorTick.TickGroup = TG_PostPhysics;
	SetReplicates(false);
}

FVector ADCCGachaTrailerDirector::Rel(float X, float Y, float Z) const
{
	return Origin + FVector(X, Y, Z);
}

void ADCCGachaTrailerDirector::Aim(const FVector& From, const FVector& At, float Fov)
{
	if (Camera)
	{
		Camera->SetActorLocationAndRotation(From, (At - From).Rotation());
		Camera->GetCameraComponent()->SetFieldOfView(Fov);
	}
}

void ADCCGachaTrailerDirector::SetClip(ADCCCharacterBase* Who, const TCHAR* Path)
{
	UAnimSequence* Clip = Who ? LoadObject<UAnimSequence>(nullptr, Path) : nullptr;
	if (Clip && Who->IdleAnimation != Clip)
	{
		Who->IdleAnimation = Clip;
		Who->WalkAnimation = Clip;
		Who->RunAnimation = Clip;
		Who->RestartLoopAnimation();
	}
}

void ADCCGachaTrailerDirector::BuildPile()
{
	// Marcel's winnings: a heap of gold coins and ticket rolls on a little rug, with a few loose ones spilling off it.
	UWorld* World = GetWorld();
	auto Keep = [this](AActor* Actor) { PileProps.Add(Actor); };
	const FLinearColor Gold(1.0f, 0.72f, 0.18f), Pink(1.0f, 0.3f, 0.65f), Paper(0.95f, 0.92f, 0.85f), Rug(0.35f, 0.06f, 0.3f);
	Keep(Prop(World, TEXT("Cylinder"), PileAt + FVector(0, 0, 1), FVector(260, 260, 2), FRotator::ZeroRotator, Rug));
	FRandomStream R(1717);
	// The heap: coins in rings that shrink as they rise.
	for (int32 Layer = 0; Layer < 7; ++Layer)
	{
		const float Ring = 100.0f - Layer * 13.0f;
		const int32 Count = FMath::Max(1, 18 - Layer * 2);
		for (int32 k = 0; k < Count; ++k)
		{
			const FVector P = FRotator(0, k * 360.0f / Count + Layer * 17.0f, 0).Vector() * R.FRandRange(0.0f, Ring);
			Keep(Prop(World, TEXT("Cylinder"), PileAt + P + FVector(0, 0, 6 + Layer * 11.0f), FVector(26, 26, 5),
				FRotator(R.FRandRange(-25, 25), R.FRandRange(0, 360), R.FRandRange(-25, 25)), Gold));
		}
	}
	// Ticket rolls leaning on the heap, and a few tickets fanned out on the floor.
	for (int32 k = 0; k < 6; ++k)
	{
		const FVector P = FRotator(0, k * 60.0f + 15.0f, 0).Vector() * 115.0f;
		Keep(Prop(World, TEXT("Cylinder"), PileAt + P + FVector(0, 0, 22), FVector(42, 42, 36), FRotator(90.0f, k * 60.0f, 0), k % 2 ? Pink : Paper));
	}
	for (int32 k = 0; k < 10; ++k)
	{
		const FVector P = FRotator(0, R.FRandRange(0, 360), 0).Vector() * R.FRandRange(130.0f, 210.0f);
		Keep(Prop(World, TEXT("Cube"), PileAt + P + FVector(0, 0, 2), FVector(30, 14, 1), FRotator(0, R.FRandRange(0, 360), 0), k % 3 ? Pink : Paper));
	}
	// A big gold coin standing up at the front, like a sign.
	Keep(Prop(World, TEXT("Cylinder"), PileAt + FVector(-30, 120, 48), FVector(80, 80, 8), FRotator(90.0f, 90.0f, 0), Gold));
}

void ADCCGachaTrailerDirector::Setup()
{
	UWorld* World = GetWorld();
	for (TActorIterator<ADCCArcadeLobby> It(World); It; ++It) { Lobby = *It; break; }
	ADCCArcadeLobby* Den = Lobby.Get();
	const AGameModeBase* Mode = World->GetAuthGameMode();
	UClass* PawnClass = Mode && Mode->DefaultPawnClass ? Mode->DefaultPawnClass.Get() : nullptr;
	APlayerController* PC = World->GetFirstPlayerController();
	if (!Den || !PawnClass || !PC)
	{
		UE_LOG(LogTemp, Warning, TEXT("[DCC GachaTrailer] needs the Den"));
		bDone = true;
		return;
	}
	Origin = Den->GetActorLocation();
	Spot = Den->PodiumCenter(1) + FVector(0, 140, 92);
	PileAt = Spot + FVector(260, 70, -92);
	ADCCHUD::bTrailerMinimal = true;

	// The real player steps out of shot.
	if (APawn* Player = PC->GetPawn())
	{
		Player->SetActorLocation(Rel(3600, 0, 120));
		Player->SetActorHiddenInGame(true);
	}

	FActorSpawnParameters Params;
	Params.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
	Marcel = World->SpawnActor<ADCCCharacterBase>(PawnClass, Rel(-50, -300, 92), FRotator(0, -90, 0), Params);
	if (Marcel)
	{
		Marcel->SetReplicates(false);
		Marcel->DisplayName = FText::FromString(TEXT("Marcel"));
		if (Marcel->Nameplate) { Marcel->Nameplate->SetVisibility(false); }
		if (UCapsuleComponent* Capsule = Marcel->GetCapsuleComponent()) { Capsule->SetCollisionEnabled(ECollisionEnabled::NoCollision); }
		if (UCharacterMovementComponent* Movement = Marcel->GetCharacterMovement()) { Movement->DisableMovement(); }
		if (Marcel->Onboarding)
		{
			Marcel->Onboarding->CrawlerId = TEXT("Marcel");
			Marcel->Onboarding->ApplyCrawlerLook();
		}
		if (USkeletalMeshComponent* Body = Marcel->GetMesh()) { Body->SetAnimationMode(EAnimationMode::AnimationSingleNode); }
		Marcel->SetActorTickEnabled(true);
		SetClip(Marcel, ClipWalk);
	}
	BuildPile();
	Camera = World->SpawnActor<ACameraActor>(Rel(-50, 250, 330), FRotator::ZeroRotator, Params);
	if (Camera)
	{
		Camera->GetCameraComponent()->bConstrainAspectRatio = false;
		PC->SetViewTarget(Camera);
	}
	bSetUp = true;
	UE_LOG(LogTemp, Display, TEXT("[DCC GachaTrailer] set up"));
}

void ADCCGachaTrailerDirector::Pull(int32 Index)
{
	ADCCArcadeLobby* Den = Lobby.Get();
	UWorld* World = GetWorld();
	if (!Den || !World || !Marcel)
	{
		return;
	}
	const FScripted& Script = Pulls[Index];
	DCCSummons::FSummon Summon;
	Summon.Friend = FName(Script.Friend);
	Summon.Variant = Script.Variant;
	// The friend's real rarity, from the podium's own table.
	const int32 RosterIndex = DCCSession::Roster().IndexOfByKey(Summon.Friend);
	Summon.Tier = Den->FriendTiers.IsValidIndex(RosterIndex) ? Den->FriendTiers[RosterIndex] : 0;
	FString WeaponName;
	if (Script.bArmed && Marcel->Equipment && Marcel->Equipment->EquipmentTable)
	{
		// A different weapon each time it comes up: the Nth main-hand row of the gear table.
		int32 Seen = 0;
		for (const TPair<FName, uint8*>& Row : Marcel->Equipment->EquipmentTable->GetRowMap())
		{
			const FDCCEquipmentDef* Def = reinterpret_cast<const FDCCEquipmentDef*>(Row.Value);
			if (Def && Def->Slot == EDCCEquipmentSlot::MainHand && Seen++ == Index % 7 + 2)
			{
				Summon.Weapon = Row.Key;
				WeaponName = Def->DisplayName.IsEmpty() ? Row.Key.ToString() : Def->DisplayName.ToString();
				break;
			}
		}
	}
	// The first pull comes down the tube Marcel is standing at; then all four, in turn.
	const int32 Tube = Index == 0 ? 1 : (Index - 1) % ADCCArcadeLobby::TubeCount;
	Den->StartTube(Tube, Summon.Tier, true);
	if (ADCCSummonedAlly* Ally = DCCSummons::SpawnAlly(World, Summon, Marcel, Den->TubeCenter(Tube) + FVector(0, 0, 70.0f), 640.0f, Den->TubeExit(Tube)))
	{
		if (Ally->Nameplate) { Ally->Nameplate->SetVisibility(false); }
		if (Ally->Stats)
		{
			Ally->Stats->MaxHitPoints = 100000;
			Ally->Stats->CurrentHitPoints = 100000;
		}
		Summoned.Add(Ally);
	}
	if (APlayerController* PC = World->GetFirstPlayerController())
	{
		if (ADCCHUD* Hud = Cast<ADCCHUD>(PC->GetHUD()))
		{
			Hud->ShowSummonResult(Summon.Friend, Summon.Tier, Summon.Variant, WeaponName);
		}
	}
}

void ADCCGachaTrailerDirector::Act(float T)
{
	if (!Marcel)
	{
		return;
	}
	// The pulls, on their beats.
	while (NextPull < PullCount && T >= PullTime(NextPull))
	{
		Pull(NextPull++);
	}

	const FVector Start = Rel(-50, -300, 92);
	const float FacePodium = -90.0f;
	const float FacePile = (PileAt - Spot).Rotation().Yaw;
	const TCHAR* Clip = ClipWalk;
	FVector At = Spot;
	float Yaw = FacePodium;
	if (T < 4.0f)
	{
		// Walking up to the podium.
		At = FMath::Lerp(Start, Spot, Ease(Phase(T, 0.3f, 4.0f)));
		Clip = T < 3.9f ? ClipWalk : ClipPress;
	}
	else if (T < 9.4f)
	{
		Clip = T < 5.0f ? ClipNod : ClipPress; // a nod at the screen, then the press
	}
	else if (T < 12.6f)
	{
		// He turns to the pile, freezes at the sight of it, then nods slowly.
		Yaw = FMath::Lerp(FacePodium, FacePile, Ease(Phase(T, 9.4f, 10.0f)));
		Clip = T < 11.0f ? ClipShock : ClipNod;
	}
	else if (T < 27.0f)
	{
		// Back to the button, mashing it.
		Yaw = FMath::Lerp(FacePile, FacePodium, Ease(Phase(T, 12.6f, 13.0f)));
		Clip = ClipPress;
	}
	else
	{
		Yaw = FacePodium + 180.0f * Ease(Phase(T, 27.0f, 28.0f)); // turns round to his new crowd
		Clip = Dances[0];
	}
	Marcel->SetActorLocationAndRotation(At, FRotator(0, Yaw, 0));
	SetClip(Marcel, Clip);
	// Once he knows he can make more friends, his hand is a blur (Zoe: "make his hand mash faster").
	if (USkeletalMeshComponent* Body = Marcel->GetMesh())
	{
		Body->SetPlayRate(T >= 13.0f && T < 27.0f ? 2.8f : 1.0f);
	}
	// The pile is spent as he pulls: gone from the top down by the last one.
	if (T >= SpamFrom && !PileProps.IsEmpty())
	{
		const float Spent = FMath::Clamp((T - SpamFrom) / (PullTime(PullCount - 1) + 0.5f - SpamFrom), 0.0f, 1.0f);
		const int32 Keep = FMath::RoundToInt((1.0f - Spent) * PileProps.Num());
		for (int32 i = PileProps.Num() - 1; i >= Keep; --i)
		{
			if (AActor* Gone = PileProps[i].Get())
			{
				Gone->SetActorHiddenInGame(true);
			}
		}
	}

	// The party: everyone stops where they are and dances.
	if (T >= 27.4f && !bDancing)
	{
		bDancing = true;
		for (int32 i = 0; i < Summoned.Num(); ++i)
		{
			ADCCCharacterBase* Ally = Summoned[i];
			if (!Ally)
			{
				continue;
			}
			if (UCharacterMovementComponent* Move = Ally->GetCharacterMovement())
			{
				Move->StopMovementImmediately();
				Move->DisableMovement();
			}
			Ally->SetActorRotation(FRotator(0, (Spot - Ally->GetActorLocation()).Rotation().Yaw, 0));
			SetClip(Ally, Dances[i % UE_ARRAY_COUNT(Dances)]);
		}
	}
}

void ADCCGachaTrailerDirector::Shoot(float T)
{
	ADCCArcadeLobby* Den = Lobby.Get();
	if (!Den)
	{
		return;
	}
	const FVector Tube1 = Den->TubeCenter(1);
	if (T < 4.5f)
	{
		// Wide on the four tubes, pushing in as Marcel walks up.
		const float A = Ease(Phase(T, 0.0f, 4.5f));
		Aim(FMath::Lerp(Rel(-50, 300, 340), Rel(-50, -350, 300), A), Rel(-50, -1450, 330), 75.0f);
	}
	else if (T < 6.2f)
	{
		// Over his shoulder at the podium, the tube beyond.
		Aim(Spot + FVector(110, 170, 90), Tube1 + FVector(0, 0, 230), 62.0f);
	}
	else if (T < 9.4f)
	{
		// The tube, close: the friend shoots down it and steps out of the gap.
		const float A = Phase(T, 6.2f, 9.4f);
		Aim(Tube1 + FVector(FMath::Lerp(-60.0f, 60.0f, A), 560, 230), Tube1 + FVector(0, 0, 260), 58.0f);
	}
	else if (T < 13.0f)
	{
		// Marcel and the pile.
		const FVector Mid = (Spot + PileAt) * 0.5f + FVector(0, 0, 60);
		Aim(Mid + FVector(-60, 430, 90), Mid, 55.0f);
	}
	else if (T < 17.0f)
	{
		// Low and wide: all four tubes going off.
		Aim(Rel(0, -420, 110), Rel(0, -1450, 360), 82.0f);
	}
	else if (T < 20.5f)
	{
		// Close on Marcel mashing the button.
		Aim(Spot + FVector(-170, 150, 60), Spot + FVector(0, -60, 40), 50.0f);
	}
	else if (T < 24.0f)
	{
		// Tracking along the tubes as friends keep coming down.
		const float A = Ease(Phase(T, 20.5f, 24.0f));
		Aim(Rel(FMath::Lerp(1100.0f, -1100.0f, A), -420, 420), Rel(FMath::Lerp(600.0f, -600.0f, A), -1450, 230), 66.0f);
	}
	else if (T < 27.0f)
	{
		// From above: the crowd round him keeps growing.
		Aim(Spot + FVector(0, 650, 650), Spot, 70.0f);
	}
	else if (T < 34.0f)
	{
		// The party, circling.
		const float Angle = FMath::Lerp(-30.0f, 120.0f, Phase(T, 27.0f, 34.0f));
		const FVector Around = FRotator(0, Angle, 0).Vector() * 720.0f;
		Aim(Spot + FVector(Around.X, Around.Y + 150.0f, 230), Spot + FVector(0, 0, 40), 68.0f);
	}
	else
	{
		// Up and away off the crowd, for the title.
		const float A = Ease(Phase(T, 34.0f, 40.0f));
		const FVector Around = FRotator(0, 120.0f, 0).Vector() * FMath::Lerp(720.0f, 1100.0f, A);
		Aim(Spot + FVector(Around.X, Around.Y + 150.0f, FMath::Lerp(230.0f, 1250.0f, A)), Spot + FVector(0, 0, 20), 70.0f);
	}
}

void ADCCGachaTrailerDirector::Tick(float DeltaSeconds)
{
	Super::Tick(DeltaSeconds);
	if (bDone)
	{
		return;
	}
	Elapsed += DeltaSeconds;
	if (!bSetUp)
	{
		if (Elapsed > 1.0f)
		{
			Setup();
		}
		return;
	}
	const float T = FMath::Max(0.0f, Elapsed - PreRoll);
	Act(T);
	Shoot(T);
	APlayerController* PC = GetWorld()->GetFirstPlayerController();
	if (PC && Camera && PC->GetViewTarget() != Camera)
	{
		PC->SetViewTarget(Camera);
	}
	if (bCapture && Elapsed >= PreRoll)
	{
		FScreenshotRequest::RequestScreenshot(FString::Printf(TEXT("%s/frame_%05d.png"), *CaptureDir, Frame++), /*bShowUI=*/true, /*bAddFilenameSuffix=*/false);
	}
	if (T >= Length)
	{
		bDone = true;
		ADCCHUD::bTrailerMinimal = false;
		UE_LOG(LogTemp, Display, TEXT("[DCC GachaTrailer] done, %d frames"), Frame);
		if (bCapture && PC)
		{
			PC->ConsoleCommand(TEXT("quit"));
		}
	}
}

static void DCCGachaTrailerCommand(const TArray<FString>& Args, UWorld* World)
{
	if (!World)
	{
		return;
	}
	FActorSpawnParameters Params;
	Params.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
	ADCCGachaTrailerDirector* Director = World->SpawnActor<ADCCGachaTrailerDirector>(FVector::ZeroVector, FRotator::ZeroRotator, Params);
	if (Director && Args.Num() > 0 && Args[0].Equals(TEXT("capture"), ESearchCase::IgnoreCase))
	{
		Director->bCapture = true;
		Director->CaptureDir = Args.Num() > 1 ? Args[1] : FString(TEXT("D:/Builds/GachaTrailerFrames"));
	}
	UE_LOG(LogTemp, Display, TEXT("[DCC GachaTrailer] rolling%s"), Director && Director->bCapture ? TEXT(", capturing frames") : TEXT(""));
}

static FAutoConsoleCommandWithWorldAndArgs GGachaTrailerCommand(
	TEXT("DCC.GachaTrailer"),
	TEXT("DCC.GachaTrailer [capture <dir>]: play the F.I.A.T. ad (in the Den). Tools/make_trailer.py --gacha renders it."),
	FConsoleCommandWithWorldAndArgsDelegate::CreateStatic(&DCCGachaTrailerCommand));
