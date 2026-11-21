#pragma once

#include "CoreMinimal.h"

class USkeleton;
class USkeletalMeshComponent;

namespace DCCRetarget
{
	/**
	 * Translation retargeting so shared clips stop forcing one body's bone
	 * lengths onto another (the "backwards S spine"): root = AnimationScaled,
	 * every other bone = Skeleton. Returns how many bones changed, or -1 when
	 * the skeleton's bone tree is not ready to write. Safe at runtime:
	 * animation decompression reads these modes every evaluation.
	 */
	int32 Apply(USkeleton* Skeleton);

	/**
	 * Applies the fix to this mesh's skeleton if it is the shared crawler
	 * skeleton. Called from every crawler's BeginPlay and on every body swap,
	 * so it works in PIE and packaged builds without saving the asset.
	 */
	void EnsureForMesh(const USkeletalMeshComponent* Mesh);
}
