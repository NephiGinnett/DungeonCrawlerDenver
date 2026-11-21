// ---------------------------------------------------------------------------
// DCC.FixCrawlerRetargeting - stop shared animations stretching crawlers
//
// WHY THE CRAWLERS LOOK DISTORTED (found 2026-11-05)
//
// Every Meshy clip keys TRANSLATION on all 24 bones, not just rotation (checked
// by reading the .glb channels directly: every joint has rotation, scale AND
// translation tracks). A translation key is a bone length. So when Marcel plays
// A_DCC_HW_Running, which was authored on CJ's body, Marcel's forearm is pulled
// to CJ's forearm length for the duration of the clip. Shared skeleton, shared
// clips - and nobody's proportions survive them. The Blender rig-fit pass fixed
// each mesh's bind pose, but a bind pose is overwritten by translation keys the
// moment any animation plays.
//
// The standard UE fix is per-bone Translation Retargeting on the skeleton:
//   - every bone: "Skeleton"        -> bone length comes from the mesh being
//                                      animated, the animation supplies rotation
//   - the root (Hips): "AnimationScaled" -> keeps the motion (bobbing, jumping)
//                                      but scaled to this mesh's hip height
//
// This is an asset setting, normally clicked in the Skeleton editor. Since
// 2026-11-13 it is applied AUTOMATICALLY at runtime (DCCRetarget::EnsureForMesh
// from every crawler's BeginPlay and body swap), because the console command
// below was written on 11-05 and never run, and the spines stayed bent.
// The command still exists for writing the setting into the asset for good:
//   DCC.FixCrawlerRetargeting [optional skeleton path], then save the skeleton.
// ---------------------------------------------------------------------------

#include "Characters/DCCRetargetFix.h"

#include "Animation/Skeleton.h"
#include "Components/SkeletalMeshComponent.h"
#include "Engine/SkeletalMesh.h"
#include "HAL/IConsoleManager.h"
#include "ReferenceSkeleton.h"
#include "UObject/UnrealType.h"

namespace
{
	const TCHAR* DefaultCrawlerSkeleton = TEXT("/Game/DCC/Characters/Shared/SK_DCCCrawler_Skeleton.SK_DCCCrawler_Skeleton");
}

namespace DCCRetarget
{
	int32 BoneTreeCount(const USkeleton* Skeleton)
	{
		// BoneTree is protected. Its length has to match the reference skeleton
		// before anyone writes a retarget mode: SetBoneTranslationRetargetingMode
		// indexes BoneTree with no bounds check, and on the first ticks after a
		// body swap the tree can still be empty while GetNum() already is not.
		const FArrayProperty* Prop = FindFProperty<FArrayProperty>(USkeleton::StaticClass(), TEXT("BoneTree"));
		if (!Prop || !Skeleton)
		{
			return INDEX_NONE;
		}
		FScriptArrayHelper Helper(Prop, Prop->ContainerPtrToValuePtr<void>(Skeleton));
		return Helper.Num();
	}

	int32 Apply(USkeleton* Skeleton)
	{
		if (!Skeleton || Skeleton->HasAnyFlags(RF_NeedPostLoad | RF_NeedInitialization))
		{
			return 0;
		}
		const FReferenceSkeleton& Ref = Skeleton->GetReferenceSkeleton();
		const int32 BoneCount = Ref.GetNum();
		// -1 means "not ready", so the caller tries again. 0 means the modes
		// were already right. Writing past BoneTree is an out-of-bounds store.
		if (BoneCount <= 0 || BoneTreeCount(Skeleton) != BoneCount)
		{
			return -1;
		}
		int32 Changed = 0;
		for (int32 BoneIndex = 0; BoneIndex < BoneCount; ++BoneIndex)
		{
			// Index 0 is the root of the hierarchy (Hips on every Meshy rig).
			const EBoneTranslationRetargetingMode::Type Wanted = BoneIndex == 0
				? EBoneTranslationRetargetingMode::AnimationScaled
				: EBoneTranslationRetargetingMode::Skeleton;

			if (Skeleton->GetBoneTranslationRetargetingMode(BoneIndex) != Wanted)
			{
				Skeleton->SetBoneTranslationRetargetingMode(BoneIndex, Wanted, /*bChildrenToo=*/false);
				++Changed;
			}
		}
		if (Changed > 0)
		{
			UE_LOG(LogTemp, Display,
				TEXT("[DCC Retarget] %s: %d of %d bones set (root '%s' = AnimationScaled, the rest = Skeleton)."),
				*Skeleton->GetName(), Changed, Ref.GetNum(), *Ref.GetBoneName(0).ToString());
		}
		return Changed;
	}

	void EnsureForMesh(const USkeletalMeshComponent* Mesh)
	{
		static bool bDone = false;
		if (bDone)
		{
			return;
		}

		const USkeletalMesh* Asset = Mesh ? Mesh->GetSkeletalMeshAsset() : nullptr;
		// The skeleton is shared engine data; the mesh only hands it out as const.
		USkeleton* Skeleton = Asset ? const_cast<USkeleton*>(Asset->GetSkeleton()) : nullptr;
		const UPTRINT Address = reinterpret_cast<UPTRINT>(Skeleton);
		if (Address < 0x10000 || (Address & (sizeof(void*) - 1)) != 0 || !Skeleton->IsValidLowLevelFast(false))
		{
			return;
		}
		if (Skeleton->GetPathName() != DefaultCrawlerSkeleton)
		{
			return;
		}

		const int32 Changed = Apply(Skeleton);
		if (Changed < 0)
		{
			return;
		}
		bDone = true;
		if (Changed == 0)
		{
			UE_LOG(LogTemp, Display, TEXT("[DCC Retarget] %s already had crawler retargeting set; nothing changed."), *Skeleton->GetName());
		}
	}
}

#if WITH_EDITOR

namespace
{
	void FixCrawlerRetargeting(const TArray<FString>& Args)
	{
		const FString Path = Args.Num() > 0 ? Args[0] : FString(DefaultCrawlerSkeleton);
		USkeleton* Skeleton = LoadObject<USkeleton>(nullptr, *Path);
		if (!Skeleton)
		{
			UE_LOG(LogTemp, Error, TEXT("[DCC Retarget] No skeleton at %s"), *Path);
			return;
		}
		Skeleton->Modify();
		DCCRetarget::Apply(Skeleton);
		Skeleton->MarkPackageDirty();
		UE_LOG(LogTemp, Display, TEXT("[DCC Retarget] Save %s to keep the setting in the asset."), *Skeleton->GetName());
	}

	FAutoConsoleCommand GFixCrawlerRetargetingCommand(
		TEXT("DCC.FixCrawlerRetargeting"),
		TEXT("Writes translation retargeting into the shared crawler skeleton asset (it is also applied automatically at runtime). Optional arg: skeleton object path."),
		FConsoleCommandWithArgsDelegate::CreateStatic(&FixCrawlerRetargeting));
}

#endif // WITH_EDITOR
