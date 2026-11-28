#include "Arcade/DCCSnickTape.h"

#include "Animation/AnimSequence.h"
#include "Arcade/DCCSets.h"
#include "Camera/CameraActor.h"
#include "Camera/CameraComponent.h"
#include "Characters/DCCCharacterBase.h"
#include "Components/CapsuleComponent.h"
#include "Components/DirectionalLightComponent.h"
#include "Components/PointLightComponent.h"
#include "Components/SkeletalMeshComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Components/TextRenderComponent.h"
#include "Components/WidgetComponent.h"
#include "Dom/JsonObject.h"
#include "Engine/DirectionalLight.h"
#include "Engine/PointLight.h"
#include "Engine/StaticMesh.h"
#include "Engine/StaticMeshActor.h"
#include "Engine/World.h"
#include "EngineUtils.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "GameFramework/GameModeBase.h"
#include "GameFramework/PlayerController.h"
#include "HAL/IConsoleManager.h"
#include "Materials/MaterialInstanceDynamic.h"
#include "Misc/FileHelper.h"
#include "Misc/Paths.h"
#include "Onboarding/DCCOnboardingComponent.h"
#include "Serialization/JsonReader.h"
#include "Serialization/JsonSerializer.h"
#include "UI/DCCHUD.h"
#include "UnrealClient.h"

namespace
{
	FString TapeDir() { return FPaths::ProjectSavedDir() / TEXT("SnickTape"); }

	TSharedPtr<FJsonObject> ReadJson(const FString& Name)
	{
		FString Text;
		TSharedPtr<FJsonObject> Out;
		if (FFileHelper::LoadFileToString(Text, *(TapeDir() / Name)))
		{
			FJsonSerializer::Deserialize(TJsonReaderFactory<>::Create(Text), Out);
		}
		return Out;
	}

	FVector Vec(const TArray<TSharedPtr<FJsonValue>>* A, const FVector& Default = FVector::ZeroVector)
	{
		if (!A || A->Num() < 3)
		{
			return Default;
		}
		return FVector((*A)[0]->AsNumber(), (*A)[1]->AsNumber(), (*A)[2]->AsNumber());
	}

	const TArray<TSharedPtr<FJsonValue>>* Arr(const TSharedPtr<FJsonObject>& O, const TCHAR* Key)
	{
		const TArray<TSharedPtr<FJsonValue>>* A = nullptr;
		return O.IsValid() && O->TryGetArrayField(Key, A) ? A : nullptr;
	}

	TSharedPtr<FJsonObject> Obj(const TSharedPtr<FJsonObject>& O, const TCHAR* Key)
	{
		const TSharedPtr<FJsonObject>* Out = nullptr;
		return O.IsValid() && O->TryGetObjectField(Key, Out) ? *Out : nullptr;
	}

	double Num(const TSharedPtr<FJsonObject>& O, const TCHAR* Key, double Default)
	{
		double V = Default;
		return O.IsValid() && O->TryGetNumberField(Key, V) ? V : Default;
	}

	AStaticMeshActor* Mesh(UWorld* World, const TCHAR* Path, const FVector& At, const FRotator& Rot, const FVector& Scale)
	{
		UStaticMesh* Asset = LoadObject<UStaticMesh>(nullptr, Path);
		if (!Asset)
		{
			UE_LOG(LogTemp, Warning, TEXT("[DCC SnickTape] missing %s"), Path);
			return nullptr;
		}
		FActorSpawnParameters Params;
		Params.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
		AStaticMeshActor* Actor = World->SpawnActor<AStaticMeshActor>(At, Rot, Params);
		if (Actor)
		{
			Actor->GetStaticMeshComponent()->SetMobility(EComponentMobility::Movable);
			Actor->GetStaticMeshComponent()->SetStaticMesh(Asset);
			Actor->SetActorScale3D(Scale);
		}
		return Actor;
	}
}

ADCCSnickTapeDirector::ADCCSnickTapeDirector()
{
	PrimaryActorTick.bCanEverTick = true;
	PrimaryActorTick.TickGroup = TG_PostPhysics;
	SetReplicates(false);
}

FVector ADCCSnickTapeDirector::Lodge(const TArray<TSharedPtr<FJsonValue>>* Values) const
{
	return LodgeOrigin + Vec(Values);
}

FVector ADCCSnickTapeDirector::Snick(const TArray<TSharedPtr<FJsonValue>>* Values) const
{
	return SnickSpot + Vec(Values);
}

bool ADCCSnickTapeDirector::Load()
{
	Timeline = ReadJson(TEXT("timeline.json"));
	Layout = ReadJson(TEXT("layout.json"));
	if (!Timeline.IsValid() || !Layout.IsValid())
	{
		UE_LOG(LogTemp, Warning, TEXT("[DCC SnickTape] needs Saved/SnickTape/timeline.json and layout.json (Tools/snick_tape.py)"));
		return false;
	}
	LodgeOrigin = Vec(Arr(Layout, TEXT("lodge_origin")), FVector(0, 150000, 0));
	SnickOrigin = Vec(Arr(Layout, TEXT("snick_origin")), FVector(0, -150000, 0));
	return true;
}

ADCCCharacterBase* ADCCSnickTapeDirector::SpawnCrawler(const FName& Id, const FVector& At, float Yaw)
{
	UWorld* World = GetWorld();
	const AGameModeBase* Mode = World->GetAuthGameMode();
	UClass* PawnClass = Mode && Mode->DefaultPawnClass ? Mode->DefaultPawnClass.Get() : nullptr;
	if (!PawnClass)
	{
		return nullptr;
	}
	FActorSpawnParameters Params;
	Params.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
	ADCCCharacterBase* Who = World->SpawnActor<ADCCCharacterBase>(PawnClass, At, FRotator(0, Yaw, 0), Params);
	if (!Who)
	{
		return nullptr;
	}
	Who->SetReplicates(false);
	Who->DisplayName = FText::FromName(Id);
	if (Who->Nameplate) { Who->Nameplate->SetVisibility(false); }
	if (UCapsuleComponent* Capsule = Who->GetCapsuleComponent()) { Capsule->SetCollisionEnabled(ECollisionEnabled::NoCollision); }
	if (UCharacterMovementComponent* Movement = Who->GetCharacterMovement()) { Movement->DisableMovement(); Movement->GravityScale = 0.0f; }
	if (Who->Onboarding)
	{
		Who->Onboarding->CrawlerId = Id;
		Who->Onboarding->ApplyCrawlerLook();
	}
	if (USkeletalMeshComponent* Body = Who->GetMesh()) { Body->SetAnimationMode(EAnimationMode::AnimationSingleNode); }
	SetClip(Who, TEXT("Idle"));
	return Who;
}

void ADCCSnickTapeDirector::SetClip(ADCCCharacterBase* Who, const FString& Clip)
{
	if (!Who)
	{
		return;
	}
	const FString Name = Clip == TEXT("Idle") ? FString(TEXT("GS_Idle")) : Clip;
	const FString Path = FString::Printf(TEXT("/Game/DCC/Characters/Shared/AnimsFixed/A_DCC_%s.A_DCC_%s"), *Name, *Name);
	UAnimSequence* Seq = LoadObject<UAnimSequence>(nullptr, *Path);
	if (Seq && Who->IdleAnimation != Seq)
	{
		Who->IdleAnimation = Seq;
		Who->WalkAnimation = Seq;
		Who->RunAnimation = Seq;
		Who->RestartLoopAnimation();
	}
}

void ADCCSnickTapeDirector::Setup()
{
	UWorld* World = GetWorld();
	APlayerController* PC = World->GetFirstPlayerController();
	if (!PC || !Load())
	{
		bDone = true;
		return;
	}
	ADCCHUD::bTrailerMinimal = true;
	if (APawn* Player = PC->GetPawn())
	{
		Player->SetActorHiddenInGame(true);
	}

	// The Lodge, a lid on it (the set has no ceiling and the world's sun would pour in), and Hiss.
	const double LodgeScale = Num(Layout, TEXT("lodge_scale"), 1.0);
	Mesh(World, TEXT("/Game/DCC/Sets/BlackLodge/SM_BlackLodge/StaticMeshes/SM_BlackLodge.SM_BlackLodge"), LodgeOrigin,
		FRotator(0, Num(Layout, TEXT("lodge_yaw"), 0.0), 0), FVector(LodgeScale));
	if (AStaticMeshActor* Lid = Mesh(World, TEXT("/Engine/BasicShapes/Cube.Cube"), LodgeOrigin + FVector(0, 0, Num(Layout, TEXT("lid_z"), 680.0)),
		FRotator::ZeroRotator, FVector(22.0, 22.0, 0.3)))
	{
		UMaterialInterface* Base = LoadObject<UMaterialInterface>(nullptr, TEXT("/Engine/BasicShapes/BasicShapeMaterial.BasicShapeMaterial"));
		if (UMaterialInstanceDynamic* Black = Base ? UMaterialInstanceDynamic::Create(Base, Lid) : nullptr)
		{
			Black->SetVectorParameterValue(TEXT("Color"), FLinearColor(0.01f, 0.005f, 0.005f));
			Lid->GetStaticMeshComponent()->SetMaterial(0, Black);
		}
	}
	const TSharedPtr<FJsonObject> HissSpec = Obj(Layout, TEXT("hiss"));
	Hiss = Mesh(World, TEXT("/Game/DCC/Characters/Hiss/SM_HissLizard/StaticMeshes/SM_HissLizard.SM_HissLizard"), Lodge(Arr(HissSpec, TEXT("at"))),
		FRotator(0, Num(HissSpec, TEXT("yaw"), 0.0), 0), FVector(Num(HissSpec, TEXT("scale"), 1.0)));
	if (Hiss)
	{
		HissRest = Hiss->GetActorTransform();
	}

	const TSharedPtr<FJsonObject> CJSpec = Obj(Layout, TEXT("cj"));
	const TSharedPtr<FJsonObject> VaughnSpec = Obj(Layout, TEXT("vaughn"));
	CJ = SpawnCrawler(TEXT("CJ"), Lodge(Arr(CJSpec, TEXT("at"))) + FVector(0, 0, 92), Num(CJSpec, TEXT("yaw"), 0.0));
	Vaughn = SpawnCrawler(TEXT("Vaughn"), Lodge(Arr(VaughnSpec, TEXT("at"))) + FVector(0, 0, 92), Num(VaughnSpec, TEXT("yaw"), 0.0));

	// Lamps for the Lodge.
	const TArray<TSharedPtr<FJsonValue>>* Lights = Arr(Layout, TEXT("lights"));
	FActorSpawnParameters Params;
	Params.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
	for (int32 i = 0; Lights && i < Lights->Num(); ++i)
	{
		const TSharedPtr<FJsonObject> L = (*Lights)[i]->AsObject();
		if (APointLight* Lamp = World->SpawnActor<APointLight>(Lodge(Arr(L, TEXT("at"))), FRotator::ZeroRotator, Params))
		{
			UPointLightComponent* C = Lamp->PointLightComponent;
			C->SetMobility(EComponentMobility::Movable);
			const FVector Col = Vec(Arr(L, TEXT("color")), FVector(1, 1, 1));
			C->SetLightColor(FLinearColor(Col.X, Col.Y, Col.Z));
			C->SetIntensity(Num(L, TEXT("intensity"), 5000.0));
			C->SetAttenuationRadius(Num(L, TEXT("radius"), 1200.0));
		}
	}

	// The sun, low and gold, for the tape (the Lodge has its lid).
	if (const TSharedPtr<FJsonObject> Sun = Obj(Layout, TEXT("sun")))
	{
		for (TActorIterator<ADirectionalLight> It(World); It; ++It)
		{
			It->SetActorRotation(FRotator(Num(Sun, TEXT("pitch"), -12.0), Num(Sun, TEXT("yaw"), 40.0), 0));
			if (UDirectionalLightComponent* C = Cast<UDirectionalLightComponent>(It->GetLightComponent()))
			{
				const FVector Col = Vec(Arr(Sun, TEXT("color")), FVector(1.0, 0.75, 0.5));
				C->SetLightColor(FLinearColor(Col.X, Col.Y, Col.Z));
				C->SetIntensity(Num(Sun, TEXT("intensity"), 6.0));
			}
		}
	}

	// The tape's set. Its spot is found once it is loaded: the ground under a point near its middle.
	DCCSets::FOptions Options;
	TWeakObjectPtr<ADCCSnickTapeDirector> Self(this);
	const FString SetName = Layout->HasField(TEXT("snick_set")) ? Layout->GetStringField(TEXT("snick_set")) : FString(TEXT("Elderboom"));
	DCCSets::Load(World, SetName, SnickOrigin, FRotator::ZeroRotator, TEXT("DCCSnickSet"), Options, [Self](const FBox& Bounds)
	{
		ADCCSnickTapeDirector* D = Self.Get();
		if (!D)
		{
			return;
		}
		const FVector Off = Vec(Arr(D->Layout, TEXT("snick_spot_offset")));
		const FVector P = Bounds.GetCenter() + FVector(Off.X, Off.Y, 0);
		double Ground = Bounds.Min.Z;
		double Top = Bounds.Max.Z + 100.0;
		for (int32 i = 0; i < 10; ++i)
		{
			FHitResult Hit;
			if (!D->GetWorld()->LineTraceSingleByChannel(Hit, FVector(P.X, P.Y, Top), FVector(P.X, P.Y, Bounds.Min.Z - 200.0), ECC_Visibility))
			{
				break;
			}
			Ground = Hit.ImpactPoint.Z;
			Top = Ground - 8.0;
		}
		D->SnickSpot = FVector(P.X, P.Y, Ground);
		// The tape's own lights (golden hour on the two of them), now that the spot is known.
		const TArray<TSharedPtr<FJsonValue>>* SnickLights = Arr(D->Layout, TEXT("snick_lights"));
		FActorSpawnParameters LightParams;
		LightParams.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
		for (int32 i = 0; SnickLights && i < SnickLights->Num(); ++i)
		{
			const TSharedPtr<FJsonObject> L = (*SnickLights)[i]->AsObject();
			if (APointLight* Lamp = D->GetWorld()->SpawnActor<APointLight>(D->Snick(Arr(L, TEXT("at"))), FRotator::ZeroRotator, LightParams))
			{
				UPointLightComponent* C = Lamp->PointLightComponent;
				C->SetMobility(EComponentMobility::Movable);
				const FVector Col = Vec(Arr(L, TEXT("color")), FVector(1, 1, 1));
				C->SetLightColor(FLinearColor(Col.X, Col.Y, Col.Z));
				C->SetIntensity(Num(L, TEXT("intensity"), 5000.0));
				C->SetAttenuationRadius(Num(L, TEXT("radius"), 1200.0));
			}
		}
		D->bSnickReady = true;
		UE_LOG(LogTemp, Log, TEXT("[DCC SnickTape] tape set ready, spot %s"), *D->SnickSpot.ToString());
	});

	Camera = World->SpawnActor<ACameraActor>(LodgeOrigin + FVector(0, 0, 300), FRotator::ZeroRotator, Params);
	if (Camera)
	{
		Camera->GetCameraComponent()->bConstrainAspectRatio = false;
		// Exposure: auto unless the layout pins a bias (auto drifts when a shot cuts from the dark Lodge to the sun).
		FPostProcessSettings& PP = Camera->GetCameraComponent()->PostProcessSettings;
		if (Layout->HasField(TEXT("exposure_bias")))
		{
			PP.bOverride_AutoExposureBias = true;
			PP.AutoExposureBias = Num(Layout, TEXT("exposure_bias"), 0.0);
		}
		if (Layout->HasField(TEXT("exposure_speed")))
		{
			PP.bOverride_AutoExposureSpeedUp = PP.bOverride_AutoExposureSpeedDown = true;
			PP.AutoExposureSpeedUp = PP.AutoExposureSpeedDown = Num(Layout, TEXT("exposure_speed"), 3.0);
		}
		// A little film: grain, a soft vignette, a warm grade.
		PP.bOverride_VignetteIntensity = true;
		PP.VignetteIntensity = Num(Layout, TEXT("vignette"), 0.45);
		PP.bOverride_FilmGrainIntensity = true;
		PP.FilmGrainIntensity = Num(Layout, TEXT("grain"), 0.15);
		PC->SetViewTarget(Camera);
	}
	bSetUp = true;
	UE_LOG(LogTemp, Log, TEXT("[DCC SnickTape] set up"));
}

void ADCCSnickTapeDirector::PlaceForSnick(bool bSnick)
{
	if (bSnick == bInSnick)
	{
		return;
	}
	bInSnick = bSnick;
	const TSharedPtr<FJsonObject> CJSpec = Obj(Layout, bSnick ? TEXT("snick_cj") : TEXT("cj"));
	const TSharedPtr<FJsonObject> VaughnSpec = Obj(Layout, bSnick ? TEXT("snick_vaughn") : TEXT("vaughn"));
	auto Put = [&](ADCCCharacterBase* Who, const TSharedPtr<FJsonObject>& Spec)
	{
		if (Who)
		{
			const FVector At = (bSnick ? Snick(Arr(Spec, TEXT("at"))) : Lodge(Arr(Spec, TEXT("at")))) + FVector(0, 0, 92);
			Who->SetActorLocationAndRotation(At, FRotator(0, Num(Spec, TEXT("yaw"), 0.0), 0), false, nullptr, ETeleportType::TeleportPhysics);
		}
	};
	Put(CJ, CJSpec);
	Put(Vaughn, VaughnSpec);
}

void ADCCSnickTapeDirector::ApplyShot(const FString& Shot, float Alpha)
{
	const TSharedPtr<FJsonObject> Spec = Obj(Obj(Layout, TEXT("shots")), *Shot);
	if (!Spec || !Camera)
	{
		return;
	}
	const bool bSnick = Shot.StartsWith(TEXT("snick"));
	auto P = [&](const TCHAR* Key, const FVector& Fallback)
	{
		const TArray<TSharedPtr<FJsonValue>>* A = Arr(Spec, Key);
		return A ? (bSnick ? Snick(A) : Lodge(A)) : Fallback;
	};
	const FVector From0 = P(TEXT("from"), Camera->GetActorLocation());
	const FVector At0 = P(TEXT("at"), From0 + FVector(100, 0, 0));
	const FVector From = FMath::Lerp(From0, P(TEXT("to"), From0), Alpha);
	const FVector At = FMath::Lerp(At0, P(TEXT("at_to"), At0), Alpha);
	Camera->SetActorLocationAndRotation(From, (At - From).Rotation());
	Camera->GetCameraComponent()->SetFieldOfView(FMath::Lerp((float)Num(Spec, TEXT("fov"), 40.0), (float)Num(Spec, TEXT("fov_to"), Num(Spec, TEXT("fov"), 40.0)), Alpha));
}

void ADCCSnickTapeDirector::Act(float T)
{
	const TArray<TSharedPtr<FJsonValue>>* Entries = Arr(Timeline, TEXT("entries"));
	if (!Entries)
	{
		return;
	}
	bool bHissTalking = false;
	for (const TSharedPtr<FJsonValue>& V : *Entries)
	{
		const TSharedPtr<FJsonObject> E = V->AsObject();
		const double Start = Num(E, TEXT("start"), 0.0);
		const double End = Num(E, TEXT("end"), 0.0);
		if (T < Start || T >= End)
		{
			continue;
		}
		const FString Shot = E->GetStringField(TEXT("shot"));
		PlaceForSnick(Shot.StartsWith(TEXT("snick")));
		if (Shot != LastShot || Start != ShotStart)
		{
			LastShot = Shot;
			ShotStart = Start;
			SetClip(CJ, E->GetStringField(TEXT("cj")));
			SetClip(Vaughn, E->GetStringField(TEXT("vaughn")));
		}
		ApplyShot(Shot, FMath::Clamp((T - Start) / FMath::Max(0.01, End - Start), 0.0, 1.0));
		FString Speaker;
		if (E->TryGetStringField(TEXT("speaker"), Speaker) && Speaker == TEXT("Hiss"))
		{
			const double VoiceAt = Num(E, TEXT("voice_at"), Start);
			bHissTalking = T >= VoiceAt && T < End - 0.4;
		}
		break;
	}
	// Hiss lives a little: breathing always, a nod and a sway while he talks.
	if (Hiss)
	{
		const float Nod = bHissTalking ? 3.5f * FMath::Sin(T * 6.3f) + 1.5f * FMath::Sin(T * 2.1f) : 0.6f * FMath::Sin(T * 0.9f);
		const float Sway = bHissTalking ? 4.0f * FMath::Sin(T * 1.6f) : 1.5f * FMath::Sin(T * 0.5f);
		FTransform Now = HissRest;
		Now.SetRotation(HissRest.GetRotation() * FRotator(Nod, Sway, 0.0f).Quaternion());
		const FVector S = HissRest.GetScale3D();
		Now.SetScale3D(FVector(S.X, S.Y, S.Z * (1.0f + 0.008f * FMath::Sin(T * 1.8f))));
		Hiss->SetActorTransform(Now);
	}
}

void ADCCSnickTapeDirector::Tick(float DeltaSeconds)
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
	// Wait for the tape's set, then a few seconds for textures to stream in, before the first frame.
	if (ReadyAt < 0.0f)
	{
		if (bSnickReady)
		{
			ReadyAt = Elapsed + Num(Layout, TEXT("settle"), 6.0);
		}
		return;
	}
	const float T = StartAt + FMath::Max(0.0f, Elapsed - ReadyAt);
	Act(T);
	APlayerController* PC = GetWorld()->GetFirstPlayerController();
	if (PC && Camera && PC->GetViewTarget() != Camera)
	{
		PC->SetViewTarget(Camera);
	}
	if (Elapsed < ReadyAt)
	{
		return;
	}
	if (bCapture)
	{
		FScreenshotRequest::RequestScreenshot(FString::Printf(TEXT("%s/frame_%05d.png"), *CaptureDir, Frame++), /*bShowUI=*/false, /*bAddFilenameSuffix=*/false);
	}
	if (T >= Num(Timeline, TEXT("length"), 60.0))
	{
		bDone = true;
		ADCCHUD::bTrailerMinimal = false;
		UE_LOG(LogTemp, Log, TEXT("[DCC SnickTape] done, %d frames"), Frame);
		if (bCapture && PC)
		{
			PC->ConsoleCommand(TEXT("quit"));
		}
	}
	else if (FMath::Fmod(T, 1.0f) < DeltaSeconds)
	{
		UE_LOG(LogTemp, Log, TEXT("[DCC SnickTape] t=%.0f shot %s"), T, *LastShot);
	}
}

static void DCCSnickTapeCommand(const TArray<FString>& Args, UWorld* World)
{
	if (!World)
	{
		return;
	}
	FActorSpawnParameters Params;
	Params.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
	ADCCSnickTapeDirector* Director = World->SpawnActor<ADCCSnickTapeDirector>(FVector::ZeroVector, FRotator::ZeroRotator, Params);
	if (!Director)
	{
		return;
	}
	if (Args.Num() > 0 && Args[0].Equals(TEXT("capture"), ESearchCase::IgnoreCase))
	{
		Director->bCapture = true;
		Director->CaptureDir = Args.Num() > 1 ? Args[1] : FString(TEXT("D:/Builds/SnickTapeFrames"));
	}
	else if (Args.Num() > 1 && Args[0].Equals(TEXT("at"), ESearchCase::IgnoreCase))
	{
		Director->StartAt = FCString::Atof(*Args[1]);
	}
	UE_LOG(LogTemp, Log, TEXT("[DCC SnickTape] rolling%s"), Director->bCapture ? TEXT(", capturing frames") : TEXT(""));
}

static FAutoConsoleCommandWithWorldAndArgs GSnickTapeCommand(
	TEXT("DCC.SnickTape"),
	TEXT("DCC.SnickTape [capture <dir> | at <seconds>]: the PRIMETIME snick tape short (Tools/snick_tape.py renders it)."),
	FConsoleCommandWithWorldAndArgsDelegate::CreateStatic(&DCCSnickTapeCommand));
