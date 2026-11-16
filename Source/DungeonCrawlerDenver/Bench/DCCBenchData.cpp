#include "Bench/DCCBenchData.h"

#include "Components/SkeletalMeshComponent.h"
#include "Dom/JsonObject.h"
#include "Engine/SkeletalMesh.h"
#include "Engine/SkeletalMeshSocket.h"
#include "HAL/FileManager.h"
#include "Misc/FileHelper.h"
#include "Misc/Paths.h"
#include "ReferenceSkeleton.h"
#include "Serialization/JsonReader.h"
#include "Serialization/JsonSerializer.h"
#include "Serialization/JsonWriter.h"

namespace
{
	bool GLoaded = false;
	TMap<FString, DCCBench::FNudge> GGrips;                       // "ItemId" or "MeshPath|ItemId"
	TMap<FString, TMap<FName, DCCBench::FNudge>> GSockets;        // MeshPath -> socket
	TMap<FString, TMap<FName, DCCBench::FNudge>> GBones;          // MeshPath -> bone
	TMap<FString, TMap<FName, DCCBench::FNudge>> GOriginalSockets; // as authored, captured before overriding

	FString ContentPath() { return FPaths::ProjectContentDir() / TEXT("DCC/Bench/DCCBench.json"); }
	FString SavedPath() { return FPaths::ProjectSavedDir() / TEXT("DCCBench.json"); }

	FString GripKey(const FString& MeshPath, FName ItemId)
	{
		return MeshPath.IsEmpty() ? ItemId.ToString() : MeshPath + TEXT("|") + ItemId.ToString();
	}

	TArray<TSharedPtr<FJsonValue>> Vec(double A, double B, double C)
	{
		return { MakeShared<FJsonValueNumber>(A), MakeShared<FJsonValueNumber>(B), MakeShared<FJsonValueNumber>(C) };
	}

	TSharedRef<FJsonObject> ToJson(const DCCBench::FNudge& N)
	{
		TSharedRef<FJsonObject> O = MakeShared<FJsonObject>();
		O->SetArrayField(TEXT("loc"), Vec(N.Location.X, N.Location.Y, N.Location.Z));
		O->SetArrayField(TEXT("rot"), Vec(N.Rotation.Pitch, N.Rotation.Yaw, N.Rotation.Roll));
		O->SetArrayField(TEXT("scale"), Vec(N.Scale.X, N.Scale.Y, N.Scale.Z));
		return O;
	}

	FVector ReadVec(const TSharedPtr<FJsonObject>& O, const TCHAR* Field, const FVector& Default)
	{
		const TArray<TSharedPtr<FJsonValue>>* A = nullptr;
		if (!O->TryGetArrayField(Field, A) || A->Num() != 3)
		{
			return Default;
		}
		return FVector((*A)[0]->AsNumber(), (*A)[1]->AsNumber(), (*A)[2]->AsNumber());
	}

	DCCBench::FNudge FromJson(const TSharedPtr<FJsonObject>& O)
	{
		DCCBench::FNudge N;
		N.Location = ReadVec(O, TEXT("loc"), FVector::ZeroVector);
		const FVector R = ReadVec(O, TEXT("rot"), FVector::ZeroVector);
		N.Rotation = FRotator(R.X, R.Y, R.Z);
		N.Scale = ReadVec(O, TEXT("scale"), FVector::OneVector);
		return N;
	}

	void ReadNested(const TSharedPtr<FJsonObject>& Root, const TCHAR* Field, TMap<FString, TMap<FName, DCCBench::FNudge>>& Out)
	{
		const TSharedPtr<FJsonObject>* Section = nullptr;
		if (!Root->TryGetObjectField(Field, Section))
		{
			return;
		}
		for (const TPair<FString, TSharedPtr<FJsonValue>>& Mesh : (*Section)->Values)
		{
			const TSharedPtr<FJsonObject> Inner = Mesh.Value->AsObject();
			if (!Inner.IsValid())
			{
				continue;
			}
			TMap<FName, DCCBench::FNudge>& Map = Out.FindOrAdd(Mesh.Key);
			for (const TPair<FString, TSharedPtr<FJsonValue>>& Entry : Inner->Values)
			{
				if (Entry.Value->AsObject().IsValid())
				{
					Map.Add(FName(*Entry.Key), FromJson(Entry.Value->AsObject()));
				}
			}
		}
	}

	TSharedRef<FJsonObject> WriteNested(const TMap<FString, TMap<FName, DCCBench::FNudge>>& In)
	{
		TSharedRef<FJsonObject> Section = MakeShared<FJsonObject>();
		for (const TPair<FString, TMap<FName, DCCBench::FNudge>>& Mesh : In)
		{
			if (Mesh.Value.IsEmpty())
			{
				continue;
			}
			TSharedRef<FJsonObject> Inner = MakeShared<FJsonObject>();
			for (const TPair<FName, DCCBench::FNudge>& Entry : Mesh.Value)
			{
				Inner->SetObjectField(Entry.Key.ToString(), ToJson(Entry.Value));
			}
			Section->SetObjectField(Mesh.Key, Inner);
		}
		return Section;
	}

	void CaptureOriginal(USkeletalMesh* Mesh, FName Socket)
	{
		if (!Mesh)
		{
			return;
		}
		TMap<FName, DCCBench::FNudge>& Originals = GOriginalSockets.FindOrAdd(Mesh->GetPathName());
		if (Originals.Contains(Socket))
		{
			return;
		}
		if (const USkeletalMeshSocket* Found = Mesh->FindSocket(Socket))
		{
			DCCBench::FNudge N;
			N.Location = Found->RelativeLocation;
			N.Rotation = Found->RelativeRotation;
			N.Scale = Found->RelativeScale;
			Originals.Add(Socket, N);
		}
	}

	void WriteSocket(USkeletalMesh* Mesh, FName Socket, const DCCBench::FNudge& Value)
	{
		if (USkeletalMeshSocket* Found = Mesh ? Mesh->FindSocket(Socket) : nullptr)
		{
			Found->RelativeLocation = Value.Location;
			Found->RelativeRotation = Value.Rotation;
			Found->RelativeScale = Value.Scale;
		}
	}

	/**
	 * Every frame, after the pose is final: rebuild component-space transforms
	 * from the local pose with the nudges folded in. Writes the buffer the
	 * renderer and the socket attachments read next.
	 */
	void ApplyBoneNudges(TWeakObjectPtr<USkeletalMeshComponent> WeakMesh)
	{
		USkeletalMeshComponent* Component = WeakMesh.Get();
		USkeletalMesh* Mesh = Component ? Component->GetSkeletalMeshAsset() : nullptr;
		if (!Mesh || Component->IsSimulatingPhysics())
		{
			return;
		}

		const TMap<FName, DCCBench::FNudge>* Nudges = GBones.Find(Mesh->GetPathName());
		if (!Nudges || Nudges->IsEmpty())
		{
			return;
		}

		const FReferenceSkeleton& Ref = Mesh->GetRefSkeleton();
		TArray<FTransform>& Space = const_cast<TArray<FTransform>&>(Component->GetComponentSpaceTransforms());
		const TArrayView<const FTransform> Local = Component->GetBoneSpaceTransformsView();
		const int32 Count = Ref.GetNum();
		if (Space.Num() != Count || Local.Num() != Count)
		{
			return;
		}

		TArray<int32, TInlineAllocator<16>> Indices;
		TArray<const DCCBench::FNudge*, TInlineAllocator<16>> Values;
		int32 First = Count;
		for (const TPair<FName, DCCBench::FNudge>& Pair : *Nudges)
		{
			const int32 Index = Ref.FindBoneIndex(Pair.Key);
			if (Index != INDEX_NONE && !Pair.Value.IsIdentity())
			{
				Indices.Add(Index);
				Values.Add(&Pair.Value);
				First = FMath::Min(First, Index);
			}
		}
		if (Indices.IsEmpty())
		{
			return;
		}

		// Bones are stored parent-first, so one forward pass from the first
		// nudged bone recomputes it and everything after it in the right order.
		for (int32 Bone = First; Bone < Count; ++Bone)
		{
			FTransform L = Local[Bone];
			const int32 Slot = Indices.IndexOfByKey(Bone);
			if (Slot != INDEX_NONE)
			{
				const DCCBench::FNudge& N = *Values[Slot];
				L.SetRotation(L.GetRotation() * N.Rotation.Quaternion());
				L.AddToTranslation(N.Location);
				L.SetScale3D(L.GetScale3D() * N.Scale);
			}
			const int32 Parent = Ref.GetParentIndex(Bone);
			Space[Bone] = Parent == INDEX_NONE ? L : L * Space[Parent];
		}
	}
}

namespace DCCBench
{
	FString GetSavePath()
	{
#if WITH_EDITOR
		return ContentPath();
#else
		return SavedPath();
#endif
	}

	void EnsureLoaded()
	{
		if (GLoaded)
		{
			return;
		}
		GLoaded = true;

		FString Text;
		const bool bRead =
#if !WITH_EDITOR
			FFileHelper::LoadFileToString(Text, *SavedPath()) ||
#endif
			FFileHelper::LoadFileToString(Text, *ContentPath());
		if (!bRead)
		{
			return;
		}

		TSharedPtr<FJsonObject> Root;
		if (!FJsonSerializer::Deserialize(TJsonReaderFactory<>::Create(Text), Root) || !Root.IsValid())
		{
			UE_LOG(LogTemp, Warning, TEXT("[DCC Bench] DCCBench.json did not parse; ignoring it."));
			return;
		}

		const TSharedPtr<FJsonObject>* Grips = nullptr;
		if (Root->TryGetObjectField(TEXT("grips"), Grips))
		{
			for (const TPair<FString, TSharedPtr<FJsonValue>>& Pair : (*Grips)->Values)
			{
				if (Pair.Value->AsObject().IsValid())
				{
					GGrips.Add(Pair.Key, FromJson(Pair.Value->AsObject()));
				}
			}
		}
		ReadNested(Root, TEXT("sockets"), GSockets);
		ReadNested(Root, TEXT("bones"), GBones);

		// Sockets live on the mesh assets, so push them now for every body named.
		for (const TPair<FString, TMap<FName, FNudge>>& Pair : GSockets)
		{
			if (USkeletalMesh* Mesh = LoadObject<USkeletalMesh>(nullptr, *Pair.Key))
			{
				ApplySockets(Mesh);
			}
		}

		UE_LOG(LogTemp, Log, TEXT("[DCC Bench] Loaded %d grips, %d bodies with sockets, %d bodies with bones."),
			GGrips.Num(), GSockets.Num(), GBones.Num());
	}

	bool Save(FString& OutWhere)
	{
		EnsureLoaded();

		TSharedRef<FJsonObject> Root = MakeShared<FJsonObject>();
		Root->SetStringField(TEXT("note"), TEXT("Written by the gear bench (F3). grips: item id, or mesh|item id. sockets and bones: per body."));

		TSharedRef<FJsonObject> Grips = MakeShared<FJsonObject>();
		for (const TPair<FString, FNudge>& Pair : GGrips)
		{
			Grips->SetObjectField(Pair.Key, ToJson(Pair.Value));
		}
		Root->SetObjectField(TEXT("grips"), Grips);
		Root->SetObjectField(TEXT("sockets"), WriteNested(GSockets));
		Root->SetObjectField(TEXT("bones"), WriteNested(GBones));

		FString Text;
		const TSharedRef<TJsonWriter<>> Writer = TJsonWriterFactory<>::Create(&Text);
		FJsonSerializer::Serialize(Root, Writer);

		OutWhere = GetSavePath();
		IFileManager::Get().MakeDirectory(*FPaths::GetPath(OutWhere), true);
		return FFileHelper::SaveStringToFile(Text, *OutWhere, FFileHelper::EEncodingOptions::ForceUTF8WithoutBOM);
	}

	bool SaveEntry(const TCHAR* Kind, const FString& MeshPath, FName Name, FString& OutWhere)
	{
		EnsureLoaded();
		OutWhere = GetSavePath();

		// Start from what is on disk, not from memory, so other unsaved nudges stay unsaved.
		TSharedPtr<FJsonObject> Root;
		FString Text;
		if (FFileHelper::LoadFileToString(Text, *OutWhere) || FFileHelper::LoadFileToString(Text, *ContentPath()))
		{
			FJsonSerializer::Deserialize(TJsonReaderFactory<>::Create(Text), Root);
		}
		if (!Root.IsValid())
		{
			Root = MakeShared<FJsonObject>();
		}
		Root->SetStringField(TEXT("note"), TEXT("Written by the gear bench (F3). grips: item id, or mesh|item id. sockets and bones: per body."));

		auto Section = [&Root](const TCHAR* Field) -> TSharedPtr<FJsonObject>
		{
			const TSharedPtr<FJsonObject>* Found = nullptr;
			if (Root->TryGetObjectField(Field, Found) && Found->IsValid())
			{
				return *Found;
			}
			TSharedPtr<FJsonObject> Made = MakeShared<FJsonObject>();
			Root->SetObjectField(Field, Made);
			return Made;
		};
		Section(TEXT("grips")); Section(TEXT("sockets")); Section(TEXT("bones"));

		const FString K = Kind;
		if (K == TEXT("grips"))
		{
			const FString Key = GripKey(MeshPath, Name);
			TSharedPtr<FJsonObject> Grips = Section(TEXT("grips"));
			if (const FNudge* Value = GGrips.Find(Key))
			{
				Grips->SetObjectField(Key, ToJson(*Value));
			}
			else
			{
				Grips->RemoveField(Key);
			}
		}
		else
		{
			const TMap<FString, TMap<FName, FNudge>>& Live = (K == TEXT("sockets")) ? GSockets : GBones;
			TSharedPtr<FJsonObject> Outer = Section(*K);
			const TSharedPtr<FJsonObject>* MeshObj = nullptr;
			TSharedPtr<FJsonObject> Mesh;
			if (Outer->TryGetObjectField(MeshPath, MeshObj) && MeshObj->IsValid())
			{
				Mesh = *MeshObj;
			}
			else
			{
				Mesh = MakeShared<FJsonObject>();
			}
			const TMap<FName, FNudge>* Map = Live.Find(MeshPath);
			if (const FNudge* Value = Map ? Map->Find(Name) : nullptr)
			{
				Mesh->SetObjectField(Name.ToString(), ToJson(*Value));
			}
			else
			{
				Mesh->RemoveField(Name.ToString());
			}
			if (Mesh->Values.Num() > 0)
			{
				Outer->SetObjectField(MeshPath, Mesh);
			}
			else
			{
				Outer->RemoveField(MeshPath);
			}
		}

		FString Out;
		FJsonSerializer::Serialize(Root.ToSharedRef(), TJsonWriterFactory<>::Create(&Out));
		IFileManager::Get().MakeDirectory(*FPaths::GetPath(OutWhere), true);
		return FFileHelper::SaveStringToFile(Out, *OutWhere, FFileHelper::EEncodingOptions::ForceUTF8WithoutBOM);
	}

	bool FindGrip(const FString& MeshPath, FName ItemId, FNudge& Out)
	{
		EnsureLoaded();
		if (const FNudge* Body = MeshPath.IsEmpty() ? nullptr : GGrips.Find(GripKey(MeshPath, ItemId)))
		{
			Out = *Body;
			return true;
		}
		if (const FNudge* Every = GGrips.Find(GripKey(FString(), ItemId)))
		{
			Out = *Every;
			return true;
		}
		return false;
	}

	void SetGrip(const FString& MeshPathOrEmpty, FName ItemId, const FNudge& Grip)
	{
		EnsureLoaded();
		GGrips.Add(GripKey(MeshPathOrEmpty, ItemId), Grip);
	}

	void ClearGrip(const FString& MeshPathOrEmpty, FName ItemId)
	{
		EnsureLoaded();
		GGrips.Remove(GripKey(MeshPathOrEmpty, ItemId));
	}

	bool GetOriginalSocket(USkeletalMesh* Mesh, FName Socket, FNudge& Out)
	{
		if (!Mesh)
		{
			return false;
		}
		CaptureOriginal(Mesh, Socket);
		if (const TMap<FName, FNudge>* Originals = GOriginalSockets.Find(Mesh->GetPathName()))
		{
			if (const FNudge* Found = Originals->Find(Socket))
			{
				Out = *Found;
				return true;
			}
		}
		return false;
	}

	bool FindSocket(const FString& MeshPath, FName Socket, FNudge& Out)
	{
		EnsureLoaded();
		if (const TMap<FName, FNudge>* Map = GSockets.Find(MeshPath))
		{
			if (const FNudge* Found = Map->Find(Socket))
			{
				Out = *Found;
				return true;
			}
		}
		return false;
	}

	void SetSocket(USkeletalMesh* Mesh, FName Socket, const FNudge& Value)
	{
		if (!Mesh)
		{
			return;
		}
		EnsureLoaded();
		CaptureOriginal(Mesh, Socket);
		GSockets.FindOrAdd(Mesh->GetPathName()).Add(Socket, Value);
		WriteSocket(Mesh, Socket, Value);
	}

	void ClearSocket(USkeletalMesh* Mesh, FName Socket)
	{
		if (!Mesh)
		{
			return;
		}
		EnsureLoaded();
		if (TMap<FName, FNudge>* Map = GSockets.Find(Mesh->GetPathName()))
		{
			Map->Remove(Socket);
		}
		FNudge Original;
		if (GetOriginalSocket(Mesh, Socket, Original))
		{
			WriteSocket(Mesh, Socket, Original);
		}
	}

	void ApplySockets(USkeletalMesh* Mesh)
	{
		if (!Mesh)
		{
			return;
		}
		EnsureLoaded();
		if (const TMap<FName, FNudge>* Map = GSockets.Find(Mesh->GetPathName()))
		{
			for (const TPair<FName, FNudge>& Pair : *Map)
			{
				CaptureOriginal(Mesh, Pair.Key);
				WriteSocket(Mesh, Pair.Key, Pair.Value);
			}
		}
	}

	bool FindBone(const FString& MeshPath, FName Bone, FNudge& Out)
	{
		EnsureLoaded();
		if (const TMap<FName, FNudge>* Map = GBones.Find(MeshPath))
		{
			if (const FNudge* Found = Map->Find(Bone))
			{
				Out = *Found;
				return true;
			}
		}
		return false;
	}

	void SetBone(const FString& MeshPath, FName Bone, const FNudge& Value)
	{
		EnsureLoaded();
		GBones.FindOrAdd(MeshPath).Add(Bone, Value);
	}

	void ClearBone(const FString& MeshPath, FName Bone)
	{
		EnsureLoaded();
		if (TMap<FName, FNudge>* Map = GBones.Find(MeshPath))
		{
			Map->Remove(Bone);
		}
	}

	void BindBoneNudges(USkeletalMeshComponent* Mesh)
	{
		if (!Mesh)
		{
			return;
		}
		EnsureLoaded();

		// One binding per component, ever. The callback looks the body up each
		// frame, so swapping a crawler's mesh needs no rebinding.
		static TSet<TWeakObjectPtr<USkeletalMeshComponent>> Bound;
		for (auto It = Bound.CreateIterator(); It; ++It)
		{
			if (!It->IsValid())
			{
				It.RemoveCurrent();
			}
		}
		if (Bound.Contains(Mesh))
		{
			return;
		}
		Bound.Add(Mesh);

		TWeakObjectPtr<USkeletalMeshComponent> Weak(Mesh);
		Mesh->RegisterOnBoneTransformsFinalizedDelegate(
			FOnBoneTransformsFinalizedMultiCast::FDelegate::CreateLambda([Weak]() { ApplyBoneNudges(Weak); }));
	}
}
