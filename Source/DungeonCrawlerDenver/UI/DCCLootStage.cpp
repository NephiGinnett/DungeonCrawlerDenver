#include "UI/DCCLootStage.h"

#include "Components/PointLightComponent.h"
#include "Components/SceneCaptureComponent2D.h"
#include "Components/StaticMeshComponent.h"
#include "Engine/StaticMesh.h"
#include "Engine/TextureRenderTarget2D.h"
#include "Kismet/KismetRenderingLibrary.h"
#include "Materials/MaterialInstanceDynamic.h"
#include "HAL/IConsoleManager.h"
#include "EngineUtils.h"
#include "Misc/Paths.h"
#include "TimerManager.h"

namespace
{
	// Each set has its own corner of the stage, far enough apart that one's lights never touch another.
	const FVector IdleSpot(0.0f, 0.0f, 0.0f);
	const FVector RevealSpot(0.0f, 2000.0f, 0.0f);
	FVector BoothSpot(int32 Slot) { return FVector(0.0f, 4000.0f + Slot * 2000.0f, 0.0f); }

	UStaticMesh* LoadMesh(const TCHAR* Path) { return LoadObject<UStaticMesh>(nullptr, Path); }
	const TCHAR* ChestPath = TEXT("/Game/UnrealBucket/Library/Various/Chest/SM_Chest.SM_Chest");
	const TCHAR* ChestOpenPath = TEXT("/Game/UnrealBucket/Library/Various/Chest/SM_Chest_Open.SM_Chest_Open");

	// The studio cameras' fixed exposure. Too high washes every picture to white, too low leaves it black.
	TAutoConsoleVariable<float> CVarLootExposure(TEXT("DCC.LootExposure"), 10.0f,
		TEXT("Exposure bias of the loot box screen's chest and item pictures."));
	TAutoConsoleVariable<int32> CVarLootSource(TEXT("DCC.LootSource"), 0,
		TEXT("(dev) What the loot studio cameras record: 0 the finished picture, 1 plain colours (no lighting)."));
}

// DCC.LootDump [delay]: (dev) saves what each loot studio camera sees to Saved/LootDump, for checking its pictures.
static FAutoConsoleCommandWithWorldAndArgs GLootDumpCommand(TEXT("DCC.LootDump"), TEXT("DCC.LootDump [delay] [name suffix] [exposure]: (dev) save the loot studio pictures."),
	FConsoleCommandWithWorldAndArgsDelegate::CreateLambda([](const TArray<FString>& Args, UWorld* World)
	{
		TWeakObjectPtr<UWorld> Weak(World);
		const FString Tag = Args.Num() > 1 ? Args[1] : FString();
		auto Dump = [Weak, Tag]()
		{
			if (!Weak.IsValid()) { return; }
			const FString Dir = FPaths::ProjectSavedDir() / TEXT("LootDump");
			for (TActorIterator<ADCCLootStage> It(Weak.Get()); It; ++It)
			{
				TArray<TPair<FString, UTextureRenderTarget2D*>> All = { { TEXT("Idle"), It->IdleImage() }, { TEXT("Reveal"), It->RevealImage() } };
				for (int32 i = 0; i < ADCCLootStage::BoothCount; ++i) { All.Add({ FString::Printf(TEXT("Booth%d"), i), It->BoothImage(i) }); }
				for (const auto& Pair : All)
				{
					if (Pair.Value) { UKismetRenderingLibrary::ExportRenderTarget(Weak.Get(), Pair.Value, Dir, Pair.Key + Tag + TEXT(".png")); }
				}
				UE_LOG(LogTemp, Log, TEXT("[DCC LootStage] dumped to %s"), *Dir);
			}
		};
		const float Delay = Args.Num() > 0 ? FCString::Atof(*Args[0]) : 0.0f;
		if (!World) { return; }
		// An optional exposure to try first: set it, give the cameras a moment, then save.
		const bool bExposure = Args.Num() > 2;
		const float Exposure = bExposure ? FCString::Atof(*Args[2]) : 0.0f;
		FTimerHandle Handle;
		World->GetTimerManager().SetTimer(Handle, FTimerDelegate::CreateLambda([Weak, Dump, bExposure, Exposure]()
		{
			if (bExposure) { CVarLootExposure->Set(Exposure, ECVF_SetByConsole); }
			if (Weak.IsValid())
			{
				FTimerHandle Later;
				Weak->GetTimerManager().SetTimer(Later, FTimerDelegate::CreateLambda(Dump), 0.5f, false);
			}
		}), FMath::Max(Delay, 0.01f), false);
	}));

ADCCLootStage::ADCCLootStage()
{
	PrimaryActorTick.bCanEverTick = true;
	bReplicates = false;
	Root = CreateDefaultSubobject<USceneComponent>(TEXT("Root"));
	RootComponent = Root;
	Root->SetMobility(EComponentMobility::Movable);
}

USceneCaptureComponent2D* ADCCLootStage::MakeCamera(const TCHAR* Name, UTextureRenderTarget2D*& OutTarget, int32 Size)
{
	OutTarget = UKismetRenderingLibrary::CreateRenderTarget2D(this, Size, Size, RTF_RGBA8_SRGB, FLinearColor(0.0f, 0.0f, 0.0f, 1.0f));
	USceneCaptureComponent2D* Camera = NewObject<USceneCaptureComponent2D>(this, Name);
	Camera->SetupAttachment(Root);
	Camera->TextureTarget = OutTarget;
	Camera->CaptureSource = SCS_FinalColorLDR;
	Camera->FOVAngle = 30.0f;
	Camera->bCaptureEveryFrame = false;
	Camera->bCaptureOnMovement = false;
	// Only its own subject and backdrop (Frame), never the world it hangs under or the other booths.
	Camera->PrimitiveRenderMode = ESceneCapturePrimitiveRenderMode::PRM_UseShowOnlyList;
	Camera->ShowFlags.SetFog(false);
	// None of the world's light either: its purple sky and bounce light drowned the studio lamps (2026-11-16).
	Camera->ShowFlags.SetSkyLighting(false);
	Camera->ShowFlags.SetGlobalIllumination(false);
	Camera->ShowFlags.SetLumenGlobalIllumination(false);
	Camera->ShowFlags.SetLumenReflections(false);
	Camera->ShowFlags.SetAmbientOcclusion(false);
	Camera->ShowFlags.SetScreenSpaceReflections(false);
	Camera->ShowFlags.SetGrain(false);
	Camera->ShowFlags.SetVignette(false);
	Camera->ShowFlags.SetEyeAdaptation(false);
	Camera->ShowFlags.SetAtmosphere(false);
	Camera->ShowFlags.SetVolumetricFog(false);
	Camera->ShowFlags.SetMotionBlur(false);
	Camera->ShowFlags.SetBloom(true);
	// A fixed exposure, so a bright burst reads as bright rather than being evened out.
	Camera->PostProcessSettings.bOverride_AutoExposureMethod = true;
	Camera->PostProcessSettings.AutoExposureMethod = EAutoExposureMethod::AEM_Manual;
	Camera->PostProcessSettings.bOverride_AutoExposureBias = true;
	Camera->PostProcessSettings.AutoExposureBias = CVarLootExposure.GetValueOnGameThread();
	Camera->PostProcessBlendWeight = 1.0f;
	Camera->RegisterComponent();
	// A dark backdrop behind and under whatever it looks at, so the picture's background is black (the HUD adds it
	// over a dark screen) and never the world's sky.
	if (UStaticMesh* Plane = LoadObject<UStaticMesh>(nullptr, TEXT("/Engine/BasicShapes/Plane.Plane")))
	{
		UMaterialInterface* Shape = LoadObject<UMaterialInterface>(nullptr, TEXT("/Engine/BasicShapes/BasicShapeMaterial.BasicShapeMaterial"));
		UMaterialInstanceDynamic* Black = Shape ? UMaterialInstanceDynamic::Create(Shape, this) : nullptr;
		if (Black)
		{
			Black->SetVectorParameterValue(TEXT("Color"), FLinearColor::Black);
		}
		for (int32 k = 0; k < 1; ++k) // a wall only: a floor caught the glow and showed as a hard pink square
		{
			UStaticMeshComponent* Back = NewObject<UStaticMeshComponent>(this);
			Back->SetupAttachment(Root);
			Back->SetMobility(EComponentMobility::Movable);
			Back->SetCollisionEnabled(ECollisionEnabled::NoCollision);
			Back->SetStaticMesh(Plane);
			Back->SetMaterial(0, Black);
			// Lit by nothing (the lamps are on channel 0 only), so it stays black when the chest glows; without it the
			// sky shows through.
			Back->SetLightingChannels(false, true, false);
			Back->SetWorldScale3D(FVector(40.0f));
			Back->RegisterComponent();
			Backdrops.Add({ Camera, Back, k == 0 });
		}
	}
	return Camera;
}

void ADCCLootStage::Frame(USceneCaptureComponent2D* Camera, UStaticMeshComponent* Subject, float Margin, UPointLightComponent* Key) const
{
	if (!Camera || !Subject || !Subject->GetStaticMesh())
	{
		return;
	}
	// From the front, a little above, far enough back that the whole thing fits with some air round it.
	const FBoxSphereBounds Bounds = Subject->Bounds;
	const float Radius = FMath::Max(Bounds.SphereRadius, 1.0f) * Margin;
	const float Distance = Radius / FMath::Tan(FMath::DegreesToRadians(Camera->FOVAngle * 0.5f));
	const FVector Look = FVector(-1.0f, 0.0f, 0.42f).GetSafeNormal();
	const FVector At = Bounds.Origin + Look * Distance;
	Camera->SetWorldLocationAndRotation(At, (Bounds.Origin - At).Rotation());
	UE_LOG(LogTemp, Log, TEXT("[DCC LootStage] %s frames %s mesh %s bounds %s r=%.1f cam %s"), *Camera->GetName(), *Subject->GetName(),
		*GetNameSafe(Subject->GetStaticMesh()), *Bounds.Origin.ToString(), Bounds.SphereRadius, *At.ToString());
	// The key light: up and to the left of the camera, as far from the subject as the camera is, so every picture is
	// lit the same however big the thing in it is.
	if (Key)
	{
		const FVector Side = FVector::CrossProduct(FVector::UpVector, Look).GetSafeNormal();
		Key->SetWorldLocation(Bounds.Origin + (Look + Side * 0.8f + FVector(0.0f, 0.0f, 0.6f)).GetSafeNormal() * Distance);
		Key->SetAttenuationRadius(Distance * 3.0f);
		Key->SetIntensity(100.0f * FMath::Square(Distance / 270.0f));
	}
	Camera->ClearShowOnlyComponents();
	Camera->ShowOnlyComponent(Subject);
	// Its backdrop: a wall well behind the subject, facing the camera, and a floor under it.
	for (const FBackdrop& B : Backdrops)
	{
		if (B.Camera == Camera && B.Plane)
		{
			Camera->ShowOnlyComponent(B.Plane);
			B.Plane->SetWorldScale3D(FVector(Radius * 14.0f / 100.0f)); // the plane is 1 m across
			if (B.bWall)
			{
				const FVector Away = (Bounds.Origin - At).GetSafeNormal2D();
				B.Plane->SetWorldLocationAndRotation(Bounds.Origin + Away * (Radius * 6.0f), FRotationMatrix::MakeFromZ(-Away).Rotator());
			}
			else
			{
				B.Plane->SetWorldLocationAndRotation(Bounds.Origin - FVector(0.0f, 0.0f, Bounds.BoxExtent.Z + 1.0f), FRotator::ZeroRotator);
			}
		}
	}
}

void ADCCLootStage::SetActive(bool bInActive)
{
	bActive = bInActive;
	if (!IdleChest)
	{
		// Built the first time it is needed.
		auto Mesh = [this](const TCHAR* Name, UStaticMesh* Asset, const FVector& At) -> UStaticMeshComponent*
		{
			UStaticMeshComponent* Component = NewObject<UStaticMeshComponent>(this, Name);
			Component->SetupAttachment(Root);
			Component->SetMobility(EComponentMobility::Movable);
			Component->SetCollisionEnabled(ECollisionEnabled::NoCollision);
			Component->SetStaticMesh(Asset);
			Component->SetRelativeLocation(At);
			Component->SetRelativeRotation(FRotator(0.0f, 180.0f, 0.0f)); // the chest's front toward the camera
			Component->RegisterComponent();
			return Component;
		};
		auto Lamp = [this](const TCHAR* Name, const FVector& At, float Intensity, const FLinearColor& Colour) -> UPointLightComponent*
		{
			UPointLightComponent* Light = NewObject<UPointLightComponent>(this, Name);
			Light->SetupAttachment(Root);
			Light->SetRelativeLocation(At);
			Light->SetIntensityUnits(ELightUnits::Candelas);
			Light->SetIntensity(Intensity);
			Light->SetLightColor(Colour);
			Light->SetAttenuationRadius(900.0f);
			Light->SetCastShadows(false);
			Light->RegisterComponent();
			return Light;
		};
		IdleChest = Mesh(TEXT("IdleChest"), LoadMesh(ChestPath), IdleSpot);
		IdleLight = Lamp(TEXT("IdleLight"), IdleSpot + FVector(-220.0f, -160.0f, 260.0f), 100.0f, FLinearColor(1.0f, 0.92f, 0.82f));
		UTextureRenderTarget2D* NewIdle = nullptr;
		IdleCamera = MakeCamera(TEXT("IdleCamera"), NewIdle, 384);
		IdleTarget = NewIdle;
		Frame(IdleCamera, IdleChest, 1.15f, IdleLight);

		RevealChest = Mesh(TEXT("RevealChest"), LoadMesh(ChestPath), RevealSpot);
		RevealChestOpen = Mesh(TEXT("RevealChestOpen"), LoadMesh(ChestOpenPath), RevealSpot);
		RevealChestOpen->SetVisibility(false);
		RevealChestHome = RevealSpot;
		RevealKey = Lamp(TEXT("RevealKey"), RevealSpot + FVector(-220.0f, -160.0f, 260.0f), 100.0f, FLinearColor(1.0f, 0.92f, 0.82f));
		RevealGlow = Lamp(TEXT("RevealGlow"), RevealSpot + FVector(-40.0f, 0.0f, 70.0f), 0.0f, FLinearColor::White);
		UTextureRenderTarget2D* NewReveal = nullptr;
		RevealCamera = MakeCamera(TEXT("RevealCamera"), NewReveal, 640);
		RevealTarget = NewReveal;
		Frame(RevealCamera, RevealChest, 1.45f, RevealKey); // room to shake
		if (RevealChestOpen)
		{
			RevealCamera->ShowOnlyComponent(RevealChestOpen); // what it bursts into
		}

		for (int32 i = 0; i < BoothCount; ++i)
		{
			UStaticMeshComponent* Booth = Mesh(*FString::Printf(TEXT("Booth%d"), i), nullptr, BoothSpot(i));
			BoothMeshes.Add(Booth);
			BoothLights.Add(Lamp(*FString::Printf(TEXT("BoothLight%d"), i), BoothSpot(i) + FVector(-200.0f, -150.0f, 220.0f), 100.0f, FLinearColor(1.0f, 0.95f, 0.9f)));
			UTextureRenderTarget2D* Target = nullptr;
			BoothCameras.Add(MakeCamera(*FString::Printf(TEXT("BoothCamera%d"), i), Target, 256));
			BoothTargets.Add(Target);
		}
	}
	// The cameras draw every frame while the screen is open, and not at all otherwise.
	IdleCamera->bCaptureEveryFrame = bActive;
	RevealCamera->bCaptureEveryFrame = bActive;
	for (int32 i = 0; i < BoothCameras.Num(); ++i)
	{
		// An empty booth has nothing to photograph, so its camera stays off.
		BoothCameras[i]->bCaptureEveryFrame = bActive && BoothMeshes.IsValidIndex(i) && BoothMeshes[i]->GetStaticMesh();
	}
	SetActorTickEnabled(bActive);
}

void ADCCLootStage::Tick(float DeltaSeconds)
{
	Super::Tick(DeltaSeconds);
	// Follow the exposure setting, so it can be tuned while looking at it.
	const float Bias = CVarLootExposure.GetValueOnGameThread();
	const ESceneCaptureSource Source = CVarLootSource.GetValueOnGameThread() == 1 ? SCS_BaseColor : SCS_FinalColorLDR;
	for (USceneCaptureComponent2D* Camera : TArray<USceneCaptureComponent2D*>{ IdleCamera, RevealCamera })
	{
		if (Camera) { Camera->PostProcessSettings.AutoExposureBias = Bias; Camera->CaptureSource = Source; }
	}
	for (USceneCaptureComponent2D* Camera : BoothCameras)
	{
		if (Camera) { Camera->PostProcessSettings.AutoExposureBias = Bias; Camera->CaptureSource = Source; }
	}
	// The chests in the row turn slowly; so do the items, so you see them from more than one side.
	IdleSpin += DeltaSeconds * 25.0f;
	if (IdleChest)
	{
		IdleChest->SetRelativeRotation(FRotator(0.0f, 180.0f + FMath::Sin(FMath::DegreesToRadians(IdleSpin)) * 28.0f, 0.0f));
	}
	for (UStaticMeshComponent* Booth : BoothMeshes)
	{
		if (Booth && Booth->GetStaticMesh())
		{
			Booth->SetRelativeRotation(FRotator(0.0f, 180.0f + IdleSpin * 1.6f, 0.0f));
		}
	}
}

void ADCCLootStage::PoseReveal(float T, float BurstAt, const FLinearColor& Colour)
{
	if (!RevealChest || !RevealChestOpen || !RevealGlow)
	{
		return;
	}
	if (T < BurstAt)
	{
		// Shaking itself apart: small at first, wild at the end, hopping, and glowing brighter and brighter.
		const float U = FMath::Clamp(T / BurstAt, 0.0f, 1.0f);
		const float Shake = 1.5f + U * U * 11.0f;
		const FRotator Jitter(FMath::Sin(T * 53.0f) * Shake * 0.5f, 180.0f + FMath::Sin(T * 41.0f) * Shake, FMath::Cos(T * 67.0f) * Shake);
		const float Hop = FMath::Abs(FMath::Sin(T * 31.0f)) * U * U * 9.0f;
		RevealChest->SetVisibility(true);
		RevealChestOpen->SetVisibility(false);
		RevealChest->SetRelativeLocationAndRotation(RevealChestHome + FVector(0.0f, 0.0f, Hop), Jitter);
		RevealChest->SetRelativeScale3D(FVector(1.0f + U * 0.06f));
		RevealGlow->SetLightColor(Colour);
		RevealGlow->SetIntensity(FMath::Square(U) * 160.0f);
		return;
	}
	// Burst: the lid is off, the light pours out, and it settles.
	const float B = T - BurstAt;
	RevealChest->SetVisibility(false);
	RevealChestOpen->SetVisibility(true);
	const float Bounce = FMath::Max(0.0f, 1.0f - B * 3.0f);
	RevealChestOpen->SetRelativeLocationAndRotation(RevealChestHome + FVector(0.0f, 0.0f, Bounce * 14.0f), FRotator(0.0f, 180.0f, 0.0f));
	RevealChestOpen->SetRelativeScale3D(FVector(1.0f + Bounce * 0.1f));
	RevealGlow->SetLightColor(Colour);
	RevealGlow->SetIntensity(FMath::Lerp(40.0f, 400.0f, FMath::Clamp(1.0f - B * 1.5f, 0.0f, 1.0f)));
}

UTextureRenderTarget2D* ADCCLootStage::Photo(int32 Slot, UStaticMesh* Mesh)
{
	if (!BoothMeshes.IsValidIndex(Slot))
	{
		return nullptr;
	}
	UStaticMeshComponent* Booth = BoothMeshes[Slot];
	Booth->SetStaticMesh(Mesh);
	Booth->SetRelativeRotation(FRotator(0.0f, 180.0f, 0.0f));
	BoothCameras[Slot]->bCaptureEveryFrame = bActive && Mesh;
	if (!Mesh)
	{
		return nullptr;
	}
	Booth->UpdateBounds();
	// Centred in its booth whatever its pivot, then framed to fill the picture.
	Booth->SetRelativeLocation(BoothSpot(Slot) - (Mesh->GetBounds().Origin));
	Booth->UpdateBounds();
	Frame(BoothCameras[Slot], Booth, 1.05f, BoothLights[Slot]);
	return BoothTargets[Slot];
}
