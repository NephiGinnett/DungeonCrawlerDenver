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
// This is an asset setting, normally clicked in the Skeleton editor. A command
// instead, so it can be run over MCP (or the console) without opening the
// editor UI, and re-run safely - it is idempotent.
//
// Usage (editor console):  DCC.FixCrawlerRetargeting [optional skeleton path]
// Then save the skeleton asset.
// ---------------------------------------------------------------------------

#include "CoreMinimal.h"

#if WITH_EDITOR

#include "Animation/Skeleton.h"
#include "HAL/IConsoleManager.h"
#include "ReferenceSkeleton.h"

namespace
{
	const TCHAR* DefaultCrawlerSkeleton = TEXT("/Game/DCC/Characters/Shared/SK_DCCCrawler_Skeleton.SK_DCCCrawler_Skeleton");

	void FixCrawlerRetargeting(const TArray<FString>& Args)
	{
		const FString Path = Args.Num() > 0 ? Args[0] : FString(DefaultCrawlerSkeleton);
		USkeleton* Skeleton = LoadObject<USkeleton>(nullptr, *Path);
		if (!Skeleton)
		{
			UE_LOG(LogTemp, Error, TEXT("[DCC Retarget] No skeleton at %s"), *Path);
			return;
		}

		const FReferenceSkeleton& Ref = Skeleton->GetReferenceSkeleton();
		Skeleton->Modify();

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

		Skeleton->MarkPackageDirty();
		UE_LOG(LogTemp, Display,
			TEXT("[DCC Retarget] %s: %d of %d bones updated (root '%s' = AnimationScaled, the rest = Skeleton). Save the asset to keep it."),
			*Skeleton->GetName(), Changed, Ref.GetNum(), *Ref.GetBoneName(0).ToString());
	}

	FAutoConsoleCommand GFixCrawlerRetargetingCommand(
		TEXT("DCC.FixCrawlerRetargeting"),
		TEXT("Sets translation retargeting on the shared crawler skeleton so shared animations stop stretching bones. Optional arg: skeleton object path."),
		FConsoleCommandWithArgsDelegate::CreateStatic(&FixCrawlerRetargeting));
}

#endif // WITH_EDITOR
