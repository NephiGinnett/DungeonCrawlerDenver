#include "Arcade/DCCSpray.h"
#include "Characters/DCCReactions.h"

#include "Arcade/DCCArcadeArena.h"
#include "Arcade/DCCSprayTag.h"
#include "Arcade/DCCWalletComponent.h"
#include "Characters/DCCCharacterBase.h"
#include "Vehicles/DCCRover.h"
#include "Session/DCCProfile.h"

#include "Brushes/SlateImageBrush.h"
#include "Dom/JsonObject.h"
#include "Engine/Engine.h"
#include "Engine/GameViewportClient.h"
#include "Engine/Texture2D.h"
#include "Engine/World.h"
#include "EngineUtils.h"
#include "GameFramework/PlayerController.h"
#include "GameFramework/PlayerState.h"
#include "HAL/FileManager.h"
#include "IImageWrapper.h"
#include "IImageWrapperModule.h"
#include "Materials/MaterialInstanceDynamic.h"
#include "Misc/Base64.h"
#include "Misc/FileHelper.h"
#include "Misc/Paths.h"
#include "Modules/ModuleManager.h"
#include "Serialization/JsonReader.h"
#include "Serialization/JsonSerializer.h"
#include "Serialization/JsonWriter.h"
#include "Widgets/SWindow.h"
#include "UObject/StrongObjectPtr.h"
#include "TimerManager.h"

#if PLATFORM_WINDOWS
#include "Windows/AllowWindowsPlatformTypes.h"
#include <commdlg.h>
#include "Windows/HideWindowsPlatformTypes.h"
#endif

DEFINE_LOG_CATEGORY_STATIC(LogDCCSpray, Log, All);

namespace DCCSpray
{
	/** Server: put this arena's saved tags back now, keeping only the ones that still have a surface. */
	void RestoreNow(ADCCArcadeArena* Arena);

	namespace
	{
		FString Dir() { return FPaths::ProjectSavedDir() / TEXT("Sprays"); }

		FString Safe(const FString& Key)
		{
			FString Out;
			for (const TCHAR C : Key)
			{
				Out.AppendChar(FChar::IsAlnum(C) ? C : TEXT('_'));
			}
			return Out.IsEmpty() ? FString(TEXT("Crawler")) : Out;
		}

		FString MinePath(const FString& Key) { return Dir() / FString::Printf(TEXT("Mine_%s.rgb"), *Safe(Key)); }

		FString WorldPath(const ADCCArcadeArena* Arena) { return Dir() / FString::Printf(TEXT("World_%s.json"), *Arena->GetClass()->GetName()); }

		/** Bumped whenever this machine picks a new picture, so the shop's preview is rebuilt. */
		int32 PickSerial = 0;

		ADCCArcadeArena* ArenaIn(UWorld* World)
		{
			for (TActorIterator<ADCCArcadeArena> It(World); It; ++It)
			{
				return *It;
			}
			return nullptr;
		}

		/** Crop to the middle square, average down to Res x Res, and band the colours: a stencil, not a photo. */
		void Pixelate(const TArray<uint8>& BGRA, int32 W, int32 H, TArray<uint8>& OutRGB)
		{
			const int32 Side = FMath::Min(W, H);
			const int32 X0 = (W - Side) / 2, Y0 = (H - Side) / 2;
			OutRGB.SetNumZeroed(Bytes);
			const int32 Levels = 6;
			for (int32 Y = 0; Y < Res; ++Y)
			{
				for (int32 X = 0; X < Res; ++X)
				{
					const int32 SX0 = X0 + X * Side / Res, SX1 = FMath::Max(SX0 + 1, X0 + (X + 1) * Side / Res);
					const int32 SY0 = Y0 + Y * Side / Res, SY1 = FMath::Max(SY0 + 1, Y0 + (Y + 1) * Side / Res);
					// Sample a sparse grid inside the cell: big photos would otherwise cost millions of reads.
					const int32 StepX = FMath::Max(1, (SX1 - SX0) / 8), StepY = FMath::Max(1, (SY1 - SY0) / 8);
					double R = 0, G = 0, B = 0;
					int32 N = 0;
					for (int32 SY = SY0; SY < SY1; SY += StepY)
					{
						for (int32 SX = SX0; SX < SX1; SX += StepX)
						{
							const int32 I = (SY * W + SX) * 4;
							B += BGRA[I]; G += BGRA[I + 1]; R += BGRA[I + 2];
							++N;
						}
					}
					auto Band = [Levels](double V)
					{
						return uint8(FMath::Clamp(FMath::RoundToInt(V / 255.0 * (Levels - 1)) * 255 / (Levels - 1), 0, 255));
					};
					const int32 O = (Y * Res + X) * 3;
					OutRGB[O] = Band(R / N);
					OutRGB[O + 1] = Band(G / N);
					OutRGB[O + 2] = Band(B / N);
				}
			}
		}

		void Notice(APlayerController* PC, const FString& Text)
		{
			if (const ADCCCharacterBase* Crawler = PC ? Cast<ADCCCharacterBase>(PC->GetPawn()) : nullptr)
			{
				Crawler->ShowLocalNotice(FText::FromString(Text));
			}
		}

		ADCCSprayTag* SpawnTag(UWorld* World, const FVector& At, const FRotator& Rot, const TArray<uint8>& RGB, const FString& Painter, double When)
		{
			const FTransform Transform(Rot, At);
			ADCCSprayTag* Tag = World->SpawnActorDeferred<ADCCSprayTag>(ADCCSprayTag::StaticClass(), Transform, nullptr, nullptr, ESpawnActorCollisionHandlingMethod::AlwaysSpawn);
			if (!Tag)
			{
				return nullptr;
			}
			Tag->Pixels = RGB;
			Tag->Painter = Painter;
			Tag->SprayedAt = When;
			Tag->FinishSpawning(Transform);
			return Tag;
		}

		/** The arena whose saved tags are already back in the world. Until then, a save would drop them. */
		TWeakObjectPtr<const ADCCArcadeArena> RestoredArena;

		void Save(UWorld* World)
		{
			ADCCArcadeArena* Arena = ArenaIn(World);
			if (!Arena)
			{
				return; // only the arcade's arenas keep their walls between sessions
			}
			if (RestoredArena.Get() != Arena)
			{
				RestoreNow(Arena); // someone sprayed before the old tags were back: bring them back first
			}
			const FTransform Origin = Arena->GetActorTransform();
			TArray<TSharedPtr<FJsonValue>> Rows;
			for (TActorIterator<ADCCSprayTag> It(World); It; ++It)
			{
				if (It->GetAttachParentActor())
				{
					continue; // on a vehicle: it leaves with it
				}
				const FVector L = Origin.InverseTransformPosition(It->GetActorLocation());
				const FRotator R = Origin.InverseTransformRotation(It->GetActorQuat()).Rotator();
				TSharedPtr<FJsonObject> Row = MakeShared<FJsonObject>();
				Row->SetNumberField(TEXT("x"), L.X); Row->SetNumberField(TEXT("y"), L.Y); Row->SetNumberField(TEXT("z"), L.Z);
				Row->SetNumberField(TEXT("pitch"), R.Pitch); Row->SetNumberField(TEXT("yaw"), R.Yaw); Row->SetNumberField(TEXT("roll"), R.Roll);
				Row->SetStringField(TEXT("painter"), It->Painter);
				Row->SetNumberField(TEXT("t"), It->SprayedAt);
				Row->SetStringField(TEXT("px"), FBase64::Encode(It->Pixels));
				Rows.Add(MakeShared<FJsonValueObject>(Row));
			}
			TSharedPtr<FJsonObject> Root = MakeShared<FJsonObject>();
			Root->SetArrayField(TEXT("tags"), Rows);
			FString Text;
			const TSharedRef<TJsonWriter<>> Writer = TJsonWriterFactory<>::Create(&Text);
			FJsonSerializer::Serialize(Root.ToSharedRef(), Writer);
			FFileHelper::SaveStringToFile(Text, *WorldPath(Arena));
		}
	}

	bool PickPicture(const FString& ProfileKey, FString& Error)
	{
		FString Path;
#if PLATFORM_WINDOWS
		WCHAR File[2048] = { 0 };
		OPENFILENAMEW Dialog;
		FMemory::Memzero(Dialog);
		Dialog.lStructSize = sizeof(Dialog);
		if (GEngine && GEngine->GameViewport)
		{
			if (TSharedPtr<SWindow> Window = GEngine->GameViewport->GetWindow())
			{
				if (Window->GetNativeWindow().IsValid())
				{
					Dialog.hwndOwner = (HWND)Window->GetNativeWindow()->GetOSWindowHandle();
				}
			}
		}
		Dialog.lpstrFilter = L"Pictures (*.jpg, *.jpeg, *.png, *.bmp)\0*.jpg;*.jpeg;*.png;*.bmp\0\0";
		Dialog.lpstrFile = File;
		Dialog.nMaxFile = UE_ARRAY_COUNT(File);
		Dialog.lpstrTitle = L"Pick a picture for your spray tag";
		Dialog.Flags = OFN_FILEMUSTEXIST | OFN_PATHMUSTEXIST | OFN_NOCHANGEDIR;
		if (!GetOpenFileNameW(&Dialog))
		{
			return false; // cancelled
		}
		Path = File;
#else
		Error = TEXT("Picking a picture needs Windows.");
		return false;
#endif
		return UsePicture(ProfileKey, Path, Error);
	}

	bool UsePicture(const FString& ProfileKey, const FString& Path, FString& Error)
	{
		TArray<uint8> RGB;
		if (!DecodePicture(Path, RGB, Error))
		{
			return false;
		}
		IFileManager::Get().MakeDirectory(*Dir(), true);
		if (!FFileHelper::SaveArrayToFile(RGB, *MinePath(ProfileKey)))
		{
			Error = TEXT("Could not save the tag.");
			return false;
		}
		++PickSerial;
		return true;
	}

	bool DecodePicture(const FString& Path, TArray<uint8>& RGB, FString& Error)
	{
		TArray<uint8> Compressed;
		if (!FFileHelper::LoadFileToArray(Compressed, *Path) || Compressed.Num() > 64 * 1024 * 1024)
		{
			Error = TEXT("That picture could not be read.");
			return false;
		}
		IImageWrapperModule& Images = FModuleManager::LoadModuleChecked<IImageWrapperModule>(TEXT("ImageWrapper"));
		const EImageFormat Format = Images.DetectImageFormat(Compressed.GetData(), Compressed.Num());
		const TSharedPtr<IImageWrapper> Wrapper = Format == EImageFormat::Invalid ? nullptr : Images.CreateImageWrapper(Format);
		TArray<uint8> Raw;
		if (!Wrapper.IsValid() || !Wrapper->SetCompressed(Compressed.GetData(), Compressed.Num()) || !Wrapper->GetRaw(ERGBFormat::BGRA, 8, Raw))
		{
			Error = TEXT("That file is not a picture this can read. Try a JPG or PNG.");
			return false;
		}
		const int32 W = int32(Wrapper->GetWidth()), H = int32(Wrapper->GetHeight());
		if (W < 4 || H < 4 || Raw.Num() < W * H * 4)
		{
			Error = TEXT("That picture is too small.");
			return false;
		}
		Pixelate(Raw, W, H, RGB);
		return true;
	}

	bool LoadMine(const FString& ProfileKey, TArray<uint8>& OutRGB)
	{
		return FFileHelper::LoadFileToArray(OutRGB, *MinePath(ProfileKey), FILEREAD_Silent) && OutRGB.Num() == Bytes;
	}

	const FSlateBrush* PreviewBrush(const FString& ProfileKey)
	{
		struct FCached { int32 Serial = -1; TSharedPtr<FSlateBrush> Brush; TStrongObjectPtr<UTexture2D> Texture; };
		static TMap<FString, FCached> Cache;
		FCached& Entry = Cache.FindOrAdd(ProfileKey);
		if (Entry.Serial != PickSerial)
		{
			Entry.Serial = PickSerial;
			Entry.Brush.Reset();
			Entry.Texture.Reset();
			TArray<uint8> RGB;
			if (LoadMine(ProfileKey, RGB))
			{
				if (UTexture2D* Texture = MakeTexture(RGB))
				{
					Entry.Texture.Reset(Texture);
					Entry.Brush = MakeShared<FSlateImageBrush>(Texture, FVector2D(176.0f, 176.0f));
				}
			}
		}
		return Entry.Brush.Get();
	}

	void TryLocalSpray(APlayerController* PC)
	{
		APawn* Pawn = PC ? PC->GetPawn() : nullptr;
		UWorld* World = PC ? PC->GetWorld() : nullptr;
		if (!Pawn || !World)
		{
			return;
		}
		static double LastTry = 0.0;
		const double Now = FPlatformTime::Seconds();
		if (Now - LastTry < 1.2)
		{
			return;
		}
		LastTry = Now;

		const FString Key = DCCProfile::KeyFor(PC->PlayerState);
		ADCCCharacterBase* Crawler = Cast<ADCCCharacterBase>(Pawn);
		if (Crawler && Crawler->Wallet && !Crawler->Wallet->Owns(CanId))
		{
			Notice(PC, TEXT("No spray can. The Prize Counter in the Safe Room sells one: pick any picture and it becomes your tag."));
			return;
		}
		TArray<uint8> RGB;
		if (!LoadMine(Key, RGB))
		{
			Notice(PC, TEXT("Your can has no picture on this PC. Pick one at the Prize Counter (CHANGE PICTURE)."));
			return;
		}

		FVector Eye;
		FRotator Look;
		PC->GetPlayerViewPoint(Eye, Look);
		const FVector Dir = Look.Vector();
		FCollisionQueryParams Params(SCENE_QUERY_STAT(DCCSpray), true, Pawn);
		TArray<AActor*> Attached;
		Pawn->GetAttachedActors(Attached, true, true);
		Params.AddIgnoredActors(Attached);
		const float Length = Reach + FVector::Dist(Eye, Pawn->GetActorLocation());
		FHitResult Hit;
		if (!World->LineTraceSingleByChannel(Hit, Eye, Eye + Dir * Length, ECC_Visibility, Params) || Cast<APawn>(Hit.GetActor()))
		{
			Notice(PC, TEXT("Nothing to paint there. Look at a wall, the ground or a truck."));
			return;
		}
		if (FVector::Dist(Hit.ImpactPoint, Pawn->GetActorLocation()) > Reach + 200.0f)
		{
			Notice(PC, TEXT("Too far to reach. Get closer."));
			return;
		}
		if (Crawler && Crawler->Wallet)
		{
			Crawler->Wallet->Server_Spray(Hit.ImpactPoint, Hit.ImpactNormal, Dir, RGB);
		}
		else if (ADCCRover* Rover = Cast<ADCCRover>(Pawn))
		{
			Rover->Server_Spray(Hit.ImpactPoint, Hit.ImpactNormal, Dir, RGB);
		}
	}

	ADCCSprayTag* PaintTag(UWorld* World, const FVector& At, const FVector& Normal, const FVector& ViewDir, const TArray<uint8>& RGB,
		const FString& Painter, const AActor* Ignore)
	{
		if (!World || RGB.Num() != Bytes)
		{
			return nullptr;
		}
		// The decal projects along its X axis, into the surface. The picture's top follows the decal's -Y (found by
		// test, 2026-11-16), so -Y is aimed at "up": world up on a wall, away from the painter on a floor or a roof.
		const FVector N = Normal.GetSafeNormal(UE_SMALL_NUMBER, FVector::UpVector);
		FVector Up = FMath::Abs(N.Z) < 0.7f ? FVector::UpVector : FVector(ViewDir.X, ViewDir.Y, 0.0f).GetSafeNormal(UE_SMALL_NUMBER, FVector::ForwardVector);
		if (N.Z < -0.7f)
		{
			Up = -Up;
		}
		const FRotator Rot = FRotationMatrix::MakeFromXY(-N, -Up).Rotator();
		ADCCSprayTag* Tag = SpawnTag(World, At, Rot, RGB, Painter, FDateTime::UtcNow().ToUnixTimestamp());

		// Painted on something that moves (a truck, a car): it rides along, and is not kept, since that thing will not be
		// back next session.
		FHitResult Under;
		FCollisionQueryParams Params(SCENE_QUERY_STAT(DCCSprayAttach), true, Ignore);
		if (Tag && World->LineTraceSingleByChannel(Under, At + N * 20.0f, At - N * 40.0f, ECC_Visibility, Params))
		{
			// Only things that really move: a pawn (the trucks, the traffic) or something under physics. Every wall the
			// arcade builds at runtime is a Movable mesh too, and treating those as vehicles meant no tag was ever kept
			// (2026-11-21, Zoe: "I had hoped that sprays might persist between games!").
			UPrimitiveComponent* Surface = Under.GetComponent();
			AActor* Thing = Under.GetActor();
			const bool bRides = Surface && Thing && !Cast<ADCCArcadeArena>(Thing)
				&& (Cast<APawn>(Thing) || Surface->IsSimulatingPhysics() || (Thing->GetAttachParentActor() && Cast<APawn>(Thing->GetAttachParentActor())));
			if (bRides)
			{
				Tag->AttachToComponent(Surface, FAttachmentTransformRules::KeepWorldTransform);
			}
		}

		return Tag;
	}

	void ServerSpray(APawn* Painter, APlayerState* PS, const FVector& At, const FVector& Normal, const FVector& ViewDir, const TArray<uint8>& RGB)
	{
		UWorld* World = Painter ? Painter->GetWorld() : nullptr;
		if (!World || !PS || RGB.Num() != Bytes || !Painter->HasAuthority())
		{
			return;
		}
		const FString Key = DCCProfile::KeyFor(PS);
		if (Key.IsEmpty() || !DCCProfile::GetCosmetics(Key).Contains(CanId))
		{
			return;
		}
		static TMap<FString, double> Last;
		const double Now = FPlatformTime::Seconds();
		if (double* When = Last.Find(Key); When && Now - *When < 1.0)
		{
			return;
		}
		Last.Add(Key, Now);
		if (FVector::Dist(Painter->GetActorLocation(), At) > Reach + 600.0f)
		{
			return; // a truck is long; its pivot can be a few metres from where the driver is looking
		}

		PaintTag(World, At, Normal, ViewDir, RGB, Key, Painter);
		// The game may care where it went (the cage in Dead Air, the summit in Fourteener). From the truck, the driver.
		ADCCCharacterBase* Crawler = Cast<ADCCCharacterBase>(Painter);
		if (!Crawler)
		{
			if (const ADCCRover* Rover = Cast<ADCCRover>(Painter))
			{
				Crawler = Cast<ADCCCharacterBase>(Rover->Driver);
			}
		}
		if (ADCCArcadeArena* Arena = ArenaIn(World); Arena && Crawler)
		{
			Arena->OnTagged(Crawler, At);
		}
		if (Crawler)
		{
			Crawler->ReactTo(DCCReactions::MomentParty);
		}

		// Too many: the oldest is painted over.
		TArray<ADCCSprayTag*> Tags;
		for (TActorIterator<ADCCSprayTag> It(World); It; ++It)
		{
			Tags.Add(*It);
		}
		if (Tags.Num() > MaxPerArena)
		{
			Tags.Sort([](const ADCCSprayTag& A, const ADCCSprayTag& B) { return A.SprayedAt < B.SprayedAt; });
			for (int32 i = 0; i < Tags.Num() - MaxPerArena; ++i)
			{
				Tags[i]->Destroy();
			}
		}
		Save(World);
	}

	void Restore(ADCCArcadeArena* Arena)
	{
		if (!Arena || !Arena->HasAuthority())
		{
			return;
		}
		// Most arenas build their walls after BeginPlay (the casino floor, the Gate dungeon once its seed lands), so
		// the tags go back a few seconds in, when there is something to stick to.
		TWeakObjectPtr<ADCCArcadeArena> Weak(Arena);
		FTimerHandle Handle;
		Arena->GetWorldTimerManager().SetTimer(Handle, FTimerDelegate::CreateLambda([Weak]()
		{
			if (ADCCArcadeArena* Live = Weak.Get(); Live && RestoredArena.Get() != Live)
			{
				RestoreNow(Live);
			}
		}), 4.0f, false);
	}

	void RestoreNow(ADCCArcadeArena* Arena)
	{
		RestoredArena = Arena;
		FString Text;
		if (!IFileManager::Get().FileExists(*WorldPath(Arena)) || !FFileHelper::LoadFileToString(Text, *WorldPath(Arena)))
		{
			return;
		}
		TSharedPtr<FJsonObject> Root;
		if (!FJsonSerializer::Deserialize(TJsonReaderFactory<>::Create(Text), Root) || !Root.IsValid())
		{
			return;
		}
		const TArray<TSharedPtr<FJsonValue>>* Rows = nullptr;
		if (!Root->TryGetArrayField(TEXT("tags"), Rows))
		{
			return;
		}
		const FTransform Origin = Arena->GetActorTransform();
		int32 Count = 0;
		int32 Skipped = 0;
		for (const TSharedPtr<FJsonValue>& Value : *Rows)
		{
			const TSharedPtr<FJsonObject> Row = Value.IsValid() ? Value->AsObject() : nullptr;
			TArray<uint8> RGB;
			if (!Row.IsValid() || !FBase64::Decode(Row->GetStringField(TEXT("px")), RGB) || RGB.Num() != Bytes)
			{
				continue;
			}
			const FVector L(Row->GetNumberField(TEXT("x")), Row->GetNumberField(TEXT("y")), Row->GetNumberField(TEXT("z")));
			const FRotator R(Row->GetNumberField(TEXT("pitch")), Row->GetNumberField(TEXT("yaw")), Row->GetNumberField(TEXT("roll")));
			const FVector At = Origin.TransformPosition(L);
			const FRotator Rot = Origin.TransformRotation(R.Quaternion()).Rotator();
			// Still a wall there? A Gate dungeon is a new layout every run; a tag from last run's rooms is not hung in the air.
			const FVector Into = Rot.Vector();
			FHitResult Hit;
			FCollisionQueryParams Params(SCENE_QUERY_STAT(DCCSprayRestore), true);
			if (!Arena->GetWorld()->LineTraceSingleByChannel(Hit, At - Into * 30.0f, At + Into * 60.0f, ECC_Visibility, Params)
				|| Cast<APawn>(Hit.GetActor()))
			{
				++Skipped;
				continue;
			}
			SpawnTag(Arena->GetWorld(), At, Rot, RGB, Row->GetStringField(TEXT("painter")), Row->GetNumberField(TEXT("t")));
			++Count;
		}
		UE_LOG(LogDCCSpray, Log, TEXT("[DCC Spray] %d tag(s) put back in %s (%d with nothing left to stick to)."), Count,
			*Arena->GetClass()->GetName(), Skipped);
	}

	void ClearAll(UWorld* World)
	{
		if (!World || World->GetNetMode() == NM_Client)
		{
			return;
		}
		for (TActorIterator<ADCCSprayTag> It(World); It; ++It)
		{
			It->Destroy();
		}
		TArray<FString> Files;
		IFileManager::Get().FindFiles(Files, *(Dir() / TEXT("World_*.json")), true, false);
		for (const FString& File : Files)
		{
			IFileManager::Get().Delete(*(Dir() / File));
		}
		UE_LOG(LogDCCSpray, Log, TEXT("[DCC Spray] Host cleared every tag (%d saved file(s))."), Files.Num());
	}

	UTexture2D* MakeTexture(const TArray<uint8>& RGB)
	{
		if (RGB.Num() != Bytes)
		{
			return nullptr;
		}
		UTexture2D* Texture = UTexture2D::CreateTransient(Res, Res, PF_B8G8R8A8);
		if (!Texture || !Texture->GetPlatformData() || Texture->GetPlatformData()->Mips.IsEmpty())
		{
			return nullptr;
		}
		Texture->Filter = TF_Nearest; // keep it pixelated up close
		Texture->SRGB = true;
		FTexture2DMipMap& Mip = Texture->GetPlatformData()->Mips[0];
		uint8* Out = static_cast<uint8*>(Mip.BulkData.Lock(LOCK_READ_WRITE));
		for (int32 i = 0; i < Res * Res; ++i)
		{
			Out[i * 4 + 0] = RGB[i * 3 + 2];
			Out[i * 4 + 1] = RGB[i * 3 + 1];
			Out[i * 4 + 2] = RGB[i * 3 + 0];
			Out[i * 4 + 3] = 255;
		}
		Mip.BulkData.Unlock();
		Texture->UpdateResource();
		return Texture;
	}

	UMaterialInterface* MakeMaterial(UObject* Outer, UTexture2D* Texture)
	{
		// The Cyberpunk pack's graffiti decal: the same paint look as the city's own tags, with our picture in it.
		static const TCHAR* ParentPath = TEXT("/Game/Cyberpunk_City/Materials/Decals/MI_decal_vandalism_01.MI_decal_vandalism_01");
		UMaterialInterface* Parent = LoadObject<UMaterialInterface>(nullptr, ParentPath);
		if (!Parent || !Texture)
		{
			UE_LOG(LogDCCSpray, Warning, TEXT("[DCC Spray] No decal material (%s)."), ParentPath);
			return nullptr;
		}
		UMaterialInstanceDynamic* MID = UMaterialInstanceDynamic::Create(Parent, Outer);
		TArray<FMaterialParameterInfo> Infos;
		TArray<FGuid> Ids;
		Parent->GetAllTextureParameterInfo(Infos, Ids);
		static bool bLogged = false;
		for (const FMaterialParameterInfo& Info : Infos)
		{
			UTexture* Current = nullptr;
			Parent->GetTextureParameterValue(Info, Current);
			if (!bLogged)
			{
				UE_LOG(LogDCCSpray, Log, TEXT("[DCC Spray] decal texture parameter '%s' = %s"), *Info.Name.ToString(), Current ? *Current->GetName() : TEXT("none"));
			}
			// The one holding the pack's graffiti picture is the one to swap.
			if (Infos.Num() == 1 || (Current && Current->GetName().Contains(TEXT("vandalism"))))
			{
				MID->SetTextureParameterValueByInfo(Info, Texture);
			}
		}
		TArray<FMaterialParameterInfo> Scalars;
		Parent->GetAllScalarParameterInfo(Scalars, Ids);
		for (const FMaterialParameterInfo& Info : Scalars)
		{
			float Value = 0.0f;
			Parent->GetScalarParameterValue(Info, Value);
			if (!bLogged)
			{
				UE_LOG(LogDCCSpray, Log, TEXT("[DCC Spray] decal scalar '%s' = %f"), *Info.Name.ToString(), Value);
			}
			if (Info.Name.ToString().Contains(TEXT("Hue")))
			{
				MID->SetScalarParameterValueByInfo(Info, 0.0f); // the picture's own colours, not the pack's tint
			}
		}
		bLogged = true;
		return MID;
	}
}
