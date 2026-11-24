// DCC.MakeItemIcons [overwrite 0/1] [delay] (2026-11-23, Zoe: "Can you make the icon art just a smaller flattened version
// of their item image?").
//
// Renders every gear row's own mesh (GroundMesh, else Mesh) into a 256x256 picture with a clear background and saves it
// as Content/DCC/UI/ItemIcons/<ItemId>.png, which DCCMenu::IconTexture loads when no hand-made T_Icon_<Id> exists. Run it
// in any test game (it needs a crawler for the equipment table):
//     python Tools/run_test.py DCCLobbyGameMode "DCC.MakeItemIcons 0 8" 30 1 1 icons
// Each item is lit by a key, a fill and a rim light far above the level, seen three-quarters from the front, framed to
// its bounds, so a knife and a sledgehammer both fill the square.

#include "Characters/DCCCharacterBase.h"
#include "Components/PointLightComponent.h"
#include "Components/SceneCaptureComponent2D.h"
#include "Components/StaticMeshComponent.h"
#include "Engine/DataTable.h"
#include "Engine/PointLight.h"
#include "Engine/SceneCapture2D.h"
#include "Engine/StaticMesh.h"
#include "Engine/StaticMeshActor.h"
#include "Engine/TextureRenderTarget2D.h"
#include "Engine/World.h"
#include "Materials/MaterialInterface.h"
#include "Equipment/DCCEquipmentComponent.h"
#include "Equipment/DCCEquipmentTypes.h"
#include "GameFramework/PlayerController.h"
#include "HAL/FileManager.h"
#include "HAL/IConsoleManager.h"
#include "ImageUtils.h"
#include "Misc/FileHelper.h"
#include "Misc/Paths.h"
#include "TimerManager.h"
#include "TextureResource.h"

namespace
{
	constexpr int32 IconSize = 256;

	void MakeIcons(UWorld* World, bool bOverwrite)
	{
		APlayerController* PC = World ? World->GetFirstPlayerController() : nullptr;
		const ADCCCharacterBase* Crawler = PC ? Cast<ADCCCharacterBase>(PC->GetPawn()) : nullptr;
		UDataTable* Table = Crawler && Crawler->Equipment ? Crawler->Equipment->EquipmentTable.Get() : nullptr;
		if (!Table)
		{
			UE_LOG(LogTemp, Warning, TEXT("[DCC Icons] no crawler or equipment table"));
			return;
		}
		const FString Folder = FPaths::ProjectContentDir() / TEXT("DCC/UI/ItemIcons");
		IFileManager::Get().MakeDirectory(*Folder, true);

		// The studio: far above everything, so nothing else is in the light or the frame.
		const FVector Studio(0.0f, 0.0f, 80000.0f);
		FActorSpawnParameters Params;
		Params.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
		AStaticMeshActor* Prop = World->SpawnActor<AStaticMeshActor>(Studio, FRotator::ZeroRotator, Params);
		Prop->SetMobility(EComponentMobility::Movable);
		UStaticMeshComponent* PropMesh = Prop->GetStaticMeshComponent();
		PropMesh->SetCastShadow(false);

		auto Light = [&](const FVector& Offset, float Intensity, const FLinearColor& Colour)
		{
			APointLight* L = World->SpawnActor<APointLight>(Studio + Offset, FRotator::ZeroRotator, Params);
			L->SetMobility(EComponentMobility::Movable);
			UPointLightComponent* C = L->PointLightComponent;
			C->SetIntensityUnits(ELightUnits::Candelas);
			C->SetIntensity(Intensity);
			C->SetAttenuationRadius(20000.0f);
			C->SetLightColor(Colour);
			C->SetCastShadows(false);
			return L;
		};
		TArray<AActor*> Spawned = { Prop };
		Spawned.Add(Light(FVector(-900.0f, -700.0f, 900.0f), 900.0f, FLinearColor(1.0f, 0.96f, 0.9f)));    // key
		Spawned.Add(Light(FVector(-700.0f, 900.0f, 200.0f), 350.0f, FLinearColor(0.85f, 0.92f, 1.0f)));    // fill
		Spawned.Add(Light(FVector(900.0f, 100.0f, 600.0f), 600.0f, FLinearColor(1.0f, 1.0f, 1.0f)));       // rim

		ASceneCapture2D* Camera = World->SpawnActor<ASceneCapture2D>(Studio, FRotator::ZeroRotator, Params);
		Spawned.Add(Camera);
		USceneCaptureComponent2D* Cap = Camera->GetCaptureComponent2D();
		Cap->ProjectionType = ECameraProjectionMode::Orthographic;
		Cap->bCaptureEveryFrame = false;
		Cap->bCaptureOnMovement = false;
		Cap->PrimitiveRenderMode = ESceneCapturePrimitiveRenderMode::PRM_UseShowOnlyList;
		Cap->ShowOnlyActors.Add(Prop);
		Cap->ShowFlags.SetAtmosphere(false);
		Cap->ShowFlags.SetFog(false);
		Cap->ShowFlags.SetVolumetricFog(false);
		Cap->ShowFlags.SetBloom(false);
		Cap->ShowFlags.SetMotionBlur(false);
		Cap->ShowFlags.SetEyeAdaptation(false);
		Cap->ShowFlags.SetLensFlares(false);
		Cap->ShowFlags.SetVignette(false);
		Cap->PostProcessSettings.bOverride_AutoExposureMethod = true;
		Cap->PostProcessSettings.AutoExposureMethod = AEM_Manual;
		Cap->PostProcessSettings.bOverride_AutoExposureBias = true;
		Cap->PostProcessSettings.AutoExposureBias = 9.0f;
		Cap->PostProcessBlendWeight = 1.0f;

		UTextureRenderTarget2D* Ldr = NewObject<UTextureRenderTarget2D>();
		Ldr->RenderTargetFormat = RTF_RGBA8_SRGB;
		Ldr->ClearColor = FLinearColor::Black;
		Ldr->InitAutoFormat(IconSize, IconSize);
		Ldr->UpdateResourceImmediate(true);
		UTextureRenderTarget2D* Hdr = NewObject<UTextureRenderTarget2D>();
		Hdr->RenderTargetFormat = RTF_RGBA16f;
		Hdr->ClearColor = FLinearColor::Black;
		Hdr->InitAutoFormat(IconSize, IconSize);
		Hdr->UpdateResourceImmediate(true);

		// One item every few frames: a mesh set on a component reaches the renderer at the end of the frame, so it is
		// set on one tick and captured a few ticks later, once its textures have streamed in too.
		struct FJob
		{
			TArray<FName> Ids;
			TArray<const FDCCEquipmentDef*> Defs;
			int32 Next = 0;
			bool bArmed = false;
			int32 Made = 0, Skipped = 0;
			FTimerHandle Timer;
		};
		TSharedRef<FJob> Job = MakeShared<FJob>();
		for (const TPair<FName, uint8*>& Row : Table->GetRowMap())
		{
			const FString Out = Folder / (Row.Key.ToString() + TEXT(".png"));
			if (!bOverwrite && FPaths::FileExists(Out))
			{
				++Job->Skipped;
				continue;
			}
			Job->Ids.Add(Row.Key);
			Job->Defs.Add(reinterpret_cast<const FDCCEquipmentDef*>(Row.Value));
		}
		// Pictures for things that are not gear rows (the event page's prize).
		static FDCCEquipmentDef Extras[1];
		Extras[0].Mesh = TSoftObjectPtr<UStaticMesh>(FSoftObjectPath(TEXT("/Game/Beach_house/Meshes/Smalle_props/SM_mic.SM_mic")));
		if (bOverwrite || !FPaths::FileExists(Folder / TEXT("HotMic.png")))
		{
			Job->Ids.Add(TEXT("HotMic"));
			Job->Defs.Add(&Extras[0]);
		}
		TWeakObjectPtr<UWorld> WeakWorld(World);
		TArray<TWeakObjectPtr<AActor>> Owned;
		for (AActor* A : Spawned)
		{
			Owned.Add(A);
		}
		auto Step = [Job, WeakWorld, Owned, Prop, PropMesh, Camera, Cap, Ldr, Hdr, Studio, Folder]()
		{
			UWorld* W = WeakWorld.Get();
			auto Finish = [&]()
			{
				// Everything first, then the timer: clearing it frees this lambda and what it captured.
				UE_LOG(LogTemp, Log, TEXT("[DCC Icons] made %d, skipped %d, in %s"), Job->Made, Job->Skipped, *Folder);
				for (const TWeakObjectPtr<AActor>& A : Owned)
				{
					if (A.IsValid())
					{
						A->Destroy();
					}
				}
				Ldr->RemoveFromRoot();
				Hdr->RemoveFromRoot();
				if (W)
				{
					FTimerHandle Handle = Job->Timer;
					W->GetTimerManager().ClearTimer(Handle);
				}
			};
			if (!W || !IsValid(Prop) || !IsValid(Camera))
			{
				Finish();
				return;
			}
			if (Job->Next >= Job->Ids.Num())
			{
				Finish();
				return;
			}
			const FDCCEquipmentDef* Def = Job->Defs[Job->Next];
			if (!Job->bArmed)
			{
				UStaticMesh* Mesh = Def ? (!Def->GroundMesh.IsNull() ? Def->GroundMesh.LoadSynchronous() : Def->Mesh.LoadSynchronous()) : nullptr;
				if (!Mesh)
				{
					++Job->Skipped;
					++Job->Next;
					return;
				}
				PropMesh->SetStaticMesh(Mesh);
				PropMesh->SetWorldScale3D(FVector::OneVector);
				for (int32 m = 0; m < PropMesh->GetNumMaterials(); ++m)
				{
					if (UMaterialInterface* Mat = PropMesh->GetMaterial(m))
					{
						Mat->SetForceMipLevelsToBeResident(false, false, 5.0f, 0, true);
					}
				}
				// Centre it, then frame it: a three-quarter view from the front and a little above.
				const FBoxSphereBounds B = Mesh->GetBounds();
				Prop->SetActorLocation(Studio - B.Origin);
				// Long things (swords, poles, rifles) lie corner to corner, so they fill the square instead of a thin strip.
				TArray<double> Ext = { B.BoxExtent.X, B.BoxExtent.Y, B.BoxExtent.Z };
				Ext.Sort();
				const bool bLong = Ext[2] > Ext[1] * 2.5;
				const FRotator Base(-22.0f, 35.0f, 0.0f);
				FRotator View = Base;
				if (bLong)
				{
					// Roll the camera so the long axis lands on the diagonal.
					const FVector Long = B.BoxExtent.X >= B.BoxExtent.Y && B.BoxExtent.X >= B.BoxExtent.Z ? FVector::ForwardVector
						: B.BoxExtent.Y >= B.BoxExtent.Z ? FVector::RightVector : FVector::UpVector;
					const FRotationMatrix M(Base);
					const FVector2D OnScreen(FVector::DotProduct(Long, M.GetScaledAxis(EAxis::Y)), FVector::DotProduct(Long, M.GetScaledAxis(EAxis::Z)));
					const float Angle = FMath::RadiansToDegrees(FMath::Atan2(OnScreen.Y, OnScreen.X));
					View = (FQuat(Base) * FQuat(FVector::ForwardVector, FMath::DegreesToRadians(Angle - 45.0f))).Rotator();
				}
				Camera->SetActorLocationAndRotation(Studio - View.Vector() * (B.SphereRadius * 4.0f + 100.0f), View);
				Cap->OrthoWidth = B.SphereRadius * (bLong ? 1.7f : 2.05f);
				Job->bArmed = true;
				return;
			}
			Job->bArmed = false;
			const FString Out = Folder / (Job->Ids[Job->Next].ToString() + TEXT(".png"));
			++Job->Next;

			Cap->CaptureSource = SCS_FinalColorLDR;
			Cap->TextureTarget = Ldr;
			Cap->CaptureScene();
			Cap->CaptureSource = SCS_SceneColorHDR;
			Cap->TextureTarget = Hdr;
			Cap->CaptureScene();

			TArray<FColor> Colour;
			TArray<FLinearColor> Alpha;
			Ldr->GameThread_GetRenderTargetResource()->ReadPixels(Colour);
			Hdr->GameThread_GetRenderTargetResource()->ReadLinearColorPixels(Alpha);
			if (Colour.Num() != IconSize * IconSize || Alpha.Num() != Colour.Num())
			{
				++Job->Skipped;
				return;
			}
			// Scene colour's alpha is 1 where nothing was drawn; flip it if a corner says otherwise.
			const bool bInvert = Alpha[0].A > 0.5f;
			for (int32 i = 0; i < Colour.Num(); ++i)
			{
				const float A = FMath::Clamp(bInvert ? 1.0f - Alpha[i].A : Alpha[i].A, 0.0f, 1.0f);
				FColor& C = Colour[i];
				// The edge was drawn over black: lift it back out so the outline is not dark.
				if (A > 0.02f && A < 0.99f)
				{
					C.R = uint8(FMath::Min(255.0f, C.R / A));
					C.G = uint8(FMath::Min(255.0f, C.G / A));
					C.B = uint8(FMath::Min(255.0f, C.B / A));
				}
				C.A = uint8(FMath::RoundToInt(A * 255.0f));
			}
			TArray64<uint8> Png;
			FImageUtils::PNGCompressImageArray(IconSize, IconSize, TArrayView64<const FColor>(Colour.GetData(), Colour.Num()), Png);
			if (FFileHelper::SaveArrayToFile(Png, *Out))
			{
				++Job->Made;
			}
		};
		Ldr->AddToRoot();
		Hdr->AddToRoot();
		World->GetTimerManager().SetTimer(Job->Timer, FTimerDelegate::CreateLambda(Step), 0.25f, true);
	}
}

static FAutoConsoleCommandWithWorldAndArgs GDevMakeItemIcons(TEXT("DCC.MakeItemIcons"),
	TEXT("DCC.MakeItemIcons [overwrite 0/1] [delay]: (dev) render every gear row's mesh to Content/DCC/UI/ItemIcons/<Id>.png."),
	FConsoleCommandWithWorldAndArgsDelegate::CreateLambda([](const TArray<FString>& Args, UWorld* World)
	{
		const bool bOverwrite = Args.Num() > 0 && FCString::Atoi(*Args[0]) != 0;
		const float Delay = Args.Num() > 1 ? FCString::Atof(*Args[1]) : 0.0f;
		if (!World)
		{
			return;
		}
		if (Delay <= 0.0f)
		{
			MakeIcons(World, bOverwrite);
			return;
		}
		FTimerHandle Handle;
		TWeakObjectPtr<UWorld> Weak(World);
		World->GetTimerManager().SetTimer(Handle, FTimerDelegate::CreateLambda([Weak, bOverwrite]() { if (Weak.IsValid()) { MakeIcons(Weak.Get(), bOverwrite); } }), Delay, false);
	}));
