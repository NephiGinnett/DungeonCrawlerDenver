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

namespace
{
	const TCHAR* DefaultCrawlerSkeleton = TEXT("/Game/DCC/Characters/Shared/SK_DCCCrawler_Skeleton.SK_DCCCrawler_Skeleton");
}

namespace DCCRetarget
{
	int32 Apply(USkeleton* Skeleton)
	{
		if (!Skeleton)
		{
			return 0;
		}
		const FReferenceSkeleton& Ref = Skeleton->GetReferenceSkeleton();
		int32 Changed = 0;
		for (int32 BoneIndex = 0; BoneIndex < Ref.GetNum(); ++BoneIndex)
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
		const USkeletalMesh* Asset = Mesh ? Mesh->GetSkeletalMeshAsset() : nullptr;
		// The skeleton is shared engine data; the mesh only hands it out as const.
		USkeleton* Skeleton = Asset ? const_cast<USkeleton*>(Asset->GetSkeleton()) : nullptr;
		if (Skeleton && Skeleton->GetPathName() == DefaultCrawlerSkeleton)
		{
			const int32 Changed = Apply(Skeleton);
			static bool bReported = false;
			if (!bReported && Changed == 0)
			{
				UE_LOG(LogTemp, Display, TEXT("[DCC Retarget] %s already had crawler retargeting set; nothing changed."), *Skeleton->GetName());
			}
			bReported = true;
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
